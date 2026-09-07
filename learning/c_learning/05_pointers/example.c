/*
 * Chapter 05 example: pointers, & and *, arithmetic, arrays <-> pointers,
 * out-parameters, const variants, struct pointers, int**, argv, void*,
 * function pointers, *p++ vs (*p)++.
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 *   ./ex_demo 1000 0.01
 *   ./ex_demo                (uses defaults; shows the argc check)
 *
 * Strongly recommended second build while learning:
 *   cc -Wall -Wextra -std=c11 -O1 -g -fsanitize=address,undefined -o ex_demo example.c -lm
 */

#include <stdio.h>
#include <stdlib.h>   /* malloc, free, strtol, strtod */
#include <string.h>   /* strcmp, memcpy */
#include <stddef.h>   /* NULL, ptrdiff_t */
#include <math.h>     /* sin, M_PI */

/* ---- 7. out-parameters ---- */
static void swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/* status via return, results via pointers; read-only input is const */
static int vec_minmax(const double *v, size_t n, double *min_out, double *max_out)
{
    if (n == 0) return -1;
    double mn = v[0], mx = v[0];
    for (size_t i = 1; i < n; i++) {
        if (v[i] < mn) mn = v[i];
        if (v[i] > mx) mx = v[i];
    }
    *min_out = mn;
    *max_out = mx;
    return 0;
}

/* ---- 8. returning pointers ---- */
/* OK: points into the caller's data, which outlives the call */
static const char *find_char(const char *s, char c)
{
    for (; *s; s++) if (*s == c) return s;
    return NULL;
}

/* OK: the caller owns the buffer. (Returning &local would be a dangling pointer.) */
static char *make_label(char *buf, size_t size, int n)
{
    snprintf(buf, size, "item_%d", n);
    return buf;
}

/* ---- 10. struct pointers ---- */
struct vec3 { double x, y, z; };

static void vec3_scale(struct vec3 *v, double k)             /* mutates */
{
    v->x *= k; v->y *= k; v->z *= k;                         /* v->x is (*v).x */
}

static double vec3_dot(const struct vec3 *a, const struct vec3 *b)   /* read-only */
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/* ---- 11. pointer to pointer: change the caller's pointer ---- */
static int alloc_ramp(double **out, size_t n)
{
    double *p = malloc(n * sizeof *p);                       /* void* -> double* implicitly */
    if (p == NULL) return -1;
    for (size_t i = 0; i < n; i++) p[i] = (double)i;
    *out = p;                                                /* write into the caller's variable */
    return 0;
}

/* ---- 13. void*: generic byte access ---- */
static void dump_bytes(const void *obj, size_t n)
{
    const unsigned char *b = obj;                            /* void* -> unsigned char*, no cast needed */
    for (size_t i = 0; i < n; i++) printf("%02x ", b[i]);
    printf("\n");
}

/* ---- 14. function pointers ---- */
static double square(double x) { return x * x; }

static double trapezoid(double (*f)(double), double a, double b, int n)
{
    double h = (b - a) / n;
    double s = 0.5 * (f(a) + f(b));
    for (int i = 1; i < n; i++) s += f(a + i * h);
    return s * h;
}

/* ---- 15. restrict: promise of no aliasing on a hot kernel ---- */
static void vec_add(size_t n, double *restrict out,
                    const double *restrict a, const double *restrict b)
{
    for (size_t i = 0; i < n; i++) out[i] = a[i] + b[i];
}

