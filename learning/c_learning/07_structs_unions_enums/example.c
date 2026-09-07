/*
 * Chapter 07 — Structs, Unions, Enums: worked example
 *
 * Compile + run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * Demonstrates:
 *   1. struct definition / typedef / three kinds of initialization / . vs ->
 *   2. nested structs and arrays of structs
 *   3. pass by value vs by pointer (and what the callee can change)
 *   4. padding, alignment, offsetof, field reordering
 *   5. comparing structs with a function
 *   6. union: float <-> bits
 *   7. enum + switch (every case listed, no default)
 *   8. tagged union Value type
 *   9. opaque type (simulated within one file)
 *  10. Matrix struct: zeros/free/copy/matmul; the shallow-copy trap
 *  11. Layer sketch
 *  12. flexible array member
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>     /* offsetof */
#include <stdint.h>
#include <math.h>

/* ------------------------------------------------------------------ 1, 2 */
typedef struct { double x, y; } Vec2;              /* small POD: pass by value is fine */

typedef struct {
    Vec2   pos;      /* nested struct, embedded inline */
    Vec2   vel;
    double mass;
    int    id;
} Particle;

static Vec2   vec2_add(Vec2 a, Vec2 b)   { return (Vec2){a.x + b.x, a.y + b.y}; }
static double vec2_norm(Vec2 v)          { return sqrt(v.x * v.x + v.y * v.y); }
static int    vec2_eq(Vec2 a, Vec2 b)    { return a.x == b.x && a.y == b.y; }     /* no == for structs */

/* ------------------------------------------------------------------ 3 */
static void scale_by_value(Vec2 v, double k)    { v.x *= k; v.y *= k; (void)v; }  /* modifies a copy */
static void scale_by_pointer(Vec2 *v, double k) { v->x *= k; v->y *= k; }         /* modifies caller's */

/* ------------------------------------------------------------------ 4 */
struct Bad  { char c; double d; int i; };   /* 24 bytes */
struct Good { double d; int i; char c; };   /* 16 bytes */

/* ------------------------------------------------------------------ 6 */
typedef union { float f; uint32_t u; } FloatBits;   /* same 4 bytes, two views */

/* ------------------------------------------------------------------ 7 */
typedef enum { ACT_RELU, ACT_TANH, ACT_SIGMOID, ACT_COUNT } Activation;
static const char *act_names[ACT_COUNT] = { "relu", "tanh", "sigmoid" };

static double activate(Activation a, double x) {
    switch (a) {                               /* every case listed: -Wall warns if one is missing */
        case ACT_RELU:    return x > 0 ? x : 0;
        case ACT_TANH:    return tanh(x);
        case ACT_SIGMOID: return 1.0 / (1.0 + exp(-x));
        case ACT_COUNT:   break;
    }
    return NAN;
}

/* ------------------------------------------------------------------ 8 */
typedef enum { VAL_NUM, VAL_STR, VAL_VEC } ValueKind;
typedef struct {
    ValueKind kind;                 /* the tag: which union member is live */
    union {
        double num;
        char  *str;                                 /* owned heap string */
        struct { double *data; size_t n; } vec;     /* owned heap array */
    } as;
} Value;

static Value value_num(double x)             { return (Value){.kind = VAL_NUM, .as.num = x}; }
static Value value_str(const char *s) {      /* copies s; the Value owns the copy */
    Value v = {.kind = VAL_STR};
    size_t n = strlen(s) + 1;
    v.as.str = malloc(n);
    if (v.as.str) memcpy(v.as.str, s, n);
    return v;
}
static Value value_vec(const double *src, size_t n) {   /* copies src; Value owns the copy */
    Value v = {.kind = VAL_VEC, .as.vec = {malloc(n * sizeof(double)), n}};
    if (v.as.vec.data) memcpy(v.as.vec.data, src, n * sizeof(double));
    return v;
}
static void value_print(const Value *v) {
    switch (v->kind) {
        case VAL_NUM: printf("%g", v->as.num); break;
        case VAL_STR: printf("\"%s\"", v->as.str); break;
        case VAL_VEC:
            printf("[");
            for (size_t i = 0; i < v->as.vec.n; i++) printf("%s%g", i ? ", " : "", v->as.vec.data[i]);
            printf("]");
            break;
    }
}
static void value_free(Value *v) {             /* the tag tells us what (if anything) to free */
    switch (v->kind) {
        case VAL_NUM: break;
        case VAL_STR: free(v->as.str); v->as.str = NULL; break;
        case VAL_VEC: free(v->as.vec.data); v->as.vec.data = NULL; v->as.vec.n = 0; break;
    }
}

