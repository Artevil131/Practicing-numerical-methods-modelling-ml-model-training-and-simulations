# Chapter 13 — Modern C++ and Idioms

## What you'll be able to do after this chapter

- Use the C++17 features that make numerical code shorter and safer: structured bindings, `if constexpr`, `std::optional`, `std::variant` + `std::visit`, `std::string_view`, fold expressions, CTAD, `std::filesystem`.
- Recognise the C++20 features (concepts, ranges, `std::span`, `<format>`, `<=>`, designated initializers, coroutines, modules) when you meet them, and know which ones Apple clang 21 gives you today.
- Apply the idioms that experienced C++ programmers apply without thinking: RAII, Rule of Zero, `enum class`, `const`/`constexpr` by default, `explicit`, strong types, `unique_ptr` over `new`.
- Choose between `std::function` and templates, and know what pimpl, CRTP, type erasure, and tag dispatch are for when you see them in a library.
- Read Eigen, PyTorch ATen, and GLM without panic, and know the ~20% of C++ your ML/physics code actually needs.

## Why this matters for ML / numerics / sims

Chapters 01–12 gave you the language. This chapter is about *taste*: which features to reach for, which to avoid, and how the codebases you'll read (Eigen, PyTorch, GLM) actually look. An autograd engine is a graph of nodes with different operations — `std::variant` is the right tool, and knowing that saves you from a virtual-dispatch hierarchy. A tokenizer that slices strings a million times wants `std::string_view`, not `std::string`. A `Matrix(3, 4)` vs `Matrix(4, 3)` bug is prevented by strong types. None of this is required to make code *work*; all of it is required to make code you can still read and extend six months later.

---

## 1. Structured bindings

Unpack a `pair`, `tuple`, array, or struct with public members into named variables.

```cpp
#include <map>
#include <tuple>
#include <iostream>

struct Shape { std::size_t rows, cols; };
Shape shape_of() { return {3, 4}; }

std::tuple<double, double, int> solve_stats() { return {0.001, 1e-9, 42}; }

int main() {
    auto [r, c] = shape_of();                       // struct: members in declaration order
    auto [loss, grad_norm, iters] = solve_stats();  // tuple
    std::map<std::string, double> params{{"lr", 0.01}, {"wd", 1e-4}};
    for (const auto& [name, value] : params)        // pair<const string, double>
        std::cout << name << '=' << value << ' ';
    if (auto [it, inserted] = params.insert({"lr", 0.1}); !inserted)
        std::cout << "\nlr already present: " << it->second << '\n';
    std::cout << r << 'x' << c << ' ' << loss << ' ' << grad_norm << ' ' << iters << '\n';
}
// Output:
// lr=0.01 wd=0.0001
// lr already present: 0.01
// 3x4 0.001 1e-09 42
```

Python equivalent: `r, c = shape_of()`, `for name, value in params.items()`. Use `auto&` / `const auto&` to bind by reference (no copy); plain `auto` copies the whole object first.

---

## 2. `if constexpr`

A compile-time `if` inside a template: the discarded branch is not instantiated, so it may contain code that wouldn't compile for that type.

```cpp
#include <type_traits>
#include <string>

template <typename T>
std::string describe(const T& x) {
    if constexpr (std::is_floating_point_v<T>) {
        return "float-like " + std::to_string(x);
    } else if constexpr (std::is_integral_v<T>) {
        return "int-like " + std::to_string(x);
    } else {
        return "other of size " + std::to_string(sizeof(T));   // x.size() etc. would be fine here
    }
}
// describe(2.5) -> "float-like 2.500000"; describe(7) -> "int-like 7"; describe(Shape{}) -> "other of size 16"
```

Before C++17 this needed overloads, SFINAE, or tag dispatch (section 22). Typical numerics use: `if constexpr (std::is_same_v<T, float>)` to call `cblas_sgemm` vs `cblas_dgemm`; choosing `float32x4_t` vs `float64x2_t` NEON types.

---

## 3. `std::optional`

Covered in `../10_error_handling/lesson.md` §4. Summary: a value that may be absent, with `has_value()`, `*`, `->`, `.value()`, `.value_or()`. Use for lookups and parsers; not for errors with a reason.

```cpp
std::optional<int> token_id(std::string_view tok);   // nullopt if unknown
if (auto id = token_id("hello")) use(*id);
```

---

## 4. `std::variant` and `std::visit`

A **type-safe tagged union**: holds exactly one of a fixed set of types, and remembers which. This is the right way to write "a node is one of these kinds" — the thing C did with `enum` + `union` + discipline (`../../c_learning/07_structs_unions_enums/lesson.md`), and Python does with duck typing.

