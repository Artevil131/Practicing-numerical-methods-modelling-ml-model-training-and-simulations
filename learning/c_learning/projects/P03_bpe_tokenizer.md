# P03 — Byte-Level BPE Tokenizer

**Difficulty:** ★★★☆☆   **Prereq chapters:** C 06, 10 (plus 01-05)   **Builds on:** P01

## Goal

A byte-pair-encoding tokenizer with the same `encode`/`decode`/`save`/`load` interface as P01, but a vocabulary that is *learned*: starting from 256 byte tokens, repeatedly find the most frequent adjacent token pair in the training text and replace it with a new token. Trained on tiny Shakespeare to a vocab of 512 or 1024, your output must match Karpathy's `minbpe` (`BasicTokenizer`) merge-for-merge.

## Why

This is the tokenizer GPT-2 through GPT-4 use (minus regex pre-splitting and special tokens). The pair counter here needs a hash table keyed on `(int, int)` — the first real hash table you write, and its design (open addressing or chaining, `struct` keys, resize) is reused for the vocab in P16 and for the string-interning table in the C++ transformer. The `int` token streams it emits are what P16 trains on. If you skip this and use char tokens, P16 still works; but you will not understand why LLM vocabularies have 50k-100k entries or why "SolidGoldMagikarp" exists.

## The math

Let the text be a sequence of tokens $t_1, \dots, t_n$, initially bytes ($0 \le t_i < 256$). One training step:

1. Count every adjacent pair: $c(a, b) = |\{ i : t_i = a, t_{i+1} = b \}|$.
2. Pick $(a^*, b^*) = \arg\max c(a,b)$ (ties: `minbpe` picks the pair encountered first in iteration order of a Python dict, i.e. first occurrence in the text; match that to be bit-identical).
3. Allocate a new id $k = 256 + (\text{number of merges so far})$, record merge $(a^*, b^*) \to k$, and replace every occurrence of $a^*, b^*$ in the sequence with $k$ (left to right, non-overlapping).

Repeat until vocab reaches the target size $V$; the number of merges is $V - 256$.

**Encoding** new text: convert to bytes, then apply merges in the order they were learned — at each step, among all adjacent pairs currently present, find the one with the *lowest merge index* and merge all its occurrences; stop when no pair is in the merge table. Merge order = priority.

**Decoding**: each token $k \ge 256$ expands to the concatenation of `decode(a)` and `decode(b)` where $(a,b) \to k$. Precompute `vocab[k]` as a byte string once after training so decode is a lookup and `memcpy`, not recursion.

Compression ratio: $\text{bytes} / \text{tokens}$. Expect roughly 1.5-2.0 at $V = 512$ on Shakespeare and higher for bigger $V$.

Cost: naive training is $O(n)$ per merge (recount everything), so $O(n \cdot (V-256))$. For 1 MB and 768 merges that is ~ $10^9$ simple ops — a few seconds in C; fine. The stretch is the incremental version.

## Spec

**CLI**

```
./bpe train <textfile> <vocab_size> <model.bpe>      # learn merges, save model
./bpe encode <model.bpe> <textfile> <out.bin>         # ids as int32 (same format as P01)
./bpe decode <model.bpe> <in.bin>                     # writes bytes to stdout
./bpe roundtrip <model.bpe> <textfile>                # encode->decode->memcmp, print ratio
```

**Model file format** (`.bpe`, text, so you can diff it against minbpe):

```
bpe v1
<vocab_size>
<a> <b>          # merge 0  -> id 256
<a> <b>          # merge 1  -> id 257
...
```

**Types and signatures:**

```c
typedef struct { int a, b; } Pair;

/* hash table: Pair -> count */
typedef struct PairTable PairTable;
PairTable *pt_create(size_t initial_capacity);
void       pt_free(PairTable *t);
void       pt_increment(PairTable *t, Pair p);          /* +1, inserting if absent */
int        pt_get(const PairTable *t, Pair p);           /* 0 if absent */
Pair       pt_argmax(const PairTable *t, int *out_count); /* most frequent pair */
size_t     pt_size(const PairTable *t);

typedef struct {
    int   vocab_size;          /* 256 + n_merges */
    int   n_merges;
    Pair *merges;              /* merges[i] -> id 256+i */
    unsigned char **vocab;     /* vocab[k]: bytes for token k */
    size_t *vocab_len;
} BPE;

void   bpe_train(BPE *m, const unsigned char *text, size_t len, int vocab_size, int verbose);
int   *bpe_encode(const BPE *m, const unsigned char *text, size_t len, size_t *out_n);
unsigned char *bpe_decode(const BPE *m, const int *ids, size_t n, size_t *out_len);
int    bpe_save(const BPE *m, const char *path);
int    bpe_load(BPE *m, const char *path);
void   bpe_free(BPE *m);

/* helpers you will want */
size_t count_pairs(const int *ids, size_t n, PairTable *t);
size_t merge_pair(int *ids, size_t n, Pair p, int new_id);   /* in place, returns new length */
```

