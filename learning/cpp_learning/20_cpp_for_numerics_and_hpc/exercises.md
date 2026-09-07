# Chapter 20 — Exercises

Write each exercise as `ex20_K.cpp` in this folder. Baseline compile line:
`c++ -Wall -Wextra -std=c++17 -O2 -o ex20_K ex20_K.cpp`; add the flags each exercise names
(`-framework Accelerate`, `-Xpreprocessor -fopenmp ... -lomp`, `-I$(brew --prefix eigen)/include/eigen3`,
`mpicxx`). Guard optional dependencies (`#ifdef USE_ACCELERATE`, `#ifdef _OPENMP`, `#if defined(__ARM_NEON)`)
so the baseline line always compiles. Every timing exercise: `-O3 -mcpu=native -DNDEBUG`, warm-up,
≥ 5 repetitions, report the minimum, use the result (`DoNotOptimize`), and record GFLOP/s or GB/s
next to the roofline bound for that kernel in a comment. Verify every numerical result against a
reference (your own naive kernel, NumPy, or an analytic solution) — in this chapter "it ran" is not
"it is right".

---

**20.1 — `cblas_dgemm` vs your kernels, with the roofline**
Under `#ifdef USE_ACCELERATE` (`-DUSE_ACCELERATE -framework Accelerate`; Linux `-DUSE_OPENBLAS -lopenblas` with `<cblas.h>`), benchmark `cblas_dgemm` against your `ikj` and blocked kernels for `n ∈ {128, 256, 512, 1024, 2048}`, single-threaded (`VECLIB_MAXIMUM_THREADS=1`) and default threading. Verify `rel_err < n·ε` against `ikj`. Then compute `C = A·Bᵀ` (a) by transposing `B` into a temporary and (b) with `CblasTrans` and the right `ldb`; confirm equality and compare times. Finally multiply the 256×256 *sub-block* at `(100, 100)` of a 1024×1024 matrix without copying (pointer offset + `lda = 1024`) and check it against a copied version. Report GFLOP/s for every row of the table and, for `dgemm`, the fraction of your core's double-precision peak (compute the peak from your clock and 4 FMA pipes × 2 lanes × 2).

Example: `n=1024  ikj 18.2 GFLOP/s   blocked 24.1   dgemm(1 thr) 118.4 (=~85% of 140 peak?)   dgemm(all) 640`.

<details><summary>Hint</summary>
`sysctl -n hw.cpufrequency` is not available on Apple silicon; estimate the P-core clock from `powermetrics` or assume ~4 GHz and say so. If the single-thread `dgemm` exceeds your computed NEON peak, Accelerate is using the AMX coprocessor — note it.
</details>

---

**20.2 — LAPACK from C++: `dgesv_` and `dsyev_` with every pitfall handled**
Under `#ifdef USE_ACCELERATE`: (a) build a random 6×6 row-major `A` and `b = A·x_true`; solve with `dgesv_` — first *incorrectly* by passing the row-major array directly (observe you solved `Aᵀx = b`; verify by computing the residual of both interpretations), then correctly by transposing to column-major; check `‖x − x_true‖ < 1e-10`, and demonstrate `info > 0` on a singular matrix (duplicate a row). (b) Symmetrize `S = A + Aᵀ` and compute its eigendecomposition with `dsyev_` using a *workspace query*; verify `S·vₖ = λₖ·vₖ` for every k and `VᵀV = I`; confirm eigenvalues are ascending. (c) Wrap both in C++ functions `solve(ConstMatrixView, span) → optional<vector>` and `eigh(...) → struct {values, vectors}` that hide the Fortran conventions (row-major in, row-major out, `info` mapped to `optional`/exception). Compare eigenvalues with `np.linalg.eigvalsh` on the same matrix dumped as `.npy` (exercise 20.6).

<details><summary>Hint</summary>
Define `ACCELERATE_NEW_LAPACK` before the include to silence deprecation warnings on recent macOS. `dsyev_`'s eigenvectors come back as *columns* of the column-major array, i.e. contiguous runs of `n` doubles — convenient. On Linux use `LAPACKE_dgesv(LAPACK_ROW_MAJOR, ...)` for (a) and note that LAPACKE transposes internally.
</details>

---

