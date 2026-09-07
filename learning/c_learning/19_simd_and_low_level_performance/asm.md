# 19 — asm.md: NEON, accumulators, and the register file as a resource

Companion to `lesson.md` §4–8. Prerequisite: `13_debugging_testing_perf/asm.md`.
Output is `cc -O2 -S -o -` on this machine; noise lines removed.

## What to look for

* NEON registers are `v0`–`v31`, 128 bits each. Names: `q` = the whole 128 bits,
  `d` = low 64, `s` = low 32. A suffix says how to *slice* it: `.2d` = two
  doubles, `.4s` = four floats, `.16b` = sixteen bytes.
* `fmla.2d v0, v1, v2` is two fused multiply-adds. `fmla.2d v0, v1, v3[0]` is
  two, against a *broadcast* scalar — that `[0]` is free and does the work of
  `vdupq_n`.
* `ld1`/`ldp` of `q` registers move 16 or 32 bytes per instruction.
  `faddp` / `addv` / `vaddvq` are the **horizontal** reduction at the end.
* The dot product's real problem is not SIMD, it is the **accumulator**: one
  running sum is a serial dependency chain. Multiple accumulators are what
  actually buys the speed, and you can see the chain (or its absence) in the asm.
* Register pressure is visible: `; 16-byte Folded Spill` / `Reload` comments in
  the inner loop mean your kernel is too wide for the machine.

## The C

```c
#include <arm_neon.h>
double dot_scalar(const double *restrict a, const double *restrict b, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}
double dot_neon(const double *a, const double *b, size_t n) {
    float64x2_t acc0 = vdupq_n_f64(0.0), acc1 = vdupq_n_f64(0.0);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        acc0 = vfmaq_f64(acc0, vld1q_f64(a+i),   vld1q_f64(b+i));
        acc1 = vfmaq_f64(acc1, vld1q_f64(a+i+2), vld1q_f64(b+i+2));
    }
    double s = vaddvq_f64(vaddq_f64(acc0, acc1));
    for (; i < n; i++) s += a[i]*b[i];
    return s;
}
void saxpy(float *restrict y, const float *restrict x, float alpha, size_t n) {
    for (size_t i = 0; i < n; i++) y[i] += alpha * x[i];
}
/* C[4][4] += A[4][k] * B[k][4] — the register-blocked micro-kernel */
void kern4x4(const double *restrict A, const double *restrict B,
             double *restrict C, size_t k, size_t ld) {
    float64x2_t c00=vdupq_n_f64(0), c01=vdupq_n_f64(0), c10=vdupq_n_f64(0), c11=vdupq_n_f64(0);
    float64x2_t c20=vdupq_n_f64(0), c21=vdupq_n_f64(0), c30=vdupq_n_f64(0), c31=vdupq_n_f64(0);
    for (size_t p = 0; p < k; p++) {
        float64x2_t b0 = vld1q_f64(B + p*ld), b1 = vld1q_f64(B + p*ld + 2);
        double a0=A[0*ld+p], a1=A[1*ld+p], a2=A[2*ld+p], a3=A[3*ld+p];
        c00=vfmaq_n_f64(c00,b0,a0); c01=vfmaq_n_f64(c01,b1,a0);
        c10=vfmaq_n_f64(c10,b0,a1); c11=vfmaq_n_f64(c11,b1,a1);
        c20=vfmaq_n_f64(c20,b0,a2); c21=vfmaq_n_f64(c21,b1,a2);
        c30=vfmaq_n_f64(c30,b0,a3); c31=vfmaq_n_f64(c31,b1,a3);
    }
    /* ...then 8 load / fadd / store pairs writing C back. */
}
```

## The assembly

### saxpy: the easy case, and what a fully vectorised loop looks like

