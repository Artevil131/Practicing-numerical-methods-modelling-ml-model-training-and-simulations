// Chapter 20 — C++ for numerics and HPC: the ecosystem pieces you can run on one machine.
//
// Compile (baseline, no external libraries; zero warnings on any platform):
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp
// With Accelerate BLAS (macOS) — enables the cblas_dgemm section:
//   c++ -Wall -Wextra -std=c++17 -O2 -DUSE_ACCELERATE -framework Accelerate -o ex_demo example.cpp
// Linux equivalent (OpenBLAS):
//   c++ -Wall -Wextra -std=c++17 -O2 -DUSE_OPENBLAS -o ex_demo example.cpp -lopenblas
// With OpenMP (macOS: brew install libomp) — parallel SpMV and the irreproducible-reduction demo:
//   c++ -Wall -Wextra -std=c++17 -O2 -Xpreprocessor -fopenmp -I$(brew --prefix libomp)/include \
//       -L$(brew --prefix libomp)/lib -lomp -o ex_demo example.cpp
// Run:      ./ex_demo
// Afterwards, in Python:  python3 -c "import numpy as np; a = np.load('ex20_demo.npy'); print(a.shape, a.dtype, a.sum())"
//
// Sections:
//   1  .npy writer + reader; header bytes verified against the format spec (lesson §13.1)
//   2  CSR sparse matrix: 2-D Laplacian, SpMV vs dense mat-vec, bytes moved (lesson §5)
//   3  Conjugate Gradient on the Laplacian: relative residual, iteration count (lesson §5.1)
//   4  Compensated summation: naive vs Kahan vs Neumaier vs pairwise, with a case Kahan fails (lesson §6.2)
//   5  Parallel-reduction reproducibility (OpenMP, guarded) (lesson §6.1)
//   6  cblas_dgemm vs ikj (USE_ACCELERATE / USE_OPENBLAS, guarded) (lesson §1.2)
//   7  NEON Vec2d wrapper: 4-accumulator dot (guarded by __ARM_NEON) (lesson §7)
//   8  Roofline arithmetic: AI and attainable GFLOP/s for the kernels above (lesson §8)
//   9  VTK legacy writer for a 2-D field (lesson §13.3)
//   10 bfloat16 round-trip: software conversion with round-to-nearest-even (lesson §6.3)
//
// Files written to the current directory: ex20_demo.npy, ex20_demo.vtk (small; delete when done).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(USE_ACCELERATE)
#define ACCELERATE_NEW_LAPACK
#include <Accelerate/Accelerate.h>
#define HAVE_CBLAS 1
#elif defined(USE_OPENBLAS)
#include <cblas.h>
#define HAVE_CBLAS 1
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

// ---------------------------------------------------------------------------------------
// Tiny timing helpers (../19 has the full harness).
// ---------------------------------------------------------------------------------------
template <class T> inline void DoNotOptimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }
template <class F> double min_ms(F&& f, int reps = 5) {
    using clock = std::chrono::steady_clock;
    f();
    double best = std::numeric_limits<double>::infinity();
    for (int r = 0; r < reps; ++r) {
        auto t0 = clock::now(); f();
        best = std::min(best, std::chrono::duration<double, std::milli>(clock::now() - t0).count());
    }
    return best;
}

