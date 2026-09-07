# Chapter 14 — Exercises

Write each exercise as `ex14_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++20 -O2 -o ex14_K ex14_K.cpp`. Most checks in this chapter are
`static_assert`s — a program that compiles *is* the passing test, so several exercises also ask you
to provoke a compile error and record the first line of the diagnostic in a comment. Where an
exercise says "measure", use `-ftime-trace` or `/usr/bin/time -p c++ ...` and record the numbers.

---

**14.1 — Deduction and forwarding, observed**
Write `template <class T> void probe(T&& x)` that prints, using `std::is_lvalue_reference_v<T>`,
`std::is_const_v<std::remove_reference_t<T>>` and `sizeof(T)`-style checks, the category and
constness of what it received. Call it with an `int` variable, a `const int`, a literal, a
`std::move`d variable, a `std::string` temporary, and an array `double a[4]`. Then write a `Tracker`
type that logs copies and moves, and a factory `template <class T, class... A> T build(A&&... a)`;
show with the log that `build<Holder>(tracker)` copies and `build<Holder>(std::move(tracker))` moves.
Finally replace `std::forward<A>(a)...` with `a...` and with `std::move(a)...` and record what each
does to the two calls.

Example: `probe(ci) -> lvalue, const, T = const int&` / `build(lvalue): copy;move;`.

<details><summary>Hint</summary>
`T` for an lvalue argument is a reference type; for an rvalue it isn't. `std::move(a)...` on an lvalue argument steals from the caller's variable — check it is empty afterwards. The array case: `T` deduces to `double (&)[4]`, no decay through a reference.
</details>

---

**14.2 — Variadic three ways + `tuple_for_each`**
Implement `min_all(a, b, c, ...)` (any arithmetic types, result type `std::common_type_t<...>`) three times: recursive, fold expression, and via `std::apply` over a tuple. Implement `tuple_for_each(tuple, f)` with `std::index_sequence` and use it to print a `std::tuple<int, std::string, double, char>` with element indices. Add `tuple_transform(tuple, f)` returning a new tuple of the results. Then compile each `min_all` with `-ftime-trace` for a 40-argument call and compare the `InstantiateFunction` count.

Example: `min_all(3, 1.5, 2) == 1.5` (type `double`); `[0]=1 [1]=two [2]=3.0 [3]=x`.

<details><summary>Hint</summary>
For the fold version keep a running minimum with the comma fold `((m = x < m ? x : m), ...)`. `tuple_transform` is `std::make_tuple(f(std::get<I>(t))...)` inside an `index_sequence` helper.
</details>

---

**14.3 — Traits: `void_t` detection, then the same as concepts**
Write detection traits `has_forward<T>` (`t.forward(double)` exists), `has_backward<T>`, and
`is_layer<T>` (both), with `void_t`; then `element_type_t<T>` that yields `T::value_type` if present,
`T` for arithmetic types, and the element type of a raw pointer/array otherwise. Write each of them a
second time as a concept (`Layer`, `HasForward`) or a `requires`-based alias. `static_assert` all of
them against `std::vector<float>`, `double`, `int[3]`, `float*`, a struct with only `forward`, and a
full layer struct. Provoke one failure of each style and paste the first error line into a comment.

<details><summary>Hint</summary>
For arrays: `std::remove_extent_t`; for pointers: `std::remove_pointer_t`; combine with `std::conditional_t` or three partial specializations. The concept version of `is_layer` is `HasForward<T> && HasBackward<T>` — note it now also *subsumes* both.
</details>

---

**14.4 — Rewrite a real SFINAE header with concepts**
Take this C++17 overload set (typical of a numerics library) and rewrite it with concepts, keeping behaviour identical:

```cpp
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
T   mean(const std::vector<T>& v);                                 // integer average, rounds toward zero
template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
T   mean(const std::vector<T>& v);                                 // Kahan-summed average
template <class C, class = std::void_t<decltype(std::declval<C>().begin()), typename C::value_type>,
          std::enable_if_t<!std::is_same_v<C, std::vector<typename C::value_type>>, int> = 0>
auto mean(const C& c) -> typename C::value_type;                   // any other range with begin(): plain average
```
Your version must still choose Kahan for `std::vector<double>`, integer rounding for `std::vector<int>`, and the generic path for `std::array<double, 4>`, `std::list<float>` and `std::span<const double>`. Use standard concepts (`std::ranges::range`, `std::floating_point`) and one concept of your own. Demonstrate that `mean(std::string("abc"))` fails to compile and record both the old and new diagnostic.

