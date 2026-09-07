# Chapter 11 — Exercises

Write each exercise as `ex11_K.c` in this folder (`ex11_1.c`, `ex11_2.c`, ...). Compile with
`cc -Wall -Wextra -std=c11 -O2 -o ex11_K ex11_K.c -lm`. Zero warnings.

---

### 11.1 — **Declare and call**

Declare a function pointer variable (no `typedef`) that can hold `sqrt`, `fabs`, and `exp`. Assign each in turn and print the result of calling it with `2.0`. Then write the same program using a `typedef`.

Example:
```
sqrt(2.0) = 1.414214
fabs(2.0) = 2.000000
exp(2.0)  = 7.389056
```

<details><summary>Hint</summary>
The type is "pointer to function taking double, returning double". Put the `*` and the name inside parentheses.
</details>

### 11.2 — **`map` and `reduce`**

Write `void map_inplace(double *xs, size_t n, double (*f)(double))` and `double reduce(const double *xs, size_t n, double init, double (*op)(double, double))`. Use them to compute the sum of squares of `{1, 2, 3, 4}` by mapping `square` then reducing with `add`.

Example: `sum of squares = 30.000000`

<details><summary>Hint</summary>
`reduce` is a loop: `acc = op(acc, xs[i])`. Same as Python's `functools.reduce`.
</details>

### 11.3 — **Integrate anything**

Write `double integrate(double (*f)(double), double a, double b, int n)` using the trapezoidal rule. Test on `sin` over `[0, π]` (expect 2), `exp` over `[0, 1]` (expect e − 1 ≈ 1.718282), and your own `x*x` over `[0, 3]` (expect 9). Print the absolute error for `n = 10, 100, 1000` and observe how it shrinks.

<details><summary>Hint</summary>
Trapezoid: `h * (f(a)/2 + f(b)/2 + Σ f(a + i*h))` for `i = 1..n-1`. Error should drop ~100x per 10x increase in n.
</details>

### 11.4 — **Newton with a convergence report**

Write `double newton(double (*f)(double), double (*df)(double), double x0, double tol, int max_iter, int *iters_out)` that returns the root and writes the number of iterations taken via the out-parameter. Solve `x³ − x − 2 = 0` from `x0 = 1.5` and `cos(x) − x = 0` from `x0 = 1`.

Example:
```
root = 1.521380  (iters = 5)
root = 0.739085  (iters = 4)
```

<details><summary>Hint</summary>
For `cos(x) − x` you need a second function for the derivative `−sin(x) − 1`. Stop when `|f(x)| < tol`.
</details>

### 11.5 — **Activation dispatch table**

Define `enum { ACT_RELU, ACT_SIGMOID, ACT_TANH, ACT_COUNT }`, a table `act_fns[ACT_COUNT]` of function pointers, a parallel `act_dfns[ACT_COUNT]` of derivatives, and `act_names[ACT_COUNT]`. Print a table: for `x ∈ {-2, -1, 0, 1, 2}` and each activation, print `name(x)` and `name'(x)`. Use designated initializers so the tables are indexed by enum.

Example (one row):
```
sigmoid( 1.0) = 0.731059   d/dx = 0.196612
```

<details><summary>Hint</summary>
sigmoid'(x) = s(1−s); tanh'(x) = 1 − tanh²; relu'(x) = x > 0. Loop over the enum with `for (int a = 0; a < ACT_COUNT; a++)`.
</details>

### 11.6 — **Closure via `void *ctx`**

Write `double integrate_ctx(double (*f)(double, void *), void *ctx, double a, double b, int n)`. Define a struct holding polynomial coefficients `{double c[4]; int deg;}` and a callback `poly_eval(x, ctx)` that evaluates it. Integrate `3x² + 2x + 1` over `[0, 2]` (expect 14) and `x³` over `[0, 1]` (expect 0.25) with the *same* callback and different ctx structs.

<details><summary>Hint</summary>
The callback casts `ctx` back: `const Poly *p = ctx;`. The integrator never touches `ctx`, just forwards it.
</details>

### 11.7 — **`qsort` two ways, plus `bsearch`**

Given `struct { char name[16]; double loss; int epoch; }` records (make up 6), sort (a) by ascending loss, (b) by descending epoch then name ascending. Print both orders. Then `bsearch` for a record by name in the name-sorted array and print its loss.

<details><summary>Hint</summary>
For doubles use `(a > b) - (a < b)`. For `bsearch` the key is a pointer to a struct with only `name` filled in (or to a string, if your comparator only looks at `name`).
</details>

### 11.8 — **Generic `Vec` for a tokenizer** (ML)

Implement the `void*`-based `Vec` (`vec_new(elem_size)`, `vec_push`, `vec_at`, `vec_free`). Read tokens (whitespace-separated words) from a hard-coded string, store `struct { char tok[32]; int count; }` records in the `Vec` — incrementing `count` if the token already exists — then `qsort` by count descending and print the top 5. This is the first step of BPE: counting symbol frequencies.

Example (input `"the cat sat on the mat the end"`):
```
the 3
cat 1
...
```

<details><summary>Hint</summary>
Linear search for existing tokens is fine here. `qsort(v.data, v.len, v.elem_size, cmp)` works directly on the raw buffer.
</details>

### 11.9 — **`VEC_DEFINE(T)` and a `Layer` interface** (ML)

Write `VEC_DEFINE(T)` generating `Vec_T`, `vec_T_push`, `vec_T_free`. Then define `struct Layer` with `forward`, `backward`, `free` function pointers and a `void *state`. Implement two concrete layers: `Scale` (multiplies by a stored `k`; backward multiplies grad by `k`) and `ReLU`. Store `Layer` values in a `Vec_Layer` (typedef `Layer` first), run a forward pass over `{-1, 0.5, 2}` through `[Scale(3), ReLU]`, then a backward pass with `grad_out = {1,1,1}`.

Example:
```
forward:  -3.0 1.5 6.0 -> 0.0 1.5 6.0
backward: grad_in = 0.0 3.0 3.0
```

<details><summary>Hint</summary>
Backward runs layers in reverse order. Each layer caches what it needs during forward (ReLU: mask; Scale: nothing but `k`).
</details>

### 11.10 — **Scalar autograd `Value` with `backward` pointers** (ML)

Build a micrograd-style `Value` (`data`, `grad`, two parents, `BackwardFn backward`). Implement `val_add`, `val_mul`, `val_tanh` each setting its own backward function. Build `L = tanh(a*b + c)` with `a=2, b=-3, c=5`, set `L->grad = 1`, walk nodes in reverse topological order calling `backward`, and print `dL/da`, `dL/db`, `dL/dc`. Check against finite differences: perturb `a` by `1e-6`, recompute `L`, compare.

Example:
```
dL/da = -3 * (1 - tanh(-1)^2) = -1.259923
fd    = -1.259923
```

<details><summary>Hint</summary>
Topological order for this tiny graph is just the creation order reversed. `tanh` backward: `parent->grad += (1 - out*out) * self->grad`.
</details>
