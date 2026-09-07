# Chapter 01 — Exercises

Write each exercise as `ex01_K.c` in this folder (e.g. `ex01_1.c`). Compile with:

```sh
cc -Wall -Wextra -std=c11 -O2 -o ex01_K ex01_K.c -lm
```

Every program must compile with zero warnings.

---

**01.1 — Sizes table**

Print a table of `sizeof` for `char`, `short`, `int`, `long`, `long long`, `float`, `double`, `long double`, and `void*`, one per line, aligned in two columns (type name left-justified in 12 characters, size right-justified in 3).

Example:

```
char          1
short         2
int           4
...
```

<details><summary>Hint</summary>
Use `%-12s%3zu\n`. `sizeof` returns `size_t`.
</details>

---

**01.2 — Format drill**

Given `int n = -42; unsigned u = 42; long big = 1234567890123L; double x = 3.14159265; char c = 'Q';`, print each value on its own line, then print `x` again with: 2 decimals, width 12 with 4 decimals, scientific with 3 decimals, and `%g`. Print `u` in decimal and hex with `0x` prefix. Print the address of `n`.

Example (last lines):

```
3.14
      3.1416
3.142e+00
3.14159
42 0x2a
0x16f8a3b1c   (your address will differ)
```

<details><summary>Hint</summary>
`%p` requires a `void*` cast: `(void*)&n`. `%#x` adds the prefix.
</details>

---

**01.3 — Exit codes**

Write a program that reads one integer with `scanf`. If the read fails, print `not an integer` to stderr and return 2. If the integer is negative, print `negative` and return 1. Otherwise print `ok` and return 0. Verify with `echo 5 | ./ex01_3; echo $?` and `echo abc | ./ex01_3; echo $?`.

<details><summary>Hint</summary>
Check the return value of `scanf` before using the variable. `fprintf(stderr, ...)` writes to the error stream.
</details>

---

**01.4 — Temperature table**

Print a table of Celsius from 0 to 100 in steps of 20 and the corresponding Fahrenheit (`F = C * 9 / 5 + 32`) using only `int` variables. Then repeat using `double`. Explain in a comment why the two tables agree here, and give (in the comment) one Celsius value for which the integer version would be off.

Example:

```
  0   32
 20   68
...
```

<details><summary>Hint</summary>
Think about the order of `*` and `/` and when the truncation happens. Try C = 1.
</details>

---

**01.5 — Division and remainder audit**

For each pair `(a, b)` in `(7,2), (-7,2), (7,-2), (-7,-2)`, print `a / b` and `a % b`. In a comment, write what Python's `//` and `%` would give for each and explain the rule for the sign of C's `%`.

Example (first line):

```
7 / 2 = 3, 7 % 2 = 1
```

<details><summary>Hint</summary>
C truncates toward zero; Python floors. The identity `(a / b) * b + a % b == a` always holds in C.
</details>

---

**01.6 — Overflow observation**

Store `INT_MAX` in an `int` and `UINT_MAX` in an `unsigned`. Add 1 to the unsigned and print it. For the signed value, do NOT add 1 directly (that is UB); instead write a check that prints `would overflow` if adding `k` would exceed `INT_MAX`, for `k = 1` and `k = 0`. Then compute `100000 * 100000` correctly into a `long` and print it. Also print the incorrect version's type-promotion result with a comment explaining why the compiler may warn.

Example:

```
unsigned wrap: 0
k=1: would overflow
k=0: 2147483647
10000000000
```

<details><summary>Hint</summary>
`if (k > 0 && n > INT_MAX - k)` never overflows. Cast one operand to `long` BEFORE multiplying.
</details>

---

**01.7 — Float precision probe**

Print `0.1f`, `0.1`, and `0.1L` with 25 decimal places. Print `FLT_EPSILON` and `DBL_EPSILON`. Then find, by adding 1.0 repeatedly in a `float` starting from 16777216.0f, the first integer that a `float` cannot represent (the loop stops when `f + 1.0f == f`). Print it.

Example (last line):

```
float loses integers at 16777216
```

<details><summary>Hint</summary>
`float` has 24 significand bits. `%.25f` works for `float` (promoted to double), and `%.25Lf` for `long double`.
</details>

---

**01.8 — Mean of integer measurements (numerics)**

Hard-code these `int` values: `3, 4, 4, 5, 5, 5, 6`. Compute and print their mean (as a `double` with 4 decimals) and the sum of squared deviations from the mean. Your first attempt must compute the mean with integer division and print it too, so you can see the difference.

Example:

```
int mean:    4
double mean: 4.5714
ss:          6.8571
```

<details><summary>Hint</summary>
The sum is an `int`; the count is an `int`. Where does the cast go? Compute the deviations in `double`.
</details>

---

**01.9 — Byte view of a float (ML/tokenizer prep)**

Store `1.0f` in a `float`. Copy its 4 bytes into an `unsigned char[4]` using `memcpy` (`#include <string.h>`), and print them in hex, most significant byte first. Repeat for `-2.5f` and `0.15625f`. In a comment, mark which bits are the sign, exponent, and mantissa for `1.0f`.

Example:

```
1.0f      = 3f 80 00 00
```

<details><summary>Hint</summary>
This machine is little-endian: byte 0 in memory is the least significant. Print `b[3], b[2], b[1], b[0]`. `%02x` zero-pads to two hex digits.
</details>

---

**01.10 — Fixed-step Euler in float vs double (sims prep)**

Integrate `dx/dt = -x` from `x(0) = 1` to `t = 10` with Euler's method (`x = x + dt * (-x)`) using `dt = 0.001`, once in `float` and once in `double`. Print both final values with 10 significant digits (`%.10g`), the exact answer `exp(-10)` from `<math.h>`, and the absolute error of each. Use a `long` counter for the number of steps and compute it as `(long)(10.0 / dt)`.

Example:

```
float:  4.539...e-05  err ...
double: 4.539...e-05  err ...
exact:  4.539992976e-05
```

<details><summary>Hint</summary>
Write the step count once and reuse it for both loops. `fabs` for double error, `fabsf` for float. Link with `-lm`.
</details>
