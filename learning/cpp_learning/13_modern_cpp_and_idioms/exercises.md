# Chapter 13 — Exercises

Write each exercise as `ex13_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex13_K ex13_K.cpp`. Exercise 13.7 additionally asks for a
`-std=c++20` build. Several exercises ask you to *provoke* a compile error and record the message
in a comment — that's part of the deliverable, not a failure.

---

**13.1 — Structured bindings and `if constexpr`**
Write `struct Stats { double mean, var; std::size_t n; }` and `Stats stats(const std::vector<double>&)`. In `main`, unpack it with a structured binding, iterate a `std::map<std::string, std::vector<double>>` with `const auto& [name, values]`, and print per-key stats. Then write `template <class T> std::string fmt(const T& x)` that uses `if constexpr` to format floating point with 3 decimals (`snprintf`), integers as-is, and anything else as `"<" + std::to_string(sizeof(T)) + " bytes>"`. Show that a non-`constexpr` `if` version fails to compile for the third case, and record the error.

Example: `fmt(2.0/3)` → `0.667`; `fmt(42)` → `42`; `fmt(Stats{})` → `<24 bytes>`.

<details><summary>Hint</summary>
`std::is_floating_point_v<T>`, `std::is_integral_v<T>`. The plain `if` version fails because `std::to_string(Stats)` has to compile even in the untaken branch.
</details>

---

**13.2 — `std::string_view` tokenizer (ML)**
Write `std::vector<std::string_view> split(std::string_view text, std::string_view delims)` that splits on any delimiter character with no allocation except the output vector, and `std::unordered_map<std::string, int> build_vocab(const std::vector<std::string_view>&)` (only here do you construct `std::string`s). Tokenize a paragraph, print the 5 most frequent tokens. Then write the *bug*: a function that returns a `string_view` into a local `std::string` — compile with `-fsanitize=address` and record what happens when you print it. Delete the bug.

Example: `split("a b,c", " ,")` → `{"a", "b", "c"}`.

<details><summary>Hint</summary>
`std::string_view::find_first_of` / `find_first_not_of`; `remove_prefix`. For the top-5, copy the map into a `std::vector<std::pair<std::string,int>>` and `std::partial_sort`.
</details>

---

**13.3 — Autograd ops as `std::variant` (ML)**
Define `struct Leaf{double v;}; struct Add{int a,b;}; struct Mul{int a,b;}; struct Tanh{int a;}; struct Pow{int a; double p;};` and `using Op = std::variant<Leaf, Add, Mul, Tanh, Pow>`. A `Graph` holds `std::vector<Node>` where `Node { Op op; double value = 0, grad = 0; }`, and nodes are appended in topological order. Implement `forward()` with a visitor struct and `backward()` (reverse iteration, chain rule) with the `overloaded{}` lambda idiom. Test on `f(x, y) = tanh(x*y + x^2)` at `x = 0.5, y = -1.2` and compare gradients with central finite differences (`h = 1e-6`). Then add a sixth op `Exp` and note that every non-exhaustive visitor fails to compile until you handle it — record the error message.

Example: `df/dx` analytic and numeric agree to `1e-8`.

<details><summary>Hint</summary>
`d tanh(u)/du = 1 - tanh(u)^2`; `d(a^p)/da = p a^(p-1)`. In `backward`, seed `grad = 1` on the output, then for each node from last to first, `std::visit` a lambda that adds `node.grad * local_derivative` to each input's `grad`.
</details>

---

**13.4 — Strong types and `explicit` for `Matrix`**
Write `struct Rows { std::size_t v; }; struct Cols { std::size_t v; };` and a `Matrix(Rows, Cols)` constructor plus an `explicit Matrix(std::size_t n)` square constructor. Show three compile errors, each recorded in a comment: (a) `Matrix m(Cols{4}, Rows{3});`, (b) `Matrix m = 5;`, (c) passing a bare `std::size_t` where `Rows` is expected. Then add `struct Seconds { double v; }` and `struct Steps { int v; }` and a function `simulate(Seconds dt, Steps n)`; show that `simulate(Steps{100}, Seconds{0.01})` does not compile. Finally, give `Rows`/`Cols` `explicit constexpr` constructors so `Rows{3.5}` also fails.

<details><summary>Hint</summary>
Aggregate initialisation `Rows{3}` works with no constructor; adding `explicit constexpr Rows(std::size_t v) : v(v) {}` keeps the syntax and rejects narrowing.
</details>

---

**13.5 — Rule of Zero vs Rule of Five**
Write two classes: `Buffer` (holds a `std::vector<double>` and a `std::string name` — Rule of Zero: no special members) and `CFile` (wraps a `FILE*` — Rule of Five: destructor closes, copies deleted, moves transfer ownership and are `noexcept`). Use `static_assert` to verify `std::is_nothrow_move_constructible_v` for both, `std::is_copy_constructible_v<Buffer>` and `!std::is_copy_constructible_v<CFile>`. Put 100 `Buffer`s in a `std::vector` via `emplace_back` without `reserve` and count copies by temporarily adding a counting copy constructor — then remove it and explain in a comment why adding it broke the Rule of Zero (and what else it silently disabled).

<details><summary>Hint</summary>
Declaring a copy constructor suppresses the implicit move constructor. With no move, `vector` growth copies. Re-adding `= default` for the four others restores it — which is the Rule of Five.
</details>

