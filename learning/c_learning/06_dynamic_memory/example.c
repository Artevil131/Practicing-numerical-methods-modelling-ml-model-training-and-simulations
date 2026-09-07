/*
 * Chapter 06 — Dynamic Memory: worked example
 *
 * Compile + run (normal):
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * Compile + run with sanitizers (recommended while learning):
 *   cc -g -fsanitize=address,undefined -Wall -Wextra -std=c11 -o ex_demo example.c -lm
 *   ./ex_demo
 *
 * Deliberately trigger a memory bug (under the ASan build):
 *   ./ex_demo dfree | ./ex_demo uaf | ./ex_demo overflow
 * Leak detection (LeakSanitizer is not supported on Apple Silicon; use `leaks`):
 *   cc -g -O0 -Wall -Wextra -std=c11 -o ex_demo example.c -lm
 *   MallocStackLogging=1 leaks --atExit -- ./ex_demo leak
 *
 * Demonstrates:
 *   1. malloc / calloc / realloc / free semantics, sizeof *p idiom, NULL checks
 *   2. ownership documented in comments
 *   3. growable array (Vec) with doubling
 *   4. flat 2D matrix vs array-of-pointers
 *   5. aligned_alloc
 *   6. arena / bump allocator
 *   7. the four memory bugs, opt-in via argv
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ---------------------------------------------------------------------------
 * 1. A tiny fail-fast allocator wrapper. Fine for demos and small programs;
 *    a reusable library should return an error to the caller instead.
 * ------------------------------------------------------------------------- */
static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "fatal: out of memory (%zu bytes)\n", n); exit(EXIT_FAILURE); }
    return p;
}

/* ---------------------------------------------------------------------------
 * 2. Ownership in comments. Three verbs: returns ownership / takes ownership /
 *    borrows. Every function that touches a heap pointer states one of them.
 * ------------------------------------------------------------------------- */

/* Returns a newly allocated array of n doubles, each set to v.
 * Caller owns the result and must free() it. Returns NULL on failure. */
static double *vec_filled(size_t n, double v) {
    double *p = malloc(n * sizeof *p);        /* sizeof *p: follows p's type */
    if (!p) return NULL;
    for (size_t i = 0; i < n; i++) p[i] = v;
    return p;
}

/* Borrows x (read-only during the call). Does not allocate. */
static double vec_sum(const double *x, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += x[i];
    return s;
}

/* Fills out[0..n) with softmax(x). Caller provides out; nothing is allocated.
 * This is the "caller buffer" style used in hot loops. */
static void softmax_into(double *out, const double *x, size_t n) {
    double mx = x[0];
    for (size_t i = 1; i < n; i++) if (x[i] > mx) mx = x[i];
    double z = 0.0;
    for (size_t i = 0; i < n; i++) { out[i] = exp(x[i] - mx); z += out[i]; }
    for (size_t i = 0; i < n; i++) out[i] /= z;
}

/* ---------------------------------------------------------------------------
 * 3. Growable array with doubling: amortized O(1) push.
 * ------------------------------------------------------------------------- */
typedef struct {
    double *data;   /* owned by the Vec; released in vec_free */
    size_t  len;    /* elements in use */
    size_t  cap;    /* elements allocated */
} Vec;

/* Appends x. Returns 0 on success, -1 on allocation failure (v unchanged). */
static int vec_push(Vec *v, double x) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 8;
        /* realloc(NULL, n) behaves as malloc(n), so an empty Vec {0} just works.
         * Store into tmp first: if realloc fails, v->data is still valid. */
        double *tmp = realloc(v->data, ncap * sizeof *tmp);
        if (!tmp) return -1;
        v->data = tmp;
        v->cap  = ncap;
    }
    v->data[v->len++] = x;
    return 0;
}

/* Releases the Vec's storage and resets it to a valid empty state. */
static void vec_free(Vec *v) {
    free(v->data);
    v->data = NULL;              /* NULL after free: double free becomes harmless */
    v->len = v->cap = 0;
}

/* ---------------------------------------------------------------------------
 * 4. 2D matrices: flat block (preferred) vs array of row pointers (jagged).
 * ------------------------------------------------------------------------- */

/* Returns a zeroed rows*cols block, row-major. Caller owns; free() it. */
static double *flat_alloc(size_t rows, size_t cols) {
    return calloc(rows * cols, sizeof(double));   /* calloc checks the multiply for overflow */
}
#define FLAT(m, cols, i, j) ((m)[(size_t)(i) * (cols) + (j)])

/* Returns rows separate heap rows. Caller owns; release with jagged_free().
 * Cleans up already-allocated rows if one allocation fails midway. */
