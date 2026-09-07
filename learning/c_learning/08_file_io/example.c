/*
 * Chapter 08 — File I/O: worked example
 *
 * Compile + run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * Writes a handful of scratch files named ex_demo_* in the current directory
 * and removes them at the end (except ex_demo_frame.pgm and ex_demo_map.ppm,
 * which are kept so you can open them in Preview; delete them when done).
 *
 * Demonstrates:
 *   1. fopen modes, NULL check + perror, fclose
 *   2. fprintf text output (CSV)
 *   3. fgets + strtod line reading into a growable Matrix
 *   4. the feof trap (shown safely) vs checking the return value
 *   5. fread/fwrite of a raw array and of a struct array
 *   6. fseek / ftell / rewind, reading a whole file into memory
 *   7. endianness: read_be32 / bswap32; a fake MNIST IDX file written and read back
 *   8. PGM / PPM image output
 *   9. errno, fflush, stderr, rename, remove
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

/* ------------------------------------------------------------ Matrix (from ch07) */
typedef struct { size_t rows, cols; double *data; } Matrix;
static void mat_free(Matrix *m) { free(m->data); m->data = NULL; m->rows = m->cols = 0; }

/* ------------------------------------------------------------ 2. write CSV */
/* Returns 0 on success, -1 on error. */
static int csv_write_demo(const char *path) {
    FILE *f = fopen(path, "w");                 /* "w": truncates or creates */
    if (!f) { perror(path); return -1; }
    fprintf(f, "t,sin,cos\n");                  /* header row */
    for (int i = 0; i < 6; i++)
        fprintf(f, "%.2f,%.9g,%.9g\n", i * 0.5, sin(i * 0.5), cos(i * 0.5));   /* %.9g: enough digits for Python */
    return fclose(f) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------ 3. read CSV robustly */
/* Reads a numeric CSV with one header row into a row-major Matrix.
 * Caller owns m->data (mat_free). Returns 0 on success, -1 on error (message on stderr). */
static int csv_read(const char *path, Matrix *m) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }
    char line[8192];
    double *data = NULL;
    size_t len = 0, cap = 0, rows = 0, cols = 0;
    long lineno = 0;

    if (!fgets(line, sizeof line, f)) { fprintf(stderr, "%s: empty file\n", path); fclose(f); return -1; }
    lineno++;                                                   /* header consumed */

    while (fgets(line, sizeof line, f)) {                       /* loop on the RETURN VALUE, not feof */
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';                     /* strip newline (and \r) */
        if (line[0] == '\0') continue;
        size_t ncol = 0;
        char *p = line;
        for (;;) {
            char *end;
            double x = strtod(p, &end);                         /* end tells us where parsing stopped */
            if (end == p) { fprintf(stderr, "%s:%ld: expected number at '%s'\n", path, lineno, p); goto fail; }
            if (len == cap) {
                size_t ncap = cap ? cap * 2 : 64;
                double *t = realloc(data, ncap * sizeof *t);
                if (!t) goto fail;
                data = t; cap = ncap;
            }
            data[len++] = x;
            ncol++;
            while (*end == ' ' || *end == '\t') end++;
            if (*end == ',')       p = end + 1;
            else if (*end == '\0') break;
            else { fprintf(stderr, "%s:%ld: unexpected character '%c'\n", path, lineno, *end); goto fail; }
        }
        if (cols == 0) cols = ncol;
        else if (ncol != cols) { fprintf(stderr, "%s:%ld: %zu fields, expected %zu\n", path, lineno, ncol, cols); goto fail; }
        rows++;
    }
    if (ferror(f)) { perror(path); goto fail; }
    fclose(f);
    *m = (Matrix){rows, cols, data};
    return 0;
fail:                                                            /* single cleanup point */
    fclose(f);
    free(data);
    return -1;
}

/* ------------------------------------------------------------ 5. binary struct records */
typedef struct { uint32_t id; float x, y; uint8_t flag; } Rec;  /* sizeof == 16: 3 bytes tail padding */

/* ------------------------------------------------------------ 6. whole file into memory */
/* Returns a NUL-terminated heap buffer with the file's bytes; *len = byte count.
 * Caller frees. Returns NULL on failure. Works for binary files (embedded NULs). */
static char *read_entire_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { perror("fseek"); fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { perror("ftell"); fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { fprintf(stderr, "%s: short read\n", path); free(buf); return NULL; }
    buf[n] = '\0';
    *len = got;
    return buf;
}

/* ------------------------------------------------------------ 7. endianness */
static uint32_t bswap32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0x0000ff00u) | ((x << 8) & 0x00ff0000u) | (x << 24);
}
/* Assembles a big-endian uint32 from 4 bytes. Host-independent. Returns 0 on short read. */
static uint32_t read_be32(FILE *f) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}
static int write_be32(FILE *f, uint32_t v) {
    unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v };
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

/* Reads an IDX3 unsigned-byte file (MNIST images). On success *out holds n*rows*cols
 * bytes (caller frees). Returns 0, or -1 with a message on stderr. */
