/*
 * Chapter 13 — Debugging, Testing, and Performance
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 *
 * Also try:
 *   cc -Wall -Wextra -std=c11 -O3 -march=native -Rpass=loop-vectorize -o ex_demo example.c -lm
 *   cc -Wall -Wextra -std=c11 -g -O1 -fsanitize=address,undefined -o ex_demo example.c -lm
 *   cc -Wall -Wextra -std=c11 -O2 -DDEBUG -o ex_demo example.c -lm      (enables DBG output)
 *   leaks --atExit -- ./ex_demo
 *
 * Contents:
 *   1. DBG macro (stderr, file:line:func, compiled out unless -DDEBUG)
 *   2. a minimal unit-test framework: TEST / RUN / ASSERT_TRUE / ASSERT_EQ_INT / ASSERT_NEAR
 *   3. a small Matrix lib developed against those tests (algebraic identity tests)
 *   4. gradient checking (finite differences vs analytic) on a tiny model — passes,
 *      then a deliberately broken gradient — fails, and the test catches it
 *   5. TIMER macros + clock_gettime, warmup, repeats, min-of-N
 *   6. the matmul loop-order experiment: ijk vs ikj vs blocked ikj, GFLOP/s, verified equal.
 *      The results are consumed (checksum printed) so -O2 cannot delete the work.
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* 1. Debug printing                                                    */
/* ------------------------------------------------------------------ */
#ifdef DEBUG
#  define DBG(fmt, ...) fprintf(stderr, "[%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#  define DBG(fmt, ...) ((void)0)
#endif

