# Chapter 05 — Pointers

## What you'll be able to do after this chapter

- Explain what a pointer holds, draw the memory picture for `int x; int *p = &x;`, and read `*p`, `&x`, `p + 1` correctly.
- Use pointers to let a function modify the caller's variables (out-parameters, `swap`) and to walk arrays.
- Read any `const` pointer declaration right-to-left and choose the right one for a function signature.
- Use `->` on struct pointers, `int**` to modify a caller's pointer, `void*` for generic memory, and function pointers to pass behavior.
- Handle `argc`/`argv` to read command-line arguments.
- Recognize the six classic pointer bugs on sight: uninitialized, dangling, NULL dereference, off-by-one, `*p++` vs `(*p)++`, returning a local's address.

## Why this matters for ML / numerics / sims

Everything in the remaining chapters is pointers. `malloc` returns one. A matrix struct holds one. `mat_mul(const Mat *a, const Mat *b, Mat *out)` takes three. An ODE solver takes a function pointer to the right-hand side. An autograd node holds pointers to its parents. `argv` is how your training script receives `--lr 0.01`. A tokenizer's vocabulary is `char **`. If pointers are fuzzy, every one of those will be a struggle; if they are clear, they are all the same thing: an address, a type that says how to interpret the bytes there, and arithmetic that moves by `sizeof(type)`.

## 1. What a pointer is

Memory is a huge array of bytes, each with an address (a number). A variable occupies some consecutive bytes. A **pointer** is a variable whose value is an address.

```c
#include <stdio.h>
int main(void)
{
    int x = 42;            /* 4 bytes somewhere, say at address 0x1000 */
    int *p = &x;           /* p holds 0x1000. Type: "pointer to int" */
    printf("x=%d  &x=%p  p=%p  *p=%d  sizeof p=%zu\n", x, (void *)&x, (void *)p, *p, sizeof p);
    *p = 7;                /* write 7 to the 4 bytes at address p: x is now 7 */
    printf("x=%d\n", x);
    return 0;
}
```

Output (addresses vary):

```
x=42  &x=0x16d3a7a3c  p=0x16d3a7a3c  *p=42  sizeof p=8
x=7
```

```
address    contents
        +-----------------+
0x1000  | 42              |  <- x (int, 4 bytes)
        +-----------------+
  ...
        +-----------------+
0x1020  | 0x1000          |  <- p (pointer, 8 bytes): "points to" x
        +-----------------+

   &x  == 0x1000        "address of x"
   p   == 0x1000        p's value
   *p  == 42            "the int at the address stored in p"
```

Every pointer on this platform is 8 bytes, regardless of what it points to. Its TYPE (`int*`, `double*`, `char*`) is not stored in memory; it exists only in the compiler's knowledge and determines how `*p` and `p + 1` behave.

Python equivalent: there is no direct equivalent. Every Python variable is implicitly a reference, and you never see addresses. `id(x)` is the closest peek. NumPy `arr.ctypes.data` is an actual pointer value.

## 2. `&` and `*`

| Operator | Name | Meaning |
|---|---|---|
| `&x` | address-of | the address where `x` lives; type is "pointer to type-of-x" |
| `*p` | dereference (indirection) | the object at address `p`; type is "type-pointed-to" |

They are inverses: `*&x` is `x`; `&*p` is `p`. `*` in a declaration (`int *p`) means "p is a pointer"; `*` in an expression (`*p = 5`) means "the thing p points to". Same symbol, two meanings.

Declaration style: `int *p` and `int* p` are identical to the compiler. But `int* a, b;` declares `a` as a pointer and `b` as a plain `int`; the `*` binds to the name. Write `int *a, *b;` or one declaration per line.

## 3. Pointer types: why `int*` vs `double*` matters

The pointed-to type controls two things:

1. How many bytes `*p` reads/writes and how they are interpreted.
2. How far `p + 1` moves: by `sizeof(*p)` bytes.

```c
#include <stdio.h>
int main(void)
{
    double d[3] = {1.0, 2.0, 3.0};
    double *pd = d;
    char   *pc = (char *)d;          /* same address, different type */
    printf("pd=%p  pd+1=%p  (moved %ld bytes)\n", (void *)pd, (void *)(pd + 1),
           (long)((char *)(pd + 1) - (char *)pd));
    printf("pc=%p  pc+1=%p  (moved %ld bytes)\n", (void *)pc, (void *)(pc + 1),
           (long)((pc + 1) - pc));
    printf("*(pd+1)=%g\n", *(pd + 1));  /* 2.0 */
    return 0;
}
```

