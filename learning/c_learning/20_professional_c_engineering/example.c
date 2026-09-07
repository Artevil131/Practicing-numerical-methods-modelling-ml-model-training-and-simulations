/*
 * Chapter 20 — Professional C Engineering
 *
 * ONE file, THREE builds:
 *
 * 1. Standalone demo + test suite (the default):
 *      cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *      MAT_LOG=debug ./ex_demo                    (logging level from the environment)
 *
 * 2. Shared library for FFI (Python ctypes/cffi, C++, anything):
 *    macOS:
 *      cc -Wall -Wextra -std=c11 -O2 -fvisibility=hidden -DMATRIX_NO_MAIN -dynamiclib \
 *         -install_name @rpath/libmatrix.dylib -o libmatrix.dylib example.c -lm
 *      nm -gU libmatrix.dylib                     (only the mat_* symbols are exported)
 *      otool -L libmatrix.dylib                   (what it links against)
 *    Linux:
 *      cc -Wall -Wextra -std=c11 -O2 -fPIC -fvisibility=hidden -DMATRIX_NO_MAIN -shared \
 *         -Wl,-soname,libmatrix.so.1 -o libmatrix.so.1.0.0 example.c -lm && ln -sf libmatrix.so.1.0.0 libmatrix.so
 *      nm -D --defined-only libmatrix.so ; ldd libmatrix.so
 *    Static library (either OS):
 *      cc -Wall -Wextra -std=c11 -O2 -DMATRIX_NO_MAIN -c -o matrix.o example.c && ar rcs libmatrix.a matrix.o
 *    Then from Python (see lesson §5 for the full script):
 *      python3 -c "import ctypes; L = ctypes.CDLL('./libmatrix.dylib'); L.mat_version_string.restype = ctypes.c_char_p; print(L.mat_version_string())"
 *
 * 3. libFuzzer target for the CSV row parser (needs Homebrew LLVM; Apple's clang lacks libFuzzer):
 *      $(brew --prefix llvm)/bin/clang -g -O1 -fsanitize=fuzzer,address,undefined -DMATRIX_FUZZ \
 *         -o fuzz_csv example.c -lm && mkdir -p corpus && ./fuzz_csv corpus -max_total_time=30
 *    Linux: clang -fsanitize=fuzzer,address ... (same flags; libFuzzer ships with clang)
 *
 * What this file demonstrates:
 *   - API design: opaque handle (mat_t), status codes + mat_strerror, const-correctness, ownership
 *     documented per function, versioning (semver + ABI check), symbol visibility (MAT_API),
 *     extern "C" guards so C++ can include the same header
 *   - a header section that would be matrix.h (kept inline so the example is one file)
 *   - logging with levels, configured from the environment (MAT_LOG=error|warn|info|debug)
 *   - Doxygen comments on the public API
 *   - a hand-rolled test framework: unit tests, a property test with a seeded RNG, a golden value
 *   - a fuzz target (LLVMFuzzerTestOneInput) for the parser, also exercised by the unit tests
 *   - zero-copy interop: mat_matmul_raw takes plain pointers, exactly what NumPy hands over
 */

/* ========================================================================================== */
/*                                   matrix.h  (public header)                                 */
/* ========================================================================================== */
#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {                        /* C++ callers get C linkage: no name mangling */
#endif

/* Symbol visibility: with -fvisibility=hidden everything is hidden unless marked MAT_API.      */
/* Users of the header (not the library build) get plain declarations.                          */
#if defined(_WIN32)
#  if defined(MATRIX_BUILD)
#    define MAT_API __declspec(dllexport)
#  else
#    define MAT_API __declspec(dllimport)
#  endif
#else
#  define MAT_API __attribute__((visibility("default")))
#endif

/** Semantic version of the library: bump MAJOR on ABI breaks, MINOR on additions, PATCH on fixes. */
#define MAT_VERSION_MAJOR 1
#define MAT_VERSION_MINOR 2
#define MAT_VERSION_PATCH 0
#define MAT_VERSION ((MAT_VERSION_MAJOR << 16) | (MAT_VERSION_MINOR << 8) | MAT_VERSION_PATCH)

