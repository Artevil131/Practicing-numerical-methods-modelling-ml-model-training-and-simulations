# Chapter 12 — Numbers, Bits, and Floats

## What you'll be able to do after this chapter

- Pick the right integer type (`uint8_t` for pixels, `int64_t` for counters, `size_t` for sizes) and know which operations overflow safely and which are undefined behavior.
- Use bitwise operators fluently: masks, set/clear/toggle/test, shifts, popcount, byte swap.
- Read the IEEE 754 layout of a `float`/`double` and predict when precision runs out.
- Compare floats correctly, handle `NaN`/`Inf`, and diagnose "loss is NaN".
- Write numerically stable softmax, log-sum-exp, sigmoid, and compensated summation.
- Implement a fast, reproducible PRNG (xorshift64\*, splitmix64) and Gaussian sampling via Box-Muller for weight initialization.

## Why this matters for ML / numerics / sims

Every bug class in numerical code traces back here. MNIST pixels are `uint8_t`; multiply two of them into an `int` or you overflow. Softmax on logits of 1000 gives `exp(1000) = inf`, `inf/inf = NaN`, and your loss is NaN forever. Summing a million `float` losses naively loses digits. PyTorch defaults to `float32` for speed and memory; your from-scratch code should know exactly what that costs. Weight init needs Gaussians; a hash of the token id needs bit mixing; an FFT needs power-of-two checks; a PIC plasma sim needs a fast, seedable RNG. Bits and floats are the material everything is built from.

---

## 1. Fixed-width integers: `<stdint.h>`

`int`, `long` have platform-dependent sizes (`long` is 8 bytes on macOS/Linux, 4 on Windows). When the width matters — file formats, pixel data, hashing, bit tricks — use fixed-width types.

```c
#include <stdint.h>
#include <stddef.h>     // size_t, ptrdiff_t
#include <stdio.h>
#include <inttypes.h>   // PRId64 etc. for portable printf

int main(void) {
    uint8_t  pixel = 255;         // MNIST pixel: 0..255, exactly one byte
    int32_t  logit_id = -7;       // exactly 4 bytes, two's complement
    int64_t  step = 1LL << 40;    // exactly 8 bytes
    uint64_t hash = 0x9E3779B97F4A7C15ull;
    size_t   n = sizeof(double);  // unsigned, the type of sizeof and array indices
    ptrdiff_t d = &pixel - &pixel;// signed, the type of (pointer - pointer)

    printf("%u %d %" PRId64 " %" PRIx64 " %zu %td\n", pixel, logit_id, step, hash, n, d);
    // 255 -7 1099511627776 9e3779b97f4a7c15 8 0
}
```

| Type | Bytes | Range | Use for |
|---|---|---|---|
| `uint8_t` | 1 | 0..255 | pixels, bytes of a file, token bytes for BPE |
| `int8_t` | 1 | -128..127 | quantized weights (int8 inference) |
| `int32_t` | 4 | ±2.1e9 | token ids, indices when memory matters |
| `uint32_t` | 4 | 0..4.3e9 | RNG state halves, hashes, RGBA pixel |
| `int64_t` | 8 | ±9.2e18 | step counters, byte offsets, timestamps |
| `uint64_t` | 8 | 0..1.8e19 | RNG state, hashes, bitsets |
| `size_t` | 8 (64-bit) | 0..1.8e19 | sizes, counts, array indices — what `malloc`/`strlen` use |
| `ptrdiff_t` | 8 | signed | result of subtracting pointers |

`int` is still fine for "small loop counter, obviously fits". Use `size_t` for anything that indexes memory. Note `uint8_t` is `unsigned char`, so `printf("%d")` works after default promotion to `int`; `%u` also fine.

`<limits.h>` gives `INT_MAX`, `INT_MIN`, `UINT_MAX`, `LLONG_MAX`, `CHAR_BIT` (8), `SIZE_MAX` (in `<stdint.h>`).

