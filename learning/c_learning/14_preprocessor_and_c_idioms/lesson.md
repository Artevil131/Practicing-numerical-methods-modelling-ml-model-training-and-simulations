# Chapter 14 — The Preprocessor and C Idioms

## What you'll be able to do after this chapter

- Predict what the preprocessor does to your source, and check with `-E`.
- Write macros that don't bite: parenthesized, `do { } while (0)`, side-effect-aware, variadic, with stringification and token pasting.
- Generate an enum + string table + function table from one X-macro list.
- Ship a header-only library the stb way, with `static inline` helpers.
- Handle errors the C way: return codes, `errno`, out-parameters, `goto cleanup`, error enums with a string table.
- Read real C: opaque pointers, const-correctness, designated initializers, compound literals, `static` linkage, naming conventions, and monster declarations like `int (*signal(int, void (*)(int)))(int)`.
- Know what the standard library gives you (little) and reach for `pthread`/OpenMP when you need every core.

## Why this matters for ML / numerics / sims

Your matrix library will be a header you drop into every project — you need the header-only pattern. Your ops (`ADD, MUL, TANH, MATMUL, ...`) need an enum, a printable name, and a forward/backward function each — that's an X-macro. Loading a dataset touches three resources and can fail at any step — that's `goto cleanup`. A `Tensor` type whose internals you want to change later — that's an opaque pointer. And when your N-body sim or matmul needs to use all 10 cores of your Mac, one `#pragma omp parallel for` does it. None of this is language syntax; it's the vocabulary of C programs you'll read and write.

---

## 1. How the preprocessor works

The preprocessor runs *before* the compiler. It is a **text substitution** engine: it does not know types, scopes, or expressions. It handles `#include` (paste a file), `#define` (text macro), and `#if` (conditional inclusion), and then hands the resulting text ("translation unit") to the compiler.

See its output:

```
cc -E -std=c11 prog.c | less        # everything after preprocessing; your code is at the bottom
cc -E -std=c11 -P prog.c            # -P drops the # line markers
cc -E -std=c11 -dM - < /dev/null    # list every predefined macro
```

`#include <stdio.h>` pastes ~1000 lines. That's why a "hello world" `.i` file is huge, and why include guards exist.

## 2. Object-like and function-like macros

```c
#define N_HIDDEN 128                    // object-like: replaced wherever the token N_HIDDEN appears
#define SQUARE(x) ((x) * (x))           // function-like: NO space before '(' or it's object-like
#define PI 3.14159265358979323846

double area = PI * SQUARE(r);           // -> 3.14159265358979323846 * ((r) * (r))
```

Prefer `const double PI = 3.14...;` or `enum { N_HIDDEN = 128 };` for constants when you can — they have types and show up in the debugger. Macros are needed when the value must be a compile-time constant in places C11 requires one (array sizes at file scope, `case` labels, `#if`).

## 3. Macro hygiene

Since macros are text, the classic failures are all about operator precedence and re-evaluation.

```c
#define BAD_SQUARE(x) x * x
BAD_SQUARE(a + 1)        // -> a + 1 * a + 1     (wrong: means a + a + 1)
#define SQUARE(x) ((x) * (x))
SQUARE(a + 1)            // -> ((a + 1) * (a + 1))   parenthesize every argument AND the whole body

#define MAX(a, b) ((a) > (b) ? (a) : (b))
MAX(i++, j)              // -> ((i++) > (j) ? (i++) : (j))    i incremented TWICE if it wins
                         // Never pass expressions with side effects to a macro that uses its arg more than once.

// Multi-statement macros: wrap in do { } while (0) so they behave as one statement.
#define SWAP_BAD(a, b) double _t = a; a = b; b = _t
if (x > y) SWAP_BAD(x, y);       // only `double _t = a;` is inside the if. Bug.
#define SWAP(a, b) do { double _t = (a); (a) = (b); (b) = _t; } while (0)
if (x > y) SWAP(x, y); else puts("ordered");   // works; the trailing ; completes the statement
```

