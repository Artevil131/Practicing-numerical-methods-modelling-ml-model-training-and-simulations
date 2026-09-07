/*
 * Chapter 09 — Multi-File Projects and Make: worked example
 *
 * Compile + run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * Generate a real multi-file project (matrix library + Makefile) to play with:
 *   ./ex_demo scaffold demo_project
 *   make -C demo_project            # release build -> demo_project/bin/prog
 *   make -C demo_project DEBUG=1 test   # ASan build + tests
 *   make -C demo_project clean
 *
 * This file is ONE translation unit, but it is organized as if it were four:
 * a "matrix.h" section (declarations), a "matrix.c" section (definitions with
 * static helpers), a "config.h/.c" section (extern global), and "main.c".
 * The comments mark where the file boundaries would be. The scaffold command
 * writes exactly that split to disk so you can build it with make.
 *
 * Demonstrates:
 *   1. declaration vs definition; why main can call a function whose body comes later
 *   2. static (internal linkage) helpers vs exported functions
 *   3. extern global: declaration in a "header", one definition in a "source"
 *   4. static inline accessors that belong in a header
 *   5. what the preprocessor does to #include (shown as text)
 *   6. the two linker errors, explained with the exact message
 *   7. a complete Makefile (written out by `scaffold`)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>   /* mkdir */

/* =====================================================================
 * ---- would be include/config.h --------------------------------------
 * ===================================================================== */
extern int g_verbose;                 /* DECLARATION: "exists somewhere". No storage. */
void config_set_verbose(int v);       /* functions are extern by default */

/* =====================================================================
 * ---- would be include/matrix.h --------------------------------------
 * Include guard would wrap this: #ifndef MATRIX_H / #define MATRIX_H / #endif
 * ===================================================================== */
typedef struct {
    size_t  rows, cols;
    double *data;                     /* owned; row-major */
} Matrix;

/* Returns a zeroed matrix. Caller owns; release with mat_free. data==NULL on failure. */
Matrix mat_zeros(size_t rows, size_t cols);
Matrix mat_eye(size_t n);
void   mat_free(Matrix *m);
/* out = a @ b. out pre-allocated (a->rows x b->cols). Returns 0, or -1 on shape mismatch. */
int    mat_matmul(Matrix *out, const Matrix *a, const Matrix *b);
double mat_sum(const Matrix *m);
void   mat_print(const char *name, const Matrix *m);

/* static inline in a header is allowed: every translation unit gets its own
 * private copy, so there is no duplicate symbol at link time. */
static inline double mat_get(const Matrix *m, size_t i, size_t j) { return m->data[i * m->cols + j]; }
static inline void   mat_set(Matrix *m, size_t i, size_t j, double v) { m->data[i * m->cols + j] = v; }

/* =====================================================================
 * ---- would be src/config.c ------------------------------------------
 * ===================================================================== */
int g_verbose = 0;                    /* DEFINITION: storage lives here, exactly once in the program */
void config_set_verbose(int v) { g_verbose = v; }

/* =====================================================================
 * ---- would be src/matrix.c ------------------------------------------
 * (it would start with #include "matrix.h" -- its own header first)
 * ===================================================================== */

/* static: visible only inside this translation unit. Another file could
 * define its own shapes_ok() without a clash. Not part of the interface. */
static int shapes_ok(const Matrix *out, const Matrix *a, const Matrix *b) {
    return a->cols == b->rows && out->rows == a->rows && out->cols == b->cols;
}

