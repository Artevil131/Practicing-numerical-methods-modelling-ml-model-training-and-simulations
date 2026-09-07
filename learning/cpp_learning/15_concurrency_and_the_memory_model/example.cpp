// Chapter 15 — Concurrency and the C++ memory model.
//
// REQUIRES C++20 (std::jthread, std::stop_token, std::latch, std::barrier, std::counting_semaphore,
// std::atomic_ref, std::atomic<T>::wait/notify, std::hardware_destructive_interference_size).
//
// Compile:  c++ -Wall -Wextra -std=c++20 -O2 -pthread -o ex_demo example.cpp
// Run:      ./ex_demo
// ThreadSanitizer (finds data races; ~5-10x slower):
//           c++ -Wall -Wextra -std=c++20 -O1 -g -pthread -fsanitize=thread -o ex_demo example.cpp && ./ex_demo
//           (set RACE_DEMO=1 in the environment to run a deliberately racy counter and watch TSan report it)
//           Two KNOWN FALSE POSITIVES on macOS/libc++ (see lesson §17): TSan does not model
//           std::atomic_thread_fence (section 5, fence variant), and std::barrier's arrival algorithm
//           is compiled into libc++.dylib, which is not instrumented, so data ordered only by a barrier
//           (sections 6 and 13) is reported. Silence them with a suppressions file:
//               printf 'race:s5::run\nrace:s6::run\nrace:s13::\n' > tsan.supp
//               TSAN_OPTIONS=suppressions=tsan.supp ./ex_demo
//           Everything else in this file runs TSan-clean.
// With OpenMP (Apple clang ships no libomp; `brew install libomp`):
//           c++ -Wall -Wextra -std=c++20 -O2 -Xpreprocessor -fopenmp \
//               -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp -o ex_demo example.cpp
// Parallel STL on macOS (libc++ experimental, libdispatch backend):
//           c++ -Wall -Wextra -std=c++20 -O2 -fexperimental-library -DHAVE_PSTL -o ex_demo example.cpp
//
// The OpenMP and PSTL sections are guarded (#ifdef _OPENMP / HAVE_PSTL) so the plain command
// compiles with zero warnings everywhere. On the machine this was written on libomp was not
// installed, so the OpenMP block is compile-checked against the guard only.
//
// Sections:
//   1  std::thread vs std::jthread, stop_token cancellation
//   2  mutexes: lock_guard, unique_lock, scoped_lock (deadlock-free transfer), shared_mutex
//   3  condition_variable with a predicate loop: a bounded queue
//   4  atomics: counter, CAS loop (atomic max of doubles), is_lock_free, atomic_ref
//   5  the memory model: message passing with acquire/release, checked; relaxed counter
//   6  C++20 sync primitives: latch, barrier, counting_semaphore, atomic::wait
//   7  futures: async, promise, packaged_task
//   8  a thread pool with a work queue returning futures
//   9  SPSC lock-free ring buffer with acquire/release, producer/consumer asserted
//  10  false sharing: padded vs unpadded per-thread counters, timed
//  11  call_once, static local init, thread_local
//  12  parallel reduction (deterministic per-thread partials) vs serial: timed, asserted
//  13  domain decomposition: 1-D heat equation with a barrier per step, asserted vs serial
//  14  OpenMP (guarded) and parallel STL (guarded)
//  15  speedup / efficiency table

#include <algorithm>
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <queue>
#include <semaphore>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif
#ifdef HAVE_PSTL
#include <execution>
#endif

using clk = std::chrono::steady_clock;
static double ms_since(clk::time_point t0) { return std::chrono::duration<double, std::milli>(clk::now() - t0).count(); }
static const unsigned HW = std::max(2u, std::thread::hardware_concurrency());

