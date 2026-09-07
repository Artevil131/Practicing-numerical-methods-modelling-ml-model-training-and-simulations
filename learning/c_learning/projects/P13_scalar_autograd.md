# P13 — Scalar Autograd (micrograd in C)

**Difficulty:** ★★★★☆   **Prereq chapters:** C 10, 11, 14 (plus 01-09)   **Builds on:** P12

## Goal

A reverse-mode automatic differentiation engine over scalars: every arithmetic operation creates a `Value` node recording its inputs and a backward function; `backward()` on the final loss topologically sorts the graph and propagates gradients. Nodes live in an arena allocator so a whole graph is freed in one call. On top of it, a tiny MLP (like Karpathy's micrograd) trained on a toy 2-D dataset, with gradients checked against PyTorch to 1e-10.

## Why

P12 was backprop for one architecture, derived by hand. This is backprop for *any* computation, derived by the machine — the idea behind `loss.backward()`. Building it scalar-first keeps the graph and the chain rule visible; C++ P03 lifts the same design to tensors with broadcasting. The arena allocator (C 14 idioms) is the memory strategy for every per-step graph in the C++ framework, and the topological sort is the exact algorithm PyTorch's engine runs. After this, `requires_grad`, `retain_graph`, `no_grad`, and leaf-vs-non-leaf tensors all have concrete meanings.

## The math

A computation graph is a DAG whose nodes are scalars $v_i$ computed from parents by primitive ops. For output $L$, reverse-mode AD computes $\bar v_i = \partial L / \partial v_i$ for every node with one pass in reverse topological order, using the chain rule

$$\bar v_j \mathrel{+}= \bar v_i \,\frac{\partial v_i}{\partial v_j} \quad \text{for each parent } j \text{ of } i$$

The `+=` matters: a node used twice accumulates gradient from both uses (this is why gradients must be zeroed before each backward).

**Local derivatives** for the primitive set:

| op | $v = $ | $\partial v/\partial a$ | $\partial v/\partial b$ |
|---|---|---|---|
| add | $a + b$ | 1 | 1 |
| mul | $a b$ | $b$ | $a$ |
| pow (const $n$) | $a^n$ | $n a^{n-1}$ | — |
| neg | $-a$ | $-1$ | — |
| exp | $e^a$ | $e^a = v$ | — |
| log | $\ln a$ | $1/a$ | — |
| tanh | $\tanh a$ | $1 - v^2$ | — |
| relu | $\max(0,a)$ | $\mathbb 1[a > 0]$ | — |

Subtraction and division are `add(a, neg(b))` and `mul(a, pow(b, -1))`.

**Topological order**: DFS from the output, appending a node after visiting all its children (post-order); reverse that list. Then set $\bar L = 1$ and call each node's backward in that order.

**Verification target**: the sum-of-squares loss of a 2-16-16-1 MLP on the "moons"-style points, exactly as micrograd's demo; every parameter's `.grad` must equal PyTorch's to ~1e-10 (double precision on both sides).

## Spec

**Signatures** (`value.h`, `value.c`, `arena.h`, `arena.c`, `nn_micro.h`, `nn_micro.c`):

```c
typedef struct Arena Arena;
Arena *arena_create(size_t bytes_per_block);
void  *arena_alloc(Arena *a, size_t bytes);      /* bump pointer; new block when full; 16-byte aligned */
void   arena_reset(Arena *a);                    /* free everything logically, keep blocks */
void   arena_free(Arena *a);

typedef struct Value Value;
typedef void (*BackwardFn)(Value *self);
struct Value {
    double data, grad;
    Value *prev[2];      /* parents (NULL if unused) */
    int    n_prev;
    BackwardFn backward; /* NULL for leaves */
    double aux;          /* e.g. exponent for pow */
    const char *op;      /* "+", "*", "tanh", ... for graph dumps */
    int    visited;      /* scratch for topo sort */
};

Value *val_new(Arena *a, double data);           /* leaf */
Value *val_add(Arena *a, Value *x, Value *y);
Value *val_mul(Arena *a, Value *x, Value *y);
Value *val_pow(Arena *a, Value *x, double n);
Value *val_neg(Arena *a, Value *x);
Value *val_sub(Arena *a, Value *x, Value *y);
Value *val_div(Arena *a, Value *x, Value *y);
Value *val_exp(Arena *a, Value *x);
Value *val_log(Arena *a, Value *x);
Value *val_tanh(Arena *a, Value *x);
Value *val_relu(Arena *a, Value *x);

void   val_backward(Value *root, Arena *scratch);     /* topo sort + reverse sweep; sets root->grad = 1 */
void   val_zero_grad_all(Value **params, size_t n);
void   val_dump_dot(Value *root, FILE *out);          /* Graphviz "digraph" of the graph */

/* micro MLP: parameters are leaf Values living in a *separate*, long-lived arena */
typedef struct { Value **w; Value *b; int nin; int nonlin; } Neuron;
typedef struct { Neuron *neurons; int nout; } Layer;
typedef struct { Layer *layers; int n; Value **params; int n_params; } MLP;
MLP   *mlp_create(Arena *param_arena, const int *sizes, int n_sizes, unsigned seed);
Value **mlp_forward(Arena *graph_arena, MLP *m, Value **x, int *out_n);  /* returns array of outputs */
```

Two arenas: **parameters** (persist across steps) and **graph** (reset every step after the optimizer update). That's `retain_graph=False`.

**CLI**

```
./autograd test                     # runs unit tests, prints "ok"
./autograd expr                     # evaluates a fixed expression, prints all grads (compare to torch)
./autograd train <steps> <lr>       # micrograd demo: 100 moon points, 2-16-16-1, hinge/MSE loss, prints loss & acc
./autograd dot > graph.dot          # tiny graph dump; `dot -Tpng graph.dot > graph.png`
```

## Milestones

1. **M1 — arena.** Allocate a million 40-byte objects; memory usage ~40 MB, no per-object `free`. `arena_reset` then re-allocate: no growth. ASan-clean.
2. **M2 — leaves and ops build a graph.** `c = a*b + a` with `a=2, b=3` has `data = 8`; `c->prev[0]->op` is `"*"`. `val_dump_dot` produces a graph you can render.
3. **M3 — `val_backward` on a hand-checkable expression.** For $L = \tanh(2a + 3b) \cdot a$ with $a = 0.5, b = -0.25$: compute $\partial L/\partial a$, $\partial L/\partial b$ by hand and by code; match to 1e-12. Test a reused node ($L = a \cdot a + a$, $\bar a = 2a + 1$) — this catches missing `+=`.
4. **M4 — `expr` vs PyTorch.** The micrograd README expression (`a = -4, b = 2, c = a + b, d = a*b + b**3, ...`) gives `g.data = 24.7041`, `a.grad = 138.8338`, `b.grad = 645.5773` — match to 4 decimals, then to 1e-10 against a `float64` torch run.
5. **M5 — MLP forward/backward.** 2-16-16-1 with tanh on 4 points: all 337 parameter gradients match PyTorch (Verification) to 1e-10.
6. **M6 — training.** 100 steps of SGD (lr 0.05→0.01 decay) on the moons data: loss drops, accuracy reaches 100%; write the decision boundary grid to CSV and plot it. The graph arena is reset each step; peak memory stays flat.

## Verification

```python
import torch
torch.set_default_dtype(torch.float64)
# 1) micrograd README expression
a = torch.tensor(-4.0, requires_grad=True); b = torch.tensor(2.0, requires_grad=True)
c = a + b; d = a*b + b**3; c = c + c + 1; c = c + 1 + c + (-a); d = d + d*2 + (b + a).relu()
d = d + 3*d + (b - a).relu(); e = c - d; f = e**2; g = f/2.0 + 10.0/f
g.backward(); print(g.item(), a.grad.item(), b.grad.item())   # 24.7041, 138.8338, 645.5773
# 2) MLP grads: dump your params (in creation order) and 4 inputs to a text file; rebuild in torch:
import numpy as np
W = np.loadtxt("params.txt")             # your w's and b's in the same order as mlp_create
# build the same 2-16-16-1 tanh MLP with those weights, compute the same loss, .backward(),
# and compare param.grad with your grads.txt: np.abs(diff).max() < 1e-10
```

Also check `val_backward` twice without zeroing grads doubles them (that's expected, and it's why `zero_grad` exists).

## Stretch goals

- Variable-arity nodes (a `sum` op with $k$ parents) so a dot product is one node instead of a chain; measure the node count for the MLP.
- Higher-order derivatives: make `backward` itself build graph nodes (grads are `Value`s), then compute $d^2L/da^2$ and check against torch's `torch.autograd.grad(..., create_graph=True)`.
- A `no_grad` flag on the arena that makes ops compute `data` but not record parents (for evaluation passes).
- Vectorize: a `VecValue` holding `n` doubles with elementwise ops — the halfway house to C++ P03.

## Hints

- Each op's backward is a `static` function that reads `self->grad`, `self->prev[]` and `self->aux`, and does `prev[i]->grad += ...`. Store enough in the node to compute local derivatives (e.g. `exp` needs `self->data`; `pow` needs `aux`).
- Topological sort: recursive DFS with a `visited` flag; append to a dynamic array (allocated from the scratch arena) after recursing into children. Reset `visited` flags afterwards or use a generation counter to avoid a second pass.
- Parameters must be allocated from the persistent arena and referenced, not copied, by the graph: `val_mul(graph, w, x)` stores the pointer `w` as a parent, so `w->grad` accumulates correctly.
- Graph nodes from the previous step become garbage after `arena_reset(graph)`; never keep a pointer into it across steps except through parameters.
- The moons dataset: generate in Python with `sklearn.datasets.make_moons(100, noise=0.1)`, write CSV, read in C. Same points → same loss curve as micrograd's demo (modulo init).
- Use `double`, and make PyTorch use `float64` too, or you'll chase 1e-7 "bugs" that are just `float32`.

## Where to put it

`neural_network_c/autograd/` — `arena.h/.c`, `value.h/.c`, `nn_micro.h/.c`, `main.c`, `Makefile`, `check_torch.py`.
