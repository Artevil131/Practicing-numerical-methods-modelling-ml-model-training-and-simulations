/*
 * Chapter 19 — SIMD and Low-Level Performance
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo            (matrix size defaults to 512; ./ex_demo 1024 for a longer run)
 *
 * Also try:
 *   cc -Wall -Wextra -std=c11 -O3 -march=native -o ex_demo example.c -lm            (best auto-vectorization)
 *   cc -Wall -Wextra -std=c11 -O3 -march=native -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
 *      -c -o /dev/null example.c 2>&1 | grep -E "vectorized|not vectorized"        (what the compiler did)
 *   cc -Wall -Wextra -std=c11 -O3 -march=native -S -o example.s example.c && grep -c fmla example.s
 *   cc -Wall -Wextra -std=c11 -O3 -march=native -ffast-math -o ex_demo example.c -lm (reassociation: see §6)
 *   cc -Wall -Wextra -std=c11 -O2 -DUSE_ACCELERATE -framework Accelerate -o ex_demo example.c -lm
 *                                                                        (adds cblas_dgemm to the matmul table)
 *
 * Contents:
 *   1. bench   — micro-benchmark methodology: warm-up, repeats, MEDIAN, a volatile sink
 *   2. roofline— arithmetic intensity of saxpy / dot / matmul and where the ridge point is
 *   3. dot     — scalar 1-accumulator vs 4 accumulators (dependency chains) vs NEON intrinsics
 *                vs clang vector extensions; all verified equal
 *   4. saxpy   — float32 NEON vs scalar at L1 size and at DRAM size: compute-bound vs memory-bound
 *   5. matmul  — naive ijk -> ikj (restrict) -> cache-blocked -> packed 4x4 NEON micro-kernel
 *                [-> cblas_dgemm]; GFLOP/s for each, verified against naive
 *   6. fma     — FMA accuracy: fma(a,b,-c) vs a*b-c
 *   7. branch  — array compaction: branchy vs branchless, random vs sorted input
 *   8. denorm  — subnormal arithmetic speed (x86 pays ~100x; Apple Silicon usually does not)
 *   9. int/flt — integer vs floating-point add throughput with 4 accumulators
 *
 * NEON sections are guarded by #if defined(__ARM_NEON); on x86 they fall back to the portable
 * vector-extension or scalar version so the program still builds and runs everywhere.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#ifdef USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

/* ------------------------------------------------------------------------------------------ */
/* 1. MICRO-BENCHMARK METHODOLOGY                                                              */
/* ------------------------------------------------------------------------------------------ */
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* a volatile sink: writing a result here forces the compiler to actually compute it */
static volatile double g_sink;
static void sink(double x) { g_sink = x; }

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* BENCH(out_seconds, reps, statement): runs `statement` once to warm up (page faults, cache,  */
/* clock frequency ramp), then `reps` times, and stores the MEDIAN time. The median is robust  */
/* to the one run that got descheduled; the minimum is a fine alternative for pure compute.    */
#define BENCH(out, reps, stmt)                                                                   \
    do {                                                                                        \
        double _t[64]; int _r = (reps) > 64 ? 64 : (reps);                                      \
        { stmt; }                                                                               \
        for (int _i = 0; _i < _r; _i++) { double _t0 = now_sec(); { stmt; } _t[_i] = now_sec() - _t0; } \
        qsort(_t, (size_t)_r, sizeof _t[0], cmp_double);                                        \
        (out) = _t[_r / 2];                                                                     \
    } while (0)

static double *alloc_doubles(size_t n) {
    double *p = aligned_alloc(64, ((n * sizeof(double)) + 63) & ~(size_t)63);
    if (!p) { perror("aligned_alloc"); exit(1); }
    return p;
}
static float *alloc_floats(size_t n) {
    float *p = aligned_alloc(64, ((n * sizeof(float)) + 63) & ~(size_t)63);
    if (!p) { perror("aligned_alloc"); exit(1); }
    return p;
}
static void fill_random(double *a, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; a[i] = (double)(seed >> 8) / 16777216.0 - 0.5; }
}