int main(int argc, char *argv[])
{
    /* ---------------------------------------------------------------- */
    /* 1-2. What a pointer is; & and *                                  */
    /* ---------------------------------------------------------------- */
    printf("== address and dereference ==\n");
    int x = 42;
    int *p = &x;                          /* p holds the address of x */
    int **pp = &p;                        /* pp holds the address of p */
    printf("  x=%d  &x=%p  p=%p  *p=%d  sizeof p=%zu\n", x, (void *)&x, (void *)p, *p, sizeof p);
    printf("  pp=%p  *pp=%p  **pp=%d\n", (void *)pp, (void *)*pp, **pp);
    *p = 7;                               /* writes through p: x changes */
    **pp = **pp + 1;                      /* two hops: x changes again */
    printf("  after *p=7; **pp+=1:  x=%d\n", x);

    /* ---------------------------------------------------------------- */
    /* 3. Pointer type controls the step size                           */
    /* ---------------------------------------------------------------- */
    printf("\n== type and arithmetic scale ==\n");
    double d[4] = {1.5, 2.5, 3.5, 4.5};
    double *pd = d;
    char *pc = (char *)d;                 /* char* may alias anything */
    printf("  double*: p+1 moves %td bytes;  char*: p+1 moves %td byte\n",
           (char *)(pd + 1) - (char *)pd, (pc + 1) - pc);
    printf("  *(pd+2)=%g  pd[2]=%g  d[2]=%g  (all the same: a[i] is *(a+i))\n",
           *(pd + 2), pd[2], d[2]);

    /* ---------------------------------------------------------------- */
    /* 4. NULL                                                          */
    /* ---------------------------------------------------------------- */
    printf("\n== NULL ==\n");
    int *nothing = NULL;
    printf("  nothing == NULL: %d;  !nothing: %d  (never dereference it)\n",
           nothing == NULL, !nothing);
    const char *hit = find_char("hello world", ' ');
    const char *miss = find_char("hello", 'z');
    printf("  find_char(' ') -> \"%s\";  find_char('z') -> %s\n",
           hit ? hit + 1 : "(null)", miss ? miss : "NULL");

    /* ---------------------------------------------------------------- */
    /* 5. Pointer arithmetic and comparison                             */
    /* ---------------------------------------------------------------- */
    printf("\n== arithmetic ==\n");
    int a[5] = {10, 20, 30, 40, 50};
    int *q = &a[3];
    printf("  q - a = %td elements;  a < q: %d\n", q - a, a < q);
    int sum = 0;
    for (int *it = a; it != a + 5; it++) sum += *it;   /* a+5: one past the end, compare only */
    printf("  pointer-walk sum = %d\n", sum);

    /* ---------------------------------------------------------------- */
    /* 6. Arrays vs pointers                                            */
    /* ---------------------------------------------------------------- */
    printf("\n== array vs pointer ==\n");
    printf("  sizeof d = %zu (array), sizeof pd = %zu (pointer)\n", sizeof d, sizeof pd);
    printf("  d == &d[0] == pd: %d %d\n", d == &d[0], d == pd);
    pd = pd + 1;                          /* legal: pd is a variable */
    /* d = d + 1;                            ERROR: an array is not assignable */
    printf("  after pd++: *pd = %g\n", *pd);

    /* ---------------------------------------------------------------- */
    /* 7. Out-parameters                                                */
    /* ---------------------------------------------------------------- */
    printf("\n== out-parameters ==\n");
    int u = 1, v = 2;
    swap(&u, &v);
    printf("  swap -> u=%d v=%d\n", u, v);
    double lo, hi;
    if (vec_minmax(d, 4, &lo, &hi) == 0) printf("  minmax -> %g %g\n", lo, hi);
    printf("  minmax on empty -> status %d\n", vec_minmax(d, 0, &lo, &hi));

    /* ---------------------------------------------------------------- */
    /* 8. Returning pointers safely                                     */
    /* ---------------------------------------------------------------- */
    printf("\n== returning pointers ==\n");
    char label[32];
    printf("  %s  (caller-owned buffer)\n", make_label(label, sizeof label, 7));

    /* ---------------------------------------------------------------- */
    /* 9. const variants, read right to left                            */
    /* ---------------------------------------------------------------- */
    printf("\n== const ==\n");
    int m = 1, n2 = 2;
    const int *ptr_to_const = &m;         /* cannot write *ptr_to_const; can repoint */
    ptr_to_const = &n2;
    int *const const_ptr = &m;            /* can write *const_ptr; cannot repoint */
    *const_ptr = 10;
    const int *const both = &n2;          /* neither */
    printf("  *ptr_to_const=%d  *const_ptr=%d (m=%d)  *both=%d\n",
           *ptr_to_const, *const_ptr, m, *both);
    /* *ptr_to_const = 5;   ERROR: read-only variable
       const_ptr = &n2;     ERROR: cannot assign to const pointer */

    /* ---------------------------------------------------------------- */
    /* 10. Struct pointers and ->                                       */
    /* ---------------------------------------------------------------- */
    printf("\n== struct pointer ==\n");
    struct vec3 w = {1.0, 2.0, 3.0};
    struct vec3 *pw = &w;
    vec3_scale(pw, 2.0);
    printf("  w = (%g, %g, %g)  dot(w,w) = %g\n", w.x, pw->y, (*pw).z, vec3_dot(&w, &w));

    /* ---------------------------------------------------------------- */
    /* 11. int**: modify caller's pointer; arrays of strings            */
    /* ---------------------------------------------------------------- */
    printf("\n== pointer to pointer ==\n");
    double *ramp = NULL;
    if (alloc_ramp(&ramp, 4) == 0) {
        printf("  ramp[3] = %g (allocated inside the callee)\n", ramp[3]);
        free(ramp);
        ramp = NULL;
    }
    const char *vocab[] = {"the", "cat", "sat"};   /* array of 3 char pointers */
    const char **pv = vocab;                        /* decays to const char** */
    printf("  vocab[1]=%s  pv[2]=%s  pv[1][0]=%c\n", vocab[1], pv[2], pv[1][0]);

    /* ---------------------------------------------------------------- */
    /* 12. argc / argv                                                  */
    /* ---------------------------------------------------------------- */
    printf("\n== argv ==\n");
    printf("  argc=%d\n", argc);
    for (int i = 0; i < argc; i++) printf("  argv[%d]=\"%s\"\n", i, argv[i]);
    long steps = 100;
    double lr = 0.1;
    if (argc >= 3) {
        char *end;
        long s = strtol(argv[1], &end, 10);
        if (*end == '\0' && s > 0) steps = s;
        else fprintf(stderr, "  bad steps \"%s\", keeping default\n", argv[1]);
        double l = strtod(argv[2], &end);
        if (*end == '\0') lr = l;
        else fprintf(stderr, "  bad lr \"%s\", keeping default\n", argv[2]);
    } else {
        printf("  (usage: %s STEPS LR; using defaults)\n", argv[0]);
    }
    printf("  steps=%ld lr=%g\n", steps, lr);

    /* ---------------------------------------------------------------- */
    /* 13. void*                                                        */
    /* ---------------------------------------------------------------- */
    printf("\n== void* ==\n");
    float f = 1.0f;
    printf("  bytes of 1.0f: ");
    dump_bytes(&f, sizeof f);                       /* float* -> const void* implicitly */
    unsigned bits;
    memcpy(&bits, &f, sizeof bits);                 /* reinterpret safely (no aliasing UB) */
    printf("  as uint32: 0x%08x\n", bits);

    /* ---------------------------------------------------------------- */
    /* 14. Function pointers                                            */
    /* ---------------------------------------------------------------- */
    printf("\n== function pointers ==\n");
    double (*fp)(double) = square;
    printf("  fp(3) = %g\n", fp(3.0));
    printf("  integral x^2 on [0,1] = %.6f;  integral sin on [0,pi] = %.6f\n",
           trapezoid(square, 0.0, 1.0, 1000), trapezoid(sin, 0.0, M_PI, 1000));

    /* ---------------------------------------------------------------- */
    /* 15. restrict                                                     */
    /* ---------------------------------------------------------------- */
    printf("\n== restrict ==\n");
    double va[3] = {1, 2, 3}, vb[3] = {10, 20, 30}, vout[3];
    vec_add(3, vout, va, vb);                       /* distinct arrays: promise kept */
    printf("  vec_add -> %g %g %g\n", vout[0], vout[1], vout[2]);

    /* ---------------------------------------------------------------- */
    /* 16. *p++ vs (*p)++                                               */
    /* ---------------------------------------------------------------- */
    printf("\n== *p++ vs (*p)++ ==\n");
    int arr[3] = {10, 20, 30};
    int *ip = arr;
    int v1 = *ip++;                       /* v1 = 10, ip -> arr[1] */
    int v2 = (*ip)++;                     /* v2 = 20, arr[1] = 21, ip unchanged */
    int v3 = *++ip;                       /* ip -> arr[2], v3 = 30 */
    int v4 = ++*ip;                       /* arr[2] = 31, v4 = 31 */
    printf("  v1=%d v2=%d v3=%d v4=%d  arr = {%d, %d, %d}  ip - arr = %td\n",
           v1, v2, v3, v4, arr[0], arr[1], arr[2], ip - arr);

    /* strcpy idiom: consume and advance both pointers */
    char src[] = "copy me", dst[16];
    char *s = src, *t = dst;
    while ((*t++ = *s++) != '\0') { }
    printf("  pointer-walk copy: \"%s\"\n", dst);

    return 0;
}