Output:

```
pd=0x16b8e7a40  pd+1=0x16b8e7a48  (moved 8 bytes)
pc=0x16b8e7a40  pc+1=0x16b8e7a41  (moved 1 bytes)
*(pd+1)=2
```

Assigning `double *pd = some_int_pointer;` is a compile warning ("incompatible pointer types") and reading through it is UB (strict aliasing, and the bytes mean something else anyway). The exception is `char*`/`unsigned char*`, which may alias anything: that is how `memcpy` and byte dumps work.

## 4. `NULL` and null checks

`NULL` (from `<stddef.h>`, also via `<stdio.h>`/`<stdlib.h>`) is a pointer value guaranteed not to point at any object. It is the "no value" of pointers. Dereferencing it is UB; on macOS/Linux it is a segmentation fault.

```c
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    int *p = NULL;
    if (p == NULL) printf("p is null\n");
    if (!p)        printf("also null (NULL is false)\n");
    /* *p = 1;  <- UB: segfault */
    double *buf = malloc(8 * sizeof *buf);
    if (buf == NULL) { fprintf(stderr, "out of memory\n"); return 1; }
    buf[0] = 1.0;
    free(buf);
    buf = NULL;    /* after free, null the pointer so a later use is a clean crash, not silent corruption */
    return 0;
}
```

Convention: functions that return pointers return `NULL` on failure (`malloc`, `fopen`, `strchr`, `strstr`). Always check. Initialize pointers you do not yet have a target for to `NULL`, never leave them uninitialized.

Python equivalent: `None`, but only for pointers. Ints and doubles have no `None`.

## 5. Pointer arithmetic

Valid operations on pointers into the same array:

| Expression | Meaning | Result type |
|---|---|---|
| `p + n`, `p - n` | address of element `n` positions after/before | pointer |
| `p++`, `p--`, `p += n` | move `p` | pointer |
| `p - q` | number of ELEMENTS between them | `ptrdiff_t` (signed; print with `%td`) |
| `p < q`, `p == q`, ... | position comparison | `int` |
| `p[n]` | `*(p + n)` | element |

```c
#include <stdio.h>
int main(void)
{
    int a[5] = {10, 20, 30, 40, 50};
    int *p = a;                  /* &a[0] */
    int *q = &a[3];
    printf("*p=%d  *(p+2)=%d  p[2]=%d\n", *p, *(p + 2), p[2]);   /* 10 30 30 */
    printf("q - p = %td elements\n", q - p);                       /* 3 */
    printf("p < q: %d\n", p < q);                                  /* 1 */
    p++;                         /* now &a[1] */
    printf("after p++: *p=%d\n", *p);                              /* 20 */
    /* Walk the array with a pointer: */
    int sum = 0;
    for (int *it = a; it != a + 5; it++) sum += *it;              /* a + 5 is "one past the end" */
    printf("sum=%d\n", sum);                                       /* 150 */
    return 0;
}
```

Rules:

- Arithmetic is in units of the pointed-to type. `p + 1` on `double*` moves 8 bytes.
- You may form a pointer to any element and to ONE PAST the last element (`a + n`); that one may be compared but not dereferenced. Anything beyond that, or before `a`, is UB even without dereferencing.
- `p - q` is only defined when both point into the same array.
- Adding two pointers is meaningless and does not compile. Multiplying is not allowed.
- `void*` arithmetic is not allowed in standard C (GCC/clang permit it as an extension; do not rely on it). Cast to `char*` for byte offsets.

## 6. Pointers and arrays

`a[i]` is DEFINED as `*(a + i)`. An array name, in most contexts, becomes a pointer to its first element. This is the "decay" from `../04_arrays_and_strings/lesson.md`.

```c
#include <stdio.h>
int main(void)
{
    double a[4] = {1.5, 2.5, 3.5, 4.5};
    double *p = a;                          /* decay: same as &a[0] */
    printf("%g %g %g %g\n", a[2], *(a + 2), p[2], *(p + 2));   /* all 3.5 */
    printf("%g\n", 2[a]);                   /* 3.5: legal, since *(2 + a). Never write this. */
    printf("sizeof a = %zu, sizeof p = %zu\n", sizeof a, sizeof p);   /* 32, 8 */
    printf("&a = %p, &a[0] = %p, a = %p\n", (void *)&a, (void *)&a[0], (void *)a);   /* all equal */
    /* p = p + 1;   fine: p is a variable */
    /* a = a + 1;   ERROR: a is not a variable you can assign; it is the array */
    return 0;
}
```