```cpp
#include <variant>
#include <vector>
#include <iostream>

// An autograd op as a closed set of alternatives. Each alternative is a plain struct.
struct Add  { int lhs, rhs; };
struct Mul  { int lhs, rhs; };
struct Tanh { int arg; };
struct Leaf { double value; };
using Op = std::variant<Leaf, Add, Mul, Tanh>;

struct Node { Op op; double value = 0, grad = 0; };

// A visitor: one overload per alternative. std::visit picks the right one at run time.
struct Forward {
    const std::vector<Node>& nodes;
    double operator()(const Leaf& l) const { return l.value; }
    double operator()(const Add& a)  const { return nodes[a.lhs].value + nodes[a.rhs].value; }
    double operator()(const Mul& m)  const { return nodes[m.lhs].value * nodes[m.rhs].value; }
    double operator()(const Tanh& t) const { return std::tanh(nodes[t.arg].value); }
};

// The "overloaded" helper: build a visitor from lambdas (C++17 idiom; std::overload proposed).
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;   // CTAD guide (section 9)

int main() {
    std::vector<Node> g{{Leaf{2.0}}, {Leaf{3.0}}, {Mul{0, 1}}, {Tanh{2}}};
    for (auto& n : g) n.value = std::visit(Forward{g}, n.op);        // topological order
    std::cout << g[2].value << ' ' << g[3].value << '\n';

    for (const auto& n : g)
        std::cout << std::visit(overloaded{
            [](const Leaf&) { return "leaf"; }, [](const Add&) { return "add"; },
            [](const Mul&)  { return "mul"; },  [](const Tanh&) { return "tanh"; }}, n.op) << ' ';
    std::cout << '\n';

    if (auto* m = std::get_if<Mul>(&g[2].op)) std::cout << "node 2 multiplies " << m->lhs << " and " << m->rhs << '\n';
    std::cout << g[3].op.index() << ' ' << std::holds_alternative<Tanh>(g[3].op) << '\n';
}
// Output:
// 6 0.999988
// leaf leaf mul tanh
// node 2 multiplies 0 and 1
// 3 1
```

Why variant instead of a virtual `Op` hierarchy: the set of ops is closed and known; `std::visit` is a jump table (no vtable pointer per node, nodes stay contiguous and copyable); adding a new op makes every non-exhaustive visitor a *compile error* rather than a silent fall-through. Use inheritance (`../09_inheritance_and_polymorphism/lesson.md`) when the set is open (plugins, user-defined layers).

API: `std::get<T>(v)` (throws `bad_variant_access`), `std::get_if<T>(&v)` (nullptr), `v.index()`, `std::holds_alternative<T>(v)`, `std::monostate` as an "empty" alternative.

---

## 5. `std::string_view`

A non-owning `(pointer, length)` view of characters. Slicing a `string_view` is O(1) and allocates nothing; slicing a `std::string` copies.

```cpp
#include <string_view>
#include <vector>

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    while (!s.empty()) {
        auto pos = s.find(sep);
        out.push_back(s.substr(0, pos));                  // view into the original; no copy
        if (pos == std::string_view::npos) break;
        s.remove_prefix(pos + 1);
    }
    return out;
}
// split("1.5,2,abc", ',') -> {"1.5", "2", "abc"}  — three pointers into the same buffer
```

Rules:
- Take `std::string_view` as a parameter whenever you only *read* the string (replaces `const std::string&` and `const char*`; accepts literals, `std::string`, other views without copies).
- **Never** store a `string_view` that outlives the string it views (dangling — UB). Returning a view into a local `std::string` is the classic bug.
- Not null-terminated: don't pass `.data()` to C APIs expecting a C string.

For your tokenizer: read the corpus into *one* `std::string`, tokenize into `string_view`s, and only build `std::string` keys where you must (the vocabulary map). `std::span<T>` (C++20) is the same idea for arrays.

---

## 6. `std::any` (rarely)

Holds a value of *any* type, type-checked at extraction (`std::any_cast<T>`). It's a heap-allocating, type-erased box. Use when you genuinely can't enumerate the types (a plugin config bag). If you can enumerate them, use `std::variant`. In numerics code you will almost never need it.

```cpp
std::any a = 42; a = std::string("hi");
if (auto* s = std::any_cast<std::string>(&a)) std::cout << *s;   // "hi"
```

---

## 7. Fold expressions

Apply a binary operator across a parameter pack (variadic template).

