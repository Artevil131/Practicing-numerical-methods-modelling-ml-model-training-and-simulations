# Chapter 19 — Exercises

Write each exercise as `ex19_K.c` in this folder (`ex19_1.c`, `ex19_2.c`, ...). Default compile:
`cc -Wall -Wextra -std=c11 -O2 -o ex19_K ex19_K.c -lm`; several exercises say `-O3 -march=native`
and `-Rpass=loop-vectorize` — the flags are part of the exercise. Every SIMD kernel must be guarded
with `#if defined(__ARM_NEON)` and have a scalar fallback; every kernel must be verified against its
scalar version on sizes that are NOT multiples of the vector width (1, 7, 9, 4095); every timing
uses warm-up + median and consumes its result (chapter 13 / lesson §2).

---

### 19.1 — **Roofline for this machine**

Measure your machine's one-core roofline empirically: (a) peak f64 FMA throughput with a loop of 16 independent `fma()` chains on register-resident data (no loads), (b) peak f32 the same way, (c) sequential read bandwidth from L1 (32 KB), L2 (4 MB), and DRAM (512 MB). Print `P`, `B` for each level, the ridge points, and then the *predicted* attainable GFLOP/s for saxpy, dot, N-body pair, and matmul at N=512 — followed by the measured GFLOP/s of your own dot and saxpy to see how close you get.

Example:
```
peak f64 61.8 GFLOP/s  f32 123.4 GFLOP/s   BW: L1 231 GB/s  L2 118 GB/s  DRAM 71 GB/s
ridge (DRAM) 0.87 flop/B   dot predicted 8.9 GFLOP/s (DRAM) measured 8.1
```

<details><summary>Hint</summary>
For peak compute, the compiler must not fold the loop: make the 16 accumulators depend on a `volatile`-loaded seed and sum them into a sink. Unroll by hand; check the assembly has 16 `fmla`/`fmadd` per trip and no loads.
</details>

### 19.2 — **Make the compiler vectorize it**

Write six loops the vectorizer refuses: (1) two `double *` parameters without `restrict`, (2) a loop with `break` on a data condition, (3) `a[i] = a[i-1] + b[i]`, (4) a call to `exp()` in the body, (5) a stride-2 access, (6) an FP sum reduction. Compile with `-O3 -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize` and paste each remark into a comment. Then fix each one that *can* be fixed (restrict, restructure, 4 accumulators, `-fno-math-errno`, SoA) and show the `-Rpass=loop-vectorize` remark for the fixed version plus the speedup.

Example:
```
loop 1: "loop not vectorized: cannot prove it is safe to reorder memory operations"  -> restrict -> width 2 interleave 4, 3.9x
loop 6: "loop not vectorized: cannot prove it is safe to reorder floating-point operations" -> 4 accumulators -> 4.1x
```

<details><summary>Hint</summary>
`-Rpass-analysis` prints the *reason*. For (4) note that `sqrt` vectorizes with `-fno-math-errno` but `exp` needs a vector math library or your own polynomial.
</details>

### 19.3 — **Read the assembly**

Compile a scalar dot product, a 4-accumulator dot product, and your NEON dot product with `-O3 -march=native -S`. For each, extract the hot loop into a comment and annotate every instruction (loads, `fmla`/`fmadd`, counter, branch). Count FMAs per load and identify the dependency chain in the 1-accumulator version. Then run `llvm-mca` (`brew install llvm`; `$(brew --prefix llvm)/bin/llvm-mca -mcpu=apple-m1 loop.s`) on each loop and compare its predicted cycles/iteration with your measured ns/element × clock.

Example:
```
dot_scalar loop: ldr d0,[x0,x8]; ldr d1,[x1,x8]; fmadd d2,d0,d1,d2; add x8,#8; cmp; b.ne  -> chain on d2, 1 FMA/2 loads
llvm-mca: 4.0 cycles/iter (latency-bound)   measured 3.9 cycles/element
```

<details><summary>Hint</summary>
`-fno-asynchronous-unwind-tables` removes clutter. Loop labels look like `LBB0_3:`; the back-edge is the `b.ne` to it. `llvm-mca` wants just the loop body between `# LLVM-MCA-BEGIN` / `# LLVM-MCA-END` comments.
</details>

### 19.4 — **NEON toolbox**

