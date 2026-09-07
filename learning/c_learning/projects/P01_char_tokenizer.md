# P01 — Character Tokenizer

**Difficulty:** ★☆☆☆☆   **Prereq chapters:** C 01-05   **Builds on:** nothing (first project)

## Goal

A command-line program that reads a text file, builds a sorted vocabulary of every distinct byte that occurs, encodes the text into an array of integer token ids, decodes the ids back to text, and proves the round trip is lossless. You will have your first working "tokenizer" — the same object `tiktoken` or a Hugging Face tokenizer is, just with a vocabulary of single characters.

## Why

Every language model starts with `text -> int[]`. This project is the character-level version; P03 replaces "one char = one token" with learned BPE merges but keeps the exact same interface (`encode`, `decode`, `vocab_save`, `vocab_load`), so design the interface here and P03 slots in. P16 (char language model) consumes the ids this program produces. The lookup table (256-entry array mapping byte to id) is your first hash-table-in-disguise; P03 makes it a real hash table.

## The math

None beyond the mapping. A vocabulary is a bijection between a set of symbols $S$ and $\{0, 1, \dots, |S|-1\}$:

$$\text{encode}(c) = \text{id}[c], \qquad \text{decode}(i) = \text{sym}[i], \qquad \text{decode}(\text{encode}(c)) = c.$$

Ids should be assigned in sorted byte order so the vocabulary is deterministic (same text always gives the same ids — a property you rely on when saving models).

Treat the input as raw bytes (`unsigned char`, 0-255), not "characters". UTF-8 multi-byte sequences will be split into several tokens; that is fine and is exactly what byte-level BPE (P03) does too.

## Spec

**CLI**

```
./tokenizer <textfile>                 # print vocab size, first 40 ids, round-trip result
./tokenizer <textfile> --vocab out.txt # also write the vocab file
./tokenizer <textfile> --encode out.bin # also write encoded ids as int32 binary
```

**Vocab file format** (`out.txt`): one line per token, `id<TAB>byte_value_decimal`, sorted by id. Example:

```
0	10
1	32
2	97
```

**Encoded ids file** (`out.bin`): a 4-byte little-endian `int32` count `n`, followed by `n` little-endian `int32` ids. (You will read this exact format from Python with `np.fromfile(f, dtype=np.int32)` — skip the first element.)

**Types and function signatures to implement** (in `tokenizer.c`; a header is optional at this stage):

```c
typedef struct {
    int size;                 /* number of distinct symbols */
    int byte_to_id[256];      /* -1 if the byte never occurs */
    unsigned char id_to_byte[256];
} Vocab;

char *read_file(const char *path, size_t *out_len);   /* malloc'd buffer, caller frees */
void  vocab_build(Vocab *v, const unsigned char *text, size_t len);
int  *encode(const Vocab *v, const unsigned char *text, size_t len, size_t *out_n);
unsigned char *decode(const Vocab *v, const int *ids, size_t n);   /* NUL-terminated, malloc'd */
int   vocab_save(const Vocab *v, const char *path);
int   vocab_load(Vocab *v, const char *path);
```

`encode` and `decode` return malloc'd buffers the caller frees. `vocab_save`/`vocab_load` return 0 on success, nonzero on failure.

**Output of a run** (exact numbers depend on the file):

```
read 1115394 bytes
vocab size: 65
first 40 ids: 18 47 56 57 58 1 15 47 58 47 64 43 52 10 0 14 43 44 53 56 43 1 61 43 1 54 56 53 41 43 43 42 1 39 52 63 1 44 59 56
round trip: OK (1115394 bytes match)
```

## Milestones

1. **M1 — read the whole file into memory.** `read_file` returns a buffer and its length; print the length. You'll know it works when the length matches `wc -c file`.
2. **M2 — build the vocab.** Mark which bytes occur (a 256-entry presence array), then assign ids in ascending byte order. You'll know it works when `vocab size` matches `python3 -c "print(len(set(open('f','rb').read())))"`.
3. **M3 — encode.** Produce an `int` array of length `len`. You'll know it works when the first 40 ids match the Python check in Verification.
4. **M4 — decode and round-trip test.** `memcmp` the decoded buffer against the original. Print `OK` or the first mismatching offset.
5. **M5 — save/load vocab and ids.** Load the saved vocab into a fresh `Vocab`, encode again, and `memcmp` the two id arrays. Read `out.bin` from NumPy and confirm the ids.
6. **M6 — no leaks, no warnings.** Build with `-fsanitize=address,undefined` and run; ASan must report nothing. Run with a 0-byte file and a file with all 256 byte values.

## Verification

```python
import numpy as np
raw = open("input.txt", "rb").read()
chars = sorted(set(raw))
stoi = {c: i for i, c in enumerate(chars)}
ids = np.array([stoi[c] for c in raw], dtype=np.int32)
print(len(chars))                 # must equal your "vocab size"
print(ids[:40])                   # must equal your "first 40 ids"
mine = np.fromfile("out.bin", dtype=np.int32)
assert mine[0] == len(ids) and np.array_equal(mine[1:], ids)
print("match")
```

Use `input.txt` = tiny Shakespeare (`curl -O https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt`); expected vocab size 65.

## Stretch goals

- Add `--stats`: print the 10 most frequent tokens with their counts and the empirical entropy $H = -\sum_i p_i \log_2 p_i$ in bits/char (Shakespeare is around 4.6).
- Support a `--special <string>` flag that appends a special token (e.g. `<|endoftext|>`) with id = `size` and makes `decode` emit it as that string.
- Write `encode` as a streaming function that processes the file in 4 KB chunks without loading it all — same output.

## Hints

- `fopen` with `"rb"`; find the size with `fseek`/`ftell` or grow a buffer with `realloc` in a read loop. Either is fine; the second is more robust to pipes.
- Index arrays with `unsigned char`, never `char` — `char` may be negative on this platform for bytes >= 128 and you will index before the array.
- The byte-to-id table is a fixed 256-int array: initialize all entries to -1 first.
- `memcmp` returns 0 on equality; do not compare with `strcmp`, the text may contain NUL bytes and `strcmp` stops there.
- For the binary file: write the count first with `fwrite(&n, sizeof n, 1, f)`; arm64 macOS is little-endian, so raw `fwrite` of an `int32_t` array is already the format NumPy expects.
- Every `malloc` in this program has exactly one matching `free` in `main`. Write the `free` the moment you write the `malloc`.

## Where to put it

`neural_network_c/tokenizer/` — `tokenizer.c`, a `Makefile` (one rule is enough), and a `tests/` folder with a tiny fixture text. Later P03 adds `bpe.c` beside it.