/* ------------------------------------------------------------------ */
/* 2. Mini test framework                                               */
/* ------------------------------------------------------------------ */
static int tests_run = 0, tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name) do { tests_run++; fprintf(stderr, "  %-32s", #name); test_##name(); } while (0)
#define FAIL_AT(fmt, ...) do { tests_failed++; \
    fprintf(stderr, "FAIL %s:%d: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); return; } while (0)
#define ASSERT_TRUE(cond) do { if (!(cond)) FAIL_AT("%s", #cond); } while (0)
#define ASSERT_EQ_INT(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) FAIL_AT("%s == %ld, expected %ld", #a, _a, _b); } while (0)
/* !(x <= tol) rather than x > tol so that NaN fails the test. */
#define ASSERT_NEAR(a, b, tol) do { double _a = (a), _b = (b), _t = (tol); \
    if (!(fabs(_a - _b) <= _t)) FAIL_AT("%s = %.10g, expected %.10g (tol %g)", #a, _a, _b, _t); } while (0)
#define PASS() fprintf(stderr, "ok\n")

/* ------------------------------------------------------------------ */
/* 3. Matrix lib (row-major)                                            */
/* ------------------------------------------------------------------ */
typedef struct { size_t rows, cols; double *data; } Matrix;

static Matrix *mat_new(size_t rows, size_t cols) {
    Matrix *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->rows = rows; m->cols = cols;
    m->data = calloc(rows * cols, sizeof *m->data);
    if (!m->data) { free(m); return NULL; }
    return m;
}
static void mat_free(Matrix *m) { if (m) { free(m->data); free(m); } }

static double mat_get(const Matrix *m, size_t i, size_t j) {
    assert(i < m->rows && j < m->cols);   /* programmer error if violated; gone with -DNDEBUG */
    return m->data[i * m->cols + j];
}
static void mat_set(Matrix *m, size_t i, size_t j, double v) {
    assert(i < m->rows && j < m->cols);
    m->data[i * m->cols + j] = v;
}
static Matrix *mat_identity(size_t n) {
    Matrix *m = mat_new(n, n);
    for (size_t i = 0; i < n; i++) mat_set(m, i, i, 1.0);
    return m;
}
static Matrix *mat_mul(const Matrix *a, const Matrix *b) {
    assert(a->cols == b->rows);
    Matrix *c = mat_new(a->rows, b->cols);
    for (size_t i = 0; i < a->rows; i++)
        for (size_t k = 0; k < a->cols; k++) {
            double aik = a->data[i * a->cols + k];
            for (size_t j = 0; j < b->cols; j++)
                c->data[i * c->cols + j] += aik * b->data[k * b->cols + j];
        }
    return c;
}
static Matrix *mat_transpose(const Matrix *a) {
    Matrix *t = mat_new(a->cols, a->rows);
    for (size_t i = 0; i < a->rows; i++)
        for (size_t j = 0; j < a->cols; j++) t->data[j * t->cols + i] = a->data[i * a->cols + j];
    return t;
}
static int mat_allclose(const Matrix *a, const Matrix *b, double tol) {
    if (a->rows != b->rows || a->cols != b->cols) return 0;
    for (size_t i = 0; i < a->rows * a->cols; i++)
        if (!(fabs(a->data[i] - b->data[i]) <= tol)) return 0;
    return 1;
}

/* Tiny deterministic RNG (xorshift64*) for reproducible random tests. */
static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static double rng_uniform(void) {
    uint64_t x = rng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    rng_state = x;
    return (double)((x * 0x2545F4914F6CDD1Dull) >> 11) * (1.0 / 9007199254740992.0);
}
static Matrix *mat_random(size_t rows, size_t cols) {
    Matrix *m = mat_new(rows, cols);
    for (size_t i = 0; i < rows * cols; i++) m->data[i] = rng_uniform() * 2.0 - 1.0;
    return m;
}

TEST(mat_new_shape_zero) {
    Matrix *m = mat_new(2, 3);
    ASSERT_EQ_INT(m->rows, 2);
    ASSERT_EQ_INT(m->cols, 3);
    ASSERT_NEAR(mat_get(m, 1, 2), 0.0, 0.0);
    mat_free(m);
    PASS();
}
TEST(mat_set_get_roundtrip) {
    Matrix *m = mat_new(3, 3);
    mat_set(m, 1, 2, 5.5);
    ASSERT_NEAR(mat_get(m, 1, 2), 5.5, 0.0);
    ASSERT_NEAR(mat_get(m, 2, 1), 0.0, 0.0);   /* would catch i/j swapped in indexing */
    mat_free(m);
    PASS();
}
TEST(mat_mul_hand_computed) {
    Matrix *a = mat_new(2, 3), *b = mat_new(3, 2);
    double av[] = { 1, 2, 3, 4, 5, 6 }, bv[] = { 7, 8, 9, 10, 11, 12 };
    memcpy(a->data, av, sizeof av);
    memcpy(b->data, bv, sizeof bv);
    Matrix *c = mat_mul(a, b);           /* [[58, 64], [139, 154]] */
    ASSERT_EQ_INT(c->rows, 2); ASSERT_EQ_INT(c->cols, 2);
    ASSERT_NEAR(mat_get(c, 0, 0), 58, 1e-12);
    ASSERT_NEAR(mat_get(c, 0, 1), 64, 1e-12);
    ASSERT_NEAR(mat_get(c, 1, 0), 139, 1e-12);
    ASSERT_NEAR(mat_get(c, 1, 1), 154, 1e-12);
    mat_free(a); mat_free(b); mat_free(c);
    PASS();
}
TEST(mat_mul_identity) {
    Matrix *a = mat_random(4, 3), *i4 = mat_identity(4);
    Matrix *c = mat_mul(i4, a);
    ASSERT_TRUE(mat_allclose(c, a, 0.0));
    mat_free(a); mat_free(i4); mat_free(c);
    PASS();
}
TEST(transpose_identities) {
    /* Algebraic identities: no expected values to type, and one wrong index breaks them. */
    Matrix *a = mat_random(3, 4), *b = mat_random(4, 2);
    Matrix *at = mat_transpose(a), *att = mat_transpose(at);
    ASSERT_TRUE(mat_allclose(att, a, 0.0));                        /* (A^T)^T == A */
    Matrix *ab = mat_mul(a, b), *abt = mat_transpose(ab);
    Matrix *bt = mat_transpose(b), *btat = mat_mul(bt, at);
    ASSERT_TRUE(mat_allclose(abt, btat, 1e-12));                    /* (AB)^T == B^T A^T */
    mat_free(a); mat_free(b); mat_free(at); mat_free(att);
    mat_free(ab); mat_free(abt); mat_free(bt); mat_free(btat);
    PASS();
}
TEST(mul_associative) {
    Matrix *a = mat_random(4, 5), *b = mat_random(5, 3), *c = mat_random(3, 4);
    Matrix *ab = mat_mul(a, b), *ab_c = mat_mul(ab, c);
    Matrix *bc = mat_mul(b, c), *a_bc = mat_mul(a, bc);
    ASSERT_TRUE(mat_allclose(ab_c, a_bc, 1e-12));                   /* (AB)C == A(BC) */
    mat_free(a); mat_free(b); mat_free(c); mat_free(ab); mat_free(ab_c); mat_free(bc); mat_free(a_bc);
    PASS();
}

/* ------------------------------------------------------------------ */
/* 4. Gradient checking                                                 */
/* ------------------------------------------------------------------ */
/* Model: loss(w) = sum_i tanh(w_i * x_i)^2  for fixed inputs x.  Analytic gradient:
 *   dloss/dw_i = 2 tanh(w_i x_i) (1 - tanh(w_i x_i)^2) x_i                                  */
typedef struct { const double *x; } LossCtx;

static double loss_fn(const double *w, size_t n, void *ctx) {
    const LossCtx *c = ctx;
    double s = 0;
    for (size_t i = 0; i < n; i++) { double t = tanh(w[i] * c->x[i]); s += t * t; }
    return s;
}
static void grad_correct(const double *w, size_t n, const LossCtx *c, double *g) {
    for (size_t i = 0; i < n; i++) { double t = tanh(w[i] * c->x[i]); g[i] = 2 * t * (1 - t * t) * c->x[i]; }
}
static void grad_buggy(const double *w, size_t n, const LossCtx *c, double *g) {
    for (size_t i = 0; i < n; i++) { double t = tanh(w[i] * c->x[i]); g[i] = 2 * t * c->x[i]; } /* forgot (1 - t^2) */
}

typedef double (*LossFn)(const double *, size_t, void *);

/* Central differences; returns max relative error over all parameters. */
static double grad_check(LossFn loss, double *params, size_t n, const double *analytic, void *ctx, double h) {
    double max_rel = 0;
    for (size_t i = 0; i < n; i++) {
        double orig = params[i];
        params[i] = orig + h; double lp = loss(params, n, ctx);
        params[i] = orig - h; double lm = loss(params, n, ctx);
        params[i] = orig;                              /* restore */
        double numeric = (lp - lm) / (2 * h);
        double rel = fabs(numeric - analytic[i]) / fmax(fabs(numeric) + fabs(analytic[i]), 1e-12);
        if (rel > max_rel) max_rel = rel;
    }
    return max_rel;
}

TEST(gradient_check_passes) {
    double w[5] = { 0.3, -1.2, 0.8, 2.0, -0.1 }, x[5] = { 1.0, 0.5, -2.0, 0.7, 3.0 }, g[5];
    LossCtx c = { x };
    grad_correct(w, 5, &c, g);
    double err = grad_check(loss_fn, w, 5, g, &c, 1e-5);
    DBG("max rel err (correct) = %.3e", err);
    ASSERT_TRUE(err < 1e-7);
    PASS();
}
TEST(gradient_check_catches_bug) {
    double w[5] = { 0.3, -1.2, 0.8, 2.0, -0.1 }, x[5] = { 1.0, 0.5, -2.0, 0.7, 3.0 }, g[5];
    LossCtx c = { x };
    grad_buggy(w, 5, &c, g);
    double err = grad_check(loss_fn, w, 5, g, &c, 1e-5);
    DBG("max rel err (buggy)   = %.3e", err);
    ASSERT_TRUE(err > 1e-3);      /* the check must flag the wrong gradient */
    PASS();
}

/* ------------------------------------------------------------------ */
/* 5. Timing                                                            */
/* ------------------------------------------------------------------ */
static double now_sec(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
#define TIMER_START(name) double _t_##name = now_sec()
#define TIMER_END(name)   printf("  %-24s %9.3f ms\n", #name, (now_sec() - _t_##name) * 1e3)

/* ------------------------------------------------------------------ */
/* 6. Matmul loop-order experiment                                      */
/* ------------------------------------------------------------------ */
/* All three compute C = A*B for n x n row-major doubles. C must be zeroed first. */
static void matmul_ijk(size_t n, const double *A, const double *B, double *C) {
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            double s = 0;
            for (size_t k = 0; k < n; k++) s += A[i * n + k] * B[k * n + j];   /* B stride n: bad */
            C[i * n + j] = s;
        }
}
static void matmul_ikj(size_t n, const double *A, const double *B, double *C) {
    for (size_t i = 0; i < n; i++)
        for (size_t k = 0; k < n; k++) {
            double a = A[i * n + k];
            for (size_t j = 0; j < n; j++) C[i * n + j] += a * B[k * n + j];   /* stride 1: good */
        }
}
static void matmul_blocked(size_t n, const double *restrict A, const double *restrict B,
                           double *restrict C, size_t bs) {
    for (size_t ii = 0; ii < n; ii += bs)
        for (size_t kk = 0; kk < n; kk += bs)
            for (size_t jj = 0; jj < n; jj += bs) {
                size_t imax = ii + bs < n ? ii + bs : n;
                size_t kmax = kk + bs < n ? kk + bs : n;
                size_t jmax = jj + bs < n ? jj + bs : n;
                for (size_t i = ii; i < imax; i++)
                    for (size_t k = kk; k < kmax; k++) {
                        double a = A[i * n + k];
                        for (size_t j = jj; j < jmax; j++) C[i * n + j] += a * B[k * n + j];
                    }
            }
}

typedef void (*MatmulFn)(size_t, const double *, const double *, double *);

/* Warmup once, then min of `reps` timed runs. The checksum keeps the compiler honest. */
static double bench(MatmulFn f, size_t n, const double *A, const double *B, double *C, int reps, double *checksum) {
    memset(C, 0, n * n * sizeof *C);
    f(n, A, B, C);                                  /* warmup: page faults, cold cache */
    double best = 1e300;
    for (int r = 0; r < reps; r++) {
        memset(C, 0, n * n * sizeof *C);
        double t0 = now_sec();
        f(n, A, B, C);
        double dt = now_sec() - t0;
        if (dt < best) best = dt;
    }
    double s = 0;
    for (size_t i = 0; i < n * n; i++) s += C[i];
    *checksum = s;
    return best;
}
static void matmul_blocked64(size_t n, const double *A, const double *B, double *C) {
    matmul_blocked(n, A, B, C, 64);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv) {
    DBG("debug build; argc=%d", argc);

    fputs("== unit tests ==\n", stderr);
    RUN(mat_new_shape_zero);
    RUN(mat_set_get_roundtrip);
    RUN(mat_mul_hand_computed);
    RUN(mat_mul_identity);
    RUN(transpose_identities);
    RUN(mul_associative);
    RUN(gradient_check_passes);
    RUN(gradient_check_catches_bug);
    fprintf(stderr, "%d tests, %d failed\n\n", tests_run, tests_failed);
    if (tests_failed) return 1;

    puts("== timing: TIMER macros ==");
    {
        const size_t N = 5000000;
        double *a = malloc(N * sizeof *a), *b = malloc(N * sizeof *b);
        if (!a || !b) return 1;
        for (size_t i = 0; i < N; i++) { a[i] = rng_uniform(); b[i] = rng_uniform(); }
        TIMER_START(dot_5M_cold);
        double dot = 0;
        for (size_t i = 0; i < N; i++) dot += a[i] * b[i];
        TIMER_END(dot_5M_cold);
        TIMER_START(dot_5M_warm);
        double dot2 = 0;
        for (size_t i = 0; i < N; i++) dot2 += a[i] * b[i];
        TIMER_END(dot_5M_warm);
        volatile double sink = dot + dot2;     /* use the results or -O2 removes both loops */
        printf("  (dot = %.3f; second run is warm-cache)\n", sink / 2);
        free(a); free(b);
    }

    puts("\n== matmul loop-order experiment (double, row-major) ==");
    size_t n = (argc > 1) ? (size_t)strtoul(argv[1], NULL, 10) : 512;
    if (n < 16) n = 16;
    double *A = malloc(n * n * sizeof *A), *B = malloc(n * n * sizeof *B), *C = malloc(n * n * sizeof *C);
    if (!A || !B || !C) return 1;
    for (size_t i = 0; i < n * n; i++) { A[i] = rng_uniform(); B[i] = rng_uniform(); }
    double flops = 2.0 * (double)n * (double)n * (double)n;

    double cs_ijk, cs_ikj, cs_blk;
    double t_ijk = bench(matmul_ijk, n, A, B, C, 3, &cs_ijk);
    double t_ikj = bench(matmul_ikj, n, A, B, C, 3, &cs_ikj);
    double t_blk = bench(matmul_blocked64, n, A, B, C, 3, &cs_blk);

    printf("  n = %zu  (%.1f MB per matrix)\n", n, n * n * sizeof(double) / 1e6);
    printf("  %-14s %9.1f ms  %6.2f GFLOP/s  checksum %.6e\n", "ijk (naive)", t_ijk * 1e3, flops / t_ijk / 1e9, cs_ijk);
    printf("  %-14s %9.1f ms  %6.2f GFLOP/s  checksum %.6e\n", "ikj",         t_ikj * 1e3, flops / t_ikj / 1e9, cs_ikj);
    printf("  %-14s %9.1f ms  %6.2f GFLOP/s  checksum %.6e\n", "ikj blocked64", t_blk * 1e3, flops / t_blk / 1e9, cs_blk);
    printf("  speedup ikj/ijk = %.1fx   blocked/ijk = %.1fx\n", t_ijk / t_ikj, t_ijk / t_blk);
    /* Same arithmetic, different memory access order. The checksums agree to rounding, which
     * proves the fast versions compute the same thing; a relative tolerance is used because
     * the summation order differs. */
    double rel = fabs(cs_ijk - cs_ikj) / fabs(cs_ijk);
    printf("  results agree: %s (rel diff %.1e)\n", rel < 1e-10 && fabs(cs_ijk - cs_blk) / fabs(cs_ijk) < 1e-10 ? "yes" : "NO", rel);
    puts("  try: ./ex_demo 1024   (B no longer fits in L2 -> gap widens)");

    free(A); free(B); free(C);
    return 0;
}
