/*
 * Chapter 04 example: arrays, flat matrices, decay, strings, string.h,
 * snprintf, strtol/strtod, splitting, ctype, UTF-8 bytes.
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 *   ./ex_demo
 *
 * Recommended second build while learning (catches out-of-bounds at runtime):
 *   cc -Wall -Wextra -std=c11 -O1 -g -fsanitize=address -o ex_demo example.c -lm
 */

#include <stdio.h>
#include <stdlib.h>   /* strtol, strtod */
#include <string.h>   /* strlen, strcpy, memcpy, ... */
#include <ctype.h>    /* isdigit, isalpha, toupper, ... */
#include <errno.h>    /* errno, ERANGE */
#include <math.h>     /* isnan, NAN */

#define ARRAY_LEN(a) (sizeof (a) / sizeof (a)[0])

/* Flat row-major index helper: the matrix idiom used throughout the course. */
static inline size_t idx(size_t i, size_t j, size_t cols) { return i * cols + j; }

/* A function receiving an array receives a pointer + must be told the length. */
static double vec_sum(const double *v, size_t n)
{
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += v[i];
    return s;
}

/* Declaring this as `double v[]` is IDENTICAL to `double *v`; clang even warns
   (-Wsizeof-array-argument) if you then write `sizeof v`, because it knows
   you probably wanted the array size. We spell it as the pointer it is. */
static void show_decay(double *v, size_t n)
{
    printf("  inside function: sizeof v = %zu (a pointer), n = %zu\n", sizeof v, n);
}

static void mat_print(const double *m, size_t rows, size_t cols)
{
    for (size_t i = 0; i < rows; i++) {
        printf("   ");
        for (size_t j = 0; j < cols; j++) printf("%6.1f", m[idx(i, j, cols)]);
        printf("\n");
    }
}

/* out (rows x cols) = a (rows x k) * b (k x cols), all flat row-major */
static void mat_mul(const double *a, const double *b, double *out,
                    size_t rows, size_t k, size_t cols)
{
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++) {
            double acc = 0.0;
            for (size_t t = 0; t < k; t++) acc += a[idx(i, t, k)] * b[idx(t, j, cols)];
            out[idx(i, j, cols)] = acc;
        }
}

/* Parse a whole string as a long. 0 on success, -1 on any problem. */
static int parse_long(const char *s, long *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s) return -1;                       /* nothing parsed */
    if (errno == ERANGE) return -1;                /* overflow */
    while (isspace((unsigned char)*end)) end++;    /* allow trailing spaces */
    if (*end != '\0') return -1;                   /* junk after number */
    *out = v;
    return 0;
}

/* CSV row of doubles, manual scan (no strtok): keeps empty fields as NAN. */
static int parse_csv_row(const char *s, double *out, int max)
{
    int n = 0;
    while (n < max) {
        while (*s == ' ') s++;
        if (*s == ',' || *s == '\0') {
            out[n++] = NAN;                        /* empty field */
        } else {
            char *end;
            out[n++] = strtod(s, &end);
            if (end == s) return -1;               /* not a number */
            s = end;
            while (*s == ' ') s++;
        }
        if (*s == '\0') break;
        if (*s != ',') return -1;                  /* junk */
        s++;
    }
    return n;
}

