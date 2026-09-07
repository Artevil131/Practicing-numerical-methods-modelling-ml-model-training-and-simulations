# 02 — asm.md: branches, loops and switches in the instruction stream

Companion to `lesson.md`. Prerequisite: `00_arm64_assembly_primer/lesson.md`.
Output is `cc -O1 -S -o -` on this machine unless marked `-O2`, with
`.cfi_*`/`.p2align`/`.section` lines removed.

## What to look for

* `if`/`else` becomes `cmp` + `b.cond` — but only when both sides have side
  effects. Pure value selection becomes **`csel`/`cset`/`cneg`** with no branch.
* `switch` with dense cases → a **jump/lookup table** indexed by the value;
  sparse cases → a compare chain (or a chain of `csel`).
* `while`/`for` come out **rotated**: guard at the top, body, test at the bottom.
* `&&` is a sequence of early-exit branches — you can see the short circuit.
* At `-O1` the compiler already replaces some loops with closed-form arithmetic.

## The C

```c
/* pure value selection */
int sign(int x)  { if (x > 0) return 1; else if (x < 0) return -1; else return 0; }
int abs_i(int x) { return x < 0 ? -x : x; }
int min_i(int a, int b) { return a < b ? a : b; }
int grade(int s) { if (s >= 90) return 'A'; if (s >= 80) return 'B'; if (s >= 70) return 'C'; return 'F'; }

/* branches with side effects */
void on_pos(void); void on_neg(void); void on_zero(void);
void classify(int x) { if (x > 0) on_pos(); else if (x < 0) on_neg(); else on_zero(); }

/* short circuit */
int expensive(int);
int guard(int *p, int k) { if (p != 0 && expensive(*p)) return k; return -1; }

/* switch: dense vs sparse */
const char *day(int d) {
    switch (d) { case 0: return "Sun"; case 1: return "Mon"; case 2: return "Tue"; case 3: return "Wed";
                 case 4: return "Thu"; case 5: return "Fri"; case 6: return "Sat"; default: return "?"; }
}
int sparse(int d) { switch (d) { case 1: return 10; case 100: return 20; case 1000: return 30; default: return 0; } }

/* loops */
long find(const long *a, long n, long key) { long i = 0; while (i < n && a[i] != key) i++; return i; }
double poly(const double *c, int n, double x) { double r = 0; for (int i = n-1; i >= 0; i--) r = r*x + c[i]; return r; }
long sum_to(long n) { long s = 0; for (long i = 1; i <= n; i++) s += i; return s; }
int count_digits(unsigned n) { int c = 0; while (n) { n /= 10; c++; } return c; }
```

## The assembly

### if/else that only picks a value → no branch

```
_sign:
	asr	w8, w0, #31              ; w8 = x >> 31 (arith): 0 for x ≥ 0, -1 for x < 0
	cmp	w0, #1                   ; flags = x - 1
	csinc	w0, w8, wzr, lt          ; x < 1 ? w8 : wzr + 1  →  (0 or -1) or 1
	ret

_abs_i:
	cmp	w0, #0
	cneg	w0, w0, mi               ; w0 = N-flag ? -w0 : w0   (Conditional NEGate)
	ret

_min_i:
	cmp	w0, w1
	csel	w0, w0, w1, lt           ; ternary → one conditional select
	ret
```

`sign` has three `return`s in the C and **zero** branches in the asm. The
compiler noticed each path only produces a value, computed the candidates, and
selected. Branches cost mispredictions; `csel` never mispredicts.

### The if-ladder becomes a csel-ladder

```
_grade:
	mov	w8, #65                  ; 'A'
	mov	w9, #66                  ; 'B'
	mov	w10, #67                 ; 'C'
	mov	w11, #70                 ; 'F'
	cmp	w0, #70
	csel	w10, w11, w10, lt        ; w10 = s < 70 ? 'F' : 'C'
	cmp	w0, #79
	csel	w9, w9, w10, gt          ; w9  = s > 79 ? 'B' : w10
	cmp	w0, #89
	csel	w0, w8, w9, gt           ; w0  = s > 89 ? 'A' : w9
	ret
```

Read it bottom-up: the last `csel` decides the first `if`. Note `s >= 80` became
`s > 79` — `gt` against `#79` and `ge` against `#80` are the same test; the
compiler canonicalises.

### if/else with side effects → real branches, and tail calls

