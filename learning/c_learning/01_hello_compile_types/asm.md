# 01 — asm.md: the four stages, hello's assembly, how types look

Companion to `lesson.md`. Read `00_arm64_assembly_primer/lesson.md` first.
All output below is from `cc -O1 -S -o -` on this machine (Apple clang 21,
arm64), with `.cfi_*`/`.p2align`/`.section`/`.loh` lines removed.

## What to look for

* Where each of the four compilation stages stops, and which tool inspects its output.
* How `printf("…", "world")` looks: string literals as labelled `.asciz` data,
  addresses built with `adrp`+`add`, and the variadic argument going **on the stack**.
* How `int`, `char`, `long`, `double`, `float` literals are materialised — the
  register (`w`/`x`/`d`/`s`) *is* the type.
* Casts as single instructions (`fcvtzs`, `scvtf`, `sxtw`, `and #0xff`) — or as
  nothing at all.
* `sizeof` decides the store width (`strb`/`strh`/`str w`/`str x`) and the stack
  frame layout.

## The C

```c
/* c1.c */
#include <stdio.h>
int main(void) { printf("hello, %s\n", "world"); return 0; }
```

```c
/* c1b.c — one function per idea, so each is a few lines of asm */
int    lit_int(void)   { return 42; }
long   lit_big(void)   { return 0x123456789abL; }
char   lit_char(void)  { return 'A'; }
double lit_dbl(void)   { return 3.5; }
float  lit_flt(void)   { return 0.1f; }
int    trunc_cast(double d)     { return (int)d; }
double widen_cast(int i)        { return i; }
unsigned char narrow(int i)     { return (unsigned char)i; }
long   widen_signed(int i)      { return i; }
unsigned long widen_uns(unsigned i) { return i; }
int    div_by_ten(int i)        { return i / 10; }
int    sizes(void) {
    char c; short s; int i; long l; double d;
    volatile char *pc = &c; volatile short *ps = &s; volatile int *pi = &i;
    volatile long *pl = &l; volatile double *pd = &d;
    *pc = 1; *ps = 2; *pi = 3; *pl = 4; *pd = 5.0;
    return (int)(sizeof c + sizeof s + sizeof i + sizeof l + sizeof d);
}
```

(`volatile` in `sizes` forces the stores to happen so the frame is visible;
otherwise `-O1` would delete all five locals and return `23` directly.)

## The four stages made visible

```sh
cc -E c1.c | wc -l        # 1. preprocess: 568 lines — 2 of them are yours
cc -S -o c1.s c1.c        # 2. compile:    C → assembly text (below)
cc -c c1.c                # 3. assemble:   → c1.o, 808 bytes of Mach-O
cc -o c1 c1.o             # 4. link:       → 33 KB executable
```

What the object file knows (`nm c1.o`):

```
0000000000000000 T _main      ; T = defined in the text section, exported
                 U _printf    ; U = undefined: someone else must provide it
0000000000000044 s l_.str     ; s = local data symbol (our format string)
000000000000004f s l_.str.1
```

What the linker resolved it against (`otool -L c1`):

```
/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1356.0.0)
```

`printf` is not in your binary. It is in `libSystem` and found at load time by
`dyld`; the `U` becomes a stub that jumps into the library.

## The assembly

### hello

```
_main:
	sub	sp, sp, #32                  ; frame: 32 bytes (16-aligned)
	stp	x29, x30, [sp, #16]          ; save fp and lr — we are about to `bl`
	add	x29, sp, #16                 ; new frame pointer
	adrp	x8, l_.str.1@PAGE            ; x8 = page of "world"
	add	x8, x8, l_.str.1@PAGEOFF     ; x8 = &"world"
	str	x8, [sp]                     ; variadic args go ON THE STACK (Apple arm64 ABI)
	adrp	x0, l_.str@PAGE
	add	x0, x0, l_.str@PAGEOFF       ; x0 = &"hello, %s\n"  — the fixed arg, in a register
	bl	_printf
	mov	w0, #0                       ; return 0
	ldp	x29, x30, [sp, #16]          ; restore fp, lr
	add	sp, sp, #32
	ret

l_.str:
	.asciz	"hello, %s\n"             ; NUL-terminated, in __TEXT,__cstring
l_.str.1:
	.asciz	"world"
```

Two things you cannot see in C: the `\n` is a single byte `0x0a` inside the
`.asciz`; and on Apple arm64 **every `...` argument is passed in memory**, even
when registers are free — that is the `str x8, [sp]`. (Linux/AAPCS64 would pass
`"world"` in `x1`.) The leading `l` on `l_.str` means "private to this object".

### Literals — the register is the type

