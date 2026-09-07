# 03 — asm.md: the calling convention in practice

Companion to `lesson.md`. Prerequisite: `00_arm64_assembly_primer/lesson.md` §3 and §6.
Output is `cc -O1 -S -o -` on this machine unless marked otherwise; noise lines removed.

## What to look for

* A 3-argument leaf: arguments arrive in `x0, x1, x2`, result leaves in `x0`,
  nothing else happens.
* A function that calls another must save `x30` (the return address) and park
  any value it needs afterwards in a **callee-saved** register (`x19…`), which
  it must then save too. That is the whole prologue/epilogue story.
* Recursion: at `-O0` the recursion depth lives in stack frames of 48 bytes
  each; at `-O1` tail recursion becomes a loop and only *non-tail* recursion
  (`fib`) still calls itself.
* `static` locals are not on the stack — they are a labelled word in the data
  section, addressed with `adrp`/`@PAGEOFF`.
* A local array forces a real stack frame, plus a **stack canary**.
* A `static` helper is inlined and its symbol disappears.

## The C

```c
long lerp3(long a, long b, long t) { return a + (b - a) * t; }
long fact(long n) { return n <= 1 ? 1 : n * fact(n - 1); }
long fib(long n)  { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
long g(long);
long two_calls(long x) { long a = g(x); long b = g(x + 1); return a * b; }
int next_id(void) { static int id = 100; return id++; }
static long sq(long x) { return x * x; }
long sumsq(long a, long b) { return sq(a) + sq(b); }
void fill(long *out, size_t n);
long with_local_array(void) { long buf[4]; fill(buf, 4); return buf[0] + buf[3]; }
```

## The assembly

### Three arguments, one result, no frame

```
_lerp3:
	sub	x8, x1, x0               ; x8 = b - a         (a=x0, b=x1, t=x2 — declaration order)
	madd	x0, x8, x2, x0           ; x0 = x8 * t + a    (multiply-add in one instruction)
	ret
```

A **leaf** function: it calls nothing, so `x30` still holds the caller's return
address and nothing needs saving. Scratch work happens in `x8` (caller-saved,
free to clobber). Two instructions of work, one `ret`.

### Calling out twice — what must survive a call

```
_two_calls:
	stp	x20, x19, [sp, #-32]!    ; PROLOGUE: push x19, x20 (sp -= 32, stays 16-aligned)
	stp	x29, x30, [sp, #16]      ;   save frame pointer and RETURN ADDRESS
	add	x29, sp, #16             ;   x29 = this frame
	mov	x19, x0                  ; x is needed after the first call → park it in callee-saved x19
	bl	_g                       ; a = g(x)           — clobbers x0-x17, NOT x19/x20
	mov	x20, x0                  ; a must survive the second call → x20
	add	x0, x19, #1              ; argument x + 1
	bl	_g                       ; b = g(x+1)
	mul	x0, x0, x20              ; a * b
	ldp	x29, x30, [sp, #16]      ; EPILOGUE: restore fp, lr
	ldp	x20, x19, [sp], #32      ;   restore x19, x20, sp += 32
	ret                          ;   jump to the restored x30
```

The contract in one sentence: **anything in `x0–x17` may be garbage after `bl`;
anything in `x19–x28` is exactly as you left it — but then *you* must give
those back to *your* caller unchanged**, hence the `stp`/`ldp` pair. The frame
is 32 bytes because two register pairs are saved; no locals live in memory.

### Recursion at -O0: the depth is on the stack

