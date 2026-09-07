# Chapter 01 — Hello, Compilation, and Types

## What you'll be able to do after this chapter

- Compile and run a C program from the terminal and understand what each of the four build stages does.
- Print any integer, floating-point, character, string, or pointer value with the correct `printf` specifier and control its width and precision.
- Choose the correct integer and floating-point type for a value and know its exact size on this machine.
- Predict the result of mixed-type arithmetic (integer division, promotion, implicit conversion) and force the result you want with a cast.
- Recognize signed overflow as undefined behavior and unsigned overflow as modular wraparound.
- Read a compiler error message and locate the line and cause.

## Why this matters for ML / numerics / sims

Every numerical program you will write (matrix library, autograd, N-body, FDTD) is a pile of arithmetic on `double` and `float` arrays indexed by integers. In Python, `3 / 2` is `1.5` and `2**100` is a big integer. In C, `3 / 2` is `1`, and `int` is 32 bits wide and silently misbehaves past 2,147,483,647. A wrong type or a forgotten cast in an inner loop produces answers that are wrong by a little (float precision), wrong by a lot (integer division), or garbage (overflow). The tokenizer you will write processes bytes as `unsigned char`; the matrix library will index with `size_t`; the physics sims will run in `double`. You need to know what each of those is before you can write any of them.

## 1. What the compiler actually does

A C source file is text. The CPU executes machine code. Turning one into the other happens in four stages, and `cc` runs all of them for you by default.

```
hello.c  --preprocess-->  hello.i  --compile-->  hello.s  --assemble-->  hello.o  --link-->  hello
 (text)                 (text, #includes         (assembly             (object file:         (executable)
                         pasted in, #defines     text)                 machine code +
                         replaced)                                     unresolved names)
```

| Stage | Tool | Input | Output | What it does |
|---|---|---|---|---|
| Preprocess | `cc -E` | `.c` | `.i` | Textual substitution: pastes `#include` files in, replaces `#define` macros, removes comments. No knowledge of C syntax. |
| Compile | `cc -S` | `.i` | `.s` | Parses C, type-checks, emits assembly for this CPU (arm64 here). This is where warnings and errors come from. |
| Assemble | `cc -c` | `.s` | `.o` | Translates assembly into machine code bytes. Calls to `printf` are left as "unresolved symbol" placeholders. |
| Link | `cc` / `ld` | `.o` + libraries | executable | Glues object files together and resolves placeholders against the C standard library (`libc`) and any `-l` libraries you name. |

You can watch each stage:

```sh
cc -E hello.c | tail -20     # see what the preprocessor produced (thousands of lines from stdio.h)
cc -S hello.c && cat hello.s # arm64 assembly
cc -c hello.c && ls -l hello.o
cc -o hello hello.o          # link only
```

"Undefined symbol" errors come from the linker (you called a function nobody defined, or forgot `-lm`). "Expected ';'" errors come from the compiler. Knowing which stage complained tells you where to look.

Python equivalent: none, really. CPython compiles to bytecode invisibly and runs it immediately. C makes you produce a standalone binary first; that binary contains no interpreter and needs no Python installation.

## 2. `main`, `#include`, and the minimal program

```c
#include <stdio.h>      /* declares printf, scanf, ... */

int main(void)          /* the OS calls this function to start the program */
{
    printf("hello\n"); /* \n = newline; printf does NOT add one for you */
    return 0;          /* 0 means "success" to the OS */
}
```

- `#include <stdio.h>` pastes the text of the header `stdio.h` here. Angle brackets mean "system header"; quotes (`#include "mymath.h"`) mean "look in my directory first".
- `int main(void)` is the entry point. There are two legal signatures: `int main(void)` and `int main(int argc, char *argv[])` (command-line arguments; see `../05_pointers/lesson.md`).
- Statements end with `;`. Blocks are delimited with `{ }`. Whitespace and indentation carry no meaning (unlike Python).
- Every variable has a type declared before use. There is no `None`, no dynamic typing.

