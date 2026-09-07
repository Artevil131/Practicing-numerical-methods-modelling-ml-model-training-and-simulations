# Chapter 07 — Exercises

Write each exercise as `ex07_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex07_K ex07_K.cpp`. For exercises that concern leaks or
double frees, also build with `c++ -g -fsanitize=address,undefined -std=c++17 -o ex07_K ex07_K.cpp`
and run it — the sanitizer report is part of the exercise.

---

**07.1 — Watch the special members run**
Write `struct Loud` whose default constructor, copy constructor, move constructor, copy assignment, move assignment and destructor each print their name (e.g. `Loud(Loud&&)`). Then, in `main`, run these statements one per labelled section and predict the output in a comment *before* running: (a) `Loud a; Loud b = a;` (b) `Loud c = std::move(a);` (c) `Loud d = Loud{};` (d) `Loud f() { Loud l; return l; }  Loud e = f();` (e) `std::vector<Loud> v; v.reserve(2); v.push_back(Loud{}); v.push_back(Loud{}); v.push_back(Loud{});` — count the moves during the reallocation.

Example: (a) prints `Loud()` then `Loud(const Loud&)`.

<details><summary>Hint</summary>
(c) and (d) print fewer constructors than you expect because of guaranteed/elided copies in C++17. In (e), the third `push_back` relocates two elements.
</details>

---

**07.2 — Rule of Five by hand**
Implement `class Buffer` owning `double* p_` and `std::size_t n_` with all five special members (see the lesson) plus `size()`, `operator[]` (const and non-const) and a member `swap`. Make the move operations `noexcept`. Use a static counter to count live allocations (`+1` in allocating constructors, `-1` in the destructor when `p_ != nullptr`) and print the count at the end of `main` — it must be `0`. Exercise: copy, move, self-assignment `b = b;`, `b = std::move(b);`, and `std::swap(a, b)`.

Then **remove** the `o.p_ = nullptr;` line from the move constructor, rebuild with the sanitizer, and paste the first line of the sanitizer's report in a comment. Restore the line.

<details><summary>Hint</summary>
Copy-and-swap makes copy assignment one line: `Buffer tmp(o); swap(tmp); return *this;`. Self-move-assignment must not free the buffer you are about to take.
</details>

---

**07.3 — The destructor that killed the moves**
Take a Rule-of-Zero `struct Rec { std::vector<double> data; std::string name; };`. Write `Rec make(std::size_t n)` that returns a `Rec` with `n` elements. Time `for (int i = 0; i < 1000; ++i) { Rec r = make(1'000'000); total += r.data[0]; }` with `<chrono>`. Now add a user-declared destructor `~Rec() {}` that does nothing, and time again. Explain the difference in a comment (which special members did the destructor suppress?). Then fix it with `= default` declarations so the timing recovers, keeping the destructor.

<details><summary>Hint</summary>
A user-declared destructor suppresses the implicit move constructor and move assignment; the class still copies, so each `return` may copy 8 MB. Check with `static_assert(std::is_nothrow_move_constructible<Rec>::value)` in both versions.
</details>

---

**07.4 — `noexcept` and vector growth**
Write `struct Elem` with a move constructor that prints `M` and a copy constructor that prints `C`. Make the move constructor `noexcept` in version A and *not* `noexcept` in version B (compile with `-DNOEXCEPT_MOVE` to switch). `push_back` 9 elements into a `std::vector<Elem>` without `reserve` and print the sequence of letters. Report in a comment how many `C`s appear in each version and why.

Example: version A prints only `M`s; version B prints `C`s during each reallocation.

<details><summary>Hint</summary>
`std::vector` uses `std::move_if_noexcept` when relocating. If the move might throw, it copies to keep the strong exception guarantee.
</details>

---

**07.5 — `unique_ptr` with a custom deleter for C resources**
Write `using FilePtr = std::unique_ptr<FILE, decltype(&std::fclose)>;` and a function `FilePtr open_file(const char* path, const char* mode)` returning `{std::fopen(path, mode), &std::fclose}`. Use it to write 100 doubles to a binary file and read them back, with an early `return` in the middle of the write function guarded by a condition — prove the file is still closed (no resource leak) by checking you can reopen and read it. Then do the same with a stateless functor deleter `struct FCloser { void operator()(FILE*) const; }` and print `sizeof` both `unique_ptr` types; explain the difference in a comment.

<details><summary>Hint</summary>
A function-pointer deleter is stored inside the `unique_ptr` (two words); an empty functor deleter takes no space (one word). `fclose(nullptr)` is UB — guard it in the functor.
</details>

---

