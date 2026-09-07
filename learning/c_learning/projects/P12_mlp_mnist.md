# P12 — Multilayer Perceptron on MNIST

**Difficulty:** ★★★★☆   **Prereq chapters:** C 11, 13 (plus 01-10)   **Builds on:** P11, P02

## Goal

First a hard-coded 2-layer network (784 → 128 → 10, ReLU) with backprop derived by hand in matrix form and verified by finite differences; then a general N-layer network built from a `Layer` struct with `forward`/`backward` function pointers; optimizers SGD → momentum → Adam; He initialization; ~98% test accuracy; checkpoints saved in your `.mat` format.

## Why

This is backprop, the thing PyTorch hides. Writing the matrix-form chain rule once by hand — and *proving* it with a gradient check — is what makes P13 (autograd) and C++ P03-P05 (tensor autograd, framework, transformer) understandable rather than magical. The `Layer` abstraction with function pointers is the C answer to `nn.Module`; C++ P04 replaces it with virtual functions. Adam is the optimizer you will use in every later training run including the transformer. The gradient-check harness is reused verbatim in P13 and P16.

## The math

Batch $X \in \mathbb{R}^{n\times d_0}$, layers $l = 1..L$ with $W_l \in \mathbb{R}^{d_{l-1}\times d_l}$, $\mathbf b_l \in \mathbb{R}^{1\times d_l}$.

**Forward**

$$Z_l = A_{l-1}W_l + \mathbf 1\mathbf b_l, \qquad A_l = \phi(Z_l) \ (l < L), \qquad A_0 = X$$

with $\phi = \text{ReLU}$, $\phi(z) = \max(0, z)$, $\phi'(z) = \mathbb 1[z > 0]$. The last layer's $Z_L$ are logits; loss $L = \text{CE}(\text{softmax}(Z_L), y)$ from P11.

**Backward** — define $\delta_l = \partial L/\partial Z_l \in \mathbb{R}^{n\times d_l}$.

$$\delta_L = \frac1n(P - Y) \quad\text{(P11)}$$

For the linear map $Z_l = A_{l-1}W_l + \mathbf 1 \mathbf b_l$:

$$\nabla_{W_l} L = A_{l-1}^T\,\delta_l, \qquad \nabla_{\mathbf b_l} L = \mathbf 1^T\delta_l \ (\text{column sums}), \qquad \frac{\partial L}{\partial A_{l-1}} = \delta_l W_l^T$$

Through the activation $A_{l-1} = \phi(Z_{l-1})$:

$$\delta_{l-1} = \frac{\partial L}{\partial A_{l-1}} \odot \phi'(Z_{l-1}) = (\delta_l W_l^T) \odot \mathbb 1[Z_{l-1} > 0]$$

Shapes check every line: $(d_{l-1}\times n)(n\times d_l) = d_{l-1}\times d_l$ ✓; $(n\times d_l)(d_l\times d_{l-1}) = n\times d_{l-1}$ ✓.

**Layer abstraction.** Each layer stores what it needs from forward (its input, or its pre-activation) and implements: `forward(in) -> out`, `backward(dout) -> din` (also filling parameter gradients). Loss is the last "layer". The network is a list; forward runs left to right, backward right to left.

**He initialization** for ReLU: $W_{ij} \sim \mathcal N(0, 2/d_{\text{in}})$, $\mathbf b = 0$. Keeps activation variance constant across layers; with $\mathcal N(0, 1)$ init a 784-wide layer produces logits of size ~30 and the loss starts at ~15 instead of 2.3.

**Optimizers**, for each parameter $\theta$ with gradient $g$:

- SGD: $\theta \leftarrow \theta - \eta g$
- Momentum: $v \leftarrow \beta v + g$; $\theta \leftarrow \theta - \eta v$ ($\beta = 0.9$)
- Adam: $m \leftarrow \beta_1 m + (1-\beta_1)g$; $v \leftarrow \beta_2 v + (1-\beta_2) g^2$; $\hat m = m/(1-\beta_1^t)$, $\hat v = v/(1-\beta_2^t)$; $\theta \leftarrow \theta - \eta\,\hat m/(\sqrt{\hat v} + \epsilon)$ with $\beta_1 = 0.9, \beta_2 = 0.999, \epsilon = 10^{-8}$, $\eta = 10^{-3}$.

**Gradient check.** For a parameter entry $\theta_j$: $g_j^{\text{num}} = \dfrac{L(\theta_j + \epsilon) - L(\theta_j - \epsilon)}{2\epsilon}$, $\epsilon = 10^{-5}$, in `double`. Relative error $\dfrac{|g^{\text{num}} - g^{\text{an}}|}{|g^{\text{num}}| + |g^{\text{an}}| + 10^{-12}} < 10^{-6}$ (ReLU kinks can give rare larger errors when $|z| < \epsilon$; report the fraction passing, expect > 99%).

## Spec

**CLI**

```
./mlp2 <mnist paths...> <lr> <epochs> <batch> <seed>                # part 1: hard-coded 784-128-10
./mlp  <mnist paths...> --arch 784,256,128,10 --opt adam --lr 1e-3 --epochs 10 --batch 64 --seed 0 --out ckpt/
./mlp  --gradcheck --arch 20,15,10 --n 7                            # small random problem, prints pass fraction
./mlp  --load ckpt/ <test_images> <test_labels>                     # evaluate a checkpoint
```

**Signatures** (`nn.h`, `nn.c`, `layers.c`, `optim.c`):

