// Chapter 12 — Performance: measure, then fix layout, allocation, vectorization, parallelism.
//
// Compile (baseline):
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp
// Faster (uses every instruction your CPU has):
//   c++ -Wall -Wextra -std=c++17 -O3 -mcpu=native -o ex_demo example.cpp
// See what the vectorizer did:
//   c++ -std=c++17 -O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -c example.cpp -o /dev/null
// With OpenMP (Apple clang has no libomp by default):
//   brew install libomp
//   c++ -Wall -Wextra -std=c++17 -O2 -Xpreprocessor -fopenmp \
//       -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp -o ex_demo example.cpp
//   OMP_NUM_THREADS=8 ./ex_demo
// With Accelerate BLAS (macOS):
//   c++ -Wall -Wextra -std=c++17 -O2 -DUSE_ACCELERATE -framework Accelerate -o ex_demo example.cpp
// Run:      ./ex_demo
//
// Everything below is guarded so the BASELINE command compiles with zero warnings on any
// platform: NEON code is under #if defined(__ARM_NEON), OpenMP under #ifdef _OPENMP, BLAS under
// #ifdef USE_ACCELERATE. Numbers in the comments are from an Apple M-series P-core at -O2; yours
// will differ — that's the point of measuring.
//
// Sections:
//   1  Timer (RAII) + bench harness + DoNotOptimize
//   2  matmul loop order: ijk vs ikj vs blocked  (memory hierarchy in action)
//   3  AoS vs SoA particle update
//   4  hidden copies: counting copies of a Matrix
//   5  allocation in the hot loop: matmul() vs matmul_into()
//   6  dot product: scalar, 4-accumulator, NEON intrinsics
//   7  parallelism: std::thread parallel_for, OpenMP reduction, atomics/mutex, false sharing
//   8  BLAS (optional) + the checklist

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#ifdef USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

// ---- 1. Measuring ---------------------------------------------------------------------------
class Timer {
    using clock = std::chrono::steady_clock;
    const char* label_;
    clock::time_point start_ = clock::now();
public:
    explicit Timer(const char* label) : label_(label) {}
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    double elapsed_ms() const { return std::chrono::duration<double, std::milli>(clock::now() - start_).count(); }
    ~Timer() { std::printf("    %-34s %9.3f ms\n", label_, elapsed_ms()); }
};

// Tell the optimizer "someone reads this value" without generating code. Google Benchmark's trick.
template <class T>
inline void DoNotOptimize(const T& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// Warm-up once, then repeat; return the MINIMUM (noise only ever adds time).
template <class F>
double bench_ms(F&& f, int repeats = 5) {
    using clock = std::chrono::steady_clock;
    f();   // warm-up: cold caches, page faults, frequency ramp
    double best = 1e300;
    for (int r = 0; r < repeats; ++r) {
        auto t0 = clock::now();
        f();
        auto t1 = clock::now();
        best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return best;
}

static void report(const char* label, double ms, double flops = 0.0) {
    if (flops > 0) std::printf("    %-34s %9.3f ms  %7.2f GFLOP/s\n", label, ms, flops / (ms * 1e6));
    else           std::printf("    %-34s %9.3f ms\n", label, ms);
}

// ---- 2. matmul loop order --------------------------------------------------------------------
// Row-major: element (i,j) at i*n + j. All three compute C = A*B for square n x n.

// Textbook order. Inner loop reads B(k, j) for k = 0..n: stride n doubles -> one cache line per
// element, no vectorization. ~1 GFLOP/s.
static void matmul_ijk(const double* A, const double* B, double* C, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k) s += A[i * n + k] * B[k * n + j];
            C[i * n + j] = s;
        }
}

// Inner loop walks a ROW of B and a ROW of C: unit stride, vectorizes to width 2 (NEON).
// A(i,k) is hoisted into a register. Typically 3-10x faster than ijk.
static void matmul_ikj(const double* A, const double* B, double* C, std::size_t n) {
    std::fill(C, C + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t k = 0; k < n; ++k) {
            const double aik = A[i * n + k];
            const double* brow = B + k * n;
            double* crow = C + i * n;
            for (std::size_t j = 0; j < n; ++j) crow[j] += aik * brow[j];
        }
}

