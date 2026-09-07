# Chapter 08 — File I/O

## What you'll be able to do after this chapter

- Open, read, write, and close text and binary files with proper error handling (`perror`, `errno`).
- Read a file line by line robustly (`fgets` + `strtod`) and explain why `fscanf` and `feof` loops are traps.
- Load a CSV of floats into a growable `Matrix`, and write results back as CSV for plotting in Python.
- Read and write raw binary arrays and structs with `fread`/`fwrite`, seek within files, and slurp a whole file into memory.
- Parse the MNIST IDX format (big-endian header + raw bytes) with a byte-swap function, and explain endianness.
- Write PGM/PPM images — the frame output format for every simulation in this course.

## Why this matters for ML / numerics / sims

Data comes from files and results go to files. Your linear regression needs to read a CSV. Your MNIST classifier needs to read 47 MB of IDX bytes. Your N-body sim needs to dump a frame every step so you can `ffmpeg` them into a video. Your FDTD solver needs to write field snapshots you plot in matplotlib. Python hides all this behind `pd.read_csv` and `plt.imsave`. In C, the whole stack is `fopen`, `fread`, `fwrite`, `fclose` — a dozen functions — and once you know them you can read *any* format, including ones nobody wrote a library for.

---

## 1. `FILE *`, `fopen`, `fclose`

A `FILE` is an opaque struct (Chapter 07 §13) managed by the C library. It holds the OS file descriptor, a buffer, the current position, and error/EOF flags. You only ever hold a `FILE *`.

```c
#include <stdio.h>

FILE *f = fopen("data.txt", "r");    // returns NULL on failure
if (!f) {
    perror("fopen data.txt");        // prints: fopen data.txt: No such file or directory
    return 1;
}
/* ... use f ... */
if (fclose(f) != 0) perror("fclose"); // flushes buffers; can fail (disk full, NFS)
```

`perror(msg)` prints `msg: <description of errno>`. Always use it (or `strerror(errno)`) — "could not open file" without the reason wastes an hour of debugging when the real cause was a permissions problem.

### Modes

| Mode | Meaning | If file exists | If not |
|---|---|---|---|
| `"r"` | read text | open at start | fail (NULL) |
| `"w"` | write text | **truncate to empty** | create |
| `"a"` | append text | open at end | create |
| `"r+"` | read and write | open at start | fail |
| `"w+"` | read and write | truncate | create |
| `"rb"`, `"wb"`, `"ab"` | same, binary | | |

On macOS/Linux, `b` changes nothing — text and binary are the same bytes. On Windows it disables `\n` ↔ `\r\n` translation. Always write `"rb"`/`"wb"` for binary data anyway; it documents intent and keeps code portable.

**`"w"` destroys the file immediately on `fopen`, before you write anything.** Opening your only copy of a dataset with `"w"` by mistake is unrecoverable.

Every `fopen` must be paired with a `fclose`. Open files are a limited resource (~256 per process by default on macOS: `ulimit -n`), and unflushed data is lost if the process crashes before `fclose`.

**Python equivalent:** `with open(path, "r") as f:` — Python's `with` calls `close` for you. C does not; you write it, on every exit path.

---

## 2. `stdin`, `stdout`, `stderr` and redirection

Three `FILE *` are open when `main` starts:

| Stream | Purpose | Buffering | Shell redirect |
|---|---|---|---|
| `stdin` | input | line-buffered on terminal | `< in.txt` |
| `stdout` | normal output | line-buffered on terminal, fully buffered to a file/pipe | `> out.txt`, `\| next_prog` |
| `stderr` | errors, diagnostics | unbuffered | `2> err.txt` |

```
./prog < input.txt > output.txt      # read from file, write results to file
./prog < input.txt 2> errors.log     # errors go elsewhere
./prog | python3 plot.py             # pipe C output straight into Python
```

