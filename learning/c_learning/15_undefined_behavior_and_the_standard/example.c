/*
 * Chapter 15 — Undefined Behavior and the C Standard
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 * Prove it's clean (this file contains NO undefined behavior — every "surprising" result
 * below is DEFINED, and every UB pattern is shown only as a comment next to its fix):
 *           cc -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-aliasing=2 -std=c11 \
 *              -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -o ex_demo example.c -lm && ./ex_demo
 *           Expect ZERO sanitizer reports. Expect exactly three -Wsign-conversion warnings, all in
 *           section 2, on the deliberate `-1 < 1u`-style lines: that warning is the tool that finds this
 *           trap in real code, and the demo keeps the lines so you can see it fire.
 * See what the optimizer does:  cc -O2 -std=c11 -S -o ex_demo.s example.c && grep -v '^\s*\.' ex_demo.s
 *
 * Contents:
 *   1. which standard the compiler speaks; implementation-defined facts about this machine
 *   2. integer promotions and usual arithmetic conversions: -1 < 1u and friends (all defined, all surprising)
 *   3. char signedness and the <ctype.h> trap
 *   4. signed overflow: the UB, and three defined ways to detect it
 *   5. shifts: the guards that make them defined
 *   6. type punning: memcpy and union (defined) vs pointer cast (UB, shown as a comment)
 *   7. one-past-the-end pointers; misaligned reads via memcpy; endianness without pointer casts
 *   8. offsetof, _Alignof, _Alignas, _Static_assert: pinning a file-format header layout
 *   9. restrict on a kernel that really doesn't alias; sequencing done right
 *  10. volatile sig_atomic_t flag pattern; setjmp/longjmp with a volatile local
 *  11. _Generic dispatch; _Noreturn
 */
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 8 (used early). Compile-time checks: 6.7.10 _Static_assert          */
/* ------------------------------------------------------------------ */
_Static_assert(CHAR_BIT == 8, "this demo assumes 8-bit bytes (POSIX guarantees it; ISO C does not)");
_Static_assert(sizeof(float) == 4 && sizeof(uint32_t) == 4, "float must be 32-bit for the punning demo");
_Static_assert(sizeof(double) == 8, "double must be 64-bit");
_Static_assert(sizeof(size_t) >= 8, "64-bit platform assumed");

/* A checkpoint header. The asserts below pin its layout so a change breaks the build, not the file. */
typedef struct {
    uint32_t magic;     /* 'NBDY' */
    uint16_t version;
    uint8_t  endian;    /* 1 = little, 2 = big */
    uint8_t  fsize;     /* sizeof(float) used to write the payload */
    uint64_t n;         /* element count */
} CkptHeader;
_Static_assert(offsetof(CkptHeader, magic)   == 0, "layout");
_Static_assert(offsetof(CkptHeader, version) == 4, "layout");
_Static_assert(offsetof(CkptHeader, endian)  == 6, "layout");
_Static_assert(offsetof(CkptHeader, fsize)   == 7, "layout");
_Static_assert(offsetof(CkptHeader, n)       == 8, "layout");
_Static_assert(sizeof(CkptHeader) == 16, "layout: no padding expected at end");

/* ------------------------------------------------------------------ */
/* 11. _Noreturn: the compiler may drop the return path after die()    */
/* ------------------------------------------------------------------ */
static _Noreturn void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------ */
/* 1. Which standard, and the implementation-defined facts             */
/* ------------------------------------------------------------------ */
static void section_standard(void) {
    puts("== 1. standard version and implementation-defined facts ==");
    printf("__STDC_VERSION__ = %ldL  (201112L = C11, 201710L = C17, 202311L = C23)\n", (long)__STDC_VERSION__);
    /* Everything printed here is IMPLEMENTATION-DEFINED (J.3), not UB: each compiler documents its choice. */
    printf("sizeof(int)=%zu long=%zu long long=%zu size_t=%zu ptrdiff_t=%zu void*=%zu\n",
           sizeof(int), sizeof(long), sizeof(long long), sizeof(size_t), sizeof(ptrdiff_t), sizeof(void *));
    printf("CHAR_MIN=%d  -> char is %s (6.2.5p15, implementation-defined)\n",
           CHAR_MIN, CHAR_MIN < 0 ? "signed" : "unsigned");
    printf("-7 / 2 = %d, -7 %% 2 = %d      (DEFINED since C99: truncation toward zero, 6.5.5p6)\n", -7 / 2, -7 % 2);
    printf("-8 >> 1 = %d                  (IMPLEMENTATION-DEFINED, 6.5.7p5: arithmetic shift here)\n", -8 >> 1);
    printf("(int)3.99 = %d, (int)-3.99 = %d (DEFINED: truncation toward zero, 6.3.1.4p1)\n", (int)3.99, (int)-3.99);
    printf("FLT_EVAL_METHOD=%d            (0 = evaluate float ops in float, not extended precision)\n", FLT_EVAL_METHOD);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 2. Integer promotions (6.3.1.1) and usual arithmetic conversions    */
/*    (6.3.1.8). Every line below is DEFINED — and most are surprising. */
/* ------------------------------------------------------------------ */
#define TYPE_NAME(x) _Generic((x), \
    int: "int", unsigned int: "unsigned int", long: "long", unsigned long: "unsigned long", \
    long long: "long long", unsigned long long: "unsigned long long", \
    char: "char", signed char: "signed char", unsigned char: "unsigned char", \
    short: "short", unsigned short: "unsigned short", \
    float: "float", double: "double", default: "other")