// Tiled: work on T x T blocks so the three active blocks (3*T*T*8 bytes) stay in L1/L2.
// T=64 -> 96 KB.  Gains over ikj grow with n (for n=256 the whole problem already fits in L2).
static void matmul_blocked(const double* A, const double* B, double* C, std::size_t n, std::size_t T) {
    std::fill(C, C + n * n, 0.0);
    for (std::size_t ii = 0; ii < n; ii += T)
        for (std::size_t kk = 0; kk < n; kk += T)
            for (std::size_t jj = 0; jj < n; jj += T) {
                const std::size_t imax = std::min(ii + T, n), kmax = std::min(kk + T, n), jmax = std::min(jj + T, n);
                for (std::size_t i = ii; i < imax; ++i)
                    for (std::size_t k = kk; k < kmax; ++k) {
                        const double aik = A[i * n + k];
                        const double* brow = B + k * n;
                        double* crow = C + i * n;
                        for (std::size_t j = jj; j < jmax; ++j) crow[j] += aik * brow[j];
                    }
            }
}

static double max_abs_diff(const std::vector<double>& a, const std::vector<double>& b) {
    double m = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}

static void section_matmul() {
    std::printf("== 2. matmul loop order (n=256, 2n^3 flops) ==\n");
    const std::size_t n = 256;
    const double flops = 2.0 * static_cast<double>(n) * n * n;
    std::mt19937 gen(1);
    std::uniform_real_distribution<double> U(-1, 1);
    std::vector<double> A(n * n), B(n * n), C1(n * n), C2(n * n), C3(n * n);
    for (auto& v : A) v = U(gen);
    for (auto& v : B) v = U(gen);

    report("ijk (naive, strided B)", bench_ms([&] { matmul_ijk(A.data(), B.data(), C1.data(), n); }), flops);
    report("ikj (unit stride, vectorizes)", bench_ms([&] { matmul_ikj(A.data(), B.data(), C2.data(), n); }), flops);
    report("ikj blocked T=64", bench_ms([&] { matmul_blocked(A.data(), B.data(), C3.data(), n, 64); }), flops);
#ifdef USE_ACCELERATE
    std::vector<double> C4(n * n);
    report("cblas_dgemm (Accelerate)", bench_ms([&] {
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, static_cast<int>(n), static_cast<int>(n),
                    static_cast<int>(n), 1.0, A.data(), static_cast<int>(n), B.data(), static_cast<int>(n),
                    0.0, C4.data(), static_cast<int>(n));
    }), flops);
    std::printf("    max |ikj - dgemm| = %.2e\n", max_abs_diff(C2, C4));
#else
    std::printf("    (recompile with -DUSE_ACCELERATE -framework Accelerate to see cblas_dgemm: ~100x ijk)\n");
#endif
    // Different summation ORDER -> results differ in the last bits. Compare with a tolerance.
    std::printf("    max |ijk - ikj| = %.2e   max |ikj - blocked| = %.2e  (same math, different rounding)\n",
                max_abs_diff(C1, C2), max_abs_diff(C2, C3));
}

// ---- 3. AoS vs SoA ---------------------------------------------------------------------------
struct Particle { double x, y, z, vx, vy, vz, mass; };          // 56 bytes; one cache line holds ~1.1

struct Particles {                                                // SoA: 7 contiguous streams
    std::vector<double> x, y, z, vx, vy, vz, mass;
    explicit Particles(std::size_t n) : x(n), y(n), z(n), vx(n), vy(n), vz(n), mass(n) {}
};

static void section_aos_soa() {
    std::printf("== 3. AoS vs SoA: position update for 2M particles ==\n");
    const std::size_t N = 2'000'000;
    const double dt = 1e-3;
    std::vector<Particle> aos(N);
    Particles soa(N);
    for (std::size_t i = 0; i < N; ++i) {
        const double v = static_cast<double>(i % 97) * 0.01;
        aos[i] = {v, v, v, 1.0, 0.5, 0.25, 1.0};
        soa.x[i] = soa.y[i] = soa.z[i] = v;
        soa.vx[i] = 1.0; soa.vy[i] = 0.5; soa.vz[i] = 0.25; soa.mass[i] = 1.0;
    }
    // AoS: every iteration pulls 56 B to touch 48 B, and the pattern is a strided gather for SIMD.
    double t_aos = bench_ms([&] {
        for (auto& p : aos) { p.x += dt * p.vx; p.y += dt * p.vy; p.z += dt * p.vz; }
        DoNotOptimize(aos[N / 2].x);
    });
    // SoA: three axpy-style streams; auto-vectorizes (check with -Rpass=loop-vectorize).
    double t_soa = bench_ms([&] {
        const std::size_t n = soa.x.size();
        double *x = soa.x.data(), *y = soa.y.data(), *z = soa.z.data();
        const double *vx = soa.vx.data(), *vy = soa.vy.data(), *vz = soa.vz.data();
        for (std::size_t i = 0; i < n; ++i) x[i] += dt * vx[i];
        for (std::size_t i = 0; i < n; ++i) y[i] += dt * vy[i];
        for (std::size_t i = 0; i < n; ++i) z[i] += dt * vz[i];
        DoNotOptimize(x[n / 2]);
    });
    report("AoS  vector<Particle>", t_aos);
    report("SoA  struct of vectors", t_soa);
    std::printf("    (both are memory-bound here; SoA moves fewer bytes per useful flop and vectorizes)\n");
}

