// Chapter 11 — Headers, build model, and testing numerics: a one-file "project".
//
// Compile:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp
// Run:      ./ex_demo            (exit status 0 = all checks passed, 1 = a check failed)
//
// This file is deliberately laid out as if it were THREE files of a CMake project so you can
// see what belongs where. The markers below show the split:
//
//     // ===== include/mylib/matrix.hpp =====   -> class defs, inline, constexpr, templates
//     // ===== src/matrix.cpp ===============   -> non-inline function definitions
//     // ===== tests/test_matrix.cpp =========   -> hand-rolled CHECK framework + numerics tests
//
// The matching CMakeLists.txt is in lesson.md section 10. To turn this into that project,
// cut at the markers, replace "matrix.hpp" content with a real header (#pragma once at top),
// and add `#include <mylib/matrix.hpp>` to the two .cpp files.
//
// Concepts shown: ODR-safe header content (inline / constexpr / template / class), anonymous
// namespace for TU-local helpers, inline variables, a CHECK/CHECK_CLOSE/CHECK_THROWS macro
// trio, tolerance-based comparison (rtol + atol), property tests with seeded randomness, a
// central-difference gradient check, and the exit-code contract with ctest.

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ===== include/mylib/matrix.hpp ==============================================================
// (would start with `#pragma once` and `#include <vector>`, `#include <string>`, `#include <cmath>`)

namespace ml {

// inline variable (C++17): ONE object program-wide even if 50 TUs include this header.
// Pre-17, `static const` here would create 50 copies; `extern` would need a .cpp.
inline constexpr double kDefaultRtol = 1e-12;
inline constexpr double kDefaultAtol = 1e-14;

// constexpr functions are implicitly inline -> header is the right place.
constexpr std::size_t idx(std::size_t i, std::size_t j, std::size_t cols) noexcept { return i * cols + j; }

// Templates MUST be in the header: the compiler can only instantiate what it can see.
template <typename T>
bool close(T a, T b, T rtol = static_cast<T>(kDefaultRtol), T atol = static_cast<T>(kDefaultAtol)) {
    // Python equivalent: np.isclose(a, b, rtol=rtol, atol=atol)
    return std::fabs(a - b) <= atol + rtol * std::fmax(std::fabs(a), std::fabs(b));
}

class Matrix {
public:
    Matrix() = default;
    Matrix(std::size_t r, std::size_t c, double fill = 0.0) : rows_(r), cols_(c), data_(r * c, fill) {}

    // Defined inside the class body -> implicitly inline -> fine in a header.
    std::size_t rows() const noexcept { return rows_; }
    std::size_t cols() const noexcept { return cols_; }
    double& operator()(std::size_t i, std::size_t j) noexcept { return data_[idx(i, j, cols_)]; }
    double operator()(std::size_t i, std::size_t j) const noexcept { return data_[idx(i, j, cols_)]; }

    // Declared here, DEFINED in matrix.cpp (non-inline). Two TUs including this header is fine
    // because a declaration may appear any number of times.
    Matrix transpose() const;
    double trace() const;

private:
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<double> data_;
};

// Free-function declarations. Definitions live in the .cpp below.
Matrix matmul(const Matrix& a, const Matrix& b);
Matrix identity(std::size_t n);
Matrix random_matrix(std::size_t r, std::size_t c, unsigned seed);
bool allclose(const Matrix& a, const Matrix& b, double rtol = 1e-12, double atol = 1e-14);

// A NON-inline function defined in a header would be an ODR violation the moment two .cpp
// files included it:  "duplicate symbol ml::bad() in a.o and b.o". Either mark it inline or
// move it to the .cpp.  (Commented out on purpose.)
// double bad() { return 1.0; }

}  // namespace ml

// ===== src/matrix.cpp ========================================================================
// (would start with `#include <mylib/matrix.hpp>`)

