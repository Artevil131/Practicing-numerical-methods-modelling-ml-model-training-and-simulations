# 05 — asm.md: a pointer is a register holding an address

Companion to `lesson.md`. Prerequisite: `00_arm64_assembly_primer/lesson.md` §4.
Output is `cc -O1 -S -o -` on this machine; noise lines removed.

## What to look for

* `*p` is `ldr x0, [x0]`; `*p = v` is `str`. Nothing more.
* `&x` forces `x` into memory: a stack slot whose address is `sp + offset`.
* `p + 1` is `add x0, x0, #sizeof(*p)` — the scaling is the only thing the type
  contributes.
* `a[i]`, `*(a + i)`, a `long a[]` parameter and a `long *p` parameter: the
  assembly is **identical**. Arrays decay to pointers before the compiler sees them.
* `->` is a load with an immediate offset; `n->next->val` is two dependent loads;
  `**pp` is two dependent loads.
* `if (p)` is `cbz`; pointer difference is `sub` then `asr #3`.

## The C

```c
long deref(const long *p)        { return *p; }
void store(long *p, long v)      { *p = v; }
void by_ref(long *out);
long addr_of(void)               { long x = 0; by_ref(&x); return x; }
const long *next(const long *p)  { return p + 1; }
const int  *next_i(const int *p) { return p + 1; }
long second(const long *p)       { return p[1]; }
long second_ptr(const long *p)   { return *(p + 1); }
long sum_arr(const long a[], long n) { long s = 0; for (long i = 0; i < n; i++) s += a[i]; return s; }
long sum_ptr(const long *p, long n)  { long s = 0; const long *e = p + n; while (p < e) s += *p++; return s; }
struct node { long val; struct node *next; };
long val_of(const struct node *n)   { return n->val; }
long next_val(const struct node *n) { return n->next->val; }
long safe_val(const struct node *n) { return n ? n->val : -1; }
long pp(long **pp)                  { return **pp; }
void swap(long *a, long *b)         { long t = *a; *a = *b; *b = t; }
ptrdiff_t dist(const long *a, const long *b) { return b - a; }
```

## The assembly

### `*p` and `*p = v`

```
_deref:
	ldr	x0, [x0]                 ; x0 = memory at the address in x0
	ret
_store:
	str	x1, [x0]                 ; memory at x0 = v
	ret
```

A pointer is an integer in an `x` register. Dereference = use it as the base of
a load or store. `const` produced nothing — it is a compile-time promise only.

### `&x` puts x in memory

```
_addr_of:
	sub	sp, sp, #32
	stp	x29, x30, [sp, #16]      ; non-leaf: save fp/lr
	add	x29, sp, #16
	str	xzr, [sp, #8]            ; x = 0  — x MUST be in memory: someone will take its address
	add	x0, sp, #8               ; &x  == sp + 8   ← this is what the & operator compiles to
	bl	_by_ref
	ldr	x0, [sp, #8]             ; reload x: by_ref may have written through the pointer
	ldp	x29, x30, [sp, #16]
	add	sp, sp, #32
	ret
```

Without `&x`, `x` would have been a register (or nothing). Taking the address is
what forces a stack slot. After the call the compiler must **reload** `x` — it
cannot assume the register copy is still current, because the callee had a
pointer to it. This is aliasing, and it is why pointers cost optimisations.

### `p + 1` scales by the element size

```
_next:                                   ; const long *
	add	x0, x0, #8               ; +1 element = +8 bytes
	ret
_next_i:                                 ; const int *
	add	x0, x0, #4               ; +1 element = +4 bytes
	ret
```

Pointer arithmetic is integer arithmetic on the address with the element size
folded in. `void *` has no size, which is why `vp + 1` is not allowed.

### Array and pointer syntax compile to the same thing

```
_second:                                 ; p[1]
	ldr	x0, [x0, #8]
	ret
_second_ptr:                             ; *(p + 1)
	ldr	x0, [x0, #8]             ; byte-for-byte identical
	ret
```

And the two loop styles:

```
_sum_arr:                                ; for (i…) s += a[i]     — "array" parameter
	cmp	x1, #1
	b.lt	LBB7_4
	mov	x8, #0
LBB7_2:
	ldr	x9, [x0], #8             ; the index i is GONE: the compiler walks the pointer
	add	x8, x9, x8
	subs	x1, x1, #1
	b.ne	LBB7_2
	mov	x0, x8
	ret
LBB7_4:
	mov	x8, #0
	mov	x0, x8
	ret

_sum_ptr:                                ; while (p < e) s += *p++  — "pointer" parameter
	cmp	x1, #1
	b.lt	LBB8_4
	mov	x8, #0
	add	x9, x0, x1, lsl #3       ; e = p + n*8
LBB8_2:
	ldr	x10, [x0], #8            ; *p++  — post-index, exactly the C idiom
	add	x8, x10, x8
	cmp	x0, x9
	b.lo	LBB8_2                   ; p < e   (UNSIGNED compare: addresses are unsigned)
	mov	x0, x8
	ret
LBB8_4:
	mov	x8, #0
	mov	x0, x8
	ret
```

