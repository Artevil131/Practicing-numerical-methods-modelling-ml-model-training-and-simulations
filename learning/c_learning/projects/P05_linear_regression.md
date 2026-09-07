# P05 — Linear Regression

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C 01-09, 12   **Builds on:** P02

## Goal

Two solvers for the same problem — fit $y = wx + b$ to noisy synthetic data — that must agree: the closed-form normal-equations solution (which requires you to write a small Gaussian elimination) and batch gradient descent (your first training loop). Both log MSE per iteration to CSV; you plot the convergence in Python. Then generalize to $d$ features.

## Why

This is the smallest possible "model": a linear map, a loss, a gradient, a loop. The gradient you derive by hand here ($X^T(Xw - y)$) reappears unchanged as the last layer of P06, P11, P12 and as the `Linear` backward in P13 and C++ P03. The Gaussian elimination is the seed of the C++ P06 numerical library (LU, pivoting). The CSV logging + Python plotting workflow is how you will inspect every later training run and simulation.

## The math

Design matrix $X \in \mathbb{R}^{n \times (d+1)}$ with a column of ones appended (so the bias is just another weight), parameters $\theta \in \mathbb{R}^{d+1}$, targets $y \in \mathbb{R}^n$.

**Loss** (mean squared error):

$$L(\theta) = \frac{1}{n}\lVert X\theta - y \rVert^2 = \frac{1}{n}\sum_{i=1}^{n}(x_i^T\theta - y_i)^2$$

**Gradient** — expand $\lVert X\theta - y\rVert^2 = \theta^T X^T X \theta - 2y^T X\theta + y^Ty$, differentiate w.r.t. $\theta$:

$$\nabla_\theta L = \frac{2}{n} X^T (X\theta - y)$$

**Closed form** — set the gradient to zero: $X^TX\theta = X^Ty$ (the *normal equations*). For $d = 1$ this is a $2\times2$ system; solve it in general with Gaussian elimination with partial pivoting on the $(d+1)\times(d+1)$ matrix $A = X^TX$ and right-hand side $b = X^Ty$.

**Gradient descent**: $\theta \leftarrow \theta - \eta \nabla_\theta L$. Converges for $0 < \eta < 2/\lambda_{\max}(\tfrac{2}{n}X^TX)$. For unnormalized $x \in [0, 10]$ that bound is small (try $\eta = 0.1$ and watch it diverge, then $0.01$); standardizing features fixes it. Compute $\lambda_{\max}$ for the $2\times2$ case by hand ($\lambda = \tfrac{\text{tr} \pm \sqrt{\text{tr}^2 - 4\det}}{2}$) and check the bound empirically.

**Gaussian elimination with partial pivoting** on $A\theta = b$: for column $k$, swap in the row with largest $|a_{ik}|$, $i \ge k$; eliminate below; back-substitute. $O(m^3)$ for an $m\times m$ system.

## Spec

**CLI**

```
./linreg gen <n> <noise_std> <seed> data.csv          # y = 3x + 2 + N(0, noise)
./linreg closed data.csv                              # prints w, b, mse
./linreg gd data.csv <lr> <iters> log.csv             # prints w, b, mse; log has iter,mse,w,b
./linreg gen_multi <n> <d> <seed> data.csv            # d features, random true theta printed
```

**Data CSV**: header `x,y` (or `x0,x1,...,y`), one sample per line, `%.10g` precision.

**Signatures** (`linreg.c`, using `matrix.h`):

```c
Matrix *csv_load(const char *path, size_t *out_rows, size_t *out_cols, int skip_header);
Matrix *add_bias_column(const Matrix *X);                      /* n x (d+1), last col = 1 */
double  mse(const Matrix *X, const Matrix *theta, const Matrix *y);
Matrix *mse_grad(const Matrix *X, const Matrix *theta, const Matrix *y);   /* (d+1) x 1 */
int     solve_gauss(Matrix *A, Matrix *b, Matrix *out_x);      /* A, b modified; 0 = ok, 1 = singular */
Matrix *fit_closed_form(const Matrix *X, const Matrix *y);
Matrix *fit_gd(const Matrix *X, const Matrix *y, double lr, int iters, FILE *log);
```