**07.6 — Ownership audit**
For each of the following, write the C++ signature you would choose and a one-line justification as a comment: (a) a function that prints a `Matrix`; (b) a function that stores a `Matrix` in a cache and may keep it after returning; (c) a function that appends a layer to a network and the network becomes the sole owner; (d) a function that may or may not return a `Matrix` depending on whether a file exists; (e) a `Body` in an N-body sim that needs to refer to its nearest neighbour without owning it; (f) a `Node` in a tree where each child has exactly one parent and parents are deleted before children are needed. Then implement (c) and (d) minimally and call them.

<details><summary>Hint</summary>
Reach for, in order: `const T&`, `T` by value or `shared_ptr<T>`, `unique_ptr<T>` by value (sink), `std::optional<T>`, `T*` (observer), `unique_ptr<Node>` children with a raw `Node* parent`.
</details>

---

**07.7 — `shared_ptr` cycle and `weak_ptr` cure**
Write `struct Person { std::string name; std::shared_ptr<Person> friend_; ~Person() { std::cout << "bye " << name << '\n'; } };`. Create `alice` and `bob` in a block, make them each other's `friend_`, leave the block, and observe that neither destructor prints. Print `use_count()` before leaving the block to see why. Then change `friend_` to `std::weak_ptr<Person>`, show both destructors now run, and demonstrate `lock()` returning empty after one is destroyed. Confirm with the sanitizer (`-fsanitize=address` reports the leak in the first version on exit; note that LeakSanitizer may need `ASAN_OPTIONS=detect_leaks=1` on macOS or may be unsupported — say what you observed).

<details><summary>Hint</summary>
Each `Person` is kept alive by the other's `shared_ptr`; both counts are 1 when the locals die, never 0.
</details>

---

**07.8 — micrograd node graph ownership (ML)**
Implement `struct Node { double data, grad = 0; std::vector<std::shared_ptr<Node>> children; std::vector<std::weak_ptr<Node>> parents; char op; }` with free functions `add`, `mul`, `tanh_` returning `std::shared_ptr<Node>` and wiring children (owning) and parents (weak). Build `L = tanh((a*b) + c)` with `a=2, b=-3, c=10`. Print `use_count()` for `a` (expect 2: your variable and the `mul` node). Drop `L` (`L.reset()`) and print `use_count()` again for `a`, `b`, `c` (expect 1 each — the internal nodes were freed). Add a destructor to `Node` that prints its `op` to see the cascade order. Do not implement backward yet (chapter 08 gives you `std::function`); the point here is that the graph frees itself.

<details><summary>Hint</summary>
`out->children = {a, b}; a->parents.push_back(out);` — the last line converts `shared_ptr` to `weak_ptr` implicitly. The cascade runs from the root: `tanh` node dies → releases `+` node → releases `*` node.
</details>

---

**07.9 — `Sequential` as `vector<unique_ptr<Layer>>` (ML)**
Define `struct Layer { virtual ~Layer() = default; virtual std::vector<double> forward(const std::vector<double>&) = 0; virtual const char* name() const = 0; };` and two implementations: `Scale` (multiplies each element by a stored factor) and `Shift` (adds a stored offset). Write `class Sequential` holding `std::vector<std::unique_ptr<Layer>> layers_` with `void add(std::unique_ptr<Layer> l)` (a sink — caller must `std::move`), `std::vector<double> forward(std::vector<double> x)` that passes through each layer, and `void summary() const` printing each layer's `name()`. Build `Scale(2) → Shift(1) → Scale(0.5)` and run `{1, 2, 3}` through it. Then try `Sequential copy = net;` and record the compiler error in a comment: why can it not be copied, and what would you need to add to make deep copy possible (name the virtual function, do not implement it)?

Example: `{1,2,3}` → `{1.5, 2.5, 3.5}`.

<details><summary>Hint</summary>
`unique_ptr` is move-only, so `Sequential`'s implicit copy is deleted. Deep copy of a polymorphic object needs a `virtual std::unique_ptr<Layer> clone() const`.
</details>

---

**07.10 — Move-aware `Matrix` and a benchmark (numerics)**
Write a Rule-of-Zero `Matrix` (`std::vector<double>` storage) with `operator+(Matrix a, const Matrix& b)` and `operator*` (matmul) as in chapter 06. Add a static counter inside a *custom allocation probe*: give `Matrix` a constructor that increments `g_allocs` whenever it allocates a non-empty vector. Now count allocations for each of these on 200x200 matrices, resetting between them: (a) `D = A + B + C;` (b) `D = A * B + C;` (c) `D = (A + B) * C;` (d) `for (k) acc = acc + A;` versus `for (k) acc += A;` for `k = 10`. Print the counts, then time (a) and (d) with `<chrono>` and comment on the ratio. Finally write `std::optional<Matrix> try_inverse_2x2(const Matrix&)` that returns `std::nullopt` when the determinant is `0` and use it with `value_or`.

<details><summary>Hint</summary>
By-value left operand means `A + B + C` allocates once (for `A + B`) and reuses it. Matmul always allocates its result. `acc = acc + A` allocates per iteration; `acc += A` never does.
</details>
