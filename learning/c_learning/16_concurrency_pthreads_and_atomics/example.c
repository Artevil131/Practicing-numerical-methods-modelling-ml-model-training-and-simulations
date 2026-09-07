/*
 * Chapter 16 — Concurrency: pthreads, Atomics, and the Memory Model
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -pthread -o ex_demo example.c -lm
 * Run:      ./ex_demo
 * Race-check the whole thing (slow, small inputs are used automatically):
 *           cc -Wall -Wextra -std=c11 -g -O1 -pthread -fsanitize=thread -o ex_demo example.c -lm && ./ex_demo
 * With OpenMP (macOS, after `brew install libomp`):
 *           clang -Wall -Wextra -std=c11 -O2 -Xpreprocessor -fopenmp \
 *                 -I"$(brew --prefix libomp)/include" -L"$(brew --prefix libomp)/lib" -lomp \
 *                 -o ex_demo example.c -lm
 *           Linux: cc -Wall -Wextra -std=c11 -O2 -fopenmp -o ex_demo example.c -lm
 *
 * C11 <threads.h> is NOT available on macOS, so everything here is POSIX pthreads
 * (man 3 pthread_create) plus C11 <stdatomic.h> (which IS available).
 * All POSIX declarations used here (pthread_*, sysconf, clock_gettime, nanosleep)
 * are visible on macOS under -std=c11. On Linux/glibc add
 *   #define _POSIX_C_SOURCE 200809L
 * as the first line, before any #include.
 *
 * Contents (each section is a function called from main, in order):
 *   1. parallel reduction with per-thread partials: sum of squares of 1e7 doubles,
 *      serial vs T threads, speedup and GB/s
 *   2. producer/consumer bounded buffer: mutex + two condition variables, clean shutdown
 *   3. counters: mutex vs atomic_fetch_add vs thread-local-then-reduce, measured
 *   4. CAS loop: atomic max of a double through its bit pattern
 *   5. false sharing: adjacent vs cache-line-padded per-thread slots, measured
 *   6. _Thread_local: per-thread RNG state, no locks
 *   7. barrier (mutex + condvar + generation counter) driving a 1-D stencil timestep loop
 *   8. counting semaphore built from mutex + condvar (POSIX sem_init is unavailable on macOS)
 *   9. thread pool: task ring buffer + N workers + pool_wait
 *  10. OpenMP version of section 1, compiled only when _OPENMP is defined
 */
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* Under ThreadSanitizer everything is 5-15x slower; shrink the problem sizes so the
 * whole run stays under a few seconds. __has_feature is a clang extension. */
#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define UNDER_TSAN 1
#  endif
#endif
#ifndef UNDER_TSAN
#  define UNDER_TSAN 0
#endif

#define MAX_THREADS 64
#define CACHE_LINE 128            /* Apple Silicon: sysctl -n hw.cachelinesize -> 128; x86 -> 64 */

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* pthread_* functions RETURN the error code and do not set errno, so perror()
 * would print garbage. This macro prints the right message and aborts. */
#define PT(call)                                                            \
    do {                                                                    \
        int rc_ = (call);                                                   \
        if (rc_ != 0) {                                                     \
            fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, #call,   \
                    strerror(rc_));                                         \
            abort();                                                        \
        }                                                                   \
    } while (0)

/* Wall-clock time in seconds. CLOCK_MONOTONIC: never jumps, unlike CLOCK_REALTIME.
 * Never use clock() to time threads: it sums CPU time across all of them. */
static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int ncpu(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);       /* POSIX; macOS: also sysctl -n hw.ncpu */
    if (n < 1) n = 1;
    if (n > MAX_THREADS) n = MAX_THREADS;
    return (int)n;
}

/* ------------------------------------------------------------------ */
/* 1. Parallel reduction with per-thread partials                       */
/* ------------------------------------------------------------------ */

/* Everything a worker needs goes IN through this struct and the result comes OUT
 * through it. The struct lives in main's frame, which outlives the thread because
 * main joins before returning. Never pass &loop_counter to every thread. */
typedef struct {
    const double *x;
    size_t lo, hi;                /* half-open range [lo, hi) of indices this thread owns */
    double partial;               /* result: written by exactly one thread, read after join */
} ReduceJob;

static void *sum_squares_worker(void *arg) {
    ReduceJob *j = arg;
    double s = 0.0;               /* LOCAL accumulator: no sharing, no false sharing, no atomics */
    for (size_t i = j->lo; i < j->hi; i++) s += j->x[i] * j->x[i];
    j->partial = s;               /* one write at the end */
    return NULL;
}

static double sum_squares_serial(const double *x, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += x[i] * x[i];
    return s;
}

