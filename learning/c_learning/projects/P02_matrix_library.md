# P02 — Matrix Library

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C 01-09   **Builds on:** P01 (file I/O habits)

## Goal

A two-file library, `matrix.h` / `matrix.c`, that is your NumPy: a `Matrix` struct holding a flat row-major `double` buffer plus shape, and functions to allocate, free, fill, randomize, transpose, multiply, add, scale, apply an elementwise function, print, and save/load in a binary format. A `test_matrix.c` with `assert`-based tests and a `Makefile` that builds the library, the tests, and runs them.

## Why

Every later ML project (P05, P06, P07, P11, P12, P16) is written against this header. If `mat_matmul` is wrong, backprop is wrong. The binary save/load format you define here is what P04 writes MNIST into and what P12 uses to checkpoint weights. In C++ P01 you port this class-for-function and benchmark against it, so keep the C version simple and correct — you will measure it later. The `apply(fn)` function is your first use of function pointers, which P09 (ODE solvers) and P12 (`Layer` with forward/backward pointers) depend on.

## The math

Row-major storage: element $(i, j)$ of an $m \times n$ matrix lives at flat index $i \cdot n + j$.

Matrix product $C = AB$ with $A \in \mathbb{R}^{m \times k}$, $B \in \mathbb{R}^{k \times n}$:

$$C_{ij} = \sum_{p=0}^{k-1} A_{ip} B_{pj}$$

Three nested loops. The loop order matters for speed (see Stretch): the inner loop should walk memory contiguously. Order `i, p, j` (accumulate a row of `C` while scanning a row of `B`) is typically 2-5x faster than the textbook `i, j, p` for large matrices because `B[p*n + j]` is contiguous in `j`.

Transpose: $A^T_{ji} = A_{ij}$.

Broadcasting is **not** in scope except one case you need constantly: adding a $1 \times n$ row vector (a bias) to every row of an $m \times n$ matrix. Implement that as its own function rather than general broadcasting.

## Spec

**Header `matrix.h`** — implement exactly these (add more if you need them, but these names are what later specs assume):

```c
#ifndef MATRIX_H
#define MATRIX_H
#include <stddef.h>
#include <stdio.h>

typedef struct {
    size_t rows, cols;
    double *data;          /* rows*cols doubles, row-major */
} Matrix;

Matrix *mat_alloc(size_t rows, size_t cols);        /* zero-filled */
void    mat_free(Matrix *m);
Matrix *mat_copy(const Matrix *a);
void    mat_fill(Matrix *m, double value);
void    mat_random_uniform(Matrix *m, double lo, double hi, unsigned seed);
void    mat_random_normal(Matrix *m, double mean, double std, unsigned seed);

double  mat_get(const Matrix *m, size_t i, size_t j);
void    mat_set(Matrix *m, size_t i, size_t j, double v);

Matrix *mat_transpose(const Matrix *a);
Matrix *mat_matmul(const Matrix *a, const Matrix *b);      /* returns NULL on shape mismatch */
Matrix *mat_add(const Matrix *a, const Matrix *b);         /* elementwise, same shape */
Matrix *mat_sub(const Matrix *a, const Matrix *b);
Matrix *mat_hadamard(const Matrix *a, const Matrix *b);    /* elementwise product */
Matrix *mat_add_rowvec(const Matrix *a, const Matrix *row); /* row is 1 x cols, added to every row */
void    mat_scale_inplace(Matrix *m, double s);
void    mat_apply_inplace(Matrix *m, double (*fn)(double));
Matrix *mat_apply(const Matrix *a, double (*fn)(double));

double  mat_sum(const Matrix *m);
Matrix *mat_sum_rows(const Matrix *m);    /* 1 x cols: column sums (like a.sum(axis=0)) */
Matrix *mat_sum_cols(const Matrix *m);    /* rows x 1: row sums   (like a.sum(axis=1)) */
size_t  mat_argmax_row(const Matrix *m, size_t i);

int     mat_equal(const Matrix *a, const Matrix *b, double tol);
void    mat_print(const Matrix *m, FILE *out);   /* NumPy-like, 4 decimals, max 8x8 then "..." */

int     mat_save(const Matrix *m, const char *path);   /* 0 on success */
Matrix *mat_load(const char *path);                    /* NULL on failure */
#endif
```

**Binary file format** (`.mat`): magic bytes `"MAT1"` (4 bytes), then `uint64 rows`, `uint64 cols`, then `rows*cols` little-endian `float64` values. Python reads it with:

```python
import numpy as np
def load_mat(path):
    with open(path, "rb") as f:
        assert f.read(4) == b"MAT1"
        r, c = np.frombuffer(f.read(16), dtype=np.uint64)
        return np.frombuffer(f.read(), dtype=np.float64).reshape(int(r), int(c))
```