**20.3 — Eigen: `Map`, expression templates and the `auto` trap**
Install Eigen (`brew install eigen`). (a) Wrap your P01 `Matrix`'s buffer with `Eigen::Map<Eigen::Matrix<double, Dynamic, Dynamic, RowMajor>>` and compute `A*B + C` into another `Map` — zero copies; verify against your kernel. (b) Time `MatrixXd r = A*B + C;` vs `r.noalias() = A*B; r += C;` vs a hand loop for `n = 512`, and `-DNDEBUG` vs not. (c) Demonstrate the `auto` trap: `auto e = A * B;` then modify `A`, then `MatrixXd r = e;` — show the result reflects the *modified* `A`; and `auto row = M.row(0); M.setZero();` — show `row` changed. (d) Demonstrate the aliasing bug `A = A.transpose()` on a 3×3 (print the corrupted matrix) and the two fixes. (e) Solve a 500×500 SPD system with `A.ldlt().solve(b)`, `ConjugateGradient` on the sparse version, and your own CG from 20.7; compare residuals and times. Comment on which of your own kernels Eigen made unnecessary.

<details><summary>Hint</summary>
`Eigen::Map<...> m(ptr, rows, cols)`; for the RowMajor `Map` you need the `RowMajor` option in the *matrix type*, not the map. `Eigen::internal::` nothing — use only the public API. Compile with `-I$(brew --prefix eigen)/include/eigen3`.
</details>

---

**20.4 — CSR, SpMV and the 2-D Laplacian**
Build the 5-point Laplacian of an `m×m` grid (n = m², Dirichlet boundaries) three ways: dense (`m ≤ 64` only), COO triplets → CSR (sort by row then column, sum duplicates), and *matrix-free* (a function computing `A·x` directly from the stencil). Verify all three agree on a random `x` to 1e-14. Benchmark SpMV for `m ∈ {64, 256, 1024}` and report GFLOP/s *and* GB/s (bytes = 8·nnz values + 4·nnz `uint32` indices + 8·n for `x` + 8·n for `y`); compare with your measured STREAM bandwidth (a `y = a·x + y` loop over 100 MB) to show SpMV is bandwidth-bound. Then implement `spmv` with `std::uint32_t` vs `std::size_t` indices and measure the difference. Add `#pragma omp parallel for schedule(static)` under `#ifdef _OPENMP` and report the scaling for 1/2/4/8 threads.

Example: `m=1024 nnz=5.2M  spmv 1.1 GFLOP/s = 11.4 GB/s (uint32) vs 9.8 GB/s (size_t); stream triad 92 GB/s; 8 threads 7.9x`.

<details><summary>Hint</summary>
Grid point `(i, j)` is row `i*m + j`; neighbours `±1` and `±m` exist only inside the grid. Diagonal `4`, off-diagonals `-1`. For the bandwidth comparison count bytes *actually moved*, not touched — `x[col_idx[k]]` mostly hits cache for a banded matrix, which is why SpMV on the Laplacian is faster than the pessimistic 20 B/nnz estimate suggests.
</details>

---

**20.5 — Compensated summation and reproducible parallel reductions**
Implement `naive_sum`, `kahan`, `neumaier`, `pairwise` and `sum_4acc` (four independent accumulators, what the vectorizer does). Test on (a) 10⁷ values `1.0 + tiny noise` (expected exact sum known), (b) `[1e16, 1.0, -1e16]` (Kahan fails, Neumaier succeeds — show it), (c) alternating `±x` with a known small residual. Report each method's absolute error and time; compile once with `-O2` and once with `-O2 -ffast-math`, and show Kahan/Neumaier silently become naive under `-ffast-math`. Then, under `#ifdef _OPENMP`, sum (a) with `reduction(+:s)` for 1, 2, 4, 8 threads five times each and print the bit patterns (`%a`) — observe they differ across thread counts (and possibly across runs with `schedule(dynamic)`); implement a *fixed-order* reduction (per-thread partials in an array, summed serially in index order with `schedule(static)`) and show it is bit-identical across runs for a fixed thread count. Finish with a compensated dot product using `std::fma` for the exact product error (Ogita–Rump–Oishi) and compare its error with the naive dot on an ill-conditioned pair of vectors.

<details><summary>Hint</summary>
For the exact expected sum in (a), build the data so the sum is representable: e.g. `x[i] = 1.0 + (i % 7) * 2^-20`. For (c), a randomly permuted set `{+x, -x}` sums to exactly 0. `printf("%a")` prints the hex float — the way to *see* last-bit differences.
</details>

