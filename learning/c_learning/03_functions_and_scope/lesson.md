# Chapter 03 — Functions and Scope

## What you'll be able to do after this chapter

- Declare, define, and call functions; explain why a prototype must appear before the first call.
- Predict what a function can and cannot change in the caller, given that arguments are copied.
- Write recursive functions, know when recursion is exponential, and know how deep the stack goes before it overflows.
- Use `static` for persistent locals and for file-private functions; know why globals are avoided.
- Draw the call stack for a chain of calls and explain what a stack frame holds.
- Split a program into `mymath.h` + `mymath.c` + `main.c` with include guards and compile it as multiple files.
- Use `<math.h>` and name functions in the `struct_verb` convention.

## Why this matters for ML / numerics / sims

A matrix library is a set of functions: `mat_alloc`, `mat_mul`, `mat_transpose`, `mat_free`. An ODE solver is a function that takes a function (the right-hand side). Backpropagation is recursion over a graph. Every one of these is a function with a header declaring it and a `.c` file defining it, and every one of them lives or dies on the by-value rule: a `double` argument is copied, a `double*` argument gives the callee the caller's memory. Getting scope and lifetime right is also what prevents the two bugs that will otherwise eat your first week: returning a pointer to a local (garbage) and forgetting that a `static` local persists between calls (stale state).

## 1. Declaration vs definition

A **declaration** tells the compiler a function's name, return type, and parameter types. A **definition** additionally supplies the body. There can be many declarations of a function; exactly one definition across the whole program.

```c
double square(double x);          /* declaration (also called a prototype) */

double square(double x)           /* definition */
{
    return x * x;
}
```

The compiler processes a file top to bottom. When it meets a call `square(3.0)`, it must already know the signature to check argument types and generate the right code. If it has not seen a declaration, C11 makes that an error (`call to undeclared function`). Older C silently assumed `int square()` and produced wrong code for `double`. Always declare before use.

```c
#include <stdio.h>

double cube(double x);            /* prototype: now main can call it */

int main(void)
{
    printf("%g\n", cube(3.0));    /* 27 */
    return 0;
}

double cube(double x)             /* definition after main is fine, given the prototype */
{
    return x * x * x;
}
```

Two conventions: define helpers above `main` (no prototypes needed, but the file reads bottom-up), or put prototypes at the top (or in a header) and define in any order. Larger programs use headers.

Python equivalent: `def` both declares and defines; Python resolves names at call time so order within a module rarely matters. C resolves at compile time and order matters.

## 2. Prototypes and `void`

A prototype lists parameter types. Names are optional in the prototype but make it self-documenting.

```c
double dot(const double *a, const double *b, int n);   /* names help the reader */
double dot(const double *, const double *, int);       /* legal, less clear */
```

`void` appears in two roles:

- As the return type: the function returns nothing. `return;` (no value) exits early.
- As the parameter list: `int f(void)` means "takes no arguments". `int f()` in C means "takes an unspecified number of arguments" (an obsolescent form). Always write `(void)`.

```c
void print_vec(const double *v, int n)
{
    if (n <= 0) return;                   /* early exit, nothing to print */
    for (int i = 0; i < n; i++) printf("%g ", v[i]);
    printf("\n");
}
```

## 3. Parameters are passed BY VALUE

When you call `f(x)`, the callee receives a COPY of `x`. Modifying the parameter inside `f` does not touch the caller's `x`.

```c
#include <stdio.h>

void try_to_double(int n)
{
    n = n * 2;                            /* modifies the local copy only */
    printf("  inside: n=%d\n", n);
}

int main(void)
{
    int n = 21;
    try_to_double(n);
    printf("  after:  n=%d\n", n);        /* still 21 */
    return 0;
}
```

Output:

```
  inside: n=42
  after:  n=21
```

The same holds for `double`, `char`, structs (the whole struct is copied), and pointers (the pointer value, an address, is copied).