C11 fix for the double-evaluation problem in `MAX`: write an `static inline` function instead (section 10) — the compiler inlines it and arguments are evaluated once. Use macros only when you need the textual power (generic across types, stringify, paste).

## 4. Conditional compilation

```c
#ifdef DEBUG                     // defined at all (any value, even empty)?
#  define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#  define DBG(...) ((void)0)
#endif

#ifndef MATRIX_H                 // classic include guard
#define MATRIX_H
/* ... header body ... */
#endif

#if defined(__APPLE__) && defined(__aarch64__)
#  define PLATFORM "macOS arm64"
#elif defined(__linux__)
#  define PLATFORM "Linux"
#else
#  error "Unsupported platform"   // stops compilation with this message
#endif

#if USE_FLOAT                     // #if evaluates an integer expression; undefined macros are 0
typedef float real;
#else
typedef double real;
#endif
```

`-DDEBUG` on the command line is `#define DEBUG 1`. `-DUSE_FLOAT=1` sets a value. `#pragma once` (non-standard but universal) replaces the include-guard triple; both are fine — this course uses guards in code that might be read by strict tools, `#pragma once` otherwise.

## 5. Predefined macros

| Macro | Expands to |
|---|---|
| `__FILE__` | `"prog.c"` string literal |
| `__LINE__` | current line number (int) |
| `__func__` | current function name (a `const char[]`, C99 — technically not a macro) |
| `__DATE__`, `__TIME__` | compile date/time strings |
| `__STDC_VERSION__` | `201112L` for C11, `201710L` for C17 |
| `__GNUC__`, `__clang__` | compiler identity |
| `__APPLE__`, `__linux__`, `_WIN32` | platform |
| `__x86_64__`, `__aarch64__` | architecture |
| `__OPTIMIZE__` | defined when `-O1` or higher |

```c
printf("%s:%d in %s, built %s %s, C%ld\n", __FILE__, __LINE__, __func__, __DATE__, __TIME__, __STDC_VERSION__);
// prog.c:12 in main, built Sep  4 2026 10:15:00, C201112
```

## 6. Stringification `#x` and token pasting `a##b`

```c
#define STR(x) #x                      // turns the argument text into a string literal
#define XSTR(x) STR(x)                 // extra level so macro *arguments* get expanded first
#define CAT(a, b) a##b                 // glues two tokens into one identifier

STR(hello world)    // -> "hello world"
STR(N_HIDDEN)       // -> "N_HIDDEN"     (no expansion inside #)
XSTR(N_HIDDEN)      // -> "128"          (expanded, then stringified)
CAT(vec_, push)     // -> vec_push
CAT(Vec_, double)   // -> Vec_double     (this is how VEC_DEFINE(T) built names in chapter 11)

#define ASSERT_MSG(cond) do { if (!(cond)) fprintf(stderr, "assert failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); } while (0)
ASSERT_MSG(a->cols == b->rows);   // prints the exact source text of the condition
```

Stringification is why `assert(x > 0)` can print `x > 0`. Pasting is how you generate families of names.

## 7. Variadic macros: `__VA_ARGS__`

```c
#define LOG(level, fmt, ...) \
    fprintf(stderr, "[%s %s:%d] " fmt "\n", level, __FILE__, __LINE__, ##__VA_ARGS__)

LOG("INFO", "epoch %d loss %.4f", epoch, loss);   // [INFO train.c:40] epoch 3 loss 0.2311
LOG("WARN", "lr is zero");                        // works because ##__VA_ARGS__ eats the comma
```

Adjacent string literals concatenate (`"[%s] " fmt "\n"` becomes one format string), which is how the prefix gets glued on. `##__VA_ARGS__` swallowing the comma when empty is a GNU/clang extension — universal in practice; C23 standardizes `__VA_OPT__(,)`. Under strict `-std=c11 -Wpedantic` it warns; add a `fmt` argument and always pass at least one extra argument, or accept the extension.

## 8. X-macros: one list, many tables

