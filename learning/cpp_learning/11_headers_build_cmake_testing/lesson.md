# Chapter 11 — Headers, the Build Model, CMake, and Testing

## What you'll be able to do after this chapter

- Explain what a translation unit is, what the One Definition Rule forbids, and why templates and `inline` functions must live in headers.
- Lay out a library as `include/mylib/*.hpp` + `src/*.cpp`, use `#pragma once`, and cut compile times with forward declarations.
- Write a `CMakeLists.txt` from scratch for a library + executable + tests, build out-of-source in Debug/Release, and turn on sanitizers with an `option()`.
- Write unit tests: first with a hand-rolled `CHECK` macro, then with doctest, run through `ctest`.
- Test numerical code properly: tolerances, gradient checks, and algebraic property tests.
- Read a compile-flag line (`-O3 -march=native -ffast-math -fsanitize=address`) and know what each flag risks.

## Why this matters for ML / numerics / sims

Your matrix library, autograd engine, and N-body sim will each be several files. Once that happens, a single `c++ file.cpp` no longer cuts it: you need to build a library once and link it into a demo, a benchmark, and a test binary. Every serious C++ project — PyTorch, Eigen, LAMMPS, GROMACS — uses CMake, so learning it is not optional if you want to read or contribute to them. And numerical code is *exactly* the code that needs tests: a sign error in a gradient produces a model that trains a little worse, not a crash. A gradient check catches that in 5 lines.

`../../c_learning/09_multi_file_projects_and_make/lesson.md` covered `make` and multi-file C. This chapter is the C++ continuation: the same compilation model plus templates, plus the tool everyone actually uses.

---

## 1. The C++ build model recap

```
matrix.cpp ──preprocess──> matrix.i ──compile──> matrix.o ─┐
demo.cpp   ──preprocess──> demo.i   ──compile──> demo.o   ─┼─link──> demo
                                                  libm etc ─┘
```

- A **translation unit (TU)** is one `.cpp` file *after* preprocessing: all its `#include`s pasted in. Each TU is compiled independently into an object file. The compiler has no idea other TUs exist.
- The **linker** joins object files, resolving every *declared-but-not-defined* symbol to exactly one definition.
- A **declaration** says a name exists (`Matrix matmul(const Matrix&, const Matrix&);`). A **definition** provides the body/storage. You may declare as often as you like; you may define most things exactly once across the whole program.

Python equivalent: there is none. Python imports at runtime and every module is its own namespace. C++ pastes text and then has one flat symbol table at link time.

---

## 2. The One Definition Rule (ODR)

| Entity | Definitions allowed per program | Where it goes |
|---|---|---|
| Non-inline function | Exactly one | `.cpp` |
| Global variable | Exactly one | `.cpp` (declare `extern` in header) |
| Class definition | One *per TU*, all identical | header |
| `inline` function / variable | One per TU, all identical | header |
| Template | One per TU, all identical | header |
| `constexpr` function | Implicitly inline | header |

Violations:
- **Defining a normal function in a header** and including it from two `.cpp` files → linker error: `duplicate symbol matmul(...) in demo.o and test.o`.
- **Two different definitions** of an `inline` function/class with the same name in two TUs → *no diagnostic required*, UB. The linker picks one silently. This happens when two TUs include different versions of a header or define different classes named `Node` in the global namespace. Fix: namespaces, anonymous namespaces for TU-local helpers.

```cpp
// matrix.hpp
#pragma once
#include <vector>

namespace ml {

class Matrix {                          // class definition: fine in a header
public:
    Matrix(std::size_t r, std::size_t c);
    std::size_t rows() const noexcept { return rows_; }   // defined in-class: implicitly inline
    double& operator()(std::size_t i, std::size_t j) noexcept { return data_[i * cols_ + j]; }
private:
    std::size_t rows_, cols_;
    std::vector<double> data_;
};

Matrix matmul(const Matrix& a, const Matrix& b);        // declaration only

inline double sq(double x) { return x * x; }             // inline: definition allowed in header

template <typename T>
T clamp01(T x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }  // template: must be in header

}  // namespace ml
```