**Arrays are the exception, in appearance.** You cannot pass an array by value. When you write `f(arr)`, the array "decays" to a pointer to its first element, and THAT pointer is copied. The callee receives the address of the caller's data and can modify it.

```c
#include <stdio.h>

void scale(double *v, int n, double k)    /* v is a copy of the ADDRESS */
{
    for (int i = 0; i < n; i++) v[i] *= k; /* writes to the caller's memory */
}

int main(void)
{
    double x[3] = {1.0, 2.0, 3.0};
    scale(x, 3, 10.0);
    printf("%g %g %g\n", x[0], x[1], x[2]);   /* 10 20 30 */
    return 0;
}
```

So: scalars are copied (callee cannot change them), arrays are effectively shared (callee can change the elements). The pointer itself is still a copy: `v = NULL;` inside `scale` does not affect `x` in `main`. To let a function change a caller's scalar, pass its address (`../05_pointers/lesson.md`).

Also: a `double *v` parameter does not know the array's length. You must pass `n` separately. There is no `len(v)`.

| What you pass | What the callee gets | Callee can modify caller's data? |
|---|---|---|
| `int`, `double`, `char` | a copy of the value | no |
| `struct point` | a copy of the whole struct | no |
| `double arr[10]` | a `double*` to element 0 | yes (the elements) |
| `double *p` | a copy of the address | yes (what it points to), no (the pointer itself) |

Python equivalent: Python passes object references; mutating a list inside a function changes the caller's list, rebinding the name does not. C arrays behave like Python lists in this respect; C scalars behave like Python ints (which are immutable anyway).

## 4. Return values

A function returns at most one value with `return expr;`. The value is converted to the declared return type. A non-`void` function that falls off the end without `return` has an undefined return value (UB to use it; `-Wall` warns "control reaches end of non-void function").

To return several values: return a `struct`, or write results through pointer out-parameters (`../05_pointers/lesson.md`).

```c
#include <stdio.h>

struct minmax { double min, max; };

struct minmax vec_minmax(const double *v, int n)
{
    struct minmax r = { v[0], v[0] };
    for (int i = 1; i < n; i++) {
        if (v[i] < r.min) r.min = v[i];
        if (v[i] > r.max) r.max = v[i];
    }
    return r;                             /* the struct is copied out */
}

int main(void)
{
    double d[] = {3.0, -1.0, 7.5, 2.0};
    struct minmax mm = vec_minmax(d, 4);
    printf("min=%g max=%g\n", mm.min, mm.max);
    return 0;
}
```

Output:

```
min=-1 max=7.5
```

Python equivalent: `return a, b` builds a tuple; C uses a struct.

## 5. Recursion

A function may call itself. Each call gets a fresh set of local variables on the stack.

```c
#include <stdio.h>

long factorial(int n)
{
    if (n <= 1) return 1;                 /* base case: mandatory */
    return n * factorial(n - 1);          /* recursive case */
}

long fib_naive(int n)
{
    if (n < 2) return n;
    return fib_naive(n - 1) + fib_naive(n - 2);
}

long fib_iter(int n)
{
    long a = 0, b = 1;
    for (int i = 0; i < n; i++) { long t = a + b; a = b; b = t; }
    return a;
}

int main(void)
{
    printf("20! = %ld\n", factorial(20));         /* 2432902008176640000, fits in long */
    printf("fib(30) = %ld  %ld\n", fib_naive(30), fib_iter(30));
    return 0;
}
```

Output:

```
20! = 2432902008176640000
fib(30) = 832040  832040
```

**Why naive fib is exponential**: `fib_naive(n)` calls `fib_naive(n-1)` and `fib_naive(n-2)`, each of which recomputes overlapping subproblems. The call tree has about 1.6^n nodes; `fib_naive(40)` makes over 300 million calls. The iterative version is O(n). The fix is either iteration or memoization (store results in an array).