/* ------------------------------------------------------------------ 9 */
/* Opaque type. In a real project the typedef line lives in rng.h and the
 * struct body + functions live in rng.c; callers can only hold Rng *. */
typedef struct Rng Rng;                          /* incomplete type */
struct Rng { uint64_t state; };                  /* full definition (would be private to rng.c) */
static Rng *rng_new(uint64_t seed) {             /* returns ownership */
    Rng *r = malloc(sizeof *r);
    if (r) r->state = seed ? seed : 0x9E3779B97F4A7C15ull;
    return r;
}
static double rng_uniform(Rng *r) {              /* xorshift64*: good enough for demos */
    uint64_t x = r->state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    r->state = x;
    return (double)((x * 0x2545F4914F6CDD1Dull) >> 11) / 9007199254740992.0;
}
static void rng_free(Rng *r) { free(r); }

/* ------------------------------------------------------------------ 10 */
typedef struct {
    size_t  rows, cols;
    double *data;        /* owned; row-major: data[i*cols + j] */
} Matrix;

/* Returns a zeroed matrix. Caller owns m.data; release with mat_free.
 * On allocation failure data == NULL and rows == cols == 0. */
static Matrix mat_zeros(size_t rows, size_t cols) {
    Matrix m = {rows, cols, calloc(rows * cols, sizeof *m.data)};
    if (!m.data) m.rows = m.cols = 0;
    return m;
}
/* Frees m->data; resets m to empty. Safe to call twice. */
static void mat_free(Matrix *m) { free(m->data); m->data = NULL; m->rows = m->cols = 0; }

static inline double mat_get(const Matrix *m, size_t i, size_t j)       { return m->data[i * m->cols + j]; }
static inline void   mat_set(Matrix *m, size_t i, size_t j, double v)   { m->data[i * m->cols + j] = v; }

/* Returns a deep copy. Caller owns the result. */
static Matrix mat_copy(const Matrix *src) {
    Matrix m = mat_zeros(src->rows, src->cols);
    if (m.data) memcpy(m.data, src->data, src->rows * src->cols * sizeof *m.data);
    return m;
}
/* out += a @ b. out must be pre-allocated (a->rows x b->cols). Returns -1 on shape mismatch. */
static int mat_matmul(Matrix *out, const Matrix *a, const Matrix *b) {
    if (a->cols != b->rows || out->rows != a->rows || out->cols != b->cols) return -1;
    for (size_t i = 0; i < a->rows; i++)
        for (size_t k = 0; k < a->cols; k++) {
            double aik = mat_get(a, i, k);
            for (size_t j = 0; j < b->cols; j++)              /* inner loop is contiguous */
                out->data[i * out->cols + j] += aik * mat_get(b, k, j);
        }
    return 0;
}
static void mat_print(const char *name, const Matrix *m) {
    printf("%s (%zux%zu):\n", name, m->rows, m->cols);
    for (size_t i = 0; i < m->rows; i++) {
        printf("  ");
        for (size_t j = 0; j < m->cols; j++) printf("%7.2f", mat_get(m, i, j));
        printf("\n");
    }
}

/* ------------------------------------------------------------------ 11 */
typedef struct {
    Matrix     W;         /* out x in; owned */
    Matrix     b;         /* out x 1;  owned */
    Activation act;
} Layer;

/* Returns 0 on success; on failure frees partial allocations and returns -1. */
static int layer_init(Layer *l, size_t in_f, size_t out_f, Activation act) {
    l->W = mat_zeros(out_f, in_f);
    l->b = mat_zeros(out_f, 1);
    l->act = act;
    if (!l->W.data || !l->b.data) { mat_free(&l->W); mat_free(&l->b); return -1; }
    return 0;
}
static void layer_free(Layer *l) { mat_free(&l->W); mat_free(&l->b); }

