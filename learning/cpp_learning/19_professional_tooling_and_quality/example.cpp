// Chapter 19 — Professional tooling and quality: what the tools do, built by hand.
//
// Compile (the professional flag set — this file is clean under all of it):
//   c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
//       -Wold-style-cast -Wnon-virtual-dtor -Werror -o ex_demo example.cpp
// Run:
//   ./ex_demo                 # runs the test suite, the property tests, then the micro-benchmarks
//   ./ex_demo -tc=gemm        # filter test cases by substring (like doctest's -tc)
//   ./ex_demo -seed=7         # property-test seed (default 42); a failing seed is replayable
//   ./ex_demo -nobench        # skip the benchmarks
// Sanitizer build (what the CI "asan" job does):
//   c++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all \
//       -o ex_demo example.cpp && UBSAN_OPTIONS=print_stacktrace=1 ./ex_demo -nobench
//
// Sections:
//   1  A 60-line doctest-style framework: TEST_CASE / CHECK / CHECK_THROWS / Approx, registry, filter
//   2  Tiny Matrix + three gemm kernels (one with a planted remainder bug, enabled by -DPLANT_BUG)
//   3  Example-based tests using the framework (fixture, "parameterized" via a loop, Approx)
//   4  Property-based tests with shrinking: (AB)^T = B^T A^T, associativity, distributivity, trace
//   5  A micro-benchmark harness: warm-up, repetitions, min/median, DoNotOptimize + ClobberMemory
//   6  main(): argument parsing, run everything, exit code = number of failures (what ctest needs)
//
// Numbers in comments are from an Apple M-series P-core at -O2; yours will differ.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// =====================================================================================
// 1. A doctest-style mini framework. Real doctest adds: expression decomposition (printing
//    both sides of a == b), SUBCASE re-execution, reporters, timeouts, ~6000 more lines.
// =====================================================================================
namespace mini {

struct TestCase { const char* name; void (*fn)(); };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }   // function-local static: no init-order fiasco
struct Registrar { Registrar(const char* n, void (*f)()) { registry().push_back({n, f}); } };

struct Context {                                 // per-run counters, reset per test case
    int checks = 0, failures = 0;
    const char* current = "";
};
inline Context& ctx() { static Context c; return c; }

// Approx: |a-b| <= eps * (scale + max(|a|,|b|)). Same formula as doctest::Approx.
struct Approx {
    double value, eps = 1e-12, scale_ = 0.0;
    explicit Approx(double v) : value(v) {}
    Approx epsilon(double e) const { Approx a = *this; a.eps = e; return a; }
    Approx scale(double s) const { Approx a = *this; a.scale_ = s; return a; }
    friend bool operator==(double lhs, const Approx& a) {
        return std::fabs(lhs - a.value) <= a.eps * (a.scale_ + std::max(std::fabs(lhs), std::fabs(a.value)));
    }
    friend bool operator==(const Approx& a, double rhs) { return rhs == a; }
};

inline void report_failure(const char* file, int line, const char* expr, const std::string& extra) {
    ++ctx().failures;
    std::printf("  FAIL %s:%d  CHECK(%s)%s%s\n", file, line, expr, extra.empty() ? "" : "  ", extra.c_str());
}

}  // namespace mini

// Token pasting to make unique function/registrar names per TEST_CASE.
#define MINI_CAT2(a, b) a##b
#define MINI_CAT(a, b) MINI_CAT2(a, b)
#define TEST_CASE(name)                                                                     \
    static void MINI_CAT(mini_test_, __LINE__)();                                           \
    static mini::Registrar MINI_CAT(mini_reg_, __LINE__)(name, &MINI_CAT(mini_test_, __LINE__)); \
    static void MINI_CAT(mini_test_, __LINE__)()

