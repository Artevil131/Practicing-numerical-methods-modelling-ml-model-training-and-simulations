# Chapter 13 — Debugging, Testing, and Performance

## What you'll be able to do after this chapter

- Treat compiler warnings as bugs, use `assert` correctly, and print debug output that tells you where it came from.
- Drive `lldb` to break, step, inspect variables, and read a backtrace after a crash.
- Run AddressSanitizer / UndefinedBehaviorSanitizer and `leaks`, and read their reports.
- Write a 30-line unit-test framework and use it to TDD a `Matrix` library, including gradient checking.
- Time code correctly (monotonic clock, warmup, repeats, defeating the optimizer) and explain results using the memory hierarchy.
- Run the matmul loop-order experiment, see a 5-10x difference from cache behavior alone, and know which compiler flags and code patterns unlock auto-vectorization.

## Why this matters for ML / numerics / sims

A from-scratch MLP has a dozen places to get an index wrong, and the symptom is "accuracy stuck at 10%" — not a crash. Gradient checking is the one test that catches backward-pass bugs; sanitizers catch the off-by-one that silently reads the next row. Then performance: matrix multiply is 90% of the runtime of an MLP or transformer, and the naive triple loop is 10x slower than the same loop with two lines swapped, purely because of cache lines. N-body and FDTD are the same story — memory layout decides speed. You can't write fast code without measuring, and you can't measure without knowing how the compiler and the clock lie to you.

---

## 1. Warnings are errors: `-Wall -Wextra -Werror`

Every warning in this course's flags corresponds to a real bug class. `-Werror` turns them into compile failures so you cannot ignore them.

```c
int f(int n) {
    int sum;                          // -Wuninitialized (at -O2, -Wsometimes-uninitialized)
    for (unsigned i = 0; i < n; i++)  // -Wsign-compare: n is signed, i unsigned
        sum += i;
    return sum;
}
// example.c:3: warning: comparison of integers of different signs
// example.c:4: warning: variable 'sum' is uninitialized when used here
```

Useful extras: `-Wshadow` (inner variable hides outer), `-Wconversion` (implicit narrowing — noisy but finds `size_t → int` truncation), `-Wdouble-promotion` (accidental `float → double`), `-Wpedantic` (non-standard extensions). `-Wno-unused-parameter` when a callback interface forces unused args (or write `(void)arg;`).

Python equivalent: there is no compile step; the closest is `mypy --strict`. Treat `-Werror` as the type checker you actually have.

## 2. `assert` and `NDEBUG`

```c
#include <assert.h>
double mat_get(const Matrix *m, size_t i, size_t j) {
    assert(m != NULL);
    assert(i < m->rows && j < m->cols);   // preconditions: caller's bug if false
    return m->data[i * m->cols + j];
}
// On failure: Assertion failed: (i < m->rows && j < m->cols), function mat_get, file m.c, line 5.
// then abort() — a core dump / lldb stop right at the bug.
```

Rules:
- `assert` is for *programmer errors* (violated invariants, impossible states), not for runtime errors like "file not found" — those need real error handling (`../14_preprocessor_and_c_idioms/lesson.md`).
- Compile with `-DNDEBUG` and every `assert` vanishes. So **never put side effects in an assert**: `assert(vec_push(&v, &x) == 0);` does nothing in release builds.
- Assert shape compatibility before every matrix op: `assert(a->cols == b->rows)`. This one line converts "garbage output" into "crash at the exact line".

Python equivalent: `assert` in Python is the same idea (and also disabled with `python -O`).

## 3. printf debugging done right

Print to `stderr` (unbuffered, separate from program output you might be piping), include *where*, and make it removable with one flag:

```c
#include <stdio.h>
#ifdef DEBUG
#  define DBG(fmt, ...) fprintf(stderr, "[%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#  define DBG(fmt, ...) ((void)0)
#endif

// DBG("loss=%.6f step=%d", loss, step);
// -> [train.c:88 train_step] loss=2.302585 step=0      (compiled with -DDEBUG)
// -> nothing                                             (without)
```