// =====================================================================================
// 1. std::thread vs std::jthread, stop_token
// =====================================================================================
namespace s1 {
void run() {
    std::puts("--- 1. threads and stop tokens");
    // std::thread: you MUST join() (or detach()) before the destructor runs, or std::terminate.
    int result = 0;
    std::thread t([&] { result = 42; });
    t.join();
    assert(result == 42);

    // std::jthread: joins in its destructor, and passes a stop_token if the callable accepts one.
    std::atomic<long> iterations{0};
    {
        std::jthread worker([&](std::stop_token st) {
            while (!st.stop_requested()) { iterations.fetch_add(1, std::memory_order_relaxed); std::this_thread::yield(); }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        // worker.request_stop() is called by ~jthread, then join(). Cooperative: the loop must check.
    }
    // stop_callback: runs when stop is requested (used to wake a blocked wait).
    std::stop_source src;
    bool cb_ran = false;
    std::stop_callback cb(src.get_token(), [&] { cb_ran = true; });
    src.request_stop();
    assert(cb_ran && src.stop_requested());
    std::printf("hardware_concurrency=%u; jthread spun %ld iterations before cooperative stop\n", HW, iterations.load());
}
}  // namespace s1

// =====================================================================================
// 2. Mutexes
// =====================================================================================
namespace s2 {
struct Account {
    mutable std::mutex m;
    double balance = 0;
};
// scoped_lock locks any number of mutexes with a deadlock-avoidance algorithm (std::lock): the
// classic transfer(a,b) || transfer(b,a) deadlock cannot happen.
void transfer(Account& from, Account& to, double amt) {
    std::scoped_lock lk(from.m, to.m);
    from.balance -= amt; to.balance += amt;
}

// shared_mutex: many readers OR one writer. Good for read-mostly config/lookup tables.
class Params {
    mutable std::shared_mutex m_;
    std::vector<double> w_;
public:
    explicit Params(std::size_t n) : w_(n, 1.0) {}
    double get(std::size_t i) const { std::shared_lock lk(m_); return w_[i]; }          // concurrent readers
    void   scale(double s) { std::unique_lock lk(m_); for (auto& x : w_) x *= s; }      // exclusive writer
};

void run() {
    std::puts("--- 2. mutexes");
    Account a, b; a.balance = 100; b.balance = 100;
    {
        std::jthread t1([&] { for (int i = 0; i < 10000; ++i) transfer(a, b, 1); });
        std::jthread t2([&] { for (int i = 0; i < 10000; ++i) transfer(b, a, 1); });
    }
    assert(a.balance == 100 && b.balance == 100);          // money conserved, no deadlock

    // unique_lock: deferred/timed/manually unlockable; needed by condition_variable.
    std::mutex m;
    std::unique_lock lk(m, std::defer_lock);
    assert(!lk.owns_lock()); lk.lock(); assert(lk.owns_lock()); lk.unlock();

    Params p(64);
    std::atomic<int> reads{0};
    {
        std::vector<std::jthread> readers;
        for (unsigned i = 0; i < 4; ++i)
            readers.emplace_back([&] { for (int k = 0; k < 20000; ++k) { volatile double x = p.get(k % 64); (void)x; reads.fetch_add(1, std::memory_order_relaxed); } });
        std::jthread writer([&] { for (int k = 0; k < 100; ++k) p.scale(1.0); });
    }
    assert(reads == 80000 && p.get(0) == 1.0);
    std::puts("scoped_lock transfer both directions: balances conserved, no deadlock; shared_mutex 4 readers + 1 writer ok");
}
}  // namespace s2

// =====================================================================================
// 3. condition_variable: bounded queue
// =====================================================================================
namespace s3 {
template <class T>
class BoundedQueue {
    std::mutex m_;
    std::condition_variable not_empty_, not_full_;
    std::deque<T> q_;
    std::size_t cap_;
    bool closed_ = false;
public:
    explicit BoundedQueue(std::size_t cap) : cap_(cap) {}
    void push(T v) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_; });      // PREDICATE LOOP: handles spurious wakeups
        q_.push_back(std::move(v));
        lk.unlock();                                               // unlock before notify: waiter doesn't block on m_
        not_empty_.notify_one();
    }
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;                       // closed and drained
        T v = std::move(q_.front()); q_.pop_front();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }
    void close() { { std::lock_guard lk(m_); closed_ = true; } not_empty_.notify_all(); }
};

void run() {
    std::puts("--- 3. condition_variable bounded queue");
    BoundedQueue<int> q(8);
    const int N = 50000;
    long long consumed_sum = 0; int consumed = 0;
    {
        std::jthread consumer([&] { while (auto v = q.pop()) { consumed_sum += *v; ++consumed; } });
        std::jthread producer([&] { for (int i = 1; i <= N; ++i) q.push(i); q.close(); });
    }
    assert(consumed == N && consumed_sum == 1LL * N * (N + 1) / 2);
    std::printf("producer/consumer through a capacity-8 queue: %d items, sum ok\n", consumed);
}
}  // namespace s3

