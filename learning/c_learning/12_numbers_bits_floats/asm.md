# 12 — asm.md: bits are the machine's native type; floats live in another register file

Companion to `lesson.md`. Prerequisite: `00_arm64_assembly_primer/lesson.md` §2–3.
Output is `cc -O1 -S -o -` on this machine; noise lines removed.

## What to look for

* `&`, `|`, `^`, `~` are one instruction each — `and`, `orr`, `eor`, `mvn` — and
  ARM64 throws in `bic` (and-not) for free.
* The compiler recognises bit *idioms*: `(x>>8)&0xFF` becomes `ubfx`,
  `(x & ~mask) | (f<<8)` becomes `bfi`. Write the idiom, get the instruction.
* `>>` on a signed value is `asr`, on unsigned is `lsr`. **This is why signedness
  is not cosmetic.** And `x / 16` on a signed int is *not* `asr #4` — it needs a
  bias correction, so unsigned division by a power of two is genuinely cheaper.
* Comparisons: `b.lt`/`cset lt` is **signed**, `b.lo`/`cset lo` is **unsigned**.
  Same `cmp`, different condition code. A wrong `int` vs `size_t` is a wrong
  branch instruction.
* Floats live in `d`/`s` registers, a separate file. `fmov` moves *bits*;
  `scvtf`/`fcvtzs` *convert values*. `int` ↔ `double` costs an instruction.
* `fcmp` sets a fourth flag state for NaN: **unordered**. `a != a` is `cset vs`.

## The C

```c
#include <stdint.h>
uint32_t b_and(uint32_t a, uint32_t b) { return a & b; }
uint32_t b_or (uint32_t a, uint32_t b) { return a | b; }
uint32_t b_xor(uint32_t a, uint32_t b) { return a ^ b; }
uint32_t b_not(uint32_t a)             { return ~a; }
uint32_t b_andn(uint32_t a, uint32_t b){ return a & ~b; }
uint32_t set_bit(uint32_t x,int n) { return x |  (1u<<n); }
uint32_t clr_bit(uint32_t x,int n) { return x & ~(1u<<n); }
int      tst_bit(uint32_t x,int n) { return (x >> n) & 1u; }
uint32_t mask_low(uint32_t x)      { return x & 0xFFu; }
uint32_t extract(uint32_t x)       { return (x >> 8) & 0xFFu; }
uint32_t insert (uint32_t x,uint32_t f){ return (x & ~(0xFFu<<8)) | ((f&0xFFu)<<8); }

int32_t  sar(int32_t x)    { return x >> 4; }
uint32_t shr(uint32_t x)   { return x >> 4; }
int32_t  sdiv16(int32_t x) { return x / 16; }
uint32_t udiv16(uint32_t x){ return x / 16; }
int32_t  smod16(int32_t x) { return x % 16; }

int lt_s(int32_t a, int32_t b)  { return a < b; }
int lt_u(uint32_t a, uint32_t b){ return a < b; }
int      is_pow2(uint32_t x) { return x && !(x & (x-1)); }
uint32_t lowest (uint32_t x) { return x & -x; }
int      popcnt(uint64_t x)  { return __builtin_popcountll(x); }
int      lead0 (uint32_t x)  { return __builtin_clz(x); }
int      trail0(uint32_t x)  { return __builtin_ctz(x); }
uint32_t rev   (uint32_t x)  { return __builtin_bitreverse32(x); }
uint32_t bswap (uint32_t x)  { return __builtin_bswap32(x); }

double f_add(double a,double b)          { return a+b; }
double f_fma(double a,double b,double c) { return a*b+c; }
double f_div(double a,double b)          { return a/b; }
float  g_add(float a,float b)            { return a+b; }
double  i2d(int64_t i) { return (double)i; }
double  u2d(uint64_t u){ return (double)u; }
int64_t d2i(double d)  { return (int64_t)d; }
float   d2f(double d)  { return (float)d; }
int f_lt(double a,double b) { return a<b; }
int f_isnan(double a)       { return a != a; }
uint64_t d_bits(double d)   { union { double d; uint64_t u; } v; v.d = d; return v.u; }
double   d_abs(double d)    { return __builtin_fabs(d); }

uint64_t mulmod(uint64_t a, uint64_t b, uint64_t m){ return (unsigned __int128)a*b % m; }
uint64_t mul_hi(uint64_t a, uint64_t b){ return (uint64_t)(((unsigned __int128)a*b)>>64); }
```