```c
typedef struct Layer Layer;
struct Layer {
    const char *name;
    Matrix *(*forward)(Layer *self, const Matrix *in);        /* returns newly allocated out (or reused buffer) */
    Matrix *(*backward)(Layer *self, const Matrix *dout);     /* returns din; fills grads */
    void    (*free_fn)(Layer *self);
    Matrix **params;  Matrix **grads;  int n_params;          /* NULL/0 for parameter-free layers */
    void    *state;                                           /* layer-specific: cached input, mask, ... */
};

Layer *layer_linear(size_t in_dim, size_t out_dim, unsigned seed);  /* He init */
Layer *layer_relu(void);
Layer *layer_tanh(void);
Layer *layer_sigmoid(void);

typedef struct { Layer **layers; int n; } Network;
Network *net_create(const int *dims, int n_dims, const char *activation, unsigned seed);
Matrix  *net_forward(Network *net, const Matrix *X);                   /* logits */
void     net_backward(Network *net, const Matrix *dlogits);
double   net_loss_and_grad(Network *net, const Matrix *X, const int *y, Matrix **out_logits);
int      net_save(const Network *net, const char *dir);
Network *net_load(const char *dir);
void     net_free(Network *net);

typedef struct Optimizer Optimizer;
Optimizer *opt_sgd(double lr);
Optimizer *opt_momentum(double lr, double beta);
Optimizer *opt_adam(double lr, double b1, double b2, double eps);
void       opt_step(Optimizer *o, Matrix **params, Matrix **grads, int n);  /* keeps m,v per param internally */
void       opt_free(Optimizer *o);

double gradcheck(Network *net, const Matrix *X, const int *y, double eps, double tol);  /* returns pass fraction */
```

**Checkpoint dir**: `arch.txt` (dims and activation), `W0.mat, b0.mat, W1.mat, ...`.

**Expected results** (batch 64, 10 epochs):

| arch | opt | lr | test acc |
|---|---|---|---|
| 784-128-10 | sgd | 0.1 | ~97.5% |
| 784-256-128-10 | momentum 0.9 | 0.05 | ~98.0% |
| 784-256-128-10 | adam | 1e-3 | ~98.2% |

Per-epoch time should be a few seconds with a decent matmul.

## Milestones

1. **M1 — 2-layer forward.** He init; loss at init on a batch is ~2.3 (not 15). You'll know shapes are right when `Z2` is `n x 10`.
2. **M2 — 2-layer backward + gradient check.** Analytic `dW1, db1, dW2, db2` vs finite differences on `784-16-10` with `n = 5`: > 99% of checked entries pass. Do not proceed until this passes — nothing after it can be debugged otherwise.
3. **M3 — train the 2-layer net with SGD.** 97.5% test accuracy; loss curve to CSV. Compare with the PyTorch snippet below at the same init (load your W into torch) for the *first step's loss and gradients* to 1e-6.
4. **M4 — `Layer` refactor.** Same 2-layer network expressed as `[linear, relu, linear]` gives bit-identical loss sequence for the same seed (a strong regression test). Then a 4-layer net trains.
5. **M5 — momentum and Adam.** Adam reaches 97% test in 1 epoch; 98%+ by epoch 10. Plot the three optimizers' test-accuracy curves.
6. **M6 — save/load + evaluation.** Load a checkpoint in a fresh process and reproduce the test accuracy exactly. Also load it in Python and verify predictions match on 100 images.

## Verification

```python
import torch, numpy as np
from load_mat import load_mat
W1, b1, W2, b2 = (torch.tensor(load_mat(f"ckpt/{n}.mat"), requires_grad=True) for n in ("W0","b0","W1","b1"))
X = torch.tensor(X_batch); y = torch.tensor(y_batch)             # same batch you used in C (save it as .mat too)
Z1 = X @ W1 + b1; A1 = torch.relu(Z1); Z2 = A1 @ W2 + b2
loss = torch.nn.functional.cross_entropy(Z2, y)                    # == your loss on that batch
loss.backward()
print(loss.item())
print(np.abs(W1.grad.numpy() - load_mat("ckpt/dW0.mat")).max())    # < 1e-9
print(np.abs(b2.grad.numpy() - load_mat("ckpt/db1.mat")).max())
```

Sanity numbers: initial loss $2.30 \pm 0.1$; after one epoch of SGD lr 0.1 the test accuracy is already > 90%; a bug in `db` (forgetting the column sum) shows up as the gradient check failing only on biases.

## Stretch goals

- Dropout layer (inverted dropout, mask cached for backward) and see the train/test gap shrink.
- Learning-rate warmup + cosine decay; label smoothing.
- `float` instead of `double` throughout (templated later in C++): 2x speed, same accuracy — verify.
- A `Softmax+CE` layer returning both loss and `dlogits`, so `net_backward` starts from the loss layer uniformly; then a `MSE` loss and train an autoencoder 784-64-784 and view reconstructions as PGM.

## Hints

- Start with the ugly version: four explicit matrices, no abstraction. Get the gradient check passing. Only then refactor into `Layer`.
- A `Layer`'s `state` for `linear` is the cached input `A_{l-1}` (needed for `dW = A^T dout`); for `relu` it is the cached mask or input. Cache pointers, not copies, where lifetime allows — but be sure the input still exists at backward time.
- Allocation strategy: let each `forward` return a fresh `Matrix*` and free the chain after `backward`; get it correct, then optimize with reusable buffers. ASan will find every leak.
- The gradient check must use the *same* batch and no randomness (dropout off). Perturb one entry, run the full forward, restore. It's slow — use tiny dims.
- Adam needs a step counter `t` starting at 1 and per-parameter `m`, `v` matrices allocated lazily on the first `opt_step`.
- `net_create` from `--arch` string: `strtok` on commas, `atoi` each piece.

## Where to put it

`neural_network_c/nn/` — `nn.h`, `nn.c`, `layers.c`, `optim.c`, `optim.h`, `mlp2.c`, `main.c`, `Makefile`, `plot.py`. Move `nn_ops.*` from P11 here.
