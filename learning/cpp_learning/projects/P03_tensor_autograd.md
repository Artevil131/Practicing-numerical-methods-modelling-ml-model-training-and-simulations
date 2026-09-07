# P03 — Tensor Autograd

**Difficulty:** ★★★★☆   **Prereq chapters:** C++ 07, 09, 13 (plus 01-06)   **Builds on:** P02, C P13

## Goal

Reverse-mode autodiff over `Tensor<T>`: a `Variable` that holds a tensor, an optional gradient, and a `shared_ptr` to the `Node` that produced it; ops (`add`, `mul`, `matmul`, `sum`, `exp`, `log`, `tanh`, `relu`, `transpose`, `reshape`, `softmax`/`log_softmax`, `gather`) recorded as a graph; `backward()` via topological sort with broadcasting-aware gradients; a `NoGradGuard` RAII object; every gradient verified against PyTorch to 1e-10.

## Why

C P13 did this for scalars; this is the real thing — the engine behind `loss.backward()`. The one genuinely new idea is the gradient of broadcasting: if the forward op expanded `b` from `[4]` to `[2,3,4]`, the backward must *sum* the incoming gradient over the expanded dims. Getting that right is what lets P04's `Linear` layer have a bias, `LayerNorm` have per-feature scale, and P05's attention add masks and positional embeddings without a single hand-derived layer gradient. `shared_ptr` graph ownership, `std::variant`/virtual dispatch for ops, and RAII guards are C++ 07, 09, 13 applied to their canonical use case.

## The math

Forward op $y = f(x_1, \dots, x_k)$ with tensors. Given upstream gradient $\bar y = \partial L/\partial y$ (same shape as $y$), each op supplies $\bar x_i = \bar y \cdot \partial y/\partial x_i$ — in practice a closed-form tensor expression, not a Jacobian:

| op | forward | $\bar x$ (accumulate with `+=`) |
|---|---|---|
| add | $x_1 + x_2$ | $\bar x_1 = \text{unbroadcast}(\bar y, \text{shape}(x_1))$, same for $x_2$ |
| mul | $x_1 \odot x_2$ | $\bar x_1 = \text{unbroadcast}(\bar y \odot x_2)$, $\bar x_2 = \text{unbroadcast}(\bar y \odot x_1)$ |
| matmul | $X W$ | $\bar X = \bar Y W^T$, $\bar W = X^T \bar Y$ (batched: transpose last two dims, sum over broadcast batch dims) |
| sum (axis $k$) | | $\bar x = \text{broadcast}(\bar y \text{ unsqueezed at } k)$ |
| sum (all) | scalar | $\bar x = \bar y \cdot \mathbf 1$ |
| exp | $e^x$ | $\bar y \odot y$ |
| log | $\ln x$ | $\bar y / x$ |
| tanh | | $\bar y \odot (1 - y^2)$ |
| relu | | $\bar y \odot \mathbb 1[x > 0]$ |
| transpose | swap dims | transpose $\bar y$ back |
| reshape | | reshape $\bar y$ to input shape |
| pow (const $n$) | $x^n$ | $\bar y \odot n x^{n-1}$ |
| log_softmax (last axis) | $x - \text{lse}(x)$ | $\bar x = \bar y - \text{softmax}(x) \odot \sum_{\text{last}} \bar y$ |
| gather (last axis, idx) | $y_i = x_{i, \text{idx}_i}$ | scatter $\bar y$ into zeros at the same indices |

**Unbroadcast** — the key derivation. If $x$ has shape $s_x$ and was broadcast to $s_y$, then $y_{j} = x_{\pi(j)}$ for many $j$ mapping to the same $\pi(j)$; by the chain rule $\bar x_i = \sum_{j : \pi(j) = i} \bar y_j$. Concretely: sum $\bar y$ over the leading dims that were added, then sum (with `keepdim`) over every dim where $s_x$ has 1 and $s_y$ has more. Result has shape $s_x$.

**Cross-entropy from logits** $Z$ and labels $y$: `nll = -gather(log_softmax(Z), y).mean()`. Its gradient w.r.t. $Z$ falls out of the table as $(P - Y)/n$ — you don't derive it, the engine does; you *check* it against C P11.

**Topological order**: DFS post-order from the loss node over `Node::inputs`; then process in reverse, calling each node's `backward(grad_output)` which returns gradients for its inputs, accumulated into the inputs' `.grad`. Leaf variables with `requires_grad=true` keep their `.grad`; non-leaf grads are discarded after use (unless `retain_grad`).