namespace ml {

// Anonymous namespace: helpers visible ONLY in this TU. Another .cpp may define its own
// `check_same_shape` without an ODR clash. This replaces C's `static` for functions.
namespace {
void check_same_shape(const Matrix& a, const Matrix& b, const char* op) {
    if (a.rows() != b.rows() || a.cols() != b.cols())
        throw std::invalid_argument("shape mismatch: (" + std::to_string(a.rows()) + "," +
                                    std::to_string(a.cols()) + ") " + op + " (" +
                                    std::to_string(b.rows()) + "," + std::to_string(b.cols()) + ")");
}
}  // namespace

Matrix Matrix::transpose() const {
    Matrix t(cols_, rows_);
    for (std::size_t i = 0; i < rows_; ++i)
        for (std::size_t j = 0; j < cols_; ++j) t(j, i) = (*this)(i, j);
    return t;
}

double Matrix::trace() const {
    double s = 0.0;
    const std::size_t n = rows_ < cols_ ? rows_ : cols_;
    for (std::size_t i = 0; i < n; ++i) s += (*this)(i, i);
    return s;
}

Matrix matmul(const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows())
        throw std::invalid_argument("shape mismatch: (" + std::to_string(a.rows()) + "," +
                                    std::to_string(a.cols()) + ") @ (" + std::to_string(b.rows()) +
                                    "," + std::to_string(b.cols()) + ")");
    Matrix c(a.rows(), b.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t k = 0; k < a.cols(); ++k) {
            const double aik = a(i, k);
            for (std::size_t j = 0; j < b.cols(); ++j) c(i, j) += aik * b(k, j);
        }
    return c;
}

Matrix identity(std::size_t n) {
    Matrix I(n, n);
    for (std::size_t i = 0; i < n; ++i) I(i, i) = 1.0;
    return I;
}

// Seeded: a failing property test can be reproduced by printing the seed.
Matrix random_matrix(std::size_t r, std::size_t c, unsigned seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    Matrix m(r, c);
    for (std::size_t i = 0; i < r; ++i)
        for (std::size_t j = 0; j < c; ++j) m(i, j) = dist(gen);
    return m;
}

bool allclose(const Matrix& a, const Matrix& b, double rtol, double atol) {
    check_same_shape(a, b, "allclose");
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t j = 0; j < a.cols(); ++j)
            if (!close(a(i, j), b(i, j), rtol, atol)) return false;
    return true;
}

}  // namespace ml

// ===== tests/test_matrix.cpp =================================================================
// (would start with `#include <mylib/matrix.hpp>`; with doctest you'd replace the macros below
//  by `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` + `#include <doctest/doctest.h>` and wrap each
//  group in TEST_CASE("...") { ... }.)

namespace {
int g_total = 0;
int g_failed = 0;
}  // namespace

// #expr stringifies the argument; __FILE__/__LINE__ locate the failure; do{}while(0) makes the
// macro a single statement so `if (x) CHECK(y); else ...` behaves.
#define CHECK(expr)                                                                          \
    do {                                                                                     \
        ++g_total;                                                                           \
        if (!(expr)) {                                                                       \
            ++g_failed;                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr);    \
        }                                                                                    \
    } while (0)

