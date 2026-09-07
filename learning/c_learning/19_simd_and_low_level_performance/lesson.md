# Chapter 19 — SIMD and Low-Level Performance

## What you'll be able to do after this chapter

- Draw the roofline for your machine, compute a kernel's arithmetic intensity, and say *before measuring* whether it is memory-bound or compute-bound and what speed is attainable.
- Make the compiler vectorize your loops (`restrict`, countable loops, no early exit, contiguous access), confirm it with `-Rpass=loop-vectorize`, and read the generated NEON assembly.
- Write NEON intrinsics for dot product, saxpy, and a 4×4 matmul micro-kernel; write the same with clang vector extensions; name the SSE/AVX equivalents.
- Explain dependency chains, instruction-level parallelism, unrolling, software pipelining, branchless code, denormals, and FMA rounding — and measure each one with a benchmark that does not lie.
- Take `mat_mul` from 2 GFLOP/s to ~38 GFLOP/s on one core (naive → ikj → register tile → packed blocked micro-kernel), verify it against the naive version, and know why Accelerate is still 10x faster.

## Why this matters for ML / numerics / sims

A transformer forward pass is 95% matmul; a fluid solver is 95% stencil sweeps; N-body is 95% one inner loop. The difference between a naive loop and a tuned kernel is 20-100x *on the same core*, and that factor decides whether your MNIST MLP trains in 10 seconds or 10 minutes, whether your FDTD grid is 200² or 2000². NumPy and PyTorch are fast because someone did this work in C; this chapter shows you exactly what they did, so you can read `ggml`'s kernels, write your own, and know when you have hit the hardware wall instead of a bug.

---

## 1. The roofline model

Two ceilings bound any kernel on one core:

- **Peak compute** `P` (flop/s): NEON pipes × lanes × 2 (FMA = 2 flops) × clock. An Apple M-series performance core has 4 128-bit FP pipes: 4 × 2 doubles × 2 × ~4 GHz ≈ **64 GFLOP/s f64**, 128 GFLOP/s f32. (An x86 core with 2 AVX-512 FMA units: 2 × 8 × 2 × 3 GHz ≈ 96 GFLOP/s f64.)
- **Peak bandwidth** `B` (byte/s) from wherever the data lives: ~60-100 GB/s for one core from DRAM on M-series, several hundred GB/s from L1.

A kernel's **arithmetic intensity** `I` = flops / bytes moved. Attainable performance is `min(P, I × B)`. The **ridge point** `P / B` (≈ 1 flop/byte here) separates memory-bound kernels (left) from compute-bound (right).

```
GFLOP/s (log)
   64 |                    ______________________ compute roof (P)
      |                  /
      |                /   <- slope = B (memory roof)
   10 |              /
      |            /
    1 |__________/_____________________________________  flop/byte (log)
         0.1     1 (ridge)   10        100
       saxpy  dot   nbody    blocked matmul
```

| kernel | flops | bytes | I (flop/B) | bound | attainable (P=64, B=60) |
|--------|-------|-------|------------|-------|--------------------------|
| saxpy `y = a*x + y` (f32) | 2 | 12 | 0.17 | memory | 10 GFLOP/s |
| dot (f64) | 2 | 16 | 0.125 | memory | 7.5 |
| N-body pair (f64, SoA) | ~20 | 32 | 0.63 | memory-ish | 38 |
| matmul naive, per FMA | 2 | 16 | 0.125 | memory | 7.5 |
| matmul N=512, each matrix read once | 2N³ | 24N² | N/12 = 43 | compute | 64 |

The last two rows are the whole story of this chapter: the same 2N³ flops are memory-bound or compute-bound depending on *how many times you reload the same bytes*. Blocking (§8) raises `I` from 0.125 toward N/12. The N-body row explains why direct-sum N-body at large N is compute-bound only once the `j` data fits in L1/L2 (it does: 8192 bodies × 32 B = 256 KB).

Roofline tells you when to **stop**: a dot product at 12 GFLOP/s from L2 is at the L2-bandwidth roof; no intrinsic will improve it. `example.c` §2 prints this table.

## 2. Micro-benchmark methodology (so the numbers mean something)

1. **Warm up**: the first run pays page faults (chapter 18 §8), cold caches, and the clock ramping from idle. Run once untimed.
2. **Repeat and take the median** (or minimum for pure compute). One run is noise: a context switch adds 1 ms.
3. **Defeat the optimizer**: results must be *consumed* — write them to a `volatile` sink, or print a checksum. Otherwise the compiler deletes the loop and you measure nothing. Mark measured kernels `__attribute__((noinline))` so the compiler cannot specialise them on the constants of your benchmark (`example.c` §7: inlined, the branchy/branchless difference vanished entirely).
4. **Same work, same data**: compare versions on identical inputs and verify the outputs agree (`max|diff|`).
5. **Know where the data lives**: 32 KB per vector is L1, 2 MB is L2, 64 MB is DRAM. Report which.
6. **Pinning**: macOS does not let you pin threads to cores; the scheduler may move a benchmark to an efficiency core. Use `taskpolicy -c utility ./prog` to *force* E-cores (to see the difference) and keep runs short and repeated. Linux: `taskset -c 3 ./prog`, and disable frequency scaling (`cpupower frequency-set -g performance`) for stable numbers.
7. **Report the unit that matters**: GFLOP/s for compute kernels, GB/s for streaming ones, ns/element for both.

```c
#define BENCH(out, reps, stmt) do { double _t[64]; int _r = (reps); { stmt; }       /* warm-up */ \
    for (int _i = 0; _i < _r; _i++) { double _t0 = now_sec(); { stmt; } _t[_i] = now_sec() - _t0; } \
    qsort(_t, _r, sizeof _t[0], cmp_double); (out) = _t[_r / 2]; } while (0)          /* median */
static volatile double g_sink;  static void sink(double x) { g_sink = x; }
```

Python equivalent: `timeit` does the repeat-and-min part; there is no equivalent of the optimizer problem because CPython never deletes work.

## 3. Auto-vectorization: what the compiler needs

clang vectorizes loops at `-O2` and above (since LLVM 13; GCC needs `-O3` or `-ftree-vectorize`). It will refuse — silently — unless the loop satisfies all of:

| Requirement | Why | Fix |
|-------------|-----|-----|
| **No possible aliasing** between written and read arrays | if `c` may overlap `b`, `c[j] += a*b[j]` must run in order | `restrict` on pointer parameters, or local `restrict` copies |
| **Countable trip count** known at loop entry | the vectorizer needs `n/4` iterations + a scalar tail | `for (i = 0; i < n; i++)`, no `i` modification in the body |
| **No early exit / data-dependent break** | lanes cannot stop independently | hoist the search out, or use a mask-and-reduce pattern |
| **No loop-carried dependence** except reductions | `a[i] = a[i-1] + x` is inherently serial | restructure; prefix sums need a different algorithm |
| **Contiguous (unit-stride) access** | gathers are slow or unsupported | SoA layout (chapter 18 §6) |
| **No function calls** in the body except inlinable ones or `sqrt`/`fma`/`fabs` | `exp`, `log`, `printf` are scalar | `-fveclib=...` (Linux libmvec), or your own vectorized `exp` |
| **Reductions with floating point**: `s += a[i]*b[i]` requires reassociation | IEEE addition is not associative; the compiler must preserve your order | `-ffast-math`/`-fassociative-math`, **or** write 4 accumulators yourself (§7) |

Flags:

```sh
cc -O2                     # vectorizes (clang); width chosen by cost model
cc -O3 -march=native       # + more unrolling/interleaving; -march=native = this CPU's features
cc -O3 -mcpu=apple-m1      # explicit Apple Silicon target (M1-M4 share the ISA level you care about)
cc -O3 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize -c file.c
```

What the remarks look like on `example.c`:

```
example.c:126: remark: loop not vectorized                                        [-Rpass-missed]   <- dot_scalar: FP reduction, no -ffast-math
example.c:136: remark: vectorized loop (vectorization width: 2, interleaved count: 4)              <- dot_scalar4: 4 accumulators, so it CAN
example.c:209: remark: vectorized loop (vectorization width: 4, interleaved count: 4)              <- saxpy_scalar (f32): restrict did it
example.c:273: remark: the cost-model indicates that vectorization is not beneficial               <- mm_naive inner loop: stride-n gather
example.c:283: remark: vectorized loop (vectorization width: 2, interleaved count: 4)              <- mm_ikj: contiguous j loop
```

"Vectorization width 2, interleaved count 4" = two doubles per NEON register, four registers per iteration = 8 elements per loop trip. Confirm at the assembly level: `cc -O3 -march=native -S -o example.s example.c && grep -c fmla example.s` → 114 fused multiply-adds in this file.

**`-ffast-math` and what it breaks.** It enables `-fassociative-math` (reorder sums → vectorized reductions), `-freciprocal-math` (`x/y` → `x*(1/y)`), `-fno-signed-zeros`, `-ffinite-math-only` (assumes no NaN/Inf → `isnan(x)` is *optimized to false*), and flushes denormals. Consequences: your Kahan summation (chapter 12) is deleted (it relies on non-associativity); `if (x != x)` NaN checks vanish; results change between builds; a NaN in the loss becomes silent garbage instead of a detectable NaN. Use it only on a file that contains nothing but arithmetic kernels you have verified, or use the targeted flags: `-fno-math-errno` (always safe: lets `sqrt` inline), `-ffp-contract=fast`, `-fassociative-math -fno-signed-zeros -fno-trapping-math` (reductions only). Never on the file with your test harness or NaN detection.

A before/after pair for the most common failure — aliasing — with the remarks the compiler emits:

```c
void axpy_bad(double *y, const double *x, double a, size_t n) {   /* y and x MAY overlap */
    for (size_t i = 0; i < n; i++) y[i] += a * x[i];
}
// remark: loop not vectorized: cannot prove it is safe to reorder memory operations [-Rpass-analysis]
// (clang actually emits a runtime overlap check and two versions of the loop here — "versioning" —
//  which works for simple 1-D cases but gives up for anything with 2-D indexing like C[i*n+j].)

void axpy_good(double *restrict y, const double *restrict x, double a, size_t n) {
    for (size_t i = 0; i < n; i++) y[i] += a * x[i];
}
// remark: vectorized loop (vectorization width: 2, interleaved count: 4) [-Rpass=loop-vectorize]
```

Python equivalent: NumPy ufuncs are pre-compiled vectorized loops with runtime CPU dispatch (`np.show_runtime()` lists the SIMD level); they got there by satisfying exactly this table.

## 4. Reading assembly

```sh
cc -O3 -march=native -S -o dot.s dot.c            # assembly text; -fno-asynchronous-unwind-tables cleans it up
cc -O3 -march=native -c dot.c && objdump -d dot.o  # disassemble the object (macOS: also otool -tV dot.o)
# or paste into https://godbolt.org (Compiler Explorer), compiler "armv8-a clang", flags -O3
```

The arm64 vocabulary you need (`.2d` = two doubles, `.4s` = four floats, `.2s`/`.8b` etc.):

```
ldp   q0, q1, [x0], #32          load pair of 128-bit registers from a (post-increment by 32 bytes)
ldr   q2, [x1, x8, lsl #3]       load 128 bits from b + i*8
fmla  v4.2d, v0.2d, v2.2d        v4 += v0 * v2, two doubles          <- THE instruction of this chapter
fmla  v5.2d, v1.2d, v3.d[1]      v5 += v1 * broadcast(lane 1 of v3)  <- vfmaq_laneq_f64
fmul / fadd / fsub  v.2d         plain arithmetic
faddp d0, v4.2d                  horizontal add of the two lanes -> scalar d0 (vaddvq_f64)
dup   v6.2d, v7.d[0]             broadcast one lane (vdupq_laneq_f64)
ext / zip1 / zip2 / trn1 / rev64 shuffles (vextq, vzip1q, vtrn1q, vrev64q)
fcmgt v8.2d, v0.2d, v1.2d        compare -> all-ones/all-zeros mask per lane (vcgtq_f64)
bsl   v9.16b, v8.16b, v10.16b    bitwise select = blend by mask (vbslq)
csel  x0, x1, x2, gt             conditional select (x86 cmov): branchless
subs  x8, x8, #8 ; b.ne  .LBB0_3 loop counter and back-edge
```