```cpp
// matrix.cpp
#include "mylib/matrix.hpp"
namespace ml {
Matrix::Matrix(std::size_t r, std::size_t c) : rows_(r), cols_(c), data_(r * c) {}
Matrix matmul(const Matrix& a, const Matrix& b) { /* ... */ }
}
```

---

## 3. Why templates and `inline` live in headers

A template is a recipe, not code. `clamp01<double>` only exists once some TU *instantiates* it — and the compiler can only instantiate what it can see in that TU. If the template body is in `clamp.cpp`, then `demo.cpp` sees only the declaration and the linker later complains: `undefined symbol: ml::clamp01<double>(double)`.

`inline` (the keyword) means "multiple identical definitions are allowed; the linker merges them" — it is **not** primarily an optimization hint anymore. Member functions defined inside a class body are implicitly inline. This is why header-only libraries (doctest, Eigen, GLM, `nlohmann::json`) work at all.

The cost: every TU that includes the header compiles those templates again. Compile times grow. Section 7 is about that.

---

## 4. Header organization

The conventional layout, mirroring what you'll see in every well-run project:

```
matrixlib/
├── CMakeLists.txt
├── include/
│   └── mylib/
│       ├── matrix.hpp        public API: class defs, inline/template code, declarations
│       └── ops.hpp
├── src/
│   ├── matrix.cpp            non-inline definitions
│   └── ops.cpp
├── apps/
│   └── demo.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_matrix.cpp
└── build/                    generated; never committed (.gitignore it)
```

- The extra `mylib/` directory under `include/` means users write `#include <mylib/matrix.hpp>` — no collisions with someone else's `matrix.hpp`.
- `.hpp` for C++ headers (or `.h`; be consistent). `.cpp` for sources.
- Each header **includes what it uses** and nothing more. Never rely on `matrix.hpp` having already pulled in `<vector>` for you.
- Public headers should compile standalone: `c++ -std=c++17 -fsyntax-only -Iinclude include/mylib/matrix.hpp` is a good CI check.

---

## 5. `#pragma once` vs include guards

Both prevent a header from being pasted twice into one TU (which would redefine the class → error).

```cpp
// Classic (portable, verbose):
#ifndef MYLIB_MATRIX_HPP
#define MYLIB_MATRIX_HPP
// ...
#endif

// Modern (every compiler you'll meet supports it):
#pragma once
```

Use `#pragma once`. It cannot be misspelled, cannot collide, and is marginally faster. The C course used guards because C11 compilers on odd platforms vary; for C++ on clang/gcc/MSVC, `#pragma once` is universal.

---

## 6. Forward declarations

You need a *complete* type to create one, copy one, call its methods, or know its size. You need only a *declaration* to name a pointer or reference to it, or to declare a function taking/returning it by value.

```cpp
// solver.hpp
#pragma once
#include <memory>

namespace ml { class Matrix; }        // forward declaration: no #include "matrix.hpp"

namespace sim {
class Solver {
public:
    explicit Solver(const ml::Matrix& A);        // reference: OK with incomplete type
    void step(ml::Matrix& state) const;          // OK
private:
    std::unique_ptr<ml::Matrix> A_;              // unique_ptr<T> to incomplete T: OK here,
                                                 // as long as ~Solver is defined in solver.cpp
};
}
```

Every `.cpp` that includes `solver.hpp` now skips parsing `matrix.hpp` (and `<vector>`). In a 200-header project this is the difference between 30 s and 5 min rebuilds. It also breaks circular dependencies (`Node` needs `Graph*`, `Graph` needs `vector<Node>`).