/* ------------------------------------------------------------------------------------------ */
/* 2. ROOFLINE                                                                                 */
/* ------------------------------------------------------------------------------------------ */
static void demo_roofline(size_t n_mat) {
    puts("== 2. roofline: arithmetic intensity ==");
    /* One Apple M-series performance core: 4 NEON pipes x 2 doubles x FMA(2 flops) = 16 flop/cycle.  */
    /* At ~4 GHz that is ~64 GFLOP/s double (128 single) per core. Single-core DRAM bandwidth ~60 GB/s.*/
    const double peak = 64.0, bw = 60.0;
    printf("  assumed one-core peak %.0f GFLOP/s (f64), one-core DRAM bandwidth %.0f GB/s\n", peak, bw);
    printf("  ridge point = peak / bw = %.2f flop/byte: below it you are memory-bound\n\n", peak / bw);
    struct { const char *name; double flops, bytes; } k[] = {
        { "saxpy y=a*x+y (f32)", 2, 12 },                              /* read x,y write y: 3 x 4 B */
        { "dot (f64)",           2, 16 },                              /* read x,y: 2 x 8 B */
        { "N-body pair (f64)",   20, 32 },                             /* read x,y,z,m of j: 4 x 8 B */
        { "matmul naive ijk (f64), per FMA", 2, 16 },                  /* A[i][p] reused, B[p][j] streamed */
        { "matmul, whole N=n (f64), each matrix read once", 2.0 * (double)n_mat * (double)n_mat * (double)n_mat,
          3.0 * 8.0 * (double)n_mat * (double)n_mat },
    };
    printf("  %-48s %10s %10s %12s   %s\n", "kernel", "flops", "bytes", "flop/byte", "bound by");
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) {
        double ai = k[i].flops / k[i].bytes;
        printf("  %-48s %10.3g %10.3g %12.3f   %s (attainable %.1f GFLOP/s)\n", k[i].name, k[i].flops, k[i].bytes, ai,
               ai < peak / bw ? "memory" : "compute", fmin(peak, ai * bw));
    }
    puts("  blocking turns matmul from the 4th row into the 5th: reuse each loaded byte many times.");
}

/* ------------------------------------------------------------------------------------------ */
/* 3. DOT PRODUCT: dependency chains, NEON, vector extensions                                  */
/* ------------------------------------------------------------------------------------------ */
static double dot_scalar(const double *a, const double *b, size_t n) {
    double s = 0;                                  /* ONE accumulator: every FMA waits for the previous one */
    for (size_t i = 0; i < n; i++) s += a[i] * b[i];
    return s;                                      /* latency-bound: ~4 cycles per element */
}

static double dot_scalar4(const double *a, const double *b, size_t n) {
    double s0 = 0, s1 = 0, s2 = 0, s3 = 0;         /* FOUR independent chains: the core overlaps them */
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        s0 += a[i] * b[i]; s1 += a[i + 1] * b[i + 1]; s2 += a[i + 2] * b[i + 2]; s3 += a[i + 3] * b[i + 3];
    }
    for (; i < n; i++) s0 += a[i] * b[i];
    return (s0 + s1) + (s2 + s3);
}

/* portable SIMD: clang/GCC vector extensions. v2d is two doubles; +,*,[] work elementwise. */
typedef double v2d __attribute__((vector_size(16)));
static double dot_vext(const double *a, const double *b, size_t n) {
    v2d s0 = {0, 0}, s1 = {0, 0}, s2 = {0, 0}, s3 = {0, 0};
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        v2d a0, a1, a2, a3, b0, b1, b2, b3;         /* memcpy = unaligned load, always legal */
        memcpy(&a0, a + i, 16); memcpy(&a1, a + i + 2, 16); memcpy(&a2, a + i + 4, 16); memcpy(&a3, a + i + 6, 16);
        memcpy(&b0, b + i, 16); memcpy(&b1, b + i + 2, 16); memcpy(&b2, b + i + 4, 16); memcpy(&b3, b + i + 6, 16);
        s0 += a0 * b0; s1 += a1 * b1; s2 += a2 * b2; s3 += a3 * b3;
    }
    v2d s = (s0 + s1) + (s2 + s3);
    double r = s[0] + s[1];
    for (; i < n; i++) r += a[i] * b[i];
    return r;
}

#if defined(__ARM_NEON)
static double dot_neon(const double *a, const double *b, size_t n) {
    float64x2_t s0 = vdupq_n_f64(0), s1 = vdupq_n_f64(0), s2 = vdupq_n_f64(0), s3 = vdupq_n_f64(0);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {                    /* 4 FMAs per iteration on 4 independent accumulators */
        s0 = vfmaq_f64(s0, vld1q_f64(a + i),     vld1q_f64(b + i));      /* fmla v0.2d, v1.2d, v2.2d */
        s1 = vfmaq_f64(s1, vld1q_f64(a + i + 2), vld1q_f64(b + i + 2));
        s2 = vfmaq_f64(s2, vld1q_f64(a + i + 4), vld1q_f64(b + i + 4));
        s3 = vfmaq_f64(s3, vld1q_f64(a + i + 6), vld1q_f64(b + i + 6));
    }
    double r = vaddvq_f64(vaddq_f64(vaddq_f64(s0, s1), vaddq_f64(s2, s3)));   /* horizontal add */
    for (; i < n; i++) r += a[i] * b[i];
    return r;
}
#endif