The problem: an enum of ops, a matching string table for printing, and a matching function table — three lists that must stay in sync. The X-macro solution: define the list *once* as a macro that invokes a placeholder `X` for each item, then redefine `X` for each use.

```c
#define ACTIVATION_LIST(X) \
    X(ACT_IDENTITY, "identity", act_identity, dact_identity) \
    X(ACT_RELU,     "relu",     act_relu,     dact_relu)     \
    X(ACT_SIGMOID,  "sigmoid",  act_sigmoid,  dact_sigmoid)  \
    X(ACT_TANH,     "tanh",     tanh,         dact_tanh)

// 1. The enum
#define AS_ENUM(id, name, fn, dfn) id,
typedef enum { ACTIVATION_LIST(AS_ENUM) ACT_COUNT } Activation;
#undef AS_ENUM
// -> typedef enum { ACT_IDENTITY, ACT_RELU, ACT_SIGMOID, ACT_TANH, ACT_COUNT } Activation;

// 2. The name table
#define AS_NAME(id, name, fn, dfn) [id] = name,
static const char *const act_names[ACT_COUNT] = { ACTIVATION_LIST(AS_NAME) };
#undef AS_NAME

// 3. The function tables
#define AS_FN(id, name, fn, dfn) [id] = fn,
#define AS_DFN(id, name, fn, dfn) [id] = dfn,
static double (*const act_fns[ACT_COUNT])(double)  = { ACTIVATION_LIST(AS_FN) };
static double (*const act_dfns[ACT_COUNT])(double) = { ACTIVATION_LIST(AS_DFN) };
#undef AS_FN
#undef AS_DFN

// 4. Parse a name back to an enum
static Activation act_from_name(const char *s) {
    for (int i = 0; i < ACT_COUNT; i++) if (strcmp(s, act_names[i]) == 0) return (Activation)i;
    return ACT_COUNT;   // sentinel: not found
}
// act_names[ACT_RELU] -> "relu";  act_fns[act_from_name("tanh")](0.5) -> 0.462117
```

Add a fifth activation: one line in `ACTIVATION_LIST`, and every table updates. Use the same pattern for autograd op types (`OP_ADD, OP_MUL, OP_MATMUL, ...` with forward/backward pointers), error codes (section 11), token kinds in a tokenizer, and CLI flags.

Python equivalent: a single `dict` or an `Enum` with methods — Python keeps the data together at runtime; C's X-macro keeps it together at compile time.

## 9. Header-only libraries (stb style)

Distributing a library as one `.h` file that contains both declarations and definitions. The trick: the definitions are wrapped in `#ifdef LIB_IMPLEMENTATION`, and exactly **one** `.c` file defines that macro before including the header.

```c
/* matrix.h */
#ifndef MATRIX_H
#define MATRIX_H
#include <stddef.h>

typedef struct { size_t rows, cols; double *data; } Matrix;
Matrix *mat_new(size_t rows, size_t cols);        // declarations: every includer sees these
void    mat_free(Matrix *m);
static inline double mat_get(const Matrix *m, size_t i, size_t j) { return m->data[i * m->cols + j]; }

#endif /* MATRIX_H */

#ifdef MATRIX_IMPLEMENTATION                      // definitions: compiled in exactly one TU
#include <stdlib.h>
Matrix *mat_new(size_t rows, size_t cols) {
    Matrix *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->rows = rows; m->cols = cols;
    m->data = calloc(rows * cols, sizeof *m->data);
    if (!m->data) { free(m); return NULL; }
    return m;
}
void mat_free(Matrix *m) { if (m) { free(m->data); free(m); } }
#endif /* MATRIX_IMPLEMENTATION */
```

```c
/* main.c */
#define MATRIX_IMPLEMENTATION      // this file gets the definitions
#include "matrix.h"
/* other.c */
#include "matrix.h"                // this file gets only declarations; links against main.c's definitions
```

Note the implementation block is *outside* the include guard on purpose: a file might include the header once without the macro and later with it. The stb libraries (`stb_image.h`, etc.) established this style; it's the easiest way to share your matrix/RNG/tokenizer code across the projects in this course.

