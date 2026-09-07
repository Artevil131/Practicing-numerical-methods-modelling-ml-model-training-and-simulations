# 13 — asm.md: the matmul loop-order experiment, read in the instruction stream

Companion to `lesson.md` §14 and §17. Prerequisite: `05_pointers/asm.md`.
Output is `cc -O2 -S -o -` on this machine (**-O2, not -O1** — this chapter is
about what the optimiser does); noise lines removed.

## What to look for

* The classic explanation of ijk-vs-ikj is "cache locality". True, but it is not
  the whole story: at `-O2` the two orders compile to **structurally different
  loops**. One is scalar, one is vectorised and unrolled 4×.
* The ijk inner loop has a **loop-carried dependency**: every `fmadd` needs the
  previous `fmadd`'s result. That serialises the loop at latency, not throughput.
* The ikj inner loop accumulates into *memory*, and each `j` is independent —
  which is exactly the condition the vectoriser needs.
* `-Rpass=loop-vectorize` tells you which loops were vectorised, and
  `-Rpass-missed` tells you why not. Read those two flags before reading asm.
* Look for `.2d` suffixes and `q` registers: that is 128-bit NEON, two doubles
  per instruction.
* Count instructions per element. That is a cost model you can compute by hand
  and check against `clock_gettime`.

## The C

The two loop orders from lesson §14, unchanged:

```c
void mm_ijk(const double *A, const double *B, double *C, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0.0;
            for (int k = 0; k < n; k++) s += A[i*n+k] * B[k*n+j];
            C[i*n+j] = s;
        }
}
void mm_ikj(const double *A, const double *B, double *C, int n) {
    for (int i = 0; i < n; i++)
        for (int k = 0; k < n; k++) {
            double a = A[i*n+k];
            for (int j = 0; j < n; j++) C[i*n+j] += a * B[k*n+j];
        }
}
```

## The assembly

### Step 0: ask the compiler before you read anything

```
$ cc -O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -c mm.c
mm.c:5:13: remark: the cost-model indicates that vectorization is not beneficial
    5 |  for (int k = 0; k < n; k++) s += A[i*n+k] * B[k*n+j];
mm.c:13:13: remark: vectorized loop (vectorization width: 2, interleaved count: 4)
   13 |  for (int j = 0; j < n; j++) C[i*n+j] += a * B[k*n+j];
```

Two lines, and the whole experiment is explained. The ijk inner loop was **not**
vectorised; the ikj inner loop was, with width 2 (two `double`s per 128-bit
vector) and interleave 4 (four vectors per iteration) = **8 elements per
iteration**. Learn this flag before you learn to read asm: it turns "the profiler
says this is slow" into "the compiler says exactly why".

### ijk: the inner loop is a serial dependency chain

```
LBB0_4:                                  ; the k loop
	ldr	d1, [x16], #8            ; A[i*n+k]  — post-index walk: stride 8 bytes (contiguous)
	ldr	d2, [x15]                ; B[k*n+j]
	fmadd	d0, d1, d2, d0           ; s = d1*d2 + s        ← d0 in, d0 out
	add	x15, x15, x9             ; B pointer += n*8      (x9 = n*8, a whole ROW)
	subs	x14, x14, #1             ; k counter
	b.ne	LBB0_4
```

Six instructions per element. Two things to notice, and they are different
things:

**1. The stride.** `ldr d1, [x16], #8` walks `A` by 8 bytes — contiguous, one
cache line serves 8 iterations. `add x15, x15, x9` walks `B` by `n*8` bytes —
one whole row per iteration, so every single load of `B` touches a fresh cache
line. At `n = 512` that is a 4 KB stride: guaranteed miss, and quite possibly a
TLB miss. This is the "cache locality" story, and you can read the stride
directly off the two pointer-increment instructions.

