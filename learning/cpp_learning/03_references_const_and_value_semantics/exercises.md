# Chapter 03 — Exercises

Write each exercise as `ex03_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex03_K ex03_K.cpp`. Where an exercise asks you to observe
a bug, also run with `-fsanitize=address -g` — dangling references are exactly what it catches.

---

**03.1 — Reference vs pointer behaviour table**
Write a program that demonstrates, with printed output, each row of the reference/pointer table in the lesson: (a) `r = y` assigns through rather than rebinding, (b) `p = &y` rebinds, (c) `sizeof(r)` equals `sizeof(T)` while `sizeof(p)` is 8, (d) a `const int&` bound to a literal, (e) that `int &z = 5;` fails to compile (leave it in a comment with the exact error text).

Example output line: `after r = y: x = 10, r = 10, y = 10`.

<details><summary>Hint</summary>
For (c), `sizeof` on a reference name gives the size of the referred type; use `sizeof(double)` vs `sizeof(double*)` to make it obvious.
</details>

---

**03.2 — Swap three ways**
Write `swap_ptr(int*, int*)`, `swap_ref(int&, int&)`, and a `swap_val(int, int)` that (deliberately) does not work. Call each and print the results. Then write `void swap_rows(Matrix &m, int r1, int r2)` for a `Matrix` from chapter 02 using `std::swap` on the `at()` references, and show a 3×3 matrix before and after.

Example: `swap_val(a, b)` leaves `a = 1, b = 2`; `swap_ref(a, b)` gives `a = 2, b = 1`.

<details><summary>Hint</summary>
`std::swap(m.at(r1, j), m.at(r2, j))` works because the non-const `at()` returns `double&`. Explain in a comment why it would not compile on a `const Matrix&`.
</details>

---

**03.3 — Accessor pairs and const overload forwarding**
Write `class Vector` (a fixed-length numeric vector over `double*`, Rule of Three) with `operator[]` in both const and non-const versions, where the const version does a bounds check (print and `std::abort()` on failure) and the non-const version forwards to it with the `const_cast` idiom so the check is written once. Write a free function `double dot(const Vector &, const Vector &)` and show that it uses the const overload. Try an out-of-range write and confirm the check fires.

Example: `Vector v(3); v[1] = 2.5; dot(v, v)` → `6.25`; `v[3]` → abort with message `index 3 out of range [0,3)`.

<details><summary>Hint</summary>
`double &operator[](int i) { return const_cast<double &>(static_cast<const Vector &>(*this)[i]); }`. This `const_cast` is safe because the underlying object is non-const (we are inside a non-const member).
</details>

---

**03.4 — Hunt the dangling reference**
The following four functions each compile. Copy them into your file, then for each one write a comment saying whether the returned reference is valid, and why. Then write a `main` that calls the *safe* ones and demonstrates one *unsafe* one crashing or misbehaving under `-fsanitize=address`.
```cpp
const std::string &f1(const std::string &s) { return s; }
const std::string &f2() { static std::string s = "static"; return s; }
const std::string &f3(std::string s) { return s; }
const std::string &f4(const std::vector<std::string> &v) { return v.front(); }
```
Also show the call `const std::string &r = f1(std::string("temp"));` and explain when `r` becomes invalid.

<details><summary>Hint</summary>
Ask "who owns the object the reference points to, and is that owner still alive after the function returns?" A by-value parameter is a local. A `static` lives forever. The temporary in the `f1` call dies at the semicolon — lifetime extension does not apply through a function call.
</details>

---

