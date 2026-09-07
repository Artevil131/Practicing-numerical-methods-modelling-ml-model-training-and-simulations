# Chapter 01 — Exercises

Write each exercise as `ex01_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex01_K ex01_K.cpp`. Zero warnings is part of the task.

---

**01.1 — Port a C program**
Take any small program you wrote in `../c_learning/` that uses `malloc`, a `struct`, and `printf` (a dynamic array demo is ideal). Copy it to `ex01_1.cpp` and make it compile as C++17 with no warnings, changing as little as possible. Write, as comments at the top of the file, every line you had to change and why (`void*` conversion, `const char*`, keyword collision, ...).

Example: a C line `int *a = malloc(n * sizeof *a);` becomes `int *a = static_cast<int*>(std::malloc(n * sizeof *a));`.

<details><summary>Hint</summary>
The compiler error list is the to-do list. Read the first error, fix it, recompile. Expect implicit `void*` conversions and string literals assigned to `char*` to be the two main culprits.
</details>

---

**01.2 — Stream formatting table**
Print a table of `x`, `sin(x)`, `exp(-x)` for `x = 0.0, 0.5, ..., 3.0` twice: once with `std::cout` + `<iomanip>` (`std::setw`, `std::fixed`, `std::setprecision(6)`), once with `std::printf`. The two tables must be byte-identical (diff them by redirecting to two files).

Example (first two rows, each column 12 wide, right-aligned):
```
       0.000000    0.000000    1.000000
       0.500000    0.479426    0.606531
```

<details><summary>Hint</summary>
`std::setw` applies only to the next item; `std::fixed` and `std::setprecision` are sticky. `%12.6f` is the printf equivalent.
</details>

---

**01.3 — Overloaded `clamp`**
Write three overloads of `clamp`: `int clamp(int v, int lo, int hi)`, `double clamp(double v, double lo, double hi)`, and `void clamp(double *v, int n, double lo, double hi)` that clamps an array in place. Call all three from `main` and print results. Then add a call `clamp(5, 0.0, 10)` and explain in a comment what the compiler says and why.

Example: `clamp(-3, 0, 10)` → `0`; `clamp(2.5, 0.0, 1.0)` → `1`.

<details><summary>Hint</summary>
Mixed `int`/`double` arguments make neither overload an exact match; the error message will say "ambiguous". Fix by making the argument types consistent or with `static_cast`.
</details>

---

**01.4 — References as out-parameters**
Write `bool solve_quadratic(double a, double b, double c, double &x1, double &x2)` that returns `false` when there are no real roots and otherwise stores the roots (smaller first). Use a C++17 `if (init; cond)` in `main` to compute the discriminant and branch on it. Do not use pointers anywhere.

Example: `solve_quadratic(1, -3, 2, x1, x2)` → `true`, `x1 = 1`, `x2 = 2`. `solve_quadratic(1, 0, 1, ...)` → `false`.

<details><summary>Hint</summary>
Use the numerically stable form: compute the root with the same-sign addition first, then get the other via `c / (a * x1)`. Compare with the naive formula for `a=1, b=1e8, c=1`.
</details>

---

**01.5 — `constexpr` physical constants and `static_assert`**
Create a header `constants.h` with `inline constexpr` values for `kG`, `kC` (speed of light), `kEps0`, `kMu0`, and a `constexpr` function `speed_of_light_from(eps0, mu0)` returning `1/sqrt(eps0*mu0)` — but `std::sqrt` is not `constexpr` in C++17, so implement a `constexpr` Newton-iteration square root yourself. `static_assert` that the computed value is within `1e-3` relative of `kC`. Include the header from two `.cpp` files that both use `kC`, compile them together, and confirm no duplicate-symbol error.

<details><summary>Hint</summary>
A `constexpr` function may contain loops and local variables in C++14 and later. 20 iterations of `x = 0.5*(x + a/x)` starting from `a` converge for these magnitudes. `static_assert` needs a compile-time `bool`, so `fabs` must also be your own `constexpr` helper.
</details>

---