// ---- 4. Hidden copies -------------------------------------------------------------------------
struct Matrix {
    static inline std::size_t copies = 0;                       // inline static: no .cpp needed (C++17)
    std::size_t rows = 0, cols = 0;
    std::vector<double> data;
    Matrix() = default;
    Matrix(std::size_t r, std::size_t c) : rows(r), cols(c), data(r * c, 1.0) {}
    Matrix(const Matrix& o) : rows(o.rows), cols(o.cols), data(o.data) { ++copies; }
    Matrix& operator=(const Matrix& o) { rows = o.rows; cols = o.cols; data = o.data; ++copies; return *this; }
    Matrix(Matrix&&) noexcept = default;                        // noexcept: vector<Matrix> may MOVE on growth
    Matrix& operator=(Matrix&&) noexcept = default;
    double& operator()(std::size_t i, std::size_t j) noexcept { return data[i * cols + j]; }
    double operator()(std::size_t i, std::size_t j) const noexcept { return data[i * cols + j]; }
};

static double frob_by_value(Matrix m) {                          // BAD: copies the whole matrix
    double s = 0; for (double v : m.data) s += v * v; return std::sqrt(s);
}
static double frob_by_ref(const Matrix& m) {                     // GOOD
    double s = 0; for (double v : m.data) s += v * v; return std::sqrt(s);
}

static void section_copies() {
    std::printf("== 4. hidden copies (100 matrices of 128x128) ==\n");
    const std::size_t K = 100;
    {
        Matrix::copies = 0;
        std::vector<Matrix> mats;                                // no reserve -> reallocations (moves, since noexcept)
        for (std::size_t i = 0; i < K; ++i) mats.push_back(Matrix(128, 128));   // temporary + move
        double acc = 0;
        for (auto m : mats) acc += frob_by_value(m);             // 'auto m' copies, by-value param copies again
        DoNotOptimize(acc);
        std::printf("    naive:  auto m + by-value param     copies=%zu\n", Matrix::copies);
    }
    {
        Matrix::copies = 0;
        std::vector<Matrix> mats;
        mats.reserve(K);                                         // one allocation
        for (std::size_t i = 0; i < K; ++i) mats.emplace_back(128, 128);        // construct in place
        double acc = 0;
        for (const auto& m : mats) acc += frob_by_ref(m);
        DoNotOptimize(acc);
        std::printf("    fixed:  const auto& + const& param   copies=%zu\n", Matrix::copies);
    }
    std::printf("    (Matrix move ops are noexcept; without that, vector growth would COPY every element)\n");
}

// ---- 5. Allocation in the hot loop -----------------------------------------------------------
static Matrix matmul_alloc(const Matrix& a, const Matrix& b) {   // allocates a fresh result each call
    Matrix c(a.rows, b.cols);
    std::fill(c.data.begin(), c.data.end(), 0.0);
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t k = 0; k < a.cols; ++k) {
            const double aik = a(i, k);
            for (std::size_t j = 0; j < b.cols; ++j) c(i, j) += aik * b(k, j);
        }
    return c;
}
static void matmul_into(const Matrix& a, const Matrix& b, Matrix& c) {   // caller owns the buffer
    std::fill(c.data.begin(), c.data.end(), 0.0);
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t k = 0; k < a.cols; ++k) {
            const double aik = a(i, k);
            for (std::size_t j = 0; j < b.cols; ++j) c(i, j) += aik * b(k, j);
        }
}

static void section_allocation() {
    std::printf("== 5. allocation in the hot loop: 300 x (32x64 @ 64x64) ==\n");
    Matrix X(32, 64), W(64, 64), H(32, 64);
    double t1 = bench_ms([&] {
        double acc = 0;
        for (int it = 0; it < 300; ++it) { Matrix h = matmul_alloc(X, W); acc += h(0, 0); }
        DoNotOptimize(acc);
    });
    double t2 = bench_ms([&] {
        double acc = 0;
        for (int it = 0; it < 300; ++it) { matmul_into(X, W, H); acc += H(0, 0); }
        DoNotOptimize(acc);
    });
    report("matmul() returning new Matrix", t1);
    report("matmul_into(preallocated)", t2);
    std::printf("    (small matrices make malloc/free visible; profile with `sample ./ex_demo 3` to see it)\n");
}