static double sum_squares_threads(const double *x, size_t n, int T) {
    pthread_t th[MAX_THREADS];
    ReduceJob job[MAX_THREADS];
    for (int t = 0; t < T; t++) {
        job[t].x = x;
        job[t].lo = n * (size_t)t / (size_t)T;          /* equal contiguous chunks */
        job[t].hi = n * (size_t)(t + 1) / (size_t)T;
        job[t].partial = 0.0;
        PT(pthread_create(&th[t], NULL, sum_squares_worker, &job[t]));
    }
    double total = 0.0;
    for (int t = 0; t < T; t++) {
        PT(pthread_join(th[t], NULL));                  /* join = "wait until done"; also makes */
        total += job[t].partial;                        /* the thread's writes visible to us   */
    }
    return total;
}

static void demo_reduction(void) {
    puts("== 1. parallel reduction: sum of squares ==");
    size_t n = UNDER_TSAN ? 200000u : 10000000u;
    double *x = malloc(n * sizeof *x);
    if (!x) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < n; i++) x[i] = (double)(i % 7) * 0.5;

    /* warm-up touches every page (first-touch page faults would otherwise land in the
     * first timed run), then best-of-3 for the serial baseline.
     * Calling through a volatile function pointer stops clang from noticing that three
     * calls with identical arguments to a pure function can be computed once (chapter 13,
     * "defeating the optimizer"). Without it the serial time prints as 0.00 ms. */
    double (*volatile serial_fn)(const double *, size_t) = sum_squares_serial;
    double ref = serial_fn(x, n), t_serial = 1e9;
    for (int r = 0; r < 3; r++) {
        double t0 = now_s();
        ref = serial_fn(x, n);
        double dt = now_s() - t0;
        if (dt < t_serial) t_serial = dt;
    }
    printf("n = %zu doubles (%.0f MB), %d CPUs online\n", n, (double)n * 8 / 1e6, ncpu());
    printf("serial           %8.2f ms  %6.1f GB/s   sum = %.6e\n",
           t_serial * 1e3, (double)n * 8 / t_serial / 1e9, ref);

    for (int T = 1; T <= ncpu(); T *= 2) {
        double best = 1e9, got = 0;
        for (int r = 0; r < 3; r++) {
            double t0 = now_s();
            got = sum_squares_threads(x, n, T);
            double dt = now_s() - t0;
            if (dt < best) best = dt;
        }
        /* Floating-point addition is not associative: the threaded sum differs from the
         * serial one in the last bits, and differs BY THREAD COUNT. Compare with a tolerance. */
        int ok = fabs(got - ref) <= 1e-9 * fabs(ref);
        printf("T = %2d           %8.2f ms  %6.1f GB/s   speedup %.2fx  %s\n", T, best * 1e3,
               (double)n * 8 / best / 1e9, t_serial / best, ok ? "OK" : "MISMATCH");
    }
    puts("(speedup flattens once memory bandwidth is saturated, not at the core count)");
    free(x);
}

/* ------------------------------------------------------------------ */
/* 2. Producer/consumer bounded buffer                                  */
/* ------------------------------------------------------------------ */

#define QCAP 8
typedef struct {
    int buf[QCAP];
    size_t head, tail, count;
    int closed;                   /* set once by main; consumers drain then return -1 */
    pthread_mutex_t mu;
    pthread_cond_t not_empty;     /* consumers wait on this */
    pthread_cond_t not_full;      /* producers wait on this */
} Queue;

static void q_init(Queue *q) {
    memset(q, 0, sizeof *q);
    PT(pthread_mutex_init(&q->mu, NULL));
    PT(pthread_cond_init(&q->not_empty, NULL));
    PT(pthread_cond_init(&q->not_full, NULL));
}
static void q_destroy(Queue *q) {            /* only after every user thread has been joined */
    PT(pthread_mutex_destroy(&q->mu));
    PT(pthread_cond_destroy(&q->not_empty));
    PT(pthread_cond_destroy(&q->not_full));
}

static void q_push(Queue *q, int v) {
    PT(pthread_mutex_lock(&q->mu));
    while (q->count == QCAP)                 /* WHILE, never if: spurious wakeups are legal, and */
        PT(pthread_cond_wait(&q->not_full, &q->mu));   /* another producer may have refilled it */
    q->buf[q->tail] = v;
    q->tail = (q->tail + 1) % QCAP;
    q->count++;
    PT(pthread_cond_signal(&q->not_empty));  /* signal while holding the mutex: no lost wakeup */
    PT(pthread_mutex_unlock(&q->mu));
}

/* returns -1 when the queue is closed AND empty */
static int q_pop(Queue *q) {
    PT(pthread_mutex_lock(&q->mu));
    while (q->count == 0 && !q->closed)
        PT(pthread_cond_wait(&q->not_empty, &q->mu));
    if (q->count == 0) {                     /* closed and drained */
        PT(pthread_mutex_unlock(&q->mu));
        return -1;
    }
    int v = q->buf[q->head];
    q->head = (q->head + 1) % QCAP;
    q->count--;
    PT(pthread_cond_signal(&q->not_full));
    PT(pthread_mutex_unlock(&q->mu));
    return v;
}