// =======================================================================================
// 1. .npy writer and reader.
// =======================================================================================
namespace npy {

// Header dict must be ASCII, end with '\n', and (10 + header_len) % 64 == 0 (format spec v1.0).
std::string make_header(const std::vector<std::size_t>& shape, const char* descr, bool fortran_order) {
    std::string dict = "{'descr': '" + std::string(descr) + "', 'fortran_order': " + (fortran_order ? "True" : "False") + ", 'shape': (";
    for (std::size_t i = 0; i < shape.size(); ++i) dict += std::to_string(shape[i]) + (shape.size() == 1 || i + 1 < shape.size() ? ", " : "");
    if (shape.size() == 1) dict.pop_back();                     // "(3, " -> "(3," for 1-D tuples
    dict += "), }";
    const std::size_t unpadded = 10 + dict.size() + 1;          // magic(6) + version(2) + len(2) + dict + '\n'
    const std::size_t pad = (64 - unpadded % 64) % 64;
    dict.append(pad, ' ');
    dict += '\n';
    return dict;
}

void save(const std::string& path, const double* data, const std::vector<std::size_t>& shape) {
    static_assert(sizeof(double) == 8, "IEEE-754 binary64 expected");
    const std::uint16_t probe = 1;
    unsigned char pb[2]; std::memcpy(pb, &probe, 2);
    if (pb[0] != 1) throw std::runtime_error("big-endian host: byte-swap before writing '<f8'");
    const std::string header = make_header(shape, "<f8", false);
    if (header.size() > 65535) throw std::runtime_error("header too long for .npy v1.0");
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    const unsigned char magic[8] = {0x93, 'N', 'U', 'M', 'P', 'Y', 1, 0};
    f.write(reinterpret_cast<const char*>(magic), 8);
    const auto hlen = static_cast<std::uint16_t>(header.size());
    const unsigned char lenb[2] = {static_cast<unsigned char>(hlen & 0xFF), static_cast<unsigned char>(hlen >> 8)};
    f.write(reinterpret_cast<const char*>(lenb), 2);
    f.write(header.data(), static_cast<std::streamsize>(header.size()));
    std::size_t n = 1; for (auto s : shape) n *= s;
    f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n * sizeof(double)));
}

struct Array { std::vector<std::size_t> shape; std::vector<double> data; };

// Reader for '<f8', C order, v1.0/v2.0. A real one handles more dtypes (exercise 20.6).
Array load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    unsigned char pre[10];
    f.read(reinterpret_cast<char*>(pre), 10);
    if (!f || pre[0] != 0x93 || std::memcmp(pre + 1, "NUMPY", 5) != 0) throw std::runtime_error("not a .npy file");
    const int major = pre[6];
    std::size_t hlen = 0;
    if (major == 1) hlen = static_cast<std::size_t>(pre[8]) | (static_cast<std::size_t>(pre[9]) << 8);
    else if (major == 2) { unsigned char more[2]; f.read(reinterpret_cast<char*>(more), 2);
        hlen = static_cast<std::size_t>(pre[8]) | (static_cast<std::size_t>(pre[9]) << 8) | (static_cast<std::size_t>(more[0]) << 16) | (static_cast<std::size_t>(more[1]) << 24); }
    else throw std::runtime_error("unsupported .npy version");
    std::string header(hlen, '\0');
    f.read(header.data(), static_cast<std::streamsize>(hlen));
    if (header.find("'descr': '<f8'") == std::string::npos) throw std::runtime_error("only '<f8' supported here");
    if (header.find("'fortran_order': False") == std::string::npos) throw std::runtime_error("fortran_order=True not supported here");
    const auto sp = header.find("'shape': (");
    if (sp == std::string::npos) throw std::runtime_error("no shape");
    Array a;
    std::size_t pos = sp + 10;
    while (pos < header.size() && header[pos] != ')') {
        while (pos < header.size() && (header[pos] == ' ' || header[pos] == ',')) ++pos;
        if (header[pos] == ')') break;
        std::size_t end = pos;
        while (end < header.size() && header[end] >= '0' && header[end] <= '9') ++end;
        if (end == pos) throw std::runtime_error("bad shape");
        a.shape.push_back(std::stoul(header.substr(pos, end - pos)));
        pos = end;
    }
    std::size_t n = 1; for (auto s : a.shape) n *= s;
    a.data.resize(n);
    f.read(reinterpret_cast<char*>(a.data.data()), static_cast<std::streamsize>(n * sizeof(double)));
    if (!f) throw std::runtime_error("truncated data");
    return a;
}

}  // namespace npy