```
LBB3_6:                                  ; y[i] += alpha * x[i], 16 floats per iteration
	ldp	q1, q2, [x9, #-32]       ; 8 floats of x
	ldp	q3, q4, [x9], #64        ; 8 more, pointer += 64 bytes
	ldp	q5, q6, [x10, #-32]      ; 8 floats of y
	ldp	q7, q16, [x10]           ; 8 more
	fmla.4s	v5, v1, v0[0]            ; y += alpha*x — FOUR floats, alpha broadcast from v0 lane 0
	fmla.4s	v6, v2, v0[0]
	fmla.4s	v7, v3, v0[0]
	fmla.4s	v16, v4, v0[0]
	stp	q5, q6, [x10, #-32]
	stp	q7, q16, [x10], #64
	subs	x11, x11, #16
	b.ne	LBB3_6
```

12 instructions, 16 elements, 32 FLOPs: **1.3 instructions per element.** `.4s`
because these are `float`s — four per register instead of two. The four `fmla`s
are independent (four different destinations) so they pipeline.

You wrote no intrinsics for this. `saxpy` has no loop-carried dependency (each
`y[i]` is separate) and `restrict` proved no overlap, so the auto-vectoriser had
everything it needed. **Try the plain loop with `restrict` first, always.**

Below `LBB3_6` there are two more loops — a 4-at-a-time `ldr q1` version, and a
scalar `ldr s1` / `fmadd` version. Those handle `n % 16` and `n % 4`. A vectorised
function is normally *three* loops: main, partial, remainder. That is why SIMD
helps only when `n` is big; for `n = 5` you run the slow path exclusively.

### dot_scalar: SIMD loads, serial adds — the trap

`dot_scalar` also vectorised, but look what it produced:

```
LBB0_5:                                  ; 8 elements per iteration
	ldp	q1, q2, [x9, #-32]
	ldp	q3, q4, [x9], #64
	ldp	q5, q6, [x10, #-32]
	ldp	q7, q16, [x10], #64
	fmul.2d	v1, v1, v5               ; the MULTIPLIES are vector...
	mov	d5, v1[1]                ; ...then extract lane 1 back to a scalar
	fmul.2d	v2, v2, v6
	mov	d6, v2[1]
	fmul.2d	v3, v3, v7
	mov	d7, v3[1]
	fmul.2d	v4, v4, v16
	mov	d16, v4[1]
	fadd	d0, d0, d1               ; ...and the ADDS are eight SCALAR fadds,
	fadd	d0, d0, d5               ;    each depending on the previous one.
	fadd	d0, d0, d2
	fadd	d0, d0, d6
	fadd	d0, d0, d3
	fadd	d0, d0, d7
	fadd	d0, d0, d4
	fadd	d0, d0, d16
	subs	x11, x11, #8
	b.ne	LBB0_5
```

This is the most instructive listing in the chapter. The compiler vectorised the
*loads and multiplies* but was **not allowed** to vectorise the sum: `s += ...`
must accumulate in exactly the source order, because floating-point addition is
not associative (chapter 12). So it unrolled 8×, did the multiplies in SIMD, then
extracted every lane and fed a single serial `fadd` chain — eight `fadd`s at ~3
cycles latency each, back to back. **The loop is limited by 24 cycles of
dependency, and the SIMD did nothing.** Also note `fmul` + `fadd` instead of
`fmadd`: fusing is now impossible because the multiply is vector and the add is
scalar. The rewrite lost an instruction *and* a rounding.

`-ffast-math` waives associativity and this collapses into `fmla.2d` — but then
you have changed the arithmetic globally, including in code you did not intend to
change. Better: change the source.

### dot_neon: two accumulators, and the horizontal add

```
LBB2_3:                                  ; 4 doubles per iteration
	ldp	q2, q3, [x8, #-16]       ; a[i..i+3]
	ldp	q4, q5, [x9, #-16]       ; b[i..i+3]
	fmla.2d	v0, v4, v2               ; acc0 += a*b   (2 doubles)
	add	x10, x11, #8
	fmla.2d	v1, v5, v3               ; acc1 += a*b   — INDEPENDENT of v0
	add	x11, x11, #4
	add	x8, x8, #32
	add	x9, x9, #32
	cmp	x10, x2
	b.ls	LBB2_3
; %bb.4:
	fadd.2d	v0, v1, v0               ; combine the two accumulators
LBB2_5:
	faddp.2d d0, v0                  ; horizontal: d0 = v0.d[0] + v0.d[1]
```

