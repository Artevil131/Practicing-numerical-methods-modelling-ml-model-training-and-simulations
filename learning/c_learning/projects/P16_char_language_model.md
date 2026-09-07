# P16 — Character-Level Language Model

**Difficulty:** ★★★★★   **Prereq chapters:** C 01-14   **Builds on:** P12, P13, P01/P03, P11

## Goal

Three language models of increasing power trained on a text file tokenized with your P01 (or P03) tokenizer: (1) a bigram *count* model, (2) the same bigram model as a one-layer neural net trained by gradient descent (they must converge to the same loss), (3) an MLP over a context window of $k$ previous tokens with a learned embedding table (Bengio et al. 2003 — the "makemore" MLP). Each samples text; you log train/validation loss and watch the model go from gibberish to Shakespearean-looking words. This is the last step before attention.

## Why

The C++ transformer (C++ P05) is *this model* plus self-attention: same tokenizer, same embedding table, same cross-entropy over the vocabulary, same sampling loop, same train/val split. Building the embedding lookup and its gradient (a scatter-add) by hand is the piece PyTorch's `nn.Embedding` hides. The bigram count model gives you an exact answer (the maximum-likelihood bigram loss) that the neural bigram must reach — a rare case in deep learning where you know the right number. And you'll see why context length matters, empirically, before attention makes it cheap.

## The math

Tokens $t_1..t_T$ from a vocabulary of size $V$ (65 for char-level Shakespeare, 512 for your BPE).

**Bigram count model.** $N_{ab} = $ count of $a$ followed by $b$. With additive smoothing $\lambda$:

$$P(b \mid a) = \frac{N_{ab} + \lambda}{\sum_{b'} N_{ab'} + \lambda V}$$

Negative log-likelihood per token (the loss everything is compared to):

$$\mathcal L = -\frac1{T-1}\sum_{i=1}^{T-1}\log P(t_{i+1}\mid t_i)$$

For char-level Shakespeare this is ≈ 2.45–2.5 nats (perplexity $e^{\mathcal L} \approx 12$). Uniform guessing gives $\log 65 = 4.17$.

**Neural bigram.** One-hot input $x \in \{0,1\}^V$, logits $z = xW$ with $W \in \mathbb R^{V\times V}$, softmax, cross-entropy. Since $x$ is one-hot, $z = W_{a,:}$: the logits *are* row $a$ of $W$, and $\partial\mathcal L/\partial W_{a,:} = \tfrac1n(p - y)$ for each occurrence — a scatter-add of P11's gradient into the rows indexed by the inputs. At the optimum $\text{softmax}(W_{a,:}) = P(\cdot\mid a)$, so the loss must approach the count model's loss ($\lambda \to 0$). Weight decay corresponds to smoothing.

**MLP with embeddings (Bengio 2003).** Context $(t_{i-k}, \dots, t_{i-1})$, embedding table $C \in \mathbb R^{V\times d}$.

$$\mathbf e = [C_{t_{i-k}}, \dots, C_{t_{i-1}}] \in \mathbb R^{kd} \quad(\text{concatenate rows}), \qquad \mathbf h = \tanh(\mathbf e W_1 + \mathbf b_1), \qquad \mathbf z = \mathbf h W_2 + \mathbf b_2$$

Backward: P12 gives $\partial\mathcal L/\partial \mathbf e$; then the embedding gradient is a scatter-add:

$$\frac{\partial\mathcal L}{\partial C_{v,:}} \mathrel{+}= \frac{\partial\mathcal L}{\partial \mathbf e_{[jd:(j+1)d]}} \quad\text{for every context slot } j \text{ where } t_{i-k+j} = v$$

(Rows of $C$ used many times in a batch accumulate many contributions — the `+=` from P13 again.) Typical: $k = 3$ to $8$, $d = 10$ to $32$, hidden 200. Expected val loss for char Shakespeare: $k = 3$: ≈ 2.1; $k = 8$: ≈ 1.9 (a transformer gets ≈ 1.5).

**Sampling** with temperature $\tau$: $p_b \propto \exp(z_b/\tau)$; draw from the categorical with inverse-CDF on a uniform random number. $\tau = 1$ faithful, $\tau \to 0$ greedy, $\tau = 1.5$ chaotic.