// =======================================================================================
// 2. CSR and the 2-D Laplacian.
// =======================================================================================
struct CSR {
    std::size_t n = 0;
    std::vector<std::uint32_t> row_ptr, col_idx;       // uint32: half the index bytes of size_t
    std::vector<double> val;
    std::size_t nnz() const { return val.size(); }
};

// 5-point Laplacian of an m×m grid, Dirichlet boundaries (rows for boundary points omitted: n = m*m interior points).
CSR laplacian_2d(std::size_t m) {
    CSR A; A.n = m * m;
    A.row_ptr.reserve(A.n + 1); A.row_ptr.push_back(0);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            auto push = [&](std::size_t ii, std::size_t jj, double v) {
                A.col_idx.push_back(static_cast<std::uint32_t>(ii * m + jj)); A.val.push_back(v);
            };
            if (i > 0) push(i - 1, j, -1.0);                // column order ascending within the row
            if (j > 0) push(i, j - 1, -1.0);
            push(i, j, 4.0);
            if (j + 1 < m) push(i, j + 1, -1.0);
            if (i + 1 < m) push(i + 1, j, -1.0);
            A.row_ptr.push_back(static_cast<std::uint32_t>(A.val.size()));
        }
    return A;
}

void spmv(const CSR& A, const double* x, double* y) {
    const auto n = static_cast<std::ptrdiff_t>(A.n);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        double s = 0.0;
        const auto ui = static_cast<std::size_t>(i);
        for (std::uint32_t k = A.row_ptr[ui]; k < A.row_ptr[ui + 1]; ++k) s += A.val[k] * x[A.col_idx[k]];
        y[ui] = s;
    }
}

std::vector<double> to_dense(const CSR& A) {
    std::vector<double> D(A.n * A.n, 0.0);
    for (std::size_t i = 0; i < A.n; ++i)
        for (std::uint32_t k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) D[i * A.n + A.col_idx[k]] = A.val[k];
    return D;
}
void dense_matvec(const std::vector<double>& D, std::size_t n, const double* x, double* y) {
    for (std::size_t i = 0; i < n; ++i) { double s = 0; const double* row = D.data() + i * n; for (std::size_t j = 0; j < n; ++j) s += row[j] * x[j]; y[i] = s; }
}

// =======================================================================================
// 3. Conjugate Gradient.
// =======================================================================================
static double dot(const std::vector<double>& a, const std::vector<double>& b) { return std::inner_product(a.begin(), a.end(), b.begin(), 0.0); }

int cg(const CSR& A, const std::vector<double>& b, std::vector<double>& x, double tol, int max_it, double* final_rel_res) {
    const std::size_t n = A.n;
    std::vector<double> r(n), p(n), Ap(n);
    spmv(A, x.data(), Ap.data());
    for (std::size_t i = 0; i < n; ++i) { r[i] = b[i] - Ap[i]; p[i] = r[i]; }
    double rr = dot(r, r);
    const double bnorm = std::sqrt(dot(b, b));
    int it = 0;
    for (; it < max_it; ++it) {
        if (std::sqrt(rr) <= tol * bnorm) break;
        spmv(A, p.data(), Ap.data());
        const double alpha = rr / dot(p, Ap);
        for (std::size_t i = 0; i < n; ++i) { x[i] += alpha * p[i]; r[i] -= alpha * Ap[i]; }
        const double rr_new = dot(r, r);
        const double beta = rr_new / rr;
        for (std::size_t i = 0; i < n; ++i) p[i] = r[i] + beta * p[i];
        rr = rr_new;
    }
    *final_rel_res = std::sqrt(rr) / bnorm;
    return it;
}

