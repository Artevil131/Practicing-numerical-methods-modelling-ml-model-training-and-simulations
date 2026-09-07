# 00 — ARM64 (AArch64) Assembly Primer for C Programmers

You already write C. This chapter teaches you to read what the compiler turns it
into on this machine (Apple Silicon, Apple clang 21, macOS), to understand the
calling convention well enough to trust it, and to write small leaf functions by
hand and call them from C. Every chapter after this one has an `asm.md` that
applies these ideas to that chapter's C.

All assembly in this file was produced on this machine with

```sh
cc -O1 -S -o - file.c     # readable: -O0 is noisy, -O2 is clever
cc -O2 -S -o - file.c     # when we want to see what the optimizer did
```

with the `.cfi_*`, `.p2align`, `.section` bookkeeping lines removed. Labels and
instructions are untouched.

---

## 1. The mental model

A CPU core has a small set of named **registers** (fast storage, ~one word each),
a **program counter** (address of the next instruction), **flags** (results of
the last compare), and **memory** reached through load/store instructions.
AArch64 is a *load/store architecture*: arithmetic works only on registers, so
every C expression that touches a variable in memory becomes

```
load registers  →  compute in registers  →  store back
```

Every instruction is 4 bytes. There are no variable-length encodings, no
"add memory to register" — which is exactly why the compiler output is so
regular and so readable once you know ~40 mnemonics.

---

## 2. Registers

### Integer / general purpose

| Name | Width | Role under the AAPCS64 calling convention |
|---|---|---|
| `x0`–`x7`  | 64-bit | Arguments 1–8 and the return value (`x0`, `x1` for 128-bit) |
| `x8`       | 64-bit | Indirect result location (pointer to where a large struct return goes) |
| `x9`–`x15` | 64-bit | **Caller-saved scratch.** A function may clobber freely |
| `x16`,`x17`| 64-bit | Intra-procedure-call scratch (linker/PLT stubs use them) |
| `x18`      | 64-bit | Platform register — **reserved on Apple platforms, never touch** |
| `x19`–`x28`| 64-bit | **Callee-saved.** If you use one, save it in your prologue and restore it before `ret` |
| `x29`      | 64-bit | Frame pointer (`fp`) — callee-saved |
| `x30`      | 64-bit | Link register (`lr`) — `bl` writes the return address here; `ret` jumps to it |
| `sp`       | 64-bit | Stack pointer. Must be **16-byte aligned** whenever memory is accessed through it |
| `xzr`/`wzr`| — | Zero register: reads as 0, writes are discarded |
| `pc`       | — | Program counter. Not directly addressable; changed by branches |

Every `xN` has a 32-bit view `wN` (its low half). Writing `wN` **zeroes the
upper 32 bits** of `xN`. That is why `int` arithmetic shows up as `w` registers
and `long`/pointers as `x` registers:

```
add  w9, w9, #1      ; int   ++   (32-bit wraparound, upper bits cleared)
add  x9, x9, #1      ; long  ++   / pointer arithmetic
```

### Flags — NZCV

`cmp`, `subs`, `adds`, `tst`, etc. set four flag bits: **N**egative, **Z**ero,
**C**arry, o**V**erflow. Conditional instructions read them. Plain `add`/`sub`
do *not* touch the flags (the `s` suffix is what enables it).

### Floating point / SIMD

`v0`–`v31` are 128-bit vector registers. Each has narrower scalar views:
`d0` (64-bit double), `s0` (32-bit float), `h0` (16-bit). Vector views are
written `v0.2d` (two doubles), `v0.4s` (four floats), `v0.16b` (sixteen bytes).

| Name | Role |
|---|---|
| `v0`–`v7`  | FP/vector arguments and return (`d0` returns a `double`) |
| `v8`–`v15` | Callee-saved (only the low 64 bits — `d8`–`d15`) |
| `v16`–`v31`| Caller-saved scratch |

---

## 3. The calling convention (AAPCS64) in one page

1. **Integer args** go in `x0…x7` in order; **FP args** in `d0…d7` in order —
   the two sequences are independent (`f(int, double, int)` → `w0, d0, w1`).