## 10. `static inline` helpers in headers

A non-`static` function defined in a header gets defined in every `.c` that includes it → "duplicate symbol" at link time. `static` gives each translation unit its own private copy (no link conflict); `inline` tells the compiler you'd like it inlined (it's a hint; `-O2` decides). Together, `static inline` is the correct spelling for small hot helpers in headers:

```c
static inline double relu(double x) { return x > 0 ? x : 0; }
static inline size_t idx2(size_t i, size_t j, size_t cols) { return i * cols + j; }
static inline double clampd(double x, double lo, double hi) { return x < lo ? lo : x > hi ? hi : x; }
```

Unlike a macro, arguments are evaluated once and typed; unlike a regular function, there's no call overhead. Prefer `static inline` over function-like macros whenever the types are known.

## 11. Error handling idioms

C has no exceptions. Four patterns, used together.

### Return codes and `errno`

```c
#include <errno.h>
#include <string.h>
FILE *f = fopen(path, "rb");
if (!f) {
    fprintf(stderr, "open %s: %s\n", path, strerror(errno));   // "open data.bin: No such file or directory"
    return -1;
}
```

Library functions return a sentinel (`NULL`, `-1`, `EOF`) and set the global `errno` to a code you can translate with `strerror`. Check the return *first*, then read `errno` immediately (any later call may overwrite it). Your own functions: return `0` on success and a negative/enumerated error on failure.

### Out-parameters for results

When the return value is the status, results come back through pointers:

```c
int parse_double(const char *s, double *out) {       // 0 = ok, -1 = not a number
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE) return -1;
    *out = v;
    return 0;
}
// double x; if (parse_double(tok, &x) != 0) { /* handle */ }
```

Python equivalent: raising `ValueError` vs returning `(ok, value)`. C forces the tuple style.

### `goto cleanup` for multi-resource functions

When a function acquires several resources and can fail after each, a single exit path that frees whatever was acquired (in reverse order) avoids duplicated cleanup and leaks. This is the one legitimate use of `goto` and it is idiomatic:

```c
int load_dataset(const char *path, Matrix **out) {
    int rc = -1;                          // assume failure
    FILE *f = NULL; double *buf = NULL; Matrix *m = NULL;

    f = fopen(path, "rb");
    if (!f) goto cleanup;
    buf = malloc(N * sizeof *buf);
    if (!buf) goto cleanup;
    if (fread(buf, sizeof *buf, N, f) != N) goto cleanup;
    m = mat_new(ROWS, COLS);
    if (!m) goto cleanup;
    memcpy(m->data, buf, N * sizeof *buf);
    *out = m; m = NULL;                   // ownership transferred; don't free it below
    rc = 0;                               // success
cleanup:
    mat_free(m);                          // all of these accept NULL
    free(buf);
    if (f) fclose(f);
    return rc;
}
```

Every pointer is initialized to `NULL` at the top so `cleanup` can free unconditionally. The "set to NULL after transferring ownership" line is the standard trick to reuse the same cleanup for both paths.

### Error enums + string table

```c
#define ERROR_LIST(X)                          \
    X(ERR_OK,        "ok")                     \
    X(ERR_ALLOC,     "allocation failed")      \
    X(ERR_SHAPE,     "shape mismatch")         \
    X(ERR_IO,        "I/O error")              \
    X(ERR_PARSE,     "parse error")
#define AS_ENUM(id, s) id,
typedef enum { ERROR_LIST(AS_ENUM) ERR_COUNT } Err;
#undef AS_ENUM
#define AS_STR(id, s) [id] = s,
static const char *const err_strings[ERR_COUNT] = { ERROR_LIST(AS_STR) };
#undef AS_STR
static const char *err_str(Err e) { return (e >= 0 && e < ERR_COUNT) ? err_strings[e] : "unknown"; }

// Err e = mat_mul_into(a, b, c); if (e != ERR_OK) fprintf(stderr, "matmul: %s\n", err_str(e));
```