**Train/val split**: first 90% / last 10% of the token stream; never shuffle the *text*, only the sampled context windows.

## Spec

**CLI**

```
./lm bigram  <text> <vocab: char|bpe model.bpe> <lambda>                 # prints train/val NLL, samples 500 tokens
./lm nbigram <text> <vocab...> <lr> <steps> <batch> log.csv               # neural bigram; must reach bigram NLL
./lm mlp     <text> <vocab...> --ctx 8 --emb 16 --hidden 200 --lr 1e-3 --steps 20000 --batch 64 --seed 0 --out ckpt/ log.csv
./lm sample  ckpt/ <n_tokens> <temperature> <seed>
```

`log.csv`: `step,train_loss,val_loss` (val evaluated every 200 steps on a fixed 4096-window subset).

**Signatures** (`lm.h`, `bigram.c`, `dataset.c`, `embedding.c`, `lm_mlp.c`, `sample.c`):

```c
typedef struct { int *ids; size_t n; int vocab_size; } TokenStream;
TokenStream tokens_from_file(const char *path, const char *vocab_kind, const char *bpe_path);
void tokens_split(const TokenStream *all, double frac, TokenStream *train, TokenStream *val);

/* count model */
Matrix *bigram_counts(const TokenStream *s);                       /* V x V */
Matrix *bigram_probs(const Matrix *counts, double lambda);
double  bigram_nll(const Matrix *probs, const TokenStream *s);
int     sample_categorical(const double *p, int n, unsigned *rng_state);

/* dataset: batch of contexts + targets */
void    make_batch(const TokenStream *s, int ctx, int batch, unsigned *rng_state,
                   int *X /* batch*ctx */, int *y /* batch */);

/* embedding layer: params C (V x d); forward gathers, backward scatter-adds */
typedef struct { Matrix *C, *dC; int V, d, ctx; int *last_idx; int last_batch; } Embedding;
Embedding *emb_create(int V, int d, int ctx, unsigned seed);
Matrix    *emb_forward(Embedding *e, const int *X, int batch);     /* batch x (ctx*d) */
void       emb_backward(Embedding *e, const Matrix *dE);           /* fills dC */

/* model: Embedding -> Linear(ctx*d, H) -> tanh -> Linear(H, V); reuse P12's Layer/Network + Adam */
typedef struct { Embedding *emb; Network *net; int ctx, V; } CharLM;
CharLM *lm_create(int V, int ctx, int d, int hidden, unsigned seed);
double  lm_loss_and_grad(CharLM *m, const int *X, const int *y, int batch);
double  lm_eval(CharLM *m, const TokenStream *s, int n_windows, unsigned seed);
void    lm_sample(CharLM *m, const int *seed_ctx, int n, double temperature, unsigned *rng_state, int *out);
int     lm_save(const CharLM *m, const char *dir);
CharLM *lm_load(const char *dir);
```

**Expected outputs** (char-level Shakespeare, 65 tokens):

```
$ ./lm bigram input.txt char 1
train nll 2.4547  val nll 2.4712  perplexity 11.8
sample: "Thes, yofor cany mayon ard whe th tho thedeauhe, ..."
$ ./lm mlp input.txt char --ctx 8 --emb 16 --hidden 200 --steps 20000 ...
step 0      train 4.19  val 4.18
step 2000   train 2.21  val 2.23
step 20000  train 1.88  val 1.95
$ ./lm sample ckpt/ 300 0.8 1
"KING RICHARD: What shall I have the wortless of the sen..."
```

## Milestones

