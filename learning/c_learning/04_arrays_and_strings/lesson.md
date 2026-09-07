# Chapter 04 — Arrays and Strings

## What you'll be able to do after this chapter

- Declare, initialize, and size fixed arrays; pass them to functions together with their length.
- Lay out a matrix as a flat row-major array, index it with `i * cols + j`, and explain why that beats `a[i][j]`.
- Recognize an out-of-bounds access and a buffer overflow as undefined behavior and know how to make them impossible.
- Work with C strings: NUL termination, `<string.h>` functions, safe construction with `snprintf`, and conversion to numbers with `strtol`/`strtod` and error checking.
- Split and scan strings byte by byte, treat `char` as the small integer it is, and know what UTF-8 does to "one character".
- Parse a line of comma-separated floats, which is the first step of every dataset loader you will write.

## Why this matters for ML / numerics / sims

A matrix is a flat `double` array plus two integers. A tokenizer is a loop over bytes. A CSV loader is `strtod` in a loop. An FDTD grid is a flat array indexed `i * ny + j`. A vocabulary is an array of strings. There is no `np.ndarray` with a `.shape`; you carry the shape yourself, and every off-by-one is a silent memory corruption rather than an `IndexError`. This chapter is where you learn the discipline that replaces the runtime checks Python did for you.

## 1. Fixed-size arrays

An array is a contiguous block of `N` elements of one type. The size is a compile-time constant and part of the type.

```c
#include <stdio.h>
int main(void)
{
    double x[4];                          /* 4 doubles, 32 bytes, UNINITIALIZED (garbage) */
    x[0] = 1.5;                           /* indices run 0 .. 3 */
    x[3] = -2.0;
    printf("%zu bytes, %zu elements\n", sizeof x, sizeof x / sizeof x[0]);
    printf("x[0]=%g x[3]=%g\n", x[0], x[3]);
    return 0;
}
```

Output:

```
32 bytes, 4 elements
x[0]=1.5 x[3]=-2
```

Memory:

```
address:   base   base+8  base+16 base+24
          +------+-------+-------+-------+
   x:     | 1.5  |  ???  |  ???  | -2.0  |
          +------+-------+-------+-------+
            x[0]    x[1]    x[2]    x[3]
```

Elements are adjacent; `x[i]` lives at `base + i * sizeof(double)`. There is no header, no length field, no bounds check.

Python equivalent: `np.empty(4)` (uninitialized) with a fixed dtype. Unlike a Python list, an array cannot grow and holds one type.

## 2. Initialization

```c
int a[5] = {1, 2, 3, 4, 5};              /* full */
int b[5] = {1, 2};                       /* partial: rest are ZERO -> {1,2,0,0,0} */
int c[5] = {0};                          /* all zero (idiom) */
int d[]  = {1, 2, 3};                    /* size inferred: 3 */
int e[5] = {[2] = 7, [4] = 9};           /* designated: {0,0,7,0,9} (C99) */
int f[5];                                /* NO initializer: garbage. Reading it is UB. */
double g[3] = {};                        /* empty braces: not valid C11 (C23 allows). Use {0}. */
```

Rule: once you initialize ANY element, all others become 0. `{0}` is the universal "zero everything" idiom. Uninitialized automatic arrays contain whatever was on the stack; reading them is UB and a common source of "works in debug, fails in release".

`static` and global arrays are zero-initialized automatically.

## 3. `sizeof(a) / sizeof(a[0])`

The element count of a real array (not a pointer) is `sizeof a / sizeof a[0]`. Wrap it in a macro:

```c
#define ARRAY_LEN(a) (sizeof (a) / sizeof (a)[0])

double w[] = {0.1, 0.2, 0.3, 0.4};
for (size_t i = 0; i < ARRAY_LEN(w); i++) printf("%g ", w[i]);   /* 0.1 0.2 0.3 0.4 */
```

