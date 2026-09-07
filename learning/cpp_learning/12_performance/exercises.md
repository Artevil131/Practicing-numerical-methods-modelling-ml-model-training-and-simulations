# Chapter 12 — Exercises

Write each exercise as `ex12_K.cpp` in this folder. Benchmarks must be compiled with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex12_K ex12_K.cpp` (add `-O3 -mcpu=native` when the exercise
says so). For every timing exercise: warm up once, repeat at least 5 times, report the minimum,
and *use* the result (print a checksum) so the optimizer can't delete the work. Record your
numbers in a comment at the top of the file — the point is the measurement, not the code.

---

**12.1 — A `Timer` and a `bench()` harness**
Write a RAII `Timer` class over `std::chrono::steady_clock` and a function
`template <class F> double bench_ms(F&& f, int repeats = 7)` that runs `f` once untimed, then `repeats` times, and returns the minimum in milliseconds. Include a `DoNotOptimize` helper using the empty-asm trick. Demonstrate the dead-code problem: time a `sum of 10M doubles` loop twice — once discarding the result (observe ~0 ms at `-O2`) and once through `DoNotOptimize` — and comment on the difference. Then compile with `-O0` and record the slowdown.

Example output: `sum (discarded)   0.000 ms` / `sum (kept)   4.812 ms`.

<details><summary>Hint</summary>
`std::chrono::duration<double, std::milli>(end - start).count()`. Forward `f` with `std::forward<F>(f)()`. The minimum is more stable than the mean because noise only ever adds time.
</details>

---

**12.2 — Row vs column traversal**
Allocate a `4096 × 4096` `std::vector<double>` (row-major, 128 MB). Time summing it (a) row by row (`i` outer, `j` inner) and (b) column by column (`j` outer, `i` inner). Then repeat for `512 × 512` (2 MB, fits in L2). Report the ratio for both sizes and explain in a comment why the ratio is much larger for the big matrix.

Example: `4096: row 12 ms, col 210 ms (17x)` / `512: row 0.18 ms, col 0.31 ms (1.7x)`.

<details><summary>Hint</summary>
Stride of 4096 doubles = 32 KB between consecutive accesses: every access is a new cache line and the prefetcher can't keep up once the data exceeds L2.
</details>

---

**12.3 — The matmul loop-order experiment**
Implement `matmul_ijk`, `matmul_ikj`, `matmul_jik`, `matmul_kij` and a tiled `matmul_blocked(A, B, C, n, tile)` on raw `double*` arrays, `n = 512`. Verify they all produce the same result (`allclose`, rtol 1e-12). Benchmark each and report GFLOP/s (`2n³ / seconds / 1e9`). Try tile sizes 16, 32, 64, 128 and find the best. Then recompile with `-O3 -mcpu=native` and compare. Write a paragraph in a comment explaining the ranking in terms of stride and cache lines.

<details><summary>Hint</summary>
In `ikj`, both `C[i][j]` and `B[k][j]` are unit-stride in the inner loop, and `A[i][k]` is a loop invariant you should hoist into a local. Tile so that three `tile × tile` blocks of doubles fit in L1 (128 KB).
</details>

---

**12.4 — AoS vs SoA particle update**
Define `struct Particle { double x,y,z,vx,vy,vz,mass; }` and a `Particles` SoA struct with seven `std::vector<double>`. For `N = 4'000'000`, time (a) `x += dt*vx; y += dt*vy; z += dt*vz` over all particles, and (b) computing the total kinetic energy, for both layouts. Then compile with `-Rpass=loop-vectorize -Rpass-missed=loop-vectorize` and record which loops vectorized. Explain the results with the ratio of bytes loaded to bytes used.

<details><summary>Hint</summary>
In AoS each position update loads a 56-byte struct to modify 24 bytes. In SoA the six arrays stream contiguously and the compiler can vectorize width 2 (NEON doubles).
</details>

---

**12.5 — Hunt the hidden copies**
Write a deliberately naive program: a `std::vector<Matrix>` of 200 `256×256` matrices filled in a loop with `push_back(Matrix(256,256))` and no `reserve`; a function `double frob(Matrix m)` taking by value; a `for (auto m : mats)` loop calling it; and a `Matrix` whose move constructor is *not* `noexcept`. Add a static counter to `Matrix`'s copy constructor. Print the copy count and time, then fix each issue one at a time (`reserve`, `emplace_back`, `const&`, `const auto&`, `noexcept` on the move ops), recording the copy count and time after each fix in a comment table.

Example first line: `copies=1199 time=310 ms`; final: `copies=0 time=41 ms`.

<details><summary>Hint</summary>
`std::vector` growth copies (instead of moves) when `std::is_nothrow_move_constructible_v<Matrix>` is false. Each fix should remove a specific, predictable number of copies — check your prediction.
</details>

---

**12.6 — Allocation in the hot loop**
Write `Matrix matmul(const Matrix&, const Matrix&)` (returns a new matrix) and `void matmul_into(const Matrix&, const Matrix&, Matrix& out)`. Simulate a "training loop": 2000 iterations of `h = matmul(X, W)` with `X` `64×256`, `W` `256×256`. Time the returning version vs the `_into` version with a preallocated `h`. Then wrap `operator new`/`operator delete` (global replacement) with a counter and print the number of allocations in each version. Run `sample ./ex12_6 3` (macOS) while the slow version runs and paste the top three symbols into a comment.

<details><summary>Hint</summary>
Replace `void* operator new(std::size_t n)` to call `std::malloc` and increment a global counter (and `operator delete(void*) noexcept` to `std::free`). Keep the counter a plain `std::size_t` — this exercise is single-threaded.
</details>

---

**12.7 — Reading the vectorizer, then beating it with NEON**
Write `double dot_scalar(const double*, const double*, std::size_t)`, `dot_unrolled4` (four independent accumulators), and `dot_neon` using `<arm_neon.h>` (`vld1q_f64`, `vfmaq_f64`, `vaddvq_f64`) with a scalar tail — all behind `#if defined(__ARM_NEON)` where needed. Compile with `-O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize` and record why the scalar reduction did *not* vectorize. Benchmark all three at `n = 1<<20` (in L2) and `n = 1<<26` (DRAM); explain why the NEON advantage shrinks for the big size. Verify the three results agree to `1e-10` relative — and note that they are not bit-identical.

