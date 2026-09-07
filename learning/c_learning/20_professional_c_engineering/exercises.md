# Chapter 20 — Exercises

This chapter's exercises build **one library**, not ten programs. Create a folder `matrixlib/` in this
directory and grow it across 20.1–20.10:

```
matrixlib/
  include/matrix.h     src/matrix.c     src/parse.c
  tests/               fuzz/            python/
  Makefile             matrix.pc.in     .clang-format     .clang-tidy
  LICENSE              README.md        CHANGELOG.md      .github/workflows/ci.yml
```

Seed it from `example.c`: split the header section (lines 42–161) into `include/matrix.h` and the
rest into `src/`. Default compile: `cc -Wall -Wextra -std=c11 -O2 -Iinclude`. From 20.6 onward the
strict warning set from lesson §11.1 plus `-Werror` is assumed. Every exercise that produces a
library must end with `nm -gU` output you have actually looked at.

Note: `clang-tidy`, `clang-format`, `cppcheck`, `scan-build`, `doxygen`, and libFuzzer are **not**
installed by default on macOS — `brew install llvm cppcheck doxygen graphviz` first, and use
`$(brew --prefix llvm)/bin/<tool>` for the LLVM ones (lesson §8.1, §9).

---

### 20.1 — **Split the monolith into a real library**

Turn `example.c` into the folder layout above. `include/matrix.h` gets the public API only —
declarations, the opaque `typedef struct mat mat_t;`, the enums, the `MAT_API` macro, the version
macros, include guards, and `extern "C"` guards. Everything else goes to `src/`, and every function
not in the header becomes `static`. Write a `Makefile` with targets `all`, `lib` (both `.a` and the
platform shared library), `test`, `clean`, and `install PREFIX=…`. Then prove the split is honest:
compile `include/matrix.h` on its own (`cc -fsyntax-only -x c include/matrix.h`) to show it is
self-contained, and diff `nm -gU` on your `.dylib` against the seventeen symbols in lesson §1.8.

Example:
```
$ make lib && nm -gU build/libmatrix.dylib | wc -l
      17
$ cc -fsyntax-only -x c include/matrix.h && echo "header is self-contained"
header is self-contained
```

<details><summary>Hint</summary>
A self-contained header includes what it uses: `stddef.h` for `size_t` is almost certainly needed.
The `-x c` flag is what lets you compile a `.h` file directly. If `nm -gU` shows more than 17
symbols you forgot `-fvisibility=hidden` or forgot a `static`; if it shows fewer, a `MAT_API` is
missing.
</details>

### 20.2 — **Design review: break the API on purpose**

Make five separate copies of your library, each with one deliberate defect, and write a
`BREAKAGE.md` recording for each: whether it breaks **API** or **ABI**, what the caller sees, and
how you would detect it. (a) Move `struct mat`'s definition into the header and add a field.
(b) Insert a new value in the middle of `mat_status`. (c) Change `mat_rows(const mat_t *)` to
`mat_rows(mat_t *)`. (d) Change `mat_matmul_raw`'s `size_t` parameters to `int`. (e) Remove
`mat_frobenius`. For (a), (b) and (e), build a consumer against the *old* library, then swap in the
new `.dylib` **without recompiling the consumer**, and record what actually happens at run time.

Example:
```
(b) enum renumber: ABI break. Consumer compiled MAT_E_SHAPE==2, new lib returns 3 -> caller
    prints "invalid argument" for a shape error. No crash, no warning. Detect: never insert.
(a) transparent struct +1 field: ABI break. Consumer's stack `mat_t m;` is 8 bytes too small ->
    ASan reports stack-buffer-overflow in mat_create. Detect: opaque handle makes it impossible.
```

<details><summary>Hint</summary>
"Swap without recompiling" is the whole point — build `use.c` once against v1, then rebuild only
the library and rerun the *same* executable. `-Wl,-rpath,@executable_path` plus dropping the new
`.dylib` next to the binary is the quickest setup. For (c) and (d), the consumer will not even
compile: that is your evidence for "API break".
</details>