```
_fact:                                   ; cc -O0
	sub	sp, sp, #48              ; every call: 48 fresh bytes
	stp	x29, x30, [sp, #32]      ; save fp, lr
	add	x29, sp, #32
	stur	x0, [x29, #-8]           ; n → memory  (-O0 keeps every variable in the frame)
	ldur	x8, [x29, #-8]
	subs	x8, x8, #1
	b.gt	LBB1_2                   ; n > 1 → recurse
	b	LBB1_1
LBB1_1:
	mov	x8, #1
	str	x8, [sp, #16]            ; result = 1
	b	LBB1_3
LBB1_2:
	ldur	x8, [x29, #-8]
	str	x8, [sp, #8]             ; spill n (needed after the call)
	ldur	x8, [x29, #-8]
	subs	x0, x8, #1
	bl	_fact                    ; fact(n-1) — pushes ANOTHER 48-byte frame
	ldr	x8, [sp, #8]             ; reload n
	mul	x8, x8, x0
	str	x8, [sp, #16]
	b	LBB1_3
LBB1_3:
	ldr	x0, [sp, #16]
	ldp	x29, x30, [sp, #32]
	add	sp, sp, #48
	ret
```

`fact(20)` at `-O0` = 20 nested frames × 48 bytes ≈ 1 KB of stack. Each frame
holds `n`, a spill copy of `n`, the result slot, and the saved `x29`/`x30`. The
saved `x30` chain is what a debugger walks to print a backtrace. Recurse a
million deep and this is the memory that overflows.

### Recursion at -O1: gone

```
_fact:                                   ; cc -O1
	mov	x8, x0
	mov	w0, #1                   ; result = 1
	cmp	x8, #2
	b.lt	LBB1_2
LBB1_1:
	mul	x0, x0, x8               ; result *= n
	sub	x8, x8, #1               ; n--
	cmp	x8, #2
	b.ge	LBB1_1
LBB1_2:
	ret
```

`n * fact(n-1)` is not literally a tail call, but multiplication is
associative, so LLVM turned it into an accumulator loop. No `bl`, no frame,
O(1) stack. **Do not** conclude "recursion is free" from this: it depends on the
compiler recognising the pattern.

### Recursion that must stay recursive: fib

```
_fib:
	stp	x20, x19, [sp, #-32]!    ; frame: x19 (running sum), x20 (n), fp, lr
	stp	x29, x30, [sp, #16]
	add	x29, sp, #16
	mov	x19, #0                  ; acc = 0
	subs	x20, x0, #2              ; x20 = n - 2, flags
	b.lt	LBB2_2                   ; n < 2 → return n + acc
LBB2_1:
	sub	x0, x0, #1
	bl	_fib                     ; acc += fib(n-1)      ← the ONE remaining real call
	add	x19, x19, x0
	mov	x0, x20                  ; n = n - 2
	subs	x20, x0, #2
	b.ge	LBB2_1                   ; the fib(n-2) call became this loop iteration
LBB2_2:
	add	x0, x0, x19
	ldp	x29, x30, [sp, #16]
	ldp	x20, x19, [sp], #32
	ret
```

Two recursive calls in the C; one `bl _fib` in the asm. The second call was in
tail position (after reassociation), so it became a loop. The first cannot be
eliminated — its result is needed mid-expression — so `fib` still needs a
frame, and the depth still grows with `n`. The frame is only 32 bytes: `n` and
the accumulator live in `x20`/`x19`, not in memory.

### `static` local: lives in .data, not on the stack

```
_next_id:
	adrp	x8, _next_id.id@PAGE
	ldr	w0, [x8, _next_id.id@PAGEOFF]    ; w0 = id  (return the OLD value)
	add	w9, w0, #1
	str	w9, [x8, _next_id.id@PAGEOFF]    ; id = id + 1
	ret

_next_id.id:                                 ; in the __DATA section
	.long	100                              ; initialised ONCE, at load time, by the file format
```

The symbol is `_next_id.id` — function name dot variable name — so two functions
each with `static int id` do not collide. The `= 100` costs no instructions: the
initial value is baked into the executable's data. Compare with an ordinary local,
which would be a register or `[sp, #off]` and reinitialised every call.

### Inlining a static helper

