# Chapter 10 — Exercises

Write each exercise as `ex10_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex10_K ex10_K.cpp`. Where an exercise asks you to observe
`assert` behaviour, also try `-DNDEBUG`.

---

**10.1 — `std::optional` lookup**
Write `std::optional<double> lookup(const std::vector<std::pair<std::string,double>>& table, const std::string& key)` that returns the value for `key` or `std::nullopt`. In `main`, query a present and an absent key; print the value or `"missing"` using `value_or` in one call and `if (auto v = ...)` in another. Mark it `[[nodiscard]]` and confirm that calling it as a bare statement produces a compiler warning.

Example: table `{{"lr",0.01},{"momentum",0.9}}`, `lookup(t,"lr")` → `0.01`; `lookup(t,"beta")` → `missing`.

<details><summary>Hint</summary>
Returning a `double` from a function whose return type is `std::optional<double>` converts implicitly. `return std::nullopt;` for the empty case.
</details>

---

**10.2 — `bool` + out-param parser**
Write `[[nodiscard]] bool parse_int(std::string_view s, long& out)` using `std::from_chars`. It must return `false` (leaving `out` untouched) for empty input, trailing garbage (`"12x"`), and overflow. Test with `"42"`, `""`, `"12x"`, `"-7"`, `"99999999999999999999"`.

Example: `"42"` → `true, 42`; `"12x"` → `false`.

<details><summary>Hint</summary>
`std::from_chars` returns a struct with `ptr` and `ec`. Success means `ec == std::errc{}` **and** `ptr` reached the end. Parse into a local first, then assign to `out` only on success.
</details>

---

**10.3 — Catch order and slicing**
Write a function `void risky(int mode)` that throws `std::out_of_range` for mode 0, `std::invalid_argument` for mode 1, `std::runtime_error` for mode 2, and the integer `42` for mode 3. In `main`, loop over modes 0–3 with a handler chain that prints a different line per type plus a `catch (...)` fallback. Then deliberately reorder handlers so `std::exception` comes first and read the compiler warning. Finally, change one handler to catch by value and print `typeid(e).name()` to see slicing.

Example output line: `mode 1: invalid_argument: bad arg`.

<details><summary>Hint</summary>
Handlers are matched in order; a base class matches derived objects. `typeid` needs `<typeinfo>`; on a by-value catch it reports the base type, on a by-reference catch the dynamic type.
</details>

---

**10.4 — Unwinding order with a tracer**
Create a `Tracer` struct whose constructor and destructor print its name. Build a call chain `main → a() → b() → c()` where each function creates a `Tracer` and `c()` throws. Predict the exact output **on paper first**, then run. Then add a `Tracer` created *after* the throw in `c()` and confirm it never prints. Finally, allocate one `Tracer` with raw `new` in `b()` (no delete) and show that its destructor never runs — then fix it with `std::unique_ptr`.

<details><summary>Hint</summary>
Destructors run in reverse construction order, only for fully constructed objects in scopes being exited. A raw `new`'d object is not in any scope.
</details>

---

**10.5 — `ShapeError` for `Matrix`**
Write a minimal `Matrix` (rows, cols, `std::vector<double>`) with `add(const Matrix&)` and `matmul(const Matrix&)` free functions. Define `class ShapeError : public std::invalid_argument` carrying both shapes and the operator symbol; `what()` must read exactly `shape mismatch: (2,3) + (3,2)` or `shape mismatch: (2,3) @ (2,3)`. Also give `Matrix` an unchecked `operator()(i,j)` with an `assert`, and a checked `at(i,j)` that throws `std::out_of_range` with the offending indices in the message. Demonstrate each failure in `main`, caught and printed.

<details><summary>Hint</summary>
Build the message with `std::to_string` in the constructor's initializer list and pass it to the `std::invalid_argument` base. Store the shapes as members for programmatic access.
</details>

---

**10.6 — Copy-and-swap for the strong guarantee**
Give your `Matrix` from 10.5 a copy-assignment operator implemented via copy-and-swap, and a `friend void swap(Matrix&, Matrix&) noexcept`. Add a `static int alloc_failures_remaining` hook in your class such that the copy constructor throws `std::bad_alloc` when the counter hits zero. Show that after a failed `a = b;` the matrix `a` still holds its original contents. Then `static_assert(std::is_nothrow_move_constructible_v<Matrix>)` and `static_assert(std::is_nothrow_swappable_v<Matrix>)`.