**Stack depth**: each active call occupies a stack frame (the parameters, locals, return address; typically 32-200 bytes). The default main-thread stack on macOS is 8 MB. A recursion 100,000 deep with a 100-byte frame is 10 MB: stack overflow, which manifests as a segmentation fault with no useful message. `factorial(21)` also overflows the `long` (silently wrong; signed overflow is UB). Recursion is fine for tree/graph traversals of bounded depth (autograd backward pass, quicksort partitions); use loops for anything that could recurse millions of times.

Python equivalent: Python's default recursion limit is 1000 and raises `RecursionError`. C has no such check; it just crashes.

## 6. `static` local variables

A local variable declared `static` is initialized once (at program start) and keeps its value between calls. It lives for the whole program but is visible only inside the function.

```c
#include <stdio.h>

int next_id(void)
{
    static int counter = 0;               /* initialized once, not on each call */
    counter++;
    return counter;
}

int main(void)
{
    printf("%d %d %d\n", next_id(), next_id(), next_id());
    return 0;
}
```

Output (the order of evaluation of the three calls is unspecified, but the set of values is 1, 2, 3):

```
1 2 3
```

Uses: counters, caches, one-time initialization flags, a simple random-number generator's state. Downsides: the function is no longer reentrant (two callers share state), and testing is harder. Prefer passing state explicitly through a struct pointer when the design allows.

## 7. `static` functions (file-private)

At file scope, `static` means "this name is visible only in this translation unit (this `.c` file)". Without `static`, every function has external linkage: any other `.c` file can call it, and two files defining the same name collide at link time.

```c
/* mat.c */
static double dot_row_col(const double *a, const double *b, int n, int stride)
{ /* helper; nobody outside mat.c needs it */ }

void mat_mul(const double *a, const double *b, double *c, int m, int n, int p)
{ /* public API, declared in mat.h */ }
```

Rule: make every function `static` unless it is declared in a header for other files to use. This shrinks the public surface, lets the compiler inline more aggressively, and avoids name clashes (`helper`, `swap`, `max` are defined in a hundred files).

## 8. Global variables and why to avoid them

A variable declared outside any function has file scope and static storage: it exists for the whole program and is zero-initialized if you do not initialize it.

```c
int g_count = 0;                          /* global with external linkage */
static double g_tolerance = 1e-9;         /* global visible only in this file */

void bump(void) { g_count++; }
```

Problems:

- Any function anywhere can change it, so you cannot reason about a function from its signature.
- Two functions using the same global cannot run concurrently (threads) or be tested independently.
- Name clashes across files (`n`, `i`, `data`).
- Hidden coupling: `solver_step()` silently depends on `g_dt` being set.

Acceptable uses: true program-wide constants (`static const double PI = 3.14159...;`), a configuration struct read once at startup, and debugging flags. For everything else, pass a pointer to a struct that holds the state (`struct sim *s`). Every serious C codebase does this; it is also how you will structure an N-body sim (`struct nbody { double *pos, *vel, *mass; int n; double dt; }`).

Python equivalent: module-level variables with `global` statements, similarly discouraged.

## 9. Scope and lifetime

**Scope** is where a name is visible. **Lifetime** (storage duration) is when the object exists. They are related but not identical.

| Declared where | Scope | Lifetime | Initial value if not initialized |
|---|---|---|---|
| inside `{ }` (block) | that block | until the block exits (automatic) | garbage |
| function parameter | the function body | until the function returns | (always initialized by the argument) |
| `static` inside a function | that function | whole program | 0 |
| outside any function | rest of the file (and other files if not `static`) | whole program | 0 |
| `for (int i = ...)` | the loop | the loop | (always initialized) |