// =======================================================================================
// 4. Compensated summation.
// =======================================================================================
double sum_naive(const double* x, std::size_t n) { double s = 0; for (std::size_t i = 0; i < n; ++i) s += x[i]; return s; }
double sum_kahan(const double* x, std::size_t n) {
    double s = 0.0, c = 0.0;
    for (std::size_t i = 0; i < n; ++i) { const double y = x[i] - c; const double t = s + y; c = (t - s) - y; s = t; }
    return s;
}
double sum_neumaier(const double* x, std::size_t n) {
    double s = 0.0, c = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = s + x[i];
        c += std::fabs(s) >= std::fabs(x[i]) ? (s - t) + x[i] : (x[i] - t) + s;
        s = t;
    }
    return s + c;
}
double sum_pairwise(const double* x, std::size_t n) {
    if (n <= 128) return sum_naive(x, n);
    return sum_pairwise(x, n / 2) + sum_pairwise(x + n / 2, n - n / 2);
}

// =======================================================================================
// 6. gemm reference (ikj) for the BLAS comparison.
// =======================================================================================
void gemm_ikj(const double* A, const double* B, double* C, std::size_t n) {
    std::fill(C, C + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t k = 0; k < n; ++k) {
            const double a = A[i * n + k]; const double* b = B + k * n; double* c = C + i * n;
            for (std::size_t j = 0; j < n; ++j) c[j] += a * b[j];
        }
}

// =======================================================================================
// 7. NEON Vec2d.
// =======================================================================================
#if defined(__ARM_NEON)
struct Vec2d {
    float64x2_t v;
    Vec2d() : v(vdupq_n_f64(0.0)) {}
    Vec2d(float64x2_t x) : v(x) {}
    static Vec2d load(const double* p) { return vld1q_f64(p); }
    friend Vec2d operator+(Vec2d a, Vec2d b) { return vaddq_f64(a.v, b.v); }
    friend Vec2d fma(Vec2d a, Vec2d b, Vec2d c) { return vfmaq_f64(c.v, a.v, b.v); }   // c + a*b
    double hsum() const { return vaddvq_f64(v); }
};
double dot_neon(const double* a, const double* b, std::size_t n) {
    Vec2d s0, s1, s2, s3; std::size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        s0 = fma(Vec2d::load(a + i),     Vec2d::load(b + i),     s0);
        s1 = fma(Vec2d::load(a + i + 2), Vec2d::load(b + i + 2), s1);
        s2 = fma(Vec2d::load(a + i + 4), Vec2d::load(b + i + 4), s2);
        s3 = fma(Vec2d::load(a + i + 6), Vec2d::load(b + i + 6), s3);
    }
    double s = ((s0 + s1) + (s2 + s3)).hsum();
    for (; i < n; ++i) s += a[i] * b[i];
    return s;
}
#endif
double dot_scalar(const double* a, const double* b, std::size_t n) { double s = 0; for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i]; return s; }

// =======================================================================================
// 9. VTK legacy STRUCTURED_POINTS writer (ASCII).
// =======================================================================================
void write_vtk(const std::string& path, const std::vector<double>& field, std::size_t nx, std::size_t ny, double dx, double dy, const char* name) {
    std::ofstream f(path);
    f << "# vtk DataFile Version 3.0\n" << name << " field\nASCII\nDATASET STRUCTURED_POINTS\n"
      << "DIMENSIONS " << nx << ' ' << ny << " 1\nORIGIN 0 0 0\nSPACING " << dx << ' ' << dy << " 1\n"
      << "POINT_DATA " << nx * ny << "\nSCALARS " << name << " double 1\nLOOKUP_TABLE default\n";
    for (std::size_t j = 0; j < ny; ++j) {                  // x fastest
        for (std::size_t i = 0; i < nx; ++i) f << field[j * nx + i] << (i + 1 < nx ? ' ' : '\n');
    }
}

// =======================================================================================
// 10. Software bfloat16 (round to nearest even).
// =======================================================================================
struct bf16 { std::uint16_t bits; };
bf16 to_bf16(float f) {
    std::uint32_t u; std::memcpy(&u, &f, 4);
    if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu)) return {static_cast<std::uint16_t>((u >> 16) | 0x40u)};   // NaN stays NaN
    const std::uint32_t lsb = (u >> 16) & 1u;
    u += 0x7FFFu + lsb;
    return {static_cast<std::uint16_t>(u >> 16)};
}
float from_bf16(bf16 b) { const std::uint32_t u = static_cast<std::uint32_t>(b.bits) << 16; float f; std::memcpy(&f, &u, 4); return f; }

