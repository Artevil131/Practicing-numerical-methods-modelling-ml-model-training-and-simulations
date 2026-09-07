// example.s — hand-written AArch64 leaf functions for macOS (Apple Silicon).
// Assemble+link with the C driver:
//   cc -Wall -Wextra -std=c11 -O2 -o ex_demo main.c example.s && ./ex_demo
// Conventions used throughout (AAPCS64 / Apple arm64 ABI):
//   x0-x7  : integer/pointer arguments and the integer return value (x0)
//   d0-d7  : floating-point arguments and the FP return value (d0)
//   x9-x15 : caller-saved scratch — free to clobber in a leaf function
//   x30    : link register (return address); `ret` jumps to it
// Every symbol callable from C carries a leading underscore on macOS.

	.text

// ---------------------------------------------------------------------------
// long add3(long a, long b, long c)   -> a + b + c
// ---------------------------------------------------------------------------
	.globl	_add3
	.p2align	2
_add3:
	add	x0, x0, x1		// x0 = a + b
	add	x0, x0, x2		// x0 = (a + b) + c   — result already in x0
	ret				// jump to x30

// ---------------------------------------------------------------------------
// long sum_array(const long *a, size_t n)   -> sum of a[0..n)
// ---------------------------------------------------------------------------
	.globl	_sum_array
	.p2align	2
_sum_array:
	mov	x2, #0			// acc = 0
	cbz	x1, 1f			// n == 0 ? return 0 (forward local label 1)
0:
	ldr	x3, [x0], #8		// x3 = *a; a += 8   (post-index addressing)
	add	x2, x2, x3		// acc += x3
	subs	x1, x1, #1		// n -= 1, set flags
	b.ne	0b			// loop while n != 0 (backward local label 0)
1:
	mov	x0, x2			// return acc
	ret

// ---------------------------------------------------------------------------
// double dot_product(const double *a, const double *b, size_t n)
// ---------------------------------------------------------------------------
	.globl	_dot_product
	.p2align	2
_dot_product:
	fmov	d0, xzr			// acc = 0.0 (move the zero register into d0)
	cbz	x2, 1f
0:
	ldr	d1, [x0], #8		// d1 = *a++
	ldr	d2, [x1], #8		// d2 = *b++
	fmadd	d0, d1, d2, d0		// d0 = d1*d2 + d0   (one fused multiply-add)
	subs	x2, x2, #1
	b.ne	0b
1:
	ret				// result is in d0

// ---------------------------------------------------------------------------
// size_t my_strlen(const char *s)
// ---------------------------------------------------------------------------
	.globl	_my_strlen
	.p2align	2
_my_strlen:
	mov	x1, x0			// remember the start
0:
	ldrb	w2, [x0], #1		// w2 = *s++  (load one byte, zero-extend to 32 bits)
	cbnz	w2, 0b			// keep going until the NUL byte
	sub	x0, x0, x1		// x0 = (one past NUL) - start
	sub	x0, x0, #1		// ... minus the NUL itself
	ret