```c
#include <stdio.h>

int g = 1;                                /* file scope, program lifetime */

void f(void)
{
    int a = 10;                           /* block scope (function body), automatic */
    {
        int a = 20;                       /* inner block: SHADOWS outer a; -Wshadow warns */
        printf("inner a=%d\n", a);        /* 20 */
    }
    printf("outer a=%d g=%d\n", a, g);    /* 10 1 */
}

int main(void) { f(); return 0; }
```

Output:

```
inner a=20
outer a=10 g=1
```

The critical rule: an automatic variable's memory is RELEASED when its block exits. Any pointer to it becomes invalid (dangling). This is the source of the "returning address of a local" bug (`../05_pointers/lesson.md`).

Python equivalent: Python has function scope only (no block scope); a variable assigned in an `if` is visible after it. C variables die at the closing brace.

## 10. The call stack

The stack is a region of memory that grows and shrinks as functions are called and return. Each call pushes a **frame** containing the return address, saved registers, parameters (or copies of them), and local variables. Returning pops the frame.

```
main() calls f(3), f calls g(4):

high addresses
+-------------------------+
| main's frame            |
|   int x = 7             |
|   return addr -> OS     |
+-------------------------+
| f's frame               |
|   int n = 3   (copy)    |
|   int tmp               |
|   return addr -> main   |
+-------------------------+
| g's frame               |   <- stack pointer (top of stack) while g runs
|   int k = 4   (copy)    |
|   double buf[100]       |   800 bytes of automatic storage
|   return addr -> f      |
+-------------------------+
low addresses  (stack grows downward on arm64 and x86-64)

When g returns: its frame is popped. buf no longer exists.
When f returns: its frame is popped. tmp no longer exists.
```

Consequences:

- Locals are cheap to allocate (just move the stack pointer) and automatically freed.
- Total stack is limited (8 MB default on macOS main thread). `double big[2000000];` as a local is 16 MB: instant crash. Large arrays go on the heap with `malloc` (chapter 06).
- A recursion that is too deep exhausts the stack.
- After a function returns, its frame's memory will be reused by the next call. A pointer into it points at whatever is there now.

## 11. `inline`

`inline` is a hint that the compiler may substitute the function body at the call site instead of emitting a call. Modern compilers at `-O2` inline small functions on their own; the keyword mainly matters for functions defined in headers:

```c
/* in a header, so every .c file that includes it gets its own copy */
static inline double sq(double x) { return x * x; }
```

`static inline` in a header is the standard idiom: `static` avoids multiple-definition link errors, `inline` avoids "unused function" warnings and tells the compiler it is meant to be inlined. Plain `inline` without `static` has subtle linkage rules in C99/C11 (you need exactly one `extern inline` definition somewhere); avoid it.

Use it for tiny hot helpers: `sq`, `clamp`, `idx(i, j, cols)`.

## 12. Header files as contracts

A header (`.h`) holds declarations: prototypes, struct definitions, constants, `static inline` functions. A source file (`.c`) holds definitions. Other files `#include` the header to learn the interface without seeing the implementation.

`mymath.h`:

```c
#ifndef MYMATH_H                          /* include guard: prevents double inclusion */
#define MYMATH_H

double vec_dot(const double *a, const double *b, int n);
double vec_norm(const double *a, int n);
void   vec_scale(double *a, int n, double k);

static inline double sq(double x) { return x * x; }

#endif /* MYMATH_H */
```

`mymath.c`:

```c
#include "mymath.h"                       /* include your own header: the compiler checks
                                             that definitions match declarations */
#include <math.h>

double vec_dot(const double *a, const double *b, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

double vec_norm(const double *a, int n) { return sqrt(vec_dot(a, a, n)); }

void vec_scale(double *a, int n, double k)
{
    for (int i = 0; i < n; i++) a[i] *= k;
}
```

`main.c`:

```c
#include <stdio.h>
#include "mymath.h"

int main(void)
{
    double v[3] = {3.0, 4.0, 0.0};
    printf("norm=%g\n", vec_norm(v, 3));  /* 5 */
    vec_scale(v, 3, 0.5);
    printf("v[0]=%g sq(v[1])=%g\n", v[0], sq(v[1]));   /* 1.5 4 */
    return 0;
}
```

Compile all `.c` files together (the header is never compiled on its own):

```sh
cc -Wall -Wextra -std=c11 -O2 -o prog main.c mymath.c -lm
```

Or separately, then link:

```sh
cc -Wall -Wextra -std=c11 -O2 -c mymath.c        # -> mymath.o
cc -Wall -Wextra -std=c11 -O2 -c main.c          # -> main.o
cc -o prog main.o mymath.o -lm
```

**Include guards**: `#ifndef X / #define X / ... / #endif` makes the second `#include` of the same header expand to nothing. Without it, including `mymath.h` twice (directly, or via two other headers) would define `sq` twice: error. `#pragma once` is a widely supported non-standard alternative.

Rules:

- Headers contain declarations, types, macros, `static inline` functions. Never non-inline function definitions or variable definitions (you get "duplicate symbol" at link time).
- Each `.c` file includes its own header first, so the compiler verifies the contract.
- `"quotes"` for your headers, `<angles>` for system headers.

Python equivalent: a module's public API is what `import` exposes; the header is like a `.pyi` stub file declaring signatures, and `#include` is like `from mymath import *` at the textual level.

## 13. `<math.h>` and `-lm`

`<math.h>` declares the standard math functions. They operate on `double` by default; `float` variants have an `f` suffix, `long double` an `l` suffix.

