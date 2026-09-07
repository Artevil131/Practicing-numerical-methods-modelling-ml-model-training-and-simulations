/*
 * Chapter 12 — Numbers, Bits, and Floats
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 *
 * Demonstrates, in order:
 *   1. fixed-width integer types and their sizes/limits
 *   2. unsigned wrap vs integer promotion
 *   3. bitwise ops, masks, bit tricks (pow2, popcount, bswap, next_pow2)
 *   4. IEEE 754 float layout via memcpy type-punning
 *   5. epsilon, 0.1+0.2, nearly_equal
 *   6. inf/nan generation and propagation
 *   7. catastrophic cancellation
 *   8. stable softmax / logsumexp / sigmoid vs naive versions
 *   9. Kahan and pairwise summation vs naive float sum
 *  10. xorshift64* + splitmix64 seeding, uniform, unbiased range, Box-Muller
 *  11. clock_gettime timing
 */
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>   /* ptrdiff_t, size_t */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* 3. Bit tricks                                                        */
/* ------------------------------------------------------------------ */
#define BIT(n) (1u << (n))

static void print_bits(uint32_t x, int width) {
    for (int i = width - 1; i >= 0; i--) putchar(((x >> i) & 1u) ? '1' : '0');
}
static bool is_pow2(uint64_t x) { return x && !(x & (x - 1)); }
static int popcount(uint64_t x) {
    int c = 0;
    while (x) { x &= x - 1; c++; }   /* clears lowest set bit each iteration */
    return c;
}
static uint32_t bswap32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0xFF00u) | ((x << 8) & 0xFF0000u) | (x << 24);
}
static uint64_t next_pow2(uint64_t x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; x |= x >> 32;
    return x + 1;
}

/* ------------------------------------------------------------------ */
/* 4. Float anatomy                                                     */
/* ------------------------------------------------------------------ */
static void dump_float(float f) {
    uint32_t b;
    memcpy(&b, &f, sizeof b);                 /* legal type punning; *(uint32_t*)&f is UB */
    uint32_t sign = b >> 31, expo = (b >> 23) & 0xFFu, frac = b & 0x7FFFFFu;
    printf("  %-14g = 0x%08" PRIx32 "  S=%u E=%3u (2^%4d) F=0x%06" PRIx32 "\n",
           (double)f, b, sign, expo, (int)expo - 127, frac);
}

/* ------------------------------------------------------------------ */
/* 5. Float comparison                                                  */
/* ------------------------------------------------------------------ */
static bool nearly_equal(double a, double b, double rel_tol, double abs_tol) {
    double diff = fabs(a - b);
    if (diff <= abs_tol) return true;
    return diff <= rel_tol * fmax(fabs(a), fabs(b));
}

/* ------------------------------------------------------------------ */
/* 8. Stable numerics                                                   */
/* ------------------------------------------------------------------ */
static void softmax_naive(const double *z, double *out, size_t n) {
    double s = 0;
    for (size_t i = 0; i < n; i++) { out[i] = exp(z[i]); s += out[i]; }
    for (size_t i = 0; i < n; i++) out[i] /= s;
}
static void softmax_stable(const double *z, double *out, size_t n) {
    double m = z[0];
    for (size_t i = 1; i < n; i++) if (z[i] > m) m = z[i];
    double s = 0;
    for (size_t i = 0; i < n; i++) { out[i] = exp(z[i] - m); s += out[i]; }
    for (size_t i = 0; i < n; i++) out[i] /= s;
}
static double logsumexp(const double *z, size_t n) {
    double m = z[0];
    for (size_t i = 1; i < n; i++) if (z[i] > m) m = z[i];
    double s = 0;
    for (size_t i = 0; i < n; i++) s += exp(z[i] - m);
    return m + log(s);
}
static double sigmoid_naive(double x) { return 1.0 / (1.0 + exp(-x)); }
static double sigmoid_stable(double x) {
    if (x >= 0) { double e = exp(-x); return 1.0 / (1.0 + e); }
    double e = exp(x);
    return e / (1.0 + e);
}