Compile and run:

```sh
cc -Wall -Wextra -std=c11 -O2 -o hello hello.c -lm
./hello
```

Output:

```
hello
```

## 3. `printf` format specifiers

`printf(format, args...)` scans `format` and substitutes each `%` conversion with the next argument. The argument's type MUST match the specifier; a mismatch is undefined behavior (it usually prints garbage, sometimes crashes). The compiler warns about mismatches with `-Wall` (`-Wformat`), so read those warnings.

| Specifier | Argument type | Example output |
|---|---|---|
| `%d` / `%i` | `int` | `-42` |
| `%u` | `unsigned int` | `42` |
| `%ld` | `long` | `123456789012` |
| `%lld` | `long long` | `123456789012` |
| `%lu` | `unsigned long` | |
| `%zu` | `size_t` (result of `sizeof`) | `8` |
| `%f` | `double` (float is promoted) | `3.141593` (6 decimals default) |
| `%e` | `double`, scientific | `3.141593e+00` |
| `%g` | `double`, shortest of `%f`/`%e` | `3.14159` |
| `%c` | `int` holding a character | `a` |
| `%s` | `char*` to a NUL-terminated string | `hello` |
| `%p` | `void*` (any pointer, cast to `void*`) | `0x16d1a3b2c` |
| `%x` / `%X` | `unsigned int`, hexadecimal | `1f` / `1F` |
| `%o` | `unsigned int`, octal | `37` |
| `%%` | none | `%` |

Width and precision go between `%` and the letter:

```c
#include <stdio.h>
int main(void)
{
    double pi = 3.14159265358979;
    int n = 42;
    printf("[%d]\n",     n);       /* [42]        */
    printf("[%6d]\n",    n);       /* [    42]    width 6, right-aligned */
    printf("[%-6d]\n",   n);       /* [42    ]    left-aligned */
    printf("[%06d]\n",   n);       /* [000042]    zero-padded */
    printf("[%f]\n",     pi);      /* [3.141593]  */
    printf("[%.2f]\n",   pi);      /* [3.14]      precision 2 */
    printf("[%10.3f]\n", pi);      /* [     3.142] width 10, precision 3 */
    printf("[%e]\n",     pi);      /* [3.141593e+00] */
    printf("[%.3e]\n",   1234.5);  /* [1.234e+03] */
    printf("[%g]\n",     0.00001234); /* [1.234e-05] g switches to e for tiny/huge */
    printf("[%g]\n",     100.0);   /* [100]       g drops trailing zeros */
    printf("[%x]\n",     255);     /* [ff]        */
    printf("[%#x]\n",    255);     /* [0xff]      # adds prefix */
    printf("[%c%c]\n",   'o', 'k');/* [ok]        */
    printf("[%s]\n",     "str");   /* [str]       */
    printf("[%10s]\n",   "str");   /* [       str] */
    printf("[%p]\n",     (void*)&n); /* [0x16b...]  some address */
    return 0;
}
```

For printing a matrix, `%8.3f` (fixed width, 3 decimals) keeps columns aligned. For loss values in training, `%.6e` shows magnitude clearly.

Python equivalent: `f"{pi:10.3f}"` is `%10.3f`; `f"{n:06d}"` is `%06d`; `f"{x:.3e}"` is `%.3e`. The mini-language is nearly identical because Python borrowed it from C.

## 4. `scanf` basics and why it is dangerous

`scanf` is the input counterpart. It takes a format and the ADDRESSES of variables to fill (`&x`, explained fully in `../05_pointers/lesson.md`). It returns the number of items successfully converted.

```c
#include <stdio.h>
int main(void)
{
    int n;
    double x;
    printf("enter an int and a double: ");
    if (scanf("%d %lf", &n, &x) != 2) {   /* note: %lf for double in scanf, %f in printf */
        printf("bad input\n");
        return 1;
    }
    printf("n=%d x=%f\n", n, x);
    return 0;
}
```

