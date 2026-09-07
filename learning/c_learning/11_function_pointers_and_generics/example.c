/*
 * Chapter 11 — Function Pointers and Generics
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 *
 * Demonstrates, in order:
 *   1. function pointer syntax and typedef
 *   2. passing functions: map, integrate, newton
 *   3. dispatch table of activation functions
 *   4. callbacks with void *ctx (closures, the C way)
 *   5. qsort with a two-key comparator
 *   6. a void*-based generic Vec
 *   7. a macro-generated typed Vec (VEC_DEFINE)
 *   8. C11 _Generic
 *   9. a Layer "vtable" struct, and an autograd Value with a backward pointer
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 1. Function pointer types                                            */
/* ------------------------------------------------------------------ */
typedef double (*UnaryFn)(double);                 /* (double) -> double            */
typedef double (*UnaryCtxFn)(double, void *ctx);   /* same, plus user data          */

static double square(double x) { return x * x; }

/* ------------------------------------------------------------------ */
/* 2. Higher-order numerics                                             */
/* ------------------------------------------------------------------ */
static void map_inplace(double *xs, size_t n, UnaryFn f) {
    for (size_t i = 0; i < n; i++) xs[i] = f(xs[i]);
}

/* Composite midpoint rule. */
static double integrate(UnaryFn f, double a, double b, int n) {
    double h = (b - a) / n, s = 0.0;
    for (int i = 0; i < n; i++) s += f(a + (i + 0.5) * h);
    return s * h;
}

static double newton(UnaryFn f, UnaryFn df, double x0, double tol, int max_iter) {
    double x = x0;
    for (int i = 0; i < max_iter; i++) {
        double fx = f(x);
        if (fabs(fx) < tol) break;
        x -= fx / df(x);
    }
    return x;
}
static double g_root(double x)  { return x * x - 2.0; }   /* root at sqrt(2) */
static double dg_root(double x) { return 2.0 * x; }

/* ------------------------------------------------------------------ */
/* 3. Dispatch table: activations indexed by enum                       */
/* ------------------------------------------------------------------ */
typedef enum { ACT_IDENTITY, ACT_RELU, ACT_SIGMOID, ACT_TANH, ACT_COUNT } Activation;

static double act_identity(double x) { return x; }
static double act_relu(double x)     { return x > 0 ? x : 0; }
static double act_sigmoid(double x)  { return 1.0 / (1.0 + exp(-x)); }

static const UnaryFn act_fns[ACT_COUNT] = {
    [ACT_IDENTITY] = act_identity,
    [ACT_RELU]     = act_relu,
    [ACT_SIGMOID]  = act_sigmoid,
    [ACT_TANH]     = tanh,
};
static const char *const act_names[ACT_COUNT] = { "identity", "relu", "sigmoid", "tanh" };

/* ------------------------------------------------------------------ */
/* 4. Callback with context: integrate a*x + b without a global         */
/* ------------------------------------------------------------------ */
typedef struct { double a, b; } Affine;

static double affine_eval(double x, void *ctx) {
    const Affine *p = ctx;           /* recover the captured state */
    return p->a * x + p->b;
}
static double integrate_ctx(UnaryCtxFn f, void *ctx, double lo, double hi, int n) {
    double h = (hi - lo) / n, s = 0.0;
    for (int i = 0; i < n; i++) s += f(lo + (i + 0.5) * h, ctx);
    return s * h;
}

/* ------------------------------------------------------------------ */
/* 5. qsort comparators                                                 */
/* ------------------------------------------------------------------ */
typedef struct { const char *tok; int count; } TokenFreq;

static int cmp_double_asc(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);        /* never x - y for doubles */
}
static int cmp_tokfreq(const void *a, const void *b) {
    const TokenFreq *p = a, *q = b;
    if (p->count != q->count) return (p->count < q->count) - (p->count > q->count); /* desc */
    return strcmp(p->tok, q->tok);                                                   /* asc  */
}

/* ------------------------------------------------------------------ */
/* 6. void*-based generic vector                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned char *data;
    size_t len, cap, elem_size;
} Vec;

static Vec vec_new(size_t elem_size) { return (Vec){ NULL, 0, 0, elem_size }; }

static int vec_push(Vec *v, const void *elem) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 4;
        void *p = realloc(v->data, ncap * v->elem_size);
        if (!p) return -1;
        v->data = p;
        v->cap = ncap;
    }
    memcpy(v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return 0;
}
static void *vec_at(const Vec *v, size_t i) { return v->data + i * v->elem_size; }
static void  vec_free(Vec *v) { free(v->data); *v = (Vec){ NULL, 0, 0, 0 }; }

/* ------------------------------------------------------------------ */
/* 7. Macro-generated typed vector                                      */
/* ------------------------------------------------------------------ */
#define VEC_DEFINE(T)                                                          \
    typedef struct { T *data; size_t len, cap; } Vec_##T;                     \
    static int vec_##T##_push(Vec_##T *v, T x) {                              \
        if (v->len == v->cap) {                                                \
            size_t ncap = v->cap ? v->cap * 2 : 4;                             \
            T *p = realloc(v->data, ncap * sizeof *p);                         \
            if (!p) return -1;                                                 \
            v->data = p;                                                       \
            v->cap = ncap;                                                     \
        }                                                                      \
        v->data[v->len++] = x;                                                 \
        return 0;                                                              \
    }                                                                          \
    static void vec_##T##_free(Vec_##T *v) { free(v->data); v->data = NULL; v->len = v->cap = 0; }

