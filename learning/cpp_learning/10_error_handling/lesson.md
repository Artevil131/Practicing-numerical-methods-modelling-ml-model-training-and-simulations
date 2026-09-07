# Chapter 10 — Error Handling

## What you'll be able to do after this chapter

- Choose between return codes, `std::optional`, `bool` + out-parameter, a result type, and exceptions — and justify the choice.
- Throw, catch, and rethrow exceptions correctly (`catch (const T&)`), and use the standard exception hierarchy instead of inventing your own.
- Explain why RAII + stack unwinding gives you automatic cleanup, and contrast it with C's `goto cleanup` pattern.
- State the three exception safety guarantees and mark functions `noexcept` when they deserve it.
- Separate programmer bugs (`assert`) from runtime conditions (exceptions / results), and apply a concrete error-handling strategy to a numerics library.

## Why this matters for ML / numerics / sims

Your `Matrix` class from `../06_operator_overloading/lesson.md` will eventually be multiplied with the wrong shapes. In NumPy that gives `ValueError: shapes (2,3) and (2,3) not aligned`. In C you would return `-1` and hope the caller checks. In C++ you have several tools, and picking the wrong one either makes your inner loops slow (exceptions in a hot loop) or makes your API silently produce garbage (unchecked return codes). A training loop that runs for six hours and then crashes with `terminating with uncaught exception of type std::invalid_argument: shape mismatch: (784,128) @ (784,128)` is annoying; one that silently reads past the end of a buffer and produces NaN losses is far worse.

This chapter is about knowing which tool to reach for at each layer of a program: inner loops, library API boundaries, file/IO code, and `main`.

---

## 1. The options, side by side

| Technique | Signals failure by | Caller can ignore it? | Cost when no error | Best for |
|---|---|---|---|---|
| Return code (C style) | `int` / `enum` return | Yes (bad) | Zero | C interop, `errno`-style APIs |
| `bool` + out-param | `bool f(In, Out&)` | Yes | Zero | Parsing, "try" functions |
| `std::optional<T>` | Empty optional | No — must unwrap | ~Zero | "no value" cases (`find`, parse) |
| `Expected<T,E>` result type | Value *or* error object | No — must unwrap | ~Zero | Rich errors without exceptions |
| Exceptions | `throw` | No — propagates | Zero if not thrown; µs when thrown | Rare failures, constructors, deep call stacks |
| `assert` | Aborts | N/A | Zero in release (`-DNDEBUG`) | Programmer bugs (contract violations) |

Python equivalent: Python only has exceptions (plus `None` returns as an informal optional). C++ gives you the whole menu because the language is used for things Python isn't (drivers, games, HFT) where exceptions are unacceptable.

---

## 2. Return codes (C style)

You know this from `../../c_learning/`. The C standard library returns `-1`/`NULL` and sets `errno`. It still exists in C++ because you will call C libraries (BLAS, POSIX, SDL).

```cpp
#include <cstdio>
#include <cerrno>
#include <cstring>

int read_header(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return -errno;        // negative errno: caller distinguishes ENOENT vs EACCES
    // ...
    std::fclose(f);
    return 0;
}

// int rc = read_header("nope.bin");
// if (rc < 0) std::printf("error: %s\n", std::strerror(-rc));
// Output: error: No such file or directory
```

Problems: the caller can forget to check; the return slot is taken so the real result has to go through an out-parameter; the error value collides with legal results (`-1` as an index?).

---

## 3. `bool` + out-parameter

Slightly better than a bare code: the return is a clean yes/no, the result goes out the side.

```cpp
#include <string>
#include <charconv>   // std::from_chars, C++17

bool parse_double(const std::string& s, double& out) {
    const char* b = s.data();
    const char* e = b + s.size();
    auto [ptr, ec] = std::from_chars(b, e, out);
    return ec == std::errc{} && ptr == e;
}

// double v;
// if (parse_double("3.25", v)) ... // v == 3.25
// if (!parse_double("3.2x", v)) ... // false, v unspecified
```