// =====================================================================================
// 4. Atomics
// =====================================================================================
namespace s4 {
// CAS loop: atomic max for double. There is no fetch_max; atomic<double> has only load/store/
// exchange/CAS and (C++20) fetch_add/fetch_sub. Everything else is a compare_exchange loop.
void atomic_max(std::atomic<double>& target, double v) {
    double cur = target.load(std::memory_order_relaxed);
    while (cur < v && !target.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {
        // on failure `cur` is reloaded with the current value; loop re-checks cur < v
    }
}

void run() {
    std::puts("--- 4. atomics");
    static_assert(std::atomic<int>::is_always_lock_free);
    static_assert(std::atomic<double>::is_always_lock_free);       // 8-byte on arm64/x86-64: yes
    struct Big { double a, b, c; };
    std::atomic<Big> big{};                                         // 24 bytes: NOT lock-free (uses an internal mutex)
    std::printf("atomic<int> lock-free: %d, atomic<double>: %d, atomic<24-byte struct>: %d\n",
                std::atomic<int>::is_always_lock_free, std::atomic<double>::is_always_lock_free, big.is_lock_free());

    std::atomic<long> counter{0};
    std::atomic<double> mx{-1e300};
    std::vector<double> data(1 << 16);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = std::sin(0.001 * i) * (i % 977);
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < HW; ++t)
            ts.emplace_back([&, t] {
                for (std::size_t i = t; i < data.size(); i += HW) { counter.fetch_add(1, std::memory_order_relaxed); atomic_max(mx, data[i]); }
            });
    }
    assert(counter == long(data.size()));
    assert(mx.load() == *std::max_element(data.begin(), data.end()));

    // atomic_ref (C++20): atomic operations on an ordinary object you don't own the type of —
    // e.g. one element of a std::vector<int> histogram. All accesses during its lifetime must go
    // through atomic_ref (mixing plain and atomic access is a data race).
    std::vector<int> hist(16, 0);
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < HW; ++t)
            ts.emplace_back([&, t] { for (int i = int(t); i < 16000; i += int(HW)) { std::atomic_ref<int> bin(hist[i % 16]); bin.fetch_add(1, std::memory_order_relaxed); } });
    }
    assert(std::accumulate(hist.begin(), hist.end(), 0) == 16000 && hist[3] == 1000);
    std::printf("fetch_add counter=%ld, CAS-loop atomic max=%.3f, atomic_ref histogram ok\n", counter.load(), mx.load());
}
}  // namespace s4

// =====================================================================================
// 5. The memory model: message passing
// =====================================================================================
namespace s5 {
// The canonical pattern. Producer writes payload (plain), then sets flag with RELEASE. Consumer
// spins on flag with ACQUIRE; once it sees true, the release store "synchronizes-with" the
// acquire load, so the payload write "happens-before" the payload read: reading it is safe and
// sees 42. With memory_order_relaxed on both sides, the consumer could legally see flag==true and
// payload==0 (and does, on ARM, which reorders stores; x86 would hide the bug).
struct MessagePassing {
    alignas(64) int payload = 0;
    alignas(64) std::atomic<bool> flag{false};
};

void run() {
    std::puts("--- 5. memory model: message passing");
    int ok = 0;
    const int trials = 2000;
    for (int k = 0; k < trials; ++k) {
        MessagePassing mp;
        std::jthread consumer([&] {
            while (!mp.flag.load(std::memory_order_acquire)) { /* spin */ }
            if (mp.payload == 42) ++ok;                             // guaranteed by acquire/release
        });
        mp.payload = 42;
        mp.flag.store(true, std::memory_order_release);
    }
    assert(ok == trials);

    // Relaxed is fine when the ONLY thing that matters is the final value of that one atomic
    // (statistics counters), because every atomic is still individually coherent (modification order).
    std::atomic<long> hits{0};
    { std::vector<std::jthread> ts; for (unsigned t = 0; t < HW; ++t) ts.emplace_back([&] { for (int i = 0; i < 100000; ++i) hits.fetch_add(1, std::memory_order_relaxed); }); }
    assert(hits == 100000L * HW);

    // Fences: the same message-passing with a standalone fence instead of an ordered store.
    // atomic_thread_fence(release) before a relaxed store == release store (for ordering purposes).
    MessagePassing mp2; int seen = -1;
    {
        std::jthread consumer([&] {
            while (!mp2.flag.load(std::memory_order_relaxed)) {}
            std::atomic_thread_fence(std::memory_order_acquire);
            seen = mp2.payload;
        });
        mp2.payload = 7;
        std::atomic_thread_fence(std::memory_order_release);
        mp2.flag.store(true, std::memory_order_relaxed);
    }
    assert(seen == 7);
    std::printf("message passing %d/%d correct with acquire/release; relaxed counter=%ld; fence variant ok\n", ok, trials, hits.load());
}
}  // namespace s5