static void section_promotions(void) {
    puts("== 2. promotions and usual arithmetic conversions (all defined) ==");

    /* Rule 3 of 6.3.1.8: int vs unsigned int, same rank -> unsigned. -1 becomes UINT_MAX. */
    printf("-1 < 1u   -> %d   (type of -1 < 1u operands: %s; -1 converts to %u)\n",
           -1 < 1u, TYPE_NAME(-1 + 1u), (unsigned)-1);
    /* Rule 4: long (64-bit) can represent every unsigned int -> long. Now it's true. */
    printf("-1L < 1u  -> %d   (common type: %s)\n", -1L < 1u, TYPE_NAME(-1L + 1u));
    /* Rule 3 again with unsigned long: false. */
    printf("-1 < 1ul  -> %d   (common type: %s)\n", -1 < 1ul, TYPE_NAME(-1 + 1ul));

    /* Integer promotions: every type narrower than int becomes (signed) int before any arithmetic. */
    uint8_t a = 200, b = 100;
    int sum_int = a + b;            /* 300: promoted to int, no 8-bit wrap */
    uint8_t sum_u8 = (uint8_t)(a + b); /* 44: the ASSIGNMENT truncates modulo 256 (6.3.1.3p2) — defined */
    printf("uint8_t 200 + 100: as int = %d (type %s), stored in uint8_t = %u\n", sum_int, TYPE_NAME(a + b), sum_u8);

    uint8_t x = 0xFF;
    int not_x = ~x;                 /* -256: ~ applies to the promoted int 0x000000FF */
    uint8_t not_x_u8 = (uint8_t)~x; /* 0: truncation rescues it */
    printf("~(uint8_t)0xFF: as int = %d, stored in uint8_t = %u\n", not_x, not_x_u8);

    /* uint16_t * uint16_t overflows INT — that would be UB. The cast to uint32_t is the fix. */
    uint16_t us = 65535;
    uint32_t prod = (uint32_t)us * us;   /* us promotes to int, then converts to uint32_t via rule 3; defined */
    /* int bad = us * us;  <-- UB (6.5p5): 4294836225 doesn't fit in int. UBSan: "signed integer overflow" */
    printf("(uint32_t)65535 * 65535 = %" PRIu32 "   (without the cast: UB)\n", prod);

    printf("sizeof('A') = %zu             (character constants are int in C, char in C++)\n", sizeof('A'));
    printf("1 ? 1u : -1 = %u               (?: also applies the usual arithmetic conversions)\n", 1 ? 1u : -1);

    /* size_t vs int: the loop trap, shown safely. */
    size_t n = 0;
    size_t iterations = 0;
    for (size_t i = 0; i + 1 < n; i++) iterations++;   /* i + 1 < n instead of i < n - 1: n - 1 == SIZE_MAX when n == 0 */
    printf("stencil loop with n=0 using 'i + 1 < n': %zu iterations (with 'i < n - 1' it would be ~SIZE_MAX)\n", iterations);
    for (size_t i = 4; i-- > 0; ) iterations++;         /* count-down idiom for unsigned */
    printf("count-down idiom 'for (i = 4; i-- > 0;)': %zu iterations\n", iterations);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 3. char signedness and <ctype.h>                                    */
/* ------------------------------------------------------------------ */
/* tiny stand-in so we don't need <ctype.h>'s locale machinery in the demo — same contract as isdigit:
   the argument must be EOF or representable as unsigned char (7.4p1). */
static bool isdigit_safe(int c) { return c >= '0' && c <= '9'; }

static void section_char(void) {
    puts("== 3. char signedness ==");
    /* Bytes >= 0x80 stored in a plain char are negative on this platform. Implementation-defined, not UB
       (6.3.1.3p3) — but the CONSEQUENCES are UB: table[c] with c < 0, isdigit(c) with c < 0. */
    unsigned char raw[4] = { 0x00, 0x7F, 0x80, 0xFF };      /* what a file gives you */
    char as_char[4];
    memcpy(as_char, raw, 4);
    for (int i = 0; i < 4; i++) {
        int via_char   = as_char[i];            /* promoted from (signed) char: may be negative */
        int via_uchar  = (unsigned char)as_char[i];
        printf("byte 0x%02X: as char -> %4d   as unsigned char -> %3u   pixel/255 = %.3f\n",
               raw[i], via_char, via_uchar, via_uchar / 255.0);
    }
    /* isdigit(c) requires c == EOF or 0..UCHAR_MAX (7.4p1). Always cast to unsigned char first. */
    const char *s = "a1\xE9";       /* \xE9 = 233: negative as char */
    int digits = 0;
    for (const char *p = s; *p; p++)
        if (isdigit_safe((unsigned char)*p)) digits++;
    printf("digits in \"a1\\xE9\" = %d   (isdigit((unsigned char)c) is the only defined spelling)\n", digits);
    /* getc returns int so that all 256 byte values AND EOF (-1) are distinguishable. Never store it in a char first. */
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 4. Signed overflow: UB (6.5p5). Three defined detections.           */
/* ------------------------------------------------------------------ */
static bool add_overflows_manual(int a, int b) {
    /* Check BEFORE the operation. After it, it's already too late (the compiler assumed it didn't happen). */
    return b > 0 ? a > INT_MAX - b : a < INT_MIN - b;
}
static int sat_add(int a, int b) {
    int r;
    if (__builtin_add_overflow(a, b, &r))      /* clang/gcc builtin; C23: ckd_add() in <stdckdint.h> */
        return b > 0 ? INT_MAX : INT_MIN;
    return r;
}
static bool mul_fits_size(size_t a, size_t b, size_t *out) {
    /* The rows * cols check every matrix allocator needs. Unsigned, so the comparison itself can't overflow. */
    if (a != 0 && b > SIZE_MAX / a) return false;
    *out = a * b;
    return true;
}
static void section_overflow(void) {
    puts("== 4. signed overflow ==");
    int a = INT_MAX, b = 1;
    /* int c = a + b;  <-- UB. UBSan: "signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'" */
    printf("INT_MAX + 1: manual check says overflow=%d, builtin says overflow=%d, sat_add -> %d\n",
           add_overflows_manual(a, b), __builtin_add_overflow(a, b, &b) ? 1 : 0, sat_add(a, 1));
    unsigned u = UINT_MAX;
    u += 1;                                     /* DEFINED: unsigned wraps modulo 2^32 (6.2.5p9) */
    printf("UINT_MAX + 1u = %u                 (defined wrap)\n", u);
    /* The classic ML-code overflow: rows * cols in int. */
    int rows = 50000, cols = 50000;
    long long true_n = (long long)rows * cols;
    size_t n;
    printf("50000 x 50000: as int would be UB (%lld > INT_MAX); as size_t: fits=%d n=%zu\n",
           true_n, mul_fits_size((size_t)rows, (size_t)cols, &n), n);
    printf("SIZE_MAX/2 x 3: fits=%d\n", mul_fits_size(SIZE_MAX / 2, 3, &n));
    /* abs(INT_MIN) is UB (7.22.6.1p2). Defined alternative: */
    int m = INT_MIN;
    unsigned mag = m < 0 ? 0u - (unsigned)m : (unsigned)m;   /* magnitude as unsigned: always representable */
    printf("|INT_MIN| = %u                     (abs(INT_MIN) is UB; compute in unsigned)\n", mag);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 5. Shifts (6.5.7): count < width, left operand wide and unsigned    */
/* ------------------------------------------------------------------ */
static uint64_t mask_low(unsigned k) {          /* lowest k bits set, k in [0, 64] */
    return k >= 64 ? UINT64_MAX : ((uint64_t)1 << k) - 1;   /* 1ull << 64 would be UB (6.5.7p3): guard it */
}
static uint32_t rotl32(uint32_t x, unsigned r) {
    r &= 31;
    return r == 0 ? x : (x << r) | (x >> (32 - r));          /* x >> 32 is UB: r == 0 handled separately */
}
static void section_shifts(void) {
    puts("== 5. shifts ==");
    printf("1u << 31 = 0x%08X                (1 << 31 is UB in C11: result not representable in int)\n", 1u << 31);
    printf("(uint64_t)1 << 40 = 0x%" PRIX64 "   (1 << 40 would shift a 32-bit int by 40: UB)\n", (uint64_t)1 << 40);
    printf("mask_low(0)=0x%" PRIX64 " mask_low(5)=0x%" PRIX64 " mask_low(64)=0x%" PRIX64 "\n",
           mask_low(0), mask_low(5), mask_low(64));
    printf("rotl32(0x80000001, 1)=0x%08" PRIX32 "  rotl32(x, 32)=0x%08" PRIX32 " (== x)\n",
           rotl32(0x80000001u, 1), rotl32(0x80000001u, 32));
    /* Byte assembly: the cast to uint32_t BEFORE the shift is required. Without it, p[0] promotes to int
       and 0xFF << 24 overflows int -> UB. */
    uint8_t be[4] = { 0x00, 0x00, 0x08, 0x03 };             /* MNIST magic 2051 as big-endian bytes */
    uint32_t magic = ((uint32_t)be[0] << 24) | ((uint32_t)be[1] << 16) | ((uint32_t)be[2] << 8) | be[3];
    printf("big-endian bytes 00 00 08 03 -> %" PRIu32 " (MNIST image magic)\n", magic);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 6. Type punning: the defined ways                                   */
/* ------------------------------------------------------------------ */
static uint32_t float_bits_memcpy(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof u);       /* defined; compiles to a single fmov at -O2 */
    return u;
}
static uint32_t float_bits_union(float f) {
    union { float f; uint32_t u; } x = { .f = f };
    return x.u;                     /* defined in C (6.5.2.3p3 footnote 95) — NOT in C++ */
}
/* static uint32_t float_bits_BAD(float f) { return *(uint32_t *)&f; }   <-- UB: 6.5p7 strict aliasing */
static void section_punning(void) {
    puts("== 6. type punning ==");
    float vals[] = { 1.0f, -0.0f, 1.0e-40f, INFINITY, NAN };
    const char *names[] = { "1.0f", "-0.0f", "1e-40 (denormal)", "INFINITY", "NAN" };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        uint32_t m = float_bits_memcpy(vals[i]), un = float_bits_union(vals[i]);
        printf("%-17s bits: memcpy=0x%08" PRIX32 " union=0x%08" PRIX32 "  sign=%u exp=%3u mant=0x%06" PRIX32 "\n",
               names[i], m, un, m >> 31, (m >> 23) & 0xFF, m & 0x7FFFFF);
    }
    /* unsigned char* may alias anything (6.5p7 last bullet): byte-wise inspection is always defined. */
    double d = 1.0;
    const unsigned char *bytes = (const unsigned char *)&d;
    printf("bytes of double 1.0 in memory: ");
    for (size_t i = 0; i < sizeof d; i++) printf("%02X ", bytes[i]);
    puts("  (little-endian: exponent byte 0x3F last)");
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 7. Pointers: one-past-the-end, misalignment, endianness             */
/* ------------------------------------------------------------------ */
static void section_pointers(void) {
    puts("== 7. one-past-the-end, alignment, endianness ==");
    int a[4] = { 10, 20, 30, 40 };
    int *end = a + 4;               /* DEFINED to form and compare (6.5.6p8). NOT to dereference. */
    /* int *bad = a + 5;  <-- UB the moment it's computed, even if never dereferenced. Same for a - 1. */
    int sum = 0;
    for (int *it = a; it != end; ++it) sum += *it;   /* the idiom C++ iterators formalise */
    printf("sum via [begin, end) = %d; end - a = %td (ptrdiff_t, %%td)\n", sum, end - a);

    /* Misaligned read: (uint32_t *)(buf + 1) is UB by 6.3.2.3p7 before any dereference. memcpy is defined. */
    _Alignas(8) uint8_t buf[8] = { 0xAA, 0x01, 0x02, 0x03, 0x04, 0xBB, 0xCC, 0xDD };
    uint32_t v;
    memcpy(&v, buf + 1, sizeof v);  /* clang emits one unaligned ldr at -O2; no UB */
    printf("unaligned uint32 at buf+1 via memcpy = 0x%08" PRIX32 "\n", v);

    /* Endianness detection without pointer casts: */
    uint32_t one = 1;
    uint8_t first;
    memcpy(&first, &one, 1);
    bool little = first == 1;
    printf("host is %s-endian (memcpy probe)", little ? "little" : "big");
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    printf("; __BYTE_ORDER__ agrees: %s", __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ? "little" : "big");
#endif
    putchar('\n');
    /* Write a uint64 little-endian regardless of host — byte arithmetic, no casts to wider pointers. */
    uint64_t ts = 0x0102030405060708ull;
    uint8_t out[8];
    for (int i = 0; i < 8; i++) out[i] = (uint8_t)(ts >> (8 * i));
    printf("0x0102030405060708 as LE bytes: ");
    for (int i = 0; i < 8; i++) printf("%02X ", out[i]);
    putchar('\n');
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 8. offsetof / _Alignof / _Alignas                                   */
/* ------------------------------------------------------------------ */
struct Padded { char c; double d; short s; };      /* 1 + 7pad + 8 + 2 + 6pad = 24 */
struct Packed { double d; short s; char c; };      /* 8 + 2 + 1 + 5pad = 16 */
typedef struct { _Alignas(128) double partial; } PerThreadAcc;   /* one per cache line: no false sharing (ch.16) */

static void section_layout(void) {
    puts("== 8. offsetof, _Alignof, _Alignas ==");
    printf("CkptHeader: sizeof=%zu align=%zu  magic@%zu version@%zu endian@%zu fsize@%zu n@%zu (pinned by _Static_assert)\n",
           sizeof(CkptHeader), alignof(CkptHeader), offsetof(CkptHeader, magic), offsetof(CkptHeader, version),
           offsetof(CkptHeader, endian), offsetof(CkptHeader, fsize), offsetof(CkptHeader, n));
    printf("struct Padded {char; double; short}: sizeof=%zu  c@%zu d@%zu s@%zu\n",
           sizeof(struct Padded), offsetof(struct Padded, c), offsetof(struct Padded, d), offsetof(struct Padded, s));
    printf("struct Packed {double; short; char}: sizeof=%zu  (members sorted by decreasing alignment)\n",
           sizeof(struct Packed));
    printf("_Alignof: char=%zu int=%zu double=%zu max_align_t=%zu  PerThreadAcc (_Alignas(128))=%zu sizeof=%zu\n",
           alignof(char), alignof(int), alignof(double), alignof(max_align_t), alignof(PerThreadAcc), sizeof(PerThreadAcc));
    /* aligned_alloc: size must be a multiple of alignment (7.22.3.1p1). */
    size_t sz = 4 * sizeof(PerThreadAcc);
    PerThreadAcc *accs = aligned_alloc(128, sz);
    if (!accs) die("aligned_alloc");
    printf("aligned_alloc(128, %zu) -> address %% 128 = %u\n", sz, (unsigned)((uintptr_t)accs % 128));
    free(accs);
    /* container-of: recover the enclosing struct from a pointer to a member. The char* step keeps it defined. */
    struct Node { int payload; struct Link { struct Link *next; } link; };
    struct Node node = { .payload = 42, .link = { NULL } };
    struct Link *lp = &node.link;
    struct Node *recovered = (struct Node *)((char *)lp - offsetof(struct Node, link));
    printf("container_of(&node.link) -> payload %d (same object: %d)\n", recovered->payload, recovered == &node);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 9. restrict (6.7.3p8) and sequencing (6.5p2)                        */
/* ------------------------------------------------------------------ */
/* restrict is a promise: x and y don't overlap. The compiler vectorises without a runtime alias check.
   Calling axpy(n, a, v, v) would be UB. For in-place kernels, don't write restrict. */
static void axpy(size_t n, float alpha, const float *restrict x, float *restrict y) {
    for (size_t i = 0; i < n; i++) y[i] += alpha * x[i];
}
/* Double-buffered step: in and out are distinct buffers, so restrict is honest here. */
static void step_positions(size_t n, double dt, const double *restrict vel, const double *restrict pos_in,
                           double *restrict pos_out) {
    for (size_t i = 0; i < n; i++) pos_out[i] = pos_in[i] + dt * vel[i];
}
static void section_restrict_sequencing(void) {
    puts("== 9. restrict and sequencing ==");
    float x[8], y[8];
    for (size_t i = 0; i < 8; i++) { x[i] = (float)i; y[i] = 1.0f; }
    axpy(8, 2.0f, x, y);
    printf("axpy(y = 1 + 2*i): ");
    for (size_t i = 0; i < 8; i++) printf("%.0f ", (double)y[i]);
    putchar('\n');
    double vel[3] = { 1, 2, 3 }, p0[3] = { 0, 0, 0 }, p1[3];
    step_positions(3, 0.5, vel, p0, p1);
    printf("step_positions(dt=0.5): %.1f %.1f %.1f   (restrict OK: pos_in != pos_out)\n", p1[0], p1[1], p1[2]);

    /* Sequencing. Each statement below has exactly ONE side effect per object per full expression. */
    int i = 0, arr[4] = { 0 };
    arr[i] = i;  i++;               /* the defined version of the UB  arr[i] = i++  */
    arr[i] = i;  i++;
    int j = (i++, i++);             /* comma OPERATOR sequences: defined. j == 3, i == 4 */
    int k = 0;
    if (k++ == 0 && k++ == 1) { }   /* && sequences left before right: defined. k == 2 */
    printf("arr={%d,%d} i=%d j=%d k=%d   (i = i++ / f(i, i++) / a[i] = i++ would each be UB, -Wunsequenced)\n",
           arr[0], arr[1], i, j, k);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 10. volatile sig_atomic_t; setjmp/longjmp with volatile             */
/* ------------------------------------------------------------------ */
/* The one legitimate cross-context use of volatile in ordinary programs (7.14p2): a flag a signal
   handler sets and the main loop polls. NOT for threads (still a data race) — see chapter 16. */
static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }   /* only async-signal-safe operations allowed here */

static jmp_buf g_recover;
static void parse_or_bail(const char *s) {
    if (s == NULL) longjmp(g_recover, 2);      /* returns from setjmp again, with value 2 */
}
static void section_volatile_setjmp(void) {
    puts("== 10. volatile and setjmp/longjmp ==");
    signal(SIGINT, on_sigint);                 /* sigaction() is the better API; chapter 17 */
    /* Simulate the loop: we don't actually wait for Ctrl-C in a demo. */
    int ticks = 0;
    while (!g_stop && ticks < 3) ticks++;      /* the read of g_stop can't be hoisted out of the loop: volatile */
    raise(SIGINT);                             /* deliver the signal to ourselves */
    printf("loop ran %d ticks; after raise(SIGINT): g_stop=%d (volatile sig_atomic_t)\n", ticks, (int)g_stop);

    /* setjmp/longjmp. attempts is modified between setjmp and longjmp -> MUST be volatile (7.13.2.1p3),
       otherwise its value after the jump is indeterminate. */
    volatile int attempts = 0;
    int plain_hint = 0;                        /* NOT modified after setjmp: fine without volatile */
    if (setjmp(g_recover) == 0) {             /* setjmp only as the whole controlling expression (7.13.1.1p4) */
        attempts = attempts + 1;
        parse_or_bail(NULL);                   /* -> longjmp -> we re-enter the if with nonzero */
        plain_hint = 99;                       /* never reached */
    } else {
        printf("longjmp recovered: attempts=%d (guaranteed 1 because volatile), plain_hint=%d\n",
               attempts, plain_hint);
    }
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/* 11. _Generic: type-correct dispatch, no promotion surprises         */
/* ------------------------------------------------------------------ */
#define ABS(x) _Generic((x), int: abs, long: labs, long long: llabs, float: fabsf, double: fabs)(x)
static void section_generic(void) {
    puts("== 11. _Generic ==");
    int   i = -3; long l = -4L; float f = -1.5f; double d = -2.5;
    printf("ABS(int)=%d ABS(long)=%ld ABS(float)=%.1f ABS(double)=%.1f   (each picks its own function)\n",
           ABS(i), ABS(l), (double)ABS(f), ABS(d));
    printf("TYPE_NAME(1 + 1u)=%s  TYPE_NAME('a')=%s  TYPE_NAME(1.0f + 1)=%s  TYPE_NAME(1.0f + 1.0)=%s\n",
           TYPE_NAME(1 + 1u), TYPE_NAME('a'), TYPE_NAME(1.0f + 1), TYPE_NAME(1.0f + 1.0));
    putchar('\n');
}

int main(void) {
    section_standard();
    section_promotions();
    section_char();
    section_overflow();
    section_shifts();
    section_punning();
    section_pointers();
    section_layout();
    section_restrict_sequencing();
    section_volatile_setjmp();
    section_generic();
    puts("All of the above is DEFINED behavior. Rebuild with -fsanitize=undefined to confirm: no reports.");
    return 0;
}