Limits: you cannot forward-declare `std::string` or `std::vector` (they're templates with defaults; `<iosfwd>` exists precisely because `<iostream>` is huge). You cannot forward-declare a type and then use it as a by-value member.

---

## 7. `inline` variables and the compile-time problem

**`inline` variables (C++17)** finally let headers hold constants with a single shared definition:

```cpp
// constants.hpp
#pragma once
namespace phys {
inline constexpr double G     = 6.674e-11;      // one object program-wide
inline constexpr double c     = 299792458.0;
inline const std::string kName = "nbody";       // non-constexpr inline variable also allowed
}
```

Pre-17 you either got one copy per TU (`static const`) or needed `extern` + a `.cpp`. `constexpr` static data members are implicitly inline since C++17.

**Compile time.** `#include <iostream>` pulls in ~30,000 lines; `<regex>` and `<algorithm>` are worse. Every TU pays. Tools:

```sh
c++ -std=c++17 -ftime-trace -c src/matrix.cpp -o /dev/null   # writes matrix.json
# open matrix.json at https://ui.perfetto.dev  (or chrome://tracing) -> flame graph of what took time
```

Mitigations, in order of payoff: forward declarations; include `<iosfwd>`/`<cstdio>` instead of `<iostream>` in headers; keep templates you don't need out of headers; **precompiled headers** (`target_precompile_headers(mylib PRIVATE <vector> <string>)` in CMake — the compiler parses the standard headers once and reuses the result); unity builds; and eventually C++20 modules (not yet practical on Apple clang for everyday use — see `../13_modern_cpp_and_idioms/lesson.md`).

---

## 8. CMake from zero

CMake is a **build-system generator**: it reads `CMakeLists.txt` and writes Makefiles (or Ninja files, or an Xcode project). You never edit the generated files.

Install: `brew install cmake ninja`. Check: `cmake --version` (you want ≥ 3.20).

Minimal:

```cmake
cmake_minimum_required(VERSION 3.20)
project(matrixlib LANGUAGES CXX)

add_executable(demo apps/demo.cpp src/matrix.cpp)
target_compile_features(demo PRIVATE cxx_std_17)
target_include_directories(demo PRIVATE include)
target_compile_options(demo PRIVATE -Wall -Wextra)
```

```sh
cmake -B build                # configure: writes build/ (out-of-source; delete it any time)
cmake --build build           # compile
./build/demo
```

Commands you need to know:

| Command | Meaning |
|---|---|
| `cmake_minimum_required(VERSION 3.20)` | First line, always. Sets policy behaviour. |
| `project(name LANGUAGES CXX)` | Names the project; enables the C++ compiler. |
| `add_executable(tgt a.cpp b.cpp)` | A program target. |
| `add_library(tgt STATIC a.cpp)` | A library target (`STATIC`, `SHARED`, or `INTERFACE` for header-only). |
| `target_include_directories(tgt PUBLIC include)` | `-I` flags. `PUBLIC` propagates to consumers; `PRIVATE` doesn't. |
| `target_link_libraries(demo PRIVATE mylib)` | Link, and inherit `mylib`'s `PUBLIC` includes/flags. |
| `target_compile_features(tgt PUBLIC cxx_std_17)` | `-std=c++17`, propagated. Prefer over `set(CMAKE_CXX_STANDARD)`. |
| `target_compile_options(tgt PRIVATE -Wall -Wextra)` | Extra flags. |
| `target_compile_definitions(tgt PRIVATE NDEBUG)` | `-D` macros. |
| `add_subdirectory(tests)` | Process `tests/CMakeLists.txt`. |
| `option(NAME "doc" OFF)` | A user-settable boolean: `cmake -B build -DNAME=ON`. |
| `add_test(NAME t COMMAND tgt)` + `enable_testing()` | Register tests for `ctest`. |
| `install(TARGETS ...)` | What `cmake --install build` copies where. |

Everything is **target-based**: flags attach to targets, and `PUBLIC`/`PRIVATE`/`INTERFACE` say whether they propagate to whoever links you. Avoid the old global `include_directories()`/`CMAKE_CXX_FLAGS` style you'll see in 2012-era tutorials.

---

## 9. Build types

```sh
cmake -B build-debug   -DCMAKE_BUILD_TYPE=Debug
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

| `CMAKE_BUILD_TYPE` | Flags (clang/gcc) | Use |
|---|---|---|
| `Debug` | `-g` (no optimisation) | Stepping in a debugger; asserts active |
| `Release` | `-O3 -DNDEBUG` | Benchmarks, shipping; asserts removed |
| `RelWithDebInfo` | `-O2 -g -DNDEBUG` | Profiling: optimised code with symbols |
| `MinSizeRel` | `-Os -DNDEBUG` | Embedded |
| *(empty)* | nothing | The default. Slow and confusing — always set one. |

Because `Release` defines `NDEBUG`, your `assert`s vanish there. Keep one Debug build directory around for correctness and one Release for speed; with Ninja both configure in seconds.

---

## 10. A complete `CMakeLists.txt`: library + demo + tests

```cmake
cmake_minimum_required(VERSION 3.20)
project(matrixlib VERSION 0.1 LANGUAGES CXX)

# Sensible defaults ---------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)          # build/compile_commands.json for clangd

option(MATRIXLIB_BUILD_TESTS "Build unit tests" ON)
option(MATRIXLIB_SANITIZE    "Enable ASan+UBSan" OFF)

# Warnings as an INTERFACE library: link it to get the flags, no code involved --
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

if(MATRIXLIB_SANITIZE)
  add_library(project_sanitizers INTERFACE)
  target_compile_options(project_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(project_sanitizers INTERFACE -fsanitize=address,undefined)
else()
  add_library(project_sanitizers INTERFACE)     # empty, so targets can link it unconditionally
endif()

# The library -----------------------------------------------------------------
add_library(matrix STATIC src/matrix.cpp src/ops.cpp)
add_library(mylib::matrix ALIAS matrix)          # namespaced alias, like real packages
target_include_directories(matrix PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(matrix PUBLIC cxx_std_17)
target_link_libraries(matrix PRIVATE project_warnings project_sanitizers)

# The demo --------------------------------------------------------------------
add_executable(demo apps/demo.cpp)
target_link_libraries(demo PRIVATE mylib::matrix project_warnings project_sanitizers)

# Tests -----------------------------------------------------------------------
if(MATRIXLIB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Install (optional) ------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS matrix demo
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
```

```cmake
# tests/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11)
FetchContent_MakeAvailable(doctest)              # downloads at configure time, adds target doctest::doctest

add_executable(test_matrix test_matrix.cpp)
target_link_libraries(test_matrix PRIVATE mylib::matrix doctest::doctest project_warnings project_sanitizers)
add_test(NAME matrix_unit COMMAND test_matrix)
```

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMATRIXLIB_SANITIZE=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Notes:
- `FetchContent` pulls a dependency's source at configure time and builds it as part of your tree. Works for doctest, Catch2, fmt, nlohmann::json, Eigen. For big deps (OpenCV, PyTorch) use a package manager (vcpkg/conan) or `find_package`.
- `$<BUILD_INTERFACE:...>`/`$<INSTALL_INTERFACE:...>` are *generator expressions* — they resolve at generate time. You only need them if you `install()`.
- `include(GNUInstallDirs)` gives you `bin/`, `lib/`, `include/` variables that respect platform conventions. `cmake --install build --prefix ~/.local` puts things there.

---

## 11. Unit testing, step 1: hand-rolled `CHECK`

Before adopting a framework, understand what one *is*: a macro that records the expression text, file, and line, and a counter.

```cpp
#include <cstdio>
#include <cmath>

static int g_failed = 0, g_total = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        ++g_total;                                                                    \
        if (!(expr)) {                                                                \
            ++g_failed;                                                               \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
        }                                                                             \
    } while (0)

#define CHECK_CLOSE(a, b, tol) CHECK(std::fabs((a) - (b)) <= (tol))

int main() {
    CHECK(1 + 1 == 2);
    CHECK_CLOSE(std::sqrt(2.0) * std::sqrt(2.0), 2.0, 1e-12);
    CHECK(0.1 + 0.2 == 0.3);                        // fails: floating point
    std::printf("%d/%d checks passed\n", g_total - g_failed, g_total);
    return g_failed ? 1 : 0;                        // non-zero exit = ctest reports FAIL
}
// Output:
// test.cpp:19: CHECK failed: 0.1 + 0.2 == 0.3
// 2/3 checks passed
```

`#expr` stringifies the argument; `__FILE__`/`__LINE__` come from the preprocessor; the `do { } while (0)` makes the macro a single statement (see `../../c_learning/`). The exit code is the contract with `ctest`: 0 pass, anything else fail.

---

## 12. Unit testing, step 2: doctest

[doctest](https://github.com/doctest/doctest) is a single header, compiles fast, and has the same shape as your macro plus test registration, sections, and pretty failure output. Catch2 is the bigger sibling (same ideas, slower to compile). GoogleTest is what you'll see in Google-adjacent codebases.

```cpp
// tests/test_matrix.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN     // exactly ONE TU defines this; it provides main()
#include <doctest/doctest.h>
#include <mylib/matrix.hpp>

using ml::Matrix;

TEST_CASE("matmul shape") {
    Matrix a(2, 3), b(3, 4);
    Matrix c = ml::matmul(a, b);
    CHECK(c.rows() == 2);
    CHECK(c.cols() == 4);
}

TEST_CASE("matmul shape mismatch throws") {
    Matrix a(2, 3), b(2, 3);
    CHECK_THROWS_AS(ml::matmul(a, b), std::invalid_argument);
    CHECK_THROWS_WITH(ml::matmul(a, b), "shape mismatch: (2,3) @ (2,3)");
}

TEST_CASE("identity is neutral") {
    Matrix a = ml::random(3, 3, /*seed=*/42);
    Matrix I = ml::identity(3);
    Matrix ai = ml::matmul(a, I);
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            CHECK(ai(i, j) == doctest::Approx(a(i, j)).epsilon(1e-12));
    SUBCASE("left identity too") {
        Matrix ia = ml::matmul(I, a);
        CHECK(ia(1, 2) == doctest::Approx(a(1, 2)));
    }
}
```

```sh
./build/tests/test_matrix                       # run all
./build/tests/test_matrix -tc="matmul*"         # filter by name
./build/tests/test_matrix -s                    # show successful checks too
```

Output on failure:

```
tests/test_matrix.cpp:12: ERROR: CHECK( c.cols() == 4 ) is NOT correct!
  values: CHECK( 3 == 4 )
```

`doctest::Approx` handles floating-point comparison with relative epsilon. `SUBCASE` re-runs the enclosing test body once per subcase, giving each a fresh fixture without inheritance boilerplate.

Without CMake, for one file: `c++ -std=c++17 -I path/to/doctest test_matrix.cpp src/matrix.cpp -Iinclude -o tests`.

---

## 13. `ctest` and `add_test`

`enable_testing()` + `add_test(NAME x COMMAND target)` register executables with ctest. `ctest` runs them and inspects the exit codes.

```sh
ctest --test-dir build                          # run everything
ctest --test-dir build --output-on-failure      # print stdout of failing tests
ctest --test-dir build -R matmul -V             # regex filter, verbose
ctest --test-dir build -j 8                     # parallel
```

For finer-grained ctest entries per doctest case, `include(doctest)` (shipped in the doctest repo's `scripts/cmake/`) provides `doctest_discover_tests(test_matrix)`. Optional; the single-executable approach is fine for your projects.

---

## 14. Testing numerics

Floating-point code cannot be tested with `==`. Four techniques, in order of how often you'll use them:

**1. Tolerances.** Compare with a relative *and* absolute tolerance (absolute for values near zero):

```cpp
inline bool close(double a, double b, double rtol = 1e-9, double atol = 1e-12) {
    return std::fabs(a - b) <= atol + rtol * std::max(std::fabs(a), std::fabs(b));
}
// Python equivalent: np.isclose(a, b, rtol, atol) / torch.allclose
```

Pick `rtol` from the algorithm's error, not from what makes the test pass. A matmul of 1000-length dot products in double: roughly `1e-13`. An RK4 step with `h=0.01` vs exact: `h^4 ≈ 1e-8`.

**2. Gradient checks.** The single most valuable test for autograd. Compare the analytic gradient with a central finite difference:

```cpp
// f: R^n -> R,  grad: analytic gradient at x
for (std::size_t i = 0; i < n; ++i) {
    double old = x[i];
    x[i] = old + h; double fp = f(x);
    x[i] = old - h; double fm = f(x);
    x[i] = old;
    double numeric = (fp - fm) / (2 * h);           // O(h^2) error
    CHECK(close(grad[i], numeric, 1e-5, 1e-7));     // h = 1e-5 in double is the sweet spot
}
```

Too small `h` → roundoff dominates (`fp - fm` loses digits). Too large → truncation error. `1e-5`–`1e-6` for `double`; never do this in `float`.

**3. Property tests.** Algebraic identities that must hold for *any* input:

| Property | Test |
|---|---|
| Identity | `A @ I == A`, `I @ A == A` |
| Transpose | `(A @ B)^T == B^T @ A^T`, `(A^T)^T == A` |
| Associativity | `(A @ B) @ C ≈ A @ (B @ C)` (to tolerance) |
| Inverse | `A @ inv(A) ≈ I` for well-conditioned `A` |
| Softmax | rows sum to 1, all entries in `(0, 1)`, invariant to adding a constant |
| Symmetry | `dot(a,b) == dot(b,a)`, `norm(a) == sqrt(dot(a,a))` |
| Conservation (sims) | Total energy/momentum drift over N steps below a bound |
| Known solutions | RK4 on `y' = -y` vs `e^-t`; FFT of a pure sine vs one non-zero bin |

Generate inputs with a seeded `std::mt19937` so failures are reproducible.

**4. Regression / golden values.** Save a known-good output (loss after 10 steps, particle positions after 100 frames) and check you still produce it. Fragile but catches "I refactored and something changed".

---

## 15. Compile flags cheat sheet

| Flag | What it does | When |
|---|---|---|
| `-O0 -g` | No optimisation, debug symbols | Debugging with lldb; sanitizer runs |
| `-O2` | Standard optimisation | Default for everything you run |
| `-O3` | `-O2` + aggressive inlining and vectorisation | Benchmarks; usually 0–15% over `-O2` |
| `-march=native` | Use every instruction this CPU has (NEON/SVE, AVX2) | Benchmarks on this machine. Binary won't run on older CPUs. On Apple Silicon use `-mcpu=native` or `-mcpu=apple-m1`. |
| `-ffast-math` | Assume no NaN/inf, allow reassociation, flush denormals | See below — dangerous |
| `-DNDEBUG` | Remove `assert` | Release builds (CMake does this) |
| `-fsanitize=address` | Detect heap/stack overflow, use-after-free, leaks | Every debug run |
| `-fsanitize=undefined` | Detect signed overflow, misaligned access, null deref, bad shifts | Every debug run |
| `-fsanitize=thread` | Data races (cannot combine with address) | Multithreaded code |
| `-fno-omit-frame-pointer` | Better stack traces in sanitizers/profilers | With sanitizers |
| `-Wall -Wextra -Wpedantic` | Warnings | Always |
| `-Wshadow -Wconversion -Wsign-conversion` | Stricter warnings | Once the basics are clean |
| `-Werror` | Warnings are errors | CI |
| `-ftime-trace` | Compile-time profile JSON | When builds get slow |
| `-Rpass=loop-vectorize` | Report vectorised loops | Chapter 12 |
| `-fno-exceptions -fno-rtti` | Disable exceptions/RTTI | Embedded/game codebases (Chapter 10) |

**`-ffast-math` in detail.** It enables `-fno-math-errno`, `-funsafe-math-optimizations` (reassociation: `(a+b)+c → a+(b+c)`, which breaks Kahan summation *entirely*), `-ffinite-math-only` (the compiler may assume `x != x` is always false, so your `std::isnan(x)` check *gets deleted*), `-fno-signed-zeros`, `-fno-trapping-math`, and denormal flushing. It can give 2× on reductions because reassociation enables vectorising a sum. It also makes your NaN-detection code silently disappear and changes results run-to-run. Use it only on a specific benchmark TU after you've verified results against a non-fast-math build, or use the narrower `-fno-math-errno -fassociative-math` / `#pragma clang fp reassociate(on)` on the one loop that needs it.

---

## 16. `make` vs Ninja

Both are what CMake generates *for*. Ninja is a build executor designed to be generated, not written: no built-in rules, minimal parsing, good parallelism by default, tracks header dependencies precisely.

```sh
cmake -B build -G Ninja           # once
cmake --build build               # or: ninja -C build
cmake --build build -j 8          # explicit parallelism (Ninja defaults to cores+2)
cmake --build build --target demo # a single target
cmake --build build -v            # show the actual compiler command lines
```

Rule of thumb: always `-G Ninja` if it's installed. You can also generate an Xcode project (`-G Xcode`) for the Instruments profiler; see `../12_performance/lesson.md`.

---

## 17. `compile_commands.json`, clangd, and VS Code

`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` (or `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`) makes CMake write `build/compile_commands.json`: one entry per TU with the exact flags. **clangd** (the language server behind good C++ editor support) reads it to know your include paths and `-std`.

```sh
ln -sf build/compile_commands.json compile_commands.json     # clangd looks in the project root
```

VS Code setup:
1. Install the **CMake Tools** extension (configure/build/test/debug from the status bar, picks up `option()`s).
2. Install the **clangd** extension and *disable* Microsoft's C/C++ IntelliSense (they fight). clangd gives you go-to-definition, errors as you type, clang-tidy hints, and `-Wall` diagnostics inline.
3. Optional: **CodeLLDB** for debugging (`lldb` is what Xcode ships; `gdb` is not available on macOS without pain).

A `.clang-format` file (`BasedOnStyle: Google`, `ColumnLimit: 100`) and format-on-save removes formatting from your list of things to think about.

---

## Gotchas and undefined behavior

- **Defining a non-inline function in a header** → duplicate-symbol link error the moment a second `.cpp` includes it. Either move it to a `.cpp` or mark it `inline`.
- **Two different classes/inline functions with the same name in different TUs** (ODR violation) → UB, no diagnostic. Wrap TU-local helpers in an anonymous namespace: `namespace { struct Helper {...}; }`.
- **Template defined in a `.cpp`** → `undefined symbol` at link time. Move it to the header.
- **`static` in a header** gives every TU its *own* copy — for a function that's just wasted code; for a mutable variable it's a bug (each TU sees a different counter). Use `inline`.
- **Missing include guard/`#pragma once`** → `redefinition of 'Matrix'` compile error.
- **Relying on transitive includes** (`matrix.hpp` happens to include `<string>`) → your file breaks when someone cleans up `matrix.hpp`.
- **`unique_ptr<Incomplete>` member with an inline destructor** → error `invalid application of 'sizeof' to an incomplete type`. Declare `~Solver();` in the header and define it (`= default`) in the `.cpp`.
- **Empty `CMAKE_BUILD_TYPE`** → no optimisation and no `-g`. Your "benchmark" is 10× slow. Always set it.
- **Editing files under `build/`** → overwritten at the next configure. Everything you write goes in `CMakeLists.txt`.
- **Stale cache**: switching compilers or generators requires `rm -rf build`. A `CMakeCache.txt` remembers the old choice.
- **`-ffast-math` deleting `isnan` checks** → your "NaN guard" is UB-adjacent dead code.
- **Testing floats with `==`** → tests that pass on one machine and fail on another.
- **Finite-difference `h` too small** (`1e-10` in double) → the check reports the *test* as wrong.
- **`-fsanitize=address` and `-fsanitize=thread` together** → refuses to link. Two build directories.

---

## Common mistakes checklist

- [ ] Headers contain only declarations, class definitions, `inline`/`constexpr`/template code.
- [ ] Every header has `#pragma once` and includes what it uses.
- [ ] Public headers are under `include/<libname>/`; sources under `src/`.
- [ ] Forward declarations used where only pointers/references are needed.
- [ ] `CMakeLists.txt` uses `target_*` commands with `PUBLIC`/`PRIVATE`, not globals.
- [ ] `target_compile_features(... cxx_std_17)` rather than a raw `-std` flag.
- [ ] `CMAKE_BUILD_TYPE` set; separate Debug and Release build directories.
- [ ] `CMAKE_EXPORT_COMPILE_COMMANDS ON` so clangd works.
- [ ] Sanitizers behind an `option()`, on in your debug directory.
- [ ] Tests registered with `add_test`; test executable returns non-zero on failure.
- [ ] Float comparisons use rtol + atol; gradient checks use central differences with `h ≈ 1e-5`.
- [ ] Property tests (`A@I == A`, `(AB)^T == B^T A^T`) with seeded random inputs.
- [ ] `build/` is in `.gitignore`.

---

## You can move on when...

- You can explain, without notes, why `template <class T> T f(T)` defined in a `.cpp` gives a linker error while `int g(int)` defined in a header gives a *different* linker error.
- You can write a `CMakeLists.txt` for a static library, an executable that links it, and a doctest test binary, from a blank file, and build it with `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`.
- You know what `CMAKE_BUILD_TYPE=Release` does to your `assert`s.
- You can list three things `-ffast-math` changes and one that will break your numerics.
- You can write a gradient check for a 3-parameter function and choose `h` sensibly.
- You have clangd working in your editor with `compile_commands.json`.
