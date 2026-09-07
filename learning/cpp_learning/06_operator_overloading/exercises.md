# Chapter 06 — Exercises

Write each exercise as `ex06_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex06_K ex06_K.cpp`. Zero warnings is part of the task.

---

**06.1 — `Fraction` arithmetic**
Write a `struct Fraction { long num, den; }` that is always kept in lowest terms with a positive denominator (use `std::gcd` from `<numeric>`). Implement `+ - * /` as free functions built on member `+= -= *= /=`, unary `-`, `==`, `!=`, `<` (and derive `> <= >=`), and `operator<<` printing `num/den` (just `num` when `den == 1`). Division by a zero fraction throws `std::domain_error`.

Example: `Fraction{1,2} + Fraction{1,3}` prints `5/6`; `Fraction{3,6} == Fraction{1,2}` is `true`; `Fraction{2,4} * Fraction{2,1}` prints `1`.

<details><summary>Hint</summary>
Normalise in the constructor, then every operator can assume the invariant. `a < b` for positive denominators is `a.num * b.den < b.num * a.den`.
</details>

---

**06.2 — Member or free?**
Take the `Fraction` from 06.1. Make `2 + Fraction{1,2}` compile and print `5/2`, and make `Fraction{1,2} + 2` compile as well. Then add a *deliberate* bug: move `operator+` inside the class as a member and observe which of the two expressions stops compiling. Put the compiler's error message and a one-sentence explanation in a comment at the top of the file, then restore the working version.

<details><summary>Hint</summary>
Give `Fraction` a non-`explicit` constructor from `long` so `2` converts; free-function operators apply conversions to both operands, member operators only to the right one.
</details>

---

**06.3 — `operator()` with const and non-const**
Write a minimal `Grid` class holding a `std::vector<double>` with `rows`, `cols`, and `operator()(i, j)` in both const and non-const versions. Write a free function `double trace(const Grid& g)` that sums the diagonal. Then comment out the const overload and record (in a comment) the exact compiler error `trace` produces. Add an `assert` on the indices and demonstrate that `-DNDEBUG` removes it (compile twice, check with `g(5, 5)` on a 3x3 grid; under `-DNDEBUG` the behaviour is UB — say so in the comment, and do not rely on what you see).

Example: `Grid g(3,3); g(0,0)=1; g(1,1)=2; g(2,2)=3; trace(g)` → `6`.

<details><summary>Hint</summary>
A const member function is the only kind callable on a `const Grid&`. The non-const overload must return `double&` so assignment writes through.
</details>

---

**06.4 — `Sigmoid` and friends as functors**
Write functors `Sigmoid`, `Tanh`, `ReLU`, `LeakyReLU{slope}` each with `double operator()(double) const`, and a template `apply(std::vector<double>& v, F f)` that maps in place. Add a second member `double grad(double x) const` to each functor giving the derivative. Print a table of `f(x)` and `f'(x)` for `x` in `{-2, -1, 0, 1, 2}` for each functor.

Example row: `Sigmoid   x= 0.00  f= 0.5000  f'= 0.2500`.

<details><summary>Hint</summary>
`sigmoid'(x) = s(1-s)`, `tanh'(x) = 1 - tanh²`, `relu'(x) = x > 0`. The template parameter `F` is deduced from the argument; no `std::function` needed.
</details>

---

**06.5 — Stream I/O round trip**
Add `operator<<` and `operator>>` to a small `Matrix` (rows, cols, `std::vector<double>`). `<<` writes `rows cols` on the first line then one row per line; `>>` reads that format and *resizes* the matrix to match (so `Matrix m; std::cin >> m;` works on a default-constructed matrix). Write the matrix to a `std::ostringstream`, read it back from a `std::istringstream`, and check `==` on the result.

Example: a 2x3 matrix `[[1,2,3],[4,5,6]]` writes
```
2 3
1 2 3
4 5 6
```
and reads back equal.

<details><summary>Hint</summary>
`operator>>` reads `r c` first, then assigns `m = Matrix(r, c)` before filling. Return the stream so callers can test `if (is >> m)`.
</details>

---

**06.6 — `Complex` for the FFT**
Implement `struct Complex { double re, im; }` with `+ - *` (complex product), `*` and `/` by `double`, unary `-`, `==`/`!=`, `conj(z)`, `abs(z)`, `arg(z)`, and `operator<<` printing `a+bi` / `a-bi`. Add a free function `Complex polar(double r, double theta)`. Verify `polar(1, M_PI/2) * polar(1, M_PI/2)` is approximately `-1+0i` and that `abs(z * conj(z))` equals `abs(z)²` for a few values. Do not use `<complex>` — you are writing it. (Use `std::abs` on the difference for approximate checks.)