`printf(...)` is `fprintf(stdout, ...)`. Write errors and progress to `stderr` so they do not corrupt data you are piping through `stdout`. This is how you build small C tools that plug into Python: C writes CSV to `stdout`, Python reads `sys.stdin`.

---

## 3. Text output: `fprintf`

```c
FILE *f = fopen("out.csv", "w");
if (!f) { perror("fopen"); return 1; }
fprintf(f, "x,y\n");
for (int i = 0; i < 5; i++)
    fprintf(f, "%d,%.6f\n", i, sin(i * 0.5));
fclose(f);
```

```
x,y
0,0.000000
1,0.479426
2,0.841471
...
```

Use `%.17g` for doubles when you need exact round-trip precision (17 significant digits reproduces any `double` exactly); `%.6f` for human-readable output. `fprintf` returns the number of characters written or a negative value on error — check it when writing to disks that may fill.

---

## 4. Text input: `fgets` + `strtod` (robust) vs `fscanf` (fragile)

### The `fgets` line loop

```c
char line[4096];
while (fgets(line, sizeof line, f) != NULL) {
    // line includes the trailing '\n' if the line fit; strip it:
    line[strcspn(line, "\r\n")] = '\0';
    /* parse line */
}
```

`fgets(buf, n, f)` reads at most `n-1` chars, stops after a newline, NUL-terminates, and returns NULL at EOF or on error. If a line is longer than `n-1`, you get it in pieces — the pieces without `\n` at the end tell you that happened. `strcspn(line, "\r\n")` finds the first `\r` or `\n` and lets you strip Windows line endings too.

### Converting with `strtod`

```c
char *end;
double x = strtod(line, &end);
if (end == line) { /* no number found */ }
/* end now points just past the number; parse the next field from there */
```

`strtod` skips leading whitespace, parses as much as it can, and sets `end` to the first unparsed character. It never overflows a buffer, and it tells you *exactly* where it stopped. `strtol`/`strtoll` do the same for integers (with a base argument). `atof`/`atoi` give no error indication — do not use them.

### Why not `fscanf`?

```c
double x, y;
while (fscanf(f, "%lf,%lf", &x, &y) == 2) { ... }
```

This looks shorter but: (1) on a malformed line it stops consuming input and loops forever if you test `!= EOF` instead of `== 2`; (2) it silently skips newlines, so you cannot tell which line failed; (3) `%s` without a width limit is a buffer overflow; (4) mixing `fscanf` with `fgets` leaves the newline in the buffer and confuses the next `fgets`. `fscanf` is acceptable for simple, trusted, fixed-format input where you check the return count. For real data files, `fgets` + `strtod` gives you line numbers in error messages and never loops forever.

**Python equivalent:** `for line in f: fields = line.strip().split(",")` — `fgets` is the `for line in f`, `strtod` is `float(field)`.

---

## 5. Character I/O: `getc`, `putc`, and the `feof` trap

```c
int c;
while ((c = getc(f)) != EOF)     // c must be int, not char: EOF is -1, outside char's range
    putc(c, stdout);
```

`getc` returns an `int` that is either a byte (0–255) or `EOF` (-1). Storing it in a `char` breaks the comparison (byte 255 becomes -1 on signed-char platforms).

### The classic `feof` bug

```c
while (!feof(f)) {                // WRONG
    fgets(line, sizeof line, f);  // the last call fails, but line still holds the previous content
    process(line);                // ... which gets processed twice
}
```

`feof` returns true only *after* a read has hit EOF — not before. So the loop body runs one extra time with stale data. The correct pattern is always: **call the read function, check its return value**. `feof` is only for distinguishing "why did the read fail?" afterwards:

```c
while (fgets(line, sizeof line, f)) process(line);     // correct
if (ferror(f)) perror("read error");                    // failed for a reason other than EOF
```

---

## 6. Binary I/O: `fread` / `fwrite`