```cpp
template <typename... Ts> auto sum(Ts... xs) { return (xs + ... + 0); }          // right fold with init
template <typename... Ts> bool all_positive(Ts... xs) { return ((xs > 0) && ...); }
template <typename... Ts> void print_all(const Ts&... xs) { ((std::cout << xs << ' '), ...); std::cout << '\n'; }

// sum(1, 2.5, 3) -> 6.5 ;  all_positive(1, 2, -3) -> false ;  print_all("shape", 3, 'x', 4) -> shape 3 x 4
```

Real use: a `Tensor(3, 4, 5)` constructor taking any number of dimensions: `template <class... Dims> explicit Tensor(Dims... d) : shape_{static_cast<std::size_t>(d)...}, size_((std::size_t{1} * ... * d)) {}`.

---

## 8. `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`, `[[deprecated]]`

Attributes are hints to the compiler that produce or suppress warnings.

```cpp
[[nodiscard]] Matrix transpose() const;        // m.transpose(); alone -> warning (Chapter 10 §19)
[[maybe_unused]] const int debug_level = 0;    // silence "unused variable" in release builds
switch (op) { case 0: x++; [[fallthrough]]; case 1: y++; break; }   // intentional fall-through, no warning
[[deprecated("use matmul_into")]] Matrix old_matmul(const Matrix&, const Matrix&);
```

---

## 9. Inline variables and CTAD

**Inline variables** (`inline constexpr double kG = 6.674e-11;` in a header) — see `../11_headers_build_cmake_testing/lesson.md` §7.

**Class template argument deduction (CTAD)**: the compiler deduces template arguments of a class from the constructor arguments, as it always did for function templates.

```cpp
std::pair p{1, 2.5};                    // pair<int, double>
std::vector v{1.0, 2.0, 3.0};           // vector<double>
std::array a{1, 2, 3};                  // array<int, 3>
std::lock_guard lock(mu);               // lock_guard<std::mutex>
std::tuple t{1, 'c', "s"};              // tuple<int, char, const char*>
```

For your own templates, a *deduction guide* tells the compiler how to deduce: `template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;` (section 4). Beware `std::vector v{3, 4};` is `{3, 4}` (two elements), not four elements — braces mean initializer list.

---

## 10. `std::filesystem`

Portable paths, directory iteration, file queries. Header `<filesystem>`, namespace alias `namespace fs = std::filesystem;`.

```cpp
#include <filesystem>
namespace fs = std::filesystem;

fs::path data = fs::path("data") / "mnist" / "train.bin";   // operator/ joins with the right separator
if (fs::exists(data)) std::cout << fs::file_size(data) << " bytes\n";
for (const auto& entry : fs::directory_iterator("checkpoints"))
    if (entry.path().extension() == ".bin") std::cout << entry.path().filename() << '\n';
fs::create_directories("out/run1");
std::cout << data.stem() << ' ' << data.extension() << ' ' << data.parent_path() << '\n';   // "train" ".bin" "data/mnist"
```

Throwing overloads throw `fs::filesystem_error`; every function also has an `std::error_code&` overload for the non-throwing path. Python equivalent: `pathlib.Path`.

---

## 11. Nested namespaces

```cpp
namespace ml::linalg {          // C++17: instead of namespace ml { namespace linalg { ... } }
    Matrix matmul(const Matrix&, const Matrix&);
}
namespace fs = std::filesystem;  // namespace alias
using ml::linalg::matmul;        // using-declaration: bring ONE name in (fine in .cpp files)
```

---

## 12. C++20 features to know exist

Compile with `-std=c++20`. Status on Apple clang 21 (libc++), checked on this machine:

| Feature | What | Apple clang 21 |
|---|---|---|
| **Concepts** / `requires` | Named constraints on template parameters; readable errors | Yes |
| **Ranges** (`std::ranges::`, `std::views::`) | Composable lazy pipelines over containers | Yes (`__cpp_lib_ranges` 202110) |
| **`std::span<T>`** | Non-owning view of contiguous memory (pointer + size) | Yes |
| **`<format>`** / `std::format` | Python-style `"{:.3f}"` formatting, type-safe | Yes (`__cpp_lib_format` 202110); `std::print` is C++23 |
| **`<=>`** (spaceship) | One operator generates all six comparisons; `= default` for memberwise | Yes |
| **Designated initializers** | `Config{.lr = 0.01, .epochs = 10}` | Yes |
| **Coroutines** | `co_await`/`co_yield`; needs a library (no std generator until C++23) | Compiler yes, practical use needs a helper library |
| **Modules** | `import std;` replacing headers | Compiler support exists; build-system (CMake ≥3.28 + Ninja) support is immature. Not for everyday use yet. |
| `std::jthread`, `std::atomic_ref`, `std::bit_cast`, `<numbers>` (`std::numbers::pi`), `consteval`, `constinit`, `[[likely]]` | Smaller conveniences | Yes |

