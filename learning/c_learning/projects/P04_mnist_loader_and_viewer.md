# P04 — MNIST Loader and Viewer

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C 08, 12 (plus 01-07)   **Builds on:** P02

## Goal

A loader for the MNIST IDX binary files that returns images as a `Matrix` (one row per image, 784 columns, values in $[0,1]$) and labels as an `int` array; a viewer that writes digit $k$ as a PGM image you can open on macOS; and a per-pixel mean image for each class. After this, every MNIST project (P06, P07, P11, P12) starts with a single `mnist_load` call.

## Why

Real data lives in binary formats with headers, big-endian integers, and off-by-one traps — this is the file I/O and bit manipulation of C 08 and C 12 applied to something you will use for weeks. The PGM/PPM writers you build here are the image output for every simulation project (P07 cluster means, P11 weight visualization, P14 heat frames, P15 N-body frames): a 20-line function you will copy around for the rest of the course. The `[0,1]` normalization and the `.mat` cache reuse P02's format.

## The math

IDX format (from Yann LeCun's page): all integers are 32-bit **big-endian**.

```
images file:  magic 0x00000803 | n_images | n_rows(28) | n_cols(28) | pixels (uint8, row-major, one image after another)
labels file:  magic 0x00000801 | n_labels | labels (uint8)
```

Big-endian to native: given bytes $b_0 b_1 b_2 b_3$, value $= (b_0 \ll 24) | (b_1 \ll 16) | (b_2 \ll 8) | b_3$. Do this on `unsigned char` / `uint32_t`, never signed `char`.

Normalization: $x_{ij} = p_{ij} / 255$.

Per-class mean image for class $c$ with index set $I_c$:

$$\mu_c = \frac{1}{|I_c|}\sum_{i \in I_c} x_i \quad (\text{a 784-vector; reshape to } 28\times28 \text{ for viewing})$$

Global mean and standard deviation over all pixels (for the P12 normalization stretch): $\mu \approx 0.1307$, $\sigma \approx 0.3081$ — well-known values you can check against.

PGM (P5) format: ASCII header `P5\n<width> <height>\n255\n` then `width*height` raw `uint8` bytes. PPM (P6) is the same with `P6` and 3 bytes per pixel.

## Spec

**Data**: download the four `.gz` files (`train-images-idx3-ubyte.gz` etc.; mirrors: `https://storage.googleapis.com/cvdf-datasets/mnist/` or `ossci-datasets.s3.amazonaws.com/mnist/`), `gunzip` them into `data/mnist/`. Do not commit the data; add it to `.gitignore`.

**CLI**

```
./mnist_view <images.idx> <labels.idx> <k>            # print label, ASCII-art digit k, write digit_k.pgm
./mnist_view <images.idx> <labels.idx> --means dir/    # write mean_0.pgm ... mean_9.pgm + counts
./mnist_view <images.idx> <labels.idx> --cache X.mat y.mat   # save normalized Matrix + labels
```

**Signatures** (`mnist.h` / `mnist.c`, plus `image_io.h` / `image_io.c`):

```c
typedef struct {
    Matrix *X;        /* n x 784, values in [0,1] */
    int    *y;        /* n labels 0..9 */
    size_t  n;
    int     rows, cols;   /* 28, 28 */
} MnistData;

uint32_t read_be_u32(FILE *f);                                    /* exits/returns error on EOF */
int  mnist_load(const char *images_path, const char *labels_path, MnistData *out); /* 0 = ok */
void mnist_free(MnistData *d);
void mnist_print_ascii(const MnistData *d, size_t k, FILE *out);  /* " .:-=+*#%@" ramp */
Matrix *mnist_class_mean(const MnistData *d, int label, size_t *out_count);

int  pgm_write(const char *path, const unsigned char *pixels, int width, int height);   /* 0 = ok */
int  ppm_write(const char *path, const unsigned char *rgb, int width, int height);
int  pgm_write_matrix_row(const char *path, const Matrix *m, size_t row, int w, int h); /* scales min..max -> 0..255 */
```

**ASCII output** for `k = 0` of the training set (label 5), first few rows should look like:

```
label: 5
............................
............................
............................
............................
............................
............=+*###%%%%%*=...
```

## Milestones

1. **M1 — parse headers.** Print magic, count, rows, cols for both files. You'll know it works when you see `2051 60000 28 28` and `2049 60000`.
2. **M2 — load one image and print ASCII art.** Digit 0 of the training set is a 5; digit 1 is a 0. Labels match.
3. **M3 — `pgm_write`.** Write `digit_0.pgm`, open it in Preview (or `python3 -c "from PIL import Image; Image.open('digit_0.pgm').show()"`). It is a 5.
4. **M4 — full load into `Matrix` in $[0,1]$.** `mat_sum(X)/(n*784)` prints ~0.1307. Time it: 60k images should load in well under a second.
5. **M5 — class means.** Ten PGMs that look like blurry digits; counts sum to 60000 (5923 zeros, 6742 ones, ...).
6. **M6 — cache to `.mat`** and load it in Python with `load_mat`; compare against `torchvision.datasets.MNIST` or the Python reader below.

## Verification

```python
import numpy as np, gzip, struct
def read_idx(path):
    with open(path, "rb") as f:
        magic, n = struct.unpack(">II", f.read(8))
        if magic == 2051:
            r, c = struct.unpack(">II", f.read(8))
            return np.frombuffer(f.read(), dtype=np.uint8).reshape(n, r*c)
        return np.frombuffer(f.read(), dtype=np.uint8)
X = read_idx("data/mnist/train-images-idx3-ubyte") / 255.0
y = read_idx("data/mnist/train-labels-idx1-ubyte")
print(X.mean(), X.std())                    # 0.1307, 0.3081
print(np.bincount(y))                        # [5923 6742 5958 6131 5842 5421 5918 6265 5851 5949]
from load_mat import load_mat
assert np.allclose(load_mat("X.mat"), X)
m5 = X[y == 5].mean(axis=0)                  # compare with your mean_5.pgm after scaling
```

## Stretch goals

- `mnist_shuffle(MnistData*, unsigned seed)` (Fisher–Yates over rows and labels together) and `mnist_split` into train/validation — P06 onward need both.
- A `--grid 10 10 out.pgm` mode: tile the first 100 digits into a single 280x280 image.
- Write a generic `idx_read` that handles any IDX rank from the type/dimension byte in the magic number.
- Read the `.gz` files directly through `popen("gunzip -c ...", "r")` so you never need to decompress on disk.

## Hints

- `fread` four bytes into `unsigned char b[4]` and assemble; or `fread` a `uint32_t` and byte-swap with `__builtin_bswap32`. Check `fread`'s return value every time.
- Read all pixel bytes with one `fread` into a `malloc`'d `uint8_t` buffer, then convert to `double` in a loop into `X->data`. One image is 784 consecutive bytes = one row of `X`.
- ASCII art: map `[0,1]` to an index into a 10-char ramp string; print 28 chars per line.
- For the PGM of a `Matrix` row, find min and max, scale to 0-255, cast to `unsigned char`. Mean images have max well below 1, so scaling matters.
- `fopen(path, "wb")` for PGM; the header is text (`fprintf`), the body is `fwrite`. Mixing is fine on the same `FILE*`.
- Class means: accumulate in a `double[784]`, divide by count. Don't build a per-class `Matrix` copy of the data.

## Where to put it

`neural_network_c/mnist/` — `mnist.h`, `mnist.c`, `image_io.h`, `image_io.c`, `mnist_view.c`, `Makefile` (uses `../matrix/matrix.o`). Data goes in `neural_network_c/data/mnist/` (gitignored).