```c
size_t fwrite(const void *buf, size_t elem_size, size_t count, FILE *f);  // returns elements written
size_t fread(void *buf, size_t elem_size, size_t count, FILE *f);         // returns elements read
```

Write and read a raw array:

```c
double w[1000];
/* ... fill w ... */
FILE *f = fopen("weights.bin", "wb");
if (!f) { perror("fopen"); return 1; }
if (fwrite(w, sizeof w[0], 1000, f) != 1000) { perror("fwrite"); }
fclose(f);

f = fopen("weights.bin", "rb");
size_t got = fread(w, sizeof w[0], 1000, f);   // got < 1000 means short file or error
fclose(f);
```

8000 bytes written in one call — no formatting, no parsing, roughly 50x faster than text. A `Matrix` saves as `rows, cols, then rows*cols doubles`:

```c
/* Returns 0 on success. Format: uint64 rows, uint64 cols, then rows*cols doubles, native byte order. */
int mat_save(const Matrix *m, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint64_t hdr[2] = {m->rows, m->cols};
    int ok = fwrite(hdr, sizeof hdr[0], 2, f) == 2
          && fwrite(m->data, sizeof *m->data, m->rows * m->cols, f) == m->rows * m->cols;
    return (fclose(f) == 0 && ok) ? 0 : -1;
}
```

### Writing whole structs

`fwrite(&particle, sizeof particle, 1, f)` works — but the file now contains the struct's padding bytes and depends on your compiler's layout and your CPU's byte order. It reads back correctly on the *same* machine with the *same* compiler. For files that must be portable (shared with Python, another architecture), write each field explicitly in a defined byte order, or use a text format. For checkpoints on your own machine, raw structs are fine and fast.

**Python equivalent:** `np.fromfile("weights.bin", dtype=np.float64)` reads exactly what `fwrite` of a `double` array produced. `arr.tofile()` is the inverse. NumPy's `.npy` format is just a small header + this.

---

## 7. `fseek`, `ftell`, `rewind`

```c
fseek(f, 0, SEEK_END);        // move to end
long size = ftell(f);         // current position = file size in bytes (-1 on error)
rewind(f);                    // back to byte 0 (same as fseek(f, 0, SEEK_SET))
fseek(f, 16, SEEK_SET);       // absolute: byte 16
fseek(f, -8, SEEK_CUR);       // relative: back 8 bytes
```

Use this to skip a header, jump to record `k` in a fixed-record-size file (`fseek(f, k * sizeof(Record), SEEK_SET)`), or measure a file before allocating a buffer for it.

### Reading a whole file into memory

```c
/* Returns a NUL-terminated buffer with the file's contents (caller frees), sets *len.
 * Returns NULL on failure. */
char *read_entire_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(buf); return NULL; }
    buf[n] = '\0';
    *len = got;
    return buf;
}
```

This is how a tokenizer loads a corpus: one read, then scan the buffer with pointers. Note the error handling: every failure path closes the file and frees the buffer. Get in the habit — this is what "no leaks" looks like in I/O code.

---

## 8. `errno`, buffering, `fflush`

`errno` (from `<errno.h>`) is a global set by failing library calls. Read it *immediately* after a failure — the next call may overwrite it. `perror` and `strerror(errno)` turn it into text. Common values: `ENOENT` (no such file), `EACCES` (permission denied), `EISDIR` (is a directory), `ENOSPC` (disk full).

Output is **buffered**: `fprintf` copies into a memory buffer; the bytes reach the OS when the buffer fills, on `fclose`, on `fflush(f)`, or (for line-buffered terminals) at `\n`. Consequences:

- If your program crashes, buffered output that was never flushed is lost. `printf` debugging before a segfault may show nothing — add `fflush(stdout)` or print to `stderr` (unbuffered).
- Writing a frame per step to a file and watching it from another process: `fflush` after each frame.
- Performance: buffering is why `putc` in a loop is fast. Do not call `fflush` in an inner loop.