Why it is dangerous:

1. `scanf("%s", buf)` reads a word of unbounded length into `buf`. If the user types more characters than `buf` holds, it writes past the end (buffer overflow, `../04_arrays_and_strings/lesson.md`). Always use a width: `scanf("%63s", buf)` for a 64-byte buffer.
2. If the input does not match (`abc` for `%d`), `scanf` stops, leaves `abc` in the input stream, and returns 0. A loop that calls `scanf` again without consuming the bad input spins forever.
3. Forgetting `&` (`scanf("%d", n)`) passes the value of `n` as an address. The compiler warns; the program crashes or corrupts memory.

For real programs, read a whole line with `fgets` and parse it with `strtol`/`strtod` (`../04_arrays_and_strings/lesson.md`). `scanf` is fine for throwaway experiments.

## 5. Integer types and their sizes on this platform

C guarantees only minimum sizes and the ordering `char <= short <= int <= long <= long long`. The actual sizes depend on the platform (arm64 macOS, "LP64" model):

| Type | Bytes here | Bits | Signed range | Unsigned range |
|---|---|---|---|---|
| `char` | 1 | 8 | -128 .. 127 | 0 .. 255 |
| `short` | 2 | 16 | -32,768 .. 32,767 | 0 .. 65,535 |
| `int` | 4 | 32 | -2,147,483,648 .. 2,147,483,647 | 0 .. 4,294,967,295 |
| `long` | 8 | 64 | about -9.2e18 .. 9.2e18 | 0 .. 1.8e19 |
| `long long` | 8 | 64 | same as `long` here | |
| `size_t` | 8 | 64 | (unsigned only) | 0 .. 1.8e19 |

On Windows `long` is 4 bytes. That is why portable code uses `<stdint.h>` names when the width matters: `int8_t`, `uint8_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`. For a tokenizer processing bytes, use `uint8_t` (or `unsigned char`). For token IDs up to 50k, `uint16_t` or `int32_t`. For indices into arrays, `size_t`.

`signed`/`unsigned`: every integer type has both. `int` means `signed int`. Plain `char` may be signed or unsigned depending on the platform (signed on arm64 macOS); when you mean "a byte", write `unsigned char` explicitly.

```c
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
int main(void)
{
    printf("int:  %d .. %d\n", INT_MIN, INT_MAX);
    printf("long: %ld .. %ld\n", LONG_MIN, LONG_MAX);
    printf("uint32_t max: %u\n", UINT32_MAX);
    unsigned char b = 200;
    printf("byte: %u\n", b);
    return 0;
}
```

Output:

```
int:  -2147483648 .. 2147483647
long: -9223372036854775808 .. 9223372036854775807
uint32_t max: 4294967295
byte: 200
```

Python equivalent: Python `int` is arbitrary precision. NumPy dtypes map exactly: `np.int32` is `int32_t`, `np.uint8` is `uint8_t`, `np.int64` is `int64_t`/`long`. A `np.uint8` array behaves exactly like a C `unsigned char` array, wraparound included.

## 6. `float` and `double`

| Type | Bytes | Significant decimal digits | Max | Smallest positive normal |
|---|---|---|---|---|
| `float` | 4 | ~7 | 3.4e38 | 1.2e-38 |
| `double` | 8 | ~16 | 1.8e308 | 2.2e-308 |
| `long double` | 8 on arm64 macOS (same as double) | | | |

Use `double` by default. Use `float` when memory bandwidth matters (large neural-network weight arrays, GPU-style code) and you have checked that 7 digits are enough. Never accumulate a long sum in `float` if you can avoid it: the running total loses the small additions.

```c
#include <stdio.h>
#include <float.h>
int main(void)
{
    float  f = 0.1f;
    double d = 0.1;
    printf("%.20f\n%.20f\n", f, d);
    printf("FLT_EPSILON=%e DBL_EPSILON=%e\n", FLT_EPSILON, DBL_EPSILON);
    printf("0.1+0.2==0.3 ? %d\n", 0.1 + 0.2 == 0.3);
    return 0;
}
```