Ten instructions, 4 elements, 8 FLOPs — and crucially the two `fmla`s target
different registers, so the dependency chain is now **two iterations long, not
one**. With `fmla` latency ≈ 4 cycles and throughput ≈ 4/cycle, one accumulator
gets you 1/16th of peak; you need roughly *latency × throughput* = 8–16
independent accumulators to saturate the FP units. Two is a big improvement over
one; eight is where you actually want to be.

`faddp.2d d0, v0` is the horizontal reduction — it happens **once, after the
loop**, which is the whole point. A horizontal op inside a loop is a serialising
mistake; `vaddvq_f64` outside it is free.

Note also `movi.2d v0, #0000000000000000` in the preamble: that is `vdupq_n_f64(0)`,
zeroing a vector register with an immediate.

You can get most of this without intrinsics by using explicit accumulator
variables:

```c
double s0=0,s1=0,s2=0,s3=0;
for (; i+4 <= n; i += 4) { s0+=a[i]*b[i]; s1+=a[i+1]*b[i+1];
                           s2+=a[i+2]*b[i+2]; s3+=a[i+3]*b[i+3]; }
```
```
LBB1_3:
	ld2.2d	{ v2, v3 }, [x9], #32    ; DE-INTERLEAVING load: evens -> v2, odds -> v3
	ld2.2d	{ v4, v5 }, [x8], #32
	fmla.2d	v0, v4, v2
	fmla.2d	v1, v5, v3
	...
```
Four named accumulators became two vectors, exactly as intended. The `ld2.2d` is
the cost of writing it with strided indices — a de-interleaving load, because
`s0`/`s2` want elements 0,2 and `s1`/`s3` want 1,3. It works, and it is portable C
with no intrinsics. That trade is usually the right one.

### The 4×4 micro-kernel: eight accumulators, zero spills

```
LBB0_2:                                  ; one k-step of a 4x4 block
	ldr	d16, [x0, x11]           ; a1 = A[1*ld+p]
	ldr	d17, [x0, x8]            ; a2 = A[2*ld+p]
	ldp	q18, q19, [x10, #-16]    ; b0, b1 = B[p][0..3]   — 32 bytes, one instruction
	ldr	d20, [x0, x9]            ; a3 = A[3*ld+p]
	ld1r.2d	{ v21 }, [x0], #8        ; a0, BROADCAST to both lanes, pointer post-incremented
	fmla.2d	v6,  v21, v18            ; c00
	fmla.2d	v7,  v21, v19            ; c01
	fmla.2d	v5,  v18, v16[0]         ; c10  — the [0] broadcasts a1 with no extra instruction
	fmla.2d	v4,  v19, v16[0]         ; c11
	fmla.2d	v3,  v18, v17[0]         ; c20
	fmla.2d	v2,  v19, v17[0]         ; c21
	fmla.2d	v1,  v18, v20[0]         ; c30
	fmla.2d	v0,  v19, v20[0]         ; c31
	add	x10, x10, x11
	subs	x3, x3, #1
	b.ne	LBB0_2
```

**16 instructions, 8 `fmla.2d` = 32 FLOPs: 2 FLOPs per instruction, and 5 loads
feeding 8 FMAs.** Read the register allocation: `v0`–`v7` hold the eight
accumulators and never leave registers for the whole `k` loop; `v16`–`v21` are
the operands, reloaded each step. Not a single spill comment.

That ratio is the entire reason register blocking works. A naive `C[i][j] +=
A[i][p]*B[p][j]` does 2 loads + 1 store per FMA. This kernel does 5 loads per 8
FMAs, because each `b0` is reused by four rows and each `a` by two columns. You
did not reduce the arithmetic — you reduced the *memory traffic per FLOP*, which
is the axis the roofline model (§1) measures.