// =======================================================================================
int main() {
    std::printf("== 1. .npy writer/reader ==\n");
    {
        std::vector<double> a(3 * 4);
        for (std::size_t i = 0; i < a.size(); ++i) a[i] = static_cast<double>(i) * 0.5;
        npy::save("ex20_demo.npy", a.data(), {3, 4});
        // Verify the header bytes against the spec.
        std::ifstream f("ex20_demo.npy", std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::size_t hlen = bytes[8] | (static_cast<std::size_t>(bytes[9]) << 8);
        std::printf("magic %02x %c%c%c%c%c  version %d.%d  HEADER_LEN %zu  (10+HEADER_LEN)%%64 = %zu  last header byte = 0x%02x ('\\n')\n",
                    bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], hlen, (10 + hlen) % 64, bytes[10 + hlen - 1]);
        std::string header(bytes.begin() + 10, bytes.begin() + 10 + static_cast<std::ptrdiff_t>(hlen));
        std::printf("header: %s\n", header.substr(0, header.find_last_not_of(" \n") + 1).c_str());
        std::printf("file size %zu = 10 + %zu + 12*8\n", bytes.size(), hlen);
        npy::Array back = npy::load("ex20_demo.npy");
        std::printf("read back: shape (%zu, %zu), data equal: %s  -> python3 -c \"import numpy as np; print(np.load('ex20_demo.npy'))\"\n",
                    back.shape[0], back.shape[1], back.data == a ? "yes" : "NO");
    }

    std::printf("\n== 2. CSR SpMV vs dense mat-vec (2-D Laplacian) ==\n");
    {
        const std::size_t m = 64;                            // n = 4096: dense is 128 MB... no: 4096^2 * 8 = 134 MB. Use it once.
        CSR A = laplacian_2d(m);
        const std::size_t n = A.n;
        std::vector<double> x(n), y1(n), y2(n);
        std::mt19937_64 rng(1); std::uniform_real_distribution<double> U(-1, 1);
        for (auto& v : x) v = U(rng);
        std::vector<double> D = to_dense(A);
        const double t_sparse = min_ms([&] { spmv(A, x.data(), y1.data()); DoNotOptimize(y1.data()); });
        const double t_dense = min_ms([&] { dense_matvec(D, n, x.data(), y2.data()); DoNotOptimize(y2.data()); }, 3);
        double maxdiff = 0; for (std::size_t i = 0; i < n; ++i) maxdiff = std::max(maxdiff, std::fabs(y1[i] - y2[i]));
        const double sparse_bytes = static_cast<double>(A.nnz()) * (8 + 4) + 2.0 * static_cast<double>(n) * 8;   // val + uint32 idx + x + y (x mostly cached)
        std::printf("n=%zu nnz=%zu (%.2f per row)  dense %.1f MB\n", n, A.nnz(), static_cast<double>(A.nnz()) / static_cast<double>(n), static_cast<double>(n * n * 8) / 1e6);
        std::printf("spmv  %.4f ms  %.2f GFLOP/s  %.1f GB/s   dense %.3f ms  %.2f GFLOP/s  %.1f GB/s   max|diff| = %.1e   speedup %.0fx\n",
                    t_sparse, 2.0 * static_cast<double>(A.nnz()) / (t_sparse * 1e-3) / 1e9, sparse_bytes / (t_sparse * 1e-3) / 1e9,
                    t_dense, 2.0 * static_cast<double>(n * n) / (t_dense * 1e-3) / 1e9, static_cast<double>(n * n * 8) / (t_dense * 1e-3) / 1e9,
                    maxdiff, t_dense / t_sparse);
#ifdef _OPENMP
        std::printf("(OpenMP: spmv ran with %d threads)\n", omp_get_max_threads());
#endif
    }

    std::printf("\n== 3. Conjugate Gradient on the 128x128 Laplacian ==\n");
    {
        const std::size_t m = 128;
        CSR A = laplacian_2d(m);
        std::vector<double> x_true(A.n), b(A.n), x(A.n, 0.0);
        for (std::size_t i = 0; i < m; ++i) for (std::size_t j = 0; j < m; ++j)
            x_true[i * m + j] = std::sin(M_PI * static_cast<double>(i + 1) / static_cast<double>(m + 1)) * std::sin(M_PI * static_cast<double>(j + 1) / static_cast<double>(m + 1));
        spmv(A, x_true.data(), b.data());
        double res = 0;
        const auto t0 = std::chrono::steady_clock::now();
        const int its = cg(A, b, x, 1e-10, 10000, &res);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        double err = 0; for (std::size_t i = 0; i < A.n; ++i) err = std::max(err, std::fabs(x[i] - x_true[i]));
        std::printf("n=%zu  CG iterations %d (~O(m)=%zu expected, kappa ~ m^2)  rel residual %.2e  max|x - x_true| %.2e  %.1f ms\n", A.n, its, m, res, err, ms);
    }

    std::printf("\n== 4. Compensated summation ==\n");
    {
        const std::size_t n = 10'000'000;
        std::vector<double> x(n);
        for (std::size_t i = 0; i < n; ++i) x[i] = 1.0 + static_cast<double>(i % 7) * 0x1p-20;   // exact sum representable
        double exact = 0; for (std::size_t i = 0; i < 7; ++i) exact += static_cast<double>(i) * 0x1p-20;   // per block of 7
        exact = static_cast<double>(n) + exact * static_cast<double>(n / 7) + [&] { double t = 0; for (std::size_t i = 0; i < n % 7; ++i) t += static_cast<double>(i) * 0x1p-20; return t; }();
        struct M { const char* name; double (*f)(const double*, std::size_t); } methods[] = {
            {"naive", sum_naive}, {"kahan", sum_kahan}, {"neumaier", sum_neumaier}, {"pairwise", sum_pairwise}};
        std::printf("%-10s %22s %12s %10s\n", "method", "sum", "abs error", "ms");
        for (auto& mth : methods) {
            double s = 0;
            const double ms = min_ms([&] { s = mth.f(x.data(), n); DoNotOptimize(s); }, 3);
            std::printf("%-10s %22.12f %12.3e %10.3f\n", mth.name, s, std::fabs(s - exact), ms);
        }
        const double bad[3] = {1e16, 1.0, -1e16};              // Kahan's known failure: |x| > |s| ordering
        std::printf("[1e16, 1, -1e16]: naive %g  kahan %g  neumaier %g  (exact 1)\n", sum_naive(bad, 3), sum_kahan(bad, 3), sum_neumaier(bad, 3));
    }

    std::printf("\n== 5. Parallel reduction reproducibility ==\n");
    {
        const std::size_t n = 1'000'000;
        std::vector<double> x(n);
        std::mt19937_64 rng(7); std::uniform_real_distribution<double> U(-1, 1);
        for (auto& v : x) v = U(rng) * std::exp(U(rng) * 20);   // wide dynamic range: rounding is order-dependent
#ifdef _OPENMP
        for (int threads : {1, 2, 4, 8}) {
            omp_set_num_threads(threads);
            double s = 0;
#pragma omp parallel for reduction(+ : s) schedule(static)
            for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(n); ++i) s += x[static_cast<std::size_t>(i)];
            std::printf("omp reduction, %d threads: %a\n", threads, s);
        }
        std::printf("(last bits differ across thread counts: floating-point addition is not associative)\n");
#else
        const double serial = sum_naive(x.data(), n);
        const double pair = sum_pairwise(x.data(), n);
        std::printf("serial %a\npairwise %a\n(different rounding: same numbers, different order; build with OpenMP to see it vary with thread count)\n", serial, pair);
#endif
    }

    std::printf("\n== 6. cblas_dgemm vs ikj ==\n");
    {
        const std::size_t n = 512;
        std::vector<double> A(n * n), B(n * n), C1(n * n), C2(n * n);
        std::mt19937_64 rng(3); std::uniform_real_distribution<double> U(-1, 1);
        for (auto& v : A) v = U(rng);
        for (auto& v : B) v = U(rng);
        const double flops = 2.0 * static_cast<double>(n) * static_cast<double>(n) * static_cast<double>(n);
        const double t_ikj = min_ms([&] { gemm_ikj(A.data(), B.data(), C1.data(), n); DoNotOptimize(C1.data()); }, 3);
        std::printf("n=%zu  ikj    %8.2f ms  %6.1f GFLOP/s\n", n, t_ikj, flops / (t_ikj * 1e-3) / 1e9);
#ifdef HAVE_CBLAS
        const auto ni = static_cast<int>(n);
        const double t_blas = min_ms([&] {
            cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, ni, ni, ni, 1.0, A.data(), ni, B.data(), ni, 0.0, C2.data(), ni);
            DoNotOptimize(C2.data());
        }, 5);
        double num = 0, den = 0;
        for (std::size_t i = 0; i < n * n; ++i) { num += (C1[i] - C2[i]) * (C1[i] - C2[i]); den += C1[i] * C1[i]; }
        std::printf("n=%zu  dgemm  %8.2f ms  %6.1f GFLOP/s   rel err vs ikj %.1e   speedup %.0fx\n", n, t_blas, flops / (t_blas * 1e-3) / 1e9, std::sqrt(num / den), t_ikj / t_blas);
        // C = A * B^T without transposing B in memory: CblasTrans with ldb = n (B is stored n×n).
        std::vector<double> C3(n * n);
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, ni, ni, ni, 1.0, A.data(), ni, B.data(), ni, 0.0, C3.data(), ni);
        double check = 0; for (std::size_t k = 0; k < n; ++k) check += A[0 * n + k] * B[5 * n + k];   // (A B^T)(0,5) = sum_k A(0,k) B(5,k)
        std::printf("A*B^T via CblasTrans: C(0,5) = %.12g  direct = %.12g\n", C3[5], check);