The result is `size_t`. This works ONLY in the scope where `w` is declared as an array. Inside a function receiving `double *w`, `sizeof w` is 8 (pointer size) and the macro gives 1. See section 5.

## 4. Arrays do NOT carry their length

C never stores the length. Every function operating on an array takes the length as a separate parameter, and every caller must pass it correctly.

```c
double vec_sum(const double *v, size_t n)   /* n comes from the caller */
{
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += v[i];
    return s;
}

double data[] = {1, 2, 3, 4};
vec_sum(data, ARRAY_LEN(data));            /* correct */
vec_sum(data, 10);                         /* compiles. Reads 6 doubles past the end. UB. */
```

Convention: `(pointer, length)` pairs everywhere. Later you will bundle them in a struct (`struct vec { double *data; size_t len; }`), which is what every real C numerics library does.

## 5. Out-of-bounds is undefined behavior

`a[n]` on an array of `n` elements is not an error the compiler or runtime detects. It reads or writes whatever memory follows the array: another variable, the return address, unmapped memory. The symptoms range from "nothing visible" to "wrong result 200 lines later" to "segmentation fault".

```c
#include <stdio.h>
int main(void)
{
    int guard1 = 111;
    int a[3] = {1, 2, 3};
    int guard2 = 222;
    /* a[3] = 999;   <- UB: writes past the end. Might clobber guard1 or guard2 or neither. */
    /* printf("%d", a[-1]);  <- UB: reads before the start. */
    printf("%d %d\n", guard1, guard2);
    return 0;
}
```

Do not run the commented lines to "see what happens"; what happens is not defined and will differ at `-O0` and `-O2`. To catch these while developing, compile with the sanitizer:

```sh
cc -Wall -Wextra -std=c11 -O1 -g -fsanitize=address -o prog prog.c -lm
./prog
# ==1234==ERROR: AddressSanitizer: stack-buffer-overflow ... WRITE of size 4 at ... a[3]
```

AddressSanitizer (ASan) reports the exact line. Use it on every program until bounds discipline is automatic.

Python equivalent: `IndexError`. C gives you nothing.

## 6. Multidimensional arrays and row-major layout

`double a[3][4]` is "an array of 3 arrays of 4 doubles". All 12 elements are contiguous, row after row:

```
a[3][4] in memory (row-major):

offset:  0    1    2    3    4    5    6    7    8    9   10   11
       +----+----+----+----+----+----+----+----+----+----+----+----+
       |a00 |a01 |a02 |a03 |a10 |a11 |a12 |a13 |a20 |a21 |a22 |a23 |
       +----+----+----+----+----+----+----+----+----+----+----+----+
       |<----- row 0 ----->|<----- row 1 ----->|<----- row 2 ----->|

a[i][j] is at offset i * 4 + j
```

```c
#include <stdio.h>
int main(void)
{
    double a[3][4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
    };
    printf("a[1][2]=%g\n", a[1][2]);                     /* 7 */
    printf("sizeof a=%zu, sizeof a[0]=%zu\n", sizeof a, sizeof a[0]);  /* 96, 32 */
    /* Prove contiguity: walk the whole thing with a flat pointer. */
    const double *flat = &a[0][0];
    for (int k = 0; k < 12; k++) printf("%g ", flat[k]);
    printf("\n");
    return 0;
}
```

Output:

```
a[1][2]=7
sizeof a=96, sizeof a[0]=32
1 2 3 4 5 6 7 8 9 10 11 12
```

Row-major means the LAST index varies fastest. NumPy's default (`order='C'`) is exactly this. Fortran and MATLAB are column-major.

Consequence for performance: iterating `for i: for j: a[i][j]` walks memory sequentially (cache-friendly). Iterating `for j: for i: a[i][j]` jumps by a row each step (cache-hostile, several times slower for large arrays).

Passing a 2-D array to a function requires the column count in the type: `void f(double m[][4], int rows)` or `void f(int rows, int cols, double m[rows][cols])` (VLA parameter, C99). Both are awkward, which is why flat arrays are preferred.