Examples of the three you'll actually want:

```cpp
// Concepts: constrain templates readably. Errors say "does not satisfy Number" instead of 40 lines.
template <typename T> concept Number = std::integral<T> || std::floating_point<T>;
template <Number T> T sq(T x) { return x * x; }
// sq("s")  ->  error: constraints not satisfied ... 'const char*' does not satisfy 'Number'

// span: pass "array + length" as one object, works for vector, array, C array, pointer+size.
double mean(std::span<const double> xs) { double s = 0; for (double x : xs) s += x; return s / xs.size(); }
// mean(vec); mean(arr); mean({ptr, n});  — no template, no copy

// ranges: lazy pipelines. Nothing is computed until the for-loop pulls.
for (int sq : v | std::views::filter([](int i) { return i % 2 == 0; })
                | std::views::transform([](int i) { return i * i; })) std::cout << sq << ' ';

// format: Python's f-string, type-checked at compile time.
std::cout << std::format("epoch {:3d}  loss {:.4f}  lr {:.1e}\n", epoch, loss, lr);
```

This course stays on C++17 because most libraries you'll read still target it. Move to C++20 for your own projects the moment you want `span`, `format`, or concepts — they're all safe on your toolchain.

---

## 13. Idioms: RAII everywhere, Rule of Zero

**RAII**: every resource (memory, file, lock, GPU buffer, timer) is owned by an object whose destructor releases it. There is no `finally` in C++ because you don't need one (`../02_classes_and_raii/lesson.md`, `../10_error_handling/lesson.md` §9).

**Rule of Zero**: if your class holds only RAII members (`std::vector`, `std::string`, `std::unique_ptr`, other Rule-of-Zero types), declare *none* of the five special members. The compiler-generated ones are correct.

```cpp
class Matrix {                       // Rule of Zero: no destructor, no copy/move ops written
    std::size_t rows_, cols_;
    std::vector<double> data_;        // owns the memory; copies/moves/destroys itself
public:
    Matrix(std::size_t r, std::size_t c) : rows_(r), cols_(c), data_(r * c) {}
};
```

You only write the Rule of Five (destructor + copy ctor/assign + move ctor/assign) when you *are* the RAII wrapper around a raw resource (`FILE*`, `cudaMalloc`, a socket). Then write all five, or `= delete` the copies.

---

## 14. `enum class`, `using` aliases

```cpp
enum class Activation { ReLU, Tanh, Sigmoid };     // scoped: Activation::ReLU; no implicit int conversion
enum class Axis : std::uint8_t { Row = 0, Col = 1 };   // fixed underlying type
Activation a = Activation::ReLU;
// int i = a;                    // error (good): use static_cast<int>(a) if you really mean it
switch (a) { case Activation::ReLU: ...; case Activation::Tanh: ...; case Activation::Sigmoid: ...; }
// -Wall warns if a case is missing and there's no default. Omit `default:` to keep that warning.

using Vec = std::vector<double>;                        // alias: prefer over typedef
template <class T> using Grid = std::vector<std::vector<T>>;   // alias template (typedef can't do this)
```

Plain `enum` leaks its names into the enclosing scope and converts to `int` silently; `enum class` fixes both.

---

## 15. `auto` guidelines, `const` and `constexpr` by default

`auto`:
- **Use** when the type is obvious from the right side (`auto m = std::make_unique<Matrix>(3, 4);`), when it's long and irrelevant (`auto it = params.find("lr");`), for lambdas (no other way), and in range-for (`const auto&`).
- **Avoid** when the type carries meaning the reader needs (`double lr = 0.01;` not `auto lr = 0.01;` — and `auto x = 1;` is `int`, which bites when you meant `std::size_t` or `double`).
- `auto` never deduces a reference: `auto m = mats[0];` copies. Write `auto&` or `const auto&`.

`const` by default: every local that isn't reassigned, every parameter that isn't modified (`const Matrix&`), every method that doesn't mutate (`double norm() const`). It documents intent, catches bugs, and lets the compiler assume more.

`constexpr` by default for constants: `constexpr double kEps = 1e-9;` not `const double` or `#define EPS 1e-9`. `constexpr` guarantees compile-time evaluation and has a type and a scope.

