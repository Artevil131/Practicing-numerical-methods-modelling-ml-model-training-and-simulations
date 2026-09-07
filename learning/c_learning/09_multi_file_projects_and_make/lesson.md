# Chapter 09 — Multi-File Projects and Make

## What you'll be able to do after this chapter

- Split a program into `.h` interfaces and `.c` implementations and explain what the compiler and linker each see.
- Diagnose "undefined reference" and "multiple definition" errors by reasoning about declarations vs definitions.
- Compile to object files, link them, and build a static library with `ar`.
- Write a Makefile with pattern rules, automatic variables, `-MMD` dependency tracking, and debug/release targets.
- Lay out a `src/ include/ build/ bin/` project you can reuse for every remaining project in this course.
- Structure a reusable `matrix` library (`matrix.h`, `matrix.c`, `test_matrix.c`).

## Why this matters for ML / numerics / sims

By the tiny-transformer project you will have a matrix library, a tokenizer, an autograd engine, a data loader, and an image writer. That is thousands of lines. In one file it is unnavigable, recompiles take seconds per edit, and you cannot reuse the matrix code in the N-body sim without copy-paste. Splitting into modules with clean headers is how you get from "programs" to "a codebase". `make` is how you rebuild only what changed. Every C project you will ever read — CPython, llama.cpp, SQLite — is organized this way.

---

## 1. Translation units: what the compiler actually sees

The compiler does not compile "your program". It compiles one **translation unit** at a time: a single `.c` file *after* the preprocessor has pasted in every `#include`d header and expanded every macro. Each translation unit becomes one object file (`.o`). The compiler has no knowledge of any other `.c` file.

```
matrix.c ──(preprocess: paste matrix.h, stdlib.h ...)──> big text ──(compile)──> matrix.o
main.c   ──(preprocess: paste matrix.h, stdio.h ...)───> big text ──(compile)──> main.o
                                                                                    │
                                          matrix.o + main.o ──(link)──> bin/prog  <─┘
```

Consequence: if `main.c` calls `mat_zeros`, the compiler needs to have *seen a declaration* of `mat_zeros` in main.c's translation unit (via the header) so it knows the argument and return types. It does not need the body. The **linker** later finds the body in `matrix.o` and connects the call to it.

Run the preprocessor alone to see the translation unit: `cc -E main.c | less`. It is instructive once.

---

## 2. Declaration vs definition

| | Declaration | Definition |
|---|---|---|
| What it says | "this thing exists, with this type" | "here is the thing itself" |
| Function | `Matrix mat_zeros(size_t r, size_t c);` | `Matrix mat_zeros(size_t r, size_t c) { ... }` |
| Variable | `extern int g_verbose;` | `int g_verbose = 0;` |
| Struct type | `struct Matrix;` (incomplete) | `struct Matrix { size_t rows, cols; double *data; };` |
| How many allowed | any number, in any file | **exactly one** across the whole program (for functions and globals) |
| Where it goes | header (`.h`) | source (`.c`) |

Type definitions (`struct` bodies, `typedef`s, `enum`s) are the exception: they *do* go in headers and are allowed once per translation unit — that is what include guards enforce.

---

## 3. Header = interface, source = implementation

```c
/* include/matrix.h — what users of the module need to know, nothing more */
#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>                       /* size_t */

typedef struct {
    size_t  rows, cols;
    double *data;                         /* owned; row-major */
} Matrix;

/* Returns a zeroed rows x cols matrix. Caller owns; release with mat_free. data==NULL on failure. */
Matrix mat_zeros(size_t rows, size_t cols);
void   mat_free(Matrix *m);
int    mat_matmul(Matrix *out, const Matrix *a, const Matrix *b);
void   mat_print(const char *name, const Matrix *m);

#endif /* MATRIX_H */
```

```c
/* src/matrix.c — the implementation */
#include "matrix.h"                       /* first: catches header/impl mismatches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helper visible only inside this file */
static int shapes_ok(const Matrix *out, const Matrix *a, const Matrix *b) {
    return a->cols == b->rows && out->rows == a->rows && out->cols == b->cols;
}

Matrix mat_zeros(size_t rows, size_t cols) {
    Matrix m = {rows, cols, calloc(rows * cols, sizeof *m.data)};
    if (!m.data) m.rows = m.cols = 0;
    return m;
}
void mat_free(Matrix *m) { free(m->data); m->data = NULL; m->rows = m->cols = 0; }
int mat_matmul(Matrix *out, const Matrix *a, const Matrix *b) {
    if (!shapes_ok(out, a, b)) return -1;
    /* ... */
    return 0;
}
void mat_print(const char *name, const Matrix *m) { /* ... */ (void)name; (void)m; }
```

