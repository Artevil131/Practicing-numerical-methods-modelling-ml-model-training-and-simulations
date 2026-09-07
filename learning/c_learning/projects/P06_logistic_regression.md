# P06 — Logistic Regression

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C 01-09, 12   **Builds on:** P05, P04

## Goal

A binary classifier trained with minibatch SGD: sigmoid output, binary cross-entropy loss, gradient you derive by hand, evaluated by accuracy on a held-out set. Two datasets: synthetic 2-D Gaussian blobs (so you can export and plot the decision boundary) and MNIST digits 0 vs 1 (real 784-D data, > 99% accuracy expected).

## Why

The derivative $\partial L/\partial z = \sigma(z) - y$ you derive here is the cleanest instance of "loss gradient at the logits" — P11 generalizes it to softmax ($p - y$), and P12/P13/C++ P05 reuse it as the top of every backward pass. Minibatching, shuffling per epoch, train/val split, and the numerically stable loss are the training-loop scaffolding you will carry to every later model. This is also the first time your P04 MNIST loader meets your P02 matrices in a real model.

## The math

Model: $z = Xw + b$, $\hat p = \sigma(z) = \dfrac{1}{1 + e^{-z}}$, interpreted as $P(y = 1 \mid x)$.

Loss (mean binary cross-entropy over $n$ samples):

$$L = -\frac{1}{n}\sum_{i=1}^{n}\left[y_i \log \hat p_i + (1 - y_i)\log(1 - \hat p_i)\right]$$

Gradient derivation. Per sample, with $\sigma'(z) = \sigma(z)(1 - \sigma(z))$:

$$\frac{\partial L_i}{\partial \hat p_i} = -\frac{y_i}{\hat p_i} + \frac{1-y_i}{1-\hat p_i}, \qquad \frac{\partial L_i}{\partial z_i} = \frac{\partial L_i}{\partial \hat p_i}\,\hat p_i(1-\hat p_i) = \hat p_i - y_i$$

Then $z_i = x_i^Tw + b$ gives

$$\nabla_w L = \frac{1}{n}X^T(\hat p - y), \qquad \frac{\partial L}{\partial b} = \frac{1}{n}\sum_i(\hat p_i - y_i)$$

Note it is the linear-regression gradient with $\hat p$ in place of $X\theta$ and no factor 2.

**Numerical stability.** Never compute $\log(\sigma(z))$ as two steps: for $z = -40$, $\sigma(z)$ underflows to 0 and $\log 0 = -\infty$. Use the identity

$$-\left[y\log\sigma(z) + (1-y)\log(1-\sigma(z))\right] = \max(z, 0) - yz + \log(1 + e^{-|z|})$$