**Makefile targets**: `make` builds `libmatrix.a` (or just `matrix.o`) and `test_matrix`; `make test` runs it; `make clean`. Flags: `-Wall -Wextra -std=c11 -O2`, link `-lm`. Add a `debug` target with `-g -fsanitize=address,undefined`.

**Tests** (`test_matrix.c`): each test is a `static void test_xxx(void)` with `assert`s; `main` calls them all and prints `all tests passed`. Include at minimum: alloc is zeroed; get/set round trip; transpose of a 2x3 has shape 3x2 and correct entries; matmul of a hand-computed 2x3 · 3x2; matmul with mismatched shapes returns NULL; identity times A equals A; `(AB)^T == B^T A^T`; add_rowvec; save then load gives `mat_equal` true; `mat_apply` with `exp`.

**Random numbers**: use `srand(seed)` + `rand()` for uniform; for normal use Box–Muller: with $u_1, u_2 \sim U(0,1)$, $z = \sqrt{-2 \ln u_1}\cos(2\pi u_2)$ is $\mathcal{N}(0,1)$. Guard against $u_1 = 0$.

## Milestones

1. **M1 — alloc/free/get/set/print.** You'll know it works when a 3x4 matrix prints zeros, `mat_set(m,1,2,7)` prints 7 at row 1 col 2, and ASan reports no leaks.
2. **M2 — transpose, add, scale, hadamard, add_rowvec.** Tests pass; transpose of transpose is `mat_equal` to the original.
3. **M3 — matmul.** Hand-verify a 2x3 · 3x2. Then verify a random 50x70 · 70x30 against NumPy (Verification). Shape mismatch returns NULL and does not crash.
4. **M4 — apply, sums, argmax.** `mat_apply(m, exp)` matches `np.exp`; `mat_sum_rows` matches `a.sum(axis=0)`.
5. **M5 — save/load.** Save a random matrix, load in Python with `load_mat`, compare with `np.allclose`. Load a file saved by NumPy (write it with `struct.pack` + `tofile`) and print it.
6. **M6 — Makefile + tests.** `make test` compiles from clean with zero warnings and prints `all tests passed`. Commit.

## Verification

```python
import numpy as np, subprocess
from load_mat import load_mat        # the helper above
rng = np.random.default_rng(0)
# Have your test binary save A.mat, B.mat, C.mat (=A@B), AT.mat (=A^T) for random A(50x70), B(70x30)
A, B, C, AT = (load_mat(f"{n}.mat") for n in ("A", "B", "C", "AT"))
print(np.abs(A @ B - C).max())           # expect < 1e-12
print(np.abs(A.T - AT).max())            # expect 0.0
E = load_mat("E.mat")                    # your mat_apply(A, exp)
print(np.abs(np.exp(A) - E).max())       # expect < 1e-12
```

Benchmark check (for the stretch): `mat_matmul` on 512x512 should take well under 1 s at `-O2` with the `i,p,j` loop order; if it takes several seconds, your inner loop strides through memory.

## Stretch goals

- Time the three loop orders (`ijp`, `ipj`, `pij`) on 512x512 with `clock_gettime`; write a table of GFLOP/s ($2n^3$ flops). Explain the fastest one in terms of cache lines.
- Add `mat_matmul_into(Matrix *out, const Matrix *a, const Matrix *b)` that writes into a preallocated output — no `malloc` in the training loop of P12.
- Add a `MAT_AT(m, i, j)` macro and compare its speed with `mat_get`/`mat_set` (function call overhead vs inlining; check `-O2` output with `-S`).
- Blocked (tiled) matmul with a block size of 32 or 64; measure the speedup at 1024x1024.

## Hints

- `mat_alloc` needs two allocations (the struct and the data) or one with a flexible array member; two is simpler and what the header above implies. `mat_free` must free both, in the right order, and tolerate `NULL`.
- Check shapes at the top of every binary op and return `NULL` (or `abort()` with a message in debug builds) — a silent wrong-shape matmul is the worst bug you will have in P12.
- Write the tests before the matmul. A 2x3 · 3x2 you can verify on paper catches 90% of index errors.
- For `mat_print`, print shape first (`Matrix 3x4`), then rows with `%9.4f`. Truncate large matrices like NumPy does.
- `fwrite(m->data, sizeof(double), rows*cols, f)` writes the whole buffer in one call. Check the return value.
- Header guard, `static` for internal helpers, and never `#include` a `.c` file.

## Where to put it

`neural_network_c/matrix/` — `matrix.h`, `matrix.c`, `test_matrix.c`, `Makefile`. Every later C project adds `-I../matrix` and links `matrix.o`.