### 20.3 — **Ownership audit**

Add a Doxygen block to every public function in `include/matrix.h` that names, for each pointer
parameter and the return value, exactly one of **OWNED**, **BORROWED**, or **TRANSFERRED**, plus
what invalidates it. Then write `tests/test_ownership.c` proving the claims: `mat_wrap` does not
free the caller's buffer (`mat_destroy` then read the buffer — under ASan, cleanly);
`mat_data_const` returns a pointer into live storage that a subsequent `mat_destroy` invalidates
(demonstrate the dangling read under ASan, in a subprocess so the suite survives); `mat_create`
returns NULL rather than crashing when allocation fails. For the last one, override `malloc` with a
failure-injecting wrapper and confirm every `MAT_E_ALLOC` path is reached.

Example:
```
test_ownership: wrap_borrows_buffer ok   destroy_does_not_free_wrapped ok
                data_const_invalidated_by_destroy ok (ASan heap-use-after-free in child, expected)
malloc failure injection: 6/6 MAT_E_ALLOC paths reached
```

<details><summary>Hint</summary>
On macOS you can interpose `malloc` by simply defining it in your test TU for a static link, or use
a `mat_set_allocator()` hook you add to the API — the hook is the better design and makes the test
portable. Fail the Nth allocation, sweep N from 0 upward until no failure path remains untriggered;
that loop is the coverage measurement.
</details>

### 20.4 — **Three builds, two platforms, one rpath maze**

Build `libmatrix.a`, `libmatrix.dylib` (with `-install_name @rpath/libmatrix.1.dylib`,
`-compatibility_version 1.0.0`, `-current_version 1.2.0`), and — if you have Docker or a Linux box —
`libmatrix.so.1.2.0` with its two symlinks. Then reproduce and fix the loader failure four ways:
link a consumer with no rpath and capture the exact `dyld` message; fix it with `-Wl,-rpath` at link
time; fix a *second, already-built* copy with `install_name_tool -add_rpath`; and fix a third with
`DYLD_LIBRARY_PATH`. Move the library into `../lib/` and make the binary find it from *anywhere*
using `@executable_path/../lib`. Record `otool -L`, `otool -D`, and `otool -l | grep -A2 LC_RPATH`
for every stage in a `RPATH.md`.

Example:
```
no rpath : dyld[3477]: Library not loaded: @rpath/libmatrix.dylib ... Reason: no LC_RPATH's found
link-time: -Wl,-rpath,@executable_path/../lib  -> runs from /, from ~, from anywhere
patched  : install_name_tool -add_rpath @executable_path use_norpath -> runs
env      : DYLD_LIBRARY_PATH=$PWD/far ./use_far -> runs (debug only: stripped by SIP)
```

<details><summary>Hint</summary>
`otool -D` shows the library's own install name; `otool -L` shows what the consumer recorded. They
must match, because the consumer copies the former at link time. If you rebuild the library with a
new `-install_name`, you must relink the consumer — or patch it with `install_name_tool -change`.
Remember `-change` never touches a library's own id (lesson §3.2).
</details>

### 20.5 — **pkg-config, install, and a consumer project**

Write `matrix.pc.in` with `@PREFIX@` and `@VERSION@` placeholders, have `make install
PREFIX=$PWD/stage` substitute them and install headers, both libraries, and
`$PREFIX/lib/pkgconfig/matrix.pc`. Then create a **separate** project directory that knows nothing
about your source tree and builds against the staged install using only
`PKG_CONFIG_PATH=…/stage/lib/pkgconfig` plus `pkg-config --cflags --libs matrix`. Make it work for
both the shared and the static case (`--static`, and check `Libs.private: -lm` is what makes the
static link succeed). Add a `pkg-config --atleast-version` guard to the consumer's Makefile that
fails the build with a readable message.