## 7. Flat indexing `a[i*cols + j]` and why flat is preferred

Store the matrix as ONE 1-D array of `rows * cols` elements and compute the offset yourself:

```c
#include <stdio.h>

static inline size_t idx(size_t i, size_t j, size_t cols) { return i * cols + j; }

static void mat_print(const double *m, size_t rows, size_t cols)
{
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) printf("%6.1f", m[idx(i, j, cols)]);
        printf("\n");
    }
}

int main(void)
{
    size_t rows = 2, cols = 3;
    double m[6] = {1, 2, 3,
                   4, 5, 6};
    m[idx(1, 2, cols)] = 60.0;
    mat_print(m, rows, cols);
    return 0;
}
```

Output:

```
   1.0   2.0   3.0
   4.0   5.0  60.0
```

Why flat wins:

| | `double a[R][C]` | flat `double *a` + `rows, cols` |
|---|---|---|
| dimensions | fixed at compile time (or VLA) | runtime, any size |
| heap allocation | awkward (pointer-to-array types) | `malloc(rows * cols * sizeof(double))` |
| pass to function | must know `C` in the signature | `(a, rows, cols)` |
| reshape / transpose view | not possible | change how you index |
| interop (BLAS, NumPy) | no | yes: this IS the layout they use |
| speed | same | same |

Every matrix library, and NumPy internally, is flat memory plus a shape and strides. You will build a `struct mat { double *data; size_t rows, cols; }` on this foundation.

Python equivalent: `a.reshape(-1)` gives the flat view; `a[i, j]` is `a.flat[i * a.shape[1] + j]`.

## 8. Array decay to pointer

When an array name is used in an expression (other than as the operand of `sizeof` or `&`), it "decays" to a pointer to its first element. In particular, passing an array to a function passes a pointer.

```c
#include <stdio.h>

static void show(double v[], size_t n)   /* `double v[]` here MEANS `double *v` */
{
    printf("  in function: sizeof v = %zu (pointer)\n", sizeof v);
    for (size_t i = 0; i < n; i++) printf("  %g", v[i]);
    printf("\n");
}

int main(void)
{
    double x[5] = {1, 2, 3, 4, 5};
    printf("in main: sizeof x = %zu (array)\n", sizeof x);
    show(x, sizeof x / sizeof x[0]);     /* x decays to &x[0] */
    return 0;
}
```

Output:

```
in main: sizeof x = 40 (array)
  in function: sizeof v = 8 (pointer)
  1  2  3  4  5
```

`-Wsizeof-array-argument` (in `-Wall`) warns when you use `sizeof` on an array parameter. Full treatment of pointers and this equivalence is in `../05_pointers/lesson.md`.

## 9. VLAs (variable-length arrays)

C99 allows an automatic array whose size is a runtime value:

```c
void f(int n)
{
    double tmp[n];                        /* VLA: size decided at runtime, lives on the stack */
    for (int i = 0; i < n; i++) tmp[i] = 0.0;
}
```

Discouraged, because:

- The size is not checked. `n = 10000000` overflows the stack with no error message.
- `sizeof` becomes a runtime operation.
- C11 made them optional; some compilers (MSVC) never supported them.
- You cannot initialize them (`double tmp[n] = {0};` is an error).

Use a fixed maximum (`double tmp[MAX_N]`) when the bound is known and small, otherwise `malloc` (chapter 06). Note that `const int N = 10; double a[N];` is a VLA in C (a `const` variable is not a constant expression); use `#define N 10` or `enum { N = 10 };` for true fixed arrays.

## 10. Strings are `char` arrays ending in `'\0'`

C has no string type. A "string" is a `char` array whose contents end with the byte 0 (written `'\0'`, the NUL terminator). Every string function finds the end by scanning for that byte.

