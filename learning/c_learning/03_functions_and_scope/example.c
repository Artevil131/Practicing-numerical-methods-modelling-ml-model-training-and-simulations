/*
 * Chapter 03 example: declarations, by-value parameters, recursion, static,
 * scope/lifetime, the call stack, inline, header-style API, math.h.
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 *   ./ex_demo
 *
 * This is a single file, so the "header" part is written inline between
 * the BEGIN/END mymath.h markers. In a real project those lines would be
 * in mymath.h, the definitions below them in mymath.c, and you would build
 * with:  cc ... -o prog main.c mymath.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>       /* sqrt, exp, fabs, tanh, ... : -lm */

/* ===================== BEGIN "mymath.h" ===================== */
#ifndef MYMATH_H
#define MYMATH_H

/* Prototypes: the contract. Callers need only these lines. */
double vec_dot(const double *a, const double *b, int n);
double vec_norm(const double *a, int n);
void   vec_scale(double *a, int n, double k);

/* Tiny hot helper: static inline in a header is the standard idiom. */
static inline double sq(double x) { return x * x; }

#endif /* MYMATH_H */
/* ====================== END "mymath.h" ====================== */

/* ===================== "mymath.c" definitions ================= */
double vec_dot(const double *a, const double *b, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

double vec_norm(const double *a, int n)
{
    return sqrt(vec_dot(a, a, n));        /* reuse: norm is sqrt of self-dot */
}

void vec_scale(double *a, int n, double k)
{
    for (int i = 0; i < n; i++) a[i] *= k; /* modifies the caller's elements */
}
/* ============================================================== */

/* Prototype for a function defined AFTER main (proves order rule). */
static long factorial(int n);

/* ---- by-value demonstration ---- */
static void try_to_double(int n)
{
    n *= 2;                               /* changes only this copy */
    printf("  inside try_to_double: n=%d\n", n);
}

static void scale_first(double *v, double k)
{
    v[0] *= k;                            /* changes the caller's element */
    v = NULL;                             /* changes only this copy of the pointer */
    (void)v;                              /* silence "set but unused" */
}

/* ---- returning several values: a struct ---- */
struct minmax { double min, max; };

static struct minmax vec_minmax(const double *v, int n)
{
    struct minmax r = { v[0], v[0] };     /* requires n >= 1 */
    for (int i = 1; i < n; i++) {
        if (v[i] < r.min) r.min = v[i];
        if (v[i] > r.max) r.max = v[i];
    }
    return r;                             /* copied out to the caller */
}

/* ---- recursion ---- */
static long g_fib_calls = 0;              /* file-scope: exists whole program; for
                                             instrumentation only (globals are otherwise avoided) */

static long fib_naive(int n)
{
    g_fib_calls++;
    if (n < 2) return n;                  /* base case */
    return fib_naive(n - 1) + fib_naive(n - 2);
}

static long fib_iter(int n)
{
    long a = 0, b = 1;
    for (int i = 0; i < n; i++) { long t = a + b; a = b; b = t; }
    return a;
}

/* Recursion depth probe: each frame holds `pad` (1 KB). */
static int depth_probe(int n, int cap)
{
    volatile char pad[1024];              /* volatile: compiler must keep it */
    pad[0] = (char)n;
    if (n >= cap) return n;
    return depth_probe(n + 1, cap);
}

/* ---- static local: persists across calls ---- */
static int next_id(void)
{
    static int counter = 0;               /* initialized once, at program start */
    return ++counter;
}

/* ---- scope and shadowing ---- */
static void scope_demo(void)
{
    int a = 10;
    {
        int inner = a * 2;                /* visible only in this block */
        printf("  inner block: inner=%d a=%d\n", inner, a);
    }
    /* inner does not exist here; using it would be a compile error. */
    printf("  after block: a=%d\n", a);
}

int main(void)
{
    printf("== declaration order ==\n");
    printf("  factorial(10) = %ld  (defined below main, declared above it)\n", factorial(10));

    printf("\n== pass by value ==\n");
    int n = 21;
    try_to_double(n);
    printf("  after call:            n=%d (unchanged)\n", n);

    double v[3] = {3.0, 4.0, 0.0};
    scale_first(v, 100.0);
    printf("  v[0] after scale_first = %g (array elements ARE modified)\n", v[0]);
    v[0] = 3.0;

    printf("\n== return values ==\n");
    struct minmax mm = vec_minmax(v, 3);
    printf("  min=%g max=%g\n", mm.min, mm.max);

    printf("\n== header-style API (mymath) ==\n");
    printf("  norm(v) = %g\n", vec_norm(v, 3));                  /* 5 */
    vec_scale(v, 3, 0.5);
    printf("  after vec_scale: v = %g %g %g\n", v[0], v[1], v[2]);
    printf("  sq(v[1]) = %g (static inline helper)\n", sq(v[1]));
    double w[3] = {1.0, 0.0, 0.0};
    double cosang = vec_dot(v, w, 3) / (vec_norm(v, 3) * vec_norm(w, 3));
    printf("  cos(angle(v,w)) = %.4f\n", cosang);

    printf("\n== recursion ==\n");
    g_fib_calls = 0;
    long f25 = fib_naive(25);
    printf("  fib_naive(25) = %ld using %ld calls\n", f25, g_fib_calls);
    printf("  fib_iter(25)  = %ld using 25 loop iterations\n", fib_iter(25));
    printf("  fib_iter(90)  = %ld (fits in long; fib(93) would not)\n", fib_iter(90));
    int reached = depth_probe(0, 2000);
    printf("  recursed to depth %d with 1 KB frames (~2 MB of an 8 MB stack)\n", reached);

    printf("\n== static local ==\n");
    int id1 = next_id();
    int id2 = next_id();
    int id3 = next_id();
    printf("  ids: %d %d %d (counter survived between calls)\n", id1, id2, id3);

    printf("\n== scope ==\n");
    scope_demo();

    printf("\n== call stack (conceptual) ==\n");
    printf("  main -> vec_norm -> vec_dot -> sqrt\n");
    printf("  each arrow pushes a frame; each return pops it.\n");
    printf("  Address of a main local:      %p\n", (void *)&n);
    {
        int deeper = 0;
        printf("  Address of an inner local:    %p (nearby: same stack)\n", (void *)&deeper);
    }
    static int s_var;
    printf("  Address of a static variable: %p (different region: data segment)\n",
           (void *)&s_var);

    printf("\n== math.h ==\n");
    printf("  sqrt(2)=%.10f  exp(1)=%.10f  log(10)=%.6f\n", sqrt(2.0), exp(1.0), log(10.0));
    printf("  atan2(1,-1)=%.6f  tanh(0.5)=%.6f  hypot(3,4)=%g\n",
           atan2(1.0, -1.0), tanh(0.5), hypot(3.0, 4.0));
    printf("  fabs(-3.5)=%g  abs(-3)=%d   (abs takes int: abs on a double truncates)\n",
           fabs(-3.5), abs(-3));
    printf("  isnan(0/0)=%d isinf(1/0)=%d  fmax(NAN,2)=%g\n",
           isnan(0.0 / 0.0), isinf(1.0 / 0.0), fmax(NAN, 2.0));
    printf("  1/sqrt(2*pi) = %.8f\n", 1.0 / sqrt(2.0 * M_PI));

    printf("\n== naming ==\n");
    printf("  vec_dot, vec_norm, vec_scale: module_verb, struct/array first, outputs last.\n");
    return 0;
}

/* Definition after use: legal because of the prototype above. */
static long factorial(int n)
{
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
