# P01 — Matrix Class

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C++ 01-07, 11   **Builds on:** C P02 (matrix library)

## Goal

A `Matrix` class that owns its memory through RAII, follows the Rule of Five (copy/move constructors and assignments, destructor), and reads like NumPy through operator overloading: `A * B` is matmul, `A + B` elementwise, `A(i, j)` indexing, `std::cout << A` printing, `hadamard(A, B)` (or `A % B`) for elementwise product. Built with CMake, tested with doctest, and benchmarked head-to-head against your C `mat_matmul` so you learn what C++ costs and what it buys.

## Why

This is the C++ P02: the same functionality, but now `Matrix m = a * b + c;` does the allocation and freeing for you and cannot leak. Everything downstream (P02 tensors, P03 autograd, P04 framework, P05 transformer, P11 boosting) is written in this style, so the habits formed here — value semantics, `const&` parameters, move returns, `noexcept` on moves, tests in a separate target — carry through. The benchmark against C answers the question every C programmer asks about C++: is the abstraction free? (At `-O2`, yes for this class; you'll measure it.)

## The math

Same as C P02: row-major storage, $C_{ij} = \sum_p A_{ip}B_{pj}$, transpose, elementwise ops, bias-row broadcast. New here: operator semantics.

- `A * B` requires `A.cols() == B.rows()`; throw `std::invalid_argument` otherwise (see C++ 10). Compare: the C version returned `NULL` and made the caller check.
- Expression `A * B + C` creates a temporary for `A * B`; with a move constructor, `+` can steal that temporary's buffer instead of copying it. Count allocations to see it.
- Copy elision / NRVO: returning a local `Matrix` by value costs zero copies at `-O2`. Verify by counting constructor calls with a static counter.

Performance model: the matmul kernel is the same triple loop as in C; any difference in timing comes from (1) `operator()` not being inlined (fix: define it in the header), (2) bounds checks (`assert`, compiled out with `-DNDEBUG`), (3) allocator calls. Expect within 5% of the C version.

## Spec

**Directory layout**

```
cpp/matrix/
  CMakeLists.txt
  include/matrix.hpp
  src/matrix.cpp
  tests/test_matrix.cpp       (doctest)
  bench/bench_matmul.cpp      (times C++ vs C mat_matmul linked from neural_network_c/matrix)
```

**Interface** (`matrix.hpp`; signatures only):

```cpp
#pragma once
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>

class Matrix {
public:
    Matrix();                                            // 0x0
    Matrix(std::size_t rows, std::size_t cols);          // zero-filled
    Matrix(std::size_t rows, std::size_t cols, double fill);
    Matrix(std::initializer_list<std::initializer_list<double>> rows);   // Matrix m = {{1,2},{3,4}};
    Matrix(const Matrix& other);
    Matrix(Matrix&& other) noexcept;
    Matrix& operator=(const Matrix& other);
    Matrix& operator=(Matrix&& other) noexcept;
    ~Matrix();

    std::size_t rows() const noexcept;
    std::size_t cols() const noexcept;
    std::size_t size() const noexcept;
    double*       data() noexcept;
    const double* data() const noexcept;

    double&       operator()(std::size_t i, std::size_t j);          // asserts bounds in debug
    const double& operator()(std::size_t i, std::size_t j) const;

    Matrix& operator+=(const Matrix& rhs);
    Matrix& operator-=(const Matrix& rhs);
    Matrix& operator*=(double s);

    Matrix  transpose() const;
    Matrix  apply(const std::function<double(double)>& f) const;    // or a template on F
    double  sum() const;
    Matrix  sum_rows() const;      // 1 x cols  (axis=0)
    Matrix  sum_cols() const;      // rows x 1  (axis=1)
    std::size_t argmax_row(std::size_t i) const;

    static Matrix identity(std::size_t n);
    static Matrix random_uniform(std::size_t r, std::size_t c, double lo, double hi, unsigned seed);
    static Matrix random_normal (std::size_t r, std::size_t c, double mean, double std, unsigned seed);
    static Matrix load(const std::string& path);         // MAT1 format from C P02
    void          save(const std::string& path) const;

private:
    std::size_t rows_, cols_;
    double* data_;            // owned; or std::vector<double> — see Hints
};

Matrix operator+(const Matrix& a, const Matrix& b);
Matrix operator-(const Matrix& a, const Matrix& b);
Matrix operator*(const Matrix& a, const Matrix& b);      // matmul
Matrix operator*(const Matrix& a, double s);
Matrix operator*(double s, const Matrix& a);
Matrix hadamard(const Matrix& a, const Matrix& b);
Matrix add_rowvec(const Matrix& a, const Matrix& row);
bool   allclose(const Matrix& a, const Matrix& b, double atol = 1e-9, double rtol = 1e-9);
std::ostream& operator<<(std::ostream& os, const Matrix& m);
```

Implement the class **twice**: first with a raw `double*` and hand-written Rule of Five (so you feel every special member), then switch the storage to `std::vector<double>` and delete the four special members you no longer need (Rule of Zero). Tests must pass unchanged.

**CMake**: a `matrix` library target, a `test_matrix` executable registered with `add_test`/CTest, a `bench_matmul` executable that also compiles `../../neural_network_c/matrix/matrix.c` as C. `-Wall -Wextra -std=c++17`; `-O2` in Release.

**Bench output**

```
n=512   C mat_matmul: 0.148 s   C++ operator*: 0.151 s   ratio 1.02
n=1024  C mat_matmul: 1.21 s    C++ operator*: 1.23 s    ratio 1.02
```

## Milestones

1. **M1 — RAII skeleton.** Constructor, destructor, `operator()`, `<<`. A `Matrix` created in a scope frees its memory at the closing brace — verify with `-fsanitize=address` and a loop that creates a million matrices (memory stays flat).
2. **M2 — Rule of Five.** Copy is deep (modifying the copy doesn't touch the original); move leaves the source with `rows_ == cols_ == 0` and `data_ == nullptr`; self-assignment is safe. Instrument with counters and confirm `Matrix m = f();` performs zero copies.
3. **M3 — operators + tests.** All doctest cases pass: `{{1,2},{3,4}} * {{5,6},{7,8}} == {{19,22},{43,50}}`; shape mismatch throws `std::invalid_argument` (`CHECK_THROWS_AS`); `(A*B).transpose()` allclose to `B.transpose()*A.transpose()`; `identity(n) * A == A`.
4. **M4 — save/load interop.** Save from C++, load in your C library and in Python's `load_mat`; load a C-written `.mat` in C++. Bitwise identical.
5. **M5 — benchmark.** Within 10% of the C version at 512 and 1024. If slower: check `operator()` inlining, `-DNDEBUG`, and that you are not copying inside the loop.
6. **M6 — Rule of Zero refactor.** Switch to `std::vector<double>`, remove the special members, tests and bench unchanged. Note in a comment what the compiler generates for you now.

## Verification

```python
import numpy as np
from load_mat import load_mat            # from C P02
A, B, C = load_mat("A.mat"), load_mat("B.mat"), load_mat("C.mat")   # C = A*B saved by your C++ test
print(np.abs(A @ B - C).max())            # < 1e-12
```

Plus the doctest suite (`ctest --output-on-failure`) and ASan/UBSan clean runs of both the tests and the bench.

## Stretch goals

- `operator%` as hadamard and decide, in a comment, whether that is a good idea (readability vs surprise).
- Template the scalar type: `Matrix<float>` vs `Matrix<double>` benchmark (a preview of P02's `Tensor<T>`).
- Expression templates for `A + B + C` to avoid the intermediate temporary; measure allocations before/after.
- Blocked matmul + `std::thread` over row blocks; compare with the single-threaded C version.

## Hints

- Declare the two-argument constructor `explicit` or you will get surprising implicit conversions.
- Copy-and-swap is the simplest correct copy assignment: take `other` by value, `swap(*this, other)`. Write a `friend void swap(Matrix&, Matrix&) noexcept`.
- The move constructor must set the source's pointer to `nullptr` so the source's destructor does nothing. Mark it `noexcept` — `std::vector<Matrix>` will copy instead of move otherwise.
- Put `operator()` in the header so it inlines. `assert(i < rows_ && j < cols_)` costs nothing in Release.
- doctest is a single header: download `doctest.h` into `tests/`, define `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in exactly one translation unit.
- CMake: `add_library(matrix src/matrix.cpp)`, `target_include_directories(matrix PUBLIC include)`, `target_compile_features(matrix PUBLIC cxx_std_17)`. For the bench, `project(... LANGUAGES C CXX)` and add the C file to the executable's sources.

## Where to put it

`cpp/matrix/` as laid out above. Later C++ projects add `cpp/tensor/`, `cpp/autograd/`, `cpp/nn/`, `cpp/numerics/`, `cpp/sims/...` as sibling directories under one top-level `cpp/CMakeLists.txt`.