```c
#include <stdio.h>
#include <string.h>
int main(void)
{
    char s[6] = "hello";                  /* 5 letters + '\0' = 6 bytes. Size 5 would be an error */
    char t[] = "hello";                   /* size inferred: 6 */
    char u[10] = "hi";                    /* {'h','i','\0','\0',...}: rest zero */
    printf("%s %zu %zu\n", s, strlen(s), sizeof s);   /* hello 5 6 */
    printf("%s %zu %zu\n", u, strlen(u), sizeof u);   /* hi 2 10 */
    s[0] = 'j';                           /* arrays are writable */
    printf("%s\n", s);                    /* jello */
    printf("byte values: ");
    for (size_t i = 0; i < sizeof t; i++) printf("%d ", t[i]);   /* 104 101 108 108 111 0 */
    printf("\n");
    return 0;
}
```

Memory:

```
t:  +-----+-----+-----+-----+-----+-----+
    | 'h' | 'e' | 'l' | 'l' | 'o' | \0  |
    | 104 | 101 | 108 | 108 | 111 |  0  |
    +-----+-----+-----+-----+-----+-----+
    strlen(t) = 5   sizeof t = 6
```

`strlen` counts bytes before the terminator (O(n), it scans). `sizeof` gives the array size including the terminator. Always allocate `strlen + 1`. A `char` array without a `'\0'` inside it is NOT a string; passing it to `printf("%s")` reads until it happens to find a zero byte somewhere in memory: UB.

Python equivalent: Python `str` knows its length and is immutable; C strings know nothing and are mutable bytes. Closest analogue is `bytes`, without the length.

## 11. String literals are read-only

`"hello"` in source is stored in a read-only section of the executable. Assigning it to a `char*` gives you a pointer to that memory. Writing through it is UB (on macOS: crash with `bus error`).

```c
char *p = "hello";                        /* p points to read-only memory */
/* p[0] = 'j';  <- UB: crashes on this platform */
const char *q = "hello";                  /* correct: the const documents the fact */
char buf[] = "hello";                     /* copies the literal into a writable array */
buf[0] = 'j';                             /* fine */
```

Always write `const char *` for pointers to literals. Clang's `-Wwrite-strings` (not in `-Wall` for C) makes the non-const assignment a warning.

## 12. `<string.h>` essentials

| Function | Does | Notes |
|---|---|---|
| `size_t strlen(const char *s)` | bytes before `'\0'` | O(n) |
| `char *strcpy(char *dst, const char *src)` | copy including `'\0'` | NO bounds check: dst must be big enough |
| `char *strncpy(char *dst, const char *src, size_t n)` | copy at most n bytes | does NOT add `'\0'` if src is >= n long; pads with zeros if shorter. Trap. |
| `char *strcat(char *dst, const char *src)` | append | NO bounds check |
| `int strcmp(const char *a, const char *b)` | compare | 0 if equal, <0 if a<b, >0 if a>b (byte order) |
| `int strncmp(a, b, n)` | compare first n bytes | |
| `char *strchr(const char *s, int c)` | find first byte c | returns pointer or NULL |
| `char *strrchr(s, c)` | find last byte c | |
| `char *strstr(const char *hay, const char *needle)` | find substring | pointer or NULL |
| `void *memcpy(void *dst, const void *src, size_t n)` | copy n bytes | regions must not overlap; works on any type |
| `void *memmove(dst, src, n)` | copy n bytes, overlap OK | |
| `void *memset(void *p, int byte, size_t n)` | fill n bytes | `memset(a, 0, sizeof a)` zeroes any array |
| `int memcmp(const void *a, const void *b, size_t n)` | compare n bytes | 0 if identical |

