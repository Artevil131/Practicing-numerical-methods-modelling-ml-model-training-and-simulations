# Chapter 03 — Exercises

Write each exercise as `ex03_K.c` in this folder (exercise 03.7 also needs `ex03_7.h` and a second `.c` file). Compile with:

```sh
cc -Wall -Wextra -std=c11 -O2 -o ex03_K ex03_K.c -lm
```

Zero warnings required. Every function other than `main` and header-declared ones must be `static`.

---

**03.1 — Prototype order**

Write `main` FIRST in the file, then three functions below it: `int square(int)`, `double average(int a, int b)` (must return 2.5 for `(2, 3)`), and `void print_line(int n)` which prints `n` dashes and a newline. Make the file compile by adding prototypes above `main`. Then, in a comment, paste the exact error message you get when you delete one prototype.

Example:

```
square(7) = 49
average(2,3) = 2.5
----------
```

<details><summary>Hint</summary>
The average needs a cast before the division. The error mentions "undeclared" or "implicit declaration".
</details>

---

**03.2 — By-value demonstration**

Write `static void bump_scalar(int n)` that does `n += 100` and prints `n`, and `static void bump_array(int a[], int n)` that adds 100 to every element. Call both from `main` on `int x = 1;` and `int arr[3] = {1, 2, 3};`, then print `x` and `arr`. Also print `sizeof(arr)` in `main` and `sizeof(a)` inside `bump_array` and explain the difference in a comment.

Example:

```
inside bump_scalar: 101
x after: 1
arr after: 101 102 103
sizeof in main: 12, sizeof in function: 8
```

<details><summary>Hint</summary>
Inside the function `a` is really an `int *`. Use `%zu`.
</details>

---

**03.3 — Struct return**

Define `struct stats { double mean, min, max; int n; }` and `static struct stats vec_stats(const double *v, int n)`. Call it on `{2.5, -1.0, 7.0, 3.5}` and print all four fields. Handle `n == 0` by returning all zeros (test it).

Example:

```
n=4 mean=3.0000 min=-1.0000 max=7.0000
n=0 mean=0.0000 min=0.0000 max=0.0000
```

<details><summary>Hint</summary>
`struct stats r = {0};` zero-initializes every field. Initialize `min` and `max` from `v[0]` only after checking `n > 0`.
</details>

---

**03.4 — Recursion three ways**

Implement `static long fib_naive(int n)`, `static long fib_memo(int n)` (using a `static long cache[91]` inside the function, with 0 meaning "not computed"), and `static long fib_iter(int n)`. Print `fib(40)` from each. Add a `static long call_count` global incremented inside `fib_naive` and print it. Then print `fib_iter(90)` and explain in a comment why `fib_iter(93)` is not safe in a `long`.

Example:

```
fib_naive(40) = 102334155 (331160281 calls)
fib_memo(40)  = 102334155
fib_iter(40)  = 102334155
fib_iter(90)  = 2880067194370816120
```

<details><summary>Hint</summary>
`fib(93)` exceeds `LONG_MAX`. In `fib_memo`, check `cache[n] != 0` before recursing (fib(0) = 0 is the only zero result; handle `n < 2` as the base case before the cache lookup).
</details>

---

**03.5 — Static local: a linear congruential RNG**

Write `static unsigned rng_next(void)` that keeps a `static unsigned state` (seed 12345) and updates it with `state = state * 1103515245u + 12345u;` returning `state >> 16`. Write `static double rng_uniform(void)` returning a value in `[0, 1)` using `rng_next() % 32768 / 32768.0`. Print 5 uniforms. Then estimate π by Monte Carlo with 1,000,000 samples (fraction of points with `x*x + y*y < 1` times 4). In a comment, note why this design (hidden static state) makes it impossible to run two independent streams, and sketch the struct-based signature you would use instead.

Example:

```
0.xxxx 0.xxxx 0.xxxx 0.xxxx 0.xxxx
pi ~ 3.14xxxx
```

<details><summary>Hint</summary>
Unsigned multiplication wrapping is defined behavior; that is why LCGs use unsigned. The integer-to-double division needs one operand to be double.
</details>

---

**03.6 — Stack depth probe**

Write `static int depth(int n)` that declares a local `char pad[1024];`, writes `pad[0] = (char)n;` (so the array cannot be optimized away), prints `n` every 1000 levels, and calls `depth(n + 1)`. Call it from `main` with a command-line-free hard cap: stop at `n == 5000` and return normally, printing `reached 5000`. Then raise the cap to 20000 and observe what happens (segfault). In a comment, compute the approximate stack use per frame and compare with 8 MB.