Differences between an array `a` and a pointer `p` to its first element:

| | `double a[4]` | `double *p = a` |
|---|---|---|
| `sizeof` | 32 (whole array) | 8 (the pointer) |
| assignable? | no | yes |
| `&a` type | `double (*)[4]` (pointer to array of 4) | `double **` |
| storage | the 32 bytes of data | 8 bytes holding an address |
| passed to function | decays to `double*` | passed as-is |

Inside a function, `double v[]` and `double *v` as a parameter are the same thing: a pointer. The `[]` is documentation only.

## 7. Passing pointers to modify the caller's variable

Functions receive copies (chapter 03). To let a function change your variable, pass its address; the function writes through the pointer.

```c
#include <stdio.h>

static void swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Out-parameters: return two results through pointers, status via return value. */
static int vec_minmax(const double *v, size_t n, double *min_out, double *max_out)
{
    if (n == 0) return -1;
    double mn = v[0], mx = v[0];
    for (size_t i = 1; i < n; i++) {
        if (v[i] < mn) mn = v[i];
        if (v[i] > mx) mx = v[i];
    }
    *min_out = mn;                          /* write into the caller's variables */
    *max_out = mx;
    return 0;
}

int main(void)
{
    int x = 1, y = 2;
    swap(&x, &y);
    printf("x=%d y=%d\n", x, y);            /* 2 1 */

    double data[] = {3.0, -1.0, 7.5};
    double lo, hi;
    if (vec_minmax(data, 3, &lo, &hi) == 0) printf("min=%g max=%g\n", lo, hi);
    return 0;
}
```

Output:

```
x=2 y=1
min=-1 max=7.5
```

This is the C answer to "return multiple values": the return value carries a status code, out-parameters carry results. `scanf("%d", &n)` works the same way. Convention: outputs last, named `*_out` or documented.

Python equivalent: `return lo, hi`. There is no equivalent to `swap(&x, &y)` because Python ints are immutable; you would write `x, y = y, x` at the call site.

## 8. Returning pointers and the dangling-local bug

A function may return a pointer, as long as the pointee OUTLIVES the call. Valid: pointers into caller-provided arrays, `malloc`ed memory, `static` storage, string literals. INVALID: the address of a local variable, whose stack frame is destroyed on return.

```c
#include <stdio.h>
#include <string.h>

/* OK: returns a pointer into the caller's array */
static const char *find_space(const char *s)
{
    for (; *s; s++) if (*s == ' ') return s;
    return NULL;
}

/* BUG: returns the address of a local. The array dies when the function returns. */
static char *make_label_bad(int n)
{
    char buf[32];
    snprintf(buf, sizeof buf, "item_%d", n);
    return buf;                             /* clang: "address of stack memory ... returned" */
}

/* OK: caller supplies the storage */
static void make_label(char *buf, size_t size, int n)
{
    snprintf(buf, size, "item_%d", n);
}

int main(void)
{
    const char *sp = find_space("hello world");
    printf("rest: \"%s\"\n", sp ? sp + 1 : "(none)");   /* world */
    char label[32];
    make_label(label, sizeof label, 7);
    printf("%s\n", label);                  /* item_7 */
    (void)make_label_bad;                   /* referenced only to keep the compiler quiet */
    return 0;
}
```

`make_label_bad` returns a pointer to memory that will be overwritten by the next function call. Sometimes it "works" (the memory has not been reused yet), then breaks when you add a `printf`. `-Wall` catches the direct case (`-Wreturn-stack-address`) but not indirect ones (storing `&local` in a struct that outlives the function). The fix is always one of: caller provides the buffer, `malloc`, or `static` (with the reentrancy cost).

## 9. `const` and pointers: read right-to-left

There are two things `const` can protect: the pointer, or what it points to. Read the declaration from the name outward, right to left.

| Declaration | Read as | Can change `*p`? | Can change `p`? |
|---|---|---|---|
| `int *p` | p is a pointer to int | yes | yes |
| `const int *p` | p is a pointer to a const int | no | yes |
| `int const *p` | same as above | no | yes |
| `int *const p` | p is a const pointer to int | yes | no |
| `const int *const p` | p is a const pointer to a const int | no | no |

```c
int x = 1, y = 2;
const int *p = &x;   /* pointer to const: read-only view */
/* *p = 5;   ERROR */
p = &y;              /* OK: the pointer itself can move */

int *const q = &x;   /* const pointer: fixed target */
*q = 5;              /* OK */
/* q = &y;   ERROR */
```