```c
#include <stdio.h>
#include <string.h>
int main(void)
{
    char a[32] = "matrix";
    char b[32];
    strcpy(b, a);                         /* b = "matrix" */
    strcat(b, "_mul");                    /* b = "matrix_mul" (fits in 32) */
    printf("%s %s cmp=%d\n", a, b, strcmp(a, b));   /* matrix matrix_mul cmp=-1 (or negative) */
    printf("%d %d\n", strcmp("abc", "abd") < 0, strcmp("b", "abc") > 0);   /* 1 1 */
    char *us = strchr(b, '_');
    printf("after underscore: %s, at index %ld\n", us, us - b);   /* _mul, 6 */
    printf("strstr: %s\n", strstr(b, "x_m"));                     /* x_mul */
    double v[4] = {1, 2, 3, 4}, w[4];
    memcpy(w, v, sizeof v);               /* copy 32 bytes: the fast way to copy arrays */
    memset(v, 0, sizeof v);               /* zero 32 bytes */
    printf("w[3]=%g v[3]=%g equal=%d\n", w[3], v[3], memcmp(v, w, sizeof v) == 0);
    return 0;
}
```

Output:

```
matrix matrix_mul cmp=-1
1 1
after underscore: _mul, at index 6
strstr: x_mul
w[3]=4 v[3]=0 equal=0
```

Note: `strcmp` returns 0 for EQUAL. `if (strcmp(a, b))` is true when they DIFFER. Write `if (strcmp(a, b) == 0)`. Never compare strings with `==`; that compares addresses.

`memset` with a nonzero value sets BYTES, so `memset(doubles, 1, n)` does not make the doubles equal to 1.0. Only 0 (and for `unsigned char` arrays, any byte) is meaningful.

## 13. Buffer overflow: the #1 C bug

Writing more bytes than the destination holds. `strcpy`, `strcat`, `sprintf`, `gets`, `scanf("%s")` all do it silently.

```c
char name[8];
strcpy(name, "Schrodinger");             /* 12 bytes into 8: overflow. UB. */
```

```
name:  +---+---+---+---+---+---+---+---+ - - +---+---+---+---+
       | S | c | h | r | o | d | i | n |  g  | e | r | \0|
       +---+---+---+---+---+---+---+---+ - - +---+---+---+---+
       |<------- 8 bytes owned ------->|<-- 4 bytes clobbered:
                                          whatever lives after `name`
```

What gets clobbered is other variables, saved registers, or the return address; the last is how most historical security exploits worked. With ASan: `stack-buffer-overflow WRITE of size 12`. Without it: nothing, until something strange happens.

Defenses:

1. Never use `strcpy`, `strcat`, `sprintf`, `gets`, unbounded `scanf("%s")`.
2. Use `snprintf` (next section) for building strings; it always terminates and never overflows.
3. Use `strncpy` only if you then force `dst[n-1] = '\0'`, or better, `snprintf(dst, n, "%s", src)`.
4. Use `fgets(buf, sizeof buf, stdin)` for input, never `gets`.
5. Test with `-fsanitize=address`.

## 14. `snprintf`: the safe way to build strings

`int snprintf(char *buf, size_t size, const char *fmt, ...)` formats like `printf` but writes at most `size - 1` characters into `buf` and ALWAYS appends `'\0'` (if `size > 0`). It returns the number of characters that WOULD have been written, so `ret >= size` means truncation occurred.

```c
#include <stdio.h>
int main(void)
{
    char buf[16];
    int n = snprintf(buf, sizeof buf, "loss=%.4f step=%d", 0.123456, 42);
    printf("[%s] wanted %d chars, truncated=%d\n", buf, n, n >= (int)sizeof buf);
    /* [loss=0.1235 ste] wanted 19 chars, truncated=1 */

    char path[64];
    snprintf(path, sizeof path, "out/frame_%04d.ppm", 7);   /* out/frame_0007.ppm */
    printf("%s\n", path);

    /* Appending safely: track the offset. */
    char line[64];
    int off = 0;
    double row[3] = {1.5, -2.25, 3.0};
    for (int i = 0; i < 3; i++)
        off += snprintf(line + off, sizeof line - off, "%s%g", i ? "," : "", row[i]);
    printf("%s\n", line);                 /* 1.5,-2.25,3 */
    return 0;
}
```