---

**20.6 — `.npy` writer/reader and `.vtk` writer for your FDTD grid**
Write `npy::save(path, const double*, shape)` and `npy::save_f32`, and `npy::load(path) → {shape, vector<double>}` handling `'<f8'`, `'<f4'`, `'<i8'`, `'<i4'`, `fortran_order` true/false (transpose on load), version 1.0 and 2.0 headers, and rejecting everything else with a clear error. Verify by (a) dumping the header bytes and checking `(10 + HEADER_LEN) % 64 == 0` and the trailing `\n`; (b) round-tripping your data; (c) loading it in Python (`python3 -c "import numpy as np; a=np.load('out.npy'); print(a.shape, a.dtype, a.sum())"`) and comparing the sum; (d) saving from NumPy with `fortran_order=True` and loading in C++. Then write `vtk::write_structured_points(path, field, nx, ny, dx, dy, "Ez")` in both ASCII and BINARY (big-endian doubles — implement the byte swap) and open a sequence of 50 frames of your P08 FDTD (or C P14 heat) run in ParaView; include a `matplotlib` one-liner that plots one `.npy` frame. Deliverable: the C++ file, a `plot.py`, and one PNG.

<details><summary>Hint</summary>
Header dict: `{'descr': '<f8', 'fortran_order': False, 'shape': (3, 4), }` — note the trailing comma and space, and that a 1-D shape is `(3,)`. Pad with spaces (0x20) so the total header length is a multiple of 64, with the last byte `\n`. For BINARY VTK, `__builtin_bswap64` on the `uint64_t` bit pattern of each double.
</details>

---

**20.7 — Conjugate Gradient and GMRES(m) for PDE systems**
Implement `cg(spmv_fn, b, x, tol, max_it)` matrix-free (the operator is a function), with Jacobi preconditioning as an option, and `gmres(spmv_fn, b, x, m, tol, max_it)` with Arnoldi (modified Gram–Schmidt), Givens rotations on the Hessenberg matrix, and restarts. Solve the 2-D Poisson problem `−∇²u = f` on a 256×256 grid with a known analytic solution `u = sin(πx)sin(πy)` (so `f = 2π² u`); report iterations, relative residual, and the discretization error `‖u_h − u‖∞` (should be O(h²)). Plot residual vs iteration (dump to `.npy`, plot in Python) for CG, PCG-Jacobi, GMRES(20) and GMRES(50). Then make the operator non-symmetric by adding a convection term `+ β ∂u/∂x` (upwind) and show CG fails while GMRES converges. Verify the CG iteration count scales like `O(m)` (≈ `O(√κ)`) as `m` doubles.

<details><summary>Hint</summary>
Arnoldi step j: `w = A v_j; for i ≤ j: h_ij = ⟨w, v_i⟩; w -= h_ij v_i; h_{j+1,j} = ‖w‖; v_{j+1} = w / h_{j+1,j}`. Apply the previous Givens rotations to the new column of H, compute the new rotation to zero `h_{j+1,j}`, apply it to the right-hand side `g`; `|g_{j+1}|` is the current residual norm — no need to form `x` until convergence or restart.
</details>

---

**20.8 — ML: mixed-precision `Linear` with software `bfloat16`, BLAS, and fused softmax**
Implement `struct bf16 { uint16_t bits; }` with round-to-nearest-even conversion from/to `float`, and (under `#if defined(__ARM_NEON)`) a `_Float16` path. Build a `Linear` layer whose weights are stored in `bf16`, activations in `float`, and the matmul done by converting a `bf16` weight tile to `float` on the fly and calling `cblas_sgemm` (under `#ifdef USE_ACCELERATE`) or your NEON `Vec` kernel — accumulation in `float`. Measure: memory footprint (should halve), throughput vs a `float` `Linear` and vs `double`, and *accuracy*: forward-pass relative error vs the `double` reference for `d = 128, 512, 2048` (bf16 storage should give ~1e-3; float16 storage ~1e-4 until values exceed 65504 — construct an input that overflows float16 and show `inf`, and that bf16 survives). Then implement a **fused softmax** (max, exp-sum, normalize in *two* passes over the row, versus the naive four-pass version) and a fused `LayerNorm` (Welford one-pass mean/variance + normalize); measure both against the naive versions at `rows = 1024, cols = 4096` in GB/s and place them on the roofline (AI before/after). Finally check gradients of the mixed-precision layer against finite differences with an appropriate tolerance (derive it from the bf16 ε) — this is the test that tells you whether your training will work.

