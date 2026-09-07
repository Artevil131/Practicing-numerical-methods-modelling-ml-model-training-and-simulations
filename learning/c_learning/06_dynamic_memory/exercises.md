# Chapter 06 — Exercises

Write each exercise as `ex06_K.c` in this folder (`ex06_1.c`, `ex06_2.c`, ...). Compile every one with both the normal flags and the sanitizer flags:

```
cc -Wall -Wextra -std=c11 -O2 -o ex06_1 ex06_1.c -lm
cc -g -fsanitize=address,undefined -Wall -Wextra -std=c11 -o ex06_1 ex06_1.c -lm
```

An exercise is not done until it runs clean under ASan **and** `leaks` reports zero leaks:

```
cc -g -O0 -Wall -Wextra -std=c11 -o ex06_1 ex06_1.c -lm
MallocStackLogging=1 leaks --atExit -- ./ex06_1
```

(LeakSanitizer is not available on Apple Silicon; `leaks` is Apple's replacement.)

---

## 06.1 — **Heap array of squares**

Read an integer `n` from `argv[1]`. Allocate an array of `n` `long` values on the heap, fill it with `i*i`, print them space-separated, free it. Check the allocation for NULL and print an error to `stderr` if it fails. Handle `n == 0` (print an empty line, no allocation needed or `malloc(0)` handled).

Example: `./ex06_1 5` → `0 1 4 9 16`

<details><summary>Hint</summary>

- `strtol(argv[1], NULL, 10)` converts the argument.
- Use `sizeof *arr`, not `sizeof(long)`.
</details>

---

## 06.2 — **calloc vs malloc, observed**

Allocate 16 `int` with `malloc` and print them; then allocate 16 `int` with `calloc` and print them. Then run the program under ASan. Report in a comment at the top of the file what happened (which read did ASan/UBSan complain about, or whether it happened to print zeros) and why the `malloc` version is UB even when it prints zeros.

Example output (varies):
```
malloc: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0   (or garbage)
calloc: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
```

<details><summary>Hint</summary>

- Fresh pages from the OS are zeroed, which is why small programs often see zeros. Reused blocks are not.
- Try allocating, writing, freeing, then allocating the same size again before printing.
</details>

---

## 06.3 — **Duplicate a string on the heap**

Write `char *my_strdup(const char *s)` that returns a newly allocated copy of `s` (caller owns it). Do not use `strdup`. In `main`, duplicate each `argv[i]`, uppercase the copy in place, print it, and free it. Document ownership in the function comment.

Example: `./ex06_3 hello world` →
```
HELLO
WORLD
```

<details><summary>Hint</summary>

- Length is `strlen(s) + 1` for the terminator.
- `toupper` lives in `<ctype.h>`; pass it an `unsigned char`.
</details>

---

## 06.4 — **Growable array of doubles from stdin**

Read doubles from `stdin` (one per line, or whitespace-separated) until EOF into a growable array (`data/len/cap`, doubling). Then print `count`, `min`, `max`, `mean`. Free everything. Start with `data = NULL, cap = 0` and rely on `realloc(NULL, n)`.

Example: `printf "3\n1\n2\n" | ./ex06_4` → `n=3 min=1 max=3 mean=2`

<details><summary>Hint</summary>

- `fgets` into a line buffer, then `strtod`. Skip lines where `strtod` consumed nothing.
- Never write `data = realloc(data, ...)` directly.
</details>

---

## 06.5 — **Break it four ways**

Write four functions `leak()`, `double_free()`, `use_after_free()`, `overflow()` each containing exactly one of the four bugs. `main` takes `argv[1]` in `{leak, dfree, uaf, overflow}` and calls the matching function. Compile with ASan and run each variant. Paste the *first line* of each ASan error report into a comment above the corresponding function.

Example: `./ex06_5 uaf` → ASan report containing `heap-use-after-free`.

<details><summary>Hint</summary>

- For the leak on macOS, build with `-O0 -g` (no ASan) and run `MallocStackLogging=1 leaks --atExit -- ./ex06_5 leak`; paste the `ROOT LEAK` line.
- Make the overflow an off-by-one (`<=` instead of `<`) so the report says "0 bytes after".
</details>

---

## 06.6 — **Flat matrix vs pointer-to-pointer**

Implement both layouts for a `rows x cols` matrix of `double`:
`double *flat_alloc(size_t rows, size_t cols)` and `double **jagged_alloc(size_t rows, size_t cols)`, plus matching free functions. Fill each with `m[i][j] = i*cols + j` (using the appropriate indexing), then verify the two contain identical values. `jagged_alloc` must clean up correctly if a row allocation fails partway (free the rows already allocated, return NULL).

Example: `./ex06_6 3 4` → `flat and jagged agree on 12 elements`

<details><summary>Hint</summary>

- For the partial-failure path, keep a loop counter you can walk back down.
- You can *test* the failure path by temporarily making the row `malloc` return NULL for `i == 1`.
</details>

---

## 06.7 — **Arena allocator with alignment**

Implement `Arena` with `arena_init`, `arena_alloc(a, n, align)`, `arena_reset`, `arena_free`. `align` must be a power of two; round the offset up to it. Allocate a `char[3]`, then a `double`, then an `int`, and print the offset of each from `arena.base` to show the padding. Then `arena_reset` and allocate again — offsets should restart at 0. Return NULL when full and prove it by requesting more than `cap`.

Example (with align = natural alignment of each type):
```
char[3] at offset 0
double  at offset 8
int     at offset 16
after reset: char[3] at offset 0
request of 1000000 bytes -> NULL (arena full)
```

<details><summary>Hint</summary>

- Round-up formula: `(off + align - 1) & ~(align - 1)`.
- Cast `a->base` to `unsigned char *` so pointer arithmetic is in bytes.
</details>

---

## 06.8 — **Softmax: two signatures** *(ML)*

Implement softmax both ways: `double *softmax_new(const double *x, size_t n)` (returns ownership) and `void softmax_into(double *out, const double *x, size_t n)` (caller buffer). Subtract the max before exponentiating for numerical stability. In `main`, call `softmax_into` 1,000,000 times on a length-8 input reusing one buffer, and time it with `clock()`; then call `softmax_new` 1,000,000 times (freeing each result) and time that. Print both times and the ratio.

Example: `sum of probabilities = 1.000000; into: 0.0xx s, new: 0.1xx s, ratio ≈ N`

<details><summary>Hint</summary>

- `#include <time.h>`; `clock()` returns ticks, divide by `CLOCKS_PER_SEC`.
- The ratio shows why hot loops avoid `malloc`.
</details>

---

## 06.9 — **Token ID buffer for a tokenizer** *(ML)*

Read a text file (path in `argv[1]`) and split it into whitespace-separated words. Assign each *distinct* word an integer ID in order of first appearance (a linear scan over the words seen so far is fine for now — Chapter 10 replaces it with a hash table). Store the IDs of the full text in a growable `int` array and the distinct words in a growable array of heap-allocated `char*`. Print vocabulary size, total token count, and the first 20 IDs. Free every string and both arrays; run under ASan and confirm zero leaks with `leaks`.

Example: file containing `the cat sat on the mat` →
```
vocab=5 tokens=6
0 1 2 3 0 4
```

<details><summary>Hint</summary>

- Two growable arrays with different element types: write the push logic twice or use a `void*`/element-size version.
- `strtok` on a line buffer, then `my_strdup` from 06.3 for each new word.
</details>

---

## 06.10 — **Particle system with per-frame arena** *(sim)*

Simulate `N` particles (`argv[1]`) for `T` steps (`argv[2]`) in 2D with gravity `g = -9.81` and a floor at `y = 0` that reflects velocity with coefficient 0.9. Store positions and velocities as flat heap arrays of length `2*N` (x0,y0,x1,y1,...). Each step, compute a "neighbor count" scratch array (`int[N]`, how many other particles are within distance 1.0) using memory from an `Arena` that is `arena_reset` at the start of every step — no `malloc` inside the time loop. After `T` steps print mean height and the total number of neighbor pairs found in the final step. Initialize positions with `rand()` seeded by a fixed value so output is reproducible.

Example: `./ex06_10 100 200` → `mean_y=2.3xx pairs=NN` (exact numbers depend on your RNG usage; must be deterministic run-to-run)

<details><summary>Hint</summary>

- The O(N²) neighbor loop is fine here; Chapter 10's priority queue and later Barnes-Hut fix that.
- Assert (`assert(arena_alloc(...) != NULL)`) so a too-small arena fails loudly.
- Use `dt = 0.01`.
</details>