The appending loop is only correct while `off < sizeof line`; add a check in real code. `snprintf(buf, 0, fmt, ...)` writes nothing and returns the needed length: use it to size a buffer before `malloc`.

## 15. Strings to numbers: `atoi`, `strtol`, `strtod`

`atoi("42")` is simple and useless for error handling: `atoi("abc")` returns 0, `atoi("42abc")` returns 42, overflow is UB. Use `strtol`/`strtod`, which report where parsing stopped.

```c
long   strtol(const char *s, char **endptr, int base);
double strtod(const char *s, char **endptr);
```

Both skip leading whitespace, parse as much as they can, and set `*endptr` to the first unparsed character. If `*endptr == s`, nothing was parsed. `strtol` sets `errno = ERANGE` on overflow.

```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int main(void)
{
    const char *inputs[] = {"42", "  -17xyz", "abc", "3.75", "1e3", "99999999999999999999"};
    for (int i = 0; i < 6; i++) {
        const char *s = inputs[i];
        char *end;
        errno = 0;
        long v = strtol(s, &end, 10);
        if (end == s)            printf("%-22s -> not a number\n", s);
        else if (errno == ERANGE) printf("%-22s -> out of range\n", s);
        else if (*end != '\0')   printf("%-22s -> %ld, then junk \"%s\"\n", s, v, end);
        else                     printf("%-22s -> %ld\n", s, v);
    }
    char *end;
    double d = strtod("3.75e2 rest", &end);
    printf("strtod: %g, stopped at \"%s\"\n", d, end);   /* 375, stopped at " rest" */
    printf("hex via base 16: %ld\n", strtol("ff", NULL, 16));   /* 255 */
    return 0;
}
```

Output:

```
42                     -> 42
  -17xyz               -> -17, then junk "xyz"
abc                    -> not a number
3.75                   -> 3, then junk ".75"
1e3                    -> 1, then junk "e3"
99999999999999999999   -> out of range
strtod: 375, stopped at " rest"
hex via base 16: 255
```

A robust "parse a whole string as an int" checks all three: `end != s`, `errno != ERANGE`, `*end == '\0'` (optionally allowing trailing whitespace).

Python equivalent: `int(s)` and `float(s)`, which raise `ValueError`. `strtod` accepts what Python `float()` accepts plus a few extras (`0x1p3`, `inf`, `nan`).

## 16. Splitting a string: `strtok` and the manual loop

`strtok(str, delims)` returns successive tokens separated by any of the delimiter bytes, MODIFYING the string (it writes `'\0'` over each delimiter) and keeping hidden static state between calls.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void)
{
    char line[] = "1.5, 2.25 ,3,,4";      /* must be writable: strtok writes into it */
    double vals[8];
    int n = 0;
    for (char *tok = strtok(line, ", "); tok != NULL && n < 8; tok = strtok(NULL, ", "))
        vals[n++] = strtod(tok, NULL);
    printf("%d values:", n);
    for (int i = 0; i < n; i++) printf(" %g", vals[i]);
    printf("\n");                         /* 4 values: 1.5 2.25 3 4  (the empty field vanished) */
    return 0;
}
```

Pitfalls of `strtok`:

- Destroys the input (cannot reuse it). Cannot be used on a string literal or `const` data.
- Collapses consecutive delimiters: `"3,,4"` yields `3` and `4`, losing the empty field. Bad for CSV.
- Hidden static state: not reentrant, not thread-safe, cannot tokenize two strings at once. `strtok_r` (POSIX) fixes this.

Manual scanning loop, which handles empty fields and does not modify the input:

```c
#include <stdio.h>
#include <stdlib.h>
static int parse_csv_doubles(const char *s, double *out, int max)
{
    int n = 0;
    while (n < max) {
        char *end;
        out[n++] = strtod(s, &end);       /* parses "" as 0 with end == s */
        s = end;
        while (*s == ' ') s++;            /* skip spaces */
        if (*s != ',') break;             /* end of input or junk */
        s++;                              /* skip the comma */
    }
    return n;
}
int main(void)
{
    double v[8];
    int n = parse_csv_doubles("1.5, 2.25 ,3,,4", v, 8);
    printf("%d values:", n);
    for (int i = 0; i < n; i++) printf(" %g", v[i]);
    printf("\n");                         /* 5 values: 1.5 2.25 3 0 4 */
    return 0;
}
```

Extend this with an `end == s` check to distinguish "empty field" from "0", and you have a CSV row parser for your dataset loader.

## 17. `char` is an integer

A `char` is a 1-byte integer. `'a'` is the number 97. Arithmetic on characters is arithmetic on small integers. `<ctype.h>` provides classification and conversion; they take and return `int` and expect a value in `0..255` or `EOF`, so cast to `unsigned char` first.

```
ASCII (decimal):  '0'..'9' = 48..57    'A'..'Z' = 65..90    'a'..'z' = 97..122
                  ' ' = 32   '\n' = 10   '\t' = 9   '\0' = 0