static void dot_table(size_t n, int inner) {
    double *a = alloc_doubles(n), *b = alloc_doubles(n);
    fill_random(a, n, 1); fill_random(b, n, 2);
    double t, ref = dot_scalar(a, b, n), r = 0;
    const int reps = 15;
    double flops = 2.0 * (double)n * inner;
    printf("  n=%zu (%zu KB per vector), repeated %d times:\n", n, n * 8 / 1024, inner);
    BENCH(t, reps, for (int k = 0; k < inner; k++) sink(dot_scalar(a, b, n)));
    printf("    %-24s %7.3f ms  %6.2f GFLOP/s\n", "scalar, 1 accumulator", t * 1e3, flops / t * 1e-9);
    BENCH(t, reps, for (int k = 0; k < inner; k++) sink(r = dot_scalar4(a, b, n)));
    printf("    %-24s %7.3f ms  %6.2f GFLOP/s  |diff| %.1e\n", "scalar, 4 accumulators", t * 1e3, flops / t * 1e-9, fabs(r - ref));
    BENCH(t, reps, for (int k = 0; k < inner; k++) sink(r = dot_vext(a, b, n)));
    printf("    %-24s %7.3f ms  %6.2f GFLOP/s  |diff| %.1e\n", "vector ext v2d x4", t * 1e3, flops / t * 1e-9, fabs(r - ref));
#if defined(__ARM_NEON)
    BENCH(t, reps, for (int k = 0; k < inner; k++) sink(r = dot_neon(a, b, n)));
    printf("    %-24s %7.3f ms  %6.2f GFLOP/s  |diff| %.1e\n", "NEON float64x2 x4", t * 1e3, flops / t * 1e-9, fabs(r - ref));
#else
    puts("    (no NEON on this target: SSE2 equivalent is _mm_load_pd / _mm_fmadd_pd, AVX: _mm256_fmadd_pd)");
#endif
    free(a); free(b);
}

static void demo_dot(void) {
    puts("== 3. dot product: dependency chains and SIMD ==");
    dot_table(4096, 256);              /* 32 KB per vector: L1-resident -> compute/latency bound */
    dot_table(1u << 18, 4);            /* 2 MB per vector: L2-resident -> L2 bandwidth bound */
    puts("  1 accumulator: each FMA waits ~4 cycles for the previous one (latency-bound, no matter the");
    puts("  SIMD width). 4 chains overlap. In L2 all the fast versions meet the same bandwidth ceiling.");
    puts("  the |diff|s are rounding: different summation order, same math. -ffast-math lets the compiler");
    puts("  do this reassociation itself, at the cost of bit-reproducible results.");
}

/* ------------------------------------------------------------------------------------------ */
/* 4. SAXPY: compute-bound in L1, memory-bound in DRAM                                         */
/* ------------------------------------------------------------------------------------------ */
static void saxpy_scalar(float a, const float *restrict x, float *restrict y, size_t n) {
    for (size_t i = 0; i < n; i++) y[i] = a * x[i] + y[i];      /* restrict: x and y do not overlap */
}

#if defined(__ARM_NEON)
static void saxpy_neon(float a, const float *restrict x, float *restrict y, size_t n) {
    float32x4_t va = vdupq_n_f32(a);                            /* broadcast a into all 4 lanes */
    size_t i = 0;
    for (; i + 4 <= n; i += 4)
        vst1q_f32(y + i, vfmaq_f32(vld1q_f32(y + i), va, vld1q_f32(x + i)));   /* y = y + a*x */
    for (; i < n; i++) y[i] = a * x[i] + y[i];                  /* tail */
}
#endif

#if defined(__ARM_NEON)
/* 4x unrolled: four independent load/FMA/store groups per iteration keep more memory ops in flight  */
static void saxpy_neon4(float a, const float *restrict x, float *restrict y, size_t n) {
    float32x4_t va = vdupq_n_f32(a);
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        float32x4_t y0 = vld1q_f32(y + i), y1 = vld1q_f32(y + i + 4), y2 = vld1q_f32(y + i + 8), y3 = vld1q_f32(y + i + 12);
        y0 = vfmaq_f32(y0, va, vld1q_f32(x + i));      y1 = vfmaq_f32(y1, va, vld1q_f32(x + i + 4));
        y2 = vfmaq_f32(y2, va, vld1q_f32(x + i + 8));  y3 = vfmaq_f32(y3, va, vld1q_f32(x + i + 12));
        vst1q_f32(y + i, y0); vst1q_f32(y + i + 4, y1); vst1q_f32(y + i + 8, y2); vst1q_f32(y + i + 12, y3);
    }
    for (; i < n; i++) y[i] = a * x[i] + y[i];
}
#endif