#define CHECK(expr)                                                                         \
    do { ++mini::ctx().checks; if (!(expr)) mini::report_failure(__FILE__, __LINE__, #expr, ""); } while (0)
#define CHECK_MESSAGE(expr, msg)                                                             \
    do { ++mini::ctx().checks; if (!(expr)) mini::report_failure(__FILE__, __LINE__, #expr, (msg)); } while (0)
#define REQUIRE(expr)                                                                       \
    do { ++mini::ctx().checks; if (!(expr)) { mini::report_failure(__FILE__, __LINE__, #expr, "(REQUIRE: aborting test case)"); return; } } while (0)
#define CHECK_THROWS_AS(expr, ExType)                                                       \
    do { ++mini::ctx().checks; bool mini_thrown = false;                                    \
         try { (void)(expr); } catch (const ExType&) { mini_thrown = true; } catch (...) {}  \
         if (!mini_thrown) mini::report_failure(__FILE__, __LINE__, #expr, "did not throw " #ExType); } while (0)

// =====================================================================================
// 2. The code under test: a small Matrix and three gemm kernels.
// =====================================================================================
struct Matrix {
    std::size_t rows = 0, cols = 0;
    std::vector<double> d;
    Matrix() = default;
    Matrix(std::size_t r, std::size_t c, double fill = 0.0) : rows(r), cols(c), d(r * c, fill) {}
    double& operator()(std::size_t i, std::size_t j) noexcept { return d[i * cols + j]; }
    double operator()(std::size_t i, std::size_t j) const noexcept { return d[i * cols + j]; }
    double& at(std::size_t i, std::size_t j) {
        if (i >= rows || j >= cols) throw std::out_of_range("Matrix::at");
        return d[i * cols + j];
    }
    friend bool operator==(const Matrix& a, const Matrix& b) { return a.rows == b.rows && a.cols == b.cols && a.d == b.d; }
};
static Matrix transpose(const Matrix& a) {
    Matrix t(a.cols, a.rows);
    for (std::size_t i = 0; i < a.rows; ++i) for (std::size_t j = 0; j < a.cols; ++j) t(j, i) = a(i, j);
    return t;
}
static Matrix identity(std::size_t n) { Matrix I(n, n); for (std::size_t i = 0; i < n; ++i) I(i, i) = 1.0; return I; }
static double trace(const Matrix& a) { double t = 0; for (std::size_t i = 0; i < std::min(a.rows, a.cols); ++i) t += a(i, i); return t; }
static double frob(const Matrix& a) { double s = 0; for (double x : a.d) s += x * x; return std::sqrt(s); }
static Matrix add(const Matrix& a, const Matrix& b) {
    if (a.rows != b.rows || a.cols != b.cols) throw std::invalid_argument("add: shape mismatch");
    Matrix c = a; for (std::size_t i = 0; i < c.d.size(); ++i) c.d[i] += b.d[i]; return c;
}
// Relative Frobenius error, the right way to compare two matrices.
static double rel_err(const Matrix& a, const Matrix& b) {
    double num = 0, den = 0;
    for (std::size_t i = 0; i < a.d.size(); ++i) { const double e = a.d[i] - b.d[i]; num += e * e; den += b.d[i] * b.d[i]; }
    return std::sqrt(num) / (std::sqrt(den) + std::numeric_limits<double>::min());
}

// Kernel 1: textbook ijk (reference; slow but obviously right).
static void gemm_ijk(const Matrix& A, const Matrix& B, Matrix& C) {
    if (A.cols != B.rows) throw std::invalid_argument("gemm: inner dimensions differ");
    C = Matrix(A.rows, B.cols);
    for (std::size_t i = 0; i < A.rows; ++i)
        for (std::size_t j = 0; j < B.cols; ++j) {
            double s = 0;
            for (std::size_t k = 0; k < A.cols; ++k) s += A(i, k) * B(k, j);
            C(i, j) = s;
        }
}
// Kernel 2: ikj — unit-stride inner loop (../12_performance §5).
static void gemm_ikj(const Matrix& A, const Matrix& B, Matrix& C) {
    if (A.cols != B.rows) throw std::invalid_argument("gemm: inner dimensions differ");
    C = Matrix(A.rows, B.cols);
    for (std::size_t i = 0; i < A.rows; ++i)
        for (std::size_t k = 0; k < A.cols; ++k) {
            const double aik = A(i, k);
            double* c = C.d.data() + i * C.cols; const double* b = B.d.data() + k * B.cols;
            for (std::size_t j = 0; j < B.cols; ++j) c[j] += aik * b[j];
        }
}
// Kernel 3: blocked. With -DPLANT_BUG the remainder tiles are dropped when n % TILE != 0 — the
// classic bug that example tests on 64x64 never see, and that property tests find in one second.
static void gemm_blocked(const Matrix& A, const Matrix& B, Matrix& C, std::size_t tile = 32) {
    if (A.cols != B.rows) throw std::invalid_argument("gemm: inner dimensions differ");
    C = Matrix(A.rows, B.cols);
    const std::size_t M = A.rows, K = A.cols, N = B.cols;
    for (std::size_t i0 = 0; i0 < M; i0 += tile)
        for (std::size_t k0 = 0; k0 < K; k0 += tile)
            for (std::size_t j0 = 0; j0 < N; j0 += tile) {
#ifdef PLANT_BUG
                const std::size_t i1 = i0 + tile <= M ? i0 + tile : i0;   // BUG: drops the remainder tile
                const std::size_t k1 = k0 + tile <= K ? k0 + tile : k0;
                const std::size_t j1 = j0 + tile <= N ? j0 + tile : j0;
#else
                const std::size_t i1 = std::min(i0 + tile, M), k1 = std::min(k0 + tile, K), j1 = std::min(j0 + tile, N);
#endif
                for (std::size_t i = i0; i < i1; ++i)
                    for (std::size_t k = k0; k < k1; ++k) {
                        const double aik = A(i, k);
                        for (std::size_t j = j0; j < j1; ++j) C(i, j) += aik * B(k, j);
                    }
            }
}

// =====================================================================================
// 3. Example-based tests. A "fixture" in a framework without TEST_CASE_FIXTURE is just a struct.
// =====================================================================================
struct SmallFixture {
    Matrix A{3, 3}, Ainv{3, 3};
    SmallFixture() {
        const double a[9] = {4, 7, 2, 3, 6, 1, 2, 5, 3};
        const double inv[9] = {13.0 / 9, -11.0 / 9, -5.0 / 9, -7.0 / 9, 8.0 / 9, 2.0 / 9, 1.0 / 3, -2.0 / 3, 1.0 / 3};
        std::copy(a, a + 9, A.d.begin()); std::copy(inv, inv + 9, Ainv.d.begin());
    }
};

TEST_CASE("gemm: A * inv(A) == I within n*eps*|A||Ainv|") {
    SmallFixture f;
    Matrix P; gemm_ikj(f.A, f.Ainv, P);
    // Tolerance derived, not guessed: each entry is a sum of n products, each with relative error ~eps.
    const double tol = 3 * std::numeric_limits<double>::epsilon() * frob(f.A) * frob(f.Ainv);
    CHECK(rel_err(P, identity(3)) < tol * 10);
    CHECK(P(0, 0) == mini::Approx(1.0).epsilon(1e-12));
    // Approx(0.0) NEEDS a scale: tol = eps * (scale + max(|a|,|b|)); with scale 0 only exact zero passes.
    // eps=1e-12, scale=1.0 -> an absolute tolerance of ~1e-12.
    CHECK(P(0, 1) == mini::Approx(0.0).epsilon(1e-12).scale(1.0));
}

TEST_CASE("gemm: shape mismatch throws") {
    Matrix A(2, 3), B(2, 3), C;
    CHECK_THROWS_AS(gemm_ijk(A, B, C), std::invalid_argument);
    CHECK_THROWS_AS(A.at(5, 5), std::out_of_range);
}

TEST_CASE("transpose twice is identity; trace(AB) == trace(BA) on a fixed example") {
    SmallFixture f;
    CHECK(transpose(transpose(f.A)) == f.A);
    Matrix AB, BA; gemm_ikj(f.A, f.Ainv, AB); gemm_ikj(f.Ainv, f.A, BA);
    CHECK(trace(AB) == mini::Approx(trace(BA)).epsilon(1e-12));
}

TEST_CASE("gemm kernels agree on 64x64 (the test that MISSES the planted bug)") {
    std::mt19937_64 rng(1);
    std::uniform_real_distribution<double> U(-1, 1);
    Matrix A(64, 64), B(64, 64);
    for (auto& x : A.d) x = U(rng);
    for (auto& x : B.d) x = U(rng);
    Matrix C1, C2, C3; gemm_ijk(A, B, C1); gemm_ikj(A, B, C2); gemm_blocked(A, B, C3);
    CHECK(rel_err(C2, C1) < 1e-14);
    CHECK(rel_err(C3, C1) < 1e-14);                          // 64 % 32 == 0: the bug is invisible here
}

// "Parameterized" test: a loop with CHECK_MESSAGE carrying the parameter (doctest: SUBCASE or TEST_CASE_TEMPLATE).
TEST_CASE("identity is neutral for several sizes") {
    for (std::size_t n : {1u, 2u, 7u, 33u}) {
        Matrix A(n, n); for (std::size_t i = 0; i < n * n; ++i) A.d[i] = static_cast<double>(i % 11) - 5.0;
        Matrix C; gemm_blocked(A, identity(n), C);
        CHECK_MESSAGE(rel_err(C, A) < 1e-15, "n = " + std::to_string(n));
    }
}

// =====================================================================================
// 4. Property-based tests with shrinking.
// =====================================================================================
namespace prop {

struct Shape { std::size_t m, k, n; };
struct Failure { Shape shape; std::uint64_t seed; std::string message; };

static Matrix random_matrix(std::mt19937_64& rng, std::size_t r, std::size_t c) {
    std::uniform_real_distribution<double> U(-1.0, 1.0);
    Matrix A(r, c); for (auto& x : A.d) x = U(rng); return A;
}

// for_all: run `property` on random shapes/matrices; on failure, shrink dimensions by halving
// until it passes, and report the smallest failing shape. Returns nullopt if all trials pass.
template <class Property>
std::optional<Failure> for_all(const char* name, int trials, std::uint64_t seed, std::size_t max_dim, Property property) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> D(1, max_dim);
    for (int t = 0; t < trials; ++t) {
        const std::uint64_t trial_seed = rng();
        Shape s{D(rng), D(rng), D(rng)};
        auto run = [&](Shape sh) -> std::optional<std::string> {
            std::mt19937_64 r2(trial_seed);
            return property(random_matrix(r2, sh.m, sh.k), random_matrix(r2, sh.k, sh.n), random_matrix(r2, sh.n, sh.m));
        };
        auto msg = run(s);
        if (!msg) continue;
        // Shrink: try halving each dimension while the failure persists.
        Shape best = s;
        bool progressed = true;
        while (progressed) {
            progressed = false;
            for (int dim = 0; dim < 3; ++dim) {
                Shape cand = best;
                std::size_t& v = dim == 0 ? cand.m : dim == 1 ? cand.k : cand.n;
                if (v <= 1) continue;
                v = (v + 1) / 2;
                if (auto m2 = run(cand)) { best = cand; msg = m2; progressed = true; }
            }
        }
        std::printf("  PROP %-34s FAIL  seed=%llu  shape %zux%zux%zu  %s  (shrunk from %zux%zux%zu)\n", name,
                    static_cast<unsigned long long>(trial_seed), best.m, best.k, best.n, msg->c_str(), s.m, s.k, s.n);
        return Failure{best, trial_seed, *msg};
    }
    std::printf("  PROP %-34s ok    (%d trials, seed=%llu)\n", name, trials, static_cast<unsigned long long>(seed));
    return std::nullopt;
}

static std::string err_msg(const char* what, double e) { char buf[64]; std::snprintf(buf, sizeof buf, "%s=%.2e", what, e); return buf; }
constexpr double kEps = std::numeric_limits<double>::epsilon();

}  // namespace prop

// The property tests share the fixture of three random matrices A(m×k), B(k×n), C(n×m).
static int run_property_tests(std::uint64_t seed) {
    using namespace prop;
    int failures = 0;
    std::printf("\n== property tests (seed %llu) ==\n", static_cast<unsigned long long>(seed));

    // (AB)^T == B^T A^T — catches asymmetric indexing bugs. Note: the -DPLANT_BUG remainder bug is
    // SYMMETRIC (both sides drop the same tiles), so this identity passes and "blocked == ijk" below
    // is the one that fails. That is why you test several identities, not one.
    failures += for_all("(AB)^T == B^T A^T [blocked]", 200, seed, 70, [](const Matrix& A, const Matrix& B, const Matrix&) -> std::optional<std::string> {
        Matrix AB, BtAt; gemm_blocked(A, B, AB); gemm_blocked(transpose(B), transpose(A), BtAt);
        const double e = rel_err(transpose(AB), BtAt);
        const double tol = 8 * kEps * static_cast<double>(A.cols) ;   // sums of k terms; relative to |AB|
        return e < tol + 1e-14 ? std::nullopt : std::optional<std::string>(err_msg("rel_err", e));
    }).has_value();

    // Cross-kernel agreement: blocked vs reference ijk.
    failures += for_all("blocked == ijk", 200, seed + 1, 70, [](const Matrix& A, const Matrix& B, const Matrix&) -> std::optional<std::string> {
        Matrix C1, C2; gemm_ijk(A, B, C1); gemm_blocked(A, B, C2);
        const double e = rel_err(C2, C1);
        return e < 8 * kEps * static_cast<double>(A.cols) + 1e-14 ? std::nullopt : std::optional<std::string>(err_msg("rel_err", e));
    }).has_value();

    // Associativity: A(BC) == (AB)C — sensitive to accumulation order, so the tolerance carries k and n.
    failures += for_all("A(BC) == (AB)C [ikj]", 200, seed + 2, 40, [](const Matrix& A, const Matrix& B, const Matrix& C) -> std::optional<std::string> {
        Matrix BC, ABC1, AB, ABC2; gemm_ikj(B, C, BC); gemm_ikj(A, BC, ABC1); gemm_ikj(A, B, AB); gemm_ikj(AB, C, ABC2);
        const double e = rel_err(ABC1, ABC2);
        const double tol = 16 * kEps * static_cast<double>(A.cols + B.cols) * frob(A) * frob(B) * frob(C) / (frob(ABC2) + 1e-300);
        return e < tol ? std::nullopt : std::optional<std::string>(err_msg("rel_err", e));
    }).has_value();

    // Distributivity: (A + A')B == AB + A'B, using C^T (m×n)... shapes: A is m×k, so use A + transpose(C)? no — C is n×m.
    // Simplest: A' = 2A + 1 elementwise; (A + A')B == AB + A'B.
    failures += for_all("(A+A')B == AB + A'B [ikj]", 200, seed + 3, 60, [](const Matrix& A, const Matrix& B, const Matrix&) -> std::optional<std::string> {
        Matrix A2 = A; for (auto& x : A2.d) x = 2 * x + 1;
        Matrix L, R1, R2; gemm_ikj(add(A, A2), B, L); gemm_ikj(A, B, R1); gemm_ikj(A2, B, R2);
        const double e = rel_err(L, add(R1, R2));
        return e < 32 * kEps * static_cast<double>(A.cols) + 1e-14 ? std::nullopt : std::optional<std::string>(err_msg("rel_err", e));
    }).has_value();

    // tr(AB) == tr(BA) needs square products: A (m×k), B (k×m) — reuse C^T as B when n==m is not guaranteed, so build B' = transpose(A)-shaped.
    failures += for_all("tr(A A^T) == tr(A^T A)", 200, seed + 4, 60, [](const Matrix& A, const Matrix&, const Matrix&) -> std::optional<std::string> {
        Matrix At = transpose(A), P1, P2; gemm_ikj(A, At, P1); gemm_ikj(At, A, P2);
        const double t1 = trace(P1), t2 = trace(P2);
        const double e = std::fabs(t1 - t2) / (std::fabs(t1) + 1e-300);
        return e < 64 * kEps * static_cast<double>(A.d.size()) ? std::nullopt : std::optional<std::string>(err_msg("rel_err", e));
    }).has_value();

    // Identity: A * I == A exactly (multiplying by 1.0 and adding 0.0 is exact in IEEE-754).
    failures += for_all("A * I == A exactly [blocked]", 200, seed + 5, 70, [](const Matrix& A, const Matrix&, const Matrix&) -> std::optional<std::string> {
        Matrix P; gemm_blocked(A, identity(A.cols), P);
        return P == A ? std::nullopt : std::optional<std::string>("not bitwise equal");
    }).has_value();

    return failures;
}

// =====================================================================================
// 5. Micro-benchmark harness (what Google Benchmark automates).
// =====================================================================================
namespace bench {

// DoNotOptimize: the empty asm "reads" the value, so the compiler must materialize it.
// ClobberMemory: a compiler barrier; all pending stores must complete before it.
template <class T> inline void DoNotOptimize(const T& value) { asm volatile("" : : "r,m"(value) : "memory"); }
inline void ClobberMemory() { asm volatile("" : : : "memory"); }

struct Result { double min_ms, median_ms, max_ms; int reps; };

template <class F>
Result run(F&& f, int reps = 7, double min_time_ms = 20.0) {
    using clock = std::chrono::steady_clock;
    f();                                                                   // warm-up (cold cache, page faults, frequency ramp)
    // Choose an iteration count so one repetition lasts at least min_time_ms (Google Benchmark does this too).
    int iters = 1;
    for (;;) {
        auto t0 = clock::now();
        for (int i = 0; i < iters; ++i) f();
        const double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
        if (ms >= min_time_ms || iters >= (1 << 20)) break;
        iters *= 2;
    }
    std::vector<double> samples;
    for (int r = 0; r < reps; ++r) {
        auto t0 = clock::now();
        for (int i = 0; i < iters; ++i) f();
        samples.push_back(std::chrono::duration<double, std::milli>(clock::now() - t0).count() / iters);
    }
    std::sort(samples.begin(), samples.end());
    return {samples.front(), samples[samples.size() / 2], samples.back(), reps};
}

}  // namespace bench

static void run_benchmarks() {
    std::printf("\n== micro-benchmarks (min / median of 7 reps, per call) ==\n");
    std::printf("%-28s %10s %10s %10s\n", "kernel", "min ms", "median ms", "GFLOP/s");
    for (std::size_t n : {64u, 128u, 256u}) {
        std::mt19937_64 rng(3);
        Matrix A = prop::random_matrix(rng, n, n), B = prop::random_matrix(rng, n, n), C(n, n);
        const double flops = 2.0 * static_cast<double>(n) * static_cast<double>(n) * static_cast<double>(n);
        auto report = [&](const char* name, bench::Result r) {
            std::printf("%-20s n=%4zu %10.4f %10.4f %10.2f\n", name, n, r.min_ms, r.median_ms, flops / (r.min_ms * 1e-3) / 1e9);
        };
        report("gemm_ijk", bench::run([&] { gemm_ijk(A, B, C); bench::DoNotOptimize(C.d.data()); bench::ClobberMemory(); }));
        report("gemm_ikj", bench::run([&] { gemm_ikj(A, B, C); bench::DoNotOptimize(C.d.data()); bench::ClobberMemory(); }));
        report("gemm_blocked(32)", bench::run([&] { gemm_blocked(A, B, C, 32); bench::DoNotOptimize(C.d.data()); bench::ClobberMemory(); }));
    }
    // The dead-code demonstration: a reduction whose result is discarded vs kept.
    std::vector<double> v(1 << 20, 1.0);
    auto discarded = bench::run([&] { double s = 0; for (double x : v) s += x; (void)s; });                    // may be ~0 ms at -O2
    auto kept      = bench::run([&] { double s = 0; for (double x : v) s += x; bench::DoNotOptimize(s); });   // must run
    std::printf("\nsum of 1M doubles: result discarded %.4f ms   kept via DoNotOptimize %.4f ms   (a ~0 ms result means the optimizer deleted the loop)\n",
                discarded.min_ms, kept.min_ms);
}

// =====================================================================================
// 6. main: filter, run, exit code.
// =====================================================================================
int main(int argc, char** argv) {
    std::string filter;
    std::uint64_t seed = 42;
    bool do_bench = true;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind("-tc=", 0) == 0) filter = a.substr(4);
        else if (a.rfind("-seed=", 0) == 0) seed = std::strtoull(a.c_str() + 6, nullptr, 10);
        else if (a == "-nobench") do_bench = false;
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }

    std::printf("== example-based tests ==\n");
    int total_failures = 0, ran = 0;
    for (const auto& tc : mini::registry()) {
        if (!filter.empty() && std::strstr(tc.name, filter.c_str()) == nullptr) continue;
        mini::ctx() = mini::Context{};
        mini::ctx().current = tc.name;
        try { tc.fn(); }
        catch (const std::exception& e) { mini::report_failure(__FILE__, 0, "test body", std::string("uncaught exception: ") + e.what()); }
        std::printf("  %-4s %s  (%d checks)\n", mini::ctx().failures ? "FAIL" : "ok", tc.name, mini::ctx().checks);
        total_failures += mini::ctx().failures;
        ++ran;
    }
    std::printf("%d test cases, %d failures%s\n", ran, total_failures,
#ifdef PLANT_BUG
                "   [built with -DPLANT_BUG: 64x64 tests still pass — watch the property tests]"
#else
                ""
#endif
    );

    total_failures += run_property_tests(seed);

    if (do_bench) run_benchmarks();

    std::printf("\nexit code %d (ctest treats non-zero as failure)\n", total_failures);
    return total_failures;
}