/** Status codes. 0 is success; everything else is an error. Never return -1/errno-style ints. */
typedef enum mat_status {
    MAT_OK = 0,
    MAT_ERR_NOMEM,      /**< allocation failed */
    MAT_ERR_SHAPE,      /**< dimensions do not match */
    MAT_ERR_ARG,        /**< NULL pointer or invalid argument */
    MAT_ERR_ALIAS,      /**< output aliases an input where forbidden */
    MAT_ERR_PARSE,      /**< malformed text */
    MAT_ERR_OVERFLOW,   /**< buffer too small */
    MAT_STATUS_COUNT_   /**< sentinel, not a status */
} mat_status;

/** Opaque matrix handle. Layout is private: users cannot depend on it, so we can change it. */
typedef struct mat mat_t;

/** Log levels, lowest = most severe. */
typedef enum mat_log_level { MAT_LOG_ERROR = 0, MAT_LOG_WARN, MAT_LOG_INFO, MAT_LOG_DEBUG } mat_log_level;

/** Log sink callback. `user` is the pointer registered with mat_set_log_fn. */
typedef void (*mat_log_fn)(mat_log_level level, const char *msg, void *user);

/* ---- versioning ------------------------------------------------------------------------- */
/** @return packed runtime version (compare with MAT_VERSION compiled into the caller). */
MAT_API unsigned    mat_version(void);
/** @return "MAJOR.MINOR.PATCH" as a static string; never free it. */
MAT_API const char *mat_version_string(void);
/** @return 1 if the running library is ABI-compatible with a caller compiled against `compiled`. */
MAT_API int         mat_abi_compatible(unsigned compiled);

/* ---- errors ----------------------------------------------------------------------------- */
/** @return static human-readable text for a status; never NULL, never free it. */
MAT_API const char *mat_strerror(mat_status s);

/* ---- logging ---------------------------------------------------------------------------- */
/** Set the maximum level that is emitted. Default: MAT_LOG_WARN, or $MAT_LOG if set. */
MAT_API void mat_set_log_level(mat_log_level level);
/** Redirect log output. NULL restores the default (stderr). Not thread-safe with concurrent logging. */
MAT_API void mat_set_log_fn(mat_log_fn fn, void *user);

/* ---- lifetime --------------------------------------------------------------------------- */
/**
 * Allocate a rows x cols matrix of doubles, zero-filled.
 * @return new matrix owned by the caller (release with mat_destroy), or NULL on failure.
 */
MAT_API mat_t *mat_create(size_t rows, size_t cols);
/**
 * Wrap an existing buffer WITHOUT copying. The caller keeps ownership of `data` and must keep
 * it alive for as long as the mat_t is used; mat_destroy on a wrapped matrix does not free `data`.
 * `data` must hold rows*cols doubles, row-major, contiguous (NumPy: `np.ascontiguousarray`).
 */
MAT_API mat_t *mat_wrap(double *data, size_t rows, size_t cols);
/** Release a matrix (and its data if owned). NULL is allowed and does nothing. */
MAT_API void   mat_destroy(mat_t *m);

/* ---- accessors: const in, const out --------------------------------------------------- */
MAT_API size_t        mat_rows(const mat_t *m);
MAT_API size_t        mat_cols(const mat_t *m);
/** @return pointer to the row-major data; valid until the matrix is destroyed. */
MAT_API double       *mat_data(mat_t *m);
MAT_API const double *mat_data_const(const mat_t *m);

/* ---- operations: outputs are caller-allocated, status is returned, nothing is printed ---- */
/**
 * out = a * b. Requires a->cols == b->rows, out is a->rows x b->cols, out must not alias a or b.
 * @return MAT_OK, MAT_ERR_ARG (NULL), MAT_ERR_SHAPE, or MAT_ERR_ALIAS.
 */