The `long a[]` parameter **is** a `long *` — same register, same loads. The
indexed loop was rewritten into a pointer walk (`ldr x9, [x0], #8`); the only
difference from `sum_ptr` is whether the loop counts down `n` or compares
against an end pointer. Choose whichever reads better in C; the machine does not
care.

### `->` is base + offset; chains are dependent loads

```
_val_of:                                 ; n->val     (offset 0)
	ldr	x0, [x0]
	ret
_next_val:                               ; n->next->val
	ldr	x8, [x0, #8]             ; x8 = n->next        (offset 8: after the 8-byte val)
	ldr	x0, [x8]                 ; x0 = x8->val        — cannot start until x8 arrives
	ret
_pp:                                     ; **pp
	ldr	x8, [x0]                 ; x8 = *pp
	ldr	x0, [x8]                 ; x0 = *x8            — same shape as n->next->val
	ret
```

Each arrow is one load; a chain of arrows is a chain of loads where each address
depends on the previous value. That is a **pointer-chasing dependency**: the CPU
cannot fetch the second until the first returns (≈4 cycles from L1, hundreds
from DRAM). Linked lists are slow for exactly this reason; arrays let the CPU
compute every address up front.

### NULL check → `cbz`

```
_safe_val:
	cbz	x0, LBB11_2              ; n == NULL ?  — NULL is the integer 0, so no cmp needed
	ldr	x0, [x0]                 ; n->val
	ret
LBB11_2:
	mov	x0, #-1
	ret
```

Without the check, `n == NULL` would execute `ldr x0, [xzr-ish 0]`: a load from
address 0, which the OS maps as inaccessible → `EXC_BAD_ACCESS`. The segfault is
not the language noticing a null pointer; it is the MMU refusing an address.

### swap and pointer difference

```
_swap:
	ldr	x8, [x0]                 ; t = *a
	ldr	x9, [x1]                 ; *b
	str	x9, [x0]                 ; *a = *b
	str	x8, [x1]                 ; *b = t
	ret
```

Both loads are issued before either store, so the compiler must be sure the
stores do not clobber the loads' sources — fine here because both were already
read. Note there is no "temporary" in memory: `t` is `x8`.

```
_dist:                                   ; b - a, in elements
	sub	x8, x1, x0               ; byte difference
	asr	x0, x8, #3               ; ÷ 8: arithmetic shift, because ptrdiff_t is SIGNED
	ret
```

Pointer subtraction divides by the element size (`asr #3` for 8-byte `long`).
Compare `next`'s `add #8`: multiplication on the way in, division on the way out.

## Read it yourself

1. Change `next` to take `const char *`, then `const struct node *`. Predict the
   immediate in each `add`. Then `const void *` — what error does the compiler
   give and why does the asm model explain it?
2. In `addr_of`, delete the `bl _by_ref` line (i.e. never pass `&x` anywhere).
   Predict the whole function body. Why does the `str xzr` disappear?
3. Rewrite `sum_arr` with `a[i]` replaced by `*(a + i)`, then with `i[a]`.
   Confirm all three produce identical asm (diff the `.s` files).
4. Change `struct node` to `{ int val; struct node *next; }`. Predict the
   offset in `next_val`'s first `ldr` (hint: alignment padding, chapter 07) and
   the load mnemonic for `val`.
5. In `swap`, make `a` and `b` `volatile long *`. Predict whether the order of
   the four memory operations can still be "load, load, store, store".
   Then make them `int *` and check the register widths.

## Takeaways

* A pointer is an `x` register; `*` turns it into `[xN]`; `&` turns a variable
  into a stack slot and an `add xN, sp, #off`.
* Arithmetic on a pointer scales by `sizeof(*p)`: `+1` → `add #8`, difference →
  `asr #3`. Array and pointer syntax compile identically.
* `->` is an immediate offset. Each extra `->` or `*` is another dependent load —
  that is the real cost of linked structures.
* `if (p)` is `cbz`; a NULL dereference is a load from address 0, refused by the MMU.
* Passing `&x` to a function forces the compiler to reload `x` afterwards:
  pointers create aliasing, and aliasing blocks optimisation.