static void q_close(Queue *q) {
    PT(pthread_mutex_lock(&q->mu));
    q->closed = 1;
    PT(pthread_cond_broadcast(&q->not_empty));   /* wake EVERY consumer so each re-checks */
    PT(pthread_mutex_unlock(&q->mu));
}

typedef struct { Queue *q; int id, n; long sum; } PCJob;

static void *producer(void *arg) {
    PCJob *j = arg;
    for (int k = 0; k < j->n; k++) q_push(j->q, j->id * 100000 + k);
    return NULL;
}
static void *consumer(void *arg) {
    PCJob *j = arg;
    long s = 0;
    int v;
    while ((v = q_pop(j->q)) != -1) s += v;
    j->sum = s;
    return NULL;
}

static void demo_producer_consumer(void) {
    puts("\n== 2. producer/consumer bounded buffer (cap 8) ==");
    enum { P = 3, C = 2, M = 20000 };
    Queue q;
    q_init(&q);
    pthread_t pt[P], ct[C];
    PCJob pj[P], cj[C];
    for (int i = 0; i < P; i++) { pj[i] = (PCJob){ &q, i, M, 0 }; PT(pthread_create(&pt[i], NULL, producer, &pj[i])); }
    for (int i = 0; i < C; i++) { cj[i] = (PCJob){ &q, i, 0, 0 }; PT(pthread_create(&ct[i], NULL, consumer, &cj[i])); }
    for (int i = 0; i < P; i++) PT(pthread_join(pt[i], NULL));
    q_close(&q);                                  /* producers done -> tell consumers to drain */
    long total = 0;
    for (int i = 0; i < C; i++) { PT(pthread_join(ct[i], NULL)); total += cj[i].sum; }
    q_destroy(&q);

    long expect = 0;                              /* closed form of sum over id*100000 + k */
    for (int i = 0; i < P; i++) expect += (long)i * 100000L * M + (long)M * (M - 1) / 2;
    printf("%d producers x %d items -> %d consumers: sum %ld, expected %ld  %s\n", P, M, C, total,
           expect, total == expect ? "OK" : "MISMATCH");
}

/* ------------------------------------------------------------------ */
/* 3. Counters: mutex vs atomic vs thread-local                         */
/* ------------------------------------------------------------------ */

static long g_mutex_counter = 0;
static pthread_mutex_t g_counter_mu = PTHREAD_MUTEX_INITIALIZER;
static atomic_long g_atomic_counter = 0;

typedef struct { long n; long local; } CountJob;

static void *count_mutex(void *arg) {
    CountJob *j = arg;
    for (long k = 0; k < j->n; k++) {
        PT(pthread_mutex_lock(&g_counter_mu));
        g_mutex_counter++;                        /* the read-modify-write is now indivisible */
        PT(pthread_mutex_unlock(&g_counter_mu));
    }
    return NULL;
}
static void *count_atomic(void *arg) {
    CountJob *j = arg;
    for (long k = 0; k < j->n; k++)
        atomic_fetch_add_explicit(&g_atomic_counter, 1, memory_order_relaxed);
        /* relaxed is enough: nobody uses the counter to order OTHER memory */
    return NULL;
}
static void *count_local(void *arg) {
    CountJob *j = arg;
    long c = 0;
    for (long k = 0; k < j->n; k++) c++;         /* the compiler may fold this to c = n: fine, */
    j->local = c;                                 /* the point is the pattern, not the loop     */
    return NULL;
}

static double run_counter(void *(*fn)(void *), int T, long per_thread, long *result) {
    pthread_t th[MAX_THREADS];
    CountJob job[MAX_THREADS];
    double t0 = now_s();
    for (int t = 0; t < T; t++) { job[t] = (CountJob){ per_thread, 0 }; PT(pthread_create(&th[t], NULL, fn, &job[t])); }
    long local_total = 0;
    for (int t = 0; t < T; t++) { PT(pthread_join(th[t], NULL)); local_total += job[t].local; }
    double dt = now_s() - t0;
    if (fn == count_mutex) *result = g_mutex_counter;
    else if (fn == count_atomic) *result = atomic_load(&g_atomic_counter);
    else *result = local_total;
    return dt;
}