// ---- 6. dot product: scalar / unrolled / NEON ------------------------------------------------
static double dot_scalar(const double* a, const double* b, std::size_t n) {
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i];       // FP reduction: NOT auto-vectorized
    return s;                                                    // (reordering changes rounding)
}

static double dot_unrolled4(const double* a, const double* b, std::size_t n) {
    double s0 = 0, s1 = 0, s2 = 0, s3 = 0;                       // 4 independent chains hide FMA latency
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        s0 += a[i] * b[i];
        s1 += a[i + 1] * b[i + 1];
        s2 += a[i + 2] * b[i + 2];
        s3 += a[i + 3] * b[i + 3];
    }
    for (; i < n; ++i) s0 += a[i] * b[i];
    return (s0 + s1) + (s2 + s3);
}

#if defined(__ARM_NEON)
static double dot_neon(const double* a, const double* b, std::size_t n) {
    float64x2_t acc0 = vdupq_n_f64(0.0);                         // two lanes of double
    float64x2_t acc1 = vdupq_n_f64(0.0);
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        acc0 = vfmaq_f64(acc0, vld1q_f64(a + i), vld1q_f64(b + i));           // acc0 += a*b lane-wise
        acc1 = vfmaq_f64(acc1, vld1q_f64(a + i + 2), vld1q_f64(b + i + 2));
    }
    double s = vaddvq_f64(vaddq_f64(acc0, acc1));                // horizontal sum of 2 lanes
    for (; i < n; ++i) s += a[i] * b[i];                         // scalar tail: NEVER read past n
    return s;
}
#endif

static void section_dot() {
    std::printf("== 6. dot product, n = 1<<20 (fits in L2) ==\n");
    const std::size_t n = std::size_t{1} << 20;
    std::vector<double> a(n), b(n);
    for (std::size_t i = 0; i < n; ++i) { a[i] = 1.0 + static_cast<double>(i % 7); b[i] = 0.5 - static_cast<double>(i % 5) * 0.1; }
    const double flops = 2.0 * static_cast<double>(n);
    double r0 = 0, r1 = 0;
    report("scalar (1 accumulator)", bench_ms([&] { r0 = dot_scalar(a.data(), b.data(), n); DoNotOptimize(r0); }), flops);
    report("4 accumulators", bench_ms([&] { r1 = dot_unrolled4(a.data(), b.data(), n); DoNotOptimize(r1); }), flops);
#if defined(__ARM_NEON)
    double r2 = 0;
    report("NEON vfmaq_f64", bench_ms([&] { r2 = dot_neon(a.data(), b.data(), n); DoNotOptimize(r2); }), flops);
    std::printf("    results: %.6f %.6f %.6f  (agree to ~1e-15 relative, not bit-identical)\n", r0, r1, r2);
#else
    std::printf("    results: %.6f %.6f  (no NEON on this target; see <immintrin.h> for x86)\n", r0, r1);
#endif
}

// ---- 7. Parallelism -----------------------------------------------------------------------
// A minimal parallel_for over [0, n) with std::thread. Spawning threads costs ~10-50 us each,
// so only worth it for big chunks of work; a thread POOL reuses them (exercise / library).
static void parallel_for(std::size_t n, unsigned nthreads,
                         const std::function<void(std::size_t, std::size_t)>& body) {
    std::vector<std::thread> pool;
    const std::size_t chunk = (n + nthreads - 1) / nthreads;
    for (unsigned t = 0; t < nthreads; ++t) {
        const std::size_t lo = t * chunk, hi = std::min(n, lo + chunk);
        if (lo < hi) pool.emplace_back(body, lo, hi);
    }
    for (auto& th : pool) th.join();                             // unjoined joinable thread -> std::terminate
}

struct alignas(64) PaddedSum { double v = 0.0; };                // one per cache line: no false sharing

