# Chapter 16 — Concurrency: pthreads, Atomics, and the Memory Model

## What you'll be able to do after this chapter

- Create, join, and pass structured arguments to POSIX threads; know what `pthread_create` costs and why you should not spawn a thread per matrix row.
- Protect shared state with mutexes without deadlocking, wait for conditions with condition variables (correctly, in a `while` loop), and build a bounded producer/consumer queue and a thread pool from those two primitives.
- Use C11 `<stdatomic.h>` for counters, flags, and compare-and-swap loops; explain relaxed / acquire / release / seq_cst on the message-passing example and know why `volatile` is not a substitute.
- Recognise a data race as undefined behaviour, run ThreadSanitizer, and read its report down to the two racing lines.
- Partition numerical work (reductions, matmul, N-body, stencils) across threads with per-thread partials, measure speedup honestly, and predict the ceiling with Amdahl's law and memory bandwidth.
- Reach for OpenMP when a `#pragma` is the right amount of engineering, and know the macOS setup incantation.

## Why this matters for ML / numerics / sims

Your laptop has 10 cores. A single-threaded matmul uses one. Every framework you have used — PyTorch on CPU, NumPy through OpenBLAS/Accelerate — spends its time in exactly the loops you are about to parallelise: dense matmul split by rows, reductions (loss, norms, softmax denominators) with per-thread partials, force loops in N-body, and stencil sweeps in FDTD/LBM with a barrier between timesteps. Data loading (`DataLoader(num_workers=4)`) is a producer/consumer queue. `torch.multiprocessing` vs threads is the process/thread distinction below. Getting 4-8x from the cores you already own is the cheapest speedup you will ever get — and getting it *wrong* produces results that are silently, non-reproducibly wrong, which is the worst failure mode a numerical program can have. This chapter is about getting it right first and fast second.

---

## 1. Processes vs threads

A **process** is an address space plus at least one thread. A **thread** is an execution context (program counter, registers, stack) inside a process. Threads of one process share the heap, globals, open file descriptors, and code; each has its own stack (default 512 KiB on macOS main thread 8 MiB; Linux default 8 MiB — `ulimit -s`).

| Property                 | Processes (`fork`, ch. 17)                | Threads (`pthread_create`)             |
|--------------------------|-------------------------------------------|----------------------------------------|
| Memory                   | Separate (copy-on-write after fork)       | Shared — one heap, one set of globals   |
| Communication            | Pipes, sockets, `mmap` shared memory      | Plain memory + synchronisation          |
| Creation cost (M5)       | ~100-300 µs (`fork`)                      | ~13 µs measured (`pthread_create`+`join`) |
| A crash in one           | Kills only that process                   | Kills the whole process                 |
| A data race              | Impossible unless you share memory        | Undefined behaviour, easy to write      |
| Python equivalent        | `multiprocessing`                         | `threading` (but no GIL here — threads really run in parallel) |

Rule of thumb: threads for parallel numerics (the data is one big array; copying it between processes would cost more than the computation), processes for isolation (a plugin that may crash, a shell running commands). The pthreads API is defined by POSIX.1-2008 (`man 3 pthread_create`, `man 3 pthread_mutex_lock`); the C standard's own `<threads.h>` (C11 §7.26) is **not shipped on macOS** (and was optional in C11 anyway) — use pthreads directly. Everything in `<threads.h>` (`thrd_create`, `mtx_t`, `cnd_t`) is a thin rename of a pthreads call, so nothing is lost.

Find your core count:

```sh
sysctl -n hw.ncpu                    # 10  (logical CPUs; Apple Silicon has no SMT so = physical)
sysctl -n hw.perflevel0.physicalcpu  # 4   performance cores (M5 in this machine)
sysctl -n hw.perflevel1.physicalcpu  # 6   efficiency cores
sysctl -n hw.cachelinesize           # 128 (x86 is 64)
# Linux: nproc; lscpu; getconf LEVEL1_DCACHE_LINESIZE
```

In C: `sysconf(_SC_NPROCESSORS_ONLN)` (POSIX, `<unistd.h>`).

## 2. The pthreads API: create, join, detach

```c
#include <pthread.h>
#include <stdio.h>

typedef struct { const double *x; size_t lo, hi; double partial; } Job;   /* args in, result out */

static void *sum_squares(void *arg) {
    Job *j = arg;                                  /* void* → our struct */
    double s = 0;
    for (size_t i = j->lo; i < j->hi; i++) s += j->x[i] * j->x[i];
    j->partial = s;                                /* write result into the struct we own */
    return NULL;                                   /* or return a pointer; NEVER a pointer to a local */
}

int main(void) {
    double x[1000]; for (int i = 0; i < 1000; i++) x[i] = i;
    Job jobs[4]; pthread_t th[4];
    for (int t = 0; t < 4; t++) {
        jobs[t] = (Job){ x, 250 * (size_t)t, 250 * (size_t)(t + 1), 0 };
        int rc = pthread_create(&th[t], NULL, sum_squares, &jobs[t]);   /* NULL attr = defaults */
        if (rc != 0) { fprintf(stderr, "pthread_create: %d\n", rc); return 1; }  /* returns errno, does NOT set errno */
    }
    double total = 0;
    for (int t = 0; t < 4; t++) { pthread_join(th[t], NULL); total += jobs[t].partial; }
    printf("%.0f\n", total);      /* 332833500 */
}
```

