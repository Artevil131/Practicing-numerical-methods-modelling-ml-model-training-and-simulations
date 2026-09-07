# Chapter 01 — From C to C++

## What you'll be able to do after this chapter

- Compile and run a C++17 program with `c++ -Wall -Wextra -std=c++17 -O2`, and know which C habits carry over unchanged and which ones break.
- Use `<iostream>`, `std::string`, `std::array`, references, `auto`, range-based `for`, and `enum class` instead of their C equivalents — and explain *why* each one is safer.
- Replace `#define` constants with `constexpr`, C casts with named casts, and `NULL` with `nullptr`.
- Overload functions and use default arguments to write cleaner numerical helper APIs.
- Link your existing C matrix library into a C++ program with `extern "C"`.

## Why this matters for ML / numerics / sims

You are going to write a matrix library, an autograd engine, a BPE tokenizer, and several physics simulations. In C, every one of those needs manual `malloc`/`free`, hand-rolled growable arrays, `void*` generics, and `goto cleanup` error handling. C++ keeps C's speed and memory model (you still control every byte) but adds tools that remove the most common bug classes: references remove null-pointer out-parameters, `std::string` removes buffer overflows in tokenizers, `constexpr` removes macro surprises in constants like `EPSILON`, and `extern "C"` means the C code you already wrote is not wasted. Everything in this chapter is small; the big ideas (RAII, containers, templates) come in the next four.

---

## 1. What C++ is relative to C

C++ began as "C with Classes" and is *almost* a superset of C. Nearly every C program you wrote in `../c_learning/` compiles as C++ with minor edits. The important differences:

| C | C++ | Why it matters |
|---|---|---|
| `void *p = malloc(n); int *q = p;` — implicit `void*` conversion | Error: must write `int *q = (int*)malloc(n);` or better, `static_cast<int*>(...)` | C++ is stricter about types |
| `struct Point p;` | `Point p;` — `struct` keyword optional | Struct names are real type names |
| `char *s = "hi";` allowed | `const char *s = "hi";` required (string literals are `const char[N]`) | Modifying a literal was always UB; C++ refuses to compile it |
| `int f();` means "any arguments" | `int f();` means "no arguments" (same as `int f(void)`) | Prototypes are always required |
| New identifiers OK: `class`, `new`, `delete`, `this`, `template`, `bool`, `true`, `false`, `namespace`, `using`, `public`, `private`, ... | These are keywords | Rename any C variable called `new` or `class` |
| `enum { A, B }` values are `int` | Enums are their own type; `int x = A;` OK but `Enum e = 3;` is an error | Stronger typing |
| `sizeof('a') == sizeof(int)` (4) | `sizeof('a') == 1` — `'a'` has type `char` | See section 24 |
| Designated initializers `{.x = 1}` (C99) | Only from C++20, and only in declaration order | Use constructors instead (chapter 02) |
| Variable-length arrays `int a[n]` (C99) | Not standard C++ (clang/gcc allow them as an extension, but do not use them) | Use `std::vector` |
| `_Generic` | Templates (chapter 05) | |
| `restrict` | Not a keyword (use `__restrict` if you really need it) | |

Everything else — pointers, `malloc`, structs, function pointers, `printf`, `<math.h>` — works as before.

## 2. Compiling

```sh
c++ -Wall -Wextra -std=c++17 -O2 -o hello hello.cpp
./hello
```

`c++` on macOS is Apple clang in C++ mode; `g++` on Linux is GCC. The file extension `.cpp` (also `.cc`, `.cxx`) tells the driver to compile as C++. `-std=c++17` picks the language version; C++17 is the sweet spot for this course (`if constexpr`, structured bindings, `std::string_view`, `std::optional`). Multi-file builds work exactly like C — compile each `.cpp` to a `.o`, then link — and `make` works unchanged.

## 3. `<iostream>` vs `printf`