**`train` verbose output** (one line per merge, exactly like minbpe so you can diff):

```
merge 1/256: (101, 32) -> 256 (b'e ') had 27000 occurrences
merge 2/256: (116, 104) -> 257 (b'th') had 22000 occurrences
```

## Milestones

1. **M1 — Pair hash table.** Insert 1e6 random pairs, verify counts against a brute-force array for a small case; grows by rehashing at 70% load. You'll know it works when `pt_get` agrees with a naive $O(n)$ scan on 10k pairs and ASan is clean.
2. **M2 — `count_pairs` + `pt_argmax` on real text.** For Shakespeare, the top pair should be `(101, 32)` = `"e "`. Print top 10 with counts and compare with `collections.Counter(zip(ids, ids[1:])).most_common(10)` in Python.
3. **M3 — `merge_pair` in place.** Test: `[1,2,3,1,2]` merging `(1,2) -> 4` gives `[4,3,4]`. Test overlapping `[1,1,1]` merging `(1,1) -> 2` gives `[2,1]`.
4. **M4 — `bpe_train` to 512 and save.** Diff your `.bpe` merges list against minbpe's (Verification). Tie-breaking must match.
5. **M5 — encode/decode round trip.** `roundtrip` prints `OK` and a compression ratio; your ids for the first 1000 bytes match minbpe's `encode`.
6. **M6 — load model from file and re-encode.** Fresh process, `bpe_load`, encode, ids identical to M5. Test on text containing bytes never seen in training (still round-trips because base vocab is all 256 bytes).

## Verification

```python
# pip install minbpe  (or clone github.com/karpathy/minbpe and add to path)
from minbpe import BasicTokenizer
text = open("input.txt", encoding="utf-8").read()
tok = BasicTokenizer()
tok.train(text, 512, verbose=False)
# merges in learned order
mine = [tuple(map(int, l.split())) for l in open("model.bpe").read().splitlines()[2:]]
ref = list(tok.merges.keys())
print(mine == ref)                          # True
ids_ref = tok.encode(text[:5000])
import numpy as np
ids_mine = np.fromfile("out.bin", dtype=np.int32)[1:]   # encode of the same 5000 chars
print(list(ids_mine) == ids_ref)            # True
```

Also: decoding `[256]` must print the two-byte string of merge 0; the number of ids after encoding Shakespeare at $V=512$ should be roughly 1115394 / 1.7.

## Stretch goals

- Incremental training: keep the pair table updated as you merge instead of recounting; only pairs touching a merged position change. Measure the speedup at $V = 4096$.
- Regex pre-splitting like GPT-2 (split on whitespace/punctuation categories before merging so merges never cross word boundaries). No PCRE in C — implement the simple character-class version.
- Special tokens: `<|endoftext|>` with a reserved id; `encode` recognizes it in text, `decode` emits it.
- Encode with a priority queue keyed on merge rank instead of scanning all pairs each step.

## Hints

- Hash a `Pair` by mixing two ints: e.g. `h = a * 1000003u ^ b`, then `& (capacity - 1)` with power-of-two capacity. Open addressing with linear probing is the least code. Store an "empty" sentinel (`a = -1`).
- `pt_argmax` needs deterministic tie-breaking. Track first-occurrence position in the text alongside the count, or resolve ties by scanning the text once — minbpe's `max(stats, key=stats.get)` returns the first key with the max count in dict insertion order, which is first occurrence order.
- `merge_pair` can be done in place with a read index and a write index. Do not allocate per merge.
- Build `vocab[k]` after training by walking merges in order: `vocab[k] = vocab[a] ++ vocab[b]`. Then decode is a loop of `memcpy`.
- Encoding: loop until no mergeable pair; each iteration scan the current ids for the pair with the smallest merge index (a reverse lookup: `Pair -> merge index`, another hash table or reuse `PairTable` with count = index+1).
- Print bytes for the verbose log with `%c` only for printable ASCII; otherwise `\x%02x`.

## Where to put it

`neural_network_c/tokenizer/` — `bpe.c`, `pairtable.c`, `pairtable.h`, `bpe.h`, extend the P01 `Makefile`. P16 links `bpe.o`.