VEC_DEFINE(double)   /* Vec_double, vec_double_push, vec_double_free */

/* ------------------------------------------------------------------ */
/* 8. _Generic                                                          */
/* ------------------------------------------------------------------ */
#define TYPE_NAME(x) _Generic((x),   \
    int:    "int",                   \
    double: "double",                \
    float:  "float",                 \
    char *: "char*",                 \
    default: "other")

#define ABS(x) _Generic((x), int: abs, double: fabs, float: fabsf)(x)

/* ------------------------------------------------------------------ */
/* 9a. Layer interface (struct of function pointers)                    */
/* ------------------------------------------------------------------ */
typedef struct Layer Layer;
struct Layer {
    const char *name;
    void (*forward)(Layer *self, const double *in, double *out, size_t n);
    void (*backward)(Layer *self, const double *g_out, double *g_in, size_t n);
    void (*free)(Layer *self);
    void *state;
};

/* ReLU layer: state = cached mask of which inputs were positive. */
typedef struct { unsigned char *mask; } ReluState;

static void relu_forward(Layer *self, const double *in, double *out, size_t n) {
    ReluState *s = self->state;
    for (size_t i = 0; i < n; i++) {
        s->mask[i] = in[i] > 0;
        out[i] = s->mask[i] ? in[i] : 0.0;
    }
}
static void relu_backward(Layer *self, const double *g_out, double *g_in, size_t n) {
    ReluState *s = self->state;
    for (size_t i = 0; i < n; i++) g_in[i] = s->mask[i] ? g_out[i] : 0.0;
}
static void relu_free(Layer *self) {
    ReluState *s = self->state;
    free(s->mask);
    free(s);
}
static Layer relu_layer(size_t n) {
    ReluState *s = malloc(sizeof *s);
    s->mask = calloc(n, 1);
    return (Layer){ .name = "relu", .forward = relu_forward, .backward = relu_backward,
                    .free = relu_free, .state = s };
}

/* Scale layer: state = a single double k. */
static void scale_forward(Layer *self, const double *in, double *out, size_t n) {
    double k = *(double *)self->state;
    for (size_t i = 0; i < n; i++) out[i] = k * in[i];
}
static void scale_backward(Layer *self, const double *g_out, double *g_in, size_t n) {
    double k = *(double *)self->state;
    for (size_t i = 0; i < n; i++) g_in[i] = k * g_out[i];
}
static void scale_free(Layer *self) { free(self->state); }
static Layer scale_layer(double k) {
    double *s = malloc(sizeof *s);
    *s = k;
    return (Layer){ .name = "scale", .forward = scale_forward, .backward = scale_backward,
                    .free = scale_free, .state = s };
}

/* ------------------------------------------------------------------ */
/* 9b. Autograd Value with a stored backward function                   */
/* ------------------------------------------------------------------ */
typedef struct Value Value;
typedef void (*BackwardFn)(Value *self);
struct Value {
    double data, grad;
    Value *parents[2];
    BackwardFn backward;   /* NULL for leaf nodes — same role as tensor.grad_fn */
};

static Value *val_leaf(double x) {
    Value *v = calloc(1, sizeof *v);
    v->data = x;
    return v;
}
static void mul_backward(Value *v) {
    v->parents[0]->grad += v->parents[1]->data * v->grad;
    v->parents[1]->grad += v->parents[0]->data * v->grad;
}
static Value *val_mul(Value *a, Value *b) {
    Value *v = calloc(1, sizeof *v);
    v->data = a->data * b->data;
    v->parents[0] = a; v->parents[1] = b;
    v->backward = mul_backward;
    return v;
}
static void add_backward(Value *v) {
    v->parents[0]->grad += v->grad;
    v->parents[1]->grad += v->grad;
}
static Value *val_add(Value *a, Value *b) {
    Value *v = calloc(1, sizeof *v);
    v->data = a->data + b->data;
    v->parents[0] = a; v->parents[1] = b;
    v->backward = add_backward;
    return v;
}