Output:

```
0.10000000149011611938
0.10000000000000000555
FLT_EPSILON=1.192093e-07 DBL_EPSILON=2.220446e-16
0.1+0.2==0.3 ? 0
```

Never compare floats with `==` for equality; compare `fabs(a - b) < tol`. Same as NumPy's `np.isclose`.

Python equivalent: Python `float` is C `double`. `np.float32` is C `float`. `torch.float32` is the default PyTorch dtype and is C `float`.

## 7. `sizeof`

`sizeof(x)` or `sizeof(type)` yields the size in bytes as a `size_t`. It is evaluated at compile time (except for VLAs). Print it with `%zu`.

```c
#include <stdio.h>
int main(void)
{
    int a[10];
    printf("sizeof(int)=%zu sizeof(double)=%zu sizeof(a)=%zu\n",
           sizeof(int), sizeof(double), sizeof a);    /* parentheses optional for variables */
    printf("elements in a: %zu\n", sizeof a / sizeof a[0]);
    return 0;
}
```

Output:

```
sizeof(int)=4 sizeof(double)=8 sizeof(a)=40
elements in a: 10
```

You will use `sizeof` in every `malloc` call: `malloc(n * sizeof(double))`.

Python equivalent: `np.dtype('float64').itemsize` is 8; `arr.nbytes` is `sizeof(arr)` for a fixed array.

## 8. Literals and suffixes

A literal is a constant written in source. Its type is determined by its spelling:

| Literal | Type | Notes |
|---|---|---|
| `10` | `int` | |
| `10L` | `long` | |
| `10LL` | `long long` | |
| `10U` | `unsigned int` | combine: `10UL`, `10ULL` |
| `0x1F` | `int` | hexadecimal, = 31 |
| `017` | `int` | OCTAL (leading zero!), = 15. Trap. |
| `1.0` | `double` | any literal with `.` or `e` is double |
| `1.0f` | `float` | |
| `1e-3` | `double` | = 0.001 |
| `'a'` | `int` (not `char`!) | value 97 (ASCII) |
| `'\n'` | `int` | value 10 |
| `"str"` | `char[4]` | array of 4 chars: `s t r \0`; decays to `const char*`-like |

Why this matters: `1 / 3` is integer division (result 0). `1.0 / 3` is double division. `float x = 0.1;` converts the double 0.1 to float (fine, but `0.1f` avoids the conversion). `2147483648` does not fit in `int`, so the compiler makes it `long`.

## 9. Integer division vs float division

The operator `/` looks at the types of its operands. If both are integers, it performs integer division (truncates toward zero). If either is floating, it performs floating division. `%` is the remainder and works only on integers.

```c
#include <stdio.h>
int main(void)
{
    printf("%d\n",  7 / 2);        /* 3   integer division */
    printf("%d\n",  -7 / 2);       /* -3  truncates toward zero (Python gives -4: floor) */
    printf("%d\n",  7 % 2);        /* 1   */
    printf("%d\n",  -7 % 2);       /* -1  sign follows the dividend (Python gives 1) */
    printf("%f\n",  7 / 2.0);      /* 3.500000 */
    printf("%f\n",  (double)7 / 2);/* 3.500000 */
    int a = 7, b = 2;
    printf("%f\n",  a / b);        /* WRONG: passes int 3 to %f. UB. -Wall warns. */
    printf("%f\n",  (double)a / b);/* 3.500000 */
    printf("%f\n",  (double)(a / b));/* 3.000000  cast happens AFTER the int division */
    return 0;
}
```

The last two lines are the single most common numerics bug for Python programmers: `mean = sum / n` where both are `int`. Cast one operand before dividing.