static void demo_saxpy(void) {
    puts("== 4. saxpy (f32): L1-resident vs DRAM-resident ==");
    size_t sizes[2] = { 4096, 1u << 24 };                       /* 16 KB per array; 64 MB per array */
    for (int s = 0; s < 2; s++) {
        size_t n = sizes[s];
        float *x = alloc_floats(n), *y = alloc_floats(n), *y2 = alloc_floats(n), *y3 = alloc_floats(n);
        for (size_t i = 0; i < n; i++) { x[i] = (float)(i % 97) * 0.01f; y[i] = y2[i] = y3[i] = 1.0f; }
        int inner = n <= 4096 ? 1000 : 1;                        /* repeat the small case so it is measurable */
        double t_s, t_n = 0, t_n4 = 0;
        BENCH(t_s, 11, for (int k = 0; k < inner; k++) saxpy_scalar(1.0001f, x, y, n));
        double bytes = 12.0 * (double)n * inner;                 /* 3 x 4 B per element */
#if defined(__ARM_NEON)
        BENCH(t_n, 11, for (int k = 0; k < inner; k++) saxpy_neon(1.0001f, x, y2, n));
        BENCH(t_n4, 11, for (int k = 0; k < inner; k++) saxpy_neon4(1.0001f, x, y3, n));
        float maxd = 0; for (size_t i = 0; i < n; i++) maxd = fmaxf(maxd, fmaxf(fabsf(y[i] - y2[i]), fabsf(y[i] - y3[i])));
        printf("  n=%-9zu scalar(auto-vec) %6.3f ms (%5.1f GB/s)  NEON x1 %6.3f ms (%5.1f GB/s)  NEON x4 unrolled %6.3f ms (%5.1f GB/s)  max|diff| %.0e\n",
               n, t_s * 1e3, bytes / t_s * 1e-9, t_n * 1e3, bytes / t_n * 1e-9, t_n4 * 1e3, bytes / t_n4 * 1e-9, (double)maxd);
#else
        printf("  n=%-9zu scalar %7.3f ms (%5.1f GB/s)\n", n, t_s * 1e3, bytes / t_s * 1e-9);
#endif
        sink(y[n / 2] + y2[n / 3] + y3[n / 5]);
        free(x); free(y); free(y2); free(y3);
    }
    puts("  clang -O2 already auto-vectorizes AND unrolls the scalar loop; one-vector-per-iteration");
    puts("  intrinsics lose to it in L1 (loop overhead per 4 elements). Unrolling recovers it. From DRAM");
    puts("  everything hits the same bandwidth wall: roofline in action.");
}

/* ------------------------------------------------------------------------------------------ */
/* 5. MATMUL: from naive to a NEON micro-kernel                                                */
/*    C[n x n] = A[n x n] * B[n x n], row-major doubles, C zeroed by the caller.               */
/* ------------------------------------------------------------------------------------------ */
static void mm_naive(size_t n, const double *A, const double *B, double *C) {
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            double s = 0;
            for (size_t p = 0; p < n; p++) s += A[i * n + p] * B[p * n + j];   /* B column walk: stride n */
            C[i * n + j] = s;
        }
}

/* ikj: inner loop walks B and C rows contiguously; `restrict` lets the compiler vectorize it */
static void mm_ikj(size_t n, const double *restrict A, const double *restrict B, double *restrict C) {
    for (size_t i = 0; i < n; i++)
        for (size_t p = 0; p < n; p++) {
            double a = A[i * n + p];
            for (size_t j = 0; j < n; j++) C[i * n + j] += a * B[p * n + j];
        }
}

