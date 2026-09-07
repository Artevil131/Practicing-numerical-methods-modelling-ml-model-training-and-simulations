# P04 — Neural Network Framework

**Difficulty:** ★★★☆☆   **Prereq chapters:** C++ 09 (plus 01-08, 11)   **Builds on:** P03, C P12

## Goal

A small `nn`-style library on top of your autograd: a `Module` base class with `parameters()`, concrete layers (`Linear`, `ReLU`, `Tanh`, `Softmax`, `LayerNorm`, `Embedding`, `Dropout`), a `Sequential` container, `Optimizer` implementations (`SGD` with momentum, `Adam`), a `DataLoader` that shuffles and batches, and a training script that takes an MNIST MLP to 98%. The API should read like PyTorch's; the code should be a few hundred lines because autograd does the gradients.

## Why

P05 (the transformer) is assembled entirely from these pieces: `Embedding` for tokens and positions, `Linear` for Q/K/V and the MLP block, `LayerNorm`, `Dropout`, `Adam`, and a `DataLoader` over token windows. Inheritance and virtual dispatch (C++ 09) are used the way they were designed to be — a `Module` hierarchy where `forward` is virtual and `parameters()` is collected recursively. Comparing this to C P12's function-pointer `Layer` is the concrete answer to "why classes?". Reaching 98% on MNIST here is the regression test that the whole stack (tensor → autograd → layers → optimizer) is correct.

## The math

Layers and their forward expressions (autograd supplies the backward):