#define CHECK_CLOSE(a, b, rtol, atol)                                                        \
    do {                                                                                     \
        ++g_total;                                                                           \
        const double a_ = (a), b_ = (b);                                                     \
        if (!ml::close(a_, b_, (rtol), (atol))) {                                            \
            ++g_failed;                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK_CLOSE failed: %s == %s  (%.17g vs %.17g)\n",  \
                         __FILE__, __LINE__, #a, #b, a_, b_);                                \
        }                                                                                    \
    } while (0)

#define CHECK_THROWS(expr, ExcType)                                                          \
    do {                                                                                     \
        ++g_total;                                                                           \
        bool ok_ = false;                                                                    \
        try {                                                                                \
            (void)(expr);                                                                    \
        } catch (const ExcType&) {                                                           \
            ok_ = true;                                                                      \
        } catch (...) {                                                                      \
        }                                                                                    \
        if (!ok_) {                                                                          \
            ++g_failed;                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK_THROWS failed: %s did not throw %s\n",        \
                         __FILE__, __LINE__, #expr, #ExcType);                               \
        }                                                                                    \
    } while (0)

// A tiny "TEST_CASE": prints the name, runs, reports how many checks it added.
static void run_case(const char* name, const std::function<void()>& body) {
    const int before_total = g_total, before_failed = g_failed;
    body();
    std::printf("  [%s] %-40s %d checks, %d failed\n", (g_failed == before_failed) ? "ok " : "BAD",
                name, g_total - before_total, g_failed - before_failed);
}

// ---- Numerics test 1: why == fails and tolerances are needed --------------------------------
static void test_tolerances() {
    // 0.1 + 0.2 != 0.3 in binary floating point. An == test here would be a bug in the TEST.
    CHECK(0.1 + 0.2 != 0.3);
    CHECK_CLOSE(0.1 + 0.2, 0.3, 1e-15, 1e-15);
    // atol matters near zero: relative tolerance of two tiny numbers is meaningless.
    CHECK_CLOSE(1e-300 - 1e-300, 0.0, 1e-9, 1e-12);
    // Error of a 1000-term dot product in double is ~1e-13 relative, not 1e-16.
    std::vector<double> v(1000, 0.1);
    double s = 0.0;
    for (double x : v) s += x;
    CHECK_CLOSE(s, 100.0, 1e-13, 0.0);
    CHECK(!ml::close(s, 100.0, 0.0, 0.0));   // exact comparison fails: accumulated rounding
}

// ---- Numerics test 2: algebraic property tests over seeded random inputs -----------------------
static void test_matrix_properties() {
    for (unsigned seed = 1; seed <= 10; ++seed) {
        std::mt19937 g(seed);
        std::uniform_int_distribution<std::size_t> dim(1, 6);
        const std::size_t n = dim(g), m = dim(g), p = dim(g);
        ml::Matrix A = ml::random_matrix(n, m, seed), B = ml::random_matrix(m, p, seed + 100);
        ml::Matrix C = ml::random_matrix(p, n, seed + 200);

        CHECK(ml::allclose(ml::matmul(A, ml::identity(m)), A));                  // A @ I == A
        CHECK(ml::allclose(ml::matmul(ml::identity(n), A), A));                  // I @ A == A
        CHECK(ml::allclose(A.transpose().transpose(), A));                       // (A^T)^T == A
        CHECK(ml::allclose(ml::matmul(A, B).transpose(),                         // (AB)^T == B^T A^T
                           ml::matmul(B.transpose(), A.transpose())));
        CHECK(ml::allclose(ml::matmul(ml::matmul(A, B), C),                      // (AB)C ≈ A(BC)
                           ml::matmul(A, ml::matmul(B, C)), 1e-12, 1e-13));
        ml::Matrix D = ml::random_matrix(m, n, seed + 300);                      // shape so AD and DA exist
        CHECK_CLOSE(ml::matmul(A, D).trace(),                                    // tr(AD) == tr(DA)
                    ml::matmul(D, A).trace(), 1e-12, 1e-13);
    }
    ml::Matrix a(2, 3), b(2, 3);
    CHECK_THROWS(ml::matmul(a, b), std::invalid_argument);
    CHECK_THROWS(ml::matmul(a, b), std::exception);        // base class also matches
}

// ---- Numerics test 3: gradient check (the test every autograd needs) ---------------------------
// f(w) = sum_i (w . x_i - y_i)^2  (least squares).  Analytic grad = 2 * sum_i (w.x_i - y_i) x_i.
static double lsq_loss(const std::vector<double>& w, const std::vector<std::vector<double>>& X,
                       const std::vector<double>& y) {
    double L = 0.0;
    for (std::size_t i = 0; i < X.size(); ++i) {
        double r = -y[i];
        for (std::size_t j = 0; j < w.size(); ++j) r += w[j] * X[i][j];
        L += r * r;
    }
    return L;
}
static std::vector<double> lsq_grad(const std::vector<double>& w, const std::vector<std::vector<double>>& X,
                                    const std::vector<double>& y) {
    std::vector<double> g(w.size(), 0.0);
    for (std::size_t i = 0; i < X.size(); ++i) {
        double r = -y[i];
        for (std::size_t j = 0; j < w.size(); ++j) r += w[j] * X[i][j];
        for (std::size_t j = 0; j < w.size(); ++j) g[j] += 2.0 * r * X[i][j];
    }
    return g;
}

static void test_gradient_check() {
    std::mt19937 gen(7);
    std::uniform_real_distribution<double> U(-1.0, 1.0);
    const std::size_t n = 12, d = 4;
    std::vector<std::vector<double>> X(n, std::vector<double>(d));
    std::vector<double> y(n), w(d);
    for (auto& row : X)
        for (double& v : row) v = U(gen);
    for (double& v : y) v = U(gen);
    for (double& v : w) v = U(gen);

    const std::vector<double> analytic = lsq_grad(w, X, y);
    const double h = 1e-5;   // sweet spot for double: truncation O(h^2)=1e-10, roundoff ~1e-16/h=1e-11
    for (std::size_t j = 0; j < d; ++j) {
        const double old = w[j];
        w[j] = old + h;
        const double fp = lsq_loss(w, X, y);
        w[j] = old - h;
        const double fm = lsq_loss(w, X, y);
        w[j] = old;
        const double numeric = (fp - fm) / (2 * h);
        CHECK_CLOSE(analytic[j], numeric, 1e-6, 1e-8);
    }

    // A deliberately WRONG gradient (missing factor 2) must be caught by the same check.
    std::vector<double> wrong = analytic;
    for (double& v : wrong) v *= 0.5;
    int caught = 0;
    for (std::size_t j = 0; j < d; ++j) {
        const double old = w[j];
        w[j] = old + h;
        const double fp = lsq_loss(w, X, y);
        w[j] = old - h;
        const double fm = lsq_loss(w, X, y);
        w[j] = old;
        if (!ml::close(wrong[j], (fp - fm) / (2 * h), 1e-6, 1e-8)) ++caught;
    }
    CHECK(caught == static_cast<int>(d));
}

// ---- Numerics test 4: a known analytic solution -------------------------------------------------
static void test_rk4_against_exact() {
    // y' = -y, y(0) = 1  ->  y(t) = e^-t.  RK4 global error ~ C h^4.
    auto f = [](double, double y) { return -y; };
    for (double h : {0.1, 0.05}) {
        double y = 1.0, t = 0.0;
        while (t < 1.0 - 1e-12) {
            const double k1 = f(t, y);
            const double k2 = f(t + h / 2, y + h / 2 * k1);
            const double k3 = f(t + h / 2, y + h / 2 * k2);
            const double k4 = f(t + h, y + h * k3);
            y += h / 6 * (k1 + 2 * k2 + 2 * k3 + k4);
            t += h;
        }
        // Tolerance chosen from the METHOD's order, not from what passes: h^4 * small constant.
        CHECK_CLOSE(y, std::exp(-1.0), 0.0, h * h * h * h);
    }
}

int main() {
    std::printf("running tests (a doctest/ctest run would look much like this)\n");
    run_case("tolerances: == vs close()", test_tolerances);
    run_case("matrix algebraic properties (10 seeds)", test_matrix_properties);
    run_case("gradient check: least squares", test_gradient_check);
    run_case("rk4 vs exact solution", test_rk4_against_exact);

    // Show what a FAILURE looks like, without failing the run: a scratch counter.
    std::printf("\nwhat a failing check prints (deliberate, not counted):\n");
    std::fflush(stdout);   // stderr is unbuffered; flush so the lines appear in order
    {
        const int saved_total = g_total, saved_failed = g_failed;
        CHECK(0.1 + 0.2 == 0.3);
        g_total = saved_total;
        g_failed = saved_failed;
    }

    std::printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
    std::printf("inline constants: kDefaultRtol=%g kDefaultAtol=%g  (one object program-wide)\n",
                ml::kDefaultRtol, ml::kDefaultAtol);
    // Exit-code contract: ctest treats non-zero as FAIL.
    return g_failed ? 1 : 0;
}