Rules:

- `#include "matrix.h"` (quotes) searches your include paths first; `#include <stdio.h>` (angle brackets) searches system paths. Both end up searching the `-I` directories.
- A header contains: includes it needs for its own declarations, type definitions, function declarations, `extern` variable declarations, macros, `static inline` small functions. **Never** a function body (non-inline) or a variable definition — those would be duplicated in every file that includes the header, and the linker will reject it.
- Include your own header first in the `.c` that implements it. If the header forgets an include it needs, this ordering exposes the bug immediately.
- Each header should compile on its own: it must include what it uses (`<stddef.h>` for `size_t`).

**Python equivalent:** a module `matrix.py` with a public API; the header is like the module's `__all__` plus type hints — but enforced at compile time, and with the bodies in a separate file.

---

## 4. Include guards and `#pragma once`

If `main.c` includes `matrix.h` and also `layer.h`, and `layer.h` includes `matrix.h`, the `Matrix` struct would be defined twice in main.c's translation unit — an error. Include guards make the second inclusion a no-op:

```c
#ifndef MATRIX_H        /* if not yet defined... */
#define MATRIX_H        /* ...define it, and include the body */
/* ... */
#endif
```

`#pragma once` at the top of the header does the same thing, is shorter, and is supported by every compiler you will use (clang, gcc, MSVC). It is not in the C standard. Either is fine; pick one and be consistent. The guard macro name should be unique across your project (`PROJECT_MATRIX_H` if you have many).

---

## 5. `static` and `extern` at file scope

**`static` on a function or global variable** limits its visibility to the current translation unit ("internal linkage"). Other files cannot call it, even with a declaration. Use it for every helper that is not part of the module's interface. Benefits: no name collisions between `helper()` in two files, the compiler can inline or remove it freely, and readers know it is private.

**`extern` on a variable** declares it without defining it:

```c
/* config.h */
extern int g_verbose;         /* declaration: "exists somewhere" */

/* config.c */
int g_verbose = 0;            /* definition: storage lives here, exactly once */

/* main.c */
#include "config.h"
int main(void) { g_verbose = 1; ... }   /* fine: linker connects to config.o */
```

Functions are `extern` by default (that is why you never write `extern` on a function declaration). Globals are also extern by default — so writing `int g_verbose;` in a header would *define* it in every file that includes the header: multiple definition error. Always `extern` in the header, definition in exactly one `.c`.

Minimize globals. In numerics code they make functions impossible to test in isolation and impossible to run in parallel. Pass a `Config *` or `Rng *` instead.

Note: `static` inside a function (a persistent local) is a different use of the same keyword; `../03_functions_and_scope/lesson.md` covers it.

---

## 6. The two linker errors, explained

### Undefined reference / undefined symbol

```
Undefined symbols for architecture arm64:
  "_mat_zeros", referenced from:
      _main in main.o
ld: symbol(s) not found for architecture arm64
```

The compiler saw a declaration of `mat_zeros` (so `main.c` compiled), but the linker could not find its definition in any object file or library given on the command line. Causes, in order of likelihood:

1. You forgot to compile/link `matrix.c`: `cc main.c` instead of `cc main.c matrix.c`.
2. You forgot `-lm` (math library) — `sqrt`, `exp`, `tanh` are undefined (on macOS libm is part of libSystem so this usually links anyway; on Linux it fails).
3. Typo/mismatch between the name in the header and in the `.c`.
4. The function is `static` in the `.c` but declared in the header.
5. Library order: with static libraries, `-lfoo` must come *after* the objects that use it.

### Duplicate / multiple definition

```
duplicate symbol '_g_count' in:
    main.o
    matrix.o
ld: 1 duplicate symbol for architecture arm64
```

Two object files both *define* the same symbol. Causes:

1. A variable defined (not `extern`) in a header included by two files.
2. A function body in a header without `static inline`.
3. Two `.c` files with a same-named non-`static` helper (make them `static`).
4. Missing include guard causing a type to be defined twice (that is a compiler error, not a linker one, but it looks similar).

The linker's message names the symbol and the object files. That is all you need.

---

## 7. Compiling to objects, linking, `-I`, `-L`, `-l`

```
cc -Wall -Wextra -std=c11 -O2 -Iinclude -c src/matrix.c -o build/matrix.o   # compile only (-c)
cc -Wall -Wextra -std=c11 -O2 -Iinclude -c src/main.c   -o build/main.o
cc build/matrix.o build/main.o -o bin/prog -lm                                # link
```

