# Chapter 20 — C++ for Numerics and HPC

## What you'll be able to do after this chapter

- Call BLAS/LAPACK correctly from C++ (Accelerate on macOS, OpenBLAS/MKL on Linux): `cblas_dgemm`, `dgesv_`, `dsyev_`, with the Fortran column-major/pass-by-pointer/workspace-query conventions that cause most bugs.
- Use Eigen productively (dense, sparse, `Map` over your own memory, expression templates) and avoid its `auto`/aliasing traps; know what `std::mdspan` gives you and when FFTW/pocketfft beat a hand-written FFT.
- Store and multiply sparse matrices in CSR, implement Conjugate Gradient and sketch GMRES, and choose iterative vs direct solvers for your PDE grids.
- Reason about floating point at HPC scale: why parallel sums are irreproducible, how Kahan/pairwise summation fix precision, what float16/bfloat16 buy ML and where `std::float16_t` stands.
- Place a kernel on the roofline with real M-series numbers, vectorize with a NEON `Vec<double,2>` wrapper, parallelize with OpenMP tasks/`simd`/`collapse`, exchange halos with MPI, and say what a GPU would add and when it pays off.
- Write `.npy` and VTK legacy files that NumPy/matplotlib/ParaView load, checkpoint reproducibly, and apply all of it to two case studies: making your ML stack and your N-body code fast, with expected speedups.

## Why this matters for ML / numerics / sims

Your P01–P05 stack multiplies matrices with the triple loop you wrote yourself. It trains a tiny transformer in an hour where PyTorch takes a minute. Your P07 N-body handles 5,000 bodies at interactive rates; the papers you read handle 10⁸. The gap is not cleverness. It is the ecosystem — BLAS written by people who know the microarchitecture, sparse formats that skip the zeros, SIMD lanes and cores you aren't using, and a memory system whose bandwidth, not FLOP rate, is the actual limit for most of your kernels. This chapter is the map of that ecosystem from a C++ programmer's seat: what each piece is, the exact call and build line on macOS arm64 (and Linux), the failure mode that bites, and the number you should expect. The two case studies at the end are the plan for turning your learning projects into code you would show an HPC engineer.

Prerequisites: `../12_performance` (memory hierarchy, SoA, NEON basics, OpenMP `parallel for`, first BLAS call), `../18_software_design_and_library_architecture` (Storage + View), `../19_professional_tooling_and_quality` (benchmarking, profiling). The C course's FDTD/heat (C P14), N-body (C P15) and this course's P06–P10 are the code you will apply this to.

---

## 1. BLAS and LAPACK

**BLAS** (Basic Linear Algebra Subprograms, 1979–1990) is a *specification* of ~150 routines in three levels: Level 1 vector–vector (`daxpy`: y ← αx + y, `ddot`, `dnrm2`), Level 2 matrix–vector (`dgemv`, `dtrsv`), Level 3 matrix–matrix (`dgemm`, `dsyrk`, `dtrsm`). **LAPACK** (1992–) builds factorizations and solvers on top: `dgesv` (LU solve), `dpotrf` (Cholesky), `dgeqrf` (QR), `dsyev` (symmetric eigen), `dgesvd`/`dgesdd` (SVD), `dgels` (least squares). Both are specified in Fortran; every fast implementation (Accelerate, OpenBLAS, MKL, BLIS, cuBLAS) exposes the same names. The naming: `d`/`s`/`c`/`z` = double/float/complex/double-complex; `ge`/`sy`/`po`/`tr`/`gb` = general/symmetric/positive-definite/triangular/general-banded; then the operation.

Why call them instead of your own `gemm`: a tuned `dgemm` is 50–100× your naive triple loop and 5–10× your best blocked NEON attempt, because it uses register-blocked micro-kernels, multi-level cache blocking with packed panels, all SIMD units, and (Accelerate) Apple's AMX matrix coprocessor. Your `../12_performance` `ikj` kernel at 512×512 gets ~10–20 GFLOP/s; Accelerate `cblas_dgemm` single-threaded gets ~100–150 GFLOP/s in double on M-series, and multi-threaded well over a TFLOP/s in single precision.

### 1.1 Linking

| Platform | Header | Link | Notes |
|---|---|---|---|
| macOS | `#include <Accelerate/Accelerate.h>` | `-framework Accelerate` | Defines `ACCELERATE_NEW_LAPACK` yourself (`-DACCELERATE_NEW_LAPACK -DACCELERATE_LAPACK_ILP64` optional) on macOS ≥ 13.3 to get the LAPACK 3.9.1 interfaces without deprecation warnings; without it you get the old CLAPACK prototypes (`__CLPK_integer` = `int`), which still work. |
| Linux (OpenBLAS) | `#include <cblas.h>`, `#include <lapacke.h>` | `-lopenblas` (or `-lblas -llapack -llapacke`) | `apt install libopenblas-dev liblapacke-dev`. `OPENBLAS_NUM_THREADS=1` to control threading. LAPACKE gives a C interface with a `LAPACK_ROW_MAJOR` option (it transposes internally). |
| Linux (MKL) | `#include <mkl.h>` | `-qmkl` (icpx) or the [link-line advisor](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onemkl-link-line-advisor.html) string; typically `-lmkl_intel_lp64 -lmkl_sequential -lmkl_core` | Fastest on Intel; fine on AMD since 2020. `MKL_NUM_THREADS`. |
| Any | Eigen with `-DEIGEN_USE_BLAS -DEIGEN_USE_LAPACKE` | as above | Eigen routes large products to the BLAS you link. |
| CMake | `find_package(BLAS REQUIRED)`, `find_package(LAPACK REQUIRED)`; `target_link_libraries(x BLAS::BLAS LAPACK::LAPACK)`; `set(BLA_VENDOR Apple)` / `OpenBLAS` / `Intel10_64lp` | | |

### 1.2 `cblas_dgemm` — the one call to get right

C = α·op(A)·op(B) + β·C. The CBLAS interface adds a row/column-major flag so you don't have to think in Fortran:

```cpp
#include <Accelerate/Accelerate.h>          // Linux: <cblas.h>
// A: M×K, B: K×N, C: M×N, all row-major contiguous (lda = K, ldb = N, ldc = N)
cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
            M, N, K,
            1.0, A, K,        // alpha, A, lda  (lda = number of columns of A's storage = stride between rows)
                 B, N,        // B, ldb
            0.0, C, N);       // beta, C, ldc  (beta = 0: C is overwritten; beta = 1: accumulate)
```

The leading dimension `lda` is the *stride between consecutive rows* (row-major) — the allocated width, not the logical width — which is how you multiply a sub-block of a bigger matrix without copying: pass `A + r0*lda + c0` with the parent's `lda`. `CblasTrans` multiplies by Aᵀ without transposing in memory (`A·Bᵀ` for attention scores `Q·Kᵀ`: `cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, S, S, D, 1.0, Q, D, K, D, 0.0, scores, S)` — with `ldb = D` because `K` is stored `S×D`). Integer arguments are `int` in the LP64 interface — check `n ≤ INT_MAX` (§ `../19` warnings).

Single precision `cblas_sgemm` is 2× faster (half the bytes; twice the SIMD lanes); mixed-precision `cblas_gemm` variants don't exist in BLAS — that's what Eigen/oneDNN/cuBLASLt add. Batched gemm (`cblas_dgemm_batch` in MKL/OpenBLAS, `appleblas_dgemm_batch`… not portable) — for attention heads, loop over heads and call `dgemm` per head, or reshape so one big gemm covers all heads.

### 1.3 LAPACK's Fortran calling convention — the pitfalls

LAPACK has no CBLAS-style wrapper in Accelerate (LAPACKE exists on Linux). You call the Fortran symbol directly, and *every* rule below is a bug you will otherwise write:

1. **Trailing underscore**: `dgesv_`, `dsyev_` (gfortran/Accelerate name mangling). Declared in `<Accelerate/Accelerate.h>` (`clapack.h` inside) and `<lapacke.h>`/`<lapack.h>`.
2. **Everything by pointer**, including scalars: `int n = 3; dgesv_(&n, …)`. Constants need an lvalue: `int one = 1;`.
3. **Column-major**: A(i,j) is at `a[i + j*lda]`. Your row-major matrix `M` *is* the column-major matrix `Mᵀ` — so to solve `A·x = b` with row-major `A`, either transpose first, or solve `Aᵀ·y = b` and… no: pass row-major `A` as column-major and you've handed LAPACK `Aᵀ`. For symmetric `A` (`dsyev`, `dpotrf`) it doesn't matter. For `dgesv`, transpose (or use `dgetrf` + `dgetrs` with `trans = 'T'`).
4. **Character arguments** are `char*` (`"U"`, `"N"`, `"V"`) — in the new interface with `ACCELERATE_NEW_LAPACK` some prototypes take a trailing hidden length; the header handles it.
5. **`info` output**: 0 = success; `< 0` = argument `-info` is invalid (your bug); `> 0` = numerical failure (`dgesv`: `U(info,info)` is exactly zero — singular; `dsyev`: didn't converge; `dpotrf`: not positive definite at column `info`). Always check.
6. **Workspace query**: routines with `work, lwork` want you to call once with `lwork = -1` — they write the optimal size into `work[0]` — then allocate and call again. Hard-coding `lwork = 3n` works but is slow.
7. **In-place**: `dgesv_` *overwrites* `A` with its LU factors and `b` with `x`. Copy if you need the original.
8. **Integer type**: `int` (LP64) by default; `ACCELERATE_LAPACK_ILP64` / MKL ILP64 make it `long`. Mixing = garbage `info`.

```cpp
// Solve A x = b, A is n×n ROW-major in your code. dgesv_ wants column-major, so hand it A^T ... i.e. transpose first.
std::vector<double> Acm(n * n);
for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) Acm[i + j * n] = A[i * n + j];    // row-major -> column-major
std::vector<int> ipiv(n);
int nrhs = 1, lda = n, ldb = n, info = 0;
dgesv_(&n, &nrhs, Acm.data(), &lda, ipiv.data(), b.data(), &ldb, &info);              // b now holds x
if (info < 0) throw std::invalid_argument("dgesv: bad argument " + std::to_string(-info));
if (info > 0) throw std::runtime_error("dgesv: singular, U(" + std::to_string(info) + ") == 0");

// Symmetric eigendecomposition: A = V diag(w) V^T ; A overwritten with eigenvectors (columns, column-major!)
char jobz = 'V', uplo = 'U'; int lwork = -1; double wkopt; std::vector<double> w(n);
dsyev_(&jobz, &uplo, &n, Acm.data(), &lda, w.data(), &wkopt, &lwork, &info);           // workspace query
lwork = static_cast<int>(wkopt); std::vector<double> work(lwork);
dsyev_(&jobz, &uplo, &n, Acm.data(), &lda, w.data(), work.data(), &lwork, &info);      // real call; w ascending
// eigenvector k is Acm[0 + k*n .. n-1 + k*n]
```

Python equivalent: `np.linalg.solve` calls `dgesv`; `np.linalg.eigh` calls `dsyevd`; `np.dot`/`@` calls `dgemm`. NumPy handles the row/column-major conversion for you (`np.asfortranarray`); in C++ you do it.

---

## 2. Eigen

Eigen (header-only, MPL2, `brew install eigen`, `-I$(brew --prefix eigen)/include/eigen3`; Linux `apt install libeigen3-dev`, `-I/usr/include/eigen3`) is what most C++ numerics code actually uses for dense linear algebra below the "call MKL" size. It is the reference implementation of expression templates (`../18_software_design_and_library_architecture` §9).

```cpp
#include <Eigen/Dense>
#include <Eigen/Sparse>
using Eigen::MatrixXd; using Eigen::VectorXd;          // X = dynamic size, d = double; Matrix3d, Vector4f = fixed
MatrixXd A = MatrixXd::Random(4, 4);
VectorXd b = VectorXd::Ones(4);
VectorXd x = A.colPivHouseholderQr().solve(b);          // or A.lu().solve(b), A.ldlt().solve(b) for SPD, A.fullPivLu()
MatrixXd C = A * A.transpose() + MatrixXd::Identity(4, 4);      // one fused loop; no temporaries for + and Identity
Eigen::SelfAdjointEigenSolver<MatrixXd> es(C);                  // eigenvalues().  eigenvectors()
std::cout << C.norm() << " " << C.determinant() << " " << (A.array() * A.array()).sum() << "\n";   // .array(): elementwise
```

| Feature | What to know |
|---|---|
| **Storage order** | Column-major by default (Fortran/LAPACK compatible). `Matrix<double, Dynamic, Dynamic, RowMajor>` if you must match NumPy/your `Matrix`. Mixing orders in one expression is fine but slow paths appear. |
| **`Map`** | `Eigen::Map<MatrixXd> M(ptr, rows, cols)` wraps *your* memory as an Eigen matrix — zero copy. `Map<Matrix<double,Dynamic,Dynamic,RowMajor>, Unaligned, OuterStride<>> M(ptr, r, c, OuterStride<>(lda))` for strided views. This is how you use Eigen on your `Tensor`'s storage without converting. `Map<const VectorXd>` for read-only. |
| **Fixed sizes** | `Matrix3d`, `Vector3d`: stack-allocated, fully unrolled, no heap — use for `Vec3` physics; `Matrix<double, 6, 6>` for small stencils. Sizes > 16×16 fixed become slow to compile; use dynamic. |
| **Expression templates** | `a*b + c` builds a type; assignment evaluates in one pass. Products are the exception: `A*B` materializes a temporary unless you write `C.noalias() = A * B;` (tells Eigen `C` doesn't alias `A`/`B`). |
| **Aliasing** | `A = A.transpose();` is **wrong** (reads while writing) — use `A.transposeInPlace()` or `A = A.transpose().eval()`. `A = A * A` is safe (products assume aliasing); `A.noalias() = A * A` is wrong. |
| **The `auto` trap** | `auto e = A * B + C;` — `e` is an *expression type* holding references. `MatrixXd r = e;` evaluates it — fine — but if `A` was a temporary or is modified before `e` is used, you read garbage. Rule: never `auto` an Eigen expression you keep; `auto` the result of `.eval()` or write the type. Also `auto v = M.row(0);` is a *view* (`Block`), not a copy; modifying `M` modifies `v`. |
| **Passing to functions** | `void f(const Eigen::Ref<const MatrixXd>& m)` accepts `MatrixXd`, `Map`, blocks, without copying (`Ref` is Eigen's `span`); `template<class Derived> void f(const Eigen::MatrixBase<Derived>& m)` accepts any expression. Taking `const MatrixXd&` forces evaluation of expressions into a temporary — usually what you want for correctness, not for speed. |
| **Alignment** | Fixed-size vectorizable types (`Vector4d`, `Matrix2d`) are 16-byte aligned; before C++17 storing them in `std::vector` needed `Eigen::aligned_allocator`. With C++17 aligned `new`, `std::vector<Vector4d>` is fine. Structs containing them need `EIGEN_MAKE_ALIGNED_OPERATOR_NEW` only pre-C++17. |
| **Debug vs release** | Without `-DNDEBUG` Eigen asserts shapes on every operation (good) and is 10× slower (expected). `-DEIGEN_NO_DEBUG` alone turns them off. `-O2` minimum; `-mcpu=native` enables NEON/AVX paths; check `Eigen::SimdInstructionSetsInUse()`. |
| **BLAS fallback** | `-DEIGEN_USE_BLAS -DEIGEN_USE_LAPACKE` + link: large products/solves go to Accelerate/MKL; small ones stay in Eigen (which is faster for n < ~64 because there's no call overhead). |
| **Threading** | Eigen's own products are multithreaded only with OpenMP (`-fopenmp`); otherwise single-threaded — one more reason for the BLAS fallback at large sizes. |
| **Sparse** | `SparseMatrix<double> S(n, n); std::vector<Triplet<double>> t; t.emplace_back(i, j, v); S.setFromTriplets(t.begin(), t.end());` (CSC by default; `RowMajor` option for CSR). Solvers: `SimplicialLDLT` (SPD direct), `SparseLU`, `SparseQR`, `ConjugateGradient<SparseMatrix<double>, Lower|Upper>` (SPD iterative), `BiCGSTAB` (general iterative) — all `.compute(S)` then `.solve(b)`; check `.info() == Eigen::Success`. |
| **Compile time** | `#include <Eigen/Dense>` is ~100k preprocessed lines and 1–2 s per TU. Keep it out of public headers (pimpl, `../18` §5) or use `<Eigen/Core>` + forward declarations. |

When Eigen beats hand-written code: anything under ~200×200 dense (no call overhead, fully inlined, fixed-size unrolling), all the decompositions (you will not write a better Householder QR), elementwise expressions (`(x.array().exp() / x.array().exp().sum())` — fused). When it doesn't: huge gemm (call BLAS), kernels with unusual access patterns (stencils — write the loop), anything where compile time matters more than run time.

---

## 3. `std::mdspan` and layouts

`std::mdspan` (C++23, `<mdspan>`, P0009) is the standard non-owning multidimensional view — `../18` §6.1's `MatrixView` done properly: `mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy>`.

```cpp
#include <mdspan>                                   // C++23. Check __cpp_lib_mdspan; libc++ ≥ 18, GCC ≥ 14 (libstdc++ 14 partial). 
                                                    // C++17 fallback: #include <experimental/mdspan> from github.com/kokkos/mdspan (same API, namespace std::experimental)
std::vector<double> buf(3 * 4);
std::mdspan<double, std::dextents<std::size_t, 2>> m(buf.data(), 3, 4);          // dynamic 3×4, layout_right (row-major)
m[1, 2] = 5.0;                                                                     // C++23 multi-arg operator[]; C++17 impl: m(1, 2)
std::mdspan<double, std::extents<std::size_t, 3, std::dynamic_extent>> m2(buf.data(), 4);  // rank-2, first extent static
std::mdspan<double, std::dextents<std::size_t, 2>, std::layout_left> fortran(buf.data(), 3, 4);   // column-major: pass to LAPACK
std::layout_stride::mapping<std::dextents<std::size_t, 2>> map({3, 2}, {4, 2});   // every other column of the 3×4
std::mdspan<double, std::dextents<std::size_t, 2>, std::layout_stride> sub(buf.data(), map);
// submdspan (C++26, P2630; in kokkos impl): std::submdspan(m, std::pair{1, 3}, std::full_extent)
```

Why it matters: a kernel `template <class Layout> void stencil(std::mdspan<const double, dextents<size_t,2>, Layout> in, ...)` is written *once* and instantiates for row-major, column-major and strided views, with the compiler knowing static extents when you give them (a `3×3` stencil kernel with `extents<size_t,3,3>` fully unrolls). **Kokkos** (Sandia's performance-portability library — the origin of mdspan) uses exactly this to run the same `parallel_for` over `View`s on CPU/OpenMP/CUDA/HIP with the layout chosen per backend (`LayoutRight` on CPU for cache lines, `LayoutLeft` on GPU for coalescing). If you ever target both, Kokkos is the C++ answer; mdspan is the piece you can use today.

---

## 4. FFT libraries: FFTW and pocketfft

You wrote a radix-2 FFT in P06. Production FFTs are 5–20× faster (mixed radix, SIMD, cache-aware plans, real-input symmetry):

| | FFTW 3 | pocketfft | Accelerate vDSP | KISS FFT |
|---|---|---|---|---|
| License | **GPL** (commercial license from MIT for closed source) | BSD-3 | Apple | BSD |
| Form | C library, `brew install fftw`, `-lfftw3 -lfftw3_threads` | Header-only C++ (`pocketfft_hdronly.h`, used by NumPy/SciPy since 2019) | `<Accelerate/Accelerate.h>` | Single C file |
| Sizes | Any `n`; fastest for `2^a 3^b 5^c 7^d` | Any `n`, same preference | Powers of 2 (radix 2/3/5 in newer APIs) | Any |
| Speed | Fastest generally (with `FFTW_MEASURE` planning) | ~FFTW ESTIMATE speed | Fastest on Apple for pow2 | Slow |
| Threads | Yes | Yes (`nthreads` arg) | Internal | No |

```cpp
// FFTW: plan once (expensive: FFTW_MEASURE runs benchmarks), execute many times.
#include <fftw3.h>
int n = 1024;
double* in = fftw_alloc_real(n);                         // SIMD-aligned
fftw_complex* out = fftw_alloc_complex(n / 2 + 1);       // real-to-complex: only n/2+1 outputs (Hermitian symmetry)
fftw_plan p = fftw_plan_dft_r2c_1d(n, in, out, FFTW_MEASURE);
/* fill in[] */ fftw_execute(p);                          // out[k] = Σ in[j] e^{-2πi jk/n}, UNNORMALIZED (like np.fft.rfft)
fftw_execute_dft_r2c(p, other_in, other_out);            // reuse the plan on other arrays of the same size/alignment
fftw_destroy_plan(p); fftw_free(in); fftw_free(out);
// Multi-d: fftw_plan_dft_r2c_2d(ny, nx, ...) — row-major, LAST dimension is the one halved. Wisdom: fftw_export_wisdom_to_filename.

// pocketfft: no plan object exposed; caches plans internally.
#include "pocketfft_hdronly.h"
pocketfft::shape_t shape{static_cast<std::size_t>(n)}; pocketfft::stride_t sin{sizeof(double)}, sout{sizeof(std::complex<double>)};
pocketfft::r2c(shape, sin, sout, pocketfft::shape_t{0}, pocketfft::FORWARD, in.data(), out.data(), 1.0 /*fct*/, 1 /*threads*/);
```

Rules that carry over from P06: the transform is unnormalized in the forward direction (divide by `n` after the inverse, or pass `fct = 1.0/n`); real input → `n/2+1` complex outputs; for a PIC field solve (P09) you FFT the charge density, divide by `-k²`, inverse FFT — with `k = 2π·[0, 1, …, n/2, -(n/2-1), …, -1]/L` and the `k=0` mode set to zero. FFTW's plans are not thread-safe to *create* concurrently (guard with a mutex) but `fftw_execute` is.

---

## 5. Sparse matrices: CSR/CSC, SpMV, iterative solvers

A 2-D Laplacian on a 1000×1000 grid is a 10⁶×10⁶ matrix with 5 non-zeros per row: 5·10⁶ values (40 MB) instead of 10¹² (8 TB). Sparse formats store only the non-zeros:

```
Dense 4×4:               CSR (row-major compressed):
[4 1 0 0]                values   = [4 1 1 4 1 1 4 1 1 4]       (nnz = 10)
[1 4 1 0]                col_idx  = [0 1 0 1 2 1 2 3 2 3]       (column of each value)
[0 1 4 1]                row_ptr  = [0 2 5 8 10]                (row i's entries are values[row_ptr[i] .. row_ptr[i+1])
[0 0 1 4]
CSC = the same with rows/columns swapped (what Eigen and MATLAB use by default; LAPACK-adjacent).
COO = (row, col, value) triplets: easy to build (assembly), slow to multiply → convert to CSR once.
```

```cpp
struct CSR {
    std::size_t n = 0;                                // square for simplicity
    std::vector<std::size_t> row_ptr, col_idx;
    std::vector<double> val;
};
// y = A x. Memory-bound: per non-zero, 8 (val) + 8 (col_idx as size_t; use uint32 to halve it) + 8 (x[col], random access) bytes for 2 flops.
void spmv(const CSR& A, const double* x, double* y) {
    for (std::size_t i = 0; i < A.n; ++i) {
        double s = 0.0;
        for (std::size_t k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) s += A.val[k] * x[A.col_idx[k]];
        y[i] = s;                                     // each row independent -> #pragma omp parallel for on i (schedule(dynamic,64) if rows vary)
    }
}
```

Arithmetic intensity of SpMV is ~0.1 flop/byte — it is *always* memory-bound (§8): on a 100 GB/s machine you get ~10 GFLOP/s no matter what. Optimizations are all about bytes: `uint32_t` indices, sorting rows by length, blocked formats (BSR for stencils with 3×3 blocks), and reusing `x` from cache (row ordering — Cuthill–McKee — puts neighbours close). ELL/SELL-C-σ formats are the GPU variants (uniform row lengths for SIMD lanes).

### 5.1 Conjugate Gradient (SPD systems: Poisson, heat implicit step, Laplacian smoothing)

CG solves `A x = b` for symmetric positive-definite `A` using only `spmv`, dot products and axpys — O(nnz) per iteration, converges in O(√κ) iterations (κ = condition number; for the 2-D Laplacian on an n×n grid κ ~ n², so ~n iterations — a 1000×1000 grid needs ~1000–3000 iterations without a preconditioner, ~50–100 with multigrid).

```cpp
// Hestenes–Stiefel 1952. Returns iterations used; x is the initial guess in, solution out.
int cg(const CSR& A, const std::vector<double>& b, std::vector<double>& x, double tol = 1e-10, int max_it = 10000) {
    const std::size_t n = A.n;
    std::vector<double> r(n), p(n), Ap(n);
    spmv(A, x.data(), Ap.data());
    for (std::size_t i = 0; i < n; ++i) { r[i] = b[i] - Ap[i]; p[i] = r[i]; }
    double rr = dot(r, r); const double bnorm = std::sqrt(dot(b, b));
    for (int it = 0; it < max_it; ++it) {
        if (std::sqrt(rr) <= tol * bnorm) return it;              // relative residual — never absolute
        spmv(A, p.data(), Ap.data());
        const double alpha = rr / dot(p, Ap);                      // step length along p
        for (std::size_t i = 0; i < n; ++i) { x[i] += alpha * p[i]; r[i] -= alpha * Ap[i]; }
        const double rr_new = dot(r, r);
        const double beta = rr_new / rr;                           // Gram–Schmidt against previous direction
        for (std::size_t i = 0; i < n; ++i) p[i] = r[i] + beta * p[i];
        rr = rr_new;
    }
    return max_it;                                                 // caller checks: did not converge
}
```

Preconditioning (`M⁻¹ A x = M⁻¹ b` with `M ≈ A` cheap to invert) is where the real speed is: Jacobi (`M = diag(A)`, trivial, 2× fewer iterations), incomplete Cholesky (IC(0)), and multigrid (optimal O(n) for elliptic PDEs — the algorithm you eventually want for P08's Poisson solves). In PCG you keep `z = M⁻¹ r` and use `dot(r, z)` in place of `dot(r, r)`.

### 5.2 GMRES outline (non-symmetric systems: convection–diffusion, linearized Navier–Stokes)

CG needs symmetry. **GMRES** (Saad–Schultz 1986) minimizes the residual over the Krylov space `span{r₀, A r₀, A² r₀, …}` built with **Arnoldi** iteration (modified Gram–Schmidt: each new `A·vⱼ` is orthogonalized against all previous `v`s → `H` is upper Hessenberg, `A·Vₖ = Vₖ₊₁·Hₖ`). The least-squares problem `min ‖β e₁ − Hₖ y‖` is solved incrementally with Givens rotations (O(k) per iteration), and `x = x₀ + Vₖ y`. Memory grows as O(n·k), so **restarted GMRES(m)** (m = 20–50) throws the basis away every `m` steps. Preconditioning is essential (ILU(0) is the workhorse). BiCGSTAB is the cheaper cousin (fixed memory, less robust). Eigen has both; for learning, write GMRES(m) once (exercise 20.7) — Arnoldi is 15 lines, Givens is 10.

### 5.3 Direct vs iterative

| | Direct (LU/Cholesky, sparse LU/Cholesky: SuiteSparse UMFPACK/CHOLMOD, MUMPS, PARDISO) | Iterative (CG, GMRES, BiCGSTAB + preconditioner; multigrid) |
|---|---|---|
| Cost | O(n³) dense; sparse fill-in makes 2-D grids O(n^1.5), 3-D O(n²) | O(nnz · iterations); with multigrid O(n) |
| Multiple right-hand sides | Factor once, solve each in O(n²)/O(nnz) — huge win (time stepping with a fixed matrix) | Each solve restarts (but previous solution is a great initial guess) |
| Robustness | Always works (with pivoting) | Needs SPD (CG) or a good preconditioner; can stall |
| Memory | Fill-in can be 10–100× nnz | O(nnz) |
| When | Small/medium systems, 2-D grids up to ~10⁶, many RHS | Large 3-D grids, matrix-free (`spmv` as a function — you never build `A`) |

---

## 6. Floating point at HPC scale

`../13_modern_cpp_and_idioms` and the C course covered IEEE-754 basics. At scale three issues dominate.

### 6.1 Parallel reductions are not reproducible

Floating-point addition is not associative: `(a + b) + c ≠ a + (b + c)` in general. A serial `for` sums in one fixed order; `#pragma omp parallel for reduction(+:s)` sums per-thread partials in an order that depends on thread count *and* on scheduling — so **the same program prints a different last digit on different runs**. `cblas_dgemm` multithreaded has the same property; GPU atomics are worse (order is essentially random). Consequences: a training run is not bit-reproducible; a simulation "diverges" between two runs that should be identical; tests with `==` fail intermittently.

Fixes, in increasing cost: (1) accept it and compare with tolerances ~ `n·ε·Σ|xᵢ|`; (2) **fixed-order reduction** — each thread reduces a static chunk (`schedule(static)`), partials are stored in an array and summed serially in index order → bitwise reproducible for a fixed thread count; (3) **pairwise/tree reduction** with a fixed tree (reproducible *and* more accurate); (4) reproducible BLAS (ReproBLAS, cuBLAS's deterministic modes, `torch.use_deterministic_algorithms(True)`), which use long accumulators or pre-rounding; (5) `-ffp-contract=off` so FMA fusion doesn't differ between compilers/platforms (clang on arm64 fuses `a*b+c` into an FMA by default; GCC x86-64 doesn't without `-mfma`; the results differ in the last bit).

### 6.2 Compensated summation

Naive summation of `n` terms has error bound `(n−1)·ε·Σ|xᵢ|` — summing 10⁸ floats of similar magnitude loses ~4 decimal digits (of 7). Three fixes:

```cpp
// Kahan (1965): track the rounding error of each addition and feed it back. Error bound ~ 2ε·Σ|x| independent of n.
double kahan(const double* x, std::size_t n) {
    double s = 0.0, c = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double y = x[i] - c;          // compensated input
        const double t = s + y;             // big + small: low bits of y are lost...
        c = (t - s) - y;                    // ...and recovered here: (t - s) is what was actually added; minus y = the lost part
        s = t;
    }
    return s;                               // -ffast-math BREAKS this (it "simplifies" (t - s) - y to 0)
}
// Neumaier (1974): Kahan fails when |x[i]| > |s|; this variant handles both orderings. What Python's math.fsum family and Julia use.
double neumaier(const double* x, std::size_t n) {
    double s = 0.0, c = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = s + x[i];
        c += std::fabs(s) >= std::fabs(x[i]) ? (s - t) + x[i] : (x[i] - t) + s;
        s = t;
    }
    return s + c;
}
// Pairwise (recursive halving): error O(log n · ε). What NumPy's np.sum does (in blocks of 8 with unrolling). Vectorizes and parallelizes.
double pairwise(const double* x, std::size_t n) {
    if (n <= 128) { double s = 0; for (std::size_t i = 0; i < n; ++i) s += x[i]; return s; }
    return pairwise(x, n / 2) + pairwise(x + n / 2, n - n / 2);
}
```

Kahan costs 4 flops per element instead of 1 but is still memory-bound (the loads dominate), so it is nearly free on large arrays — except it serializes (each step depends on the last) and blocks SIMD; the fix is Kahan with 4–8 independent accumulators, or pairwise summation. `std::accumulate` and `std::reduce` are naive; `std::reduce` with a parallel policy is also unordered (§6.1). Use `long double` (80-bit on x86-64, but **64-bit — same as double — on arm64 macOS**) is not a portable fix. For dot products, the same idea gives compensated dot (Ogita–Rump–Oishi 2005) using FMA to get the exact product error: `p = a*b; e = std::fma(a, b, -p);`.

Where it matters for you: loss accumulation over a batch (mean of 10⁶ terms), energy totals in N-body (Σ over 10⁵ pairs to check conservation to 1e-10 — naive summation noise is 1e-11·√n, which hides the real drift), long time integrations (`t += dt` 10⁷ times — accumulate `t = i * dt` instead), variance computations (use Welford's online algorithm, never `E[x²] − E[x]²`).

### 6.3 Mixed precision: float16, bfloat16 and why ML uses them

| Format | Bits (sign/exp/mantissa) | ε (unit roundoff) | Max | Min normal | Use |
|---|---|---|---|---|---|
| `double` (binary64) | 1/11/52 | 1.1e-16 | 1.8e308 | 2.2e-308 | Simulation, solvers, accumulation |
| `float` (binary32) | 1/8/23 | 6.0e-8 | 3.4e38 | 1.2e-38 | Most HPC kernels, ML master weights |
| `float16` (binary16, "half") | 1/5/10 | 4.9e-4 | 65504 | 6.1e-5 | ML activations/weights; graphics; needs *loss scaling* (gradients underflow below 6e-8 subnormal) |
| `bfloat16` (brain float) | 1/8/7 | 3.9e-3 | 3.4e38 | 1.2e-38 | ML training (same range as float: no scaling needed; 3 significant digits is enough for SGD noise) |
| `TF32` (NVIDIA tensor cores) | 1/8/10 | 4.9e-4 | 3.4e38 | | Internal matmul format: float inputs truncated to 10-bit mantissa, float accumulate |
| `FP8` (E4M3 / E5M2) | 1/4/3, 1/5/2 | 0.0625, 0.125 | 448, 57344 | | Inference and (with scaling) training on H100+ |

Why ML uses them: (1) **bandwidth** — training is memory-bound (§8); half the bytes = up to 2× throughput and 2× the batch/model that fits; (2) **tensor cores/AMX/NEON** do 2–16× more half-precision FLOPs per cycle than fp32; (3) SGD *tolerates* noise — gradient noise from minibatching is far larger than 1e-3 rounding. The recipe (**mixed precision training**, Micikevicius et al. 2018): weights stored in fp32 ("master copy"), forward/backward in fp16/bf16, matmul *accumulation* in fp32 (never accumulate in half — after ~2000 terms you've lost everything), loss multiplied by a scale factor `S` (e.g. 2¹⁶) before backward so small gradients don't underflow in fp16, then gradients divided by `S` before the fp32 update; bf16 skips the scaling. Simulations mostly cannot do this: an N-body position in float16 has 3 significant digits.

**C++ status**: C++23 `<stdfloat>` (P1467) defines `std::float16_t`, `std::bfloat16_t`, `std::float32_t`, `std::float64_t`, `std::float128_t` as *optional* extended floating-point types with literals `1.0f16`, `1.0bf16`. GCC 13+ provides them on x86-64/aarch64; clang/libc++ as of 2026 provides `std::float16_t`/`std::bfloat16_t` on arm64 but the library support (`<cmath>` overloads, `std::format`) lags — check `__STDCPP_FLOAT16_T__`. Pre-C++23 and on Apple clang you use the compiler extensions: `_Float16` (arithmetic type, IEEE binary16, arm64 has native `fcvt`/`fadd` for it, x86 needs AVX512-FP16 or emulates), `__fp16` (ARM storage-only type: promotes to `float` in arithmetic — what you want for a weight buffer), `__bf16` (clang ≥ 15 arithmetic on arm64 with `+bf16`). A software `bfloat16` is 10 lines: `uint16_t bits = (float_bits + 0x7FFF + ((float_bits >> 16) & 1)) >> 16;` (round-to-nearest-even), back is `float_bits = uint32_t(bits) << 16`. Exercise 20.8 builds a mixed-precision `Linear`.

---

## 7. Vectorization in C++

`../12_performance` §9–10 covered auto-vectorization reports and one NEON intrinsic loop. The C++ ways to write portable SIMD:

| Approach | Status | Portability | Notes |
|---|---|---|---|
| Auto-vectorization | Always | Yes | Fails on reductions without `-ffast-math`/`#pragma omp simd reduction`, on aliasing, on early exits; check `-Rpass-missed=loop-vectorize` |
| `#pragma omp simd` / `#pragma clang loop vectorize(assume_safety)` | Always | Yes | Tells the compiler "no dependencies" — UB if you lie; `reduction(+:s)` allows reassociation for one loop |
| **`std::experimental::simd`** (Parallelism TS v2, `<experimental/simd>`) | libstdc++ ≥ 11: yes (`std::experimental::native_simd<double>`); libc++: partial since 17, behind `-fexperimental-library`, not on Apple clang shipping libc++ as of 2026 | Yes | `native_simd<double> a(ptr, element_aligned); a = a * b + c; a.copy_to(out, element_aligned);` `simd_size_v` lanes; `reduce(a)`; `where(mask, a) = b`. **C++26 `std::simd`** (P1928, `<simd>`) standardizes it with a cleaner API (`std::simd<double>`, `std::simd_mask`). |
| **xsimd** (header-only, BSD) | Mature | NEON/SSE/AVX/AVX512/SVE | `xsimd::batch<double, xsimd::neon64> b = xsimd::load_unaligned(p); xsimd::fma(a,b,c); xsimd::reduce_add(b)`; used by xtensor, Pythran |
| **Google Highway** | Mature | Everything incl. RVV, SVE (scalable), WASM | Dynamic dispatch built in (`HWY_DYNAMIC_DISPATCH`) — compile once, pick AVX2/AVX-512/NEON at run time; the most production-grade |
| **EVE** (C++20) | Mature | x86/ARM | Algorithms-first (`eve::algo::transform_inplace`) |
| Raw intrinsics `<arm_neon.h>` / `<immintrin.h>` | Always | No | Maximum control; write a thin `Vec<T,N>` wrapper (below) and keep the platform code in one file |

A minimal NEON wrapper that reads like math and compiles to the same instructions:

```cpp
#if defined(__ARM_NEON)
#include <arm_neon.h>
struct Vec2d {                                              // Vec<double, 2>: one NEON register
    float64x2_t v;
    static constexpr std::size_t size() { return 2; }
    Vec2d() : v(vdupq_n_f64(0.0)) {}
    explicit Vec2d(double s) : v(vdupq_n_f64(s)) {}
    Vec2d(float64x2_t x) : v(x) {}
    static Vec2d load(const double* p) { return vld1q_f64(p); }           // unaligned OK on AArch64
    void store(double* p) const { vst1q_f64(p, v); }
    friend Vec2d operator+(Vec2d a, Vec2d b) { return vaddq_f64(a.v, b.v); }
    friend Vec2d operator-(Vec2d a, Vec2d b) { return vsubq_f64(a.v, b.v); }
    friend Vec2d operator*(Vec2d a, Vec2d b) { return vmulq_f64(a.v, b.v); }
    friend Vec2d fma(Vec2d a, Vec2d b, Vec2d c) { return vfmaq_f64(c.v, a.v, b.v); }   // c + a*b  (note the argument order!)
    double hsum() const { return vaddvq_f64(v); }                          // horizontal add
    friend Vec2d sqrt(Vec2d a) { return vsqrtq_f64(a.v); }
    friend Vec2d max(Vec2d a, Vec2d b) { return vmaxq_f64(a.v, b.v); }
};
#endif
// Dot product with 4 independent accumulators (hides the 4-cycle FMA latency; 4×2 = 8 doubles in flight)
double dot_neon(const double* a, const double* b, std::size_t n) {
    Vec2d s0, s1, s2, s3; std::size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        s0 = fma(Vec2d::load(a + i),     Vec2d::load(b + i),     s0);
        s1 = fma(Vec2d::load(a + i + 2), Vec2d::load(b + i + 2), s1);
        s2 = fma(Vec2d::load(a + i + 4), Vec2d::load(b + i + 4), s2);
        s3 = fma(Vec2d::load(a + i + 6), Vec2d::load(b + i + 6), s3);
    }
    double s = ((s0 + s1) + (s2 + s3)).hsum();
    for (; i < n; ++i) s += a[i] * b[i];                                   // scalar tail — never read past n
    return s;
}
```

On x86 the same wrapper is `__m256d` with `_mm256_loadu_pd`, `_mm256_fmadd_pd` (4 lanes, needs `-mavx2 -mfma`), and `_mm512_*` with 8 lanes. The pattern — a `Vec<T,N>` value type, hidden-friend operators, `load/store`, `fma`, `hsum`, scalar tail — is the whole design; xsimd/Highway are this plus every platform and every function. Note: M-series P-cores have **4 NEON FMA pipes of 128 bits** — the vector width is small, but the *issue width* is huge, which is why 4–8 independent accumulators matter more than lane count on Apple silicon.

---

## 8. Memory-bound vs compute-bound: the roofline model

**Arithmetic intensity** (AI) = flops performed / bytes moved from DRAM. **Roofline** (Williams, Waterman, Patterson 2009): attainable GFLOP/s = min(peak GFLOP/s, bandwidth GB/s × AI). Below the **ridge point** (AI = peak / bandwidth) a kernel is memory-bound: more FLOPs are free, fewer bytes are the only win. Above it, compute-bound: SIMD, FMA, ILP and threads are the wins.

Real numbers (Apple silicon; measure yours with `sysctl -a | grep -E 'hw.(l1|l2|memsize)'`, a STREAM triad, and a big `cblas_dgemm`):

| | Per P-core (double) | Per P-core (float) | Chip (M1 / M2 / M3 Pro / M4 Pro / M4 Max) |
|---|---|---|---|
| Peak FLOP/s | 4 NEON pipes × 2 lanes × 2 (FMA) × ~3.5–4.4 GHz ≈ **56–70 GFLOP/s** | ×2 ≈ 110–140 | 4–12 P-cores → 0.25–0.8 TFLOP/s double CPU; AMX/GPU far higher in fp32/fp16 |
| DRAM bandwidth | one core can pull ~60–100 GB/s | | 68 / 100 / 150 / 273 / 546 GB/s (unified memory, shared with GPU) |
| Ridge point | 60 GFLOP/s ÷ 100 GB/s ≈ **0.6 flop/byte** per core; whole chip ≈ 600 ÷ 120 ≈ **5 flop/byte** | | |
| L1d / L2 / SLC | 128 KB per P-core (64 KB on some), 4–16 MB shared L2 per cluster, 8–48 MB system cache | | L1 latency ~3–4 cycles, L2 ~15, DRAM ~100 ns |

Where common kernels sit (double precision, arrays too big for cache):

| Kernel | Flops | Bytes | AI (flop/byte) | Bound | Attainable on one core (100 GB/s, 60 GFLOP/s) |
|---|---|---|---|---|---|
| `y = a*x + y` (axpy) | 2n | 24n (read x, read y, write y) | 0.083 | Memory | 8.3 GFLOP/s |
| `dot(x, y)` | 2n | 16n | 0.125 | Memory | 12.5 |
| `sum(x)` | n | 8n | 0.125 | Memory | 12.5 |
| SpMV (CSR, `uint32` idx) | 2·nnz | 12·nnz + 8·nnz (x) ≈ 20·nnz | 0.1 | Memory | 10 |
| 2-D 5-point stencil (`u_new = c·(4 neighbours) − u`) | 6n | 16n (read u once if rows fit in cache, write u_new) | 0.375 | Memory | 37 |
| 3-D 7-point stencil | 8n | 16n (if 2 planes fit in cache) | 0.5 | Near ridge | 50 |
| `gemv` | 2n² | 8n² | 0.25 | Memory | 25 |
| `gemm` n×n, blocked | 2n³ | ~8n² × (n / block) → with 64 KB blocks ≈ 24n³/b, b ≈ 64 | ≫ 5 | **Compute** | 60 (peak) — BLAS gets 50–55 |
| N-body direct (SoA, 3-D) | ~20 per pair | 24 per pair (x, y, z of j; i in registers), less if j-block cached | ~1–20 (cache) | Compute | ~40–60 |
| `exp`/`tanh` elementwise | ~20–40 per element | 16 | 1.5–2.5 | Near ridge → compute with SIMD math | |
| FFT (n log n) | 5n log₂n | 16n per pass, log n passes if not cache-blocked | ~0.3 per pass | Memory (naive) / compute (blocked) | |
| Softmax over a row (max, exp, sum, divide) | ~30n | 16n | ~2 | Compute, borderline | fuse the four passes into one → 4× fewer bytes |
| Adam update | ~12 per param | 40 per param (w, g, m, v read; w, m, v write) | 0.3 | Memory | 30 → this is why optimizer steps are slow and why fused/8-bit optimizers exist |

How to use it: (1) count flops and bytes for your kernel *as written*; (2) compute AI; (3) compare to the ridge point → memory- or compute-bound; (4) measure GFLOP/s with `../19`'s harness and compare to the roofline value — if you're at 80% of the bound, stop optimizing that bound and change the *algorithm's AI* (fusion, blocking, lower precision, fewer passes); if you're at 20%, there's an implementation problem (strides, aliasing, no SIMD, allocation, branch). Every "optimization" in the case studies (§15–16) is one of: raise AI (fuse, block), lower bytes (precision, SoA, indices), or approach peak (SIMD, threads, BLAS).

---

## 9. OpenMP in C++

`../12_performance` §11 introduced `parallel for` and `reduction`. The rest of the model you'll use (OpenMP 4.5/5.x; Apple clang: `brew install libomp`, `-Xpreprocessor -fopenmp -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp`; Linux clang/GCC: `-fopenmp`):

```cpp
#ifdef _OPENMP
#include <omp.h>
#endif
// collapse: merge nested loops into one iteration space (better balance when the outer loop is short, e.g. 8 rows on 10 threads)
#pragma omp parallel for collapse(2) schedule(static)
for (std::size_t i = 1; i < ny - 1; ++i)
    for (std::size_t j = 1; j < nx - 1; ++j) u_new[i*nx+j] = 0.25 * (u[(i-1)*nx+j] + u[(i+1)*nx+j] + u[i*nx+j-1] + u[i*nx+j+1]);

// simd: vectorize this loop (also usable without threads); reduction allows reassociation of the sum
#pragma omp simd reduction(+:s) aligned(a, b : 16)
for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i];
// parallel for simd: threads across chunks, SIMD within
#pragma omp parallel for simd reduction(+:s)
for (...) ...

// schedule: static (default; equal chunks; best for uniform work), dynamic,chunk (work queue; for irregular work like Barnes-Hut per body),
//           guided (decreasing chunks), auto. nowait removes the implicit barrier at the end of a for inside a parallel region.
#pragma omp parallel
{
    #pragma omp for schedule(dynamic, 64) nowait
    for (std::size_t b = 0; b < n_bodies; ++b) acc[b] = tree_force(b);      // cost varies per body
    #pragma omp for
    for (...) ...                                                              // second loop reuses the same thread team (no fork/join cost)
}

// Tasks: recursive/irregular parallelism (tree build, quicksort, task graphs). One thread creates tasks; the team executes them.
void build(Node* node, int depth) {
    if (node->n_bodies < 64 || depth > 8) { build_serial(node); return; }
    partition(node);
    for (int c = 0; c < 8; ++c) {
        #pragma omp task shared(node) firstprivate(c) if(depth < 4)         // if(): stop creating tasks when they'd be too small
        build(node->child[c], depth + 1);
    }
    #pragma omp taskwait                                                    // wait for this node's children
}
// Call site:  #pragma omp parallel  {  #pragma omp single  build(root, 0);  }
// taskloop: a for loop split into tasks (composes with other tasks, unlike `for`):  #pragma omp taskloop grainsize(256)
// depend(in: x) depend(out: y): task graph with data dependencies — e.g. FDTD E-update depends on the H-update of neighbouring tiles.

// Nested parallelism: off by default (inner `parallel` runs on 1 thread). omp_set_max_active_levels(2) or OMP_MAX_ACTIVE_LEVELS=2.
// Usually the wrong tool — flatten with collapse or use tasks. Exception: outer over independent simulations, inner over the grid.

// Environment: OMP_NUM_THREADS=8 OMP_PROC_BIND=close OMP_PLACES=cores (pin threads; avoid migration between P and E cores on Apple:
// there is no API to pin to P-cores; run with OMP_NUM_THREADS = number of P-cores and let the scheduler place them),
// OMP_SCHEDULE=dynamic,64 (with schedule(runtime)), OMP_DISPLAY_ENV=true (print what the runtime decided).
// Timing: omp_get_wtime(). Threads: omp_get_max_threads(), omp_get_thread_num(), omp_in_parallel().
```

Things that bite: (1) `reduction` on a `std::vector` — OpenMP 4.5 supports user-defined reductions (`#pragma omp declare reduction(vsum : std::vector<double> : ...)`) but per-thread partial vectors combined manually are clearer; (2) exceptions must not escape a parallel region (UB → `std::terminate`) — catch inside, set a flag, rethrow after; (3) `std::size_t` loop variables are fine since OpenMP 3.0 but the loop must be in canonical form (no `break`, no modifying `i`, `i < n` with loop-invariant `n`); (4) `false sharing` on per-thread partials (`../12_performance` §12); (5) C++ objects with non-trivial constructors as `private` are default-constructed per thread — fine for `std::vector`, expensive for a 1 MB buffer (hoist a per-thread buffer outside the loop: `#pragma omp parallel { std::vector<double> buf(n); #pragma omp for ... }`); (6) OpenMP + `std::thread` + Accelerate's own threading oversubscribe — set `OMP_NUM_THREADS` and `VECLIB_MAXIMUM_THREADS=1` (Accelerate's knob) or call BLAS from one thread only.

Alternatives: **TBB** (`oneapi::tbb::parallel_for`, `parallel_reduce`, work-stealing, composable, `brew install tbb`), **C++17 parallel algorithms** (`std::sort(std::execution::par_unseq, …)` — libc++ implements them since 17 with `-fexperimental-library` on Apple; libstdc++ needs TBB as the backend), **`std::jthread` + a work queue** (you built one in the concurrency chapter), **Kokkos/RAJA** (portability to GPUs), **HPX** (futures at scale). OpenMP remains the default in HPC because it is two lines and every compiler has it.

---

## 10. MPI: distributed memory

OpenMP shares one address space; **MPI** (Message Passing Interface) runs `P` copies of your program on `P` processes — across cores or across 10,000 nodes — that share nothing and communicate by explicit messages. Every large-scale simulation (climate, CFD, cosmological N-body, lattice QCD) is MPI, usually MPI + OpenMP (one process per socket/GPU, threads inside) + GPU.

```sh
brew install open-mpi              # Linux: apt install libopenmpi-dev  (or mpich)
mpicxx -std=c++17 -O2 -Wall -Wextra -o heat_mpi heat_mpi.cpp    # mpicxx = wrapper that adds -I/-L/-l for MPI; mpicxx --showme to see
mpirun -np 4 ./heat_mpi 1024 1000     # 4 processes on this machine; --oversubscribe if -np > cores
# cluster: mpirun -np 256 --hostfile hosts ./heat_mpi ...  or through a scheduler (sbatch/srun on Slurm)
```

```cpp
#include <mpi.h>
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);                 // who am I: 0..size-1
    MPI_Comm_size(MPI_COMM_WORLD, &size);                 // how many of us
    // ... compute a local partial ...
    double local = compute_local_energy(), total = 0.0;
    MPI_Allreduce(&local, &total, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);   // everyone gets the sum (§6.1: order fixed by the implementation for a given P, not across P)
    if (rank == 0) std::printf("E = %.12g\n", total);
    MPI_Finalize();
}
```

Core calls: `MPI_Send(buf, count, MPI_DOUBLE, dest, tag, comm)` / `MPI_Recv(buf, count, type, source, tag, comm, &status)` (blocking; a `Send` may block until matched → two ranks both `Send`ing first to each other **deadlock**); `MPI_Sendrecv` (both in one call, deadlock-free); `MPI_Isend`/`MPI_Irecv` + `MPI_Waitall` (non-blocking; overlap communication with computation); collectives `MPI_Bcast`, `MPI_Reduce`/`MPI_Allreduce`, `MPI_Scatter`/`MPI_Gather`/`MPI_Allgather`, `MPI_Alltoall` (the FFT transpose), `MPI_Barrier`; `MPI_Cart_create` + `MPI_Cart_shift` to find neighbours in a 2-D/3-D process grid; `MPI_Type_vector` for strided halos (columns of a row-major grid); MPI-IO (`MPI_File_write_at_all`) or HDF5-parallel for checkpoints.

### 10.1 Domain decomposition with halo exchange — the heat/FDTD grid

Split the `ny × nx` grid into `P` horizontal slabs (1-D decomposition; 2-D with `MPI_Cart_create` scales better — halo bytes ∝ perimeter). Each rank owns rows `[r₀, r₁)` plus **one ghost/halo row above and below** holding a copy of the neighbour's boundary row. Every time step: (1) exchange halos (send my top row to `rank-1`, receive its bottom row into my top ghost; and symmetrically); (2) update interior with the usual stencil — the stencil at my first row reads the ghost row, exactly as if the grid were whole; (3) optionally `Allreduce` a global quantity (energy, max residual for convergence).

```cpp
// local grid: (rows_local + 2) × nx; row 0 and row rows_local+1 are ghosts
const int up = rank == 0 ? MPI_PROC_NULL : rank - 1;             // MPI_PROC_NULL: Send/Recv to it is a no-op (boundary ranks)
const int down = rank == size - 1 ? MPI_PROC_NULL : rank + 1;
MPI_Sendrecv(&u[1 * nx], nx, MPI_DOUBLE, up, 0,                   // send my first real row up...
             &u[(rows_local + 1) * nx], nx, MPI_DOUBLE, down, 0,  // ...receive down-neighbour's first row into my bottom ghost
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
MPI_Sendrecv(&u[rows_local * nx], nx, MPI_DOUBLE, down, 1,        // send my last real row down...
             &u[0], nx, MPI_DOUBLE, up, 1,                          // ...receive up-neighbour's last row into my top ghost
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
for (int i = 1; i <= rows_local; ++i)                             // interior update reads ghosts at i-1 = 0 and i+1 = rows_local+1
    for (int j = 1; j < nx - 1; ++j)
        u_new[i*nx+j] = u[i*nx+j] + alpha * (u[(i-1)*nx+j] + u[(i+1)*nx+j] + u[i*nx+j-1] + u[i*nx+j+1] - 4*u[i*nx+j]);
std::swap(u, u_new);
```

Communication cost per step: 2 rows × `nx` × 8 bytes per rank, latency ~1–2 µs on a node, ~5 µs across a fabric; computation: `rows_local × nx × 8` flops. The ratio improves with bigger slabs — **weak scaling** (fixed work per rank, grow P) is easy; **strong scaling** (fixed total work, grow P) hits the wall when the halo dominates. Overlap it: `Irecv`/`Isend` the halos, update the interior rows that don't need them, `Waitall`, update the two boundary rows. FDTD (P08) is identical with `Ez`, `Hx`, `Hy` fields exchanged at half steps; Barnes–Hut across MPI needs a locally-essential tree (hard — that's a research code).

The example does not link MPI (the sandbox may not have it); exercise 20.10 has the full program and build line.

---

## 11. GPU programming overview

A CPU core minimizes latency for one thread (big caches, branch prediction, out-of-order). A GPU maximizes throughput for tens of thousands of threads (tiny caches, no prediction, massive SIMD, and latency *hidden* by switching between threads while others wait on memory). If your kernel is a data-parallel loop over ≥ 10⁶ elements with regular access, it belongs on a GPU; if it is a tree walk with pointer chasing or 10⁴ elements, it doesn't.

| API | Vendor / platform | Kernel language | Host API flavour | Portability |
|---|---|---|---|---|
| **CUDA** | NVIDIA only | C++ with `__global__`, `__shared__`, `threadIdx` | `cudaMalloc/cudaMemcpy/<<<grid, block>>>` | None, but the biggest ecosystem (cuBLAS, cuDNN, Thrust, CUB, CUTLASS) |
| **HIP** | AMD (and NVIDIA via a shim) | CUDA-like (`hipify` converts) | `hipMalloc`, `hipLaunchKernelGGL` | AMD + NVIDIA; ROCm libraries (rocBLAS, MIOpen) |
| **Metal** | Apple | Metal Shading Language (C++14 subset) in `.metal` files; `kernel void f(device float* x [[buffer(0)]], uint id [[thread_position_in_grid]])` | Objective-C/Swift `MTLDevice`, `MTLComputeCommandEncoder`; from C++ via `metal-cpp` headers | Apple only; unified memory (no copies), MPS for matmul/conv; what PyTorch's `mps` backend uses |
| **SYCL** | Khronos; Intel oneAPI (DPC++), AdaptiveCpp | Single-source C++17 lambdas: `q.parallel_for(range<1>(n), [=](id<1> i){ y[i] = a*x[i] + y[i]; })` | `sycl::queue`, `buffer`/`accessor` or USM pointers | Intel/AMD/NVIDIA with the right backend; the "standard" C++ answer |
| **OpenCL** | Khronos | C99 kernels as strings | verbose | Everything; aging (Apple deprecated it) |
| **Kokkos / RAJA / std::par** | Libraries over the above | C++ lambdas | `Kokkos::parallel_for` ; `std::for_each(std::execution::par_unseq, …)` with nvc++ `-stdpar=gpu` | Portable; Kokkos is the DOE standard |
| **OpenMP target / OpenACC** | Directives | `#pragma omp target teams distribute parallel for map(to: x) map(tofrom: y)` | | Portable; less control |

**Execution model** (CUDA terms; Metal: threads/threadgroups/SIMD-groups; SYCL: work-items/work-groups/sub-groups): a kernel launches a **grid** of **blocks** of **threads**; threads in a block share fast **shared memory** (~48–228 KB per SM on NVIDIA; 32 KB threadgroup memory on Apple) and can `__syncthreads()`; a **warp** of 32 threads (Apple SIMD-group: 32; AMD wavefront: 64) executes in lockstep — divergent `if`s serialize the branches. **Memory hierarchy**: registers (~64K × 32-bit per SM, the fastest; too many per thread → low occupancy) → shared/L1 (~100 cycles cheaper than global) → L2 (tens of MB) → global HBM/GDDR (H100: 3.3 TB/s; RTX 4090: 1 TB/s; M4 Max unified: 546 GB/s shared with CPU) → host memory over PCIe 4/5 (25–64 GB/s — the transfer that kills small kernels; Apple's unified memory removes it). **Coalescing**: threads `t` and `t+1` should read addresses `a` and `a+4` → SoA is mandatory. Peak fp32: H100 ~60 TFLOP/s (fp32 CUDA cores), ~1,000 TFLOP/s fp16 tensor cores; M4 Max GPU ~16 TFLOP/s fp32.

What a tiled matmul kernel looks like (CUDA; the idea is identical in every API):

```cpp
constexpr int T = 32;                                        // tile = one block = 32×32 threads = 32 warps... no: 1024 threads = 32 warps
__global__ void gemm_tiled(const float* A, const float* B, float* C, int n) {
    __shared__ float As[T][T], Bs[T][T];                     // on-chip tiles, shared by the block
    const int row = blockIdx.y * T + threadIdx.y, col = blockIdx.x * T + threadIdx.x;
    float acc = 0.f;
    for (int k0 = 0; k0 < n; k0 += T) {
        As[threadIdx.y][threadIdx.x] = A[row * n + k0 + threadIdx.x];     // each thread loads one element of each tile (coalesced along x)
        Bs[threadIdx.y][threadIdx.x] = B[(k0 + threadIdx.y) * n + col];
        __syncthreads();                                                   // tile fully loaded
        for (int k = 0; k < T; ++k) acc += As[threadIdx.y][k] * Bs[k][threadIdx.x];   // T FMAs per 2 global loads → AI raised T×
        __syncthreads();                                                   // done reading before the next load overwrites
    }
    C[row * n + col] = acc;
}
// host: dim3 block(T, T), grid(n / T, n / T);  gemm_tiled<<<grid, block>>>(dA, dB, dC, n);  cudaDeviceSynchronize();
```

This naive tiled kernel reaches ~10–20% of peak; cuBLAS/CUTLASS add register tiling (each thread computes an 8×8 micro-tile), double-buffered shared memory, vectorized 128-bit loads, and tensor-core `mma` instructions to reach 70–90%. **When it pays off**: the work must amortize the launch (~5–10 µs) and any transfer (`n` bytes / 30 GB/s); a 1000×1000 sgemm (2 GFLOP) takes ~40 ms on one CPU core, ~0.1 ms on a GPU, plus 12 MB of transfer ≈ 0.4 ms — worth it only if the data *stays* on the GPU across many operations (which is exactly how PyTorch works: tensors live on the device, kernels are queued asynchronously). For your projects: the transformer's forward/backward (P05) at any real size, direct N-body (P07; embarrassingly parallel, 10⁵ bodies × 10⁵ = 10¹⁰ pair-interactions per step at ~1 TFLOP/s effective = 0.2 s/step vs 3 minutes on a core), FDTD/LBM grids (P08/P10; stencils are bandwidth-bound and the GPU has 5–10× the bandwidth). Not worth it: Barnes–Hut tree *construction*, small solvers, anything under ~10⁵ elements.

---

## 12. Automatic differentiation approaches

P03 built reverse-mode AD with a dynamic tape: every operation records a node with its inputs and a backward closure; `backward()` walks the tape in reverse. That is what PyTorch does ("define-by-run"). The landscape:

| Approach | How | Cost | Examples |
|---|---|---|---|
| **Forward mode (dual numbers)** | Carry `(value, derivative)` through every op: `(a, a')·(b, b') = (ab, a'b + ab')`. One pass gives ∂f/∂xᵢ for *one* input direction. | O(n) passes for n inputs; no memory overhead | `Dual<double>` template with operator overloading — 50 lines; ideal for Jacobians of small systems (Newton's method in P06, ODE sensitivities); autodiff.hpp; CppAD forward |
| **Reverse mode, tape** | Record ops during the forward pass, replay backwards: one pass gives ∂f/∂x for *all* inputs (the gradient of a scalar loss). | O(1) passes; memory O(#ops) — the tape stores every intermediate | PyTorch autograd, your P03, Adept (C++), Stan Math |
| **Reverse mode, static graph** | Build the graph once, compile it; can fuse and optimize | Same math; less flexible, faster | TensorFlow 1, JAX (`jit` traces to XLA), PyTorch `torch.compile` |
| **Source transformation** | A tool reads your source and *emits* the derivative code | No runtime overhead at all; handles loops/branches; hard to build | Tapenade (Fortran/C), ADIFOR, Zygote (Julia, on IR) |
| **Compiler-level: Enzyme** | An LLVM plugin that differentiates *LLVM IR after optimization*: works on any language that compiles to LLVM (C, C++, Rust, Julia, Fortran via flang), including vectorized and `-O3`'d code | Often faster than hand-written adjoints because it differentiates the optimized code | `__enzyme_autodiff(f, x)` — Moses & Churavy, NeurIPS 2020; `brew install enzyme`; `clang -fplugin=ClangEnzyme.so -O2` |

Reverse mode's memory problem — a 100-layer network or a 10⁶-step simulation cannot store every intermediate — is solved by **checkpointing** (Griewank's `revolve`, 1992; PyTorch `torch.utils.checkpoint`): store activations only every `k` layers and recompute the ones between during backward, trading O(√n) memory for 2× compute. Differentiating *through a simulation* (adjoint methods: gradient of a final energy with respect to initial conditions or parameters — how you fit an N-body model to observations or design an FDTD structure) is reverse-mode AD over the time loop; the adjoint of a symplectic integrator has a beautiful closed form. In C++ the practical choices are: a `Dual` type for Jacobians, your tape for ML, and Enzyme when you need gradients of an existing fast kernel without rewriting it.

---

## 13. Checkpointing and I/O formats

### 13.1 The `.npy` format — write it yourself (the example does)

NumPy's `.npy` (spec: `numpy/lib/format.py`) is the simplest way to hand results to Python:

```
offset  size     content
0       6        magic: 0x93 'N' 'U' 'M' 'P' 'Y'
6       1        major version (1)
7       1        minor version (0)
8       2        HEADER_LEN, little-endian uint16 (version 2.0 uses uint32 for headers > 64 KB)
10      HEADER_LEN  ASCII Python dict literal, padded with spaces so that (10 + HEADER_LEN) % 64 == 0, ending in '\n':
                 {'descr': '<f8', 'fortran_order': False, 'shape': (3, 4), }
10+HEADER_LEN     raw data, C order (row-major) unless fortran_order, in the dtype given by descr ('<f8' little-endian float64, '<f4' float32, '<i8', '<u4', '|u1', '<f2' float16)
```

Write: build the dict string, compute padding, emit header, `fwrite` the doubles (little-endian host — assert it). Read: check the magic, parse `descr`/`shape` with a small hand parser (or regex), `fread`. `np.load("out.npy")` then just works, and `np.save` output is readable by your C++ loader (the fuzz target from `../19`). **`.npz`** is a zip archive of `.npy` files — writing one needs a ZIP writer (stored, no compression, is ~80 lines with CRC32) or `miniz`; simpler to write multiple `.npy` files. **safetensors** (Hugging Face) is the ML weights format: 8-byte little-endian header length, JSON header `{"W": {"dtype": "F32", "shape": [784, 128], "data_offsets": [0, 401408]}, …}`, then raw bytes — as easy as `.npy` and `torch.load`-free.

### 13.2 HDF5

For anything bigger — checkpoints with dozens of arrays, time series, metadata — **HDF5** is the scientific standard (`brew install hdf5`; `apt install libhdf5-dev`; `h5c++` compiler wrapper or `find_package(HDF5 COMPONENTS C CXX)`): a hierarchical "file system in a file" with groups (`/step_0100/`), datasets (n-D arrays with a dtype), attributes (metadata on anything), chunking (append along one axis; read a sub-block without reading the file), compression (gzip/blosc per chunk), and parallel I/O over MPI. The C API is verbose; **HighFive** (header-only C++ wrapper) makes it `HighFive::File f("ckpt.h5", File::Overwrite); f.createDataSet("/positions", positions_vector); f.createAttribute("time", t);`. Inspect with `h5dump -H ckpt.h5` / `h5ls -r`; read in Python with `h5py`. NetCDF (climate), Zarr (cloud-native chunks, Python-first), ADIOS2 (exascale) are the relatives. For your P07–P10 snapshots: one HDF5 file per run, a group per output step, SoA datasets — ParaView reads it with an XDMF sidecar, or write VTK directly:

### 13.3 VTK legacy format — ParaView for grids

The legacy `.vtk` ASCII format is 8 header lines and your data; ParaView (`brew install --cask paraview`) opens it, colours by scalar, animates a numbered sequence (`field_0000.vtk … field_0999.vtk`):

```
# vtk DataFile Version 3.0
FDTD Ez field, step 120
ASCII                          (or BINARY: big-endian raw doubles follow each header — 10× smaller, use for real runs)
DATASET STRUCTURED_POINTS
DIMENSIONS 200 100 1           (nx ny nz; for a 2-D grid nz = 1)
ORIGIN 0 0 0
SPACING 0.01 0.01 1
POINT_DATA 20000               (nx*ny*nz)
SCALARS Ez double 1
LOOKUP_TABLE default
0.0 0.0012 0.0031 ...          (x fastest, then y, then z — same as row-major with j inner)
VECTORS velocity double        (optional second field: 3 components per point, even in 2-D)
0.1 0.0 0.0 ...
```

For particles (N-body, PIC): `DATASET POLYDATA` / `POINTS n double` / x y z … / `POINT_DATA n` / `SCALARS mass double 1`. For unstructured meshes: `UNSTRUCTURED_GRID` with `CELLS`. The XML variants (`.vti`, `.vtp`, `.vtu`) support compression and parallel pieces (`.pvti`) but need a small XML writer; the `.pvd` collection file adds time stamps for animation. The pipeline: sim dumps `.npy`/`.vtk` per output step → Python `matplotlib.pyplot.imshow(np.load(...))` for quick looks and publication figures → ParaView for 3-D, isosurfaces, streamlines, animations; `pvpython` scripts the latter.

---

## 14. Reproducibility

A simulation or training run is reproducible when the same inputs give bit-identical outputs — the property that lets you bisect a bug, validate an optimization, and publish. Checklist:

- **Seeds**: one master seed on the command line, logged; derive per-component seeds deterministically (`std::seed_seq`, or `master ^ hash(name)`). `std::mt19937_64`'s *output* is specified by the standard and identical everywhere; **`std::normal_distribution` and `uniform_real_distribution` are not** (implementation-defined algorithms — libc++ and libstdc++ differ). For cross-platform reproducibility write your own Box–Muller / bit-manipulation uniform (`(rng() >> 11) * 0x1.0p-53`) or use PCG/xoshiro with your own distributions.
- **Thread count**: log it; reductions are only reproducible for a fixed count (§6.1) and `schedule(static)`. Provide a `--deterministic` mode that uses fixed-order reductions and single-threaded BLAS (`VECLIB_MAXIMUM_THREADS=1`, `OPENBLAS_NUM_THREADS=1`).
- **Floating-point flags**: no `-ffast-math` (changes results *and* breaks Kahan/NaN checks); `-ffp-contract=off` if results must match across compilers (FMA vs mul+add differ in the last bit); `-O2` vs `-O3` can change vectorized reduction order — pin the build flags in the log.
- **Unordered containers**: `std::unordered_map` iteration order depends on the hash and the implementation → never iterate one to produce output; `std::sort` a key list first. Same for `std::execution::par_unseq`.
- **Uninitialized memory**: `std::vector<double>(n)` zero-initializes; `new double[n]` and `std::make_unique_for_overwrite` don't; MSan/Valgrind (Linux) find reads of them.
- **Environment capture**: write `git rev-parse HEAD` (+ dirty flag), compiler version (`__VERSION__`), flags, hostname, CPU (`sysctl -n machdep.cpu.brand_string`), `OMP_NUM_THREADS`, the seed, and the command line into the output file's header/attributes. Reproducibility without provenance is luck.
- **Time**: never `t += dt` (drifts); `t = step * dt`. Never wall-clock-dependent decisions (adaptive step controllers based on elapsed time).
- **Golden outputs**: a fixed-seed short run whose checksum (`std::hash` over the bytes, or `xxh64`) is checked in CI; any change to the number requires a justification in the PR.

---

## 15. Case study: making the ML stack fast

Your P01→P05 stack: `Matrix` (naive triple loop), `Tensor<T>`, autograd tape, `Linear/ReLU/LayerNorm/Attention`, a 2-layer transformer with `d_model = 128`, 4 heads, sequence 64, batch 16, trained on character data. Baseline: ~2 s per step at `-O2` (measured on a similar naive stack; yours will vary). Apply the roofline discipline in this order and expect these speedups (cumulative, single P-core unless noted):

| Step | What | Why (roofline) | Expected gain | Cumulative |
|---|---|---|---|---|
| 0 | Profile (`xctrace`/`perf`): find that ~85% is `matmul`, ~10% `malloc`/`free`, the rest elementwise | You cannot optimize what you haven't measured | — | 1× (2 s/step) |
| 1 | Loop order `ikj` + hoisting (`../12`) | Unit stride; vectorizes | 5–10× on matmul | ~5× |
| 2 | **BLAS**: replace matmul with `cblas_sgemm`/`dgemm` (Accelerate); keep your kernel for tests | Compute-bound kernel → let experts hit peak; ~50 GFLOP/s double, 100+ float, AMX more | 10–20× over step 1 on matmul | ~30–50× (matmul now ~30% of time) |
| 3 | **Memory reuse**: preallocate activations and gradients per layer; `_into` kernels; arena for the tape; no `std::vector` construction in `forward`/`backward` | `malloc` at 10–30% of profile; page faults on fresh memory | 1.3–2× overall | ~50–80× |
| 4 | **Fused elementwise ops**: softmax (max/exp/sum/scale in one pass), LayerNorm (Welford mean/var + normalize in two passes), GELU + bias + dropout in one kernel; fused Adam update | Each pass over a 16×64×128 tensor is bandwidth; 4 passes → 1 raises AI 4× | 2–3× on those ops (now ~40% of time) | ~80–120× |
| 5 | **`float` instead of `double`** everywhere except loss accumulation and Adam's second moment | Half the bytes; twice the SIMD lanes; `sgemm` 2× `dgemm` | 1.6–2× | ~150–200× |
| 6 | **Threads**: BLAS threads for big gemms (`VECLIB_MAXIMUM_THREADS`), OpenMP `parallel for` over batch×heads for attention and over rows for LayerNorm/softmax; one thread pool, no oversubscription | 8 P-cores; Amdahl: serial parts (tape bookkeeping, optimizer if not parallel) cap you at ~5–6× | 4–6× | ~600–1000× (≈ 2–3 ms/step) |
| 7 | **Mixed precision** (bf16 storage for weights/activations, fp32 accumulate and master weights) | Half the bytes again; AMX/NEON bf16 dot products (`bfdot`) | 1.3–1.8× on bandwidth-bound parts | — |
| 8 | Algorithmic: KV-cache for inference; flash-attention-style tiling so the `S×S` score matrix never hits memory (AI of attention goes from ~1 to ~50); gradient checkpointing for depth | Raises AI structurally | 2–4× at long sequences | — |
| 9 | GPU (Metal/CUDA) | 10–50× more FLOP/s and bandwidth; only pays if the whole training step lives on the device | 10–30× over the 8-core CPU at `d_model ≥ 512` | — |

The order matters: BLAS before threads (a naive kernel threaded 8 ways is still 10× slower than one BLAS call), allocation before fusion (you can't fuse into buffers you keep reallocating), `float` before mixed precision. At each step: re-profile, check the new hot spot against the roofline (matmul at 45 GFLOP/s double on one core = 75% of peak = done; softmax at 5 GFLOP/s with AI 2 = 4% of its bound = not done), and run the property tests (`../19` §7 — finite-difference gradient checks) *after every step*; fusion and precision changes break gradients silently.

---

## 16. Case study: the physics stack

P07 N-body, 3-D, `N` bodies, softened gravity, leapfrog. Baseline: direct O(N²) with `std::vector<Body>` (AoS), `-O2`, one core; `N = 20,000` → 4·10⁸ pair interactions × ~20 flops = 8 GFLOP per step at ~4 GFLOP/s (AoS, `1/sqrt` in a dependency chain) ≈ **2 s/step**.

| Step | What | Why | N = 20k step time | Enables |
|---|---|---|---|---|
| 1 | **Barnes–Hut** (θ = 0.5): octree, monopole (+ quadrupole) approximation for far cells | O(N log N) ≈ 20k × 15 × ~30 cells visited ≈ 10⁷ interactions instead of 4·10⁸ | ~60 ms (30×), error ~1e-3 in force | N = 10⁶ becomes feasible (~5 s/step) |
| 2 | **SoA** for positions/velocities/masses; tree nodes in a flat `std::vector<Node>` with child indices (not pointers), built in Morton (Z-order) so a depth-first walk is nearly sequential in memory | Cache lines full of useful data; tree walk is the memory-bound part (AI ~1, pointer chasing) | ~35 ms | Vectorization of the leaf interactions |
| 3 | **Vectorize the direct part**: leaf cells with ≤ 16 bodies computed with NEON `Vec2d`/`Vec4f` across the *j* bodies; `1/sqrt` via `vrsqrteq_f64` + 2 Newton steps; 4 accumulators | The pairwise kernel is compute-bound (AI ~ 20 flops / 24 B if j-block in L1); target 40+ GFLOP/s | ~20 ms | — |
| 4 | **OpenMP**: `parallel for schedule(dynamic, 256)` over bodies for the force walk (irregular cost → dynamic); tree build with tasks (§9) or serial (it's ~10% — Amdahl says fix it once the walk is fast) | 8 P-cores; force walk is embarrassingly parallel; tree build is the serial fraction | ~4 ms (6× on the walk) | 60 fps at N = 20k; N = 10⁶ at ~0.5 s/step |
| 5 | **Reproducibility & correctness gates**: energy drift < 1e-6/orbit with leapfrog (symplectic), momentum conserved to rounding (§6.2 — compensated sums for the totals), fixed reduction order in `--deterministic` | Every step above can silently break physics | — | Trust |
| 6 | **Float** for positions relative to a cell centre (mixed: double for absolute positions and time integration, float for the pairwise kernel) | 2× SIMD lanes; force needs ~1e-6 relative accuracy, positions need ~1e-12 | ~2.5 ms | — |
| 7 | **What a GPU adds**: direct O(N²) at N = 10⁵ is 10¹⁰ interactions ≈ 200 GFLOP/step — 0.1–0.2 s on a 1–2 TFLOP/s-effective GPU (N-body is *the* classic GPU demo: shared-memory tiles of j-bodies, each thread owns an i-body, `rsqrtf`); Barnes–Hut on GPU (Burtscher–Pingali 2011) reaches N = 10⁷; FMM (fast multipole, O(N)) for 10⁸–10⁹ on clusters | 10⁴ threads, 500 GB/s–3 TB/s, `rsqrt` in hardware | direct N = 10⁵ at 5–10 fps; BH N = 10⁶ at 30 fps | The regime of galaxy simulations |
| 8 | **MPI** for N ≥ 10⁸: domain decomposition (space-filling curve partition), locally essential trees, `Allreduce` for energies — the design of Gadget-4 and PKDGRAV | Beyond one node's memory (10⁸ bodies × 56 B = 5.6 GB fits; 10⁹ doesn't with the tree) | — | Cosmology |

The same ladder for the grid codes (P08 FDTD, P10 LBM): they are **bandwidth-bound** stencils (AI 0.4–1), so the order is: (1) SoA fields, contiguous row-major, `j` inner; (2) fuse the E and H updates into one sweep where the dependency allows, or fuse LBM's collide + stream into one pass ("push" scheme) so each cell is touched once per step (AI ×2); (3) spatial blocking so a tile of rows stays in L2 across several time steps (temporal blocking — hard, 2–3×); (4) `float` (2×); (5) OpenMP `collapse(2)` over the grid (6–8×); (6) GPU — stencils get the full memory-bandwidth ratio, typically 10–20× over 8 CPU cores; (7) MPI halo exchange for grids that don't fit one node (§10). Expected: a 1000×1000 FDTD step from ~15 ms naive to ~1 ms on 8 cores and ~0.1 ms on a GPU.

---

## Gotchas and undefined behavior

- **Row-major matrix handed to a Fortran LAPACK routine** = you solved with Aᵀ. Symmetric routines don't care; `dgesv`/`dgetrs` do. Transpose, or use `trans='T'`, or LAPACKE with `LAPACK_ROW_MAJOR`.
- **`lda` = logical width instead of storage stride** when multiplying a sub-block → wrong results, no error.
- **Not checking `info`** — `dgesv` returns garbage `x` for singular `A` with `info > 0`; `dsyev` may not converge.
- **Workspace not queried** (`lwork = -1` first) → too-small `work` → `info = -N` or a silent slow path.
- **BLAS `int` overflow**: `n·n > INT_MAX` for `n > 46,340` — check before casting.
- **Eigen `auto`**: `auto e = A * B;` holds references to `A`, `B`; `e` is stale after they change and dangles if they were temporaries. `MatrixXd r = …` or `.eval()`.
- **Eigen aliasing**: `A = A.transpose()` corrupts `A`; `C.noalias() = A * C` is UB-by-contract.
- **Eigen without `-DNDEBUG`** is 10× slower — expected; **with** `-DNDEBUG`, a shape mismatch is UB (out-of-bounds) — keep asserts in tests.
- **Mixed Eigen/BLAS threading** oversubscribes: 8 OpenMP threads each calling a BLAS that spawns 8 → 64 threads thrashing. Pin BLAS to 1 thread inside parallel regions.
- **FFTW plans executed on arrays with different alignment** than the plan was created for → wrong results or crash; use `fftw_alloc_*` or `FFTW_UNALIGNED`. `fftw_plan_*` is not thread-safe.
- **FFTW is GPL**: a closed-source product must license it or use pocketfft/KISS/vDSP.
- **CSR with unsorted duplicate entries** — assembling from COO without summing duplicates gives a matrix that's not what you meant; sort and coalesce.
- **CG on a non-symmetric or indefinite matrix** silently diverges or stalls; check symmetry in debug (`‖A − Aᵀ‖`), use GMRES/BiCGSTAB otherwise.
- **Absolute convergence tolerance** (`‖r‖ < 1e-10`) for a `b` of magnitude 1e6 never converges, for 1e-12 converges instantly; use relative `‖r‖/‖b‖`.
- **`-ffast-math` breaks Kahan/Neumaier** (the compensation is "simplified" away), `isnan` checks, and reproducibility. Never on numerics files.
- **`long double` on arm64 macOS is 64-bit** — no extra precision. Use compensated algorithms or `__float128`/quadmath on Linux GCC.
- **Accumulating in `float16`/`bfloat16`** — loses everything after ~1000 terms; accumulate in `float`.
- **`float16` gradients underflow** below 6e-8 → zero → training stalls; loss scaling or bf16.
- **NEON `vld1q_f64` past the end** of the array when `n % 2 != 0` → UB; always a scalar tail.
- **`vfmaq_f64(a, b, c)` computes `a + b*c`** — the accumulator is the *first* argument (unlike `std::fma(x, y, z) = x*y + z`).
- **OpenMP `reduction` on a loop that also writes a shared array without synchronization** — the reduction covers only the named variable.
- **Exception escaping an OpenMP region** → `std::terminate`.
- **`collapse(2)` with an inner bound that depends on the outer index** (`j < i`) is only allowed in OpenMP ≥ 5.0 (non-rectangular loops); older compilers reject or miscompile.
- **MPI: two ranks `MPI_Send` to each other first** → deadlock (when messages exceed the eager limit, typically 64 KB — so it "works" in small tests). `MPI_Sendrecv` or non-blocking.
- **MPI buffer reused before `MPI_Wait`** on an `Isend` → data race with the network.
- **MPI `Allreduce` result differs across process counts** — reduction order changes; not a bug, plan for it.
- **GPU kernels with divergent warps, uncoalesced AoS access, or `double` on consumer cards** (1/32–1/64 of fp32 rate) run at 1–5% of peak.
- **`.npy` header not padded to a multiple of 64** (spec ≥ 1.0 says 64; older 16) — NumPy reads it anyway, but other tools may not; and `HEADER_LEN` must include the trailing `\n`.
- **`fwrite` of a `double` array on a big-endian host** (rare today: some POWER, network appliances) needs byte-swapping for `'<f8'`; assert little-endian at startup.
- **VTK `DIMENSIONS nx ny nz` with data written in the wrong order** (y fastest) → transposed image; x varies fastest.
- **Seeding with `std::random_device` in a "reproducible" run**; seeding threads with `seed + thread_id` (correlated streams for small seeds — use `std::seed_seq` or splitmix).
- **`std::normal_distribution` differs between libc++ and libstdc++** — same seed, different numbers across macOS and Linux.
- **`std::unordered_map` iteration to produce output** — order differs across platforms and even across runs with some hash randomization.

---

## Common mistakes checklist

- [ ] BLAS calls check `n ≤ INT_MAX`; `lda/ldb/ldc` are storage strides; `beta` chosen deliberately (0 overwrite / 1 accumulate).
- [ ] LAPACK: column-major handled (transpose or `'T'`), all args by pointer, `info` checked, workspace queried, in-place overwrite understood.
- [ ] Eigen: no `auto` on kept expressions; `.noalias()` on products into distinct storage; `-DNDEBUG` in release only; `Map` over own memory instead of copies; `<Eigen/Dense>` not in public headers.
- [ ] FFT: plan reused; normalization documented; real-to-complex symmetry used; license checked.
- [ ] Sparse: CSR with `uint32` indices where possible; COO coalesced before conversion; CG only on SPD; relative tolerance; preconditioner considered.
- [ ] Reductions: tolerances scale with `n·ε·Σ|x|`; compensated or pairwise sums for totals; `--deterministic` mode with fixed-order reduction; no `-ffast-math`.
- [ ] Mixed precision: fp32 accumulation and master weights; bf16 or loss scaling; software `bfloat16` rounds to nearest even.
- [ ] SIMD: `Vec<T,N>` wrapper in one platform file; 4+ accumulators; scalar tail; `fma` argument order right.
- [ ] Roofline: AI computed for each hot kernel; measured GFLOP/s compared to `min(peak, BW·AI)`; optimization chosen accordingly (bytes vs flops).
- [ ] OpenMP: `schedule(dynamic)` for irregular work; `collapse` for short outer loops; tasks for trees; no exceptions across regions; BLAS threads pinned inside regions; guarded by `#ifdef _OPENMP`.
- [ ] MPI: `Sendrecv` or non-blocking for halos; `MPI_PROC_NULL` at boundaries; `Allreduce` for globals; buffers untouched until `Wait`.
- [ ] I/O: `.npy` header padded to 64 with `\n`; little-endian asserted; VTK x-fastest; provenance (git sha, compiler, flags, seed, threads) in every output.
- [ ] Reproducibility: master seed logged; own distributions for cross-platform; no unordered-container iteration in output; `t = step·dt`.
- [ ] Case-study order followed: profile → algorithm → BLAS → allocation → fusion → precision → threads → GPU; property tests re-run after each step.

---

## You can move on when...

- You can write a correct `cblas_dgemm` call for `C = A·Bᵀ` on row-major sub-blocks, and a `dgesv_` call with the transpose handled, `info` checked, and explain the eight Fortran-convention pitfalls.
- You can use `Eigen::Map` over your `Tensor`'s storage, explain what `auto e = A*B` holds, and name the two aliasing rules.
- You can write CSR `spmv`, implement CG from memory with a relative stopping criterion, and outline GMRES(m) (Arnoldi + Givens + restart).
- You can explain why `omp parallel for reduction` is irreproducible, implement Kahan and pairwise summation, and say what breaks them.
- You can fill in the float16/bfloat16 table (bits, ε, range), explain loss scaling and fp32 accumulation, and state the C++23 `<stdfloat>` status.
- You can compute the arithmetic intensity of axpy, SpMV, a 5-point stencil, gemm and Adam, place them on an M-series roofline, and say which optimization applies to each.
- You can write a NEON `Vec2d` wrapper with `fma` and a 4-accumulator dot, and an OpenMP task-based tree build with `taskwait`.
- You can write the halo exchange for a 1-D decomposed heat grid with `MPI_Sendrecv` and `MPI_PROC_NULL`, and build/run it with `mpicxx`/`mpirun`.
- You can describe a tiled GPU matmul kernel, the memory hierarchy it exploits, and compute whether moving a given kernel to the GPU pays off including transfer time.
- You can write a `.npy` file NumPy loads and a `.vtk` file ParaView opens, and list the reproducibility checklist.
- You can present the two case-study ladders with expected speedups and justify the order from the roofline model.
