# Chapter 16 — Exercises

Write each exercise as `ex16_K.c` in this folder (`ex16_1.c`, `ex16_2.c`, ...). Compile with
`cc -Wall -Wextra -std=c11 -O2 -pthread -o ex16_K ex16_K.c -lm`. Zero warnings. Before you call
any exercise done, rebuild it with `-g -O1 -fsanitize=thread` on a small input and get **zero**
ThreadSanitizer reports. Time with `CLOCK_MONOTONIC`, warm up once, report the min of 5 runs, and
always print the serial result next to the parallel one so you can see they agree.

`sysconf(_SC_NPROCESSORS_ONLN)` gives the core count; take the thread count from `argv[1]` so you
can sweep T = 1, 2, 4, 8, 10 without recompiling.

---

### 16.1 — **Race, then fix it three ways**

Write a program where `T` threads each increment a shared `long counter` `N` times (`T`, `N` from
`argv`). Version A: plain `counter++` (a data race — UB; run it at `-O0` and `-O2` and record what
you actually see, then run under TSan and paste the two racing lines from the report into a
comment). Version B: `pthread_mutex_t` around the increment. Version C: `atomic_long` with
`atomic_fetch_add_explicit(..., memory_order_relaxed)`. Version D: each thread counts in a local
and adds to the total once after `join`. Time all four at `T=4, N=10^7` and print a table.

Example:
```
T=4 N=10000000
A plain     : 13728201  (WRONG)   0.0210 s
B mutex     : 40000000            0.0800 s
C atomic    : 40000000            0.0730 s
D local     : 40000000            0.0031 s
```

<details><summary>Hint</summary>
Pass one `struct { long n; long local_result; } ` per thread — never `&i`. The "plain" version may
print the right answer at `-O2` because the compiler hoisted the increment out of the loop; that is
the UB talking, not correctness. TSan (`-fsanitize=thread -g -O1`) will still flag it.
</details>

### 16.2 — **Bounded buffer with clean shutdown**

Implement the `Queue` from lesson §4 (ring buffer of `int`, capacity 8, mutex + `not_empty` +
`not_full`) with `q_push`, `q_pop`, and `q_close`. `P` producers each push `M` items
(`producer_id * 1000000 + k`), `C` consumers pop and sum until `q_pop` returns `-1` (queue closed
*and* empty). Main joins everyone and checks the total against the closed-form sum. Run with
`P=3, C=2, M=100000` and `P=1, C=8, M=10`; both must terminate and be TSan-clean.

Example:
```
P=3 C=2 M=100000 cap=8: consumed 300000 items, sum matches
```

<details><summary>Hint</summary>
`q_close` sets a `closed` flag under the mutex and `pthread_cond_broadcast`s `not_empty` so every
blocked consumer re-checks its `while`. The consumer's wait condition is `count == 0 && !closed`;
after the loop, `if (count == 0) return -1;`. Producers must never push after close (assert it).
</details>

### 16.3 — **Barrier and generation counter**

Implement the mutex+condvar `Barrier` from lesson §5 (`barrier_init`, `barrier_wait`,
`barrier_destroy`). Test: `T` threads each run 1000 rounds; in round `r` thread `t` writes
`slot[t] = r`, waits on the barrier, then asserts `slot[(t+1) % T] == r` (its neighbour has also
finished round `r`), waits again, and continues. Then deliberately break it — replace the
`generation` comparison with `while (b->waiting != 0)` — and describe in a comment what goes wrong
and how many rounds it takes to show (it may not, on every run; explain why that is not evidence).

Example:
```
T=8 rounds=1000: all neighbour checks passed, 2000 barrier crossings each
```

<details><summary>Hint</summary>
Without the generation counter a fast thread re-enters the barrier, increments `waiting` to 1, and
a slow thread still in its `while (waiting != 0)` from the *previous* round never leaves. Two
condvar waits per round is the price of correctness — measure the per-crossing cost with `T=8`.
</details>

### 16.4 — **Message passing and memory orders**