`##__VA_ARGS__` (a GNU/clang extension, fine on macOS) swallows the comma when there are no variadic args so `DBG("here")` works. Tips: print array *summaries* (min/max/mean/first 3/`isnan` count), not whole matrices; print `%p` of pointers when chasing aliasing; flush with `fflush(stderr)` isn't needed (stderr is unbuffered) but `stdout` is line-buffered to a terminal and fully buffered to a pipe — a crash can eat your last `printf("got here\n")` if it went to `stdout` through a pipe. Always debug through `stderr`.

## 4. `lldb` on macOS

Compile with `-g -O0` (debug info, no optimization so variables aren't reordered/eliminated). A worked session on a program that crashes in a matrix multiply:

```
$ cc -g -O0 -Wall -Wextra -std=c11 -o prog prog.c -lm
$ lldb ./prog
(lldb) run                          # start; stops on crash or breakpoint
Process stopped: EXC_BAD_ACCESS (code=1, address=0x0)
    frame #0: prog`mat_mul(a=0x..., b=0x..., out=0x0) at prog.c:42
   41       for (size_t j = 0; j < b->cols; j++)
-> 42           out->data[i * out->cols + j] = s;     # out is NULL
(lldb) bt                           # backtrace: who called us?
  frame #0: mat_mul at prog.c:42
  frame #1: forward at prog.c:97
  frame #2: main at prog.c:130
(lldb) frame select 1               # move up to forward()
(lldb) print layer->out             # (Matrix *) 0x0000000000000000   <- never allocated
(lldb) print *a                     # (Matrix) (rows = 4, cols = 3, data = 0x6000...)
(lldb) print a->data[0]@6           # first 6 elements as an array
(lldb) quit
```

Core commands:

| Command | Meaning |
|---|---|
| `run` / `r` (args after) | start the program |
| `breakpoint set -n mat_mul` / `b mat_mul` | break at function entry |
| `b prog.c:42` | break at file:line |
| `b prog.c:42 -c 'i == 3'` | conditional breakpoint |
| `next` / `n` | step over one line |
| `step` / `s` | step into a call |
| `finish` | run to the end of the current function |
| `continue` / `c` | resume |
| `print expr` / `p expr` | evaluate and print (`p m->rows`, `p *m`, `p arr[2]@5`) |
| `frame variable` / `fr v` | all locals in this frame |
| `bt` | backtrace |
| `up` / `down` | move between frames |
| `watchpoint set variable x` / `watch set var x` | stop when `x` changes (hardware watchpoint — finds "who overwrote my value") |
| `memory read -fx -s8 -c4 ptr` | dump 4 8-byte words at `ptr` in hex |
| `process launch --stop-at-entry` | for inspecting before `main` |

Watchpoints are the killer feature for "my weight matrix gets corrupted somewhere": `watch set var w->data[17]` and run; lldb stops on the write that clobbers it.

## 5. AddressSanitizer and UndefinedBehaviorSanitizer

```
cc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -std=c11 -o prog prog.c -lm
./prog
```

ASan instruments every load/store and detects: heap/stack/global buffer overflow, use-after-free, double free, memory leaks (on Linux; on macOS use `leaks`, below). UBSan detects: signed overflow, shift out of range, null deref, misaligned access, out-of-bounds on arrays of known size, division by zero. ~2x slowdown; use it for all test runs, not production.

Reading an ASan report:

```
==1234==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6020000000f8
READ of size 8 at 0x6020000000f8 thread T0
    #0 0x1045 in mat_get prog.c:17            <- where the bad access happened
    #1 0x1201 in mat_mul prog.c:44
    #2 0x1390 in main prog.c:120
0x6020000000f8 is located 0 bytes to the right of 96-byte region [0x602000000098,0x6020000000f8)
allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in mat_new prog.c:9             <- which allocation was overrun
```

"0 bytes to the right of a 96-byte region" = you read element `n` of an `n`-element (12 doubles) array. Classic off-by-one: `for (i = 0; i <= n; i++)`. The two stack traces (bad access, allocation) together locate the bug instantly.

UBSan report: `prog.c:31:15: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`. Add `-fno-sanitize-recover=all` to make it abort on first error.

Common findings in numeric code: reading `data[rows*cols]` (one past the end) in a reduction; `free`ing a matrix twice when the same pointer sits in two layers; using a struct after `realloc` moved it; `int` overflow in `rows * cols` for large images; `1 << 31`.

## 6. `leaks` on macOS

macOS ASan does not report leaks by default. Use Apple's `leaks` tool:

```
$ cc -g -O0 -o prog prog.c -lm
$ leaks --atExit -- ./prog
Process 4321: 12 nodes malloced for 3 KB
Process 4321: 1 leak for 96 total leaked bytes.
    1 (96 bytes) ROOT LEAK: 0x600000c04000 [96]   malloc in mat_new  prog.c:9   called from main prog.c:55
```

Set `MallocStackLogging=1` in the environment for full allocation stacks: `MallocStackLogging=1 leaks --atExit -- ./prog`. Every `mat_new` needs a matching `mat_free` — check with `leaks` after every session that touches allocation. On Linux the equivalent is `valgrind --leak-check=full ./prog` or ASan's built-in `detect_leaks=1`.

## 7. A minimal unit-test framework

You don't need a library. Two macros and two counters:

```c
#include <math.h>
#include <stdio.h>

static int tests_run = 0, tests_failed = 0;

#define TEST(name) static void test_##name(void)          // defines a test function
#define RUN(name) do { tests_run++; fprintf(stderr, "  %-40s", #name); test_##name(); } while (0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { tests_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); return; } \
} while (0)
#define ASSERT_EQ_INT(a, b) do { long _a = (a), _b = (b); \
    if (_a != _b) { tests_failed++; fprintf(stderr, "FAIL %s:%d: %s == %ld, expected %ld\n", \
                                            __FILE__, __LINE__, #a, _a, _b); return; } } while (0)
#define ASSERT_NEAR(a, b, tol) do { double _a = (a), _b = (b); \
    if (!(fabs(_a - _b) <= (tol))) { tests_failed++; fprintf(stderr, "FAIL %s:%d: %s = %.10g, expected %.10g (tol %g)\n", \
                                            __FILE__, __LINE__, #a, _a, _b, (double)(tol)); return; } } while (0)
#define PASS() fprintf(stderr, "ok\n")

TEST(sqrt_two) { ASSERT_NEAR(sqrt(2.0) * sqrt(2.0), 2.0, 1e-12); PASS(); }
TEST(int_div)  { ASSERT_EQ_INT(7 / 2, 3); PASS(); }

int main(void) {
    RUN(sqrt_two);
    RUN(int_div);
    fprintf(stderr, "%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;       // nonzero exit code -> `make test` fails -> CI fails
}
```

The `return` inside the assert macros exits the test function on first failure (so a `TEST` body must return `void`). `!(fabs(...) <= tol)` rather than `fabs(...) > tol` so that a `NaN` result *fails* the test. Keep tests in `test_matrix.c`, build them with a `make test` target (`../09_multi_file_projects_and_make/lesson.md`), and run under sanitizers.

Python equivalent: `pytest` with `math.isclose` / `np.testing.assert_allclose`. `ASSERT_NEAR` is `assert_allclose(a, b, atol=tol)`.

## 8. Test-driven development of a `Matrix` lib

Write the test first, watch it fail, make it pass, repeat. For a matrix library the order that works:

1. `mat_new(rows, cols)` → all zeros, correct shape. Test: `m->rows == 2`, `m->data[3] == 0`.
2. `mat_get`/`mat_set` round trip. Test: set (1,2) = 5, get (1,2) == 5, get (0,0) still 0.
3. `mat_identity(n)`. Test: diagonal 1s, off-diagonal 0s.
4. `mat_mul(a, b)`. Tests: `I * A == A`; a hand-computed 2×3 · 3×2; shape of result.
5. `mat_transpose`. Test: `(A^T)^T == A`; `(AB)^T == B^T A^T` (an *algebraic identity* test — catches index bugs without hand computation).
6. `mat_add`, `mat_scale`, `mat_sum`, `mat_frobenius_norm`.
7. Property tests using your RNG (`../12_numbers_bits_floats/lesson.md`): random A, B, C; check `(AB)C ≈ A(BC)` to `1e-9`, `A(B+C) ≈ AB + AC`.

Algebraic identities are the best matrix tests: no expected values to type in, and one wrong index breaks them. Compare with `ASSERT_NEAR` and a relative tolerance scaled by the matrix norm.

## 9. Gradient checking — the most important test in ML

Backprop is just the chain rule, but the code for it is easy to get subtly wrong (transposed weight, wrong sign, forgot to sum over the batch). Finite differences give an independent, slow, *correct* gradient to compare against:

```c
// For each parameter theta_i:  dL/dtheta_i ≈ (L(theta + h e_i) - L(theta - h e_i)) / (2h)
// Central difference has O(h^2) error; h = 1e-5 in double gives ~1e-10 accuracy.
typedef double (*LossFn)(const double *params, size_t n, void *ctx);

static double grad_check(LossFn loss, double *params, size_t n, const double *analytic_grad,
                         void *ctx, double h) {
    double max_rel_err = 0.0;
    for (size_t i = 0; i < n; i++) {
        double orig = params[i];
        params[i] = orig + h; double lp = loss(params, n, ctx);
        params[i] = orig - h; double lm = loss(params, n, ctx);
        params[i] = orig;                                   // restore!
        double numeric = (lp - lm) / (2 * h);
        double denom = fmax(fabs(numeric) + fabs(analytic_grad[i]), 1e-12);
        double rel = fabs(numeric - analytic_grad[i]) / denom;
        if (rel > max_rel_err) max_rel_err = rel;
    }
    return max_rel_err;     // < 1e-6 is a pass in double; > 1e-3 means a bug
}
```

Rules: use `double` for the check (in `float`, `h=1e-5` gives cancellation noise around 1e-3 — indistinguishable from bugs). Check on tiny networks (3 inputs, 4 hidden, 2 outputs) — it's O(n) forward passes. Use a *relative* error so both huge and tiny gradients are judged fairly. Check every op in isolation (matmul backward, ReLU backward, softmax-cross-entropy backward) and then the whole net. PyTorch: `torch.autograd.gradcheck`. Do this before you ever train; it saves days.

## 10. Timing code

```c
#include <time.h>
static double now_sec(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
#define TIMER_START(name) double _t_##name = now_sec()
#define TIMER_END(name)   fprintf(stderr, "%-20s %10.3f ms\n", #name, (now_sec() - _t_##name) * 1e3)

// TIMER_START(matmul);
// mat_mul(a, b, c);
// TIMER_END(matmul);          -> matmul                 12.345 ms
```

`CLOCK_MONOTONIC` is a steady wall clock (ns resolution on macOS). `clock()` measures *CPU* time summed over threads — useful, but it is not what the user waits for. For sub-microsecond things, time a loop of `N` repetitions and divide.

## 11. Why measurement is hard

- **Warmup**: the first run pays for page faults (touching fresh `malloc`'d memory), cold caches, CPU frequency ramp. Run once untimed, then time.
- **Repeats**: take the *minimum* or *median* of 5–10 runs, not the mean — the noise (other processes, thermal throttling) is strictly additive.
- **`-O0` vs `-O2`**: never benchmark `-O0`; it's 5–20x slower and not representative. `-O2` is the baseline; `-O3 -march=native` for the fast build.
- **The compiler deletes your benchmark.** If the result of a computation is never used, `-O2` removes the computation. A loop that sums into a local that's never printed takes 0 ms. Defenses: print the result (or a checksum), store it into a `volatile` variable, or pass it to an opaque function. Also: a loop with constant inputs may be folded at compile time — read inputs from `argv` or fill with RNG.

```c
double sum = 0;
for (int i = 0; i < N; i++) sum += a[i] * b[i];
volatile double sink = sum;      // forces the computation to actually happen
(void)sink;
```

- **Turbo / power**: laptop CPUs change frequency under load. Consistent, repeated measurements matter more than absolute numbers.
- **What to report**: for matmul, GFLOP/s = `2*n^3 / seconds / 1e9`. Your M-series Mac does ~100+ GFLOP/s single-core with a good kernel; naive C gets 1–5.

## 12. The memory hierarchy

| Level | Size (typical M-series) | Latency | ~Cycles |
|---|---|---|---|
| Registers | ~32 × 64-bit + 32 × 128-bit SIMD | 0 | 0 |
| L1 cache | 128–192 KB per core | ~1 ns | 3–4 |
| L2 cache | 4–16 MB shared per cluster | ~4 ns | 12–16 |
| L3 / SLC | 8–32 MB | ~15 ns | 40–60 |
| RAM | 8–128 GB | ~100 ns | 300+ |
| SSD | TB | ~50–100 µs | 10^5 |

A RAM access costs ~100 multiplies. Fast code = code that keeps its working set in L1/L2 and touches RAM in a predictable streaming pattern. A 512×512 double matrix is 2 MB — fits in L2; 2048×2048 is 32 MB — does not.

## 13. Cache lines and spatial locality

Memory moves between levels in **cache lines** of 64 bytes (128 on Apple Silicon) — 8 (or 16) doubles at a time. Touch `a[0]` and `a[1..7]` are already in L1 for free. Touch `a[0]`, `a[1000]`, `a[2000]` and each access is a separate line fetch: 8–16x more memory traffic for the same number of loads. The hardware prefetcher also detects sequential streams and fetches ahead — but only for simple stride patterns.

Consequence: iterate over memory in the order it is laid out.

## 14. The matmul loop-order experiment

Row-major `C[i][j] = Σ_k A[i][k] * B[k][j]`, three nested loops, six orders. Two of them:

```c
// ijk (textbook): inner loop over k reads A[i][k] sequentially (good) but B[k][j] with stride n (bad).
for (i) for (j) { s = 0; for (k) s += A[i*n+k] * B[k*n+j]; C[i*n+j] = s; }

// ikj: inner loop over j reads B[k][j] and C[i][j] sequentially, A[i][k] is a loop invariant.
for (i) for (k) { a = A[i*n+k]; for (j) C[i*n+j] += a * B[k*n+j]; }
```

Same arithmetic, same count of flops. On a 512×512 double matmul at `-O2` the `ikj` order is typically **5–10x faster** because every inner-loop access is a stride-1 stream (and the compiler can vectorize it), while `ijk` strides through `B` by a full row per step — one cache line fetch per multiply. `example.c` in this chapter measures it. Try `n = 1024` (`B` no longer fits L2) and watch the gap widen.

Beyond loop order: **blocking/tiling** (process 64×64 sub-blocks so all three tiles fit in L1), transposing `B` once so `ijk` becomes two sequential streams, then SIMD and multi-threading. That path leads to BLAS; `numpy.dot` calls it. Your goal is to understand *why* it's fast, not to beat it.

## 15. Row-major traversal, AoS vs SoA

Row-major: `m[i][j]` is at `data[i*cols + j]`; the last index varies fastest in memory. So `for i: for j:` is sequential; `for j: for i:` jumps by `cols` each step. NumPy default is row-major too ("C order"); `np.asfortranarray` gives column-major. Fortran, MATLAB, Julia, and BLAS are column-major — the reason `dgemm` takes a "transpose" flag.

Array-of-structs vs struct-of-arrays for N-body:

```c
// AoS: natural, but a loop over only positions loads velocity and mass too (wasted cache lines).
typedef struct { double x, y, z, vx, vy, vz, m; } Body;  Body bodies[N];
// SoA: each field contiguous; the position loop streams 3 arrays; SIMD loads 2-4 x's at once.
typedef struct { double *x, *y, *z, *vx, *vy, *vz, *m; } Bodies;
```

For hot loops that touch a subset of fields, SoA is often 2–3x faster and vectorizes cleanly. PyTorch tensors are SoA by nature (one contiguous buffer per tensor). Choose per hotspot, not globally.

## 16. Branch prediction

The CPU guesses which way an `if` goes and speculatively executes ahead. A correct guess is free; a wrong one costs ~15–20 cycles of flushed work. Predictable branches (loop conditions, `if (i == n-1)`) are free. Unpredictable ones (`if (x[i] > 0)` on random data) are expensive. Fixes: branchless arithmetic (`y = x * (x > 0)` for ReLU; the compiler often does this), sorting data first, or lookup tables. The famous demo: summing elements `> 128` of a random byte array is ~5x faster after sorting the array. Measure before believing.

## 17. Compiler flags: `-O3 -march=native`, `restrict`, vectorization

- `-O2`: safe default optimization. `-O3`: more aggressive inlining/unrolling/vectorization. `-march=native`: use every instruction your CPU has (on Apple Silicon this enables full NEON; on x86 it enables AVX2/AVX-512). Binaries won't run on older CPUs — fine for your own experiments.
- `restrict`: promise the compiler that a pointer is the *only* way to reach its memory during the function. Without it, in `void axpy(double *y, const double *x, double a, size_t n)` the compiler must assume `y` and `x` could overlap and reload after every store. With `double *restrict y, const double *restrict x` it can vectorize freely. Lying (passing overlapping pointers) is UB.

**Auto-vectorization** = the compiler rewriting a scalar loop to process 2 doubles (NEON, 128-bit) or 4/8 (AVX) per instruction. Requirements: simple loop bounds, no loop-carried dependencies (except reductions, which need `-ffast-math` or explicit accumulators for floats because reassociation changes rounding), no aliasing doubts (`restrict`), no function calls in the body. Check what the compiler did:

```
cc -O3 -march=native -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -c matmul.c
matmul.c:23:5: remark: vectorized loop (vectorization width: 2, interleaved count: 4)
matmul.c:31:5: remark: loop not vectorized: cannot identify array bounds   <- fix this one
```

`-Rpass-analysis=loop-vectorize` explains *why* a loop was not vectorized. Also `-fsave-optimization-record` writes a YAML with all remarks. Look at the assembly with `-S` or `objdump -d` and search for `fmla v0.2d` (NEON) / `vfmadd231pd` (AVX) to confirm.

## 18. Sampling profilers (brief)

Timers tell you *how long*; a profiler tells you *where*. On macOS: **Instruments** (Xcode → Open Developer Tool → Instruments → Time Profiler), or from the terminal `xctrace record --template 'Time Profiler' --launch ./prog`; or the simpler `sample prog_pid 5` which prints a call-tree of where the process spent 5 seconds. On Linux: `perf record ./prog && perf report`, or `gprof` with `-pg`. Compile with `-g -O2` so the profile maps to source lines. Expect to find 90% of time in one function; optimize that, re-measure, repeat.

## 19. Amdahl's law

If a fraction `p` of runtime is in the part you speed up by factor `s`, total speedup is `1 / ((1 - p) + p / s)`. Make matmul (90% of runtime) infinitely fast and you get at most 10x. Make data loading (10%) 10x faster and you get 1.1x. Profile first, then optimize the top of the list. The same law bounds multi-threading: with 8 cores and 5% serial code, max speedup is 5.9x, not 8x.

## 20. Premature optimization vs knowing your constants

"Premature optimization is the root of all evil" is about *micro*-optimizing before profiling. It is **not** an excuse to ignore algorithmic complexity or memory layout at design time — those are hard to retrofit. Know your constants:

| Operation | Approximate cost |
|---|---|
| `double` add/mul | 1 cycle throughput (0.3 ns) |
| `double` divide, `sqrt` | ~10–20 cycles |
| `exp`, `log`, `tanh` | ~20–100 cycles (library call) |
| L1 hit / RAM miss | 1 ns / 100 ns |
| `malloc`/`free` | ~50–200 ns — never in an inner loop |
| Function call | ~1–5 ns (unless inlined) |
| Branch mispredict | ~5 ns |
| Syscall (`write`, `read`) | ~1 µs |
| Disk read (SSD) | ~100 µs per random access |

Rule of thumb order: right algorithm (O(n log n) FFT vs O(n²) DFT) → right memory layout (row-major, SoA) → no allocation in hot loops → enable `-O3 -march=native` and check vectorization → then, and only then, hand-tune.

---

## Gotchas and undefined behavior

- **Side effects inside `assert`** vanish under `-DNDEBUG`.
- **`-O2` and debugging**: variables may be "optimized out" in lldb; recompile with `-O0 -g` to debug, then go back.
- **Benchmarking dead code**: if you don't use the result, there's nothing to time. Use the result.
- **Timing with `clock()`** on a multithreaded program gives total CPU time across threads, not elapsed time.
- **`float` gradient checks** are unreliable: `h=1e-5` in `float` has ~1e-2 relative error just from rounding. Check in `double`.
- **Forgetting to restore the parameter** after perturbing in gradient check corrupts all later checks.
- **`restrict` lie**: `axpy(y, y, ...)` with `restrict` is UB and may produce wrong results silently at `-O3`.
- **Uninitialized memory from `malloc`** looks fine at `-O0` (often zero) and garbage at `-O2`. Use `calloc` or ASan.
- **Sanitizer + `-O2`**: fine, but stack traces are clearer at `-O1 -fno-omit-frame-pointer`.
- **ASan doesn't catch everything**: reading uninitialized heap memory is not reported by ASan (that's MemorySanitizer, Linux only, or valgrind). Reading past a stack array into the *next* variable may be caught or not.
- **`leaks` false positives** for memory still reachable from globals at exit are *not* leaks; the tool distinguishes "leak" (unreachable) from "reachable".
- **Cache effects in tests**: a microbenchmark running the same 8 KB array 1000 times is measuring L1, not what your real 100 MB dataset will see.

## Common mistakes checklist

- [ ] Ignored a warning because "it still compiled".
- [ ] `assert(file != NULL)` for a user-supplied path (that's an error, not a bug).
- [ ] Debug prints to `stdout`, lost when the program crashed through a pipe.
- [ ] Benchmarked at `-O0`, or a computation whose result was never used.
- [ ] Took the mean of timings including the cold first run.
- [ ] Wrote `ijk` matmul and wondered why it's slow; never tried `ikj` or blocking.
- [ ] Wrote tests but not a gradient check; trained for hours on a wrong backward pass.
- [ ] Never ran the test suite under `-fsanitize=address,undefined`.
- [ ] `TEST` body doesn't `return void` so `ASSERT_*` macros don't compile.
- [ ] Compared against `tol` with `>` so `NaN` passes.

## You can move on when...

- You can explain what `assert` is for, what `-DNDEBUG` does to it, and where runtime errors belong instead.
- You can start `lldb`, break on a line, step, print a struct and an array slice, read a backtrace, and set a watchpoint.
- You can read an ASan heap-buffer-overflow report and name the line of the bad access and the line of the allocation.
- You can write `TEST`/`ASSERT_NEAR`/`RUN` from memory and explain why `NaN` must fail.
- You can write `grad_check` and state the expected relative error for a correct gradient in `double`.
- You can run the loop-order experiment, report GFLOP/s for both, and explain the difference in terms of cache lines and stride.
- You can defeat the optimizer in a benchmark and explain why `-Rpass=loop-vectorize` matters.
