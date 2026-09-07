# P05 — Tiny Transformer

**Difficulty:** ★★★★★   **Prereq chapters:** C++ 01-13   **Builds on:** P04, C P16, C P03

## Goal

A character-level GPT trained on tiny Shakespeare with your own tensor, autograd and layer library: token and positional embeddings, causal self-attention (single-head first, then multi-head), a feed-forward block, residual connections and LayerNorm (pre-norm), a cross-entropy training loop with AdamW and warmup/cosine schedule, and sampling with temperature and top-k. Target: validation loss ≈ 1.5–1.6 nats/char on a 4-layer, 4-head, 128-dim model within an hour of CPU time, producing text that looks like Shakespeare from a distance.

## Why

This is the destination of the whole ML track: tokenizer (C P01/P03) → embeddings (C P16) → autograd (P03) → layers (P04) → attention (here). Nothing new is needed from the framework except one op — attention is matmuls, a softmax, a mask and a transpose. What is new is understanding *why* it works: the same MLP as C P16 but with a learned, content-dependent context window. Everything you read about LLMs afterwards (KV cache, RoPE, flash attention, scaling laws) is a modification of the code you write here.

## The math

Tokens $t_1..t_T$ in a block of length $T$ (context size), vocabulary $V$, model width $d$, heads $h$, head size $d_h = d/h$.

**Embeddings**: $X = E_{\text{tok}}[t] + E_{\text{pos}}[0..T-1] \in \mathbb R^{T\times d}$ (batched: $B\times T\times d$).

**Single-head causal self-attention** on $X$:

$$Q = XW_Q,\quad K = XW_K,\quad V = XW_V \quad (W_\cdot \in \mathbb R^{d\times d_h})$$

$$A = \text{softmax}\!\left(\frac{QK^T}{\sqrt{d_h}} + M\right), \qquad M_{ij} = \begin{cases}0 & j \le i\\ -\infty & j > i\end{cases}, \qquad \text{out} = AV$$

The mask makes position $i$ attend only to $j \le i$ — this is what makes it a *language model*: predicting $t_{i+1}$ from $t_{\le i}$ for all $i$ in parallel. The $1/\sqrt{d_h}$ keeps the logits' variance at 1 when $Q, K$ have unit-variance entries (variance of a dot product of $d_h$ independent unit terms is $d_h$).

**Multi-head**: run $h$ heads with separate $W_Q^{(k)}, W_K^{(k)}, W_V^{(k)}$, concatenate outputs to $T\times d$, project with $W_O \in \mathbb R^{d\times d}$. Implementation: one `Linear(d, 3d)` for QKV, reshape to $[B, T, 3, h, d_h]$, transpose to $[B, h, T, d_h]$, batched matmul — this is why P02 needed rank-4 batched matmul and P03 needed transpose/reshape gradients.

**Block** (pre-norm, as in GPT-2):

$$X \leftarrow X + \text{MHA}(\text{LN}_1(X)), \qquad X \leftarrow X + \text{FFN}(\text{LN}_2(X)), \qquad \text{FFN}(x) = W_2\,\text{GELU}(W_1 x + b_1) + b_2,\ W_1: d\to 4d$$

GELU: $x\,\Phi(x) \approx \tfrac12 x\left(1 + \tanh\left(\sqrt{2/\pi}\,(x + 0.044715x^3)\right)\right)$; ReLU is fine too. Dropout after attention weights, after the attention projection, and after the FFN.

**Head**: $\text{logits} = \text{LN}_f(X)\,W_{\text{out}}$, $W_{\text{out}} \in \mathbb R^{d\times V}$, optionally tied to $E_{\text{tok}}^T$. Loss: mean cross-entropy over all $B\cdot T$ positions against targets $t_2..t_{T+1}$.

**Parameter count** (untied): $Vd + Td + L(12d^2 + 13d) + 2d + dV$. For $V=65, T=128, d=128, h=4, L=4$: ≈ 0.8 M.

**Sampling**: start with a context, take logits at the last position, divide by temperature $\tau$, optionally zero all but the top-$k$, softmax, sample, append, crop to the last $T$ tokens, repeat.

**Expected losses** (char-level Shakespeare): bigram 2.45; C P16 MLP ≈ 1.9; this model ≈ 1.5 val after ~5000 steps at batch 32, block 128; nanoGPT's larger CPU config reaches ~1.47.