This is the `TryParse` pattern. It's fine for small helpers, but `out` must be default-constructible and the reader has to notice the `&`. (Note: `std::from_chars` for floating point exists in Apple clang 21's libc++; if you hit a linker error on an older toolchain, use `std::strtod`.)

---

## 4. `std::optional<T>` (C++17)

For "there might not be a value, and that's not an error" — `dict.get(k)` returning `None`.

```cpp
#include <optional>
#include <vector>
#include <iostream>

std::optional<std::size_t> find_index(const std::vector<int>& v, int target) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i] == target) return i;      // implicit optional<size_t>(i)
    return std::nullopt;                   // "no value"
}

int main() {
    std::vector<int> v{4, 8, 15};
    if (auto idx = find_index(v, 15)) {    // optional converts to bool
        std::cout << "found at " << *idx << '\n';
    }
    std::cout << find_index(v, 99).value_or(999) << '\n';
    // find_index(v, 99).value()  -> throws std::bad_optional_access
}
// Output:
// found at 2
// 999
```

Key API: `has_value()` / `operator bool`, `*opt` / `opt->` (UB if empty!), `.value()` (throws if empty), `.value_or(default)`, `std::nullopt`, `std::make_optional`.

`std::optional` carries **no reason** for the failure. If the caller needs to know *why*, it's the wrong tool.

Python equivalent: `Optional[int]` / returning `None`. But `*opt` on an empty optional is undefined behavior, not an `AttributeError`.

---

## 5. A hand-rolled `Expected<T, E>` result type

C++23 has `std::expected<T, E>`. On Apple clang 21 with `-std=c++17` you don't have it, but a minimal version is 30 lines and teaches the shape of the idea: a value **or** an error, and the caller has to look.

```cpp
#include <variant>
#include <string>

template <typename T, typename E>
class Expected {
    std::variant<T, E> data_;
public:
    Expected(T v) : data_(std::move(v)) {}
    static Expected failure(E e) { Expected r; r.data_ = std::move(e); return r; }

    bool ok() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }
    T&       value()       { return std::get<T>(data_); }   // throws bad_variant_access if error
    const T& value() const { return std::get<T>(data_); }
    const E& error() const { return std::get<E>(data_); }
private:
    Expected() : data_(E{}) {}
};

Expected<double, std::string> safe_div(double a, double b) {
    if (b == 0.0) return Expected<double, std::string>::failure("division by zero");
    return a / b;
}
// auto r = safe_div(1, 0);
// if (!r) std::cout << r.error();   // division by zero
```

`std::variant` is covered in `../13_modern_cpp_and_idioms/lesson.md`; here it is just "a type-safe union holding either a `T` or an `E`". Rust programmers will recognize `Result<T, E>`. This is the approach many `-fno-exceptions` codebases use (LLVM has `llvm::Expected`, Abseil has `absl::StatusOr`).

---

## 6. Exceptions: `throw`, `try`, `catch`

An exception is an object of any type that is *thrown* from one place and *caught* somewhere up the call stack. Everything between the throw and the catch is abandoned — but destructors run (section 9).

```cpp
#include <stdexcept>
#include <iostream>

double sqrt_checked(double x) {
    if (x < 0) throw std::invalid_argument("sqrt of negative: " + std::to_string(x));
    return std::sqrt(x);
}

int main() {
    try {
        std::cout << sqrt_checked(4.0) << '\n';
        std::cout << sqrt_checked(-1.0) << '\n';   // throws; next line never runs
        std::cout << "unreachable\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "caught: " << e.what() << '\n';
    }
}
// Output:
// 2
// caught: sqrt of negative: -1.000000
```

Rules:
- **Catch by `const&`.** `catch (std::exception e)` copies and *slices* a derived exception down to the base (see `../09_inheritance_and_polymorphism/lesson.md`), losing the derived `what()`. `catch (const std::exception& e)` is the idiom.
- Handlers are tried top to bottom; the first match wins. Put derived classes before bases or the base will swallow everything.
- `throw;` (nothing after it) inside a handler rethrows the current exception unchanged (section 14).