In function signatures, `const T *` is the important one: it promises the function will not modify the pointed-to data, lets callers pass `const` data, and helps the compiler. `mat_mul(const Mat *a, const Mat *b, Mat *out)` says exactly which argument is written. Every read-only array parameter should be `const`.

A `const int *` can be assigned from an `int *` (adding const is safe). The reverse needs a cast and is usually a design smell.

## 10. Pointer to struct and `->`

`(*p).field` accesses a field through a pointer. `p->field` is the same thing, shorter. Structs are passed to functions by pointer almost always: copying a struct with a 1000-element array inside is slow, and the function often needs to modify it.

```c
#include <stdio.h>

struct vec3 { double x, y, z; };

static void vec3_scale(struct vec3 *v, double k)     /* modifies the caller's struct */
{
    v->x *= k;                                       /* same as (*v).x *= k */
    v->y *= k;
    v->z *= k;
}

static double vec3_dot(const struct vec3 *a, const struct vec3 *b)   /* read-only */
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

int main(void)
{
    struct vec3 v = {1.0, 2.0, 3.0};
    struct vec3 *pv = &v;
    vec3_scale(pv, 2.0);
    printf("%g %g %g  dot=%g\n", v.x, pv->y, (*pv).z, vec3_dot(&v, &v));   /* 2 4 6 dot=56 */
    return 0;
}
```

This is the shape of every "object" in C: a struct, plus functions taking a pointer to it as the first argument. `self` in Python is `struct T *self` here. Full struct treatment is in the next chapter.

## 11. Pointers to pointers: `int **`

A pointer can point to another pointer. Two uses dominate:

**(a) Letting a function change the caller's POINTER.** Since arguments are copied, to change a `double *buf` in the caller you pass `&buf` (a `double **`).

```c
#include <stdio.h>
#include <stdlib.h>

static int alloc_buffer(double **out, size_t n)   /* out points to the caller's pointer */
{
    double *p = malloc(n * sizeof *p);
    if (!p) return -1;
    for (size_t i = 0; i < n; i++) p[i] = (double)i;
    *out = p;                                       /* write the address into the caller's variable */
    return 0;
}

int main(void)
{
    double *buf = NULL;
    if (alloc_buffer(&buf, 4) != 0) return 1;
    printf("buf[3]=%g\n", buf[3]);                  /* 3 */
    free(buf);
    return 0;
}
```

```
main's frame                 heap
+-----------------+          +-----+-----+-----+-----+
| buf: 0x6000...  |--------->| 0.0 | 1.0 | 2.0 | 3.0 |
+-----------------+          +-----+-----+-----+-----+
       ^
       | out (double **) inside alloc_buffer holds &buf
```

**(b) Arrays of strings.** A string is `char *`; an array of them is `char *arr[]`, which decays to `char **`.

```c
const char *vocab[] = {"the", "cat", "sat"};   /* array of 3 pointers to char */
const char **pv = vocab;                        /* points to vocab[0] */
printf("%s %s %c\n", vocab[1], pv[2], pv[1][0]);   /* cat sat c */
```

```
vocab:  +--------+--------+--------+
        | ptr    | ptr    | ptr    |
        +---|----+---|----+---|----+
            v        v        v
          "the\0"  "cat\0"  "sat\0"     (read-only literals, scattered in memory)
```

Note this is NOT a 2-D array. `char grid[3][4]` is 12 contiguous chars; `char *vocab[3]` is 3 pointers to strings of any length anywhere. Both are indexed `x[i][j]`, which is why people confuse them.

## 12. `argc` and `argv`: command-line arguments

`int main(int argc, char *argv[])` receives the command line. `argc` is the count; `argv` is an array of `argc + 1` `char*` (the last is `NULL`). `argv[0]` is the program name.

```c
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char *argv[])                    /* char **argv is identical */
{
    printf("argc=%d\n", argc);
    for (int i = 0; i < argc; i++) printf("argv[%d]=\"%s\"\n", i, argv[i]);

    /* Typical: ./prog N LR */
    if (argc < 3) { fprintf(stderr, "usage: %s N LR\n", argv[0]); return 2; }
    char *end;
    long n = strtol(argv[1], &end, 10);
    if (*end != '\0' || n <= 0) { fprintf(stderr, "bad N\n"); return 2; }
    double lr = strtod(argv[2], &end);
    if (*end != '\0') { fprintf(stderr, "bad LR\n"); return 2; }
    printf("n=%ld lr=%g\n", n, lr);
    return 0;
}
```