static double **jagged_alloc(size_t rows, size_t cols) {
    double **m = malloc(rows * sizeof *m);
    if (!m) return NULL;
    for (size_t i = 0; i < rows; i++) {
        m[i] = calloc(cols, sizeof *m[i]);
        if (!m[i]) {
            while (i-- > 0) free(m[i]);   /* walk back down */
            free(m);
            return NULL;
        }
    }
    return m;
}
static void jagged_free(double **m, size_t rows) {
    if (!m) return;
    for (size_t i = 0; i < rows; i++) free(m[i]);
    free(m);
}

/* ---------------------------------------------------------------------------
 * 6. Arena / bump allocator: allocate one big block, hand out pieces by bumping
 *    an offset, free everything at once. No per-object free.
 * ------------------------------------------------------------------------- */
typedef struct {
    unsigned char *base;   /* owned; released in arena_free */
    size_t used, cap;
} Arena;

/* Returns 0 on success, -1 on allocation failure. */
static int arena_init(Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return -1;
    a->used = 0;
    a->cap  = cap;
    return 0;
}

/* Returns n bytes aligned to `align` (power of two), or NULL if full.
 * The arena owns the memory: do NOT free() the returned pointer. */
static void *arena_alloc(Arena *a, size_t n, size_t align) {
    size_t start = (a->used + align - 1) & ~(align - 1);   /* round up */
    if (start + n > a->cap) return NULL;
    a->used = start + n;
    return a->base + start;
}

static void arena_reset(Arena *a) { a->used = 0; }         /* O(1) "free all" */
static void arena_free(Arena *a)  { free(a->base); a->base = NULL; a->used = a->cap = 0; }

/* ---------------------------------------------------------------------------
 * 7. The four bugs. Only executed when explicitly requested via argv, so the
 *    default run is clean. Compile with -fsanitize=address to see the reports.
 * ------------------------------------------------------------------------- */
static void bug_leak(void) {
    double *p = xmalloc(100 * sizeof *p);
    p[0] = 1.0;
    printf("leak: allocated 800 bytes at %p and 'forgot' to free.\n", (void *)p);
    printf("      LeakSanitizer is NOT available on Apple Silicon; use:  leaks --atExit -- ./ex_demo leak\n");
    p = NULL;              /* drop the last reference; the 800 bytes are now unreachable */
    (void)p;
    /* missing free(p) */
}
static void bug_double_free(void) {
    int *p = xmalloc(sizeof *p);
    free(p);
    printf("dfree: about to free the same pointer twice...\n");
    free(p);                                  /* UB: double free */
}
static void bug_use_after_free(void) {
    int *p = xmalloc(4 * sizeof *p);
    p[0] = 42;
    free(p);
    printf("uaf: reading freed memory...\n");
    printf("p[0] = %d\n", p[0]);              /* UB: use after free */
}
static void bug_overflow(void) {
    double *v = xmalloc(10 * sizeof *v);
    printf("overflow: writing v[10] on a 10-element array...\n");
    for (int i = 0; i <= 10; i++) v[i] = 0.0; /* UB: off-by-one, writes past the end */
    free(v);
}