**2. The dependency.** `fmadd d0, d1, d2, d0` reads `d0` and writes `d0`.
Iteration k+1's `fmadd` cannot start until iteration k's has finished. `fmadd`
has ~4 cycles of latency but the core can issue 2–4 per cycle — so this loop runs
at **one fmadd per 4 cycles**, using maybe 1/8th of the FP throughput, even with
every operand in L1. That is a **loop-carried dependency chain**, and it is why
the vectoriser refused: it cannot reorder additions of floats without changing
the result (chapter 12 — FP addition is not associative), so it cannot split `s`
into independent lanes. `-ffast-math` gives it permission and it will.

Everything above `LBB0_4` is loop setup you can mostly skim, but two lines are
worth knowing:

```
	ubfiz	x9, x3, #3, #32          ; x9 = (uint32)n << 3  = the row stride in BYTES
	mul	x12, x8, x10             ; i*n ...
	add	x12, x2, x12, lsl #3     ; ... &C[i*n], hoisted OUT of the j loop
```

The compiler already did the strength reduction you might have done by hand:
`i*n` is computed once per `i`, not once per element.

### ikj: a different loop entirely

The same source shape, one index swap, and the inner loop is now this:

```
LBB1_8:                                  ; the VECTOR j loop — 8 doubles per iteration
	ldp	q1, q2, [x20, #-32]      ; load 4 doubles of B  (q = 128-bit register, 2 doubles)
	ldp	q3, q4, [x20], #64       ; load 4 more, and advance the pointer by 64 bytes
	ldp	q5, q6, [x19, #-32]      ; load 4 doubles of C
	ldp	q7, q16, [x19]           ; load 4 more
	fmla.2d	v5, v1, v0[0]            ; C += a*B, TWO doubles at a time
	fmla.2d	v6, v2, v0[0]            ;   v0[0] = the scalar `a`, broadcast to both lanes
	fmla.2d	v7, v3, v0[0]            ;   four INDEPENDENT fmlas: no chain
	fmla.2d	v16, v4, v0[0]
	stp	q5, q6, [x19, #-32]      ; store 4 doubles of C
	stp	q7, q16, [x19], #64      ; store 4 more, advance by 64
	subs	x21, x21, #8
	b.ne	LBB1_8
```

Read it against the ijk loop:

| | ijk inner (`k`) | ikj inner (`j`) |
|---|---|---|
| elements per iteration | 1 | 8 |
| instructions per iteration | 6 | 12 |
| **instructions per element** | **6** | **1.5** |
| FP ops per iteration | 1 fmadd | 4 `fmla.2d` = 8 FLOPs-worth |
| dependency chain | `d0 → d0`, every iteration | none between iterations |
| stride on `B` | `n*8` bytes | 8 bytes |

Four times fewer instructions per element, *and* the four `fmla`s in one
iteration are independent of each other so they pipeline. `fmla.2d v5, v1, v0[0]`
is "multiply each of `v1`'s two doubles by lane 0 of `v0`, add into `v5`" — the
`[0]` is a *by-element* multiply, the hardware's way of broadcasting the scalar
`a` for free.

`ldp q1, q2, [x20], #64` loads **32 bytes in one instruction** and post-increments
by 64 — this is where the "unrolled 4×, width 2" from the remark shows up.
Note also that this loop *stores* to `C` every iteration, where ijk kept `s` in a
register and stored once. That is more memory traffic, but the stores are
contiguous, and store bandwidth is not the bottleneck. Contiguity beats fewer
stores.

### The tax on not saying `restrict`

Before it can run the vector loop, ikj checks something:

```
	cmp	x5, x10                  ; does C's row overlap B's array?
	ccmp	x1, x6, #2, lo           ; conditional compare — chain two tests, no branch
	cset	w5, lo                   ; w5 = "the ranges might alias"
	...
	cmp	w3, #8                   ; is n big enough for the vector loop?
	cset	w19, lo
	orr	w19, w19, w5
	tbz	w19, #0, LBB1_7          ; both fine -> vector loop; otherwise ->
LBB1_10 / LBB1_11:                       ; ...the SCALAR fallback loop
	ldr	d1, [x20], #8
	ldr	d2, [x19]
	fmadd	d1, d0, d1, d2
	str	d1, [x19], #8
	subs	x21, x21, #1
	b.ne	LBB1_11
```

