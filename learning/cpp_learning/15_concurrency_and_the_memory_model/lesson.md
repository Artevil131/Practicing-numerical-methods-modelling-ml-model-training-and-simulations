# Chapter 15 — Concurrency and the C++ Memory Model

This chapter is C++20. Compile with `c++ -Wall -Wextra -std=c++20 -O2 -pthread`. (`-pthread` is
accepted and harmless on macOS, where libpthread is part of libSystem; on Linux it is required.)
Prerequisites: `../12_performance/lesson.md` §§15–16 and §19 (first contact with threads, atomics,
false sharing, Amdahl), `../07_move_semantics_and_smart_pointers/lesson.md`, and the C course's
`../../c_learning/13_debugging_testing_perf/lesson.md` for sanitizers. C11 `<threads.h>` does not
exist on macOS; C code uses pthreads. Everything in this chapter is the C++ layer over the same
pthreads and the same hardware.

## What you'll be able to do after this chapter

- Use `std::jthread`, the mutex family, condition variables (with the predicate loop), `std::shared_mutex`, and the C++20 primitives (`latch`, `barrier`, `counting_semaphore`, `atomic::wait`) correctly, and know which one a given problem wants.
- State the C++ memory model precisely — happens-before, synchronizes-with, the three ordering levels, fences — and prove a small lock-free protocol (message passing, an SPSC ring buffer) correct from those rules.
- Build a thread pool with futures, and choose between `std::async`, OpenMP, parallel STL and hand-rolled threads for a given kernel.
- Recognize and fix false sharing, choose deterministic reductions, decompose a grid or particle problem across threads, and measure speedup, efficiency, and strong/weak scaling honestly.
- Run ThreadSanitizer on macOS, read its report, and know its two false-positive classes on libc++.

## Why this matters for ML / numerics / sims

Your machine has 10 cores (4 performance + 6 efficiency on this M-series chip); single-threaded code uses one. Every kernel in projects P07–P10 (Barnes–Hut, FDTD, PIC, LBM) and the matmul in P01/P02 is embarrassingly or nearly-embarrassingly parallel, and the difference between a 1-thread and a 10-thread run is the difference between a 20-minute experiment and a 2-minute one. But threads are the one place where "compiles and produces the right output on my machine" proves nothing: a data race is undefined behaviour that may produce correct results for a year and then not. The memory model is the contract that lets you reason instead of test. A data loader feeding an ML training loop is a bounded queue (§3); a parameter server is `shared_mutex` (§2); gradient accumulation across threads is a deterministic reduction (§18); a particle-to-grid deposit is the scatter-add problem (§18); a GPU is the same ideas with 10 000 threads (§19).

---

## 1. `std::thread` and `std::jthread`; `std::stop_token`

`std::thread` (C++11, `<thread>`) starts a function on a new OS thread (`pthread_create` underneath). Its destructor calls `std::terminate` if the thread is still *joinable* — you must `join()` (wait for it) or `detach()` (abandon it; almost never right) on every path, including exceptions. `std::jthread` (C++20) fixes both problems: its destructor requests stop and joins, and if the callable's first parameter is a `std::stop_token`, the thread gets one.

```cpp
std::atomic<long> iterations{0};
{
    std::jthread worker([&](std::stop_token st) {
        while (!st.stop_requested()) { iterations.fetch_add(1, std::memory_order_relaxed); std::this_thread::yield(); }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}   // ~jthread: request_stop(), then join()
```

Cancellation is **cooperative**: nothing interrupts the thread; the loop must check. `std::stop_source` creates tokens; `std::stop_callback cb(token, fn)` runs `fn` when stop is requested (use it to wake a thread blocked in a wait — e.g. call `cv.notify_all()`). `std::condition_variable_any::wait(lock, stop_token, pred)` integrates the two: it returns `false` if stopped without the predicate becoming true (§9 uses this).

`std::thread::hardware_concurrency()` returns the logical core count (10 here). Thread creation costs 10–50 µs and ~512 KB of stack address space per thread (8 MB for the main thread on macOS); do not create a thread per task — that is what a pool (§9) is for. Arguments to the thread function are *copied* (decay-copied) into the thread; pass `std::ref(x)` to share, and make sure `x` outlives the thread.

Python equivalent: `threading.Thread`, but with a real difference — CPython's GIL serializes Python bytecode, so Python threads never speed up compute; C++ threads do. `multiprocessing` is the Python workaround; here you don't need it.

---

## 2. Mutexes: `mutex`, `lock_guard`, `unique_lock`, `scoped_lock`, `shared_mutex`

A **data race** ([intro.races]/21) is two conflicting accesses (at least one a write) to the same memory location from different threads, not ordered by happens-before, at least one not atomic. It is **undefined behaviour** — not "you get one of the two values" but "the program has no meaning": the compiler may hoist a plain load out of a loop (spinning forever on a non-atomic flag), tear a 64-bit store, or delete the check. Mutexes are the coarse tool that turns conflicting accesses into ordered ones.

| Type | What | Use |
|---|---|---|
| `std::mutex` | Non-recursive lock; `lock()`/`unlock()`/`try_lock()` | Never call these directly; use a guard |
| `std::lock_guard<M>` | RAII lock for one mutex, no unlock before scope end | The default |
| `std::scoped_lock<M...>` (C++17) | RAII lock for **several** mutexes, acquired deadlock-free via `std::lock` | Any time you hold two locks |
| `std::unique_lock<M>` | Movable, can `unlock()`/`lock()` again, deferred (`std::defer_lock`), timed | Required by `condition_variable`; when you must release early |
| `std::shared_mutex` (C++17) | Many `shared_lock` readers *or* one `unique_lock` writer | Read-mostly data (parameter tables, config) |
| `std::recursive_mutex` | Same thread may re-lock | A design smell; refactor instead |
| `std::timed_mutex` | `try_lock_for(duration)` | Rare |

The two-lock deadlock and its cure:

```cpp
struct Account { mutable std::mutex m; double balance = 0; };
void transfer(Account& from, Account& to, double amt) {
    std::scoped_lock lk(from.m, to.m);      // std::lock: try-and-back-off, never deadlocks, any order of arguments
    from.balance -= amt; to.balance += amt;
}
// transfer(a, b) on one thread and transfer(b, a) on another with two separate lock_guards would
// deadlock as soon as each holds its first lock. example.cpp §2 runs 10 000 of each concurrently.
```