/* cache blocking: work on BI x BP x BJ tiles so the B tile (BP x BJ doubles) and the C tile      */
/* (BI x BJ) stay in L1 while the i loop reuses them. Tune with -DBI=.. -DBP=.. -DBJ=..            */
#ifndef BI
#define BI 64
#endif
#ifndef BP
#define BP 64
#endif
#ifndef BJ
#define BJ 256
#endif
static void mm_blocked(size_t n, const double *restrict A, const double *restrict B, double *restrict C) {
    for (size_t j0 = 0; j0 < n; j0 += BJ)
        for (size_t p0 = 0; p0 < n; p0 += BP)
            for (size_t i0 = 0; i0 < n; i0 += BI) {
                size_t i1 = i0 + BI < n ? i0 + BI : n, p1 = p0 + BP < n ? p0 + BP : n, j1 = j0 + BJ < n ? j0 + BJ : n;
                for (size_t i = i0; i < i1; i++)
                    for (size_t p = p0; p < p1; p++) {
                        double a = A[i * n + p];
                        double *restrict c = C + i * n; const double *restrict b = B + p * n;
                        for (size_t j = j0; j < j1; j++) c[j] += a * b[j];
                    }
            }
}

#if defined(__ARM_NEON)
/* Packed micro-kernel version (the structure of every real BLAS):                              */
/*   - KC x NC panel of B is packed so each 4-column strip is contiguous: Bp[strip][p][0..3]      */
/*   - MC x KC panel of A is packed so each 4-row strip is contiguous:    Ap[strip][p][0..3]      */
/*   - the micro-kernel keeps a 4x4 tile of C in 8 NEON registers and streams the two panels:    */
/*     per p: 2 loads of A (4 doubles), 2 loads of B (4 doubles), 8 fmla  -> 2 FMAs per load.   */
/* Requires n % 4 == 0 (edge handling is exercise 19.6).                                         */
#define KC 256
#define MC 64
#define NC 512

/* register blocking alone: a 4x4 tile of C in 8 registers, streaming A and B straight from the   */
/* matrices with their natural strides, over the full K. No packing, no cache blocking.            */
static void mm_kernel_noblock(size_t n, const double *restrict A, const double *restrict B, double *restrict C) {
    assert(n % 4 == 0);
    for (size_t j = 0; j < n; j += 4)
        for (size_t i = 0; i < n; i += 4) {
            float64x2_t c00 = vdupq_n_f64(0), c01 = c00, c10 = c00, c11 = c00, c20 = c00, c21 = c00, c30 = c00, c31 = c00;
            const double *a0 = A + i * n, *a1 = a0 + n, *a2 = a1 + n, *a3 = a2 + n;
            for (size_t p = 0; p < n; p++) {
                float64x2_t b01 = vld1q_f64(B + p * n + j), b23 = vld1q_f64(B + p * n + j + 2);   /* stride n: new line every p */
                c00 = vfmaq_n_f64(c00, b01, a0[p]); c01 = vfmaq_n_f64(c01, b23, a0[p]);
                c10 = vfmaq_n_f64(c10, b01, a1[p]); c11 = vfmaq_n_f64(c11, b23, a1[p]);
                c20 = vfmaq_n_f64(c20, b01, a2[p]); c21 = vfmaq_n_f64(c21, b23, a2[p]);
                c30 = vfmaq_n_f64(c30, b01, a3[p]); c31 = vfmaq_n_f64(c31, b23, a3[p]);
            }
            double *c = C + i * n + j;
            vst1q_f64(c, c00); vst1q_f64(c + 2, c01); vst1q_f64(c + n, c10); vst1q_f64(c + n + 2, c11);
            vst1q_f64(c + 2 * n, c20); vst1q_f64(c + 2 * n + 2, c21); vst1q_f64(c + 3 * n, c30); vst1q_f64(c + 3 * n + 2, c31);
        }
}

static void pack_a(size_t n, const double *A, size_t i0, size_t mc, size_t p0, size_t kc, double *Ap) {
    for (size_t s = 0; s < mc; s += 4)                       /* 4-row strips */
        for (size_t p = 0; p < kc; p++)
            for (size_t r = 0; r < 4; r++)
                Ap[s * kc + p * 4 + r] = A[(i0 + s + r) * n + p0 + p];
}
static void pack_b(size_t n, const double *B, size_t p0, size_t kc, size_t j0, size_t nc, double *Bp) {
    for (size_t s = 0; s < nc; s += 4)                       /* 4-column strips */
        for (size_t p = 0; p < kc; p++)
            for (size_t c = 0; c < 4; c++)
                Bp[s * kc + p * 4 + c] = B[(p0 + p) * n + j0 + s + c];
}

