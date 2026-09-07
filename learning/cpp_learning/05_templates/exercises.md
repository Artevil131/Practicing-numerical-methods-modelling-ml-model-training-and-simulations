# Chapter 05 — Exercises

Write each exercise as `ex05_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex05_K ex05_K.cpp`. Where an exercise asks for a header,
create it next to the `.cpp` and `#include` it.

---

**05.1 — `clamp`, `lerp`, `argmax` as function templates**
Write `template <typename T> T clamp(T v, T lo, T hi)`, `template <typename T> T lerp(T a, T b, T t)`, and `template <typename T> std::size_t argmax(const std::vector<T> &v)`. Call each with `int`, `float`, and `double` arguments. Then call `clamp(5, 0.0, 10)` and paste the deduction error into a comment; fix it two ways (cast, explicit `<double>`).

Example: `clamp(-3, 0, 10)` → `0`; `lerp(0.0, 10.0, 0.25)` → `2.5`; `argmax({1.0, 9.0, 3.0})` → `1`.

<details><summary>Hint</summary>
The error will say "deduced conflicting types for parameter 'T' ('int' vs. 'double')". Deduction never applies implicit conversions between arguments.
</details>

---

**05.2 — `Matrix<T>` header-only, two precisions**
Put `template <typename T> class Matrix` (flat `std::vector<T>`, `operator()(i,j)` overloads, `rows()`, `cols()`) plus free-function templates `matmul`, `transpose`, and `operator<<` into `matrix.hpp`. In `ex05_2.cpp`, build the same 4×4 matrix as `Matrix<float>` and `Matrix<double>`, multiply each by itself 20 times (normalizing by the Frobenius norm each step to avoid overflow), and print the difference between the two results element-wise — this is float32 vs float64 drift. Add `static_assert(std::is_arithmetic_v<T>, ...)` and try `Matrix<std::string>`; paste the one-line error.

Example: after 20 normalized squarings, differences on the order of `1e-6` to `1e-4`.

<details><summary>Hint</summary>
To compare the two you need a `Matrix<double>` from the `Matrix<float>` — either write the converting constructor now (exercise 05.4) or compare element by element with `static_cast<double>(f(i,j))`.
</details>

---

**05.3 — `Vec<N>` fixed-size physics vector**
Write `template <int N> struct Vec { double v[N]; ... }` with `operator+`, `operator-`, `operator*(double)`, `dot`, `norm`, `operator[]`, and a free `template <int N> std::ostream &operator<<`. Provide `Vec<3> cross(const Vec<3>&, const Vec<3>&)` as a plain (non-template) function so that calling it on `Vec<2>` is a compile error. Verify `sizeof(Vec<3>) == 24`, that `Vec<2>{} + Vec<3>{}` fails to compile (comment + error), and compute the angular momentum `r × (m v)` for a test particle.

Example: `cross({1,0,0}, {0,1,0})` → `(0, 0, 1)`.

<details><summary>Hint</summary>
Brace-initialize as `Vec<3> r{{1.0, 0.0, 0.0}}` (outer braces for the struct, inner for the array) or add a variadic constructor later. Inside a member of `Vec<N>`, plain `Vec` means `Vec<N>`.
</details>

---

**05.4 — Member function templates: `.to<U>()` and `.apply(f)`**
Extend `Matrix<T>` from 05.2 with `template <typename U> explicit Matrix(const Matrix<U>&)` (converting constructor) and `template <typename U> Matrix<U> to() const`, mirroring PyTorch `.to(torch.float32)`. Also add `template <typename F> Matrix &apply(F f)` that maps a callable over every element, and use it with a lambda to apply `tanh` and with a plain function pointer `std::exp`. Show that `Matrix<double> d = f;` (no `explicit`) would compile but `explicit` makes precision changes visible.

Example: `Matrix<float>(3,3,0.1f).to<double>()(0,0)` prints `0.100000001490116` — float32's nearest value to 0.1.

<details><summary>Hint</summary>
`std::exp` is overloaded, so `apply(std::exp)` is ambiguous; wrap it in a lambda `[](T x){ return std::exp(x); }` or cast to `double(*)(double)`. Print with `std::setprecision(17)` to see the float32 representation error.
</details>

---

**05.5 — Full and partial specialization: `dtype` and `Storage`**
Write `template <typename T> struct DType { static constexpr const char *name = "unknown"; static constexpr int bytes = sizeof(T); };` and fully specialize `name` for `float` (`"float32"`), `double` (`"float64"`), `int` (`"int32"`), `std::int64_t` (`"int64"`), `bool` (`"bool"`). Then write a primary `template <typename T> struct Describe` with partial specializations for `T*`, `std::vector<T>`, and `Matrix<T>` (from 05.2) whose `static std::string get()` returns e.g. `"pointer to float32"`, `"vector of float64"`, `"Matrix of int32"`. Print `Describe<X>::get()` for six types.

Example: `Describe<std::vector<Matrix<float>>>::get()` → `vector of Matrix of float32`.

<details><summary>Hint</summary>
Partial specializations recurse naturally: `Describe<std::vector<T>>::get()` returns `"vector of " + Describe<T>::get()`. The primary template's `get()` returns `DType<T>::name`.
</details>

---

