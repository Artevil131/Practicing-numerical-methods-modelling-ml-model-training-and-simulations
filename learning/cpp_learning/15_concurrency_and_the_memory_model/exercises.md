# Chapter 15 — Exercises

Write each exercise as `ex15_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++20 -O2 -pthread -o ex15_K ex15_K.cpp`. Before you consider any
exercise finished, also build and run it under ThreadSanitizer:
`c++ -Wall -Wextra -std=c++20 -O1 -g -pthread -fsanitize=thread -o ex15_K ex15_K.cpp && ./ex15_K`
— a clean TSan run (modulo the two libc++ false positives from lesson §17, which you must
identify and suppress explicitly, not ignore) is part of the deliverable. For timing exercises follow
Chapter 12's rules (warm-up, ≥5 repeats, minimum, use the result) and record your numbers in a
comment at the top of the file. Exercises that mention OpenMP must compile *and run correctly*
without it (`#ifdef _OPENMP`).

---

**15.1 — `jthread`, `stop_token`, and the lifetime bugs**
Write a `Ticker` that runs a `std::jthread` printing a counter every 10 ms until stopped, using
`std::stop_token`. Then reproduce and fix, one at a time, each of these bugs — record the symptom
(hang, `terminate`, TSan report, garbage) in a comment: (a) a `std::thread` that isn't joined when
an exception is thrown between construction and `join()`; (b) a lambda capturing a local by
reference in a thread that outlives the scope; (c) a class with a `std::jthread` member declared
*before* the `std::mutex` it uses; (d) a `stop_token` checked only once before a blocking
`sleep_for(10s)` — fix it with `std::stop_callback` or `condition_variable_any::wait(lock, st, pred)`
so stop takes effect within 1 ms.

Example: `(c) TSan: data race ... std::mutex::lock ... ~Ticker` → fixed by reordering members.

<details><summary>Hint</summary>
For (a) put a `throw` between `std::thread t(...)` and `t.join()` inside a `try`. For (d) a `std::condition_variable_any` with `wait_for(lk, st, 10s, [] { return false; })` returns as soon as stop is requested.
</details>

---

**15.2 — Bank transfers: deadlock, then `scoped_lock`, then lock ordering**
Model 8 `Account`s each with its own `std::mutex`. Run 8 threads doing 100 000 random transfers each. Version 1: lock `from.m` then `to.m` with two `lock_guard`s — observe the deadlock (add a watchdog thread that prints "deadlocked" after 2 s and `std::abort`s). Version 2: `std::scoped_lock(from.m, to.m)`. Version 3: manual ordering by `&from < &to`. Assert the total money is conserved. Measure all three (versions 2 and 3 only, obviously) and also a version 4 with one global mutex; report ops/s for each and explain the ranking.

Example: `global mutex 3.1 M ops/s ; scoped_lock 9.8 M ops/s ; ordered 10.2 M ops/s`.

<details><summary>Hint</summary>
`std::scoped_lock` uses `std::lock`'s try-and-back-off; the address-ordering version acquires in a fixed order with two plain locks. Same-account transfers (`from == to`) must be skipped or use one lock — `scoped_lock(m, m)` is UB.
</details>

---

**15.3 — A bounded queue three ways, with shutdown**
Implement `BoundedQueue<T>` with `push`, `pop` returning `std::optional<T>`, and `close()`, using (a) `std::mutex` + two `std::condition_variable`s with predicate loops, (b) `std::counting_semaphore`s for slots/items plus a mutex for the deque, (c) the SPSC ring buffer from the lesson (single producer, single consumer only; `try_push`/`try_pop` with a yield-loop). Drive each with 1 producer / 1 consumer and (a, b) with 4 producers / 4 consumers moving 10⁷ `int`s; assert the sum. Report items/s for each configuration and capacities 1, 16, 1024. In a comment, explain why (a) deadlocks if you remove the predicate loop and how "closed" must interact with waiting consumers.

<details><summary>Hint</summary>
For (b): `counting_semaphore slots(cap), items(0)`; `push` = `slots.acquire(); {lock; push_back;} items.release();`. Shutdown with semaphores needs a poison-pill value or a `closed` flag checked after `items.acquire()` plus `items.release(n_consumers)` on close.
</details>

---

**15.4 — Litmus tests: see the memory model on ARM**
Write a harness that runs a two-thread litmus test N = 10⁶ times and histograms the outcomes: (a) **message passing** with `relaxed`/`relaxed`, then `release`/`acquire`; (b) **store buffering** with `release`/`acquire`, then `seq_cst`; (c) **Dekker's mutual exclusion** (two flags, "set mine, check yours, enter") with `acquire`/`release` and with `seq_cst`, counting how often both threads are in the critical section simultaneously. Use `std::barrier` (or a spinning atomic counter) to start both threads as simultaneously as possible each trial, and pad the two variables onto separate cache lines. Record the outcome counts for each variant. Then run the same binary under Rosetta if available (`arch -x86_64`) or on a Linux x86 box and compare.