static void demo_counters(void) {
    puts("\n== 3. shared counter, 4 threads ==");
    const int T = 4;
    const long per = UNDER_TSAN ? 50000L : 2500000L, total = per * T;
    long got;
    double dt;
    dt = run_counter(count_mutex, T, per, &got);
    printf("mutex lock/++/unlock : %ld  %7.1f ms  %5.1f ns/op  %s\n", got, dt * 1e3, dt / (double)total * 1e9, got == total ? "OK" : "WRONG");
    dt = run_counter(count_atomic, T, per, &got);
    printf("atomic_fetch_add     : %ld  %7.1f ms  %5.1f ns/op  %s\n", got, dt * 1e3, dt / (double)total * 1e9, got == total ? "OK" : "WRONG");
    dt = run_counter(count_local, T, per, &got);
    printf("local + reduce       : %ld  %7.1f ms  %5.1f ns/op  %s\n", got, dt * 1e3, dt / (double)total * 1e9, got == total ? "OK" : "WRONG");
    puts("(contended mutex and atomic both pay for bouncing ONE cache line between cores;\n"
         " a plain `counter++` here would be a data race = undefined behaviour, so it is not shown)");
}

/* ------------------------------------------------------------------ */
/* 4. CAS loop: atomic max of a double                                  */
/* ------------------------------------------------------------------ */

/* There is no atomic_fetch_max, and double is not an atomic arithmetic type. Store the
 * double's bit pattern in a 64-bit atomic and use compare-exchange. Type punning goes
 * through memcpy (chapter 15: the union/cast routes are not portable). */
static _Atomic uint64_t g_max_bits;

static void atomic_max_double(double v) {
    uint64_t newb;
    memcpy(&newb, &v, sizeof newb);
    uint64_t old = atomic_load_explicit(&g_max_bits, memory_order_relaxed);
    for (;;) {
        double oldv;
        memcpy(&oldv, &old, sizeof oldv);
        if (!(v > oldv)) return;              /* current max already >= v */
        /* _weak may fail spuriously (arm64 ldxr/stxr), which is fine in a loop.
         * On failure `old` is refreshed with the value actually seen, so we re-check. */
        if (atomic_compare_exchange_weak_explicit(&g_max_bits, &old, newb,
                                                  memory_order_relaxed, memory_order_relaxed))
            return;
    }
}

typedef struct { const double *x; size_t lo, hi; } MaxJob;
static void *max_worker(void *arg) {
    MaxJob *j = arg;
    double local = -INFINITY;                 /* reduce locally first ... */
    for (size_t i = j->lo; i < j->hi; i++) if (j->x[i] > local) local = j->x[i];
    atomic_max_double(local);                 /* ... then ONE CAS per thread, not one per element */
    return NULL;
}

static void demo_cas_max(void) {
    puts("\n== 4. compare-exchange loop: atomic max of a double ==");
    size_t n = 1000000;
    double *x = malloc(n * sizeof *x);
    if (!x) { perror("malloc"); exit(1); }
    unsigned s = 12345u;
    double ref = -INFINITY;
    for (size_t i = 0; i < n; i++) {
        s = s * 1103515245u + 12345u;
        x[i] = (double)(s >> 8) / 16777216.0 * 1000.0 - 500.0;   /* uniform in [-500, 500) */
        if (x[i] > ref) ref = x[i];
    }
    double neg_inf = -INFINITY;
    uint64_t init;
    memcpy(&init, &neg_inf, sizeof init);
    atomic_store(&g_max_bits, init);

    const int T = 4;
    pthread_t th[T];
    MaxJob job[T];
    for (int t = 0; t < T; t++) { job[t] = (MaxJob){ x, n * (size_t)t / T, n * (size_t)(t + 1) / T }; PT(pthread_create(&th[t], NULL, max_worker, &job[t])); }
    for (int t = 0; t < T; t++) PT(pthread_join(th[t], NULL));
    uint64_t bits = atomic_load(&g_max_bits);
    double got;
    memcpy(&got, &bits, sizeof got);
    printf("max over %zu values: parallel %.6f  serial %.6f  %s\n", n, got, ref, got == ref ? "OK" : "MISMATCH");
    free(x);
}

/* ------------------------------------------------------------------ */
/* 5. False sharing                                                     */
/* ------------------------------------------------------------------ */

/* Four "independent" per-thread counters. In `unpadded` all four share one 128-byte
 * cache line, so every increment on one core invalidates the line in the other three.
 * `padded` gives each its own line. Same instructions, very different speed. */
struct Unpadded { atomic_long v; };
struct Padded { _Alignas(CACHE_LINE) atomic_long v; };   /* _Alignas: C11 6.7.5; sizeof becomes 128 */

static struct Unpadded g_unpadded[4];
static struct Padded g_padded[4];

typedef struct { atomic_long *slot; long n; } FSJob;
static void *fs_worker(void *arg) {
    FSJob *j = arg;
    for (long k = 0; k < j->n; k++) atomic_fetch_add_explicit(j->slot, 1, memory_order_relaxed);
    return NULL;
}

static double run_false_sharing(atomic_long *slots[4], long n) {
    pthread_t th[4];
    FSJob job[4];
    double t0 = now_s();
    for (int t = 0; t < 4; t++) { job[t] = (FSJob){ slots[t], n }; PT(pthread_create(&th[t], NULL, fs_worker, &job[t])); }
    for (int t = 0; t < 4; t++) PT(pthread_join(th[t], NULL));
    return now_s() - t0;
}

