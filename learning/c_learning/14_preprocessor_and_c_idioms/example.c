/*
 * Chapter 14 — The Preprocessor and C Idioms
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 * See the preprocessor's output:   cc -E -P -std=c11 example.c | tail -120
 * Enable debug logging:            cc ... -DLOG_MIN_LEVEL=0 ...
 *
 * Contents:
 *   1. hygienic macros: SQUARE, MAX, SWAP (do/while(0)), and the MAX(i++, j) trap
 *   2. predefined macros, stringification, token pasting
 *   3. variadic LOG macro with levels
 *   4. X-macros: one activation list -> enum + names + fn table + dfn table + parser
 *   5. "header-only" section: the stb-style IMPLEMENTATION pattern, in one file
 *   6. error enum + string table via X-macro, out-parameters, errno/strerror, goto cleanup
 *   7. opaque pointer (Counter) with a const-correct API
 *   8. designated initializers, compound literals, Config-as-kwargs
 *   9. reading a signal()-shaped declaration, with and without typedef
 *  10. static inline helpers
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 1. Hygienic macros                                                   */
/* ------------------------------------------------------------------ */
#define BAD_SQUARE(x) x * x
#define SQUARE(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SWAP(T, a, b) do { T _tmp = (a); (a) = (b); (b) = _tmp; } while (0)

/* The static inline alternative: typed, argument evaluated exactly once. */
static inline int max_int(int a, int b) { return a > b ? a : b; }

/* ------------------------------------------------------------------ */
/* 2. Stringify and paste                                               */
/* ------------------------------------------------------------------ */
#define STR(x) #x
#define XSTR(x) STR(x)          /* expand first, then stringify */
#define CAT(a, b) a##b
#define N_HIDDEN 128