```sh
$ ./prog 1000 0.01
argc=3
argv[0]="./prog"
argv[1]="1000"
argv[2]="0.01"
n=1000 lr=0.01
```

```
argv ---> +--------+--------+--------+--------+
          | ptr    | ptr    | ptr    | NULL   |
          +---|----+---|----+---|----+--------+
              v        v        v
           "./prog"  "1000"   "0.01"
```

All arguments are strings; convert with `strtol`/`strtod` and check `endptr`. This is how your training programs will take `--epochs 10 --lr 0.001`, and how your sims will take grid sizes.

Python equivalent: `sys.argv`, a list of `str`; `sys.argv[0]` is the script name.

## 13. `void *`: the generic pointer

`void *` is "pointer to something, type unknown". Any object pointer converts to and from `void *` implicitly (no cast needed in C). You cannot dereference it or do arithmetic on it; you must convert it back to a typed pointer first.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    double *d = malloc(4 * sizeof *d);   /* malloc returns void*; converts to double* silently */
    void *anything = d;                   /* store a double* as void* */
    double *back = anything;              /* convert back */
    back[0] = 2.5;
    printf("%g\n", d[0]);                 /* 2.5 */
    /* Byte-level access: cast to unsigned char* */
    unsigned char *bytes = anything;
    printf("first byte: %02x\n", bytes[0]);
    /* memcpy/memset/qsort take void* so they work on any element type */
    memset(anything, 0, 4 * sizeof(double));
    free(d);
    return 0;
}
```

Used by `malloc`, `free`, `memcpy`, `memset`, `qsort`, `bsearch`, and any "container of anything" you write yourself (e.g. a generic dynamic array storing `elem_size` bytes per element). Type safety is gone at these boundaries, so keep them small.

## 14. Function pointers (introduction)

A function has an address too. A pointer to a function lets you pass behavior as an argument. Syntax: `return_type (*name)(param_types)`.

```c
#include <stdio.h>
#include <math.h>

static double square(double x) { return x * x; }

/* Numerical integration: takes the integrand as a function pointer */
static double integrate(double (*f)(double), double a, double b, int n)
{
    double h = (b - a) / n, s = 0.5 * (f(a) + f(b));
    for (int i = 1; i < n; i++) s += f(a + i * h);
    return s * h;
}

