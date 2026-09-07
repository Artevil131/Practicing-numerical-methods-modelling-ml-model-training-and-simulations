/*
 * 00_c_basics.c — everything you need to write the tokenizer and a matrix library.
 *
 * Compile:  cc -Wall -Wextra -O2 -o basics 00_c_basics.c -lm
 * Run:      ./basics
 *
 * Read top to bottom. Change things. Recompile. Break it on purpose.
 * Comments compare to Python/NumPy where it helps.
 */

#include <stdio.h>    /* printf, fopen                    (like: print)                 */
#include <stdlib.h>   /* malloc, calloc, realloc, free    (memory — NumPy does this)    */
#include <string.h>   /* strlen, strcmp, memcpy           (str methods)                 */
#include <math.h>     /* exp, sqrt, tanh                  (np.exp ...) — needs -lm      */

/* ---------------------------------------------------------------------------
 * 1. Functions must be declared before use. Types are fixed at compile time.
 *    Python:  def sigmoid(x): return 1/(1+math.exp(-x))
 * ------------------------------------------------------------------------- */
double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

/* ---------------------------------------------------------------------------
 * 2. Arrays don't know their own length. You pass the length alongside.
 *    Python:  sum(a)
 *    `const` = promise not to modify. `double *a` = pointer to first element.
 * ------------------------------------------------------------------------- */
double sum(const double *a, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++) {
        s += a[i];              /* a[i] is shorthand for *(a + i) */
    }
    return s;
}

/* ---------------------------------------------------------------------------
 * 3. Functions receive COPIES of arguments. To modify the caller's variable
 *    you pass its ADDRESS (a pointer) and write through it with `*`.
 *    Python has no equivalent — mutation of ints is impossible there.
 * ------------------------------------------------------------------------- */
void swap(int *x, int *y) {
    int tmp = *x;               /* *x  = "the value x points at" */
    *x = *y;
    *y = tmp;
}

/* ---------------------------------------------------------------------------
 * 4. struct = a bundle of fields. This is your numpy.ndarray.
 *    2D data is stored FLAT, row-major: element (i, j) is data[i*cols + j].
 *    That is exactly what NumPy does under the hood too.
 * ------------------------------------------------------------------------- */
typedef struct {
    int rows, cols;
    double *data;               /* pointer to rows*cols doubles on the heap */
} Matrix;

Matrix mat_alloc(int rows, int cols) {
    Matrix m;
    m.rows = rows;
    m.cols = cols;
    m.data = calloc((size_t)rows * cols, sizeof(double));   /* np.zeros */
    if (!m.data) { fprintf(stderr, "out of memory\n"); exit(1); }
    return m;
}

void mat_free(Matrix *m) {      /* pointer so we can null out the field */
    free(m->data);              /* m->data  ==  (*m).data */
    m->data = NULL;
}

/* Macro: MAT_AT(m, i, j) expands textually to the indexing expression. */
#define MAT_AT(m, i, j) ((m).data[(i) * (m).cols + (j)])

void mat_print(const Matrix *m) {
    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            printf("%8.3f ", MAT_AT(*m, i, j));   /* %8.3f: width 8, 3 decimals */
        }
        printf("\n");
    }
}

/* C = A @ B.  Triple loop. This is the whole secret of NumPy's matmul
 * (minus 30 years of cache and SIMD tricks you'll learn later). */
Matrix mat_mul(const Matrix *a, const Matrix *b) {
    if (a->cols != b->rows) { fprintf(stderr, "shape mismatch\n"); exit(1); }
    Matrix c = mat_alloc(a->rows, b->cols);
    for (int i = 0; i < a->rows; i++)
        for (int j = 0; j < b->cols; j++) {
            double s = 0.0;
            for (int k = 0; k < a->cols; k++)
                s += MAT_AT(*a, i, k) * MAT_AT(*b, k, j);
            MAT_AT(c, i, j) = s;
        }
    return c;
}

/* ---------------------------------------------------------------------------
 * 5. Growable array = Python list. Track length and capacity, realloc when full.
 *    You will need exactly this for the BPE tokenizer.
 * ------------------------------------------------------------------------- */
typedef struct {
    int *items;
    int len, cap;
} IntVec;