int main(void)
{
    /* ---------------------------------------------------------------- */
    /* 1. Fixed arrays, initialization, sizeof                          */
    /* ---------------------------------------------------------------- */
    printf("== arrays ==\n");
    int full[5]    = {1, 2, 3, 4, 5};
    int partial[5] = {1, 2};                       /* rest zero */
    int zeros[5]   = {0};
    int desig[5]   = {[1] = 7, [4] = 9};
    double w[]     = {0.1, 0.2, 0.3, 0.4};         /* size inferred */
    printf("  partial: %d %d %d %d %d\n", partial[0], partial[1], partial[2], partial[3], partial[4]);
    printf("  desig:   %d %d %d %d %d   zeros[3]=%d\n", desig[0], desig[1], desig[2], desig[3], desig[4], zeros[3]);
    printf("  sizeof full=%zu bytes, ARRAY_LEN(w)=%zu, sum(w)=%g\n",
           sizeof full, ARRAY_LEN(w), vec_sum(w, ARRAY_LEN(w)));
    show_decay(w, ARRAY_LEN(w));

    /* Out-of-bounds would be UB. We do not execute it; ASan would report:
       full[5] = 0;   // stack-buffer-overflow WRITE of size 4 */

    /* ---------------------------------------------------------------- */
    /* 2. 2-D arrays and row-major layout                               */
    /* ---------------------------------------------------------------- */
    printf("\n== 2-D layout ==\n");
    double a2[2][3] = {{1, 2, 3}, {4, 5, 6}};
    const double *flat = &a2[0][0];
    printf("  a2[1][2]=%g == flat[1*3+2]=%g; sizeof a2=%zu, sizeof a2[0]=%zu\n",
           a2[1][2], flat[1 * 3 + 2], sizeof a2, sizeof a2[0]);
    printf("  contiguous: ");
    for (size_t k = 0; k < 6; k++) printf("%g ", flat[k]);
    printf("\n");

    /* ---------------------------------------------------------------- */
    /* 3. Flat matrices: the preferred representation                   */
    /* ---------------------------------------------------------------- */
    printf("\n== flat matrix ==\n");
    double A[6] = {1, 2, 3,
                   4, 5, 6};                       /* 2x3 */
    double B[6] = {7, 8,
                   9, 10,
                   11, 12};                        /* 3x2 */
    double C[4];                                   /* 2x2 */
    mat_mul(A, B, C, 2, 3, 2);
    printf("  A (2x3):\n");  mat_print(A, 2, 3);
    printf("  A*B (2x2):\n"); mat_print(C, 2, 2);

    /* ---------------------------------------------------------------- */
    /* 4. Strings: char arrays with a terminator                        */
    /* ---------------------------------------------------------------- */
    printf("\n== strings ==\n");
    char s[] = "hello";                            /* 6 bytes: h e l l o \0 */
    printf("  \"%s\": strlen=%zu sizeof=%zu bytes:", s, strlen(s), sizeof s);
    for (size_t i = 0; i < sizeof s; i++) printf(" %d", s[i]);
    printf("\n");
    s[0] = 'j';                                    /* arrays are writable */
    const char *lit = "read-only literal";        /* writing through lit would be UB */
    printf("  modified: %s;  literal: %s\n", s, lit);

    /* ---------------------------------------------------------------- */
    /* 5. string.h                                                      */
    /* ---------------------------------------------------------------- */
    printf("\n== string.h ==\n");
    char buf[32];
    strcpy(buf, "matrix");                         /* OK: 7 bytes into 32 */
    strcat(buf, "_mul");                           /* OK: total 11 */
    printf("  buf=%s  strcmp(buf,\"matrix\")=%d  strcmp(\"a\",\"a\")=%d\n",
           buf, strcmp(buf, "matrix") > 0, strcmp("a", "a"));
    char *us = strchr(buf, '_');
    printf("  strchr '_' -> \"%s\" at offset %ld; strstr \"x_m\" -> \"%s\"\n",
           us, (long)(us - buf), strstr(buf, "x_m"));
    double src[4] = {1, 2, 3, 4}, dst[4];
    memcpy(dst, src, sizeof src);                  /* copy 32 bytes */
    memset(src, 0, sizeof src);                    /* zero 32 bytes */
    printf("  memcpy: dst[3]=%g; memset: src[3]=%g; memcmp equal=%d\n",
           dst[3], src[3], memcmp(src, dst, sizeof src) == 0);

    /* strncpy trap: no terminator if src fills the buffer */
    char small[4];
    strncpy(small, "abcdef", sizeof small);        /* small = a b c d, NO '\0' */
    small[sizeof small - 1] = '\0';                /* must terminate by hand */
    printf("  strncpy + manual terminator: \"%s\"\n", small);

    /* ---------------------------------------------------------------- */
    /* 6. snprintf: safe string building                                */
    /* ---------------------------------------------------------------- */
    printf("\n== snprintf ==\n");
    char name[16];
    int want = snprintf(name, sizeof name, "loss=%.4f step=%d", 0.123456, 42);
    printf("  \"%s\" wanted %d chars, truncated=%s\n",
           name, want, want >= (int)sizeof name ? "yes" : "no");
    char path[64];
    snprintf(path, sizeof path, "out/frame_%04d.ppm", 7);
    printf("  %s\n", path);
    char line[64];
    int off = 0;
    double row[3] = {1.5, -2.25, 3.0};
    for (int i = 0; i < 3 && off < (int)sizeof line - 1; i++)
        off += snprintf(line + off, sizeof line - (size_t)off, "%s%g", i ? "," : "", row[i]);
    printf("  joined: %s\n", line);

    /* ---------------------------------------------------------------- */
    /* 7. Strings to numbers                                            */
    /* ---------------------------------------------------------------- */
    printf("\n== strtol / strtod ==\n");
    const char *inputs[] = {"42", "  -17  ", "abc", "42abc", "99999999999999999999"};
    for (size_t i = 0; i < ARRAY_LEN(inputs); i++) {
        long v;
        if (parse_long(inputs[i], &v) == 0) printf("  \"%s\" -> %ld\n", inputs[i], v);
        else                                printf("  \"%s\" -> error\n", inputs[i]);
    }
    char *end;
    double d = strtod("3.75e2 rest", &end);
    printf("  strtod(\"3.75e2 rest\") = %g, stopped at \"%s\"\n", d, end);
    printf("  atoi(\"abc\")=%d (useless for errors)  strtol(\"ff\",16)=%ld\n",
           atoi("abc"), strtol("ff", NULL, 16));

    /* ---------------------------------------------------------------- */
    /* 8. Splitting: strtok vs manual scan                              */
    /* ---------------------------------------------------------------- */
    printf("\n== splitting ==\n");
    char csv_tok[] = "1.5, 2.25 ,3,,4";            /* writable copy: strtok destroys it */
    printf("  strtok:  ");
    for (char *tok = strtok(csv_tok, ", "); tok; tok = strtok(NULL, ", "))
        printf("[%s] ", tok);
    printf(" <- empty field lost\n");
    double fields[8];
    int nf = parse_csv_row("1.5, 2.25 ,3,,4", fields, 8);
    printf("  manual:  %d fields:", nf);
    for (int i = 0; i < nf; i++) printf(" %g", fields[i]);
    printf("  <- empty field kept as nan\n");
    printf("  manual on \"1,abc\": %d (error)\n", parse_csv_row("1,abc", fields, 8));

    /* ---------------------------------------------------------------- */
    /* 9. char is an integer; ctype                                     */
    /* ---------------------------------------------------------------- */
    printf("\n== char / ctype ==\n");
    char c = 'a';
    printf("  '%c'=%d, '%c'+1='%c', '%c'-32='%c', '7'-'0'=%d\n",
           c, c, c, c + 1, c, c - 32, '7' - '0');
    const char *mixed = "Mixed Case 123!";
    char upper[32];
    int nd = 0, na = 0;
    size_t i = 0;
    for (; mixed[i] != '\0'; i++) {
        unsigned char uc = (unsigned char)mixed[i];   /* cast before ctype calls */
        upper[i] = (char)toupper(uc);
        if (isdigit(uc)) nd++;
        if (isalpha(uc)) na++;
    }
    upper[i] = '\0';
    printf("  toupper: %s   digits=%d letters=%d\n", upper, nd, na);

    /* Word count: a first pre-tokenizer */
    const char *text = "the quick  brown fox\tjumps";
    int words = 0, in_word = 0;
    for (const char *p = text; *p; p++) {
        int sp = isspace((unsigned char)*p);
        if (!sp && !in_word) words++;
        in_word = !sp;
    }
    printf("  \"%s\" has %d words\n", text, words);

    /* ---------------------------------------------------------------- */
    /* 10. UTF-8 is bytes                                               */
    /* ---------------------------------------------------------------- */
    printf("\n== UTF-8 ==\n");
    const char *u8 = "h\xc3\xa9llo";                /* "héllo": é = C3 A9 */
    printf("  strlen=%zu (bytes)  hex:", strlen(u8));
    int cps = 0;
    for (size_t k = 0; u8[k]; k++) {
        unsigned char b = (unsigned char)u8[k];
        printf(" %02x", b);
        if ((b & 0xC0) != 0x80) cps++;              /* not a continuation byte */
    }
    printf("  code points=%d\n", cps);
    printf("  A byte-level BPE tokenizer never needs to know where characters begin.\n");

    return 0;
}