<details><summary>Hint</summary>
Copy first into a temporary (the only step that can throw), then swap. If the temporary's construction throws, `*this` was never touched.
</details>

---

**10.7 — Hand-rolled `Expected<T, E>`**
Implement `template <class T, class E> class Expected` on top of `std::variant<T, E>` with `ok()`, `explicit operator bool`, `value()`, `error()`, and a static `failure(E)` factory. Mark the class `[[nodiscard]]`. Use it for `Expected<std::vector<double>, std::string> parse_csv_row(std::string_view line)` that fails with a message naming the bad column index (`"column 2: 'abc' is not a number"`). Test with a valid row and two different invalid rows.

Example: `"1.5,2,abc"` → error `column 2: 'abc' is not a number`.

<details><summary>Hint</summary>
Split on commas with `std::string_view::find`, reuse your 10.2 approach with `double`. Keep `Expected` minimal — you only need to construct, test, and extract.
</details>

---

**10.8 — Softmax with validated input (ML)**
Write `std::vector<double> softmax(const std::vector<double>& logits)` that throws `std::invalid_argument("softmax: empty input")` on an empty vector and `std::domain_error` (message naming the index) if any input is NaN or ±inf. Use the max-subtraction trick so large logits don't overflow. Inside the loops, use `[]` and `assert`, not `.at()`. In `main`, show a correct call, an empty call, and one containing `std::numeric_limits<double>::infinity()`, each caught.

Example: `{1,2,3}` → `0.0900306 0.244728 0.665241` (sums to 1).

<details><summary>Hint</summary>
`std::isfinite` from `<cmath>`. Validate in one pass before the arithmetic pass. Subtract `*std::max_element(...)` before `std::exp`.
</details>

---

**10.9 — `main` boundary + `std::system_error` for a weights file (ML / IO)**
Write `std::vector<float> load_weights(const std::string& path)` that reads raw little-endian `float32` values from a binary file. Use `std::ifstream`; on open failure throw `std::system_error(errno, std::generic_category(), "open " + path)`. If the file size is not a multiple of 4, throw `std::runtime_error` stating the size. Give `main` a function-try-block that prints `fatal: <what()>` and returns 1 for `std::exception`, 2 for anything else. Test with a missing file, a 6-byte file you create with `std::ofstream`, and a valid 8-byte file. Check `$?` in the shell after each.

Example: missing file → stderr `fatal: open weights.bin: No such file or directory`, exit code 1.

<details><summary>Hint</summary>
`errno` is set by the underlying `fopen`/`open` even when going through `std::ifstream`; read it immediately after the failed open. Get file size with `seekg(0, std::ios::end)` + `tellg()`.
</details>

---

**10.10 — Error strategy for an RK4 stepper (numerics)**
Design and implement the error handling for a small ODE integrator: `struct OdeSystem { std::size_t dim; std::function<void(double t, const double* y, double* dydt)> f; }` and `void rk4_step(const OdeSystem&, double t, double h, std::vector<double>& y)`. Apply the chapter's layered strategy: the public `rk4_step` throws `std::invalid_argument` if `y.size() != dim`, `h <= 0`, or `h` is not finite, and `std::runtime_error` if the *result* contains a non-finite value (report `t` and the first bad index); the inner loops use only `[]`; the temporary buffers are `std::vector`s declared once (so a throw cannot leak). Integrate `y' = -y` for 10 steps to show a normal run, then integrate `y' = y*y` from `y0 = 1` with `h = 0.5` until the runtime error fires. Wrap `main` in a function-try-block. Comment each check with which layer of the strategy it belongs to.

Example: `y' = -y, y0 = 1, h = 0.1, 10 steps` → `y ≈ 0.367879` (vs exact `e^-1 = 0.367879`).

<details><summary>Hint</summary>
Four evaluations `k1..k4`, each into its own preallocated vector; `y += h/6 (k1 + 2k2 + 2k3 + k4)`. Check finiteness of `y` once after the update, not inside the stage loops. Ask yourself: which checks would you convert to `assert` if this were a private helper called by an already-validating driver?
</details>