static inline void kernel_4x4(size_t kc, const double *restrict Ap, const double *restrict Bp,
                              double *restrict C, size_t ldc) {
    float64x2_t c00 = vld1q_f64(C),               c01 = vld1q_f64(C + 2);
    float64x2_t c10 = vld1q_f64(C + ldc),         c11 = vld1q_f64(C + ldc + 2);
    float64x2_t c20 = vld1q_f64(C + 2 * ldc),     c21 = vld1q_f64(C + 2 * ldc + 2);
    float64x2_t c30 = vld1q_f64(C + 3 * ldc),     c31 = vld1q_f64(C + 3 * ldc + 2);
    for (size_t p = 0; p < kc; p++) {
        float64x2_t a01 = vld1q_f64(Ap + p * 4), a23 = vld1q_f64(Ap + p * 4 + 2);   /* A[i..i+3][p] */
        float64x2_t b01 = vld1q_f64(Bp + p * 4), b23 = vld1q_f64(Bp + p * 4 + 2);   /* B[p][j..j+3] */
        c00 = vfmaq_laneq_f64(c00, b01, a01, 0); c01 = vfmaq_laneq_f64(c01, b23, a01, 0);  /* row i   */
        c10 = vfmaq_laneq_f64(c10, b01, a01, 1); c11 = vfmaq_laneq_f64(c11, b23, a01, 1);  /* row i+1 */
        c20 = vfmaq_laneq_f64(c20, b01, a23, 0); c21 = vfmaq_laneq_f64(c21, b23, a23, 0);  /* row i+2 */
        c30 = vfmaq_laneq_f64(c30, b01, a23, 1); c31 = vfmaq_laneq_f64(c31, b23, a23, 1);  /* row i+3 */
    }
    vst1q_f64(C, c00);           vst1q_f64(C + 2, c01);
    vst1q_f64(C + ldc, c10);     vst1q_f64(C + ldc + 2, c11);
    vst1q_f64(C + 2 * ldc, c20); vst1q_f64(C + 2 * ldc + 2, c21);
    vst1q_f64(C + 3 * ldc, c30); vst1q_f64(C + 3 * ldc + 2, c31);
}

static void mm_kernel(size_t n, const double *restrict A, const double *restrict B, double *restrict C,
                      double *restrict Ap, double *restrict Bp) {
    assert(n % 4 == 0);
    for (size_t j0 = 0; j0 < n; j0 += NC) {
        size_t nc = j0 + NC < n ? NC : n - j0;
        for (size_t p0 = 0; p0 < n; p0 += KC) {
            size_t kc = p0 + KC < n ? KC : n - p0;
            pack_b(n, B, p0, kc, j0, nc, Bp);                     /* B panel: kc x nc, lives in L2 */
            for (size_t i0 = 0; i0 < n; i0 += MC) {
                size_t mc = i0 + MC < n ? MC : n - i0;
                pack_a(n, A, i0, mc, p0, kc, Ap);                 /* A panel: mc x kc, lives in L1 */
                for (size_t j = 0; j < nc; j += 4)
                    for (size_t i = 0; i < mc; i += 4)
                        kernel_4x4(kc, Ap + i * kc, Bp + j * kc, C + (i0 + i) * n + j0 + j, n);
            }
        }
    }
}
#endif

static double max_abs_diff(const double *x, const double *y, size_t n) {
    double m = 0; for (size_t i = 0; i < n; i++) m = fmax(m, fabs(x[i] - y[i])); return m;
}

static void demo_matmul(size_t n) {
    printf("== 5. matmul N=%zu: %.0f MFLOP per multiply ==\n", n, 2.0 * n * n * n * 1e-6);
    double *A = alloc_doubles(n * n), *B = alloc_doubles(n * n), *C = alloc_doubles(n * n), *R = alloc_doubles(n * n);
    fill_random(A, n * n, 3); fill_random(B, n * n, 4);
    double flops = 2.0 * (double)n * (double)n * (double)n, t;
    int reps = n <= 512 ? 5 : 3;

    printf("  %-34s %9s %10s %10s\n", "version", "ms", "GFLOP/s", "max|diff|");
    if (n <= 1024) {                                  /* naive at 2 GFLOP/s: 17 s for N=2048 — skip */
        BENCH(t, 1, mm_naive(n, A, B, R));
        printf("  %-34s %9.1f %10.2f %10s\n", "naive ijk", t * 1e3, flops / t * 1e-9, "(ref)");
    } else {
        memset(R, 0, n * n * sizeof *R); mm_ikj(n, A, B, R);
        puts("  (naive ijk skipped for N > 1024; ikj is the reference)");
    }

    BENCH(t, reps, (memset(C, 0, n * n * sizeof *C), mm_ikj(n, A, B, C)));
    printf("  %-34s %9.1f %10.2f %10.1e\n", "ikj + restrict (vectorizable)", t * 1e3, flops / t * 1e-9, max_abs_diff(C, R, n * n));

    BENCH(t, reps, (memset(C, 0, n * n * sizeof *C), mm_blocked(n, A, B, C)));
    char label[48]; snprintf(label, sizeof label, "cache-blocked ikj (%d,%d,%d)", BI, BP, BJ);
    printf("  %-34s %9.1f %10.2f %10.1e\n", label, t * 1e3, flops / t * 1e-9, max_abs_diff(C, R, n * n));

#if defined(__ARM_NEON)
    if (n % 4 == 0) {
        BENCH(t, reps, mm_kernel_noblock(n, A, B, C));
        printf("  %-34s %9.1f %10.2f %10.1e\n", "4x4 NEON register tile, no packing", t * 1e3, flops / t * 1e-9, max_abs_diff(C, R, n * n));
        double *Ap = alloc_doubles((size_t)MC * KC), *Bp = alloc_doubles((size_t)KC * NC);
        BENCH(t, reps, (memset(C, 0, n * n * sizeof *C), mm_kernel(n, A, B, C, Ap, Bp)));
        printf("  %-34s %9.1f %10.2f %10.1e\n", "packed + blocked + 4x4 kernel", t * 1e3, flops / t * 1e-9, max_abs_diff(C, R, n * n));
        free(Ap); free(Bp);
    }
#endif
#ifdef USE_ACCELERATE
    BENCH(t, reps, cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, (int)n, (int)n, (int)n, 1.0, A, (int)n, B, (int)n, 0.0, C, (int)n));
    printf("  %-34s %9.1f %10.2f %10.1e\n", "Accelerate cblas_dgemm", t * 1e3, flops / t * 1e-9, max_abs_diff(C, R, n * n));