```
_sumsq:
	mul	x8, x0, x0               ; sq(a) — inlined
	madd	x0, x1, x1, x8           ; sq(b) + x8 — inlined and fused
	ret
```

There is **no `_sq` symbol anywhere in the output**. It was `static`, every
caller is in this file, so the compiler inlined both calls and dropped the body.
Remove `static` and `_sq` reappears as an exported function (the calls in
`sumsq` still inline — `-O2` inlines non-static small functions too, but must
keep a copy for other files).

### A local array forces a frame — and a canary

```
_with_local_array:
	sub	sp, sp, #64              ; 32 bytes buf + 8 canary + 16 fp/lr, rounded to 64
	stp	x29, x30, [sp, #48]
	add	x29, sp, #48
	adrp	x8, ___stack_chk_guard@GOTPAGE
	ldr	x8, [x8, ___stack_chk_guard@GOTPAGEOFF]
	ldr	x8, [x8]                 ; x8 = random canary value
	stur	x8, [x29, #-8]           ; store it just ABOVE buf
	add	x0, sp, #8               ; &buf[0]  = sp + 8
	mov	w1, #4                   ; n = 4
	bl	_fill
	ldr	x8, [sp, #8]             ; buf[0]
	ldr	x9, [sp, #32]            ; buf[3]  = sp + 8 + 3*8
	ldur	x10, [x29, #-8]          ; reload the stored canary
	adrp	x11, ___stack_chk_guard@GOTPAGE
	ldr	x11, [x11, ___stack_chk_guard@GOTPAGEOFF]
	ldr	x11, [x11]
	cmp	x11, x10
	b.ne	LBB6_2                   ; overwritten? → abort
	add	x0, x9, x8
	ldp	x29, x30, [sp, #48]
	add	sp, sp, #64
	ret
LBB6_2:
	bl	___stack_chk_fail
```

`buf` needs an address (it is passed to `fill`), so it must be in memory: this is
what "a local lives on the stack" actually looks like. Because a callee writes
into it, clang (by default on macOS) plants a **canary** between `buf` and the
saved `x29/x30`; if `fill` wrote 5 elements instead of 4 the canary changes and
the program aborts instead of returning to a corrupted `x30`.

## Read it yourself

1. Give `lerp3` a fourth argument `long d` and add it to the result. Predict
   which register it arrives in. Then give it 9 arguments: where does the 9th
   come from? (Look for a `ldr … [sp]` on entry.)
2. In `two_calls`, remove the `* b` and return `a + g(x+1)` instead, then make
   the *only* thing after the second call be `return g(x + 1);`. Predict when the
   second `bl _g` becomes `b _g` and the frame disappears.
3. Change `fact` to `return n <= 1 ? 1 : fact(n - 1) * n;` and then to use a
   subtraction: `fact(n-1) - n`. Which one does `-O1` still turn into a loop?
   Why does associativity matter?
4. Compile `fib` with `-O0` and count the bytes per frame. Estimate the stack
   used by `fib(40)` (depth 40) at `-O0` and at `-O1`. Verify with lldb:
   break inside the deepest call and `register read sp` at two depths.
5. Change `static int id = 100;` to `int id = 100;` (plain local). Predict the
   new body of `next_id` (hint: `mov w0, #100; ret`) and explain why the
   increment vanished.

## Takeaways

* Args in `x0…x7` in declaration order; result in `x0`. A leaf function has no
  prologue at all.
* `bl` clobbers `x0–x17`. To keep something across a call, put it in `x19–x28`
  — and then save/restore that register with `stp`/`ldp`. Always save `x30`.
* Recursion depth is stack frames of saved `x29/x30` plus spills; the compiler
  removes tail recursion (and some near-tail recursion) entirely at `-O1`.
* `static` locals are data-section words with a `func.var` symbol; their
  initialiser is free.
* `static` functions inline and vanish; arrays and `&x` force real stack slots,
  and a stack canary guards the return address.