// =====================================================================================
// 6. C++20 primitives: latch, barrier, counting_semaphore, atomic::wait
// =====================================================================================
namespace s6 {
void run() {
    std::puts("--- 6. latch, barrier, semaphore, atomic wait");
    // latch: single-use countdown. "Wait until all N workers have initialized."
    std::latch ready(HW);
    std::atomic<int> initialized{0};
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < HW; ++t) ts.emplace_back([&] { initialized.fetch_add(1); ready.count_down(); });
        ready.wait();
        assert(initialized == int(HW));
    }

    // barrier: reusable; all threads meet at the end of each phase. The completion function runs
    // once per phase by exactly one thread (here: advance the shared step counter).
    int phase = 0;
    std::barrier sync(HW, [&]() noexcept { ++phase; });
    std::vector<int> phase_seen(HW, 0);
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < HW; ++t)
            ts.emplace_back([&, t] { for (int s = 0; s < 5; ++s) { phase_seen[t] = phase; sync.arrive_and_wait(); } });
    }
    assert(phase == 5);
    for (unsigned t = 0; t < HW; ++t) assert(phase_seen[t] == 4);   // every thread saw phase 4 before its last arrive

    // counting_semaphore: limit concurrency to K (e.g. K GPU streams or K open files).
    std::counting_semaphore<4> slots(2);
    std::atomic<int> inside{0}, max_inside{0};
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < HW; ++t)
            ts.emplace_back([&] {
                for (int k = 0; k < 50; ++k) {
                    slots.acquire();
                    int now = inside.fetch_add(1) + 1;
                    int m = max_inside.load(); while (m < now && !max_inside.compare_exchange_weak(m, now)) {}
                    std::this_thread::yield();
                    inside.fetch_sub(1);
                    slots.release();
                }
            });
    }
    assert(max_inside <= 2);

    // atomic<T>::wait/notify (C++20): block until the value changes — a futex, no spinning.
    std::atomic<int> state{0};
    {
        std::jthread waiter([&] { state.wait(0); assert(state == 1); state = 2; state.notify_one(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        state = 1; state.notify_one();
        state.wait(1);
    }
    assert(state == 2);
    std::printf("latch ok; barrier ran %d phases; semaphore max concurrency %d (limit 2); atomic wait/notify ok\n", phase, max_inside.load());
}
}  // namespace s6

// =====================================================================================
// 7. Futures
// =====================================================================================
namespace s7 {
double slow_norm(const std::vector<double>& v) { double s = 0; for (double x : v) s += x * x; return std::sqrt(s); }

void run() {
    std::puts("--- 7. futures");
    std::vector<double> v(1 << 20, 3.0);
    // std::async with std::launch::async runs on a new thread; the future's destructor JOINS it
    // (the one place a future blocks in its destructor). Without the launch policy, the runtime
    // may defer execution until .get().
    auto f1 = std::async(std::launch::async, slow_norm, std::cref(v));
    auto f2 = std::async(std::launch::deferred, [] { return 1 + 1; });   // runs on .get(), same thread
    assert(f2.get() == 2);
    assert(std::abs(f1.get() - 3.0 * 1024.0) < 1e-9);

    // promise/future: hand a value (or exception) from one thread to another, once.
    std::promise<std::string> p;
    std::future<std::string> fut = p.get_future();
    std::jthread t([&p] { p.set_value("done"); });
    assert(fut.get() == "done");

    // exceptions travel through futures.
    std::promise<int> perr;
    auto ferr = perr.get_future();
    perr.set_exception(std::make_exception_ptr(std::runtime_error("bad")));
    bool caught = false;
    try { ferr.get(); } catch (const std::runtime_error& e) { caught = std::string(e.what()) == "bad"; }
    assert(caught);

    // packaged_task: a callable whose result goes to a future — the building block of a thread pool.
    std::packaged_task<int(int, int)> task([](int a, int b) { return a * b; });
    auto fres = task.get_future();
    task(6, 7);
    assert(fres.get() == 42);
    std::puts("async/deferred, promise, exception propagation, packaged_task ok");
}
}  // namespace s7

// =====================================================================================
// 8. Thread pool
// =====================================================================================
namespace s8 {
class ThreadPool {
    // MEMBER ORDER IS LOAD-BEARING. Members are destroyed in reverse declaration order, and the
    // jthreads join in their destructor. If workers_ were declared first it would be destroyed
    // LAST — i.e. the mutex and condition variable would be destroyed while workers still wait on
    // them ("mutex lock failed: Invalid argument" at exit, and a TSan report). The first draft of
    // this file had exactly that bug; ThreadSanitizer found it.
    std::queue<std::function<void()>> tasks_;
    std::mutex m_;
    std::condition_variable_any cv_;             // _any: can wait with a stop_token
    std::vector<std::jthread> workers_;          // last member => destroyed (joined) first
public:
    explicit ThreadPool(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this](std::stop_token st) {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock lk(m_);
                        // returns false only if stop requested AND queue empty: drain before exit
                        if (!cv_.wait(lk, st, [&] { return !tasks_.empty(); })) return;
                        job = std::move(tasks_.front()); tasks_.pop();
                    }
                    job();
                }
            });
    }
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        // shared_ptr because std::function requires copyable callables and packaged_task is move-only.
        auto task = std::make_shared<std::packaged_task<R()>>(
            [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable { return std::invoke(f, args...); });
        std::future<R> fut = task->get_future();
        { std::lock_guard lk(m_); tasks_.emplace([task] { (*task)(); }); }
        cv_.notify_one();
        return fut;
    }
    ~ThreadPool() { for (auto& w : workers_) w.request_stop(); cv_.notify_all(); }   // jthreads join
};