#else
    puts("  (rebuild with -DUSE_ACCELERATE -framework Accelerate to add cblas_dgemm to this table)");
#endif
    puts("  same arithmetic, same answer to rounding; only the order the bytes are touched changed.");
    free(A); free(B); free(C); free(R);
}

/* ------------------------------------------------------------------------------------------ */
/* 6. FMA ACCURACY                                                                             */
/* ------------------------------------------------------------------------------------------ */
static void demo_fma(void) {
    puts("== 6. FMA accuracy ==");
    volatile double a = 1.0 + ldexp(1.0, -27), b = 1.0 - ldexp(1.0, -27);   /* a*b = 1 - 2^-54 exactly */
    volatile double prod = a * b;                /* volatile: the product must be rounded to a double */
    double sep = prod - 1.0;                     /* a*b rounded to 1.0 first: result 0 */
    double fused = fma(a, b, -1.0);              /* one rounding at the end: exact -2^-54 */
    printf("  a*b - 1 separately: %.3e   fma(a,b,-1): %.3e   exact: %.3e\n", sep, fused, -ldexp(1.0, -54));
    puts("  FMA keeps the full product before subtracting: more accurate, and different from a machine");
    puts("  without FMA. clang's default -ffp-contract=on fuses a*b+c within one expression (the volatile");
    puts("  above stops it); -ffp-contract=off forbids it. One reason results differ at the last bit.");
}

/* ------------------------------------------------------------------------------------------ */
/* 7. BRANCHY vs BRANCHLESS: compaction (copy elements above a threshold)                      */
/* ------------------------------------------------------------------------------------------ */
__attribute__((noinline)) static size_t compact_branchy(const int32_t *restrict in, int32_t *restrict out, size_t n, int32_t thr) {
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (in[i] > thr) out[k++] = in[i];        /* a real branch: the store depends on it */
    return k;
}
__attribute__((noinline)) static size_t compact_branchless(const int32_t *restrict in, int32_t *restrict out, size_t n, int32_t thr) {
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        out[k] = in[i];                           /* always store (out has room for n) ... */
        k += (size_t)(in[i] > thr);               /* ... and advance only if kept: no branch */
    }
    return k;
}
static int cmp_i32(const void *a, const void *b) { int32_t x = *(const int32_t *)a, y = *(const int32_t *)b; return (x > y) - (x < y); }