static int idx3_read(const char *path, uint8_t **out, uint32_t *n, uint32_t *rows, uint32_t *cols) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }
    uint32_t magic = read_be32(f);
    if (magic != 0x00000803u) { fprintf(stderr, "%s: bad magic 0x%08x (expected 0x00000803)\n", path, magic); fclose(f); return -1; }
    *n = read_be32(f); *rows = read_be32(f); *cols = read_be32(f);
    size_t total = (size_t)*n * *rows * *cols;
    uint8_t *buf = malloc(total);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, total, f) != total) { fprintf(stderr, "%s: truncated\n", path); free(buf); fclose(f); return -1; }
    fclose(f);
    *out = buf;
    return 0;
}

/* ------------------------------------------------------------ 8. images */
/* 8-bit grayscale, row-major w*h bytes. Returns 0 on success. */
static int pgm_write(const char *path, const uint8_t *pix, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    fprintf(f, "P5\n%d %d\n255\n", w, h);             /* text header ... */
    size_t n = (size_t)w * h;
    int ok = fwrite(pix, 1, n, f) == n;                /* ... binary payload */
    return (fclose(f) == 0 && ok) ? 0 : -1;
}
/* RGB, w*h*3 bytes. */
static int ppm_write(const char *path, const uint8_t *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    size_t n = (size_t)w * h * 3;
    int ok = fwrite(rgb, 1, n, f) == n;
    return (fclose(f) == 0 && ok) ? 0 : -1;
}
/* blue -> white -> red colormap for t in [0,1] */
static void colormap(double t, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    if (t < 0.5) { double s = t * 2;       *r = (uint8_t)(255 * s); *g = (uint8_t)(255 * s); *b = 255; }
    else         { double s = (t - 0.5) * 2; *r = 255; *g = (uint8_t)(255 * (1 - s)); *b = (uint8_t)(255 * (1 - s)); }
}