```
_classify:
	cmp	w0, #1
	b.lt	LBB0_2                   ; x < 1  → not positive, go test further
	b	_on_pos                  ; TAIL CALL: jump, don't `bl`; on_pos's ret returns to OUR caller
LBB0_2:
	tbnz	w0, #31, LBB0_4          ; Test Bit 31 (sign) Non-Zero → negative
	b	_on_zero                 ; tail call
LBB0_4:
	b	_on_neg                  ; tail call
```

Three calls, no stack frame: because each call is the last thing the function
does, `bl f; ret` collapses to `b f`. Also note `x < 0` was tested with `tbnz`
on the sign bit — no `cmp` needed.

### `&&` short-circuit

```
_guard:
	stp	x20, x19, [sp, #-32]!    ; needs x19 across the call, and x30
	stp	x29, x30, [sp, #16]
	add	x29, sp, #16
	cbz	x0, LBB1_2               ; p == 0 → skip straight to the -1 path: *p is NEVER loaded
	mov	x19, x1                  ; keep k alive across the call
	ldr	w0, [x0]                 ; *p
	bl	_expensive
	cbnz	w0, LBB1_3               ; nonzero → return k
LBB1_2:
	mov	w19, #-1
LBB1_3:
	mov	x0, x19
	ldp	x29, x30, [sp, #16]
	ldp	x20, x19, [sp], #32
	ret
```

The `cbz x0` **is** the short circuit: the right operand's code lies after it
and is only reached when the left is true.

### switch: dense → lookup table

```
_day:
	cmp	w0, #6
	b.hi	LBB2_2                   ; UNSIGNED compare: d > 6 OR d < 0 both land here (one test!)
	adrp	x8, l_switch.table.day@PAGE
	add	x8, x8, l_switch.table.day@PAGEOFF
	ldr	x0, [x8, w0, uxtw #3]    ; x0 = table[d]   (zero-extend d, scale by 8)
	ret
LBB2_2:
	adrp	x0, l_.str.7@PAGE
	add	x0, x0, l_.str.7@PAGEOFF ; "?"
	ret

l_switch.table.day:
	.quad	l_.str                   ; "Sun"
	.quad	l_.str.1                 ; "Mon"
	...
	.quad	l_.str.6                 ; "Sat"
```

Because every case just returns a constant, the compiler built a **table of the
results**, not a table of code addresses. A switch whose cases run different
code produces a table of labels and a `br xN` into it — same idea, one more
indirection. The `b.hi` trick (unsigned compare covers negative inputs as huge
numbers) is worth remembering.

### switch: sparse → compare chain

```
_sparse:
	mov	w8, #10
	mov	w9, #30
	mov	w10, #20
	cmp	w0, #100
	csel	w10, wzr, w10, ne        ; d != 100 ? 0 : 20
	cmp	w0, #1000
	csel	w9, w9, w10, eq          ; d == 1000 ? 30 : ^
	cmp	w0, #1
	csel	w0, w8, w9, eq           ; d == 1 ? 10 : ^
	ret
```

Cases 1/100/1000 are too far apart for a 1000-entry table; three compares are
cheaper. Again value-only, so `csel` rather than branches.

### while → rotated loop

```
_find:
	cmp	x1, #1
	b.lt	LBB2_6                   ; guard: n < 1 → return 0 without entering
	mov	x8, x0                   ; x8 = a
	mov	x0, #0                   ; i = 0  (kept in the return register)
LBB2_2:
	ldr	x9, [x8, x0, lsl #3]     ; a[i]
	cmp	x9, x2
	b.eq	LBB2_5                   ; found → exit with i in x0
	add	x0, x0, #1               ; i++
	cmp	x1, x0
	b.ne	LBB2_2                   ; i != n → loop  (test moved to the BOTTOM)
	mov	x0, x1                   ; fell off: i == n
LBB2_5:
	ret
LBB2_6:
	mov	x0, #0
	ret
```

C tests the condition at the top of every iteration; the machine tests it at the
bottom and jumps back. One guard before the loop preserves the "zero iterations"
case. `-O2` produces identical code here: a data-dependent early exit cannot be
vectorized.

### for → count-down loop with fmadd

