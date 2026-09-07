/* main.c — calls the hand-written functions in example.s and asserts results.
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo main.c example.s && ./ex_demo
 * The prototypes below are the whole "interface": the linker matches
 * `add3` here to `_add3` in example.s (macOS adds the underscore for us). */
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

long   add3(long a, long b, long c);
long   sum_array(const long *a, size_t n);
double dot_product(const double *a, const double *b, size_t n);
size_t my_strlen(const char *s);

int main(void) {
    assert(add3(1, 2, 3) == 6);
    assert(add3(-5, 5, 0) == 0);

    long v[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    assert(sum_array(v, 10) == 55);
    assert(sum_array(v, 0) == 0);

    double a[] = {1.0, 2.0, 3.0}, b[] = {4.0, 5.0, 6.0};
    assert(fabs(dot_product(a, b, 3) - 32.0) < 1e-12);
    assert(dot_product(a, b, 0) == 0.0);

    const char *s = "hello, asm";
    assert(my_strlen(s) == strlen(s));
    assert(my_strlen("") == 0);

    printf("add3(1,2,3)        = %ld\n", add3(1, 2, 3));
    printf("sum_array(1..10)   = %ld\n", sum_array(v, 10));
    printf("dot_product        = %.1f\n", dot_product(a, b, 3));
    printf("my_strlen(\"%s\") = %zu\n", s, my_strlen(s));
    puts("all assertions passed");
    return 0;
}