Python equivalent: Python `//` floors (`-7 // 2 == -4`); C `/` truncates (`-7 / 2 == -3`). Python `%` has the sign of the divisor; C `%` has the sign of the dividend. They agree for non-negative operands.

## 10. Implicit conversions and integer promotion

When operands of a binary operator have different types, C converts them to a common type before computing ("usual arithmetic conversions"):

1. Anything smaller than `int` (`char`, `short`, `_Bool`) is promoted to `int` (integer promotion).
2. If either operand is `long double`, `double`, or `float`, the other is converted to that floating type (`float + double -> double`).
3. Otherwise both are integers. The one with lower "rank" is converted to the higher (`int + long -> long`).
4. If one is signed and the other unsigned of the same rank, the SIGNED one becomes UNSIGNED. This is the dangerous rule.

```c
#include <stdio.h>
int main(void)
{
    unsigned char a = 250, b = 10;
    printf("%d\n", a + b);             /* 260: both promoted to int first, no wrap */
    unsigned char c = a + b;
    printf("%u\n", c);                 /* 4:   storing 260 into 8 bits wraps mod 256 */

    int i = -1;
    unsigned int u = 1;
    printf("%d\n", i < u);             /* 0 (false!): -1 is converted to unsigned 4294967295 */

    double d = 3.99;
    int t = d;                         /* implicit double->int truncates: t = 3 */
    printf("%d\n", t);

    float f = 16777217;                /* 2^24 + 1 cannot be represented in float */
    printf("%.0f\n", f);               /* 16777216 */
    return 0;
}
```

Output:

```
260
4
0
3
16777216
```

The `-1 < 1u` being false is a classic. `-Wall -Wextra` warns about signed/unsigned comparison (`-Wsign-compare`). Do not silence the warning; fix the types.

## 11. Explicit casts

`(type)expr` converts `expr` to `type`. Use casts to:

- Force floating division: `(double)sum / n`.
- Truncate deliberately: `(int)floor(x)`, `(int)(x + 0.5)` for rounding positive x.
- Silence a deliberate narrowing where you have checked the range: `(uint8_t)byte_value`.
- Convert `void*` from `malloc` (not required in C, required in C++).

Do not use casts to make the compiler stop complaining about something you do not understand. A cast tells the compiler "trust me"; if you are wrong, it will not warn you again.

```c
double x = 2.7;
int n1 = (int)x;          /* 2  truncation toward zero */
int n2 = (int)(-2.7);     /* -2 */
long big = (long)INT_MAX + 1;   /* correct: cast before the add, so the add is done in long */
long bad = (long)(INT_MAX + 1); /* UB: INT_MAX + 1 overflows int BEFORE the cast */
```

## 12. Integer overflow: signed is UB, unsigned wraps

- Unsigned arithmetic is defined to be modulo 2^N. `UINT_MAX + 1u == 0u`. `0u - 1u == UINT_MAX`. This is guaranteed and useful (hashing, checksums, RNGs).
- Signed overflow is UNDEFINED BEHAVIOR. `INT_MAX + 1` is not "wraps to INT_MIN"; it is "the program has no meaning". With `-O2` the compiler assumes it never happens and optimizes accordingly, so `if (x + 1 < x)` may be deleted entirely because for a signed `x` it "cannot" be true.

```c
#include <stdio.h>
#include <limits.h>
int main(void)
{
    unsigned int u = UINT_MAX;
    u = u + 1;
    printf("%u\n", u);                 /* 0, defined */

    int n = INT_MAX;
    /* n = n + 1;   <- UB. Do not do this. At -O0 you may see INT_MIN; at -O2 anything. */

    /* Safe check before adding: */
    int add = 5;
    if (add > 0 && n > INT_MAX - add) printf("would overflow\n");
    else n += add;

    /* Common real bug: computing a size */
    int rows = 100000, cols = 100000;
    long elems_bad  = rows * cols;          /* int * int overflows BEFORE widening: UB */
    long elems_good = (long)rows * cols;    /* 10000000000 */
    printf("%ld %ld\n", elems_bad, elems_good);
    return 0;
}
```