Implement with NEON intrinsics and verify against scalar: `sum_f32`, `max_f32` (horizontal reductions with 4 accumulators), `relu_inplace_f32` (`vmaxq`), `clamp_f32(lo, hi)`, `axpby` (`y = a*x + b*y`), `count_greater_f32(thr)` (compare → mask → `vnegq`/`vaddq` of the mask as -1/0 integers, then horizontal sum), `l2_normalize_f32` (sum of squares, `vrsqrteq` + one Newton step, scale; compare the estimate's error against `1/sqrtf`), and `transpose_4x4_f32` with `vtrn1q/vtrn2q` + `vzip`. Time each against the scalar version at L1 size and DRAM size.

Example:
```
count_greater: n=4095 scalar 41 -> neon 41 ok    max_f32 scalar 2.9 ns/elt neon 0.31 ns/elt (L1)
rsqrt estimate error 3.1e-4 -> after one Newton step 4.9e-8
```

<details><summary>Hint</summary>
`vcgtq_f32` yields `uint32x4_t` with lanes 0xFFFFFFFF; `vreinterpretq_s32_u32` then `vnegq_s32` gives +1 per true lane. Transpose 4x4: two `vtrn`, then two `vcombine`/`vzip` on the 64-bit halves.
</details>

### 19.5 — **Portable SIMD with vector extensions**

Rewrite 19.4's `sum`, `axpby`, `relu`, and `count_greater` with `typedef float v4f __attribute__((vector_size(16)))` and `v8f` (32 bytes), no intrinsics, no ISA guard. Use `__builtin_reduce_add` / `__builtin_reduce_max` for the reductions and `?:` on comparison masks for relu/count. Verify and time against the NEON versions; compile the same file with `-target x86_64-apple-macos -S` (cross-compile to assembly only) and confirm it produced SSE/AVX instructions from the *same source*.

Example:
```
axpby v8f: 0.27 ns/elt   neon: 0.26 ns/elt   (v8f = two NEON registers per op)
x86 cross-compile: 214 vfmadd231ps / vmulps instructions, 0 arm instructions
```

<details><summary>Hint</summary>
Loads: `memcpy(&v, p, sizeof v)`. Width 32 bytes on arm64 legalises to two 128-bit ops — it is fine. `-target x86_64-apple-macos -mavx2 -mfma -S -o x86.s` needs no SDK for assembly output.
</details>

### 19.6 — **Micro-kernel with edges, and block-size tuning**

Extend the lesson's packed matmul to arbitrary M, N, K (not multiples of 4): pad the packed panels with zeros to a multiple of 4 and store only the valid part of the C tile (a small "edge kernel" or a masked store via a temporary 4×4 buffer). Then sweep `KC ∈ {64,128,256,512}`, `MC ∈ {32,64,128}`, `NC ∈ {256,512,1024}` at N = 1000 and N = 2048, print a table of GFLOP/s, and pick the best. Verify every configuration against `ikj`. Finally run `cblas_dgemm` (`-framework Accelerate`) on the same sizes and report your fraction of it.

Example:
```
N=1000 KC=256 MC=64 NC=512: 36.1 GFLOP/s   N=2048 best KC=256 MC=32 NC=1024: 34.7   cblas: 480 (7.2%)
edges: M=1001 N=999 K=1003 max|diff| vs ikj 2.3e-13
```

<details><summary>Hint</summary>
Pack functions read `min(4, remaining)` rows/cols and zero-fill the rest — then the micro-kernel needs no changes. For the C edge, compute into a 4x4 stack buffer and copy the valid `mr x nr` part.
</details>

### 19.7 — **Branchless and ILP zoo**

Write and time on random vs sorted data: (a) branchy vs branchless `count_if(x > t)`, `clamp`, `max`, and `abs`; (b) a 1-accumulator vs 2/4/8/16-accumulator `double` sum — plot (print a table) GFLOP/s vs accumulator count and find the knee; (c) the same for `int64` sums; (d) `x / y` in a loop vs `x * (1/y)` hoisted; (e) a subnormal-heavy loop (`x *= 0.999` from 1e-310) vs normal, and — on x86 only, under `#if defined(__x86_64__)` — with `_MM_SET_FLUSH_ZERO_MODE`. Explain each ratio in one comment line.

Example:
```
sum f64 accumulators: 1: 2.2  2: 4.3  4: 7.9  8: 12.6  16: 12.9 GFLOP/s  (knee at 8: 4 pipes x 4 cycles / 2 lanes)
count_if random: branchy 9.8 ms, branchless 0.9 ms; sorted: 0.9 / 0.9
```

<details><summary>Hint</summary>
Mark every kernel `__attribute__((noinline))`. For (b) generate the accumulator variants with a macro or write them out; the compiler will not do it for you without -ffast-math — that is the point.
</details>

### 19.8 — **Vectorized softmax and layer-norm for your MLP (ML goal)**

Implement `softmax_rows_f32(float *x, size_t rows, size_t cols)` and `layernorm_rows_f32` with NEON: row max via `vmaxq` + `vmaxvq`, a NEON `exp` approximation (range-reduce `x = k ln2 + r`, degree-5 polynomial for `e^r`, rebuild with `vreinterpretq_f32_s32` of the exponent bits — max relative error must be < 2e-7), sum, scale by the reciprocal. Verify against your chapter 12 stable scalar softmax to 1e-6 and time both on a `(256, 10)` MNIST logits batch and a `(64, 4096)` transformer-shaped batch. Then vectorize the ReLU backward (`dx = dy * (x > 0)`) with masks and the bias-gradient reduction over the batch with 4 accumulators.

Example:
```
softmax (64,4096): scalar 1.92 ms  neon 0.31 ms   max rel err vs scalar 8.7e-7   row sums 1.0000000 ± 6e-8
exp poly max rel error 1.4e-7 on [-87, 88]
```

<details><summary>Hint</summary>
`k = round(x / ln2)`, `r = x - k ln2` (use `fma` with a hi/lo split of ln2 for accuracy), `e^r ≈ 1 + r + r²/2 + ... `; then `2^k` is `(k + 127) << 23` reinterpreted as float. Clamp `x` to `[-87, 88]` first.
</details>

### 19.9 — **N-body force kernel: from scalar to NEON to roofline (sim goal)**

Take your P15 direct-sum force loop (softened gravity, SoA from exercise 18.8). Produce: (1) scalar, (2) auto-vectorized (`-O3 -march=native -fno-math-errno`, check `-Rpass`), (3) NEON f64 with `vrsqrteq_f64` + 2 Newton steps for `1/sqrt(d2)` (verify accuracy ≤ 1e-12 relative), (4) NEON f32 (4 lanes, `vrsqrteq_f32` + 1 Newton step) — for N = 1024, 4096, 16384. Report pair-interactions/s and GFLOP/s (count 20 flops per pair) for each, place them on the roofline from 19.1, and state which roof each version hits. Confirm energy drift over 1000 leapfrog steps is unchanged (to 1e-10 for f64) by the vectorization, and report the f32 version's drift separately.

Example:
```
N=4096  scalar 0.79 Gpair/s  auto-vec 1.6  neon f64 2.4 (48 GFLOP/s, 75% of roof)  neon f32 4.9 (98 GFLOP/s)
energy drift 1000 steps: f64 3.1e-11 (all versions)  f32 2.4e-6
```

<details><summary>Hint</summary>
Process `j` in chunks of 2 (f64) or 4 (f32) against a broadcast `i` particle; accumulate `ax, ay, az` in vector registers and reduce once per `i`. `vrsqrtsq_f64(d2 * y, y)` gives the Newton factor: `y = y * vrsqrtsq(d2*y, y)`.
</details>

### 19.10 — **Optimize a real hot loop end-to-end (ML + sim goal)**

Pick one of: (a) your P12 MLP training step (784-256-10, batch 128), (b) your P14 2-D heat/wave stencil on a 1024² grid, or (c) your P16 char-LM attention. Profile it with Instruments (`xcrun xctrace record --template 'Time Profiler' --launch ./prog`) or `sample`, paste the top 5 functions into a comment, then apply the chapter's method to the hottest one: state the roof it is at, change one thing at a time (loop order → restrict/vectorization → accumulators/register tiling → blocking/packing → threads if you did chapter 16), verify correctness after each step (gradient check / conserved quantity / golden output), and record a table of steps × speed. Stop when you are within 2x of the roof or the profile has moved elsewhere. Final line: end-to-end speedup of the *whole program*.

Example:
```
profile: mat_mul 71%  relu_backward 9%  softmax 6%  mat_transpose 5%  other 9%
mat_mul: 2.0 -> 17.9 -> 26.8 -> 37.2 GFLOP/s; transpose eliminated via strided view (ch18); end-to-end 6.8x
epoch time 41.2 s -> 6.1 s, test accuracy unchanged 97.9%
```

<details><summary>Hint</summary>
Amdahl: at 71% in `mat_mul`, a 20x kernel speedup gives at most 3.2x end-to-end — the next hot spot moves. For the stencil, time-block: update a tile for 2-4 steps before moving on (halo = steps).
</details>