void run() {
    std::puts("--- 8. thread pool");
    ThreadPool pool(HW);
    std::vector<std::future<double>> results;
    for (int i = 0; i < 64; ++i)
        results.push_back(pool.submit([](int n) { double s = 0; for (int k = 1; k <= n; ++k) s += 1.0 / k; return s; }, 1000 * (i + 1)));
    double total = 0; for (auto& f : results) total += f.get();
    assert(total > 500 && total < 700);
    auto fex = pool.submit([]() -> int { throw std::logic_error("task failed"); });
    bool caught = false; try { fex.get(); } catch (const std::logic_error&) { caught = true; }
    assert(caught);
    std::printf("64 tasks on %u workers: sum of harmonic numbers = %.3f; exception propagated\n", HW, total);
}
}  // namespace s8

// =====================================================================================
// 9. SPSC ring buffer
// =====================================================================================
namespace s9 {
// Single-producer single-consumer lock-free queue. Correctness rests on exactly two facts:
//   * head_ is written only by the consumer, tail_ only by the producer (single writer each).
//   * The producer's tail_.store(release) after writing the slot synchronizes-with the consumer's
//     tail_.load(acquire), so the slot contents are visible; symmetrically for head_ so the
//     producer never overwrites a slot the consumer is still reading.
// Capacity is a power of two so the modulo is a mask. One slot is kept empty to distinguish
// full from empty. head_ and tail_ live on separate cache lines (each is written by a different
// core; sharing a line would make every push invalidate the consumer's line — false sharing).
template <class T, std::size_t N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head_{0};   // consumer owns
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail_{0};   // producer owns
    alignas(std::hardware_destructive_interference_size) T buf_[N];
public:
    bool try_push(const T& v) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);           // own variable: relaxed
        const std::size_t next = (t + 1) & (N - 1);
        if (next == head_.load(std::memory_order_acquire)) return false;        // full; acquire: consumer done reading slot
        buf_[t] = v;
        tail_.store(next, std::memory_order_release);                           // publish the slot
        return true;
    }
    bool try_pop(T& out) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;           // empty; acquire: see producer's write
        out = buf_[h];
        head_.store((h + 1) & (N - 1), std::memory_order_release);              // free the slot
        return true;
    }
};

