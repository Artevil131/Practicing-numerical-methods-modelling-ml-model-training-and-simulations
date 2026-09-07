# Chapter 04 — Exercises

Write each exercise as `ex04_K.c` in this folder. Compile with:

```sh
cc -Wall -Wextra -std=c11 -O2 -o ex04_K ex04_K.c -lm
```

Run each one at least once with `-fsanitize=address -O1 -g` added. Zero warnings, zero sanitizer reports.

---

**04.1 — Array basics and the length rule**

Declare `double v[] = {3.0, 1.0, 4.0, 1.0, 5.0, 9.0, 2.0, 6.0}`. Write `static double vec_sum(const double *v, size_t n)`, `static double vec_max(const double *v, size_t n)`, and `static size_t vec_argmin(const double *v, size_t n)`. Compute the length with a `ARRAY_LEN` macro in `main` and print sum, max, argmin. Then add a `static void reverse(double *v, size_t n)` that reverses in place using the `i-- > 0` idiom or a two-index swap, and print the reversed array.

Example:

```
sum=31 max=9 argmin=1
reversed: 6 2 9 5 1 4 1 3
```

<details><summary>Hint</summary>
Swap `v[i]` and `v[n-1-i]` for `i < n/2`. `vec_argmin` starts from index 0, not from a sentinel.
</details>

---

**04.2 — Initialization forms**

Declare five `int` arrays of size 6 using: full initialization, partial `{1, 2}`, `{0}`, designated `{[1] = 5, [4] = 7}`, and inference from `{9, 8, 7}` (this one will have size 3). Print each with its `sizeof`-derived length. Then declare `static int s[6];` and a plain local `int u[6];` -- print the static one, and in a comment explain why printing `u` before writing to it is UB.

Example:

```
full:       1 2 3 4 5 6 (6)
partial:    1 2 0 0 0 0 (6)
```

<details><summary>Hint</summary>
Write one `static void print_ints(const char *label, const int *a, size_t n)` and call it five times.
</details>

---

**04.3 — Flat matrix: transpose and multiply**

Store `A` (2x3) `{1,2,3,4,5,6}` and `B` (3x2) `{7,8,9,10,11,12}` as flat arrays. Write `static size_t idx(size_t i, size_t j, size_t cols)`, `static void mat_print(const double *m, size_t r, size_t c)`, `static void mat_transpose(const double *m, size_t r, size_t c, double *out)`, and `static void mat_mul(const double *a, size_t ar, size_t ac, const double *b, size_t bc, double *out)`. Print `A^T` (3x2) and `A*B` (2x2). Then verify `(A*B)^T == B^T * A^T` by computing both and comparing with `memcmp` or a loop.

Example:

```
A*B =
   58.0   64.0
  139.0  154.0
identity holds: yes
```

<details><summary>Hint</summary>
`out[idx(j, i, r)] = m[idx(i, j, c)]` for transpose. In `mat_mul` re-zero the accumulator per output cell. Compare doubles with `fabs(x - y) < 1e-12`, not `memcmp`, if you did any operation in a different order.
</details>

---

**04.4 — Safe string building**

Write `static int build_filename(char *buf, size_t size, const char *prefix, int frame, const char *ext)` producing `prefix_0007.ext` and returning 0 on success or -1 if truncated. Test with a 64-byte buffer and with a 10-byte buffer. Then write `static void join_doubles(char *buf, size_t size, const double *v, size_t n, char sep)` that produces `1.5,2.25,3` with no trailing separator and no overflow, even when the buffer is too small (it must stop cleanly, still NUL-terminated).

Example:

```
frame_0007.ppm
truncated: frame_000
1.5,2.25,3
```

<details><summary>Hint</summary>
`snprintf` returns the length it wanted; compare it with `size`. For the join loop, track an offset and stop when `offset >= size - 1`.
</details>

---

**04.5 — Robust integer parsing**

Write `static int parse_int(const char *s, long *out)` that returns 0 and sets `*out` if the ENTIRE string (allowing leading/trailing whitespace) is a valid decimal integer in `long` range, and returns -1 otherwise. Test on `"42"`, `"  -17  "`, `"abc"`, `"42abc"`, `""`, `"99999999999999999999"`, `"0x1F"`, `"+5"`. Print each input and either the value or `error`.

Example:

```
"42"      -> 42
"  -17  " -> -17
"abc"     -> error
```

<details><summary>Hint</summary>
Use `strtol` with base 10, check `end != s`, `errno != ERANGE`, then skip trailing `isspace` and require `*end == '\0'`. Reset `errno = 0` before the call. `"0x1F"` should be an error in base 10 (it parses `0` then stops at `x`).
</details>

---

**04.6 — Character statistics**