Other rules: lock ordering (always acquire in a global order, e.g. by address) is the manual alternative; never hold a lock while calling unknown code (callbacks, `operator<<` to a shared stream); never hold a lock across a blocking I/O call; keep critical sections tiny — an uncontended lock/unlock pair costs ~20 ns, a contended one ~100 ns–1 µs plus a context switch if it has to sleep. `mutable std::mutex` lets `const` getters lock. A mutex protects *data*, not *code*: document which fields each mutex guards.

`std::shared_mutex`: `std::shared_lock` for readers, `std::unique_lock` for writers. It's slower than a plain mutex for short uncontended sections (two atomic ops and a bigger structure) and wins only when reads are long or greatly outnumber writes. For a lookup table updated once per epoch and read millions of times per epoch, it's right; for a counter, it's wrong.

---

## 3. `std::condition_variable` and the predicate loop

A condition variable lets a thread sleep until another thread signals that *something changed*, releasing the mutex while asleep. It is always paired with a mutex and a **predicate** — a condition on the protected data. The waiter must loop, because of **spurious wakeups** (the OS may wake a waiter with no notify; POSIX permits it) and **stolen wakeups** (another thread consumed the item between notify and wake-up). The overload `cv.wait(lock, pred)` *is* the loop: `while (!pred()) cv.wait(lock);`.

```cpp
template <class T> class BoundedQueue {                    // the data-loader / pipeline primitive
    std::mutex m_; std::condition_variable not_empty_, not_full_;
    std::deque<T> q_; std::size_t cap_; bool closed_ = false;
public:
    void push(T v) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_; });        // sleeps with m_ released; re-locks before returning
        q_.push_back(std::move(v));
        lk.unlock();                                                 // release BEFORE notify so the waiter doesn't wake into a held lock
        not_empty_.notify_one();
    }
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;                         // closed and drained: consumer exits
        T v = std::move(q_.front()); q_.pop_front();
        lk.unlock(); not_full_.notify_one();
        return v;
    }
    void close() { { std::lock_guard lk(m_); closed_ = true; } not_empty_.notify_all(); }
};
```

Rules: the predicate reads only data protected by the same mutex; modify the data *under the lock*, then notify (notifying under or after the lock are both correct; after is slightly faster); `notify_one` when exactly one waiter can make progress, `notify_all` when the condition affects everyone (shutdown, "closed"). A lost wakeup — notifying before the waiter has checked the predicate — is harmless *because* the predicate is re-checked under the lock; without the predicate it hangs forever. `wait_for`/`wait_until` return `std::cv_status::timeout` or, in the predicate form, the predicate's value.

Python equivalent: `queue.Queue(maxsize=cap)` is exactly this class; `threading.Condition` is the raw primitive with the same `wait_for(pred)` loop.

---

## 4. `std::atomic<T>`

`std::atomic<T>` (`<atomic>`) gives indivisible loads, stores and read-modify-writes on a `T` that is trivially copyable. Its operations never constitute a data race, and each one takes a `std::memory_order` (default `seq_cst`, §5).

| Operation | Meaning | Note |
|---|---|---|
| `load(o)`, `store(v, o)` | Read / write | `x = v`, `T y = x` are `seq_cst` shorthands |
| `exchange(v, o)` | Write, return old | |
| `fetch_add/sub/and/or/xor(v, o)` | RMW, return old | `++x` is `fetch_add(1)`; integers, pointers; `fetch_add` on floating types since C++20 |
| `compare_exchange_weak(expected, desired, o)` | If `*this == expected` write `desired`, else load into `expected`; may fail spuriously | Use in a loop |
| `compare_exchange_strong(...)` | Same, no spurious failure | Use when not in a loop |
| `is_lock_free()`, `is_always_lock_free` | Does the hardware do this without a hidden mutex? | 1, 2, 4, 8 bytes: yes on arm64/x86-64; 16 sometimes; 24: no |
| `wait(old, o)`, `notify_one/all()` (C++20) | Block until value ≠ old | A futex; §7 |

**CAS loops** are how you build any RMW the hardware doesn't have. There is no `fetch_max`, and for `double` there is no `fetch_mul`, no `fetch_max`, and (before C++20) not even `fetch_add`:

```cpp
void atomic_max(std::atomic<double>& target, double v) {
    double cur = target.load(std::memory_order_relaxed);
    while (cur < v && !target.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
    // on failure, cur is refreshed with the current value; loop exits if someone stored a bigger one
}
```

`compare_exchange_weak` may fail even when the value matched (LL/SC architectures like ARM implement CAS as a load-linked/store-conditional pair that can be interrupted); in a loop that's free. `_strong` costs an inner retry loop on ARM; use it only for a single, non-looped attempt.

**`atomic<double>` limits**: lock-free 8-byte load/store/CAS on every mainstream 64-bit CPU, `fetch_add` since C++20 (implemented as a CAS loop by libc++ — 5–20× slower than an integer `fetch_add`). A hot reduction through one `atomic<double>` is a serial bottleneck plus a contended cache line (§12 of Chapter 12 measured ~100× slower than per-thread partials). And floating-point addition is not associative, so the result differs between runs in the last bits — §18 fixes both.