This is your own `strerror`. Return `Err` from every function that can fail; callers check `!= ERR_OK`.

## 12. Opaque pointers for encapsulation

Declare the struct in the header, define it only in the `.c`. Users get a `Tensor *` they can pass around but cannot look inside — you can change the layout without recompiling their code. This is C's `private`.

```c
/* tensor.h */
typedef struct Tensor Tensor;                    // incomplete type: size unknown to users
Tensor *tensor_new(size_t ndim, const size_t *shape);
void    tensor_free(Tensor *t);
size_t  tensor_numel(const Tensor *t);
double *tensor_data(Tensor *t);                  // accessor functions instead of field access

/* tensor.c */
struct Tensor { size_t ndim; size_t shape[8]; size_t numel; double *data; int refcount; };  // private
```

Users can't write `t->data` (compile error: incomplete type), can't put a `Tensor` on the stack (size unknown), and can't `sizeof` it. `FILE *` from `<stdio.h>` is the canonical opaque pointer. Cost: every access is a function call (usually inlined across files only with LTO), and one heap allocation per object.

## 13. Const-correctness

`const` on a parameter documents "this function won't modify it" and lets the compiler check it.

```c
double mat_sum(const Matrix *m);           // reads m; can be called with a const or non-const Matrix*
void   mat_scale(Matrix *m, double k);     // modifies m

const double *p = arr;    // pointer to const double: *p = 1 is an error; p++ is fine
double *const q = arr;    // const pointer to double: *q = 1 fine; q++ is an error
const double *const r = arr;  // neither
const char *name = "relu";    // string literals are const; writing through name is UB
```

Read `const` right-to-left: `const double *` = "pointer to double that is const". Casting `const` away and writing is UB if the object was really const (e.g. a string literal). Make every pointer parameter `const` unless the function writes through it — it costs nothing and catches bugs like calling `mat_transpose_inplace` on the shared weight matrix.

## 14. Designated initializers and compound literals

```c
typedef struct { size_t rows, cols; double *data; } Matrix;
typedef struct { double lr, momentum; int epochs; const char *name; } Config;

Matrix m = { .rows = 2, .cols = 3, .data = NULL };            // designated: named fields, any order
Config c = { .lr = 0.01, .epochs = 10 };                      // unnamed fields are zero/NULL: momentum=0, name=NULL
int lut[8] = { [3] = 1, [7] = 1 };                            // array designators: {0,0,0,1,0,0,0,1}

// Compound literal: an unnamed object of a given type, built in place.
mat_print(&(Matrix){ .rows = 1, .cols = 3, .data = (double[]){ 1, 2, 3 } });
c = (Config){ .lr = 0.1 };                                    // reset a struct: everything else zeroed
return (Vec){ 0 };                                            // return a zeroed struct
```

Compound literals have the lifetime of the enclosing block (not just the statement) — don't return a pointer to one. Designated initializers are why `(Layer){ .forward = relu_forward, ... }` in chapter 11 works and why `Config` structs are the C answer to Python keyword arguments with defaults: `train(&(Config){ .lr = 0.01, .epochs = 5 })`.

## 15. `static` everything that isn't exported

At file scope, `static` means *internal linkage*: the name is invisible to other `.c` files. Every helper function and file-level variable that isn't part of your header's API should be `static`. Benefits: no name collisions between files (two files can each have their own `static int count`), the compiler can inline and optimize more aggressively (it knows all call sites), and `-Wunused-function` can tell you when a helper is dead.

```c
static double sigmoid(double x) { ... }     // private to this file
double mat_sum(const Matrix *m) { ... }     // exported: declared in matrix.h
static int g_verbose = 0;                   // file-private global (rare, but if you must)
```

Inside a function, `static` means something different: the variable persists across calls (one instance for the whole program). Useful for a lazily-initialized table; dangerous with threads.

## 16. Naming conventions