---

## 16. Prefer `std::array`/`vector`, algorithms, `unique_ptr`

| Instead of | Use | Why |
|---|---|---|
| `double v[3];` | `std::array<double, 3> v;` | Knows its size, copyable, works with algorithms, bounds-checked `.at()` |
| `double* buf = new double[n];` | `std::vector<double> buf(n);` | Owns and frees itself; `.size()`; grows |
| `T* p = new T(...); ... delete p;` | `auto p = std::make_unique<T>(...);` | Can't leak, can't double-free, moves cleanly |
| hand-written loop to find/sum/sort | `std::find_if`, `std::accumulate`, `std::sort` | Names the intent; fewer index bugs |
| `for (i...) if (v[i] > max) max = v[i];` | `*std::max_element(v.begin(), v.end())` | One line, obviously correct |

"Prefer algorithms *when clearer*." A three-line `for` loop that does two things is clearer than a `std::transform` with a 5-line lambda. Hot inner kernels (matmul) are hand-written loops — that's fine.

---

## 17. Never `using namespace std;` in headers; `explicit`; initialize everything

- `using namespace std;` in a header pollutes every file that includes it, and causes silent ambiguity (`std::size`, `std::data`, `std::ref` vs yours). In a `.cpp` it's tolerable; in a header, never.
- **`explicit` on single-argument constructors** prevents the compiler from converting silently:

```cpp
class Matrix { public: explicit Matrix(std::size_t n); };   // square n x n
void f(const Matrix&);
// f(5);          // without explicit: compiles, builds a 5x5 matrix, calls f. With explicit: error. Good.
```

- **Initialize every member** at declaration (`double lr_ = 0.01;`, `std::size_t n_ = 0;`) or in every constructor. An uninitialized `double` read is UB, and uninitialized members are the number-one source of "works in Debug, garbage in Release".

---

## 18. Strong types

Two `std::size_t` parameters in a row are a bug waiting to happen: `Matrix(cols, rows)`. A one-line wrapper struct makes the compiler check argument order.

```cpp
struct Rows { std::size_t v; };
struct Cols { std::size_t v; };

class Matrix {
public:
    Matrix(Rows r, Cols c) : rows_(r.v), cols_(c.v), data_(r.v * c.v) {}
    // ...
};
Matrix m(Rows{3}, Cols{4});      // reads like the math
// Matrix bad(Cols{4}, Rows{3}); // error: no matching constructor. Caught at compile time, not after training.
```

Same idea for `Seconds{0.01}` vs `Steps{100}`, `Radians` vs `Degrees`, `TokenId` vs `Position`. Zero runtime cost. Libraries: `NamedType`, `std::chrono` is itself a strong-type system (`std::chrono::milliseconds`).

---

## 19. The pimpl idiom (brief)

"Pointer to implementation": the header exposes only a class with a `std::unique_ptr<Impl>`; all members live in the `.cpp`. Consumers don't recompile when the implementation changes, and heavy includes (Accelerate, CUDA) stay out of your public headers.

```cpp
// solver.hpp
class Solver {
public:
    Solver(); ~Solver();                       // dtor DEFINED in .cpp, where Impl is complete
    Solver(Solver&&) noexcept; Solver& operator=(Solver&&) noexcept;
    void step();
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};
// solver.cpp
struct Solver::Impl { std::vector<double> state; /* BLAS handles etc. */ };
Solver::Solver() : p_(std::make_unique<Impl>()) {}
Solver::~Solver() = default;
void Solver::step() { /* p_->state ... */ }
```

Cost: one indirection per call, one heap allocation per object. Don't pimpl your `Vec3`.

---

## 20. CRTP (brief recap)

Curiously Recurring Template Pattern: a base class templated on its derived class, giving *compile-time* polymorphism — the base can call `static_cast<Derived&>(*this).method()` with no virtual call. Eigen uses it everywhere: `MatrixBase<Derived>` implements `.norm()`, `.transpose()` once for all matrix/expression types. See `../09_inheritance_and_polymorphism/lesson.md` for the mechanics. You'll *read* it far more than write it.

```cpp
template <class Derived> struct VecOps {
    double norm() const { const auto& d = static_cast<const Derived&>(*this); return std::sqrt(d.dot(d)); }
};
struct Vec3 : VecOps<Vec3> { double x, y, z; double dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; } };
```

---

## 21. Type erasure (brief)