/* ------------------------------------------------------------------ */
/* 3. Variadic LOG                                                      */
/* ------------------------------------------------------------------ */
enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
static const char *const log_names[] = { "DEBUG", "INFO", "WARN", "ERROR" };
#ifndef LOG_MIN_LEVEL
#  define LOG_MIN_LEVEL LOG_INFO
#endif
#define LOG(lvl, fmt, ...) do { if ((lvl) >= LOG_MIN_LEVEL) \
    fprintf(stderr, "[%s %s:%d %s] " fmt "\n", log_names[lvl], __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

/* ------------------------------------------------------------------ */
/* 4. X-macros for activations                                          */
/* ------------------------------------------------------------------ */
static double act_identity(double x)  { return x; }
static double dact_identity(double x) { (void)x; return 1.0; }
static double act_relu(double x)      { return x > 0 ? x : 0; }
static double dact_relu(double x)     { return x > 0 ? 1.0 : 0.0; }
static double act_sigmoid(double x)   { return 1.0 / (1.0 + exp(-x)); }
static double dact_sigmoid(double x)  { double s = act_sigmoid(x); return s * (1 - s); }
static double dact_tanh(double x)     { double t = tanh(x); return 1 - t * t; }

#define ACTIVATION_LIST(X)                                   \
    X(ACT_IDENTITY, "identity", act_identity, dact_identity) \
    X(ACT_RELU,     "relu",     act_relu,     dact_relu)     \
    X(ACT_SIGMOID,  "sigmoid",  act_sigmoid,  dact_sigmoid)  \
    X(ACT_TANH,     "tanh",     tanh,         dact_tanh)

#define AS_ENUM(id, name, fn, dfn) id,
typedef enum { ACTIVATION_LIST(AS_ENUM) ACT_COUNT } Activation;
#undef AS_ENUM

#define AS_NAME(id, name, fn, dfn) [id] = name,
static const char *const act_names[ACT_COUNT] = { ACTIVATION_LIST(AS_NAME) };
#undef AS_NAME

typedef double (*UnaryFn)(double);
#define AS_FN(id, name, fn, dfn) [id] = fn,
#define AS_DFN(id, name, fn, dfn) [id] = dfn,
static const UnaryFn act_fns[ACT_COUNT]  = { ACTIVATION_LIST(AS_FN) };
static const UnaryFn act_dfns[ACT_COUNT] = { ACTIVATION_LIST(AS_DFN) };
#undef AS_FN
#undef AS_DFN

static Activation act_from_name(const char *s) {
    for (int i = 0; i < ACT_COUNT; i++)
        if (strcmp(s, act_names[i]) == 0) return (Activation)i;
    return ACT_COUNT;
}

/* ------------------------------------------------------------------ */
/* 5. stb-style header-only pattern, simulated inside one file.         */
/*    In real life the block below lives in matrix.h; exactly one .c    */
/*    does `#define MATRIX_IMPLEMENTATION` before including it.         */
/* ------------------------------------------------------------------ */
#define MATRIX_IMPLEMENTATION          /* this TU provides the definitions */

/* ---- begin "matrix.h" ---- */
#ifndef MATRIX_H
#define MATRIX_H
#include <stddef.h>
typedef struct { size_t rows, cols; double *data; } Matrix;
Matrix *mat_new(size_t rows, size_t cols);
void    mat_free(Matrix *m);
static inline size_t mat_idx(const Matrix *m, size_t i, size_t j) { return i * m->cols + j; }
static inline double mat_get(const Matrix *m, size_t i, size_t j) { return m->data[mat_idx(m, i, j)]; }
#endif /* MATRIX_H */

#ifdef MATRIX_IMPLEMENTATION
Matrix *mat_new(size_t rows, size_t cols) {
    Matrix *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->rows = rows; m->cols = cols;
    m->data = calloc(rows * cols, sizeof *m->data);
    if (!m->data) { free(m); return NULL; }
    return m;
}
void mat_free(Matrix *m) { if (m) { free(m->data); free(m); } }
#endif /* MATRIX_IMPLEMENTATION */
/* ---- end "matrix.h" ---- */

/* ------------------------------------------------------------------ */
/* 6. Error handling                                                    */
/* ------------------------------------------------------------------ */
#define ERROR_LIST(X)                       \
    X(ERR_OK,    "ok")                      \
    X(ERR_ALLOC, "allocation failed")       \
    X(ERR_SHAPE, "shape mismatch")          \
    X(ERR_IO,    "I/O error")               \
    X(ERR_PARSE, "parse error")
#define AS_ENUM(id, s) id,
typedef enum { ERROR_LIST(AS_ENUM) ERR_COUNT } Err;
#undef AS_ENUM
#define AS_STR(id, s) [id] = s,
static const char *const err_strings[ERR_COUNT] = { ERROR_LIST(AS_STR) };
#undef AS_STR
static const char *err_str(Err e) { return (e >= 0 && e < ERR_COUNT) ? err_strings[e] : "unknown"; }

/* Out-parameter for the result; return value is the status. */
static Err parse_double(const char *s, double *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE) return ERR_PARSE;
    *out = v;
    return ERR_OK;
}

/* Multi-resource function with a single cleanup path.
 * Parses `text` ("1,2,3\n4,5,6\n") into a rows x cols matrix. */
static Err parse_matrix_text(const char *text, size_t rows, size_t cols, Matrix **out) {
    Err rc = ERR_OK;
    char *copy = NULL;        /* every resource starts NULL so cleanup can free unconditionally */
    Matrix *m = NULL;

    copy = malloc(strlen(text) + 1);
    if (!copy) { rc = ERR_ALLOC; goto cleanup; }
    strcpy(copy, text);

    m = mat_new(rows, cols);
    if (!m) { rc = ERR_ALLOC; goto cleanup; }

    size_t r = 0, c = 0;
    for (char *line = strtok(copy, "\n"); line; line = strtok(NULL, "\n"), r++) {
        if (r >= rows) { rc = ERR_SHAPE; goto cleanup; }
        c = 0;
        char *save = NULL;
        for (char *tok = line; ; tok = NULL) {
            char *field = strtok_r(tok, ",", &save);   /* strtok_r: re-entrant, safe to nest */
            if (!field) break;
            if (c >= cols) { rc = ERR_SHAPE; goto cleanup; }
            if (parse_double(field, &m->data[mat_idx(m, r, c)]) != ERR_OK) { rc = ERR_PARSE; goto cleanup; }
            c++;
        }
        if (c != cols) { rc = ERR_SHAPE; goto cleanup; }
    }
    if (r != rows) { rc = ERR_SHAPE; goto cleanup; }

    *out = m;
    m = NULL;                 /* ownership transferred: don't free below */
cleanup:
    mat_free(m);              /* NULL-safe */
    free(copy);               /* NULL-safe */
    return rc;
}