static void demo_false_sharing(void) {
    puts("\n== 5. false sharing: 4 threads, each incrementing ITS OWN counter ==");
    const long n = UNDER_TSAN ? 50000L : 5000000L;
    atomic_long *up[4], *pd[4];
    for (int i = 0; i < 4; i++) { up[i] = &g_unpadded[i].v; pd[i] = &g_padded[i].v; }
    double best_up = 1e9, best_pd = 1e9;
    for (int r = 0; r < 3; r++) {
        double a = run_false_sharing(up, n), b = run_false_sharing(pd, n);
        if (a < best_up) best_up = a;
        if (b < best_pd) best_pd = b;
    }
    printf("sizeof(Unpadded) = %zu, sizeof(Padded) = %zu (cache line assumed %d B)\n",
           sizeof(struct Unpadded), sizeof(struct Padded), CACHE_LINE);
    printf("adjacent slots   : %7.1f ms  %5.2f ns/increment\n", best_up * 1e3, best_up / (double)(4 * n) * 1e9);
    printf("padded slots     : %7.1f ms  %5.2f ns/increment   (%.1fx faster)\n", best_pd * 1e3, best_pd / (double)(4 * n) * 1e9, best_up / best_pd);
    puts("(the real fix is section 1's pattern: accumulate in a local, store once)");
}

/* ------------------------------------------------------------------ */
/* 6. Thread-local storage                                              */
/* ------------------------------------------------------------------ */

/* Each thread gets its own copy, initialised from a constant expression at thread start.
 * C11 keyword (6.7.1); `__thread` is the GNU/clang spelling. errno works this way. */
static _Thread_local uint64_t tls_rng_state = 0x9E3779B97F4A7C15ull;

static uint64_t tls_rand(void) {                          /* xorshift64*; no lock needed */
    uint64_t x = tls_rng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    tls_rng_state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

typedef struct { int id; double estimate; } PiJob;
static void *pi_worker(void *arg) {
    PiJob *j = arg;
    tls_rng_state ^= (uint64_t)(j->id + 1) * 0xD1B54A32D192ED03ull;  /* distinct stream per thread */
    const long n = UNDER_TSAN ? 20000L : 1000000L;
    long inside = 0;
    for (long k = 0; k < n; k++) {
        double u = (double)(tls_rand() >> 11) / 9007199254740992.0;  /* [0,1) from 53 bits */
        double v = (double)(tls_rand() >> 11) / 9007199254740992.0;
        if (u * u + v * v < 1.0) inside++;
    }
    j->estimate = 4.0 * (double)inside / (double)n;
    return NULL;
}

static void demo_thread_local(void) {
    puts("\n== 6. _Thread_local: per-thread RNG, Monte Carlo pi ==");
    const int T = 4;
    pthread_t th[T];
    PiJob job[T];
    for (int t = 0; t < T; t++) { job[t] = (PiJob){ t, 0 }; PT(pthread_create(&th[t], NULL, pi_worker, &job[t])); }
    double mean = 0;
    for (int t = 0; t < T; t++) { PT(pthread_join(th[t], NULL)); mean += job[t].estimate / T; printf("thread %d: pi ~ %.5f\n", t, job[t].estimate); }
    printf("mean of %d independent streams: %.5f (pi = %.5f); main thread's state untouched: %#llx\n",
           T, mean, M_PI, (unsigned long long)tls_rng_state);
}

/* ------------------------------------------------------------------ */
/* 7. Barrier + stencil timestep loop                                   */
/* ------------------------------------------------------------------ */

/* pthread_barrier_t is optional POSIX and macOS does not ship it. This is the portable
 * version. The generation counter matters: a fast thread may leave and re-enter the
 * barrier before a slow one has woken; waiting on `generation != my_gen` (not on
 * `waiting == 0`) keeps rounds from bleeding into each other. */
typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int n, waiting, generation;
} Barrier;

static void barrier_init(Barrier *b, int n) {
    PT(pthread_mutex_init(&b->mu, NULL));
    PT(pthread_cond_init(&b->cv, NULL));
    b->n = n; b->waiting = 0; b->generation = 0;
}
static void barrier_destroy(Barrier *b) { PT(pthread_mutex_destroy(&b->mu)); PT(pthread_cond_destroy(&b->cv)); }

static void barrier_wait(Barrier *b) {
    PT(pthread_mutex_lock(&b->mu));
    int my_gen = b->generation;
    if (++b->waiting == b->n) {              /* last to arrive releases everyone */
        b->waiting = 0;
        b->generation++;
        PT(pthread_cond_broadcast(&b->cv));
    } else {
        while (my_gen == b->generation) PT(pthread_cond_wait(&b->cv, &b->mu));
    }
    PT(pthread_mutex_unlock(&b->mu));
}

