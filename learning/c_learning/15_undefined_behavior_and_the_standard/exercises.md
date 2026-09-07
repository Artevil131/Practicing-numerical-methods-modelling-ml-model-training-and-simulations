# Chapter 15 — Exercises

Write each exercise as `ex15_K.c` in this folder (`ex15_1.c`, `ex15_2.c`, ...). Compile with
`cc -Wall -Wextra -Wpedantic -Wshadow -Wconversion -std=c11 -O2 -o ex15_K ex15_K.c -lm` — note the
extra flags; zero warnings. Then rebuild every exercise once with
`cc -std=c11 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -o ex15_K ex15_K.c -lm`
and run it: a chapter about UB is graded by the sanitizer. Where an exercise asks you to *demonstrate*
UB, isolate it in a function compiled with a `-DSHOW_UB` guard so the default build stays clean.

---

### 15.1 — **Three behaviors, catalogued**

Write a program that prints, in a table, one *observed* value for each of these implementation-defined
or unspecified items on your machine, plus the C11 clause you found it under: `sizeof(int)`,
`sizeof(long)`, `sizeof(size_t)`, `CHAR_MIN` (is `char` signed?), `-7 / 2` and `-7 % 2`, `-8 >> 1`,
`(int)3.99`, `(int)-3.99`, the result of `printf("%p", NULL)`, and whether `"abc" == "abc"` (two
identical literals) compares equal. In a comment above each line state which category it is
(implementation-defined / unspecified / defined) and why it is *not* UB.

Example:
```
sizeof(int)   4     impl-defined  6.2.5p5 / 5.2.4.2.1
CHAR_MIN      -128  impl-defined  6.2.5p15  (char is signed here)
-7 / 2        -3    defined       6.5.5p6 (truncation toward zero since C99)
```

<details><summary>Hint</summary>
Annex J.3 lists every implementation-defined item with its clause. Truncation toward zero for
integer division has been *defined* since C99 — before that it was implementation-defined.
</details>

### 15.2 — **Promotion calculator, by hand then by machine**

For each expression below, first write your predicted type and value as a comment (apply 6.3.1.1
then 6.3.1.8 step by step in the comment), then print the actual value with the correct format
specifier. Use `_Generic` to also print the *type name* of each expression to check your prediction:
`-1 < 1u`, `-1L < 1u`, `-1 < 1ul`, `(uint8_t)200 + (uint8_t)100`, `~(uint8_t)0xFF`,
`(uint16_t)65535 * (uint16_t)65535` (predict, then explain why you must NOT execute this one as
written), `1u << 31 >> 31`, `'A' + 1`, `sizeof('A')`, `1 ? 1u : -1`, `(char)-1 == 255`.

Example:
```
-1 < 1u           pred: unsigned int, 0   actual: 0   type: unsigned int
~(uint8_t)0xFF    pred: int, -256          actual: -256 type: int
```