static void demo_branch(void) {
    puts("== 7. branchy vs branchless compaction (n = 2^22 int32, threshold = median) ==");
    const size_t n = 1u << 22;
    int32_t *in = malloc(n * sizeof *in), *out = malloc(n * sizeof *out);
    if (!in || !out) { perror("malloc"); exit(1); }
    uint32_t s = 99;
    for (size_t i = 0; i < n; i++) { s = s * 1664525u + 1013904223u; in[i] = (int32_t)(s >> 1); }
    double tb, tl; size_t kb, kl;
    BENCH(tb, 11, kb = compact_branchy(in, out, n, INT32_MAX / 2));
    BENCH(tl, 11, kl = compact_branchless(in, out, n, INT32_MAX / 2));
    assert(kb == kl);
    printf("  random input: branchy %6.2f ms   branchless %6.2f ms   (%.0f%% taken, unpredictable)\n",
           tb * 1e3, tl * 1e3, 100.0 * kb / n);
    qsort(in, n, sizeof *in, cmp_i32);
    BENCH(tb, 11, kb = compact_branchy(in, out, n, INT32_MAX / 2));
    BENCH(tl, 11, kl = compact_branchless(in, out, n, INT32_MAX / 2));
    printf("  sorted input: branchy %6.2f ms   branchless %6.2f ms   (same data, predictable branch)\n",
           tb * 1e3, tl * 1e3);
    puts("  a mispredicted branch costs ~15 cycles; branchless code costs the same on any input.");
    puts("  (both functions are noinline: inlined into main, clang specialised them and the effect vanished.)");
    puts("  clang turns `x = c ? a : b` into csel (x86: cmov) by itself; stores behind a branch it will not.");
    free(in); free(out);
}

/* ------------------------------------------------------------------------------------------ */
/* 8. SUBNORMALS                                                                               */
/* ------------------------------------------------------------------------------------------ */
static double decay(double x0, size_t n) {
    double x = x0, acc = 0;
    for (size_t i = 0; i < n; i++) { x *= 0.999; acc += x; }
    return acc;
}
static void demo_denormals(void) {
    puts("== 8. subnormal (denormal) arithmetic ==");
    const size_t n = 1u << 22;
    double tn, td;
    BENCH(tn, 7, sink(decay(1.0, n)));                 /* normal range */
    BENCH(td, 7, sink(decay(1e-310, n)));              /* starts subnormal (< 2.2e-308), stays there */
    printf("  normal operands %.2f ms, subnormal operands %.2f ms: ratio x%.2f\n", tn * 1e3, td * 1e3, td / tn);
    puts("  x86 cores take a ~100x microcode assist per subnormal op (fix: FTZ/DAZ via _MM_SET_FLUSH_ZERO_MODE);");
    puts("  Apple Silicon handles subnormals in hardware at (nearly) full speed. Know which machine you are on.");
    puts("  Where they come from: exp(-large) in softmax, decaying activations, tiny gradients.");
}

/* ------------------------------------------------------------------------------------------ */
/* 9. INTEGER vs FLOAT THROUGHPUT                                                              */
/* ------------------------------------------------------------------------------------------ */
static int64_t sum_i64(const int64_t *a, size_t n) {
    int64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (size_t i = 0; i + 4 <= n; i += 4) { s0 += a[i]; s1 += a[i + 1]; s2 += a[i + 2]; s3 += a[i + 3]; }
    return s0 + s1 + s2 + s3;
}
static double sum_f64(const double *a, size_t n) {
    double s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (size_t i = 0; i + 4 <= n; i += 4) { s0 += a[i]; s1 += a[i + 1]; s2 += a[i + 2]; s3 += a[i + 3]; }
    return s0 + s1 + s2 + s3;
}
static void demo_int_float(void) {
    puts("== 9. integer vs floating-point add throughput (n = 8192, L1-resident, 4 accumulators) ==");
    const size_t n = 8192;
    int64_t *ai = malloc(n * sizeof *ai); double *af = alloc_doubles(n);
    if (!ai) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < n; i++) { ai[i] = (int64_t)i; af[i] = (double)i; }
    double ti, tf;
    BENCH(ti, 11, for (int k = 0; k < 200; k++) sink((double)sum_i64(ai, n)));
    BENCH(tf, 11, for (int k = 0; k < 200; k++) sink(sum_f64(af, n)));
    printf("  int64 add: %.3f ns/element   double add: %.3f ns/element   (at -O3 both vectorize; int wins on latency)\n",
           ti / (200.0 * n) * 1e9, tf / (200.0 * n) * 1e9);
    puts("  integer add latency 1 cycle, fp add 3-4 cycles: reductions need more independent chains for fp.");
    free(ai); free(af);
}

int main(int argc, char **argv) {
    size_t n = argc > 1 ? (size_t)strtoul(argv[1], NULL, 10) : 512;
    if (n < 64 || n > 4096) n = 512;
#if defined(__ARM_NEON)
    puts("target: arm64 with NEON (__ARM_NEON defined)");
#else
    puts("target: no NEON; running portable paths only");
#endif
    demo_roofline(n);
    demo_dot();
    demo_saxpy();
    demo_matmul(n);
    demo_fma();
    demo_branch();
    demo_denormals();
    demo_int_float();
    return 0;
}