Python equivalent: Python `int` is arbitrary precision; NumPy has `np.uint8`, `np.int32`, `np.int64` — these are the C types. `np.uint8(200) + np.uint8(100)` wraps to 44 exactly like C.

## 2. Overflow: signed is UB, unsigned wraps

```c
uint8_t  a = 200, b = 100;
uint8_t  c = a + b;            // a+b is computed as int (300), then truncated: c == 44
unsigned u = UINT_MAX; u++;    // defined: wraps to 0 (mod 2^32)
int      i = INT_MAX; i++;     // UNDEFINED BEHAVIOR. Compiler may assume it never happens.
```

Unsigned arithmetic is modular (mod 2^N) by definition. Signed overflow is undefined: the compiler may optimize `if (i + 1 > i)` to `true`, delete your bounds check, or produce a negative number — all legal. Detect with `-fsanitize=undefined` (`../13_debugging_testing_perf/lesson.md`).

Integer promotion trap: any arithmetic on types smaller than `int` (`uint8_t`, `char`, `short`) first converts operands to `int`. So `uint8_t x = 255; x << 1` is `510` as an `int`, not `254` — until you store it back into a `uint8_t`.

Mixed signed/unsigned: `-1 < 1u` is **false** (the `-1` converts to `UINT_MAX`). `-Wsign-compare` (in `-Wextra`) catches most of these. Classic bug: `for (size_t i = n - 1; i >= 0; i--)` never terminates because `size_t` is never negative.

## 3. Bitwise operators

| Op | Name | `a=1100b`, `b=1010b` | Truth table (per bit) |
|---|---|---|---|
| `a & b` | AND | `1000` | 1 only if both 1 |
| `a \| b` | OR | `1110` | 1 if either 1 |
| `a ^ b` | XOR | `0110` | 1 if different |
| `~a` | NOT | `...0011` | flips every bit (width of the type) |
| `a << n` | shift left | `a<<1 = 11000` | multiply by 2^n, zeros in from the right |
| `a >> n` | shift right | `a>>1 = 110` | divide by 2^n (unsigned: zeros in; signed: implementation-defined) |

```
      AND          OR           XOR
  & | 0 1      | | 0 1      ^ | 0 1
  --+----      --+----      --+----
  0 | 0 0      0 | 0 1      0 | 0 1
  1 | 0 1      1 | 1 1      1 | 1 0
```

Do not confuse `&` (bitwise) with `&&` (logical), `|` with `||`, `~` with `!`. `!5 == 0`, `~5 == -6`.

Precedence trap: `&`, `|`, `^` bind *looser* than `==`. `x & 1 == 0` parses as `x & (1 == 0)` == `x & 0` == 0. Always parenthesize: `(x & 1) == 0`.

## 4. Masks: set, clear, toggle, test

```c
uint32_t flags = 0;
#define BIT(n) (1u << (n))

flags |=  BIT(3);            // set bit 3       -> 0b1000
flags |=  BIT(0);            // set bit 0       -> 0b1001
flags &= ~BIT(3);            // clear bit 3     -> 0b0001
flags ^=  BIT(1);            // toggle bit 1    -> 0b0011
int on = (flags & BIT(1)) != 0;   // test bit 1  -> 1
uint32_t low4 = flags & 0xF;      // extract low nibble
uint32_t field = (flags >> 4) & 0x3F;  // extract 6-bit field starting at bit 4
```

Use `1u` (unsigned) in `BIT`: `1 << 31` on a 32-bit `int` is UB (shifting a 1 into the sign bit).

Application: a bitset of 64 booleans in one `uint64_t` (visited flags in a graph, "token is a byte-pair candidate" sets in BPE, 64 bits of RGBA channel enable). Also RGBA packing: `pixel = (r << 24) | (g << 16) | (b << 8) | a`.

## 5. Shifting signed values is a trap