<details><summary>Hint</summary>
bf16 from float: `uint32_t u; memcpy(&u, &f, 4); uint32_t lsb = (u >> 16) & 1; u += 0x7FFF + lsb; bits = u >> 16;` (NaN needs a special case). To float: `uint32_t u = bits << 16`. For the fused softmax, the *online* trick `m_new = max(m, x); s = s·exp(m − m_new) + exp(x − m_new)` does it in one pass — the core of flash attention.
</details>

---

**20.9 — Numerics: OpenMP tasks + NEON + roofline on your Barnes–Hut / FDTD**
Take your P07 Barnes–Hut (or the C P15 direct N-body) and P08 FDTD (or C P14 heat). For the N-body: (a) convert to SoA with a flat `std::vector<Node>` octree using child indices; (b) vectorize the leaf-cell direct interactions with a `Vec2d` (or `Vec4f`) wrapper using `vrsqrteq_f64` + two Newton–Raphson steps for `1/√r²` — verify against `1.0/std::sqrt` to 1e-12; (c) parallelize the force walk with `#pragma omp parallel for schedule(dynamic, 256)` and the tree build with `#pragma omp task`/`taskwait` (cut-off depth for task creation); (d) check energy drift and momentum conservation after every change (they *will* catch a bug). For the FDTD: (a) fuse the E and H updates where the dependency allows, (b) `collapse(2)` parallel loops, (c) `float` fields. For both, report a table: step time, GFLOP/s, GB/s, arithmetic intensity, roofline bound, % of bound — for each version — and the parallel efficiency at 1/2/4/8 threads. State which kernel is compute-bound and which is memory-bound and how you know.

<details><summary>Hint</summary>
Count bytes as the fields you *must* stream from DRAM per step (each field read once and written once if the working set exceeds L2), and flops from the update formula. For the tree build with tasks: `#pragma omp parallel` + `#pragma omp single` around the recursive call; use `if(depth < 3)` on the task pragma. `OMP_PROC_BIND=close OMP_PLACES=cores` and `OMP_NUM_THREADS` = number of P-cores for stable timings.
</details>

---

**20.10 — Sim: MPI halo exchange for the heat equation (build with `mpicxx`)**
`brew install open-mpi` (Linux: `apt install libopenmpi-dev`). Write `ex20_10.cpp`: a 2-D explicit heat solver on an `ny × nx` grid decomposed into horizontal slabs over `P` ranks with one ghost row above and below; halo exchange with `MPI_Sendrecv` (then a second version with `MPI_Isend/Irecv/Waitall` overlapping the interior update); `MPI_Allreduce` of the total heat every 100 steps (must be conserved with insulated boundaries — the property test); rank 0 gathers the slabs with `MPI_Gatherv` and writes a `.npy` (20.6) every 1000 steps. Build: `mpicxx -std=c++17 -O2 -Wall -Wextra -o ex20_10 ex20_10.cpp`; run: `mpirun -np 1 ./ex20_10 2048 2048 2000` and `-np 2/4/8` (`--oversubscribe` if needed). Verify all `P` produce bit-identical fields to `P = 1` (the halo exchange is exact; only the `Allreduce` may differ in the last bit — explain why), report strong-scaling speedup, and estimate the communication fraction from the measured time and the halo bytes. Then swap the 1-D slab decomposition for a 2-D `MPI_Cart_create` grid and exchange columns with an `MPI_Type_vector` — compare the halo bytes per rank for `P = 16`. If MPI cannot run in your environment, compile-check with `mpicxx` and note it in the header.

<details><summary>Hint</summary>
Use `MPI_PROC_NULL` for the missing neighbour at the top/bottom rank — `Sendrecv` to it is a no-op, so no special-casing. Row `r` global = `r_local − 1 + row_offset[rank]`. For `Gatherv`, ranks may own different numbers of rows when `ny % P != 0` — compute `counts`/`displs` from `ny / P` and `ny % P`. Time with `MPI_Wtime()` after an `MPI_Barrier`.
</details>