`ld1r.2d { v21 }, [x0], #8` is worth naming: "load one element, **r**eplicate to
all lanes, post-increment". `vfmaq_n_f64(acc, vec, scalar)` compiles to either
this or the `vN[0]` by-element form; both are free broadcasts.

### Too wide: what a spill looks like

Widen the same kernel to 8×8 — 32 accumulator vectors — and the register file
(32 total, and some are needed for operands) runs out:

```
LBB0_2:
	stp	q17, q23, [sp]                  ; 32-byte Folded Spill    ← accumulators going to memory
	stp	q2, q3, [sp, #32]               ; 32-byte Folded Spill
	ldp	q3, q2, [x15, #-32]
	...
	ldr	q23, [sp, #96]                  ; 16-byte Folded Reload   ← and coming back
	fmla.2d	v23, v4, v2
	str	q23, [sp, #96]                  ; 16-byte Folded Spill    ← load-modify-store per FMA!
	...
	mov.16b	v17, v16                        ; register shuffling, pure overhead
	mov.16b	v16, v7
```

**64 instructions for 32 `fmla`s, 10 of them spill/reload traffic**, versus 16
instructions for 8 FMAs in the 4×4. FLOPs per instruction fell from 2.0 to 1.0.
The accumulators that spilled are now doing `ldr` → `fmla` → `str` every single
iteration: the exact memory traffic register blocking existed to eliminate.

Grep is your tool here:

```
$ cc -O2 -S kernel.c -o - | grep -c 'Folded Spill'
```

Zero in the inner loop is the target. If you see spills, your block is too big;
shrink it until they disappear. That is how you *choose* 4×4 rather than
guessing — the ARM64 vector file is 32 registers, so a `MR × NR` block of doubles
needs `MR*NR/2` accumulators plus operand space, and 4×4 (8) and 8×4 (16) fit
while 8×8 (32) does not.

## Read it yourself

1. Compile `dot_scalar` with `-ffast-math` and diff against the listing above.
   Count the `fmul`+`fadd` pairs before and the `fmla.2d`s after. Then decide
   whether you would ship that flag, and why the source rewrite is safer.
2. Change `dot_neon` to use one accumulator instead of two. Predict what happens
   to the inner loop's instruction count (barely changes) and to its runtime
   (roughly doubles). Measure both.
3. Convert `kern4x4` to `float` (`float32x4_t`, `.4s`). Predict the new FLOPs per
   instruction and how much wider a block you could now afford before spilling.
4. Grow `kern4x4` to 6×4 and then 8×4, grepping for `Folded Spill` each time.
   Find the exact width where spills appear on this machine, and check it against
   "32 vector registers".
5. Remove `restrict` from `saxpy` and diff. Predict which extra basic block
   appears (hint: chapter 13's `ccmp`) and whether the `fmla.4s` loop survives.

## Takeaways

* `v0`–`v31`, 128 bits; the `.2d`/`.4s`/`.16b` suffix chooses the lane width.
  `fmla` with a `[0]` operand broadcasts a scalar for free.
* Auto-vectorisation handles `saxpy`-shaped loops perfectly given `restrict`.
  Write the plain loop first and check with `-Rpass=loop-vectorize`.
* A reduction is different: FP non-associativity means the compiler will
  vectorise the multiplies and leave a **serial scalar `fadd` chain**. Multiple
  accumulators in the *source* is the fix; you need ~8 to saturate the FP units.
* Horizontal reductions (`faddp`, `addv`, `vaddvq`) belong after the loop, never
  inside it.
* Register blocking is about FLOPs per load, not FLOPs total: the 4×4 kernel does
  8 FMAs on 5 loads with zero spills.
* `grep -c 'Folded Spill'` on the inner loop is a hard, objective signal that
  your kernel is too wide. Shrink until it reads zero.
