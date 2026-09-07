# Chapter 02 — Exercises

Write each exercise as `ex02_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex02_K ex02_K.cpp`. For every exercise that manages memory,
also build once with `-fsanitize=address -g` and confirm a clean run — that is the real test.

---

**02.1 — `Vec3` as an aggregate**
Define `struct Vec3 { double x, y, z; };` with **no constructors** and add member functions `norm() const`, `dot(const Vec3&) const`, `cross(const Vec3&) const`, and `scaled(double) const` (all return new values, none mutate). Brace-initialize a few vectors in `main`, verify `a.cross(b).dot(a) == 0`, and print `sizeof(Vec3)`. Explain in a comment why this type should stay an aggregate with public fields.

Example: `Vec3{1,0,0}.cross(Vec3{0,1,0})` → `{0, 0, 1}`.

<details><summary>Hint</summary>
Member functions do not stop a struct from being an aggregate; only user-declared constructors, private fields, and virtual functions do. `Vec3 c{a.y*b.z - a.z*b.y, ...}; return c;` or return the brace-list directly.
</details>

---

**02.2 — Constructors and member init order**
Write `class Counter` with members `int start_;`, `int step_;`, `int current_;` and a constructor `Counter(int start, int step) : current_(start_), step_(step), start_(start) {}` — copied exactly. Compile with `-Wall -Wextra`. Paste the warning into a comment, explain what value `current_` actually gets and why, then fix it. Add a default constructor with `= default` plus default member initializers so `Counter c;` yields `start_ = 0, step_ = 1, current_ = 0`.

<details><summary>Hint</summary>
The warning is `-Wreorder`. `current_(start_)` reads `start_` before `start_` has been initialized — that is reading an indeterminate value, which is UB, even though the compiler only warns about the ordering.
</details>

---

**02.3 — RAII `Timer`**
Write `class Timer` whose constructor takes a `const char *label` and records `std::chrono::steady_clock::now()`, and whose destructor prints `label: <ms> ms`. In `main`, open several nested blocks `{ Timer t("outer"); ... { Timer t2("inner"); ... } ... }` around loops that sum a few million doubles, and observe the order in which the messages appear. Add an early `return` inside one block and confirm the timer still prints.

Example output shape:
```
inner: 4.21 ms
outer: 9.87 ms
```

<details><summary>Hint</summary>
`std::chrono::duration<double, std::milli>(now - start_).count()` gives milliseconds as a double. Store the label as `const char*` or `std::string`; the destructor runs even on `return`.
</details>

---

**02.4 — RAII `File` wrapper around `FILE*`**
Write `class File` that opens a file in the constructor (`std::fopen`), closes it in the destructor, exposes `bool ok() const`, `FILE *get() const`, and is **non-copyable** (`= delete` both copy operations). Use it to write 100 lines `i, i*i` to `squares.csv` and read them back, printing the sum of the second column. Then try `File f2 = f;` and paste the compiler error into a comment.

<details><summary>Hint</summary>
`std::fclose(nullptr)` is UB, so guard the destructor with `if (fp_)`. Deleting the copy constructor is the whole point: two `File` objects closing the same `FILE*` would be a double close.
</details>

---

**02.5 — `Matrix` with the Rule of Three**
Implement `class Matrix` holding `int rows_, cols_; double *data_;` with: parameterized constructor `(rows, cols, fill = 0.0)` marked `explicit`, destructor, copy constructor, copy assignment via **copy-and-swap** with a `friend swap`, `rows()/cols()` const accessors, const/non-const `at(i, j)` overloads, and a `friend std::ostream &operator<<`. In `main`: `Matrix a(2,2,1.0); Matrix b = a; b.at(0,0) = 5; a = b; a = a;` and print all matrices after each step. Run with `-fsanitize=address`. Then comment out the copy constructor, rebuild, run, and paste the sanitizer's double-free report into a comment.

Example: after `b.at(0,0) = 5`, `a.at(0,0)` must still be `1`.

<details><summary>Hint</summary>
Copy-and-swap: `Matrix &operator=(Matrix other) { swap(*this, other); return *this; }`. The by-value parameter is the copy; the old buffer dies with `other`. `std::copy(src, src + n, dst)` from `<algorithm>` does the element copy.
</details>

---

**02.6 — `static` factories and a live-object counter**
Extend 02.5 with `static Matrix zeros(int r, int c)`, `static Matrix identity(int n)`, `static Matrix diag(const double *v, int n)`, and a `static inline int live_` that every constructor increments and the destructor decrements. Print `Matrix::live()` at several points, including inside a nested block that creates three matrices and after it ends. Explain in a comment why the copy constructor must also increment the counter.