With `-fsanitize=undefined` clang inserts runtime checks and prints `runtime error: signed integer overflow` at the exact line. Use it while learning.

Python equivalent: none; Python ints never overflow. NumPy `np.int32` arrays wrap silently (like C unsigned, but for signed too), which is a NumPy choice, not a C guarantee.

## 13. `const`, `#define`, and comments

Two ways to name a constant:

```c
#define N_ITERS 1000            /* preprocessor: textual replacement, no type, no scope */
const double LEARNING_RATE = 0.01; /* typed, scoped, visible in the debugger */
```

- `#define` happens before compilation. `N_ITERS` is replaced by `1000` everywhere. No `;` at the end (a `;` becomes part of the replacement, a classic bug). Convention: ALL_CAPS.
- `const` declares a variable that cannot be assigned after initialization. The compiler enforces it. In C (unlike C++), a `const int` is not a "constant expression", so `const int N = 10; int a[N];` makes `a` a VLA (see `../04_arrays_and_strings/lesson.md`). Use `#define` or `enum { N = 10 };` for array sizes.
- Comments: `/* ... */` (C89, can span lines) and `// ...` (C99+, to end of line). Both are removed by the preprocessor.

```c
#define SQUARE(x) ((x) * (x))   /* function-like macro: parenthesize EVERYTHING */
int y = SQUARE(3 + 1);          /* -> ((3 + 1) * (3 + 1)) = 16. Without parens: 3 + 1 * 3 + 1 = 7 */
```

## 14. `return 0` and exit codes

`main` returns an `int` to the operating system. `0` = success, nonzero = failure (the meaning of specific values is yours to define; 1 is the common "generic error"). The shell stores it in `$?`.

```c
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    if (1 + 1 != 2) {
        fprintf(stderr, "arithmetic is broken\n"); /* stderr: error channel, separate from stdout */
        return 1;
    }
    /* exit(2); also works from anywhere, not just main */
    return 0;
}
```

```sh
./prog; echo $?      # prints 0
./prog > out.txt     # stdout goes to the file, stderr still shows on the terminal
```

`EXIT_SUCCESS` and `EXIT_FAILURE` from `<stdlib.h>` are 0 and 1. Scripts that drive your simulations will check `$?`, so return nonzero on failure.

## 15. Compiler flags

```sh
cc -Wall -Wextra -std=c11 -O2 -o NAME file.c -lm
```

| Flag | Meaning |
|---|---|
| `-Wall` | Enable the common warning set (format mismatches, unused variables, uninitialized use, ...). |
| `-Wextra` | More warnings: unused parameters, signed/unsigned comparison, missing field initializers. |
| `-Werror` | Turn warnings into errors. Good discipline. |
| `-std=c11` | Use the C11 standard (no GNU extensions unless you ask). |
| `-O0` | No optimization. Fast compile, slow code, variables stay in memory so the debugger sees them. |
| `-O2` | Standard optimization. 2x-10x faster numerics. UB bugs become visible here. |
| `-O3` | More aggressive (vectorization). Sometimes faster, sometimes not. |
| `-g` | Include debug symbols so `lldb` can show source lines. Combine with `-O0` when debugging. |
| `-o NAME` | Output filename (default is `a.out`). |
| `-lm` | Link the math library (`sin`, `sqrt`, `exp`, ...). On macOS it is part of libc, but keep it for portability to Linux where it is required. |
| `-fsanitize=address,undefined` | Insert runtime checks for memory errors and UB. Slower; use in testing. |
| `-c` | Compile to `.o` only, do not link. |
| `-E` | Preprocess only. |

Compile with `-O0 -g` while developing, `-O2` when measuring speed. Warnings are bugs you have not met yet; keep the build at zero warnings.

## 16. Reading compiler error messages

```c
#include <stdio.h>
int main(void)
{
    int x = 5
    printf("%d\n", x);
    return 0;
}
```