What to look for: (1) is the hot loop full of `fmla v*.2d` (vectorized) or `fmadd d*` (scalar)? (2) how many `fmla` per `ldr`/`ldp` — the load:FMA ratio tells you if you are load-port bound (§8); (3) a `fmla` whose destination is the source of the next `fmla` is a dependency chain (§7); (4) `bl _sqrt` or `bl _exp` inside the loop means a scalar call killed vectorization (`-fno-math-errno` fixes `sqrt`). On x86 the equivalents are `vfmadd231pd ymm` (AVX2, 4 doubles) / `zmm` (AVX-512, 8), `vmovupd` loads, `vhaddpd`, `vblendvpd`, `vpermpd`.

## 5. NEON intrinsics

`#include <arm_neon.h>`, guarded by `#if defined(__ARM_NEON)`. Types are `<lane type>x<lanes>_t`: `float64x2_t`, `float32x4_t`, `int32x4_t`, `uint8x16_t`, and 64-bit halves `float32x2_t`. Intrinsic names encode the operation, `q` for 128-bit, and the lane type suffix:

| Operation | f64 (2 lanes) | f32 (4 lanes) | SSE2/AVX2 equivalent |
|-----------|---------------|---------------|----------------------|
| load / store (unaligned OK) | `vld1q_f64(p)` / `vst1q_f64(p, v)` | `vld1q_f32` / `vst1q_f32` | `_mm_loadu_pd` / `_mm256_loadu_pd`, `_mm_storeu_pd` |
| broadcast scalar | `vdupq_n_f64(x)` | `vdupq_n_f32(x)` | `_mm_set1_pd` / `_mm256_set1_pd` |
| add / sub / mul | `vaddq_f64`, `vsubq_f64`, `vmulq_f64` | `vaddq_f32`, ... | `_mm_add_pd`, ... |
| FMA `a + b*c` | `vfmaq_f64(a, b, c)` | `vfmaq_f32` | `_mm_fmadd_pd(b, c, a)` (FMA3) |
| FMA with broadcast lane | `vfmaq_laneq_f64(a, b, c, lane)` | `vfmaq_laneq_f32` | `_mm256_fmadd_pd(b, _mm256_permute4x64_pd(c, ...), a)` |
| FMA with scalar | `vfmaq_n_f64(a, b, x)` | `vfmaq_n_f32` | broadcast then fmadd |
| horizontal sum | `vaddvq_f64(v)` → double | `vaddvq_f32` | `_mm_hadd_pd`, or shuffles + adds |
| max / min | `vmaxq_f64`, `vminq_f64`, `vmaxvq_f64` (reduce) | `vmaxq_f32` | `_mm_max_pd` |
| sqrt, reciprocal estimate | `vsqrtq_f64`, `vrecpeq_f64`, `vrsqrteq_f64` (+ Newton step `vrsqrtsq_f64`) | `vsqrtq_f32` | `_mm_sqrt_pd`, `_mm_rsqrt_ps` (f32 only) |
| compare → mask | `vcgtq_f64(a, b)` → `uint64x2_t` all-ones/zeros | `vcgtq_f32` → `uint32x4_t` | `_mm_cmpgt_pd` |
| blend by mask | `vbslq_f64(mask, a, b)` (mask ? a : b) | `vbslq_f32` | `_mm_blendv_pd(b, a, mask)` |
| shuffles | `vextq_f64(a, b, 1)`, `vzip1q_f64`, `vzip2q_f64`, `vtrn1q_f64`, `vrev64q_f32`, `vcombine_f64`, `vget_low_f64`/`vget_high_f64` | same names `_f32` | `_mm_shuffle_pd`, `_mm_unpacklo_pd`, `_mm256_permute2f128_pd` |
| convert | `vcvtq_f64_s64`, `vcvtq_s64_f64`, `vcvt_f32_f64` | `vcvtq_f32_s32` | `_mm_cvtepi32_ps` |
| get/set one lane | `vgetq_lane_f64(v, 1)`, `vsetq_lane_f64(x, v, 1)` | | `_mm_cvtsd_f64` |

Three complete kernels (all in `example.c`, verified against scalar):

```c
/* dot product: 4 independent accumulators, 8 elements per iteration, horizontal add at the end */
double dot_neon(const double *a, const double *b, size_t n) {
    float64x2_t s0 = vdupq_n_f64(0), s1 = s0, s2 = s0, s3 = s0;
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        s0 = vfmaq_f64(s0, vld1q_f64(a + i),     vld1q_f64(b + i));
        s1 = vfmaq_f64(s1, vld1q_f64(a + i + 2), vld1q_f64(b + i + 2));
        s2 = vfmaq_f64(s2, vld1q_f64(a + i + 4), vld1q_f64(b + i + 4));
        s3 = vfmaq_f64(s3, vld1q_f64(a + i + 6), vld1q_f64(b + i + 6));
    }
    double r = vaddvq_f64(vaddq_f64(vaddq_f64(s0, s1), vaddq_f64(s2, s3)));
    for (; i < n; i++) r += a[i] * b[i];                          /* scalar tail: n % 8 elements */
    return r;
}

/* saxpy f32, 4x unrolled: 16 floats per iteration */
void saxpy_neon4(float a, const float *restrict x, float *restrict y, size_t n) {
    float32x4_t va = vdupq_n_f32(a);
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        float32x4_t y0 = vld1q_f32(y + i), y1 = vld1q_f32(y + i + 4), y2 = vld1q_f32(y + i + 8), y3 = vld1q_f32(y + i + 12);
        y0 = vfmaq_f32(y0, va, vld1q_f32(x + i));     y1 = vfmaq_f32(y1, va, vld1q_f32(x + i + 4));
        y2 = vfmaq_f32(y2, va, vld1q_f32(x + i + 8)); y3 = vfmaq_f32(y3, va, vld1q_f32(x + i + 12));
        vst1q_f32(y + i, y0); vst1q_f32(y + i + 4, y1); vst1q_f32(y + i + 8, y2); vst1q_f32(y + i + 12, y3);
    }
    for (; i < n; i++) y[i] = a * x[i] + y[i];
}
```