#else
        std::printf("(build with -DUSE_ACCELERATE -framework Accelerate to run cblas_dgemm here; expect 50-150 GFLOP/s single-threaded)\n");
#endif
    }

    std::printf("\n== 7. NEON Vec2d dot with 4 accumulators ==\n");
    {
        const std::size_t n = 1 << 20;
        std::vector<double> a(n), b(n);
        std::mt19937_64 rng(5); std::uniform_real_distribution<double> U(-1, 1);
        for (std::size_t i = 0; i < n; ++i) { a[i] = U(rng); b[i] = U(rng); }
        double s1 = 0;
        const double t_scalar = min_ms([&] { s1 = dot_scalar(a.data(), b.data(), n); DoNotOptimize(s1); });
        const double bytes = 16.0 * static_cast<double>(n);
        std::printf("scalar dot: %.4f ms  %.1f GB/s  %.2f GFLOP/s  (AI = 2 flops / 16 bytes = 0.125: memory-bound)\n", t_scalar, bytes / (t_scalar * 1e-3) / 1e9, 2.0 * static_cast<double>(n) / (t_scalar * 1e-3) / 1e9);
#if defined(__ARM_NEON)
        double s2 = 0;
        const double t_neon = min_ms([&] { s2 = dot_neon(a.data(), b.data(), n); DoNotOptimize(s2); });
        std::printf("neon   dot: %.4f ms  %.1f GB/s  %.2f GFLOP/s  |diff| = %.1e  (differs in rounding: different summation order)\n",
                    t_neon, bytes / (t_neon * 1e-3) / 1e9, 2.0 * static_cast<double>(n) / (t_neon * 1e-3) / 1e9, std::fabs(s1 - s2));
#else
        std::printf("(no NEON on this platform; the x86 version uses __m256d / _mm256_fmadd_pd)\n");
#endif
    }

    std::printf("\n== 8. Roofline arithmetic (per P-core; edit the two machine numbers) ==\n");
    {
        const double peak_gflops = 4 * 2 * 2 * 4.0;             // 4 FMA pipes x 2 lanes x 2 flops x 4.0 GHz = 64 GFLOP/s double
        const double bw_gbs = 100.0;                            // ~single-core DRAM bandwidth on M-series
        const double ridge = peak_gflops / bw_gbs;
        std::printf("peak %.0f GFLOP/s, bandwidth %.0f GB/s, ridge point %.2f flop/byte\n", peak_gflops, bw_gbs, ridge);
        struct K { const char* name; double flops, bytes; } ks[] = {
            {"axpy y=a*x+y", 2, 24}, {"dot", 2, 16}, {"spmv (CSR,u32)", 2, 20}, {"5-pt stencil", 6, 16},
            {"gemv", 2, 8}, {"Adam update", 12, 40}, {"softmax (4 passes)", 30, 64}, {"softmax (fused)", 30, 16},
            {"gemm n=512 blocked", 2.0 * 512, 8 * 512 / 32.0}, {"N-body pair (SoA)", 20, 24}};
        std::printf("%-22s %8s %10s %14s %s\n", "kernel", "AI", "bound", "attainable", "");
        for (auto& k : ks) {
            const double ai = k.flops / k.bytes;
            const double attain = std::min(peak_gflops, bw_gbs * ai);
            std::printf("%-22s %8.3f %10s %10.1f GF/s\n", k.name, ai, ai < ridge ? "memory" : "compute", attain);
        }
    }

    std::printf("\n== 9. VTK legacy writer ==\n");
    {
        const std::size_t nx = 40, ny = 20;
        std::vector<double> field(nx * ny);
        for (std::size_t j = 0; j < ny; ++j) for (std::size_t i = 0; i < nx; ++i)
            field[j * nx + i] = std::sin(2 * M_PI * static_cast<double>(i) / nx) * std::cos(M_PI * static_cast<double>(j) / ny);
        write_vtk("ex20_demo.vtk", field, nx, ny, 0.05, 0.05, "Ez");
        std::ifstream f("ex20_demo.vtk"); std::string line; int k = 0;
        std::printf("ex20_demo.vtk header:\n");
        while (k++ < 10 && std::getline(f, line)) std::printf("  %s\n", line.substr(0, 60).c_str());
        std::printf("(open in ParaView: File > Open, Apply, color by Ez)\n");
    }

    std::printf("\n== 10. bfloat16 round trip ==\n");
    {
        const float vals[] = {1.0f, 3.14159265f, 65504.0f, 1e-3f, 123456.0f, 3.0e38f};
        std::printf("%14s %14s %12s\n", "float", "bf16->float", "rel err");
        for (float v : vals) {
            const float r = from_bf16(to_bf16(v));
            std::printf("%14.7g %14.7g %12.2e\n", static_cast<double>(v), static_cast<double>(r), static_cast<double>(std::fabs(r - v) / v));
        }
        std::printf("(bf16: 8 exponent bits = float range, 7 mantissa bits = ~3 significant digits; float16 would overflow at 65504)\n");
        // Accumulation in low precision loses everything: sum 1.0 4096 times in bf16 vs float.
        bf16 acc = to_bf16(0.0f);
        for (int i = 0; i < 4096; ++i) acc = to_bf16(from_bf16(acc) + 1.0f);
        std::printf("sum of 4096 ones accumulated in bf16 = %g  (stalls at 256: 1.0 < ulp(256)=2)  -> always accumulate in fp32\n", static_cast<double>(from_bf16(acc)));
    }

    return 0;
}
