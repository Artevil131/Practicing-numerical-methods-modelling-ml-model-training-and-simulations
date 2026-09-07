# Chapter 15 — Undefined Behavior and the C Standard

## What you'll be able to do after this chapter

- Say precisely which document defines C, what changed between C11, C17 and C23, and look up any rule in the standard or on cppreference in under a minute.
- Distinguish implementation-defined, unspecified and undefined behavior, and recite the UB catalogue that actually shows up in matrix libraries, parsers and simulations.
- Predict the result of any mixed-type integer expression by running the integer promotions and usual arithmetic conversions by hand — and explain why `-1 < 1u` is false.
- Read `-O2 -S` output and recognise the fingerprints of a compiler exploiting UB: deleted null checks, loops replaced by `brk`, folded arithmetic.
- Build with the full warning set, UBSan, the clang static analyzer and clang-tidy, and read their reports.
- Write portable code: fixed-width types, `CHAR_BIT`, explicit endianness, `offsetof`, `_Static_assert`, `_Alignas`, `restrict` used correctly, `volatile` used only where it means something.

## Why this matters for ML / numerics / sims

Every index expression in a matrix library — `data[i * cols + j]` — is integer arithmetic on mixed types, and every one of them is a place where signed overflow, a signed/unsigned comparison, or a one-past-the-end pointer turns a correct algorithm into a program with no defined meaning. UB doesn't crash reliably: at `-O0` the program "works", at `-O2` the compiler deletes your bounds check because it proved (from your own UB) that it can never fail, and the MLP trains to 10% accuracy for no visible reason. Float-to-int conversions in quantisation, aliasing between `float*` and `uint32_t*` in a bit-hacked `fast_inverse_sqrt`, `restrict` on an in-place N-body update, a `volatile` flag that isn't actually atomic in the simulation loop — these are pro-level bugs and they are all in this chapter. Knowing the standard is what separates "it works on my machine at -O0" from a library you can ship.

---

## 1. What the C standard is

C is defined by ISO/IEC 9899. The versions that matter:

| Name | Year | `__STDC_VERSION__` | Free draft | What it added |
|------|------|--------------------|------------|---------------|
| C89/C90 | 1989/1990 | (undefined) | — | The baseline K&R describes |
| C99 | 1999 | `199901L` | N1256 | `//` comments, `long long`, `<stdint.h>`, `<stdbool.h>`, VLAs, `inline`, designated initializers, compound literals, `restrict`, mixed declarations |
| **C11** | 2011 | `201112L` | **N1570** | `_Static_assert`, `_Alignas/_Alignof`, `_Noreturn`, `_Generic`, `_Thread_local`, `<stdatomic.h>`, `<threads.h>` (optional), anonymous structs/unions, `aligned_alloc`; VLAs became optional |
| C17/C18 | 2017/2018 | `201710L` | N2176 | Bug-fix release only. Zero new features. Same as C11 for our purposes. |
| **C23** | 2024 | `202311L` | N3220 | `nullptr`, `constexpr` (objects only), `typeof`, `[[attributes]]`, `#embed`, `bool/true/false` as keywords, `static_assert` keyword, `_BitInt(N)`, binary literals `0b101`, digit separators `1'000`, `#elifdef`, `= {}` empty init, `unreachable()`, two's complement mandated, removed K&R function definitions |

The standard is not free, but the final drafts (N1570 for C11, N3220 for C23) are byte-for-byte what you need: search "N1570 pdf". Citations in this course look like **6.5.7p3** = section 6.5.7 (Bitwise shift operators), paragraph 3. **Annex J.2** is the informative list of every UB in the language — 200+ items — and is the single most useful appendix to skim once.

```sh
# Which version does the compiler think it's speaking?
echo __STDC_VERSION__ | cc -std=c11 -E -P -   # 201112L
echo __STDC_VERSION__ | cc -std=c17 -E -P -   # 201710L
echo __STDC_VERSION__ | cc -std=c23 -E -P -   # 202311L   (Apple clang 21 accepts -std=c23 and -std=c2x)
```

This course compiles with `-std=c11`. Without `-std`, Apple clang defaults to `gnu17` — C17 plus GNU extensions (`##__VA_ARGS__`, statement expressions, `typeof` as an extension). Extensions are fine to use knowingly; `-Wpedantic` tells you when you're using one.

Python equivalent: Python has one implementation that *is* the spec. C has a spec and a dozen implementations (clang, gcc, MSVC, tcc, icc), and the spec deliberately leaves gaps so each can be fast on its hardware. The gaps are this chapter.

## 2. Three kinds of "the standard doesn't say"

The standard uses three distinct terms (3.4.1, 3.4.3, 3.4.4). They are not synonyms.