Example: `MP relaxed: 1000000 ok, 213 payload==0 ; MP acq/rel: 1000000 ok, 0 bad` / `SB acq/rel: r1=r2=0 in 4127 trials ; SB seq_cst: 0`.

<details><summary>Hint</summary>
You may need many trials and a tight start (spin on an atomic "go" flag rather than a barrier) to see the relaxed anomalies; `std::this_thread::yield()` between trials helps the OS schedule the threads on different cores. Zero observed anomalies does not prove correctness; the memory model does.
</details>

---

**15.5 — Atomic toolbox: CAS loops, `atomic_ref`, `wait/notify`**
Implement with atomics only (no mutex): (a) `atomic_fetch_max(std::atomic<double>&, double)` and `atomic_fetch_mul` as CAS loops; (b) a `SpinLock` class with `std::atomic<bool>` (`test_and_set`-style via `exchange(true, acquire)`, `store(false, release)`), with a `try_lock` and a `std::atomic_flag` variant using `wait()`/`notify_one()` instead of spinning; (c) a histogram over `std::vector<int>` using `std::atomic_ref` from 8 threads; (d) a `OnceValue<T>` that lets many threads race to compute a value but publishes exactly one (CAS on a state enum + `wait`/`notify`). Measure `SpinLock` vs `std::mutex` for a 10-ns critical section at 1, 2, 8 threads, and explain when each wins. TSan-clean.

<details><summary>Hint</summary>
Spin with `load(relaxed)` before attempting `exchange` (test-and-test-and-set) to avoid hammering the cache line. `std::atomic_flag::wait(true)` blocks until the flag is cleared. For (d), states: `empty → computing → ready`; losers `wait(computing)`.
</details>

---

**15.6 — Thread pool with work stealing and fork-join**
Extend the lesson's `ThreadPool` into `WorkStealingPool`: one deque per worker, workers push/pop their own back (LIFO), steal from others' front (FIFO), guarded by a per-deque mutex (a lock-free Chase–Lev deque is optional extra credit). Add `template <class F> auto fork(F&&)` that returns a future and `wait(future&)` that *helps* by running other tasks while the future isn't ready, so recursive fork-join can't deadlock with a small pool. Implement parallel mergesort and a parallel Fibonacci(35) on it with sequential cutoffs; verify results, compare against `std::async`-based fork-join (depth-limited) and against `std::sort`. Report tasks/s and speedup with 1, 2, 4, 8 workers.

Example: `sort 1e7 doubles: std::sort 0.92 s ; pool(8) 0.19 s (4.8x) ; async depth 4 0.24 s`.

<details><summary>Hint</summary>
Helping: `while (fut.wait_for(0s) != ready) { if (auto job = try_pop_or_steal()) job(); else yield(); }`. Fibonacci with cutoff n < 20 is a pure overhead benchmark — that's its point.
</details>

---

**15.7 — False sharing and the interference size**
Build a per-thread accumulator experiment: `std::vector<Slot> slots(nthreads)` where each thread increments `slots[t].v` 10⁷ times. Slots: (a) `long`, (b) `std::atomic<long>`, (c) `alignas(64) long`, (d) `alignas(std::hardware_destructive_interference_size) long`, (e) a local variable written back once at the end. Run with 1, 2, 4, 8, 10 threads; table of ms and slowdown vs (e). Then write a `PaddedArray<T>` template that stores `n` elements each on its own interference-size stride and use it to fix a nondeterministic-looking slowdown in a `std::vector<std::mt19937_64>` per-thread RNG array (each thread draws 10⁷ numbers from its own generator). Print `std::hardware_destructive_interference_size` and `sizeof(std::mt19937_64)`.

Example: `8 threads: long 612 ms ; atomic 640 ms ; alignas(64) 71 ms ; alignas(256) 8 ms ; local 8 ms`.

<details><summary>Hint</summary>
On arm64 libc++ reports 256; try `alignas(128)` too and see whether 128-byte lines or the adjacent-line prefetcher decide. `std::mt19937_64` is ~2.5 KB, so RNGs don't false-share — check whether they do before "fixing" it; measure first.
</details>

---