## Spec

**Interface** (`gpt.hpp`):

```cpp
struct GPTConfig { std::size_t vocab, block, dim, heads, layers; double dropout; bool tie_weights; unsigned seed; };

class CausalSelfAttention : public Module {
public:
    explicit CausalSelfAttention(const GPTConfig&);
    Variable forward(const Variable& x) override;          // x: [B,T,d] -> [B,T,d]
private:
    Linear qkv_, proj_;  Dropout attn_drop_, resid_drop_;
    Tensor<bool> mask_;                                     // [block, block] lower-triangular, built once
    std::size_t heads_, head_dim_;
};
class FeedForward : public Module { /* Linear(d,4d) -> GELU -> Linear(4d,d) -> Dropout */ };
class Block       : public Module { /* LayerNorm, CausalSelfAttention, LayerNorm, FeedForward, pre-norm residuals */ };
class GPT : public Module {
public:
    explicit GPT(const GPTConfig&);
    Variable forward(const Tensor<std::size_t>& idx);     // [B,T] -> logits [B,T,V]
    Variable forward(const Variable&) override;           // throws: use index overload
    Variable loss(const Tensor<std::size_t>& idx, const Tensor<std::size_t>& targets);
    std::vector<std::size_t> generate(std::vector<std::size_t> ctx, std::size_t n, double temperature, std::size_t top_k, unsigned seed);
    const GPTConfig& config() const;
};

Variable gelu(const Variable&);                            // add to autograd if missing
Variable masked_fill(const Variable& x, const Tensor<bool>& mask, T value);   // where(mask, x, value)

// data
struct TokenDataset { std::vector<std::size_t> train, val; std::size_t vocab; };
TokenDataset load_text_char(const std::string& path, double train_frac = 0.9);           // char vocab (C P01 rules)
TokenDataset load_text_bpe (const std::string& path, const std::string& bpe_model, double train_frac = 0.9);   // reads C P03 .bpe
void get_batch(const std::vector<std::size_t>& data, std::size_t B, std::size_t T, unsigned& rng, Tensor<std::size_t>& x, Tensor<std::size_t>& y);
double estimate_loss(GPT&, const std::vector<std::size_t>& data, std::size_t iters, std::size_t B, std::size_t T, unsigned seed);
```

**CLI**

```
./gpt train input.txt --vocab char --layers 4 --heads 4 --dim 128 --block 128 --batch 32 --steps 5000 --lr 1e-3 --warmup 100 --wd 0.1 --dropout 0.1 --eval_every 250 --out ckpt/ --log log.csv
./gpt sample ckpt/ --n 500 --temp 0.8 --topk 40 --seed 1 [--prompt "ROMEO:"]
./gpt train input.txt --vocab bpe model.bpe ...            # with C P03 tokens
```

`log.csv`: `step,train_loss,val_loss,lr,tokens_per_sec`. Checkpoint: `config.json`-like text file + `.npy` per parameter.

**Expected training trace** (4L/4H/128d, block 128, batch 32, M2 Mac):

```
step 0     train 4.17  val 4.17   lr 1e-5
step 250   train 2.50  val 2.52
step 1000  train 1.95  val 2.00
step 2500  train 1.62  val 1.70
step 5000  train 1.42  val 1.56
```

Sample at 5000 steps should have correct-looking character names in caps, line structure, and mostly real words.

## Milestones

1. **M1 — bigram in the framework.** `GPT` with `layers=0` (embeddings → head) trains to val ≈ 2.45–2.5 (matches C P16). You'll know the data pipeline and loss are right when this number appears.
2. **M2 — single-head attention, one block, no mask.** Compare `CausalSelfAttention::forward` (mask disabled, heads=1) output and input-gradient against a hand-written PyTorch version on a `[2, 8, 16]` fixture: 1e-10. Then enable the mask and confirm output at position $i$ does not change when tokens $> i$ are changed (the causality test — write it as a unit test).
3. **M3 — multi-head + full block vs PyTorch.** Load your weights into a `torch` re-implementation (or `nn.MultiheadAttention` with care about weight layout); logits and *all* parameter gradients match to 1e-8 on one batch.
4. **M4 — train the 4-layer model.** Val loss < 1.7 by step 2500. Gradient norm logged; clipping at 1.0 active in the early steps. Tokens/sec printed — expect a few thousand.
5. **M5 — sampling.** Temperature 0.8, top-k 40 gives readable text; temperature 0.2 loops; 1.5 is noise. `--prompt` seeds the context.
6. **M6 — BPE tokens and scaling.** Train on C P03's 512-vocab stream: loss per token ≈ 2.9, per *byte* ≈ 1.5 — comparable. Then a 6-layer/256-dim run overnight; log the loss curve; compare with the 4/128 model at equal wall-clock.