Example: `Matrix::identity(3)` → diagonal ones; live count returns to `0` before `main` returns.

<details><summary>Hint</summary>
Every constructor — including the copy constructor — creates an object, so every constructor must `++live_`. Returning a `Matrix` from a static factory may or may not invoke the copy constructor (copy elision); your counter must be correct either way, which it is if every constructor and the destructor are paired.
</details>

---

**02.7 — `explicit` and const-correctness audit**
Write `class Polynomial` storing coefficients in `double *coef_` with `int degree_`. Provide `explicit Polynomial(int degree)` (all coefficients zero), Rule of Three, `double &operator[](int i)` and `double operator[](int i) const`, `double eval(double x) const` (Horner's rule), `Polynomial derivative() const`, and `int degree() const`. Write a free function `void print(const Polynomial &p)` that uses only const members. Intentionally remove `const` from `degree()` and paste the resulting error from `print` into a comment; then restore it. Also show that `print(3)` does not compile because of `explicit`.

Example: `p` for `x^2 + 2x + 3` → `p.eval(2) = 11`, `p.derivative().eval(2) = 6`.

<details><summary>Hint</summary>
Horner: `acc = acc * x + coef_[i]` from the highest degree down. The const `operator[]` returns `double`; the non-const one returns `double&`. `-Wall` may warn about the unused parameter `x` in a stub — write the real thing.
</details>

---

**02.8 — `Vocab` for a BPE tokenizer (invariant-preserving class)**
Write `class Vocab` that maintains two parallel arrays as one invariant: `id_to_token` (an array of `std::string`) and a lookup from token to id. Without `std::map` yet (chapter 04), use a linear search for the lookup. Provide `int add(const std::string &tok)` (returns existing id if present), `int id_of(const std::string &tok) const` (−1 if absent), `const std::string &token_of(int id) const`, and `int size() const`. Store the strings in a heap array `std::string *tokens_` with capacity doubling (like your C `IntVec`), and implement the Rule of Three correctly — the copy constructor must copy the `std::string` objects, not the pointer. Seed it with all 256 single-byte tokens in the constructor.

Example: `v.add("th")` → `256`; `v.add("th")` again → `256`; `v.token_of(97)` → `"a"`.

<details><summary>Hint</summary>
`new std::string[cap]` constructs `cap` empty strings; `delete[]` destroys them. Growing means allocate a bigger array, copy (or `std::move`) the strings across, delete the old. Byte tokens: `std::string(1, static_cast<char>(i))`.
</details>

---

**02.9 — `Grid2D` for a heat-equation step**
Write `class Grid2D` (raw `double*`, Rule of Three, `explicit Grid2D(int nx, int ny)`) with `at(i, j)` overloads and a **free function** `Grid2D step(const Grid2D &u, double alpha)` that returns a new grid where each interior point is `u[i][j] + alpha * (u[i+1][j] + u[i-1][j] + u[i][j+1] + u[i][j-1] - 4 u[i][j])` and boundaries are copied unchanged. Run 100 steps from a grid that is `100.0` in the centre cell and `0` elsewhere, with `alpha = 0.2`, and print the total sum after every 25 steps (it should stay approximately constant — heat is conserved on the interior). Verify with `-fsanitize=address` that 100 returned-by-value grids leak nothing.

Example: initial sum `100`; after 25 steps still `100` (within `1e-9`) as long as heat has not reached the boundary.

<details><summary>Hint</summary>
Returning `Grid2D` by value from `step` is fine — the compiler elides the copy in most cases and the destructor handles the rest. `u = step(u, alpha);` inside the loop exercises copy assignment; copy-and-swap makes that trivially correct.
</details>

---

**02.10 — Convert a `goto cleanup` C function to RAII**
Take (or write) a C function `int solve_system(int n)` that allocates three matrices with `mat_alloc`, fills `A` with a diagonally dominant random matrix and `b` with ones, performs Gaussian elimination without pivoting into `x`, and uses `goto cleanup` for the failure paths (singular pivot, allocation failure). Rewrite it in C++ using your `Matrix` from 02.5/02.6 so that there is **no cleanup code anywhere**: each failure path is a plain `return`. Print the residual `||Ax - b||` for `n = 5`. Then count and compare: how many lines of the C version exist only to free memory?

Example: residual printed as `residual = 1.2e-15` or similar (anything below `1e-10` for a 5×5 diagonally dominant system).

<details><summary>Hint</summary>
Diagonally dominant: set `A(i,i) = n` and off-diagonals in `[0, 1)`. A `Matrix` local dies on every `return`, so the "singular" branch is just `return -1;`. Keep the elimination in a free function taking `Matrix&` parameters.
</details>