**15.8 — Parallel matmul and deterministic gradient accumulation (ML)**
Take the tiled `ikj` matmul from Chapter 12 (`double`, n = 1024) and parallelize it three ways: (a) `std::jthread`s over row blocks with a deterministic partition, (b) your thread pool from 15.6 with 2-D tiles as tasks (`schedule(dynamic)`-style), (c) OpenMP `parallel for collapse(2)` over tiles (guarded). Assert all agree with the serial result *bit-for-bit* (they must — each `C(i,j)` is computed by one thread in the same order). Then implement mini-batch gradient accumulation for a linear layer: `grad_W = Σ_b x_bᵀ·δ_b` over B = 1024 samples on 8 threads, (i) with `std::atomic_ref<double>::fetch_add` into a shared `grad_W`, (ii) with per-thread `grad_W` copies reduced in fixed order, (iii) OpenMP `reduction(+:...)` on the array (guarded). Report time and whether each is bit-identical across 5 runs; compute GFLOP/s and efficiency for the matmul.

Example: `matmul 1024: serial 1.41 s ; jthread(8) 0.21 s eff 84% ; grad: atomic_ref 118 ms nondeterministic ; per-thread 9 ms deterministic`.

<details><summary>Hint</summary>
Tiles of 64×64 give 256 tasks for n = 1024 — enough for dynamic balance. For (ii) each thread's copy is `out_features × in_features` doubles; sum copies in thread-index order. OpenMP array reductions: `reduction(+ : grad[:n])`.
</details>

---

**15.9 — 2-D heat / FDTD domain decomposition with barriers and halos (sims)**
Parallelize the explicit 2-D heat equation from exercise 12.10 (2048² grid, 200 steps) by slab decomposition over rows: each thread owns a contiguous row range, one halo row on each side, `std::barrier` per step with a completion function swapping buffers. Assert bit-identical to serial. Measure strong scaling for 1, 2, 4, 8, 10 threads and *weak scaling* (grid rows ∝ threads, 512 rows per thread). Then implement temporal blocking: each thread does k = 4 steps per barrier with a 4-row halo computed redundantly; measure the trade-off for k = 1, 2, 4, 8. Finally, replace the barrier with a *neighbour-only* sync (each thread has two `std::atomic<int>` step counters; wait until both neighbours have finished step s before starting s+1) and compare — this is what removes the global barrier in production FDTD codes. Run under TSan with the barrier false positive suppressed and confirm the neighbour-sync version is TSan-clean *without* suppressions.

Example: `strong: 1→10 threads 3.9 s → 0.61 s (6.4x, 64%) ; k=4 temporal blocking 0.48 s ; neighbour-sync 0.44 s`.

<details><summary>Hint</summary>
Halo exchange is implicit — neighbours read your last row of the previous step directly from the shared buffer, which is why the barrier (or the neighbour counters, with acquire/release) is the only sync needed. For neighbour sync: `done[t].store(s, release)` after your rows; before step s+1, spin `while (done[t-1].load(acquire) < s || done[t+1].load(acquire) < s)`. Temporal blocking needs a per-thread private buffer of `k` extra rows on each side.
</details>

---

**15.10 — Parallel Barnes–Hut / PIC scatter: gather vs scatter, and the scaling report (sims)**
Take N = 200 000 particles in 3-D. Part A (gather): parallelize the O(N²) direct-summation force step (or your Barnes–Hut force traversal from P07) with a `parallel for` over particles — static chunks, then dynamic chunks via a shared `std::atomic<std::size_t>` counter handing out blocks of 256; report the difference on a *clustered* distribution (Plummer sphere) vs uniform. Part B (scatter): deposit particle charge onto a 128³ grid with cloud-in-cell weights (8 cells per particle) four ways: (i) serial; (ii) `std::atomic_ref<double>` `fetch_add`; (iii) per-thread grid copies + fixed-order reduction; (iv) sort particles by cell index (`std::sort` by Morton/cell key) then each thread owns a contiguous cell range and deposits only its particles — no atomics. Assert (ii)–(iv) match (i) to 1e-12 (relative) and say which are bit-identical. Part C: produce the full report for A and B — table of threads → time → speedup → efficiency, strong scaling at N = 200k and weak scaling at N = 50k × threads, and one paragraph explaining each curve's shape in terms of bandwidth, load balance, atomics contention, and the P/E-core split. Run everything under TSan once.

Example: `deposit 128^3, 8 threads: atomic_ref 410 ms (eff 9%) ; per-thread grids 61 ms (eff 58%) ; sorted 38 ms (eff 90%, incl. sort)`.

<details><summary>Hint</summary>
Per-thread grids cost 128³ × 8 B × threads = 16 MB × threads — fine here, not at 512³; that's when sorting wins outright (and LBM/PIC codes sort every few steps anyway for cache locality). For the sorted version a particle's 8 target cells span two cell rows; own cell *ranges* so that all of a particle's cells belong to the same thread except at range boundaries — handle those with a small per-thread overflow buffer merged serially at the end.
</details>