Matrix mat_zeros(size_t rows, size_t cols) {
    Matrix m = {rows, cols, calloc(rows * cols, sizeof *m.data)};
    if (!m.data) m.rows = m.cols = 0;
    if (g_verbose) fprintf(stderr, "[matrix] alloc %zux%zu\n", rows, cols);
    return m;
}
Matrix mat_eye(size_t n) {
    Matrix m = mat_zeros(n, n);
    for (size_t i = 0; i < n && m.data; i++) mat_set(&m, i, i, 1.0);
    return m;
}
void mat_free(Matrix *m) {
    if (g_verbose && m->data) fprintf(stderr, "[matrix] free %zux%zu\n", m->rows, m->cols);
    free(m->data); m->data = NULL; m->rows = m->cols = 0;
}
int mat_matmul(Matrix *out, const Matrix *a, const Matrix *b) {
    if (!shapes_ok(out, a, b)) return -1;
    for (size_t i = 0; i < a->rows; i++)
        for (size_t k = 0; k < a->cols; k++) {
            double aik = mat_get(a, i, k);
            for (size_t j = 0; j < b->cols; j++)
                out->data[i * out->cols + j] += aik * mat_get(b, k, j);
        }
    return 0;
}
double mat_sum(const Matrix *m) {
    double s = 0;
    for (size_t i = 0; i < m->rows * m->cols; i++) s += m->data[i];
    return s;
}
void mat_print(const char *name, const Matrix *m) {
    printf("%s (%zux%zu):\n", name, m->rows, m->cols);
    for (size_t i = 0; i < m->rows; i++) {
        printf("  ");
        for (size_t j = 0; j < m->cols; j++) printf("%6.2f", mat_get(m, i, j));
        printf("\n");
    }
}

/* =====================================================================
 * ---- scaffold: write a real project to disk ------------------------
 * ===================================================================== */

/* Writes `text` to `path`. Returns 0 on success. */
static int write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return -1; }
    fputs(text, f);
    return fclose(f) == 0 ? 0 : -1;
}
static int make_dir(const char *path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) { perror(path); return -1; }
    return 0;
}

static const char *MATRIX_H =
"#ifndef MATRIX_H\n#define MATRIX_H\n\n#include <stddef.h>\n\n"
"typedef struct { size_t rows, cols; double *data; } Matrix;\n\n"
"/* constructors return ownership; data==NULL on failure; release with mat_free */\n"
"Matrix mat_zeros(size_t rows, size_t cols);\nMatrix mat_eye(size_t n);\nvoid   mat_free(Matrix *m);\n"
"int    mat_matmul(Matrix *out, const Matrix *a, const Matrix *b);\n"
"double mat_sum(const Matrix *m);\nvoid   mat_print(const char *name, const Matrix *m);\n\n"
"static inline double mat_get(const Matrix *m, size_t i, size_t j) { return m->data[i * m->cols + j]; }\n"
"static inline void   mat_set(Matrix *m, size_t i, size_t j, double v) { m->data[i * m->cols + j] = v; }\n\n"
"#endif /* MATRIX_H */\n";

static const char *MATRIX_C =
"#include \"matrix.h\"   /* own header first */\n#include <stdio.h>\n#include <stdlib.h>\n\n"
"static int shapes_ok(const Matrix *out, const Matrix *a, const Matrix *b) {\n"
"    return a->cols == b->rows && out->rows == a->rows && out->cols == b->cols;\n}\n\n"
"Matrix mat_zeros(size_t rows, size_t cols) {\n"
"    Matrix m = {rows, cols, calloc(rows * cols, sizeof *m.data)};\n"
"    if (!m.data) m.rows = m.cols = 0;\n    return m;\n}\n"
"Matrix mat_eye(size_t n) {\n    Matrix m = mat_zeros(n, n);\n"
"    for (size_t i = 0; i < n && m.data; i++) mat_set(&m, i, i, 1.0);\n    return m;\n}\n"
"void mat_free(Matrix *m) { free(m->data); m->data = NULL; m->rows = m->cols = 0; }\n"
"int mat_matmul(Matrix *out, const Matrix *a, const Matrix *b) {\n"
"    if (!shapes_ok(out, a, b)) return -1;\n"
"    for (size_t i = 0; i < a->rows; i++)\n        for (size_t k = 0; k < a->cols; k++) {\n"
"            double aik = mat_get(a, i, k);\n"
"            for (size_t j = 0; j < b->cols; j++) out->data[i * out->cols + j] += aik * mat_get(b, k, j);\n"
"        }\n    return 0;\n}\n"
"double mat_sum(const Matrix *m) {\n    double s = 0;\n"
"    for (size_t i = 0; i < m->rows * m->cols; i++) s += m->data[i];\n    return s;\n}\n"
"void mat_print(const char *name, const Matrix *m) {\n"
"    printf(\"%s (%zux%zu):\\n\", name, m->rows, m->cols);\n"
"    for (size_t i = 0; i < m->rows; i++) {\n        printf(\"  \");\n"
"        for (size_t j = 0; j < m->cols; j++) printf(\"%6.2f\", mat_get(m, i, j));\n"
"        printf(\"\\n\");\n    }\n}\n";