MAT_API mat_status mat_matmul(const mat_t *a, const mat_t *b, mat_t *out);
/**
 * Raw-pointer version for FFI: c[m x n] = a[m x k] * b[k x n], all row-major contiguous doubles.
 * This is the signature Python's ctypes/cffi call; no handles cross the boundary.
 */
MAT_API mat_status mat_matmul_raw(const double *a, const double *b, double *c, size_t m, size_t k, size_t n);
/** Frobenius norm. Returns NaN for NULL (documented!) rather than aborting. */
MAT_API double     mat_frobenius(const mat_t *m);

/* ---- parsing ---------------------------------------------------------------------------- */
/**
 * Parse one CSV line of decimal numbers ("1.5, -2,3e4") into out[0..cap). Whitespace around
 * fields is ignored; an empty line yields 0 values. Stops at '\n' or '\0'.
 * @param count receives the number of values parsed (also on error: how far it got).
 * @return MAT_OK, MAT_ERR_ARG, MAT_ERR_PARSE (bad token), MAT_ERR_OVERFLOW (more than cap values).
 */
MAT_API mat_status mat_parse_csv_row(const char *line, double *out, size_t cap, size_t *count);

#ifdef __cplusplus
}
#endif
#endif /* MATRIX_H */

/* ========================================================================================== */
/*                                   matrix.c  (implementation)                                */
/* ========================================================================================== */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The private struct. Nothing outside this file knows its layout. */
struct mat {
    size_t rows, cols;
    double *data;
    bool owns_data;
};

/* --- logging ------------------------------------------------------------------------------ */
static mat_log_level g_log_level = MAT_LOG_WARN;
static bool          g_log_level_from_env_checked = false;
static mat_log_fn    g_log_fn = NULL;
static void         *g_log_user = NULL;

static void log_default(mat_log_level level, const char *msg, void *user) {
    (void)user;
    static const char *names[] = { "ERROR", "WARN", "INFO", "DEBUG" };
    fprintf(stderr, "[matrix %s] %s\n", names[level], msg);
}

static void log_init_from_env(void) {
    if (g_log_level_from_env_checked) return;
    g_log_level_from_env_checked = true;
    const char *e = getenv("MAT_LOG");             /* configuration via environment: 12-factor style */
    if (!e) return;
    if      (!strcmp(e, "error")) g_log_level = MAT_LOG_ERROR;
    else if (!strcmp(e, "warn"))  g_log_level = MAT_LOG_WARN;
    else if (!strcmp(e, "info"))  g_log_level = MAT_LOG_INFO;
    else if (!strcmp(e, "debug")) g_log_level = MAT_LOG_DEBUG;
}

/* printf-style logging, compiled to a cheap level check when disabled */
__attribute__((format(printf, 2, 3)))
static void mat_log(mat_log_level level, const char *fmt, ...) {
    log_init_from_env();
    if (level > g_log_level) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    (g_log_fn ? g_log_fn : log_default)(level, buf, g_log_user);
}
#define LOG_ERROR(...) mat_log(MAT_LOG_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  mat_log(MAT_LOG_WARN,  __VA_ARGS__)
#define LOG_INFO(...)  mat_log(MAT_LOG_INFO,  __VA_ARGS__)
#define LOG_DEBUG(...) mat_log(MAT_LOG_DEBUG, __VA_ARGS__)

void mat_set_log_level(mat_log_level level) { g_log_level_from_env_checked = true; g_log_level = level; }
void mat_set_log_fn(mat_log_fn fn, void *user) { g_log_fn = fn; g_log_user = user; }

/* --- version / errors --------------------------------------------------------------------- */
unsigned    mat_version(void)        { return MAT_VERSION; }
const char *mat_version_string(void) { return "1.2.0"; }
int mat_abi_compatible(unsigned compiled) {
    /* same major, and the library is at least as new as what the caller compiled against */
    return (compiled >> 16) == MAT_VERSION_MAJOR && compiled <= MAT_VERSION;
}