| Function | Meaning |
|---|---|
| `sqrt(x)`, `cbrt(x)` | square root, cube root |
| `pow(x, y)` | x^y (slow; use `x*x` for squares) |
| `exp(x)`, `log(x)`, `log2`, `log10`, `log1p(x)`, `expm1(x)` | exponentials/logs; the last two are accurate near 0 |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2(y, x)` | trig; `atan2` gets the quadrant right |
| `sinh`, `cosh`, `tanh` | hyperbolic; `tanh` is an activation function |
| `fabs(x)` | absolute value (NOT `abs`, which is for `int` and lives in `<stdlib.h>`) |
| `floor`, `ceil`, `round`, `trunc`, `lround` | rounding; `lround` returns `long` |
| `fmod(x, y)` | floating remainder |
| `fmax(x, y)`, `fmin(x, y)` | max/min that handle NaN sensibly |
| `hypot(x, y)` | `sqrt(x*x + y*y)` without overflow |
| `isnan(x)`, `isinf(x)`, `isfinite(x)` | classification macros |
| `INFINITY`, `NAN`, `M_PI` | constants (`M_PI` is POSIX, not ISO C; define your own if portability matters) |

```c
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("%g %g %g\n", sqrt(2.0), exp(1.0), log(10.0));
    printf("%g %g\n", atan2(1.0, -1.0), tanh(0.5));
    printf("%g %g\n", fabs(-3.5), fmax(NAN, 2.0));      /* 3.5 2 */
    printf("%d %d\n", isnan(0.0 / 0.0), isinf(1.0 / 0.0));
    printf("%g\n", 1.0 / sqrt(2.0 * M_PI));              /* Gaussian normalization */
    return 0;
}
```

`-lm` links `libm`. On macOS, the math functions live in the system library and link without it; on Linux, omitting `-lm` gives `undefined reference to 'sqrt'`. Always pass `-lm`; it is harmless where unneeded.

Two traps: `abs(-3.5)` silently truncates to `abs(-3)` = 3 (it takes `int`); use `fabs`. `sqrt(-1.0)` returns `NaN`, not an error.

## 14. Function naming conventions

C has no namespaces, classes, or overloading. Every function name is global to the program. The convention that keeps this manageable is `noun_verb` (or `module_verb`): the "type" or module comes first, the action second.

| Good | Bad | Why |
|---|---|---|
| `mat_mul(a, b, c)` | `multiply(a, b, c)` | `multiply` will clash with the vector one |
| `vec_push(v, x)` | `push(v, x)` | which push? |
| `mat_alloc(rows, cols)` / `mat_free(m)` | `new_matrix` / `destroy` | pair alloc/free with the same prefix |
| `tok_encode(t, s)` / `tok_decode(t, ids)` | `encode` / `decode` | |
| `nbody_step(sim, dt)` | `step(dt)` | |
| `sim_init`, `sim_run`, `sim_dump` | | consistent lifecycle verbs |

Other conventions:

- Lowercase with underscores. `CamelCase` is uncommon in C.
- The struct being operated on is the first parameter: `mat_mul(const Mat *a, const Mat *b, Mat *out)`. This is the C version of `self`.
- Output parameters come last.
- Boolean queries: `mat_is_square(m)`, `vec_is_empty(v)`.
- Private helpers: `static`, optionally with a trailing underscore or no prefix.
- Constants and macros: `ALL_CAPS`.

Python equivalent: `Mat.mul(self, other)` becomes `mat_mul(a, b, out)`; the class name becomes the prefix, `self` becomes the first pointer argument.

## Gotchas and undefined behavior

- Calling a function before any declaration is a C11 error; if your compiler lets it through (old mode), the implicit `int` return corrupts `double` results.
- A non-`void` function that reaches the end without `return` and whose value is used: UB.
- Modifying a parameter expecting to change the caller's variable: silently does nothing.
- Passing an array and using `sizeof(param)` inside: gives the pointer size (8), not the array size.
- Returning the address of a local variable: dangling pointer, UB when dereferenced.
- Large local arrays (`double buf[1000000]`) overflow the stack. Use `malloc` or `static`.
- Deep recursion overflows the stack; no error message, just a crash. `fib_naive(50)` is also hopelessly slow.
- `static` local state makes functions non-reentrant; a function using one cannot be safely called from two threads or recursively.
- Two `.c` files each defining a non-`static` function with the same name: link error "duplicate symbol".
- Defining a variable or non-inline function in a header: duplicate symbol when two files include it.
- Missing include guard: "redefinition of struct" errors when a header is included twice.
- `abs()` on a `double` truncates. Use `fabs`.
- Shadowing an outer variable with an inner declaration of the same name compiles and confuses; `-Wshadow` catches it.
- `int f()` is not the same as `int f(void)`; the former accepts any arguments unchecked.

## Common mistakes checklist

- [ ] Every function is declared (prototype or definition) before its first call.
- [ ] Every non-`void` function returns on every path.
- [ ] Zero-argument functions are written `(void)`.
- [ ] Functions that must modify a caller's scalar take a pointer (chapter 05); those that only read take `const` pointers.
- [ ] Array parameters come with an explicit length parameter.
- [ ] No pointer to a local is returned.
- [ ] Helpers are `static`; only header-declared functions are external.
- [ ] Headers have include guards and contain no definitions (except `static inline`).
- [ ] Each `.c` includes its own `.h` first.
- [ ] Recursion has a base case and a depth you can bound.
- [ ] `fabs` for doubles, `abs` for ints; `-lm` on the command line.
- [ ] Names follow `module_verb`.

## You can move on when...

- You can explain the difference between `double f(double);` and `double f(double x) { ... }` and why the first must precede any call.
- You can predict the output of a program that passes an `int` and an `int[]` to a function that modifies both.
- You can draw the stack frames for `main -> f -> g` and say which locals exist at each point.
- You can write a header with an include guard and a `static inline` helper, a matching `.c`, and compile the pair with `main.c` in one `cc` command.
- You can state why `fib_naive(40)` is slow and rewrite it as a loop.
- You can name a `static` local's initialization time and lifetime.
- You can explain what `static` means at file scope and why most functions should have it.