Example:
```
$ PKG_CONFIG_PATH=$PWD/stage/lib/pkgconfig pkg-config --cflags --libs --static matrix
-I/…/stage/include -L/…/stage/lib -lmatrix -lm
$ make -C consumer && ./consumer/app
success: 19 22 43 50  (lib 1.2.0)
$ make -C consumer MATRIX_MIN=9.0
Makefile:4: *** matrix >= 9.0 required, found 1.2.0.  Stop.
```

<details><summary>Hint</summary>
`sed -e 's|@PREFIX@|$(PREFIX)|g' matrix.pc.in > …` is the whole substitution step. Drop `-lm` from
`Libs` into `Libs.private` and try the static link without `--static` to see the undefined `_sqrt`
error that `Libs.private` exists to prevent. `$(error …)` inside an `ifeq` is how a Makefile fails
loudly.
</details>

### 20.6 — **Testing at scale**

Split your suite into `tests/test_api.c`, `tests/test_property.c`, `tests/test_parse.c` with a
shared `tests/harness.h` (the 40-line framework), and a `make test` that runs all three and fails on
any non-zero exit. Add: **error-path tests** covering every `mat_status` value at least once and
asserting `mat_strerror` is non-NULL for every enum value plus one out-of-range; **property tests**
for `A(BC)==(AB)C`, `A·I==A`, `(AB)ᵀ==BᵀAᵀ`, `A(B+C)==AB+AC` and `‖A‖_F==‖Aᵀ‖_F` on 200 random
shapes with dimensions 1–7, seeded from `MAT_SEED` (defaulted, printed, and printed again on
failure); a **golden test** hashing a fixed 32×32 product; and a **golden file** test comparing
parser output against `tests/golden/rows.txt` with `diff`. Then measure coverage with
`-fprofile-instr-generate -fcoverage-mapping` and list every uncovered line with a one-line
justification.

Example:
```
$ make test
test_api       23 tests, 512 checks, 0 failed
test_property  5 identities x 200 trials (MAT_SEED=20240906), 0 failed
test_parse     31 tests, 0 failed   golden/rows.txt: identical
$ xcrun llvm-cov report ./tests/test_api -instr-profile=t.profdata
Lines: 91.4%   -- uncovered: 6 OOM branches (covered by 20.3 injection), 2 unreachable defaults
```

<details><summary>Hint</summary>
Tolerances, never `==`: `fabs(a-b) <= 1e-9 * (1.0 + fabs(a))`. Print the seed *before* running so a
crash still tells you how to replay. For `-Wswitch-enum` to help you, write the `mat_strerror`
switch with no `default:` — then adding a status without a message is a compile error, and the
"unknown" fallback lives after the switch.
</details>

### 20.7 — **The full quality gate: fuzz, analyze, format, CI**

Four parts, all wired into one `make check`.
(a) Write `fuzz/fuzz_parse.c` with `LLVMFuzzerTestOneInput` around `mat_parse_csv_row`, asserting
your invariants (`count <= cap`, `count == 0` on failure, no NaN unless the input said `nan`). Build
with Homebrew LLVM (`-fsanitize=fuzzer,address,undefined`), run 60 s, and turn every crash and every
input in the minimised corpus into a case in `tests/test_parse.c`. Add a `fuzz/csv.dict`.
(b) Write `.clang-tidy` (start from `-*`, add `bugprone-*,cert-*,clang-analyzer-*,performance-*`)
and get to zero findings or a documented `// NOLINT(check)` with a reason. Run `scan-build
--status-bugs` and `cppcheck --error-exitcode=1` too, and record what each of the three found that
the others did not.
(c) Write `.clang-format`, reformat the tree in one isolated commit, add `.git-blame-ignore-revs`,
and add a `--dry-run --Werror` gate.
(d) Write `.github/workflows/ci.yml` with the macOS+Linux × clang+gcc × sanitizer matrix, a
format/analyze job, and a Linux-only fuzz job that uploads `crash-*` on failure.