/* ------------------------------------------------------------------ */
int main(void) {
    puts("== 1. function pointer basics ==");
    double (*f)(double) = square;          /* raw syntax */
    UnaryFn h = sqrt;                      /* via typedef */
    printf("square(3) = %.1f, sqrt(16) = %.1f, (*h)(9) = %.1f\n", f(3.0), h(16.0), (*h)(9.0));

    puts("\n== 2. map / integrate / newton ==");
    double xs[] = { 1, 4, 9, 16 };
    map_inplace(xs, 4, sqrt);
    printf("map sqrt: %.0f %.0f %.0f %.0f\n", xs[0], xs[1], xs[2], xs[3]);
    printf("integrate(sin, 0, pi)   = %.6f (exact 2)\n", integrate(sin, 0, M_PI, 1000));
    printf("integrate(square, 0, 3) = %.6f (exact 9)\n", integrate(square, 0, 3, 1000));
    printf("newton(x^2-2, x0=1)     = %.6f (exact %.6f)\n",
           newton(g_root, dg_root, 1.0, 1e-12, 50), sqrt(2.0));

    puts("\n== 3. dispatch table ==");
    for (int a = 0; a < ACT_COUNT; a++)
        printf("%-9s(-1.0) = %9.6f   (0.5) = %9.6f\n",
               act_names[a], act_fns[a](-1.0), act_fns[a](0.5));

    puts("\n== 4. callback with void *ctx ==");
    Affine p1 = { .a = 2, .b = 1 }, p2 = { .a = -1, .b = 4 };
    printf("int_0^1 (2x+1) dx = %.6f (exact 2)\n", integrate_ctx(affine_eval, &p1, 0, 1, 100));
    printf("int_0^2 (4-x) dx  = %.6f (exact 6)\n", integrate_ctx(affine_eval, &p2, 0, 2, 100));

    puts("\n== 5. qsort ==");
    double losses[] = { 0.93, 0.41, 2.10, 0.41, 0.07 };
    qsort(losses, 5, sizeof losses[0], cmp_double_asc);
    printf("sorted losses:");
    for (int i = 0; i < 5; i++) printf(" %.2f", losses[i]);
    putchar('\n');
    TokenFreq tf[] = { { "the", 50 }, { "cat", 7 }, { "a", 50 }, { "sat", 12 } };
    qsort(tf, 4, sizeof tf[0], cmp_tokfreq);
    printf("tokens by count desc, name asc:");
    for (int i = 0; i < 4; i++) printf(" %s:%d", tf[i].tok, tf[i].count);
    putchar('\n');

    puts("\n== 6. void* Vec ==");
    Vec v = vec_new(sizeof(double));
    for (int i = 1; i <= 5; i++) { double x = i * 0.5; vec_push(&v, &x); }
    printf("len=%zu cap=%zu elems:", v.len, v.cap);
    for (size_t i = 0; i < v.len; i++) printf(" %.1f", *(double *)vec_at(&v, i));
    putchar('\n');
    vec_free(&v);

    puts("\n== 7. VEC_DEFINE(double) ==");
    Vec_double vd = { 0 };
    for (int i = 0; i < 6; i++) vec_double_push(&vd, i * i);
    printf("typed vec: len=%zu elems:", vd.len);
    for (size_t i = 0; i < vd.len; i++) printf(" %.0f", vd.data[i]);   /* no cast needed */
    putchar('\n');
    vec_double_free(&vd);

    puts("\n== 8. _Generic ==");
    printf("TYPE_NAME(1)=%s  TYPE_NAME(1.0)=%s  TYPE_NAME(1.0f)=%s  TYPE_NAME(\"s\")=%s\n",
           TYPE_NAME(1), TYPE_NAME(1.0), TYPE_NAME(1.0f), TYPE_NAME("s"));
    printf("ABS(-3)=%d  ABS(-2.5)=%.1f  ABS(-1.5f)=%.1f\n", ABS(-3), ABS(-2.5), (double)ABS(-1.5f));

    puts("\n== 9a. Layer interface ==");
    enum { N = 3 };
    Layer net[2] = { scale_layer(3.0), relu_layer(N) };
    double in[N] = { -1.0, 0.5, 2.0 }, mid[N], out[N];
    net[0].forward(&net[0], in, mid, N);
    net[1].forward(&net[1], mid, out, N);
    printf("forward : [%.1f %.1f %.1f] -> scale3 -> [%.1f %.1f %.1f] -> relu -> [%.1f %.1f %.1f]\n",
           in[0], in[1], in[2], mid[0], mid[1], mid[2], out[0], out[1], out[2]);
    double g_out[N] = { 1, 1, 1 }, g_mid[N], g_in[N];
    net[1].backward(&net[1], g_out, g_mid, N);     /* reverse order */
    net[0].backward(&net[0], g_mid, g_in, N);
    printf("backward: grad_in = [%.1f %.1f %.1f]\n", g_in[0], g_in[1], g_in[2]);
    for (int i = 0; i < 2; i++) net[i].free(&net[i]);

    puts("\n== 9b. autograd Value ==");
    Value *a = val_leaf(2.0), *b = val_leaf(-3.0), *c = val_leaf(5.0);
    Value *ab = val_mul(a, b);        /* -6 */
    Value *L  = val_add(ab, c);       /* -1 */
    L->grad = 1.0;
    Value *topo[] = { L, ab };        /* reverse creation order == reverse topological */
    for (size_t i = 0; i < 2; i++) if (topo[i]->backward) topo[i]->backward(topo[i]);
    printf("L = a*b + c = %.1f   dL/da = %.1f (b)   dL/db = %.1f (a)   dL/dc = %.1f\n",
           L->data, a->grad, b->grad, c->grad);
    free(a); free(b); free(c); free(ab); free(L);
    return 0;
}