Rules that follow from the model:
- `pthread_create(&tid, attr, fn, arg)` — `fn` has the fixed signature `void *(*)(void *)`. Pass everything through a struct; **the struct must outlive the thread** (stack of `main` is fine here because `main` joins before returning; a loop-local struct that goes out of scope before `join` is a use-after-scope bug).
- Do **not** pass `&t` for the loop counter — every thread reads `t` after the loop has advanced. Pass `(void *)(intptr_t)t` if you only need a small integer, or one struct per thread.
- `pthread_join(tid, &retval)` blocks until the thread returns and collects its `void *` return. Every created thread must be joined or detached; an unjoined finished thread leaks its stack (like a zombie process).
- `pthread_detach(tid)` — you will never join it; the system reclaims it when it returns. Use for fire-and-forget workers, never for anything whose result you need.
- Return values: `pthread_create` and friends return an error code directly (`EAGAIN`, `EINVAL`) and do not touch `errno`. Wrap them: `#define PT(call) do { int rc_ = (call); if (rc_) { fprintf(stderr, "%s: %s\n", #call, strerror(rc_)); abort(); } } while (0)`.
- Stack size: `pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 16u << 20);` if a thread recurses deeply (a parallel tree solver). Default secondary-thread stack on macOS is 512 KiB — a `double buf[100000]` local (800 KB) segfaults in a thread but works in `main`.
- Thread creation costs ~13 µs on this machine (measured: 2000 create+join pairs). A `1000x1000` matmul row takes ~1 µs. Never spawn a thread per row; spawn `ncpu` threads once and give each a *range* (§13), or keep a pool alive (§10).

cppreference: this chapter's pthreads material is on the POSIX man pages, not cppreference; the C11 `<threads.h>` equivalents are documented at cppreference "Thread support library" if you ever read Linux code that uses them.

## 3. Mutexes

Two threads doing `counter++` on the same variable is a **data race**: undefined behaviour (C11 §5.1.2.4p25 — "The execution of a program contains a data race if it contains two conflicting actions in different threads, at least one of which is not atomic, and neither happens before the other. Any such data race results in undefined behavior."). See `../15_undefined_behavior_and_the_standard/lesson.md` for what "undefined" buys the compiler; here it buys this:

```c
static long counter = 0;
static void *worker(void *arg) { (void)arg; for (int i = 0; i < 100000; i++) counter++; return NULL; }
/* -O2, two threads:  counter = 200000   ← "works" — clang hoisted counter into a register and
                                             stored once per thread; the race is 2 stores, rarely lost
   -O1 -fsanitize=thread: counter = 100000 ← the same program, the update lost; TSan also reports it */
```

A **mutex** (mutual exclusion lock) makes the read-modify-write one indivisible critical section:

```c
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;   /* static init; or pthread_mutex_init(&mu, NULL) */
static long counter = 0;

static void *worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 100000; i++) {
        pthread_mutex_lock(&mu);        /* blocks if another thread holds it */
        counter++;                      /* critical section: exactly one thread here at a time */
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}
/* always 200000.  Cost measured: 3.6 ns per lock/unlock pair uncontended (single thread),
   ~30 ns/op with 4 threads fighting over it — 10x slower than one thread doing all the work. */
```

The mutex also provides **visibility**: everything thread A wrote before `unlock` is visible to thread B after B's `lock` of the same mutex (`unlock` *synchronizes-with* the next `lock`, C11 §5.1.2.4p9-11). That is the second, equally important job of a lock: without it, B could see A's writes in any order or not at all.

Three ways to deadlock and how to avoid them:

| Deadlock                              | Fix                                                                     |
|---------------------------------------|-------------------------------------------------------------------------|
| Thread A locks M1 then M2; B locks M2 then M1 | **Lock ordering**: define a global order (e.g. by address: `if (&a < &b) lock(a),lock(b) else ...`) and always take locks in that order |
| Lock M, call a function that locks M  | Don't call out of a critical section; or use `PTHREAD_MUTEX_RECURSIVE` (smell) |
| Return/`goto` out of a critical section without unlocking | Single exit, or `goto unlock;` idiom (`../14_preprocessor_and_c_idioms/lesson.md` §6) |

Mutex kinds (`pthread_mutexattr_settype`):
- `PTHREAD_MUTEX_NORMAL` / `DEFAULT` — relocking from the same thread deadlocks (macOS) or is UB (POSIX). Fast.
- `PTHREAD_MUTEX_ERRORCHECK` — relock returns `EDEADLK`, unlocking a mutex you don't hold returns `EPERM`. Use in debug builds: `pthread_mutexattr_t a; pthread_mutexattr_init(&a); pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK); pthread_mutex_init(&mu, &a);`.
- `PTHREAD_MUTEX_RECURSIVE` — a counter; same thread can relock N times and must unlock N times. Convenient, but it hides the fact that you don't know your own lock state, and it cannot be used with condition variables sanely.

`pthread_mutex_trylock` returns `EBUSY` instead of blocking — used for "do something else if the lock is taken" and for lock-ordering-free acquisition of two locks (try the second, back off if busy).

Python equivalent: `threading.Lock()` / `with lock:`. `RLock` is the recursive kind.

## 4. Condition variables

A mutex protects state; a **condition variable** lets a thread sleep until the state changes. Never busy-wait on a flag under a mutex (`while (!ready) { unlock; lock; }`) — that burns a core. `pthread_cond_wait(&cv, &mu)` atomically: unlocks `mu`, sleeps, and re-locks `mu` before returning.

The **canonical pattern** — memorise its shape:

```c
pthread_mutex_lock(&mu);
while (!condition)                    /* while, NEVER if: see spurious wakeups below */
    pthread_cond_wait(&cv, &mu);      /* returns with mu held and condition (probably) true */
/* ... use/modify shared state ... */
pthread_mutex_unlock(&mu);

/* the other side */
pthread_mutex_lock(&mu);
make_condition_true();
pthread_cond_signal(&cv);             /* wake one waiter;  pthread_cond_broadcast: wake all */
pthread_mutex_unlock(&mu);
```

Why `while`:
1. **Spurious wakeups** are permitted by POSIX (`man pthread_cond_wait`: "spurious wakeups ... may occur") — the kernel may return from a wait without any signal.
2. Even with a genuine signal, another thread may grab the mutex first and consume the state (two consumers, one item). The condition must be re-checked with the mutex held.
3. `pthread_cond_signal` with no waiter does nothing — it is not a counter (unlike a semaphore). If the condition is set before the waiter checks, the `while` sees it true and never sleeps. That is why the *state* (`condition`) is the truth and the condvar is only a wake-up hint.

**Bounded buffer (producer/consumer)** — the structure behind every data-loader, log queue, and thread pool:

```c
#define CAP 8
typedef struct {
    int buf[CAP]; size_t head, tail, count;
    pthread_mutex_t mu; pthread_cond_t not_empty, not_full;
} Queue;

void q_push(Queue *q, int v) {
    pthread_mutex_lock(&q->mu);
    while (q->count == CAP) pthread_cond_wait(&q->not_full, &q->mu);
    q->buf[q->tail] = v; q->tail = (q->tail + 1) % CAP; q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
}
int q_pop(Queue *q) {
    pthread_mutex_lock(&q->mu);
    while (q->count == 0) pthread_cond_wait(&q->not_empty, &q->mu);
    int v = q->buf[q->head]; q->head = (q->head + 1) % CAP; q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mu);
    return v;
}
```

Two condvars because producers and consumers wait for different conditions; one condvar with `broadcast` also works but wakes the wrong side. Shutdown: add a `done` flag, set it under the mutex, `broadcast`, and have `q_pop` return a sentinel when `count == 0 && done`.

Measured on this machine: a strict ping-pong between two threads through a mutex+condvar costs **1.7 µs per handoff** (200 000 round trips in 0.68 s). That is your floor for "wake another thread to do something" — do not hand off items smaller than ~10 µs of work one at a time; batch them.

`pthread_cond_timedwait(&cv, &mu, &abs_deadline)` for timeouts (absolute `struct timespec`, `CLOCK_REALTIME` by default; returns `ETIMEDOUT`).

Python equivalent: `threading.Condition` — `with cv: while not pred: cv.wait()` — same `while`; `queue.Queue(maxsize)` is the bounded buffer.

## 5. Read-write locks and barriers

