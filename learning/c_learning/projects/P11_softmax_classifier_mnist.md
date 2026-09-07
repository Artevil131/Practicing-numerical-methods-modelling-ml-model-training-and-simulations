# P11 — Softmax Classifier on MNIST

**Difficulty:** ★★★☆☆   **Prereq chapters:** C 01-09, 12   **Builds on:** P06, P04

## Goal

A 10-class linear classifier (softmax regression) on all of MNIST: logits $Z = XW + \mathbf b$, a numerically stable softmax, cross-entropy loss, the gradient with respect to $W$ and $\mathbf b$ in matrix form, minibatch SGD, ~92% test accuracy, a confusion matrix, and the 10 rows of $W$ saved as PGM images (they look like ghostly digit templates).

## Why

This is the output layer of every classifier you will ever build. The result $\partial L/\partial Z = (P - Y)/n$ is the gradient that P12's backprop starts from, that P13's autograd must reproduce, and that the C++ transformer's training loop uses for next-token prediction (vocabulary size instead of 10). Softmax + log-sum-exp stability is the single most common numerical bug in from-scratch ML code; you fix it here once. The weight-image visualization is the first look at "what did the model learn".

## The math

$X \in \mathbb{R}^{n\times 784}$, $W \in \mathbb{R}^{784\times 10}$, $\mathbf b \in \mathbb{R}^{1\times 10}$. Logits $Z = XW + \mathbf 1\mathbf b$. Softmax per row:

$$P_{ic} = \frac{e^{Z_{ic}}}{\sum_{k} e^{Z_{ik}}}$$

**Stability.** $e^{Z}$ overflows for $Z > 709$. Subtract the row max first — softmax is invariant to it:

$$P_{ic} = \frac{e^{Z_{ic} - m_i}}{\sum_k e^{Z_{ik} - m_i}}, \quad m_i = \max_k Z_{ik}$$

and compute the loss via log-sum-exp, never `log(P)`:

$$\log P_{ic} = Z_{ic} - m_i - \log\sum_k e^{Z_{ik} - m_i}$$

**Cross-entropy** with one-hot targets $Y$ ($Y_{ic} = 1$ iff $y_i = c$):

$$L = -\frac1n\sum_{i=1}^n \log P_{i, y_i}$$

**Gradient derivation.** For one sample, $L_i = -Z_{y} + \log\sum_k e^{Z_k}$. Then

$$\frac{\partial L_i}{\partial Z_c} = -\mathbb 1[c = y] + \frac{e^{Z_c}}{\sum_k e^{Z_k}} = P_c - Y_c$$

So $\dfrac{\partial L}{\partial Z} = \dfrac1n(P - Y)$, and via $Z = XW + \mathbf 1 \mathbf b$:

$$\nabla_W L = \frac1n X^T(P - Y), \qquad \nabla_{\mathbf b} L = \frac1n \mathbf 1^T (P - Y) = \frac1n \sum_i (P_i - Y_i)$$

(The transpose rule: for $Z = XW$, $\partial L/\partial W = X^T\,\partial L/\partial Z$ and $\partial L/\partial X = \partial L/\partial Z\; W^T$ — you need the second one in P12.)

Sanity: the initial loss with $W = 0$ is $\log 10 \approx 2.3026$ exactly.

## Spec

**CLI**

```
./softmax <train_images> <train_labels> <test_images> <test_labels> <lr> <epochs> <batch> <seed> outdir/
```

Writes `outdir/log.csv` (`epoch,train_loss,test_loss,test_acc`), `outdir/W.mat`, `outdir/b.mat`, `outdir/w_0.pgm ... w_9.pgm`, `outdir/confusion.txt`.

**Signatures** (`softmax.c`, plus shared `nn_ops.h`/`nn_ops.c` that P12 extends):

```c
void    softmax_rows_inplace(Matrix *Z);                          /* Z -> P, stable */
double  cross_entropy_from_logits(const Matrix *Z, const int *y);  /* mean, log-sum-exp form */
Matrix *one_hot(const int *y, size_t n, int n_classes);
Matrix *logits(const Matrix *X, const Matrix *W, const Matrix *b);   /* XW + b (add_rowvec) */
void    softmax_grad(const Matrix *X, const Matrix *P, const Matrix *Y,
                     Matrix *dW, Matrix *db);                      /* fills preallocated dW, db */
double  accuracy_multiclass(const Matrix *P_or_Z, const int *y);
void    confusion_matrix(const Matrix *P_or_Z, const int *y, int n_classes, int *out /* C x C */);
void    sgd_update(Matrix *param, const Matrix *grad, double lr);
```