const char *mat_strerror(mat_status s) {
    static const char *const msgs[MAT_STATUS_COUNT_] = {
        [MAT_OK]           = "success",
        [MAT_ERR_NOMEM]    = "out of memory",
        [MAT_ERR_SHAPE]    = "shape mismatch",
        [MAT_ERR_ARG]      = "invalid argument",
        [MAT_ERR_ALIAS]    = "output aliases an input",
        [MAT_ERR_PARSE]    = "parse error",
        [MAT_ERR_OVERFLOW] = "buffer too small",
    };
    if ((unsigned)s >= MAT_STATUS_COUNT_) return "unknown status";
    return msgs[s];
}

/* --- lifetime ----------------------------------------------------------------------------- */
mat_t *mat_create(size_t rows, size_t cols) {
    if (rows == 0 || cols == 0 || rows > SIZE_MAX / cols / sizeof(double)) {   /* overflow check */
        LOG_ERROR("mat_create(%zu, %zu): invalid shape", rows, cols);
        return NULL;
    }
    mat_t *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->data = calloc(rows * cols, sizeof(double));
    if (!m->data) { free(m); LOG_ERROR("mat_create: out of memory for %zux%zu", rows, cols); return NULL; }
    m->rows = rows; m->cols = cols; m->owns_data = true;
    LOG_DEBUG("mat_create %zux%zu -> %p", rows, cols, (void *)m);
    return m;
}

mat_t *mat_wrap(double *data, size_t rows, size_t cols) {
    if (!data || rows == 0 || cols == 0) return NULL;
    mat_t *m = malloc(sizeof *m);
    if (!m) return NULL;
    *m = (mat_t){ .rows = rows, .cols = cols, .data = data, .owns_data = false };
    LOG_DEBUG("mat_wrap %zux%zu around %p (borrowed)", rows, cols, (void *)data);
    return m;
}

void mat_destroy(mat_t *m) {
    if (!m) return;
    if (m->owns_data) free(m->data);
    free(m);
}

size_t        mat_rows(const mat_t *m)       { return m ? m->rows : 0; }
size_t        mat_cols(const mat_t *m)       { return m ? m->cols : 0; }
double       *mat_data(mat_t *m)             { return m ? m->data : NULL; }
const double *mat_data_const(const mat_t *m) { return m ? m->data : NULL; }

/* --- operations --------------------------------------------------------------------------- */
mat_status mat_matmul_raw(const double *a, const double *b, double *c, size_t m, size_t k, size_t n) {
    if (!a || !b || !c) return MAT_ERR_ARG;
    if (c == a || c == b) return MAT_ERR_ALIAS;
    /* ikj order (chapter 13/19): contiguous inner loop */
    for (size_t i = 0; i < m; i++) {
        double *ci = c + i * n;
        for (size_t j = 0; j < n; j++) ci[j] = 0.0;
        for (size_t p = 0; p < k; p++) {
            const double aip = a[i * k + p];
            const double *bp = b + p * n;
            for (size_t j = 0; j < n; j++) ci[j] += aip * bp[j];
        }
    }
    return MAT_OK;
}

mat_status mat_matmul(const mat_t *a, const mat_t *b, mat_t *out) {
    if (!a || !b || !out) return MAT_ERR_ARG;
    if (a->cols != b->rows || out->rows != a->rows || out->cols != b->cols) {
        LOG_WARN("mat_matmul: shape mismatch (%zux%zu)*(%zux%zu)->(%zux%zu)",
                 a->rows, a->cols, b->rows, b->cols, out->rows, out->cols);
        return MAT_ERR_SHAPE;
    }
    if (out == a || out == b || out->data == a->data || out->data == b->data) return MAT_ERR_ALIAS;
    return mat_matmul_raw(a->data, b->data, out->data, a->rows, a->cols, b->cols);
}