/* ------------------------------------------------------------------ */
/* 9. Summation                                                         */
/* ------------------------------------------------------------------ */
static float sum_naive_f(const float *x, size_t n) {
    float s = 0.0f;
    for (size_t i = 0; i < n; i++) s += x[i];
    return s;
}
static float sum_kahan_f(const float *x, size_t n) {
    float sum = 0.0f, c = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float y = x[i] - c;
        float t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}
static float sum_pairwise_f(const float *x, size_t n) {
    if (n <= 8) { float s = 0.0f; for (size_t i = 0; i < n; i++) s += x[i]; return s; }
    return sum_pairwise_f(x, n / 2) + sum_pairwise_f(x + n / 2, n - n / 2);
}

/* ------------------------------------------------------------------ */
/* 10. Random numbers                                                   */
/* ------------------------------------------------------------------ */
typedef struct { uint64_t s; } Rng;

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static Rng rng_seed(uint64_t seed) {
    Rng r = { splitmix64(&seed) };
    if (r.s == 0) r.s = 1;                     /* xorshift state must be nonzero */
    return r;
}
static uint64_t rng_next(Rng *r) {             /* xorshift64* */
    uint64_t x = r->s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1Dull;
}
static double rng_uniform(Rng *r) {            /* [0, 1) with full 53-bit mantissa */
    return (double)(rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}
static uint64_t rng_below(Rng *r, uint64_t n) { /* unbiased [0, n) */
    uint64_t limit = UINT64_MAX - UINT64_MAX % n, x;
    do { x = rng_next(r); } while (x >= limit);
    return x % n;
}
static double rng_gaussian(Rng *r) {           /* Box-Muller, one sample */
    double u1 = rng_uniform(r), u2 = rng_uniform(r);
    if (u1 < 1e-300) u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ------------------------------------------------------------------ */
/* 11. Timing                                                           */
/* ------------------------------------------------------------------ */
static double now_sec(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* ------------------------------------------------------------------ */
int main(void) {
    puts("== 1. fixed-width types ==");
    printf("  sizeof: uint8_t=%zu int32_t=%zu int64_t=%zu size_t=%zu ptrdiff_t=%zu\n",
           sizeof(uint8_t), sizeof(int32_t), sizeof(int64_t), sizeof(size_t), sizeof(ptrdiff_t));
    printf("  INT_MAX=%d  UINT8 max=%u  INT64_MAX=%" PRId64 "  SIZE_MAX=%zu  CHAR_BIT=%d\n",
           INT_MAX, (unsigned)UINT8_MAX, INT64_MAX, SIZE_MAX, CHAR_BIT);

    puts("\n== 2. wrap vs promotion ==");
    uint8_t a = 200, b = 100;
    uint8_t c = (uint8_t)(a + b);
    printf("  uint8 200+100: promoted sum=%d (>255: %s), stored back=%u\n",
           a + b, (a + b > 255) ? "yes" : "no", c);
    uint32_t u = UINT32_MAX; u++;
    printf("  UINT32_MAX + 1 = %" PRIu32 " (defined wrap)\n", u);
    printf("  -1 < 1u is %s (signed converts to unsigned!)\n", (-1 < 1u) ? "true" : "false");

    puts("\n== 3. bitwise ==");
    uint32_t flags = 0;
    flags |= BIT(3); flags |= BIT(0);  printf("  set 3,0    : "); print_bits(flags, 8); putchar('\n');
    flags &= ~BIT(3);                  printf("  clear 3    : "); print_bits(flags, 8); putchar('\n');
    flags ^= BIT(1);                   printf("  toggle 1   : "); print_bits(flags, 8); putchar('\n');
    printf("  test bit 1 : %d   test bit 5: %d\n", (flags & BIT(1)) != 0, (flags & BIT(5)) != 0);
    printf("  0xC & 0xA=%x  0xC | 0xA=%x  0xC ^ 0xA=%x  ~0xC (u8)=%x  1<<4=%d  0xF0>>4=%d\n",
           0xC & 0xA, 0xC | 0xA, 0xC ^ 0xA, (uint8_t)~0xC, 1 << 4, 0xF0 >> 4);
    printf("  is_pow2(64)=%d is_pow2(96)=%d  popcount(0xFF)=%d popcount(0x2545F4914F6CDD1D)=%d\n",
           is_pow2(64), is_pow2(96), popcount(0xFF), popcount(0x2545F4914F6CDD1Dull));
    printf("  bswap32(0x12345678)=0x%08" PRIx32 "  next_pow2(100)=%" PRIu64 "  next_pow2(1024)=%" PRIu64 "\n",
           bswap32(0x12345678u), next_pow2(100), next_pow2(1024));
    uint32_t rgba = (255u << 24) | (128u << 16) | (0u << 8) | 255u;
    printf("  packed RGBA=0x%08" PRIx32 "  G=%" PRIu32 "\n", rgba, (rgba >> 16) & 0xFFu);

    puts("\n== 4. IEEE 754 float layout ==");
    dump_float(1.0f);
    dump_float(-2.5f);
    dump_float(0.1f);
    dump_float(16777216.0f);
    dump_float(16777217.0f);    /* same bits as above: float has only 24 significant bits */
    dump_float(INFINITY);
    dump_float(NAN);

    puts("\n== 5. epsilon and comparison ==");
    printf("  FLT_EPSILON=%g DBL_EPSILON=%g  FLT_DIG=%d DBL_DIG=%d\n", FLT_EPSILON, DBL_EPSILON, FLT_DIG, DBL_DIG);
    printf("  0.1+0.2 = %.17g   0.3 = %.17g   == ? %d\n", 0.1 + 0.2, 0.3, 0.1 + 0.2 == 0.3);
    printf("  nearly_equal(0.1+0.2, 0.3) = %d   nearly_equal(NAN, NAN) = %d\n",
           nearly_equal(0.1 + 0.2, 0.3, 1e-9, 1e-12), nearly_equal(NAN, NAN, 1e-9, 1e-12));
    printf("  1e16 + 1.0 == 1e16 ? %d  (spacing at 1e16 is %g)\n",
           1e16 + 1.0 == 1e16, nextafter(1e16, INFINITY) - 1e16);

    puts("\n== 6. inf / nan ==");
    volatile double zero = 0.0;   /* volatile: stop the compiler folding 1/0 at compile time */
    double inf = 1.0 / zero, nan = zero / zero;
    printf("  1/0=%g  0/0=%g  inf-inf=%g  inf*0=%g  exp(1000)=%g  exp(-1000)=%g  log(0)=%g\n",
           inf, nan, inf - inf, inf * 0.0, exp(1000.0), exp(-1000.0), log(zero));
    printf("  isnan(nan)=%d isinf(inf)=%d  nan==nan: %d  nan!=nan: %d  fmax(nan,1)=%g\n",
           isnan(nan), isinf(inf), nan == nan, nan != nan, fmax(nan, 1.0));
    double w = 0.5;
    w = w * nan + 1.0;    /* one NaN poisons everything downstream */
    printf("  0.5*nan+1 = %g   <- 'loss is nan' after one bad op\n", w);

    puts("\n== 7. catastrophic cancellation ==");
    /* volatile forces each product to be rounded to float separately. Without it clang's
     * default -ffp-contract=on fuses a*b - c*d into an FMA and computes one product exactly,
     * hiding the cancellation. Results that depend on compiler flags are themselves a lesson. */
    volatile float fx = 1e4f;
    volatile float p1 = (fx + 1) * (fx + 1);    /* 100020001 -> rounds to 100020000 (spacing 8) */
    volatile float p2 = fx * fx;                /* 100000000 exact */
    float fa = p1 - p2;                         /* exact answer: 20001 */
    printf("  float: (1e4+1)^2 - (1e4)^2 = %.1f (exact 20001; lost the 1)\n", (double)fa);
    double x = 1e-10;
    printf("  1-cos(x) for x=1e-10: naive=%g  via 2sin^2(x/2)=%g  (exact 5e-21)\n",
           1.0 - cos(x), 2.0 * sin(x / 2) * sin(x / 2));
    printf("  log(1+x): naive=%g  log1p=%g\n", log(1.0 + x), log1p(x));

    puts("\n== 8. stable softmax / logsumexp / sigmoid ==");
    double z[3] = { 1000, 1001, 1002 }, out[3];
    softmax_naive(z, out, 3);
    printf("  naive softmax(1000,1001,1002)  = %g %g %g\n", out[0], out[1], out[2]);
    softmax_stable(z, out, 3);
    printf("  stable softmax(1000,1001,1002) = %.6f %.6f %.6f  (sum=%.15f)\n",
           out[0], out[1], out[2], out[0] + out[1] + out[2]);
    printf("  logsumexp = %.6f   log_softmax[2] = %.6f  cross-entropy(target=0) = %.6f\n",
           logsumexp(z, 3), z[2] - logsumexp(z, 3), logsumexp(z, 3) - z[0]);
    printf("  sigmoid(-1000): naive=%g stable=%g   sigmoid(40): naive=%.17g stable=%.17g\n",
           sigmoid_naive(-1000), sigmoid_stable(-1000), sigmoid_naive(40), sigmoid_stable(40));

    puts("\n== 9. summation ==");
    const size_t N = 10000000;
    float *xs = malloc(N * sizeof *xs);
    if (!xs) return 1;
    for (size_t i = 0; i < N; i++) xs[i] = 0.1f;
    double t0 = now_sec(); float s_naive = sum_naive_f(xs, N);     double t1 = now_sec();
    float s_kahan = sum_kahan_f(xs, N);                            double t2 = now_sec();
    float s_pair  = sum_pairwise_f(xs, N);                         double t3 = now_sec();
    double exact = (double)N * (double)0.1f;   /* 0.1f is really 0.100000001490116 */
    printf("  exact (N * (double)0.1f) = %.3f\n", exact);
    printf("  naive    = %.3f  err=%.2e  %.1f ms\n", (double)s_naive, fabs(s_naive - exact), (t1 - t0) * 1e3);
    printf("  kahan    = %.3f  err=%.2e  %.1f ms\n", (double)s_kahan, fabs(s_kahan - exact), (t2 - t1) * 1e3);
    printf("  pairwise = %.3f  err=%.2e  %.1f ms\n", (double)s_pair,  fabs(s_pair  - exact), (t3 - t2) * 1e3);
    free(xs);

    puts("\n== 10. random numbers ==");
    Rng r = rng_seed(42);
    printf("  seed 42, first 3 uniforms: %.6f %.6f %.6f\n", rng_uniform(&r), rng_uniform(&r), rng_uniform(&r));
    Rng r2 = rng_seed(42);
    printf("  seed 42 again           : %.6f  (reproducible)\n", rng_uniform(&r2));
    const int M = 200000;
    double mean = 0, m2 = 0;              /* Welford's online mean/variance */
    for (int i = 1; i <= M; i++) {
        double g = rng_gaussian(&r);
        double d = g - mean;
        mean += d / i;
        m2 += d * (g - mean);
    }
    printf("  Box-Muller %d samples: mean=%.4f var=%.4f (expect 0, 1)\n", M, mean, m2 / (M - 1));
    int hist[5] = { 0 };
    for (int i = 0; i < M; i++) hist[rng_below(&r, 5)]++;
    printf("  rng_below(5) histogram: %d %d %d %d %d (expect ~%d each)\n",
           hist[0], hist[1], hist[2], hist[3], hist[4], M / 5);
    double fan_in = 784.0;
    printf("  Kaiming init sample for fan_in=784: %.5f %.5f %.5f (std %.4f)\n",
           rng_gaussian(&r) * sqrt(2.0 / fan_in), rng_gaussian(&r) * sqrt(2.0 / fan_in),
           rng_gaussian(&r) * sqrt(2.0 / fan_in), sqrt(2.0 / fan_in));

    puts("\n== 11. integer vs float division ==");
    int p = 7, q = 2;
    printf("  7/2=%d  7%%2=%d  -7/2=%d  -7%%2=%d  7/(double)2=%.1f  (double)(7/2)=%.1f\n",
           p / q, p % q, -p / q, -p % q, p / (double)q, (double)(p / q));
    return 0;
}