**Gradient check**: for a scalar loss $L(\theta)$, $\dfrac{L(\theta + \epsilon e_j) - L(\theta - \epsilon e_j)}{2\epsilon}$ in `double`; relative error $< 10^{-6}$ for smooth ops.

## Spec

**Interface** (`autograd.hpp`):

```cpp
using T = double;                 // start with double only; template later
struct Node;

class Variable {
public:
    Variable();
    explicit Variable(Tensor<T> data, bool requires_grad = false);
    const Tensor<T>& data() const;  Tensor<T>& data();
    const Tensor<T>& grad() const;  bool has_grad() const;
    bool requires_grad() const;  bool is_leaf() const;
    void zero_grad();
    void backward();                                     // this must be a scalar (numel()==1)
    Variable detach() const;                             // same data, no graph
    std::shared_ptr<Node> grad_fn() const;
private:
    struct Impl;  std::shared_ptr<Impl> impl_;           // shared so copies alias the same variable
};

struct Node {
    std::vector<Variable> inputs;                        // parents
    std::vector<std::weak_ptr<Node>> children;           // optional, for debugging/dot output
    virtual ~Node() = default;
    virtual std::vector<Tensor<T>> backward(const Tensor<T>& grad_output) = 0;   // one grad per input
    virtual const char* name() const = 0;
};
// One subclass per op, e.g.:
struct AddNode : Node { std::vector<std::size_t> shape_a, shape_b; std::vector<Tensor<T>> backward(const Tensor<T>&) override; ... };
struct MatMulNode : Node { Tensor<T> a, b; ... };
// Alternative design: std::variant<AddOp, MulOp, ...> + std::visit. Pick one; write in a comment why.

// ops (record a Node iff grad mode is on and any input requires grad)
Variable operator+(const Variable&, const Variable&);   Variable operator-(const Variable&, const Variable&);
Variable operator*(const Variable&, const Variable&);   Variable operator/(const Variable&, const Variable&);
Variable operator+(const Variable&, T);                 Variable operator*(const Variable&, T);
Variable operator-(const Variable&);
Variable matmul(const Variable&, const Variable&);
Variable sum(const Variable&);  Variable sum(const Variable&, int axis, bool keepdim = false);
Variable mean(const Variable&); Variable mean(const Variable&, int axis, bool keepdim = false);
Variable exp(const Variable&);  Variable log(const Variable&);  Variable tanh(const Variable&);
Variable relu(const Variable&); Variable pow(const Variable&, T n);  Variable sqrt(const Variable&);
Variable transpose(const Variable&, int a, int b);  Variable reshape(const Variable&, std::vector<std::size_t>);
Variable softmax(const Variable&, int axis);        Variable log_softmax(const Variable&, int axis);
Variable gather(const Variable&, const Tensor<std::size_t>& index);   // along last axis
Variable cross_entropy(const Variable& logits, const Tensor<std::size_t>& labels);   // mean NLL
Variable where(const Tensor<bool>& mask, const Variable& a, T other);  // for causal masks in P05

Tensor<T> unbroadcast(const Tensor<T>& grad, const std::vector<std::size_t>& target_shape);

struct NoGradGuard { NoGradGuard(); ~NoGradGuard(); NoGradGuard(const NoGradGuard&) = delete; /* ... */ };
bool grad_enabled();

// testing
double gradcheck(const std::function<Variable(const std::vector<Variable>&)>& f,
                 std::vector<Variable> inputs, double eps = 1e-6, double tol = 1e-6);   // returns max rel error
void   dump_dot(const Variable& root, std::ostream& os);
```

**Tests** (doctest + a PyTorch-generated `.npy` fixture set): every op individually via `gradcheck`; the unbroadcast cases `[2,3,4] <- [4]`, `[3,1]`, `[1,3,4]`, `[]`; matmul batched; a 2-layer MLP loss on 8 MNIST rows whose parameter gradients match PyTorch's (float64) to 1e-10; `NoGradGuard` produces variables with no `grad_fn`; a node used twice accumulates; `backward()` on a non-scalar throws.

**CLI demo**: `./autograd_demo` trains a 2-layer MLP on a moons CSV for 100 steps and prints loss/accuracy — the same demo as C P13, now in ~40 lines.

## Milestones