<details><summary>Hint</summary>
Order the overloads by subsumption instead of by mutual exclusion: `std::ranges::range<C>` for the generic one, and a more constrained `std::same_as<C, std::vector<...>>`-style concept for the vector ones — or simply constrain the vector overloads with `template <std::floating_point T> T mean(const std::vector<T>&)`; a non-template-parameter `std::vector<T>` is more specialized than `const C&` by partial ordering already.
</details>

---

**14.5 — CRTP: `Comparable`, `Arithmetic`, and a static `Integrator`**
Write mixins `Comparable<D>` (from `<` and `==` derive the other four), `Arithmetic<D>` (from
`D& operator+=(const D&)` and `*=(double)` derive `+`, `-`, `*`, `/`, unary `-`), and `Counted<D>` (live-instance count). Build `struct Vec3 : Arithmetic<Vec3>, Comparable<Vec3>, Counted<Vec3>` and `struct Money : Arithmetic<Money>, Comparable<Money>`. Then write `template <class D> struct Integrator { void run(int n); }` calling `derived().step(dt)`, with `Euler` and `RK4` derived types integrating `x'' = -x`; verify RK4's energy drift is ~1e-8 after 1000 steps and Euler's is not. Compare with a `virtual step()` version: same results, and measure both loops with 10⁸ steps at `-O2` (`Timer` from Chapter 12).

Example: `Euler drift 4.9e-01, RK4 drift 2.3e-09 ; static 0.31 s, virtual 0.44 s`.

<details><summary>Hint</summary>
Hidden friends: define the operators as `friend` inside the mixin, taking `const D&`. For `Arithmetic`, `operator+` is `D r = a; r += b; return r;` — it needs `D` to be copyable. The virtual version's cost is mostly lost inlining, not the indirect call itself.
</details>

---

**14.6 — `constexpr` tables and `consteval`**
Compute at compile time: (a) a `std::array<double, 1024>` of `sin(2πi/1024)` with a constexpr Taylor series and range reduction, `static_assert`ed within 1e-12 of a few known values; (b) a `std::array<double, 64>` of `exp(-x²)` for x in `[0, 4]` (constexpr `exp` via series); (c) a 256-entry `popcount` table and a 256-entry bit-reverse table (for FFT index permutation); (d) a `consteval std::uint64_t hash(std::string_view)` used as `switch` cases for activation names. Add a `constinit` global step counter and show that making it `constexpr` fails to compile when you mutate it. Measure compile time with and without the tables (`-ftime-trace`) and record the difference. Then, at runtime, verify each table against `<cmath>` and `std::popcount`.

<details><summary>Hint</summary>
Range-reduce `x` to `[-π, π]` before the series or it won't converge for large `x`. For `exp`, `1 + x + x²/2! + …` with 20 terms is accurate to 1e-15 on `[0, 4]`. If clang says "constexpr evaluation hit maximum step limit", raise `-fconstexpr-steps` or reduce the loop.
</details>

---

**14.7 — Class-type NTTPs and policies: `Matrix<T, Storage, Layout>`**
Implement `struct Shape { std::size_t rows, cols; };` and `StaticMatrix<Shape S>` (data in a `std::array`), with `matmul` whose `requires (A.cols == B.rows)` rejects mismatched shapes at compile time. Then implement a runtime-sized `Matrix<T, StoragePolicy = Heap, LayoutPolicy = RowMajor>` with `Heap`, `Stack<N>`, `Aligned64` (aligned heap allocation via `std::aligned_alloc` or `operator new(size, std::align_val_t)`) storage policies, and `RowMajor`/`ColMajor` layout policies deciding the index formula. Check `sizeof` of each combination, `static_assert` the alignment of `Aligned64`, and write one free function `template <class M> double trace(const M&)` that works for every combination via a concept `MatrixLike`.

Example: `sizeof(Matrix<double, Stack<9>>) == 88`; `matmul(StaticMatrix<Shape{2,3}>, StaticMatrix<Shape{2,3}>)` → compile error `constraints not satisfied`.

<details><summary>Hint</summary>
A policy is a class with a nested `template <class T> struct type` (or `using`), so the host writes `typename Storage::template type<T> s_;`. The layout policy needs only `static std::size_t index(i, j, rows, cols)`. For `Aligned64`, pair `operator new(n, std::align_val_t{64})` with the matching `operator delete(p, std::align_val_t{64})` in a `unique_ptr` deleter.
</details>

---

