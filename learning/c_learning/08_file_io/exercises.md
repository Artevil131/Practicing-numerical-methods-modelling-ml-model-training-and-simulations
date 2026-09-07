# Chapter 08 — Exercises

Write each exercise as `ex08_K.c` in this folder. Compile with:

```
cc -Wall -Wextra -std=c11 -O2 -o ex08_1 ex08_1.c -lm
```

Create any input files you need in this folder too (name them `ex08_K_*.txt` / `.csv` / `.bin`). Run everything under `-fsanitize=address,undefined` at least once; I/O code has many error paths and each one must close and free.

---

## 08.1 — **`cat` with error handling**

Write a program that takes any number of file paths on the command line and copies each to `stdout` using `getc`/`putc`. If a file cannot be opened, print `perror`-style message with the filename to `stderr` and continue with the next file. Exit status 1 if any file failed, else 0. With no arguments, copy `stdin` to `stdout`.

Example: `./ex08_1 a.txt missing.txt` → contents of `a.txt`, then `missing.txt: No such file or directory` on stderr, exit 1.

<details><summary>Hint</summary>

- `int c;` not `char c;` for the `getc` result.
- `perror(argv[i])` prints the filename and the reason.
</details>

---

## 08.2 — **Line statistics with `fgets`**

Read a text file and print: number of lines, number of non-empty lines, length of the longest line, and the line number of the longest line. Handle lines longer than your buffer correctly (a line only ends where `\n` appears). Test on a file containing a 10,000-character line to prove it.

Example:
```
lines=42 nonempty=39 longest=10000 (line 17)
```

<details><summary>Hint</summary>

- After `fgets`, if the buffer does not end with `\n` (and you are not at EOF), the line continues in the next call.
- Generate the long-line test file with `python3 -c "print('x'*10000)" > ex08_2_long.txt`.
</details>

---

## 08.3 — **`feof` bug, demonstrated and fixed**

Write two functions that count the numbers in a whitespace-separated text file: `count_wrong` using `while (!feof(f)) { fscanf(f, "%lf", &x); n++; }` and `count_right` using the return value of the read. Run both on a file with 5 numbers and print both counts. Add a comment explaining exactly why the wrong version is off by one. Then make a file whose last line is `5 abc` and show what each version does (the wrong one may loop forever — protect it with a cap of 100 iterations).

Example:
```
count_wrong=6 count_right=5
with 'abc': count_wrong=100 (capped) count_right=5
```

<details><summary>Hint</summary>

- `fscanf` returns the number of items matched: 1 on success, 0 on a non-number, `EOF` at end.
- On `abc`, `fscanf` consumes nothing and returns 0 forever.
</details>

---

## 08.4 — **Binary round trip of a struct array**

Define `typedef struct { uint32_t id; float x, y; uint8_t flags; } Rec;`. Print `sizeof(Rec)`. Fill an array of 10 records, `fwrite` them to `ex08_4.bin`, read them back with `fread` into a fresh array, and verify with a comparison function that every field matches. Then use `fseek` to jump directly to record 7, read just that one, and print it. Print the file size from `ftell` and confirm it equals `10 * sizeof(Rec)`.

Example:
```
sizeof(Rec)=16  file=160 bytes  round-trip OK  rec[7] = {7, 7.5, -7.5, 0x07}
```

<details><summary>Hint</summary>

- `sizeof(Rec)` is 16, not 13: three bytes of tail padding are written to the file too.
- `fseek(f, 7 * sizeof(Rec), SEEK_SET)`.
</details>

---

## 08.5 — **Read entire file, count words and bytes**

Implement `char *read_entire_file(const char *path, size_t *len)` (caller frees). Use it to load a text file and print byte count, line count, and word count (whitespace-separated). Then load a *binary* file (your `ex08_4.bin`) with the same function and print the byte count to show it handles embedded zero bytes (do not use `strlen` for the size).

Example: `./ex08_5 lesson.md` → `bytes=31207 lines=452 words=4801`

<details><summary>Hint</summary>

- `fseek`/`ftell`/`rewind` to size, then one `fread`.
- Every error path: `fclose` and `free`.
</details>

---

## 08.6 — **Endianness and `read_be32`**

Write `uint32_t bswap32(uint32_t)`, `uint32_t read_be32(FILE*)`, and `void write_be32(FILE*, uint32_t)`. Detect and print the host's endianness. Write the values `1, 256, 0x00000803, 0xDEADBEEF` to a file with `write_be32`, then dump the file's raw bytes in hex (using `getc`) to show the big-endian order, then read them back with `read_be32` and print them.