<details><summary>Hint</summary>
Floating-point reduction reordering changes results, so the compiler refuses without `-ffast-math`. At DRAM sizes the loop is memory-bandwidth-bound: ~8 bytes per FMA, and a core can't pull more than ~20–50 GB/s.
</details>

---

**12.8 — Parallel N-body forces with OpenMP (sims)**
Implement the O(N²) gravitational acceleration computation for `N = 4000` bodies stored SoA (`x, y, z, m` → `ax, ay, az`) with softening `eps = 1e-3`. Add `#pragma omp parallel for schedule(static)` on the outer loop, guarded by `#ifdef _OPENMP`, and a separate `#pragma omp parallel for reduction(+:ke)` for kinetic energy. Build once without OpenMP and once with `brew install libomp` and `-Xpreprocessor -fopenmp -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp`. Report time vs `OMP_NUM_THREADS=1,2,4,8` and compute the parallel efficiency. Remove the `reduction` clause once to see the wrong answer (put it back).

Example: `threads=1 412 ms | threads=4 108 ms (eff 95%) | threads=8 62 ms (eff 83%)`.

<details><summary>Hint</summary>
Each outer iteration writes only `ax[i], ay[i], az[i]` and reads everything else — no race without a reduction. Use `1/sqrt(r2 + eps2)` cubed; hoist `x[i], y[i], z[i]` into locals. The efficiency drop at 8 threads on Apple Silicon is partly the efficiency cores.
</details>

---

**12.9 — Your matmul vs Accelerate's `cblas_dgemm` (ML)**
Write your best `matmul` from 12.3 (tiled `ikj`, `-O3 -mcpu=native`) and compare against `cblas_dgemm` from `<Accelerate/Accelerate.h>` (compile with `-framework Accelerate`; guard the include with `#ifdef __APPLE__`). Sizes 64, 128, 256, 512, 1024, 2048. Print a table of ms and GFLOP/s for both, plus the speedup. Verify equality with `allclose` (rtol 1e-10). Then do the same in `float` with `cblas_sgemm` and compare `float` vs `double` throughput for both implementations. In a comment, estimate what fraction of the machine's peak your code reaches.

<details><summary>Hint</summary>
`cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1.0, A, k, B, n, 0.0, C, n)`. Peak per P-core in double is roughly 4 FMA/cycle × 2 lanes × 2 flops × ~3.5 GHz ≈ 56 GFLOP/s; Accelerate exceeds it because it uses the AMX units.
</details>

---

**12.10 — The full checklist on a 2D heat-equation stencil (numerics / sims)**
Implement the explicit finite-difference heat equation `u_new[i][j] = u[i][j] + r*(u[i-1][j] + u[i+1][j] + u[i][j-1] + u[i][j+1] - 4u[i][j])` on a `2048 × 2048` grid for 200 steps, Dirichlet boundaries. Start naive: `std::vector<std::vector<double>>`, allocate `u_new` every step, `j` outer / `i` inner. Then apply the checklist one step at a time, timing after each: (1) measure & profile with `sample`; (2) algorithm — none here, note it; (3) memory layout — flat row-major `std::vector<double>`, correct loop order; (4) allocation — two preallocated buffers, `std::swap` them; (5) vectorize — check the report, use `data()`/locals so the inner loop vectorizes, try `float`; (6) parallelize — OpenMP on the row loop (guarded); (7) library — note what you'd call (e.g. vDSP / a sparse solver / FFT for the implicit version). Verify each version against the naive one (`allclose`). Produce a table of step → time → speedup in a comment. Plot nothing; the table is the deliverable.

Example table row: `step 4 (no alloc)   1.92 s   9.4x`.

<details><summary>Hint</summary>
Keep `r = dt/dx² ≤ 0.25` for stability. The whole grid is 32 MB — bigger than L2 — so after the layout fix you are memory-bound; tiling in `j` with a few rows of reuse is the next win if you want to go beyond the checklist. Total energy (sum of `u`) is conserved only up to boundary flux; use it as a sanity check, not an exact test.
</details>