| Thing | Convention | Example |
|---|---|---|
| Variables, functions | `snake_case` | `learning_rate`, `mat_mul` |
| Public functions | `MODULE_verb` prefix (C has no namespaces) | `mat_new`, `mat_free`, `tok_encode`, `rng_uniform` |
| Types (`typedef`) | `PascalCase` or `snake_case_t` | `Matrix`, `Tensor`, `size_t`-style `matrix_t` |
| Macros, enum constants | `SCREAMING_CASE` | `MAX_LAYERS`, `ACT_RELU`, `ERR_SHAPE` |
| Enum constants | prefixed by type | `ACT_`, `ERR_`, `OP_` |
| Header guards | `FILENAME_H` | `MATRIX_H` |
| File-private globals | `g_` prefix (optional) | `g_rng_state` |
| Out-parameters | last, named `out`/`*_out` | `int parse(const char *s, double *out)` |
| Constructor / destructor pairs | `x_new`/`x_free`, `x_init`/`x_destroy` | never `x_create` with `x_delete` — pick a pair and stick to it |

Names ending in `_t` are reserved by POSIX; avoid defining your own `foo_t` in code that might be ported (in practice everyone does it — just know the rule). Names starting with `_` + uppercase or `__` are reserved for the implementation everywhere: never `#define __MY_GUARD__`.

## 17. `-std=c11` vs GNU extensions

`-std=c11` asks for strict ISO C11; `-std=gnu11` (the default when you pass nothing) adds GNU extensions. Extensions you will meet in real code:

| Extension | Standard alternative |
|---|---|
| `##__VA_ARGS__` comma swallowing | C23 `__VA_OPT__`; or always pass ≥1 variadic arg |
| `typeof(x)` | C23 `typeof`; in C11 there's none — use `_Generic` or pass the type |
| Statement expressions `({ ...; val; })` | a `static inline` function |
| `__attribute__((unused))`, `((packed))`, `((aligned(64)))` | C11 `_Alignas(64)`; `(void)x;` for unused |
| Zero-length arrays `T arr[0]` | C99 flexible array member `T arr[]` as last field |
| Case ranges `case 1 ... 5:` | multiple `case` labels |
| `__builtin_popcountll`, `__builtin_expect` | write the loop; ignore the hint |
| `M_PI` (POSIX, not ISO) | `#define PI 3.14159265358979323846` |

With `-std=c11`, macOS headers still expose POSIX things like `M_PI` and `clock_gettime` because Apple defines `_DARWIN_C_SOURCE` by default; on Linux you may need `-D_POSIX_C_SOURCE=200809L` or `-D_DEFAULT_SOURCE`. `-Wpedantic` warns on extensions. Use them knowingly; don't be surprised when code that "compiled fine" on macOS needs a feature-test macro on Linux.

## 18. Reading real C: decoding declarations

The rule: find the identifier, then read outward, going right when you can (`[]` array-of, `()` function-returning) and left when you can't (`*` pointer-to), respecting parentheses.

`int (*signal(int, void (*)(int)))(int)` — the POSIX `signal` function:

1. Identifier: `signal`.
2. Right: `(int, void (*)(int))` — `signal` is a **function** taking an `int` and a `void (*)(int)` (pointer to function taking int returning void — the handler).
3. Can't go further right inside the parens; go left: `*` — that function **returns a pointer**.
4. Leave the parentheses. Right: `(int)` — pointer to a **function taking int**.
5. Left: `int` — **returning int**... wait, no: the outermost type is `int (*...)(int)`: the returned pointer points to a function `(int) -> int`? Check: `signal` returns the *previous* handler, which is `void (*)(int)`. The declaration in real headers is `void (*signal(int, void (*)(int)))(int)` — pointer to function `(int) -> void`. The `int` version in the section title is the classic textbook puzzle; decode it the same way: returns pointer to function `(int) -> int`.

With `typedef` it's trivial, which is the lesson:

```c
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```

More practice:

| Declaration | Reading |
|---|---|
| `char *argv[]` | array of pointers to char (array of strings) |
| `char (*p)[16]` | pointer to array of 16 chars (pointer to one row of a `char rows[N][16]`) |
| `double (*mat)[cols]` | pointer to array of `cols` doubles — a VLA-typed pointer for 2D indexing `mat[i][j]` |
| `int *(*fp)(void)` | pointer to function `(void) -> int*` |
| `void (*table[4])(void *)` | array of 4 pointers to functions `(void*) -> void` |
| `const char *const names[]` | array of const pointers to const char (fully immutable string table) |

Tool: `cdecl` (`brew install cdecl`; `cdecl> explain int (*f)(double)`).

## 19. What the C standard library gives you, and doesn't

| You get | Header |
|---|---|
| I/O: `printf`, `fopen`, `fread`, `fgets` | `<stdio.h>` |
| Memory, conversion, `qsort`, `bsearch`, `exit`, `getenv` | `<stdlib.h>` |
| Byte/string ops: `memcpy`, `memset`, `strlen`, `strcmp`, `strtok`, `strerror` | `<string.h>` |
| Math | `<math.h>` |
| Char classes: `isdigit`, `isspace`, `toupper` | `<ctype.h>` |
| Fixed-width ints, limits | `<stdint.h>`, `<inttypes.h>`, `<limits.h>`, `<float.h>` |
| `assert`, `errno`, `bool`, `size_t`/`NULL`, varargs | `<assert.h>`, `<errno.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdarg.h>` |
| Time | `<time.h>` |
| C11: threads, atomics, alignment, `_Generic` | `<threads.h>` (optional — **not provided on macOS**), `<stdatomic.h>`, `<stdalign.h>` |

| You do NOT get | What people use |
|---|---|
| Containers (vector, hash map, list) | write your own (chapters 10, 11) or a header-only lib (stb_ds.h, klib) |
| A string type | `char*` + `strlen`; write a `Str { char *p; size_t len; }` |
| Threads (portably) | POSIX `pthread` (`<pthread.h>`, everywhere but Windows), or OpenMP |
| Networking, filesystem walking, directories | POSIX `<sys/socket.h>`, `<dirent.h>` |
| Regex, JSON, images, linear algebra | libraries: PCRE, cJSON, stb_image, BLAS/LAPACK |
| Exceptions, RAII, generics, namespaces | idioms from this chapter |

Python gives you all of that in the standard library; in C, "batteries" means `man 3` and a folder of `.h` files you trust.

## 20. Using all your cores: `pthread` and OpenMP

C11's `<threads.h>` is optional and absent on macOS. The pragmatic paths:

**pthread** — explicit threads. Create N threads, each processes a slice of rows, join them:

```c
#include <pthread.h>
typedef struct { const double *A, *B; double *C; size_t n, row0, row1; } Job;
static void *worker(void *arg) {
    Job *j = arg;
    for (size_t i = j->row0; i < j->row1; i++) /* ... compute row i of C ... */;
    return NULL;
}
// pthread_t th[8]; Job jobs[8];
// for (t) { jobs[t] = (Job){ A, B, C, n, t*n/8, (t+1)*n/8 }; pthread_create(&th[t], NULL, worker, &jobs[t]); }
// for (t) pthread_join(th[t], NULL);
// compile: cc ... -lpthread   (implicit on macOS)
```

Note `worker`'s signature `void *(*)(void *)` and the `Job` struct — exactly the `void *ctx` pattern from chapter 11.

**OpenMP** — one pragma, the compiler does the thread pool:

```c
#pragma omp parallel for
for (size_t i = 0; i < n; i++)             // rows split across all cores automatically
    for (size_t k = 0; k < n; k++) {
        double a = A[i * n + k];
        for (size_t j = 0; j < n; j++) C[i * n + j] += a * B[k * n + j];
    }
```