double mat_frobenius(const mat_t *m) {
    if (!m) return NAN;
    double s = 0;
    for (size_t i = 0; i < m->rows * m->cols; i++) s += m->data[i] * m->data[i];
    return sqrt(s);
}

/* --- parsing ------------------------------------------------------------------------------ */
mat_status mat_parse_csv_row(const char *line, double *out, size_t cap, size_t *count) {
    if (!line || (!out && cap) || !count) return MAT_ERR_ARG;
    *count = 0;
    const char *p = line;
    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') {
            /* an empty line has 0 fields; "1," (trailing comma) has an empty 2nd field -> parse error */
            if (*count > 0 && p > line && p[-1] == ',') return MAT_ERR_PARSE;
            return MAT_OK;
        }
        char *end;
        errno = 0;
        double v = strtod(p, &end);
        if (end == p) return MAT_ERR_PARSE;                      /* no digits at all */
        if (errno == ERANGE) LOG_DEBUG("csv: value out of range at column %zu", *count);
        p = end;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != ',' && *p != '\0' && *p != '\n' && *p != '\r') return MAT_ERR_PARSE;   /* junk after a number */
        if (*count == cap) return MAT_ERR_OVERFLOW;
        out[(*count)++] = v;
        if (*p == ',') p++;
    }
}

/* ========================================================================================== */
/*                          fuzz target (build 3): -DMATRIX_FUZZ                               */
/* ========================================================================================== */
#ifdef MATRIX_FUZZ
/* libFuzzer calls this millions of times with mutated inputs; ASan/UBSan catch any memory error.
 * Rules: deterministic, no global state, never crash on ANY input, return 0.                  */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *line = malloc(size + 1);              /* the parser wants a NUL-terminated string */
    if (!line) return 0;
    memcpy(line, data, size);
    line[size] = '\0';
    double vals[16];
    size_t n = 0;
    mat_status s = mat_parse_csv_row(line, vals, 16, &n);
    assert(n <= 16);                            /* the contract; a violation is a real bug found */
    assert(s != MAT_OK || n == 0 || !isnan(vals[0]) || strstr(line, "nan") || strstr(line, "NAN") || strstr(line, "NaN"));
    free(line);
    return 0;
}
#endif

/* ========================================================================================== */
/*                     standalone demo + tests (builds 1): default                              */
/* ========================================================================================== */
#if !defined(MATRIX_NO_MAIN) && !defined(MATRIX_FUZZ)

/* --- a 40-line test framework (chapter 13), with per-test names and a summary -------------- */
static int g_tests_run = 0, g_tests_failed = 0, g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_tests_failed++; \
    fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); return; } } while (0)
#define CHECK_EQ_STATUS(expr, want) do { mat_status _s = (expr); g_checks++; if (_s != (want)) { g_tests_failed++; \
    fprintf(stderr, "  FAIL %s:%d: %s -> %s, want %s\n", __FILE__, __LINE__, #expr, mat_strerror(_s), mat_strerror(want)); return; } } while (0)
#define CHECK_NEAR(a, b, tol) CHECK(fabs((a) - (b)) <= (tol))
#define RUN(test) do { g_tests_run++; printf("  %-40s", #test); int _before = g_tests_failed; test(); \
    puts(_before == g_tests_failed ? "ok" : "FAILED"); } while (0)

/* --- unit tests ---------------------------------------------------------------------------- */
static void test_version(void) {
    CHECK(mat_version() == MAT_VERSION);
    CHECK(strcmp(mat_version_string(), "1.2.0") == 0);
    CHECK(mat_abi_compatible(MAT_VERSION));
    CHECK(mat_abi_compatible((1 << 16) | (0 << 8) | 0));          /* older 1.x caller: fine */
    CHECK(!mat_abi_compatible((2 << 16)));                        /* major mismatch: not fine */
    CHECK(!mat_abi_compatible((1 << 16) | (9 << 8)));             /* caller newer than library: not fine */
}