For the string `"The Quick Brown Fox, 42 times; jumps over 7 lazy dogs!"`, count uppercase letters, lowercase letters, digits, whitespace, and punctuation using `<ctype.h>`. Then build a 26-element histogram of letter frequencies (case-insensitive) and print the letters with nonzero counts in the form `a:1 b:1 ...`. Finally print the string with every letter shifted by 13 positions (ROT13), preserving case and leaving non-letters unchanged.

Example (last lines):

```
upper=4 lower=35 digit=3 space=10 punct=3
...
Gur Dhvpx Oebja Sbk, 42 gvzrf; whzcf bire 7 ynml qbtf!
```

<details><summary>Hint</summary>
Histogram index is `tolower(c) - 'a'`. ROT13: `(c - 'a' + 13) % 26 + 'a'` for lowercase. Cast to `unsigned char` before every ctype call.
</details>

---

**04.7 — UTF-8 code point counter and byte dumper**

For each string in `{"hello", "héllo", "日本語", "🙂 ok", ""}`, print its `strlen`, its number of UTF-8 code points, and its bytes in hex. Then write `static int utf8_seq_len(unsigned char first)` returning 1, 2, 3, or 4 based on the leading byte pattern (or -1 for an invalid leading byte / continuation byte), and use it to print each code point's byte count in sequence.

Example:

```
"héllo": bytes=6 codepoints=5 hex=68 c3 a9 6c 6c 6f seqs=1 2 1 1 1
```

<details><summary>Hint</summary>
Leading-byte tests: `< 0x80` -> 1; `(b & 0xE0) == 0xC0` -> 2; `(b & 0xF0) == 0xE0` -> 3; `(b & 0xF8) == 0xF0` -> 4. Advance by the sequence length.
</details>

---

**04.8 — CSV row parser for a dataset loader (ML)**

Write `static int parse_row(const char *line, double *out, int max, int *n_out)` that parses a comma-separated row of doubles WITHOUT `strtok`, preserving empty fields as `NAN`, tolerating spaces around values, and returning -1 (with the offending column index printed to stderr) if any field is not a number. Test on `"1.5, 2.25,3,,4"`, `"0.1,0.2,0.3"`, `"1,abc,3"`, and `"7"`. Then compute the mean of each successfully parsed row ignoring `NAN`s.

Example:

```
row 0: 5 fields [1.5 2.25 3 nan 4] mean=2.6875
row 2: error in column 1
```

<details><summary>Hint</summary>
For each field: skip spaces, if the next char is `,` or `'\0'` the field is empty; else `strtod`, then skip spaces, then require `,` or `'\0'`. `isnan` from `<math.h>`.
</details>

---

**04.9 — Byte-pair counting, the core of BPE (ML)**

Treat the string `"aaabdaaabac"` as an array of `unsigned char`. Build a 256x256 flat `int` count table of adjacent byte pairs (`counts[a * 256 + b]`), find the most frequent pair, print it and its count, then produce a new byte array where every occurrence of that pair is replaced with a single new token value `256` (so the output must be an `int` array, not `char`). Print the token sequence before and after. Repeat the merge step once more on the result (you will need the count table to be indexable by values up to 256, so make it 257x257 or use a `long` key).

Example:

```
tokens: 97 97 97 98 100 97 97 97 98 97 99
best pair (97,97) count=4
after merge: 256 97 98 100 256 97 98 97 99
```

<details><summary>Hint</summary>
Count pairs as they appear scanning left to right (`aaa` contributes 2 pairs). When merging, scan left to right and skip ahead by 2 after a merge so overlapping occurrences are not double-merged. A 257*257 `int` table is 264 KB; that fits on the stack, but declare it `static` so it is zero-initialized and off the stack. Remember to reset it between merge rounds.
</details>

---

**04.10 — 2-D grid stencil for a heat/FDTD step (sims)**

Allocate a flat `double grid[32 * 32]` initialized to 0 with a hot spot `grid[idx(16,16,32)] = 100.0`. Write `static void step(const double *in, double *out, size_t nx, size_t ny, double alpha)` that applies the 5-point stencil `out[i,j] = in[i,j] + alpha * (in[i-1,j] + in[i+1,j] + in[i,j-1] + in[i,j+1] - 4 in[i,j])` on interior points and copies boundaries unchanged. Run 50 steps with `alpha = 0.2`, swapping two buffers (no copying the whole grid). Print the total heat (sum) before and after (should be conserved up to boundary losses), and render the final grid as ASCII with `" .:-=+*#%@"` mapped from the value range.

Example (partial):

```
total before=100 after=99.99...
             .:.
            .:=:.
           .:=+=:.
```

<details><summary>Hint</summary>
Swap with two pointers: `double *a = buf0, *b = buf1; ... step(a, b, ...); double *t = a; a = b; b = t;`. The character index is `(int)(9.0 * v / vmax)` clamped to 0..9.
</details>