void run() {
    std::puts("--- 9. SPSC ring buffer");
    static SpscRing<std::uint64_t, 1024> ring;          // static: 3 cache lines + 8 KB, keep it off the stack
    const std::uint64_t N = 2'000'000;
    std::uint64_t received = 0, sum = 0; bool in_order = true;
    auto t0 = clk::now();
    {
        std::jthread consumer([&] {
            std::uint64_t expect = 0, v;
            while (expect < N) {
                if (ring.try_pop(v)) { if (v != expect) in_order = false; sum += v; ++expect; ++received; }
            }
        });
        std::jthread producer([&] { for (std::uint64_t i = 0; i < N; ++i) while (!ring.try_push(i)) {} });
    }
    double ms = ms_since(t0);
    assert(received == N && in_order && sum == N * (N - 1) / 2);
    std::printf("%llu items through the ring in %.1f ms (%.0f M items/s), all in order, checksum ok\n",
                (unsigned long long)N, ms, N / ms / 1000.0);
    // MPMC (many producers/consumers) is a different problem: a second producer racing on tail_
    // needs a CAS, slots need sequence numbers (Vyukov's bounded MPMC queue), and reclamation gets
    // hard. Use a mutex+deque or a library (moodycamel::ConcurrentQueue, boost::lockfree) instead.
}
}  // namespace s9

// =====================================================================================
// 10. False sharing
// =====================================================================================
namespace s10 {
struct Unpadded { std::atomic<long> v{0}; };                                          // 8 bytes: 8 per line → shared
struct Padded { alignas(std::hardware_destructive_interference_size) std::atomic<long> v{0}; };   // one per line

template <class Slot>
double time_counters(unsigned nthreads, long iters) {
    std::vector<Slot> slots(nthreads);
    auto t0 = clk::now();
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < nthreads; ++t)
            ts.emplace_back([&, t] { for (long i = 0; i < iters; ++i) slots[t].v.fetch_add(1, std::memory_order_relaxed); });
    }
    long total = 0; for (auto& s : slots) total += s.v.load();
    assert(total == long(nthreads) * iters);
    return ms_since(t0);
}

void run() {
    std::puts("--- 10. false sharing");
    const long iters = 2'000'000;
    double unp = time_counters<Unpadded>(HW, iters), pad = time_counters<Padded>(HW, iters);
    std::printf("hardware_destructive_interference_size=%zu; %u threads x %ld increments: unpadded %.1f ms, padded %.1f ms (%.1fx)\n",
                std::hardware_destructive_interference_size, HW, iters, unp, pad, unp / pad);
}
}  // namespace s10

// =====================================================================================
// 11. call_once, static locals, thread_local
// =====================================================================================
namespace s11 {
std::once_flag flag;
int init_count = 0;
const std::vector<double>& table() {                       // thread-safe lazy init: guaranteed once since C++11
    static const std::vector<double> t = [] { ++init_count; std::vector<double> v(256); for (int i = 0; i < 256; ++i) v[i] = std::exp(-i / 32.0); return v; }();
    return t;
}
thread_local int tl_counter = 0;                            // one instance per thread

void run() {
    std::puts("--- 11. once, statics, thread_local");
    int once_count = 0;
    std::vector<int> per_thread(HW, 0);
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < HW; ++t)
            ts.emplace_back([&, t] {
                std::call_once(flag, [&] { ++once_count; });
                (void)table();
                for (int i = 0; i < 1000; ++i) ++tl_counter;   // no race: each thread has its own
                per_thread[t] = tl_counter;
            });
    }
    assert(once_count == 1 && init_count == 1);
    for (unsigned t = 0; t < HW; ++t) assert(per_thread[t] == 1000);
    assert(tl_counter == 0);                                 // main thread's copy untouched
    std::puts("call_once ran once; static local initialized once under contention; thread_local isolated");
}
}  // namespace s11

// =====================================================================================
// 12. Parallel reduction vs serial
// =====================================================================================
namespace s12 {
double serial_dot(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0; for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i]; return s;
}
// Deterministic: fixed chunking → each thread's partial is bit-identical run to run, and the
// final combine is in fixed order. An atomic<double> accumulator would be both slow (contended
// cache line) and nondeterministic in the last bits (fp addition isn't associative).
double parallel_dot(const std::vector<double>& a, const std::vector<double>& b, unsigned nthreads) {
    std::vector<double> partial(nthreads, 0.0);
    const std::size_t n = a.size(), chunk = (n + nthreads - 1) / nthreads;
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < nthreads; ++t)
            ts.emplace_back([&, t] {
                const std::size_t lo = t * chunk, hi = std::min(n, lo + chunk);
                double s = 0;                                  // local accumulator: no false sharing in the loop
                for (std::size_t i = lo; i < hi; ++i) s += a[i] * b[i];
                partial[t] = s;                                // one write per thread
            });
    }
    double s = 0; for (double p : partial) s += p;             // fixed order
    return s;
}