| Term | Definition (3.4.x) | Compiler must document? | Program still meaningful? | Example |
|------|--------------------|-------------------------|---------------------------|---------|
| **Implementation-defined** | Unspecified behavior where each implementation documents how the choice is made | **Yes** | Yes | `sizeof(int)`, whether `char` is signed, `>>` on negative values, `CHAR_BIT`, how `printf("%p")` looks |
| **Unspecified** | Two or more possibilities; the standard imposes no further requirements on which is chosen in any instance | No | Yes (but you don't know which) | Order of evaluation of `f() + g()`; order of argument evaluation; whether two identical string literals share storage; padding bytes' values |
| **Undefined** | Behavior for which this document imposes **no requirements** | No | **No** — the entire execution has no meaning | Signed overflow, out-of-bounds access, null deref, `i = i++`, data race, strict aliasing violation |

The key sentence is 3.4.3 note 2: "Possible undefined behavior ranges from ignoring the situation completely with unpredictable results, to behaving during translation or program execution in a documented manner characteristic of the environment, to terminating a translation or execution." *Ignoring the situation completely* is what optimizers do — they assume UB never happens and derive facts from that assumption. Section 7 shows this happening in real assembly.

A subtle consequence: UB is not local in time. If a program will execute UB at step 1000, the standard places no requirement on steps 1–999 either. Compilers do actually exploit this ("time travel"): a `printf` before the UB line can be removed if the compiler can prove the UB is reachable.

```c
int x = INT_MAX;
printf("before\n");
x = x + 1;              // UB: 6.5p5 — result not representable
printf("after %d\n", x);
// At -O0 you'll see "before / after -2147483648". At -O2 you might. Neither is guaranteed.
// -fsanitize=undefined: "runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'"
```

## 3. The UB catalogue — what beginners hit and pros still hit

Each entry: what it is, the clause, how it surfaces, and the defined alternative. Sections 4–6 expand the ones with more to say.

### 3.1 Signed integer overflow (6.5p5)

`int` arithmetic that produces a value outside `[INT_MIN, INT_MAX]` is UB. This includes `+ - *`, unary `-INT_MIN`, `INT_MIN / -1`, `INT_MIN % -1`, and `abs(INT_MIN)`. **Unsigned** arithmetic is *defined* to wrap modulo 2ⁿ (6.2.5p9) — that's the escape hatch.

```c
#include <limits.h>
int a = INT_MAX, b = 1;
int c = a + b;                              // UB
unsigned u = UINT_MAX; u += 1;              // defined: u == 0
// Defined overflow check, before the operation:
if (b > 0 ? a > INT_MAX - b : a < INT_MIN - b) { /* would overflow */ }
// Or the compiler builtin (clang/gcc): returns true on overflow, result wraps into *r
int r; if (__builtin_add_overflow(a, b, &r)) { /* would overflow */ }
// C23: <stdckdint.h> ckd_add(&r, a, b) is the portable spelling.
```

Where it bites in ML code: `int n = rows * cols;` with `rows = cols = 50000` overflows `int` (2.5e9 > 2.1e9). Use `size_t` for sizes and indices (`size_t n = (size_t)rows * cols;`), and check the multiply if the dimensions come from a file.

`-fwrapv` makes signed overflow wrap (a language extension; the loop in section 7 stops being deleted). `-ftrapv` traps. Neither is standard C; use them as a debugging aid, not a fix.

### 3.2 Shifts (6.5.7)

- Shift count negative or `>= width` of the *promoted* left operand: UB (6.5.7p3). `1 << 32` on a 32-bit `int` is UB. `1u << 31` is fine. `(uint64_t)1 << 32` is fine.
- Left shift of a negative value: UB (6.5.7p4). `-1 << 1` is UB in C11 (C23 makes it defined, two's complement).
- Left shift of a positive signed value whose result doesn't fit: UB. `1 << 31` on 32-bit `int` is UB (result 2³¹ not representable) — and this is exactly what `INT_MIN` looks like when people write it by hand. Write `INT_MIN` or `(int)0x80000000u`... or just `1u << 31`.
- Right shift of a negative signed value: **implementation-defined** (6.5.7p5) — arithmetic (sign-extending) on every compiler you'll meet, but not guaranteed.

```c
unsigned bit(unsigned k) { return 1u << k; }         // UB if k >= 32 — check it
uint64_t mask(unsigned k) { return k >= 64 ? ~0ull : (1ull << k) - 1; }   // the guard is not optional
// UBSan: "shift exponent 32 is too large for 32-bit type 'int'"
```

Note the promotion: in `1 << k`, the `1` is `int`, so the width that matters is 32 even if you assign the result to a `uint64_t`. Cast first: `(uint64_t)1 << k`.

### 3.3 Out-of-bounds access (6.5.6p8, 6.5.2.1)

Reading or writing `a[n]` on an array of `n` elements is UB — not "reads garbage", *UB*. So is forming the pointer `a + n + 1` (one past one-past-the-end). Forming `a + n` is fine and comparing against it is fine (see 3.10). Off-by-one in `<=` loops and `strlen`-without-`+1` allocations are the classic instances (`../04_arrays_and_strings/lesson.md`, `../06_dynamic_memory/lesson.md`). `-fsanitize=address` catches heap/stack/global OOB at runtime; `-fsanitize=undefined` catches OOB on arrays whose bound is known at compile time (`int a[4]; a[i]`).

### 3.4 Null pointer dereference (6.5.3.2p4)

Dereferencing a null pointer is UB. On macOS/Linux it usually faults (address 0 is unmapped) — but "usually" is doing a lot of work: `p->field` where `field` is at offset 4 GiB won't fault, and the compiler may reorder your check after the deref and then delete it (section 7). Check before you deref, always.

### 3.5 Uninitialized reads (6.3.2.1p2, 6.7.9p10)

Automatic variables without an initializer have **indeterminate** values. Reading one is UB in the specific case where the variable's address is never taken and it could have been in a register (6.3.2.1p2) — and clang treats it as producing an arbitrary value regardless, which is what lets it delete the `sum += i` loop from `int sum; for (...) sum += i;` entirely. `malloc` memory is indeterminate too; `calloc` zeroes. Static and global variables are zero-initialized (6.7.9p10). Padding bytes in a struct are unspecified even after `= {0}` — use `memset` if you're going to hash or `memcmp` a struct.

```c
double acc;                     // indeterminate
for (...) acc += x[i];          // UB; -Wuninitialized / -Wsometimes-uninitialized catch simple cases
double acc = 0.0;               // fix. MemorySanitizer (Linux only) catches the rest.
```

### 3.6 Strict aliasing (6.5p7) — section 4.

### 3.7 Unsequenced modifications (6.5p2) — section 5.

### 3.8 Data races (5.1.2.4p25)

Two threads access the same memory location, at least one is a write, neither is atomic, and nothing orders them: UB, the whole program. Not "you might read a stale value" — UB. `volatile` does **not** fix this (section 12). Chapter 16 (`../16_concurrency_pthreads_and_atomics/lesson.md`) is about the fix: mutexes, `<stdatomic.h>`, and ThreadSanitizer to find them.

### 3.9 Misaligned access (6.3.2.3p7)

Converting a pointer to a type with stricter alignment than the object actually has is UB — even before you dereference. `uint32_t *p = (uint32_t *)(buf + 1);` on a `char buf[]` is UB. On x86 it "works" (slowly); on arm64 plain loads tolerate misalignment but NEON loads and atomics can fault, and the compiler may emit an instruction that assumes alignment. The defined tool is `memcpy`:

```c
uint32_t v;
memcpy(&v, buf + 1, sizeof v);     // defined for any alignment; clang emits one unaligned ldr at -O2
```

`_Alignof(T)` tells you the requirement (section 13). Also, this is why `malloc` returns memory aligned for every fundamental type (7.22.3p1) and why `struct { char c; double d; }` has 7 padding bytes.

### 3.10 Invalid pointer arithmetic and the one-past-the-end rule (6.5.6p8, 6.5.8p5, 6.5.9p6)

Pointer arithmetic is defined only within an array object *and* to the element one past its end. So:

```c
int a[4];
int *end = a + 4;      // OK: one past the end. May be compared and used in arithmetic, NOT dereferenced.
int *p   = a + 5;      // UB: already, just by computing it
int *q   = a - 1;      // UB: before the beginning — the classic "sentinel at index -1" trick is UB
for (int *it = a; it < end; ++it) ...   // the idiomatic defined loop; this is what C++ iterators formalise
```

Comparing pointers with `< > <= >=` from *different* arrays is UB (6.5.8p5); `==`/`!=` between any two valid pointers is fine (6.5.9p6). Subtracting pointers from different arrays: UB, and the result of a valid subtraction is `ptrdiff_t` (6.5.6p9) — if the array is bigger than `PTRDIFF_MAX` elements, UB. A `double *` in and `char *` out of the same object is fine as long as you convert back to the original type before dereferencing.

Also: a pointer to freed memory has an indeterminate value (6.2.4p2) — even *comparing* it (`if (p == old_p)`) is technically UB, not just dereferencing. Set freed pointers to `NULL`.

### 3.11 `restrict` violations (6.7.3p8) — section 6.

### 3.12 Others you will meet

| UB | Clause | Typical spelling |
|----|--------|------------------|
| Division/modulo by zero | 6.5.5p5 | `x % n` with `n == 0` from bad input |
| `float`→`int` conversion out of range (also NaN) | 6.3.1.4p1 | `(int)(1e10)`, `(int)NAN`, `(uint8_t)(x * 255)` with `x > 1` — clamp first |
| Calling a function through a pointer of the wrong type | 6.5.2.2p9 | `qsort` comparator declared `int(const int*, const int*)` |
| Modifying a string literal | 6.4.5p7 | `char *s = "abc"; s[0] = 'x';` |
| Modifying a `const` object through a cast | 6.7.3p6 | `*(int *)&const_var = 1` |
| Reaching `}` of a non-void function and *using* the value | 6.9.1p12 | missing `return` (`-Wreturn-type`) |
| Passing the wrong type to `printf`/`scanf` | 7.21.6.1p9 | `printf("%d", (long)x)`, `printf("%lu", (size_t)x)` is fine on macOS but write `%zu` |
| `va_arg` with the wrong type | 7.16.1.1p2 | variadic `double` read as `float` (there is no `float` in varargs — promoted) |
| Object accessed after lifetime ends | 6.2.4p2 | returning `&local`, using a VLA after its block |
| Infinite loop with no side effects and non-constant condition | 6.8.5p6 | compilers may assume it terminates — see section 7 |
| Same identifier declared with incompatible types in two TUs | 6.2.7p2 | `extern int n;` in one file, `long n;` in another — no linker check |
| `longjmp` into a function that has returned | 7.13.2.1p2 | section 12 |
| Two `#include`s of `<assert.h>` with different `NDEBUG` — fine actually; but `NDEBUG` toggled mid-file and `assert` with side effects | 7.2p1 | `assert(vec_push(...) == 0)` |

Annex J.2 has the complete list. Read it once; you'll recognise a dozen you've written.

## 4. Strict aliasing and type punning

**The rule** (6.5p7): an object may only be accessed through an lvalue of (a) its own type (qualified or signed/unsigned variants), (b) an aggregate/union type containing that type, or (c) a **character type**. Everything else is UB. The consequence: the compiler may assume an `int *` and a `float *` never point to the same memory, and reorder or cache loads/stores accordingly.

```c
int alias(int *a, float *b) {
    *a = 1;
    *b = 2.0f;     // cannot alias *a under 6.5p7
    return *a;     // compiler returns the constant 1 without reloading
}
```

Real `cc -O2 -std=c11 -S` output on arm64:

```asm
_alias:
    mov  w8, #1
    str  w8, [x0]           ; *a = 1
    mov  w8, #1073741824    ; 0x40000000 == bits of 2.0f
    str  w8, [x1]           ; *b = 2.0f
    mov  w0, #1             ; return 1  <-- no reload of *a
    ret
```

Compare `alias_ok(int *a, int *b)` with the same body: the compiler emits `ldr w0, [x0]` at the end because two `int *` *may* alias. If you call `alias((int*)&x, &x)` — i.e. `a` and `b` point at the same bytes — the function returns 1 but memory holds `0x40000000`. Your program is now lying to you.

**The classic violation** — reading float bits through an integer pointer:

```c
uint32_t pun_bad(float f) { return *(uint32_t *)&f; }    // UB (6.5p7); -Wstrict-aliasing=2 warns
```

Ironically clang emits the ideal `fmov w0, s0` for this — the UB is invisible *here* and shows up three functions away when an optimizer pass assumes the float and the int are unrelated.

**The three defined ways to reinterpret bits:**

```c
// 1. memcpy — the portable, standard, and (at -O2) free way. Compiles to fmov.
uint32_t pun_memcpy(float f) { uint32_t u; memcpy(&u, &f, sizeof u); return u; }

// 2. Union — defined in C (6.5.2.3p3 + footnote 95: "type punning" is explicitly allowed
//    when reading a member other than the last one written). NOT allowed in C++.
uint32_t pun_union(float f) { union { float f; uint32_t u; } x = { .f = f }; return x.u; }

// 3. char* / unsigned char* — always allowed to alias anything (6.5p7 last bullet).
//    Good for byte-wise serialisation, awkward for whole-value reinterpretation.
unsigned char *bytes = (unsigned char *)&f;
```

`_Static_assert(sizeof(float) == sizeof(uint32_t), "...")` before either — see section 13.

**`-fno-strict-aliasing`**: tells the compiler to assume any pointer may alias any other. The Linux kernel builds with it. It costs performance in numeric loops (every store to a `float*` forces reloading every `int*` counter) and it is not standard C — your code is still UB, you've just told *this* compiler not to exploit it. Use it as a diagnostic: if behavior changes with the flag, you have an aliasing bug.

Why it matters for numerics: a matmul kernel `void mm(float *C, const float *A, const float *B, size_t n)` where the `size_t` counters live in memory (they don't, but imagine a struct field) can only be vectorised because the compiler knows a `float` store can't change a `size_t`. Strict aliasing is what lets it hoist the loop bound.

## 5. Sequence points and unsequenced modifications

C11 replaced the C99 "sequence point" vocabulary with **sequenced before / unsequenced / indeterminately sequenced** (5.1.2.3p3). The rule that bites (6.5p2): *if a side effect on a scalar object is unsequenced relative to either a different side effect on the same object or a value computation using the value of the same object, the behavior is undefined.*

```c
int i = 0;
i = i++;            // UB: two side effects on i, unsequenced
a[i] = i++;         // UB: value computation of i (for a[i]) unsequenced with the side effect
f(i, i++);          // UB: argument evaluations are unsequenced relative to each other (6.5.2.2p10)
printf("%d %d", i, ++i);   // UB — the classic interview trap; the answer is "no answer"
x = f() + g();      // NOT UB: unspecified *order*, but f and g are indeterminately sequenced (each completes before the other starts). Just don't depend on the order.
```

Where sequencing *is* guaranteed: `;` at end of a full expression, `&&`, `||`, `?:`, `,` (the comma operator — not commas between arguments), function call (all arguments are sequenced before the body). So `i++, i++` (comma) is fine; `f(i++, i++)` is not.

```
-Wall includes -Wunsequenced (clang) / -Wsequence-point (gcc):
   warning: multiple unsequenced modifications to 'i' [-Wunsequenced]
```

Python equivalent: Python defines left-to-right evaluation for everything. C deliberately doesn't, so the compiler can schedule loads and stores freely.

## 6. `restrict`

`restrict` (6.7.3p8, C99) is a promise *from you to the compiler*: during the lifetime of this pointer, if the object it points to is modified, every access to that object goes through this pointer (or one derived from it). Violating the promise is UB. In exchange the compiler can assume no aliasing and vectorise.

```c
// Without restrict: the compiler must assume out[i] might alias a[i+1] and reload a each iteration.
void axpy(size_t n, float alpha, const float *restrict x, float *restrict y) {
    for (size_t i = 0; i < n; i++) y[i] += alpha * x[i];       // vectorises to 4-wide NEON fmla
}
axpy(n, 2.0f, v, v);        // UB: x and y alias and y is written. Compiles. Wrong at -O2 in creative ways.
```

Rules of thumb:
- `restrict` on a `const` pointer whose target is never modified is harmless but meaningless.
- `memcpy`'s declaration is `void *memcpy(void *restrict dst, const void *restrict src, size_t n)` — that's why overlapping `memcpy` is UB and `memmove` exists.
- For your N-body sim: `void step(Body *restrict out, const Body *restrict in, size_t n)` is right (double-buffered). An in-place `step(bodies, bodies, n)` with those signatures is UB. Don't put `restrict` on in-place kernels.
- You can check whether it did anything: `-Rpass=loop-vectorize -Rpass-missed=loop-vectorize` (`../13_debugging_testing_perf/lesson.md`).

## 7. Why compilers exploit UB — real `-O2 -S` output

The compiler is not malicious. It reasons: "the standard says this can't happen in a valid program, therefore I may assume it doesn't." Three examples, compiled with `cc -O2 -std=c11 -S` on Apple clang 21, arm64. Get your own with:

```sh
cc -O2 -std=c11 -S -o out.s file.c && grep -v '^\s*\.' out.s    # strip assembler directives
# Linux: same command; on x86-64 expect mov/ret instead of ldr/ret.  Or paste into godbolt.org.
```

**Example 1 — a null check deleted.**

```c
int deref_then_check(int *p) {
    int v = *p;                // (a) deref: if p is NULL this is UB
    if (p == NULL) return 0;   // (b) so by the time we get here, p != NULL — check is dead code
    return v;
}
```

```asm
; -O0: 19 instructions including cbnz x8 (the branch on p == NULL)
; -O2:
_deref_then_check:
    ldr  w0, [x0]        ; return *p
    ret                  ; the if is gone
```

This exact pattern was a Linux kernel privilege escalation (CVE-2009-1897, `tun_chr_poll`): a `struct sock *sk = tun->sk;` before `if (!tun) return`. GCC removed the check.

**Example 2 — a loop replaced by a trap.**

```c
int loop_overflow(int n) {
    int count = 0;
    for (int i = n; i >= n; i++)   // "i >= n" is always true unless i overflows — which is UB
        count++;
    return count;
}
```

```asm
_loop_overflow:
    brk  #0x1            ; clang: this loop can never terminate without UB, so it is unreachable
```

Two rules stack here. (1) Signed overflow is UB, so `i >= n` is assumed true forever. (2) 6.8.5p6: an iteration statement whose controlling expression is not a constant and which has no side effects (no I/O, no volatile access, no atomics) **may be assumed to terminate**. Together: an infinite loop of pure computation is UB, and clang turns the function body into `brk` (an arm64 breakpoint trap — on x86 you'd see `ud2`). Compile with `-fwrapv` and you get a real 5-instruction loop back. Neither is a bug in the compiler.

**Example 3 — algebra that's only valid without overflow.**

```c
int mul_shift(int x) { return (x * 2) / 2; }
```

```asm
_mul_shift:
    ret                  ; returns x unchanged: (x*2)/2 == x if x*2 can't overflow
```

With `unsigned x` this would have to actually shift left and right (because `0x80000000u * 2 / 2 == 0`, defined). The signed version is faster *because* overflow is UB. This is the real reason the committee has never made signed overflow wrap: `for (int i = 0; i < n; i++) a[i]` could not be strength-reduced to a pointer increment if `i` could wrap.

The lesson: UB is not "the compiler being pedantic". It is the contract that lets `-O2` exist. Your job is to hold up your end.

## 8. Integer promotions and the usual arithmetic conversions — in full

This is the part of the language most people never learn and then debug for an afternoon. Two rules, applied in order.

**Rule 1 — integer promotions** (6.3.1.1p2). Anywhere an `int` may be used, an object of type `char`, `signed char`, `unsigned char`, `short`, `unsigned short`, `_Bool`, or a bit-field is converted to `int` if `int` can represent all its values, otherwise to `unsigned int`. On every platform you'll use, `int` is 32 bits and all those types are ≤ 16 bits, so **they all become `int`** (signed!), including `unsigned char` and `unsigned short`.

```c
unsigned char a = 200, b = 100;
int s = a + b;              // 300 — both promoted to int first. No wrap at 8 bits.
unsigned char c = a + b;    // 44 — the *assignment* truncates (6.3.1.3p2: modulo 256). Defined.
uint8_t x = 0xFF;
int y = ~x;                 // -256, not 0! ~ applies to the promoted int 0x000000FF -> 0xFFFFFF00
uint8_t z = ~x;             // 0 — truncation saves you, but only in the assignment
unsigned short us = 65535;
int prod = us * us;         // 65535 * 65535 = 4294836225 > INT_MAX -> signed overflow, UB!
```

That last one is real: two `uint16_t` multiplied overflow `int`. Cast one to `unsigned` or `uint32_t` first.

**Rule 2 — usual arithmetic conversions** (6.3.1.8p1), for binary operators `* / % + - < > <= >= == != & ^ |` and `?:`. After promotion, find a *common type*:

1. If either is `long double` → `long double`. Else `double` → `double`. Else `float` → `float`. (Integers convert to the float type.)
2. Otherwise both are integers. If same signedness → the one with greater *rank* (`int < long < long long`).
3. Different signedness, and the **unsigned** type has rank ≥ the signed type → convert to the **unsigned** type.
4. Different signedness, signed type has greater rank and **can represent all values** of the unsigned type → convert to the **signed** type.
5. Otherwise → the unsigned version of the signed type.

```c
-1 < 1u          // rule 3: int vs unsigned int, same rank -> unsigned. -1 becomes 4294967295. FALSE.
-1 < 1ul         // rule 3: unsigned long wins. FALSE.
-1L < 1u         // rule 4: long (64-bit) can hold all unsigned int -> long. -1L < 1L. TRUE.
-1 < 1ull        // rule 3. FALSE.
sizeof(int) > -1 // sizeof yields size_t (unsigned long on macOS). -1 -> huge. FALSE. And -Wsign-compare says so.
```

The rule 3 trap with `size_t`:

```c
for (size_t i = 0; i < n - 1; i++)      // if n == 0, n - 1 == SIZE_MAX: loops "forever" (until it segfaults)
for (size_t i = n - 1; i >= 0; i--)      // always true: infinite loop. -Wtype-limits / -Wextra warns.
for (size_t i = n; i-- > 0; )            // the idiom for counting down with unsigned
for (int i = 0; i < vec_len(v); i++)     // int vs size_t: -Wsign-compare. Make i size_t.
```

The compiler flags that catch this class: `-Wsign-compare` (in `-Wextra`), `-Wsign-conversion` (in `-Wconversion`), `-Wtype-limits`. Turn them on for the matrix library; the noise is the point.

Python equivalent: Python has one `int` with unlimited range and one `float`. There are no promotions. Every `int`/`size_t`/`uint8_t` decision in C is a decision Python made for you.

## 9. `char` signedness and the `<ctype.h>` trap

Whether `char` is signed is **implementation-defined** (6.2.5p15). arm64 macOS and x86-64 Linux: signed. arm Linux (32-bit) and PowerPC: unsigned. `CHAR_MIN` tells you (`<limits.h>`): 0 or -128.

Consequences:
- `char c = 200;` is implementation-defined on signed-char platforms (out of range for signed char) — you get -56.
- `int x = buf[i];` where `buf` is `char[]` and the byte is ≥ 0x80 gives a **negative** `int`. Then `table[x]` indexes at -56 → OOB → UB. The MNIST loader that does `pixels[i] = buf[16 + i]` into a `float` will produce negative pixel values for bytes ≥ 128. Use `unsigned char` for bytes, or `(unsigned char)buf[i]`.
- The `<ctype.h>` functions take an `int` that must be either `EOF` or representable as `unsigned char` (7.4p1). `isdigit(c)` with `char c = -56` is UB (glibc actually indexes a table at `c` and reads before it). Always: `isdigit((unsigned char)c)`.
- `getc`/`fgetc` return `int` for this reason — 256 byte values *plus* `EOF`. Storing the result in a `char` before comparing to `EOF` breaks: byte 0xFF becomes -1 == EOF on signed-char platforms.

```c
// Portable byte type: uint8_t (which is unsigned char under the hood, so it may alias anything).
uint8_t *bytes = ...;
float pixel = bytes[i] / 255.0f;   // 0..1, never negative
```

`-funsigned-char` / `-fsigned-char` force it. Don't; write code that doesn't care.

## 10. `size_t`, `ptrdiff_t`, and mixing them

| Type | Header | Meaning | macOS arm64 |
|------|--------|---------|-------------|
| `size_t` | `<stddef.h>` | result of `sizeof`; unsigned; can index any array | `unsigned long`, 64-bit, `%zu` |
| `ptrdiff_t` | `<stddef.h>` | result of pointer subtraction; signed | `long`, 64-bit, `%td` |
| `ssize_t` | `<sys/types.h>` (POSIX) | `read`/`write` return: size or -1 | `long`, `%zd` |
| `intptr_t`/`uintptr_t` | `<stdint.h>` (optional!) | integer big enough to hold a pointer | 64-bit, `PRIdPTR`/`PRIuPTR` |

Rules:
- Sizes, counts, indices: `size_t`. Not `int`. `int` overflows at 2 GiB — a `float` matrix of 23170×23170 elements (2.1 GB) is a realistic size today.
- Differences and signed offsets (stencil neighbours `i - 1`, `j + 1`): `ptrdiff_t`, or compute in `size_t` with the loop range adjusted (`for (i = 1; i < n - 1; i++)`), never `size_t i - 1` when `i` may be 0.
- Never `int i` vs `size_t n` in a comparison without thinking. `-Wsign-compare`.
- Converting `size_t` → `int` silently truncates (`-Wconversion` / `-Wshorten-64-to-32` catches it). Converting a negative `int` → `size_t` gives a value near 2⁶⁴ — `malloc((size_t)-1)` fails, `a[(size_t)-1]` is UB.
- `printf("%zu", n)`. `%lu` happens to work on macOS/Linux 64-bit and is UB on platforms where `size_t` isn't `unsigned long` (Windows 64-bit).

```c
// Stencil update, the safe shape:
for (size_t i = 1; i + 1 < n; i++)           // i + 1 < n  instead of  i < n - 1  (safe when n == 0)
    out[i] = 0.5 * (in[i - 1] + in[i + 1]);   // i >= 1 so i - 1 can't wrap
```

## 11. Finding UB: the toolchain

There is no single tool. Use all of them, in this order.

### 11.1 Compiler warnings

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
   -Wstrict-aliasing=2 -Wcast-align -Wdouble-promotion -Wformat=2 -Wvla -Wnull-dereference \
   -Werror -O2 -c file.c
```

| Flag | Catches |
|------|---------|
| `-Wall -Wextra` | Unsequenced, uninitialized (simple cases), sign-compare, unused, return-type, type-limits |
| `-Wpedantic` | Use of extensions (`##__VA_ARGS__`, empty struct, zero-size arrays, `typeof` in C11) |
| `-Wshadow` | Inner `i` hiding outer `i` — the nested-loop bug |
| `-Wconversion` | Implicit narrowing: `double`→`float`, `size_t`→`int`, `int`→`uint8_t`. Noisy on purpose. |
| `-Wstrict-aliasing=2` | `*(uint32_t*)&f` patterns (gcc's is better; clang catches the obvious ones) |
| `-Wcast-align` | `(uint32_t *)char_ptr` — the misalignment UB (needs `-Wcast-align=strict` on gcc) |
| `-Wvla` | VLAs, which are optional in C11 and a stack overflow waiting for user input |
| `-Wnull-dereference` | Paths where the compiler can see a null deref |

Note: `-O2` enables more analysis, so some warnings (`-Wsometimes-uninitialized`, `-Wnull-dereference`) only fire at `-O2`. Build both.

### 11.2 UndefinedBehaviorSanitizer

```sh
cc -std=c11 -g -O1 -fsanitize=undefined -fno-sanitize-recover=all -o prog file.c -lm && ./prog
# Linux: identical. Add -fsanitize=address for the memory bugs UBSan doesn't cover.
```

Real output (from a 3-line program):

```
ub2.c:3:66: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
ub2.c:3:85: runtime error: shift exponent 32 is too large for 32-bit type 'int'
ub2.c:3:144: runtime error: index 4 out of bounds for type 'int[4]'
```

What UBSan checks: signed overflow, shifts, division by zero, null deref, misaligned access (`-fsanitize=alignment`), OOB on statically-sized arrays, `float`→`int` out of range (`-fsanitize=float-cast-overflow`, not in the default set — add it for quantisation code), VLA bound ≤ 0, unreachable, `bool` not 0/1, enum out of range, function-pointer type mismatch (`-fsanitize=function`), missing return. What it doesn't: strict aliasing, uninitialized reads (MemorySanitizer, Linux/clang only), data races (TSan), heap OOB (ASan). `-fno-sanitize-recover=all` aborts on the first report instead of continuing — use it in tests. `-fsanitize=undefined,integer` adds *unsigned* overflow reports too, useful when hunting a wrap that is defined but unintended.

### 11.3 Clang static analyzer and clang-tidy

These are not in the Xcode toolchain. Install LLVM from Homebrew:

```sh
brew install llvm                             # ~2 GB; puts tools in $(brew --prefix llvm)/bin, not on PATH
L=$(brew --prefix llvm)/bin
$L/scan-build -o report cc -std=c11 -Wall -c file.c     # static analyzer; writes HTML into report/
$L/scan-view report/2026-*                               # open the report in a browser
$L/clang-tidy -checks='clang-analyzer-*,bugprone-*,cert-*,portability-*' file.c -- -std=c11
# Linux (Debian/Ubuntu): apt install clang-tools clang-tidy; then scan-build / clang-tidy are on PATH.
```

The analyzer does path-sensitive symbolic execution: it finds "pointer freed on line 40 is dereferenced on line 52 when the `if` on line 45 is false" — bugs that don't happen on your test inputs. Typical findings: null deref after a `malloc` that could fail, use-after-free, uninitialized value used in a branch, division by a value that could be zero. It has false positives; read each one. clang-tidy's `bugprone-*` and `cert-*` sets are the checklist of this chapter mechanised: `bugprone-signed-char-misuse`, `bugprone-narrowing-conversions`, `cert-int32-c` (overflow), `cert-exp30-c` (unsequenced).

### 11.4 Two compilers

`-O0` vs `-O2` giving different results is UB with near certainty. gcc vs clang differing is almost always UB (or implementation-defined behavior you relied on). `brew install gcc` and build with `gcc-14` occasionally; on Linux you already have both.

## 12. `volatile`, `setjmp/longjmp`

**`volatile`** (6.7.3p7): every access to a `volatile` object is a *side effect* that the compiler must perform exactly as written — no caching in a register, no elimination, no reordering *relative to other volatile accesses and sequence points*. What it is for:

1. Memory-mapped hardware registers (`*(volatile uint32_t *)0x40021000 = 1;`) — embedded.
2. A variable modified by a signal handler and read by the main loop: `volatile sig_atomic_t stop;` (7.14p2 — `sig_atomic_t` is the one type guaranteed to be readable/writable atomically w.r.t. signals). Chapter 17 uses this.
3. Locals modified between `setjmp` and `longjmp` (below).
4. Defeating the optimizer in benchmarks (`../13_debugging_testing_perf/lesson.md`).

What it is **not**: a thread-synchronisation tool. `volatile int flag` shared between threads is still a data race (UB, 5.1.2.4p25); `volatile` doesn't make the access atomic (a `volatile uint64_t` on a 32-bit machine is two stores), doesn't insert memory barriers, and doesn't stop the CPU from reordering. Java's `volatile` does those things; C's does not. Use `_Atomic` (`../16_concurrency_pthreads_and_atomics/lesson.md`).

```c
volatile int v = 0;
v = 1; v = 2;        // both stores emitted; without volatile the first is dead and removed
int x = v + v;       // two loads emitted
```

**`setjmp`/`longjmp`** (7.13): non-local goto. `setjmp(buf)` saves the register state and returns 0; a later `longjmp(buf, val)` returns *again* from that `setjmp` with `val` (or 1 if `val == 0`). Used for exception-like error recovery in parsers and for `assert`-style test harnesses that recover from failures. The caveats are all UB traps:

- `setjmp` may only appear as the whole controlling expression of `if`/`switch`/`while`, optionally compared to a constant, or as an expression statement (7.13.1.1p4-5). `int r = setjmp(buf);` is technically UB (works everywhere, but).
- The function containing `setjmp` must still be active when `longjmp` is called (7.13.2.1p2). Jumping into a returned frame: UB.
- Non-`volatile` automatic variables modified between `setjmp` and `longjmp` have **indeterminate** values after the jump (7.13.2.1p3). The register copy saved by `setjmp` gets restored, and the memory copy may or may not have been written. Declare them `volatile`.
- `longjmp` out of a signal handler: only if the signal interrupted an async-signal-safe point; use `sigsetjmp`/`siglongjmp` (POSIX) to restore the signal mask.
- No destructors in C, so `longjmp` over a `malloc` leaks and over a `pthread_mutex_lock` deadlocks. Every resource acquired between `setjmp` and `longjmp` must be tracked.

```c
#include <setjmp.h>
static jmp_buf on_error;
static void parse(const char *s) { if (!s) longjmp(on_error, 2); /* ... */ }
int main(void) {
    volatile int attempts = 0;               // volatile: modified between setjmp and longjmp
    if (setjmp(on_error) == 0) {
        attempts = attempts + 1;
        parse(NULL);                         // longjmps back to the if with value 2
    } else {
        printf("recovered after %d attempt(s)\n", attempts);   // 1 — guaranteed only because volatile
    }
}
```

## 13. Portable-code toolkit

**Fixed-width types** (`<stdint.h>`, 7.20): `int8_t … int64_t`, `uint8_t … uint64_t` are *optional* in the standard but present everywhere you'll be; `int_least32_t`, `int_fast32_t`, `intmax_t` are required. Use `uint8_t` for bytes, `int32_t`/`int64_t` for file formats and network protocols, `uint32_t` for bit manipulation, `uint64_t` for hashes/RNG state, and plain `int`/`size_t`/`double` for everything else — over-using fixed widths for loop counters costs performance on some targets. Print with `<inttypes.h>`: `printf("%" PRIu64 "\n", x)`.

**`CHAR_BIT`** (`<limits.h>`): bits in a byte. Is 8 on POSIX (required by POSIX, not by ISO C). Write `sizeof(x) * CHAR_BIT` for bit widths rather than `* 8` — free portability and it documents intent.

**Endianness** is not something the standard names; it's a property of how `uint32_t` is laid out in memory. arm64 macOS and x86-64: little-endian. Network protocols and MNIST IDX headers: big-endian. Detect at runtime with `memcpy`, or handle without detecting by assembling bytes arithmetically (the only fully portable way):

```c
uint32_t be32(const uint8_t *p) {      // read big-endian regardless of host endianness
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
// The casts to uint32_t before shifting are REQUIRED: p[0] promotes to int and (int)0xFF << 24 overflows -> UB.
void put_le64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
// C23 adds <stdbit.h> and __STDC_ENDIAN_NATIVE__; clang/gcc have __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ as a macro, and htonl()/ntohl() are POSIX.
```

**`offsetof(type, member)`** (`<stddef.h>`, 7.19): byte offset of a member. Use it for (a) documenting/checking struct layout in a file format header, (b) the container-of idiom (`(Node *)((char *)list_ptr - offsetof(Node, link))`), (c) writing struct members by offset in a generic serialiser. Never compute offsets by hand or assume `sizeof(struct)` == sum of members.

```c
struct Hdr { uint32_t magic; uint16_t ver; uint8_t flags; uint64_t nbytes; };
// offsetof: magic 0, ver 4, flags 6, nbytes 8 (padding at 7), sizeof 16
_Static_assert(offsetof(struct Hdr, nbytes) == 8, "layout changed — bump the format version");
```

**`_Static_assert(expr, "msg")`** (6.7.10, C11): compile-time check; `expr` must be an integer constant expression. C23 spells it `static_assert` and makes the message optional (C11 has the `static_assert` macro in `<assert.h>`). Use it for every layout/size assumption: `_Static_assert(sizeof(float) == 4, "")`, `_Static_assert(sizeof(size_t) >= 8, "64-bit only")`, `_Static_assert(N_OPS == sizeof op_names / sizeof op_names[0], "table out of sync")` — the X-macro tables from `../14_preprocessor_and_c_idioms/lesson.md`.

**`_Alignof(T)` / `_Alignas(N)`** (6.5.3.4, 6.7.5): alignment requirement of a type; request a stronger alignment. `<stdalign.h>` provides `alignof`/`alignas` macros. On arm64 macOS: `_Alignof(double) == 8`, `_Alignof(max_align_t) == 8` (16 on x86-64 Linux — `long double` is 16 bytes there, 8 here), cache line is 128 bytes (`sysctl hw.cachelinesize`; 64 on most x86). Use `_Alignas(64)` or `_Alignas(128)` on per-thread accumulators to avoid false sharing (Chapter 16) and `_Alignas(16)` on SIMD data. `aligned_alloc(alignment, size)` (7.22.3.1) for heap memory; `size` must be a multiple of `alignment`. `posix_memalign` on older systems.

**`_Noreturn`** (6.7.4, C11; `<stdnoreturn.h>` gives `noreturn`): tells the compiler a function never returns (`exit`, `abort`, your `die(fmt, ...)`), which silences "control reaches end of non-void function" after a call to it and lets the optimizer drop the return path. Returning from a `_Noreturn` function: UB. C23 deprecates it in favour of `[[noreturn]]`.

**`_Generic`** recap (6.5.1.1, `../11_function_pointers_and_generics/lesson.md`): compile-time type dispatch. `<tgmath.h>` is built on it. The portable-code use: `#define ABS(x) _Generic((x), int: abs, long: labs, long long: llabs, double: fabs, float: fabsf)(x)` — the right function for the actual type, no promotion surprises.

## 14. Reading the standard and cppreference effectively

**cppreference** (`en.cppreference.com/w/c`): the fastest path. Page names to know: *"Undefined behavior"* (the overview), *"Implicit conversions"* (promotions and usual arithmetic conversions, with the full algorithm), *"Order of evaluation"* (sequencing), *"Object and alignment"*, *"Type"* (compatible/effective types = strict aliasing), *"Arithmetic operators"* (overflow, shifts), *"Pointer arithmetic"*, *"setjmp"*, *"volatile"* (under `cv`), *"C23"* (the change list). Every function page has a "Notes" section listing the traps and a "Defect reports" section — read both.

**The standard draft** (N1570 for C11): structure you need:

| Section | Content |
|---------|---------|
| 3 | Terms — read 3.4.1–3.4.4 (the three behaviors) |
| 5.1.2.3 | Program execution: side effects, sequencing, the as-if rule (p4) |
| 6.2.5 | Types; p9 = unsigned wraps |
| 6.2.6 | Representation of types; trap representations, padding |
| 6.3 | Conversions — 6.3.1.1 promotions, 6.3.1.3 int→int, 6.3.1.4 float→int, 6.3.1.8 usual arithmetic |
| 6.5 | Expressions — p2 sequencing, p5 overflow, p7 aliasing; then one subsection per operator |
| 6.7 | Declarations — 6.7.3 qualifiers (`const`/`volatile`/`restrict`), 6.7.9 initialization |
| 6.8.5 | Iteration — p6 the forward-progress rule |
| 7 | Library — one subsection per header |
| J.1 / J.2 / J.3 | Unspecified / **Undefined** / Implementation-defined — complete lists |

Reading tip: the standard says "shall". A "shall" in a *Constraints* paragraph means the compiler must diagnose violations (compile error). A "shall" anywhere else that your program violates is UB (4p2) — the compiler need not tell you. That single rule explains why so much UB compiles cleanly.

## 15. C23 previews

Apple clang 21 supports these with `-std=c23`. Use them when you control the toolchain; keep `-std=c11` for the course so the habits transfer to any compiler.

```c
#include <stdio.h>
#include <stddef.h>
constexpr int N = 4;                   // a real compile-time constant (can size arrays, unlike const int in C11).
                                        // Objects only — no constexpr functions (unlike C++).
[[nodiscard]] int must_use(void) { return 1; }   // warning if the result is ignored
[[maybe_unused]] static int debug_counter;       // no -Wunused warning
[[deprecated("use v2")]] void old_api(void);
[[noreturn]] void die(const char *msg);           // replaces _Noreturn
[[fallthrough]];                                  // in a switch: "I meant to fall through"
int main(void) {
    typeof(N) x = N;                   // type of an expression; typeof_unqual strips const/volatile. gnu: __typeof__
    int *p = nullptr;                  // type nullptr_t; converts to any pointer, never to int (fixes NULL ambiguity in _Generic and varargs)
    bool ok = true;                    // keywords; <stdbool.h> no longer needed
    static_assert(N == 4);             // message optional
    int arr[N] = {};                   // empty initializer: all zero
    unsigned mask = 0b1010'1010;       // binary literal, digit separator
    static const unsigned char font[] = {
    #embed "font.bin"                  // file contents as a comma-separated byte list, at compile time
    };
    unreachable();                     // <stddef.h>: UB if reached — tells the optimizer, like __builtin_unreachable
    // ... also: _BitInt(256) big; ckd_add() in <stdckdint.h>; stdc_count_ones() in <stdbit.h>
}
```

C23 also removed things: K&R-style `int f(a, b) int a; int b; {}` definitions are gone; `int f()` now means `int f(void)`; trigraphs are gone; two's complement is the only allowed signed representation (so `INT_MIN == -INT_MAX - 1` and `-1 >> 1 == -1` are now guaranteed — but signed *overflow* is still UB).

---

## Gotchas and undefined behavior

This chapter *is* the UB list; here are the ones that survive the list and bite in practice.

- **"It works" is not evidence.** UB that produces the right answer at `-O0`, on your machine, today, is still UB. The compiler upgrade next year is when it stops working. If UBSan or `-O2` vs `-O0` disagree, you have a bug even if every test passes.
- **`int` for sizes.** `rows * cols` overflows `int` at 46341×46341. Use `size_t` and check.
- **`1 << 31`** is UB in C11 (result doesn't fit `int`). `1u << 31` or `INT_MIN`.
- **`(uint32_t)p[0] << 24`** — without the cast, `uint8_t` promotes to `int` and `0xFF << 24` overflows. Every byte-assembly function needs the casts.
- **`uint16_t a, b; a * b`** overflows `int`. Cast to `uint32_t`.
- **`char` from a byte buffer is negative** on this platform for bytes ≥ 0x80. `uint8_t` for bytes.
- **`isdigit(c)` with `char c`** is UB for negative `c`. `isdigit((unsigned char)c)`.
- **`size_t i = n - 1; i >= 0`** loops forever. `for (i = n; i-- > 0;)`.
- **`*(uint32_t *)&f`** violates aliasing. `memcpy`.
- **`(float *)(char_ptr + 3)`** is misaligned UB before you dereference. `memcpy`.
- **`a[i] = i++`** and `f(i, i++)` are unsequenced. One modification per expression.
- **`restrict` on an in-place kernel.** If `in == out` is legal, don't write `restrict`.
- **`volatile` for threads.** It's not atomic and has no ordering guarantees. `_Atomic`.
- **Non-`volatile` locals across `setjmp`.** Indeterminate after `longjmp`.
- **Comparing a freed pointer.** Indeterminate value; UB. `p = NULL` after `free`.
- **`printf("%d", size)`** with a `size_t`. `%zu`. `-Wformat` catches it.
- **Missing `return`** in a non-void function whose value is used. `-Wreturn-type`, and at `-O2` clang may emit `brk` (unreachable) instead of returning.
- **Infinite loop with no side effects** (`while (1);` waiting on a non-volatile flag). Compiler may delete it or assume it terminates. Add a `volatile`/atomic read or a real side effect.

## Common mistakes checklist

- [ ] Every size/count/index is `size_t`; every signed offset is `ptrdiff_t`; no `int` vs `size_t` comparisons (`-Wsign-compare` clean).
- [ ] Every byte buffer is `uint8_t`/`unsigned char`, and every `<ctype.h>` argument is cast to `unsigned char`.
- [ ] Every shift has a compile-time or checked count `< width`, and the left operand is cast to the wide unsigned type before shifting.
- [ ] Every `rows * cols` from external input is checked for overflow before allocation.
- [ ] No pointer casts between unrelated types except to/from `char *`/`void *`; reinterpretation goes through `memcpy`.
- [ ] Every struct written to a file has `_Static_assert`s on `sizeof` and `offsetof`, and explicit-endianness serialisation (or a documented "native endian, not portable" note).
- [ ] `restrict` appears only where aliasing is truly impossible; in-place kernels don't have it.
- [ ] Every `volatile` has a comment saying which of the four legitimate uses it is.
- [ ] Builds clean with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-aliasing=2 -Wcast-align`.
- [ ] Tests run under `-fsanitize=address,undefined -fno-sanitize-recover=all` at least once per commit.
- [ ] `-O0` and `-O2` produce the same output on the test suite.
- [ ] `scan-build` has been run once on the matrix library and every report was read.

## You can move on when...

- You can name the three behavior categories, give two examples of each, and cite the clause for signed overflow, strict aliasing, sequencing, and one-past-the-end.
- You can evaluate `-1 < 1u`, `(uint8_t)0xFF << 24`, `us * us` for `uint16_t us = 65535`, and `~(uint8_t)0xFF` by running the promotion rules on paper and get the standard's answer.
- Given a 10-line function and its `-O2 -S` output, you can point at the instruction that proves the compiler assumed UB didn't happen.
- You can build any of your projects with the full warning set plus UBSan and either it is clean or you can explain each report.
- You've rewritten one existing byte-parsing routine (the MNIST IDX header reader from `../08_file_io/lesson.md` is the natural target) to be endianness-independent, alignment-safe, `uint8_t`-based, with `_Static_assert`s on the header layout.
- You know which flag to reach for when two optimization levels disagree (`-fsanitize=undefined`, then `-fno-strict-aliasing`, then `-fwrapv`) and what each result would tell you.