---

**13.6 — `std::function` vs template: measure the difference**
Implement `template <class F> double integrate_t(F f, double a, double b, int n)` (midpoint rule) and `double integrate_f(const std::function<double(double)>& f, double a, double b, int n)`. Integrate `sin(x)*exp(-x)` over `[0, 5]` with `n = 20'000'000` through both. Use the `bench_ms` harness from `../12_performance/exercises.md` 12.1 (copy it in). Report both times and the ratio. Then compile with `-Rpass-missed=loop-vectorize` and record why the `std::function` version's loop did not vectorize. Write a two-sentence rule in a comment for when each is appropriate.

Example: `template 38 ms | std::function 95 ms | ratio 2.5x`.

<details><summary>Hint</summary>
The template version inlines the lambda into the loop body; the `std::function` version makes an indirect call per element, which the vectorizer cannot see through.
</details>

---

**13.7 — C++20 tour (compile with `-std=c++20`)**
Write `ex13_7.cpp` that uses: a `concept Number` constraining `template <Number T> T dot(std::span<const T>, std::span<const T>)`; a `struct Config { double lr = 0.01; int epochs = 10; bool verbose = false; }` created with designated initializers; `auto operator<=>(const Version&) const = default;` on a `struct Version { int major, minor, patch; }` and a sort of versions; a ranges pipeline (`views::filter | views::transform | views::take`) over `std::vector<double>`; and `std::format` for all output. Record at the top of the file which features compiled on your Apple clang 21 and the value of `__cpp_lib_format`, `__cpp_lib_ranges`. Then try `dot` on a `std::vector<std::string>` and paste the concept error message.

<details><summary>Hint</summary>
`template <class T> concept Number = std::integral<T> || std::floating_point<T>;`. `std::span<const T>` constructs from `const std::vector<T>&` implicitly. Print `__cpp_lib_format` with `#ifdef` guards so the file still compiles if a macro is missing.
</details>

---

**13.8 — `std::filesystem` checkpoint manager (ML)**
Write a program that creates `checkpoints/run1/` (with `create_directories`), writes 5 fake checkpoint files `step_000100.bin` … `step_000500.bin` (a few bytes each), then: lists all `.bin` files sorted by the step number parsed from the `stem()`, prints each with `file_size`, deletes all but the newest 2, and prints the total bytes remaining. Handle a non-existent directory with the `std::error_code` overloads (no exceptions) and a permission problem with a caught `fs::filesystem_error`. Clean up the directory at the end with `remove_all`.

Example line: `step_000500.bin  16 bytes  (kept)`.

<details><summary>Hint</summary>
`fs::directory_iterator`, `entry.path().extension() == ".bin"`, `std::stoi(stem.substr(5))`. `fs::remove(p, ec)` never throws. `std::sort` with a lambda on the parsed step.
</details>

---

**13.9 — Refactor a "C-in-C++" N-body step to idiomatic C++ (sims)**
Start from this deliberately bad code (copy it in): raw `new double[3*n]` arrays for positions/velocities, `#define G 6.674e-11`, `#define DT 0.01`, `int` loop indices compared with `size_t`, a plain `enum { EULER, LEAPFROG }`, `(float)` C-style casts, a `Body* bodies = (Body*)malloc(...)` never freed, `using namespace std;` at the top, `std::endl` in the output loop, and `NULL` checks. Refactor step by step to the chapter's subset: SoA `struct Bodies { std::vector<double> x, y, z, vx, vy, vz, m; }` (Rule of Zero), `constexpr` constants in a `namespace phys`, `enum class Integrator`, `std::size_t` indices, `static_cast`, a `Seconds` strong type for `dt`, `std::span`-style `const double*` + `n` kernel signature, `'\n'`. After each refactor step, run and confirm the final positions after 100 steps are bit-identical to the original. List in a comment which "avoid" table rows you eliminated.

<details><summary>Hint</summary>
Do the refactor in the order that keeps the program compiling at every step. Bit-identical output is achievable because you are changing only storage and types, not arithmetic order — if it changes, you reordered a sum.
</details>

---

**13.10 — Type-erased `Layer` vs `std::variant<...>` layers: build both, decide (ML)**
Implement a 3-layer MLP forward pass (`Linear → ReLU → Linear`) on a `std::vector<double>` input two ways. (A) `std::variant<Linear, ReLU, Softmax>` with `std::visit` in a `for (auto& layer : layers)` loop. (B) type erasure: `class Layer` holding a `std::unique_ptr<Concept>` where `Concept` has `virtual std::vector<double> forward(const std::vector<double>&) = 0` and `template <class T> struct Model : Concept` wraps any type with a `.forward()` — no inheritance required of `Linear`/`ReLU`. Both must produce identical output for identical seeded weights. Time 10,000 forward passes of a `256 → 512 → 10` network for both. Then write a 5-line comment answering: which one would you use for *your* autograd project, and what would make you switch? Also `[[nodiscard]]` the `forward` functions and confirm the warning fires when the result is dropped.

<details><summary>Hint</summary>
Both designs will run at nearly the same speed — the matmul dominates. The decision is about *closed vs open* set of layer types and compile-time exhaustiveness vs plugin flexibility, not speed. Preallocating the activation buffers (Chapter 12) matters more than either.
</details>
