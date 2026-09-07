# Chapter 13 — Exercises

Write each exercise as `ex13_K.c` in this folder (`ex13_1.c`, `ex13_2.c`, ...). Default compile:
`cc -Wall -Wextra -std=c11 -O2 -o ex13_K ex13_K.c -lm`. Several exercises tell you to use other
flags (`-g -O0`, `-fsanitize=...`, `-O3 -march=native`) — the flags *are* the exercise.

---

### 13.1 — **Warning hunt**

Write a 30-line program that intentionally triggers at least six *different* warnings under `-Wall -Wextra` (e.g. sign-compare, unused variable, uninitialized use, implicit conversion changing value, missing return, format mismatch). Record each warning message in a comment. Then fix all of them so it compiles clean with `-Werror`, and add `-Wshadow -Wconversion` and fix whatever else appears.

<details><summary>Hint</summary>
`printf("%d", 3.0)` and `size_t n = -1;` are two easy ones. Keep the broken version in a comment block for reference.
</details>

### 13.2 — **`DBG` macro and `assert` discipline**

Write a `DBG(fmt, ...)` macro that prints `[file:line func]` to `stderr` only when `-DDEBUG` is set. Write `mat_get(m, i, j)` with `assert` preconditions. Show (a) the assert message when you access out of bounds, (b) that with `-DNDEBUG` the same program reads garbage instead, (c) that `assert(counter++ < 10)` behaves differently with and without `-DNDEBUG` (print `counter` afterwards).

Example (debug build):
```
Assertion failed: (i < m->rows && j < m->cols), function mat_get, file ex13_2.c, line 21.
```

<details><summary>Hint</summary>
`##__VA_ARGS__` swallows the trailing comma. Case (c) is the lesson: never put side effects in asserts.
</details>

### 13.3 — **lldb session transcript**

Write a program with a deliberate NULL-pointer dereference three calls deep (`main → forward → mat_mul`). Compile with `-g -O0`, run it in `lldb`, and produce a transcript (paste into a comment block at the bottom of the file) showing: `run`, the crash location, `bt`, `frame select 1`, `print` of the local that is NULL, `print *some_valid_struct`, and `print arr[0]@4`. Then set a breakpoint on `mat_mul`, rerun, and `step` through 3 lines showing `frame variable` output.

<details><summary>Hint</summary>
`b mat_mul` then `r`. Use `n` to stay in the function, `s` to descend. `fr v` shows all locals.
</details>

### 13.4 — **Sanitizer zoo**

Write a program with five bugs behind `switch (atoi(argv[1]))`: (1) heap buffer read one past the end, (2) use-after-free, (3) double free, (4) signed integer overflow, (5) shift by 40 on a 32-bit int. Compile with `-fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer` and run each case. Paste the first 3 lines of each report into comments and, for the heap overflow, identify both the access line and the allocation line from the report. Finally run `leaks --atExit -- ./ex13_4 0` with a case that leaks and show the output.

<details><summary>Hint</summary>
For case (4) use a value from `argv` so `-O1` can't fold it. Case (1) needs `-fsanitize=address`; cases (4),(5) need `undefined`.
</details>

### 13.5 — **Mini test framework + TDD `Matrix`**

Build the `TEST`/`RUN`/`ASSERT_TRUE`/`ASSERT_EQ_INT`/`ASSERT_NEAR` framework and use it to develop `mat_new`, `mat_free`, `mat_get`/`mat_set`, `mat_identity`, `mat_mul`, `mat_transpose`, `mat_add`. Write each test *before* its function. Required tests: shape of `mat_new`; `I*A == A`; a hand-computed 2×3·3×2; `(A^T)^T == A`; `(AB)^T == B^T A^T` on random 3×4 and 4×2 matrices; `(AB)C ≈ A(BC)` for random 4×4. Exit code nonzero if any test fails. Run under `-fsanitize=address,undefined` too.

Example:
```
  mat_new_shape                            ok
  mul_identity                             ok
  ...
8 tests, 0 failed
```

<details><summary>Hint</summary>
For the random tests seed your xorshift RNG from chapter 12 with a constant so failures are reproducible. Use a helper `mat_allclose(a, b, tol)` returning int.
</details>

### 13.6 — **`TIMER` macros and the disappearing benchmark**

Write `TIMER_START(name)`/`TIMER_END(name)` using `clock_gettime(CLOCK_MONOTONIC)`. Time a dot product of two 10M-element arrays three ways: (a) result unused, (b) result stored to a `volatile double`, (c) result printed. Compile at `-O0`, `-O2`, `-O3 -march=native` and tabulate the 9 timings. Explain in a comment which cells are "the compiler deleted the loop" and which are real. Then implement warmup + 7 repeats and report min and median.