**Expected numbers** (lr 0.5 on mean loss, batch 128, 10 epochs, no regularization): train loss from 2.30 to ~0.28; test accuracy ~92.0-92.5%. Takes well under a minute.

**Confusion matrix** (`confusion.txt`): 10 rows, true class = row, predicted = column, integer counts; diagonal dominates; the largest off-diagonal entries should be 4→9, 7→9, 5→3, 8→5.

## Milestones

1. **M1 — stable softmax + CE.** Softmax of `[1000, 1000, 1000]` is `[1/3, 1/3, 1/3]` with no NaN. Loss at $W=0$ is 2.302585. Compare 5 random `(Z, y)` against `torch.nn.functional.cross_entropy`.
2. **M2 — gradient check** on 20 samples with random $W$: finite-difference vs analytic for 30 random entries of $W$ and all of $\mathbf b$; relative error $< 10^{-6}$.
3. **M3 — full-batch GD on 1000 training images**, 200 steps: loss decreases monotonically; train accuracy > 90% on those 1000.
4. **M4 — minibatch SGD on all 60k**: 92% test accuracy after 10 epochs; `log.csv` plotted shows test loss flattening around 0.28-0.30.
5. **M5 — visualizations.** `w_c.pgm` (reshape column $c$ of $W$ to 28x28, scale min→0, max→255): each looks like a blurry template of digit $c$ with a dark "anti-template". Confusion matrix printed.
6. **M6 — preallocated training loop.** No `malloc` inside the batch loop (use `*_into` variants or reuse buffers); time per epoch printed; ASan clean.

## Verification

```python
import numpy as np, torch
from load_mat import load_mat
W, b = load_mat("out/W.mat"), load_mat("out/b.mat")
# X_test, y_test from the P04 reader (in [0,1])
Z = X_test @ W + b
print((Z.argmax(1) == y_test).mean())                    # your test_acc, ~0.92
Zt = torch.tensor(Z); yt = torch.tensor(y_test, dtype=torch.long)
print(torch.nn.functional.cross_entropy(Zt, yt).item())   # your test_loss
# gradient reference on a batch
P = np.exp(Z - Z.max(1, keepdims=True)); P /= P.sum(1, keepdims=True)
Y = np.eye(10)[y_test]
print((X_test.T @ (P - Y) / len(y_test))[:3, :3], (P - Y).mean(0))
# sklearn baseline: LogisticRegression(multi_class="multinomial", max_iter=200) ~ 92.5%
```

## Stretch goals

- L2 weight decay: derive the gradient term, sweep $\lambda$, watch test accuracy move by ~0.3%.
- Learning-rate decay and momentum ($v \leftarrow \beta v + g$, $W \leftarrow W - \eta v$) — measure epochs to 92%.
- Per-pixel standardization (subtract mean image, divide by std with $\epsilon$) — converges faster with the same lr.
- Export the 10 most confidently *wrong* test images as a PGM grid with true/predicted labels in the filename.

## Hints

- Keep labels as `int *y` and only materialize `one_hot` per batch (or compute $P - Y$ by subtracting 1 at index $y_i$ of row $i$ — no one-hot matrix at all).
- Row max and row sum are loops over `cols` inside a loop over `rows`; write them once in `nn_ops.c` as `row_max`, `row_logsumexp` — P12 and P16 need them.
- Use $lr = 0.5$ for *mean* loss; if you accidentally use *summed* loss the effective lr is 128x larger and training explodes — a classic.
- `accuracy_multiclass` needs only `mat_argmax_row` per row; it works on logits or probabilities identically.
- For the weight images, scale each column independently by its own min/max; the mean of $W$ is near zero so gray = 0, white = positive evidence, black = negative.
- Track the time per epoch with `clock_gettime`; a slow epoch (> 10 s) means your matmul loop order is wrong (P02 stretch).

## Where to put it

`neural_network_c/softmax/` — `softmax.c`, `nn_ops.h`, `nn_ops.c`, `Makefile`, `plot_log.py`. P12 moves `nn_ops` up to `neural_network_c/nn/`.