static const char *MAIN_C =
"#include \"matrix.h\"\n#include <stdio.h>\n\n"
"int main(void) {\n"
"    Matrix a = mat_zeros(2, 2), i = mat_eye(2), out = mat_zeros(2, 2);\n"
"    if (!a.data || !i.data || !out.data) return 1;\n"
"    mat_set(&a, 0, 0, 1); mat_set(&a, 0, 1, 2); mat_set(&a, 1, 0, 3); mat_set(&a, 1, 1, 4);\n"
"    mat_matmul(&out, &a, &i);\n    mat_print(\"a @ I\", &out);\n"
"    mat_free(&a); mat_free(&i); mat_free(&out);\n    return 0;\n}\n";

static const char *TEST_C =
"#include \"matrix.h\"\n#include <assert.h>\n#include <math.h>\n#include <stdio.h>\n\n"
"static void test_identity(void) {\n"
"    Matrix a = mat_zeros(3, 3), i = mat_eye(3), out = mat_zeros(3, 3);\n"
"    for (size_t k = 0; k < 9; k++) a.data[k] = (double)k * 0.5;\n"
"    assert(mat_matmul(&out, &a, &i) == 0);\n"
"    for (size_t k = 0; k < 9; k++) assert(fabs(out.data[k] - a.data[k]) < 1e-12);\n"
"    mat_free(&a); mat_free(&i); mat_free(&out);\n}\n"
"static void test_shape_mismatch(void) {\n"
"    Matrix a = mat_zeros(2, 3), b = mat_zeros(2, 3), out = mat_zeros(2, 3);\n"
"    assert(mat_matmul(&out, &a, &b) == -1);\n"
"    mat_free(&a); mat_free(&b); mat_free(&out);\n}\n"
"int main(void) {\n    test_identity();\n    test_shape_mismatch();\n"
"    printf(\"all matrix tests passed\\n\");\n    return 0;\n}\n";

/* NOTE: recipe lines below begin with a real TAB character. */
static const char *MAKEFILE =
"# ---- toolchain ----------------------------------------------------\n"
"CC      := cc\n"
"CFLAGS  := -Wall -Wextra -std=c11 -Iinclude -MMD -MP\n"
"LDFLAGS :=\n"
"LDLIBS  := -lm\n\n"
"# ---- build type: `make` (release) or `make DEBUG=1` ------------------\n"
"ifdef DEBUG\n"
"  CFLAGS  += -g -O0 -fsanitize=address,undefined\n"
"  LDFLAGS += -fsanitize=address,undefined\n"
"  BUILD   := build/debug\n"
"else\n"
"  CFLAGS  += -O2 -DNDEBUG\n"
"  BUILD   := build/release\n"
"endif\n\n"
"# ---- files ---------------------------------------------------------\n"
"LIB_SRCS := $(wildcard src/*.c)\n"
"LIB_OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(LIB_SRCS))\n"
"LIB      := $(BUILD)/libmat.a\n"
"BIN      := bin/prog\n"
"TEST_BIN := bin/test_matrix\n"
"DEPS     := $(LIB_OBJS:.o=.d) $(BUILD)/main.d $(BUILD)/test_matrix.d\n\n"
"# ---- rules ---------------------------------------------------------\n"
".PHONY: all clean run test\n\n"
"all: $(BIN)\n\n"
"$(LIB): $(LIB_OBJS)\n"
"\tar rcs $@ $^\n\n"
"$(BIN): $(BUILD)/main.o $(LIB) | bin\n"
"\t$(CC) $(LDFLAGS) $< -L$(BUILD) -lmat -o $@ $(LDLIBS)\n\n"
"$(TEST_BIN): $(BUILD)/test_matrix.o $(LIB) | bin\n"
"\t$(CC) $(LDFLAGS) $< -L$(BUILD) -lmat -o $@ $(LDLIBS)\n\n"
"$(BUILD)/%.o: src/%.c | $(BUILD)\n"
"\t$(CC) $(CFLAGS) -c $< -o $@\n\n"
"$(BUILD)/%.o: app/%.c | $(BUILD)\n"
"\t$(CC) $(CFLAGS) -c $< -o $@\n\n"
"$(BUILD)/%.o: tests/%.c | $(BUILD)\n"
"\t$(CC) $(CFLAGS) -c $< -o $@\n\n"
"$(BUILD) bin:\n"
"\tmkdir -p $@\n\n"
"run: $(BIN)\n"
"\t./$(BIN)\n\n"
"test: $(TEST_BIN)\n"
"\t./$(TEST_BIN)\n\n"
"clean:\n"
"\trm -rf build bin\n\n"
"-include $(DEPS)\n";