void vec_push(IntVec *v, int x) {
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;               /* ternary: cond ? a : b */
        v->items = realloc(v->items, (size_t)v->cap * sizeof(int));
    }
    v->items[v->len++] = x;                              /* use len, then increment */
}

/* ---------------------------------------------------------------------------
 * 6. main() — program entry point. Returns 0 on success.
 * ------------------------------------------------------------------------- */
int main(void) {
    /* --- scalars & printf ------------------------------------------------ */
    int    n = 5;               /* 32-bit integer                          */
    double x = 2.5;             /* 64-bit float  (np.float64)              */
    char   c = 'A';             /* 1 byte; 'A' is just the number 65       */
    printf("n=%d x=%f c=%c (as int: %d)\n", n, x, c, c);

    /* --- stack array (fixed size, freed automatically) ------------------- */
    double a[5] = {1, 2, 3, 4, 5};
    printf("sum = %f\n", sum(a, n));
    printf("sizeof(a) = %zu bytes, elements = %zu\n", sizeof(a), sizeof(a) / sizeof(a[0]));

    /* --- pointers -------------------------------------------------------- */
    int p = 1, q = 2;
    swap(&p, &q);               /* &p = "address of p" */
    printf("after swap: p=%d q=%d\n", p, q);

    double *ptr = a;            /* an array name decays to a pointer to its first element */
    printf("a[2] = %f, *(ptr+2) = %f, ptr[2] = %f  (all the same)\n", a[2], *(ptr + 2), ptr[2]);

    /* --- strings: char arrays terminated by '\0' ------------------------- */
    const char *text = "hello world";
    int len = (int)strlen(text);            /* counts until '\0' */
    printf("len=%d first='%c' as bytes:", len, text[0]);
    for (int i = 0; i < len; i++) printf(" %d", text[i]);   /* ← this IS a char tokenizer */
    printf("\n");

    /* --- heap array (you choose the size at runtime, you must free it) --- */
    int m = 10;
    double *sq = malloc((size_t)m * sizeof(double));
    for (int i = 0; i < m; i++) sq[i] = sigmoid(i - 5.0);
    printf("sigmoid(-5..4):");
    for (int i = 0; i < m; i++) printf(" %.3f", sq[i]);
    printf("\n");
    free(sq);                   /* forget this → memory leak. Sanitizer will catch it. */

    /* --- Matrix ---------------------------------------------------------- */
    Matrix A = mat_alloc(2, 3);
    Matrix B = mat_alloc(3, 2);
    for (int i = 0; i < 6; i++) { A.data[i] = i + 1; B.data[i] = 6 - i; }
    Matrix C = mat_mul(&A, &B);
    printf("A =\n"); mat_print(&A);
    printf("B =\n"); mat_print(&B);
    printf("A @ B =\n"); mat_print(&C);
    mat_free(&A); mat_free(&B); mat_free(&C);

    /* --- growable vector ------------------------------------------------- */
    IntVec v = {0};             /* all fields zero: items=NULL, len=0, cap=0 */
    for (int i = 0; i < 20; i++) vec_push(&v, i * i);
    printf("vec len=%d cap=%d last=%d\n", v.len, v.cap, v.items[v.len - 1]);
    free(v.items);

    /* --- control flow you haven't seen yet -------------------------------- */
    int i = 0;
    while (i < 3) { i++; }                  /* same as Python while */
    do { i--; } while (i > 0);              /* runs body at least once */
    switch (c) {                            /* multiway branch on an int/char */
        case 'A': printf("c is A\n"); break;
        case 'B': printf("c is B\n"); break;
        default:  printf("c is something else\n");
    }

    return 0;
}

/*
 * EXERCISES (do them in this file, recompile each time):
 *  1. Write mat_transpose(const Matrix *m) and print A^T.
 *  2. Make mat_mul crash with a shape mismatch. Read the error. Fix it.
 *  3. Remove a free() and build with:
 *       cc -g -fsanitize=address -o basics 00_c_basics.c -lm && ./basics
 *     Read the leak report. Put the free() back.
 *  4. Write `int count_char(const char *s, char target)`.
 *  5. Access a[10]. Run it. Then run it with the sanitizer. Understand why C
 *     didn't stop you the first time — this is the whole difference from Python.
 */
