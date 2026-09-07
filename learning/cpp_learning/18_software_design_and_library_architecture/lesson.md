# Chapter 18 — Software Design and Library Architecture

## What you'll be able to do after this chapter

- Design types that behave like `int` (regular types, value semantics) and know when a type must *not* be a value (identity, resources, polymorphism) — and what to do then.
- Implement type erasure by hand (`Function<R(Args...)>`, a `Layer` any-type with a small buffer), explain what `std::function`/`std::any` do internally, and choose between it, templates, and `virtual`.
- Use strong types, pimpl, hidden friends, customization points and constructor injection to build an API that is hard to misuse, stable across releases, and testable without frameworks.
- Separate computation from storage (`Tensor = Storage + View`), design a versioned binary format that survives schema changes, and load code at runtime through a factory across a `dlopen` boundary.
- Lay out a 50k-line numerics library the way professionals do (public/internal headers, layering rules, tests, benches, CI) and read PyTorch/Eigen/LLVM source knowing what to look at first.

## Why this matters for ML / numerics / sims

Projects P01–P05 give you a `Matrix` → `Tensor` → autograd → NN → transformer stack of perhaps 5–10k lines. It works. It is also, most likely, one big directory where `Tensor` knows about autograd, autograd knows about `Linear`, and changing the storage layout means editing forty files. That is normal for a first version and fatal for a second. The difference between a project and a library is *architecture*: which parts may know about which other parts, what is a value and what is a handle, where the compile-time/run-time polymorphism boundary sits, and what a user is allowed to depend on. PyTorch's ATen has ~2,000 operators dispatched through one mechanism; Eigen compiles `A*B+C` into a single fused loop; LLVM's `SmallVector` avoids millions of heap allocations. None of that is clever code — it is *design*, and all of it is learnable. This chapter is the design vocabulary you need to make the P04 framework something you would not be embarrassed to publish.

Prerequisites: all of `../01_from_c_to_cpp` … `../13_modern_cpp_and_idioms`, especially `../09_inheritance_and_polymorphism` (the `Layer` hierarchy and `std::variant` alternative), `../07_move_semantics_and_smart_pointers`, `../11_headers_build_cmake_testing`. Chapters 14–17 (`../14_templates_advanced_and_metaprogramming`, `../16_object_lifetime_allocators_and_memory`, and the concurrency/standard-library chapters) are referenced where relevant.

---

## 1. Value semantics as the default design

Alexander Stepanov (designer of the STL) formalized the idea in *Elements of Programming* (2009): a **regular type** is one that behaves like `int` — you can copy it, and the copy is equal to and independent of the original; you can compare for equality; default-construct; assign; destroy. C++20 spells it as the `std::regular` concept ([concepts.object] in the C++20 standard; cppreference "std::regular"):

```cpp
// C++20
template <class T>
concept Regular = std::semiregular<T> && std::equality_comparable<T>;
// semiregular = copyable (copy/move ctor+assign, destructible) + default_initializable
```

In C++17 you state the same thing in a comment and a static-assert:

```cpp
static_assert(std::is_default_constructible_v<Matrix> && std::is_copy_constructible_v<Matrix> &&
              std::is_copy_assignable_v<Matrix>     && std::is_nothrow_move_constructible_v<Matrix>);
```

Why default to values? Three reasons you can measure:

| Property | Value type (`Matrix`, `Vec3`, `Complex`) | Reference/handle type (`shared_ptr<Node>`, `Layer*`) |
|---|---|---|
| Aliasing | None. `b = a; b(0,0) = 1;` cannot change `a`. Local reasoning works. | `b = a;` shares state; any mutation is visible through every alias. |
| Lifetime | Scope-bound (RAII). No leaks, no dangling. | Ownership graph must be tracked; cycles leak (`shared_ptr` ↔ `shared_ptr`). |
| Threads | Values can be handed to another thread by copy/move; no shared mutable state. | Every alias is a potential data race. |
| Testing | `assert(f(x) == expected)` — pure function of inputs. | Need to construct the object graph and inspect side effects. |
| Cost | Copies cost O(size). Moves are O(1). RVO makes returning free. | Pointer copy O(1); indirection on every access; allocator traffic. |

The rule: **make it a value unless it has identity** (two `Optimizer`s with the same hyperparameters are still different objects because they hold different momentum buffers — *for the same parameter*), **holds a non-copyable resource** (file handle, GPU context), or **is polymorphic** (then see §2 for keeping value semantics anyway).

A regular type's operations should also be *equationally reasonable*: `a == a`; `T b = a; b == a`; `a = b` implies `a == b`; move leaves the source in a valid-but-unspecified state (C++17 [lib.types.movedfrom]). Your `Matrix` from P01 is already regular if you followed `../03_references_const_and_value_semantics`. Your P03 autograd `Tensor` is probably *not*: it shares a `shared_ptr<Node>` so copies alias. That is a legitimate design — PyTorch's `torch.Tensor` is a handle too — but say so in the docs ("`Tensor` is a reference type; use `.clone()` for a deep copy") and do not give it `operator==` that compares contents (users will assume regularity).

Python equivalent: `int`, `str`, `tuple` are values (immutable, so copying is trivial); `list`, `np.ndarray`, `torch.Tensor` are handles (`b = a; b[0] = 1` changes `a`). C++ lets you choose per type; choose value.

---

## 2. Type erasure

`../09_inheritance_and_polymorphism` showed two ways to hold "some Layer": a hierarchy behind `unique_ptr<Layer>` (open set, heap, pointer semantics) or `std::variant<Linear, ReLU, …>` (closed set, value semantics). **Type erasure** gives you the third option: open set *and* value semantics. `std::function`, `std::any`, `std::shared_ptr`'s deleter, and every "concept-model" idiom in Sean Parent's talks work this way.

### 2.1 The pattern

```
   Layer (value)                       heap (or small buffer)
 +---------------------+             +------------------------------+
 | unique_ptr<Concept> |----------->| Model<Linear> : Concept        |
 +---------------------+             |   Linear value_;               |
                                     |   forward() override {...}     |
                                     +------------------------------+
```

- **Concept**: an abstract class with the operations you need (`forward`, `parameters`, `clone`). Private to the implementation; users never see it.
- **Model<T>**: a template that derives from Concept and forwards each operation to a `T` it owns.
- **Wrapper** (`Layer`): a regular type with a templated constructor `template<class T> Layer(T x)` that heap-allocates `Model<T>`; copy = `clone()`; move = pointer steal.

```cpp
class Layer {
    struct Concept {
        virtual ~Concept() = default;
        virtual Matrix forward(const Matrix&) = 0;
        virtual std::vector<Matrix*> parameters() = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    template <class T> struct Model final : Concept {
        T v_;
        explicit Model(T v) : v_(std::move(v)) {}
        Matrix forward(const Matrix& x) override { return v_.forward(x); }          // duck-typed
        std::vector<Matrix*> parameters() override { return v_.parameters(); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }
    };
    std::unique_ptr<Concept> p_;
public:
    template <class T, class = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Layer>>>
    Layer(T x) : p_(std::make_unique<Model<std::decay_t<T>>>(std::move(x))) {}
    Layer(const Layer& o) : p_(o.p_ ? o.p_->clone() : nullptr) {}
    Layer(Layer&&) noexcept = default;
    Layer& operator=(Layer o) noexcept { p_ = std::move(o.p_); return *this; }      // copy-and-swap
    Matrix forward(const Matrix& x) { return p_->forward(x); }
};
// Linear, ReLU, Dropout... are plain structs with forward()/parameters(). No base class. No virtual.
std::vector<Layer> net = { Linear(784, 128), ReLU{}, Linear(128, 10) };   // a vector of VALUES
```