static void test_strerror_covers_every_status(void) {
    for (int s = 0; s < MAT_STATUS_COUNT_; s++) {
        CHECK(mat_strerror((mat_status)s) != NULL);
        CHECK(strcmp(mat_strerror((mat_status)s), "unknown status") != 0);   /* table has no holes */
    }
    CHECK(strcmp(mat_strerror((mat_status)999), "unknown status") == 0);
}

static void test_create_destroy(void) {
    mat_t *m = mat_create(3, 4);
    CHECK(m != NULL);
    CHECK(mat_rows(m) == 3 && mat_cols(m) == 4);
    CHECK(mat_data(m)[11] == 0.0);                                /* zero-filled */
    mat_destroy(m);
    mat_destroy(NULL);                                            /* documented no-op */
    CHECK(mat_create(0, 5) == NULL);
    CHECK(mat_create(SIZE_MAX, 2) == NULL);                       /* overflow rejected, no huge calloc */
}

static void test_wrap_is_zero_copy(void) {
    double buf[6] = { 1, 2, 3, 4, 5, 6 };
    mat_t *m = mat_wrap(buf, 2, 3);
    CHECK(m != NULL && mat_data(m) == buf);                       /* same pointer: no copy */
    mat_data(m)[0] = 42;
    CHECK(buf[0] == 42);                                          /* writes go to the caller's buffer */
    mat_destroy(m);                                               /* must NOT free buf (ASan would tell) */
    CHECK(buf[5] == 6);
}

static void test_matmul_errors(void) {
    mat_t *a = mat_create(2, 3), *b = mat_create(3, 2), *c = mat_create(2, 2), *bad = mat_create(2, 3);
    CHECK_EQ_STATUS(mat_matmul(NULL, b, c), MAT_ERR_ARG);
    CHECK_EQ_STATUS(mat_matmul(a, bad, c), MAT_ERR_SHAPE);       /* 2x3 * 2x3 */
    CHECK_EQ_STATUS(mat_matmul(a, b, bad), MAT_ERR_SHAPE);       /* out has wrong shape */
    CHECK_EQ_STATUS(mat_matmul(c, c, c), MAT_ERR_ALIAS);
    CHECK_EQ_STATUS(mat_matmul(a, b, c), MAT_OK);
    mat_destroy(a); mat_destroy(b); mat_destroy(c); mat_destroy(bad);
}

static void test_matmul_known_values(void) {
    double A[6] = { 1, 2, 3, 4, 5, 6 }, B[6] = { 7, 8, 9, 10, 11, 12 }, C[4];
    CHECK_EQ_STATUS(mat_matmul_raw(A, B, C, 2, 3, 2), MAT_OK);
    /* [[1,2,3],[4,5,6]] @ [[7,8],[9,10],[11,12]] = [[58,64],[139,154]]  (checked in NumPy) */
    CHECK(C[0] == 58 && C[1] == 64 && C[2] == 139 && C[3] == 154);
}

/* --- property test: random inputs, algebraic identities, seeded so failures reproduce ------ */
static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static double rnd(void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return (double)(g_rng >> 11) / 9007199254740992.0 - 0.5; }
static void fill(mat_t *m) { for (size_t i = 0; i < mat_rows(m) * mat_cols(m); i++) mat_data(m)[i] = rnd(); }

