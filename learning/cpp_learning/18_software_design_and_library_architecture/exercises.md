# Chapter 18 — Exercises

Write each exercise as `ex18_K.cpp` in this folder (multi-file exercises say which extra files to
create). Compile with `c++ -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -O2 -o ex18_K ex18_K.cpp`
and run every exercise once under `-g -fsanitize=address,undefined` too — most of this chapter is
about ownership, and the sanitizer is the fastest reviewer you have. Exercises marked C++20 use
`-std=c++20`. These exercises produce *design*, so each one also asks for a short written
justification in a comment at the top of the file; write it before the code, not after.

---

**18.1 — Regularity audit**
Write a `template <class T> constexpr bool is_regular_v` in C++17 using `<type_traits>` (default-, copy-, move-constructible; copy-/move-assignable; destructible; and detect `operator==` with a detection idiom). Apply it via `static_assert` to `int`, `std::string`, `std::vector<double>`, `std::unique_ptr<int>` (must be false), a `Vec3` you write, and a `Handle` class holding a `std::shared_ptr<int>` with no `operator==` (false). Then give `Handle` an `operator==` that compares *pointer identity* and write in a comment why `is_regular_v<Handle>` being `true` is now a lie, and what a user of `std::sort` or `std::unique` would observe.

Example: `int: 1  string: 1  vector<double>: 1  unique_ptr<int>: 0  Vec3: 1  Handle: 0`.

<details><summary>Hint</summary>
Detection idiom: a `template <class, class = void> struct has_eq : std::false_type {}` and a partial specialization on `std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>`. Regularity is semantic, not syntactic — the trait can only check syntax; the comment is the point.
</details>

---

**18.2 — `Function<R(Args...)>` with move-only support**
Implement `Function<Sig>` as in §2.2, then extend it: (a) the converting constructor must be constrained with `std::is_invocable_r_v` *and* must not hijack copy/move; (b) support move-only callables (a lambda capturing a `std::unique_ptr`) — copying a `Function` holding one must throw `std::logic_error("not copyable")` at run time rather than fail to compile (implement `clone()` via `if constexpr (std::is_copy_constructible_v<F>)`); (c) `operator()` on an empty `Function` throws `std::bad_function_call`. Test with a free function, a lambda, a function object with state, a member function via a lambda, and `std::plus<double>`. Measure the overhead: time 100M calls of `Function<double(double)>` vs a direct lambda call vs a function pointer, with `DoNotOptimize`.

Example: `direct 0.09 ns/call   fnptr 1.1 ns/call   Function 1.9 ns/call` (yours will differ; explain the ranking).

<details><summary>Hint</summary>
`Model<F>::clone()` is a virtual function, so it must exist for every `F`; `if constexpr` inside its body picks the throwing branch for non-copyable `F`. The `enable_if` for "not a Function" uses `std::decay_t<F>`. The measurement needs the callable to be opaque to the optimizer (e.g. pick between two lambdas based on `argc`).
</details>

---

**18.3 — Small-buffer `Any` with `std::launder`**
Write an `Any` type (like `std::any`) with a 32-byte inline buffer and a manual vtable `{ destroy, copy, move, type }`. Objects that fit and are nothrow-move-constructible go inline; others go to the heap (the buffer then holds a pointer). Provide `template <class T> T* any_cast(Any*)` that compares `typeid` via the stored `const std::type_info*` and returns `nullptr` on mismatch. Print `sizeof(Any)` and for each of `int`, `std::string`, `std::array<double,8>` (64 bytes) whether it went inline. Check with ASan that assigning an `Any` holding a heap object over one holding an inline object leaks nothing.

Example: `sizeof(Any)=40  int: inline  string: inline  array<double,8>: heap`.

<details><summary>Hint</summary>
The vtable is a `static constexpr` struct of function pointers per `T`, exactly as in §2.3. `typeid(T)` is a constant expression address you can store. Every path that reuses the buffer must call `destroy` first; write the assignment as copy-and-swap so you only have to get it right once.
</details>

---

