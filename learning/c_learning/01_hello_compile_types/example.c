/*
 * Chapter 01 example: compilation, printf, types, conversions, overflow.
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 *   ./ex_demo; echo "exit code: $?"
 *
 * Try also:
 *   cc -E example.c | wc -l          (how much text the preprocessor pastes in)
 *   cc -S -o - example.c | head -40  (arm64 assembly for this file)
 */

#include <stdio.h>    /* printf, fprintf */
#include <stdlib.h>   /* EXIT_SUCCESS */
#include <limits.h>   /* INT_MAX, UINT_MAX, ... */
#include <float.h>    /* FLT_EPSILON, DBL_EPSILON */
#include <stdint.h>   /* uint8_t, int32_t, ... */
#include <math.h>     /* fabs, sqrt: needs -lm on Linux */

/* Preprocessor constant: textual replacement, no type, no semicolon. */
#define N_SAMPLES 5

/* Typed constant: has a type and a scope; visible in a debugger. */
static const double TOL = 1e-9;

int main(void)
{
    /* ---------------------------------------------------------------- */
    /* 1. Sizes of the fundamental types on this platform (arm64 macOS) */
    /* ---------------------------------------------------------------- */
    printf("== sizes (bytes) ==\n");
    printf("char=%zu short=%zu int=%zu long=%zu long long=%zu\n",
           sizeof(char), sizeof(short), sizeof(int), sizeof(long), sizeof(long long));
    printf("float=%zu double=%zu pointer=%zu size_t=%zu\n",
           sizeof(float), sizeof(double), sizeof(void *), sizeof(size_t));

    /* ---------------------------------------------------------------- */
    /* 2. printf specifiers, width, precision                           */
    /* ---------------------------------------------------------------- */
    printf("\n== printf ==\n");
    int      n   = -42;
    unsigned u   = 42u;
    long     big = 1234567890123L;      /* L suffix: long literal */
    size_t   sz  = sizeof(double);
    double   pi  = 3.14159265358979;
    char     c   = 'Q';                 /* 'Q' is an int literal (81), stored in a char */
    const char *s = "matrix";

    printf("%%d   -> %d\n",   n);
    printf("%%u   -> %u\n",   u);
    printf("%%ld  -> %ld\n",  big);
    printf("%%zu  -> %zu\n",  sz);
    printf("%%f   -> %f\n",   pi);
    printf("%%.2f -> %.2f\n", pi);
    printf("%%10.4f -> [%10.4f]\n", pi);   /* width 10, 4 decimals, right-aligned */
    printf("%%-10.4f -> [%-10.4f]\n", pi); /* left-aligned */
    printf("%%e   -> %e\n",   pi);
    printf("%%.3e -> %.3e\n", 6.02214076e23);
    printf("%%g   -> %g   %g   %g\n", pi, 1e-7, 100.0);
    printf("%%c   -> %c (as int: %d)\n", c, c);
    printf("%%s   -> %s   [%10s]\n", s, s);
    printf("%%p   -> %p\n",   (void *)&n);    /* %p wants a void* */
    printf("%%x   -> %x   %#X   %08x\n", 255u, 255u, 255u);

    /* ---------------------------------------------------------------- */
    /* 3. Literals and their types                                      */
    /* ---------------------------------------------------------------- */
    printf("\n== literals ==\n");
    printf("0x1F = %d, 017 (octal!) = %d, 1e3 = %g, 'a' = %d\n", 0x1F, 017, 1e3, 'a');
    printf("sizeof 'a' = %zu (it is an int), sizeof \"a\" = %zu (char[2])\n",
           sizeof 'a', sizeof "a");
    printf("sizeof 1.0f = %zu, sizeof 1.0 = %zu\n", sizeof 1.0f, sizeof 1.0);

    /* ---------------------------------------------------------------- */
    /* 4. Integer vs floating division                                  */
    /* ---------------------------------------------------------------- */
    printf("\n== division ==\n");
    int a = 7, b = 2;
    printf("7 / 2         = %d\n", a / b);              /* 3  */
    printf("-7 / 2        = %d (truncates toward 0; Python floors to -4)\n", -a / b);
    printf("7 %% 2         = %d\n", a % b);              /* 1  */
    printf("-7 %% 2        = %d (sign of dividend; Python gives 1)\n", -a % b);
    printf("7 / 2.0       = %f\n", a / 2.0);            /* 3.5 */
    printf("(double)7 / 2 = %f\n", (double)a / b);      /* 3.5 */
    printf("(double)(7/2) = %f  <- cast too late\n", (double)(a / b)); /* 3.0 */

    /* The mean bug: sum and count are ints. */
    int samples[N_SAMPLES] = {3, 4, 4, 5, 6};
    int sum = 0;
    for (int i = 0; i < N_SAMPLES; i++) sum += samples[i];
    printf("mean wrong = %d, mean right = %.4f\n", sum / N_SAMPLES, (double)sum / N_SAMPLES);

    /* ---------------------------------------------------------------- */
    /* 5. Implicit conversions and integer promotion                    */
    /* ---------------------------------------------------------------- */
    printf("\n== conversions ==\n");
    unsigned char b1 = 250, b2 = 10;
    printf("250 + 10 as int      = %d (promoted to int, no wrap)\n", b1 + b2);
    unsigned char b3 = (unsigned char)(b1 + b2);
    printf("stored in uchar      = %u (wraps mod 256)\n", b3);

    int    si = -1;
    unsigned ui = 1u;
    /* Written with an explicit cast to show the rule; without the cast
       clang warns (-Wsign-compare), which is what you want. */
    printf("-1 < 1u              = %d (false: -1 becomes %u)\n",
           (unsigned)si < ui, (unsigned)si);

    double d = 3.99;
    int truncated = (int)d;
    printf("(int)3.99            = %d\n", truncated);
    printf("(int)-3.99           = %d\n", (int)-3.99);

    float f = 16777217.0f;   /* 2^24 + 1 is not representable in float */
    printf("float(16777217)      = %.1f\n", f);

    /* ---------------------------------------------------------------- */
    /* 6. Overflow: unsigned wraps (defined), signed is UB              */
    /* ---------------------------------------------------------------- */
    printf("\n== overflow ==\n");
    unsigned umax = UINT_MAX;
    umax = umax + 1u;
    printf("UINT_MAX + 1u        = %u (defined: modulo 2^32)\n", umax);
    printf("0u - 1u              = %u\n", 0u - 1u);

    int imax = INT_MAX;
    int k = 1;
    /* imax + 1 would be UB. Check first. */
    if (k > 0 && imax > INT_MAX - k)
        printf("INT_MAX + %d         : would overflow, not computed\n", k);

    int rows = 100000, cols = 100000;
    long elems = (long)rows * cols;   /* cast BEFORE multiply */
    printf("100000*100000 in long = %ld\n", elems);

    /* ---------------------------------------------------------------- */
    /* 7. Floating point facts                                          */
    /* ---------------------------------------------------------------- */
    printf("\n== floats ==\n");
    printf("0.1f = %.20f\n0.1  = %.20f\n", 0.1f, 0.1);
    printf("FLT_EPSILON = %e, DBL_EPSILON = %e\n", FLT_EPSILON, DBL_EPSILON);
    double x = 0.1 + 0.2;
    printf("0.1 + 0.2 == 0.3 ? %d;  |diff| < TOL ? %d\n", x == 0.3, fabs(x - 0.3) < TOL);
    printf("1.0 / 0.0 = %f, 0.0 / 0.0 = %f (IEEE: defined, not UB)\n", 1.0 / 0.0, 0.0 / 0.0);
    printf("sqrt(2) = %.15f\n", sqrt(2.0));

    /* Fixed-width types for when the width matters (bytes, token ids). */
    uint8_t byte = 255;
    int32_t token_id = 50256;
    printf("uint8_t %u, int32_t %d, sizeof(int64_t)=%zu\n", byte, token_id, sizeof(int64_t));

    /* ---------------------------------------------------------------- */
    /* 8. Exit code                                                     */
    /* ---------------------------------------------------------------- */
    if (sizeof(int) != 4) {
        fprintf(stderr, "unexpected int size\n");
        return 1;                 /* nonzero = failure; check with echo $? */
    }
    return EXIT_SUCCESS;          /* 0 */
}