| Flag | Meaning |
|---|---|
| `-c` | compile to `.o`, do not link |
| `-o file` | output name |
| `-Iinclude` | add `include/` to the search path for `#include "..."` |
| `-Lpath` | add a directory to the *library* search path |
| `-lmat` | link with `libmat.a` or `libmat.dylib` (the `lib` prefix and extension are implied) |
| `-lm` | the math library |
| `-g` | debug info (line numbers in ASan/lldb) |
| `-O0` / `-O2` / `-O3` | optimization level |
| `-DNDEBUG` | define a macro (disables `assert`) |
| `-MMD -MP` | emit `.d` dependency files (Section 10) |

Why separate compile and link? If you edit `main.c`, only `main.o` must be rebuilt; `matrix.o` is reused. On a 50-file project this turns a 30-second rebuild into 1 second. `make` automates the "which .o is stale?" question.

### Static vs dynamic libraries

```
ar rcs build/libmat.a build/matrix.o build/csv.o     # bundle objects into a static library
cc build/main.o -Lbuild -lmat -lm -o bin/prog         # link against it
```

| | Static (`.a`) | Dynamic (`.dylib` on macOS, `.so` on Linux) |
|---|---|---|
| What it is | an archive of `.o` files | a shared executable image loaded at runtime |
| Linked into your binary | yes, code is copied in | no, loaded when the program starts |
| Binary size | larger | smaller |
| Deployment | one self-contained file | must ship the library too |
| Update library | relink | just replace the file |
| Build | `ar rcs libX.a *.o` | `cc -shared -o libX.dylib *.o` |

For this course, static libraries are all you need: `libmat.a` built once, linked into every project.

---

## 8. Makefile fundamentals

`make` reads a `Makefile` describing **targets**, their **prerequisites**, and the **recipe** to build them. It rebuilds a target only if it is missing or older than any prerequisite.

```make
target: prerequisite1 prerequisite2
	recipe command          # MUST be indented with a TAB, not spaces
```

Minimal example, fully explicit:

```make
bin/prog: build/main.o build/matrix.o
	cc build/main.o build/matrix.o -o bin/prog -lm

build/main.o: src/main.c include/matrix.h
	cc -Wall -Wextra -std=c11 -O2 -Iinclude -c src/main.c -o build/main.o

build/matrix.o: src/matrix.c include/matrix.h
	cc -Wall -Wextra -std=c11 -O2 -Iinclude -c src/matrix.c -o build/matrix.o
```

`make bin/prog` (or just `make`, which builds the first target) checks timestamps and runs only the needed recipes. Touch `src/main.c` and only `main.o` and the link step rerun.

### Variables

```make
CC      = cc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -Iinclude
LDLIBS  = -lm

build/main.o: src/main.c include/matrix.h
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o
```

`$(NAME)` expands a variable. `CC`, `CFLAGS`, `LDFLAGS` (linker flags like `-L`), `LDLIBS` (`-l` libs) are the conventional names — `make` even has built-in rules that use them.

### Pattern rules and automatic variables

Writing one rule per `.o` does not scale. A **pattern rule** says "any `build/X.o` is built from `src/X.c` like this":

```make
build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
```

| Automatic variable | Meaning |
|---|---|
| `$@` | the target (`build/main.o`) |
| `$<` | the first prerequisite (`src/main.c`) |
| `$^` | all prerequisites, space-separated, duplicates removed |
| `$*` | the stem matched by `%` (`main`) |

### `.PHONY`, `clean`, `all`

```make
.PHONY: all clean
all: bin/prog
clean:
	rm -rf build bin
```

`clean` is not a file. Without `.PHONY`, if a file named `clean` ever existed in the directory, `make clean` would say "clean is up to date" and do nothing. Mark every non-file target `.PHONY`.

### Functions: `wildcard`, `patsubst`

```make
SRCS := $(wildcard src/*.c)                        # src/main.c src/matrix.c ...
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))      # build/main.o build/matrix.o ...
```

`:=` evaluates immediately (once); `=` re-evaluates every time it is used. Use `:=` for these.

---

## 9. `-MMD` dependency tracking

The pattern rule `build/%.o: src/%.c` does not know that `main.o` also depends on `matrix.h`. Edit the header and `make` will not rebuild `main.o` — stale object, mysterious bugs. Listing headers by hand does not scale either. Instead, let the compiler generate the dependency list:

```make
CFLAGS += -MMD -MP
DEPS   := $(OBJS:.o=.d)
-include $(DEPS)
```

`-MMD` makes `cc -c src/main.c` also write `build/main.d` containing a Make rule:

```make
build/main.o: src/main.c include/matrix.h include/csv.h
```