Python equivalent: `raise ValueError(...)` / `try: ... except ValueError as e:`. The mechanics are nearly identical; the difference is what happens to locals in between (RAII, not garbage collection).

---

## 7. The standard exception hierarchy

Don't invent exception types until you've used these:

```
std::exception                 what() -> const char*
├── std::logic_error           bug detectable before running
│   ├── std::invalid_argument  bad argument (shape mismatch!)
│   ├── std::domain_error      math domain (sqrt(-1))
│   ├── std::length_error      container too big
│   └── std::out_of_range      vector::at(), map::at(), stoi
├── std::runtime_error         only detectable at run time
│   ├── std::range_error       result out of representable range
│   ├── std::overflow_error
│   ├── std::underflow_error
│   └── std::system_error      OS errors, carries an error_code (errno)
├── std::bad_alloc             new / vector growth failed
├── std::bad_cast              dynamic_cast<T&> failed
├── std::bad_optional_access   optional::value() on empty
├── std::bad_variant_access    std::get on wrong alternative
└── std::bad_function_call     empty std::function invoked
```

All of `<stdexcept>`'s types take a `std::string` (or `const char*`) message and return it from `what()`.

```cpp
try {
    std::vector<int> v(3);
    v.at(10) = 1;
} catch (const std::out_of_range& e) {
    std::cout << e.what() << '\n';
}
// Output (libc++): vector
// (libstdc++ gives a longer message. Do not parse what() — it's for humans.)
```

`std::bad_alloc` is the one you almost never catch usefully: if a 10 GB allocation fails, what would you do? Let it propagate to `main` and report.

---

## 8. Custom exception classes

Derive from the closest standard type so generic handlers (`catch (const std::exception&)`) still work. Add fields for structured information.

```cpp
#include <stdexcept>
#include <string>

struct Shape { std::size_t rows, cols; };

class ShapeError : public std::invalid_argument {
    Shape a_, b_;
public:
    ShapeError(Shape a, Shape b, const char* op)
        : std::invalid_argument("shape mismatch: (" + std::to_string(a.rows) + "," +
                                std::to_string(a.cols) + ") " + op + " (" +
                                std::to_string(b.rows) + "," + std::to_string(b.cols) + ")"),
          a_(a), b_(b) {}
    Shape lhs() const noexcept { return a_; }
    Shape rhs() const noexcept { return b_; }
};

// throw ShapeError({2,3}, {2,3}, "@");
// what(): shape mismatch: (2,3) @ (2,3)
```

Guidelines: derive publicly from `std::exception` (via a stdexcept class); keep the class cheap to copy (exceptions may be copied during unwinding); build the `what()` string in the constructor; don't throw from `what()` — it's `noexcept`.

Python equivalent: `class ShapeError(ValueError): ...`.

---

## 9. Stack unwinding + RAII = automatic cleanup

This is the single biggest reason RAII (`../02_classes_and_raii/lesson.md`) exists. When an exception propagates out of a scope, every fully-constructed local object in that scope has its destructor run, in reverse construction order. Files close, mutexes unlock, `std::vector`s free, `unique_ptr`s delete.

Compare with C:

```c
/* C: manual cleanup with goto */
int process(const char* path) {
    int rc = -1;
    FILE* f = fopen(path, "rb");
    if (!f) goto done;
    double* buf = malloc(N * sizeof *buf);
    if (!buf) goto close_file;
    if (read_into(f, buf) != 0) goto free_buf;
    rc = 0;
free_buf:   free(buf);
close_file: fclose(f);
done:       return rc;
}
```

```cpp
// C++: the same, with RAII types
int process(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<double> buf(N);            // freed automatically no matter how we leave
    read_into(f, buf);                     // may throw — f closes, buf frees
    return 0;
}
```