1. **M1 — bigram counts.** $V\times V$ count matrix; the most common bigram in Shakespeare is `"e "` (matches P03 M2). Val NLL ≈ 2.47 with $\lambda = 1$; matches the NumPy snippet to 1e-10. Samples are pronounceable gibberish.
2. **M2 — neural bigram.** With plain SGD (lr ≈ 10-50 on mean loss, since the gradient of a one-hot layer is tiny per row) or Adam, the loss reaches the count model's *unsmoothed* train NLL within 0.01 in a few thousand steps. `softmax(W)` rows ≈ empirical conditional probabilities.
3. **M3 — dataset + embedding forward/backward.** Gradient check the embedding layer with finite differences (small $V = 10, d = 3, k = 2$, batch 5; rows used multiple times in the batch must pass — that tests the scatter-add).
4. **M4 — MLP trains.** ctx 3, emb 10, hidden 200, Adam 1e-3, 10k steps: val ≈ 2.1. Loss curve to CSV. Samples have real English words.
5. **M5 — context sweep.** ctx ∈ {1, 2, 3, 5, 8}: val loss decreases monotonically (≈ 2.45, 2.3, 2.1, 2.0, 1.95). ctx = 1 must match the bigram loss — a strong test that everything is wired right.
6. **M6 — save/load/sample + BPE tokens.** Sample from a loaded checkpoint at temperatures 0.5/1.0/1.5. Then train on P03's 512-token BPE stream: the NLL *per token* is higher (~3.5) but NLL *per byte* (divide by compression ratio) is comparable — compute and report both.

## Verification

```python
import numpy as np, torch
raw = open("input.txt","rb").read(); chars = sorted(set(raw)); stoi = {c:i for i,c in enumerate(chars)}
ids = np.array([stoi[c] for c in raw]); n = int(0.9*len(ids)); tr, va = ids[:n], ids[n:]
V = len(chars); N = np.zeros((V, V)); np.add.at(N, (tr[:-1], tr[1:]), 1)
P = (N + 1) / (N + 1).sum(1, keepdims=True)
print(-np.log(P[va[:-1], va[1:]]).mean())          # your bigram val nll (lambda=1)
# MLP gradient reference: build the same tiny model in torch (float64) with your saved C, W1, b1, W2, b2
# and the same batch X (batch x ctx), y; loss = F.cross_entropy(logits, y); loss.backward();
# compare C.grad to your dC.mat (this is the scatter-add check) — max abs diff < 1e-9.
```

Reference numbers from Karpathy's makemore (same architecture on names.txt) and nanoGPT's bigram baseline (2.45-2.5 on Shakespeare) bracket what you should see.

## Stretch goals

- Batch normalization after the hidden layer (forward with batch stats, backward derived; running stats for eval) — makes deeper/wider MLPs train.
- A hierarchical (WaveNet-style) MLP: merge pairs of context tokens layer by layer instead of flattening all at once.
- Learning-rate finder (sweep lr exponentially for 1000 steps, pick the one before the loss blows up) and a decay schedule.
- Compute per-character *perplexity* on a held-out paragraph of your own writing vs Shakespeare — the model should find yours more surprising.

## Hints

- Reuse everything: P12's `Network` for the hidden and output layers, Adam from P12, `cross_entropy_from_logits` and `softmax_rows_inplace` from P11, the tokenizer from P01/P03. The only new layer is `Embedding`.
- `emb_forward` is a gather: row `b` of the output is the concatenation of `C[X[b*ctx + j]]` for `j = 0..ctx-1`. Keep `last_idx` pointing at `X` (or copy it) so `emb_backward` knows where to scatter.
- Zero `dC` at the start of each `emb_backward`; then `+=` slices of `dE` into the right rows.
- Batches: sample `batch` random offsets `i` in `[ctx, n)`; context is `ids[i-ctx..i)`, target `ids[i]`. Pad the beginning of the stream with a dedicated token (id `V` — grow the vocab by one) or just start at `i = ctx`.
- A fast portable RNG (xorshift64* or splitmix) for sampling; `rand()` is fine but seed-dependent behaviour across platforms is annoying.
- For the neural bigram, initialize $W = 0$ so the initial loss is exactly $\log V$; after training, print `softmax(W[stoi['q']])` — it should put nearly all mass on `u`.

## Where to put it

`neural_network_c/lm/` — `lm.h`, `bigram.c`, `dataset.c`, `embedding.c`, `lm_mlp.c`, `sample.c`, `main.c`, `Makefile` (links `../nn`, `../tokenizer`, `../matrix`), `plot.py`. C++ P05 starts from this model.