**Output example**

```
$ ./linreg gen 200 1.0 42 data.csv
$ ./linreg closed data.csv
theta = [2.9873, 2.0411]   (w, b)
mse   = 0.9634
$ ./linreg gd data.csv 0.01 2000 log.csv
iter 0     mse 89.23
iter 500   mse 0.9702
iter 1999  mse 0.9634
theta = [2.9873, 2.0411]
```

## Milestones

1. **M1 — generate and load.** `gen` writes 200 points; `csv_load` reads them back into an `n x 2` Matrix; you'll know it works when `pandas.read_csv` gives the same means.
2. **M2 — `solve_gauss`.** Test on a $3\times3$ system with a known solution and on one that needs pivoting (zero in position $(0,0)$). Compare with `np.linalg.solve`.
3. **M3 — closed form.** `w` within ~0.05 of 3 and `b` within ~0.15 of 2 for `n=200, noise=1`. Compare against `np.polyfit(x, y, 1)` to 1e-8.
4. **M4 — MSE and gradient.** Check the gradient with finite differences: $(L(\theta + \epsilon e_j) - L(\theta - \epsilon e_j))/2\epsilon$ vs your analytic gradient, $\epsilon = 10^{-5}$, relative error $< 10^{-6}$.
5. **M5 — gradient descent.** Converges to the closed-form $\theta$ to 1e-4 with `lr=0.01, iters=2000`. Plot `log.csv` (iter vs mse, log-scale y) — a straight-ish line going down then flattening at the noise floor. Show it diverging with `lr=0.1`.
6. **M6 — multi-feature.** `gen_multi` with $d=5$; closed form recovers the true $\theta$; GD converges after standardizing columns.

## Verification

```python
import numpy as np, pandas as pd
d = pd.read_csv("data.csv")
w, b = np.polyfit(d.x, d.y, 1)
print(w, b)                                   # match your closed-form to ~1e-8
X = np.c_[d.x, np.ones(len(d))]
theta = np.linalg.solve(X.T @ X, X.T @ d.y)  # same numbers
print(((X @ theta - d.y)**2).mean())         # your mse
log = pd.read_csv("log.csv")
import matplotlib.pyplot as plt
plt.semilogy(log.iter, log.mse); plt.show()   # monotone decrease to the noise floor
```

## Stretch goals

- Ridge regression: add $\lambda\lVert\theta\rVert^2$ (excluding bias) to the loss; closed form becomes $(X^TX + \lambda I)\theta = X^Ty$. Show it shrinks $\theta$ as $\lambda$ grows.
- Polynomial features $x, x^2, \dots, x^p$ for a sine-shaped target; watch overfitting at $p=15$ on 20 points.
- Stochastic and minibatch GD; log per-epoch loss and compare noise in the curves.
- Report the condition number of $X^TX$ (ratio of eigenvalues via power iteration on $A$ and $A^{-1}$) before and after standardization.

## Hints

- Store $\theta$ as a `(d+1) x 1` Matrix so `mat_matmul(X, theta)` gives predictions `n x 1` and `mse_grad` is `mat_matmul(mat_transpose(X), residual)` scaled by $2/n$. You will allocate temporaries — free them every iteration or the loop leaks megabytes.
- Parse CSV with `strtok`/`strtod` line by line (`fgets` with a generous buffer); count rows on a first pass or grow with `realloc`.
- In Gaussian elimination, operate on copies (`mat_copy`) — the caller's `A` and `b` get destroyed otherwise, which the signature admits; just be deliberate.
- Partial pivoting: swap whole rows of `A` *and* the corresponding entries of `b`. A pivot with $|a_{kk}| < 10^{-12}$ means singular.
- Log with `fprintf(log, "%d,%.10g,%.10g,%.10g\n", ...)` and flush at the end; it's fine to log every iteration at this size.
- For the finite-difference check write a tiny `loss_at(theta)` helper; you will reuse this exact pattern in P12's gradient check.

## Where to put it

`neural_network_c/linreg/` — `linreg.c`, `csv.c`, `csv.h`, `gauss.c`, `gauss.h` (you will move `gauss` to `numerical_methods/` later), `Makefile`, `plot.py`.
