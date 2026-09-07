/*
 * Chapter 02 example: branching, loops, switch, goto cleanup, numeric loop idioms.
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 *   ./ex_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/* An enum gives named integer constants; ideal as a switch subject.
   This is how an autograd library dispatches on the node type. */
enum op { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_RELU };

static double apply(enum op o, double a, double b)
{
    switch (o) {
    case OP_ADD: return a + b;
    case OP_SUB: return a - b;
    case OP_MUL: return a * b;
    case OP_DIV:
        /* short-circuit guard: the division is not evaluated when b == 0 */
        return b != 0.0 ? a / b : NAN;
    case OP_RELU:
        return a > 0.0 ? a : 0.0;          /* ternary as an expression */
    default:
        fprintf(stderr, "unknown op %d\n", (int)o);
        return NAN;
    }
}

/* goto-based cleanup: the one place goto is standard practice in C. */
static int allocate_two(size_t n)
{
    int status = 1;                        /* assume failure */
    double *a = malloc(n * sizeof *a);
    if (a == NULL) goto out;
    double *b = malloc(n * sizeof *b);
    if (b == NULL) goto free_a;

    for (size_t i = 0; i < n; i++) { a[i] = (double)i; b[i] = 2.0 * a[i]; }
    printf("  a[3]=%g b[3]=%g\n", a[3], b[3]);
    status = 0;                            /* success */

    free(b);
free_a:
    free(a);
out:
    return status;
}