```c
/* 4x4 micro-kernel: C[4x4] += A[4xK] * B[Kx4], A and B pre-packed so each p step reads 4 contiguous
   doubles of each. 8 accumulators = a 4x4 tile of C living in registers for the whole K loop.   */
static inline void kernel_4x4(size_t kc, const double *Ap, const double *Bp, double *C, size_t ldc) {
    float64x2_t c00 = vld1q_f64(C),           c01 = vld1q_f64(C + 2);
    float64x2_t c10 = vld1q_f64(C + ldc),     c11 = vld1q_f64(C + ldc + 2);
    float64x2_t c20 = vld1q_f64(C + 2 * ldc), c21 = vld1q_f64(C + 2 * ldc + 2);
    float64x2_t c30 = vld1q_f64(C + 3 * ldc), c31 = vld1q_f64(C + 3 * ldc + 2);
    for (size_t p = 0; p < kc; p++) {
        float64x2_t a01 = vld1q_f64(Ap + 4 * p), a23 = vld1q_f64(Ap + 4 * p + 2);   /* A[i..i+3][p]  */
        float64x2_t b01 = vld1q_f64(Bp + 4 * p), b23 = vld1q_f64(Bp + 4 * p + 2);   /* B[p][j..j+3]  */
        c00 = vfmaq_laneq_f64(c00, b01, a01, 0); c01 = vfmaq_laneq_f64(c01, b23, a01, 0);
        c10 = vfmaq_laneq_f64(c10, b01, a01, 1); c11 = vfmaq_laneq_f64(c11, b23, a01, 1);
        c20 = vfmaq_laneq_f64(c20, b01, a23, 0); c21 = vfmaq_laneq_f64(c21, b23, a23, 0);
        c30 = vfmaq_laneq_f64(c30, b01, a23, 1); c31 = vfmaq_laneq_f64(c31, b23, a23, 1);
    }
    vst1q_f64(C, c00); vst1q_f64(C + 2, c01); /* ... store the other 6 ... */
}
```

Per `p`: 4 loads (16 doubles), 8 FMAs (16 flops... ×2 lanes = 32 flops), 0 stores. That 2:1 FMA:load ratio is what the plain `ikj` loop (1 FMA : 2 loads + 1 store) cannot reach — see §8.

**Masks and blends** replace `if` inside SIMD code: `mask = vcgtq_f64(x, thr); y = vbslq_f64(mask, a, b);` computes both sides and selects. ReLU: `vmaxq_f32(x, vdupq_n_f32(0))`. Clamp: `vminq/vmaxq`. A softmax's `exp` has no NEON instruction — you write a polynomial `exp` on `float32x4_t` (exercise 19.8) or call Accelerate's `vvexpf`.

```c
/* ReLU backward: dx = dy where x > 0 else 0 — the SIMD form of an `if` */
void relu_backward_f32(const float *restrict x, const float *restrict dy, float *restrict dx, size_t n) {
    float32x4_t zero = vdupq_n_f32(0);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        uint32x4_t keep = vcgtq_f32(vld1q_f32(x + i), zero);          /* lanes: 0xFFFFFFFF or 0 */
        vst1q_f32(dx + i, vbslq_f32(keep, vld1q_f32(dy + i), zero));  /* keep ? dy : 0 */
    }
    for (; i < n; i++) dx[i] = x[i] > 0 ? dy[i] : 0;
}

/* count elements above a threshold: mask -> integer -1/0 -> negate -> accumulate -> horizontal add */
size_t count_gt_f32(const float *x, size_t n, float thr) {
    int32x4_t acc = vdupq_n_s32(0); float32x4_t t = vdupq_n_f32(thr);
    size_t i = 0;
    for (; i + 4 <= n; i += 4)
        acc = vsubq_s32(acc, vreinterpretq_s32_u32(vcgtq_f32(vld1q_f32(x + i), t)));  /* acc -= (-1 per hit) */
    size_t c = (size_t)vaddvq_s32(acc);
    for (; i < n; i++) c += x[i] > thr;
    return c;
}
```

**Fast reciprocal square root** for N-body / normalisation: `vrsqrteq_f64(d2)` gives ~8 correct bits; each Newton step `y = y * vrsqrtsq_f64(d2 * y, y)` doubles them (2 steps → ~1e-12 for f64, 1 step → ~1e-7 for f32). Cheaper than `vsqrtq` + `vdivq` (both ~10-cycle latency, not pipelined well). Accuracy trade-offs like this are why a simulation's energy drift must be checked after every kernel change (exercise 19.9).

## 6. Portable SIMD: vector extensions

clang and GCC accept a type attribute that gives you SIMD arithmetic without intrinsics and without an ISA guard:

```c
typedef double v2d __attribute__((vector_size(16)));   /* two doubles; on x86 the same type maps to SSE2 */
typedef float  v8f __attribute__((vector_size(32)));   /* eight floats; AVX on x86, two NEON registers on arm64 */

v2d a = {1, 2}, b = {3, 4};
v2d c = a * b + a;                 /* elementwise; compiles to fmla */
double s = c[0] + c[1];            /* lane access */
v2d m = a > b;                     /* comparison -> mask vector of -1/0 (as the same-size integer vector type) */
v2d sel = m ? a : b;               /* clang: blend (GCC ≥ 8 too) */
memcpy(&a, ptr, sizeof a);         /* unaligned load; direct *(v2d *)ptr requires 16-byte alignment */
v2d sh = __builtin_shufflevector(a, b, 1, 2);   /* clang shuffle; GCC: __builtin_shuffle */
```

`dot_vext` in `example.c` runs at the same speed as the intrinsics version (10.0 vs 10.2 GFLOP/s in L1). The compiler splits wider vectors than the hardware has (`v8f` → 2× `float32x4_t`) — writing 256-bit-wide code that also runs on NEON is a real portability strategy (Highway, xsimd, and `std::simd`/`std::experimental::simd` in C++ formalise it). What you lose vs intrinsics: horizontal reductions (`__builtin_reduce_add` exists in clang ≥ 15), lane-broadcast FMAs, and specialised instructions (`rsqrte`, table lookups, saturating integer ops).

## 7. Instruction-level parallelism, dependency chains, unrolling, pipelining