```

```c
#include <stdio.h>
#include <ctype.h>
int main(void)
{
    char c = 'a';
    printf("%c %d %c %c\n", c, c, c + 1, c - 32);        /* a 97 b A */
    printf("digit value of '7': %d\n", '7' - '0');        /* 7 */
    printf("%d %d %d\n", isdigit('7'), isalpha('x'), isspace('\n'));   /* nonzero for true */
    printf("%c %c\n", toupper('q'), tolower('Q'));        /* Q q */
    const char *s = "Mixed Case 123";
    char out[32];
    int i = 0;
    for (; s[i] != '\0'; i++) out[i] = (char)toupper((unsigned char)s[i]);
    out[i] = '\0';
    printf("%s\n", out);                                  /* MIXED CASE 123 */
    return 0;
}
```

`isdigit` and friends return nonzero (not necessarily 1) for true; use them as conditions, do not compare to 1.

Uses in a tokenizer: `isspace` to split on whitespace, `isalpha`/`isdigit` for GPT-2-style pre-tokenization classes, `c - '0'` to parse digits by hand.

## 18. UTF-8 is bytes

C strings are byte sequences. UTF-8 encodes one Unicode code point as 1 to 4 bytes. `strlen` counts BYTES, not characters. Indexing `s[i]` gives a byte, which may be the middle of a multi-byte character.

```c
#include <stdio.h>
#include <string.h>
int main(void)
{
    const char *s = "héllo";              /* é is 2 bytes in UTF-8: 0xC3 0xA9 */
    printf("strlen=%zu\n", strlen(s));    /* 6, not 5 */
    for (size_t i = 0; s[i]; i++) printf("%02x ", (unsigned char)s[i]);
    printf("\n");                         /* 68 c3 a9 6c 6c 6f */
    /* Count code points: a byte starting with bits 10xxxxxx is a continuation byte. */
    int cps = 0;
    for (size_t i = 0; s[i]; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) cps++;
    printf("code points=%d\n", cps);     /* 5 */
    return 0;
}
```

UTF-8 structure:

| First byte pattern | Bytes in sequence | Code point range |
|---|---|---|
| `0xxxxxxx` | 1 | U+0000..U+007F (ASCII) |
| `110xxxxx` | 2 | U+0080..U+07FF |
| `1110xxxx` | 3 | U+0800..U+FFFF |
| `11110xxx` | 4 | U+10000..U+10FFFF |
| `10xxxxxx` | continuation | |

For a BPE tokenizer this is exactly right: GPT-2's tokenizer operates on bytes (256 base tokens) and merges byte pairs; it never needs to know where characters begin. You store text as `unsigned char` arrays and let the merges discover structure.

Python equivalent: Python `str` is code points; `s.encode('utf-8')` gives the bytes a C program sees. `len("héllo")` is 5 in Python and `strlen` is 6 in C.

## 19. Iterating a string byte by byte

Three equivalent idioms; pick one and be consistent.

```c
const char *s = "abc";
for (size_t i = 0; s[i] != '\0'; i++)   putchar(s[i]);   /* index, explicit test */
for (size_t i = 0; s[i]; i++)           putchar(s[i]);   /* index, '\0' is false */
for (const char *p = s; *p; p++)        putchar(*p);     /* pointer walk (chapter 05) */
```

Counting words (a first pre-tokenizer):

```c
#include <stdio.h>
#include <ctype.h>
int main(void)
{
    const char *text = "the quick  brown fox\tjumps";
    int words = 0, in_word = 0;
    for (size_t i = 0; text[i]; i++) {
        int sp = isspace((unsigned char)text[i]);
        if (!sp && !in_word) words++;     /* transition space -> non-space starts a word */
        in_word = !sp;
    }
    printf("%d words\n", words);          /* 5 */
    return 0;
}
```

## Gotchas and undefined behavior

- Reading an uninitialized array element: UB. Initialize with `{0}` or fill explicitly.
- Any index `< 0` or `>= n`: UB, no diagnostic. Use ASan while learning.
- `sizeof` on an array parameter gives the pointer size. Pass the length.
- `int a[n]` with `const int n`: a VLA, not a fixed array. Use `#define` or `enum`.
- `char s[5] = "hello";` compiles (no room for `'\0'`) and `s` is not a string; `printf("%s", s)` reads past it.
- `char *s = "lit"; s[0] = 'x';` writes to read-only memory: crash.
- `strcpy`/`strcat`/`sprintf` into a too-small buffer: overflow.
- `strncpy` does not NUL-terminate when the source fills the buffer.
- `if (strcmp(a, b))` means "if different". `a == b` compares pointers.
- `memset(arr, 1, n)` sets bytes, not elements.
- `memcpy` with overlapping regions: UB. Use `memmove`.
- `strtok` destroys its input and collapses empty fields.
- `atoi` gives 0 for garbage and has UB on overflow; use `strtol` with checks.
- `isdigit(c)` with a negative `char` (byte >= 128 in a signed `char`): UB. Cast to `unsigned char`.
- `strlen` on a UTF-8 string is a byte count.
- Iterating a matrix column-first is correct but slow; keep the last index innermost.
- Forgetting `+ 1` for the terminator when sizing a buffer.
- `sizeof "abc"` is 4, `strlen("abc")` is 3.