/* ============================================================ main */
int main(void) {
    printf("=== 1. fopen failure + perror + errno ===\n");
    FILE *bad = fopen("/nonexistent_dir/file.txt", "r");
    if (!bad) {
        int saved = errno;                                  /* save immediately; later calls may overwrite */
        perror("fopen /nonexistent_dir/file.txt");           /* to stderr */
        printf("errno was %d (%s)\n", saved, strerror(saved));
    }

    printf("\n=== 2-3. write CSV, read it back into a Matrix ===\n");
    if (csv_write_demo("ex_demo_data.csv") != 0) return 1;
    Matrix m;
    if (csv_read("ex_demo_data.csv", &m) != 0) return 1;
    printf("read %zux%zu matrix; row 2 = %.2f %.9g %.9g\n", m.rows, m.cols,
           m.data[2 * m.cols + 0], m.data[2 * m.cols + 1], m.data[2 * m.cols + 2]);
    mat_free(&m);

    /* a malformed file: the error names the line */
    FILE *f = fopen("ex_demo_bad.csv", "w");
    if (!f) { perror("ex_demo_bad.csv"); return 1; }
    fputs("a,b\n1,2\n3,oops\n", f);
    fclose(f);
    printf("parsing a broken CSV (expect an error on stderr naming line 3):\n");
    fflush(stdout);                                          /* keep stdout/stderr ordering sane */
    if (csv_read("ex_demo_bad.csv", &m) == 0) { printf("unexpected success\n"); mat_free(&m); }

    printf("\n=== 4. the feof trap ===\n");
    f = fopen("ex_demo_nums.txt", "w");
    if (!f) return 1;
    fputs("10\n20\n30\n", f);
    fclose(f);
    f = fopen("ex_demo_nums.txt", "r");
    if (!f) return 1;
    int wrong = 0; double x = 0;
    while (!feof(f)) {                 /* WRONG: feof is true only AFTER a read fails */
        if (fscanf(f, "%lf", &x) != 1) { /* ...so the loop runs once more; without this guard we'd count 4 */ }
        wrong++;
    }
    rewind(f);
    int right = 0;
    while (fscanf(f, "%lf", &x) == 1) right++;     /* RIGHT: test the read's return value */
    fclose(f);
    printf("3 numbers in file. feof-loop iterations: %d   return-value loop: %d\n", wrong, right);

    printf("\n=== 5. binary: raw array and struct records ===\n");
    double w[8];
    for (int i = 0; i < 8; i++) w[i] = i * 1.5;
    f = fopen("ex_demo_weights.bin", "wb");
    if (!f) return 1;
    if (fwrite(w, sizeof w[0], 8, f) != 8) perror("fwrite");
    fclose(f);
    double w2[8] = {0};
    f = fopen("ex_demo_weights.bin", "rb");
    if (!f) return 1;
    size_t got = fread(w2, sizeof w2[0], 8, f);
    fclose(f);
    printf("wrote 8 doubles (64 bytes), read back %zu: w2[7] = %.1f  (np.fromfile(..., dtype=np.float64) reads this)\n", got, w2[7]);

    Rec recs[4];
    for (uint32_t i = 0; i < 4; i++) recs[i] = (Rec){i, i * 0.5f, -(float)i, (uint8_t)(i & 1)};
    f = fopen("ex_demo_recs.bin", "wb");
    if (!f) return 1;
    fwrite(recs, sizeof recs[0], 4, f);
    fclose(f);
    printf("sizeof(Rec)=%zu (13 bytes of fields + 3 padding, all written to disk)\n", sizeof(Rec));

    printf("\n=== 6. fseek / ftell / rewind, read_entire_file ===\n");
    f = fopen("ex_demo_recs.bin", "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 2 * (long)sizeof(Rec), SEEK_SET);              /* jump straight to record 2 */
    Rec r2;
    if (fread(&r2, sizeof r2, 1, f) != 1) { perror("fread"); fclose(f); return 1; }
    fclose(f);
    printf("file size %ld = 4 * %zu; record 2 = {id=%u x=%.1f y=%.1f flag=%u}\n", size, sizeof(Rec), r2.id, r2.x, r2.y, r2.flag);

    size_t len;
    char *text = read_entire_file("ex_demo_data.csv", &len);
    if (!text) return 1;
    int lines = 0;
    for (size_t i = 0; i < len; i++) if (text[i] == '\n') lines++;
    printf("read_entire_file: %zu bytes, %d lines; first line: \"%.*s\"\n", len, lines, (int)strcspn(text, "\n"), text);
    free(text);

    printf("\n=== 7. endianness and a fake MNIST IDX file ===\n");
    uint16_t one = 1;
    printf("host is %s-endian\n", *(unsigned char *)&one == 1 ? "little" : "big");
    printf("bswap32(0x00000803) = 0x%08x\n", bswap32(0x00000803u));
    /* write a tiny IDX3 file: 3 images of 4x4 */
    f = fopen("ex_demo_fake.idx3", "wb");
    if (!f) return 1;
    write_be32(f, 0x00000803u); write_be32(f, 3); write_be32(f, 4); write_be32(f, 4);
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < 16; i++) { unsigned char px = (unsigned char)(k * 80 + i * 5); fwrite(&px, 1, 1, f); }
    fclose(f);
    /* dump the header bytes to show the big-endian order */
    f = fopen("ex_demo_fake.idx3", "rb");
    if (!f) return 1;
    printf("first 8 raw bytes: ");
    for (int i = 0; i < 8; i++) printf("%02x ", getc(f));
    printf(" <- 00 00 08 03 = magic, 00 00 00 03 = n\n");
    fclose(f);
    uint8_t *imgs; uint32_t n, rows, cols;
    if (idx3_read("ex_demo_fake.idx3", &imgs, &n, &rows, &cols) != 0) return 1;
    printf("idx3_read: n=%u rows=%u cols=%u; image 1 pixel (0,3) = %u\n", n, rows, cols, imgs[1 * 16 + 3]);
    free(imgs);

    printf("\n=== 8. PGM and PPM images ===\n");
    enum { W = 256, H = 64 };
    uint8_t *gray = malloc((size_t)W * H);
    uint8_t *rgb  = malloc((size_t)W * H * 3);
    if (!gray || !rgb) { free(gray); free(rgb); return 1; }
    for (int y = 0; y < H; y++)
        for (int xx = 0; xx < W; xx++) {
            /* a 2D "field": a Gaussian bump, mapped to 0..255 */
            double dx = (xx - W / 2) / 40.0, dy = (y - H / 2) / 15.0;
            double v = exp(-(dx * dx + dy * dy));
            gray[y * W + xx] = (uint8_t)(255 * v);
            colormap(v, &rgb[(y * W + xx) * 3], &rgb[(y * W + xx) * 3 + 1], &rgb[(y * W + xx) * 3 + 2]);
        }
    if (pgm_write("ex_demo_frame.pgm", gray, W, H) != 0) return 1;
    if (ppm_write("ex_demo_map.ppm", rgb, W, H) != 0) return 1;
    free(gray); free(rgb);
    printf("wrote ex_demo_frame.pgm (P5) and ex_demo_map.ppm (P6): open them in Preview\n");

    printf("\n=== 9. rename, remove, fflush ===\n");
    f = fopen("ex_demo_out.tmp", "w");
    if (!f) return 1;
    fprintf(f, "checkpoint data\n");
    fflush(f);                                               /* force bytes to the OS now */
    fclose(f);
    if (rename("ex_demo_out.tmp", "ex_demo_out.txt") != 0) perror("rename");
    else printf("rename ex_demo_out.tmp -> ex_demo_out.txt (atomic: readers never see a half-written file)\n");

    const char *scratch[] = { "ex_demo_data.csv", "ex_demo_bad.csv", "ex_demo_nums.txt", "ex_demo_weights.bin",
                              "ex_demo_recs.bin", "ex_demo_fake.idx3", "ex_demo_out.txt" };
    for (size_t i = 0; i < sizeof scratch / sizeof scratch[0]; i++)
        if (remove(scratch[i]) != 0) perror(scratch[i]);
    printf("removed %zu scratch files; kept the two images.\n", sizeof scratch / sizeof scratch[0]);
    fprintf(stderr, "(this line went to stderr: try ./ex_demo 2>/dev/null)\n");
    return 0;
}