/* ------------------------------------------------------------------------- */
int main(int argc, char **argv) {
    if (argc > 1) {
        if      (!strcmp(argv[1], "leak"))     bug_leak();
        else if (!strcmp(argv[1], "dfree"))    bug_double_free();
        else if (!strcmp(argv[1], "uaf"))      bug_use_after_free();
        else if (!strcmp(argv[1], "overflow")) bug_overflow();
        else fprintf(stderr, "unknown bug '%s' (leak|dfree|uaf|overflow)\n", argv[1]);
        return 0;
    }

    printf("=== 1. malloc / calloc / realloc / free ===\n");
    int *a = malloc(4 * sizeof *a);            /* 16 bytes, contents are garbage */
    if (!a) { perror("malloc"); return 1; }
    for (int i = 0; i < 4; i++) a[i] = i * i;  /* write before read */

    int *z = calloc(4, sizeof *z);             /* 16 bytes, all zero */
    if (!z) { perror("calloc"); free(a); return 1; }
    printf("calloc'd: %d %d %d %d\n", z[0], z[1], z[2], z[3]);
    free(z);

    int *tmp = realloc(a, 8 * sizeof *tmp);    /* grow; a[0..3] preserved; may move */
    if (!tmp) { free(a); return 1; }           /* on failure, a is still valid */
    a = tmp;
    for (int i = 4; i < 8; i++) a[i] = i * i;
    printf("after realloc: ");
    for (int i = 0; i < 8; i++) printf("%d ", a[i]);
    printf("\n");
    free(a);
    a = NULL;
    free(a);                                   /* free(NULL) is a no-op */
    printf("free(NULL) was fine\n");

    printf("\n=== 2. ownership ===\n");
    double *ones = vec_filled(5, 1.0);         /* we now own `ones` */
    if (!ones) return 1;
    printf("sum of five ones = %.1f\n", vec_sum(ones, 5));   /* vec_sum borrows */
    free(ones); ones = NULL;

    double logits[4] = {2.0, 1.0, 0.1, -1.0};
    double probs[4];                           /* caller-provided buffer on the stack */
    softmax_into(probs, logits, 4);
    printf("softmax = %.3f %.3f %.3f %.3f (sum %.3f)\n",
           probs[0], probs[1], probs[2], probs[3], vec_sum(probs, 4));

    printf("\n=== 3. growable array ===\n");
    Vec v = {0};                               /* data=NULL, len=0, cap=0: valid empty Vec */
    for (int i = 0; i < 100; i++)
        if (vec_push(&v, i * 0.5) != 0) { fprintf(stderr, "push failed\n"); vec_free(&v); return 1; }
    printf("len=%zu cap=%zu  (cap doubled 8->16->32->64->128)\n", v.len, v.cap);
    printf("v[0]=%.1f v[99]=%.1f\n", v.data[0], v.data[99]);
    vec_free(&v);

    printf("\n=== 4. flat vs jagged 2D ===\n");
    size_t rows = 3, cols = 4;
    double  *flat   = flat_alloc(rows, cols);
    double **jagged = jagged_alloc(rows, cols);
    if (!flat || !jagged) { free(flat); jagged_free(jagged, rows); return 1; }
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++) {
            FLAT(flat, cols, i, j) = (double)(i * cols + j);
            jagged[i][j]           = (double)(i * cols + j);
        }
    int same = 1;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            if (FLAT(flat, cols, i, j) != jagged[i][j]) same = 0;
    printf("flat is 1 block of %zu bytes; jagged is %zu blocks. Same contents: %s\n",
           rows * cols * sizeof(double), rows + 1, same ? "yes" : "no");
    printf("flat row 1: %.0f %.0f %.0f %.0f\n",
           FLAT(flat, cols, 1, 0), FLAT(flat, cols, 1, 1), FLAT(flat, cols, 1, 2), FLAT(flat, cols, 1, 3));
    /* addresses show the difference: flat rows are exactly cols*8 bytes apart;
     * jagged rows are wherever malloc put them (subtracting pointers from
     * different blocks is UB, so we just print the addresses). */
    printf("flat  row stride: %td bytes\n", (char *)&FLAT(flat, cols, 1, 0) - (char *)&FLAT(flat, cols, 0, 0));
    printf("jagged row0 at %p, row1 at %p (separate blocks, arbitrary spacing)\n",
           (void *)jagged[0], (void *)jagged[1]);
    free(flat);
    jagged_free(jagged, rows);

    printf("\n=== 5. aligned_alloc ===\n");
    double *al = aligned_alloc(64, 64 * sizeof *al);   /* size must be a multiple of alignment */
    if (!al) return 1;
    printf("aligned_alloc(64,...): address %% 64 = %d\n", (int)((uintptr_t)al % 64));
    free(al);                                          /* plain free() */

    printf("\n=== 6. arena allocator ===\n");
    Arena ar;
    if (arena_init(&ar, 1024) != 0) return 1;
    char   *c3 = arena_alloc(&ar, 3, 1);
    double *d1 = arena_alloc(&ar, sizeof *d1, sizeof *d1);
    int    *i1 = arena_alloc(&ar, sizeof *i1, sizeof *i1);
    if (!c3 || !d1 || !i1) return 1;
    printf("char[3] at offset %td\n", (unsigned char *)c3 - ar.base);
    printf("double  at offset %td (padded up to 8)\n", (unsigned char *)d1 - ar.base);
    printf("int     at offset %td\n", (unsigned char *)i1 - ar.base);
    void *big = arena_alloc(&ar, 100000, 16);
    printf("request 100000 bytes -> %s\n", big ? "ok" : "NULL (arena full)");
    arena_reset(&ar);                                  /* everything freed in O(1) */
    char *again = arena_alloc(&ar, 3, 1);
    printf("after reset: offset %td\n", (unsigned char *)again - ar.base);
    arena_free(&ar);                                   /* one free for everything */

    printf("\nAll allocations freed. Run under ASan to confirm.\n");
    return 0;
}