Hide the concrete type behind a uniform interface *without* requiring the type to inherit from anything. `std::function`, `std::any`, and `std::shared_ptr`'s deleter are type-erased. You write it when you need "anything with a `.step(dt)` method" stored in one `std::vector` and the types come from unrelated libraries. Implementation: an internal abstract `Concept` + templated `Model<T>` behind a `unique_ptr`. It's the flexible-but-slower cousin of `std::variant` (which needs the closed set) and templates (which need the type at compile time).

---

## 22. Tag dispatch (brief)

Select an overload with an empty struct as an extra argument, at compile time:

```cpp
struct RowMajorTag {}; struct ColMajorTag {};
void fill_impl(Matrix&, RowMajorTag);
void fill_impl(Matrix&, ColMajorTag);
template <class Layout> void fill(Matrix& m) { fill_impl(m, Layout{}); }   // fill<RowMajorTag>(m)
```

The standard library uses it for iterator categories (`std::random_access_iterator_tag`). Since C++17, `if constexpr` replaces most uses; since C++20, concepts replace the rest.

---

## 23. `std::function` vs templates

| | Template parameter `template <class F> void apply(F f)` | `std::function<double(double)>` |
|---|---|---|
| Call cost | Inlined; zero | Indirect call; ~2–5 ns; blocks vectorization |
| Storage | Type known; no allocation | Type-erased; may heap-allocate (captures > ~16 bytes) |
| Can be stored in a container / member? | Awkward (type differs per lambda) | Yes — this is what it's for |
| Compile time / code size | One instantiation per lambda | One |
| Header-only? | Must be | No |

