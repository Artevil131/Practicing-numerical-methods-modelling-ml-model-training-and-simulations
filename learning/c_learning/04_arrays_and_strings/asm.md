# 04 — asm.md: arrays are address arithmetic

Companion to `lesson.md`. Prerequisite: `00_arm64_assembly_primer/lesson.md` §4 (addressing modes).
Output is `cc -O1 -S -o -` on this machine unless marked otherwise; noise lines removed.

## What to look for

* `a[i]` is **one instruction**: `ldr x0, [base, index, lsl #3]`. The shift
  amount is `log2(sizeof element)` — that is the type system, compiled.
* The load *width and extension* (`ldr x` / `ldr w` / `ldrsb`) is the element type.
* 2D row-major `m[r][c]` = `base + (r*COLS + c)*8`: a multiply-add and a scaled load.
* A `strlen` loop is `ldrb` + `cbnz`; string copy is `ldrb`/`strb` with post-index.
* Out-of-bounds is silent because the hardware just computes `base + i*8` and
  loads — there is no bounds anywhere in the instruction.
* String literals are `.asciz` in `__TEXT,__cstring`; a `char s[] = "…"` is a copy.

## The C

```c
long get(const long *a, long i)        { return a[i]; }
int  geti(const int *a, long i)        { return a[i]; }
char getc_(const char *s, long i)      { return s[i]; }
void put(long *a, long i, long v)      { a[i] = v; }
#define COLS 7
double cell(const double m[][COLS], int r, int c)          { return m[r][c]; }
double cell_dyn(const double *m, int ncols, int r, int c)  { return m[r * ncols + c]; }
size_t my_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
void my_strcpy(char *d, const char *s) { while ((*d++ = *s++)) ; }
long oob(void) { long a[4] = {1, 2, 3, 4}; volatile long i = 4; return a[i]; }
const char *greet(void) { return "hello"; }
int first_char(void) { char s[] = "hello"; return s[0] + s[4]; }
```

## The assembly

### `a[i]` — one instruction, and the shift is `sizeof`

```
_get:                                    ; long a[]
	ldr	x0, [x0, x1, lsl #3]     ; x0 = *(a + (i << 3))   — 8-byte elements
	ret
_geti:                                   ; int a[]
	ldr	w0, [x0, x1, lsl #2]     ; 4-byte elements → lsl #2, 32-bit destination
	ret
_getc_:                                  ; char s[]
	ldrsb	w0, [x0, x1]             ; 1-byte elements → no shift; SIGN-extend (char is signed here)
	ret
_put:
	str	x2, [x0, x1, lsl #3]     ; a[i] = v: same address mode, store instead of load
	ret
```

`a[i]` is defined as `*(a + i)`, and the `+` scales by the element size. AArch64
has an addressing mode that does exactly `base + (index << k)`, so the whole
expression folds into the load. Look at three things and you know the element
type: the shift (`#3`/`#2`/none), the destination width (`x`/`w`), and the
extension (`ldrsb` vs `ldrb`).

### 2D row-major

```
_cell:                                   ; m[r][c], COLS = 7 known at compile time
	mov	w8, #56                  ; 56 = 7 columns × 8 bytes = one row
	smaddl	x8, w1, w8, x0           ; x8 = r*56 + m       (Signed Multiply-ADD Long: 32×32→64 + 64)
	ldr	d0, [x8, w2, sxtw #3]    ; d0 = *(x8 + (c << 3)) — c sign-extended from int
	ret

_cell_dyn:                               ; m[r*ncols + c], ncols only known at run time
	madd	w8, w2, w1, w3           ; w8 = r*ncols + c       (32-bit — the C expression is int)
	ldr	d0, [x0, w8, sxtw #3]    ; then scale and load
	ret
```

Both are "row stride times row, plus column, times 8". With `COLS` a constant the
compiler pre-multiplied `7 × 8 = 56`, so the row stride is a byte count and only
one multiply remains. The `sxtw` inside the address is the `int → long`
conversion of the index — done for free by the addressing mode. Note the
`cell_dyn` index is computed in **32 bits** (`madd w8`): `r*ncols + c` overflows
`int` on a large matrix exactly as the C standard says it may.

### strlen: `ldrb` + `cbnz`

`-O1` recognises the idiom and emits a tail call — `_my_strlen: b _strlen` — so
to see the loop, compile with `-fno-builtin`:

```
_my_strlen:                              ; cc -O1 -fno-builtin
	mov	x8, #0                   ; n = 0
LBB6_1:
	ldrb	w9, [x0, x8]             ; w9 = s[n]  (byte, zero-extended)
	add	x8, x8, #1               ; n++          — done BEFORE the test
	cbnz	w9, LBB6_1               ; loop while the byte was non-zero
	sub	x0, x8, #1               ; we incremented one too many; undo
	ret
```

One byte per iteration, three instructions. The real `strlen` in libSystem
loads 16 bytes at a time with NEON and finds the first zero with a bit trick;
that is why the compiler prefers to call it.

### strcpy: post-index on both pointers

```
_my_strcpy:
LBB7_1:
	ldrb	w8, [x1], #1             ; w8 = *s; s++
	strb	w8, [x0], #1             ; *d = w8; d++
	cbnz	w8, LBB7_1               ; stop AFTER copying the NUL
	ret
```

`*d++ = *s++` is two post-indexed accesses — the increment happens inside the
load/store instruction. The loop exits after copying the terminator, which is why
the C idiom works.

### Out of bounds is silent

```
_oob:
	sub	sp, sp, #16
	mov	w8, #4
	str	x8, [sp, #8]             ; volatile i = 4 → really stored
	ldr	x8, [sp, #8]             ; and really reloaded
	adrp	x9, l___const.oob.a@PAGE
	add	x9, x9, l___const.oob.a@PAGEOFF   ; a[] was never written → kept as a constant table
	ldr	x0, [x9, x8, lsl #3]     ; load table[4]: 8 bytes past the end. No check. Whatever is there.
	add	sp, sp, #16
	ret

l___const.oob.a:
	.quad	1
	.quad	2
	.quad	3
	.quad	4                        ; ← a[3]. a[4] is whatever the assembler placed next.
```

There is no instruction that knows the array has 4 elements. The `ldr` computes
an address and loads. Here that address is the next thing in the constant
section (in this file: the string "hello"). On the stack it would be another
local or the saved return address — which is how buffer overflows become
control-flow hijacks, and why chapter 03 showed the stack canary.

### String literal vs char array

```
_greet:                                  ; return "hello";
	adrp	x0, l_.str@PAGE
	add	x0, x0, l_.str@PAGEOFF   ; pointer to read-only data — no copy
	ret

_first_char:                             ; char s[] = "hello"; return s[0] + s[4];
	mov	w0, #215                 ; 'h' + 'o' = 104 + 111, computed at compile time
	ret

l_.str:                                  ; lives in __TEXT,__cstring — read-only, shared
	.asciz	"hello"
```

A literal is an address into read-only memory (writing through it faults). A
`char s[] = "hello"` is a 6-byte **array copy** on the stack; with the values
known, the compiler folded the whole function to a constant. Make `s` escape
(pass it to a function) and you will see the copy: a `mov x8, #0x6f6c6c6568`
(the bytes of "hello\0") and a `str`.

## Read it yourself

1. Change `get` to take `const short *a`. Predict the mnemonic, the shift
   amount and the destination register width. Then `const unsigned char *`.
2. In `cell`, change `COLS` to 8. Predict what replaces `mov w8, #56; smaddl`.
   (Hint: a power of two is a shift; can it fold into `add x8, x0, w1, sxtw #6`?)
3. Compile `my_strlen` with plain `-O1` and with `-O1 -fno-builtin`; then write
   it as `for (const char *p = s; *p; p++); return p - s;`. Does the pointer
   version still get recognised as `strlen`?
4. In `oob`, remove `volatile`. Predict what the compiler does with `a[4]`
   (it is undefined behaviour and the value is compile-time known). Compile with
   `-Wall` too and read the warning.
5. Make `first_char` pass `s` to an external `void use(char *)` before the
   return. Find the 8-byte immediate that holds "hello\0" and the `str` that
   writes it to the stack; explain why it is one store, not six `strb`s.

## Takeaways

* Indexing is address arithmetic folded into the load: `[base, idx, lsl #k]`
  with `k = log2(sizeof T)`; the load mnemonic gives width and signedness.
* Row-major 2D is `row*stride + col` in the integer unit, then the same scaled
  load. Constant strides get pre-multiplied.
* String loops are byte loads with post-index and `cbz`/`cbnz` on the byte;
  the compiler will swap your loop for libc's vectorised `strlen` when it can.
* Nothing in the instruction stream knows an array's length. OOB reads read
  the neighbour; OOB writes overwrite it.
* `"literal"` is an `adrp`/`add` to `__cstring`; `char s[] = "…"` is a copy the
  compiler may constant-fold away entirely.