```
err.c:4:14: error: expected ';' after expression
    4 |     int x = 5
      |              ^
      |              ;
1 error generated.
```

Format: `file:line:column: severity: message`. Then the source line and a caret pointing at the location. Rules:

1. Fix the FIRST error, recompile. Later errors are often cascades from the first.
2. The reported line may be one AFTER the real mistake (a missing `;` is noticed on the next token).
3. `warning: implicit declaration of function 'foo'`: you called a function without a prototype; you forgot an `#include` or misspelled the name.
4. `error: use of undeclared identifier 'x'`: typo, or `x` is declared in another scope.
5. `Undefined symbols for architecture arm64: "_sqrt"`: linker error, missing `-lm` (on Linux) or a function you declared but never defined.
6. `warning: format specifies type 'int' but the argument has type 'double'`: a `printf` mismatch. Fix the specifier or the argument.

## Gotchas and undefined behavior

- `printf("%d", 3.0)` and `printf("%f", 3)`: format/argument mismatch is UB. The compiler warns; do not ignore it.
- `printf("%ld", sizeof(x))` is wrong on platforms where `size_t != long`; use `%zu`.
- `int n = 7 / 2 * 2.0;` is `6.0` truncated to `6`, not `7`: the integer division happens first (left to right).
- Leading zero makes an octal literal: `010 == 8`.
- `'a'` is an `int`; `"a"` is a two-byte array. Not interchangeable.
- `char` may be signed. Storing byte `0xFF` in a `char` and comparing `== 0xFF` fails (it is `-1`). Use `unsigned char` for bytes.
- Signed overflow is UB even in intermediate results: `(long)(a * b)` with `int a, b` overflows before the cast.
- `-1 < 1u` is false. Mixing signed and unsigned in comparisons or loop bounds silently converts the signed value.
- `float f = 1e40;` overflows to `inf`. `double` division by zero gives `inf` or `nan` (defined by IEEE, not UB). Integer division by zero IS UB (usually a crash).
- Uninitialized local variables hold garbage: `int sum; sum += x;` is UB. Always initialize: `int sum = 0;`.
- `#define N 10;` (with semicolon) expands to `10;` and breaks `int a[N];`.
- `scanf("%d", n)` without `&` is a crash. `scanf("%f", &d)` with `double d` is wrong; use `%lf`.

## Common mistakes checklist

- [ ] Did every `printf` specifier match its argument type? (`-Wall` tells you.)
- [ ] Did you use `%zu` for `sizeof` and `size_t`, `%ld` for `long`, `%u` for unsigned?
- [ ] Is any division between two integers where you wanted a fraction? Cast one operand.
- [ ] Is any product of two `int`s stored in a `long`? Cast before multiplying.
- [ ] Are all local variables initialized before first read?
- [ ] Is `main` returning `0` on success and nonzero on failure?
- [ ] Does the program compile with zero warnings under `-Wall -Wextra`?
- [ ] Did you use `unsigned char` (or `uint8_t`) for raw bytes?
- [ ] Are you comparing floats with `==`?
- [ ] Did you write `0.1f` for a float literal, or accept the double-to-float conversion knowingly?

## You can move on when...

- You can explain, in one sentence each, what the preprocessor, compiler, assembler, and linker produce.
- You can print an `int`, `long`, `size_t`, `double` (fixed and scientific, 3 decimals, width 12), `char`, string, and pointer without checking a table.
- You can state the byte size of `char`, `short`, `int`, `long`, `float`, `double`, and a pointer on this machine.
- Given `int a = 7, b = 2;`, you can predict `a / b`, `a % b`, `(double)a / b`, and `(double)(a / b)`.
- You can explain why `-1 < 1u` is false and why `INT_MAX + 1` is not "just INT_MIN".
- You can compile with `-Wall -Wextra`, get a warning, and fix the cause rather than the symptom.
- You can check the exit code of a program from the shell.