Rule: **templates for hot paths** (`integrate(f, ...)` where `f` is called a million times — the ODE right-hand side, the element-wise op in `map`), **`std::function` for storage and boundaries** (a callback stored in an `Optimizer`, a layer's activation chosen at run time and called once per batch). If the `std::function` call happens per *element*, you've lost 5–50×.

---

## 24. The C++ Core Guidelines and where to look things up

- **C++ Core Guidelines** (Stroustrup & Sutter): https://isocpp.github.io/CppCoreGuidelines — the rules behind everything in this chapter (F.16 "pass cheap types by value, others by `const&`", C.20 Rule of Zero, ES.20 initialize, I.4 strong types...). Search by rule code.
- **cppreference.com** — the reference. Always shows which standard added a feature and has runnable examples. Bookmark it; ignore anything else Google returns first.
- **Compiler Explorer** (godbolt.org) — paste code, see the assembly, compare `-O2` vs `-O3`, check whether a loop vectorized. Invaluable for Chapter 12 questions.
- **C++ Weekly** (Jason Turner, YouTube) — 10-minute videos, one feature each.
- Books: *A Tour of C++* (Stroustrup, short), *Effective Modern C++* (Meyers, C++11/14 idioms, still the best explanation of `auto`, moves, smart pointers).

---

## 25. Reading other people's C++

**Eigen** (header-only linear algebra): everything is a template; `Eigen::MatrixXd` is `Matrix<double, Dynamic, Dynamic>`; `a * b + c` builds an *expression template* (a type encoding the whole expression) that's evaluated once on assignment — no temporaries. CRTP `MatrixBase<Derived>` everywhere. Expect 200-line error messages when you mistype; read the *first* line. Column-major by default. `eigen_assert` for checks (off in release).

**PyTorch ATen / c10** (the C++ core under `torch`): `at::Tensor` is a handle (`intrusive_ptr` to `TensorImpl`) — copying a `Tensor` shares storage, exactly like Python. Ops are registered in a dispatcher keyed by device/dtype (`CPU`, `CUDA`, `Float`, `Half`) — a giant runtime tag dispatch. `TORCH_CHECK(cond, msg)` throws `c10::Error`. `AT_DISPATCH_FLOATING_TYPES(dtype, "name", [&] { ... scalar_t ... })` is a macro that instantiates a lambda for `float` and `double` — `if constexpr` done with macros because it predates C++17. Heavy on `c10::optional`, `c10::ArrayRef<T>` (= `span`), `IntArrayRef` for shapes.

**GLM** (graphics math, header-only): `glm::vec3`, `glm::mat4`, mirrors GLSL. Small fixed-size types, aggressive `inline`, operator overloading, SIMD via `#ifdef`s. The closest to what your own `Vec3` for the N-body/donut should look like.

What to expect in general: heavy templates, `namespace detail` for internals, `#ifdef` platform switches, macros for dispatch, `[[nodiscard]]`, `noexcept` on moves, `const&` everywhere, `std::size_t`/`int64_t` for indices, and very few raw `new`s.

---

## 26. What to avoid

| Avoid | Because | Instead |
|---|---|---|
| Raw `new`/`delete` in application code | Leaks on early return/throw; double frees | `std::vector`, `std::make_unique`, `std::make_shared` |
| C-style casts `(int)x`, `(double*)p` | Silently does `const_cast`/`reinterpret_cast` too | `static_cast<int>(x)`; `reinterpret_cast` only at byte-level IO |
| Macros for constants / small functions | No type, no scope, surprising expansion | `constexpr` variables, `inline`/`constexpr` functions, templates |
| `NULL` / `0` for pointers | Ambiguous overloads (`f(int)` vs `f(T*)`) | `nullptr` |
| Manual memory in application code (`malloc` buffers, ownership via comments) | Every bug in Chapters 05–06 of the C course | RAII types; raw pointers only as non-owning views |
| Deep inheritance hierarchies (`Layer → Activation → ReLU → LeakyReLU`) | Fragile, slow (virtual), hard to read | Composition, `std::variant`, templates, one level of interface |
| Premature abstraction (a `TensorBackendFactoryInterface` before you have two backends) | You'll guess the wrong interface | Write the concrete thing; abstract when the second use appears |
| `using namespace std;` in headers | Name pollution | Qualify, or `using` specific names in `.cpp` |
| `std::endl` | Flushes every line: 10–100× slower output | `'\n'` |
| `std::list`, `std::map` in hot paths | Pointer chasing, cache misses (Chapter 12) | `std::vector`, `std::unordered_map`, sorted vector |
| `volatile` for threading | Does not provide atomicity or ordering | `std::atomic`, `std::mutex` |
| `#define`-based include guards | Typos, collisions | `#pragma once` |
| Signed/unsigned mixing in loops (`for (int i = 0; i < v.size(); ++i)`) | `-Wsign-compare` warnings; wrap-around bugs | `std::size_t i`, or `std::ssize`/`std::ptrdiff_t` |

---

## 27. A C++ subset for numerics

The ~20% of C++ your ML/physics code needs. If a feature isn't here, you can look it up when a library forces it on you.

**Types & values**: `double`, `float`, `std::size_t`, `std::int64_t`, `bool`; `constexpr` constants; `enum class`; `struct` with public members for plain data; `class` with invariants for everything else.

**Ownership & containers**: `std::vector<T>` (99% of storage), `std::array<T, N>` (small fixed vectors), `std::string` + `std::string_view`, `std::unordered_map` (vocab), `std::unique_ptr` (rare: pimpl, polymorphic nodes). No raw owning pointers. Raw `T*`/`const T*` + length only as non-owning kernel arguments (or `std::span` in C++20).

**Functions**: `const T&` in, value out; `[[nodiscard]]`; `noexcept` on moves/swaps; `explicit` ctors; strong types for dimension arguments; default arguments sparingly; overloads for `float`/`double` or a template.

**Templates**: `template <typename T>` functions and classes for `float`/`double` genericity; `if constexpr` for type-dependent branches; concepts (C++20) if you want readable errors. No SFINAE, no template metaprogramming.

**Errors**: `std::invalid_argument`/`std::out_of_range` at API boundaries; `assert` in kernels; `std::optional` for lookups (Chapter 10).

**Sum types**: `std::variant` + `std::visit` for autograd ops / AST nodes / event kinds. Inheritance only for open plugin-style interfaces (one abstract base, `virtual`, `override`, `final`).

**Algorithms**: `std::sort`, `std::fill`, `std::copy`, `std::accumulate`/`std::reduce`, `std::min_element`/`max_element`, `std::transform`, `std::iota`, `std::clamp`; lambdas with `[&]`/`[=]` captures; range-for with `const auto&`.

**Numerics library**: `<cmath>` (`std::sqrt`, `std::exp`, `std::fma`, `std::isfinite`), `<random>` (`std::mt19937`, `std::normal_distribution`), `<numeric>`, `<limits>` (`std::numeric_limits<double>::epsilon()`), `<chrono>` for timing.

**Performance**: raw pointers + `size` hoisted out of hot loops, `reserve`/`emplace_back`, `_into` output parameters, SoA layouts, OpenMP pragmas (Chapter 12).

**Build**: headers with `#pragma once`, `inline` for header-defined functions, CMake with targets, doctest, sanitizers (Chapter 11).

**Skip for now**: multiple inheritance, virtual inheritance, `dynamic_cast`, exceptions specifications beyond `noexcept`, custom allocators, `std::shared_ptr` cycles/`weak_ptr`, coroutines, modules, template metaprogramming, `std::any`, `volatile`, `goto`, `register`/`auto` storage classes, C-style variadic functions, `union` (use `variant`), `std::bind` (use lambdas), locale/iostream formatting flags (use `std::format` or `snprintf`).

---

## Gotchas and undefined behavior

- **Dangling `std::string_view`**: returning a view into a local `std::string`, or storing a view of a temporary (`std::string_view sv = s + "x";`). UB on read.
- **`std::visit` with a non-exhaustive visitor** is a *compile* error (good). A visitor with mismatched return types across alternatives is also a compile error — make them agree (`return std::string(...)` in all).
- **`std::get<T>` on the wrong alternative** throws `std::bad_variant_access`. `get_if` returns `nullptr` instead.
- **Structured bindings copy** unless you write `auto&`/`const auto&`. `for (auto [k, v] : map)` copies every pair.
- **`auto x = 1;` is `int`**; `auto x = 1u;` is `unsigned`; `auto x = 1.0f;` is `float`. Mixing with `std::size_t` gives sign-compare warnings and wraparound.
- **`auto` never deduces a reference or `const`**: `auto m = get_const_ref();` is a mutable copy.
- **`explicit` missing** → `Matrix m = 5;` compiles and means something unintended.
- **Uninitialized members** → UB when read; `-O2` produces different garbage than `-O0`. Initialize at declaration.
- **`std::vector v{3, 4}` vs `std::vector<int> v(3, 4)`**: two elements `{3,4}` vs three elements of value 4.
- **CTAD with `std::vector v{some_vector}`** deduces `vector<vector<...>>`? No — it copies (special rule), but `std::vector v{a, b}` with two vectors gives `vector<vector<T>>`. Be explicit when in doubt.
- **`std::function` in a per-element loop**: correct, but 5–50× slower than a template parameter and blocks vectorization.
- **Non-`noexcept` move constructor** (Rule of Five written by hand and forgetting `noexcept`) → `vector` copies on growth. Rule of Zero avoids this entirely.
- **`enum class` in `switch` with a `default:`** hides the "unhandled enumerator" warning when you add a new value. Omit `default` (or put a `static_assert`-style unreachable in it) so the compiler tells you.
- **Pimpl with inline destructor** → `unique_ptr<Impl>` incomplete-type error. Define `~Solver()` in the `.cpp`.
- **`std::filesystem` paths** are `char` on POSIX and `wchar_t` on Windows — use `.string()` / `.u8string()` when converting; don't assume `.c_str()` is `const char*` portably.
- **C++20 `std::format` with a runtime format string** needs `std::vformat`; a non-constant format string in `std::format` is a compile error.

---

## Common mistakes checklist

- [ ] `const auto&` / `auto&` in range-for and structured bindings unless you want a copy.
- [ ] `std::string_view` parameters never stored beyond the call; no views of temporaries.
- [ ] Closed sets of alternatives use `std::variant`, not inheritance; visitors are exhaustive.
- [ ] Rule of Zero: no hand-written special members in classes made of RAII members.
- [ ] `enum class` everywhere; `switch` without `default` so missing cases warn.
- [ ] `constexpr` for constants; no `#define` for values.
- [ ] `explicit` on every single-argument constructor.
- [ ] Every data member initialized at its declaration or in every constructor.
- [ ] Strong types (`Rows`, `Cols`, `Seconds`) for same-typed adjacent parameters.
- [ ] Templates for per-element callbacks; `std::function` only for stored/boundary callbacks.
- [ ] No `new`/`delete`, no C-style casts, no `NULL`, no `using namespace std;` in headers, no `std::endl`.
- [ ] One level of inheritance at most; composition and `variant` first.
- [ ] Looked up the Core Guidelines rule before inventing a convention.

---

## You can move on when...

- You can write an autograd `Op` as a `std::variant` with a forward-pass visitor and explain why it beats a virtual hierarchy here.
- You can split a `std::string_view` into tokens without allocating, and state the one rule that keeps `string_view` safe.
- You can turn `Matrix(std::size_t, std::size_t)` into a strong-typed constructor and show the compile error it produces for swapped arguments.
- You can state the Rule of Zero and say which one class in your project needs the Rule of Five instead.
- You can name three C++20 features Apple clang 21 supports today and the flag that enables them.
- You can look at a `template <class F> void integrate(F f, ...)` and a `std::function<double(double)>` member and say which belongs where, and why.
- You can list ten things in the "avoid" table from memory and the replacement for each.