A core issues several instructions per cycle (Apple P-cores: up to 8), but an instruction whose input is the output of the previous one must wait for its **latency**: FMA 4 cycles, integer add 1, L1 load 3-4, L2 load ~15, divide ~10-15. Throughput is the inverse of how often the pipe can *start* a new independent instruction: 4 FMAs per cycle here.

```c
for (i...) s += a[i] * b[i];        /* ONE chain: fma -> fma -> fma; each waits 4 cycles: 0.25 FMA/cycle */
/* measured: 2.2 GFLOP/s in L1, regardless of SIMD width — latency-bound */

for (i += 4) { s0 += ...; s1 += ...; s2 += ...; s3 += ...; }   /* FOUR chains overlap: 1 FMA/cycle */
/* measured: 3.8 GFLOP/s scalar; 10 GFLOP/s with 4 NEON accumulators (4 chains x 2 lanes) */
```

To saturate 4 FMA pipes × 4 cycles latency you need **16 independent accumulators in flight** — 8 NEON registers of 2 doubles covers half of that; the 4×4 kernel's 8 accumulators are chosen for exactly this reason (and 32 NEON registers total limits how many you can hold). This is why `-ffast-math` speeds up reductions: it lets the compiler create the chains for you. Integer reductions do not have this problem (latency 1) — `example.c` §9: 0.09 ns/element int64 vs 0.15 ns double with the same 4 accumulators.

**Loop unrolling** processes k iterations per trip: fewer counter increments/branches, more independent work visible to the scheduler, and it enables the multiple-accumulator pattern. The compiler unrolls automatically (`interleaved count: 4` in the remarks); do it by hand only in intrinsics code, where the compiler is conservative — `saxpy_neon` one vector per iteration ran at 87 GB/s from L1, `saxpy_neon4` at 190 GB/s, the auto-vectorized scalar loop at 220 GB/s (clang unrolled it 4× itself).

**Software pipelining** overlaps iteration i's loads with iteration i−1's compute so the loads' latency is hidden: load the *next* A/B vectors at the top of the body, compute on the ones loaded last time. Out-of-order hardware does most of this for you on Apple/Intel cores (the reorder buffer is 600+ instructions deep); it matters on in-order cores (Cortex-A55, GPUs, DSPs) and for very long-latency loads (prefetch, chapter 18 §7).

```c
/* software-pipelined dot: the loads for iteration i+1 are issued before the FMAs of iteration i */
float64x2_t a_next = vld1q_f64(a), b_next = vld1q_f64(b), s = vdupq_n_f64(0);
for (size_t i = 0; i + 2 < n; i += 2) {
    float64x2_t a_cur = a_next, b_cur = b_next;
    a_next = vld1q_f64(a + i + 2); b_next = vld1q_f64(b + i + 2);   /* in flight while we compute */
    s = vfmaq_f64(s, a_cur, b_cur);
}
s = vfmaq_f64(s, a_next, b_next);
```

Latencies and throughputs you should carry in your head (Apple M-series P-core; x86 Zen4/Golden Cove within ±30%):

| instruction | latency (cycles) | throughput (per cycle) |
|-------------|------------------|------------------------|
| int add/sub/logic | 1 | 6 |
| int multiply | 3 | 2 |
| int divide (64-bit) | ~10-20 | 0.1 |
| FP add / mul / FMA (scalar or NEON) | 3-4 | 4 |
| FP divide / sqrt (f64) | ~10-15 | ~0.25-0.5 |
| `rsqrte` estimate | 3-4 | 1+ |
| L1 load hit | 3-4 | 3 loads + 2 stores |
| L2 hit | ~15 | |
| DRAM | ~400 (100 ns) | limited by outstanding misses (~30-60) |
| mispredicted branch | ~15-20 (pipeline flush) | |

The single most useful ratio: **FMA latency × FMA throughput = 16 independent FMAs must be in flight** to saturate the FP units. Every reduction, every stencil update, every micro-kernel is designed around that number.

## 8. Cache blocking, register blocking, and the matmul progression

All measured on one M5 performance core, `-O2`, doubles, verified `max|diff| = 0` against naive:

| version | N=512 | N=1024 | N=2048 | what changed |
|---------|-------|--------|--------|--------------|
| naive `ijk` | 2.1 GFLOP/s | 1.8 | (17 s, skipped) | inner loop walks a column of B: stride-N, one cache line (and TLB entry) per element |
| `ikj` + `restrict` | 18.8 | 16-18 | 15.3 | inner loop contiguous in B and C; vectorized (width 2 × 4) |
| cache-blocked `ikj` (64,64,256) | 18.5 | 15.7 | 15.7 | **no gain**: see below |
| 4×4 register tile, no packing | 27.5 | 22 | 13.3 | 8 accumulators, 2 FMA per load; but strided B loads thrash at large N |
| packed + blocked + 4×4 kernel | **38.0** | **37.6** | **33.5** | panels packed contiguous; B panel in L2, A panel in L1 |
| Accelerate `cblas_dgemm` | — | **520** | — | AMX matrix coprocessor + multithreading; not reachable from NEON |

Why cache blocking alone did nothing here — and why that is the important lesson: the `ikj` inner loop `c[j] += a * b[j]` does 2 loads + 1 store per FMA vector. A core with 3 load/store slots per cycle can therefore issue ~1 vector FMA per cycle = 4 flops/cycle ≈ 16-19 GFLOP/s — **it is load/store-port bound, not cache bound**. Reducing cache misses (blocking) cannot help a loop that is not waiting on cache misses; on M-series, with a 16 MB L2 and aggressive prefetchers, streaming through B rows is *already* served fast enough for this loop. (On a 2015 laptop with a 256 KB L2 the blocked version wins 2-3x; always measure on the target.)

What *does* help is **register blocking**: hold a 4×4 tile of C in 8 registers so each loaded A/B vector feeds 2-4 FMAs → 27 GFLOP/s at N=512. But that kernel reads B with stride N: at N=2048 the 4-column strip of B touches 2048 different cache lines (256 KB > L1) per tile, and it falls to 13 GFLOP/s. **Packing** fixes it: copy the K×4 strip of B into a contiguous 8 KB buffer once and reuse it for every i-tile; copy the 4×K strip of A likewise. Then add **cache blocking** around the kernel so the packed B panel (KC × NC = 256×512×8 = 1 MB) stays in L2 and the A panel (MC × KC = 64×256×8 = 128 KB) stays in L1 — this is the Goto/BLIS algorithm every BLAS uses:

```
for j0 in 0..N step NC:            # B panel columns
  for p0 in 0..N step KC:          # K block
    pack B[p0:p0+KC, j0:j0+NC] -> Bp     (contiguous 4-column strips)   lives in L2
    for i0 in 0..N step MC:        # A panel rows
      pack A[i0:i0+MC, p0:p0+KC] -> Ap   (contiguous 4-row strips)      lives in L1
      for j in 0..NC step 4:
        for i in 0..MC step 4:
          kernel_4x4(KC, Ap + i*KC, Bp + j*KC, &C[i0+i][j0+j], N)
```

Deriving block sizes: `KC` so that a 4×KC A strip + KC×4 B strip ≪ L1 (2 × 8 KB at KC = 256: fine, and the kernel's K loop is long enough to amortise the C load/store); `MC × KC × 8 ≤ L1/2` (64 × 256 × 8 = 128 KB — at the limit on a 128 KB L1; 32 would be safer on a 64 KB L1); `KC × NC × 8 ≤ L2/2` (1 MB ≪ 16 MB; could go much larger). Powers of two for N cause **set aliasing** (stride 8192 B maps every row to the same L1 set): naive at N=1000 ran 2.9 GFLOP/s vs 1.8 at N=1024. Padding leading dimensions to N+8 avoids it.

The remaining gap to Accelerate is (1) AMX, Apple's undocumented matrix unit that Accelerate uses (≈ 8-10x NEON), (2) 8+ threads, (3) an 8×6 or 12×8 kernel with better FMA:load ratio, prefetching inside the kernel, and edge handling. On x86, OpenBLAS/MKL reach 80-90% of the AVX-512 roof with exactly the structure above and a 24×8 or 32×6 micro-kernel. Your 38 GFLOP/s is ~60% of the one-core NEON roof — the point where you should call `cblas_dgemm` (`-framework Accelerate`; Linux: `-lopenblas`) for production and keep your kernel for understanding.

The packing routines, so the layout is concrete (each 4-wide strip is contiguous along K):

```c
/* Ap[strip][p][r]: for strip s (rows i0+s..i0+s+3), the 4 values A[i0+s+r][p0+p] are adjacent */
static void pack_a(size_t n, const double *A, size_t i0, size_t mc, size_t p0, size_t kc, double *Ap) {
    for (size_t s = 0; s < mc; s += 4)
        for (size_t p = 0; p < kc; p++)
            for (size_t r = 0; r < 4; r++)
                Ap[s * kc + p * 4 + r] = A[(i0 + s + r) * n + p0 + p];
}
/* Bp[strip][p][c]: for strip s (cols j0+s..j0+s+3), the 4 values B[p0+p][j0+s+c] are adjacent */
static void pack_b(size_t n, const double *B, size_t p0, size_t kc, size_t j0, size_t nc, double *Bp) {
    for (size_t s = 0; s < nc; s += 4)
        for (size_t p = 0; p < kc; p++)
            for (size_t c = 0; c < 4; c++)
                Bp[s * kc + p * 4 + c] = B[(p0 + p) * n + j0 + s + c];
}
```

Packing is O(N²) per panel against O(N³) compute, so it costs ~2-5% and buys sequential, aligned, TLB-friendly loads inside the O(N³) kernel. Real BLAS packs into the micro-kernel's exact register shape (MR × KC and KC × NR), and pads edges with zeros so the kernel never sees a partial tile.

Stencils (FDTD, heat equation) block the same way: tile the 2-D grid so a tile plus its halo fits in L1 and sweep time steps over the tile ("temporal blocking") when the update is cheap relative to the load:

```c
/* 2-D 5-point heat update, tiled: each TI x TJ tile (+1 halo) is L1-resident; inner j loop vectorizes */
for (size_t i0 = 1; i0 < n - 1; i0 += TI)
    for (size_t j0 = 1; j0 < n - 1; j0 += TJ)
        for (size_t i = i0; i < min(i0 + TI, n - 1); i++) {
            const double *restrict up = u + (i - 1) * n, *restrict mid = u + i * n, *restrict dn = u + (i + 1) * n;
            double *restrict out = v + i * n;
            for (size_t j = j0; j < min(j0 + TJ, n - 1); j++)
                out[j] = mid[j] + r * (up[j] + dn[j] + mid[j - 1] + mid[j + 1] - 4 * mid[j]);
        }
/* arithmetic intensity: 6 flops / (5 loads + 1 store, mostly cache hits) -> memory-bound from DRAM,
   near the compute roof when the three rows sit in L1: TI*TJ*8*3 <= 64 KB, e.g. TI=16, TJ=512 */
```

## 9. Branchless code

A branch predictor guesses each branch's direction; a mispredict flushes the pipeline (~15-20 cycles). Branches on *data* (random, 50/50) mispredict half the time; branches on *structure* (loop ends, sorted data) are free.

```c
if (in[i] > thr) out[k++] = in[i];        /* branchy compaction */
out[k] = in[i]; k += (in[i] > thr);       /* branchless: always store, conditionally advance */
// random input: branchy 10.3 ms, branchless 0.94 ms  (11x)
// sorted input: branchy 0.92 ms, branchless 0.95 ms  (branch predicted perfectly)
```

Compilers convert `x = c ? a : b` and small `if` assignments to `csel`/`cmov` on their own ("if-conversion"); they will *not* speculate stores or loads that might fault, so patterns like compaction, `if (cond) sum += x` with a side effect, or early-exit searches need your help. Arithmetic masks: `mask = -(int64_t)(x > t)` is all-ones or zero; `y = (a & mask) | (b & ~mask)`; `y = t + ((x - t) & mask)` is `max`. In SIMD there are no branches at all — masks/blends are the only way (§5). When the branch is *predictable* (e.g. 99% of tokens are ASCII), the branchy version is faster: it skips the work; branchless always does both sides.

Related: **avoid divisions** (`x / y` → `x * inv_y` with one reciprocal per row; 10-15 cycle latency vs 4), avoid `pow(x, 2.0)` (use `x*x`), avoid `int → double` conversions inside the loop, avoid `%` (use `&` for powers of two).

## 10. Denormals, FMA, and the last bit

**Subnormals** (|x| < 2.2e-308 double, 1.2e-38 float; chapter 12) are handled by microcode on x86 — 50-100x slower per operation — and in hardware at full speed on Apple Silicon (`example.c` §8 measured 1.03x). They arise from `exp(-large)` in softmax tails, decaying signals (`x *= 0.999` a million times), tiny gradients in deep nets. On x86 enable flush-to-zero: `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(...)` (`<xmmintrin.h>`), or `-ffast-math` (which sets them at startup). PyTorch's `torch.set_flush_denormal(True)` is this. Clamp inputs to `exp` instead where you can.

**FMA** computes `a*b + c` with a single rounding (C11 §7.12.13, `fma()`). `a*b - c` computed as two operations rounds the product first: for `a = 1 + 2⁻²⁷`, `b = 1 - 2⁻²⁷`, `a*b - 1` is `0.0` in two steps and the exact `-2⁻⁵⁴` with FMA. clang's default `-ffp-contract=on` fuses within an expression; `-ffp-contract=off` forbids it; `=fast` fuses across statements. This is why the same C code gives bit-different results on a machine without FMA (or with a different contraction policy), and why your gradient check tolerance must be `1e-6`-ish relative, never `== 0.0`. When you *want* the accuracy — compensated dot products, `x*x - y*y`, Newton steps — call `fma()` explicitly.

## 11. Integer vs float throughput

Per cycle an M-series P-core can issue ~6 integer ops, 4 FP/NEON ops, 3 loads, 2 stores. Integer add latency is 1 cycle, FP add/mul/FMA 3-4, FP divide ~10, integer divide ~10-20. Consequences: index arithmetic is nearly free next to FP; an f32 kernel has 2x the lanes of f64 (4 vs 2 per register) — **use `float` for ML weights and activations**, `double` for accumulators and physics energies; `int8`/`int16` quantized matmul gets 8-16 lanes (ggml's Q8_0 kernels use `vdotq_s32`: 4×4 int8 dot products per lane per instruction); and FP reductions need 4x more independent accumulators than integer ones (§7).

## 12. Alignment for SIMD

`vld1q_f64` works on any address on arm64, but a load that straddles two cache lines costs two accesses; on x86 `_mm_load_pd` (aligned) *faults* on a misaligned address while `_mm_loadu_pd` tolerates it. Allocate SIMD buffers with `aligned_alloc(64, round_up(bytes, 64))` (chapter 18 §3), pad row lengths to a multiple of the vector width (`ld = round_up(n, 4)`), and peel the first few scalar iterations if you cannot control the input alignment. Tell the compiler: `__builtin_assume_aligned(p, 64)` lets it use aligned loads and drop the peel. Structs of vectors: `float64x2_t` has 16-byte alignment; a struct holding one is 16-aligned automatically.

```c
/* alignment peel: handle leading elements until p is 16-byte aligned, then run the SIMD loop */
size_t i = 0, mis = ((uintptr_t)x & 15) / sizeof(float);          /* 0..3 misaligned floats */
for (; mis && mis < 4 && i < n; i++, mis++) y[i] = a * x[i] + y[i];   /* peel (x aligned after this) */
const float *xa = __builtin_assume_aligned(x + i, 16);            /* promise to the compiler */
for (; i + 4 <= n; i += 4) { /* ... vld1q_f32(xa + ...) ... */ }
/* NB: if x and y have DIFFERENT misalignments only one can be aligned; pad allocations so both are. */
```

The padding rule for matrices: allocate `rows × ld` with `ld = round_up(cols, 8)` doubles (64 bytes) so every row starts on a cache line and the kernel never needs a tail — that is what `lda`/`ldb`/`ldc` in the BLAS interface are for, and why your `Mat` from chapter 18 carries a row stride separate from `cols`.

## 13. Profiling and hardware counters

Benchmarks tell you *how fast*; profilers tell you *where*. macOS:

```sh
xcrun xctrace record --template 'Time Profiler' --launch ./prog --output prof.trace   # then open prof.trace
xcrun xctrace record --template 'CPU Counters'  --launch ./prog --output ctr.trace    # cycles, instructions, cache misses, branch mispredicts
sample ./prog 5                    # 5-second poor man's profiler: stack samples, no setup
cc -g -O2 ...                      # keep -g: Instruments shows source lines with optimized code
```

In Instruments' Time Profiler, turn on "Invert call tree" and "Hide system libraries"; the *Disassembly* view shows per-instruction sample counts — the `fmla` that is waiting on a load lights up. CPU Counters can record `INST_RETIRED`, `L1D_CACHE_MISS_LD`, `BRANCH_MISPRED_NONSPEC`, `FIXED_CYCLES`: IPC = instructions/cycles (≥ 3 is good, < 1 means you are stalled on memory or a dependency chain); mispredicts/branch > 5% says "branchless it".

Linux: `perf stat -e cycles,instructions,cache-misses,branch-misses ./prog` for the counters, `perf record -g ./prog && perf report` for the profile, `perf annotate` for per-instruction; `valgrind --tool=cachegrind` simulates the cache hierarchy exactly (slow, deterministic). `llvm-mca` (both platforms, `brew install llvm`) statically estimates a loop's throughput per iteration from its assembly — the right tool for "how many cycles *should* this kernel take".

```
$ perf stat -e cycles,instructions,L1-dcache-load-misses,branch-misses ./ex_demo      (Linux)
     3,912,004,118      cycles
    11,240,377,561      instructions              #    2.87  insn per cycle
        91,772,033      L1-dcache-load-misses
        22,961,405      branch-misses             #    0.55% of all branches

$ $(brew --prefix llvm)/bin/llvm-mca -mcpu=apple-m1 -iterations 100 kernel_loop.s | head      (macOS/Linux)
Iterations:        100
Instructions:      1600
Total Cycles:      412
Block RThroughput: 4.0          <- 16 instructions (4 loads + 8 fmla + 4 bookkeeping) per 4 cycles: FMA-bound
```

How to read the counters against the roofline: IPC 2.9 with few cache misses = compute-bound, look at dependency chains; IPC < 1 with many L1/L2 misses = memory-bound, look at layout and blocking; branch-misses > 5% of branches = branchless the hot `if`. `llvm-mca`'s "Block RThroughput" is the cycles per loop trip if nothing stalls on memory — if your measurement is 3x slower than that, memory is the problem, not the instruction mix.

**Instruments walk-through** for a training loop: record with Time Profiler → in the call tree, `mat_mul` 71%, `relu_backward` 9%, `softmax` 6% → double-click `mat_mul` → the disassembly shows 80% of samples on two `ldr`/`fmla` pairs inside one loop → that loop is the target for §14. After optimising, re-profile: the distribution moved, `mat_mul` is 40%, and `relu_backward` (a bandwidth-bound elementwise pass) is now worth fusing into the preceding matmul epilogue — which is how real frameworks end up with "fused kernels".

## 14. The full worked optimization of `mat_mul`

Putting it together — the path, the number, and the reason, so you can repeat it on any kernel:

| step | change | GFLOP/s (N=512) | tool that told you what to do |
|------|--------|-----------------|-------------------------------|
| 0 | naive `ijk`, `-O0` | ~0.3 | — |
| 1 | `-O2` | 2.1 | `-Rpass-missed`: "cost model: not beneficial" (stride-N gather) |
| 2 | swap to `ikj`, add `restrict` | 18.8 | `-Rpass`: "vectorized, width 2, interleave 4"; roofline: at the load/store roof |
| 3 | cache blocking only | 18.5 | *no change* → the bottleneck is not cache; `llvm-mca`/counters: IPC high, loads:FMA = 3:1 |
| 4 | 4×4 register tile (8 accumulators, `vfmaq_laneq`) | 27.5 | assembly: 8 `fmla` per 4 loads; falls at N=2048 (strided B: L1 misses) |
| 5 | pack A and B strips, block for L1/L2 | 38.0 | steady across N; ~60% of the NEON roof |
| 6 | (next) 8×6 kernel, prefetch inside the kernel, threads | 50-60 / core | diminishing; call `cblas_dgemm` at 520 |

Each step was verified with `max|diff|` against the naive result and timed with warm-up + median. This is the discipline: **measure → identify the roof you are hitting → change the one thing that raises that roof → verify → repeat**. When the kernel is within 2x of the hardware roof, stop and go optimise the algorithm instead (fewer flops beats faster flops: Barnes–Hut over direct sum, FFT over DFT, KV-cache over recomputation).

---

## Gotchas and undefined behavior

- `restrict` is a *promise*: if the arrays do overlap, the vectorized code silently computes garbage (UB, C11 §6.7.3.1). `mat_mul(a, b, a)` with `restrict` parameters is UB — check `out != a && out != b` at the API boundary.
- `*(v2d *)ptr` with `ptr` not 16-byte aligned: UB (§6.3.2.3p7). Use `memcpy` into the vector or `vld1q_f64` (which is defined for any alignment).
- Casting `float *` to `int32_t *` to do bit tricks violates strict aliasing (§6.5p7; chapter 15): use `memcpy` or `vreinterpretq_s32_f32`, which is exactly what those intrinsics exist for.
- `-ffast-math` makes `isnan()`/`isinf()` return false and deletes Kahan summation. It is a *per-file* decision, never global.
- A benchmark whose result is unused measures nothing; a benchmark of a function inlined into `main` with constant arguments measures a specialised version — `volatile` sinks and `noinline`.
- The scalar tail loop `for (; i < n; i++)` after a SIMD loop is where off-by-one bugs live; test with `n` not a multiple of the width (n = 1, 7, 9, 4095).
- Reductions in a different order give different last bits — tests use tolerances (`fabs(a-b) <= 1e-12 * fabs(a)`), never `==`.
- Signed integer overflow in index arithmetic (`int i * int n` past 2³¹) is UB and breaks vectorization analysis; use `size_t`.
- Timing with `clock()` measures CPU time summed over threads; use `clock_gettime(CLOCK_MONOTONIC)` for wall time (chapter 13).
- Power-of-two array dimensions cause cache-set conflicts (N=1024: 1.8 GFLOP/s, N=1000: 2.9 for the naive loop). Pad the leading dimension.

## Common mistakes checklist

- [ ] Optimising before profiling — 90% of the time is in one loop; find it first.
- [ ] Trusting a single timing, or the first (cold) run.
- [ ] Writing intrinsics before checking `-Rpass=loop-vectorize` — the compiler often already did it (saxpy: auto-vectorized scalar beat one-vector intrinsics).
- [ ] One accumulator in a reduction.
- [ ] Forgetting `restrict` and wondering why the loop is scalar.
- [ ] Cache blocking a loop that is load/store-bound; register blocking is what it needed.
- [ ] Ignoring the scalar tail or assuming `n % 4 == 0`.
- [ ] Global `-ffast-math`.
- [ ] Comparing to a BLAS and concluding your code is "wrong" — Accelerate uses a coprocessor; 60% of the NEON roof is excellent.
- [ ] Benchmarking a kernel on L1-sized data and extrapolating to DRAM-sized problems (saxpy: 220 GB/s vs 120 GB/s).

## You can move on when...

- You can compute the arithmetic intensity of your N-body inner loop and your `mat_mul`, place both on a roofline sketch, and predict the attainable GFLOP/s within 2x of what you measure.
- `-Rpass=loop-vectorize` reports your matrix add, scale, and `ikj` loops vectorized, and you can explain every "not vectorized" remark in your matrix library.
- You can find the `fmla` loop in `-S` output of your dot product and count FMAs per load.
- Your NEON dot product and 4×4 kernel match the scalar versions to 1e-12 relative on sizes 1..4097 and reach ≥ 8 GFLOP/s (dot, L1) and ≥ 25 GFLOP/s (matmul, N=512).
- You can explain, with your own numbers, why 4 accumulators beat 1, why branchless compaction is 10x faster on random data and equal on sorted data, and why cache blocking alone did not speed up `ikj` on this machine.
- Your `mat_mul` reaches ≥ 30 GFLOP/s single-core with packing and blocking, and you have compared it against `cblas_dgemm`.