/* 1-D heat equation, explicit Euler: u_new[i] = u[i] + r*(u[i-1] - 2u[i] + u[i+1]).
 * Thread t owns cells [lo, hi) of u_new and reads u[lo-1 .. hi] (one halo cell each side,
 * owned by a neighbour). The barrier guarantees every thread finished step k before
 * anyone reads step k's result as input to step k+1. */
#define GRID 4096
typedef struct {
    double *u, *u_new;            /* each thread swaps ITS OWN copies of these after the barrier */
    size_t lo, hi;
    int steps;
    Barrier *bar;
} StencilJob;

static void *stencil_worker(void *arg) {
    StencilJob *j = arg;
    const double r = 0.25;
    for (int s = 0; s < j->steps; s++) {
        for (size_t i = j->lo; i < j->hi; i++) {
            if (i == 0 || i == GRID - 1) { j->u_new[i] = 0.0; continue; }   /* fixed boundary */
            j->u_new[i] = j->u[i] + r * (j->u[i - 1] - 2.0 * j->u[i] + j->u[i + 1]);
        }
        barrier_wait(j->bar);     /* everyone has written step s */
        double *tmp = j->u; j->u = j->u_new; j->u_new = tmp;   /* local swap: no race */
    }
    return NULL;
}

static void demo_barrier_stencil(void) {
    puts("\n== 7. barrier: 1-D heat equation, 4 threads, one barrier per timestep ==");
    const int T = 4, steps = UNDER_TSAN ? 50 : 400;
    double *a = calloc(GRID, sizeof *a), *b = calloc(GRID, sizeof *b);
    double *sa = calloc(GRID, sizeof *sa), *sb = calloc(GRID, sizeof *sb);
    if (!a || !b || !sa || !sb) { perror("calloc"); exit(1); }
    for (size_t i = GRID / 2 - 50; i < GRID / 2 + 50; i++) a[i] = sa[i] = 1.0;   /* hot bar in the middle */

    /* serial reference */
    for (int s = 0; s < steps; s++) {
        for (size_t i = 1; i < GRID - 1; i++) sb[i] = sa[i] + 0.25 * (sa[i - 1] - 2.0 * sa[i] + sa[i + 1]);
        double *tmp = sa; sa = sb; sb = tmp;
    }

    Barrier bar;
    barrier_init(&bar, T);
    pthread_t th[T];
    StencilJob job[T];
    for (int t = 0; t < T; t++) {
        job[t] = (StencilJob){ a, b, GRID * (size_t)t / T, GRID * (size_t)(t + 1) / T, steps, &bar };
        PT(pthread_create(&th[t], NULL, stencil_worker, &job[t]));
    }
    for (int t = 0; t < T; t++) PT(pthread_join(th[t], NULL));
    barrier_destroy(&bar);

    /* after an even/odd number of steps the result lives in job[0].u (all threads agree) */
    const double *par = job[0].u;
    double maxdiff = 0, total = 0;
    for (size_t i = 0; i < GRID; i++) { maxdiff = fmax(maxdiff, fabs(par[i] - sa[i])); total += par[i]; }
    printf("%d steps on %d cells: max |parallel - serial| = %.3g (identical arithmetic -> expect 0), heat = %.6f\n",
           steps, GRID, maxdiff, total);
    printf("barrier crossings per thread: %d\n", steps);
    free(a); free(b); free(sa); free(sb);
}

/* ------------------------------------------------------------------ */
/* 8. Counting semaphore from mutex + condvar                           */
/* ------------------------------------------------------------------ */

/* POSIX unnamed semaphores (sem_init) are not implemented on macOS (returns -1, ENOSYS).
 * Named sem_open works, and Apple's dispatch_semaphore works, but a counter under a
 * mutex is portable and 20 lines. Unlike a condvar, a semaphore REMEMBERS posts. */
typedef struct { pthread_mutex_t mu; pthread_cond_t cv; int count; } Sem;

static void sem_init_(Sem *s, int initial) { PT(pthread_mutex_init(&s->mu, NULL)); PT(pthread_cond_init(&s->cv, NULL)); s->count = initial; }
static void sem_destroy_(Sem *s) { PT(pthread_mutex_destroy(&s->mu)); PT(pthread_cond_destroy(&s->cv)); }
static void sem_wait_(Sem *s) {
    PT(pthread_mutex_lock(&s->mu));
    while (s->count == 0) PT(pthread_cond_wait(&s->cv, &s->mu));
    s->count--;
    PT(pthread_mutex_unlock(&s->mu));
}
static void sem_post_(Sem *s) {
    PT(pthread_mutex_lock(&s->mu));
    s->count++;
    PT(pthread_cond_signal(&s->cv));
    PT(pthread_mutex_unlock(&s->mu));
}

/* limit concurrency: at most `permits` of 8 threads inside the "GPU" at once */
static Sem g_gpu_sem;
static atomic_int g_inside = 0, g_max_inside = 0;