**`std::atomic_ref<T>`** (C++20): atomic operations on an *ordinary* object — one element of a `std::vector<int>`, a field of a struct you don't control. Every access during the `atomic_ref`'s lifetime must go through an `atomic_ref` (mixing plain and atomic access is a race), and the object must be suitably aligned. This is the tool for a histogram or a scatter-add into an existing array without changing its type to `vector<atomic<int>>` (which isn't copyable or resizable).

---

## 5. The C++ memory model, precisely

The model ([intro.multithread], [atomics.order]) is defined in terms of relations between *evaluations*, not in terms of caches or reordering. Learn the five terms; everything else follows.

1. **Sequenced-before**: program order within one thread.
2. **Synchronizes-with**: an atomic *release* operation A on object M synchronizes-with an atomic *acquire* operation B on M if B reads the value written by A (or a value later in M's release sequence). Mutex unlock synchronizes-with the next lock; `thread::join` synchronizes-with the joining thread's continuation; thread creation synchronizes-with the start of the new thread; `latch`/`barrier`/`future` completions likewise.
3. **Happens-before**: the transitive closure of sequenced-before and synchronizes-with (plus a few inter-thread rules). If a write W happens-before a read R and no other write to that location intervenes, R *must* see W's value. If neither of two conflicting non-atomic accesses happens-before the other → **data race → UB**.
4. **Modification order**: every atomic object has a single total order of all writes to it that all threads agree on (*coherence*). This is why a `relaxed` counter still ends up exactly right: every `fetch_add` sees the previous one.
5. **Sequential consistency** (`seq_cst`): additionally, there is a single total order S of *all* `seq_cst` operations, consistent with happens-before — the "interleaving" model programmers assume.

The three levels you choose between:

| `memory_order` | Guarantees | Cost on arm64 | Cost on x86-64 |
|---|---|---|---|
| `relaxed` | Atomicity + coherence of *this object* only. No ordering with other memory. | plain `ldr`/`str`; RMW is `ldxr/stxr` or `ldadd` | plain `mov`; RMW is `lock xadd` |
| `acquire` (loads) / `release` (stores) / `acq_rel` (RMW) | Release store + acquire load that reads it ⇒ everything sequenced-before the store is visible after the load. Pairwise, between the two threads involved. | `ldar` / `stlr` (~free on M-series) | free: x86 loads are acquire, stores are release (TSO) |
| `seq_cst` | acq/rel **plus** one global order all threads agree on | `ldar`/`stlr` + `dmb ish` on store or full barriers | `mfence`/`xchg` on stores: ~20–40 cycles |

Three textbook litmus tests, with what each order guarantees:

**Message passing (MP)** — the pattern behind every lock, queue and "ready flag":

```cpp
int payload = 0; std::atomic<bool> flag{false};
// producer                                        // consumer
payload = 42;                                      while (!flag.load(std::memory_order_acquire)) {}
flag.store(true, std::memory_order_release);       assert(payload == 42);   // guaranteed
```
The release store synchronizes-with the acquire load that reads `true`; `payload = 42` is sequenced-before the store; so it happens-before the `assert`. With `relaxed` on both sides the assert can fail — the consumer may see `flag == true` and `payload == 0`. This *does* happen on ARM (stores may become visible out of order) and *never* happens on x86 (TSO forbids store-store reordering), which is why "it works on my Intel laptop" is worthless evidence. example.cpp §5 runs the acquire/release version 2000 times: 2000/2000.

**Store buffering (SB)** — why `acquire`/`release` is not enough for mutual exclusion:

```cpp
std::atomic<int> x{0}, y{0}; int r1, r2;
// thread 1                       // thread 2
x.store(1, release);              y.store(1, release);
r1 = y.load(acquire);             r2 = x.load(acquire);
// Can r1 == 0 && r2 == 0?  With acq/rel: YES (each thread's store may still be in its store buffer
// when the other loads). With seq_cst on all four: NO — some operation is first in S, and the load
// after it must see the store before it.
```

**Dekker's / Peterson's algorithm** is store buffering used as a lock: "I set my flag, then check yours". It is correct *only* under `seq_cst` (or with a `seq_cst` fence between the store and the load). This is the one common case where `seq_cst` is required rather than merely convenient — and why the default is `seq_cst`: correctness first, then relax where measured.

**Fences**: `std::atomic_thread_fence(order)` is ordering without a specific atomic. A release fence followed (later in the same thread) by *any* atomic store acts like a release store; an acquire fence preceded by an atomic load acts like an acquire load. Use: publish several relaxed stores with one fence; or a release fence + relaxed store to avoid the `stlr` cost in a loop. `std::atomic_signal_fence` orders only against a signal handler on the same thread (a compiler barrier). Fences are rarer than you think in application code; the SPSC queue in §11 doesn't need one.

**Why data races are UB and not just "nondeterministic"**: the compiler is allowed to assume no other thread modifies non-atomic memory between two of your accesses. `while (!done) {}` on a plain `bool` becomes `if (!done) for (;;) {}`. A 64-bit store on a 32-bit machine is two stores. A `bool` write may be implemented as a byte-wide read-modify-write of the containing word on some architectures, clobbering a neighbour. Benign races don't exist in C++; `volatile` is not a fix (it prevents *elision*, not reordering or tearing, and it is for hardware registers). Use `std::atomic` or a lock.

Python equivalent: none exposed — the GIL and reference-counting make every Python-level operation effectively `seq_cst`. NumPy releases the GIL inside C loops, which is why NumPy code *can* race on shared arrays across threads.

---

## 6. Which `memory_order` to use

- **Default `seq_cst`** until profiling shows an atomic on the hot path. On x86 it's free for loads; on ARM it costs a barrier per store.
- **`release`/`acquire`** for every "publish data, then set flag" / "check flag, then read data" pair: mutexes, queues, ready flags, reference counts on *decrement* (the last decrementer must see all other threads' writes before deleting — `shared_ptr` uses `acq_rel` on the decrement).
- **`relaxed`** only when the *value of that atomic alone* is what matters: statistics counters, `shared_ptr` reference-count *increments*, a "seen" flag that gates nothing, the producer reading its own index in an SPSC queue.
- **`consume`** exists on paper; every compiler promotes it to `acquire`. Don't.

Rule from Herb Sutter's *atomic<> Weapons*: if you can't write the proof (which release synchronizes with which acquire, and which happens-before edge you rely on) in a comment next to the code, use `seq_cst`.

---

## 7. C++20 primitives: `latch`, `barrier`, `counting_semaphore`, `atomic::wait`

| Primitive | Semantics | Reusable | Typical use |
|---|---|---|---|
| `std::latch(n)` | `count_down()` decrements; `wait()` blocks until zero | No | "Start when all N workers are ready"; "join" N tasks without threads |
| `std::barrier(n, completion)` | All `n` threads `arrive_and_wait()` per phase; `completion` runs once per phase by the last arriver before releasing anyone | Yes | Time-stepping simulations: one phase = one step; the completion swaps buffers |
| `std::counting_semaphore<Max>(k)` | `acquire()` blocks while count is 0; `release()` increments | Yes | Limit concurrency to k (GPU streams, file handles); producer/consumer signalling |
| `std::binary_semaphore` | `counting_semaphore<1>` | Yes | Hand-off between two threads |
| `atomic<T>::wait(old)` / `notify_one/all()` | Block until value ≠ old (futex/`__ulock_wait`) | — | Build your own primitives without spinning |

```cpp
int phase = 0;
std::barrier sync(HW, [&]() noexcept { ++phase; });          // completion: runs exactly once per phase
std::vector<std::jthread> ts;
for (unsigned t = 0; t < HW; ++t)
    ts.emplace_back([&, t] { for (int s = 0; s < 5; ++s) { work(t, phase); sync.arrive_and_wait(); } });
```

The standard guarantees that the completion step *strongly happens before* the return of every `arrive_and_wait` in that phase, so the workers' reads of `phase` after the barrier are race-free — even though ThreadSanitizer says otherwise on macOS (§17). `arrive_and_drop()` lets a thread leave the barrier permanently (shrinking the count) — for work that finishes early. A barrier costs 10–50 µs per phase with 10 threads (a futex wake per thread), so a time step must do a few hundred µs of work to amortize it (§18).

Semaphores are the primitive from which Dijkstra built everything; in modern code they are for *counting a resource*. A binary semaphore is not a mutex: it has no owner, so any thread may `release()` — that is the feature (hand-off) and the danger.

---

## 8. Futures: `std::async`, `std::promise`, `std::packaged_task`

A **future** is a handle to a value that will exist later; `get()` blocks until it does, and re-throws any exception the producer stored. Three ways to create one:

```cpp
auto f1 = std::async(std::launch::async, slow_norm, std::cref(v));    // new thread; ~future JOINS it
auto f2 = std::async(std::launch::deferred, [] { return 1 + 1; });    // runs lazily, on the thread that calls get()
auto f3 = std::async(slow_norm, std::cref(v));                        // policy unspecified: EITHER of the above — avoid

std::promise<std::string> p; std::future<std::string> fut = p.get_future();
std::jthread t([&p] { p.set_value("done"); });                        // or p.set_exception(std::current_exception())
fut.get();                                                            // "done"

std::packaged_task<int(int, int)> task([](int a, int b) { return a * b; });
auto fres = task.get_future(); task(6, 7); fres.get();               // 42 — the thread-pool building block
```

Gotchas: `std::async`'s future is the *only* future whose destructor blocks (it joins the thread), so `std::async(std::launch::async, f);` without keeping the future runs *synchronously*. Without an explicit policy, the implementation may choose `deferred` and never run your work in parallel — always pass `std::launch::async`. `std::async` creates one OS thread per call: fine for a handful of tasks, wrong for 10 000 (use a pool). A `std::future` is one-shot and move-only; `std::shared_future` can be waited on by many. `wait_for(0s) == std::future_status::ready` is the non-blocking poll. There is no `.then()` continuation in the standard (P2300 executors/`std::execution` arrives in C++26); libraries: Folly futures, `stdexec`, `Asio`.

Python equivalent: `concurrent.futures.Future`; `std::async(launch::async, f)` ≈ `ThreadPoolExecutor().submit(f)`; `packaged_task` is what the executor wraps your callable in.

---

## 9. A thread pool with a work queue and futures

The standard has no pool (until C++26's `std::execution`). Sixty lines give you a correct one — example.cpp §8:

```cpp
class ThreadPool {
    std::queue<std::function<void()>> tasks_;
    std::mutex m_;
    std::condition_variable_any cv_;               // _any: waitable with a stop_token
    std::vector<std::jthread> workers_;            // LAST member: destroyed (joined) FIRST
public:
    explicit ThreadPool(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this](std::stop_token st) {
                for (;;) {
                    std::function<void()> job;
                    { std::unique_lock lk(m_);
                      if (!cv_.wait(lk, st, [&] { return !tasks_.empty(); })) return;   // false ⇔ stop && empty: drains first
                      job = std::move(tasks_.front()); tasks_.pop(); }
                    job();
                }
            });
    }
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(          // shared_ptr: std::function needs a copyable callable
            [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable { return std::invoke(f, args...); });
        auto fut = task->get_future();
        { std::lock_guard lk(m_); tasks_.emplace([task] { (*task)(); }); }
        cv_.notify_one();
        return fut;
    }
    ~ThreadPool() { for (auto& w : workers_) w.request_stop(); cv_.notify_all(); }    // jthreads join after this body
};
```

The member order is load-bearing and the first draft of the example got it wrong: with `workers_` declared first, `~ThreadPool` destroyed the mutex and condition variable *before* the worker threads (destroyed last) were joined, giving "mutex lock failed: Invalid argument" one run in three and a ThreadSanitizer report every run. Members are destroyed in reverse declaration order; anything a thread touches must outlive the thread.

Design points: one shared queue + one condition variable is fine up to ~10⁵ tasks/s; beyond that, per-worker queues with work stealing (what Intel TBB, Rust's Rayon and Go's scheduler do) avoid the single lock. Tasks should be ≥ 10 µs of work to amortize the ~1 µs of queue + future overhead. Exceptions thrown inside a task land in the future; a task that blocks on another task's future can deadlock a pool with fewer workers than the dependency depth — fork-join (§14) needs "help while waiting" or enough threads.

---

## 10. Parallel STL and `std::execution::par`

C++17 added execution policies to ~70 algorithms: `std::sort(std::execution::par, v.begin(), v.end())`, `std::transform`, `std::reduce`, `std::transform_reduce` (the parallel dot product), `std::for_each`, `std::inclusive_scan`. Policies: `seq` (serial), `par` (threads), `par_unseq` (threads + vectorization; your callable must not lock or allocate), `unseq` (C++20, vectorize only).

**Status on this machine**: libc++ ships its PSTL as *experimental*. The plain `-std=c++20` build has `<execution>` but no `std::execution::par` (`__cpp_lib_parallel_algorithm` is undefined). With `-fexperimental-library` the policies appear and dispatch to libdispatch (Grand Central Dispatch) — example.cpp's `HAVE_PSTL` section works with `c++ -std=c++20 -fexperimental-library`. The linker warns that `libc++experimental.a` was built for a newer macOS; it links and runs. On Linux, libstdc++'s PSTL requires Intel TBB (`-ltbb`) and is production quality; MSVC's is built in.

Alternatives when you need portable, dependable parallel algorithms today: OpenMP (§11; simplest for loops), Intel oneTBB (`tbb::parallel_for`, `parallel_reduce`, concurrent containers, work-stealing scheduler — `brew install tbb`), or your own pool + `parallel_for(lo, hi, f)` helper (30 lines, Chapter 12 §15). For numerics, OpenMP wins on simplicity; for irregular task graphs (tree builds, sparse solvers), TBB.

---

## 11. OpenMP from C++

OpenMP is a compiler-directive layer over threads: you annotate loops, the compiler outlines the body into a function and runs it on a persistent thread team. Apple clang understands the pragmas but ships no runtime; install one:

```sh
brew install libomp
c++ -Wall -Wextra -std=c++20 -O2 -Xpreprocessor -fopenmp \
    -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp -o prog prog.cpp
OMP_NUM_THREADS=8 ./prog
# Linux (GCC): g++ -fopenmp prog.cpp ; clang: clang++ -fopenmp prog.cpp -lomp
```

Always guard with `#ifdef _OPENMP` (defined only when `-fopenmp` is active) so the plain build compiles and runs serially — pragmas the compiler doesn't recognize are ignored, but `#include <omp.h>` and `omp_get_num_threads()` are not.

```cpp
#ifdef _OPENMP
#include <omp.h>
#endif
double s = 0;
#pragma omp parallel for reduction(+ : s) schedule(static)
for (std::ptrdiff_t i = 0; i < n; ++i) s += x[i] * x[i];          // each thread a private s, combined at the end

#pragma omp parallel for collapse(2) schedule(dynamic, 64)         // flatten i,j into one iteration space; chunks of 64
for (int i = 0; i < R; ++i) for (int j = 0; j < C; ++j) grid[i * C + j] = f(i, j);
```

| Clause | Meaning | When |
|---|---|---|
| `parallel for` | Split the following loop across the team | Loop with independent iterations, ≥ ~10 µs total per thread |
| `reduction(op : var)` | Private copy per thread, combined with `op` at the end (`+ * min max & \| ^ && \|\|`; user-defined via `declare reduction`) | Any accumulation; forgetting it is a data race |
| `schedule(static)` | Contiguous equal chunks, decided at compile/launch time | Uniform work (stencils, dot products); best locality |
| `schedule(dynamic, k)` | Threads grab `k` iterations at a time | Irregular work (Barnes–Hut force per particle, adaptive time steps) |
| `schedule(guided)` | Dynamic with shrinking chunks | Compromise |
| `collapse(k)` | Merge `k` perfectly nested loops | Outer loop too short to balance (e.g. 8 rows × 4096 cols) |
| `private(x)` / `firstprivate(x)` / `shared(x)` | Storage class per variable | Loop-body temporaries are private automatically if declared inside the loop; declare them inside |
| `nowait`, `barrier`, `critical`, `atomic` | Sync control | `atomic` for a scatter-add; `critical` sparingly |
| `#pragma omp simd` | Vectorize this loop (no threads) | Inner loops the auto-vectorizer misses |
| `#pragma omp task` / `taskwait` | Task parallelism | Recursive algorithms (§14) |

The loop variable must be an integer type with a computable trip count (`std::ptrdiff_t` or `int`; `std::size_t` is accepted by modern compilers). Iterations must be independent, or the dependency must be expressed with `reduction`/`atomic`. The team is created once and reused across parallel regions; entering a region costs ~5–20 µs. `omp_get_wtime()` is a convenient timer. Environment: `OMP_NUM_THREADS`, `OMP_PROC_BIND=true` (pin threads; matters on NUMA Linux boxes, not on a single M-series die), `OMP_SCHEDULE=dynamic,32` when `schedule(runtime)` is used.

---

## 12. Task-based parallelism: fork-join for recursive algorithms

Loops parallelize by index; trees and divide-and-conquer parallelize by *task*: fork the two halves, join, combine. Mergesort:

```cpp
void psort(std::span<double> a, int depth = 0) {
    if (a.size() < 4096) { std::sort(a.begin(), a.end()); return; }        // sequential cutoff: tasks must be worth a fork
    auto mid = a.size() / 2;
    if (depth < 4) {                                                        // 2^4 = 16 tasks for 10 cores; deeper is pure overhead
        auto f = std::async(std::launch::async, [&] { psort(a.subspan(0, mid), depth + 1); });
        psort(a.subspan(mid), depth + 1);
        f.get();
    } else { psort(a.subspan(0, mid), depth + 1); psort(a.subspan(mid), depth + 1); }
    std::inplace_merge(a.begin(), a.begin() + mid, a.end());
}
// OpenMP version:  #pragma omp task shared(a) { psort(left); }  psort(right);  #pragma omp taskwait
```

Two parameters govern every fork-join code: the **sequential cutoff** (below which forking costs more than it saves — 1–10 µs of work is the floor, so ~1000–10 000 elements) and the **fork depth** (≈ log₂(cores) + 1–2 for balance). The Barnes–Hut tree build (P07) is the same shape: partition particles into 8 octants, fork per octant down to depth 2–3, build the subtrees independently (each subtree is written by exactly one thread — no locks), then link. Force evaluation is then a `parallel for` over particles with `schedule(dynamic)` because far-from-center particles are cheaper. With `std::async` the depth limit is mandatory (one OS thread per fork); with OpenMP tasks or TBB the runtime steals work and the depth limit is just for overhead.

---

## 13. Lock-free structures: the SPSC ring buffer

Lock-free means: no thread blocks another indefinitely; progress is guaranteed for *some* thread. The simplest useful lock-free structure — and one of the few you should write yourself — is the **single-producer single-consumer ring buffer**: audio callbacks, a simulation thread feeding a renderer, a sampler thread feeding a logger. Full code in example.cpp §9:

```cpp
template <class T, std::size_t N> class SpscRing {                    // N a power of two
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head_{0};   // written by consumer only
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail_{0};   // written by producer only
    alignas(std::hardware_destructive_interference_size) T buf_[N];
public:
    bool try_push(const T& v) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);          // (1) my own index: relaxed
        const std::size_t next = (t + 1) & (N - 1);
        if (next == head_.load(std::memory_order_acquire)) return false;       // (2) full? acquire pairs with (6)
        buf_[t] = v;                                                           // (3) write the slot
        tail_.store(next, std::memory_order_release);                          // (4) publish: pairs with (5)
        return true;
    }
    bool try_pop(T& out) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;          // (5) empty? sees (4) ⇒ sees (3)
        out = buf_[h];                                                         // read the slot
        head_.store((h + 1) & (N - 1), std::memory_order_release);             // (6) free the slot: pairs with (2)
        return true;
    }
};
```

The proof, in memory-model terms: (3) is sequenced-before (4); (4) synchronizes-with (5) when the consumer reads the new tail; so (3) happens-before the consumer's read of `buf_[h]` — it sees the value, not garbage. Symmetrically, the consumer's read of the slot is sequenced-before (6), which synchronizes-with (2) when the producer sees the new head; so the producer's next write to that slot happens-after the read — no overwrite of unread data. Each index has exactly one writer, so `relaxed` is enough for a thread reading its *own* index. One slot is kept empty to distinguish full from empty; the power-of-two size makes the wrap a mask. `head_` and `tail_` are on separate cache lines because each is written by a different core (§15). On this machine it moves 13 M `uint64_t`/s with both threads spinning; batching (publish every k items) reaches 100 M+.

**MPMC caveats**: add a second producer and (1)–(4) break — two producers read the same `tail_`, both write slot `t`, both publish. The fix needs a CAS on the index *and* per-slot sequence numbers so consumers can tell a claimed-but-unwritten slot from a written one (Dmitry Vyukov's bounded MPMC queue), and unbounded versions need safe memory reclamation (hazard pointers, epochs — the hard problem of lock-free programming; the ABA problem is its most famous symptom). Don't write these; use `moodycamel::ConcurrentQueue`, `boost::lockfree::queue`, or a mutex + `std::deque`, which is faster than a bad lock-free queue and correct.

---

## 14. False sharing and `std::hardware_destructive_interference_size`

Cores don't share bytes; they share **cache lines**. When two cores write different variables that sit on the same line, the line ping-pongs between their caches (MESI invalidations) on every write — each write costs a ~40–100 ns coherence round trip instead of ~1 ns. Nothing is wrong logically; the code is just 10–100× slower.

```cpp
struct Unpadded { std::atomic<long> v{0}; };                                          // 8 B: 8-32 per line → shared
struct Padded   { alignas(std::hardware_destructive_interference_size) std::atomic<long> v{0}; };  // one per line
// example.cpp §10, 10 threads x 2M increments each into slots[t]:
//   unpadded 397 ms, padded 4.5 ms  (89x)
```

`std::hardware_destructive_interference_size` (`<new>`, C++17) is the padding that avoids false sharing; `hardware_constructive_interference_size` is the size that keeps things together. libc++ on arm64 reports **256** (Apple Silicon's L2 uses 128-byte lines and the adjacent-line prefetcher effectively pairs them); x86-64 reports 64. Where it bites in your projects: per-thread accumulators in a `std::vector<double> partial(nthreads)` (adjacent doubles — pad, or accumulate in a local and write once at the end, as §18 does); a `std::vector<std::atomic<int>>` histogram; an array of per-thread RNG states; OpenMP `reduction` handles its own privates correctly, but a hand-rolled `partial[omp_get_thread_num()] += x` in the loop does not.

---

## 15. Thread-safe lazy initialization and thread-local storage

**Static local variables** are initialized exactly once, thread-safely, since C++11 ([stmt.dcl]/4 — "magic statics"): the first thread to reach the declaration runs the initializer; others block until it finishes. That's a correct, lock-free-on-the-fast-path singleton or lookup table:

```cpp
const std::vector<double>& exp_table() {
    static const std::vector<double> t = [] { std::vector<double> v(256); /* fill */ return v; }();
    return t;                                            // after init: one guard-byte check per call
}
```

**`std::call_once(flag, f)`** with a `std::once_flag` is the same guarantee for code that isn't a variable initializer (initializing a library, opening a device). The double-checked locking pattern you'll see in old code is broken without atomics and unnecessary with them — use one of these two.

**`thread_local`** gives each thread its own instance of a variable (`errno` is the classic). Uses: a per-thread RNG (`thread_local std::mt19937_64 rng{seed ^ thread_id}` — a shared `std::mt19937` is a data race and a serial bottleneck), per-thread scratch buffers (avoid allocating in the hot loop *and* avoid a lock), per-thread accumulators for a scatter-add. Costs: on macOS/Linux a `thread_local` with a non-trivial constructor is initialized lazily on first access (guard check per access; `constinit` from Chapter 14 makes it static-initialized); destructors run at thread exit; there is one copy per thread, so 10 threads × 1 MB scratch = 10 MB. A `thread_local` inside a lambda in a pool task belongs to the *worker*, not the task — it persists across tasks.

---

## 16. `std::stop_token` in depth

The stop machinery (`<stop_token>`): `stop_source` owns the state and issues `request_stop()`; `stop_token` copies observe it (`stop_requested()`, `stop_possible()`); `stop_callback<F>` registers a callable that runs *synchronously inside* `request_stop()` on the requesting thread — or immediately at registration if stop was already requested. Patterns:

- A worker in a loop: check `st.stop_requested()` once per iteration (an atomic load, ~1 ns).
- A worker blocked in a condition variable: use `std::condition_variable_any::wait(lock, st, pred)`, which registers a `stop_callback` that notifies the CV.
- A worker blocked in a *non*-stop-aware call (a socket read, `std::this_thread::sleep_for`): register a `stop_callback` that closes the socket / sets an atomic and `notify`s, or poll with a timeout.
- Fan-out cancellation: one `stop_source`, its token passed to all N workers; `request_stop()` cancels all.

A training loop that checks `st.stop_requested()` once per batch, saves a checkpoint, and returns is how you make Ctrl-C (a signal handler setting an atomic that the main thread turns into `request_stop()`) safe.

---

## 17. ThreadSanitizer on macOS

TSan (`-fsanitize=thread`) instruments every memory access and every synchronization operation, tracks happens-before with vector clocks, and reports any pair of conflicting accesses not ordered by it — a *dynamic* proof that the executed paths are race-free, with no false negatives on executed paths and very few false positives.

```sh
c++ -Wall -Wextra -std=c++20 -O1 -g -pthread -fsanitize=thread -o prog prog.cpp && ./prog
# -O1 -g: readable stacks, tolerable speed (5-15x slower, 5-10x memory). Linux identical; GCC also supports it.
# Cannot be combined with ASan; UBSan + TSan is fine.
RACE_DEMO=1 ./ex_demo     # example.cpp's deliberate race: two threads ++ a plain long
```

The report gives two stacks — the racing write/read and the *previous* conflicting access — plus where each thread was created and what locks were held. Read the top frame of each stack that is in your file. `TSAN_OPTIONS=halt_on_error=1` stops at the first report; `second_deadlock_stack=1` for lock-order inversions (TSan also detects potential deadlocks); `history_size=7` when it says the previous access's stack is unavailable.

Two **false-positive classes on macOS/libc++**, both present in example.cpp on purpose:

1. `std::atomic_thread_fence` — TSan does not model standalone fences (documented limitation); the fence-based message-passing variant in §5 is reported. Rewrite with release/acquire *operations*, or suppress.
2. `std::barrier` — libc++ compiles the barrier's arrival algorithm into `libc++.dylib`, which is not instrumented, so the happens-before edge from a non-last arriver to the completion function and to the other waiters is invisible. Data ordered only by a barrier (§7's `phase`, §18's grid halos) is reported. `std::latch` (header-only) is fine.

Suppressions file (`race:<function-substring>` per line), applied with `TSAN_OPTIONS=suppressions=tsan.supp ./prog`: example.cpp runs clean with `race:s5::run`, `race:s6::run`, `race:s13::`. Everything else in the file — including the SPSC queue's acquire/release protocol — TSan verifies. It also caught the real bug in the thread pool (§9) before it was documented here.

Related tools: `-fsanitize=address` for memory errors in threaded code (works on Apple Silicon; LeakSanitizer does not — use `leaks --atExit -- ./prog`); Instruments' *System Trace* to see threads sleeping vs running; `sample` / `xcrun xctrace record --template 'Time Profiler'` to find where parallel time goes.

---

## 18. Designing parallel numerics

**Domain decomposition for grids** (FDTD, heat/wave equation, LBM): split the grid into contiguous blocks of *rows* (row-major → each thread's block is contiguous memory); each thread updates its block from the previous step's values, reading one row of **halo** from each neighbour. Because the halo was written in the *previous* step, the only synchronization is a barrier between steps — no locks in the update loop. example.cpp §13 does it for the 1-D heat equation with `std::barrier` and a completion function that swaps the buffers; the result is bit-identical to serial because each cell is computed by exactly one thread with the same operations in the same order.

```
step s:   thread 0 | thread 1 | thread 2 | thread 3      each writes its own rows of u_new
          barrier (completion: swap(u, u_new))
step s+1: each reads its rows + 1 halo row from each neighbour — written before the barrier
```

The 1-D example scales poorly (1.2× at 10 threads) because a step is ~0.1 ms of work and a barrier is ~30 µs: **granularity**. Fixes: bigger problem (2-D 2048² is 4 M cells, ~4 ms/step → near-linear scaling), or fewer barriers (compute *k* steps per barrier with a halo *k* cells wide — the "temporal blocking" trick, trading redundant halo computation for synchronization). For FDTD Maxwell (P08) the E and H updates are two half-steps → two barriers per step; same decomposition. For 2-D/3-D decomposition (blocks instead of slabs) halos are on 4/6 sides and the surface-to-volume ratio is better — that's what you do across *nodes* with MPI; within one node, slabs are usually enough.

**Per-thread accumulators for N-body / PIC**: the *gather* direction (each particle sums forces from all others, or from the tree) is a `parallel for` over particles with private locals — no race, because each thread writes only its own particles' accelerations. The *scatter* direction (deposit each particle's charge onto grid cells in PIC; histogram; gradient accumulation into a shared weight tensor) races on the target. Options, fastest first for typical sizes: (a) **per-thread copies of the target** + a reduction at the end — memory ×threads, but no contention; right for grids up to ~10⁶ cells and for gradient accumulation; (b) **sort particles by cell, then each thread owns a cell range** — no races, cache-friendly, and the sort is often needed anyway (LBM/PIC do it every few steps); (c) `std::atomic_ref<double>` + `fetch_add` on the target — correct but 10–50× slower under contention and nondeterministic; (d) a mutex per stripe of cells — rarely better than (a).

**Deterministic reductions**: floating-point addition is not associative, so `sum` over threads in nondeterministic order gives results that differ in the last bits between runs — which makes bugs irreproducible and tests flaky. Fix: fixed chunking (thread `t` always sums indices `[t·chunk, (t+1)·chunk)`), each thread into a *local* variable (no false sharing), one write to `partial[t]`, then combine in fixed order after joining. example.cpp §12 asserts two runs are bit-identical. OpenMP's `reduction` combines in an unspecified order — use the manual version when you need reproducibility. Pairwise/tree summation within each chunk additionally reduces the rounding error from O(n·ε) to O(log n·ε); Kahan summation (C course ch. 12) is the heavier alternative.

**Load balance**: static chunks for uniform work; `schedule(dynamic)` or a shared atomic counter (`next = counter.fetch_add(chunk)`) for irregular work such as Barnes–Hut force evaluation, adaptive-mesh cells, or variable-length sequences in an ML batch. On Apple Silicon, dynamic scheduling also absorbs the P-core/E-core speed difference (~2×).

---

## 19. GPU offloading: what changes

A GPU (Metal on this machine; CUDA on NVIDIA; SYCL/HIP elsewhere) runs the *same* algorithm shapes — `parallel for` over cells or particles, reductions, scatters — with three differences that invert some of this chapter's advice:

| | CPU threads (this chapter) | GPU |
|---|---|---|
| Parallelism | 10 threads, each fast and independent | 10⁴–10⁵ threads in lock-step groups of 32 (warps / SIMD-groups); a thread is cheap, a branch divergence is not |
| Memory | Shared cache-coherent memory; false sharing is the hazard | Separate device memory (unified on Apple Silicon, but still a copy or a mapping); the hazard is *uncoalesced* access — adjacent threads must read adjacent addresses, i.e. SoA is mandatory |
| Synchronization | Mutexes, CVs, barriers across all threads | Barriers only *within* a work-group (`threadgroup_barrier`, `__syncthreads`); global sync = end the kernel; atomics are cheap for integers, expensive for floats |
| Reduction | Per-thread partial, combine | Tree reduction in shared/threadgroup memory, then across groups with a second kernel or atomics |
| Cost model | Amortize barriers and locks | Amortize *kernel launches* (~10 µs) and host↔device copies (~10 GB/s PCIe; unified on M-series) |

The mental model transfers: a Metal compute kernel is the loop body of your `parallel for`; a threadgroup is a team that shares an L1-sized scratchpad; `std::barrier` between steps becomes "launch the next kernel". Domain decomposition, SoA layout, deterministic reductions and per-thread accumulators are all the *same* decisions. Learn them here first, where the debugger and TSan work.

---

## 20. Measuring parallel code

- **Speedup** S(p) = T(1) / T(p); **efficiency** E(p) = S(p) / p. Report both; efficiency exposes the overhead that speedup hides. T(1) must be the best *serial* code, not the parallel code on one thread (which carries thread overhead — example.cpp §15 uses the 1-thread parallel version as the baseline and shows 101% at p=1 as a result; that's the honest caveat).
- **Strong scaling**: fixed problem size, increasing p — how fast can I solve *this* problem? Limited by Amdahl (Chapter 12 §19): serial fraction s ⇒ S ≤ 1/s. **Weak scaling**: problem size grows with p (constant work per thread) — how big a problem can I solve in the same time? Gustafson's law; this is the regime of large simulations, and it scales much better because the serial fraction shrinks with problem size.
- **Memory-bound kernels** stop scaling when they saturate bandwidth, not when they run out of cores: the dot product in example.cpp §12 goes from 8.1 ms to 2.1 ms (3.8×, 38% efficiency) at 10 threads because it reaches ~125 GB/s, near this chip's limit. Compute the arithmetic intensity (flops per byte) before expecting linear scaling; the *roofline model* is the one-page version of this idea.
- **Heterogeneous cores**: on Apple Silicon a 10-thread run uses 4 P-cores and 6 E-cores (~half speed); efficiency naturally drops past 4 threads even for perfectly parallel work. Report per-thread-type numbers or use `schedule(dynamic)`.
- **Methodology**: same rules as Chapter 12 — warm up, repeat, take the minimum, keep the machine quiet, use the result. Add: pin nothing on macOS (no `sched_setaffinity`; QoS classes exist instead), check `OMP_NUM_THREADS` and `hardware_concurrency()`, and measure with `steady_clock` around the *whole* parallel region including thread start-up and join.

---

## Gotchas and undefined behavior

- **Any data race is UB.** Plain `bool done` as a stop flag, `++counter` from two threads, reading a `double` another thread writes, `std::vector::push_back` from two threads, two threads writing different *bits* of one `int`. `volatile` doesn't fix it; `std::atomic` or a mutex does.
- **`std::thread` destroyed while joinable → `std::terminate`.** Including when an exception skips the `join()`. Use `jthread`.
- **Capturing by reference in a detached or outliving thread** dangles when the scope ends. `jthread` in the same scope, or capture by value / `shared_ptr`.
- **Member/variable destruction order vs thread lifetime**: threads must be joined before anything they touch is destroyed. Declare the `jthread`s *last*. (The example's first draft had this bug; TSan found it.)
- **`condition_variable::wait` without a predicate** → hangs on lost wakeups, proceeds wrongly on spurious ones. Always the predicate form.
- **Notifying without modifying under the lock** (or checking the predicate outside it) is a race between the check and the sleep → missed wakeup.
- **Two `lock_guard`s on two mutexes** in different orders on different threads → deadlock. `scoped_lock(a, b)`.
- **Holding a lock while calling out** (callbacks, I/O, `std::cout`) → deadlock or a serial bottleneck.
- **`std::async` without `std::launch::async`** may run deferred — sequentially, at `get()`. The `async` future's destructor blocks; a temporary future serializes your "parallel" loop.
- **`atomic<double>` accumulation in a hot loop**: correct, ~100× slower, nondeterministic last bits.
- **`memory_order_relaxed` for a flag that guards data** — message passing breaks on ARM. Relaxed only when the atomic's own value is all that matters.
- **Store-buffering / Dekker with acquire/release** — both threads can read 0. Needs `seq_cst`.
- **Reading the wrong side of a CAS**: `compare_exchange_weak` *overwrites* `expected` on failure — a loop that doesn't expect that spins with stale data.
- **Mixing `atomic_ref` and plain access** to the same object during the `atomic_ref`'s lifetime is a race.
- **False sharing** silently costs 10–100×. Pad per-thread data with `alignas(std::hardware_destructive_interference_size)` (256 on arm64 libc++, 64 on x86) or accumulate in locals.
- **`thread_local` with a heavy constructor** runs it once per thread, lazily; in a pool it persists across tasks — clear it if tasks assume fresh state.
- **Signed loop variable required** by older OpenMP; missing `reduction` = race = wrong answer that changes each run; `#pragma omp parallel` (without `for`) runs the *whole* block on every thread.
- **A pool task that waits on another pool task's future** with more dependency depth than workers → deadlock.
- **Lock-free ≠ wait-free ≠ fast.** A CAS loop under heavy contention can be slower than a mutex; an SPSC queue used with two producers is UB.
- **TSan false positives** on libc++/macOS: `atomic_thread_fence` and `std::barrier`. Everything else it reports is real.

---

## Common mistakes checklist

- [ ] Every shared mutable variable is either `std::atomic`, protected by exactly one named mutex, or provably owned by one thread at a time (document which).
- [ ] `std::jthread` everywhere; `jthread` members declared last; no `detach()`.
- [ ] `scoped_lock` for multiple mutexes; critical sections contain no I/O or callbacks.
- [ ] Every `condition_variable::wait` has a predicate that reads only mutex-protected state; producers modify under the lock, then notify.
- [ ] Every release/acquire pair is commented with what it publishes; `relaxed` only for standalone counters and own-index reads; `seq_cst` when unsure.
- [ ] No `atomic<double>` in a hot reduction; per-thread locals → `partial[t]` → fixed-order combine; determinism asserted by running twice.
- [ ] Per-thread data padded or written once; `hardware_destructive_interference_size` used, not a hard-coded 64.
- [ ] Tasks ≥ 10 µs; fork depth ≈ log₂(cores)+1; sequential cutoff present in every recursive parallel algorithm.
- [ ] OpenMP guarded by `#ifdef _OPENMP`; every accumulator in a `reduction`; `schedule(dynamic)` for irregular work.
- [ ] `std::async` always with `std::launch::async` and its future kept.
- [ ] TSan run on every threaded program before trusting it, with the two libc++ false positives suppressed and understood.
- [ ] Speedup *and* efficiency reported, against the best serial code, with thread start-up included; memory-bound kernels identified by GB/s.

---

## You can move on when...

- You can define happens-before, synchronizes-with and modification order without notes, and use them to prove the SPSC ring buffer correct — and to explain why two producers break it.
- You can state what message passing, store buffering and Dekker each need (`acq/rel`, `seq_cst`, `seq_cst`) and why x86 hides the first bug.
- You can write a bounded queue with two condition variables and the predicate loop, and explain lost and spurious wakeups.
- You can write a thread pool returning futures from memory, and explain the member-order bug.
- You can measure false sharing, know the interference size on your machine, and fix a per-thread accumulator array two ways.
- You can add OpenMP to a stencil and a Barnes–Hut force loop with the right `schedule`, and build it on macOS with libomp.
- You can run ThreadSanitizer, read its two stacks, and name its two false-positive classes on libc++.
- You can design the parallelization of a 2-D FDTD step (decomposition, halos, barriers per step, granularity) and of a PIC charge deposit (per-thread grids vs sort-by-cell vs atomics), and justify the choice with numbers.
- You can produce a strong-scaling table with speedup and efficiency, and explain the shape of the curve for a memory-bound kernel on a P/E-core chip.