`-MP` adds empty rules for each header so deleting a header does not break the build. `-include` (with the leading dash) includes those `.d` files if they exist and silently skips them the first time. Now header edits trigger exactly the right rebuilds, forever, with zero maintenance.

---

## 10. A complete Makefile for `src/ include/ build/ bin/`

```make
# ---- toolchain ---------------------------------------------------------
CC       := cc
CFLAGS   := -Wall -Wextra -std=c11 -Iinclude -MMD -MP
LDLIBS   := -lm

# ---- build type: `make` (release) or `make DEBUG=1` ----------------------
ifdef DEBUG
  CFLAGS += -g -O0 -fsanitize=address,undefined
  LDFLAGS += -fsanitize=address,undefined
  BUILD := build/debug
else
  CFLAGS += -O2 -DNDEBUG
  BUILD := build/release
endif

# ---- files -------------------------------------------------------------
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
BIN  := bin/prog

# ---- rules -------------------------------------------------------------
.PHONY: all clean run test

all: $(BIN)

$(BIN): $(OBJS) | bin
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD) bin:
	mkdir -p $@

run: $(BIN)
	./$(BIN)

test: $(BIN)
	./$(BIN) --self-test

clean:
	rm -rf build bin

-include $(DEPS)
```

Things to notice:

- `| bin` and `| $(BUILD)` are **order-only prerequisites**: the directory must exist before the recipe runs, but its timestamp (which changes whenever a file is added) must not trigger rebuilds.
- `make DEBUG=1` selects the sanitizer build into a separate directory so debug and release objects never mix.
- `make -j8` builds up to 8 objects in parallel. Because each `.o` rule is independent, this is safe and roughly 8x faster on a multi-core Mac. (`nproc` is `sysctl -n hw.ncpu` on macOS.)
- `make -n` prints what would run without running it. `make -B` forces a full rebuild. `make -C dir` runs in another directory.
- If you see `*** missing separator. Stop.`, a recipe line is indented with spaces instead of a tab.

### Project layout

```
myproject/
├── Makefile
├── include/          # public headers (.h)
│   ├── matrix.h
│   └── csv.h
├── src/              # implementations (.c), one per header, plus main.c
│   ├── matrix.c
│   ├── csv.c
│   └── main.c
├── tests/            # test programs (optional; add a rule for them)
│   └── test_matrix.c
├── build/            # .o and .d files — generated, git-ignored
└── bin/              # executables — generated, git-ignored
```

Add `build/` and `bin/` to `.gitignore`.

Variant: when `src/` is meant to become a library (`libmat.a`), move `main.c` into `app/` so `$(wildcard src/*.c)` yields only library objects and `main.o` does not end up inside the archive. `example.c`'s `scaffold` command writes exactly that variant.

---

## 11. The bare-bones alternative for tiny projects

You do not need a Makefile for a 2-file experiment:

```
cc -Wall -Wextra -std=c11 -O2 -Iinclude src/*.c -o prog -lm
```

This compiles and links everything in one step. Downsides: recompiles every file every time, and no debug/release switch. Fine for anything under ~5 files or 2000 lines. Graduate to the Makefile when rebuilds start to feel slow or you have more than one executable (e.g. `prog` and `test_matrix`).

---

## 12. Structuring the reusable `matrix` library

This is the module you will link into linear regression, the MLP, the transformer, FDTD, and everything else. Structure it as three files:

```
include/matrix.h      the interface: Matrix struct, all mat_* declarations with ownership comments
src/matrix.c          the implementation; helpers are static
tests/test_matrix.c   a main() that exercises every function with asserts
```

`matrix.h` API sketch (declarations only — you will implement these across the coming projects):

```c
#pragma once
#include <stddef.h>

typedef struct { size_t rows, cols; double *data; } Matrix;

/* creation / destruction — all constructors return ownership; data==NULL on failure */
Matrix mat_zeros(size_t rows, size_t cols);
Matrix mat_ones(size_t rows, size_t cols);
Matrix mat_eye(size_t n);
Matrix mat_rand(size_t rows, size_t cols, unsigned seed);   /* uniform [0,1) */
Matrix mat_copy(const Matrix *m);
void   mat_free(Matrix *m);

/* element access */
static inline double mat_get(const Matrix *m, size_t i, size_t j) { return m->data[i * m->cols + j]; }
static inline void   mat_set(Matrix *m, size_t i, size_t j, double v) { m->data[i * m->cols + j] = v; }

/* operations writing into a caller-provided out (no allocation); return 0 or -1 on shape mismatch */
int mat_matmul(Matrix *out, const Matrix *a, const Matrix *b);   /* out = a @ b */
int mat_add(Matrix *out, const Matrix *a, const Matrix *b);      /* out = a + b */
int mat_transpose(Matrix *out, const Matrix *a);
void mat_scale(Matrix *m, double k);
void mat_apply(Matrix *m, double (*f)(double));                  /* elementwise, in place */

/* reductions / io */
double mat_sum(const Matrix *m);
double mat_frobenius(const Matrix *m);
void   mat_print(const char *name, const Matrix *m);
int    mat_save(const Matrix *m, const char *path);              /* binary; see ../08_file_io */
int    mat_load(Matrix *out, const char *path);
```