Example:
```
host: little-endian
raw bytes: 00 00 00 01 00 00 01 00 00 00 08 03 de ad be ef
read back: 1 256 2051 3735928559
```

<details><summary>Hint</summary>

- Detect: `uint16_t one = 1; *(uint8_t*)&one == 1` means little-endian.
- Build from bytes with `<< 24 | << 16 | << 8 |` — no host assumptions.
</details>

---

## 08.7 — **PGM gradient and a PPM colormap**

Write `pgm_write` and `ppm_write`. Produce `ex08_7_gradient.pgm` (256x64, horizontal gradient 0..255) and `ex08_7_colormap.ppm` (256x32) that shows a blue→white→red colormap of `t` in `[0,1]` across the width. Open them in Preview (macOS opens `.pgm`/`.ppm` natively) or with `python3 -c "from PIL import Image; Image.open('ex08_7_colormap.ppm').show()"`.

Example: two image files; `head -c 15 ex08_7_gradient.pgm | xxd` shows `P5\n256 64\n255\n`.

<details><summary>Hint</summary>

- Colormap: for `t<0.5` interpolate blue→white, else white→red.
- Header with `fprintf`, pixels with one `fwrite`.
</details>

---

## 08.8 — **CSV → Matrix → summary CSV** *(ML)*

Implement `csv_read(path, Matrix*)` (header row, growable, error with line number) and `csv_write(path, const Matrix*, const char **colnames)`. Read a CSV, compute per-column `mean`, `std`, `min`, `max`, and write them as a 4-row CSV with the original column names as header and a leading `stat` column. Test with a hand-made 6x3 CSV and with a deliberately broken file (a row with a missing field) to confirm the error message names the line.

Example input `ex08_8_data.csv`:
```
x,y,z
1,2,3
4,5,6
```
Output `ex08_8_stats.csv`:
```
stat,x,y,z
mean,2.5,3.5,4.5
...
```

<details><summary>Hint</summary>

- Keep the header names: copy the first line's fields into a heap array of strings before parsing numbers.
- Population std: `sqrt(mean(x^2) - mean(x)^2)` is numerically poor — use two passes.
</details>

---

## 08.9 — **MNIST IDX reader → PGM contact sheet** *(ML)*

Download and `gunzip` `train-images-idx3-ubyte` and `train-labels-idx1-ubyte` (or generate a fake IDX file with the same header layout if you are offline: 100 images of 28x28 with a simple pattern). Write `idx3_read` and `idx1_read` using `read_be32`. Validate the magics (`0x803`, `0x801`) and that image and label counts match. Write the first 100 images as a 10x10 grid into one PGM (280x280) and print the first 10 labels. Convert the first image to `double` in `[0,1]` and print its mean intensity.

Example:
```
images: n=60000 28x28   labels: n=60000
labels[0..9] = 5 0 4 1 9 2 1 3 1 4
mean intensity of image 0 = 0.1377
wrote ex08_9_sheet.pgm
```

<details><summary>Hint</summary>

- Image `k` at grid `(k/10, k%10)`: copy row `r` of the image to sheet row `(k/10)*28 + r`, column offset `(k%10)*28`.
- `size_t total = (size_t)n * rows * cols;` before `malloc` — avoid `int` overflow.
</details>

---

## 08.10 — **Heat diffusion frames** *(sim)*

Simulate 1D heat diffusion on `N = 256` cells with the explicit scheme `u_new[i] = u[i] + alpha*(u[i-1] - 2u[i] + u[i+1])`, `alpha = 0.25`, fixed boundaries `u[0]=u[N-1]=0`, initial hot spot `u[N/2 .. N/2+8] = 1`. Every 10 steps for 500 steps, write (a) a row to `ex08_10_energy.csv` with `step,total_heat,max_u` and (b) a 256x32 PGM `ex08_10_frame_%03d.pgm` where each column's brightness is `u[i]` scaled to 0..255. Use `snprintf` to build filenames and `fflush` the CSV after each row. Then plot `ex08_10_energy.csv` in Python and assemble the frames with `ffmpeg -framerate 10 -i ex08_10_frame_%03d.pgm out.mp4` (if you have ffmpeg) or view a few in Preview.

Example: 50 PGM files + a CSV whose `total_heat` column is roughly constant until heat reaches the boundaries, then decreases.

<details><summary>Hint</summary>

- Two arrays `u` and `u_new`, swap pointers each step.
- The PGM is 32 identical rows; write the 256-byte row 32 times or fill a 8192-byte buffer once.
</details>