The `enable_if` on the converting constructor is essential: without it `Layer(Layer&)` (non-const lvalue) would prefer the template over the copy constructor and recurse forever. In C++20 write `requires (!std::same_as<std::remove_cvref_t<T>, Layer>)`.

What you gain over `unique_ptr<LayerBase>`: `Linear` has no base class, so you can erase *anyone's* type (including `int` if it has a `forward`); copying a `std::vector<Layer>` deep-copies the network; no `nullptr` state to check (unless moved-from). What you pay: one virtual call and one indirection per operation, same as inheritance; one allocation per object (fixable, §2.3).

### 2.2 `Function<R(Args...)>` — how `std::function` works

`std::function` is exactly this pattern with one operation, `operator()`, plus a small-buffer optimization. A minimal version (partial specialization on the function type, as in `<functional>` [func.wrap.func]):

```cpp
template <class Sig> class Function;                    // primary: undefined
template <class R, class... Args>
class Function<R(Args...)> {
    struct Concept { virtual ~Concept() = default; virtual R call(Args...) = 0;
                     virtual std::unique_ptr<Concept> clone() const = 0; };
    template <class F> struct Model final : Concept {
        F f;
        explicit Model(F g) : f(std::move(g)) {}
        R call(Args... a) override { return std::invoke(f, std::forward<Args>(a)...); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }
    };
    std::unique_ptr<Concept> p_;
public:
    Function() = default;
    template <class F, class = std::enable_if_t<std::is_invocable_r_v<R, F&, Args...>>>
    Function(F f) : p_(std::make_unique<Model<F>>(std::move(f))) {}
    Function(const Function& o) : p_(o.p_ ? o.p_->clone() : nullptr) {}
    Function(Function&&) noexcept = default;
    Function& operator=(Function o) noexcept { p_ = std::move(o.p_); return *this; }
    explicit operator bool() const noexcept { return p_ != nullptr; }
    R operator()(Args... a) const { if (!p_) throw std::bad_function_call{}; return p_->call(std::forward<Args>(a)...); }
};
Function<double(double)> f = [k = 3.0](double x) { return k * x; };
// f(2.0) == 6.0
```