double best_of(int reps, const std::function<double()>& f, double& result) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) { auto t0 = clk::now(); result = f(); best = std::min(best, ms_since(t0)); }
    return best;
}

void run() {
    std::puts("--- 12. parallel reduction vs serial");
    const std::size_t n = 1 << 24;                             // 16M doubles x2 = 256 MB: memory-bound
    std::vector<double> a(n), b(n);
    for (std::size_t i = 0; i < n; ++i) { a[i] = 1.0 + (i % 7) * 0.125; b[i] = 0.5 - (i % 3) * 0.25; }
    double rs, rp;
    double ts = best_of(3, [&] { return serial_dot(a, b); }, rs);
    double tp = best_of(3, [&] { return parallel_dot(a, b, HW); }, rp);
    double rp2 = parallel_dot(a, b, HW);
    assert(std::abs(rs - rp) <= 1e-9 * std::abs(rs));
    assert(rp == rp2);                                         // deterministic: bit-identical across runs
    std::printf("dot of %zu: serial %.1f ms, %u threads %.1f ms, speedup %.2fx, efficiency %.0f%% (memory-bound: ~%.1f GB/s)\n",
                n, ts, HW, tp, ts / tp, 100 * ts / tp / HW, 2.0 * n * sizeof(double) / tp / 1e6);
}
}  // namespace s12

// =====================================================================================
// 13. Domain decomposition: 1-D heat equation with a barrier per step
// =====================================================================================
namespace s13 {
// u_new[i] = u[i] + r (u[i-1] - 2u[i] + u[i+1]), Dirichlet u[0]=u[n-1]=0. Each thread owns a
// contiguous block of cells; it reads its neighbours' boundary cells (halo) which were written in
// the PREVIOUS step, so a barrier between steps is the only synchronization needed. The barrier's
// completion function swaps the buffers exactly once per step.
void serial(std::vector<double>& u, std::vector<double>& v, int steps, double r) {
    const std::size_t n = u.size();
    for (int s = 0; s < steps; ++s) {
        for (std::size_t i = 1; i + 1 < n; ++i) v[i] = u[i] + r * (u[i - 1] - 2 * u[i] + u[i + 1]);
        std::swap(u, v);
    }
}
void parallel(std::vector<double>& u, std::vector<double>& v, int steps, double r, unsigned nthreads) {
    const std::size_t n = u.size();
    double* cur = u.data(); double* nxt = v.data();
    std::barrier step_done(nthreads, [&]() noexcept { std::swap(cur, nxt); });
    {
        std::vector<std::jthread> ts;
        const std::size_t interior = n - 2, chunk = (interior + nthreads - 1) / nthreads;
        for (unsigned t = 0; t < nthreads; ++t)
            ts.emplace_back([&, t] {
                const std::size_t lo = 1 + t * chunk, hi = std::min(n - 1, lo + chunk);
                for (int s = 0; s < steps; ++s) {
                    double* c = cur; double* x = nxt;          // read the pointers once per step (swapped by the barrier)
                    for (std::size_t i = lo; i < hi; ++i) x[i] = c[i] + r * (c[i - 1] - 2 * c[i] + c[i + 1]);
                    step_done.arrive_and_wait();
                }
            });
    }
    if (steps % 2 == 1) std::swap(u, v);                       // result ends in whichever buffer `cur` points at
}

void run() {
    std::puts("--- 13. domain decomposition: 1-D heat equation");
    const std::size_t n = 1 << 20; const int steps = 200; const double r = 0.25;
    std::vector<double> u0(n, 0.0); for (std::size_t i = n / 4; i < 3 * n / 4; ++i) u0[i] = 1.0;
    std::vector<double> us = u0, vs(n, 0.0), up = u0, vp(n, 0.0);
    auto t0 = clk::now(); serial(us, vs, steps, r); double t_s = ms_since(t0);
    t0 = clk::now(); parallel(up, vp, steps, r, HW); double t_p = ms_since(t0);
    double maxdiff = 0; for (std::size_t i = 0; i < n; ++i) maxdiff = std::max(maxdiff, std::abs(us[i] - up[i]));
    assert(maxdiff == 0.0);                                    // same operations in the same order per cell: bit-identical
    std::printf("n=%zu, %d steps: serial %.1f ms, %u threads %.1f ms (%.2fx); max |serial - parallel| = %g\n",
                n, steps, t_s, HW, t_p, t_s / t_p, maxdiff);
    // The speedup is modest on purpose: one step is ~0.1 ms of work for 1M cells and each barrier
    // costs 10-50 us with 10 threads, so synchronization is a large fraction of the step. Make
    // the grid 2-D and 16x larger (or take several steps per barrier with wider halos) and the
    // same code scales. Granularity, not correctness, is the usual parallel-numerics problem.
}
}  // namespace s13

