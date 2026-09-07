# Chapter 02 — Exercises

Write each exercise as `ex02_K.c` in this folder. Compile with:

```sh
cc -Wall -Wextra -std=c11 -O2 -o ex02_K ex02_K.c -lm
```

Zero warnings required. Hard-code input data in the program unless the exercise says otherwise.

---

**02.1 — FizzBuzz with a twist**

Print the numbers 1 to 30, one per line. Replace multiples of 3 with `Fizz`, multiples of 5 with `Buzz`, multiples of both with `FizzBuzz`. Write it twice in the same program: once with an `if`/`else if` chain, once with a `switch` on `(i % 3 == 0) * 2 + (i % 5 == 0)`.

Example (lines 13-15):

```
13
14
FizzBuzz
```

<details><summary>Hint</summary>
The switch expression takes values 0, 1, 2, 3. Map each to a case. Test the "both" case before the single cases in the if-chain.
</details>

---

**02.2 — Grade classifier with the ternary**

For each score in `{95, 82, 71, 64, 40, 100, 0}`, print the letter grade (A >= 90, B >= 80, C >= 70, D >= 60, else F) using ONLY nested ternary operators inside a single `printf` (no `if`). Then, in a comment, explain why you would not do this in real code.

Example:

```
95 -> A
82 -> B
```

<details><summary>Hint</summary>
`?:` is right-associative: `s >= 90 ? 'A' : s >= 80 ? 'B' : ...`. Print the grade with `%c`.
</details>

---

**02.3 — Collatz steps**

For each starting value `n` in 1..20, count how many steps the Collatz map (`n` even: `n/2`; odd: `3n+1`) takes to reach 1. Print `n` and its step count. Use a `while` loop. Use `long` for `n` and add a safety cap of 10000 steps; print `CAP` instead of the count if hit.

Example:

```
1 0
2 1
3 7
...
27 would be 111 (not in range)
```

<details><summary>Hint</summary>
`n % 2 == 0` tests evenness. The loop condition is `n != 1 && steps < cap`.
</details>

---

**02.4 — Multiplication table, formatted**

Print a 12x12 multiplication table with column headers and a row header, every cell right-aligned in 4 characters, using nested `for` loops. The top-left corner cell must be blank. Do not print a trailing space at the end of a line.

Example (first two lines):

```
       1   2   3   4 ...
   1   1   2   3   4 ...
```

<details><summary>Hint</summary>
`%4d` for cells. For the header row, print `"    "` first (4 spaces).
</details>

---

**02.5 — Precedence quiz, self-graded**

Declare `int a = 6, b = 3, c = 2;` and `unsigned flags = 0x5;`. Before running, write in a comment your predicted value for each of: `a / b * c`, `a / (b * c)`, `a % b + c * 2`, `flags & 4 == 4`, `(flags & 4) == 4`, `a > b > c`, `-a % 4`, `1 << 2 + 1`, `a++ + ++a` (write "UB" if it is). Then print the actual value of each one that is NOT UB, and confirm your predictions.

Example:

```
a / b * c       = 4
a / (b * c)     = 1
```

<details><summary>Hint</summary>
Look up `&` vs `==` and `<<` vs `+` in the precedence table. Do not compile the UB expression; leave it in the comment only.
</details>

---

**02.6 — Bisection root finder**

Find the root of `f(x) = x*x*x - x - 2` on `[1, 2]` by bisection. Loop until the interval width is below `1e-10` or 200 iterations pass. Print the iteration number, the midpoint, and `f(midpoint)` every 10 iterations, then the final root with 12 decimals and the iteration count. Use `do-while` or `while` (your choice, justify in a comment). Include a NaN guard.

Example (last line):

```
root = 1.521379706804 after 34 iterations
```

<details><summary>Hint</summary>
Keep `lo`, `hi`; `mid = 0.5 * (lo + hi)`. If `f(lo)` and `f(mid)` have the same sign, the root is in `[mid, hi]`. `fabs(hi - lo)` is the width.
</details>

---

**02.7 — Prime sieve with early exit**

Print all primes below 200 using trial division. For each candidate `n`, test divisors `d = 2, 3, ...` but `break` as soon as `d * d > n` or a divisor is found. Then print how many candidates you tested and how many division operations you performed in total. Then print the same list backwards using a `size_t` counter and the `i-- > 0` idiom (store the primes in an array `int primes[64]` first).

Example (first line):

```
2 3 5 7 11 13 ...
```

<details><summary>Hint</summary>
A flag `is_prime` set to 1 before the inner loop and cleared on the first divisor. Count divisions with a `long` counter incremented next to each `%`.
</details>

---

**02.8 — Argmax and softmax over logits (ML)**

Given `double logits[] = {2.0, 1.0, 0.1, -1.0, 3.5}`, compute the argmax with a loop, then compute softmax `exp(z_i - max) / sum_j exp(z_j - max)` (subtract the max first for numerical stability). Print each probability with 6 decimals and their sum. Then repeat for `{1000.0, 1001.0, 1002.0}` and show that subtracting the max prevents `inf`/`nan` (compute the naive version too and print it).

Example:

```
argmax = 4
p = 0.170 0.063 0.025 0.008 0.734 (sum 1.000000)
```

<details><summary>Hint</summary>
Two passes: first pass max, second pass exp and sum, third pass divide. `exp(1000.0)` overflows to `inf` in double.
</details>

---

**02.9 — Welford running statistics (numerics)**

Hard-code `double x[] = {1e8 + 4, 1e8 + 7, 1e8 + 13, 1e8 + 16}`. Compute the mean and sample variance three ways: (a) naive two-pass (`mean` then `sum((x-mean)^2)/(n-1)`), (b) the "textbook" one-pass formula `(sum(x^2) - sum(x)^2/n)/(n-1)`, (c) Welford's algorithm. Print all three variances with 6 decimals. Repeat with `float` arrays and explain in a comment which method fails and why. The correct answer is 30.

Example:

```
double: two-pass 30.000000  textbook 30.000000  welford 30.000000
float:  two-pass ...        textbook ...        welford ...
```

<details><summary>Hint</summary>
`sum(x^2)` for values near 1e8 is around 4e16, beyond float's 7 digits. `1e8f + 4` may not even be exactly representable in float. Welford only ever squares small deltas.
</details>

---

**02.10 — ASCII wave, spinning-donut warm-up (sims)**

Render `y = sin(x)` on a 60-column by 21-row grid of characters for `x` from 0 to 4π. For each row `r` (0 at top), compute the `y` value it represents (`+1` at row 0, `-1` at row 20). For each column `c`, compute `x = c * 4π / 59` and `y = sin(x)`; print `*` if `y` rounds to that row, `-` if the row is the axis (`y = 0`), else a space. Use nested loops, `switch` or `if` to choose the character, and `%c`. Then add a `for` loop that shifts the phase by `0.3` each frame and prints 5 frames separated by a blank line (this is the "animation loop" of the donut).

Example (one row somewhere near the top):

```
    ****                                 ****
```

<details><summary>Hint</summary>
Row index from y: `r = (int)lround((1.0 - y) * 10.0)`. Inner loop over columns; outer over rows; you must recompute `sin` per cell or precompute a `int row_of_col[60]` array per frame.
</details>