**14.8 — Expression templates for `Tensor1D` with broadcasting and reductions (ML)**
Implement expression templates over a `Vec` (owning `std::vector<double>`): element-wise `+ - * /`, scalar broadcast on either side, unary functions as nodes (`exp(e)`, `tanh(e)`, `relu(e)`), and lazy reductions `sum(e)`, `dot(a, b)`, `max(e)` that never allocate. Build `softmax(x) = exp(x - max(x)) / sum(exp(x - max(x)))` and `mse(pred, target) = sum((pred - target) * (pred - target)) / n` from these nodes. Count evaluations (a static counter incremented in `Vec::assign`) and assert `softmax` on a 10⁶-element vector causes exactly one `Vec` allocation and that results match a naive NumPy-style implementation to 1e-12. Bench both at n = 10⁷ and report the ratio. Then demonstrate the `auto` trap: `auto e = a + b;` with `a` destroyed before use, run under `-fsanitize=address` and record the report.

Example: `softmax(1e6): evaluations=1, allocs=1, max|diff| = 4.4e-16, naive 41 ms / ET 12 ms`.

<details><summary>Hint</summary>
`max(x)` and `sum(exp(x - m))` are reductions that consume an expression: `template <IsExpr E> double sum(const E& e)` loops `e[i]`. Store nested nodes by value and `Vec` by `const&` (`std::conditional_t`). Unary nodes: `template <class E, class F> struct UnOp { operand_t<E> e; double operator[](i) const { return F::apply(e[i]); } }`.
</details>

---

**14.9 — `Sequential<Layers...>` as a tuple, with compile-time shape checking (ML)**
Write layer types templated on their sizes: `Linear<In, Out>` (`std::array<double, In*Out>` weights, `forward(std::array<double, In>) -> std::array<double, Out>`), `ReLU<N>`, `Softmax<N>`. Write `template <class... Ls> struct Sequential { std::tuple<Ls...> layers; }` whose `forward` folds the input through every layer using `std::index_sequence`/`std::apply`, and whose constructor `static_assert`s that each layer's `Out` equals the next layer's `In` (a recursive trait or a fold over `index_sequence` comparing `std::tuple_element_t<I, ...>::out == std::tuple_element_t<I+1, ...>::in`). Add `template <std::size_t I> auto& layer()` and `constexpr std::size_t num_params()` (fold over `Ls::params...`). Show `Sequential<Linear<4,8>, ReLU<8>, Linear<8,3>, Softmax<3>>` running on one input, and that `Sequential<Linear<4,8>, Linear<7,3>>` fails at compile time with your message.

<details><summary>Hint</summary>
Each layer exposes `static constexpr std::size_t in, out;`. The chain check: `template <class Tup, std::size_t... I> constexpr bool chained(std::index_sequence<I...>) { return ((std::tuple_element_t<I, Tup>::out == std::tuple_element_t<I + 1, Tup>::in) && ...); }` called with `make_index_sequence<sizeof...(Ls) - 1>`. `forward` is a left fold: `auto out = std::get<0>(layers).forward(x); ` then repeat via index sequence or recursion on `I`.
</details>

---

**14.10 — Compile-time lattice and kernels for LBM / FDTD (sims)**
For a D2Q9 lattice Boltzmann solver, compute at compile time the nine discrete velocities `c_i` (as `std::array<std::array<int, 2>, 9>`), the weights `w_i` (4/9, 1/9, 1/36 pattern), and the opposite-direction table `opp[i]` (for bounce-back), all `static_assert`ed (weights sum to 1, `c[opp[i]] == -c[i]`). Then write `template <Shape S> struct Grid` (class-type NTTP) with a `constexpr` stencil kernel `template <int Order> constexpr auto laplacian_coeffs()` returning the 2nd- or 4th-order finite-difference coefficients as a `std::array`, and use `if constexpr` on `Order` to pick the loop bounds. Write one templated `step<Order>(Grid<S>&)` and verify the 4th-order stencil on `sin(kx)` has error ~h⁴ vs ~h² for 2nd order by halving `h` twice (compile-time sizes `Shape{64,1}`, `Shape{128,1}`, `Shape{256,1}`). Finally use policy classes `Periodic`/`Dirichlet` for the boundary and confirm both compile with the same kernel.

Example: `order 2: err(64)=1.6e-3 err(128)=4.0e-4 ratio 4.0 ; order 4: ratio 16.0`.

<details><summary>Hint</summary>
2nd-order coefficients: `{1, -2, 1}/h²`; 4th-order: `{-1/12, 4/3, -5/2, 4/3, -1/12}/h²`. The boundary policy needs only `static std::size_t wrap(std::ptrdiff_t i, std::size_t n)` (periodic) or a `static bool inside(...)`. Compute `h` from `S.rows` as `constexpr double h = 2π / S.rows;`.
</details>