static void test_property_matmul_identities(void) {
    for (int trial = 0; trial < 20; trial++) {
        size_t m = 1 + (size_t)(fabs(rnd()) * 12), k = 1 + (size_t)(fabs(rnd()) * 12), n = 1 + (size_t)(fabs(rnd()) * 12);
        mat_t *A = mat_create(m, k), *B = mat_create(k, n), *I = mat_create(n, n);
        mat_t *AB = mat_create(m, n), *ABI = mat_create(m, n);
        fill(A); fill(B);
        for (size_t i = 0; i < n; i++) mat_data(I)[i * n + i] = 1.0;
        CHECK_EQ_STATUS(mat_matmul(A, B, AB), MAT_OK);
        CHECK_EQ_STATUS(mat_matmul(AB, I, ABI), MAT_OK);                    /* (AB)I == AB */
        for (size_t i = 0; i < m * n; i++) CHECK_NEAR(mat_data(AB)[i], mat_data(ABI)[i], 1e-15);
        /* row-sum property: (A B) 1 == A (B 1) */
        mat_t *ones = mat_create(n, 1), *B1 = mat_create(k, 1), *AB1 = mat_create(m, 1), *A_B1 = mat_create(m, 1);
        for (size_t i = 0; i < n; i++) mat_data(ones)[i] = 1.0;
        mat_matmul(AB, ones, AB1); mat_matmul(B, ones, B1); mat_matmul(A, B1, A_B1);
        for (size_t i = 0; i < m; i++) CHECK_NEAR(mat_data(AB1)[i], mat_data(A_B1)[i], 1e-12 * (double)k);
        mat_destroy(A); mat_destroy(B); mat_destroy(I); mat_destroy(AB); mat_destroy(ABI);
        mat_destroy(ones); mat_destroy(B1); mat_destroy(AB1); mat_destroy(A_B1);
    }
}

/* --- golden value: a checksum recorded once from a trusted run, guards against regressions --- */
#define GOLDEN_FROBENIUS 5.242987517513933   /* recorded from the first trusted run (seed 12345) */
static void test_golden_checksum(void) {
    g_rng = 12345;
    mat_t *A = mat_create(16, 16), *B = mat_create(16, 16), *C = mat_create(16, 16);
    fill(A); fill(B);
    mat_matmul(A, B, C);
    double fro = mat_frobenius(C);
    /* Recorded from the first correct implementation. If this changes, either the RNG, the matmul,
       or the summation order changed — decide consciously which, then update the constant.      */
    const double golden = GOLDEN_FROBENIUS;
    if (fabs(fro - golden) > 1e-12) fprintf(stderr, "  frobenius = %.15f (golden %.15f)\n", fro, golden);
    CHECK_NEAR(fro, golden, 1e-12);
    mat_destroy(A); mat_destroy(B); mat_destroy(C);
}

/* --- parser tests, including the corner cases a fuzzer finds first ------------------------- */
static void test_csv_parser(void) {
    double v[4]; size_t n;
    CHECK_EQ_STATUS(mat_parse_csv_row("1.5, -2,3e4\n", v, 4, &n), MAT_OK);
    CHECK(n == 3 && v[0] == 1.5 && v[1] == -2 && v[2] == 3e4);
    CHECK_EQ_STATUS(mat_parse_csv_row("", v, 4, &n), MAT_OK);              CHECK(n == 0);
    CHECK_EQ_STATUS(mat_parse_csv_row("   \n", v, 4, &n), MAT_OK);         CHECK(n == 0);
    CHECK_EQ_STATUS(mat_parse_csv_row("1,2,3,4,5", v, 4, &n), MAT_ERR_OVERFLOW); CHECK(n == 4);
    CHECK_EQ_STATUS(mat_parse_csv_row("1,abc", v, 4, &n), MAT_ERR_PARSE);  CHECK(n == 1);
    CHECK_EQ_STATUS(mat_parse_csv_row("1,", v, 4, &n), MAT_ERR_PARSE);
    CHECK_EQ_STATUS(mat_parse_csv_row("1 2", v, 4, &n), MAT_ERR_PARSE);
    CHECK_EQ_STATUS(mat_parse_csv_row("1e999", v, 4, &n), MAT_OK);          CHECK(isinf(v[0]));  /* ERANGE logged, not an error */
    CHECK_EQ_STATUS(mat_parse_csv_row(NULL, v, 4, &n), MAT_ERR_ARG);
    CHECK_EQ_STATUS(mat_parse_csv_row("1,2", NULL, 0, &n), MAT_ERR_OVERFLOW);  /* cap 0: counts nothing */
}