## The assembly

### The four operators, and a fifth you get free

```
_b_and:	and	w0, w1, w0
_b_or:	orr	w0, w1, w0
_b_xor:	eor	w0, w1, w0               ; "exclusive or", ARM's name for ^
_b_not:	mvn	w0, w0                   ; "move not" — ~x is really `x EOR -1`
_b_andn:bic	w0, w0, w1               ; BIt Clear:  a & ~b  in ONE instruction
```

`bic` matters: the C idiom `x & ~mask` looks like two operations and is one. ARM
also has `orn` (or-not) and `eon`. All of these are single-cycle, 4-per-cycle
throughput — bit manipulation is the cheapest thing the CPU does.

### The four mask idioms

```
_set_bit:                                ; x |  (1u<<n)
	mov	w8, #1
	lsl	w8, w8, w1               ; variable shift: 1 << n
	orr	w0, w8, w0
_clr_bit:                                ; x & ~(1u<<n)
	mov	w8, #1
	lsl	w8, w8, w1
	bic	w0, w0, w8               ; ~ and & fused again
_tst_bit:                                ; (x >> n) & 1
	lsr	w8, w0, w1
	and	w0, w8, #0x1
_mask_low:                               ; x & 0xFF
	and	w0, w0, #0xff            ; the mask is an IMMEDIATE — no mov needed
```

`and w0, w0, #0xff` works because ARM64 encodes a clever family of "bitmask
immediates" (any rotated run of 1s) directly in the instruction. `0xFF`,
`0xFF00`, `0x5555…` all encode; `0x12345678` does not and would need a `mov`
pair first. Round masks are free; arbitrary constants are not.

### The compiler knows bitfield idioms

```
_extract:                                ; (x >> 8) & 0xFF
	ubfx	w0, w0, #8, #8           ; Unsigned BitField eXtract: 8 bits starting at bit 8
	ret
_insert:                                ; (x & ~(0xFF<<8)) | ((f & 0xFF) << 8)
	bfi	w0, w1, #8, #8           ; BitField Insert: put f's low 8 bits at bit 8
	ret
```

Four C operations collapse to one instruction. You do not need intrinsics for
this — write the shift-and-mask idiom plainly and the pattern matcher finds it.
(`sbfx` is the signed variant, used for sign-extending a packed field.)

### `>>` : `asr` vs `lsr` is the whole meaning of signedness

```
_sar:	asr	w0, w0, #4               ; int32_t  x >> 4  — Arithmetic Shift Right: copies the sign bit in
_shr:	lsr	w0, w0, #4               ; uint32_t x >> 4  — Logical Shift Right: shifts ZEROS in
```

`-16 >> 4` is `-1` with `asr` and `268435455` with `lsr`. The *only* thing that
selects between them is the declared type. This is why "just use `int`" is not
free advice, and why shifting a signed negative was implementation-defined in
older standards.

Now the trap, and it is a good one:

```
_udiv16:                                 ; uint32_t x / 16
	lsr	w0, w0, #4               ; one instruction
	ret
_sdiv16:                                 ; int32_t x / 16
	add	w8, w0, #15              ; bias: x + (16-1)
	cmp	w0, #0
	csel	w8, w8, w0, lt           ; use the biased value only when x < 0
	asr	w0, w8, #4               ; then shift
	ret
```

`x / 16` for signed `x` is **not** `x >> 4`. C division truncates toward zero
(`-1/16 == 0`) but `asr` floors (`-1 >> 4 == -1`). The compiler adds 15 first for
negatives to turn floor into truncate. Four instructions instead of one — a real
reason to use `unsigned`/`size_t` for indices and sizes in hot loops.

`%` inherits the same asymmetry:

```
_smod16:                                 ; int32_t x % 16
	negs	w8, w0                   ; w8 = -x, and set flags from it
	and	w8, w8, #0xf             ; (-x) & 15
	and	w9, w0, #0xf             ;   x  & 15
	csneg	w0, w9, w8, mi           ; x>0 ? (x&15) : -((-x)&15)
	ret
```

The sign of `%` follows the dividend in C, so both branches are computed and
selected. For `unsigned`, `x % 16` is just `and w0, w0, #0xf`.

### Signed vs unsigned compare: `lt` vs `lo`