(this is what PyTorch's `BCEWithLogitsLoss` does). Also compute $\sigma$ itself branch-wise: for $z \ge 0$ use $1/(1+e^{-z})$; for $z < 0$ use $e^{z}/(1+e^{z})$.

**Decision boundary** in 2-D: $w_0 x_0 + w_1 x_1 + b = 0 \Rightarrow x_1 = -(w_0 x_0 + b)/w_1$.

**Accuracy**: fraction of samples with $\mathbb{1}[\hat p_i > 0.5] = y_i$.

## Spec

**CLI**

```
./logreg blobs <n_per_class> <sep> <seed> data.csv         # two Gaussians at (±sep/2, 0), unit variance
./logreg train data.csv <lr> <epochs> <batch> model.txt log.csv boundary.csv
./logreg mnist <images> <labels> <timages> <tlabels> <lr> <epochs> <batch> log.csv
```

**Files**: `data.csv` header `x0,x1,y`; `model.txt` = `w0 w1 ... b` on one line; `log.csv` header `epoch,train_loss,val_loss,val_acc`; `boundary.csv` = two points on the line (`x0,x1` per row).

**Signatures** (`logreg.c`):

```c
double  sigmoid(double z);                                   /* stable, both branches */
double  bce_with_logits(const Matrix *z, const Matrix *y);   /* mean, stable form */
Matrix *forward_logits(const Matrix *X, const Matrix *w, double b);   /* n x 1 */
void    grad_step(const Matrix *Xb, const Matrix *yb, Matrix *w, double *b, double lr);  /* one minibatch */
double  accuracy(const Matrix *X, const Matrix *y, const Matrix *w, double b);
void    shuffle_indices(size_t *idx, size_t n, unsigned seed);         /* Fisher-Yates */
Matrix *mat_take_rows(const Matrix *X, const size_t *idx, size_t k);   /* gather rows -> k x cols */
void    train_test_split(const Matrix *X, const Matrix *y, double frac, unsigned seed,
                         Matrix **Xtr, Matrix **ytr, Matrix **Xte, Matrix **yte);
```

**MNIST mode**: filter classes 0 and 1 from train (12665 images) and test (2115), relabel 0/1, train with batch 64, lr 0.1, 5 epochs. Expected test accuracy > 99.5%.

**Per-epoch output**

```
epoch 1  train_loss 0.0412  val_loss 0.0390  val_acc 0.9967
```

## Milestones

1. **M1 — stable sigmoid and BCE.** `sigmoid(-800)` returns 0 without a NaN; `bce_with_logits` with $z = -100, y = 1$ returns ~100, not `inf`. Compare 10 random `(z, y)` pairs with `torch.nn.functional.binary_cross_entropy_with_logits`.
2. **M2 — gradient check.** On 10 blob points, finite differences vs analytic gradient for each $w_j$ and $b$: relative error $< 10^{-6}$.
3. **M3 — full-batch GD on blobs.** Loss decreases every step; accuracy > 95% for `sep = 3`. Export `boundary.csv`, plot points colored by class plus the line.
4. **M4 — minibatch SGD with shuffling and a train/val split.** Same final accuracy, noisier loss curve, far fewer passes. Confirm that without shuffling and with sorted-by-class data, training gets worse.
5. **M5 — MNIST 0 vs 1.** > 99.5% test accuracy in 5 epochs; loss curve in `log.csv`. Save `w` as a `.mat` and view it as a 28x28 PGM with your P04 writer — you should see a light "1"-shaped stripe and a dark "0"-shaped ring.
6. **M6 — match sklearn.** Weights are not expected to match exactly (different regularization/solver), but accuracy and the sign pattern of $w$ should. Match the unregularized `LogisticRegression(penalty=None)` loss to within 1e-3 after enough epochs.

## Verification

```python
import numpy as np, pandas as pd, torch
d = pd.read_csv("data.csv"); X = d[["x0","x1"]].values; y = d.y.values
w = np.loadtxt("model.txt"); b = w[-1]; w = w[:-1]
z = torch.tensor(X @ w + b); yt = torch.tensor(y, dtype=torch.float64)
print(torch.nn.functional.binary_cross_entropy_with_logits(z, yt).item())   # your final train loss
from sklearn.linear_model import LogisticRegression
clf = LogisticRegression(penalty=None, max_iter=5000).fit(X, y)
print(clf.coef_, clf.intercept_, clf.score(X, y))    # close to your w, b, accuracy
# gradient check reference
p = 1/(1+np.exp(-(X@w+b))); print(X.T @ (p - y) / len(y), (p - y).mean())
```

## Stretch goals

- L2 regularization $\tfrac{\lambda}{2}\lVert w\rVert^2$ — derive the extra gradient term; show the decision boundary barely moves but $\lVert w\rVert$ shrinks.
- Learning-rate schedule (step decay or $1/t$); compare final loss after a fixed budget.
- Precision/recall/F1 and a threshold sweep (ROC curve to CSV, plot in Python).
- One-vs-rest: train 10 binary classifiers on full MNIST and combine by argmax — a preview of P11 (expect ~90%).

## Hints

- Keep `y` as an `n x 1` Matrix of 0.0/1.0 so `mat_sub(p, y)` is your $\partial L/\partial z$ directly.
- The epoch loop: shuffle an index array once per epoch, then slice it into batches and gather rows with `mat_take_rows`. The last batch may be smaller — handle it.
- Do not allocate `Xb` fresh per batch if you can help it: preallocate `batch x d` and copy rows in. Free temporaries from `mat_matmul` every step; ASan will show you if you forget.
- Initialize $w = 0, b = 0$ — logistic regression is convex, no random init needed.
- `sep` controls difficulty: with `sep = 1` blobs overlap and 100% accuracy is impossible; that is correct behavior, not a bug.
- The MNIST filter: build an index list of rows where the label is 0 or 1, then `mat_take_rows`.

## Where to put it

`neural_network_c/logreg/` — `logreg.c`, `train_utils.c`/`.h` (shuffle, split, take_rows — reused by P11/P12), `Makefile`, `plot_boundary.py`.