- Left-shifting a negative value: **UB**. Left-shifting a positive value into or past the sign bit: **UB**.
- Right-shifting a negative value: **implementation-defined** (arithmetic shift, sign-extending, on every compiler you'll meet, but not guaranteed).
- Shifting by ≥ the width (`x << 32` for 32-bit) or by a negative amount: **UB**.

Rule: do bit manipulation on **unsigned** types. Cast in, shift, cast out.

```c
int x = -8;
unsigned ux = (unsigned)x;     // well-defined: 0xFFFFFFF8
printf("%u\n", ux >> 1);       // 0x7FFFFFFC = 2147483644 (logical shift)
printf("%d\n", x >> 1);        // -4 on clang/gcc (arithmetic), but implementation-defined
```

## 6. Bit tricks

```c
#include <stdbool.h>
static bool is_pow2(uint64_t x)  { return x && !(x & (x - 1)); }   // FFT needs n to be a power of 2
static int  popcount(uint64_t x) { int c = 0; while (x) { x &= x - 1; c++; } return c; }
                                  // x & (x-1) clears the lowest set bit; loop runs once per set bit
static uint32_t bswap32(uint32_t x) {          // reverse byte order (endianness)
    return (x >> 24) | ((x >> 8) & 0xFF00u) | ((x << 8) & 0xFF0000u) | (x << 24);
}
static uint64_t next_pow2(uint64_t x) {        // smallest power of 2 >= x
    if (x <= 1) return 1;
    x--; x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; x |= x >> 32;
    return x + 1;
}
// is_pow2(64)=1 is_pow2(96)=0 popcount(0xFF)=8 bswap32(0x12345678)=0x78563412 next_pow2(100)=128
```

Compilers provide `__builtin_popcountll`, `__builtin_bswap32`, `__builtin_clzll` (count leading zeros) which become single instructions. Your Mac (arm64) is little-endian: the least significant byte is stored first. The MNIST IDX file format stores its header as big-endian `int32` — you must `bswap32` after reading it (`../08_file_io/lesson.md`).

## 7. Hex/binary literals and printing bits

C has hex literals `0xFF`, octal `0755`, and (C23 / gcc / clang extension) binary `0b1010`. Under `-std=c11`, `0b...` is a clang extension — it compiles, but avoid it in portable code; write `0xA` and comment. C has no `%b` in `printf` until C23, so write your own:

```c
static void print_bits(uint32_t x, int width) {
    for (int i = width - 1; i >= 0; i--) putchar((x >> i) & 1 ? '1' : '0');
}
// print_bits(0xA5, 8) -> 10100101
// printf("%08x\n", 0xA5) -> 000000a5     (hex, zero-padded to 8)
// printf("%#x\n", 255)   -> 0xff          (# adds prefix)
```

## 8. IEEE 754 layout

A `float` is 32 bits, a `double` 64 bits, each split into sign, exponent, mantissa (fraction):

```
float (binary32):
  bit 31    bits 30..23        bits 22..0
  +---+-----------------+--------------------------------+
  | S |   exponent (8)  |         fraction (23)          |
  +---+-----------------+--------------------------------+
  value = (-1)^S * 1.fraction * 2^(exponent - 127)

double (binary64):
  bit 63    bits 62..52        bits 51..0
  +---+-----------------+--------------------------------+
  | S |  exponent (11)  |         fraction (52)          |
  +---+-----------------+--------------------------------+
  value = (-1)^S * 1.fraction * 2^(exponent - 1023)
```

The leading `1.` is implicit (not stored), giving 24 / 53 significant bits. Special exponent values: all zeros → zero or *subnormal* (tiny numbers with reduced precision); all ones → `Inf` (fraction 0) or `NaN` (fraction ≠ 0).

Inspect the bits with a union or `memcpy` (type-punning through a union is allowed in C; pointer casts `*(uint32_t*)&f` violate strict aliasing and are UB):

```c
float f = 1.0f;
uint32_t bits; memcpy(&bits, &f, 4);
printf("%08x\n", bits);            // 3f800000 : S=0, exp=0x7F=127 (so 2^0), frac=0 -> 1.0
f = -2.5f; memcpy(&bits, &f, 4);   // c0200000 : S=1, exp=128 (2^1), frac=.01b -> 1.25*2 = 2.5
```

| | `float` | `double` |
|---|---|---|
| Significant bits | 24 | 53 |
| Decimal digits | ~7.2 | ~15.9 |
| Machine epsilon | `FLT_EPSILON` = 1.19e-7 | `DBL_EPSILON` = 2.22e-16 |
| Max | 3.4e38 | 1.8e308 |
| Min normal | 1.2e-38 | 2.2e-308 |
| Spacing at 1e6 | 0.0625 | 1.2e-10 |
| Integers exact up to | 2^24 = 16,777,216 | 2^53 = 9.0e15 |

That last row: `float` cannot represent 16,777,217. A `float` step counter or sample index breaks past 16M. Use integers for counting.

## 9. Machine epsilon and why `0.1 + 0.2 != 0.3`

`DBL_EPSILON` (from `<float.h>`) is the gap between 1.0 and the next representable double: 2^-52. Relative rounding error of any single operation is at most `DBL_EPSILON/2`.

`0.1` in binary is `0.000110011001100...` repeating — not representable exactly. So `0.1`, `0.2`, `0.3` are each stored as the nearest double, and the rounded sum of the first two is not the rounded value of the third:

```c
printf("%.17g\n", 0.1 + 0.2);   // 0.30000000000000004
printf("%.17g\n", 0.3);         // 0.29999999999999999
printf("%d\n", 0.1 + 0.2 == 0.3); // 0
```

Python equivalent: identical — `0.1 + 0.2 == 0.3` is `False` in Python because Python floats *are* C doubles.

## 10. Never compare floats with `==`

Use a tolerance that combines absolute (for values near zero) and relative (for large values):

```c
#include <math.h>
static bool nearly_equal(double a, double b, double rel_tol, double abs_tol) {
    double diff = fabs(a - b);
    if (diff <= abs_tol) return true;                         // handles a,b near 0
    return diff <= rel_tol * fmax(fabs(a), fabs(b));          // scale-aware
}
// nearly_equal(0.1+0.2, 0.3, 1e-9, 1e-12) -> true
```

This is `math.isclose` / `np.isclose` (`rtol=1e-5, atol=1e-8` in NumPy; `torch.allclose` same defaults). Note `NaN` fails every comparison, so `nearly_equal(NAN, NAN)` returns false, which is what you want in tests.

Loop termination: never `while (x != 1.0)` with float steps; use `for (int i = 0; i < n; i++) x = a + i * h;` (compute from the index, don't accumulate).

## 11. `INFINITY`, `NAN`, `isnan`, `isinf`, propagation

```c
#include <math.h>
double inf = INFINITY, nan = NAN;          // macros from <math.h>
printf("%f %f\n", 1.0 / 0.0, -1.0 / 0.0); // inf -inf   (floating division by 0 is NOT UB; integer is)
printf("%f\n", 0.0 / 0.0);                 // nan  (or -nan)
printf("%f %f\n", inf - inf, inf * 0.0);   // nan nan
printf("%d %d\n", isnan(nan), isinf(inf)); // 1 1
printf("%d\n", nan == nan);                // 0 — NaN is not equal to anything, including itself
printf("%d\n", nan != nan);                // 1 — the one test that detects NaN without isnan
printf("%f\n", sqrt(-1.0));                // nan (or -nan)
printf("%f\n", log(0.0));                  // -inf
printf("%f\n", exp(1000.0));               // inf  (overflow)
printf("%g\n", exp(-1000.0));              // 0    (underflow to zero)
```

**NaN propagates**: any arithmetic with a NaN input produces NaN. One NaN in one weight poisons the entire network after one matmul. `fmax`/`fmin` are the exceptions (they return the non-NaN argument).

### The loss-is-NaN debugging story

Symptoms: loss prints `nan` at step 37 and stays there. Causes, in order of likelihood:
1. `log(0)` in cross-entropy: predicted probability underflowed to exactly 0 → `-inf` loss → `inf - inf` → NaN. Fix: `log(p + 1e-12)` or compute `log_softmax` directly (section 13).
2. `exp(large)` in softmax → `inf` → `inf/inf` = NaN. Fix: subtract the max (section 13).
3. Learning rate too high → weights blow past 1e308 → `inf` → NaN. Fix: lower LR, gradient clipping.
4. `sqrt(negative)` from a variance computed as `E[x²] − E[x]²` that went slightly negative due to cancellation. Fix: `sqrt(fmax(var, 0))` or Welford's algorithm.
5. Division by a zero norm/std. Fix: `+ eps`.

Debugging technique: add `assert(!isnan(loss))` after each step (chapter 13); bisect with prints of `isnan` on each layer's output to find where it first appears. This is what `torch.autograd.set_detect_anomaly(True)` automates.

## 12. Overflow, underflow, and catastrophic cancellation

- **Overflow**: `exp(710.)` → `inf` for double; `expf(89.f)` → `inf` for float. Logits above ~88 kill a `float` softmax.
- **Underflow**: `exp(-746.)` → `0.` Products of many probabilities (`p1*p2*...*pn`) underflow fast — this is why you sum log-probabilities instead.
- **Catastrophic cancellation**: subtracting two nearly-equal numbers destroys relative precision.

```c
double x = 1e8;
double a = (x + 1) * (x + 1) - x * x;   // exact answer 2x+1 = 200000001
printf("%.1f\n", a);                     // 200000001.0 — fine at 1e8 in double...
volatile float fx = 1e4f;                // volatile: force each product to round to float
volatile float p1 = (fx + 1) * (fx + 1); // 100020001 rounds to 100020000 (float spacing here is 8)
volatile float p2 = fx * fx;             // 100000000 exact
printf("%.1f\n", p1 - p2);               // 20000.0 — exact answer 20001; float already lost the 1
```

Why `volatile`? Written as one expression, clang's default `-ffp-contract=on` fuses `a*b - c*d` into an FMA that computes one product exactly, and the demo "works" by accident. Your numeric results can depend on compiler flags; `-ffp-contract=off` restores strict per-operation rounding.

Classic victims: variance via `E[x²] − E[x]²`; `1 − cos(x)` for small x (use `2*sin²(x/2)`); quadratic formula `(-b + sqrt(b²−4ac))/2a` when `b² ≫ 4ac` (use the stable form `2c / (-b − sqrt(...))`); `log(1 + x)` for tiny x (use `log1p(x)`); `exp(x) − 1` (use `expm1(x)`).

## 13. Log-sum-exp and stable softmax

Naive `softmax(z)_i = exp(z_i) / Σ exp(z_j)` overflows. Since softmax is shift-invariant, subtract the max first:

```c
// Stable softmax, in place. z has n entries.
static void softmax(double *z, size_t n) {
    double m = z[0];
    for (size_t i = 1; i < n; i++) if (z[i] > m) m = z[i];
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) { z[i] = exp(z[i] - m); sum += z[i]; }   // every arg <= 0, so exp <= 1
    for (size_t i = 0; i < n; i++) z[i] /= sum;
}
// z = {1000, 1001, 1002} -> {0.090031, 0.244728, 0.665241}   naive version: nan nan nan

// log(sum(exp(z))) without overflow — used for log_softmax and cross-entropy
static double logsumexp(const double *z, size_t n) {
    double m = z[0];
    for (size_t i = 1; i < n; i++) if (z[i] > m) m = z[i];
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += exp(z[i] - m);
    return m + log(s);
}
// log_softmax(z)_i = z_i - logsumexp(z)   — never takes log of a zero
// cross_entropy(z, target) = logsumexp(z) - z[target]
```

Python equivalent: `scipy.special.logsumexp`, `torch.nn.functional.log_softmax`, `F.cross_entropy` (which fuses log_softmax + NLL for exactly this stability reason).

## 14. Stable sigmoid

`1/(1+exp(-x))` overflows `exp(-x)` for `x = -1000` → `1/(1+inf) = 0`, which is *correct* by luck, but `exp(-x)` for `x = 1000` is `exp(-1000) = 0` → `1/(1+0) = 1`, fine — the real issue is `float` and gradient `s(1-s)` at extremes. The robust form branches on sign so `exp` is never called with a positive argument:

```c
static double sigmoid(double x) {
    if (x >= 0) { double e = exp(-x); return 1.0 / (1.0 + e); }
    else        { double e = exp(x);  return e / (1.0 + e); }
}
// sigmoid(-1000) = 0.000000, sigmoid(1000) = 1.000000, sigmoid(0) = 0.5, no inf ever generated
```

For the loss, use `log(sigmoid(x))` = `-softplus(-x)` with `softplus(x) = log1p(exp(-fabs(x))) + fmax(x, 0)` — this is what `BCEWithLogitsLoss` does.

## 15. Kahan and pairwise summation

Adding a million `float`s of size ~1 with a running sum of ~1e6 loses everything below `1e6 * FLT_EPSILON ≈ 0.06` per add. Two fixes:

```c
// Kahan compensated summation: tracks the rounding error and adds it back.
static double kahan_sum(const double *x, size_t n) {
    double sum = 0.0, c = 0.0;
    for (size_t i = 0; i < n; i++) {
        double y = x[i] - c;          // subtract the error from last round
        double t = sum + y;           // low bits of y are lost here...
        c = (t - sum) - y;            // ...and recovered here
        sum = t;
    }
    return sum;
}
// Pairwise (recursive halving): O(log n) error growth instead of O(n). This is what NumPy's sum uses.
static double pairwise_sum(const double *x, size_t n) {
    if (n <= 8) { double s = 0; for (size_t i = 0; i < n; i++) s += x[i]; return s; }
    return pairwise_sum(x, n / 2) + pairwise_sum(x + n / 2, n - n / 2);
}
// Summing 1e7 copies of 0.1f as float: naive -> 1087937.0, kahan -> 1000000.0 (exact 1e6)
```

Compile note: with `-ffast-math` the compiler may algebraically simplify `(t - sum) - y` to `0` and destroy Kahan. Never use `-ffast-math` with compensated algorithms.

## 16. Integer vs float division, again

```c
int a = 7, b = 2;
printf("%d\n", a / b);              // 3   (truncates toward zero; -7/2 == -3)
printf("%d\n", a % b);              // 1   (-7 % 2 == -1: sign follows the dividend, unlike Python's -7 % 2 == 1)
printf("%f\n", a / (double)b);      // 3.500000  (cast ONE operand before dividing)
printf("%f\n", (double)(a / b));    // 3.000000  (too late — division already happened in int)
double mean = sum / n;              // if sum is int and n is size_t → integer division. Cast.
```

Python equivalent: C `/` on ints is Python's `//` (except rounding toward zero rather than floor for negatives). `%` on negatives differs from Python.

## 17. `float` vs `double`: speed vs accuracy

| | `float` | `double` |
|---|---|---|
| Memory per element | 4 B | 8 B |
| SIMD lanes per 128-bit register (NEON) | 4 | 2 |
| Throughput on GPU | 1x (or 16x with tensor cores) | ~1/32 to 1/2 |
| Accumulated error after 1e6 adds | ~0.1 relative | ~1e-10 relative |

PyTorch defaults to `float32` because network training tolerates noise (SGD is already noisy), and memory bandwidth is the bottleneck; it uses `float32` *accumulators* for reductions even on `float16` data. Your numerical methods (ODE solvers, N-body over 1e6 steps, FDTD) should use `double` — errors compound over time steps and there's no gradient descent to absorb them. Rule: `double` unless you measured that `float` is needed and verified the error is acceptable. In this course, prefer `double` for the matrix lib and let a `typedef double real;` make switching a one-line change.

## 18. Random numbers

`rand()` is bad: `RAND_MAX` may be only 32767, the low bits have short periods, `rand() % n` is biased, and the algorithm differs per platform (non-reproducible). Write your own — it's 5 lines.

```c
// xorshift64* : 64-bit state, period 2^64-1, passes BigCrush. State must be nonzero.
typedef struct { uint64_t s; } Rng;

static uint64_t rng_next(Rng *r) {
    uint64_t x = r->s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1Dull;
}
// splitmix64: excellent for seeding / hashing an integer into a random-looking one (deterministic per input)
static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static Rng rng_seed(uint64_t seed) {       // run seed through splitmix so seed=1 and seed=2 aren't correlated
    uint64_t s = seed; Rng r = { splitmix64(&s) };
    if (r.s == 0) r.s = 1;
    return r;
}
// Uniform double in [0, 1): take the top 53 bits (a double's mantissa width) and scale.
static double rng_uniform(Rng *r) { return (rng_next(r) >> 11) * (1.0 / 9007199254740992.0); }  // / 2^53

// Unbiased integer in [0, n): rejection sampling instead of the biased `% n`.
static uint64_t rng_below(Rng *r, uint64_t n) {
    uint64_t limit = UINT64_MAX - UINT64_MAX % n, x;
    do { x = rng_next(r); } while (x >= limit);
    return x % n;
}
// Standard normal via Box-Muller: two uniforms -> two independent Gaussians.
static double rng_gaussian(Rng *r) {
    double u1 = rng_uniform(r), u2 = rng_uniform(r);
    if (u1 < 1e-300) u1 = 1e-300;                     // avoid log(0)
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);   // the sin(...) partner is a second sample
}
// Weight init (Kaiming / He for ReLU): w ~ N(0, sqrt(2 / fan_in))
//   w[i] = rng_gaussian(&rng) * sqrt(2.0 / fan_in);
```

Seeding and reproducibility: same seed → same sequence → same initial weights → same training curve, bit for bit (on the same machine and compiler flags). Take the seed from the command line so you can re-run a failing experiment. This is `torch.manual_seed(42)`; PCG and xoshiro are the modern alternatives (NumPy's default is PCG64). PCG's core is the same idea: a simple state update (an LCG) plus an output permutation that scrambles the weak bits.

## 19. `<math.h>` survey

All take and return `double`; `f`-suffixed versions (`expf`, `sqrtf`) are for `float` — calling `exp` on a `float` silently promotes to `double` and back, which is slower but correct. Link with `-lm`.

| Function | Notes |
|---|---|
| `exp, log, log2, log10, log1p, expm1` | `log1p(x)` accurate for tiny x; `expm1` likewise |
| `pow(x, y)` | slow; for `x*x` write `x*x`; for `2^k` use `ldexp(1, k)` or shifts |
| `sqrt, cbrt, hypot(x,y)` | `hypot` avoids overflow in `sqrt(x*x+y*y)` |
| `sin, cos, tan, asin, acos, atan, atan2(y,x)` | `atan2` handles quadrants — use it for angles |
| `sinh, cosh, tanh` | `tanh` is the activation |
| `fabs` | not `abs` (that's for `int`) |
| `floor, ceil, round, trunc, rint` | `(int)x` truncates; `lround` returns `long` |
| `fmod(x, y), remainder` | float modulo; `fmod(-7, 2) == -1` |
| `fmax, fmin` | NaN-aware: `fmax(NAN, 1) == 1` |
| `fma(a, b, c)` | `a*b + c` with one rounding; hardware instruction; used in dot products |
| `isnan, isinf, isfinite, signbit` | classification macros |
| `nextafter(x, y)` | next representable double after x toward y — for exploring ulps |
| `M_PI, M_E` | POSIX constants (available on macOS; define your own if a compiler complains) |

## 20. `<time.h>` for timing

```c
#include <time.h>
struct timespec t0, t1;
clock_gettime(CLOCK_MONOTONIC, &t0);
/* ... work ... */
clock_gettime(CLOCK_MONOTONIC, &t1);
double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
printf("%.3f ms\n", secs * 1e3);
```

`CLOCK_MONOTONIC` never goes backwards (unlike wall-clock time). `clock()` measures CPU time in `CLOCKS_PER_SEC` units and is portable but coarse. `time(NULL)` gives seconds since 1970 — fine as a *default* RNG seed but useless for benchmarking. More in `../13_debugging_testing_perf/lesson.md`.

---

## Gotchas and undefined behavior

- **Signed integer overflow is UB.** `INT_MAX + 1`, `abs(INT_MIN)`, `-INT_MIN`, `INT_MIN / -1`. Unsigned wraps and is fine.
- **Shifts**: negative shift amount, shift ≥ width, left-shifting a negative or into the sign bit — UB. Right shift of negative — implementation-defined.
- **Integer division by zero is UB** (crashes). Floating division by zero is defined (`±inf` or `NaN`).
- **`1 << 31` is UB** for 32-bit `int`; write `1u << 31`.
- **Type-punning via pointer cast** `*(uint32_t *)&f` violates strict aliasing — UB. Use `memcpy` or a `union`.
- **Precedence**: `x & 1 == 0` is `x & (1 == 0)`. `x << 2 + 1` is `x << 3`. Parenthesize.
- **Promotion**: `uint8_t a = 200, b = 100; if (a + b > 255)` is true — the addition happens in `int`. `uint8_t c = a + b;` truncates to 44.
- **`size_t` going negative**: `for (size_t i = n - 1; i >= 0; i--)` is infinite. Write `for (size_t i = n; i-- > 0;)`.
- **`-1 < 1u` is false.** Mixed signed/unsigned compare converts to unsigned.
- **`NAN == NAN` is false**; `x != x` is the NaN test; `isnan` is the readable one.
- **`float` literals**: `0.1` is a `double`; `0.1f` is a `float`. `float x = 0.1;` rounds twice. `x * 2.0` promotes to double; `x * 2.0f` stays float.
- **`%f` for `float`** in `printf` is fine (promoted to double). `%lf` in `scanf` is required for `double`, `%f` for `float`.
- **`-ffast-math`** breaks Kahan summation, `isnan` checks (compiler assumes no NaNs), and IEEE semantics generally. Don't use it for anything you need to reason about.
- **`rand() % n`** is biased and platform-dependent. Use your own RNG.
- **Uninitialized RNG state of 0** makes xorshift return 0 forever.

## Common mistakes checklist

- [ ] Used `int` for a byte offset into a >2 GB file (use `size_t`/`int64_t`).
- [ ] Multiplied two `int32_t` expecting an `int64_t` result — cast *before* multiplying: `(int64_t)a * b`.
- [ ] Compared floats with `==`, or looped `while (x != end)` with float steps.
- [ ] Naive softmax / `log(0)` in cross-entropy — NaN loss.
- [ ] `abs()` on a `double` (truncates to int silently — `-Wabsolute-value` warns; use `fabs`).
- [ ] `sqrt(var)` where `var` came out as `-1e-17` from cancellation.
- [ ] Summed a million floats naively into a `float`.
- [ ] Forgot `-lm`; forgot `#include <math.h>` (implicit declaration → wrong return type → garbage).
- [ ] Used `float` for time-stepping a long simulation.
- [ ] Shifted a signed `int`; wrote `1 << 31`.

## You can move on when...

- You can explain why `uint8_t a = 200, b = 100; a + b` compares greater than 255 but stores as 44.
- You can write set/clear/toggle/test bit expressions and `is_pow2` from memory, and say why `1u` matters.
- You can sketch the 32-bit float layout and decode `0x3f800000` and `0xc0200000` by hand.
- You can write `nearly_equal` with both tolerances and explain why `NaN` fails it.
- You can write stable softmax and `logsumexp` without looking, and explain the NaN chain `exp → inf → inf/inf`.
- You can write xorshift64\*, uniform [0,1) from the top 53 bits, and Box-Muller, and use them to initialize a weight matrix with `N(0, sqrt(2/fan_in))`.
