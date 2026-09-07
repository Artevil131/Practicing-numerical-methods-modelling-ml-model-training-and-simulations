# Chapter 12 — Exercises

Write each exercise as `ex12_K.c` in this folder (`ex12_1.c`, `ex12_2.c`, ...). Compile with
`cc -Wall -Wextra -std=c11 -O2 -o ex12_K ex12_K.c -lm`. Zero warnings. For exercises about UB,
also try `-fsanitize=undefined` and observe what it reports.

---

### 12.1 — **Sizes and limits table**

Print a table of `sizeof` and min/max for `char`, `short`, `int`, `long`, `long long`, `size_t`, `int8_t`, `uint8_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `float`, `double`. Use `<limits.h>`, `<stdint.h>`, `<float.h>`, and `<inttypes.h>` format macros so it compiles without warnings.

Example (rows):
```
int32_t   4  -2147483648  2147483647
uint8_t   1  0            255
double    8  2.22507e-308 1.79769e+308   (min normal, max)
```

<details><summary>Hint</summary>
`INT32_MIN`, `UINT64_MAX`, `DBL_MIN`, `DBL_MAX`. Print `uint64_t` with `PRIu64`, `size_t` with `%zu`.
</details>

### 12.2 — **Wrap vs UB**

Demonstrate: (a) `uint8_t` addition wrapping (`250 + 10`), (b) `uint32_t` wrap at `UINT32_MAX + 1`, (c) that `uint8_t a = 200, b = 100; a + b > 255` is true (promotion) while `(uint8_t)(a + b)` is 44. Then write — but keep behind `if (argc > 1)` so it only runs when asked — `int x = INT_MAX; x++;` and run it under `-fsanitize=undefined` to see the report.

<details><summary>Hint</summary>
The sanitizer message says "runtime error: signed integer overflow". Note that without the sanitizer, `-O2` may print anything or delete the code.
</details>

### 12.3 — **Bit toolbox**

Implement and test `set_bit`, `clear_bit`, `toggle_bit`, `test_bit`, `extract_field(x, start, width)`, `print_bits(x, width)`, `is_pow2`, `popcount`, `bswap32`, `next_pow2`. Test each on at least 3 values and print `PASS`/`FAIL` lines by comparing against expected values (compare `popcount` against `__builtin_popcount`).

Example:
```
print_bits(0xA5, 8)   = 10100101
extract_field(0xABCD, 4, 8) = 0xBC
bswap32(0x12345678) = 0x78563412
next_pow2(1000) = 1024
```

<details><summary>Hint</summary>
`extract_field`: shift right by `start`, then mask with `(1u << width) - 1`. Be careful when `width == 32`.
</details>

### 12.4 — **Float anatomy**

Write `void dump_float(float f)` that prints the hex bits, then sign, exponent (raw and unbiased), and fraction bits, and reconstructs the value from the parts to prove it matches. Run on `1.0f, -2.5f, 0.1f, 1e-40f (subnormal), INFINITY, NAN, 16777216.0f, 16777217.0f`. Explain in a comment why the last two print the same bits. Use `memcpy`, not a pointer cast.

Example:
```
1.0f      = 0x3f800000  S=0 E=127 (2^0)   F=0x000000  -> 1.000000
-2.5f     = 0xc0200000  S=1 E=128 (2^1)   F=0x200000  -> -2.500000
```

<details><summary>Hint</summary>
`value = (-1)^S * (1 + F / 2^23) * 2^(E - 127)` for normal numbers. For subnormals (E == 0) it's `(F / 2^23) * 2^-126`, no implicit 1.
</details>

### 12.5 — **`nearly_equal` and its failure modes**

Implement `nearly_equal(a, b, rel_tol, abs_tol)`. Show that `0.1+0.2 == 0.3` fails but `nearly_equal` passes; show that with only a relative tolerance, `nearly_equal(1e-20, 0, 1e-9, 0)` is false (why?) but with `abs_tol=1e-12` it's true; show `nearly_equal(NAN, NAN, ...)` is false; show that for `1e16` and `1e16 + 1` `==` is *true* (why?). Print each case with your explanation as a string.

<details><summary>Hint</summary>
Near zero, `rel_tol * max(|a|,|b|)` is itself near zero. Above 2^53, adjacent doubles are 2 apart, so `1e16 + 1` rounds back to `1e16`.
</details>

### 12.6 — **Float summation shootout**

Fill an array with `n = 10,000,000` values of `0.1f` (as `float`). Sum four ways: naive `float` accumulator, naive `double` accumulator, Kahan in `float`, pairwise in `float`. Print each result and its error versus the exact `1e6`. Time each with `clock_gettime`.

Example:
```
naive float   : 1087937.000000  err 8.79e+04   4.2 ms
naive double  : 1000000.017813  err 1.78e-02   4.1 ms
kahan float   : 1000000.000000  err 0.00e+00  12.9 ms
pairwise float: 1000000.000000  err ...
```

<details><summary>Hint</summary>
Why is the naive double not exactly 1e6? Because `0.1f` isn't 0.1 — it's 0.100000001490116... Kahan in float can actually beat naive double here.
</details>

### 12.7 — **Stable vs naive: softmax, sigmoid, logsumexp**

Implement `softmax_naive`, `softmax_stable`, `sigmoid_naive`, `sigmoid_stable`, `logsumexp`. Run both versions on: logits `{1, 2, 3}` (should agree), `{1000, 1001, 1002}` (naive → NaN), `{-1000, -1001, -1002}` (naive → 0/0). Sigmoid at `±1000`. Verify `softmax_stable` sums to 1 (within 1e-12) and that `log_softmax_i = z_i - logsumexp(z)` equals `log(softmax_stable(z)_i)` where the latter is finite.

<details><summary>Hint</summary>
Print `isnan(...)` explicitly. For the `{-1000, ...}` case, `exp(-1000) == 0` in double, so the naive sum is 0 and you get `0/0`.
</details>

### 12.8 — **Seedable PRNG with statistical checks** (ML)

Implement `Rng` with splitmix64 seeding, xorshift64\*, `rng_uniform` (top 53 bits), `rng_below(n)` (unbiased), and `rng_gaussian` (Box-Muller, returning both samples — cache the second). Generate 1,000,000 samples and print: mean and variance of uniforms (expect 0.5, 1/12), mean and variance of Gaussians (expect 0, 1), a 10-bin histogram of `rng_below(10)` (each bin ≈ 100,000), and confirm the same seed gives the same first 5 numbers on two runs.

<details><summary>Hint</summary>
Compute variance with Welford's online algorithm, not `E[x²] − E[x]²` — this ties into the cancellation section. Take the seed from `argv[1]` with `strtoull`.
</details>

### 12.9 — **Kaiming initialization and activation statistics** (ML)

Using your RNG, create a 512×512 weight matrix with `N(0, sqrt(2/fan_in))` (Kaiming/He init), a 512-vector `x ~ N(0,1)`, compute `y = relu(W x)` and repeat for 10 layers, feeding `y` forward. Print the standard deviation of the activations after each layer. Then repeat with `N(0, 1)` init (no scaling) and with `N(0, 0.01)` and watch the activations explode / vanish. Use `double`; then `typedef` to `float` and see if the results change meaningfully.

Example:
```
layer  1  std=1.0xx   (kaiming)
layer 10  std=0.9xx   (kaiming)
layer 10  std=1e+12   (unit init)  -> would overflow float
layer 10  std=1e-17   (0.01 init)
```

<details><summary>Hint</summary>
Kaiming keeps `Var(y) ≈ Var(x)` through ReLU layers because ReLU halves the variance and the `2/fan_in` compensates. Matrix-vector product: `y[i] = Σ_j W[i*n + j] * x[j]`.
</details>

### 12.10 — **MNIST-style byte pixels to floats, big-endian header** (ML)

Simulate reading an IDX file: build an in-memory `uint8_t` buffer containing a 16-byte big-endian header (magic `0x00000803`, count 2, rows 4, cols 4) followed by 2 images of 16 `uint8_t` pixels. Parse it: read each 32-bit header field with `bswap32` (or by assembling bytes manually), validate the magic with an error message on mismatch, then convert pixels to `float` in `[0, 1]` (`p / 255.0f`) and also to standardized `(p/255 − 0.1307) / 0.3081` (MNIST mean/std). Print both images as 4×4 grids of floats with 3 decimals. Then compute the mean pixel over both images two ways: summing `uint8_t` into a `uint8_t` (wrong — show the wrap) and into a `uint32_t` (right).

<details><summary>Hint</summary>
Big-endian: the most significant byte comes first, so `value = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]` — cast each byte to `uint32_t` before shifting so `b[0] << 24` isn't done in a signed `int`. The real format is documented at yann.lecun.com/exdb/mnist; the header you build here matches it.
</details>
