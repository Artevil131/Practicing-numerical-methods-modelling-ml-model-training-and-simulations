# Chapter 17 — Exercises

Write each exercise as `ex17_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++20 -O2 -o ex17_K ex17_K.cpp`. Where an exercise uses a C++23 library
feature, guard it with the feature-test macro (`#include <version>`, `#if __cpp_lib_xxx >= ...`) and
provide a fallback so the file **also** compiles under `-std=c++20`; then build it both ways and
record which path ran. Exercise 17.6 (modules) needs the special commands given there.

---

**17.1 — Feature probe**
Write a program that prints a table of feature-test macros and their values (or `MISSING`) for: `__cpp_concepts`, `__cpp_lib_ranges`, `__cpp_lib_ranges_zip`, `__cpp_lib_ranges_enumerate`, `__cpp_lib_ranges_chunk`, `__cpp_lib_ranges_to_container`, `__cpp_lib_span`, `__cpp_lib_mdspan`, `__cpp_lib_format`, `__cpp_lib_print`, `__cpp_lib_generator`, `__cpp_impl_coroutine`, `__cpp_modules`, `__cpp_lib_expected`, `__cpp_lib_flat_map`, `__cpp_lib_jthread`, `__cpp_lib_chrono`, `__cpp_lib_start_lifetime_as`, `__cpp_lib_stacktrace`. Compile it with `-std=c++17`, `-std=c++20`, `-std=c++23` and diff the three outputs. Then, for each macro that is `MISSING` under `-std=c++23`, write a 3-line test that tries to *use* the feature anyway and record whether the macro was honest (`views::zip` is the interesting case on libc++ 21).

Example: `__cpp_lib_ranges_zip MISSING` under `-std=c++23`, yet `std::views::zip(a, b)` compiles.

<details><summary>Hint</summary>
`#ifdef M / #else` around a struct initializer; you cannot stringify a macro's absence any other way. The `-std=c++17` build will fail on `<version>`? No — `<version>` exists in C++17 mode on libc++; only the values differ.
</details>

---

**17.2 — Ranges pipeline over a CSV of floats**
Given a `std::string` holding CSV lines (`"1.0,2.0,3.0\n4.0,5.0,6.0\n"`), build a **single lazy pipeline** with `views::split('\n')`, `views::filter` (skip empty lines), `views::transform` (line → `std::vector<double>` via `split(',')` + `std::from_chars`), and materialize with `std::ranges::to<std::vector>` (C++23) or your own `to_vector`. Then compute per-column means with `views::iota(0, ncols) | views::transform(column_mean)`. Add a counter to the line-parsing lambda and prove laziness: with `| views::take(1)` only one line is parsed. Finally, compare the time of this pipeline against a hand-written `getline`/`strtod` loop for 100 000 lines.

Example: `means = [2.5, 3.5, 4.5]`, `parsed 1 line(s) when take(1) applied`.

<details><summary>Hint</summary>
A subrange of a contiguous `std::string` has `.data()`/`.size()` → `std::string_view`. `std::from_chars(sv.data(), sv.data() + sv.size(), value)` (works for `double` on libc++ ≥ 20). Expect the ranges version within ~1.2× of the loop.
</details>

---

**17.3 — Custom view: `stride_view` for a tensor axis**
`std::views::stride` is missing from libc++ 21. Implement `my::stride_view<V>` (random-access base, `view_interface`, iterator with `+=`/`-`/`[]` so it is itself random access) and a range adaptor closure `my::stride(n)` supporting `r | my::stride(n)`. Use it to view column `j` of a row-major `std::vector<double>` (`data | drop(j) | my::stride(cols)`), compute the column sum, and `std::ranges::sort` a *strided column in place*. Add `static_assert`s that your view models `std::ranges::random_access_range`, `std::ranges::sized_range`, and `std::ranges::view`. Under `-std=c++23`, if `__cpp_lib_ranges_stride` is defined, compare against `std::views::stride`.

Example: for a 3×4 matrix `0..11`, `column(1) = 1 5 9`, sum 15; after sorting column 1 descending the matrix reads `0 9 2 3 / 4 5 6 7 / 8 1 10 11`.

<details><summary>Hint</summary>
Iterator distance for the last partial stride: `end` is `begin + ceil(size / n) * n` clipped — easiest is to store the *count* of elements and compute `cur = begin + k*n` from an index `k`. `iterator_concept = std::random_access_iterator_tag` plus all of `++ -- += -= + - [] <=>`. `std::ranges::sort` requires random access *and* `std::sortable` (the reference must be assignable) — a `transform` view of a projection wouldn't qualify; yours will.
</details>

---