<details><summary>Hint</summary>
`(a+bi)(c+di) = (ac - bd) + (ad + bc)i`. `polar` is `{r cos θ, r sin θ}`. Keep `*` in terms of `*=` as in the lesson.
</details>

---

**06.7 — `explicit` and ambiguity**
Start with a `Matrix` that has a non-`explicit` constructor `Matrix(double fill)` producing a 1x1 matrix, plus `operator*(const Matrix&, const Matrix&)` (matmul) and `operator*(Matrix, double)` (scale). Write `Matrix m(2,2,1.0); auto r = m * 2.0;` and record what the compiler says (ambiguous, or silently picks one — which?). Fix it with `explicit` and confirm that `m * 2.0` now scales. Then add `explicit operator bool()` returning "non-empty" and show that `if (m)` works while `double d = m;` fails to compile (leave the failing line in a comment).

<details><summary>Hint</summary>
Overload resolution prefers exact matches; `2.0` → `double` is exact for the scale overload, but the ambiguity appears in other shapes, e.g. `m * 2` with an `int`. Try both and report.
</details>

---

**06.8 — `Vec3` and a two-body step (physics)**
Implement the `Vec3` from the lesson (all operators plus `dot`, `cross`, `norm`, `normalized`, `<<`). Then simulate two bodies under Newtonian gravity for 1000 steps of semi-implicit Euler (`v += a*dt; x += v*dt`) with `G = 1`, masses `1` and `1e-3`, the small body at `(1,0,0)` with velocity `(0,1,0)`. Print total energy `E = ½ m₁|v₁|² + ½ m₂|v₂|² - G m₁ m₂ / |r|` at steps 0, 500, 1000. It should drift only slightly. Write the force computation as one expression using your operators.

Example (approximate): `E0 = -0.0005`, `E1000` within a few percent of `E0`.

<details><summary>Hint</summary>
Force on 1 from 2: `r = x2 - x1; f = r * (G*m1*m2 / (norm(r)^3))`; `a1 = f/m1; a2 = -f/m2`. Use `dt = 0.001`.
</details>

---

**06.9 — Matrix with matmul, hadamard, broadcasting bias (ML)**
Write a `Matrix` with `+ - ` (same shape), `*` (matmul), `hadamard(a, b)`, scalar `*` on both sides, `transpose(m)`, `operator()`, `operator<<`, and a **row-broadcast add** `add_rowvec(const Matrix& m, const Matrix& b)` where `b` is 1xN and is added to every row (NumPy's `X @ W + b`). Compute one forward pass of a linear layer: `X` 4x3 filled with `i+j`, `W` 3x2 filled with `0.1*(i+1)`, `b` 1x2 `{0.5, -0.5}`; print `Y = X * W + b` via `add_rowvec`. Then print `hadamard(Y, Y)`. Make shape mismatch throw `std::invalid_argument` and demonstrate catching it.

Example: `Y(0,0)` = `0*0.1 + 1*0.2 + 2*0.3 + 0.5` = `1.3`.

<details><summary>Hint</summary>
Write `+=` first, define `+` via a by-value copy. Document at the top of the class that `*` is matmul. For `add_rowvec`, loop rows outer, cols inner, `y(i,j) += b(0,j)`.
</details>

---

**06.10 — Dual numbers: forward-mode autograd through operators (ML)**
Implement `struct Dual { double val, der; }` where `der` tracks d/dx. Overload `+ - * /` (Dual×Dual and Dual×double both sides), unary `-`, and free functions `sin`, `cos`, `exp`, `log`, `sqrt`, `pow(Dual, double)` applying the chain rule (e.g. `exp(a) = {exp(a.val), a.der * exp(a.val)}`). Write a **template** `template <typename T> T f(T x) { return exp(-x*x) * sin(3.0*x) + x/(1.0+x*x); }` that works for both `double` and `Dual`. Evaluate `f'(x)` at `x = 0.7` via `f(Dual{0.7, 1.0}).der` and compare to a central finite difference `(f(x+h) - f(x-h)) / 2h` with `h = 1e-6`. Print both and their difference (should be ~1e-9 or smaller).

<details><summary>Hint</summary>
Product rule: `(a*b).der = a.der*b.val + a.val*b.der`. Quotient rule: `(a/b).der = (a.der*b.val - a.val*b.der)/(b.val*b.val)`. For the template to pick your `sin` over `std::sin`, put `Dual` and its functions in a namespace and rely on argument-dependent lookup (call `sin(x)` unqualified, with `using std::sin;` in the template body).
</details>