C++ streams are type-safe: the compiler picks the right overload of `operator<<` for each argument, so there is no `%d`/`%f` mismatch bug.

```cpp
#include <iostream>
#include <iomanip>   // std::setprecision, std::fixed, std::setw
#include <cstdio>    // printf still works

int main() {
    double pi = 3.14159265358979;
    int n = 42;
    std::cout << "n = " << n << ", pi = " << pi << '\n';
    std::cout << std::fixed << std::setprecision(3) << pi << '\n';
    std::cout << std::setw(8) << n << "|\n";   // right-aligned in 8 columns
    std::printf("%d %.3f\n", n, pi);            // still fine, still fast
    std::cerr << "errors go here\n";           // unbuffered, like stderr
}
// n = 42, pi = 3.14159
// 3.142
//       42|
// 42 3.142
```

Notes:
- `'\n'` vs `std::endl`: `std::endl` writes a newline **and flushes**. In a loop that prints a million rows, `std::endl` is dramatically slower. Use `'\n'`.
- `std::fixed`/`std::setprecision` are *sticky* — they stay in effect for later output on that stream. `printf` formats are per-call. For numeric tables `printf` is often shorter and clearer; both are legitimate.
- `std::cin >> x` reads one whitespace-delimited token, like `scanf("%lf", &x)` but without the format string. It returns the stream, which converts to `false` on failure: `while (std::cin >> x) { ... }` is the idiomatic read-until-EOF loop.

Python equivalent: `print(f"n = {n}, pi = {pi:.3f}")`. Streams are closer to `sys.stdout.write(str(a) + str(b))`; `printf` is closer to `%`-formatting.

## 4. C headers in C++: `<cstdio>`, `<cmath>`, `<cstdlib>`

Every C standard header `<xxx.h>` has a C++ twin `<cxxx>` that puts the names into namespace `std`:

```cpp
#include <cmath>     // std::sqrt, std::exp, std::fabs  (also plain sqrt etc. usually visible)
#include <cstdlib>   // std::malloc, std::free, std::exit
#include <cstring>   // std::memcpy, std::strlen
#include <cstdint>   // std::int64_t, std::uint8_t
#include <climits>   // INT_MAX
#include <cfloat>    // DBL_EPSILON
```

`<math.h>` still works, but prefer `<cmath>`. One real difference: `<cmath>` overloads `std::sqrt`, `std::abs`, etc. for `float`, `double`, `long double`, so `std::abs(-2.5)` gives `2.5`. In C, `abs(-2.5)` silently truncates to `int` and gives `2` — a classic numerics bug.

## 5. Namespaces and `std::`