## Verification

```python
import torch, numpy as np, math
torch.set_default_dtype(torch.float64)
x = torch.tensor(np.load("x.npy"), requires_grad=True)        # [2,8,16]
Wqkv = torch.tensor(np.load("qkv_w.npy")); bqkv = torch.tensor(np.load("qkv_b.npy"))
Wo = torch.tensor(np.load("proj_w.npy")); bo = torch.tensor(np.load("proj_b.npy"))
B, T, d = x.shape; h = 4; dh = d // h
qkv = x @ Wqkv + bqkv; q, k, v = qkv.split(d, dim=-1)
q, k, v = (t.view(B, T, h, dh).transpose(1, 2) for t in (q, k, v))
att = (q @ k.transpose(-2, -1)) / math.sqrt(dh)
att = att.masked_fill(torch.tril(torch.ones(T, T)) == 0, float("-inf")).softmax(-1)
y = (att @ v).transpose(1, 2).reshape(B, T, d) @ Wo + bo
y.backward(torch.tensor(np.load("dy.npy")))
print(np.abs(y.detach().numpy() - np.load("y.npy")).max(), np.abs(x.grad.numpy() - np.load("dx.npy")).max())   # < 1e-10
```

Causality unit test: for random $x$, perturb rows $j > i$ of the input; assert `out[:, :i+1]` unchanged to 1e-15.

## Stretch goals

- KV cache for generation: sampling cost per token drops from $O(T^2)$ to $O(T)$; verify identical samples with the same seed.
- Rotary position embeddings (RoPE) instead of learned positions; compare val loss.
- Weight tying (`tie_weights`) and its effect on parameter count and loss.
- Multithreaded batched matmul (`std::thread` over the batch×heads dimension) — measure tokens/sec before and after; this is where the time goes.

## Hints

- Build the causal mask once in the constructor as `Tensor<bool>` `[block, block]`; slice to `[T, T]` per forward. `masked_fill` with `-1e9` (not `-inf`) avoids NaN in `softmax` backward when a whole row is masked — or handle `-inf` carefully.
- The reshape/transpose dance: `qkv: [B,T,3d] -> reshape [B,T,3,h,dh] -> ` slice the 3-axis into q,k,v -> `transpose(1,2)` to `[B,h,T,dh]`. After attention: `transpose(1,2)` back, `.contiguous()`, `reshape [B,T,d]`. Every one of these needs the P03 gradients to be right; the fixture test in M2/M3 is how you find out.
- Batched matmul `[B,h,T,dh] @ [B,h,dh,T]` needs P02's batched matmul with broadcasting over leading dims; `k.transpose(-2,-1)` is a view.
- Initialization matters: `Linear` weights $\mathcal N(0, 0.02)$, residual projections scaled by $1/\sqrt{2L}$ (GPT-2 trick), embeddings $\mathcal N(0, 0.02)$. With He init the loss starts far above $\log V$.
- Training speed: with a naive autograd, ~80% of time is matmul. Make sure your `Tensor` matmul uses the good loop order and `-O2`/`-O3`; consider `-ffast-math` only after verifying gradients.
- `estimate_loss` runs under `NoGradGuard` and `eval()`; forgetting `eval()` leaves dropout on and val loss ~0.1 too high.
- Save `config` as plain text (`key value` lines) so `sample` can rebuild the model before loading weights.

## Where to put it

`cpp/gpt/` — `include/gpt.hpp`, `src/gpt.cpp`, `src/data.cpp`, `apps/gpt.cpp`, `tests/test_attention.cpp`, `tests/gen_fixtures.py`, `CMakeLists.txt`. Data (`input.txt`, `model.bpe`) in `cpp/data/` (gitignored).