int main(void)
{
    /* ---------------------------------------------------------------- */
    /* 1. if / else if / else, truthiness                               */
    /* ---------------------------------------------------------------- */
    printf("== if/else ==\n");
    double losses[] = {0.05, 0.4, 3.0};
    for (int i = 0; i < 3; i++) {
        double loss = losses[i];
        if (loss < 0.1)       printf("  %.2f: converged\n", loss);
        else if (loss < 1.0)  printf("  %.2f: training\n", loss);
        else                  printf("  %.2f: diverging\n", loss);
    }
    int zero = 0, five = 5;
    bool flag = 42;                        /* bool normalizes any nonzero to 1 */
    printf("  0 is %s, 5 is %s, bool(42)=%d\n",
           zero ? "true" : "false", five ? "true" : "false", flag);
    printf("  3 < 4 evaluates to %d; 3 > 4 to %d\n", 3 < 4, 3 > 4);

    /* Chained comparison trap: `0 < x < 10` parses as (0 < x) < 10, which is
       (0 or 1) < 10: always true. clang warns if you write it literally; we
       compute the two halves by hand to show the values. */
    int x = 50;
    int first_half = 0 < x;                /* 1 */
    printf("  0 < x < 10 with x=50: %d (wrong!)   0 < x && x < 10: %d\n",
           first_half < 10, 0 < x && x < 10);

    /* ---------------------------------------------------------------- */
    /* 2. switch dispatch                                               */
    /* ---------------------------------------------------------------- */
    printf("\n== switch ==\n");
    const char *names[] = {"add", "sub", "mul", "div", "relu"};
    for (int o = OP_ADD; o <= OP_RELU; o++)
        printf("  %-4s(6, 0) = %g\n", names[o], apply((enum op)o, 6.0, 0.0));

    /* Fallthrough, deliberate: group cases. */
    for (char c = 'a'; c <= 'f'; c++) {
        switch (c) {
        case 'a': case 'e':                /* vowels share one body */
            printf("  %c vowel\n", c);
            break;
        case 'b':
            printf("  %c ", c);
            /* fallthrough */              /* no break: continues into 'c' */
        case 'c':
            printf("consonant (b or c)\n");
            break;
        default:
            printf("  %c other\n", c);
            break;
        }
    }

    /* ---------------------------------------------------------------- */
    /* 3. while: Newton iteration until converged, with a cap           */
    /* ---------------------------------------------------------------- */
    printf("\n== while ==\n");
    double r = 1.0, prev = 0.0;
    int iters = 0;
    while (fabs(r - prev) > 1e-12 && iters < 100) {
        prev = r;
        r = 0.5 * (r + 2.0 / r);           /* Newton step for sqrt(2) */
        iters++;
    }
    printf("  sqrt(2) = %.15f in %d iterations\n", r, iters);

    /* ---------------------------------------------------------------- */
    /* 4. do-while: body runs at least once                             */
    /* ---------------------------------------------------------------- */
    printf("\n== do-while ==\n");
    int attempts = 0, val;
    do {
        val = attempts * 7 % 5;
        attempts++;
    } while (val != 3 && attempts < 10);
    printf("  got %d after %d attempts\n", val, attempts);

    /* ---------------------------------------------------------------- */
    /* 5. for: all clause variants                                      */
    /* ---------------------------------------------------------------- */
    printf("\n== for ==\n  ");
    for (int i = 0; i < 5; i++) printf("%d ", i);
    printf("\n  ");
    for (int i = 0, j = 10; i < j; i += 3, j -= 3) printf("(%d,%d) ", i, j);
    printf("\n  ");
    int n = 0;
    for (; n * n < 50; n++) { }            /* empty init, empty body */
    printf("first n with n*n >= 50: %d\n  ", n);
    int count = 0;
    for (;;) { if (++count >= 3) break; }  /* infinite loop, exit via break */
    printf("for(;;) broke at count=%d\n", count);

    /* Floating loop counter: rounding makes the count unreliable. */
    int steps = 0;
    for (double t = 0.0; t < 1.0; t += 0.1) steps++;
    printf("  double counter ran %d times (integer counter would give 10)\n", steps);

    /* ---------------------------------------------------------------- */
    /* 6. break / continue                                              */
    /* ---------------------------------------------------------------- */
    printf("\n== break/continue ==\n");
    double data[] = {1.0, -1.0, 2.5, -3.0, 4.0, 1e300, 0.5};
    double sum = 0.0;
    int used = 0;
    for (int i = 0; i < 7; i++) {
        if (data[i] < 0) continue;         /* skip negatives */
        if (data[i] > 1e100) break;        /* stop at absurd value */
        sum += data[i];
        used++;
    }
    printf("  sum=%g over %d values\n", sum, used);

    /* ---------------------------------------------------------------- */
    /* 7. Nested loops + goto as labeled break                          */
    /* ---------------------------------------------------------------- */
    printf("\n== nested search ==\n");
    int grid[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    int fi = -1, fj = -1;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            if (grid[i][j] == 7) { fi = i; fj = j; goto found; }
found:
    printf("  7 is at (%d,%d)\n", fi, fj);

    /* ---------------------------------------------------------------- */
    /* 8. goto cleanup                                                  */
    /* ---------------------------------------------------------------- */
    printf("\n== goto cleanup ==\n");
    printf("  allocate_two returned %d\n", allocate_two(16));

    /* ---------------------------------------------------------------- */
    /* 9. Precedence traps                                              */
    /* ---------------------------------------------------------------- */
    printf("\n== precedence ==\n");
    unsigned flags = 0x5;                  /* binary 101 */
    /* If you write `flags & 4 == 4` literally, clang warns (-Wparentheses),
       because it parses as `flags & (4 == 4)`. Written that way explicitly: */
    printf("  flags & 4 == 4   -> %u  (parsed as flags & (4==4) = flags & 1)\n", flags & (4 == 4));
    printf("  (flags & 4) == 4 -> %d\n", (flags & 4) == 4);
    int a = 6, b = 3, c = 2;
    printf("  a / b * c = %d, a / (b * c) = %d\n", a / b * c, a / (b * c));
    /* Likewise `1 << 2 + 1` warns; it parses as 1 << (2 + 1). */
    printf("  1 << 2 + 1 = %d (shift binds looser than +)\n", 1 << (2 + 1));
    int i = 5;
    int post = i++;                        /* post = 5, i = 6 */
    int pre  = ++i;                        /* i = 7, pre = 7 */
    printf("  i++ gave %d, ++i gave %d, i is now %d\n", post, pre, i);

    /* Dangling else: braces make the intent explicit. */
    int p = 1, q = 0;
    if (p) {
        if (q) printf("  both\n");
    } else {
        printf("  not p\n");
    }
    printf("  (dangling-else case printed nothing, correctly)\n");

    /* ---------------------------------------------------------------- */
    /* 10. Numeric loop idioms                                          */
    /* ---------------------------------------------------------------- */
    printf("\n== numeric idioms ==\n");
    double v[] = {-3.0, -1.5, -7.25, -0.5, -2.0};
    int nv = 5;

    /* argmax: start from element 0, not from 0.0 (all values are negative here) */
    int best = 0;
    for (int k = 1; k < nv; k++)
        if (v[k] > v[best]) best = k;
    printf("  argmax=%d value=%g\n", best, v[best]);

    /* Welford running mean/variance */
    double mean = 0.0, m2 = 0.0;
    for (int k = 0; k < nv; k++) {
        double delta = v[k] - mean;
        mean += delta / (k + 1);           /* double / int -> double */
        m2 += delta * (v[k] - mean);
    }
    printf("  mean=%.4f sample var=%.4f\n", mean, m2 / (nv - 1));

    /* i/j/k matrix multiply on flat row-major arrays: C(2x2) = A(2x3) * B(3x2) */
    double A[6] = {1, 2, 3,
                   4, 5, 6};
    double B[6] = {7, 8,
                   9, 10,
                   11, 12};
    double C[4];
    int m = 2, kdim = 3, pcols = 2;
    for (int ii = 0; ii < m; ii++)
        for (int jj = 0; jj < pcols; jj++) {
            double acc = 0.0;              /* re-zero for every output cell */
            for (int kk = 0; kk < kdim; kk++)
                acc += A[ii * kdim + kk] * B[kk * pcols + jj];
            C[ii * pcols + jj] = acc;
        }
    printf("  C = [%g %g; %g %g]\n", C[0], C[1], C[2], C[3]);

    /* Reverse iteration with an unsigned index: the i-- > 0 idiom */
    size_t len = 5;
    printf("  reverse: ");
    for (size_t k = len; k-- > 0; ) printf("%zu ", k);
    printf("\n");

    /* Convergence loop with cap and NaN guard */
    double err = INFINITY;
    int it = 0, max_it = 50;
    double y = 10.0;
    while (err > 1e-9 && it < max_it && !isnan(err)) {
        double ynew = y * 0.5;             /* pretend solver step */
        err = fabs(ynew - y);
        y = ynew;
        it++;
    }
    printf("  converged=%s after %d steps, y=%g\n", err <= 1e-9 ? "yes" : "no", it, y);

    return 0;
}