1. **M1 — `Variable`, leaf, `add`/`mul` without broadcasting, `backward`.** $L = \text{sum}(a \odot b + a)$: $\bar a = b + 1$, $\bar b = a$. You'll know it works when a reused variable accumulates correctly.
2. **M2 — `unbroadcast` + broadcasting ops.** `[2,3,4] + [4]`: `gradcheck` passes and the bias gradient has shape `[4]` equal to the sum over the first two dims of the upstream grad.
3. **M3 — matmul, transpose, reshape, reductions.** `gradcheck` on `sum(matmul(X, W) * M)` for `X:[5,3], W:[3,4]`; batched `[2,5,3] @ [3,4]`. All < 1e-8.
4. **M4 — activations, `log_softmax`, `gather`, `cross_entropy`.** Gradient of `cross_entropy(Z, y)` w.r.t. `Z` equals $(P - Y)/n$ from C P11 to 1e-12 (load the same `Z` from a `.npy`).
5. **M5 — full MLP vs PyTorch.** Parameters and a batch from `.npy`; loss and all four parameter gradients match `torch.float64` to 1e-10. `NoGradGuard` around the eval pass; assert no nodes were created.
6. **M6 — memory and lifetime.** Train 1000 steps of the demo under ASan; memory stays flat (graph freed when the loss `Variable` goes out of scope). `dump_dot` of a small graph renders with Graphviz.

## Verification

```python
import torch, numpy as np
torch.set_default_dtype(torch.float64)
W1 = torch.tensor(np.load("W1.npy"), requires_grad=True); b1 = torch.tensor(np.load("b1.npy"), requires_grad=True)
W2 = torch.tensor(np.load("W2.npy"), requires_grad=True); b2 = torch.tensor(np.load("b2.npy"), requires_grad=True)
X = torch.tensor(np.load("X.npy")); y = torch.tensor(np.load("y.npy"))
loss = torch.nn.functional.cross_entropy(torch.relu(X @ W1 + b1) @ W2 + b2, y)
loss.backward()
for name, p in [("W1", W1), ("b1", b1), ("W2", W2), ("b2", b2)]:
    print(name, np.abs(p.grad.numpy() - np.load(f"grad_{name}.npy")).max())   # < 1e-10 each
print(loss.item())   # == your loss
```

## Stretch goals

- `Tensor<float>` instantiation with `T` as a template parameter through the whole graph; compare speed (2x) and gradient-check tolerance (1e-3).
- `retain_graph` / second `backward` semantics: make the default free the graph (`inputs.clear()` after use) and add an opt-in to keep it.
- Higher-order gradients: build graph nodes during `backward` when `create_graph` is set; compute a Hessian-vector product and compare with `torch.autograd.functional.hvp`.
- Checkpointing (recompute activations in backward) for one layer type; measure memory vs time.

## Hints

- Ownership: `Variable` → `shared_ptr<Impl>`; `Impl` holds `data`, `grad`, `requires_grad`, `shared_ptr<Node> grad_fn`. `Node` holds `Variable inputs` (which hold their own `Impl`s). The loss variable owns the whole graph; when it dies, the graph dies. No cycles as long as `Node` never holds a `shared_ptr` to its *output*.
- Each op does three things: compute the output tensor, and (if grad mode on) create a node capturing what backward needs (saved tensors or shapes), and attach it as the output's `grad_fn`. Write a helper `make_node<NodeType>(inputs..., saved...)`.
- `unbroadcast`: first sum over the extra leading dims (while `grad.rank() > target.rank()`, `grad = grad.sum(0)`); then for each remaining dim where target is 1 and grad isn't, `grad = grad.sum(k, keepdim=true)`.
- Topo sort: recursive DFS on `grad_fn` pointers with a `std::unordered_set<Node*>` visited set; reverse the post-order; iterate calling `backward` and accumulate into `input.impl_->grad` (allocate zeros on first touch).
- `NoGradGuard`: a thread-local (or global) `bool`, saved in the constructor, restored in the destructor. Deleted copy operations so it can't escape its scope.
- Decide early whether a node stores copies of input *tensors* (cheap: they're shared-storage views) or the input `Variable`s (needed to find where to accumulate). You need the `Variable`s for accumulation and possibly tensors for math — store both.
- For `gradcheck`, perturb through `Variable::data()` in place and re-run `f`; use `eps = 1e-6` in double.

## Where to put it

`cpp/autograd/` — `include/autograd.hpp`, `src/autograd.cpp` (node implementations), `tests/test_autograd.cpp`, `tests/gen_fixtures.py`, `demo/moons_demo.cpp`, `CMakeLists.txt`.