`setvbuf(f, NULL, _IONBF, 0)` disables buffering; `_IOFBF` with a large buffer speeds up huge writes.

---

## 9. `remove` and `rename`

```c
if (rename("out.tmp", "out.csv") != 0) perror("rename");   // atomic on the same filesystem
if (remove("scratch.bin") != 0)        perror("remove");
```

The write-to-temp-then-rename pattern makes file writes atomic: a reader never sees a half-written `out.csv`. Use it for checkpoints so a crash mid-write does not destroy the previous good checkpoint.

---

## 10. CSV parsing into a growable `Matrix`

Requirements: skip a header row, split on commas, convert each field with `strtod`, grow storage as rows arrive, fail loudly on malformed lines with the line number.

```c
/* Reads a numeric CSV with a header row into a row-major Matrix. Caller owns m.data.
 * Returns 0 on success, -1 on error (message on stderr). */
int csv_read(const char *path, Matrix *m) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }
    char line[8192];
    double *data = NULL; size_t len = 0, cap = 0, rows = 0, cols = 0;
    long lineno = 0;

    if (!fgets(line, sizeof line, f)) { fprintf(stderr, "%s: empty\n", path); fclose(f); return -1; }
    lineno++;                                               /* header consumed and ignored */

    while (fgets(line, sizeof line, f)) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;                      /* skip blank lines */
        size_t ncol = 0;
        char *p = line;
        for (;;) {
            char *end;
            double x = strtod(p, &end);
            if (end == p) { fprintf(stderr, "%s:%ld: bad number at '%s'\n", path, lineno, p); goto fail; }
            if (len == cap) {                               /* grow */
                size_t ncap = cap ? cap * 2 : 64;
                double *t = realloc(data, ncap * sizeof *t);
                if (!t) goto fail;
                data = t; cap = ncap;
            }
            data[len++] = x; ncol++;
            while (*end == ' ' || *end == '\t') end++;
            if (*end == ',') p = end + 1;
            else if (*end == '\0') break;
            else { fprintf(stderr, "%s:%ld: unexpected '%c'\n", path, lineno, *end); goto fail; }
        }
        if (cols == 0) cols = ncol;
        else if (ncol != cols) { fprintf(stderr, "%s:%ld: %zu fields, expected %zu\n", path, lineno, ncol, cols); goto fail; }
        rows++;
    }
    fclose(f);
    *m = (Matrix){rows, cols, data};
    return 0;
fail:
    fclose(f); free(data); return -1;
}
```

Two techniques to notice. First, splitting manually with `strtod`'s `end` pointer instead of `strtok`: `strtok` modifies the string, is not reentrant, and collapses consecutive delimiters (so `1,,3` looks like two fields). Manual splitting handles empty fields and tells you exactly where the problem is. Second, `goto fail` for cleanup: in C, a single cleanup label at the end of the function is the standard, readable way to release resources on every error path. It is one of the two legitimate uses of `goto` (the other is breaking out of nested loops).

**Python equivalent:** `np.loadtxt(path, delimiter=",", skiprows=1)`.

---

## 11. Writing CSV for Python plotting

```c
FILE *f = fopen("trajectory.csv", "w");
fprintf(f, "t,x,y,energy\n");
for (int step = 0; step < n; step++)
    fprintf(f, "%.6f,%.9g,%.9g,%.9g\n", step * dt, x[step], y[step], e[step]);
fclose(f);
```

Then in Python: `import pandas as pd; df = pd.read_csv("trajectory.csv"); df.plot(x="t", y="energy")`. This is the workflow for the whole course: compute in C, plot in Python. Print doubles with enough digits (`%.9g` or `%.17g`) or you will "discover" energy drift that is actually rounding in the output.

---

## 12. Endianness

A multi-byte integer like `0x00000803` (2051) is stored as 4 bytes. Which byte comes first?

