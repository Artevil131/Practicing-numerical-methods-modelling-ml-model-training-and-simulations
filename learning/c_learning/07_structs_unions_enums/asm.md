# 07 — asm.md: a struct is an offset table

Companion to `lesson.md`. Prerequisite: `00_arm64_assembly_primer/lesson.md` §4,
and `05_pointers/asm.md`.
Output is `cc -O1 -S -o -` on this machine; noise lines removed.

## What to look for

* A struct has **no runtime existence**. `s->field` is `ldr` with a constant
  immediate. The struct type is a compile-time table of `name → byte offset`.
* Padding is *visible*: the offsets in the immediates jump over the holes.
  `offsetof` and `sizeof` are `mov wN, #imm` — computed by the compiler, not the CPU.
* Passing by value vs by pointer: ≤16 bytes of integers goes in `x0`/`x1`; a
  homogeneous float aggregate goes in `d0`–`d3`; anything else is passed
  **by hidden pointer** — and then by-value and by-pointer produce *identical* asm.
* Returning a big struct: the caller allocates the space and passes its address
  in **`x8`**, the indirect result register. There is no "returning a struct".
* A `union` is one address with several load instructions to choose from.
* An `enum` is an immediate. `enum Color` and `int` are the same register.

## The C

```c
#include <stddef.h>
struct P   { int x; int y; };                    /* 8 bytes, no padding        */
struct Pad { char c; int i; char d; double e; }; /* padding everywhere         */
struct Pair2 { long a, b; };                     /* 16 bytes                   */
struct Rec   { long a, b, c; };                  /* 24 bytes: too big for regs */
struct Big   { double a, b, c, d; };             /* 32 bytes, but all doubles  */
struct Matrix { int rows, cols; double *data; };
struct Packed { char c; double e; int i; char d; } __attribute__((packed));

int    p_x(const struct P *p)       { return p->x; }
int    p_y(const struct P *p)       { return p->y; }
int    pad_i(const struct Pad *s)   { return s->i; }
char   pad_d(const struct Pad *s)   { return s->d; }
double pad_e(const struct Pad *s)   { return s->e; }
size_t size_pad(void)               { return sizeof(struct Pad); }
size_t off_e(void)                  { return offsetof(struct Pad, e); }
double pak_e(const struct Packed *p){ return p->e; }

int  sum_val(struct P p)            { return p.x + p.y; }
int  sum_ptr(const struct P *p)     { return p->x + p->y; }
long pair_sum(struct Pair2 p)       { return p.a + p.b; }
long rec_sum(struct Rec r)          { return r.a + r.b + r.c; }
long rec_sum_p(const struct Rec *r) { return r->a + r->b + r->c; }
double big_val(struct Big b)        { return b.a + b.d; }

struct Rec make_rec(long v)         { struct Rec r = { v, v+1, v+2 }; return r; }
struct Rec ext_make(long v);                       /* defined elsewhere */
long call_it(long v) { struct Rec r = ext_make(v); return r.b; }

union U { int i; float f; unsigned u; };
int      u_i(const union U *u) { return u->i; }
float    u_f(const union U *u) { return u->f; }
unsigned bits_of(float f)      { union U u; u.f = f; return u.u; }

enum Color { RED, GREEN = 5, BLUE };
enum Color pick(int n)          { return n ? GREEN : BLUE; }
int  color_code(enum Color c)   { return (int)c + 1; }

double m_at(const struct Matrix *m, int r, int c) { return m->data[r * m->cols + c]; }
```

## The assembly

### Field access is a load with a constant offset

```
_p_x:                                    ; p->x
	ldr	w0, [x0]                 ; offset 0
	ret
_p_y:                                    ; p->y
	ldr	w0, [x0, #4]             ; offset 4 — the ONLY thing the field name became
	ret
```

`w0` not `x0`: `int` is 32 bits, so the 32-bit view of the register. There is no
"struct" instruction, no lookup, no header. The `.` and `->` operators are
resolved entirely at compile time into these immediates.

### Padding, made visible

`struct Pad { char c; int i; char d; double e; }`

```
_pad_i:                                  ; s->i
	ldr	w0, [x0, #4]             ; NOT #1 — 3 bytes of padding after `c`
	ret
_pad_d:                                  ; s->d
	ldrsb	w0, [x0, #8]             ; ldrsb = load byte, sign-extend (char is signed here)
	ret
_pad_e:                                  ; s->e
	ldr	d0, [x0, #16]            ; NOT #9 — 7 bytes of padding after `d`
	ret
_size_pad:
	mov	w0, #24                  ; sizeof == 24, computed at compile time
	ret
_off_e:
	mov	w0, #16                  ; offsetof(Pad, e) — also a constant
	ret
```