A demonstration of ordering:

```cpp
struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) { std::cout << "ctor " << name << '\n'; }
    ~Tracer() { std::cout << "dtor " << name << '\n'; }
};

void inner() { Tracer c("c"); throw std::runtime_error("boom"); }
void outer() { Tracer b("b"); inner(); }

int main() {
    try { Tracer a("a"); outer(); }
    catch (const std::exception& e) { std::cout << "caught " << e.what() << '\n'; }
}
// Output:
// ctor a
// ctor b
// ctor c
// dtor c
// dtor b
// dtor a
// caught boom
```

Consequences:
- **Never throw from a destructor.** If a destructor throws *during* unwinding (a second exception in flight), `std::terminate` is called. Destructors are implicitly `noexcept`.
- Raw `new` without a smart pointer leaks on throw. This is why "prefer `unique_ptr`" is not a style preference — it is exception safety.
- Constructors *should* throw when they cannot establish their invariants. There is no other way for a constructor to report failure (no return value). An object whose constructor threw was never constructed, so its destructor does not run — but already-constructed members' destructors do.

---

## 10. Exception safety guarantees

When a function throws, what state is the program in? Every function you write sits at one of these levels; document it.

| Guarantee | Meaning | Example |
|---|---|---|
| **Nothrow** (`noexcept`) | Never throws. | `std::vector::size()`, `swap`, destructors, move ctors |
| **Strong** | If it throws, state is exactly as before (transactional). | `std::vector::push_back` (either appended or unchanged) |
| **Basic** | If it throws, invariants hold and nothing leaks, but state may have changed. | An in-place `Matrix::operator+=` that throws halfway on a bad shape — but you check shape first, so it doesn't |
| **None** | Anything goes. | Code with raw `new` and no RAII. Don't write this. |

The **copy-and-swap** idiom gives the strong guarantee for free: build the new state in a temporary (which may throw), then `swap` it in (`noexcept`).

```cpp
Matrix& Matrix::operator=(const Matrix& other) {
    Matrix tmp(other);      // may throw (allocation) — *this untouched
    swap(*this, tmp);       // noexcept
    return *this;           // tmp destroys old data
}
```

---

## 11. `noexcept`

`noexcept` is a promise that a function will not throw. If it does anyway, `std::terminate` is called — no unwinding, no handler.

```cpp
double dot(const double* a, const double* b, std::size_t n) noexcept {
    double s = 0;
    for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}

static_assert(noexcept(dot(nullptr, nullptr, 0)));   // noexcept operator: compile-time query
```

Why it matters:
- **Move constructors and `swap` must be `noexcept`.** `std::vector` checks `std::is_nothrow_move_constructible<T>` when it grows; if your move ctor is not `noexcept`, vector *copies* every element instead (to preserve the strong guarantee). This is a real, measurable slowdown in a `std::vector<Matrix>`. See `../07_move_semantics_and_smart_pointers/lesson.md`.
- Destructors are `noexcept` by default.
- It's documentation: "this is safe to call during cleanup."

Don't slap `noexcept` on everything. Put it on functions that genuinely cannot throw (no allocation, no `.at()`, no user callbacks). Conditional forms exist: `noexcept(noexcept(expr))`.

---

## 12. When NOT to use exceptions

Exceptions are for **exceptional** paths. Concretely, avoid them when:

1. **The failure is expected and frequent.** "Key not found" in a lookup, "end of file", "user typed a non-number" — return `optional`/`bool`. A parser that throws on every malformed token in a 1 GB file will spend all its time in the unwinder.
2. **Inside hot loops.** Not because a *non-thrown* exception costs anything (it doesn't — section 13), but because `try` blocks and potentially-throwing calls constrain the optimizer, and because a throw in there means your error strategy is wrong: check shapes *once* before the loop, not per element.
3. **Real-time / embedded / GPU kernels.** The unwinder's timing is unbounded and needs RTTI + unwind tables; many such platforms compile with `-fno-exceptions`.
4. **Across a C ABI boundary.** An exception escaping through a C callback (e.g. from a function you passed to `qsort` or a C library) is UB. Catch everything at the boundary and convert to a code.

---

## 13. The cost of exceptions

Modern implementations (Itanium ABI, used by clang/gcc on macOS and Linux) are **zero-cost when not thrown**: the `try` block itself compiles to nothing; instead, the compiler emits side tables that map each instruction range to its cleanup actions. The "cost" moves entirely to the throw:

| Path | Cost |
|---|---|
| Enter `try`, no throw | 0 instructions |
| Call a function that *could* throw | Slight optimizer constraints; unwind table entries (binary size) |
| `throw` + unwind through N frames + catch | ~1–2 µs plus ~100s of ns per frame; involves `__cxa_allocate_exception`, RTTI comparisons, table lookups |

Rule of thumb: a thrown exception costs roughly 10,000–100,000× a normal function return. Throwing once per training run is free; throwing once per token in a tokenizer is a disaster.

Binary size: unwind tables add ~10–15% to code size. That's the `-fno-exceptions` motivation on constrained targets.

---

## 14. `catch(...)`, rethrow, and `std::nested_exception`

```cpp
void load_weights(const std::string& path) {
    try {
        parse(path);
    } catch (const std::exception& e) {
        std::cerr << "while loading " << path << ": " << e.what() << '\n';
        throw;                         // rethrow the SAME object (keeps dynamic type)
    } catch (...) {                    // anything else (int, char*, custom non-std types)
        std::cerr << "unknown exception while loading " << path << '\n';
        throw;
    }
}
```

- `throw;` rethrows the current exception — use it rather than `throw e;`, which would copy `e` as its *static* type (slicing again).
- `catch (...)` catches everything. Use it at boundaries (thread entry, C callbacks, `main`) — never to silently swallow.
- `std::throw_with_nested(std::runtime_error("loading failed"))` wraps the current exception in a new one; `std::rethrow_if_nested(e)` unpacks it. This gives Python-style "The above exception was the direct cause of..." chains. Rarely needed; know it exists (`<exception>`).

---

## 15. `std::terminate` and uncaught exceptions

If an exception reaches the top of `main` (or a thread's entry function) without a handler, `std::terminate()` is called, which calls `std::abort()`. Output on macOS:

```
libc++abi: terminating due to uncaught exception of type std::invalid_argument: shape mismatch: (2,3) @ (2,3)
zsh: abort      ./ex_demo
```

Whether destructors run before `terminate` is implementation-defined — on libc++ they generally **do not**. So files may not flush. Wrap `main`:

```cpp
int main() try {
    run();
    return 0;
} catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "fatal: unknown exception\n";
    return 2;
}
```

(That is a *function-try-block* — legal on any function, mostly used on `main` and constructors.)

Other paths to `terminate`: throwing from a `noexcept` function, throwing from a destructor during unwinding, a `std::thread` destroyed while joinable.

---

## 16. `assert` vs exceptions vs `std::terminate`

| Tool | Answers the question | Compiled out in release? | Recoverable? |
|---|---|---|---|
| `assert(cond)` | "Is my code correct?" (contract, invariant) | Yes, with `-DNDEBUG` | No — aborts |
| Exception | "Did the environment/input cooperate?" | No | Yes |
| `std::terminate` / `std::abort` | "Is continuing worse than dying?" | No | No |

```cpp
#include <cassert>

double& Matrix::operator()(std::size_t i, std::size_t j) {
    assert(i < rows_ && j < cols_ && "Matrix index out of range");  // bug in *caller's* code
    return data_[i * cols_ + j];
}

Matrix matmul(const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows())                                     // caller passed bad shapes
        throw ShapeError({a.rows(), a.cols()}, {b.rows(), b.cols()}, "@");
    // ...
}
```

Both examples check arguments. The difference is *who is wrong*: an out-of-range index in an inner loop means the algorithm is buggy (assert; you want a core dump at the exact line). A shape mismatch at an API entry point means the user of the library composed things wrong (exception; they can catch and report). `assert` costs nothing in `-O2 -DNDEBUG` builds, so it is the right tool inside hot loops.

The `&& "message"` trick: a string literal is truthy, so `cond && "msg"` is still `cond`, but the message appears in the assertion failure output.

---

## 17. `.at()` vs `[]`

`std::vector::operator[]` does **not** check bounds — out of range is UB (silent memory corruption or a crash somewhere else). `.at()` checks and throws `std::out_of_range`.

| | `v[i]` | `v.at(i)` |
|---|---|---|
| Bounds check | No (UB if out of range) | Yes (throws) |
| Cost | Zero | A compare + branch per access |
| Use in | Inner loops after validating bounds once | Parsing user indices, tests, debugging |

Libc++ offers `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG` (older: `-D_LIBCPP_DEBUG=1`) which makes `[]` trap on out-of-range — good for a debug build. `-fsanitize=address` catches it too.

Design your own `Matrix` the same way: `operator()(i,j)` unchecked (with `assert`), `.at(i,j)` checked.

---

## 18. `std::system_error` and `errno`

When wrapping OS or C calls, convert `errno` into an exception that carries the code:

```cpp
#include <system_error>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

int open_or_throw(const char* path) {
    int fd = ::open(path, O_RDONLY);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), std::string("open ") + path);
    return fd;
}
// what(): "open /nope: No such file or directory"
// e.code().value() == ENOENT
// e.code() == std::errc::no_such_file_or_directory  (portable comparison)
```

`std::filesystem` functions (`../13_modern_cpp_and_idioms/lesson.md`) throw `std::filesystem::filesystem_error`, a subclass of `system_error`; they also offer overloads taking `std::error_code&` for a non-throwing path.

---

## 19. `[[nodiscard]]`

The fundamental weakness of return codes/optionals is that callers can drop them on the floor. `[[nodiscard]]` (C++17) makes the compiler warn when they do.

```cpp
[[nodiscard]] bool parse_double(const std::string& s, double& out);
[[nodiscard]] std::optional<std::size_t> find_index(const std::vector<int>&, int);

class [[nodiscard]] Expected;     // can be put on the type itself: any function returning it is nodiscard

// parse_double("1", x);          // warning: ignoring return value of function declared with 'nodiscard'
```

Under `-Wall` this is a warning; add `-Werror=unused-result` to make it an error. Put it on every function whose return value is the *only* way to learn about failure. Apply it to `Matrix::transpose()` too — `m.transpose();` (ignoring the returned copy) is a classic silent bug.

---

## 20. `-fno-exceptions` codebases

Many game engines (Unreal, id Tech), Google's C++ style guide, LLVM, and much HPC code compile with `-fno-exceptions`. With that flag, `throw` is a compile error and the standard library calls `std::abort()` where it would throw (`vector::at` out of range → abort). Those codebases use result types (`absl::StatusOr`, `llvm::Expected`), error codes, and `assert`.

You do not need to follow them for personal projects — exceptions at API boundaries plus results/optionals for expected failures is the mainstream approach — but know that when you read Eigen or PyTorch source, you'll see `TORCH_CHECK(cond, msg)` macros that *do* throw `c10::Error` (PyTorch uses exceptions), whereas Eigen uses `eigen_assert` (asserts, off in release).

---

## 21. Error handling strategy for your projects

Apply this layered rule to the matrix library, autograd, ODE solvers, and simulations:

```
main()                     catch (const std::exception&) → print what(), return 1
  │
API boundary (public       validate arguments ONCE; throw std::invalid_argument /
 Matrix::matmul, Solver    std::out_of_range / custom ShapeError with a HELPFUL message
 ::step, Tokenizer::load)  ("shape mismatch: (2,3) @ (2,3)", not "bad input")
  │
Expected failures          std::optional (lookup miss), bool (parse), Expected<T,E>
 (find token, parse line)  never exceptions — these happen millions of times
  │
Inner loops                NO checks or assert() only; indexes were validated above.
 (matmul kernel, RK4 step) noexcept where true so vector<T> can move.
  │
Destructors, swap, move    noexcept. Never throw.
```

Concrete rules:
1. Every public function that takes shapes/sizes validates them at the top and throws with a message that includes the actual numbers.
2. Provide both `operator()(i,j)` (assert) and `at(i,j)` (throw).
3. Lookups return `std::optional`; parsing returns `bool`+out or `Expected`.
4. Move constructors, `swap`, destructors: `noexcept`.
5. `[[nodiscard]]` on anything returning a status, an optional, or a new value the caller might think was in-place.
6. `main` has a function-try-block.
7. Compile debug builds *without* `-DNDEBUG` so asserts fire; compile benchmarks with `-DNDEBUG`.

---

## Gotchas and undefined behavior

- **Catching by value slices.** `catch (std::exception e)` loses the derived `what()`. Always `const&`.
- **`throw e;` in a handler copies as the static type.** Use bare `throw;` to rethrow.
- **Throwing from a destructor** during unwinding → `std::terminate`. Destructors are `noexcept` by default; a `throw` inside one terminates even when not unwinding.
- **Throwing through `noexcept`** → `std::terminate`, no unwinding.
- **`*opt` on an empty `std::optional`** is UB. `.value()` throws instead.
- **`std::get<T>` on the wrong variant alternative** throws `std::bad_variant_access` — not UB, but easy to hit in a hand-rolled `Expected`.
- **Exception escaping a C callback or `extern "C"` function** is UB.
- **Raw `new` + throw = leak.** RAII or don't `new`.
- **`assert` with side effects** (`assert(f() == 0)`) — the call disappears under `-DNDEBUG`.
- **A non-`noexcept` move constructor** makes `std::vector<T>` copy on reallocation. Silent 2× slowdown.
- **Constructor throws → destructor does not run** for that object, but fully-constructed members and bases are destroyed. Half-initialized raw resources leak; use members that are RAII types.
- **Handler order.** `catch (const std::exception&)` before `catch (const std::out_of_range&)` makes the second unreachable (clang warns).

---

## Common mistakes checklist

- [ ] Every `catch` is `const T&`.
- [ ] Rethrow with `throw;`, not `throw e;`.
- [ ] Derived-class handlers appear before base-class handlers.
- [ ] Custom exceptions derive from a `<stdexcept>` class and build `what()` in the constructor.
- [ ] Shape/size validation happens once at the API boundary, not per element.
- [ ] No exceptions used for "not found" or "parse failed" in bulk-data paths.
- [ ] Move ctor / move assignment / `swap` / destructor are `noexcept`.
- [ ] No `throw` in destructors.
- [ ] `[[nodiscard]]` on status-returning and value-returning "pure" functions.
- [ ] `main` catches `std::exception` and returns non-zero.
- [ ] Debug builds keep `assert` enabled; benchmarks use `-DNDEBUG`.
- [ ] No raw owning pointers in code that can throw.

---

## You can move on when...

- You can list five ways to signal failure and pick the right one for: `Matrix::matmul` shape mismatch, `Tokenizer::lookup(token)`, `parse_csv_row`, `Vector::operator[]`, `std::vector<Matrix>` reallocation.
- You can predict the exact destructor output order for a nested throw (section 9) without running it.
- You can explain why a non-`noexcept` move constructor makes `std::vector` slower.
- You can write a `ShapeError` deriving from `std::invalid_argument` whose `what()` reads `shape mismatch: (2,3) @ (2,3)`.
- You can state the difference between basic and strong exception safety and implement copy-and-swap.
- You know why `assert` and exceptions are not interchangeable, and which one belongs in a matmul inner loop.