static void *sem_worker(void *arg) {
    (void)arg;
    for (int k = 0; k < 50; k++) {
        sem_wait_(&g_gpu_sem);
        int cur = atomic_fetch_add(&g_inside, 1) + 1;
        /* CAS loop for atomic max of an int, same idea as section 4 */
        int seen = atomic_load(&g_max_inside);
        while (cur > seen && !atomic_compare_exchange_weak(&g_max_inside, &seen, cur)) { }
        struct timespec nap = { 0, 20000 };   /* 20 us of "work" */
        nanosleep(&nap, NULL);
        atomic_fetch_sub(&g_inside, 1);
        sem_post_(&g_gpu_sem);
    }
    return NULL;
}

static void demo_semaphore(void) {
    puts("\n== 8. counting semaphore (mutex + condvar): 8 threads, 3 permits ==");
    sem_init_(&g_gpu_sem, 3);
    pthread_t th[8];
    for (int t = 0; t < 8; t++) PT(pthread_create(&th[t], NULL, sem_worker, NULL));
    for (int t = 0; t < 8; t++) PT(pthread_join(th[t], NULL));
    sem_destroy_(&g_gpu_sem);
    printf("max threads simultaneously inside: %d (permits = 3)  %s\n", atomic_load(&g_max_inside),
           atomic_load(&g_max_inside) <= 3 ? "OK" : "VIOLATED");
}

/* ------------------------------------------------------------------ */
/* 9. Thread pool                                                       */
/* ------------------------------------------------------------------ */

#define POOL_QCAP 256
typedef struct { void (*fn)(void *); void *arg; } Task;

typedef struct {
    Task q[POOL_QCAP];
    size_t head, tail, count;
    size_t pending;               /* submitted but not yet finished (for pool_wait) */
    int stop;
    pthread_mutex_t mu;
    pthread_cond_t has_work;      /* workers wait here */
    pthread_cond_t not_full;      /* submitters wait here when the ring is full */
    pthread_cond_t all_done;      /* pool_wait waits here */
    pthread_t th[MAX_THREADS];
    int n;
} Pool;

static void *pool_worker(void *arg) {
    Pool *p = arg;
    for (;;) {
        PT(pthread_mutex_lock(&p->mu));
        while (p->count == 0 && !p->stop) PT(pthread_cond_wait(&p->has_work, &p->mu));
        if (p->count == 0 && p->stop) { PT(pthread_mutex_unlock(&p->mu)); return NULL; }
        Task t = p->q[p->head];
        p->head = (p->head + 1) % POOL_QCAP;
        p->count--;
        PT(pthread_cond_signal(&p->not_full));
        PT(pthread_mutex_unlock(&p->mu));

        t.fn(t.arg);                                  /* run OUTSIDE the lock or the pool is serial */

        PT(pthread_mutex_lock(&p->mu));
        if (--p->pending == 0) PT(pthread_cond_broadcast(&p->all_done));
        PT(pthread_mutex_unlock(&p->mu));
    }
}

static void pool_create(Pool *p, int n) {
    memset(p, 0, sizeof *p);
    PT(pthread_mutex_init(&p->mu, NULL));
    PT(pthread_cond_init(&p->has_work, NULL));
    PT(pthread_cond_init(&p->not_full, NULL));
    PT(pthread_cond_init(&p->all_done, NULL));
    p->n = n;
    for (int i = 0; i < n; i++) PT(pthread_create(&p->th[i], NULL, pool_worker, p));
}

static void pool_submit(Pool *p, void (*fn)(void *), void *arg) {
    PT(pthread_mutex_lock(&p->mu));
    while (p->count == POOL_QCAP) PT(pthread_cond_wait(&p->not_full, &p->mu));
    p->q[p->tail] = (Task){ fn, arg };
    p->tail = (p->tail + 1) % POOL_QCAP;
    p->count++;
    p->pending++;
    PT(pthread_cond_signal(&p->has_work));
    PT(pthread_mutex_unlock(&p->mu));
}

static void pool_wait(Pool *p) {                   /* block until every submitted task finished */
    PT(pthread_mutex_lock(&p->mu));
    while (p->pending != 0) PT(pthread_cond_wait(&p->all_done, &p->mu));
    PT(pthread_mutex_unlock(&p->mu));
}

static void pool_destroy(Pool *p) {
    PT(pthread_mutex_lock(&p->mu));
    p->stop = 1;
    PT(pthread_cond_broadcast(&p->has_work));
    PT(pthread_mutex_unlock(&p->mu));
    for (int i = 0; i < p->n; i++) PT(pthread_join(p->th[i], NULL));
    PT(pthread_mutex_destroy(&p->mu));
    PT(pthread_cond_destroy(&p->has_work));
    PT(pthread_cond_destroy(&p->not_full));
    PT(pthread_cond_destroy(&p->all_done));
}