Read the layout straight off the immediates:

```
byte:  0    1  2  3    4 5 6 7    8    9 ... 15    16 .. 23
       c    ---pad---  i i i i    d    ---pad---   e e e e e e e e
```

Each field sits at an offset that is a multiple of its own alignment (4 for
`int`, 8 for `double`), and the struct's total size is rounded up to the largest
member's alignment so that `Pad arr[2]` keeps every element aligned. Reorder to
`{ double e; int i; char c; char d; }` and `sizeof` drops to 16 — the same
fields, 8 bytes saved per element. In an array of a million particles that is
8 MB of cache you did not have to touch.

`__attribute__((packed))` removes the holes and the cost shows up in the load:

```
_s_pak:  mov	w0, #14                  ; sizeof(Packed) == 14, not 24
_pak_e:  ldur	d0, [x0, #1]             ; ldUr: UNSCALED/unaligned load at offset 1
```

`ldr` immediates must be a multiple of the access size; `ldur` handles an
arbitrary byte offset. On this core that is cheap, on some it is not, and taking
`&p->e` from a packed struct yields a misaligned pointer, which is UB. Packed is
for wire formats, not for performance.

### Passing by value: the size rules, in three examples

```
_sum_val:                                ; int sum_val(struct P p)   — 8 bytes
	lsr	x8, x0, #32              ; p.y lives in the TOP half of x0
	add	w0, w8, w0               ; p.x is the bottom half
	ret
_sum_ptr:                                ; int sum_ptr(const struct P *p)
	ldp	w8, w9, [x0]             ; two words from memory in one instruction
	add	w0, w9, w8
	ret
```

The whole 8-byte struct was **packed into one register**. `p.y` is extracted with
a shift, not a load. Compare `sum_ptr`, which must actually go to memory. For
small structs, by value is *cheaper*.

```
_pair_sum:                               ; struct Pair2 {long,long} — 16 bytes
	add	x0, x0, x1               ; a in x0, b in x1. Still no memory at all.
	ret
```

Up to 16 bytes of integer/pointer fields ride in `x0`+`x1`. Now go one field further:

```
_rec_sum:                                ; struct Rec {long,long,long} BY VALUE
	ldp	x8, x9, [x0]
	ldr	x10, [x0, #16]
	add	x8, x9, x8
	add	x0, x8, x10
	ret
_rec_sum_p:                              ; const struct Rec *      BY POINTER
	ldp	x8, x9, [x0]
	ldr	x10, [x0, #16]
	add	x8, x9, x8
	add	x0, x8, x10
	ret                                  ; ← byte-for-byte the same function
```

This is the punchline of §5 of the lesson. Over 16 bytes, "by value" is
implemented as *the caller copies the struct into its stack frame and passes a
pointer*. The callee's code is identical to the by-pointer version; the
difference is entirely on the caller's side, where the by-value call must first
make a copy. That copy is the cost — and it grows with `sizeof`.

The 16-byte rule has one big exception:

```
_big_val:                                ; struct Big { double a,b,c,d; } — 32 bytes
	fadd	d0, d0, d3               ; a is in d0, d is in d3 — no memory!
	ret
```

A **homogeneous floating aggregate** (≤4 members, all the same float type) rides
in `d0`–`d3` even at 32 bytes. This is why a `struct vec4 { float x,y,z,w; }`
passed by value is free, and why graphics/SIMD code passes small float structs
by value without a second thought.

### Returning a struct: the `x8` indirect result register

```
_make_rec:                               ; struct Rec make_rec(long v)
	add	x9, x0, #1
	stp	x0, x9, [x8]             ; write r.a, r.b to the address in x8
	add	x9, x0, #2
	str	x9, [x8, #16]            ; write r.c
	ret                              ; returns NOTHING in x0
```

`x8` was never assigned in this function — it arrived as a hidden parameter. The
caller side:

```
_call_it:                                ; struct Rec r = ext_make(v); return r.b;
	sub	sp, sp, #48
	stp	x29, x30, [sp, #32]
	add	x29, sp, #32
	add	x8, sp, #8               ; x8 = &r  — caller allocates the return slot
	bl	_ext_make
	ldr	x0, [sp, #16]            ; r.b  (sp+8 is r.a, sp+16 is r.b)
	ldp	x29, x30, [sp, #32]
	add	sp, sp, #48
	ret
```