## Common mistakes checklist

- [ ] Every array is initialized before it is read.
- [ ] Every function taking an array also takes its length, and every loop bound uses that length.
- [ ] Matrices are flat, indexed with a helper `idx(i, j, cols)`, and loops run row-major.
- [ ] String buffers have room for the terminator (`strlen + 1`).
- [ ] No `strcpy`, `strcat`, `sprintf`, `gets`, or unbounded `%s` in `scanf`.
- [ ] String building uses `snprintf` with `sizeof buf`, and truncation is checked where it matters.
- [ ] String comparison uses `strcmp(...) == 0`.
- [ ] Number parsing uses `strtol`/`strtod` with `endptr` checks.
- [ ] `<ctype.h>` calls cast their argument to `unsigned char`.
- [ ] Pointers to literals are `const char *`.
- [ ] Tested at least once with `-fsanitize=address`.

## You can move on when...

- You can draw the memory layout of `double a[2][3]` and give the flat index of `a[1][2]`.
- You can write `mat_print(const double *m, size_t rows, size_t cols)` with a flat index and correct loop order.
- You can explain why `sizeof v` inside `void f(double v[])` is 8.
- You can build `"frame_0042.ppm"` into a fixed buffer without any possibility of overflow.
- You can parse `"1.5, 2.25 ,3,,4"` into an array of doubles, preserving the empty field, and detect `"abc"` as an error.
- You can state what `strncpy` does when the source is exactly `n` bytes long, and why `snprintf` is preferred.
- You can explain why `strlen("héllo")` is 6 and what that means for a byte-level tokenizer.