**01.6 — Casts, deliberately**
Write a program that (a) computes the average of an `int` array correctly using `static_cast<double>`, (b) prints the bit pattern of a `float` as a hex `std::uint32_t` using `std::memcpy` (the safe way) **and** comments on why `reinterpret_cast<std::uint32_t&>(f)` would be UB (strict aliasing), (c) calls a function `void legacy_print(char *s)` (that only reads) with a string literal, using `const_cast`. Add `-Wold-style-cast` to your compile flags and confirm zero warnings.

Example: `float f = 1.0f;` → bits `0x3f800000`.

<details><summary>Hint</summary>
`std::memcpy(&bits, &f, sizeof bits)` is the blessed type-punning idiom in C++17 (`std::bit_cast` arrives in C++20). Print hex with `std::hex` or `%08x`.
</details>

---

**01.7 — `enum class` activation dispatcher**
Define `enum class Activation { ReLU, Sigmoid, Tanh, Identity };` and `double apply(Activation a, double x)` with a `switch` and no `default`. Compile with `-Wall -Wextra`; add a fifth enumerator `LeakyReLU` to the enum but not the switch and observe the warning. Then finish it. Also write `const char *name(Activation a)` and print a table of all activations at `x = -1, 0, 1`.

Example: `apply(Activation::Sigmoid, 0)` → `0.5`.

<details><summary>Hint</summary>
To iterate over all enumerators, keep a `std::array<Activation, 5>` listing them, or cast integers `0..4` with `static_cast<Activation>(i)`.
</details>

---

**01.8 — Softmax with `std::array` and range-for**
Write `std::array<double, 5> softmax(const std::array<double, 5> &logits)` using the numerically stable form (subtract the max first). Use range-based `for` exclusively — no index variables anywhere in the function. Verify the outputs sum to `1` within `1e-12` and print them with `std::setprecision(6)`.

Example: input `{1, 2, 3, 4, 5}` → `0.011656 0.031685 0.086129 0.234122 0.636409`.

<details><summary>Hint</summary>
Three range-for loops: find max, compute `exp(x - max)` and accumulate the sum, divide. `std::array` copies on return, which is fine here (40 bytes).
</details>

---

**01.9 — Parse a CSV row of floats**
Write `int parse_row(const std::string &line, double *out, int max_n)` that splits `line` on commas and converts each field with `std::stod`, returning the count (or `-1` if any field fails to parse or there are more than `max_n`). Read rows from `std::cin` with `std::getline` until EOF and print each parsed row's sum. Handle a trailing newline and empty lines gracefully.

Example: input line `1.5,2.5,-4` → prints `sum = 0`. Input `1.5,abc` → prints `parse error`.

<details><summary>Hint</summary>
`std::string::find(',', pos)` returns `std::string::npos` when there are no more commas. `std::stod` throws `std::invalid_argument` on bad input; catch it with `try { ... } catch (const std::invalid_argument &) { ... }` (exceptions are covered later — for now, the syntax is enough) or pre-validate the field. `std::stod` also takes a second parameter reporting how many characters were consumed, which lets you detect trailing junk like `1.5x`.
</details>

---

**01.10 — Reuse your C matrix library via `extern "C"`**
Take the matrix code from the C course (or write a minimal `matrix.c`/`matrix.h` with `mat_alloc`, `mat_free`, `mat_set`, `mat_get`, `mat_matmul`). Add `extern "C"` guards to `matrix.h`. Write `ex01_10.cpp` that includes the header, builds two 2×2 matrices, multiplies them, prints the result with `std::cout`, and frees them. Build with `cc -std=c11 -c matrix.c` followed by `c++ -std=c++17 ex01_10.cpp matrix.o`. Then remove the `extern "C"` guards, rebuild, and paste the linker error into a comment — that error is name mangling.

Example: `[[1,2],[3,4]] · [[5,6],[7,8]]` → `[[19,22],[43,50]]`.

<details><summary>Hint</summary>
The guard is `#ifdef __cplusplus extern "C" { #endif ... #ifdef __cplusplus } #endif`. The linker error will mention a symbol like `__Z9mat_allocii` — the `ii` encodes two `int` parameters.
</details>