Requirements: `brew install libomp`, then `cc -Xpreprocessor -fopenmp -lomp ...` on Apple clang (or `clang -fopenmp` with Homebrew LLVM; on Linux `gcc -fopenmp`). Without the flag the pragma is silently ignored and the code runs serially — the same source works both ways. Rules: the loop body must be independent across iterations (each `i` writes its own row of `C` — fine; a shared accumulator needs `reduction(+:sum)`). For N-body: `#pragma omp parallel for` over bodies `i`, each computing its own acceleration from all `j`. For a 10-core M-series Mac, expect 6–9x on matmul and N-body. Python equivalent: `numpy` calls a threaded BLAS; `torch.set_num_threads`.

---

## Gotchas and undefined behavior

- **Unparenthesized macro args/bodies** change meaning by precedence. Always `((x) * (x))`.
- **Side effects in macro arguments** evaluated multiple times: `MAX(i++, j)`, `SQUARE(f())`.
- **Multi-statement macros without `do { } while (0)`** break inside `if` without braces.
- **`#define` with a trailing semicolon**: `#define N 10;` → `int a[N]` becomes `int a[10;]`.
- **Redefining a macro to a different value** without `#undef` is a constraint violation (warning/error).
- **Include guard name collision**: two headers both guarded by `UTIL_H` — the second is silently skipped.
- **Missing `#include`** for a function → C11 makes implicit declaration an error in clang (good); older compilers assumed `int` return → `sqrt` returns garbage.
- **`##__VA_ARGS__`** is an extension; `-Wpedantic` complains.
- **Compound literal lifetime**: returning `&(Matrix){...}` from a function is a dangling pointer.
- **Casting away `const`** and writing to a string literal is UB (crash on macOS: literals are in read-only memory).
- **`errno` clobbering**: `if (!f) { printf(...); perror(...) }` — `printf` may change `errno`. Save it first.
- **`goto` jumping over a VLA declaration** is a constraint violation; jumping *backward* over initializations of ordinary variables is allowed but leaves them uninitialized on re-entry — keep `cleanup` at the end.
- **Opaque type with `sizeof`**: `malloc(sizeof(Tensor))` in user code fails to compile (incomplete type) — that's the point, but confusing the first time.
- **OpenMP race**: `#pragma omp parallel for` around a loop that does `sum += x[i]` without `reduction(+:sum)` gives wrong, nondeterministic results.
- **`static` local in a threaded function** is shared by all threads — a data race.

## Common mistakes checklist

- [ ] `#define SQUARE(x) x*x`.
- [ ] `#define SWAP(a,b) { ... }` with braces but no `do/while(0)`, then `if (c) SWAP(a,b); else ...` fails to compile.
- [ ] Forgot `#undef X` between X-macro uses, got "macro redefined".
- [ ] Defined the header-only implementation macro in two `.c` files → duplicate symbols.
- [ ] Non-`static` helper in a header → duplicate symbol at link.
- [ ] Ignored return code of `fopen`/`malloc`/`fread`.
- [ ] Freed resources in three different `return` paths, missed one — use `goto cleanup`.
- [ ] `strerror(errno)` after another call already changed `errno`.
- [ ] Function takes `Matrix *` but only reads — should be `const Matrix *`.
- [ ] Mixed `x_new`/`x_destroy`/`x_delete` naming across modules.
- [ ] OpenMP pragma with a shared write and no `reduction`.

## You can move on when...

- You can explain why `#define SQUARE(x) x*x` is wrong and write the correct version, plus `do { } while (0)` for multi-statement macros.
- You can write a `LOG(fmt, ...)` macro with `__FILE__`/`__LINE__` and explain `##__VA_ARGS__`.
- You can write an X-macro list producing an enum, a name table, and a function table, and add an entry without touching the tables.
- You can split a `matrix.h` into declaration and `#ifdef MATRIX_IMPLEMENTATION` parts and explain `static inline`.
- You can write a `load_*` function with `goto cleanup` that leaks nothing on any failure path (verify with `leaks`).
- You can decode `void (*signal(int, void (*)(int)))(int)` out loud and rewrite it with a `typedef`.
- You can say what `static` at file scope does, what `const Matrix *` promises, and what `(Config){ .lr = 0.1 }` produces.
- You know that `<threads.h>` isn't on macOS and can name the two practical alternatives for a parallel matmul.