```
value 0x00000803

little-endian (x86, Apple Silicon arm64):  [03][08][00][00]   least significant byte first
big-endian    (network order, MNIST IDX):  [00][00][08][03]   most significant byte first
```

Your Mac is little-endian. Files written by `fwrite(&n, 4, 1, f)` on your Mac are little-endian. Many file formats and all network protocols specify big-endian. Reading a big-endian `uint32_t` portably:

```c
#include <stdint.h>
static uint32_t read_be32(FILE *f) {          /* returns 0 on short read; check ferror/feof */
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}
static uint32_t bswap32(uint32_t x) {          /* swap byte order in place */
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}
```

Assembling from bytes with shifts (the first function) is correct on *any* host; you never need to know the host's endianness. `bswap32` is for when you already read the raw word.

You can detect the host at runtime: `uint16_t one = 1; int little = *(unsigned char *)&one == 1;`.

---

## 13. The MNIST IDX format

The MNIST files (`train-images-idx3-ubyte`, `train-labels-idx1-ubyte`) are:

```
images:  magic 0x00000803 (uint32 BE)  = "unsigned byte, 3 dimensions"
         n_images (uint32 BE)
         n_rows   (uint32 BE)   = 28
         n_cols   (uint32 BE)   = 28
         then n_images * 28 * 28 bytes, one pixel each, 0..255, row-major

labels:  magic 0x00000801 (uint32 BE)  = "unsigned byte, 1 dimension"
         n_labels (uint32 BE)
         then n_labels bytes, each 0..9
```

The magic's third byte encodes the data type (0x08 = unsigned byte) and the fourth the number of dimensions. A reader:

```c
/* Reads an IDX3 ubyte file. On success *out points to n*rows*cols bytes (caller frees). Returns 0. */
int idx3_read(const char *path, uint8_t **out, uint32_t *n, uint32_t *rows, uint32_t *cols) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }
    uint32_t magic = read_be32(f);
    if (magic != 0x00000803u) { fprintf(stderr, "%s: bad magic 0x%08x\n", path, magic); fclose(f); return -1; }
    *n = read_be32(f); *rows = read_be32(f); *cols = read_be32(f);
    size_t total = (size_t)*n * *rows * *cols;
    uint8_t *buf = malloc(total);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, total, f) != total) { fprintf(stderr, "%s: truncated\n", path); free(buf); fclose(f); return -1; }
    fclose(f);
    *out = buf;
    return 0;
}
```

Convert to `double` in `[0,1]` by dividing by 255 when you load into a `Matrix` (`n x 784`). The label file reader is the same with magic `0x801` and two header words. Note the files on the internet are gzipped; run `gunzip` first (or read `.gz` with zlib — later).

**Python equivalent:** `np.frombuffer(open(p,'rb').read(), dtype=np.uint8, offset=16).reshape(-1, 784)` — the `offset=16` skips the same four big-endian words.

---

## 14. Writing PGM / PPM images

Netpbm formats are the simplest image formats that exist: a tiny text header followed by raw bytes. No library needed. Every simulation in this course writes frames this way; `ffmpeg -i frame_%04d.ppm out.mp4` or Python's `PIL.Image.open` reads them.

```
PGM (grayscale, "P5"):          PPM (RGB, "P6"):
P5\n                            P6\n
<width> <height>\n              <width> <height>\n
255\n                           255\n
<width*height bytes>            <width*height*3 bytes, RGB RGB RGB ...>
```

```c
/* Writes an 8-bit grayscale image. pix is row-major, w*h bytes. Returns 0 on success. */
int pgm_write(const char *path, const uint8_t *pix, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    size_t n = (size_t)w * h;
    int ok = fwrite(pix, 1, n, f) == n;
    return (fclose(f) == 0 && ok) ? 0 : -1;
}

/* Writes an RGB image. rgb is w*h*3 bytes. */
int ppm_write(const char *path, const uint8_t *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    size_t n = (size_t)w * h * 3;
    int ok = fwrite(rgb, 1, n, f) == n;
    return (fclose(f) == 0 && ok) ? 0 : -1;
}
```