static void section_parallel() {
    std::printf("== 7. parallelism: kinetic energy of 8M particles ==\n");
    const std::size_t N = 8'000'000;
    std::vector<double> m(N, 1.0), vx(N), vy(N);
    for (std::size_t i = 0; i < N; ++i) { vx[i] = static_cast<double>(i % 11) * 0.1; vy[i] = 0.3; }
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    std::printf("    hardware_concurrency = %u\n", hw);

    double serial = 0;
    report("serial", bench_ms([&] {
        double s = 0;
        for (std::size_t i = 0; i < N; ++i) s += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i]);
        serial = s; DoNotOptimize(serial);
    }));

    // std::thread with per-thread PADDED partial sums, combined at the end.
    const unsigned T = std::min(hw, 8u);
    double par = 0;
    report("std::thread, padded partials", bench_ms([&] {
        std::vector<PaddedSum> partial(T);
        parallel_for(N, T, [&](std::size_t lo, std::size_t hi) {
            // Which thread am I? Derive from lo; each thread writes ONLY its own padded slot.
            const std::size_t t = lo / ((N + T - 1) / T);
            double s = 0;                                        // local accumulator: register, not memory
            for (std::size_t i = lo; i < hi; ++i) s += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i]);
            partial[t].v = s;
        });
        double s = 0; for (const auto& p : partial) s += p.v;
        par = s; DoNotOptimize(par);
    }));

    // What NOT to do: a shared std::atomic<double> hammered from every iteration. Correct (no UB),
    // but every add bounces one cache line between cores. Shown on a smaller N so it finishes.
    const std::size_t Nsmall = N / 16;
    report("atomic<double> per-iteration (N/16)", bench_ms([&] {
        std::atomic<double> total{0.0};
        parallel_for(Nsmall, T, [&](std::size_t lo, std::size_t hi) {
            for (std::size_t i = lo; i < hi; ++i) {
                const double e = 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i]);
                double cur = total.load(std::memory_order_relaxed);   // CAS loop: atomic<double> has no fetch_add in C++17
                while (!total.compare_exchange_weak(cur, cur + e, std::memory_order_relaxed)) {}
            }
        });
        DoNotOptimize(total);
    }, 2));

    // std::mutex: for anything larger than a word. lock_guard is RAII (unlocks on throw too).
    {
        std::mutex mu;
        std::vector<std::size_t> log;
        parallel_for(64, T, [&](std::size_t lo, std::size_t) {
            std::lock_guard<std::mutex> lock(mu);
            log.push_back(lo);
        });
        std::printf("    mutex-protected log has %zu entries\n", log.size());
    }

#ifdef _OPENMP
    double omp_sum = 0;
    report("OpenMP parallel for reduction(+:)", bench_ms([&] {
        double s = 0;
        // OpenMP loop variable: signed. reduction gives each thread a private s, summed at the end.
#pragma omp parallel for reduction(+ : s) schedule(static)
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(N); ++i)
            s += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i]);
        omp_sum = s; DoNotOptimize(omp_sum);
    }));
    std::printf("    omp_get_max_threads() = %d, |omp - serial| = %.2e\n", omp_get_max_threads(),
                std::fabs(omp_sum - serial));
#else
    std::printf("    (OpenMP not enabled: see compile line at top; the pragma is skipped, code stays serial)\n");
#endif
    std::printf("    |threads - serial| = %.2e  (different summation order -> tolerance, not ==)\n",
                std::fabs(par - serial));
    std::printf("    Amdahl: if 90%% of the time is parallel, 8 cores give at most 1/(0.1+0.9/8) = %.2fx\n",
                1.0 / (0.1 + 0.9 / 8.0));
}

int main() {
    std::printf("== 1. Timer RAII + bench harness ==\n");
    {
        Timer t("allocate+fill 8M doubles (Timer dtor prints)");
        std::vector<double> v(8'000'000, 1.5);
        DoNotOptimize(v.data());
    }
    {
        // The dead-code trap: without DoNotOptimize / using the result, -O2 deletes this loop.
        double ms_deleted = bench_ms([] {
            double s = 0; for (int i = 0; i < 10'000'000; ++i) s += i * 0.5;
            (void)s;                                             // unused -> whole loop removed
        });
        double ms_kept = bench_ms([] {
            double s = 0; for (int i = 0; i < 10'000'000; ++i) s += i * 0.5;
            DoNotOptimize(s);
        });
        report("10M-add loop, result discarded", ms_deleted);
        report("10M-add loop, DoNotOptimize", ms_kept);
    }

    section_matmul();
    section_aos_soa();
    section_copies();
    section_allocation();
    section_dot();
    section_parallel();

    std::printf("== 8. checklist ==\n"
                "    measure -> algorithm -> memory layout -> allocation -> vectorize -> parallelize -> BLAS\n");
    return 0;
}