Example:
```
fuzz: #16384 pulse cov: 74 ft: 118 corp: 23/94b exec/s: 21460
      crash-a1b2… -> minimised to "1e999," -> now tests/test_parse.c case 27
clang-tidy 0 findings (2 NOLINT: bugprone-easily-swappable-parameters on mat_matmul_raw m,k,n)
scan-build found 1 that tidy missed: dead store in parse_field on the MAT_E_RANGE path
cppcheck  found 1 that both missed: unreadVariable in the golden-hash helper
CI: 6 jobs green in 4m12s
```

<details><summary>Hint</summary>
Apple clang has no libFuzzer — `brew install llvm` and use `$(brew --prefix llvm)/bin/clang`, or run
that job only on Linux (the CI YAML in lesson §12 does exactly this). A fuzz target must be
deterministic: no `getenv`, no time, no files. Commit `fuzz/corpus/` — it is the seed for the next
run and makes fuzzing incremental.
</details>

### 20.8 — **Ship your ML kernels to Python: `libmlkernels`** *(ties to: ML from scratch)*

Build a second library, `mlkernels`, with the same professional structure, exposing the primitives a
from-scratch MLP needs — and drive it entirely from Python. C side: `ml_gemm` (your chapter-19
blocked kernel, with `alpha`/`beta` and transpose flags), `ml_relu_forward`/`ml_relu_backward`,
`ml_softmax_rows`, `ml_cross_entropy_loss` (returning loss and writing the gradient), `ml_sgd_step`,
and `ml_layernorm_forward`. Every one takes plain `float *`/`double *` and `size_t` dimensions — no
handles in the hot path (lesson §5.5). Python side: a `ctypes` module with full
`restype`/`argtypes`, a `_ptr()` guard that refuses non-contiguous or wrong-dtype arrays, and status
codes raised as exceptions. Then **train MNIST** (or two-moons if you want it offline) with the
forward/backward pass in C and only the loop in Python, and produce a table comparing each kernel
against its NumPy equivalent: max absolute error, and wall-clock speedup at three sizes. Verify the
backward passes with numerical gradients (central differences, `1e-5`).

Example:
```
kernel            max|err| vs NumPy    64      512      2048
ml_gemm                    0.0        0.8x     1.9x     2.4x
ml_softmax_rows        1.1e-16        1.3x     2.1x     2.2x
ml_relu_backward           0.0        0.9x     3.1x     3.4x
gradcheck: W1 2.7e-09  b1 4.1e-10  W2 1.9e-09  b2 8.8e-11   (all < 1e-6)
MNIST 784-128-10, 10 epochs: C-kernels 18.3s / NumPy 41.7s / test acc 0.9761 vs 0.9758
```

<details><summary>Hint</summary>
Structure the API so one Python call does a whole layer, not one element — the 1–2 µs ctypes
overhead is irrelevant at 512×512 and fatal per element. `ml_gemm` beating NumPy at small sizes and
losing at large ones is the expected shape (NumPy dispatches to Accelerate above a threshold); the
interesting number is the *crossover*, so plot it. For softmax, subtract the row max before `exp` or
your error column will show `inf` rather than `1e-16`.
</details>

### 20.9 — **A simulation library with a versioned checkpoint format** *(ties to: physics sims)*

Package your N-body or heat-equation solver as `libsim` with a genuinely stable API, then add the
thing simulations always need and always get wrong: a **binary checkpoint format**. Design a header
struct with a magic number, a format version, an endianness marker, the grid dimensions, the
timestep, the simulated time, and a CRC32 of the payload. Write `sim_save`/`sim_load` where `load`
accepts *every* version you have ever written (v1 with no CRC, v2 with CRC, v3 with an extra field)
and returns a specific status for each way it can fail: `SIM_E_MAGIC`, `SIM_E_VERSION`,
`SIM_E_ENDIAN`, `SIM_E_CRC`, `SIM_E_TRUNCATED`. Fuzz the loader — a checkpoint reader is a parser
handling untrusted input, and it must never crash on a corrupt file. Then expose the whole thing to
Python via ctypes so you can run the sim from C and plot each checkpoint with matplotlib, and add a
golden test that a v1 file written in 2024 still loads and produces the same energy to 1e-12.