Example:
```
             -O0       -O2       -O3 -march=native
unused     18.2 ms    0.000 ms   0.000 ms   <- deleted
volatile   18.1 ms    4.1 ms     2.3 ms
printed    18.3 ms    4.1 ms     2.3 ms
```

<details><summary>Hint</summary>
Fill the arrays with RNG values, not constants — constant inputs can be folded even when the result is used. Use `qsort` to get the median.
</details>

### 13.7 — **Cache-line stride experiment**

Allocate a 64 MB `double` array. For `stride ∈ {1, 2, 4, 8, 16, 32, 64, 128}`, time a loop that touches every `stride`-th element (`a[i] += 1` for `i = 0, stride, 2·stride, ...`), keeping the *number of touched elements* constant (so the arithmetic work is identical — wrap around the array). Print ns per element. Explain the shape: flat until the stride exceeds the cache line (16 doubles on Apple Silicon = 128 bytes; 8 on x86), then roughly constant again because every access is now a separate line.

<details><summary>Hint</summary>
Do 3 repeats and take the minimum. Use `volatile` or print a checksum to keep the loop alive. Expect ~1 ns/elem at stride 1 and ~5-15 ns/elem at large stride.
</details>

### 13.8 — **Matmul loop-order + blocking** (ML / numerics)

Implement all six loop orders (`ijk, ikj, jik, jki, kij, kji`) for square `n×n` row-major double matmul, plus a blocked `ikj` with block size `B` as a parameter. For `n ∈ {256, 512, 1024}`, time each (warmup + min of 3), verify all results match the `ijk` reference to `1e-9` relative, and print GFLOP/s (`2n³ / t / 1e9`). Then try `B ∈ {16, 32, 64, 128}`. Compile with `-O2` and again with `-O3 -march=native -Rpass=loop-vectorize` and note which inner loops the compiler vectorized.

Example (one line):
```
n=512  ikj    0.041 s   6.5 GFLOP/s   vs ijk 0.30 s   0.9 GFLOP/s   speedup 7.3x
```

<details><summary>Hint</summary>
Blocked version: `for ii step B: for kk step B: for jj step B: for i in block: for k in block: a = A[i][k]; for j in block: C[i][j] += a*B[k][j]`. Handle `n % B != 0` or restrict `n` to multiples of `B`.
</details>

### 13.9 — **Gradient check a 2-layer MLP** (ML)

Implement forward for a tiny MLP: `x (1×3) → W1 (3×4) + b1 → tanh → W2 (4×2) + b2 → softmax → cross-entropy` against a one-hot target, all in `double`. Implement the analytic backward pass for all four parameter tensors. Write `grad_check` (central differences, `h = 1e-5`, relative error) and run it over every parameter. Print the max relative error per tensor; a correct implementation gives `< 1e-7`. Then *introduce a bug* (drop the `1 - tanh²` factor) and show that the check catches it (error ~1e-1). Package it as `TEST(mlp_gradients)` in your framework.

<details><summary>Hint</summary>
dL/dlogits for softmax + cross-entropy is `p - onehot`. `dW2 = h^T · dlogits`, `dh = dlogits · W2^T`, `dz1 = dh * (1 - h²)`, `dW1 = x^T · dz1`. Flatten all parameters into one array so `grad_check` is a single loop.
</details>

### 13.10 — **N-body: AoS vs SoA, branch predictability, and `restrict`** (sims)

Implement one step of O(N²) gravitational acceleration for N = 4096 bodies two ways: array-of-structs `Body{x,y,z,vx,vy,vz,m}` and struct-of-arrays. Time both (warmup, min of 5) at `-O2` and `-O3 -march=native`. Add `restrict` to the SoA function's pointer parameters and re-time. Use `-Rpass=loop-vectorize` to confirm which inner loops vectorized. Then add a softening `if (r2 < eps) continue;` branch versus branchless `r2 = fmax(r2, eps)` and time both. Print a table of all variants with ns per pair-interaction, and verify all variants produce the same accelerations to `1e-9`.

Example:
```
variant               -O2 (ns/pair)   -O3 native (ns/pair)   vectorized?
AoS                        3.1              2.9                no
SoA                        2.4              1.1                yes
SoA + restrict             2.3              0.8                yes
SoA + branch               2.6              2.5                no
```

<details><summary>Hint</summary>
Inner loop: `dx = x[j]-x[i]; r2 = dx²+dy²+dz²+eps; inv = 1/sqrt(r2); inv3 = inv*inv*inv; ax += dx*inv3*m[j]`. The `if/continue` prevents vectorization; `fmax` does not.
</details>