**18.4 — Strong types with units (C++20 optional)**
Implement `template <class T, class Tag> struct Strong` and derive `Meters`, `Seconds`, `Kilograms`, `MetersPerSecond`. Give them exactly the arithmetic that makes sense: `Meters + Meters → Meters`, `Meters - Meters`, `Meters * double`, `Meters / Seconds → MetersPerSecond`, and make `Meters + Seconds` and `Meters * Meters` fail to compile (demonstrate with a commented-out line and the compiler's error message pasted in a comment). Add `Index<Tag>` and use it to make `bodies[i]` accept only `BodyId` and `tree[j]` only `NodeId` on two `std::vector` wrappers. Then generalize: a `Quantity<T, int L, int M, int Ti>` with dimension exponents in the type such that `Quantity<double,1,0,-1>` *is* meters per second and `operator*` adds exponents — show that `Meters{3} / Seconds{2} * Seconds{2} == Meters{3}` compiles and holds.

<details><summary>Hint</summary>
Exponent arithmetic happens in the return type: `template <int L1,int M1,int T1,int L2,int M2,int T2> Quantity<double,L1+L2,M1+M2,T1+T2> operator*(Quantity<double,L1,M1,T1>, Quantity<double,L2,M2,T2>)`. `std::chrono::duration` does the same with `std::ratio`. Keep all constructors `explicit`.
</details>

---

**18.5 — Pimpl with a stable ABI (three files)**
Create `ex18_5_solver.hpp` (public: a `Solver` class exposing only `Solver(int n)`, dtor, move ops, `void set(int i, int j, double v)`, `std::vector<double> solve(const std::vector<double>& b) const`, and a `std::unique_ptr<Impl>`), `ex18_5_solver.cpp` (implementing `Impl` with a dense LU from P06 or Gaussian elimination), and `ex18_5.cpp` (a user). Build the solver as a shared library: `c++ -std=c++17 -O2 -fvisibility=hidden -dynamiclib -o libsolver.dylib ex18_5_solver.cpp` (Linux: `-shared -fPIC -o libsolver.so`), the user against it, run. Then *change `Impl`* (add a pivot-count field and a `stats()` method used internally), rebuild **only the library**, and re-run the *old* user binary — it must still work. Print `sizeof(Solver)`. Finally add a `public:` member `double last_residual_` to `Solver` itself, rebuild only the library, and observe what the old user binary does (crash/garbage) — explain in a comment.

<details><summary>Hint</summary>
On macOS, `otool -L ex18_5` shows which dylib the binary loads; `install_name_tool`/`-Wl,-rpath,.` may be needed for the loader to find `./libsolver.dylib` (or `DYLD_LIBRARY_PATH=. ./ex18_5`). `nm -gU libsolver.dylib | c++filt` lists exported symbols — with `-fvisibility=hidden` you'll need `__attribute__((visibility("default")))` (a `SOLVER_API` macro) on the class.
</details>

---

**18.6 — `Signal` with safe emitter destruction and re-entrancy**
Implement `Signal<Args...>` and `Connection` as in §11, then harden it: (a) destroying the `Signal` while `Connection` tokens are alive must be safe (a later `Connection` destructor must not touch the dead signal — use a `shared_ptr<State>`/`weak_ptr` split between them); (b) a slot may `disconnect()` its own connection or `connect()` a new slot during `emit()` without UB; (c) `Connection` is move-only, `[[nodiscard]]`, and `connect()` on an rvalue lambda works. Test all three with ASan on, plus a `Trainer` emitting `on_step(int, double)` to a `Logger`, a `Checkpointer` (fires every 10 steps) and a lambda that disconnects itself after 3 steps. Print the number of live slots after each phase.

Example: `slots after connect: 3 / after 3 steps: 2 / after Logger destroyed: 1 / after Signal destroyed: (Checkpointer's Connection dtor is a no-op)`.

<details><summary>Hint</summary>
Put the slot vector inside a `struct State` owned by `shared_ptr` in the `Signal`; `Connection` holds `weak_ptr<State>` plus the slot id. Emission iterates over a *copy* of the slot list (or over indices with a "generation" check). Slot ids are monotonically increasing `std::uint64_t`, never reused.
</details>

---

**18.7 — Plugin: `dlopen` + `extern "C"` factory (three files)**
Define `ex18_7_api.hpp` with `struct Activation { virtual ~Activation() = default; virtual double f(double) const = 0; virtual double df(double) const = 0; virtual const char* name() const = 0; }` plus `extern "C"` typedefs `create_fn`/`destroy_fn` and `PLUGIN_API_VERSION`. Write two plugins `ex18_7_gelu.cpp` and `ex18_7_swish.cpp`, each exporting `api_version`, `create`, `destroy`. The host `ex18_7.cpp` takes plugin paths on the command line, loads each with `dlopen`, checks the version, wraps the object in `std::unique_ptr<Activation, destroy_fn>`, and prints `name  f(1.0)  df(1.0)  finite-diff-df(1.0)` for each — the finite difference must agree with `df` to 1e-6. Refuse to load (with a clear message, no crash) a plugin whose `api_version()` returns something else, and one where `dlsym` fails. Build lines for macOS and Linux in the header comment. If the sandbox blocks `dlopen`, compile-check and note it.

<details><summary>Hint</summary>
`dlerror()` after a failed `dlopen`/`dlsym` gives the reason; call it immediately. The deleter type of the `unique_ptr` is the function-pointer type, so pass `destroy` as the second constructor argument. Never `dlclose` before the `unique_ptr`s are gone — order your scopes.
</details>

---

**18.8 — ML: type-erased `Layer`, composed `Sequential`, policy optimizer**
Re-implement the `Layer` value type from §2.1 with `forward(const Matrix&)`, `backward(const Matrix& grad_out)` and `parameters()` returning `std::vector<Param>` where `Param { double* w; double* g; std::size_t n; }` (raw non-owning spans into the layer's storage). Write `Linear`, `ReLU`, `Tanh` as plain structs — no base class — and `Sequential` as a struct *containing* `std::vector<Layer>` that is itself erasable. Then `template <class Policy> class Optimizer` with `SGDPolicy` and `AdamPolicy` (state as `std::vector<double>` per parameter), taking `parameters()` once at construction. Train a 2-16-1 network on `y = sin(x)` for 2000 steps with each optimizer and print the loss every 500 steps; deep-copy the *untrained* network first (`Layer copy = net;`) and show after training that the copy's loss is unchanged (value semantics). Use your P01 `Matrix` or a minimal one.

Example: `step 0 sgd 0.512 adam 0.508 ... step 2000 sgd 0.021 adam 0.004 | untrained copy: 0.512`.

<details><summary>Hint</summary>
`parameters()` returns pointers into the layer's own `Matrix` buffers, so they stay valid as long as the *erased* object isn't reallocated — collect them *after* the network is fully built and never copy the network afterwards (copies have different addresses). Adam's bias correction needs the step count `t` in the optimizer, not the policy.
</details>

---

**18.9 — Numerics: `Tensor = Storage + View` with zero-copy `transpose`/`slice`**
Implement `Storage` (shared flat `double` buffer), `Tensor` (storage + shape + strides + offset) and `TensorView`/`ConstTensorView` (non-owning). Provide O(1) `transpose(a, b)`, `slice(dim, start, stop)`, `reshape(...)` (only when contiguous — return `std::optional<Tensor>`), `is_contiguous()`, `contiguous()`. Write one strided kernel `template <class F> void for_each(TensorView, F)` that walks an N-d index with a single odometer loop, and a fast path for contiguous tensors. Verify: `t.transpose(0,1).transpose(0,1)` shares storage with `t` (`data()` equal); modifying a slice modifies the parent; `sum()` over a transposed 1000×1000 matrix gives the same value as over the original and time both (strided vs contiguous). Then implement `matmul` that calls `.contiguous()` on non-contiguous inputs *or* passes strides as `lda/ldb` — time the two strategies for `A * B.transpose()` at n=512.

Example: `shares storage: yes | sum contiguous 0.9 ms, sum strided 4.1 ms | matmul copy-then-gemm 118 ms, matmul strided 130 ms`.

<details><summary>Hint</summary>
Row-major strides for shape `(s0, s1, s2)` are `(s1*s2, s2, 1)`; `is_contiguous` compares against those (ignoring dimensions of size 1). The odometer: increment the last index, carry into the previous when it hits the shape bound, and adjust a running pointer by `stride[d] - shape[d]*stride[d+1]`-style deltas.
</details>

---

**18.10 — Sim: a versioned checkpoint format with schema evolution (two files)**
Design a binary snapshot for your N-body/Barnes–Hut state (P07): magic `"NBDY"`, `uint32 version`, then TLV fields `{uint16 tag, uint32 len, bytes}` with tags for `time`, `n_bodies`, `positions` (n×3 f64), `velocities`, `masses`. Write `ex18_10_v1.cpp` that writes a v1 file and reads it back (round-trip exact). Then write `ex18_10_v2.cpp` whose *writer* adds two new fields (`softening` f64, `body_ids` u32×n) and bumps the version, and whose *reader* (a) loads the v1 file written by the first program, filling defaults for the missing fields and reporting them, (b) loads its own v2 files, and (c) when given a v3 file you fabricate by hand (an unknown tag inserted in the middle) skips the unknown field and still loads. Add a CRC32 trailer (implement the table-driven CRC32, ~15 lines) and show that flipping one byte of a file is detected. All integers little-endian, written via `memcpy` to `unsigned char[]`. Print a summary line per load.

Example: `loaded v1: n=1000 t=12.5 softening=<default 0.01> body_ids=<generated> crc ok` / `loaded v3: skipped unknown tag 42 (len 16) crc ok` / `corrupted: crc mismatch (expected 8a3f..., got 8a3e...)`.

<details><summary>Hint</summary>
Write a tiny `ByteWriter`/`ByteReader` pair over `std::vector<unsigned char>` with `put_u16/u32/u64/f64` (f64 via `memcpy` to `uint64_t`), and compute the CRC over everything before the trailer. Keep a *golden* v1 file in the folder and make the v2 program's first test load it — that is the test professionals never delete.
</details>