Example:
```
$ ./sim --steps 10000 --checkpoint-every 1000 out/
wrote out/step_09000.ckpt (v3, 4.2 MB, crc 0x8a41f2b1)
$ ./sim_load out/step_09000.ckpt && python3 python/plot.py out/
loaded v3: 512x512, t=9.000, E=-1.4142135624e+03
$ ./fuzz_ckpt corpus -max_total_time=120
#48211 DONE cov: 91 ft: 143  0 crashes   (3 found and fixed: truncated header, ne=0, CRC on empty)
golden: checkpoints/v1_2024.ckpt loads, E matches recorded -1.4142135624e+03 (diff 0.0e+00)
```

<details><summary>Hint</summary>
Never `fwrite(&struct, sizeof struct, 1, f)` — padding and alignment are not portable, and adding a
field silently changes the layout. Serialise field by field to a fixed-width little-endian
representation, and store `sizeof` nothing. Validate dimensions *before* multiplying them into an
allocation size: `ne_x * ne_y * sizeof(double)` overflows for attacker-chosen dimensions, which is
exactly the bug the fuzzer will find first.
</details>

### 20.10 — **A competitive-programming toolkit as a real library** *(ties to: competitive programming)*

Turn the ad-hoc snippets you retype every contest into `libcp`: fast I/O (`cp_read_int`,
`cp_read_all` with a `mmap`ed or block-buffered reader), sorting and binary search on `int64_t`,
`cp_gcd`/`cp_powmod`/`cp_modinv`, a union-find, a segment tree, a `cp_sieve`, and a hash map with
`int64_t` keys. Two deliverables that pull in the whole chapter. **First**, an *amalgamation*: a
`make amalgamate` target that concatenates the library into a single self-contained `cp.h` (all
internals `static`, guarded by `CP_IMPLEMENTATION`, like `stb` — lesson §15), because a contest
judge takes one file. Prove it works by compiling a solution with only `cp.h`. **Second**, a
**differential test harness**: for each data structure, run 100 000 random operations against a
brute-force reference and diff, plus a fuzz target for the input parser (judges feed you malformed
input more often than you think). Benchmark `cp_read_all` against `scanf` and `std::cin` on 10^6
integers, and check the amalgamated build under `-fsanitize=address,undefined` with `-Werror` on the
strict warning set.

Example:
```
$ make amalgamate && wc -l build/cp.h && cc -std=c11 -O2 -Wall -Wextra -Werror sol.c -o sol
    1843 build/cp.h
$ ./difftest
union_find   100000 ops vs brute force: identical
segtree      100000 ops (point set, range min/sum) vs brute force: identical
hashmap      100000 ops vs sorted-array reference: identical   (1 bug found: resize at load 1.0)
$ ./bench_io < 1e6.txt
scanf 412 ms   cp_read_all 19 ms   (21.7x)   fgets+strtol 78 ms
$ ./fuzz_read corpus -max_total_time=60
#31002 DONE  0 crashes  (2 fixed: leading '+', integer overflow on "99999999999999999999")
```

<details><summary>Hint</summary>
Amalgamation is easier if every file already has the shape `#include "cp_internal.h"` + statics: the
concatenation step is `cat` plus stripping the internal includes with `sed`. The `stb` pattern is
declarations always visible, definitions behind `#ifdef CP_IMPLEMENTATION`, so one TU defines it and
the rest just declare. For the differential harness, the brute force must be *obviously* correct —
an O(n) linear scan for the segment tree query — because that is the whole value of the technique.
</details>