Mapping a `double` field (e.g. temperature, E_z, density) to a pixel: clamp to `[lo, hi]`, scale to `0..255`, cast to `uint8_t`. For a colormap, write a small function `(double t) -> (r,g,b)` — a two-segment blue→white→red is 10 lines. Frame numbering: `snprintf(name, sizeof name, "frame_%04d.ppm", step)`.

Note the mix: the header is *text* (`fprintf`), the payload is *binary* (`fwrite`) in the same file. That is normal for image formats.

---

## Gotchas and undefined behavior

- **`fopen(..., "w")` truncates immediately.** Double-check the mode before opening anything you cannot regenerate.
- **`while (!feof(f))`** processes the last record twice. Check the read call's return value instead.
- **`char c = getc(f)`** cannot represent `EOF` distinctly from byte 255. Use `int`.
- **`fscanf("%s")`** with no width is a buffer overflow. Use `%63s` for a 64-byte buffer, or `fgets`.
- **`fgets` leaves `\n` in the buffer** — strip it before `strtod` if trailing characters matter, and before printing.
- **Forgetting `fclose`** leaks the descriptor and may lose buffered output. Every error path must close.
- **`ftell` returns `long`** — on 32-bit systems it overflows past 2 GB. Fine on macOS arm64 (64-bit `long`).
- **`fread` returning fewer elements than requested** means EOF *or* error. Check `ferror(f)` to tell which.
- **`fwrite` of a struct** includes padding and host byte order. Fine for same-machine checkpoints, wrong for portable formats.
- **Reading a big-endian integer with a plain `fread` into `uint32_t`** gives a byte-swapped value on your Mac. Assemble from bytes with shifts.
- **Buffered output lost on crash**: `fflush` before the risky code, or write diagnostics to `stderr`.
- **`errno` is overwritten by the next library call** — read it immediately, e.g. `perror` right after the failure.
- **`strtok` is not reentrant and merges adjacent delimiters**; for CSV with possible empty fields, split manually.
- **`fseek` on a text stream with an arbitrary offset** is technically implementation-defined; on POSIX it works because text == binary.

---

## Common mistakes checklist

- [ ] Every `fopen` result checked for NULL with `perror` giving the filename.
- [ ] Every `fopen` has a `fclose` on every path including errors (`goto fail` pattern).
- [ ] Read loops test the return value of `fgets`/`fread`/`getc`, never `feof` as the loop condition.
- [ ] Text numbers parsed with `strtod`/`strtol` and the `end` pointer checked; no `atof`/`atoi`.
- [ ] Binary files opened with `"rb"`/`"wb"`.
- [ ] `fread`/`fwrite` return values compared to the requested count.
- [ ] Multi-byte integers in external formats read byte-by-byte with shifts.
- [ ] Line numbers included in parse error messages.
- [ ] Doubles written with enough precision (`%.9g` or `%.17g`) when Python will consume them.
- [ ] Errors go to `stderr`, data goes to `stdout` or a named file.

---

## You can move on when...

- You can write a `fgets` loop that reads a CSV of doubles into a growable `Matrix`, reporting the line number of any bad field.
- You can explain what `while (!feof(f))` does wrong and rewrite it.
- You can save a `Matrix` to a binary file and load it back, and describe what would break if the file moved to a big-endian machine.
- You can write `read_be32` from memory and say why shifts-and-ORs are host-independent.
- You can write a PGM frame of a 2D `double` array with a clamp-and-scale mapping.
- You can read a whole file into a heap buffer with every error path freeing and closing correctly.
- You can state what `stderr` is for and why buffering can hide a crash's last `printf`.

Next: `../09_multi_file_projects_and_make/lesson.md` — splitting `matrix.c`, `csv.c`, and `image.c` into a real project with a Makefile.