**`pthread_rwlock_t`** — many readers or one writer. `pthread_rwlock_rdlock`, `pthread_rwlock_wrlock`, `pthread_rwlock_unlock`. Use when reads vastly outnumber writes and the critical section is long enough that a mutex would serialise readers (a lookup table rebuilt occasionally, a model's weights read by inference threads and swapped by a trainer). For short sections a plain mutex is faster: the rwlock does more bookkeeping (~2x the uncontended cost) and writer starvation is implementation-defined.

**Barriers** — all N threads wait until all N have arrived, then all proceed. Exactly what a timestep loop needs: every thread updates its rows for step `t`, barrier, every thread reads neighbours' rows for step `t+1`. POSIX has `pthread_barrier_t` but **macOS does not implement it** (it is an optional POSIX feature; Linux glibc has it). Write one — it is 25 lines and a good condvar exercise:

```c
typedef struct { pthread_mutex_t mu; pthread_cond_t cv; int n, waiting, generation; } Barrier;

void barrier_init(Barrier *b, int n) { pthread_mutex_init(&b->mu, NULL); pthread_cond_init(&b->cv, NULL); b->n = n; b->waiting = 0; b->generation = 0; }

void barrier_wait(Barrier *b) {
    pthread_mutex_lock(&b->mu);
    int gen = b->generation;                     /* remember which round I'm in */
    if (++b->waiting == b->n) {                  /* last to arrive: release everyone */
        b->waiting = 0; b->generation++;
        pthread_cond_broadcast(&b->cv);
    } else {
        while (gen == b->generation)             /* wait for the generation to change, not for waiting==0 */
            pthread_cond_wait(&b->cv, &b->mu);   /* (waiting could already be >0 for the NEXT round) */
    }
    pthread_mutex_unlock(&b->mu);
}
```

The `generation` counter is the subtle part: a fast thread can leave the barrier and re-enter for the next round before a slow one has woken up; comparing generations instead of `waiting == 0` makes that safe. Barrier cost is roughly one condvar handoff per thread (~2-5 µs for 8 threads); an FDTD step over a 1000x1000 grid takes ~1 ms, so one barrier per step is <1% overhead. One barrier per *row* would not be.

## 6. Thread-local storage

```c
#include <stdlib.h>
static _Thread_local unsigned rng_state = 1;      /* C11 keyword (§6.7.1); __thread is the GNU/clang spelling */

unsigned my_rand(void) { rng_state = rng_state * 1103515245u + 12345u; return rng_state >> 16; }
```

Each thread gets its own `rng_state`, initialised to `1` at thread start (must be a constant expression). No locking, no false sharing, no argument threading. Uses: per-thread RNG streams (seed each from thread id + global seed, so results are reproducible for a fixed thread count), per-thread scratch buffers, per-thread error state (`errno` is thread-local for exactly this reason). Access is one extra indirection through the thread pointer register (`tpidr_el0` on arm64) — ~1 ns, negligible. `_Thread_local` variables cannot be dynamically sized and cannot be `extern`-shared across dylib boundaries reliably; for dynamic per-thread storage use `pthread_key_create`/`pthread_getspecific` (older, clunkier) or simply pass a per-thread struct pointer.

Python equivalent: `threading.local()`.

## 7. Semaphores (and the macOS situation)

A semaphore is a counter with `wait` (decrement, block at 0) and `post` (increment). Unlike a condvar it remembers posts. POSIX unnamed semaphores (`sem_init`/`sem_wait`/`sem_post`) are **not implemented on macOS** — `sem_init` compiles with a deprecation warning and returns `-1`/`ENOSYS`. Options:

| Option                          | macOS | Linux | Notes                                                                  |
|---------------------------------|-------|-------|------------------------------------------------------------------------|
| `sem_open("/name", O_CREAT, 0600, initial)` named | yes | yes | Kernel object with a name; `sem_close` + `sem_unlink`. Works across processes too. |
| `dispatch_semaphore_create(n)` (`<dispatch/dispatch.h>`) | yes | no | `dispatch_semaphore_wait(s, DISPATCH_TIME_FOREVER)` / `dispatch_semaphore_signal(s)`. Fast, Apple-only. |
| Mutex + condvar + counter       | yes   | yes   | 20 lines, portable; what the example does.                              |
| `sem_init`                      | **no**| yes   |                                                                        |

Most things people reach for a semaphore for (bounded buffer slots, "wait for N workers") are clearer as a counter under a mutex with a condvar, which is portable, so that is what this course uses.

## 8. C11 atomics: `<stdatomic.h>`

An **atomic** operation is indivisible and, importantly, is *not* a data race even when concurrent (§5.1.2.4 excludes atomic accesses from the race definition). C11 §7.17 defines the types and operations; cppreference "Atomic operations library" (C) documents each function.

```c
#include <stdatomic.h>
static atomic_long hits = 0;              /* atomic_long = _Atomic long; also atomic_int, atomic_bool, atomic_size_t, _Atomic(T*) */

void record(void) { atomic_fetch_add(&hits, 1); }             /* returns the OLD value; seq_cst by default */
long read_hits(void) { return atomic_load(&hits); }
void reset(void) { atomic_store(&hits, 0); }
/* atomic_exchange, atomic_fetch_sub/or/and/xor, atomic_compare_exchange_strong/weak */
```

Measured: `atomic_fetch_add_explicit(..., memory_order_relaxed)` costs **1.5 ns** single-threaded vs 3.6 ns for the mutex pair; with 4 threads hammering one counter both degrade to ~30 ns/op — because the *cache line* is the bottleneck, not the instruction (§9). `_Atomic` arithmetic through the plain operators also works (`hits++` on an `atomic_long` is an atomic RMW, seq_cst) but hides the cost; prefer the explicit functions in real code.

**Compare-and-swap (CAS) loops** implement any read-modify-write the hardware lacks, e.g. "atomic max of a double" (there is no `atomic_fetch_max`, and `double` isn't a supported atomic arithmetic type):

```c
static _Atomic uint64_t gmax_bits;         /* store the double's bit pattern */

void atomic_max_double(double v) {
    uint64_t newb; memcpy(&newb, &v, 8);                          /* type punning via memcpy: ch. 15 */
    uint64_t old = atomic_load_explicit(&gmax_bits, memory_order_relaxed);
    for (;;) {
        double oldv; memcpy(&oldv, &old, 8);
        if (!(v > oldv)) return;                                 /* already ≥ v; nothing to do */
        if (atomic_compare_exchange_weak_explicit(&gmax_bits, &old, newb,
                memory_order_relaxed, memory_order_relaxed))
            return;                                              /* swapped: we're done */
        /* CAS failed: 'old' was updated to the current value; loop and re-check */
    }
}
```

`compare_exchange(obj, &expected, desired)`: if `*obj == expected` then `*obj = desired`, return true; else `expected = *obj`, return false. `_weak` may fail spuriously (allowed to return false even when equal — cheaper on ARM's LL/SC instructions `ldxr/stxr`) so it belongs in loops; `_strong` for single attempts.

### Memory orders

Atomicity (indivisibility) is only half the story. The other half is **ordering**: what a thread is guaranteed to see of *other* variables when it observes an atomic. The classic **message-passing** example:

```c
static int data;                     /* plain, non-atomic payload */
static atomic_bool ready = false;

/* Thread 1 (producer) */               /* Thread 2 (consumer) */
data = 42;                              while (!atomic_load_explicit(&ready, memory_order_acquire)) ;
atomic_store_explicit(&ready, true,     printf("%d\n", data);     /* guaranteed 42 */
                      memory_order_release);
```

- `memory_order_release` on the store: all writes *before* it in this thread (here `data = 42`) become visible to any thread that performs an acquire load that reads this value.
- `memory_order_acquire` on the load: all reads *after* it in this thread see those writes. Together they form a *synchronizes-with* edge (§5.1.2.4p11) — the same guarantee a mutex unlock/lock pair gives.
- With `memory_order_relaxed` on both, `ready == true` and `data == 0` is a **legal outcome**: relaxed guarantees only atomicity of that one variable; the compiler may reorder `data = 42` after the store, and on ARM (weakly ordered) the hardware may too. x86 would happen to work (its stores are ordered) — which is why code that "worked for years" breaks on Apple Silicon.
- `memory_order_seq_cst` (the default for the non-`_explicit` functions): acquire+release plus a single global order of all seq_cst operations that every thread agrees on. Needed for algorithms like Dekker's mutual exclusion where two threads each store their flag then load the other's; not needed for message passing or counters. Cost on arm64: an extra `dmb ish` barrier per operation (~5-20 ns); on x86 `mfence`/locked instructions.
- `memory_order_acq_rel` for RMW operations that both publish and consume (CAS on a lock-free stack head).
- `memory_order_consume` — deprecated in practice; treat as acquire.

| Order      | Use for                                                     | Guarantee                                                |
|------------|-------------------------------------------------------------|----------------------------------------------------------|
| relaxed    | Statistics counters, per-thread progress flags nobody acts on, the RNG state | Atomicity only; no ordering with other memory |
| release    | Publishing data: "everything I wrote is now ready"          | Prior writes visible to matching acquire                 |
| acquire    | Consuming published data                                    | Subsequent reads see the releaser's prior writes         |
| acq_rel    | RMW on a shared structure head (CAS loops)                  | Both                                                     |
| seq_cst    | When you can't prove acq/rel suffices; total order needed   | Both + global order; slowest                             |

**Fences**: `atomic_thread_fence(memory_order_release)` before a relaxed store, `atomic_thread_fence(memory_order_acquire)` after a relaxed load, give the same edge without tagging the operation — useful when you have many relaxed stores and want one barrier at the end (a thread filling its rows, then one release fence, then a relaxed flag store). `atomic_signal_fence` orders against a signal handler on the same thread only (ch. 17).

Python equivalent: none — the GIL makes every bytecode effectively seq_cst. That's why Python threads are slow and why you're here.

## 9. The memory model in practice

**Data race = UB, not "a stale value".** Beginners expect a race to produce "either the old or the new value". The standard says the whole program's behaviour is undefined, and the compiler acts on that: it hoists loads out of loops (the `-O2` `counter = 200000` surprise in §3 — the loop became a single `add`), tears wide stores, or deletes a `while (!flag) ;` loop entirely because a non-atomic, non-volatile `flag` cannot change "by itself". If two threads touch a variable and at least one writes, it must be atomic or protected by a lock. No exceptions for `int` ("it's one instruction"), no exceptions for "I only read it".

**`volatile` is not atomic** (`../15_undefined_behavior_and_the_standard/lesson.md`, §`volatile`). It forces the compiler to emit every load/store and not cache the value — which stops the loop-deletion symptom — but gives (a) no atomicity (`volatile long x; x++` is still load/add/store, still racy, still torn on 32-bit), (b) no ordering with other variables, (c) no cache-coherence guarantees beyond what the hardware gives anyway. It is for memory-mapped hardware registers and `sig_atomic_t` signal flags. For threads, `_Atomic`.

**Cache coherence and false sharing.** Cores don't share bytes; they share **cache lines** — 128 bytes on Apple Silicon, 64 on x86 (`sysctl -n hw.cachelinesize`). When core A writes a line, every other core's copy is invalidated and must be re-fetched (~40-100 ns from another core's L2, vs ~1 ns for an L1 hit). If two threads write to *different* variables that happen to sit in the *same* line, they fight over it exactly as if they shared the variable. That is **false sharing**, and it is the number one reason "I gave each thread its own counter and it got slower".

Measured on this machine, 4 threads each incrementing its own `atomic_long` 10^7 times:

```
struct unpadded { atomic_long v; }      up[4];          40.6 ns per increment   ← all four in one 128 B line
struct padded   { atomic_long v; char pad[120]; } pd[4]; 1.66 ns per increment  ← one line each: 24x faster
per-thread local variable, one store at the end               ~0                ← the actual right answer
```

Three lessons in one table: pad per-thread slots to the cache-line size (`_Alignas(128)` on the struct, or an explicit `pad[]`), better still don't share at all — accumulate in a local and write once (§13), and remember that the "unpadded" row is what you get by default with `double partial[NTHREADS]`.

## 10. Lock-free basics: the Treiber stack and ABA

A **lock-free** structure guarantees system-wide progress without locks, using CAS. The simplest is Treiber's stack (1986):

```c
typedef struct Node { int value; struct Node *next; } Node;
static _Atomic(Node *) top = NULL;

void push(Node *n) {
    n->next = atomic_load_explicit(&top, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&top, &n->next, n,
               memory_order_release, memory_order_relaxed))
        ;   /* n->next was refreshed to the current top by the failed CAS; retry */
}

Node *pop(void) {
    Node *old = atomic_load_explicit(&top, memory_order_acquire);
    while (old && !atomic_compare_exchange_weak_explicit(&top, &old, old->next,
               memory_order_acquire, memory_order_acquire))
        ;
    return old;   /* caller owns it — but see ABA before you free() it */
}
```

Release on the push CAS publishes `n->value`/`n->next`; acquire on pop's load sees them. Now the famous bug — **ABA**:

1. Thread 1 in `pop` reads `top == A`, and `A->next == B`. It is about to CAS(top, A → B).
2. Thread 2 pops A, pops B, frees both, pushes a new node that `malloc` happens to place at A's old address (call it A').
3. Thread 1's CAS compares `top` with A: equal (same address!), succeeds, sets `top = B` — a freed node. Corruption.

CAS compares *bits*, not *identity*. Fixes: (a) never free nodes — recycle them through a free list (still ABA-prone for the free list itself, but harmless if `next` is re-read), (b) tagged pointers: pack a version counter into the pointer's unused high bits (arm64 user pointers use 48 bits; steal 16) and CAS the 64-bit pair, (c) hazard pointers or epoch-based reclamation (RCU) — real solutions, real complexity, (d) use a mutex. For a numerical program, (d) is nearly always right: your critical sections are short and your contention is low; lock-free code is for OS kernels and databases and takes weeks to get right. Know the idea; don't ship it before you've read Herlihy & Shavit, *The Art of Multiprocessor Programming*.

**Spinlock vs mutex:**

```c
static atomic_flag lk = ATOMIC_FLAG_INIT;
void spin_lock(void)   { while (atomic_flag_test_and_set_explicit(&lk, memory_order_acquire)) ; }
void spin_unlock(void) { atomic_flag_clear_explicit(&lk, memory_order_release); }
```

A spinlock never sleeps: fastest when the hold time is a few ns and the holder is running on another core; catastrophic when the holder is descheduled (all waiters burn their timeslices waiting for a thread the scheduler now can't run — priority inversion). macOS `pthread_mutex_t` and Linux futex-based mutexes already spin briefly then sleep; there is almost no reason to write your own spinlock in user space. If you do, `#include <sched.h>` and call `sched_yield()` in the loop.

## 11. A thread pool

Creating threads per task costs 13 µs each plus scheduler noise. A **pool** creates `ncpu` workers once; they block on a task queue (§4's bounded buffer holding function pointers + argument pointers); `pool_submit` pushes; `pool_wait` blocks until `pending == 0`.

```c
typedef struct { void (*fn)(void *); void *arg; } Task;
typedef struct {
    Task q[256]; size_t head, tail, count;  /* ring buffer of tasks */
    pthread_mutex_t mu; pthread_cond_t has_work, all_done;
    size_t pending;                          /* submitted but not finished */
    int stop; pthread_t *th; int n;
} Pool;

static void *worker(void *p_) {
    Pool *p = p_;
    for (;;) {
        pthread_mutex_lock(&p->mu);
        while (p->count == 0 && !p->stop) pthread_cond_wait(&p->has_work, &p->mu);
        if (p->count == 0 && p->stop) { pthread_mutex_unlock(&p->mu); return NULL; }
        Task t = p->q[p->head]; p->head = (p->head + 1) % 256; p->count--;
        pthread_mutex_unlock(&p->mu);
        t.fn(t.arg);                                          /* run OUTSIDE the lock */
        pthread_mutex_lock(&p->mu);
        if (--p->pending == 0) pthread_cond_broadcast(&p->all_done);
        pthread_mutex_unlock(&p->mu);
    }
}
```

Design points: run the task outside the lock (otherwise the pool is a serial executor); `pending` counts submissions so `pool_wait` can be a `while (pending) cond_wait(all_done)`; shutdown sets `stop`, broadcasts `has_work`, joins all. The example file has the complete thing in ~80 lines. Grand Central Dispatch (`dispatch_async` on macOS) and Linux's `libdispatch`/TBB are production versions of this.

Python equivalent: `concurrent.futures.ThreadPoolExecutor` (`submit`, `shutdown(wait=True)`).

## 12. ThreadSanitizer

TSan instruments every memory access and reports two accesses to the same location, from different threads, not ordered by a lock or atomic. Build with `-fsanitize=thread -g` (`-O1` keeps line numbers accurate; TSan is incompatible with ASan in the same binary). Actual report from the racy counter in §3 on this machine:

```
$ cc -Wall -Wextra -std=c11 -g -O1 -pthread -fsanitize=thread -o race race.c && ./race
==================
WARNING: ThreadSanitizer: data race (pid=13294)
  Write of size 8 at 0x00010077c000 by thread T2:
    #0 worker race.c:7 (race:arm64+0x100000750)          ← the racing access

  Previous write of size 8 at 0x00010077c000 by thread T1:
    #0 worker race.c:7 (race:arm64+0x100000750)          ← the other access; same line here, needn't be

  Location is global 'counter' at 0x00010077c000 (race+0x100008000)   ← WHICH variable

  Thread T2 (tid=537011, running) created by main thread at:
    #1 main race.c:13
  Thread T1 (tid=537010, finished) created by main thread at:
    #1 main race.c:12

SUMMARY: ThreadSanitizer: data race race.c:7 in worker
==================
counter = 100000 (expected 200000)
ThreadSanitizer: reported 1 warnings
```

Reading it: two stack traces (the two conflicting accesses, with sizes — a size-8 write vs a size-4 read tells you about a torn access), the location (global name, or heap block with its allocation stack, or a stack variable with its owning thread), and where each thread was created. If a report names a lock ("Mutex M1 (0x...) created at"), the race is between a locked and an unlocked access — you forgot the lock in one place. TSan slows programs 5-15x and uses 5-10x memory; run on small inputs. It has no false positives for pthread/atomic-synchronised code; it *can* miss races that don't happen in that run (it observes, doesn't prove) — run several times and with a shuffled schedule. Also try `TSAN_OPTIONS=second_deadlock_stack=1` for lock-order inversions, which TSan also reports ("lock-order-inversion (potential deadlock)"). Linux: identical flags; `valgrind --tool=helgrind` is the alternative. Linux `perf` and macOS Instruments' "System Trace" show you thread scheduling when correctness is fine but speed isn't.

## 13. Work partitioning for numerics

The recipe: split the *output* into disjoint ranges, one per thread, so no two threads write the same memory and no synchronisation is needed inside the loop. Reads of shared *input* are always safe (no writer).

**Reduction with per-thread partials** (dot product, loss, norm, softmax denominator):

```c
/* each thread: */ double s = 0; for (i = lo; i < hi; i++) s += x[i]*y[i]; job->partial = s;   /* local accumulator */
/* main after join: */ total = 0; for (t...) total += jobs[t].partial;
```

Do not `atomic_fetch_add` into one shared double per element (there is no atomic add for double anyway; a CAS loop per element would be 100x slower than the multiply). Per-thread partials + one combine at the end is the pattern for every reduction. Note the result differs from serial summation in the last bits (floating-point addition is not associative, `../12_numbers_bits_floats/lesson.md`) — and differs *by thread count*. Fix the thread count and the partition for reproducibility; use Kahan/pairwise summation inside each partial if you care about accuracy.

Measured, sum of squares over 2^24 doubles (128 MiB) on this machine, best of 5:

```
T= 1   9.1 ms  14.8 GB/s
T= 2   4.5 ms  29.5 GB/s   2.0x
T= 4   2.8 ms  48.3 GB/s   3.3x
T= 8   1.8 ms  73.0 GB/s   5.1x
T=16   1.8 ms  76.0 GB/s   5.1x   ← flat: memory bandwidth reached (M5 ~100+ GB/s peak; 6 of 10 cores are E-cores)
```

**Matmul by rows**: `C = A·B`, thread `t` computes rows `[t·m/T, (t+1)·m/T)` of `C`. Each thread reads all of `B` (shared, read-only, fine) and its rows of `A`, writes only its rows of `C`. Zero synchronisation. Use the cache-friendly `i-k-j` loop order from `../13_debugging_testing_perf/lesson.md` inside each thread — parallelising the slow loop order gives you 8x of slow. Expect ~3.5-4x on 4 P-cores for a 1024x1024 double matmul (compute bound: 2·10^9 flops, ~1 GFLOP/s/core naive, ~10-20 with vectorisation and blocking).

**N-body force loop**: `for i: for j≠i: a[i] += f(i,j)`. Partition `i` across threads; each writes only `a[i]` for its own `i`. If you exploit Newton's third law (`f_ji = -f_ij`, halving the work) thread A now writes `a[j]` for `j` in B's range — a race. Either give up the symmetry, or give each thread a private full-length `acc[N]` and reduce them after the loop (`T·N` extra memory, trivial for N < 10^6). Balance: with symmetry the row `i` costs `N-i` interactions, so contiguous ranges are unbalanced — interleave (`i = t, t+T, t+2T, ...`) or use dynamic chunks.

**Stencils (FDTD, LBM, heat equation)**: each thread owns a band of rows of `u_new`, reads `u_old` including one halo row from each neighbour's band. One barrier per timestep, then swap pointers (swap under the barrier, or have each thread swap its own local pointers after the barrier — the swap must not race with a slow thread still reading). Memory-bound: a 5-point stencil is 5 loads + 1 store per ~5 flops, so speedup saturates at the bandwidth ceiling like the reduction above, not at core count. Blocking in time (do 2-4 timesteps on a tile while it's in cache) is the fix, and is a research area.

**Static vs dynamic scheduling**: equal ranges (static) are best when every element costs the same (matmul, stencil). When cost varies (N-body with symmetry, tree traversals, variable-length sequences in a tokenizer), keep an `atomic_size_t next_chunk` and have each thread `atomic_fetch_add(&next, CHUNK)` to claim work until exhausted — a one-line work-stealing scheduler. Chunk size trades balance against contention on the counter: 64-1024 iterations is typical.

## 14. OpenMP: the pragmatic path

Everything in §13 is a `#pragma` in OpenMP, and the compiler generates the threads, the partition, the per-thread partials, and the barrier:

```c
#include <omp.h>
double dot(const double *x, const double *y, size_t n) {
    double s = 0;
    #pragma omp parallel for reduction(+:s) schedule(static)
    for (size_t i = 0; i < n; i++) s += x[i] * y[i];      /* each thread: private s; combined at the end */
    return s;
}

void matmul(const double *A, const double *B, double *C, size_t n) {
    #pragma omp parallel for schedule(static)               /* rows of C split across threads */
    for (size_t i = 0; i < n; i++)
        for (size_t k = 0; k < n; k++) { double a = A[i*n+k]; for (size_t j = 0; j < n; j++) C[i*n+j] += a * B[k*n+j]; }
}

/* dynamic scheduling for unbalanced work (N-body upper triangle): */
#pragma omp parallel for schedule(dynamic, 64)
for (size_t i = 0; i < n; i++) for (size_t j = i+1; j < n; j++) { ... }

/* rare shared update inside a parallel region: */
#pragma omp critical                       /* a mutex around the block */
{ if (e < best) { best = e; best_i = i; } }
#pragma omp atomic                         /* single atomic RMW; much cheaper than critical */
hist[bin]++;
```

- `parallel for` — fork threads (pool reused between regions), split iterations. Loop variable must be an integer type with a simple bound; `i` is automatically private.
- `reduction(op:var)` — `+ * - & | ^ && || min max`: private copy per thread, combined at the end. This *is* §13's per-thread partials.
- `schedule(static|dynamic[,chunk]|guided)` — how iterations map to threads; `static` default for uniform cost.
- `private(x)`, `shared(y)`, `firstprivate(z)` — control variable sharing; the default is everything declared outside the region is shared (a race waiting to happen — declare loop-body temporaries *inside* the loop).
- `omp_get_num_threads()`, `omp_get_thread_num()`, `omp_get_wtime()`; `OMP_NUM_THREADS=4 ./prog` at runtime.
- `#pragma omp barrier`, `#pragma omp single`, `#pragma omp simd` (vectorisation hint; combine `parallel for simd`).

**macOS setup** (Apple's clang ships without the OpenMP runtime):

```sh
brew install libomp
clang -Wall -Wextra -std=c11 -O2 -Xpreprocessor -fopenmp \
      -I"$(brew --prefix libomp)/include" -L"$(brew --prefix libomp)/lib" -lomp -o prog prog.c -lm
# Linux (gcc or clang):  cc -fopenmp -O2 -o prog prog.c -lm
```

`-Xpreprocessor -fopenmp` makes Apple clang process the pragmas without trying to link its (absent) runtime; `-lomp` links Homebrew's. Without those flags the pragmas are silently ignored (warning `-Wsource-uses-openmp` if you ask) and the program runs serially — which is exactly the design: OpenMP code is valid serial C. Guard OpenMP-only API calls with `#ifdef _OPENMP` (defined by the compiler as `YYYYMM` of the spec version when enabled). The example's OpenMP section is guarded that way; without libomp it prints a note and skips.

When to use pthreads over OpenMP: producer/consumer pipelines, long-lived worker threads with different roles, anything that isn't "this loop, in parallel". For numerics loops, OpenMP first; it is what every BLAS you'll compete with uses.

## 15. Amdahl's law and when not to thread

If fraction `p` of the runtime is parallelisable and you have `T` threads, speedup is `S = 1 / ((1-p) + p/T)`.

| p (parallel fraction) | T=4  | T=8  | T=16 | T=∞ |
|-----------------------|------|------|------|-----|
| 0.50                  | 1.6x | 1.8x | 1.9x | 2x  |
| 0.90                  | 3.1x | 4.7x | 6.4x | 10x |
| 0.95                  | 3.5x | 5.9x | 9.1x | 20x |
| 0.99                  | 3.9x | 7.5x | 13.9x| 100x|

Ten percent serial code caps you at 10x forever. In an MLP training step, the matmuls are ~90% and parallelise; the loss reduction, the weight update (memory-bound, `w -= lr*g` over all parameters), and the data shuffle are the `1-p`. Measure with `sample`/Instruments (`../13_debugging_testing_perf/lesson.md`) *before* threading to find `p`.

**Do not thread memory-bound loops expecting core-count speedup.** A loop that does a few flops per byte (SAXPY `y = a*x + y`: 2 flops per 24 bytes; the weight update; a stencil) is limited by DRAM bandwidth, not by cores. This machine peaks near ~100 GB/s; one P-core alone streams ~15 GB/s (measured above), so 4-6 cores saturate it and adding more does nothing (T=8 vs T=16 above). Arithmetic intensity (flops/byte) decides: matmul is O(n) flops per byte with blocking — thread it; `x[i] + y[i]` is 1/12 — thread it only for the ~5x bandwidth gain, and expect nothing past that. (The roofline model formalises this; "Roofline: an insightful visual performance model", Williams et al. 2009.)

Other reasons not to thread: the work is < ~50 µs total (thread wake-up cost dominates); the loop has a true dependency (`x[i] = f(x[i-1])` — an ODE time-step loop is *inherently* serial in time; parallelise across the spatial grid or across independent trajectories instead); the code isn't correct yet (never debug a race and a math bug at once — get the serial version bit-exact first, keep it as the oracle).

## 16. Measuring speedup properly

1. **Wall clock**, not CPU time: `clock_gettime(CLOCK_MONOTONIC, ...)`. `clock()` sums CPU time over threads — an 8-thread program shows ~8x *more* "time".
2. **Warm up** once (page faults on first touch of a fresh `malloc`, cache fill, thread pool creation), then time `≥5` repeats and report the **minimum** (or median; never the first).
3. **Same partition for serial**: the serial baseline must be your best serial code (right loop order, `-O2`), not the parallel code run with T=1 — the latter flatters you.
4. **Verify the result** against the serial one each run (`fabs(a-b) < 1e-9 * fabs(b)`); a wrong answer 6x faster is worthless.
5. **Thread count sweep**: T = 1, 2, 4, 8, ncpu, 2·ncpu. Speedup usually peaks at the P-core count on Apple Silicon (4 here); E-cores add bandwidth but each is ~1/3 as fast, so a static equal split *slows down* when the slowest E-core determines the finish time — the classic straggler. Dynamic scheduling (`schedule(dynamic)`, or the atomic chunk counter) fixes it.
6. **You cannot pin threads to cores on macOS** (`pthread_setaffinity_np` doesn't exist; QoS hints via `pthread_set_qos_class_self_np` are all you get). Linux: `taskset -c 0-3 ./prog` or `pthread_setaffinity_np`. Expect ±10% run-to-run noise on macOS from the scheduler moving threads between P and E clusters; report min of N.
7. Report a table: T, time, speedup, efficiency (`speedup/T`), and GB/s or GFLOP/s so you can see which wall you hit.

---

## Gotchas and undefined behavior

- **Any unsynchronised write shared with another access is a data race → UB** (§5.1.2.4p25). Not "a stale read". Includes `bool` flags, `int` counters, "I only read it while the other thread writes it". Fix: `_Atomic` or a mutex.
- **`volatile` does not make threads safe.** It suppresses the compiler's caching; it provides neither atomicity nor ordering. Use it for `sig_atomic_t` signal flags and hardware registers only.
- **`if (cond) pthread_cond_wait(...)`** — must be `while`. Spurious wakeups are legal; another thread may have consumed the state.
- **Signalling without holding the mutex** can lose a wakeup: the waiter checks the condition (false), gets preempted, you set the condition and signal (no waiter yet), the waiter then sleeps forever. Modify the state *and* signal while holding the mutex (signalling after unlock is permitted but the state change must be under it).
- **Passing `&i` (loop variable) to every thread** — they all read the final value. One struct per thread, or cast a small integer through `intptr_t`.
- **Returning the address of a local** from the thread function (`return &result;`) — dangling; the thread's stack is gone after it returns. Write into the arg struct or `malloc`.
- **Thread arg struct going out of scope before `join`** — the classic "works in debug, garbage at -O2". Make the array of structs live at least until all joins.
- **Forgetting `-pthread`** (or `-lpthread` on old Linux): compiles on macOS anyway (pthreads live in libSystem) but not portably; always pass it.
- **`pthread_create` returns an error code and leaves `errno` alone** — `perror` prints the wrong message. Use `strerror(rc)`.
- **Double unlock / unlock from the wrong thread** — UB with `NORMAL` mutexes; use `ERRORCHECK` in debug builds to catch it.
- **Destroying a mutex/condvar that a thread is still blocked on** — UB. Join first, destroy last.
- **Lock-order inversion** deadlocks only under load, months later. TSan reports "lock-order-inversion (potential deadlock)" even if it didn't deadlock in the run — heed it.
- **False sharing** silently costs 20-40x (§9). Per-thread partials must be locals, or padded to `hw.cachelinesize` (128 on Apple Silicon — x86 code padded to 64 is still false-shared here).
- **Non-reproducible floating-point sums** across thread counts — not a bug, but pin `T` for regression tests, and compare with a tolerance.
- **Calling non-thread-safe functions from threads**: `strtok`, `rand`, `localtime`, `strerror` (use `strtok_r`, your own RNG per thread, `localtime_r`, `strerror_r`). `printf` is thread-safe but interleaves lines between threads; lock around related prints or buffer per thread.
- **`fork()` in a multithreaded program** copies only the calling thread; any mutex another thread held is locked forever in the child. Only `exec` immediately after (ch. 17).
- **ABA** in lock-free code (§10): CAS compares bits, not identity. Don't free nodes a CAS may still compare.
- **Signed loop counters in OpenMP** with `size_t` bounds — mixed-sign comparisons (`../15_undefined_behavior_and_the_standard/lesson.md`, usual arithmetic conversions); make the loop variable and bound the same type.

## Common mistakes checklist

- [ ] Every shared variable that is ever written is `_Atomic` or protected by exactly one mutex that you can name.
- [ ] Every `pthread_cond_wait` is inside a `while (!condition)`.
- [ ] State change and `signal`/`broadcast` happen with the mutex held.
- [ ] Every `pthread_create` has a matching `join` (or `detach`), and its argument outlives the thread.
- [ ] Threads accumulate into **locals** and combine once; per-thread arrays are cache-line padded if they must exist.
- [ ] The thread count comes from `sysconf(_SC_NPROCESSORS_ONLN)` or a flag, not a hard-coded 8.
- [ ] Work per thread wake-up is ≥ 10 µs; you're not spawning threads inside the inner loop.
- [ ] Multiple locks are always taken in the same global order.
- [ ] The serial version exists, is bit-exact-tested, and the parallel result is compared to it within tolerance every run.
- [ ] You ran it under `-fsanitize=thread` with a small input and got zero reports.
- [ ] You timed with `CLOCK_MONOTONIC`, warm-up, min-of-N, and a T=1/2/4/8 sweep, and you know whether the ceiling you hit is compute, bandwidth, or Amdahl.
- [ ] OpenMP code compiles and gives the same answer *without* `-fopenmp` (serial fallback).

## You can move on when...

- You can explain, without notes, why `counter++` from two threads is UB and not merely "sometimes off by one", and what `-O2` did to the example in §3.
- You can write the bounded buffer from memory with two condvars and shutdown handling, and it passes TSan.
- You can state what release/acquire guarantee in the message-passing example, and what relaxed permits that would break it on arm64.
- You've reproduced the false-sharing measurement (unpadded vs padded vs local) and seen ≥10x.
- You've parallelised a reduction and a matmul by rows, produced the T-sweep table with speedup and GB/s or GFLOP/s, and can say which wall each hits.
- You can read a TSan report and point to the two lines, the variable, and the missing lock.
- You've written the mutex+condvar barrier and used it in a multi-step stencil without a race.
- You know when to type `#pragma omp parallel for reduction(+:s)` instead of any of the above.
