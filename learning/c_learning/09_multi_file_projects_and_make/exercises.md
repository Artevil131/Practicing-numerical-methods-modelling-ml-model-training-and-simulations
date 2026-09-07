# Chapter 09 — Exercises

This chapter's exercises produce *projects*, not single files. Create each as a subdirectory `ex09_K/` in this folder (e.g. `ex09_1/`), containing whatever files the exercise asks for. Where an exercise is a single-file experiment, name it `ex09_K.c` as usual.

Default compile flags for every `.c`: `-Wall -Wextra -std=c11 -O2` (plus `-Iinclude` when you have an `include/` directory), link with `-lm`.

---

## 09.1 — **Two-file hello**

Create `ex09_1/` with `greet.h` (include guard, declaration of `void greet(const char *name);`), `greet.c` (definition; prints `Hello, <name>!`), and `main.c` (calls `greet(argv[1])`). Compile with the one-liner `cc -Wall -Wextra -std=c11 -O2 -o hello greet.c main.c`. Then compile the *wrong* way, `cc ... -o hello main.c`, paste the linker error into a comment in `main.c`, and explain in one sentence why the compiler was happy but the linker was not.

Example: `./hello Ada` → `Hello, Ada!`

<details><summary>Hint</summary>

- The error will mention `_greet` and `main.o`.
- The compiler only needed the declaration; the linker needed the definition.
</details>

---

## 09.2 — **Provoke and fix the duplicate symbol**

In `ex09_2/`, create `counter.h` that (wrongly) contains `int g_count = 0;` and a function body `void bump(void) { g_count++; }`. Include it from `a.c` and `b.c` (each with one function), plus `main.c`. Compile all three and paste the `duplicate symbol` error into a comment. Then fix it properly: `extern int g_count;` and a declaration in the header, definitions in `counter.c`. Finally, make `bump` a `static inline` function in the header instead and confirm that *that* form is allowed, and explain why.

Example: after fixing, `./prog` → `count = 2`

<details><summary>Hint</summary>

- `static inline` gives each translation unit a private copy; no exported symbol, no clash.
- A plain `int g_count;` (no initializer) in a header may *appear* to link on some toolchains (tentative definitions) — with `= 0` it will not.
</details>

---

## 09.3 — **Object files by hand**

Take `ex09_1` and rebuild it step by step: `cc -c greet.c -o greet.o`, `cc -c main.c -o main.o`, `cc greet.o main.o -o hello`. Run `nm greet.o` and `nm main.o` and, in a `NOTES.md` inside the folder, explain what `T _greet` in one and `U _greet` in the other mean. Then make `greet` `static` in `greet.c`, rebuild the object, run `nm` again, and record what changed and what the link step now says.

Example `nm` excerpt:
```
greet.o: 0000000000000000 T _greet
main.o:                   U _greet
```

<details><summary>Hint</summary>

- `T` = defined in the text (code) section, exported. `U` = undefined, expected from elsewhere. `t` (lowercase) = defined but local (static).
- `man nm`.
</details>

---

## 09.4 — **Static library**

In `ex09_4/`, split `vec_dot`, `vec_norm`, `vec_axpy` into `vec.h` / `vec.c`, and `mat_zeros`, `mat_free`, `mat_matvec` into `matrix.h` / `matrix.c`. Build both into `libnum.a` with `ar rcs`. Write `main.c` that computes `‖A·x‖` for a small A and x, and link it with `cc main.c -L. -lnum -lm -o prog`. Then run `ar t libnum.a` to list the archive's contents. Show what happens if you put `-lnum` *before* `main.c` on the link line (it may still work on macOS — note that, and note that on Linux it would fail).

Example: `./prog` → `||A x|| = 5.477226`

<details><summary>Hint</summary>

- `ar rcs libnum.a vec.o matrix.o` — `r` insert, `c` create, `s` index.
- Headers each include what they use (`<stddef.h>`).
</details>

---

## 09.5 — **First Makefile**

Write a Makefile for `ex09_4` with: `CC`, `CFLAGS`, `LDLIBS` variables; explicit rules for `vec.o`, `matrix.o`, `main.o`, `libnum.a`, and `prog`; `.PHONY: all clean`. Verify: `make` builds everything; `make` again prints `make: Nothing to be done` (or `'prog' is up to date`); `touch vec.c && make` rebuilds only `vec.o`, `libnum.a`, and `prog`. Record the three outputs in a comment at the top of the Makefile.

Example: after `touch vec.c`, `make` shows exactly three commands.

<details><summary>Hint</summary>

- Tabs, not spaces, before recipes.
- `prog` depends on `main.o libnum.a`; `libnum.a` depends on `vec.o matrix.o`.
</details>

---

## 09.6 — **Pattern rules and header dependencies**