<details><summary>Hint</summary>
`_Generic((expr), int: "int", unsigned: "unsigned int", long: "long", unsigned long: "unsigned long", default: "other")`.
For `sizeof('A')`, remember that character constants are `int` in C (they're `char` in C++).
</details>

### 15.3 — **Watch the optimizer exploit you**

Write four functions, each containing exactly one UB pattern: a deref-before-null-check, a signed
loop counter whose termination depends on overflow, `(x * 2) / 2` on `int`, and a function that
reads an uninitialized local. Compile with `cc -O2 -std=c11 -S` and paste the arm64 (or x86-64)
assembly of each function into your source as a comment, then annotate *which instruction proves*
the compiler assumed the UB never happens. Recompile with `-O0` and with `-fwrapv` and note which
functions change. Finally rewrite each function correctly and confirm the assembly is now what you
expect.

<details><summary>Hint</summary>
`grep -v '^\s*\.' out.s` strips assembler directives. If clang turns a function into a single `brk`
or `ud2`, it proved the function body is unreachable without UB — that counts as a "proof
instruction". For the uninitialized read, look for the *absence* of a load.
</details>

### 15.4 — **Bit-safe utilities**

Implement, with no UB for *any* input value (verify with UBSan and a brute-force loop over edge
cases: 0, 1, 31, 32, 63, 64, `INT_MIN`, `INT_MAX`, `-1`): `uint64_t mask_low(unsigned k)` (lowest
`k` bits set, `k` in `[0, 64]`), `uint32_t rotl32(uint32_t x, unsigned r)` (any `r`),
`int32_t sat_add(int32_t a, int32_t b)` (saturating), `int32_t abs_or_max(int32_t x)` (`abs` that
returns `INT32_MAX` for `INT32_MIN` instead of UB), `bool mul_fits_size(size_t a, size_t b, size_t *out)`
(the `rows * cols` check for your matrix library), and `int32_t sra(int32_t x, unsigned k)` (arithmetic
right shift that does NOT rely on the implementation-defined behavior of `>>` on negatives).

Example:
```
mask_low(0)=0x0  mask_low(64)=0xffffffffffffffff
rotl32(0x80000001, 33)=0x3
sat_add(INT32_MAX, 1)=2147483647   sat_add(INT32_MIN, -1)=-2147483648
mul_fits_size(1<<33, 1<<31) -> 0 (overflow)
sra(-8, 1)=-4
```

<details><summary>Hint</summary>
`rotl32`: reduce `r &= 31` first and handle `r == 0` separately (`x >> 32` is UB). `sra`: do the
shift on the unsigned representation and fix up the sign with a mask, or use
`x < 0 ? ~(~x >> k) : x >> k` — check that this identity is UB-free. `mul_fits_size`:
`a != 0 && b > SIZE_MAX / a`, or `__builtin_mul_overflow`.
</details>

### 15.5 — **Aliasing lab**

Write `float_bits_bad` (`*(uint32_t *)&f`), `float_bits_memcpy`, `float_bits_union`, and a fourth
version using an `unsigned char *` loop. Compile each with `-O2 -S` and confirm all four produce the
same `fmov` (or `movd`). Then write a function that *breaks* under strict aliasing: it takes
`(int *a, float *b)`, writes both, returns `*a`, and is called with both pointers aimed at the same
variable. Show its result at `-O0`, `-O2`, and `-O2 -fno-strict-aliasing`. Finish with
`-Wstrict-aliasing=2` output pasted in a comment.

Example:
```
bits(1.0f) via memcpy=0x3f800000 union=0x3f800000 bytes=0x3f800000
alias_break: -O0 -> 1073741824   -O2 -> 1   -O2 -fno-strict-aliasing -> 1073741824
```

<details><summary>Hint</summary>
To defeat inlining (which can hide the aliasing effect), mark the function
`__attribute__((noinline))` or put it in a second translation unit. The union route is legal C11
(6.5.2.3 footnote 95) and illegal C++ — say so in a comment for when you get to the C++ course.
</details>

### 15.6 — **Sequencing and `restrict`**

Part A: write six expressions, three UB by 6.5p2 and three that *look* similar but are well-defined
(one using the comma operator, one using `&&`, one using a function-call boundary). Compile with
`-Wall`; paste the `-Wunsequenced` warnings into a comment and explain why the defined three don't
warn. Part B: write `void axpy(size_t n, float a, const float *restrict x, float *restrict y)` and
`void axpy_inplace(size_t n, float a, float *y)` (no `restrict`). Compile with
`-O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize` and record which vectorised. Then call
`axpy(n, 2.0f, v, v)` in a `-DSHOW_UB` block and explain what you observe and why the output is not
guaranteed to be anything.

<details><summary>Hint</summary>
`axpy_inplace` typically still vectorises because there is only one pointer. Make it interesting: a
second version `void stencil(size_t n, const float *in, float *out)` where `out[i] = in[i-1] + in[i+1]`
— with and without `restrict` the remarks differ (runtime alias check vs none).
</details>

### 15.7 — **Portable layout: `offsetof`, `_Alignof`, `_Static_assert`**

Define `struct Sample { uint8_t tag; double value; uint16_t id; float weight; }`. Print
`sizeof` and `offsetof` of each member and `_Alignof(struct Sample)`; draw the layout with padding
bytes as an ASCII diagram in a comment. Reorder the members to minimise `sizeof` and print it again.
Add `_Static_assert`s pinning the final layout. Then implement the container-of idiom: a
`struct Link { struct Link *next; }` embedded in `struct Sample`, a function that walks a list of
`Link*` and recovers each `Sample*` with `offsetof`, and show it works with three samples.

Example:
```
Sample: sizeof=24 align=8  tag@0 value@8 id@16 weight@20   (7 pad after tag, 2 pad after id)
Packed: sizeof=16          value@0 weight@8 id@12 tag@14   (1 pad at end)
```

<details><summary>Hint</summary>
`(struct Sample *)((char *)link - offsetof(struct Sample, link))` — the `char *` step is what makes
the arithmetic defined. Members sorted by decreasing alignment minimise padding.
</details>

### 15.8 — **Overflow-proof matrix indexing** (ML)

Your matrix library (`../09_multi_file_projects_and_make/lesson.md`) computes `i * cols + j` in
hundreds of places. Write `mat_alloc(size_t rows, size_t cols)` that refuses (returns `NULL`,
sets `errno = EOVERFLOW`) if `rows * cols` or `rows * cols * sizeof(float)` would overflow `size_t`;
`mat_idx(const Matrix *m, size_t i, size_t j)` that asserts bounds; and a *signed* variant
`mat_get_offset(const Matrix *m, size_t i, size_t j, ptrdiff_t di, ptrdiff_t dj)` for stencil access
that returns a boundary value when `i + di` or `j + dj` leaves the matrix — with no UB for any
`di`/`dj`, including `PTRDIFF_MIN`. Test with `rows = SIZE_MAX / 2`, `rows = 3, cols = 4` and
offsets `-1, 0, +1, PTRDIFF_MIN, PTRDIFF_MAX`. Prove with UBSan that nothing wraps unintentionally.

Example:
```
mat_alloc(SIZE_MAX/2, 3) -> NULL (EOVERFLOW)
mat_get_offset(m, 0, 0, -1, 0) -> boundary (0.0)
mat_get_offset(m, 2, 3, PTRDIFF_MAX, 0) -> boundary
```

<details><summary>Hint</summary>
Never form `i + di` when `di < 0` and `i < (size_t)-di` — compare first, then add. For `di > 0`,
compare `(size_t)di` against `rows - i`. Converting `PTRDIFF_MIN` with unary minus is UB
(`-PTRDIFF_MIN` doesn't fit); handle it via `(size_t)0 - (size_t)di` on the unsigned side.
</details>

### 15.9 — **Portable checkpoint format** (sims / ML)

Design a binary checkpoint for an N-body state or MLP weights: a fixed 32-byte header
(`magic[4]`, `uint16_t version`, `uint8_t endianness_marker`, `uint8_t float_size`,
`uint64_t n_elements`, `uint64_t timestamp`, `uint64_t reserved`) followed by `n_elements`
`float`s in **little-endian** byte order regardless of host. Implement `ckpt_write` and `ckpt_read`
using only `uint8_t` buffers, `memcpy`, and arithmetic byte assembly — no pointer casts to wider
types, no `htonl`. `_Static_assert` the header layout with `offsetof`. Detect the host endianness at
runtime and byte-swap only when necessary. Verify: write 1000 floats including `-0.0f`, `INFINITY`,
a NaN and a denormal, read back, and confirm bit-exact equality via `memcmp` on the bit patterns.

Example:
```
host: little-endian, swap=no
wrote 1000 floats (4032 bytes)  read back: 1000/1000 bit-exact  (NaN payload preserved)
```

<details><summary>Hint</summary>
Compare bit patterns, not values: `nan == nan` is false. Read floats as `uint32_t` via 4 byte
assembly with `(uint32_t)b[k] << (8*k)` — casts first — then `memcpy` into the `float`. A file
written on this machine must read back correctly on a big-endian one; you can simulate that by
adding a `--force-swap` test path.
</details>

### 15.10 — **UBSan-clean competitive-programming template** (CP)

Typical contest code is a UB minefield: `int` everywhere, `1 << k`, `a * b % MOD` with
`a, b < 1e9+7` (overflows `int`, and `long long` if you're careless with three factors),
`scanf("%d")` into wrong types, `char` arithmetic on input bytes. Write a template `ex15_10.c` that
reads `n` then `n` pairs `(a, b)` and prints `sum(a_i * b_i) mod (1e9+7)`, plus `mulmod`, `powmod`,
`inv_mod` (Fermat), and a `gcd` that doesn't overflow on `LLONG_MIN`. Requirements: `int64_t`
throughout with `PRId64`, all shifts on unsigned types, every `mulmod` argument reduced first, no
`char`-signedness dependence in the fast input reader (use `getchar_unlocked`-style `int` returns),
and it must run clean under `-fsanitize=undefined,integer` (which also flags *unsigned* wrap, so
you'll have to justify or restructure any intentional wrap). Benchmark on 10⁶ pairs against a naive
`scanf` version.

Example:
```
input:  3
        1000000006 1000000006
        2 3
        123456789 987654321
output: 259106859
```

<details><summary>Hint</summary>
`(a % M) * (b % M)` with `M ≈ 1e9` fits in `int64_t` (≈ 1e18 < 9.2e18), but a third factor does
not — chain `mulmod`. For `-fsanitize=integer`, an intentional unsigned hash wrap can be silenced
per-function with `__attribute__((no_sanitize("unsigned-integer-overflow")))` — but the point is to
have zero such attributes in this file.
</details>
