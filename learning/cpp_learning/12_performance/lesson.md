# Chapter 12 — Performance

## What you'll be able to do after this chapter

- Time code correctly with `std::chrono::steady_clock` and a RAII `Timer`, and avoid the benchmarking traps (cold caches, no repeats, `-O0`, the optimizer deleting your work).
- Explain the memory hierarchy numerically and predict from loop order alone whether a matmul will be fast or slow.
- Choose between array-of-structs and struct-of-arrays for a particle sim, and say why one vectorizes.
- Find and remove hidden copies and hot-loop allocations — the two biggest wins in naive ML code.
- Read the compiler's vectorization report, write one NEON intrinsic loop, add an OpenMP `parallel for` with a reduction, and know when to call BLAS instead.
- Apply a fixed checklist: measure → algorithm → memory layout → allocation → vectorize → parallelize → BLAS.

## Why this matters for ML / numerics / sims

The reason you're learning C++ at all. A naive triple-loop matmul in C++ is ~50× faster than pure Python — and ~100× *slower* than what PyTorch calls under the hood. The gap between "compiles and runs" and "fast" is the content of this chapter, and every item in it is a measurable step: loop order alone is 5–10×, avoiding per-iteration allocation is often 10×, SIMD is 2–4×, threads are ~cores×, and a tuned BLAS is the rest. For simulations (N-body, FDTD, fluids) memory layout decides whether you get 60 frames per second or 6.

The rule that governs everything else: **measure first**. Most performance intuition is wrong; the profiler is not.

---

## 1. Measuring: `std::chrono` and a `Timer` class

`std::chrono::steady_clock` is monotonic (never goes backwards on NTP adjustments); `high_resolution_clock` is an alias for one of the others and not worth using; `system_clock` is wall-clock time for timestamps, not intervals.

```cpp
#include <chrono>
#include <cstdio>

class Timer {                                   // RAII: prints elapsed time on scope exit
    using clock = std::chrono::steady_clock;
    const char* label_;
    clock::time_point start_ = clock::now();
public:
    explicit Timer(const char* label) : label_(label) {}
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(clock::now() - start_).count();
    }
    ~Timer() { std::printf("%-24s %9.3f ms\n", label_, elapsed_ms()); }
};

// { Timer t("matmul 256"); c = matmul(a, b); }   // prints when the block ends
// Output: matmul 256                 14.212 ms
```

Python equivalent: `time.perf_counter()`; the RAII version is `with timer("label"):`.

Throughput numbers are more useful than raw times: a matmul does `2·n³` flops, so report `GFLOP/s = 2n³ / (seconds · 1e9)`. Your M-series core can do ~50–100 GFLOP/s in double per core with SIMD and FMA; naive code gets ~1–2.

---

## 2. Benchmarking pitfalls

| Pitfall | Symptom | Fix |
|---|---|---|
| `-O0` or no `CMAKE_BUILD_TYPE` | Everything 10–50× slow, conclusions meaningless | `-O2` minimum; `-O3 -mcpu=native` for benches |
| No warm-up | First run 2–5× slower (cold cache, page faults, CPU frequency ramp) | Run once untimed, then time |
| Single run | Noise ±30% | Repeat ≥5×, report min or median, not mean |
| Optimizer deleted the work | Suspiciously 0 ns | Use the result (print a checksum, accumulate into `volatile`, or `DoNotOptimize`) |
| Timing includes allocation | Measures `malloc`, not your kernel | Allocate outside the timed region |
| Debug iterators / asserts | libc++ hardening or `assert` in the loop | `-DNDEBUG`, no hardening flags in bench builds |
| Denormals | Sudden 100× slowdown when values → 0 | Detect with a counter; flush-to-zero or clamp |
| Thermal throttling / other processes | Later runs slower | Short benchmarks, quiet machine, look at variance |
| Tiny sizes | Function-call overhead dominates | Bench realistic sizes, or loop many times inside the timer |

The "optimizer deleted it" problem: if you compute `sum` and never use it, `-O2` removes the loop. Three fixes:

```cpp
// 1. Use the result (best: honest).
double s = dot(a, b, n); std::printf("%g\n", s);

// 2. volatile sink: the store cannot be elided.
volatile double sink = dot(a, b, n); (void)sink;

// 3. DoNotOptimize (what Google Benchmark does): an empty asm that "reads" the value.
template <class T> inline void DoNotOptimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }
DoNotOptimize(dot(a, b, n));
```

For serious work use [Google Benchmark](https://github.com/google/benchmark) or [nanobench](https://github.com/martinus/nanobench) (single header) — they handle repeats, statistics, and dead-code elimination for you.

---

## 3. Profilers

A benchmark says *how long*; a profiler says *where*.

| Tool | Platform | How |
|---|---|---|
| **Instruments** (Time Profiler) | macOS | `xcrun xctrace record --template 'Time Profiler' --launch -- ./bench`, then `open *.trace`. Or open Instruments.app, choose Time Profiler, pick your binary. Compile with `-O2 -g` so you see function names and lines. |
| `sample` | macOS | `sample ./bench 5` — a 5-second poor-man's profile printed as text. Zero setup. |
| `perf` | Linux | `perf record -g ./bench && perf report`; `perf stat -e cache-misses ./bench` gives hardware counters. |
| `gprof` | Linux/old | `-pg` instrumentation; obsolete, mentioned because tutorials still cite it. |
| `valgrind --tool=cachegrind` | Linux (not on Apple Silicon) | Simulates the cache; exact miss counts per line. |
| `-ftime-trace` | any | Profiles the *compiler*, not your program (Chapter 11). |

Always profile a `RelWithDebInfo` build (`-O2 -g`): `-O0` profiles show you the wrong bottlenecks, and without `-g` you get addresses instead of names.

---

## 4. The memory hierarchy

Everything about performance in numerics comes down to this table (Apple M-series, order-of-magnitude; x86 desktops are similar):

| Level | Size | Latency | Bandwidth | Notes |
|---|---|---|---|---|
| Registers | ~32 × 64-bit + 32 × 128-bit NEON | 0 cycles | — | The compiler allocates these |
| L1 data cache | 128 KB (P-core) | ~4 cycles ≈ 1 ns | ~100+ GB/s per core | Per core |
| L2 cache | 12–16 MB shared per cluster | ~15 cycles ≈ 5 ns | ~50 GB/s | Shared by 4–6 cores |
| System-level cache / L3 | 8–48 MB | ~40 ns | | |
| DRAM (unified memory) | 16–128 GB | ~100 ns | 100–800 GB/s total, far less per core | 100× slower than L1 |
| SSD | TB | ~50–100 µs | ~5 GB/s | 1000× slower than DRAM |

One FMA takes ~0.25 ns (4 per cycle pipelined). A DRAM access takes ~100 ns. So a loop that misses cache on every element runs at **1/400th** of peak. That is the whole story of loop order, tiling, SoA, and BLAS.

Python equivalent: none — NumPy hides this. But it is *why* `a @ b` in NumPy is fast and a Python `for` loop over the same data is slow.

---

## 5. Cache lines, spatial and temporal locality

Memory moves between levels in **64-byte cache lines** (128 B on Apple's L2). Touch one `double` and the neighbouring 7 arrive for free.

- **Spatial locality**: access memory contiguously. `a[i], a[i+1], a[i+2]` costs one miss per 8 doubles. `a[i], a[i+1000], a[i+2000]` costs one miss *each*.
- **Temporal locality**: reuse data while it's still in cache. Do all the work on a block before moving to the next one.

Hardware **prefetchers** detect sequential and strided patterns and fetch ahead, so a forward or backward sweep is nearly free even from DRAM. Random access (hash tables, linked lists, `std::map`) defeats them completely — which is why `std::vector` + linear search beats `std::map` up to hundreds of elements, and why `std::list` is almost never the right container.

---

## 6. Row-major traversal and the matmul loop-order experiment

Your `Matrix` stores `data_[i * cols + j]` — **row-major**: elements of a row are adjacent. (NumPy default is also row-major, "C order"; Fortran/MATLAB/Eigen-by-default are column-major.)

```cpp
// GOOD: inner loop walks along a row -> contiguous
for (i) for (j) sum += m(i, j);
// BAD: inner loop walks down a column -> stride of `cols` doubles, one miss per element
for (j) for (i) sum += m(i, j);
```

For `C = A·B`, three loop orders:

```cpp
// ijk (the textbook one): inner loop reads B down a column -> strided. SLOW.
for (i) for (j) { double s = 0; for (k) s += A(i,k) * B(k,j); C(i,j) = s; }

// ikj: inner loop reads a ROW of B and a ROW of C -> contiguous, vectorizable. 3-10x FASTER.
for (i) for (k) { double aik = A(i,k); for (j) C(i,j) += aik * B(k,j); }

// Blocked/tiled ikj: process BxB tiles so the working set stays in L1/L2.
for (ii = 0; ii < n; ii += B)
  for (kk = 0; kk < n; kk += B)
    for (jj = 0; jj < n; jj += B)
      for (i = ii; i < min(ii+B, n); ++i)
        for (k = kk; k < min(kk+B, n); ++k) {
          double aik = A(i,k);
          for (j = jj; j < min(jj+B, n); ++j) C(i,j) += aik * B(k,j);
        }
```

Typical `n = 512` results on an M-series core (`-O2`; run `example.cpp` for yours):

```
ijk (naive)         ~ 250 ms     ~1.1 GFLOP/s
ikj                 ~  55 ms     ~4.9 GFLOP/s
ikj blocked B=64    ~  40 ms     ~6.7 GFLOP/s
cblas_dgemm         ~   3 ms     ~90  GFLOP/s   (Accelerate; section 19)
```

The tile size `B` should make three `B×B` blocks fit in L1: `3·B²·8 bytes ≤ 128 KB` → `B ≈ 64`. The exact best value varies; measure. Same idea applies to your 2D stencil codes (FDTD, heat equation): sweep in the contiguous direction, tile if the grid doesn't fit in L2.

---

## 7. AoS vs SoA

**Array of Structs** — natural to write:

```cpp
struct Particle { double x, y, z, vx, vy, vz, mass; };   // 56 bytes
std::vector<Particle> ps(N);
for (auto& p : ps) p.x += dt * p.vx;                      // touches 56 B to update 8 B
```

**Struct of Arrays** — natural to vectorize:

```cpp
struct Particles {
    std::vector<double> x, y, z, vx, vy, vz, mass;
    explicit Particles(std::size_t n) : x(n), y(n), z(n), vx(n), vy(n), vz(n), mass(n) {}
};
for (std::size_t i = 0; i < N; ++i) ps.x[i] += dt * ps.vx[i];   // two contiguous streams; SIMD-friendly
```

| | AoS | SoA |
|---|---|---|
| Update one field for all particles | Loads whole struct (bandwidth waste 7×) | Perfect streaming |
| Use all fields of one particle | One cache line | 7 cache lines |
| Auto-vectorization | Hard (strided gathers) | Easy (unit stride) |
| Code readability | `p.x` | `ps.x[i]` |
| Adding a field | Free | Add a vector + resize logic |

For N-body force computation you touch `x,y,z,mass` of *every* other body per body — SoA with 4 arrays is 2–4× faster than AoS. Hybrid "AoSoA" (`struct { double x[8], y[8], z[8]; }`) is what production codes (GROMACS, LAMMPS) use. PyTorch tensors are SoA by construction: one array per tensor.

---

## 8. Avoiding hidden copies

Each row is a real bug found in learners' code:

| Pattern | Problem | Fix |
|---|---|---|
| `void f(Matrix m)` | Copies the whole matrix per call | `const Matrix& m` |
| `for (auto row : rows)` | Copies each element | `for (const auto& row : rows)` |
| `auto m = mats[i];` | Copies | `const auto& m = mats[i];` (or `auto&` to mutate) |
| `v.push_back(Matrix(...))` in a loop without `reserve` | log₂(n) reallocations, each copying/moving everything | `v.reserve(n)` first |
| `v.push_back(Matrix(r, c))` | Constructs a temporary then moves | `v.emplace_back(r, c)` constructs in place |
| `return std::move(local);` | Defeats copy elision (pessimisation) | `return local;` |
| `std::string name = obj.name();` when `name()` returns `const std::string&` | Copies | `const std::string& name = obj.name();` or `auto&` |
| `Matrix c = a + b;` where `operator+` returns by value | **Fine** — guaranteed elision / move | Nothing; returning by value is idiomatic |
| `m = m.transpose();` | Allocates a new matrix | Fine unless in a hot loop; then `transpose_into(dst)` |

Move semantics (`../07_move_semantics_and_smart_pointers/lesson.md`) mean returning a `std::vector`/`Matrix` by value is a pointer swap, not a copy. Don't fear it. Fear `auto` in range-for over containers of non-trivial types.

---

## 9. Allocation in hot loops

**This is the single biggest win in naive ML code.** `new`/`malloc` costs ~50–100 ns plus cache pollution, and it serialises threads through the allocator lock.

```cpp
// BAD: allocates two temporaries per training step
for (int step = 0; step < 10000; ++step) {
    Matrix h = matmul(X, W1);            // allocates
    Matrix out = matmul(relu(h), W2);    // relu allocates, matmul allocates
    ...
}

// GOOD: preallocate once, compute in place
Matrix h(n, hidden), out(n, classes);
for (int step = 0; step < 10000; ++step) {
    matmul_into(X, W1, h);               // writes into existing storage
    relu_inplace(h);
    matmul_into(h, W2, out);
    ...
}
```

Give every expensive operation an `_into(dst)` variant that takes a preallocated output, and build the convenient value-returning version on top of it. `std::vector::clear()` keeps capacity — reuse the same vector across iterations instead of making a new one. PyTorch does exactly this with its caching allocator and `out=` arguments.

Detect it: run under `sample`/Instruments and look for `malloc`/`free` near the top; or count with a global allocation counter wrapped around `operator new` in a debug build.

---

## 10. `constexpr` and compile-time computation

Anything the compiler can compute at compile time costs nothing at run time. `constexpr` functions run at compile time when given constant arguments.

```cpp
constexpr double pi = 3.14159265358979323846;
constexpr std::size_t kTile = 64;

constexpr double pow_int(double x, int n) {          // C++17: loops allowed in constexpr
    double r = 1.0;
    for (int i = 0; i < n; ++i) r *= x;
    return r;
}
constexpr double kInvDx2 = 1.0 / pow_int(0.01, 2);   // computed by the compiler: 10000
static_assert(kInvDx2 == 10000.0);

// A lookup table built at compile time:
template <std::size_t N>
constexpr std::array<double, N> make_sin_table() {
    std::array<double, N> t{};
    for (std::size_t i = 0; i < N; ++i) t[i] = i * (2 * pi / N);   // std::sin isn't constexpr in C++17; store angles
    return t;
}
```

Use `constexpr` for physical constants, grid sizes, tile sizes, and any pure integer math on them. It also lets the compiler unroll and vectorize loops whose bounds it knows exactly.

---

## 11. Inlining and `-O3`

Inlining replaces a call with the function body, removing call overhead and — more importantly — letting the optimizer see through the call (constant propagation, vectorization). The compiler decides; the `inline` keyword is about the ODR (Chapter 11), not a command. Small functions in headers (`operator()`, `dot3`) inline at `-O2`.

Blockers to inlining: function defined in another TU (use headers or `-flto` link-time optimization); virtual calls (`../09_inheritance_and_polymorphism/lesson.md`) — the compiler can't know the target; `std::function` — same problem; recursion.

`-O3` vs `-O2`: `-O3` adds more aggressive inlining, loop unrolling and vectorization of loops with unknown trip count. Sometimes 15% faster, sometimes slower (code bloat). Measure. `-mcpu=native` (Apple Silicon; `-march=native` on x86) matters more: it unlocks the full instruction set.

---

## 12. Aliasing, `restrict`, and why `std::vector` loops can be slower

```cpp
void axpy(double* y, const double* x, double a, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) y[i] += a * x[i];
}
```

Could `y` and `x` overlap? The compiler doesn't know, so it must assume writing `y[i]` might change `x[i+1]` — which blocks reordering and vectorizing... except that compilers emit a *runtime overlap check* and two loop versions, so this specific case is fine. It gets worse with more pointers or when the pointers are class members (`this->data_`): the compiler must assume any store through one pointer may modify `this->size_` or another member, and reloads it each iteration.

C99 has `restrict` ("I promise this pointer doesn't alias"). C++ does not, but every compiler accepts `__restrict`:

```cpp
void axpy(double* __restrict y, const double* __restrict x, double a, std::size_t n);
```

Practical patterns:
- Copy `size()` and `data()` into locals before a hot loop: `const std::size_t n = v.size(); double* p = v.data();`. This is why hand-written kernels take raw pointers + length — not because `std::vector` is slow, but because the loop can't see that `v.size()` won't change.
- Never write to one vector while reading another that might be the same object; if you must support `y = y + x`, check for self-aliasing at the top.

---

## 13. Auto-vectorization and how to check it

Modern CPUs have SIMD registers: NEON on ARM is 128-bit (2 doubles or 4 floats per instruction); AVX2 on x86 is 256-bit; AVX-512 is 512-bit. The compiler auto-vectorizes simple loops at `-O2`. Ask it what happened:

```sh
c++ -std=c++17 -O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize -c kernel.cpp
```

```
kernel.cpp:12:5: remark: vectorized loop (vectorization width: 2, interleaved count: 4) [-Rpass=loop-vectorize]
kernel.cpp:20:5: remark: loop not vectorized [-Rpass-missed=loop-vectorize]
kernel.cpp:20:5: remark: loop not vectorized: value that could not be identified as reduction is used outside the loop
```

What vectorizes: unit-stride loops with no loop-carried dependence, simple bodies, no function calls (except inlined ones and `std::sqrt`/`fma`), no early `break`. Floating-point **reductions** (`sum += a[i]`) do *not* vectorize by default because reassociating the sum changes rounding — that needs `-ffast-math`, `#pragma clang loop vectorize(assume_safety)`, or a manual 4-accumulator unrolling (which is what you'd do anyway).

What doesn't: `std::vector<bool>`, loops with `if` that can't be turned into a select, indirect indexing `a[idx[i]]` (gathers), loops over `std::list`/`std::map`, anything through `virtual`.

---

## 14. SIMD intrinsics: one NEON example

When the auto-vectorizer refuses (reductions!), you can write the SIMD yourself. On Apple Silicon: `<arm_neon.h>`. Types are `float64x2_t` (2 doubles), `float32x4_t` (4 floats); functions are `vXXXq_f64` (`q` = 128-bit "quad-word").

```cpp
#include <arm_neon.h>

double dot_neon(const double* a, const double* b, std::size_t n) {
    float64x2_t acc0 = vdupq_n_f64(0.0);            // two independent accumulators hide FMA latency
    float64x2_t acc1 = vdupq_n_f64(0.0);
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        float64x2_t a0 = vld1q_f64(a + i),     b0 = vld1q_f64(b + i);       // load 2 doubles each
        float64x2_t a1 = vld1q_f64(a + i + 2), b1 = vld1q_f64(b + i + 2);
        acc0 = vfmaq_f64(acc0, a0, b0);           // acc0 += a0 * b0  (fused multiply-add, lane-wise)
        acc1 = vfmaq_f64(acc1, a1, b1);
    }
    double s = vaddvq_f64(vaddq_f64(acc0, acc1)); // horizontal add of both lanes
    for (; i < n; ++i) s += a[i] * b[i];          // scalar tail
    return s;
}
```

Intrinsic families: `vld1q_*` load, `vst1q_*` store, `vaddq_*`, `vmulq_*`, `vfmaq_*` (fma), `vaddvq_*` (horizontal sum), `vmaxq_*`, `vdupq_n_*` (broadcast). Guard with `#if defined(__ARM_NEON)`; the x86 equivalents live in `<immintrin.h>` (`__m256d`, `_mm256_fmadd_pd`). Portable wrappers exist (`std::experimental::simd`, `xsimd`, `highway`).

Result changes: the 4-accumulator sum rounds differently from the sequential sum (usually *more* accurately). Your tests must use tolerances (Chapter 11).

---

## 15. Parallelism: `std::execution::par`, OpenMP, `std::thread`

**`std::execution::par`** (C++17): `std::sort(std::execution::par, v.begin(), v.end())`, `std::transform_reduce(std::execution::par, ...)`. On libc++ (Apple) this requires an additional backend (`-fexperimental-library` and/or TBB via `brew install tbb`); on GCC/libstdc++ it needs `-ltbb`. Know it exists; don't rely on it for now.

**OpenMP** — the standard for shared-memory loop parallelism in HPC. One pragma:

```cpp
#include <omp.h>   // guard with #ifdef _OPENMP

double energy = 0.0;
#pragma omp parallel for reduction(+:energy) schedule(static)
for (std::ptrdiff_t i = 0; i < n; ++i) {          // OpenMP 2.x needs a signed loop variable
    energy += 0.5 * m[i] * (vx[i]*vx[i] + vy[i]*vy[i]);
}
```

`parallel for` splits iterations across threads; `reduction(+:energy)` gives each thread a private copy and sums at the end (without it, `energy +=` is a data race). Other clauses: `private(x)`, `collapse(2)` for nested loops, `schedule(dynamic, 64)` for uneven work, `#pragma omp simd` to force vectorization.

Apple clang ships without the OpenMP runtime. To use it:

```sh
brew install libomp
c++ -std=c++17 -O2 -Xpreprocessor -fopenmp \
    -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp example.cpp -o ex_demo
OMP_NUM_THREADS=8 ./ex_demo
```

Without those flags `_OPENMP` is undefined, the pragma is ignored (with a `-Wunknown-pragmas` warning under `-Wall` — the example guards the pragma with `#ifdef _OPENMP` to stay warning-free), and the loop runs serially. In CMake: `find_package(OpenMP)` + `target_link_libraries(t PRIVATE OpenMP::OpenMP_CXX)`.

**`std::thread`** — manual threads for structure rather than loops:

```cpp
#include <thread>
#include <vector>

void parallel_for(std::size_t n, unsigned nthreads, const std::function<void(std::size_t, std::size_t)>& body) {
    std::vector<std::thread> pool;
    const std::size_t chunk = (n + nthreads - 1) / nthreads;
    for (unsigned t = 0; t < nthreads; ++t) {
        std::size_t lo = t * chunk, hi = std::min(n, lo + chunk);
        if (lo < hi) pool.emplace_back(body, lo, hi);
    }
    for (auto& th : pool) th.join();               // a joinable thread destroyed without join() -> std::terminate
}
// unsigned hw = std::thread::hardware_concurrency();   // e.g. 10 on M1 Pro
```

Creating a thread costs ~10–50 µs, so don't spawn per-iteration: spawn once per big chunk of work, or use a **thread pool** (a fixed set of workers pulling tasks from a queue — write one as an exercise, or use `BS::thread_pool`, `taskflow`). C++20 adds `std::jthread` (auto-joins).

---

## 16. Data races, `std::atomic`, `std::mutex`, false sharing

A **data race** — two threads access the same memory, at least one writes, no synchronisation — is UB. Not "sometimes wrong": UB. `-fsanitize=thread` finds them.

```cpp
#include <atomic>
#include <mutex>

std::atomic<long> counter{0};       // lock-free increments; fine for counters
counter.fetch_add(1, std::memory_order_relaxed);

std::mutex mu;                       // for anything bigger than one word
{
    std::lock_guard<std::mutex> lock(mu);    // RAII: unlocks on scope exit, even on throw
    shared_vector.push_back(x);
}
```

**Never** accumulate a floating-point sum with `std::atomic<double>` in a hot loop — every add serialises all threads through one cache line. Per-thread partial sums combined at the end (which is exactly what OpenMP's `reduction` does).

**False sharing**: two threads write to *different* variables that share a 64-byte cache line. Each write invalidates the other core's copy; the line ping-pongs. Symptom: adding threads makes it *slower*. Fix: pad per-thread data to a cache line — `alignas(64) struct Partial { double sum; };` or `std::hardware_destructive_interference_size` (C++17, may be missing on libc++ — use 64 or 128 on Apple).

---

## 17. When to call a BLAS

BLAS (Basic Linear Algebra Subprograms) is the 40-year-old Fortran API that every numerical library sits on: `dgemm` = double general matrix multiply. Vendors ship implementations tuned per CPU: **Accelerate** on macOS (uses the AMX matrix coprocessor on M-series — that's how it hits 1+ TFLOP/s), OpenBLAS, Intel MKL, BLIS. NumPy `@` and PyTorch `torch.mm` on CPU call one of these. This is "how PyTorch gets its speed": not clever C++, but calling a kernel that took experts years to tune, plus doing it on many cores.

```cpp
#include <Accelerate/Accelerate.h>     // macOS. Compile with: -framework Accelerate
// C = alpha * A @ B + beta * C, all row-major, A is m x k, B is k x n
cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
            m, n, k, 1.0, A, k, B, n, 0.0, C, n);
```

The `lda/ldb/ldc` arguments are "leading dimensions" (row stride for row-major) — they let you multiply submatrices without copying. `sgemm` is the `float` version. Level-1 BLAS (`cblas_ddot`, `cblas_daxpy`) is memory-bound and rarely worth calling; level-3 (`gemm`) is where the 100× lives.

Write your own matmul *first* — you need to understand it, and small matrices (< 32×32) are often faster with a simple inlined loop. Then, when the matrices are big, call `cblas_dgemm` and move on to the interesting part of your project.

---

## 18. Float vs double throughput, `-ffast-math`

| | `float` | `double` |
|---|---|---|
| Size | 4 B | 8 B |
| NEON lanes | 4 | 2 |
| Precision | ~7 digits | ~16 digits |
| Throughput | 2× double on SIMD; 2× less bandwidth | baseline |
| Use | ML weights/activations (PyTorch default), graphics, most sim state | Accumulators, energy/time integration, anything you'll subtract nearly-equal values from |

Rule: store in `float`, accumulate in `double` (or Kahan). A 1M-element `float` sum has ~1e-4 relative error; in `double` ~1e-12. Half precision (`_Float16`/`__fp16`, bfloat16) doubles throughput again on hardware that supports it — this is what training on GPUs uses.

`-ffast-math` (see `../11_headers_build_cmake_testing/lesson.md` §15): enables reassociation (reductions vectorize — 2–4×), assumes no NaN/inf (your `isnan` checks are deleted), reciprocal approximations. Use per-file or per-loop (`#pragma clang fp reassociate(on)`), never globally, and never on Kahan summation or on a solver's convergence checks.

---

## 19. Amdahl's law

If a fraction `p` of the runtime is parallelisable across `N` cores, the speedup is bounded:

```
S(N) = 1 / ((1 - p) + p / N)
```

| `p` | Max speedup (N→∞) | Speedup at N=8 |
|---|---|---|
| 0.50 | 2× | 1.8× |
| 0.90 | 10× | 4.7× |
| 0.95 | 20× | 5.9× |
| 0.99 | 100× | 7.5× |

The serial 10% of your program caps you at 10× no matter how many cores you buy. This is why you optimise the *serial* code (loop order, allocation, SIMD) before you parallelise — and why "it doesn't scale" is usually a serial section (allocation, IO, a mutex), not the parallel loop.

---

## 20. The performance checklist for your projects

Do these **in order**. Each step is typically worth more than all the steps after it, and each is cheaper.

1. **Measure.** `-O2` (or `-O3 -mcpu=native`), `RelWithDebInfo`, warm-up, ≥5 repeats, report GFLOP/s or elements/s. Profile with Instruments/`sample`. Find the *one* hot function.
2. **Algorithm.** O(N²) → O(N log N) beats everything below. Barnes–Hut for N-body, FFT for convolution, sparse storage for sparse matrices, better step size control for ODEs.
3. **Memory layout.** Row-major-friendly loop order; tile big matmuls/stencils; SoA for particle fields; `std::vector` not `std::list`/`std::map` in hot paths; `float` if precision allows.
4. **Allocation.** No `new`/`vector` construction in loops. `_into` variants, `reserve`, reuse buffers. Check with a profiler that `malloc` is gone.
5. **Vectorize.** Check `-Rpass-missed=loop-vectorize`. Remove aliasing (`data()`/`size()` to locals), split reductions into 4 accumulators, hand-write NEON for the top kernel only.
6. **Parallelize.** OpenMP `parallel for` on the outermost loop with enough work (>10 µs per iteration chunk). Watch for false sharing. Amdahl says fix the serial part first.
7. **BLAS / library.** `cblas_dgemm` for large matmul; FFTW/Accelerate's vDSP for FFT; LAPACK for solves and eigenvalues. You've learned enough by writing your own once.

Stop when it's fast enough. Every step costs readability.

---

## Gotchas and undefined behavior

- **Benchmarking at `-O0`.** All conclusions are wrong. Check your build type.
- **Dead code elimination.** A 0 ns result means the optimizer removed your loop. Use the result.
- **Timing the first run only.** Cold caches and CPU frequency scaling.
- **Data race = UB**, even on a `bool`, even "just a flag". Use `std::atomic<bool>` or a mutex.
- **`std::thread` destroyed while joinable → `std::terminate`.** Always `join()` (or `detach()`, rarely).
- **OpenMP loop variable must be a signed integer** (OpenMP < 3.0; clang accepts `std::size_t` in newer versions, but stick to `std::ptrdiff_t`/`long` for portability).
- **`reduction` forgotten** → race on the accumulator → wrong answer that changes each run.
- **False sharing** makes parallel code slower than serial. Pad per-thread data.
- **`-ffast-math` deletes `isnan`/`isinf` checks** and breaks Kahan summation.
- **Misaligned NEON loads** are legal on ARM (unlike some x86 aligned loads), but `vld1q_f64` on `n` not divisible by 2 reads past the end → UB. Always write the scalar tail.
- **Signed overflow in index math is UB**; unsigned wrap is not. `int` indices with `-O2` can produce surprising "optimizations". Use `std::size_t`/`std::ptrdiff_t`.
- **`__restrict` lie**: if the pointers do alias, results are UB (and silently wrong).
- **`std::atomic<double>` in a hot reduction** is correct but 100× slower than per-thread partials.
- **Denormals**: values below ~2e-308 (double) / 1e-38 (float) run 10–100× slower on some cores. Clamp or flush-to-zero if your sim decays to zero.
- **Growing `std::vector` of non-`noexcept`-movable types** copies on every reallocation (Chapter 10 §11).

---

## Common mistakes checklist

- [ ] Benchmarks built with `-O2`/`-O3`, `-DNDEBUG`, and `-mcpu=native`; warm-up + repeats; result used.
- [ ] Profiled before optimising; know the top function.
- [ ] Inner loops walk contiguous memory (row-major aware); big matmuls use `ikj` or tiling.
- [ ] Particle/grid fields stored SoA when updated field-wise.
- [ ] No allocation inside the training loop / time step; `_into` variants and preallocated buffers.
- [ ] `const&` for parameters, `const auto&` in range-for, `reserve`/`emplace_back`.
- [ ] Vectorization report checked; reductions unrolled or hand-vectorized where it matters.
- [ ] OpenMP guarded by `#ifdef _OPENMP`; every shared accumulator has a `reduction`.
- [ ] Threads joined; per-thread data padded; no `atomic<double>` in hot loops.
- [ ] Large matmul delegated to `cblas_dgemm`.
- [ ] `-ffast-math` only on specific files, with results validated against the strict build.

---

## You can move on when...

- You can explain why `ikj` beats `ijk` for row-major matmul using the words "stride", "cache line", and "prefetcher".
- You can write a `Timer` RAII class and a correct benchmark harness (warm-up, repeats, min, `DoNotOptimize`).
- You can predict which of AoS/SoA is faster for "update all positions" and for "print one particle", and why.
- You can list the compiler flag to see which loops vectorized, and name two reasons a loop fails to.
- You can write a NEON dot product with a scalar tail, and an OpenMP parallel-for with a reduction, and say what breaks without the reduction.
- You can state Amdahl's law and use it to explain why a 90%-parallel program tops out at 10×.
- You know the seven-step checklist by heart and why the order matters.