The compiler cannot prove `C` and `B` are different arrays, so it emits *both*
loops plus a runtime overlap test, and the scalar version is also the remainder
handler for `n % 8`. Add `restrict` to all three pointers and the `ccmp`/`cset`
alias check vanishes entirely (the `n < 8` guard and the remainder loop stay —
those are unavoidable). That is chapter 13 §17's `restrict` advice, made
concrete: it is not a hint, it is a *proof obligation you take on* so the
compiler can delete a branch.

`ikj` also opens with `stp x22, x21, [sp, #-32]!` and friends — it needs so many
live pointers that it must save callee-saved registers. That is a hint you are
near the register budget; chapter 19 pushes it over.

### Reading this in lldb

```
(lldb) b mm_ikj
(lldb) run
(lldb) di -f                  # disassemble the current function
(lldb) di -s $pc -c 12        # 12 instructions from where we are
(lldb) si                     # step ONE instruction (not one line)
(lldb) register read x19 x20  # the two pointers the inner loop walks
(lldb) register read v5       # see both doubles in the vector register
(lldb) memory read -f f64 -c 8 $x19
```

`di -f` after breaking in a hot function is the fastest way to answer "did this
actually vectorise". Look for `q` registers and `.2d`/`.4s` suffixes.

### The cost model you can compute by hand

Instructions/element from the table above, times `n³` elements, divided by ~4
instructions/cycle and ~3.2 GHz, gives a floor:

* ijk: 6 instr/element, but the fmadd chain caps it at ~1 element per 4 cycles →
  `n³ · 4` cycles ≈ 42 ms at n=512, and cache misses on `B` make it worse.
* ikj: 12 instr / 8 elements = 1.5, issue-limited → `n³ · 1.5 / 4` cycles ≈ 16 ms,
  and in practice better because nothing stalls.

You do not need the numbers to be right. You need the *ratio* to be roughly
right, so that when you measure a 6× difference you can say "yes, that is the
vectorisation plus the stride" instead of guessing. Measure, then check the
measurement against the instruction count; when they disagree, one of your two
mental models is wrong and finding out which is the whole skill.

## Read it yourself

1. Run the `-Rpass-missed=loop-vectorize` command on `mm_ijk` yourself, then add
   `-ffast-math` and re-run. Predict *before* you do: does the remark change, and
   which C language rule did you just waive?
2. Add `restrict` to all three parameters of `mm_ikj` and diff the asm. Predict
   which instructions disappear (hint: grep for `ccmp`). Does the scalar
   remainder loop go away too? Why not?
3. Change `double` to `float` throughout and re-run the vectorize remark.
   Predict the new vectorization width and the new register suffix before
   looking.
4. Compile `mm_ijk` at `-O0`, `-O1`, `-O2`, `-O3` and count the instructions in
   the innermost loop each time. At which level does `i*n` stop being recomputed
   per element?
5. Write the ijk loop with two accumulators (`s0` for even `k`, `s1` for odd,
   summed at the end) and read the inner loop. Predict how many `fmadd`s appear
   and whether they now depend on each other. Time it — you changed the answer's
   last bits, so also check by how much.

## Takeaways

* `-Rpass=loop-vectorize` and `-Rpass-missed=loop-vectorize` answer "why is this
  slow" faster than any profiler. Read them first.
* Loop *order* changes more than cache behaviour: it changes whether the
  vectoriser is allowed to run at all. ijk's `s += ...` is a loop-carried FP
  dependency, and FP addition's non-associativity forbids splitting it.
* A dependency chain runs at *latency* (~4 cycles/fmadd); independent operations
  run at *throughput* (2–4/cycle). Same instruction, 8× difference.
* `.2d` suffixes, `q` registers, `ldp`/`stp` of vectors, and `fmla` mean the loop
  vectorised. `d0`-only scalar `fmadd` means it did not.
* Without `restrict` the compiler ships two loops and a runtime overlap check.
  `restrict` deletes the check — and makes you responsible for the promise.
* Instructions-per-element is a cost model you can compute from the listing and
  falsify with a timer. Doing both is the point of this chapter.