`std::is_invocable_r_v` (C++17, `<type_traits>`) is the constraint the real one uses (it's the "Lvalue-Callable" requirement in [func.wrap.func.con]). Note the cost model: `std::function` is fine as a *stored callback*; it is the wrong tool as a *parameter* type — take `F&&` template parameters (zero overhead, inlinable) or, in C++23, `std::function_ref`/`std::move_only_function`. A `std::function<double(double)>` in an inner loop of an ODE solver costs an indirect call per evaluation, roughly 2–5 ns, and blocks vectorization of the loop around it.

### 2.3 Small-buffer optimization and manual vtables

libc++'s `std::function` stores callables up to 3 pointers (24 bytes on arm64) inline; libstdc++ up to 16 bytes; bigger ones go to the heap. Two techniques make that work without `unique_ptr`:

```cpp
// A manual vtable: a struct of function pointers replaces the virtual Concept.
template <class Sig> class SmallFunction;
template <class R, class... Args>
class SmallFunction<R(Args...)> {
    static constexpr std::size_t kBuf = 32;
    struct VTable { R (*call)(void*, Args...); void (*destroy)(void*); void (*copy)(void*, const void*); };
    alignas(std::max_align_t) unsigned char buf_[kBuf];
    const VTable* vt_ = nullptr;      // nullptr == empty
    template <class F> static const VTable* vtable_for() {
        static constexpr VTable vt{
            [](void* p, Args... a) -> R { return (*static_cast<F*>(p))(std::forward<Args>(a)...); },
            [](void* p) { static_cast<F*>(p)->~F(); },
            [](void* dst, const void* src) { ::new (dst) F(*static_cast<const F*>(src)); } };
        return &vt;
    }
public:
    template <class F, class D = std::decay_t<F>,
              class = std::enable_if_t<sizeof(D) <= kBuf && alignof(D) <= alignof(std::max_align_t)
                                       && std::is_nothrow_move_constructible_v<D>>>
    SmallFunction(F&& f) : vt_(vtable_for<D>()) { ::new (static_cast<void*>(buf_)) D(std::forward<F>(f)); }
    ~SmallFunction() { if (vt_) vt_->destroy(buf_); }
    SmallFunction(const SmallFunction& o) : vt_(o.vt_) { if (vt_) vt_->copy(buf_, o.buf_); }
    SmallFunction& operator=(const SmallFunction& o) { if (this != &o) { if (vt_) vt_->destroy(buf_); vt_ = o.vt_; if (vt_) vt_->copy(buf_, o.buf_); } return *this; }
    R operator()(Args... a) const { return vt_->call(const_cast<unsigned char*>(buf_), std::forward<Args>(a)...); }
};
```

Placement `new` into a `char` buffer and `static_cast<F*>` back is the sanctioned way to do this ([basic.life], [intro.object]/2 — the object is *created* in the storage, so no strict-aliasing issue). Reading `buf_` as an `F` *without* having placement-newed one there would be UB. `std::launder` (C++17) is needed only when the storage previously held a different object with `const` or reference members; in that case `std::launder(reinterpret_cast<F*>(buf_))`.

`std::any` (C++17, `<any>`) is the same mechanism with `type_info` stored instead of a call operation and `any_cast` doing a `typeid` comparison; libc++ inlines objects up to 3 words that are nothrow-move-constructible. `std::variant` is *not* type erasure — it's a tagged union with all alternatives known at compile time — and is what you use when the set is closed (`../09_inheritance_and_polymorphism` §9).

### 2.4 When to pick which

| Need | Tool |
|---|---|
| Heterogeneous *stored* elements, closed set, no heap | `std::variant` + `std::visit` |
| Heterogeneous stored elements, open set, value semantics | Type erasure (`Layer`), `std::function`, `std::any` |
| Heterogeneous stored elements, open set, identity/shared | Inheritance + `shared_ptr`/`unique_ptr` (classic OOP) |
| A callable *parameter* | Template `F&&` (or `std::function_ref`/C++23) — never `std::function` by value in hot code |
| Different behavior decided at compile time | Templates / policies (§3) — zero cost, no erasure at all |

---

## 3. Policy-based design and strategy via templates vs virtual

The **Strategy** pattern in OOP: an `Optimizer` base class with `virtual void step(Params&)`, `SGD` and `Adam` overriding it. The template version — Andrei Alexandrescu's **policy-based design** (*Modern C++ Design*, 2001) — makes the strategy a *type parameter*:

```cpp
struct SGDPolicy  { double lr = 0.01;
                    void update(double& w, double g, double& /*state*/) const { w -= lr * g; } };
struct AdamPolicy { double lr = 1e-3, b1 = 0.9, b2 = 0.999, eps = 1e-8; mutable long t = 0;
                    void update(double& w, double g, std::pair<double,double>& st) const {
                        auto& [m, v] = st; m = b1*m + (1-b1)*g; v = b2*v + (1-b2)*g*g;
                        w -= lr * m / (1 - std::pow(b1, t)) / (std::sqrt(v / (1 - std::pow(b2, t))) + eps); } };

template <class Policy, class State> class Optimizer {
    Policy pol_; std::vector<State> state_;
public:
    explicit Optimizer(Policy p, std::size_t n) : pol_(p), state_(n) {}
    void step(std::vector<double>& w, const std::vector<double>& g) {
        for (std::size_t i = 0; i < w.size(); ++i) pol_.update(w[i], g[i], state_[i]);   // inlined per policy
    }
};
Optimizer<SGDPolicy, double> opt(SGDPolicy{0.1}, w.size());
```

Compare:

| | Virtual strategy | Template policy |
|---|---|---|
| Call cost | Indirect call, no inlining, blocks vectorization of the loop *around* it | Inlined; the update loop vectorizes |
| Switch at run time (config file says "adam") | Natural | Needs a `std::variant<Optimizer<SGD>, Optimizer<Adam>>` or a type-erased wrapper at the top |
| Binary size | One copy of `step` | One `step` per policy (fine: two, not two hundred) |
| Error messages | Clear (missing override) | Long; add `static_assert(std::is_invocable_v<...>)` or a C++20 `concept OptimizerPolicy` |
| Header exposure | Implementation can live in `.cpp` | Must be in the header |

Guideline: **hot, per-element decisions are policies; cold, per-object decisions are virtual or erased.** The optimizer update is called `N_params` times per step → policy. Which optimizer the training loop uses is decided once → erase at the top with `std::variant` or a `Layer`-style wrapper. Eigen's `Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>` is policy design taken to the limit — `Options` selects row/column-major at compile time and every kernel is instantiated for it.

---

## 4. Strong types

`void resize(std::size_t rows, std::size_t cols)` and `m.resize(cols, rows)` compile identically. `dgemm`'s 13 positional arguments are the canonical source of transposed-matrix bugs. A **strong type** wraps a primitive so the compiler distinguishes them:

```cpp
template <class T, class Tag> struct Strong {
    T v;
    constexpr explicit Strong(T x) : v(x) {}
    constexpr T get() const { return v; }
    friend constexpr bool operator==(Strong a, Strong b) { return a.v == b.v; }
    friend constexpr bool operator<(Strong a, Strong b) { return a.v < b.v; }
};
using Rows = Strong<std::size_t, struct RowsTag>;       // the tag is an incomplete type; never defined
using Cols = Strong<std::size_t, struct ColsTag>;
using Meters = Strong<double, struct MetersTag>;
using Seconds = Strong<double, struct SecondsTag>;

Matrix m(Rows{3}, Cols{4});          // m(Cols{4}, Rows{3}) — compile error
```

Add only the operations that make physical sense: `Meters + Meters` yes, `Meters * Meters` gives `SquareMeters` (or is deleted), `Meters / Seconds` gives `MetersPerSecond`. This is how `std::chrono::duration` is designed — `duration<Rep, Period>` is a strong type with a *unit* in its type — and it is why `sleep_for(5)` doesn't compile but `sleep_for(5ms)` does. The full unit-system generalization (`mp-units`, `Boost.Units`) encodes dimensions as compile-time rational exponents.

The most useful strong type in a numerics library is a **typed index**:

```cpp
template <class Tag> struct Index { std::size_t i; };
struct NodeTag; struct EdgeTag;
using NodeId = Index<NodeTag>; using EdgeId = Index<EdgeTag>;
std::vector<Node> nodes;  Node& at(NodeId k) { return nodes[k.i]; }   // at(EdgeId) — compile error
```

In Barnes–Hut (P07) mixing a body index with a tree-node index is a classic silent bug; typed indices make it a type error. Cost: zero — `Strong<size_t, Tag>` is a single `size_t` in a register (the ABI passes trivially-copyable single-member structs like the member itself on arm64; see the AAPCS64 for the exact rule).

---

## 5. The pimpl idiom and ABI stability

Everything in a class's private section is still in the header: every user recompiles when you add a private member, and the class's `sizeof` is part of the **ABI** (application binary interface). If your library is a `.dylib`/`.so` and you add a member to `Matrix`, every program compiled against the old header now reads garbage. **pimpl** (pointer to implementation, also "compiler firewall", Herb Sutter GotW #100) hides all of it:

```cpp
// solver.hpp — the public header exposes ONE pointer
class Solver {
public:
    Solver(); ~Solver();                                   // must be defined in the .cpp (Impl is incomplete here)
    Solver(Solver&&) noexcept; Solver& operator=(Solver&&) noexcept;
    Solver(const Solver&) = delete; Solver& operator=(const Solver&) = delete;   // or deep-copy in .cpp
    void set_tolerance(double tol);
    std::vector<double> solve(const std::vector<double>& b) const;
private:
    struct Impl;                    // forward-declared; complete type only in solver.cpp
    std::unique_ptr<Impl> impl_;
};
// solver.cpp
struct Solver::Impl { double tol = 1e-10; Eigen::SparseLU<...> lu; /* 400 lines of state */ };
Solver::Solver() : impl_(std::make_unique<Impl>()) {}
Solver::~Solver() = default;        // HERE, where Impl is complete — in the header it would fail to compile
Solver::Solver(Solver&&) noexcept = default; Solver& Solver::operator=(Solver&&) noexcept = default;
void Solver::set_tolerance(double t) { impl_->tol = t; }
```

Rules that follow from the mechanics: the destructor and move operations must be *defined in the .cpp* (so `unique_ptr<Impl>`'s deleter sees a complete type; otherwise `static_assert(sizeof(T) > 0)` fires inside `default_delete`); `Impl` may change freely without recompiling users; `sizeof(Solver) == 8` forever. Cost: one heap allocation per object and one indirection per call — never pimpl `Vec3` or `Complex`; do pimpl anything that drags in Eigen, HDF5, or MPI headers, and anything exported from a shared library.

ABI stability is why big libraries look the way they do: no `std::string` in exported signatures (libstdc++'s `std::string` ABI changed in GCC 5 — the `_GLIBCXX_USE_CXX11_ABI` saga), no inline functions in the public API that touch private state, `virtual` tables never reordered, enumerators never renumbered. The alternative to caring is header-only (Eigen, doctest, nlohmann/json): everything recompiles, so there is no ABI. For a numerics library you control end-to-end, header-only or static linking is the sane default; pimpl is for the parts you ship as binaries or that have heavy dependencies.

---

## 6. API design for a numerics library

### 6.1 Owning containers vs non-owning views

`std::string`/`std::string_view` (C++17), `std::vector`/`std::span` (C++20), `Tensor`/`TensorView`. The owning type manages memory; the view is `{pointer, size(s), strides}` and is *cheap to copy, never owns, and can dangle*.

```cpp
struct MatrixView {                                  // non-owning, trivially copyable
    double* data; std::size_t rows, cols, row_stride;
    double& operator()(std::size_t i, std::size_t j) const { return data[i * row_stride + j]; }
    MatrixView block(std::size_t r0, std::size_t c0, std::size_t nr, std::size_t nc) const {
        return {data + r0 * row_stride + c0, nr, nc, row_stride};
    }
};
struct ConstMatrixView { const double* data; /* ... same ... */ };
void gemm(ConstMatrixView A, ConstMatrixView B, MatrixView C);   // one function serves Matrix, blocks, Eigen::Map, raw arrays
```

The API rule: **functions take views, return owners**. `gemm(A.view(), B.view(), C.view())` works for a whole matrix, a sub-block, a `std::vector<double>` and memory you got from Python via the buffer protocol. Functions that return a view of an argument (`block()`, `row()`) are fine; functions returning a view of a *temporary* are the classic dangling bug (`auto r = make_matrix().row(0);` — dead by the semicolon). C++20 `std::mdspan` (C++23 actually; `<mdspan>`) standardizes exactly this view — see `../20_cpp_for_numerics_and_hpc`.

### 6.2 Const-correctness

`const` on a view means "I won't write through it", and the *view itself* being const is different from the *elements* being const — the two-type split (`MatrixView`/`ConstMatrixView`) above mirrors `iterator`/`const_iterator` and `span<T>`/`span<const T>`. Provide an implicit conversion `MatrixView → ConstMatrixView`, never the reverse. Every member function that does not mutate is `const`; `mutable` is only for caches and mutexes (and a `mutable` cache must be thread-safe — [res.on.data.races] requires const members to be safe to call concurrently).

### 6.3 Error-handling policy — pick one and write it down

From `../10_error_handling`: exceptions for *programmer and environment errors that the caller can't reasonably check* (dimension mismatch, allocation failure, file not found); `std::optional`/`expected` return for *expected* failures (`solve()` on a singular matrix, `parse()` of user input); `assert`/contract-checks for *preconditions* in debug, nothing in release. In a numerics library the hot path must be exception-free and `noexcept`: `operator()(i, j)` is `noexcept` with an `assert`, not a bounds-checked throw (`at()` may throw, like `std::vector`). State the policy in a `docs/ERRORS.md`; users cannot guess.

### 6.4 Overload sets, ADL and customization points

An **overload set** is one *name* that means one *thing* for many types: `norm(Vec3)`, `norm(Matrix)`, `norm(Tensor)` — users write `norm(x)` and never care. **Argument-dependent lookup** (ADL, [basic.lookup.argdep]) finds `norm` in the namespace of its argument's type, which is what lets `std::cout << my::Matrix` find `my::operator<<`. The two disciplined ways to make a function customizable by users:

**Hidden friends** (Dan Saks / the "hidden friend" idiom, recommended by the Core Guidelines C.168): define non-member operators *inside* the class as `friend`. They're found *only* by ADL, keep the namespace's overload set small (faster compiles, better errors), and cannot be called with unrelated types by accident.

```cpp
namespace num {
class Matrix {
    // ...
    friend Matrix operator*(const Matrix& a, const Matrix& b) { /* ... */ }           // hidden friend
    friend std::ostream& operator<<(std::ostream& os, const Matrix& m) { /* ... */ }
    friend double norm(const Matrix& m) { /* ... */ }                                 // also works for plain functions
};
}   // num::norm is NOT visible to ordinary lookup; num::Matrix m; norm(m) works via ADL.
```

**The `std::swap` two-step / customization point**: a generic algorithm must call the *user's* `swap` if one exists, else the standard one. The C++17 idiom is `using std::swap; swap(a, b);` — the using-declaration makes `std::swap` a candidate, ADL adds the user's. C++20 replaces this fragile pattern with **customization point objects** (CPOs) — `std::ranges::swap`, `std::ranges::begin` are *function objects* (so ADL can't hijack them, hence "niebloids" for the algorithm versions, after Eric Niebler) that internally do the two-step correctly. If you write a generic `dot(a, b)` that users should be able to customize for their `MyVec`, either document "provide a hidden-friend `dot` in your namespace", or provide a CPO: `inline constexpr struct dot_fn { template<class A, class B> auto operator()(const A& a, const B& b) const { return dot_impl(a, b); /* ADL */ } } dot{};`. Also provide `tag_invoke`-style hooks only if you enjoy explaining them. For a numerics library, hidden friends + a documented ADL protocol cover 95%.

### 6.5 Overload-set hygiene

- Never overload on `int` vs `double` vs `std::size_t` where literals are ambiguous (`f(0)` picks `int`, `f(0u)` picks `unsigned`, `f(0.0)` picks `double` — `f(0L)` may be ambiguous).
- `explicit` on every single-argument constructor (`Matrix(3)` should not be a valid implicit conversion from `int`).
- `[[nodiscard]]` (C++17) on every pure function that returns a value (`inverse()`, `transpose()`); forgetting to use the result of `m.transpose()` is a bug 100% of the time.
- Parameter passing: sink parameters (things you store) by value then move; read-only by `const&` (or by value if ≤ 16 bytes and trivially copyable — `Vec3`, `MatrixView`); out-parameters as the *last* arguments and named `_into` (`gemm_into(A, B, C)`), never mixed with a return.

---

## 7. Dependency injection without frameworks

Java/C# needed Spring and Autofac because their interfaces are nominal and their types are all heap-allocated. In C++ dependency injection is just **constructor injection** plus one of two polymorphism mechanisms:

```cpp
// Template injection: zero cost, decided at compile time, the default for hot paths.
template <class Rng = std::mt19937_64, class Sink = StdoutLogger>
class Trainer {
    Rng rng_; Sink& log_;                        // Sink by reference: it outlives the trainer
public:
    Trainer(Rng rng, Sink& log) : rng_(std::move(rng)), log_(log) {}
    void epoch() { /* rng_(), log_.info("...") */ }
};
// In tests: Trainer<FixedRng, RecordingLogger> t(FixedRng{42}, rec);  assert(rec.lines.size() == 3);

// Erased injection: when the dependency is chosen at run time or must cross a .cpp boundary.
class Trainer2 { std::function<double()> rng_; Logger& log_; /* Logger is an interface */ };
```

The seam is *the constructor parameter*. Global singletons (`Logger::instance()`, `Config::get()`) are the anti-pattern: they hide dependencies, make tests order-dependent, and are initialization-order UB across TUs if they're plain globals (the "static initialization order fiasco", [basic.start.dynamic] — use function-local statics if you must). A useful compromise for something *truly* global like an allocator or thread pool: pass it explicitly to the top-level object, let it thread it down; if that's too noisy, a `thread_local` "current context" pointer set by an RAII guard (how PyTorch's `at::AutoDispatchBelowAutograd` and `torch.no_grad()` work).

---

## 8. Composition over inheritance — an NN framework, concretely

The `../09_inheritance_and_polymorphism` `Layer` hierarchy has one inheritance tree where `Sequential` is-a `Layer`, `Dropout` is-a `Layer` … and then you want `Linear` to also be `Serializable`, `Trainable`, `Quantizable` — multiple inheritance, diamond, virtual bases, misery. The composed design:

```
                Layer (type-erased value, §2)  ----- forward(), parameters(), name()
                  ^ any struct with those members
   Linear { Matrix W, b; ... }      ReLU {}      Sequential { std::vector<Layer> layers; }   // Sequential CONTAINS Layers
   Model { Layer net; Optimizer<Adam> opt; Loss loss; }                                      // Model HAS-A Layer, not IS-A
   Serializer::write(const Layer&, Archive&)       // separate free-function protocol, §11
```

- `Sequential` *contains* a `std::vector<Layer>`; its `forward` folds over them. It is itself erasable into a `Layer` — composition gives you the tree for free.
- Optimizers are policies (§3) parameterized on the update rule; the *state* (Adam's `m`, `v`) lives in a `std::vector<State>` aligned with `parameters()`, not inside the layers — layers know nothing about training.
- Loss functions are plain callables `double(const Matrix&, const Matrix&)`; autograd (P03) supplies the gradient.
- Serialization is a free function found via ADL/hidden friend, so `Linear` doesn't inherit from `Serializable`.

Each concern is one small type with one job. Adding `Quantizable` is a new free function overload set, not a new base class. This is the Go/Rust "traits are protocols, not parents" mindset expressed in C++.

---

## 9. Separating computation from storage: `Tensor = Storage + View`

PyTorch's `at::Tensor` is `{ intrusive_ptr<TensorImpl> }`, `TensorImpl` is `{ Storage storage; sizes; strides; storage_offset; dtype; ... }`, and `Storage` is `{ DataPtr data; size_bytes; Allocator* }`. `t.view(2, 3)`, `t.transpose(0, 1)`, `t[1:]` create *new `TensorImpl`s sharing the same `Storage`* — O(1), no copy. That is the design to port to P02:

```cpp
class Storage {                                   // reference-counted flat buffer, dtype-agnostic bytes
    std::shared_ptr<double[]> data_; std::size_t n_;
public:
    explicit Storage(std::size_t n) : data_(new double[n]()), n_(n) {}
    double* data() const { return data_.get(); } std::size_t size() const { return n_; }
};
class Tensor {                                    // handle: Storage + view metadata
    Storage storage_; std::vector<std::size_t> shape_, strides_; std::size_t offset_ = 0;
public:
    Tensor transpose(std::size_t a, std::size_t b) const {   // O(1): swap two strides
        Tensor t = *this; std::swap(t.shape_[a], t.shape_[b]); std::swap(t.strides_[a], t.strides_[b]); return t;
    }
    bool is_contiguous() const;                   // strides == row-major strides for shape
    Tensor contiguous() const;                    // copy into fresh Storage if not
};
```

Kernels then dispatch on `is_contiguous()`: the fast path is a flat loop over `storage.data() + offset`, the slow path walks strides. Reductions and matmul call `.contiguous()` first (or hand the strides to BLAS's `lda`). NumPy does exactly the same (`ndarray.base`, `strides`, `flags['C_CONTIGUOUS']`).

**Expression templates** (Eigen, Blaze, xtensor) push the separation one step further: `a*b + c` doesn't compute anything; it builds a type `Sum<Prod<Mat,Mat>,Mat>` and the *assignment* `Matrix r = expr;` runs a single fused loop `r[i] = a[i]*b[i] + c[i]` — no temporaries, one pass over memory. `../14_templates_advanced_and_metaprogramming` builds one; the danger is `auto e = a + b;` — `e` is the expression, not a matrix, and dangles if `a` or `b` was a temporary (Eigen's documentation page "C++11 and the auto keyword" is dedicated to this).

---

## 10. Plugin architectures: `dlopen` + factory

When users must add code you've never seen *after* your binary is built — new activation functions in a served model, new boundary conditions in a solver — you load shared libraries at run time. The only thing that crosses the boundary safely is an **`extern "C"` factory returning a pointer to a known abstract interface**:

```cpp
// plugin_api.hpp — shipped to plugin authors; frozen (ABI!)
struct Activation { virtual ~Activation() = default; virtual double operator()(double) const = 0; };
extern "C" { using create_fn = Activation* (*)(); using destroy_fn = void (*)(Activation*); }
#define PLUGIN_API_VERSION 1

// gelu_plugin.cpp — built with: c++ -std=c++17 -O2 -dynamiclib -o libgelu.dylib gelu_plugin.cpp   (Linux: -shared -fPIC -o libgelu.so)
struct Gelu final : Activation { double operator()(double x) const override { return 0.5*x*(1+std::erf(x/std::sqrt(2.0))); } };
extern "C" Activation* create() { return new Gelu; }
extern "C" void destroy(Activation* p) { delete p; }
extern "C" int api_version() { return PLUGIN_API_VERSION; }

// host
void* h = dlopen("./libgelu.dylib", RTLD_NOW | RTLD_LOCAL);          // <dlfcn.h>; link nothing extra on macOS, -ldl on old glibc
if (!h) throw std::runtime_error(dlerror());
auto ver = reinterpret_cast<int (*)()>(dlsym(h, "api_version"));   // conditionally-supported cast, fine on POSIX
auto create  = reinterpret_cast<create_fn>(dlsym(h, "create"));
auto destroy = reinterpret_cast<destroy_fn>(dlsym(h, "destroy"));
std::unique_ptr<Activation, destroy_fn> act(create(), destroy);       // delete with the PLUGIN's destroy — same allocator
// ... dlclose(h) only after every object from it is destroyed
```

Why `extern "C"`: C++ name mangling is not standardized across compilers/versions, so `dlsym("create")` must look up an unmangled name. Why `destroy` from the plugin: host and plugin may have different `operator delete` (different allocators, different C++ runtimes). Why an abstract class works at all: the Itanium C++ ABI (used by clang and GCC on macOS/Linux) fixes vtable layout, so a virtual call through `Activation*` is binary-compatible as long as the interface header doesn't change (never reorder/insert virtuals; add a new interface `Activation2` instead — the COM approach). Never pass `std::string`, `std::vector`, or exceptions across the boundary (`-fvisibility=hidden` + explicit export macros in real libraries).

Pointer-to-function `reinterpret_cast` from `void*` is *conditionally supported* ([expr.reinterpret.cast]/8); POSIX guarantees it works for `dlsym`.

---

## 11. Event systems / observer without leaks

An **observer** ("signal/slot", `EventEmitter`, `torch.nn.Module.register_forward_hook`) lets a `Trainer` notify loggers, checkpointers, LR schedulers after each step without knowing them. Two bugs define the design: *dangling* (a subscriber died, the emitter still calls it) and *leaks* (emitter and subscriber hold `shared_ptr`s to each other). The leak-free, dangle-free version uses an RAII **connection token** and `weak_ptr`:

```cpp
template <class... Args>
class Signal {
    struct Slot { std::function<void(Args...)> fn; std::shared_ptr<void> alive; };   // alive: token liveness
    std::vector<Slot> slots_;
public:
    class Connection {                                                              // RAII: destroying it disconnects
        std::weak_ptr<void> tok_; Signal* sig_ = nullptr;
    public:
        Connection() = default; Connection(std::shared_ptr<void> t, Signal* s) : tok_(t), sig_(s) {}
        Connection(Connection&& o) noexcept : tok_(std::move(o.tok_)), sig_(std::exchange(o.sig_, nullptr)) {}
        Connection& operator=(Connection&& o) noexcept { disconnect(); tok_ = std::move(o.tok_); sig_ = std::exchange(o.sig_, nullptr); return *this; }
        ~Connection() { disconnect(); }
        void disconnect() { if (sig_) { if (auto t = tok_.lock()) sig_->erase(t.get()); sig_ = nullptr; } }
    };
    [[nodiscard]] Connection connect(std::function<void(Args...)> f) {
        auto tok = std::make_shared<int>(0);                  // any object; only its address matters
        slots_.push_back({std::move(f), tok}); return Connection(tok, this);
    }
    void emit(Args... a) { auto copy = slots_; for (auto& s : copy) s.fn(a...); }   // copy: slots may (dis)connect during emit
private:
    void erase(void* p) { slots_.erase(std::remove_if(slots_.begin(), slots_.end(), [p](const Slot& s){ return s.alive.get() == p; }), slots_.end()); }
};
// Trainer has  Signal<int, double> on_step;   Logger holds  Signal<int,double>::Connection conn_ = trainer.on_step.connect([&](int s, double l){...});
// Logger dies -> Connection dtor -> slot removed. No weak_ptr<Logger>, no shared ownership of Logger.
```

The `[[nodiscard]]` on `connect` matters: discarding the token disconnects immediately (a common "why doesn't my callback fire" bug — Boost.Signals2's `scoped_connection` has the same behavior). Iterating over a *copy* during `emit` handles re-entrancy. The remaining lifetime rule the *emitter* must honor: it must outlive its connections, or connections must be `disconnect()`ed first (a `Signal` destructor that nulls its live connections handles this; left as exercise 18.6).

---

## 12. Serialization design: versioned binary, schema evolution

Checkpoints (P04/P05) and simulation snapshots (P07–P10) must be readable a year from now, by a version of the code that has new fields. Rules that professional formats (protobuf, FlatBuffers, HDF5, Arrow, PyTorch's zip-of-pickles) all share:

1. **Magic + version header.** `"NNCK" + uint32 version`. Reject unknown magic; dispatch on version.
2. **Explicit sizes and endianness.** `uint32_t`/`uint64_t`, little-endian, written byte-by-byte or `memcpy`ed on a little-endian host with a `static_assert` (C++20 `std::endian::native == std::endian::little`). Never `fwrite(&struct, sizeof(struct))` — padding and ABI make it non-portable ([class.mem] leaves padding unspecified).
3. **Length-prefixed, tagged fields** (TLV: tag, length, value). A reader that sees an unknown tag *skips `length` bytes*. That single rule gives forward compatibility (old reader, new file) for free. Backward compatibility (new reader, old file) = every field newer than the file's version has a documented default.
4. **Never reuse or renumber tags.** Deprecated tags stay reserved forever.
5. **Floating point as IEEE-754 bit patterns** (`memcpy` to `uint64_t`); write NaN payloads as-is; document precision (`float32` weights? `float64`?).
6. **Checksum the payload** (CRC32 or xxHash) so truncated files fail loudly.
7. **Test with golden files**: check a v1 file into the repo; the v2 reader must load it in CI forever.

```cpp
// Sketch: field = { uint16 tag, uint32 len, bytes[len] }
enum Tag : std::uint16_t { kShape = 1, kWeightsF64 = 2, kName = 3, kWeightsF32 = 4 /* v2 */ };
void write_field(std::ostream& os, std::uint16_t tag, const void* p, std::uint32_t len) {
    write_le(os, tag); write_le(os, len); os.write(static_cast<const char*>(p), len);
}
// Reader:  while (read_le(is, tag) && read_le(is, len)) { switch (tag) { case kShape: ...; default: is.seekg(len, std::ios::cur); } }
```

Structured, self-describing formats (HDF5, `.npz`, safetensors) trade some speed for tooling: `h5dump` and `np.load` can inspect them without your code. For the ML stack use safetensors' layout (JSON header of `{name: {dtype, shape, offsets}}` + raw bytes) — ten lines to write, PyTorch loads it natively. `../20_cpp_for_numerics_and_hpc` implements a `.npy` writer.

---

## 13. Headers vs modules organisation

C++20 **modules** (`export module num.matrix;` / `import num.matrix;`, [module]) replace textual inclusion with compiled binary module interfaces: no include guards, no macro leakage, no ODR surprises, faster builds. Status in 2026: clang and MSVC support them, CMake ≥ 3.28 supports `CXX_MODULES` with Ninja, `import std;` works in libc++ ≥ 17 with `-fmodules` flags, but Apple clang's toolchain lags, most third-party libraries are still header-only, and IDE support is uneven. Recommendation: **design for modules, ship headers**. That means the discipline modules would force:

| Rule | Why |
|---|---|
| One header = one cohesive unit; `#include` what you use, nothing more (IWYU: `include-what-you-use`) | Modules import exactly what's exported; textual includes should approximate that |
| `#pragma once` + a project-prefixed guard for portability | Modules need neither |
| No macros in public headers except feature/export guards (`NUM_EXPORT`, `NUM_HAS_OPENMP`) | Macros don't cross module boundaries |
| `namespace num::detail` for non-API symbols; `inline namespace v1 {}` for versioning | Modules export selectively; you approximate with `detail` |
| Forward declarations in a `fwd.hpp` (`class Matrix; template<class> class Tensor;`) | Cheap includes; mirrors module partitions |
| Template definitions in `-inl.hpp`/`.ipp` files included at the bottom of the header | Keeps declarations readable; a later move to module partitions is mechanical |

A header's include cost is measurable: `c++ -E file.cpp | wc -l` (preprocessed lines) and `-ftime-trace` (`../11_headers_build_cmake_testing` §11). `<iostream>` is ~20k lines after preprocessing; `<algorithm>` ~30k; Eigen/Dense ~100k. Keep them out of `fwd.hpp` and out of any header that most TUs include.

---

## 14. Layering rules: no upward dependencies

Draw the library as layers; each may `#include` only from layers *below* it:

```
   L4  apps / bindings        (train.cpp, python module, CLI)
   L3  models & training      (nn/, optim/, data/)
   L2  autograd               (autograd/)
   L1  tensor & kernels       (tensor/, kernels/ — BLAS, NEON, OpenMP live here)
   L0  core                   (core/: strong types, Storage, error types, logging interface, config)
   ---------------- third party below the line: Eigen, Accelerate, fmt, doctest
```

Violations that creep in: `tensor/` includes `autograd/` "just for `requires_grad`" (fix: a flag in `Tensor`, the autograd engine consults it); `core/` logs through `nn/`'s trainer callback (fix: `core/` defines a `Logger` interface, `nn/` implements it — dependency inversion). Enforce mechanically:

- **CMake targets per layer** with `target_link_libraries(num_tensor PUBLIC num_core)`: a `tensor/` file that includes `autograd/` fails to compile because the include path isn't there.
- **`include-what-you-use`** (`brew install include-what-you-use`; `iwyu_tool.py -p build`) to remove accidental includes.
- **Dependency graphs**: `cmake --graphviz=deps.dot . && dot -Tpng deps.dot -o deps.png` (target graph); `clang -MM file.cpp` (per-file include graph); `cinclude2dot` / `cpp-dependencies` for a whole tree. Put a `.github/workflows` job that greps `#include "autograd/` in `tensor/` and fails — ugly, effective.
- **Bazel/Buck** users get this from `visibility` attributes; CMake ≥ 3.x approximates it with `INTERFACE` include directories per target.

---

## 15. Designing for testability: seams and fakes

A **seam** (Michael Feathers, *Working Effectively with Legacy Code*) is a place where you can substitute behavior without editing the code under test. Constructor injection (§7) creates them. Practical seams in a numerics library:

| Untestable | Seam | Test double |
|---|---|---|
| `std::random_device` inside the sampler | `template<class Rng>` / `Rng&` parameter | `FixedRng` returning a scripted sequence |
| `std::chrono::steady_clock::now()` in a scheduler | `Clock` template parameter | `FakeClock` with `advance(ms)` |
| `std::cout` logging | `std::ostream&` parameter | `std::ostringstream` — assert on its `.str()` |
| Reading `weights.bin` | `std::istream&` (not a filename) | `std::istringstream` with bytes built in the test |
| `cblas_dgemm` | `template<class Gemm>` or function pointer | `naive_gemm` — cross-check results |
| `omp_get_num_threads()` | Pass `n_threads` explicitly | `1` in unit tests; the sanitizer job runs `8` |
| Wall-clock-driven early stopping | `Signal`/callback | Recording callback |

*Fakes* (working simplified implementations — `FakeClock`) beat *mocks* (expectation-recording objects — GoogleMock) in numerics: you want to check the number, not the call sequence. Test at the right granularity: kernels against a naive reference (`allclose`, `../12_performance`), autograd against finite differences (P03), solvers against analytic solutions and conservation laws (P06–P10), the whole model against a golden loss curve for a fixed seed (§ "reproducibility" in `../20_cpp_for_numerics_and_hpc`). `../19_professional_tooling_and_quality` covers the frameworks.

---

## 16. Designing for performance: data-oriented design

Data-oriented design (DOD; Mike Acton's 2014 CppCon talk "Data-Oriented Design and C++"; Richard Fabian's book) starts from the *access pattern*, not the domain model. Three concrete transformations, each measurable with `../12_performance`'s harness:

**SoA** (`../12_performance` §6): `struct Bodies { std::vector<double> x, y, z, vx, vy, vz, m; }` instead of `std::vector<Body>`. Position update touches 6 contiguous streams instead of 7-field 56-byte records → 2–4× and it vectorizes.

**Hot/cold splitting**: a `Body` has `{pos, vel, mass}` (hot: touched every step) and `{name, color, history, creation_time}` (cold: touched at render/log time). Interleaving them means every hot cache line is 30% payload. Split into two arrays indexed by the same `BodyId`; the cold one may even be a `std::unordered_map<BodyId, ColdData>`.

**Existence-based processing**: no `if (body.alive)` in the hot loop; keep dead bodies in a separate array (swap-and-pop on death). Branchless loops vectorize; a `bool` field per element costs a byte *and* a branch.

Where this meets OOP: the `Body` *class* with `update()` method is the domain model; the *storage* is SoA arrays; a `BodyRef { Bodies& b; BodyId i; double& x() { return b.x[i.i]; } }` proxy gives you back the object syntax where you need it without paying for it in the kernel. ECS (entity-component-system) engines (EnTT, flecs) are this idea industrialized.

---

## 17. Reading big C++ codebases

Every large library is unreadable from the top; read from a *question*. What to look at first:

**PyTorch — ATen and the dispatcher** (`aten/src/ATen/`). Start with `native/native_functions.yaml`: every operator's signature and which kernel implements it per backend (`CPU: add_out`, `CUDA: add_out_cuda`, `Autograd: ...`). Then `core/dispatch/Dispatcher.h` — a table `[OperatorHandle][DispatchKey] → KernelFunction`, where `DispatchKey` (`c10/core/DispatchKeySet.h`) is a bitset computed from the tensors' device/dtype/autograd status; the highest-priority key wins (Autograd wraps CPU). `c10/core/TensorImpl.h` is §9's `Storage + View`. `ATen/TensorIterator.h` is how one kernel body serves broadcasting, strides and dtype via `AT_DISPATCH_FLOATING_TYPES` macros. Read `native/cpu/BinaryOpsKernel.cpp` to see vectorization through `at::vec::Vectorized<T>`. Lesson: *one* dispatch mechanism, codegen from a declarative source, views everywhere.

**Eigen — expression trees** (`Eigen/src/Core/`). `MatrixBase.h` is the CRTP base (`Derived` is the actual expression type); `CwiseBinaryOp.h` is `a + b` as a type; `Product.h`/`ProductEvaluators.h` decide when a product must materialize a temporary; `AssignEvaluator.h` is where the fused loop actually runs (`call_dense_assignment_loop`); `arch/NEON/PacketMath.h` is the SIMD layer (`Packet2d`, `pmadd`). Lesson: CRTP + evaluators separate *what* from *how*; `auto` is dangerous because the *what* is a type.

**LLVM — small data structures and RTTI-free dispatch** (`llvm/include/llvm/ADT/`, `llvm/Support/`). `SmallVector.h`: an inline buffer of N elements, heap only after (§2.3's SBO for containers; `SmallVectorTemplateBase<T, bool TriviallyCopyable>` specializes on `memcpy`-ability). `StringRef.h`: the non-owning view that predates `string_view`. `ArrayRef.h`: `span` before `span`. `Casting.h`: `isa<>`, `cast<>`, `dyn_cast<>` — dynamic type checks *without* RTTI (`-fno-rtti`), via a `getKind()` enum in the base and a `static bool classof(const Base*)` in each derived class; used because `dynamic_cast` is slow and RTTI bloats binaries. `Instruction.h`'s `getOpcode()` is the kind. Lesson: know your sizes; a closed hierarchy with an enum kind is faster than RTTI and lets you `switch`.

**Abseil** (`absl/`). `container/flat_hash_map.h` (Swiss tables; `container/internal/raw_hash_set.h` for the SIMD group probing) — how a hash map should have been designed; `strings/str_cat.h` — how to make string formatting fast (`AlphaNum` conversion + one allocation); `status/status.h` — the `StatusOr<T>` error model (§6.3, the "expected" school); `base/optimization.h` — `ABSL_PREDICT_TRUE`, cache-line constants. Lesson: API design docs (`absl/ABSEIL_ISSUE_TEMPLATE`, the "tip of the week" series at abseil.io/tips) are as important as the code.

**folly** (`folly/`). `FBVector.h` (why `std::vector` growth factor 2 is wrong — 1.5 reuses freed blocks), `small_vector.h`, `Function.h` (a move-only `std::function` with SBO, §2.3 as production code), `io/IOBuf.h` (chained buffers), `synchronization/` (`Baton`, `RWSpinLock`). Read `folly/docs/` first; the design rationales are unusually good.

Technique: clone, build with `compile_commands.json` (CMake `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`), open in an editor with clangd, put a breakpoint at the *public* entry point (`at::add`, `Eigen::MatrixBase::operator+`) and step *down*. `git log -S'symbol'` finds why a line exists. Read the tests for the API contract before the implementation.

---

## 18. Code review checklist

Use it on your own P0x code before anyone else does.

**Correctness**
- Every function's precondition is either checked or stated (`assert` + comment).
- No UB you can name: signed overflow, uninitialized reads, dangling views/`string_view`s, out-of-bounds, strict-aliasing violations, data races, order-of-evaluation dependence, use-after-move.
- Every `new`/resource has an owner; no raw owning pointers in interfaces.
- Exceptions: every function that can throw is exception-safe (basic guarantee at least); `noexcept` moves/swaps; no throw across `extern "C"`/plugin/thread boundaries.
- Floating point: tolerance comparisons; NaN handling stated; no `==` on computed doubles.

**Design**
- Types are regular unless documented otherwise; copy means deep copy.
- Functions take views, return owners; sink params by value; `const&` otherwise.
- No upward dependency introduced; new public header pulled into `fwd.hpp` if needed.
- Hot-path polymorphism is compile-time; cold-path is erased/virtual; no `std::function` in kernels.
- No new singleton/global mutable state.
- Strong types where two same-typed parameters are adjacent.

**API**
- `explicit` single-arg ctors; `[[nodiscard]]` on pure functions; hidden friends for operators.
- Names follow the codebase (`snake_case` functions, `CamelCase` types, `trailing_` members — pick one).
- Every public function has a one-line doc comment with units and failure behavior.

**Tests & tooling**
- New behavior has a test; a bug fix has a regression test with the bug's ID.
- Builds with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`; ASan/UBSan job green.
- Benchmark before/after attached for any change claimed to be "faster".
- No commented-out code; no `TODO` without an issue number; formatted with the repo's `.clang-format`.

---

## 19. How professionals structure a 50k-line C++ library

```
num/                                  # repo root; `num` is the library and namespace name
├── CMakeLists.txt                    # project(num VERSION 0.4.1 LANGUAGES CXX); options NUM_BUILD_TESTS, NUM_USE_OPENMP, NUM_USE_BLAS
├── CMakePresets.json                 # "dev-asan", "release", "ci-ubuntu-clang" presets  (../19_professional_tooling_and_quality)
├── cmake/                            # FindAccelerate.cmake, CompilerWarnings.cmake, numConfig.cmake.in (for find_package(num))
├── include/num/                      # PUBLIC headers: the ONLY thing users may include. Mirrors namespaces.
│   ├── num.hpp                       # umbrella (convenience; discouraged in library code)
│   ├── fwd.hpp                       # forward declarations only
│   ├── core/  {types.hpp strong.hpp error.hpp storage.hpp span.hpp}
│   ├── tensor/{tensor.hpp view.hpp ops.hpp}
│   ├── autograd/{node.hpp engine.hpp}
│   ├── nn/    {layer.hpp linear.hpp activations.hpp sequential.hpp}
│   ├── optim/ {sgd.hpp adam.hpp}
│   ├── linalg/{gemm.hpp solve.hpp eig.hpp}
│   └── io/    {npy.hpp checkpoint.hpp}
├── src/                              # implementation .cpp + INTERNAL headers (not installed, may change any time)
│   ├── core/  ...
│   ├── kernels/{gemm_neon.cpp gemm_blas.cpp reduce.cpp}   # platform code, guarded; one file per backend
│   └── internal/{simd.hpp threadpool.hpp}                 # num::detail
├── tests/                            # one test file per public header: tests/tensor/test_view.cpp; doctest; ctest labels unit/slow/sanitizer
│   └── golden/                       # checkpoint_v1.bin, expected_loss_seed42.txt
├── bench/                            # Google Benchmark targets; never built by default; results/ checked in as CSV per machine
├── examples/                         # small complete programs; compiled in CI so they never rot
├── docs/                             # DESIGN.md (layers, value/handle decisions), ERRORS.md, ABI.md, CHANGELOG.md, mkdocs.yml or Doxyfile
├── tools/                            # scripts: format.sh, check_layers.py, gen_ops.py (codegen), update_golden.py
├── third_party/                      # git submodules or FetchContent pins (doctest, benchmark, fmt); NEVER edited in place
├── .github/workflows/{ci.yml sanitizers.yml tidy.yml docs.yml}
├── .clang-format  .clang-tidy  .clangd  .editorconfig  .gitattributes
└── LICENSE  README.md  CONTRIBUTING.md
```

Conventions that go with it:

- **Public vs internal headers**: `include/num/` is installed and versioned; `src/**/*.hpp` is not. Anything in `num::detail` may change in a patch release.
- **One CMake target per layer** (`num::core`, `num::tensor`, …) plus an `INTERFACE` umbrella `num::num`; tests link the layer they test. `install(EXPORT ...)` + `numConfig.cmake` so downstream does `find_package(num CONFIG)`.
- **Tests mirror headers** one-to-one so "is this tested?" is answered by `ls`. Labels split fast unit tests (run on every commit) from slow ones (nightly).
- **Benches are separate** and pinned to `-O3 -mcpu=native -DNDEBUG`; results are data, checked in with machine + compiler + commit hash.
- **Docs live with code**: a `DESIGN.md` per layer that says which types are values, the error policy, the threading contract ("all `const` methods are thread-safe; no method is safe to call concurrently with a non-const one on the same object").
- **Codegen where the schema is data**: an `ops.yaml` listing every elementwise op + a Python script generating the dispatch table and Python bindings (what ATen does). The generated files are checked in or generated at configure time — pick one, document it.
- **Versioning**: semver on the *public headers and ABI*; `inline namespace v0 {}` if you ever need two versions to coexist.

Rough line budget for such a library: 30% kernels, 20% tests, 15% public headers, 15% infrastructure/build/CI, 10% docs, 10% bindings/examples. If tests are under 15% you're not at professional grade yet.

---

## Gotchas and undefined behavior

- **Type-erasure converting constructor hijacks the copy constructor.** `template<class T> Layer(T)` without an `enable_if`/`requires` excluding `Layer` itself: `Layer b(a)` with non-const `a` calls the template → infinite recursion or a `Model<Layer>` wrapping a `Layer`. Always constrain it.
- **Placement-new'd object never destroyed / storage reused without destroy** → leak or UB ([basic.life]). Manual SBO must call the destructor through the vtable before reusing the buffer.
- **Reading a `char` buffer as a `T` you never constructed there** is UB (strict aliasing, [basic.lval]). Placement-`new` first; use `std::launder` when the previous object there had `const`/reference members.
- **`std::function` holding a lambda that captures a local by reference and outlives it**: dangling. Capture by value or hold a `shared_ptr`.
- **pimpl destructor in the header** → `unique_ptr<Impl>` instantiates `delete` on an incomplete type → compile error inside `<memory>` (`static_assert(sizeof(_Tp) > 0)`). Define `~Solver()` in the `.cpp`.
- **View outliving its owner** (`auto r = make_matrix().row(0);`) — dangling, no diagnostic. Views must never be returned from functions that take their owner by value or create it.
- **ADL surprises**: a function named `swap`, `size`, `data`, `begin` in your namespace becomes a candidate whenever one of your types is an argument — including inside standard library internals. Prefer hidden friends and avoid generic names for unrelated functions.
- **Static initialization order fiasco**: a global in TU A initialized from a global in TU B is UB-adjacent (unspecified order across TUs, [basic.start.dynamic]). Use function-local statics (`Registry& registry() { static Registry r; return r; }`), which are initialized on first use and thread-safe (C++11 [stmt.dcl]/4).
- **Plugin `dlclose` while objects live** → next virtual call jumps into unmapped memory. Destroy everything from the plugin first; many hosts never `dlclose` at all.
- **Deleting a plugin object with the host's `delete`** — different allocator/runtime → heap corruption. Always call the plugin's `destroy`.
- **`fwrite(&s, sizeof s, 1, f)` of a struct** — padding, endianness, `bool` size, `long` width all differ across platforms; the file is not a format. Serialize field by field with fixed-width types.
- **Observer emits into a subscriber during its own destruction** — if the subscriber's destructor doesn't disconnect *first*, `emit` calls a half-dead object. RAII connection tokens as members: they're destroyed *before* the enclosing object's other members only if declared *after* them — declare the connection last (members destroy in reverse declaration order, [class.dtor]/9).
- **`shared_ptr` cycles in observer/graph designs leak silently.** `weak_ptr` on the back edge; `leaks --atExit ./prog` on macOS (LeakSanitizer isn't available on Apple Silicon).
- **`-fvisibility=hidden` forgotten** on a `.dylib`: every symbol exported, including `detail::` ones; users link against internals and your "internal" becomes ABI.

---

## Common mistakes checklist

- [ ] Every type is regular (or documented as a handle); `==` compares values, not identities.
- [ ] Type-erased wrappers constrain their converting constructor and deep-copy via `clone()`.
- [ ] `std::function`/erasure only where the callee is chosen at run time; templates in kernels.
- [ ] Optimizers/schedulers/reductions that run per element are policies, not virtuals.
- [ ] Adjacent same-type parameters (`rows, cols`, `lr, momentum`) are strong types or a struct.
- [ ] Anything exported from a shared library or including heavy headers is behind pimpl; dtor/moves defined in the `.cpp`.
- [ ] Functions take views, return owners; no view returned from a temporary.
- [ ] Error policy written in `docs/ERRORS.md`; hot paths `noexcept`; `at()` vs `operator()` both exist.
- [ ] Operators and customization functions are hidden friends.
- [ ] Dependencies injected through constructors; no `instance()` singletons.
- [ ] Layers depend only downward; CMake targets enforce it; a CI grep or `iwyu` catches leaks.
- [ ] Binary formats have magic, version, fixed-width little-endian fields, TLV, checksum, golden-file test.
- [ ] Plugins cross the boundary via `extern "C"` factory + abstract interface + plugin-side `destroy`.
- [ ] Observers return RAII connection tokens; `emit` iterates a copy.
- [ ] Repo laid out `include/` vs `src/`, `tests/` mirroring headers, `bench/` separate, `docs/DESIGN.md` present.

---

## You can move on when...

- You can define "regular type" in one sentence and name which of your P01–P05 types are values and which are handles — and why.
- You can write `Function<R(Args...)>` from memory, explain why the converting constructor needs a constraint, and say how `std::function` avoids the heap for small callables.
- You can argue, with a cost model, when a strategy should be a template policy and when it should be virtual/erased.
- You can pimpl a class correctly (dtor in the `.cpp`), state what ABI means, and name two things that break it.
- You can write the "take views, return owners" rule and spot the dangling-view bug in a code review.
- You can draw your NN framework as composition (Layer erased, Sequential contains, Model has-a, optimizer as policy, serialization as free functions) and explain why it beats one inheritance tree.
- You can write a versioned TLV binary format and explain how an old reader survives a new file.
- You can load an `extern "C"` factory via `dlopen` and list the three rules that keep it from crashing.
- You can lay out a 50k-line library directory from memory and say which headers a user may include.
- You know where to start reading ATen, Eigen, LLVM ADT, Abseil, and folly, and what each teaches.