- **Linear**: $y = xW + b$, $W \in \mathbb R^{d_{in}\times d_{out}}$ initialized $\mathcal N(0, 2/d_{in})$ (He) for ReLU nets, $\mathcal U(-1/\sqrt{d_{in}}, 1/\sqrt{d_{in}})$ (PyTorch default) otherwise; $b = 0$.
- **LayerNorm** over the last axis of size $d$: $\mu = \tfrac1d\sum_i x_i$, $\sigma^2 = \tfrac1d\sum_i (x_i - \mu)^2$, $y = \gamma \odot \dfrac{x - \mu}{\sqrt{\sigma^2 + \epsilon}} + \beta$ with $\epsilon = 10^{-5}$, learnable $\gamma = \mathbf 1$, $\beta = \mathbf 0$. Written with `mean(axis, keepdim)` and broadcasting, its gradient comes for free — but check it against PyTorch, because the $\sigma$ path through `sqrt` and division is where autograd bugs hide.
- **Embedding**: $y_i = E_{\text{idx}_i}$, a gather along axis 0; backward is a scatter-add into $\bar E$ (you need a `gather_rows`/`embedding` op in autograd if P03 only has last-axis `gather`).
- **Dropout** (inverted): in training, mask $m_i \sim \text{Bernoulli}(1-p)$, $y = x \odot m/(1-p)$; in eval, identity. The `1/(1-p)` keeps the expectation unchanged so eval needs no rescaling.
- **Softmax** over the last axis (as a layer, rarely used before a loss — use `cross_entropy` on logits instead; include it for P05's attention weights).

Optimizers (per parameter $\theta$, gradient $g$, step $t$):

- SGD+momentum: $v \leftarrow \mu v + g$, $\theta \leftarrow \theta - \eta v$; optional weight decay adds $\lambda\theta$ to $g$.
- Adam: as in C P12 with bias correction; AdamW variant applies decay directly to $\theta$: $\theta \leftarrow \theta - \eta\lambda\theta$ before the Adam step (this is what GPT training uses).
- Gradient clipping by global norm: $g \leftarrow g \cdot \min(1, c/\lVert g\rVert_2)$ where the norm is over all parameters — needed for P05.

Learning-rate schedules: linear warmup for $w$ steps then cosine decay to $\eta_{\min}$: $\eta_t = \eta_{\min} + \tfrac12(\eta_{\max} - \eta_{\min})(1 + \cos(\pi \tfrac{t - w}{T - w}))$.

## Spec

**Interface** (`nn.hpp`):

```cpp
class Module {
public:
    virtual ~Module() = default;
    virtual Variable forward(const Variable& x) = 0;
    Variable operator()(const Variable& x);                       // calls forward
    virtual std::vector<Variable*> parameters();                  // default: own params + submodules', recursively
    virtual void train(bool mode = true);  void eval();  bool training() const;
    std::size_t num_parameters();
    void save(const std::string& dir) const;                      // one .npy per parameter, in parameters() order
    void load(const std::string& dir);
protected:
    Variable& register_parameter(const std::string& name, Tensor<T> init);
    Module&   register_module(const std::string& name, std::unique_ptr<Module> m);
    std::vector<std::pair<std::string, Variable>> params_;
    std::vector<std::pair<std::string, std::unique_ptr<Module>>> children_;
    bool training_ = true;
};

class Linear    : public Module { public: Linear(std::size_t in, std::size_t out, bool bias = true, unsigned seed = 0); Variable forward(const Variable&) override; };
class ReLU      : public Module { /* ... */ };
class Tanh      : public Module { /* ... */ };
class Softmax   : public Module { public: explicit Softmax(int axis = -1); /* ... */ };
class LayerNorm : public Module { public: LayerNorm(std::size_t dim, double eps = 1e-5); /* ... */ };
class Embedding : public Module { public: Embedding(std::size_t num, std::size_t dim, unsigned seed = 0);
                                  Variable forward(const Tensor<std::size_t>& idx);  Variable forward(const Variable&) override; /* throws: use the index overload */ };
class Dropout   : public Module { public: explicit Dropout(double p); /* uses training() */ };
class Sequential: public Module { public: Sequential(std::vector<std::unique_ptr<Module>> layers); Variable forward(const Variable&) override; Module& operator[](std::size_t); };

class Optimizer {
public:
    explicit Optimizer(std::vector<Variable*> params);
    virtual ~Optimizer() = default;
    virtual void step() = 0;
    void zero_grad();
    void set_lr(double lr);  double lr() const;
protected: std::vector<Variable*> params_;  double lr_;
};
class SGD  : public Optimizer { public: SGD(std::vector<Variable*> p, double lr, double momentum = 0.0, double weight_decay = 0.0); void step() override; };
class Adam : public Optimizer { public: Adam(std::vector<Variable*> p, double lr, double b1 = 0.9, double b2 = 0.999, double eps = 1e-8, double weight_decay = 0.0 /* AdamW-style */); void step() override; };
double clip_grad_norm(const std::vector<Variable*>& params, double max_norm);   // returns pre-clip norm
double lr_warmup_cosine(std::size_t step, std::size_t warmup, std::size_t total, double lr_max, double lr_min);

struct Batch { Tensor<T> x; Tensor<std::size_t> y; };
class DataLoader {
public:
    DataLoader(Tensor<T> X, Tensor<std::size_t> y, std::size_t batch_size, bool shuffle, unsigned seed);
    void reset_epoch();                       // reshuffles
    bool next(Batch& out);                    // false at end of epoch; last batch may be short
    std::size_t num_batches() const;
    // range-for support: begin()/end() iterators yielding Batch
};

Tensor<T> load_mnist_images(const std::string& idx_path);          // n x 784 in [0,1] (port of C P04)
Tensor<std::size_t> load_mnist_labels(const std::string& idx_path);
```

**CLI** (`train_mnist`):

```
./train_mnist --data data/mnist --arch 784,256,128,10 --opt adam --lr 1e-3 --epochs 10 --batch 64 --dropout 0.1 --layernorm --seed 0 --out ckpt/ --log log.csv
./train_mnist --eval ckpt/ --data data/mnist
```

Expected: 784-256-128-10 with ReLU, Adam 1e-3, 10 epochs → ≥ 98.0% test; with LayerNorm + dropout 0.1 → ~98.3%. Epoch time a few seconds (if it's 30+ s, autograd is allocating too much — see Hints).

## Milestones

1. **M1 — `Module`, `Linear`, `ReLU`, `Sequential`, `parameters()`.** A 784-128-10 net reports 101,770 parameters; `forward` on a batch gives `[64, 10]`; `parameters()` returns 4 pointers in order.
2. **M2 — `SGD` + `DataLoader` → training works.** 1 epoch, lr 0.1: > 90% test accuracy, matching C P12's first-epoch number within noise.
3. **M3 — `Adam`, clipping, schedule.** Adam 1e-3: 97% after 1 epoch; 98% by epoch 10. `clip_grad_norm` returns the norm and shrinks it when needed.
4. **M4 — `LayerNorm`, `Dropout`, `Embedding` verified against PyTorch.** Each layer's output *and* input-gradient on a random `[4, 7, 16]` tensor match `torch.float64` to 1e-10 (dropout: compare with the mask exported). Dropout is identity in `eval()`.
5. **M5 — save/load.** `save` writes `.npy`s; a fresh process `load`s and reproduces the test accuracy exactly; PyTorch loads the same files into an equivalent `nn.Sequential` and agrees on predictions for 1000 images.
6. **M6 — profile.** `perf`-style timing (`std::chrono` around forward/backward/step): find the top cost; ensure no per-step `Tensor` copies where a view would do; get ≤ 5 s/epoch for the 256-128 net.

## Verification

```python
import torch, numpy as np
torch.set_default_dtype(torch.float64)
x = torch.tensor(np.load("x.npy"), requires_grad=True)          # [4,7,16]
ln = torch.nn.LayerNorm(16); ln.weight.data = torch.tensor(np.load("gamma.npy")); ln.bias.data = torch.tensor(np.load("beta.npy"))
y = ln(x); y.backward(torch.tensor(np.load("dy.npy")))
print(np.abs(y.detach().numpy() - np.load("y.npy")).max(), np.abs(x.grad.numpy() - np.load("dx.npy")).max())   # < 1e-10
# full model check
import glob
ws = [torch.tensor(np.load(f)) for f in sorted(glob.glob("ckpt/*.npy"))]
m = torch.nn.Sequential(torch.nn.Linear(784,256), torch.nn.ReLU(), torch.nn.Linear(256,128), torch.nn.ReLU(), torch.nn.Linear(128,10))
with torch.no_grad():
    for p, w in zip(m.parameters(), ws): p.copy_(w.T if w.dim()==2 else w)   # note: torch Linear stores W^T
print((m(torch.tensor(Xtest[:1000])).argmax(1).numpy() == ytest[:1000]).mean())   # == your accuracy on those 1000
```

## Stretch goals

- `Conv2d` (3x3, via im2col + matmul) and a small CNN to 99%+ on MNIST.
- Mixed precision: `Tensor<float>` parameters with `double` Adam moments; compare accuracy and speed.
- A `Trainer` class with callbacks (logging, early stopping on validation loss, checkpoint-best).
- Data augmentation in the loader (random 2-pixel shifts) — a measurable accuracy gain.

## Hints

- `parameters()` returns raw pointers (`Variable*`) into the module tree so the optimizer can update in place; the modules own the `Variable`s. Don't return copies — `Variable` has shared semantics, so copies *would* alias, but pointers make the ownership obvious.
- `Sequential` owns its children by `unique_ptr`; construct it with a helper like `make_seq(std::make_unique<Linear>(...), std::make_unique<ReLU>(), ...)` using a variadic template, or push into a vector.
- `LayerNorm` should be written entirely in autograd ops (`mean`, `-`, `*`, `sqrt`, `/`, broadcasting). If it's slow, that's fine for now; the transformer will tell you where to hand-fuse.
- Dropout's mask is a `Tensor<T>` of 0/(1-p) values; `x * mask` records a `MulNode` whose backward multiplies by the mask automatically.
- The optimizer's `step()` must run under `NoGradGuard` and mutate `param->data()` in place — never build graph nodes for the update.
- Speed: the biggest costs are usually (1) `unbroadcast` summing on every bias add, (2) allocating a new tensor for every op. Measure before optimizing; a 5 s epoch is acceptable.

## Where to put it

`cpp/nn/` — `include/nn.hpp`, `src/layers.cpp`, `src/optim.cpp`, `src/data.cpp`, `tests/test_nn.cpp`, `tests/gen_fixtures.py`, `apps/train_mnist.cpp`, `CMakeLists.txt`.