/* out = act(W @ x + b). out must be out_f x 1. */
static int layer_forward(const Layer *l, Matrix *out, const Matrix *x) {
    memset(out->data, 0, out->rows * out->cols * sizeof *out->data);
    if (mat_matmul(out, &l->W, x) != 0) return -1;
    for (size_t i = 0; i < out->rows; i++)
        out->data[i] = activate(l->act, out->data[i] + l->b.data[i]);
    return 0;
}

/* ------------------------------------------------------------------ 12 */
typedef struct {
    size_t len;
    double data[];        /* flexible array member: header + payload in one block */
} FlexVec;
static FlexVec *flexvec_new(size_t n) {          /* returns ownership; free() releases all */
    FlexVec *v = malloc(sizeof *v + n * sizeof v->data[0]);
    if (v) { v->len = n; for (size_t i = 0; i < n; i++) v->data[i] = (double)i; }
    return v;
}

/* ================================================================== main */
int main(void) {
    printf("=== 1. definition, initialization, access ===\n");
    Vec2 a = {3.0, 4.0};               /* positional */
    Vec2 b = {.y = 1.0};               /* designated: x defaults to 0.0 */
    Vec2 z = {0};                      /* all zero */
    Vec2 *pa = &a;
    printf("a=(%g,%g) b=(%g,%g) z=(%g,%g)\n", a.x, a.y, b.x, b.y, z.x, z.y);
    printf("pa->x = %g   (*pa).y = %g   |a| = %g\n", pa->x, (*pa).y, vec2_norm(a));
    Vec2 s = vec2_add(a, b);           /* struct returned by value */
    printf("a+b = (%g,%g)\n", s.x, s.y);

    printf("\n=== 2. nested structs, array of structs ===\n");
    Particle ps[2] = {
        {.pos = {0, 0}, .vel = {1, 0.5}, .mass = 1.0, .id = 0},
        {.pos = {2, 2}, .vel = {-1, 0}, .mass = 2.0, .id = 1},
    };
    for (int i = 0; i < 2; i++) {
        ps[i].pos = vec2_add(ps[i].pos, ps[i].vel);       /* one Euler step, dt = 1 */
        printf("particle %d at (%g,%g)\n", ps[i].id, ps[i].pos.x, ps[i].pos.y);
    }
    printf("sizeof(Particle) = %zu bytes (2*16 + 8 + 4 + 4 padding)\n", sizeof(Particle));

    printf("\n=== 3. by value vs by pointer ===\n");
    Vec2 v = {1, 1};
    scale_by_value(v, 10);   printf("after scale_by_value:   (%g,%g)  <- unchanged\n", v.x, v.y);
    scale_by_pointer(&v, 10); printf("after scale_by_pointer: (%g,%g)  <- changed\n", v.x, v.y);

    printf("\n=== 4. padding and offsetof ===\n");
    printf("struct Bad  {char; double; int}: sizeof=%zu  offsets c=%zu d=%zu i=%zu\n",
           sizeof(struct Bad), offsetof(struct Bad, c), offsetof(struct Bad, d), offsetof(struct Bad, i));
    printf("struct Good {double; int; char}: sizeof=%zu  offsets d=%zu i=%zu c=%zu\n",
           sizeof(struct Good), offsetof(struct Good, d), offsetof(struct Good, i), offsetof(struct Good, c));
    printf("_Alignof(double)=%zu _Alignof(struct Bad)=%zu\n", _Alignof(double), _Alignof(struct Bad));

    printf("\n=== 5. comparing structs ===\n");
    Vec2 p1 = {1, 2}, p2 = {1, 2}, p3 = {2, 1};
    printf("p1==p2: %d   p1==p3: %d   (via vec2_eq; there is no == for structs)\n",
           vec2_eq(p1, p2), vec2_eq(p1, p3));

    printf("\n=== 6. union: float bits ===\n");
    FloatBits fb = {.f = 1.0f};
    printf("1.0f      = 0x%08x  (sign=%u exp=%u mantissa=0x%06x)\n",
           fb.u, fb.u >> 31, (fb.u >> 23) & 0xffu, fb.u & 0x7fffffu);
    fb.u ^= 0x80000000u;                       /* flip the sign bit */
    printf("flip sign = %f\n", fb.f);
    fb.f = 0.1f;
    printf("0.1f      = 0x%08x  (not exactly representable)\n", fb.u);
    printf("sizeof(FloatBits) = %zu (size of largest member)\n", sizeof(FloatBits));

    printf("\n=== 7. enum + switch ===\n");
    for (int k = 0; k < ACT_COUNT; k++)
        printf("%-8s(0.5) = %.4f   (enum value %d)\n", act_names[k], activate((Activation)k, 0.5), k);

    printf("\n=== 8. tagged union Value ===\n");
    double xs[3] = {1.5, 2.5, 3.5};
    Value vals[3] = { value_num(42), value_str("hello"), value_vec(xs, 3) };
    for (int i = 0; i < 3; i++) { printf("vals[%d] = ", i); value_print(&vals[i]); printf("\n"); }
    printf("sizeof(Value) = %zu (tag + largest payload + padding)\n", sizeof(Value));
    for (int i = 0; i < 3; i++) value_free(&vals[i]);

    printf("\n=== 9. opaque type ===\n");
    Rng *rng = rng_new(12345);
    if (!rng) return 1;
    printf("three uniforms: %.4f %.4f %.4f\n", rng_uniform(rng), rng_uniform(rng), rng_uniform(rng));
    rng_free(rng);
    /* Rng r; and rng->state would be compile errors if the struct body were hidden in rng.c */

    printf("\n=== 10. Matrix: zeros/copy/matmul and the shallow-copy trap ===\n");
    Matrix A = mat_zeros(2, 3), B = mat_zeros(3, 2), C = mat_zeros(2, 2);
    if (!A.data || !B.data || !C.data) return 1;
    for (size_t i = 0; i < 2; i++) for (size_t j = 0; j < 3; j++) mat_set(&A, i, j, (double)(i * 3 + j + 1));
    for (size_t i = 0; i < 3; i++) for (size_t j = 0; j < 2; j++) mat_set(&B, i, j, (double)(i == j));  /* "identity-ish" */
    mat_matmul(&C, &A, &B);
    mat_print("A", &A); mat_print("B", &B); mat_print("C = A@B", &C);

    Matrix shallow = A;                        /* copies rows, cols, and the POINTER only */
    shallow.data[0] = 99.0;
    printf("shallow copy: shallow.data[0]=99 -> A.data[0]=%g  (same heap block!)\n", A.data[0]);
    Matrix deep = mat_copy(&A);                /* new heap block */
    deep.data[0] = -1.0;
    printf("deep copy:    deep.data[0]=-1  -> A.data[0]=%g  (independent)\n", A.data[0]);
    /* Freeing both A and shallow would be a double free. Only A owns the block: */
    shallow.data = NULL;                       /* shallow no longer claims ownership */
    mat_free(&A); mat_free(&B); mat_free(&C); mat_free(&deep);
    mat_free(&shallow);                        /* free(NULL): harmless */

    printf("\n=== 11. Layer sketch ===\n");
    Layer l;
    if (layer_init(&l, 3, 2, ACT_RELU) != 0) return 1;
    /* W = [[1,0,-1],[0.5,0.5,0.5]], b = [0, -1] */
    double w[6] = {1, 0, -1, 0.5, 0.5, 0.5};
    memcpy(l.W.data, w, sizeof w);
    l.b.data[1] = -1.0;
    Matrix x = mat_zeros(3, 1), y = mat_zeros(2, 1);
    if (!x.data || !y.data) return 1;
    x.data[0] = 2; x.data[1] = 1; x.data[2] = 0.5;
    layer_forward(&l, &y, &x);
    printf("relu(W@x + b) = [%g, %g]   (1.5 and relu(1.75-1)=0.75)\n", y.data[0], y.data[1]);
    mat_free(&x); mat_free(&y); layer_free(&l);

    printf("\n=== 12. flexible array member ===\n");
    FlexVec *fv = flexvec_new(4);
    if (!fv) return 1;
    printf("sizeof(FlexVec)=%zu (header only); block holds len=%zu doubles: %g %g %g %g\n",
           sizeof(FlexVec), fv->len, fv->data[0], fv->data[1], fv->data[2], fv->data[3]);
    free(fv);                                  /* one free for header + payload */

    return 0;
}