Two threads. Producer fills a plain `double payload[1024]` then stores an `atomic_int ready` flag.
Consumer spins on `ready`, then checksums `payload`. Run 10000 rounds (reset the flag each round,
alternate roles with a second flag so both threads progress). Version A: `memory_order_release`
store / `memory_order_acquire` load. Version B: `memory_order_seq_cst`. Version C: `relaxed` both
sides (this is a data race on `payload` → UB; run it under TSan and confirm the report, then explain
in a comment what arm64 is permitted to do that x86 usually isn't). Time A vs B.

Example:
```
A release/acquire : 10000 rounds, checksum OK, 0.031 s
B seq_cst         : 10000 rounds, checksum OK, 0.033 s
C relaxed         : TSan: data race on payload (see comment)  — do not trust its "OK"
```

<details><summary>Hint</summary>
The consumer must reset the flag *after* it finishes reading, and the producer must not overwrite
`payload` until it observes the reset — that is a second acquire/release pair going the other way.
Draw the two arrows before writing code.
</details>

### 16.5 — **Reader–writer lock vs mutex**

A lookup table `double table[4096]` is read by `R` reader threads (random index, sum the value,
10^6 reads each) and rebuilt by 1 writer thread every 1 ms (`nanosleep`, then write all 4096
entries under the lock). Implement with (a) a `pthread_mutex_t`, (b) a `pthread_rwlock_t`. Report
reads/second for `R = 1, 2, 4, 8` for both. Then shrink the critical section to a single element
read and repeat — explain in a comment why the ranking may flip.

Example:
```
R=4  mutex: 41.2 M reads/s   rwlock: 88.7 M reads/s   (4096-element sum per read)
R=4  mutex: 95.1 M reads/s   rwlock: 61.0 M reads/s   (1-element read)
```

<details><summary>Hint</summary>
The rwlock does more bookkeeping per acquire (~2x a mutex uncontended); it wins only when readers
would otherwise wait for *each other* for long enough to cover that. Use a per-thread RNG state
(`_Thread_local` or a struct field) — `rand()` is a shared lock in disguise.
</details>

### 16.6 — **Thread pool with futures**

Implement the pool from lesson §11 (`pool_create(n)`, `pool_submit(pool, fn, arg)`, `pool_wait`,
`pool_destroy`), then add a `Future` type: `pool_submit_f` returns a `Future *` whose
`future_get(f)` blocks until that specific task has completed and returns its `void *` result.
Test: submit 1000 tasks that each compute `sum of sqrt(k)` for `k` in a range and return it via
a `malloc`ed `double`; `future_get` each in order and compare to the serial sum. Then submit 1000
tasks of 1 µs of work and 1000 tasks of 1 ms of work and print the per-task overhead you measure
for each — this tells you the pool's break-even task size.

Example:
```
pool n=8: 1000 futures, sum matches serial (1.4e-9 rel err)
overhead per task: 1 µs tasks: 2.1 µs/task (pool is 3x slower than serial)   1 ms tasks: 0.13 ms/task (7.6x speedup)
```

<details><summary>Hint</summary>
A `Future` is `{ pthread_mutex_t mu; pthread_cond_t cv; int done; void *result; }`. The worker
wraps the user's `fn`: run it, lock the future, set `result` and `done`, signal. Free futures after
`get` — decide who owns the mutex and document it in a comment.
</details>

### 16.7 — **Treiber stack and the ABA trap**

Implement the lock-free stack from lesson §10 with pre-allocated nodes (a free list, never
`free()`d). Stress test: 4 threads each do 10^6 random push/pop operations on shared nodes; count
total pushes minus pops and verify the stack length at the end; TSan-clean. Then write a *second*
version where `pop` immediately `free()`s the node and `push` `malloc`s a new one, run it under
`-fsanitize=address` (not TSan — they cannot combine), and explain in a comment why it may or may
not crash and why the absence of a crash proves nothing. Finally sketch (comment only) the
tagged-pointer fix: which bits of an arm64 user pointer are free, and what the CAS compares.

Example:
```
lock-free stack: 4 threads x 1e6 ops, final length 2137 == pushes-pops, TSan clean
free()ing version: ASan: heap-use-after-free in pop (or: no crash this run — see comment)
```

<details><summary>Hint</summary>
Pop's CAS must use `acquire` on success to see the node's `next` written by the pusher; push's must
use `release`. `atomic_compare_exchange_weak` updates `expected` on failure — do not reload it
yourself, that is the bug that turns a lock-free loop into a livelock.
</details>

### 16.8 — **Parallel matmul for the P02 matrix library** (ML)

Add `mat_mul_threads(const Matrix *A, const Matrix *B, Matrix *C, int T)` to your matrix library
(copy `matrix.h`/`matrix.c` into this folder or `#include` them by relative path). Partition the
rows of `C` across `T` threads; inside each thread use the `i-k-j` loop order from
`../13_debugging_testing_perf/lesson.md`. For `n = 256, 512, 1024` produce the sweep table
(T, time, speedup vs your best serial, efficiency, GFLOP/s) for T = 1, 2, 4, 8, 10. Then a second
table with a *static interleaved* partition (thread `t` takes rows `t, t+T, t+2T, ...`) and a third
with a dynamic `atomic_size_t next_row` claimed 16 rows at a time. Write down which wins at
T=10 (4 P-cores + 6 E-cores) and why.

Example:
```
n=1024  serial 0.412 s  (5.2 GFLOP/s)
T= 2  0.211 s  1.95x  eff 0.98
T= 4  0.108 s  3.81x  eff 0.95
T= 8  0.091 s  4.53x  eff 0.57
T=10  0.088 s  4.68x  eff 0.47   ← stragglers on E-cores; dynamic: 0.071 s 5.80x
```

<details><summary>Hint</summary>
Each thread writes only its own rows of `C`, so no locks anywhere in the loop. The per-thread job
struct holds `lo, hi` (or the atomic counter pointer). Verify `C` against the serial result with a
tolerance every run — the result should be bit-identical here (same operation order per element).
</details>

### 16.9 — **Parallel N-body force loop with per-thread accumulators** (sims)

For `N = 4096` bodies with random positions/masses, compute all pairwise gravitational
accelerations (softened: `r² + eps²`) in three ways and time each with T = 1, 2, 4, 8: (a) direct
`O(N²)` loop, `i` partitioned across threads, each thread writes only `acc[i]` for its own `i`;
(b) symmetric `j > i` loop (half the work) with a **private full-length `acc` buffer per thread**,
reduced after join; (c) symmetric loop with contiguous row ranges vs interleaved rows — show the
load imbalance in the timing. Verify (b) and (c) against (a) to `1e-9` relative. Report
interactions/second.

Example:
```
N=4096 T=4
(a) direct       16.7M pairs  0.047 s  357 M pairs/s   speedup 3.9x
(b) symmetric     8.4M pairs  0.031 s                    speedup 3.1x  (reduction of 4 x 4096 x 3 doubles: 0.2 ms)
(c) contiguous   0.052 s   interleaved 0.031 s   ← contiguous: thread 0 does 44% of the pairs
```

<details><summary>Hint</summary>
Symmetric with a *shared* `acc` is a race the moment thread A writes `acc[j]` for a `j` in thread
B's range. `T * N * 3` doubles of private buffers is 400 KB at these sizes — trivial. The reduction
is a plain serial loop after all joins (or a parallel one over `i` if you want to be thorough).
</details>

### 16.10 — **Data-parallel mini-batch gradient and a stencil with a barrier** (ML + sims)

Two parts in one file, both TSan-clean:

(1) An MLP layer `y = W x + b` (`W` is 256×784, batch of 512 MNIST-sized inputs, random data is
fine) with the backward pass `dW = dy xᵀ`. Split the **batch** across `T` threads; each thread
accumulates into a **private `dW` buffer**, and the buffers are summed after join. Compare to the
serial `dW` within `1e-9` relative; report time and speedup for T = 1, 2, 4, 8. Then try the
"obvious" version that has every thread add directly into one shared `dW` under a mutex per
element, time it, and record the slowdown factor.

(2) A 2-D heat equation / FDTD-style 5-point stencil on a 1024×1024 grid for 200 timesteps.
`T` threads own bands of rows; one `barrier_wait` (from 16.3) per timestep; swap the `u_old`/`u_new`
pointers safely (each thread swaps its own local copies *after* the barrier). Verify against the
serial run bit-exactly (the arithmetic per cell is identical). Report time, speedup, and GB/s
(bytes moved per step = 2 × 8 × 1024² for read + write, roughly), and say which wall you hit —
compare with the reduction table in lesson §13.

Example:
```
(1) dW private-buffers T=4: 0.021 s  3.7x   shared+mutex T=4: 1.94 s  (0.04x — 90x slower than serial)
(2) stencil 1024² x 200 steps  serial 0.61 s   T=4 0.19 s 3.2x  27 GB/s   T=8 0.14 s 4.4x  36 GB/s  ← bandwidth-bound
```

<details><summary>Hint</summary>
Part 1 is exactly OpenMP's `reduction` done by hand — the private buffer is the per-thread partial,
one combine at the end. Part 2: the barrier separates "everyone finished writing step t" from
"anyone starts reading step t as u_old"; a thread that swaps its pointers before the barrier reads
a neighbour's half-written row. Halo rows are the first and last row of each band — no copying is
needed because all threads read the same shared `u_old`. If you have `libomp` installed, redo both
with `#pragma omp parallel for` and compare timings and line counts.
</details>