**03.5 — Value semantics vs NumPy views**
Write a small `Matrix` (or reuse chapter 02's). Reproduce the NumPy snippet from the lesson in C++ side by side: `Matrix B = A;` (copy) vs `Matrix &C = A;` (alias) vs a hand-written `struct RowView { double *p; int n; double &operator[](int j); }` returned by `Matrix::row(int i)` that behaves like `A[i]` in NumPy (writes go through to `A`). Print `A` after mutating through `B`, `C`, and a `RowView`.

Example: after `A.row(0)[0] = 42;`, `A.at(0,0)` is `42`; after `B.at(0,0) = 7;`, `A.at(0,0)` is still `42`.

<details><summary>Hint</summary>
`RowView` is a non-owning handle: a pointer into `A`'s buffer plus a length. It is valid only while `A` is alive and not reallocated — document that in a comment. This is the seed of a strided `MatrixView` you will build later.
</details>

---

**03.6 — Count the copies**
Give your `Matrix` a `static inline int copies_ = 0;` incremented in the copy constructor and copy assignment. Then write these and print the counter after each: (a) `Matrix m = make_matrix(3, 3);` where `make_matrix` returns by value a named local; (b) `Matrix m2 = Matrix(3, 3);`; (c) `for (auto x : vec_of_matrices)`; (d) `for (const auto &x : vec_of_matrices)`; (e) `void f(Matrix m)` vs `void g(const Matrix &m)` called with an lvalue; (f) `std::vector<Matrix> v; v.push_back(m);`. Write, as a comment, what you expected and what you observed for each; explain (a) and (b) with copy elision.

<details><summary>Hint</summary>
Build with `-O2` **and** with `-O0 -fno-elide-constructors` and compare. (b) is guaranteed zero copies in C++17 even at `-O0`; (a) is NRVO, elided at `-O2` but possibly not with `-fno-elide-constructors`.
</details>

---

**03.7 — `mutable` cache for an expensive norm**
Add to `Matrix` a `double frobenius() const` that caches its result in a `mutable double cache_` with a `mutable bool valid_` flag, invalidated by every mutating member (non-const `at()`, `operator=`, `swap`). Time 10⁶ calls to `frobenius()` on a 512×512 matrix with and without the cache (use `std::chrono::steady_clock`). Then introduce a bug on purpose — forget to invalidate in `swap` — and write a test that catches it.

Example: without cache ~ hundreds of ms; with cache ~ microseconds; the swap test prints `BUG: stale norm`.

<details><summary>Hint</summary>
The non-const `at()` cannot know whether the caller will write, so it must conservatively invalidate. That is the cost of exposing `double&`. A `set(i, j, v)` method would let you invalidate only on real writes — mention the trade-off in a comment.
</details>

---

**03.8 — In-place vs out-of-place matmul and the aliasing trap**
Implement `Matrix matmul(const Matrix &a, const Matrix &b)` (returns by value) and `void matmul_into(const Matrix &a, const Matrix &b, Matrix &out)` (writes into a preallocated result — the hot-loop variant). Call `matmul_into(m, m, m)` and show that the result is wrong because `out` aliases `a` and `b`. Add an aliasing check (`&out == &a || &out == &b`) that prints a diagnostic and falls back to the by-value version. Time both variants for 200×200 over 50 iterations to see whether the "into" variant is actually faster with copy elision in play.

Example: `matmul(I, X)` equals `X`; `matmul_into(m, m, m)` without the guard produces a matrix that differs from `matmul(m, m)`.

<details><summary>Hint</summary>
Comparing addresses of references (`&out == &a`) is how you detect aliasing in C++ — references have no identity of their own, but the objects they denote do. For the timing, `out` must be allocated once outside the loop for the "into" variant to have any advantage.
</details>

---

**03.9 — Autograd node graph with non-owning references**
Define `struct Node { double value; double grad = 0; std::vector<Node*> parents; std::vector<double> local_grads; };` and build the expression `L = (a*b + c) * a` by hand as a small graph stored in a `std::vector<Node>` you `reserve()` first (explain in a comment why `reserve` matters — what happens to the `Node*` parents if the vector reallocates). Implement backward propagation in reverse topological order (the order you created the nodes) and print `dL/da`, `dL/db`, `dL/dc` for `a=2, b=3, c=1`. Then redo the parents list as `std::vector<int>` indices and note which version survives a `push_back` beyond capacity.

Example: `a=2, b=3, c=1` → `L = 14`, `dL/da = 2ab + c = 13`, `dL/db = a² = 4`, `dL/dc = a = 2`.

<details><summary>Hint</summary>
Each node stores, for each parent, the local partial derivative (∂node/∂parent). Backward: iterate nodes from last to first, and for each parent `p` do `p->grad += node.grad * local_grad`. Seed `L.grad = 1`. This is exactly what `torch.autograd` does with a dynamic graph.
</details>

---

**03.10 — N-body step with `std::reference_wrapper`**
Write `struct Body { Vec3 pos, vel; double mass; };` and store bodies in a `std::vector<Body>`. Build a second container `std::vector<std::reference_wrapper<Body>> massive` containing only the bodies with `mass > threshold`, then write `void gravity_step(std::vector<Body> &all, const std::vector<std::reference_wrapper<Body>> &sources, double dt)` that applies gravitational acceleration from every source to every body (skip self-interaction by comparing addresses). Run 100 steps of a Sun–Earth–Moon-like toy system with `G = 1` and print the total momentum before and after (it should be conserved to round-off if you update velocities symmetrically).

Example: total momentum `(0,0,0)` initially stays within `1e-12` of zero.

<details><summary>Hint</summary>
`std::reference_wrapper<Body>` is in `<functional>`; call `.get()` to obtain the `Body&`. Self-interaction check: `&body == &src.get()`. Do not `push_back` into `all` after building `massive` — reallocation would dangle every wrapper; say so in a comment.
</details>