**05.6 — The linker error, on purpose**
Split 05.2 wrongly: declare `template <typename T> Matrix<T> matmul(const Matrix<T>&, const Matrix<T>&);` in `matrix.hpp` but *define* it in `matrix_impl.cpp`. Build with `c++ -std=c++17 ex05_6.cpp matrix_impl.cpp` and paste the undefined-symbol error into a comment, explaining in two sentences why it happens. Then fix it two ways: (a) move the definition to the header; (b) keep it in the `.cpp` and add `template Matrix<double> matmul<double>(const Matrix<double>&, const Matrix<double>&);` as an explicit instantiation — and show that `Matrix<float>` then still fails to link.

<details><summary>Hint</summary>
The mangled symbol in the error will contain `6MatrixIdE` — `I...E` brackets the template argument, `d` is `double`. Explicit instantiation forces the compiler to emit that one specialization even though nothing in `matrix_impl.cpp` calls it.
</details>

---

**05.7 — `if constexpr` + type traits: a generic `nearly_equal` and `to_string_precise`**
Write `template <typename T> bool nearly_equal(T a, T b)` that uses a relative tolerance based on `std::numeric_limits<T>::epsilon()` for floating-point `T` and exact `==` otherwise, and `template <typename T> std::string to_string_precise(T x)` that prints floating types with `std::numeric_limits<T>::max_digits10` significant digits (via `std::ostringstream`) and integers plainly. Add `static_assert(std::is_arithmetic_v<T>)` to both. Test with `float`, `double`, `int`, and `long long`, and show `0.1f + 0.2f` vs `0.3f` and the double equivalent.

Example: `to_string_precise(0.1f)` → `0.100000001`; `to_string_precise(0.1)` → `0.10000000000000001`.

<details><summary>Hint</summary>
`max_digits10` is 9 for `float`, 17 for `double`. Replace the `if constexpr` with a plain `if` and observe the compile error for `int` — that difference is the point of the exercise.
</details>

---

**05.8 — Generic Kahan / pairwise summation over precision**
Write `template <typename T> T naive_sum(const std::vector<T>&)`, `template <typename T> T kahan_sum(const std::vector<T>&)`, and `template <typename T> T pairwise_sum(const std::vector<T>&)` (recursive halving, switch to naive below 64 elements). Fill a vector of 10⁷ values `0.1` in `float` and `double`; print all six sums with full precision and the absolute error against the exact `10⁶`. Use `static_assert(std::is_floating_point_v<T>)`.

Example: `naive_sum<float>` is off by roughly `10⁴–10⁵`; `kahan_sum<float>` and `pairwise_sum<float>` are off by well under `1`.

<details><summary>Hint</summary>
Kahan: `y = x - c; t = s + y; c = (t - s) - y; s = t;`. Compile with `-O2` but **not** `-ffast-math` — fast-math is allowed to "optimize" Kahan's compensation away, which is worth a comment.
</details>

---

**05.9 — `Vec<N>` N-body with a `template <int N> struct Body`**
Using `Vec<N>` from 05.3, write `template <int N> struct Body { Vec<N> pos, vel; double mass; };` and `template <int N> void step(std::vector<Body<N>> &bodies, double dt)` implementing one leapfrog (kick-drift-kick) step of gravity with softening. Run a 2-D three-body figure-eight initial condition (`pos = (±0.97000436, ∓0.24308753), (0,0)`, `vel = (0.466203685, 0.43236573), (0.466203685, 0.43236573), (−0.93240737, −0.86473146)`, masses 1, G = 1) for 2000 steps with `dt = 1e-3` in `N = 2`, and print total energy at start and end. Then instantiate the same code for `N = 3` with a random 5-body cloud to prove nothing is dimension-specific.

Example: energy drift for the figure-eight below `1e-5` relative over the run.

<details><summary>Hint</summary>
Softened acceleration on body i: `sum_j m_j (r_j − r_i) / (|r_j − r_i|² + ε²)^{3/2}`. With `N` a template parameter the inner `Vec` loops unroll; check with `-O2 -S` or just time it against a `std::vector<double>`-based version.
</details>

---

**05.10 — Variadic `log_row` for training output + a `Tensor<T, Rank>` sketch**
(a) Write `template <typename... Args> void log_row(std::string_view sep, const Args &...args)` using a fold expression, printing all arguments separated by `sep`. Use it to print a training-style table header and rows: `log_row(" | ", "epoch", "loss", "lr")`, `log_row(" | ", 3, 0.125, 1e-3)`. Add `static_assert(sizeof...(Args) > 0)`.
(b) Sketch `template <typename T, int Rank> class Tensor` holding `std::array<int, Rank> shape`, a flat `std::vector<T>`, a `T &operator()(std::array<int, Rank> idx)` computing the row-major offset with a loop over `Rank`, and `static_assert(Rank >= 1 && Rank <= 4)`. Instantiate `Tensor<float, 2>` (a matrix) and `Tensor<float, 3>` (a batch of matrices) and verify indexing against hand-computed offsets. Write a comment on what you would need to add for this to become the storage of an autograd engine.

Example: `Tensor<float,3>` with shape `{2,3,4}`: element `{1,2,3}` is at flat offset `1·12 + 2·4 + 3 = 23`.

<details><summary>Hint</summary>
Fold with a separator: `std::size_t i = 0; ((std::cout << (i++ ? sep : "") << args), ...);`. For the strides, compute `stride[Rank-1] = 1; stride[k] = stride[k+1] * shape[k+1]` once in the constructor.
</details>