/* ------------------------------------------------------------------ */
/* 7. Opaque pointer. In real code the struct definition is in the .c   */
/*    and users only see `typedef struct Counter Counter;`.             */
/* ------------------------------------------------------------------ */
typedef struct Counter Counter;
static Counter *counter_new(void);
static void     counter_free(Counter *c);
static void     counter_add(Counter *c, double x);          /* mutates: non-const */
static double   counter_mean(const Counter *c);             /* reads: const */
static size_t   counter_n(const Counter *c);

struct Counter { size_t n; double sum, m2, mean; };         /* private layout (Welford) */
static Counter *counter_new(void) { return calloc(1, sizeof(Counter)); }
static void counter_free(Counter *c) { free(c); }
static void counter_add(Counter *c, double x) {
    c->n++;
    double d = x - c->mean;
    c->mean += d / (double)c->n;
    c->m2 += d * (x - c->mean);
    c->sum += x;
}
static double counter_mean(const Counter *c) { return c->n ? c->mean : 0.0; }
static size_t counter_n(const Counter *c) { return c->n; }

/* ------------------------------------------------------------------ */
/* 8. Config struct as keyword arguments                                */
/* ------------------------------------------------------------------ */
typedef struct { double lr, momentum; int epochs, batch_size; const char *optimizer; } TrainConfig;
#define TRAIN_CONFIG_DEFAULTS \
    ((TrainConfig){ .lr = 0.01, .momentum = 0.9, .epochs = 10, .batch_size = 32, .optimizer = "sgd" })

static void print_config(const char *label, const TrainConfig *c) {
    printf("  %-22s lr=%-6g momentum=%-4g epochs=%-3d batch=%-3d opt=%s\n",
           label, c->lr, c->momentum, c->epochs, c->batch_size, c->optimizer ? c->optimizer : "(null)");
}

/* ------------------------------------------------------------------ */
/* 9. A signal()-shaped declaration                                     */
/* ------------------------------------------------------------------ */
static void on_tick(int n)  { printf("    tick %d\n", n); }
static void on_alarm(int n) { printf("    ALARM %d\n", n); }
static void (*g_handler)(int) = NULL;

/* install: function taking (int, pointer to function (int)->void), returning pointer to
 * function (int)->void — the previous handler. Read: identifier `install`, right: params,
 * left: `*` returns pointer, out of parens, right: `(int)` to function taking int, left: void. */
static void (*install(int which, void (*h)(int)))(int) {
    (void)which;
    void (*prev)(int) = g_handler;
    g_handler = h;
    return prev;
}
/* The same thing, readable: */
typedef void (*Handler)(int);
static Handler install2(int which, Handler h) { return install(which, h); }