```
_lt_s:	cmp	w0, w1
	cset	w0, lt                   ; lt = signed Less Than       (N != V)
_lt_u:	cmp	w0, w1
	cset	w0, lo                   ; lo = unsigned LOwer         (C == 0)
```

Identical `cmp`, different condition. `cmp` just subtracts and sets flags; the
*reader* of the flags decides what the bits meant. `0xFFFFFFFF < 1` is true
signed (it is −1) and false unsigned (it is 4 billion). Mnemonics worth burning
in: signed `lt/le/gt/ge`, unsigned `lo/ls/hi/hs`. If you see `b.lo` in a loop
bound you are looking at `size_t`; `b.lt` means `int`.

### Bit tricks, one instruction at a time

```
_lowest:                                 ; x & -x  — isolate the lowest set bit
	neg	w8, w0
	and	w0, w0, w8
_is_pow2:                                ; x && !(x & (x-1))
	sub	w8, w0, #1
	tst	w0, w8                   ; tst = and, discard result, keep flags
	cset	w8, eq
	cmp	w0, #0
	csel	w0, wzr, w8, eq          ; the `x &&` guard, branchlessly
	ret
_lead0:	clz	w0, w0                   ; Count Leading Zeros — a native instruction
_trail0:                                 ; count TRAILING zeros — no such instruction
	rbit	w8, w0                   ; reverse the bit ORDER, then...
	clz	w0, w8                   ; ...leading zeros of the reverse = trailing zeros
_rev:	rbit	w0, w0                   ; bit reversal (used by FFT bit-reverse permutation)
_bswap:	rev	w0, w0                   ; BYTE reversal — endianness swap. Note rbit != rev.
```

`clz` is how you get `floor(log2(x))` in one cycle: `31 - clz(x)`. `rbit`+`clz`
for trailing zeros is the canonical ARM64 sequence — it shows up in every
allocator's free-list scan and every bitboard.

Popcount is the odd one out:

```
_popcnt:                                 ; __builtin_popcountll(x)
	fmov	d0, x0                   ; move the integer's BITS into a vector register
	cnt.8b	v0, v0                   ; per-byte popcount: 8 counts in 8 lanes
	addv.8b	b0, v0                   ; horizontal add of all 8 lanes
	fmov	w0, s0                   ; bits back to a general register
	ret
```

ARM64's scalar integer unit has no popcount — only NEON does, per byte. So the
value takes a round trip through the vector register file. It is still ~4 cycles
and far better than a loop, but note the *shape*: this is your first sight of a
horizontal reduction (`addv`), which chapter 19 is built on.

### Floats: a different register file, and explicit conversions

```
_f_add:	fadd	d0, d0, d1               ; double: d registers (64-bit view)
_g_add:	fadd	s0, s0, s1               ; float:  s registers (32-bit view of the SAME regs)
_f_fma:	fmadd	d0, d0, d1, d2           ; a*b+c — ONE instruction, ONE rounding
_f_div:	fdiv	d0, d0, d1               ; ~10-15 cycles, not pipelined like fadd/fmul
```

`fmadd` computes `a*b+c` with a single rounding at the end, so it is both faster
*and* more accurate than separate `fmul`+`fadd` — which is exactly why it can
change your results (chapter 19 §10). At `-O1` with default `-ffp-contract=on`
clang forms it for you.

Conversions are instructions, not reinterpretations:

```
_i2d:	scvtf	d0, x0                   ; Signed ConVerT to Float
_u2d:	ucvtf	d0, x0                   ; Unsigned — different instruction, different answer for x<0
_d2i:	fcvtzs	x0, d0                   ; Float ConVerT to Signed, rounding toward Zero (C truncation)
_d2f:	fcvt	s0, d0                   ; double -> float, rounds to nearest
```

`(int)3.9 == 3` because the instruction is `fcvtz`**s** — round toward **z**ero
is baked into the mnemonic. Compare with pure bit moves:

```
_d_bits:fmov	x0, d0                   ; the union punning trick: bits, unchanged
_d_abs:	fabs	d0, d0                   ; clears bit 63. One instruction, no branch, no library call.
```

`fmov x0, d0` vs `fcvtzs x0, d0`: same source, same destination, completely
different meaning. `fmov` is `d_bits(1.0) == 0x3FF0000000000000`; `fcvtzs` is
`(long)1.0 == 1`.

### NaN gets its own flag state