```
_lit_int:
	mov	w0, #42                      ; int  → 32-bit w register
	ret
_lit_char:
	mov	w0, #65                      ; char 'A' is just the int 65; still w0
	ret
_lit_big:
	mov	x0, #35243                   ; 0x89ab           (bits  0..15)
	movk	x0, #17767, lsl #16          ; 0x4567 → bits 16..31   (movk = move-keep)
	movk	x0, #291, lsl #32            ; 0x0123 → bits 32..47
	ret                                  ; 0x123456789ab assembled in 3 × 16-bit chunks
_lit_dbl:
	fmov	d0, #3.50000000              ; some doubles fit an 8-bit immediate encoding
	ret
_lit_flt:
	mov	w8, #52429                   ; 0.1f does NOT: build its bit pattern 0x3dcccccd
	movk	w8, #15820, lsl #16          ;   in an integer register …
	fmov	s0, w8                       ;   … and move the bits across.  0.1 is not exact.
	ret
```

### Casts

```
_trunc_cast:                             ; (int)d
	fcvtzs	w0, d0                       ; Float ConVerT to Zero-rounded Signed. One instruction.
	ret
_widen_cast:                             ; (double)i
	scvtf	d0, w0                       ; Signed ConVerT to Float
	ret
_narrow:                                 ; (unsigned char)i
	and	w0, w0, #0xff                ; keep the low byte; the "mod 256" the standard promises
	ret
_widen_signed:                           ; (long)i
	sxtw	x0, w0                       ; Sign-eXTend Word: replicate bit 31 into 32..63
	ret
_widen_uns:                              ; (unsigned long)u
	mov	w0, w0                       ; writing w0 zeroes the top half — that IS the zero-extend
	ret
```

`mov w0, w0` looks like a no-op; it is the cheapest possible zero-extension.

### Integer division is not a `div`

```
_div_by_ten:                             ; i / 10
	mov	w8, #26215
	movk	w8, #26214, lsl #16          ; w8 = 0x66666667  ≈ 2^34 / 10
	smull	x8, w0, w8                   ; 64-bit product of two 32-bit signed
	asr	x8, x8, #34                  ; >> 34  → i/10, rounded toward −∞
	add	w0, w8, w8, lsr #31          ; + (sign bit) → fix rounding toward 0 for negatives
	ret
```

`sdiv` costs ~10× a multiply, so the compiler multiplies by a magic reciprocal.
Divide by a variable and you will see a real `sdiv`.

### `sizeof` in the stack frame

```
_sizes:
	sub	sp, sp, #32                  ; 1+2+4+8+8 = 23 bytes needed → rounded to 32
	mov	w8, #1
	strb	w8, [sp, #31]                ; char   c : 1 byte  at sp+31
	mov	w8, #2
	strh	w8, [sp, #28]                ; short  s : 2 bytes at sp+28 (2-aligned)
	mov	w8, #3
	str	w8, [sp, #24]                ; int    i : 4 bytes at sp+24 (4-aligned)
	mov	w8, #4
	str	x8, [sp, #16]                ; long   l : 8 bytes at sp+16 (8-aligned)
	mov	x8, #4617315517961601024     ; 0x4014000000000000 = bit pattern of 5.0
	str	x8, [sp, #8]                 ; double d : 8 bytes at sp+8  (stored via an x register!)
	mov	w0, #23                      ; sizeof sum computed at compile time
	add	sp, sp, #32
	ret
```

The suffix on the store — `strb`, `strh`, `str w`, `str x` — is `sizeof` made
executable. Each variable sits at an offset that is a multiple of its size
(alignment), and the frame is padded to 16. The `double` constant is written
with an integer store because the compiler only needs the bits to land in memory.

## Read it yourself

1. Change `"world"` to a second `%d` argument: `printf("hello %s %d\n", "world", 7)`.
   Predict how many `str … [sp, #…]` appear and at which offsets. Check with
   `cc -O1 -S -o - c1.c`. (Apple packs variadics tightly by their natural size —
   does `7` occupy 4 or 8 bytes?)
2. Change `lit_dbl` to return `3.7`. Predict: `fmov d0, #imm` or the
   `mov/movk/fmov` bit-pattern route? Then try `0.5`, `1.0`, `100.0`, `0.1`.
   Which shape do the "nice" values take?
3. Change `div_by_ten` to `i / 8`, then `i / 7`, then `(unsigned)i / 10`. Predict
   which produce a shift, a magic multiply, or an `sdiv`/`udiv`. Note what the
   `unsigned` version drops compared to the signed one.
4. In `sizes`, reorder the declarations to `double d; char c; short s; int i; long l;`.
   Predict the new offsets. Does the frame still fit in 32 bytes? Check.
5. Change `narrow` to return `(signed char)i`. Predict the replacement for
   `and w0, w0, #0xff`. (Hint: the primer's table lists `sxtb`.)

## Takeaways

* `-E`, `-S`, `-c`, and the link are four separate programs' worth of work;
  `nm` and `otool -L` show what the object knows and what the executable borrows.
* On Apple arm64 the fixed arguments of `printf` use registers; the variadic
  ones are always written to `[sp]`. Different from Linux — a real ABI difference.
* Type ≈ register width. `int`/`char` → `w`, `long`/pointer → `x`, `double` → `d`,
  `float` → `s`. Casts between them are single instructions or nothing.
* Constants that do not fit a 16-bit chunk are built with `mov`+`movk`;
  a few doubles have a special immediate form.
* Division by a constant is a multiply-and-shift; `sizeof` shows up as the store
  width and the stack offsets.