```
_poly:
	cmp	w1, #1
	b.lt	LBB3_4                   ; n < 1 → return 0.0
	mov	w8, w1
	add	x8, x8, #1               ; x8 = n + 1  (trip counter)
	add	x9, x0, w1, uxtw #3
	sub	x9, x9, #8               ; x9 = &c[n-1]
	movi.2d	v1, #0000000000000000    ; r = 0.0
LBB3_2:
	ldr	d2, [x9], #-8            ; d2 = *p; p--   (post-index with NEGATIVE step)
	fmadd	d1, d1, d0, d2           ; r = r*x + c[i]  — one fused op per coefficient
	sub	x8, x8, #1
	cmp	x8, #1
	b.gt	LBB3_2
	mov.16b	v0, v1                   ; return r
	ret
```

The index variable `i` no longer exists; a pointer walks backwards. Horner's
rule is a **loop-carried dependency**: each `fmadd` needs the previous result,
so the loop runs at one `fmadd` latency per coefficient regardless of width.

### The loop that vanished

```
_sum_to:                                 ; for (i=1..n) s += i
	subs	x8, x0, #1
	b.lt	LBB4_2                   ; n < 1 → 0
	sub	x9, x0, #2
	mul	x10, x8, x9              ; (n-1)(n-2)  low 64 bits
	umulh	x8, x8, x9               ;             high 64 bits
	extr	x8, x8, x10, #1          ; 128-bit product >> 1
	add	x8, x8, x0, lsl #1       ; + 2n
	sub	x0, x8, #1               ; ... = n(n+1)/2
	ret
LBB4_2:
	mov	x0, #0
	ret
```

Already at `-O1`, LLVM's scalar-evolution pass recognised the arithmetic series
and emitted Gauss's formula (with a 128-bit intermediate so it cannot overflow
where the loop would not). No loop, O(1). Timing "a loop" that the compiler
deleted is a classic benchmarking mistake — always read the asm first.

### `while (n) { n /= 10; c++; }`

```
_count_digits:
	cbz	w0, LBB5_3               ; n == 0 → return 0 (c is already 0 in w0)
	mov	x8, x0
	mov	w0, #0                   ; c = 0
	mov	w9, #52429
	movk	w9, #52428, lsl #16      ; 0xCCCCCCCD ≈ 2^35/10
LBB5_2:
	umull	x10, w8, w9              ; n * magic
	add	w0, w0, #1               ; c++
	cmp	w8, #9                   ; loop test uses the OLD n …
	lsr	x8, x10, #35             ; … while n/10 is computed in parallel
	b.hi	LBB5_2                   ; old n > 9 ⇔ new n != 0
LBB5_3:
	ret
```

The condition `n != 0` after `n /= 10` was rewritten as `old_n > 9`, letting the
`cmp` be issued before the division result exists — a small scheduling win the
compiler does for free.

## Read it yourself

1. Change `min_i` to take `unsigned` arguments. Predict which single letter in
   the `csel` changes. Check.
2. Make the `day` table sparser: renumber the cases as `0, 1, 2, 3, 4, 5, 60`.
   Predict whether the compiler keeps the table (with padding) or switches to
   compares. Then try `0..6` plus `case 1000`. Find the threshold by experiment.
3. In `guard`, swap the operands: `if (expensive(*p) && p != 0)`. Predict what
   happens to the `cbz x0` and where `*p` is loaded relative to it. (This is a
   NULL-dereference bug; confirm the asm now loads before checking.)
4. Add `-O2` to `poly` and to `find`. Predict which one changes. For `poly`,
   why can the compiler not unroll the dependency away? (Compare with what the
   primer's `-O2 sum` did with four accumulators — what property of `+` on
   integers does `fmadd` on doubles lack without `-ffast-math`?)
5. Replace the body of `sum_to` with `s += i * i`. Does `-O1` still find a closed
   form? Read the multiply sequence and identify n(n+1)(2n+1)/6.

## Takeaways

* A branch in C is a branch in asm **only if the two arms differ in effects**.
  Value-only choices become `csel`/`cset`/`cneg`/`csinc` — learn to read them
  bottom-up.
* Dense `switch` → table indexed by the value, guarded by one unsigned compare;
  sparse `switch` → compares.
* Every compiled loop is rotated: `guard; body; test-at-bottom; b.cond back`.
  The C index variable often becomes a walking pointer or disappears.
* `&&`/`||` are visible as early-exit `cbz`/`cbnz` before the right operand's code.
* A call in tail position becomes `b f`. A loop with a closed form becomes
  arithmetic. Check the asm before you measure anything.