Rewrite the Makefile from 09.5 with a single pattern rule `%.o: %.c` using `$<` and `$@`, and `$^` in the link rule. Then edit `vec.h` (add a comment), run `make`, and observe that nothing rebuilds — the bug. Add `-MMD -MP` to `CFLAGS` and `-include $(wildcard *.d)` to the Makefile, run `make -B` once, edit `vec.h` again, run `make`, and confirm that `vec.o` and `main.o` (both include `vec.h`) rebuild but `matrix.o` does not. Look inside one of the generated `.d` files and paste it into a comment.

Example `vec.d`:
```
vec.o: vec.c vec.h
vec.h:
```

<details><summary>Hint</summary>

- `-MP` is what produces the empty `vec.h:` rule.
- `-include` with the dash ignores missing files on the first build.
</details>

---

## 09.7 — **Full layout with debug/release**

Create `ex09_7/` with `src/`, `include/`, `build/`, `bin/` and the complete Makefile from the lesson (Section 10), adapted so the binary is `bin/stats`. Put a `stats` program in it (reads numbers from stdin, prints mean and std, using a `stats.h`/`stats.c` module). Confirm `make` produces `build/release/*.o` and `bin/stats`; `make DEBUG=1` produces `build/debug/*.o` and an ASan binary; `make clean` removes both; `make -j4` works. Add a `.gitignore` for `build/` and `bin/`.

Example: `echo "1 2 3 4" | make -s run` → `mean=2.5 std=1.118034`

<details><summary>Hint</summary>

- Order-only prerequisites (`| $(BUILD)`) create the directories.
- `make -s` silences the command echo so `run` output is clean.
</details>

---

## 09.8 — **The `matrix` library, for real** *(ML)*

Create `ex09_8/` as the first version of the matrix library you will reuse: `include/matrix.h` (the API sketched in the lesson, with ownership comments), `src/matrix.c` implementing at least `mat_zeros, mat_eye, mat_rand, mat_copy, mat_free, mat_matmul, mat_transpose, mat_add, mat_scale, mat_apply, mat_sum, mat_print`, and `tests/test_matrix.c` with `assert`-based tests: identity multiply, `(A^T)^T == A`, `(AB)^T == B^T A^T` within `1e-12`, shape-mismatch returns -1. Makefile targets: `all` (builds `build/.../libmat.a`), `test` (builds and runs `bin/test_matrix`), `clean`. `make DEBUG=1 test` must pass under ASan.

Example: `make test` → `all matrix tests passed`

<details><summary>Hint</summary>

- `mat_rand` with a seed and your own LCG/xorshift keeps tests deterministic.
- Tests that allocate must free — that is what `DEBUG=1` checks.
</details>

---

## 09.9 — **Linear regression using the library** *(ML)*

Create `ex09_9/` that links against `../ex09_8`'s `libmat.a` and headers (`-I../ex09_8/include -L../ex09_8/build/release -lmat`; add a Makefile rule that invokes `make -C ../ex09_8` first). Read a CSV of `x1,...,xk,y` rows (reuse your Chapter 08 CSV reader as a `csv.h`/`csv.c` module in `src/`), build `X` with a bias column, compute `X^T X` and `X^T y` with the library, and solve the `(k+1)x(k+1)` system with Gaussian elimination (write `linsolve.h`/`linsolve.c`; partial pivoting). Print the weights.

Example: for data generated from `y = 3 + 2*x1 - x2` (no noise) → `w = [3.000000, 2.000000, -1.000000]`

<details><summary>Hint</summary>

- Three modules, three headers, one `main.c`: this is what the Makefile is for.
- Generate the CSV with a 5-line Python script.
</details>

---

## 09.10 — **Simulation frame writer as a module** *(sim)*

Create `ex09_10/` with modules `image.h`/`image.c` (PGM/PPM writers and the blue-white-red colormap from Chapter 08), `field.h`/`field.c` (a 2D `double` grid with `field_alloc/free/get/set` and one explicit 2D heat-diffusion step), and `main.c` that runs 200 steps on a 128x128 grid with a hot square in the middle, writing `frames/frame_%04d.ppm` every 5 steps. Makefile: `all`, `run` (creates `frames/` first), `clean` (also removes `frames/`), `video` (runs `ffmpeg -framerate 20 -i frames/frame_%04d.ppm -pix_fmt yuv420p out.mp4` if ffmpeg is installed). Every module is `static`-clean: `nm build/release/image.o` shows only the public functions as `T`.

Example: `make run` → 40 frames; `make video` → `out.mp4` showing the square diffusing.

<details><summary>Hint</summary>

- `field.c` needs no I/O; `image.c` needs no math; `main.c` glues them. If a module needs the other's header, ask whether the dependency is real.
- Stability: `alpha <= 0.25` for the 2D explicit scheme.
</details>