/* task: one row of a matrix-vector product y = A x  (A is n x n row-major) */
typedef struct { const double *A, *x; double *y; size_t n, row; } MatVecTask;
static void matvec_row(void *arg) {
    MatVecTask *t = arg;
    double s = 0;
    const double *a = t->A + t->row * t->n;
    for (size_t j = 0; j < t->n; j++) s += a[j] * t->x[j];
    t->y[t->row] = s;                          /* each task writes a distinct y[row]: no lock */
}

static void demo_thread_pool(void) {
    puts("\n== 9. thread pool: matrix-vector product, one task per row ==");
    const size_t n = UNDER_TSAN ? 256 : 2048;
    double *A = malloc(n * n * sizeof *A), *x = malloc(n * sizeof *x);
    double *y = malloc(n * sizeof *y), *yref = malloc(n * sizeof *yref);
    MatVecTask *tasks = malloc(n * sizeof *tasks);
    if (!A || !x || !y || !yref || !tasks) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < n * n; i++) A[i] = (double)((i * 7919u) % 1000) / 1000.0;
    for (size_t i = 0; i < n; i++) x[i] = (double)(i % 13) - 6.0;

    double t0 = now_s();
    for (size_t i = 0; i < n; i++) { double s = 0; for (size_t j = 0; j < n; j++) s += A[i * n + j] * x[j]; yref[i] = s; }
    double t_serial = now_s() - t0;

    Pool pool;
    pool_create(&pool, ncpu());
    t0 = now_s();
    for (size_t i = 0; i < n; i++) { tasks[i] = (MatVecTask){ A, x, y, n, i }; pool_submit(&pool, matvec_row, &tasks[i]); }
    pool_wait(&pool);
    double t_pool = now_s() - t0;
    pool_destroy(&pool);

    double maxdiff = 0;
    for (size_t i = 0; i < n; i++) maxdiff = fmax(maxdiff, fabs(y[i] - yref[i]));
    printf("n = %zu, %d workers: serial %.2f ms, pool %.2f ms (%.2fx), max diff %.3g\n", n, ncpu(),
           t_serial * 1e3, t_pool * 1e3, t_serial / t_pool, maxdiff);
    printf("row work ~%.2f us per task; pool spends %.2f us wall per task including queue handoff\n"
           "(lesson section 4: ~1.7 us per mutex+condvar handoff -> batch rows per task, or use\n"
           " section 1's static partition, when the work per task is this small)\n",
           t_serial / (double)n * 1e6, t_pool / (double)n * 1e6);
    free(A); free(x); free(y); free(yref); free(tasks);
}

/* ------------------------------------------------------------------ */
/* 10. OpenMP                                                           */
/* ------------------------------------------------------------------ */

static void demo_openmp(void) {
    puts("\n== 10. OpenMP ==");
#ifdef _OPENMP
    printf("_OPENMP = %d, omp_get_max_threads() = %d\n", _OPENMP, omp_get_max_threads());
    const size_t n = 10000000;
    double *x = malloc(n * sizeof *x);
    if (!x) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < n; i++) x[i] = (double)(i % 7) * 0.5;
    double ref = sum_squares_serial(x, n);

    double s = 0.0;
    double t0 = omp_get_wtime();
    /* reduction(+:s): a private s per thread, combined at the end == section 1 by hand */
    #pragma omp parallel for reduction(+:s) schedule(static)
    for (size_t i = 0; i < n; i++) s += x[i] * x[i];
    double dt = omp_get_wtime() - t0;
    printf("omp parallel for reduction: %.6e in %.2f ms  %s\n", s, dt * 1e3,
           fabs(s - ref) <= 1e-9 * fabs(ref) ? "OK" : "MISMATCH");

    long hist[7] = { 0 };
    #pragma omp parallel for
    for (size_t i = 0; i < n; i++) {
        int bin = (int)(x[i] / 0.5);           /* 0..6 */
        #pragma omp atomic                     /* single atomic RMW; `critical` would be a mutex */
        hist[bin]++;
    }
    printf("omp atomic histogram: bin0 = %ld (expect %zu)\n", hist[0], (n + 6) / 7);
    free(x);
#else
    puts("compiled without OpenMP (_OPENMP undefined): the pragmas would be ignored and the code");
    puts("would run serially. See lesson section 14 for the macOS `brew install libomp` incantation.");
#endif
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("chapter 16 demo: %d CPUs online, cache line assumed %d B%s\n", ncpu(), CACHE_LINE,
           UNDER_TSAN ? " [ThreadSanitizer build: sizes reduced]" : "");
    demo_reduction();
    demo_producer_consumer();
    demo_counters();
    demo_cas_max();
    demo_false_sharing();
    demo_thread_local();
    demo_barrier_stencil();
    demo_semaphore();
    demo_thread_pool();
    demo_openmp();
    return 0;
}