// =====================================================================================
// 14. OpenMP and parallel STL (both guarded)
// =====================================================================================
namespace s14 {
void run() {
    std::puts("--- 14. OpenMP / parallel STL");
#ifdef _OPENMP
    const std::size_t n = 1 << 22;
    std::vector<double> x(n); for (std::size_t i = 0; i < n; ++i) x[i] = std::sin(0.001 * i);
    double s = 0;
    #pragma omp parallel for reduction(+ : s) schedule(static)
    for (std::ptrdiff_t i = 0; i < std::ptrdiff_t(n); ++i) s += x[i] * x[i];
    // collapse(2): flatten a 2-D loop nest so both loops share the iteration space.
    const int R = 512, C = 512; std::vector<double> grid(R * C);
    #pragma omp parallel for collapse(2) schedule(dynamic, 64)
    for (int i = 0; i < R; ++i) for (int j = 0; j < C; ++j) grid[i * C + j] = std::exp(-(i * i + j * j) * 1e-5);
    std::printf("OpenMP: %d threads, sum of squares = %.3f, grid[0]=%.3f\n", omp_get_max_threads(), s, grid[0]);
#else
    std::puts("compiled without OpenMP (see header for the brew/clang command); section skipped");
#endif
#ifdef HAVE_PSTL
    std::vector<double> v(1 << 22, 1.5);
    double total = std::reduce(std::execution::par_unseq, v.begin(), v.end(), 0.0);
    std::for_each(std::execution::par, v.begin(), v.end(), [](double& d) { d *= 2; });
    std::printf("parallel STL: reduce = %.1f, after for_each v[0] = %.1f\n", total, v[0]);
#else
    std::puts("compiled without -fexperimental-library -DHAVE_PSTL; parallel STL section skipped");
#endif
}
}  // namespace s14

// =====================================================================================
// 15. Speedup / efficiency table (strong scaling of the dot product)
// =====================================================================================
namespace s15 {
void run() {
    std::puts("--- 15. strong scaling");
    const std::size_t n = 1 << 23;
    std::vector<double> a(n, 1.0), b(n, 2.0);
    double r;
    double t1 = s12::best_of(3, [&] { return s12::parallel_dot(a, b, 1); }, r);
    std::puts("threads   ms   speedup  efficiency");
    for (unsigned p = 1; p <= HW; p *= 2) {
        double tp = s12::best_of(3, [&] { return s12::parallel_dot(a, b, p); }, r);
        std::printf("%4u  %7.2f  %6.2fx  %6.0f%%\n", p, tp, t1 / tp, 100.0 * t1 / tp / p);
    }
    std::puts("(efficiency falls because the dot product is memory-bound and Apple Silicon mixes P- and E-cores)");
}
}  // namespace s15

// Deliberate data race for ThreadSanitizer: RACE_DEMO=1 ./ex_demo (built with -fsanitize=thread).
static void race_demo() {
    long counter = 0;                                          // plain long: ++ from two threads is UB
    { std::jthread t1([&] { for (int i = 0; i < 100000; ++i) ++counter; });
      std::jthread t2([&] { for (int i = 0; i < 100000; ++i) ++counter; }); }
    std::printf("racy counter = %ld (expected 200000; any other value is the UB showing)\n", counter);
}

int main() {
    if (std::getenv("RACE_DEMO")) { race_demo(); return 0; }
    s1::run(); s2::run(); s3::run(); s4::run(); s5::run(); s6::run(); s7::run(); s8::run();
    s9::run(); s10::run(); s11::run(); s12::run(); s13::run(); s14::run(); s15::run();
    std::puts("\nall checks passed");
    return 0;
}