Example:

```
1000
2000
...
reached 5000
```

<details><summary>Hint</summary>
Compile with `-O0` for this one so the frames are not collapsed by tail-call optimization. 8 MB / 1 KB frames is about 8000 levels.
</details>

---

**03.7 — Header and two translation units**

Create `ex03_7.h` with an include guard declaring `double vec_dot(const double *a, const double *b, int n);`, `double vec_norm(const double *a, int n);`, and a `static inline double sq(double x)`. Create `ex03_7_vec.c` defining `vec_dot` and `vec_norm` (which must use `vec_dot`). Create `ex03_7.c` with `main` computing the dot product and norms of `{1, 2, 3}` and `{4, -5, 6}` and the cosine of the angle between them. Compile with `cc ... -o ex03_7 ex03_7.c ex03_7_vec.c -lm`. Then include the header twice in `ex03_7.c` and confirm it still compiles.

Example:

```
dot = 12
|a| = 3.741657  |b| = 8.774964
cos = 0.365521
```

<details><summary>Hint</summary>
`ex03_7_vec.c` must `#include "ex03_7.h"` and `<math.h>`. Without the include guard, the second include redefines `sq`.
</details>

---

**03.8 — Numerical derivative and Newton's method (numerics)**

Write `static double f(double x)` returning `cos(x) - x`, `static double dfdx(double x, double h)` computing the central difference `(f(x+h) - f(x-h)) / (2h)`, and `static double newton(double x0, double tol, int max_iter, int *iters_out)` -- for now, since pointers come in chapter 05, instead return the root and print the iteration count inside `newton`. Solve from `x0 = 1.0` with `tol = 1e-12`. Then print the derivative error `|dfdx(1.0, h) - (-sin(1.0) - 1)|` for `h = 1e-1, 1e-2, ..., 1e-10` and identify in a comment the `h` that minimizes error and why smaller `h` gets worse.

Example:

```
root = 0.739085133215 (6 iterations)
h=1e-01 err=...
h=1e-02 err=...
```

<details><summary>Hint</summary>
Newton: `x = x - f(x) / dfdx(x, h)`. Truncation error shrinks like h^2; rounding error grows like eps/h; the optimum is around `h ~ cbrt(eps)` ~ 6e-6.
</details>

---

**03.9 — Composable activation functions (ML)**

Write `static double relu(double)`, `static double sigmoid(double)` (use `1 / (1 + exp(-x))`), `static double tanh_act(double)` (call `tanh`), and `static double gelu(double)` (`0.5 * x * (1 + erf(x / sqrt(2)))`). Write `static void apply_all(double *v, int n, int which)` that applies the activation selected by `which` (0..3) via `switch` to every element in place. Apply each to a copy of `{-2, -1, -0.5, 0, 0.5, 1, 2}` and print the results in a table with 4 decimals. Also write and print the derivative of sigmoid at each point using the identity `s * (1 - s)`.

Example:

```
x       relu    sigmoid tanh    gelu
-2.0000 0.0000  0.1192 -0.9640 -0.0455
```

<details><summary>Hint</summary>
Copy the input array before each activation (`memcpy` from `<string.h>`, or a loop). `erf` is in `<math.h>`.
</details>

---

**03.10 — RK4 integrator as a function taking the step count (sims)**

Write `static double rhs(double t, double y)` returning `-2.0 * t * y` (solution `y = exp(-t^2)` for `y(0) = 1`). Write `static double rk4_step(double t, double y, double dt)` implementing one classical Runge-Kutta 4 step using `rhs`. Write `static double integrate(double y0, double t_end, int n_steps)` that calls `rk4_step` `n_steps` times and returns `y(t_end)`. Print the absolute error at `t_end = 2` for `n_steps = 10, 20, 40, 80, 160` and the ratio of consecutive errors (should approach 16, confirming 4th order). Also implement `euler_step` and show its ratio approaches 2.

Example:

```
n=  10 rk4 err=...  ratio=...   euler err=...  ratio=...
n=  20 rk4 err=...  ratio=...   euler err=...  ratio=...
```

<details><summary>Hint</summary>
RK4: `k1 = f(t, y)`, `k2 = f(t + dt/2, y + dt/2 * k1)`, `k3 = f(t + dt/2, y + dt/2 * k2)`, `k4 = f(t + dt, y + dt * k3)`, `y += dt/6 * (k1 + 2k2 + 2k3 + k4)`. Compute `dt = t_end / n_steps` as a double.
</details>