/* Writes the project tree under `root`. Returns 0 on success. */
static int scaffold(const char *root) {
    char path[1024];
    const char *dirs[] = { "", "/include", "/src", "/app", "/tests" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        snprintf(path, sizeof path, "%s%s", root, dirs[i]);
        if (make_dir(path) != 0) return -1;
    }
    struct { const char *rel; const char *text; } files[] = {
        { "/include/matrix.h",    MATRIX_H },
        { "/src/matrix.c",        MATRIX_C },
        { "/app/main.c",          MAIN_C },
        { "/tests/test_matrix.c", TEST_C },
        { "/Makefile",            MAKEFILE },
        { "/.gitignore",          "build/\nbin/\n" },
    };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        snprintf(path, sizeof path, "%s%s", root, files[i].rel);
        if (write_file(path, files[i].text) != 0) return -1;
        printf("wrote %s\n", path);
    }
    printf("\nNow run:\n  make -C %s run\n  make -C %s DEBUG=1 test\n  make -C %s clean\n", root, root, root);
    return 0;
}

/* =====================================================================
 * ---- would be app/main.c --------------------------------------------
 * ===================================================================== */
int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "scaffold") == 0)
        return scaffold(argv[2]) == 0 ? 0 : 1;

    printf("=== 1. declaration vs definition ===\n");
    printf("main() calls mat_zeros(), whose body appears ABOVE in this file -- but only the\n"
           "declaration `Matrix mat_zeros(size_t, size_t);` is required to compile the call.\n"
           "In a multi-file project that declaration comes from #include \"matrix.h\" and the\n"
           "body lives in matrix.o; the LINKER connects them.\n");

    printf("\n=== 2. static vs exported ===\n");
    printf("shapes_ok() is static: private to this translation unit. `nm ex_demo | grep shapes_ok`\n"
           "shows nothing (or a lowercase 't'); `nm ex_demo | grep mat_matmul` shows 'T _mat_matmul'.\n");

    printf("\n=== 3. extern global ===\n");
    printf("g_verbose declared `extern int g_verbose;` (header) and defined `int g_verbose = 0;` (one .c).\n");
    config_set_verbose(1);
    Matrix a = mat_zeros(2, 3), b = mat_eye(3), out = mat_zeros(2, 3);   /* verbose: logs allocs to stderr */
    config_set_verbose(0);
    if (!a.data || !b.data || !out.data) return 1;

    printf("\n=== 4. static inline accessors from the 'header' ===\n");
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) mat_set(&a, i, j, (double)(i * 3 + j));
    if (mat_matmul(&out, &a, &b) != 0) return 1;
    mat_print("A @ I3", &out);
    printf("sum = %.1f\n", mat_sum(&out));
    printf("shape mismatch (A @ A) returns %d\n", mat_matmul(&out, &a, &a));
    mat_free(&a); mat_free(&b); mat_free(&out);

    printf("\n=== 5. what #include does ===\n");
    printf("`cc -E main.c` shows main.c AFTER the preprocessor pasted every header in.\n"
           "That text is the translation unit the compiler actually sees. Try it on this file:\n"
           "  cc -E example.c | wc -l      (thousands of lines from stdio.h/stdlib.h alone)\n");

    printf("\n=== 6. the two linker errors ===\n");
    printf("Forgot to link matrix.o?  ->  Undefined symbols for architecture arm64: \"_mat_zeros\", referenced from: _main in main.o\n");
    printf("Defined a global in a header included twice?  ->  duplicate symbol '_g_verbose' in: a.o b.o\n");

    printf("\n=== 7. Makefile ===\n");
    printf("Run `./ex_demo scaffold demo_project` to write a complete src/include/app/tests/Makefile\n"
           "project with debug/release builds, -MMD dependency tracking, a static library, and tests.\n");
    return 0;
}