**17.4 — `std::format` for `Matrix<T>` and a `std::span`-based `print_matrix`**
Specialize `std::formatter<Matrix<T>>` for your Chapter 05/06 `Matrix<T>` (or a minimal 2-D class) so that `std::format("{:8.3f}", m)` prints rows on separate lines with each element formatted by the *inner* spec, and `std::format("{}", m)` uses a default. Support a custom extra flag: `"{:8.3f|t}"` prints the transpose. Write `void print_matrix(std::span<const double> data, std::size_t rows, std::size_t cols, std::string_view spec)` that uses `std::vformat` because the spec is only known at runtime. Under `-std=c++23` with `__cpp_lib_print`, use `std::println`; else the `fputs(std::format(...))` shim.

Example: `std::format("{:6.2f}", I2)` → `"  1.00   0.00\n  0.00   1.00\n"`.

<details><summary>Hint</summary>
Inherit from `std::formatter<T>` for the numeric spec and override `parse` to consume your `|t` suffix before delegating the rest to the base `parse`. `parse` must return the iterator at the closing `}`. `std::make_format_args(x)` for `vformat`.
</details>

---

**17.5 — `Generator<T>` batching with shuffling and epochs**
Implement `Generator<T>` (or reuse the one in `example.cpp`) and write `Generator<std::span<const std::size_t>> epoch_batches(std::size_t n, std::size_t batch_size, unsigned seed)` that, *per epoch*, shuffles an index permutation with `std::mt19937` and yields batches of indices; `Generator<Batch> train_batches(std::span<const float> X, std::span<const float> y, std::size_t n_features, std::size_t batch_size, int epochs)` that composes the first generator and yields `Batch{ std::vector<float> xb; std::vector<float> yb; int epoch; }`. Verify each epoch visits every index exactly once, that the last batch is short when `n % batch_size != 0`, and that destroying the generator early (break after 3 batches) leaks nothing (run under ASan; count frames with a static counter in `promise_type`'s constructor/destructor). Then write the same thing as a class with `next()` and compare line counts and readability in a comment.

Example: `n=10, batch=4 → 4,4,2 per epoch; 3 epochs → 30 indices, each of 0..9 seen 3 times`.

<details><summary>Hint</summary>
Nested generators: iterate the inner one with range-for inside the outer coroutine — each `co_yield` in the outer suspends the outer frame only. Coroutine frames are heap-allocated: `promise_type` can have a ctor/dtor to count them, and the counter must return to zero.
</details>

---

**17.6 — Modules on Apple clang 21**
Create `linalg.cppm` exporting `namespace linalg { export double dot(std::span<const double>, std::span<const double>); export struct Vec3 {...}; }` (with `import <span>;` or a global module fragment `module; #include <span> export module linalg;`), and `main.cpp` that does `import linalg;`. Build with the two-step command from `lesson.md` §12 (`-fcxx-modules --precompile` then link the `.pcm`), then (a) remove `-fcxx-modules` and record the exact error, (b) try `import std;` and record the error, (c) measure compile time of `main.cpp` with `import linalg;` vs `#include "linalg.hpp"` (a header with the same content) using `time` and `-ftime-trace`, 10 repetitions each, and (d) write a `Makefile` (or CMake ≥ 3.28 with `CXX_SCAN_FOR_MODULES`) that rebuilds the `.pcm` when the `.cppm` changes.

Example: without `-fcxx-modules`: `error: expected template` at `export module linalg;`.

<details><summary>Hint</summary>
`c++ -std=c++20 -fcxx-modules --precompile -x c++-module -o linalg.pcm linalg.cppm` then `c++ -std=c++20 -fcxx-modules -fmodule-file=linalg=linalg.pcm main.cpp linalg.pcm -o main`. Non-inline function definitions in the module unit are compiled once when you pass the `.pcm` (or a `.o` built from it) to the link step.
</details>

---

**17.7 — `<=>` for a `Fraction` and a `Version` parser with `std::expected`**
Write `class Fraction { long num, den; }` normalized by `std::gcd` with `std::strong_ordering operator<=>(const Fraction&) const` implemented by cross-multiplication in `__int128` (or `long double` guard), and `bool operator==` defaulted from it — then show that `Fraction{1,2} == Fraction{2,4}`, `<`, `>=`, and `!=` all work from those two functions alone; count how many comparison operators you wrote (2) vs C++17 (6). Add heterogeneous `operator<=>(long)` and demonstrate `3 < f` via the reversed candidate. Then write `std::expected<Version, ParseError> parse_version(std::string_view)` under `__cpp_lib_expected` (fallback: `std::optional<Version>` + an out-parameter error code), with an `enum class ParseError` and a `std::formatter<ParseError>`; sort a `std::vector<Version>` parsed from `{"1.10.0", "1.9.9", "2.0.0-rc1"}` and print rejected inputs with their error.

Example: `"1.9.9" < "1.10.0"` (numeric, not lexicographic); `"2.0.0-rc1"` → `ParseError::TrailingGarbage`.

<details><summary>Hint</summary>
`auto operator<=>(const Fraction&) const = default;` would compare `num` then `den` lexicographically — wrong for fractions; write it by hand. `std::from_chars` for each component; the monadic `and_then`/`transform`/`or_else` on `expected` chain the three parses.
</details>

---

**17.8 — `std::mdspan` (or a fallback) matmul with layout policies (ML)**
Write `template <class Layout> void matmul(mdspan<const double, dextents<size_t,2>, Layout> A, ... B, mdspan<double, ..., layout_right> C)` using `std::mdspan` under `__cpp_lib_mdspan` and your own `MdSpan2<T, Layout>` otherwise. Benchmark 512×512 for A,B both `layout_right`, both `layout_left`, and mixed; explain the timing differences from the inner-loop stride. Then implement `transpose_view(A)` using `std::layout_stride` (swap extents and strides — zero copy) and verify `matmul(A, transpose_view(A))` equals `A·Aᵀ` computed naively. Report GFLOP/s for each case.

Example: `right×right 0.31 s, left×left 0.30 s, right×left 0.95 s (inner loop strides through B)`.

<details><summary>Hint</summary>
Row-major `A(i,k) * B(k,j)` with `j` innermost streams through `B`'s row — good. With `B` in `layout_left`, `j` innermost jumps by `rows` doubles — bad; swap loops or transpose. `m.stride(0)`, `m.stride(1)`, `m.extent(d)`. `std::layout_stride::mapping<Ext>{ext, std::array{s0, s1}}`.
</details>

---

**17.9 — Particle simulation with a generator, `zip`, and `jthread` (sims)**
Use `Generator<Particle>` to produce `N = 20 000` initial particles, store them SoA (`std::vector<double> x, y, vx, vy`), then advance 200 leapfrog steps under a central gravity field. Write the update loop three ways and time each: (a) index loop, (b) `views::zip(x, vx)` (C++23; fallback to (a) with a note), (c) two `std::jthread`s each owning half of the arrays via `std::span`, synchronized per step with a `std::barrier`, and stopped at the end via `stop_token`. Compute total energy every 50 steps with `std::transform_reduce` and print it with `std::format("{:+.6e}")`; it should stay constant to ~1e-6 relative.

Example: `step 200: E = -4.98123e+02 (drift 3.1e-7); loop 12.1 ms, zip 12.4 ms, 2 threads 6.9 ms`.

<details><summary>Hint</summary>
Leapfrog: `v += a(x)*dt/2; x += v*dt; v += a(x)*dt/2`. `std::barrier<> sync(2)` and `sync.arrive_and_wait()` at each step; the jthread lambda's first parameter is the `stop_token`. Keep the `Generator` for *initialization only* — a coroutine resume per particle per step would dominate the inner loop.
</details>

---

**17.10 — Migration audit of your Matrix/Tensor project (ML)**
Take your Project P01/P02 code (or the Chapter 05 `Matrix<T>`) and apply the migration guide from `lesson.md` §20: (1) replace raw `(T*, size_t)` kernel parameters with `std::span`, (2) add a `view()` returning `std::mdspan` (guarded) or your `MdSpan2`, (3) replace `printf`/`<<` output with a `std::formatter<Matrix<T>>`, (4) constrain templates with concepts (`std::floating_point`, a `MatrixLike` concept) and record one *before/after* compiler error message for a misuse, (5) replace one index-heavy loop with a ranges pipeline and one with `std::ranges::transform`, (6) add `operator<=>` where an ordering makes sense (e.g. `Shape`) and `operator==` elsewhere, (7) mark hot-loop branches `[[likely]]`/`[[unlikely]]` only where a profile shows they matter, and (8) leave coroutines and modules out, with a comment saying why. Benchmark the three hottest kernels before and after: the requirement is **no regression** > 2%. Deliver a short `MIGRATION.md` listing each change, the diff size, and the timing table.

Example: `dot: 0.412 → 0.409 ms; matmul 256: 14.2 → 14.3 ms; format(m) replaces 40 lines of printf`.

<details><summary>Hint</summary>
`std::span<const T>` is implicitly constructible from `std::vector<T>&`, `std::array`, and `(ptr, n)`, so call sites don't change. Keep `-Wall -Wextra -Wconversion`; the span conversions surface every signed/unsigned index mismatch you had been ignoring.
</details>