```
_f_lt:	fcmp	d0, d1
	cset	w0, mi                   ; mi = "minus" = strictly less. NOT `lt`.
_f_isnan:
	fcmp	d0, d0                   ; compare a value with ITSELF
	cset	w0, vs                   ; vs = oVerflow Set = the UNORDERED flag
	ret
```

`fcmp` can produce four outcomes: less, equal, greater, **unordered** (either
operand is NaN), encoded as N=0 Z=0 C=1 V=1. So `a != a` is a one-instruction
NaN test — and it is why `!(a < b)` is not the same as `a >= b` for floats:
with a NaN, *both* are false. Every `else` branch after a float comparison
silently catches NaN, which is how a NaN loss value quietly becomes a NaN weight.

### The pieces of a `mulmod`

```
_mul_hi:                                 ; ((__int128)a*b) >> 64
	umulh	x0, x1, x0               ; Unsigned MULtiply High: the TOP 64 bits of a 128-bit product
	ret
_mulmod:                                 ; ((__int128)a*b) % m
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mul	x8, x1, x0               ; low  64 bits of the product
	umulh	x1, x1, x0               ; high 64 bits of the product
	mov	x0, x8                   ; the 128-bit value is the PAIR (x0=lo, x1=hi)
	mov	x3, #0                   ; the modulus, widened to 128: (x2=m, x3=0)
	bl	___umodti3               ; ...and a LIBRARY CALL for the division
	ldp	x29, x30, [sp], #16
	ret
```

Read the cost structure straight off this. The 128-bit *product* is two cheap
instructions (`mul` + `umulh`), because the hardware computes both halves
anyway. The 128-bit *modulo* is a `bl` to a compiler-runtime routine —
hundreds of cycles of a software division loop. That asymmetry is precisely why
real modular-arithmetic code (hashing, PRNGs, number theory) uses Barrett or
Montgomery reduction: they replace `%` with `umulh` + `mul` + `sub`, turning
that `bl` back into three register instructions.

Note also that `mul x8, x1, x0` is a *plain* multiply. Signed 64×64 overflow is
UB in C, but the instruction is the same one either way — the machine wraps
silently. UB is not something the CPU does; it is something the compiler is
allowed to assume never happens.

## Read it yourself

1. Change `sar`'s parameter to `int64_t` and `shr`'s to `uint64_t`. Predict the
   register letter and the mnemonic before checking. Then predict `sdiv16` for
   `int64_t` — how many instructions?
2. Write `int cmp_idx(int i, size_t n) { return i < n; }`, compile with
   `-Wall -Wextra`, and read both the warning *and* whether you get `lt` or `lo`.
   Explain the warning using the two condition codes.
3. Replace `extract` with `(x >> 3) & 0x1F` and predict the two immediates in the
   `ubfx`. Then try `(x >> 3) & 0x1E` — does `ubfx` survive? Why not?
4. Compile `f_fma` with `-ffp-contract=off` and diff. Then compile
   `double s(double*a,int n){double t=0;for(int i=0;i<n;i++)t+=a[i];return t;}`
   with and without `-ffast-math` and explain why only one version can reorder.
5. Write `int classify(double a, double b) { if (a < b) return -1; else if (a > b)
   return 1; else return 0; }` and find where NaN lands. Then predict the flag
   test used for the `else`.

## Takeaways

* `& | ^ ~` are one instruction; ARM64 fuses the `~` into `bic`/`orn`/`eon`.
  Shift-and-mask idioms become `ubfx`/`bfi` — write the idiom, not intrinsics.
* Signedness picks the *instruction*: `asr` vs `lsr`, `lt` vs `lo`. Signed
  division by a power of two costs three extra instructions for the bias; use
  unsigned for indices and sizes.
* `clz` is native, trailing zeros is `rbit`+`clz`, popcount detours through NEON
  (`cnt` + `addv`) — your first horizontal reduction.
* Floats are a separate register file. `fmov` moves bits, `scvtf`/`fcvtzs`
  convert values, and `fcvtzs` truncating toward zero *is* C's `(int)` cast.
  `fdiv` is slow; `fmadd` is one rounding, which is why it changes results.
* `fcmp` has a fourth outcome, unordered; `a != a` is `cset vs`, and every
  `else` after a float compare catches NaN.
* 128-bit multiply is two instructions (`mul`+`umulh`); 128-bit `%` is a library
  call. That gap is the entire motivation for Barrett/Montgomery reduction.