So `struct Rec f(void)` is really `void f(struct Rec *out)` with the out-pointer
in `x8` instead of `x0`. This is the ABI's *sret* convention. It also explains
why returning a big struct is not "slow because of the copy" in the way people
fear — there is exactly one copy, written directly into the caller's slot.

(When `make_rec` is visible and inlinable the whole thing evaporates:
`long use_make(long v) { return make_rec(v).c; }` compiles to `add x0, x0, #2`.)

### A union is one address

```
_u_i:	ldr	w0, [x0]                 ; offset 0
	ret
_u_f:	ldr	s0, [x0]                 ; offset 0 — SAME address, different register file
	ret
```

Every member of a union is at offset 0. The member you name selects the
*instruction*, not the location: `ldr w0` reads the 4 bytes as an integer,
`ldr s0` reads the same 4 bytes into a float register. Type punning through a
union is exactly "reinterpret these bits with a different load".

When the value is already in a register the load disappears entirely:

```
_bits_of:                                ; union { float f; unsigned u; }, write f, read u
	fmov	w0, s0                   ; move the bit pattern s0 -> w0. Zero conversion.
	ret
```

`fmov` between an `s` and a `w` register copies bits; `fcvtzs` would *convert* the
value. `bits_of(1.0f)` is `0x3f800000` — chapter 12 lives here.

### An enum is an immediate

```
_pick:                                   ; return n ? GREEN : BLUE;   (5 : 6)
	cmp	w0, #0
	mov	w8, #5                   ; GREEN
	cinc	w0, w8, eq               ; w0 = (n==0) ? 5+1 : 5   — BLUE == GREEN+1
	ret
_color_code:                             ; (int)c + 1
	add	w0, w0, #1               ; the cast produced nothing
	ret
```

`RED=0, GREEN=5, BLUE=6` exist only in the compiler's symbol table. At runtime an
`enum Color` is a `w` register holding an integer, and the cast to `int` is free.
Note the compiler even noticed `BLUE == GREEN + 1` and used `cinc` (conditional
increment) instead of a branch.

### The `Matrix` accessor: where the offsets and the index math meet

```
_m_at:                                   ; m->data[r * m->cols + c]
	ldr	x8, [x0, #8]             ; m->data     (offset 8: after two ints)
	ldr	w9, [x0, #4]             ; m->cols     (offset 4)
	madd	w9, w9, w1, w2           ; w9 = cols*r + c   — multiply-add, one instruction
	ldr	d0, [x8, w9, sxtw #3]    ; data + (index sign-extended, <<3 for 8-byte double)
	ret
```

Three memory touches for one element: the header's `data` pointer, the header's
`cols`, then the payload. Hoisting `m->cols` and `m->data` out of a loop by hand
is not superstition — inside a loop the compiler must re-load them after any
store it cannot prove is unrelated (aliasing, `05_pointers/asm.md`).

## Read it yourself

1. Reorder `struct Pad` to `{ double e; int i; char c; char d; }` and re-run.
   Predict `sizeof` and the immediate in each accessor **before** you look.
2. Grow `struct Pair2` to `{ long a, b, c_unused; }` and watch `pair_sum` change
   from `add x0, x0, x1` to loads. Where exactly is the 16-byte cliff?
3. Change `struct Big` to `{ double a, b, c, d, e; }` (five doubles). Predict
   whether `big_val` still uses `d0`/`d3`, and explain using the HFA rule.
4. Write `struct P make_p(int a, int b)` returning the 8-byte struct. Predict
   whether `x8` appears. (Hint: what fits in `x0`?) Then do the same for
   `struct Big` — how many registers carry it back?
5. Take a struct with a `char` first field, make it `signed char` then
   `unsigned char`, and diff the accessor. Which of `ldrsb` / `ldrb` appears, and
   what does that tell you about `char`'s signedness on this platform?

## Takeaways

* A struct is a compile-time offset table; `s.f` / `s->f` is `ldr` with an
  immediate. `sizeof` and `offsetof` are constants baked into the instructions.
* Padding is readable directly off those immediates. Order fields
  largest-alignment-first and the holes disappear.
* Passing rules: ≤16 bytes of integers → `x0`/`x1`; ≤4 same-typed floats →
  `d0`–`d3`; otherwise a hidden pointer, at which point by-value and by-pointer
  callee code is *identical* and only the caller's copy differs.
* Returning a large struct uses `x8`, the indirect result register: the caller
  supplies the destination address.
* A union is one address; the member name picks the load instruction. An enum is
  an integer immediate with no runtime type.