2. Args beyond those spill to the **stack** at `[sp]` upward at the call site.
   (Apple's ABI packs small stack args tightly; the standard ABI pads each to 8.)
3. **Return value**: `x0` (or `x0:x1`), `d0`, or — for structs > 16 bytes —
   the caller passes a hidden pointer in `x8` and the callee writes through it.
4. Small structs (≤ 16 bytes) travel **in registers**, split over `x0/x1`.
5. **Caller-saved** (`x0–x17`, `v0–v7`, `v16–v31`): assume trashed after any `bl`.
   **Callee-saved** (`x19–x28`, `x29`, `x30`, `d8–d15`): a function must return
   them exactly as it found them.
6. `sp` is 16-byte aligned at every call boundary. There is **no red zone** on
   Apple arm64 in the sense x86-64 has; leaf functions simply avoid the stack.
7. `bl target` = `lr ← pc + 4; pc ← target`. `ret` = `pc ← lr`. A function that
   itself calls something must **save `x30`** or it forgets how to return.

---

## 4. Instruction families (one line each)

### Moves and immediates

```
mov   x0, x1              ; register copy
mov   w0, #1              ; small immediate
movz  x0, #0x1234, lsl #16; put 0x1234 in bits 16..31, zero the rest
movk  x0, #0x5678         ; keep the rest, overwrite bits 0..15 → 0x12345678
mov   x8, #0              ; (assembler picks movz/orr for you)
```

### Arithmetic

```
add   x0, x1, x2          ; x0 = x1 + x2
add   x0, x1, #16         ; immediate 0..4095 (or shifted by 12)
add   x0, x1, x2, lsl #3  ; x0 = x1 + (x2 << 3)  — array indexing in one op
sub   x0, x0, #1
subs  x1, x1, #1          ; also set flags (Z when result hits 0)
neg   x0, x0
mul   x0, x1, x2
madd  x0, x1, x2, x3      ; x0 = x1*x2 + x3
msub  x0, x1, x2, x3      ; x0 = x3 - x1*x2   (remainder: n - q*d)
sdiv  x0, x1, x2          ; signed divide, truncates toward zero
udiv  x0, x1, x2
```

### Bitwise and shifts

```
and   x0, x1, #0xff
orr   x0, x1, x2
eor   x0, x1, x2          ; xor
mvn   x0, x1              ; not
lsl   x0, x1, #3          ; logical shift left
lsr   x0, x1, #3          ; logical shift right (fills 0 — unsigned /8)
asr   x0, x1, #3          ; arithmetic shift right (fills sign — signed /8, floors)
tst   x0, #1              ; and, discard result, set flags (Z ⇔ bit clear)
ubfx  x0, x1, #4, #8      ; extract 8 bits starting at bit 4, zero-extend
bfi   x0, x1, #4, #8      ; insert low 8 bits of x1 at bit 4 of x0
clz   x0, x1              ; count leading zeros
rbit  x0, x1              ; reverse bit order
```

### Loads and stores — addressing modes

```
ldr   x0, [x1]            ; x0 = *(long*)x1
ldr   w0, [x1, #4]        ; base + immediate (scaled by access size, so #4 is fine)
ldr   x0, [x1, x2, lsl #3]; base + index*8       → a[i] for long a[]
ldr   x0, [x1], #8        ; POST-index: load from x1, then x1 += 8   → *p++
ldr   x0, [x1, #8]!       ; PRE-index:  x1 += 8, then load            → *++p
ldrb  w0, [x1]            ; load byte, zero-extend  (unsigned char)
ldrsb w0, [x1]            ; load byte, sign-extend  (signed char)
ldrh / ldrsh / ldrsw      ; 16-bit, and 32→64 sign-extending
str   w0, [x1, #12]       ; store 32 bits
strb  w0, [x1]            ; store the low byte
ldp   x29, x30, [sp, #16] ; load a PAIR into two registers
stp   x29, x30, [sp, #-16]!; store a pair with pre-index — the classic prologue
ldr   d0, [x0, #8]        ; FP registers use the same modes
```

### Compare and conditional

```
cmp   x0, x1              ; flags from x0 - x1
cmn   x0, #1              ; flags from x0 + 1
b.eq  L / b.ne / b.lt / b.le / b.gt / b.ge      ; SIGNED conditions
b.lo  / b.ls / b.hi / b.hs                      ; UNSIGNED (lower/higher)
cbz   x0, L               ; branch if x0 == 0   (no cmp needed)
cbnz  x0, L
tbz   x0, #3, L           ; branch if bit 3 is zero
tbnz  x0, #63, L          ; branch if negative (sign bit set)
cset  w0, gt              ; w0 = (flags say gt) ? 1 : 0       → `a > b` as a value
csel  x0, x1, x2, gt      ; x0 = gt ? x1 : x2                 → ternary
csinc x0, x1, x2, cc      ; x0 = cond ? x1 : x2+1
cinc  x0, x0, eq          ; x0 = eq ? x0+1 : x0
```

### Control flow

```
b     L                   ; unconditional jump
bl    _f                  ; call: lr = return address, jump
blr   x8                  ; call through a function pointer
ret                       ; jump to lr
br    x8                  ; jump through a register (switch jump tables, tail calls)
```

### Globals — `adrp` + offset (macOS)

A 4-byte instruction cannot hold a 64-bit address. Two instructions build one:

```
adrp  x8, _counter@PAGE           ; x8 = 4 KiB page containing _counter
ldr   w9, [x8, _counter@PAGEOFF]  ; offset within the page
```

When the symbol may live in another library the compiler goes via the GOT:

```
adrp  x8, _counter@GOTPAGE
ldr   x8, [x8, _counter@GOTPAGEOFF]   ; x8 = &counter (from the Global Offset Table)
ldr   w9, [x8]                        ; w9 = counter
```

Note the **leading underscore**: C's `counter` is the assembly symbol `_counter`
on macOS. (Linux uses no underscore and `:lo12:` instead of `@PAGEOFF`.)

### Floating point

```
fadd  d0, d1, d2 / fsub / fmul / fdiv
fmadd d0, d1, d2, d3      ; d0 = d1*d2 + d3, single rounding
fneg / fabs / fsqrt
fcmp  d0, d1              ; sets NZCV; unordered (NaN) sets C and V
fcsel d0, d1, d2, gt
scvtf d0, w0              ; signed int → double
ucvtf d0, x0              ; unsigned → double
fcvtzs w0, d0             ; double → int, truncate toward zero (C's cast)
fcvt  s0, d0              ; double → float
fmov  d0, xzr             ; d0 = 0.0 (bit pattern of 0)
fmov  x0, d0              ; move bits between register files
```

### A taste of NEON (SIMD)

```
ld1   {v0.2d}, [x0], #16      ; load two doubles, advance pointer
fadd  v0.2d, v0.2d, v1.2d     ; two double adds in one instruction
fmla  v0.2d, v1.2d, v2.2d     ; v0 += v1 * v2, lane-wise
faddp d0, v0.2d               ; horizontal: d0 = v0[0] + v0[1]
movi.2d v0, #0                ; zero a vector (Apple's syntax puts .2d on the mnemonic)
```

Apple's assembler prints vector arrangement on the **mnemonic** (`add.2d v0, v4, v0`)
where the standard syntax puts it on the registers (`add v0.2d, v4.2d, v0.2d`).
Both are accepted as input.

---

## 5. Reading real compiler output

The source (`p.c`):

```c
long add3(long a, long b, long c) { return a + b + c; }
long sum(const long *a, size_t n) { long s = 0; for (size_t i = 0; i < n; i++) s += a[i]; return s; }
long max2(long a, long b) { return a > b ? a : b; }
int counter;
void bump(void) { counter++; }
long g(long);
long caller(long x) { long t = g(x); return t + x; }
double axpy(double a, double x, double y) { return a * x + y; }
double tod(int i) { return i * 0.5; }
long fact(long n) { return n <= 1 ? 1 : n * fact(n - 1); }
```

`cc -O1 -S -o - p.c`, trimmed:

```
_add3:
	add	x8, x1, x0          ; x8 = b + a          (scratch x8)
	add	x0, x8, x2          ; x0 = x8 + c         → return value in x0
	ret

_sum:
	mov	x8, #0              ; s = 0
	cbz	x1, LBB1_2          ; n == 0 → skip loop entirely
LBB1_1:
	ldr	x9, [x0], #8        ; x9 = *a; a += 8      (i is gone: the pointer walks)
	add	x8, x9, x8          ; s += x9
	subs	x1, x1, #1          ; n-- and set Z
	b.ne	LBB1_1              ; loop until n == 0
LBB1_2:
	mov	x0, x8              ; return s
	ret

_max2:
	cmp	x0, x1              ; flags = a - b
	csel	x0, x0, x1, gt      ; x0 = (a > b) ? a : b   — no branch at all
	ret

_bump:
	adrp	x8, _counter@GOTPAGE
	ldr	x8, [x8, _counter@GOTPAGEOFF]   ; x8 = &counter
	ldr	w9, [x8]            ; load  (w: int is 32-bit)
	add	w9, w9, #1          ; modify
	str	w9, [x8]            ; store — three separate steps; NOT atomic
	ret

_caller:
	stp	x20, x19, [sp, #-32]!   ; push x19,x20 (need x19 to survive the call)
	stp	x29, x30, [sp, #16]     ; save frame pointer and return address
	add	x29, sp, #16            ; x29 = new frame pointer
	mov	x19, x0                 ; keep x (x0 will be clobbered by g)
	bl	_g                      ; call g(x); result in x0
	add	x0, x0, x19             ; t + x
	ldp	x29, x30, [sp, #16]     ; restore fp, lr
	ldp	x20, x19, [sp], #32     ; pop x19,x20 and release the 32 bytes
	ret

_axpy:
	fmadd	d0, d0, d1, d2      ; a*x + y fused — args a=d0 x=d1 y=d2, return d0
	ret

_tod:
	scvtf	d0, w0, #1          ; fixed-point convert: i * 2^-1 == i * 0.5 in ONE instruction
	ret

_fact:
	mov	x8, x0              ; n
	mov	w0, #1              ; result = 1
	cmp	x8, #2
	b.lt	LBB7_2              ; n < 2 → return 1
LBB7_1:
	mul	x0, x0, x8          ; result *= n
	sub	x8, x8, #1          ; n--
	cmp	x8, #2
	b.ge	LBB7_1
LBB7_2:
	ret
```

Things to notice:

* `add3`, `max2`, `axpy`, `tod` are **leaf functions**: no stack, no `x29/x30`
  traffic. Arguments arrive in registers, the answer leaves in `x0`/`d0`.
* `caller` is **not** a leaf. Because it calls `g`, it must (a) save `x30` or it
  cannot return, and (b) keep `x` alive across the call — so it parks `x` in
  the callee-saved `x19`, and therefore must save `x19` too. The prologue pushes
  32 bytes (keeps `sp` 16-aligned; `x20` rides along to fill the pair).
* `bump` shows the load/modify/store triple. Two threads doing this concurrently
  lose increments — chapter 16 shows the atomic replacement.
* `fact` at `-O1` is **not recursive any more**: the compiler turned the tail
  recursion into a loop. No stack depth at all.
* The loop in `sum` is **rotated**: the test is at the bottom (`subs; b.ne`),
  with a single guard `cbz` before entry. This is the shape of nearly every
  compiled loop.

### The same code at `-O2`

`-O2` gets clever. `sum` becomes ~35 lines: a `cmp x1, #8 / b.hs` decides
between the scalar loop above and a **vectorized** body that loads 8 longs per
iteration into four `q` registers and adds them with `add.2d`, followed by a
horizontal reduction (`addp.2d d0, v0`) and a scalar tail loop for the leftover
`n % 8`. `fact` becomes an **unrolled-by-4** loop with four independent
accumulators (`x11, x13, x14, x15`) multiplied together at the end. Recognise
these patterns rather than read them line by line:

| You see | It means |
|---|---|
| a second copy of the loop body with `.2d`/`.4s` registers | auto-vectorization |
| `cmp xN, #8; b.hs` before a loop | "enough iterations for the vector path?" |
| `and x9, x1, #0xff…f8` | round the trip count down to a multiple of 8 |
| the loop body repeated 2–4× with different registers | unrolling |
| several accumulators combined after the loop | breaking a dependency chain |
| the function body is missing and appears inside the caller | inlining |
| `b _f` at the end instead of `bl _f; ret` | tail call |
| `add x0, x0, x1, lsl #3` instead of `mul`+`add` | strength reduction |

---

## 6. Stack frames

A non-leaf function's frame, top of stack at the bottom of the picture:

```
higher addresses
        ┌───────────────────────┐
        │ caller's frame        │
        ├───────────────────────┤  ← sp on entry (16-aligned)
        │ saved x19, x20 …      │  callee-saved registers this function uses
        ├───────────────────────┤
        │ saved x29 (old fp)    │  ← x29 points here after `add x29, sp, #N`
        │ saved x30 (return)    │
        ├───────────────────────┤
        │ locals / spills       │  addressed as [sp, #off] or [x29, #-off]
        └───────────────────────┘  ← sp during the body (16-aligned)
lower addresses
```

Canonical prologue / epilogue produced by clang:

```
	stp	x29, x30, [sp, #-16]!   ; push fp, lr; sp -= 16
	mov	x29, sp                 ; establish frame pointer
	sub	sp, sp, #32             ; room for locals (multiple of 16)
	...
	add	sp, sp, #32
	ldp	x29, x30, [sp], #16     ; pop; sp += 16
	ret
```

Locals that live in registers do not appear in the frame at all — with
optimisation on, most do not. A local shows up on the stack when its address is
taken (`&x`), when it is an array, or when there are more live values than
registers (a **spill**).

---

## 7. The macOS toolchain

```sh
cc -S -O1 -o f.s f.c        # C → assembly text
cc -c f.s                   # assembly → object (f.o)
cc -c f.c                   # C → object directly
cc -o prog main.o f.o       # link
nm f.o                      # symbols: T = defined text, U = undefined
otool -tv prog              # disassemble the text section
otool -L prog               # which dylibs it links against
objdump -d prog             # LLVM objdump also works
```

Minimal skeleton of a hand-written file:

```
	.text
	.globl	_myfunc         ; export the symbol so the linker can see it
	.p2align	2               ; align to 4 bytes (instructions must be)
_myfunc:
	...
	ret
```

Local numeric labels `0:`/`1:` with `b 0b` (backward) / `b 1f` (forward) are
handy in hand-written code; the compiler prefers `LBB<n>_<m>` (the leading `L`
means "do not export").

### Stepping assembly in lldb

```sh
cc -g -O1 -o prog main.c f.s
lldb ./prog
(lldb) b _sum_array               # break on a symbol
(lldb) r
(lldb) disassemble                # current function
(lldb) register read x0 x1 x2     # or `register read` for all
(lldb) si                         # step ONE instruction
(lldb) ni                         # step over a bl
(lldb) memory read -fx -c4 $x0    # 4 hex words at the address in x0
(lldb) register read/d d0         # FP register as decimal
```

### Compiler Explorer

godbolt.org with compiler "ARM64 clang" and flags `-O1` produces the same
shape of output as this machine, colour-linked to the C source line. Use it
as a reader; use `cc -S` here when you need exactly what *this* toolchain does.

---

## 8. Writing a leaf function by hand — the checklist

1. Inputs are in `x0…x7` / `d0…d7` in declaration order. Write the prototype
   in a comment at the top so you never forget which is which.
2. Use only `x0–x17` and `v0–v7, v16–v31` for temporaries → nothing to save.
3. Do not touch `sp`, `x18`, `x29`, `x30`.
4. Result in `x0` (or `d0`), then `ret`.
5. Pick `w` for `int`/`unsigned`/`char`, `x` for `long`/`size_t`/pointers.
6. Loops: test the zero case with `cbz` first, then a rotated body ending in
   `subs …; b.ne`.
7. Declare the prototype in C and link: `cc main.c f.s`. If the linker says
   `_f` undefined you forgot `.globl` or the underscore.

`example.s` in this directory does exactly this for `add3`, `sum_array`,
`dot_product` (with `fmadd`) and `my_strlen` (with `ldrb` post-index);
`main.c` asserts all four:

```sh
cc -Wall -Wextra -std=c11 -O2 -o ex_demo main.c example.s && ./ex_demo
```

Verified output on this machine:

```
add3(1,2,3)        = 6
sum_array(1..10)   = 55
dot_product        = 32.0
my_strlen("hello, asm") = 10
all assertions passed
```

`otool -tv ex_demo` shows the assembler kept `_add3` as exactly the three
instructions we wrote.

---

## 9. Quick reference — condition codes

| Suffix | Meaning (after `cmp a, b`) | Signedness |
|---|---|---|
| `eq` / `ne` | a == b / a != b | either |
| `lt` `le` `gt` `ge` | <  ≤  >  ≥ | **signed** |
| `lo` `ls` `hi` `hs` | <  ≤  >  ≥ | **unsigned** (`lo`=`cc`, `hs`=`cs`) |
| `mi` / `pl` | result negative / non-negative | flag N |
| `vs` / `vc` | overflow / no overflow | flag V |

Seeing `b.lo` where you expected `b.lt` tells you a `size_t` or `unsigned` was
compared. Seeing `asr` vs `lsr` tells you a division by a power of two was signed
vs unsigned.

---

## 10. Where next

Each chapter's `asm.md` starts with "What to look for", shows the chapter's C,
the trimmed real output, and 3–5 "change the C, predict the asm, check" tasks.
Keep this file open beside them; the instruction tables in §4 are the lookup.