`test_matrix.c`:

```c
#include "matrix.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_matmul_identity(void) {
    Matrix a = mat_rand(3, 3, 42), i = mat_eye(3), out = mat_zeros(3, 3);
    assert(mat_matmul(&out, &a, &i) == 0);
    for (size_t k = 0; k < 9; k++) assert(fabs(out.data[k] - a.data[k]) < 1e-12);
    mat_free(&a); mat_free(&i); mat_free(&out);
}
int main(void) {
    test_matmul_identity();
    /* ... one function per property ... */
    printf("all matrix tests passed\n");
    return 0;
}
```

Build the test with its own target: `bin/test_matrix: tests/test_matrix.c $(BUILD)/matrix.o`. Run it under `make DEBUG=1 test` so every test also runs under ASan. When a later project needs `mat_softmax_rows` or `mat_argmax`, you add it here, test it here, and every project gets it.

`static inline` in the header for `mat_get`/`mat_set`: the body *is* in the header, but `static` gives each translation unit its own private copy (no duplicate symbol), and `inline` tells the compiler it is fine to expand it at the call site. Use it only for tiny, hot functions.

---

## Gotchas and undefined behavior

- **Recipe lines indented with spaces** → `missing separator`. Must be a tab. Configure your editor to show tabs in Makefiles.
- **Function body in a header without `static inline`** → duplicate symbol at link time.
- **`int g;` in a header** defines it in every including file. Use `extern int g;` + one definition.
- **Header edited, `.o` not rebuilt** → silent stale-object bugs (a struct layout change is the worst case: two files disagree on offsets, which is UB). `-MMD -MP` fixes it; until then, `make -B`.
- **Mismatched declaration and definition** (header says `int f(int)`, `.c` defines `int f(long)`) compiles each file fine and produces UB at the call. Including your own header first in the `.c` turns this into a compile error.
- **Calling a function with no visible declaration** was allowed in C89 (implicit `int`) and is an error in C11 with clang 21 — good. If you see `call to undeclared function`, you forgot an `#include`.
- **`static` function declared in header** → "undefined symbol" for everyone else, or a `defined but not used` warning in each file.
- **Linking static libs before the objects that use them** → undefined symbols. Objects first, then `-l` flags.
- **Mixing debug and release objects** (ASan and non-ASan) in one link → runtime crashes. Separate `build/debug` and `build/release`.
- **`make` uses `/bin/sh`**, not zsh. Bashisms in recipes may fail.

---

## Common mistakes checklist

- [ ] Every header has an include guard or `#pragma once` and includes what it uses.
- [ ] Headers contain declarations, types, macros, `static inline` — no non-inline bodies, no variable definitions.
- [ ] Every non-interface function in a `.c` is `static`.
- [ ] Globals are `extern` in the header and defined in exactly one `.c` (and are rare).
- [ ] Each `.c` includes its own header first.
- [ ] Makefile recipes use tabs; `.PHONY` lists every non-file target.
- [ ] `-MMD -MP` and `-include $(DEPS)` are present so header edits rebuild correctly.
- [ ] Debug and release builds go to separate directories.
- [ ] `build/` and `bin/` are in `.gitignore`.
- [ ] `-lm` and other `-l` flags come after the object files on the link line.

---

## You can move on when...

- You can explain what a translation unit is and why `main.c` compiles without seeing `matrix.c`.
- Given an "undefined symbol" or "duplicate symbol" error, you can name the two or three likely causes and fix it in under a minute.
- You can write `matrix.h` with a guard, correct includes, and a `static inline` accessor, and `matrix.c` with `static` helpers.
- You can write the Makefile in Section 10 from memory (or close) and explain every line, including `$@ $< $^`, order-only prerequisites, and `-include $(DEPS)`.
- You can build a `libmat.a` with `ar rcs` and link a program against it with `-L -l`.
- You know when the one-line `cc src/*.c` build is enough and when it is not.

Next: `../10_data_structures/lesson.md` — hash tables, heaps, and trees, each built as a module you can drop into `src/`.