A namespace is a named scope for identifiers. The whole standard library lives in `std`. You can make your own to avoid collisions (your `Matrix` vs some library's `Matrix`):

```cpp
namespace linalg {
    struct Matrix { int rows, cols; double *data; };
    double dot(const double *a, const double *b, int n);
}
namespace physics {
    double dot(double ax, double ay, double az, double bx, double by, double bz);
}

// use: linalg::dot(...)  physics::dot(...)
```

Namespaces can be reopened in multiple files (your whole library can be `namespace linalg { ... }` across many headers). They nest: `linalg::detail::helper`. Python equivalent: modules — `std::sqrt` is `math.sqrt`.

## 6. `using` declarations vs `using namespace std;`

```cpp
using std::cout;           // using-declaration: brings ONE name in
using std::string;
using namespace std;       // using-directive: brings EVERYTHING in
```

Rules:
- **Never** put `using namespace std;` in a header. Every file that includes the header gets thousands of names dumped into its global scope, and names like `std::size`, `std::count`, `std::distance`, `std::max` will collide with your own or with C library names. The collision errors are confusing and appear far from the cause.
- In a small `.cpp` file or inside a function body, `using namespace std;` is tolerable. Many codebases ban it everywhere. This course writes `std::` explicitly so you learn where things live.
- `using std::cout;` at file scope in a `.cpp` is fine.
- Namespace alias: `namespace fs = std::filesystem;`

## 7. `bool`

`bool` is a real type with values `true`/`false`, no header needed (C needs `<stdbool.h>`). `sizeof(bool)` is 1 on every mainstream compiler. Integers still convert: `if (ptr)` and `if (n)` work. Printing: `std::cout << true` prints `1` unless you set `std::boolalpha`.

```cpp
bool converged = std::fabs(x_new - x_old) < 1e-9;
std::cout << std::boolalpha << converged << '\n';   // true
```

## 8. `nullptr` vs `NULL`

In C, `NULL` is `((void*)0)` or plain `0`. In C++ `NULL` is usually just `0`, an `int`, which breaks overload resolution:

```cpp
void f(int);
void f(char *);
f(NULL);      // calls f(int)! (or ambiguous) — almost never what you meant
f(nullptr);   // calls f(char*)
```

`nullptr` has its own type `std::nullptr_t` that converts to any pointer type but not to `int`. Use it everywhere you wrote `NULL`. `if (p == nullptr)` and `if (!p)` are both fine.

## 9. `auto` type deduction

`auto` makes the compiler deduce the variable's type from its initializer. It is *not* dynamic typing — the type is fixed at compile time, exactly as if you had written it.

```cpp
auto n = 42;            // int
auto x = 3.0;           // double
auto f = 3.0f;          // float
auto s = "text";        // const char*  (NOT std::string!)
auto p = &x;            // double*
auto sz = sizeof(x);    // std::size_t
```

Use `auto` when the type is obvious from the right side or when it is long (iterators, lambdas — chapter 04). Do not use it where the type carries meaning the reader needs: `double lr = 0.01;` is clearer than `auto lr = 0.01;`. Watch for `auto x = 1;` when you meant `double` — `1/2` is `0`.

`auto` strips references and top-level `const`: `auto y = some_const_ref;` makes a *copy*. Chapter 03 covers `auto&`.

## 10. References: `T&` as a safer out-parameter

A reference is an alias for an existing object. Once bound it always refers to that object, cannot be null, and cannot be reseated. Syntactically you use it like the object itself, not like a pointer.

```cpp
// C style
void min_max_c(const double *a, int n, double *out_min, double *out_max) {
    *out_min = *out_max = a[0];
    for (int i = 1; i < n; i++) { if (a[i] < *out_min) *out_min = a[i];
                                  if (a[i] > *out_max) *out_max = a[i]; }
}
// C++ style
void min_max(const double *a, int n, double &out_min, double &out_max) {
    out_min = out_max = a[0];
    for (int i = 1; i < n; i++) { if (a[i] < out_min) out_min = a[i];
                                  if (a[i] > out_max) out_max = a[i]; }
}
int main() {
    double a[] = {3, -1, 7, 2};
    double lo, hi;
    min_max_c(a, 4, &lo, &hi);   // caller passes addresses
    min_max(a, 4, lo, hi);       // caller passes variables; no & at call site
}
```

The reference version cannot be called with `NULL`, cannot be given an uninitialized pointer, and never needs `*` inside. The downside: at the call site `min_max(a, 4, lo, hi)` does not visibly say "lo and hi get modified" — some style guides prefer pointers for out-params for that reason. Both are used in practice. Chapter 03 goes deep on references.

## 11. `const T&` for passing big things cheaply

Passing a struct by value copies it. Passing by `const T&` passes an alias and promises not to modify:

```cpp
struct Particle { double x, y, z, vx, vy, vz, mass; };   // 56 bytes

double kinetic_energy(const Particle &p) {               // no copy, read-only
    return 0.5 * p.mass * (p.vx*p.vx + p.vy*p.vy + p.vz*p.vz);
}
```

Rule of thumb: pass `int`, `double`, pointers, and tiny structs by value; pass anything bigger than two or three words by `const T&`. Same as `const T*` in C, minus the null case and the `->`.

## 12. Function overloading

Several functions may share a name if their parameter lists differ. The compiler picks by argument types at compile time.

```cpp
double norm(double x, double y) { return std::sqrt(x*x + y*y); }
double norm(double x, double y, double z) { return std::sqrt(x*x + y*y + z*z); }
double norm(const double *v, int n) { double s = 0; for (int i = 0; i < n; i++) s += v[i]*v[i]; return std::sqrt(s); }

norm(3, 4);            // 5    — ints convert to double
norm(1, 2, 2);         // 3
norm(arr, 10);
```

You cannot overload on return type alone. Ambiguities are compile errors, not silent picks. This is how `std::abs`, `std::sqrt`, and `operator<<` work for every type. Under the hood, the compiler *mangles* names (`_Z4normdd`, `_Z4normddd`) so the linker sees distinct symbols — which is exactly why C code needs `extern "C"` (section 25).

## 13. Default arguments

```cpp
double newton(double (*f)(double), double (*df)(double), double x0,
              double tol = 1e-10, int max_iter = 100);

newton(f, df, 1.0);            // tol=1e-10, max_iter=100
newton(f, df, 1.0, 1e-6);      // max_iter=100
```

Defaults go on the declaration (in the header), not the definition, and only trailing parameters may have them. Python equivalent: keyword defaults — but C++ has no keyword arguments, so you cannot skip `tol` and give `max_iter`.

## 14. `constexpr` constants instead of `#define`

```cpp
#define EPS 1e-9                        // C: textual substitution, no type, no scope
constexpr double kEps = 1e-9;           // C++: typed, scoped, visible in the debugger
constexpr int    kMaxIter = 1000;
constexpr double kG = 6.674e-11;        // gravitational constant

constexpr double square(double x) { return x * x; }   // callable at compile time
constexpr double kEps2 = square(kEps);                 // evaluated by the compiler
static_assert(kMaxIter > 0, "need positive iteration cap");
```

`constexpr` variables are implicitly `const` and can be used where a compile-time constant is required (array sizes, template arguments, `static_assert`). A `constexpr` function is an ordinary function that *may* also run at compile time when given constant arguments. Macros still have one job left: conditional compilation (`#ifdef DEBUG`) and include guards.

## 15. Named casts vs C casts

A C cast `(T)x` can mean four different things and the compiler picks silently. C++ splits them:

| Cast | Does | Use for |
|---|---|---|
| `static_cast<T>(x)` | Well-defined conversions: numeric, `void*`→`T*`, up/down class hierarchy | 95% of casts: `static_cast<int>(3.7)`, `static_cast<double>(i) / n` |
| `reinterpret_cast<T>(x)` | Reinterpret bits: pointer↔integer, unrelated pointer types | Bit-hacks (fast inverse sqrt), hardware registers. Often UB if you then dereference |
| `const_cast<T>(x)` | Add or remove `const`/`volatile` | Calling a badly-declared C API that takes `char*` but does not write |
| `dynamic_cast<T>(x)` | Checked downcast in a polymorphic hierarchy (needs virtual functions) | Rare in numeric code |

```cpp
int i = 7, n = 2;
double ratio = static_cast<double>(i) / n;           // 3.5, not 3
int truncated = static_cast<int>(2.99);              // 2
void *raw = std::malloc(16);
double *d = static_cast<double*>(raw);               // C++ requires this (C did it implicitly)
std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(d);
std::free(raw);
```

Named casts are long on purpose: they are greppable and they announce intent. `-Wold-style-cast` warns on every `(T)x` if you want to enforce it.

## 16. `new`/`delete`, `new[]`/`delete[]`

`new T(args)` allocates *and constructs*; `delete p` destroys and frees. `new T[n]` allocates an array; it **must** be released with `delete[]`, never `delete` — mixing them is UB.

```cpp
double *buf = new double[1000];      // like malloc(1000*sizeof(double)), but typed; throws std::bad_alloc on failure
for (int i = 0; i < 1000; i++) buf[i] = 0.0;
delete[] buf;                        // NOT delete buf; NOT free(buf)

int *one = new int(42);              // single object, initialized to 42
delete one;

double *zeroed = new double[1000]();  // () value-initializes -> all zeros (like calloc)
delete[] zeroed;
```

Never mix `malloc`/`delete` or `new`/`free`. In practice you will almost never write `new`/`delete` directly: `std::vector` (chapter 04) and RAII classes (chapter 02) do it for you, and `std::unique_ptr` covers the remaining cases. Knowing `new`/`delete` matters for understanding what those tools do underneath and for reading older code.

## 17. `std::string` first look

```cpp
#include <string>
std::string s = "hello";          // owns its buffer; grows automatically; frees itself
s += " world";                    // append
s.size();                         // 11  (also s.length())
s[0];                             // 'h'  — char, like a C array
s.c_str();                        // const char* for C APIs (printf("%s", s.c_str()))
std::string t = s + "!";          // concatenation makes a new string
if (s == t) {}                    // real content comparison, not pointer comparison
std::string num = std::to_string(3.5);   // "3.500000"
double d = std::stod("2.718");           // string -> double
```

Compare to C: no `strlen` scanning, no `strcpy` overflow, no forgetting `+1` for the terminator, no `strcmp`. `std::string` is the basis of your tokenizer. Chapter 04 covers it fully.

## 18. Range-based `for`

```cpp
double v[] = {1.0, 2.0, 3.0};
for (double x : v) std::cout << x << ' ';       // copies each element into x
for (double &x : v) x *= 2;                     // reference: modifies in place
for (const auto &x : v) std::cout << x << ' ';  // read-only, no copy — the default choice
```

Works on C arrays (size known at compile time), `std::array`, `std::vector`, `std::string`, and anything with `begin()`/`end()`. It does **not** work on a pointer (`double *p`) — the size is unknown. Python equivalent: `for x in v:`. Note that the plain `for (double x : v)` copies; for big element types use `const auto&` (chapter 03 explains the trap).

## 19. Uniform initialization `{}` and narrowing

C++11 lets you initialize anything with braces:

```cpp
int n{5};
double xs[]{1.0, 2.0, 3.0};
struct P { double x, y; };
P p{1.0, 2.0};
std::string s{"abc"};
int zero{};                 // value-initialized: 0. Same for double{} -> 0.0, pointers -> nullptr
```

The key feature: braces **reject narrowing conversions** at compile time.

```cpp
int a = 3.7;      // compiles, a == 3 (silent truncation; -Wconversion would warn)
int b{3.7};       // ERROR: narrowing from double to int
float c{1e40};    // ERROR: doesn't fit
char d{300};      // ERROR
int e{some_long}; // ERROR unless the compiler can prove it fits
```

Prefer `T x{...}` or `T x = ...` for scalars; use `{}` when constructing structs/containers. One trap: `std::vector<int> v(5);` is five zeros, `std::vector<int> v{5};` is one element equal to 5 (chapter 04).

## 20. `enum class`

C enums leak their names into the enclosing scope and convert to `int` silently. `enum class` fixes both:

```cpp
enum Color  { RED, GREEN };              // C-style: RED is in the outer scope, RED == 0 as int
enum class Activation { ReLU, Sigmoid, Tanh };

Activation a = Activation::ReLU;
// int i = a;                    // ERROR: no implicit conversion
int i = static_cast<int>(a);     // explicit: 0
// if (a == RED) {}              // ERROR: different types — good, that comparison was nonsense

switch (a) {
    case Activation::ReLU:    /* ... */ break;
    case Activation::Sigmoid: /* ... */ break;
    case Activation::Tanh:    /* ... */ break;
}   // -Wall warns if you forget a case and have no default
```

You can pick the underlying type: `enum class Token : std::uint16_t { ... }`. Use `enum class` for anything new; use plain `enum` only when you need implicit `int` conversion (bit flags).

## 21. `inline` variables (C++17)

In C, defining a global in a header (`double g_tol = 1e-9;`) breaks the link when two `.c` files include it ("duplicate symbol"). C++17 `inline` variables solve this:

```cpp
// constants.h
inline constexpr double kPi = 3.141592653589793;   // OK in a header, included anywhere
inline int g_debug_level = 0;                      // one shared mutable global, defined in the header
```

(`constexpr` at namespace scope is already implicitly internal-linkage, so `constexpr double kPi` alone also works in headers; `inline` guarantees a single object across translation units, which matters if you take its address.) Functions defined in headers need `inline` too — or be templates, or be class member functions defined in the class body, which are implicitly inline.

## 22. Declaring variables in `if`/`for`/`switch` init

```cpp
for (int i = 0; i < n; i++) {}                // C99 already allowed this

if (double r = discriminant(a, b, c); r < 0)  // C++17: init; condition
    std::cout << "no real roots\n";
else
    std::cout << "roots exist, r = " << r << '\n';   // r visible in else too

if (auto it = table.find(key); it != table.end()) { /* use *it */ }   // the common idiom

switch (int code = status(); code) { case 0: break; default: break; }
```

The variable's scope is exactly the statement, so temporaries do not leak into the surrounding function.

## 23. `std::size` and `std::array`

C's `sizeof(a)/sizeof(a[0])` idiom silently breaks when `a` decays to a pointer. C++17 gives you a checked version and a real fixed-size array type:

```cpp
#include <array>
#include <iterator>     // std::size

double raw[] = {1, 2, 3, 4};
std::size(raw);                    // 4, compile error if raw were a pointer

std::array<double, 3> v = {1.0, 2.0, 3.0};   // fixed size, on the stack, no heap
v.size();                          // 3
v[1];                              // 2.0 — unchecked, like C
v.at(5);                           // throws std::out_of_range
std::array<double, 3> w = v;       // copies (C arrays can't be assigned!)
bool same = (v == w);              // element-wise comparison
double *p = v.data();              // pointer for C APIs
```

`std::array<T,N>` has zero overhead over `T[N]` — same size, same layout — but it copies, compares, knows its size, and does not decay to a pointer when passed to a function. Use it for 3D vectors in physics sims (`std::array<double,3>`). For sizes known only at runtime, `std::vector` (chapter 04).

## 24. `char` literals and `sizeof('a')`

In C, `'a'` has type `int`, so `sizeof('a') == 4`. In C++, `'a'` has type `char`, so `sizeof('a') == 1`. Consequences:

- `std::cout << 'a'` prints `a`; `std::cout << (int)'a'` prints `97`. `printf("%c")` unchanged.
- Overloads can distinguish `f(char)` from `f(int)`.
- `std::cout << s[i]` on a `std::string` prints the character, not a number — but `std::cout << static_cast<int>(s[i])` prints the byte value (useful when you inspect UTF-8 bytes in a tokenizer). Remember `char` may be signed: cast through `unsigned char` first to get `0..255`.

Also `true`/`false` are `bool`, not `int`, and `sizeof(true) == 1`.

## 25. `extern "C"` — linking C code

Because C++ mangles function names to support overloading (section 12), a C++ file calling `mat_alloc` would look for `_Z9mat_allocii` while your C object file exports `_mat_alloc`. `extern "C"` tells the C++ compiler to use C naming and calling convention:

```cpp
// matrix.h — usable from BOTH C and C++
#ifndef MATRIX_H
#define MATRIX_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int rows, cols; double *data; } Mat;
Mat *mat_alloc(int rows, int cols);
void mat_free(Mat *m);
void mat_matmul(const Mat *a, const Mat *b, Mat *out);

#ifdef __cplusplus
}
#endif
#endif
```

Build: `cc -c -std=c11 -O2 matrix.c` then `c++ -std=c++17 -O2 main.cpp matrix.o`. The `__cplusplus` macro is defined only by C++ compilers, so the C compiler never sees `extern "C"`. This exact pattern is how every C library (`libc`, BLAS, SQLite, zlib) is consumed from C++. Your C matrix lib from `../c_learning/` is reusable as-is.

---

## Gotchas and undefined behavior

- **`delete` vs `delete[]` mismatch** is UB. `new[]` ↔ `delete[]`, `new` ↔ `delete`, `malloc` ↔ `free`. Never cross the streams.
- **Modifying a string literal** via `char *s = "x"; s[0] = 'y';` is UB; C++ makes the first line a compile error, which is a feature.
- **`std::endl` in hot loops** is not UB but it is the most common "why is my C++ I/O so slow" cause. Use `'\n'`.
- **`auto s = "text";`** gives `const char*`, not `std::string`. Write `std::string s = "text";` or `auto s = std::string{"text"};` or use the `""s` literal (`using namespace std::string_literals;`).
- **`auto x = 1;` then `x / 2`** — integer division because `x` is `int`. Write `1.0`.
- **Range-for over a pointer** does not compile; range-for over an array parameter (`void f(double a[])`) does not compile either because `a` is a pointer. Pass `std::array` or `std::vector` by reference, or pass a pointer + size.
- **Braces vs parentheses** for containers: `std::vector<int>(5)` ≠ `std::vector<int>{5}`.
- **`NULL` in overloaded calls** resolves to `int`. Use `nullptr`.
- **Uninitialized locals** are still UB to read, exactly as in C. `int x;` is garbage; `int x{};` is zero.
- **Integer overflow** on signed types is still UB, and `-O2` will exploit it. Nothing changed from C here.
- **Using-directive in a header** pollutes every includer. Put `using` inside functions or `.cpp` files only.
- **`sizeof('a')`** changed from 4 to 1. Code that computed buffer sizes with it (rare) breaks.
- **`const` globals have internal linkage by default in C++** (unlike C). `const int N = 5;` in a header is fine and does not cause duplicate-symbol errors; `extern const int N;` is needed only if you want one shared instance.
- **`static_cast<int>(huge_double)`** where the value does not fit in `int` is UB (same as the C cast). Check the range first or use `std::llround` / clamp.

## Common mistakes checklist

- [ ] Compiled with `-std=c++17 -Wall -Wextra` and zero warnings.
- [ ] No `using namespace std;` in any header.
- [ ] `nullptr` everywhere, no `NULL`.
- [ ] `constexpr` for numeric constants, `#define` only for include guards / conditional compilation.
- [ ] `static_cast` for numeric conversions; no C-style casts.
- [ ] `delete[]` paired with `new[]`; better yet, no raw `new` at all.
- [ ] `'\n'` instead of `std::endl`.
- [ ] `const T&` for struct parameters you only read.
- [ ] `enum class`, not plain `enum`, for new enumerations.
- [ ] `std::array` / `std::size` instead of `sizeof(a)/sizeof(a[0])`.
- [ ] `extern "C"` guards in any header shared with C code.

## You can move on when...

- You can compile a `.c` file from the C course as `.cpp` and fix every error the C++ compiler raises (casts from `void*`, `const char*` literals, keyword collisions).
- You can explain why `f(NULL)` picks the wrong overload and `f(nullptr)` does not.
- You can rewrite a C function with two pointer out-parameters using references, and say one downside of doing so.
- You can state the difference between `int a = 3.7;` and `int a{3.7};`.
- You can write the `extern "C"` include-guard pattern from memory and explain what name mangling is.
- You can pick the right named cast for: `double`→`int`, `void*`→`double*`, pointer→integer.