/* ------------------------------------------------------------------ */
int main(void) {
    puts("== 1. macro hygiene ==");
    int a = 2;
    printf("  BAD_SQUARE(a+1) = %d   SQUARE(a+1) = %d   (expected 9)\n", BAD_SQUARE(a + 1), SQUARE(a + 1));
    int i = 5, j = 3;
    int m1 = MAX(i++, j);             /* i++ evaluated twice when it wins */
    printf("  MAX(i++, j) with i=5,j=3 -> %d, i is now %d (expected 6; macro trap)\n", m1, i);
    i = 5;
    int m2 = max_int(i++, j);         /* static inline: evaluated once */
    printf("  max_int(i++, j)          -> %d, i is now %d (correct)\n", m2, i);
    double x = 1.5, y = 2.5;
    if (x < y) SWAP(double, x, y); else puts("  ordered");
    printf("  after SWAP: x=%.1f y=%.1f\n", x, y);

    puts("\n== 2. predefined macros, # and ## ==");
    printf("  %s:%d in %s()  C%ld  built %s\n", __FILE__, __LINE__, __func__, __STDC_VERSION__, __DATE__);
    printf("  STR(N_HIDDEN)=\"%s\"  XSTR(N_HIDDEN)=\"%s\"  STR(a + b)=\"%s\"\n",
           STR(N_HIDDEN), XSTR(N_HIDDEN), STR(a + b));
    int CAT(var_, 42) = 7;            /* declares var_42 */
    printf("  CAT(var_, 42) declared var_42 = %d\n", var_42);

    puts("\n== 3. LOG macro (to stderr) ==");
    LOG(LOG_DEBUG, "hidden below LOG_MIN_LEVEL unless -DLOG_MIN_LEVEL=0");
    LOG(LOG_INFO, "epoch %d loss %.4f", 3, 0.2311);
    LOG(LOG_WARN, "no varargs at all");

    puts("\n== 4. X-macro tables ==");
    for (int k = 0; k < ACT_COUNT; k++)
        printf("  %d %-9s f(0.5)=%.6f  f'(0.5)=%.6f\n", k, act_names[k], act_fns[k](0.5), act_dfns[k](0.5));
    Activation act = act_from_name("tanh");
    printf("  act_from_name(\"tanh\") = %d -> %s;  act_from_name(\"gelu\") = %d (ACT_COUNT = not found)\n",
           act, act_names[act], act_from_name("gelu"));

    puts("\n== 5/6. header-only Matrix + error handling with goto cleanup ==");
    Matrix *mat = NULL;
    Err e = parse_matrix_text("1,2,3\n4,5,6\n", 2, 3, &mat);
    printf("  good text    -> %s", err_str(e));
    if (e == ERR_OK) { printf("  m[1][2]=%.0f", mat_get(mat, 1, 2)); mat_free(mat); mat = NULL; }
    putchar('\n');
    e = parse_matrix_text("1,2,3\n4,x,6\n", 2, 3, &mat);
    printf("  bad number   -> %s\n", err_str(e));
    e = parse_matrix_text("1,2,3\n4,5\n", 2, 3, &mat);
    printf("  ragged row   -> %s\n", err_str(e));
    e = parse_matrix_text("1,2,3\n", 2, 3, &mat);
    printf("  too few rows -> %s\n", err_str(e));
    FILE *f = fopen("/nonexistent/dir/data.bin", "rb");
    if (!f) {
        int saved = errno;            /* save before anything else can clobber it */
        printf("  fopen failed -> %s: %s (errno=%d)\n", err_str(ERR_IO), strerror(saved), saved);
    }

    puts("\n== 7. opaque pointer ==");
    Counter *cnt = counter_new();
    for (int k = 1; k <= 10; k++) counter_add(cnt, (double)k);
    printf("  n=%zu mean=%.2f   (fields are private; only the API touches them)\n", counter_n(cnt), counter_mean(cnt));
    counter_free(cnt);

    puts("\n== 8. designated initializers / compound literals / config-as-kwargs ==");
    TrainConfig defaults = TRAIN_CONFIG_DEFAULTS;
    print_config("defaults", &defaults);
    print_config("(TrainConfig){.lr=0.1}", &(TrainConfig){ .lr = 0.1 });     /* trap: rest is ZERO */
    TrainConfig c = TRAIN_CONFIG_DEFAULTS; c.lr = 0.1; c.optimizer = "adam";
    print_config("defaults + overrides", &c);
    int lut[8] = { [3] = 1, [7] = 1 };
    printf("  lut = {%d,%d,%d,%d,%d,%d,%d,%d}   sum of (double[]){1,2,3} via pointer = %.0f\n",
           lut[0], lut[1], lut[2], lut[3], lut[4], lut[5], lut[6], lut[7],
           ((double *)(double[]){ 1, 2, 3 })[0] + ((double[]){ 1, 2, 3 })[1] + ((double[]){ 1, 2, 3 })[2]);

    puts("\n== 9. signal()-shaped declaration ==");
    void (*prev)(int) = install(1, on_tick);
    printf("  first install returned %s\n", prev ? "a handler" : "NULL");
    g_handler(1);
    Handler prev2 = install2(1, on_alarm);
    printf("  second install returned previous == on_tick? %s\n", prev2 == on_tick ? "yes" : "no");
    g_handler(2);
    puts("  typedef void (*Handler)(int); Handler install2(int, Handler);  <- same type, readable");

    puts("\n== 10. -E shows the truth ==");
    puts("  run: cc -E -P -std=c11 example.c | grep -A3 'typedef enum' | head");
    return 0;
}