/* the fuzz harness's exact body, run on a few hand-picked inputs so it is tested even here */
static void test_fuzz_harness_inputs(void) {
    const char *inputs[] = { "", ",", ",,,", "1,,2", "-", "e", "1e", ".", "0x1p3", "nan", "inf,-inf", "\n\n", "1\r\n" };
    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; i++) {
        double v[16]; size_t n = 99;
        mat_status s = mat_parse_csv_row(inputs[i], v, 16, &n);
        CHECK(n <= 16);
        CHECK(s == MAT_OK || s == MAT_ERR_PARSE);                  /* never anything else for these */
    }
}

/* --- a log sink test: capture messages instead of printing them ------------------------------- */
static char g_captured[256];
static void capture_log(mat_log_level level, const char *msg, void *user) {
    (void)level; snprintf(user, 256, "%s", msg);
}
/* the tests deliberately trigger errors; keep those messages out of the test output */
static int g_quiet_count = 0;
static void quiet_log(mat_log_level level, const char *msg, void *user) { (void)level; (void)msg; (void)user; g_quiet_count++; }
static void test_logging_hook(void) {
    mat_set_log_fn(capture_log, g_captured);
    mat_set_log_level(MAT_LOG_WARN);
    g_captured[0] = '\0';
    mat_t *a = mat_create(2, 3), *b = mat_create(2, 3), *c = mat_create(2, 3);
    mat_matmul(a, b, c);                                           /* shape mismatch -> one WARN */
    CHECK(strstr(g_captured, "shape mismatch") != NULL);
    g_captured[0] = '\0';
    mat_set_log_level(MAT_LOG_ERROR);
    mat_matmul(a, b, c);                                           /* WARN is now below the level */
    CHECK(g_captured[0] == '\0');
    mat_destroy(a); mat_destroy(b); mat_destroy(c);
    mat_set_log_fn(getenv("MAT_LOG") ? NULL : quiet_log, NULL);
    mat_set_log_level(MAT_LOG_WARN);
}

int main(void) {
    printf("libmatrix %s (0x%06x), compiled against 0x%06x, ABI compatible: %s\n",
           mat_version_string(), mat_version(), MAT_VERSION, mat_abi_compatible(MAT_VERSION) ? "yes" : "no");
    LOG_INFO("starting tests (set MAT_LOG=info to see this line)");
    if (!getenv("MAT_LOG")) mat_set_log_fn(quiet_log, NULL);      /* MAT_LOG=warn ./ex_demo shows them */
    puts("== tests ==");
    RUN(test_version);
    RUN(test_strerror_covers_every_status);
    RUN(test_create_destroy);
    RUN(test_wrap_is_zero_copy);
    RUN(test_matmul_errors);
    RUN(test_matmul_known_values);
    RUN(test_property_matmul_identities);
    RUN(test_golden_checksum);
    RUN(test_csv_parser);
    RUN(test_fuzz_harness_inputs);
    RUN(test_logging_hook);
    printf("%d tests, %d checks, %d failed (%d expected error/warn log lines suppressed)\n",
           g_tests_run, g_checks, g_tests_failed, g_quiet_count);
    mat_set_log_fn(NULL, NULL);

    puts("== the FFI path, as Python will use it ==");
    double A[6] = { 1, 2, 3, 4, 5, 6 }, B[6] = { 7, 8, 9, 10, 11, 12 }, C[4];
    mat_status s = mat_matmul_raw(A, B, C, 2, 3, 2);
    printf("  mat_matmul_raw -> %s; C = [[%g, %g], [%g, %g]]\n", mat_strerror(s), C[0], C[1], C[2], C[3]);
    puts("  build the dylib (header comment) and run the Python script from lesson §5 to see the same");
    puts("  numbers come out of NumPy arrays passed by pointer, with zero copies.");
    return g_tests_failed ? 1 : 0;
}
#endif /* !MATRIX_NO_MAIN && !MATRIX_FUZZ */