int main(void)
{
    double (*fp)(double) = square;        /* fp holds the address of square; & is optional */
    printf("%g\n", fp(3.0));              /* 9 */
    printf("%.6f\n", integrate(square, 0.0, 1.0, 1000));   /* 0.333333 */
    printf("%.6f\n", integrate(sin, 0.0, M_PI, 1000));     /* 2.000000: pass a libm function directly */
    return 0;
}
```

`typedef double (*fn1)(double);` names the type so signatures stay readable. Function pointers are how you will pass the RHS to an ODE solver, the loss to an optimizer, the comparator to `qsort`, and per-op forward/backward functions in autograd. Full treatment in chapter 11.

Python equivalent: passing a function as an argument (`integrate(math.sin, 0, math.pi)`). C requires the exact signature to match.

## 15. Aliasing and `restrict` (mention)

Two pointers **alias** when they point to overlapping memory. The compiler must assume `a[i]` and `b[j]` might be the same object unless told otherwise, which blocks optimizations (it must reload after every store). `restrict` (C99) is a promise that, for the pointer's lifetime, only it (or pointers derived from it) accesses that memory:

```c
void vec_add(size_t n, double *restrict out, const double *restrict a, const double *restrict b)
{
    for (size_t i = 0; i < n; i++) out[i] = a[i] + b[i];   /* compiler may vectorize freely */
}
```

Calling `vec_add(n, x, x, y)` after promising `restrict` is UB. Use `restrict` on hot numerical kernels once they are correct; it is the C equivalent of what lets NumPy/BLAS vectorize. Also: the standard says accessing an object through a pointer of an incompatible type (e.g. reading a `float` as `unsigned int` via `*(unsigned *)&f`) is UB ("strict aliasing"). Use `memcpy` to reinterpret bytes.

## 16. Common bugs

| Bug | Code | What happens |
|---|---|---|
| Uninitialized pointer | `int *p; *p = 1;` | writes to a random address; crash or silent corruption |
| Dereferencing NULL | `int *p = NULL; *p = 1;` | segfault |
| Dangling (local) | `return &local;` | pointer to dead stack memory |
| Dangling (freed) | `free(p); *p = 1;` | use-after-free; corrupts heap |
| Off-by-one | `for (i = 0; i <= n; i++) p[i]` | writes one past the end |
| `*p++` vs `(*p)++` | see below | advances pointer vs increments value |
| Wrong pointer type | `int *p = (int *)&some_double;` | reads garbage; UB |
| Comparing to `0` element | `if (p == 0)` vs `if (*p == 0)` | tests the pointer, not the value |
| `sizeof(p)` for array size | `malloc(sizeof p)` | allocates 8 bytes |
| Pointer to loop variable escaping | storing `&i` and using after the loop | dangling |

`*p++` vs `(*p)++`, precisely:

```c
int a[3] = {10, 20, 30};
int *p = a;
int v1 = *p++;        /* v1 = 10; p now points to a[1]. Parsed as *(p++). */
int v2 = (*p)++;      /* v2 = 20; a[1] is now 21; p unchanged. */
int v3 = *++p;        /* p moves to a[2]; v3 = 30. */
int v4 = ++*p;        /* a[2] becomes 31; v4 = 31. */
```

`*p++` is a very common idiom for "consume an element and advance" (`while (*src) *dst++ = *src++;` is `strcpy`). Learn it; do not avoid it.

Tools: `-fsanitize=address` catches out-of-bounds, use-after-free, and stack-use-after-return (with `-fsanitize-address-use-after-return=always`); `-fsanitize=undefined` catches misaligned and null dereferences. Build with both while learning.

## Gotchas and undefined behavior

- Dereferencing an uninitialized, NULL, freed, or out-of-range pointer: UB.
- Forming a pointer more than one past the end of an array (even without dereferencing): UB.
- Subtracting pointers into different arrays: UB.
- Returning the address of a local: dangling pointer, UB on use.
- `int* a, b;` makes `b` an `int`, not a pointer.
- `printf("%p", p)` without a `(void *)` cast is technically UB for non-`void*` pointers (works in practice; cast anyway to silence `-Wformat`).
- `printf("%d", p)`: type mismatch, UB.
- `if (p = NULL)` assigns. `-Wall` warns.
- Modifying a string literal through a `char *`: UB (crash).
- Reading a `float` through an `unsigned *`: strict-aliasing UB. Use `memcpy`.
- Passing `&x` where `x` is an `int` to a function expecting `long *`: incompatible pointer warning; writing 8 bytes into 4 is UB.
- `void *` arithmetic is a GNU extension; cast to `char *`.
- `*p++` increments the pointer, not the value.
- `argv[argc]` is `NULL`; `argv[argc + 1]` is UB.
- Comparing pointers with `<` from different arrays: UB (only `==`/`!=` are defined for those).
- Forgetting that `p` and `q` may alias when writing `out[i] = a[i] + b[i]` with `out == a`: correct but slow; `restrict` fixes speed, and lying about it is UB.

## Common mistakes checklist

- [ ] Every pointer is initialized (to a target or to `NULL`) at declaration.
- [ ] Every pointer returned by a library function (`malloc`, `fopen`, `strchr`) is checked for `NULL`.
- [ ] No function returns the address of a local.
- [ ] Read-only pointer parameters are `const T *`.
- [ ] Loops over arrays via pointers stop at `a + n`, never dereference it.
- [ ] `%p` arguments are cast to `(void *)`.
- [ ] `argc` is checked before indexing `argv`; each argument is converted with `strtol`/`strtod` and validated.
- [ ] Out-parameters are the last arguments and are written on every success path.
- [ ] `*p++` is used only when advancing the pointer is intended.
- [ ] After `free(p)`, `p` is set to `NULL` if it stays in scope.
- [ ] Tested with `-fsanitize=address,undefined`.

## You can move on when...

- Given `int x = 5; int *p = &x; int **pp = &p;`, you can state the type and value of `x`, `p`, `*p`, `pp`, `*pp`, `**pp`, and draw the boxes.
- You can write `swap` and a function returning two results via out-parameters with a status code.
- You can read `const char *const *argv` aloud correctly.
- You can explain why `sizeof a` and `sizeof p` differ for `double a[4]; double *p = a;`.
- You can explain what `*p++`, `(*p)++`, and `*++p` each do to `p` and to the array.
- You can write `main(int argc, char *argv[])` that parses an integer and a double with error checking.
- You can write a function taking `double (*f)(double)` and call it with both your own function and `sin`.
- You can identify why `char *s = make_label_bad(3); printf("%s", s);` is broken without running it.
