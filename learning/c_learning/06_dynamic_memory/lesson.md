# Chapter 06 — Dynamic Memory

## What you'll be able to do after this chapter

- Explain where every byte of your program lives (stack vs heap vs static) and how long it lives.
- Allocate, resize, and free memory with `malloc`/`calloc`/`realloc`/`free` without leaks or crashes.
- Recognize the four classic memory bugs (leak, double free, use-after-free, overflow) and read AddressSanitizer output that pinpoints them.
- Build a growable array with amortized O(1) append — the foundation of every container you will write.
- Allocate 2D matrices on the heap the fast way (one flat block) and know why the `double**` way is usually worse.
- Write an arena (bump) allocator, the allocation strategy used inside real ML inference runtimes.

## Why this matters for ML / numerics / sims

In Python, `np.zeros((1000, 1000))` allocates 8 MB somewhere and you never think about it again. In C, *you* are NumPy. Every matrix, every token buffer, every particle array in your N-body sim is a block you asked the OS for and must hand back. A tokenizer that reads a 100 MB corpus cannot fit in a fixed-size stack array. A training loop that leaks 1 KB per step dies after a million steps. An autograd graph is nodes allocated on the heap linked by pointers. Getting this chapter right is the difference between C programs that run for hours and C programs that crash with `Segmentation fault: 11`.

---

## 1. Stack vs heap: where memory lives

A running process has several memory regions:

```
High addresses
+---------------------------+
|          stack            |  local variables, function arguments, return addresses.
|   (grows downward)   |     |  Automatically freed when a function returns.
|                      v    |  Size: ~8 MB on macOS by default (ulimit -s).
+---------------------------+
|            ...            |
|                      ^    |
|   (grows upward)     |    |  malloc'd blocks. Live until you call free().
|          heap             |  Size: limited by RAM + swap (gigabytes).
+---------------------------+
|   .bss  (zeroed globals)  |  static/global variables. Live for the whole program.
|   .data (init'd globals)  |
+---------------------------+
|   .text (machine code)    |
+---------------------------+
Low addresses
```

| Region | Who allocates | Lifetime | Size limit | Speed |
|---|---|---|---|---|
| Stack | compiler, on function entry | until function returns | ~8 MB total | very fast (bump a register) |
| Heap | you, via `malloc` | until you `free` it | gigabytes | slower (allocator bookkeeping) |
| Static | compiler, at program start | whole program | large but fixed at compile time | free |

Why the stack cannot hold a big matrix:

```c
void bad(void) {
    double m[2000][2000];   // 32 MB on the stack -> almost certainly a crash (stack overflow)
    m[0][0] = 1.0;
}
```

That is 32 MB against an 8 MB limit. The crash is not a nice error message — it is a segfault with no explanation. Anything larger than a few KB, or whose size is only known at runtime, belongs on the heap.

Why you cannot return a stack array:

```c
double *make_row(void) {
    double row[4] = {1, 2, 3, 4};
    return row;             // UB: row's memory is reclaimed when make_row returns
}
```

The pointer you get back points at memory that will be overwritten by the next function call. This compiles (with a warning) and sometimes "works" — which is exactly what makes it dangerous.

**Python equivalent:** there is no equivalent. Every Python object is heap-allocated and reference-counted. C gives you the stack for free and makes you manage the heap by hand.

---

## 2. `malloc`, `calloc`, `realloc`, `free` — the precise semantics

All four live in `<stdlib.h>`.

```c
void *malloc(size_t n);                 // n bytes, contents UNINITIALIZED (garbage)
void *calloc(size_t count, size_t sz);  // count*sz bytes, all bytes set to 0
void *realloc(void *p, size_t n);       // resize block p to n bytes (may MOVE it)
void  free(void *p);                    // release; p must be from malloc/calloc/realloc or NULL
```

Precise rules you must internalize:

| Fact | Consequence |
|---|---|
| `malloc` does not zero memory | Reading before writing is UB. Use `calloc` if you need zeros. |
| `calloc(count, sz)` checks `count*sz` for overflow | Prefer it over `malloc(count*sz)` for arrays when you want zeros. |
| Any of them may return `NULL` | Always check. `if (!p) { ... handle ... }` |
| `realloc` may return a *different* address | Never keep old pointers into the block across a `realloc`. Always assign the result. |
| `realloc` copies the old contents to the new block | The first `min(old,new)` bytes are preserved; the rest is uninitialized. |
| If `realloc` fails it returns `NULL` and the **old block is still valid** | So write `tmp = realloc(p, n); if (!tmp) {...} p = tmp;` — never `p = realloc(p, n)` directly. |
| `realloc(NULL, n)` is exactly `malloc(n)` | Lets a growable array start with `data = NULL, cap = 0`. |
| `free(NULL)` is a no-op | Safe to free pointers that may not have been allocated. |
| `free(p)` twice is UB | Double free. Set `p = NULL` after freeing. |
| Using `p` after `free(p)` is UB | Use-after-free. The memory may already belong to someone else. |
| `free` on a pointer *into the middle* of a block is UB | Only free the pointer `malloc` gave you. |

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *a = malloc(4 * sizeof *a);      // 16 bytes, garbage inside
    if (!a) { perror("malloc"); return 1; }
    for (int i = 0; i < 4; i++) a[i] = i * i;

    int *tmp = realloc(a, 8 * sizeof *a); // grow to 8 ints; a[0..3] preserved
    if (!tmp) { free(a); return 1; }      // a is still valid here if realloc failed
    a = tmp;
    for (int i = 4; i < 8; i++) a[i] = i * i;

    for (int i = 0; i < 8; i++) printf("%d ", a[i]);
    printf("\n");
    free(a);
    a = NULL;                              // now a double free would be harmless
    free(a);                               // free(NULL): fine
    return 0;
}
// Output: 0 1 4 9 16 25 36 49
```

### The `sizeof *p` idiom

Write `malloc(n * sizeof *p)` instead of `malloc(n * sizeof(double))`. If you later change `p`'s type from `double*` to `float*`, the allocation size follows automatically. `sizeof *p` is evaluated at compile time from the *type* of `*p`; it does not dereference the pointer, so it is safe even when `p` is uninitialized or NULL.

```c
double *w = malloc(n * sizeof *w);   // good
double *w = malloc(n * sizeof(double)); // works, but repeats the type
double *w = malloc(n);               // BUG: n bytes, not n doubles
```

### Overflow in the size computation

`rows * cols * sizeof(double)` is computed in `size_t`. If `rows` and `cols` are `int` and their product overflows `int` before being converted, you get a small (wrong) allocation followed by a buffer overflow. Cast first: `(size_t)rows * cols * sizeof(double)`, or use `calloc((size_t)rows * cols, sizeof(double))`.

---

## 3. Always check for NULL

On macOS with 16+ GB of RAM, `malloc` almost never fails for small sizes — until it does (address space exhaustion, a bug that asks for `(size_t)-1` bytes, a sandbox). The pattern:

```c
double *buf = malloc(n * sizeof *buf);
if (!buf) {
    fprintf(stderr, "out of memory allocating %zu doubles\n", n);
    return NULL;          // or exit(1) in a small program
}
```

For a learning program, a tiny wrapper is acceptable and keeps code readable:

```c
static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "fatal: out of memory (%zu bytes)\n", n); exit(EXIT_FAILURE); }
    return p;
}
```

Libraries you intend to reuse (your `matrix` library) should *return* an error to the caller instead of exiting, because the caller may be able to recover (e.g. shrink the batch size).

---

## 4. Ownership: who frees?

Every heap block has exactly one owner at any time — the code responsible for eventually calling `free`. C does not track this; **you** document it. The convention that keeps projects sane:

```c
/* Returns a newly allocated array of n doubles filled with `v`.
 * Caller owns the result and must free() it. Returns NULL on allocation failure. */
double *vec_filled(size_t n, double v);

/* Fills `out` (which must have room for n doubles) with `v`. Does not allocate. */
void vec_fill(double *out, size_t n, double v);

/* Takes ownership of `data`: the Matrix will free it in mat_free(). Do not free `data` yourself. */
Matrix mat_wrap(size_t rows, size_t cols, double *data);

/* Borrows `m`: reads it, does not free it, does not keep a pointer past the call. */
double mat_sum(const Matrix *m);
```

Three verbs cover almost every case: **returns ownership** ("caller must free"), **takes ownership** ("do not free after passing"), **borrows** ("read only during the call"). Write one of them in every function comment that touches a heap pointer. Rust enforces this in the type system; C enforces it with your comments and ASan.

Rule of thumb: whoever `malloc`s should `free`, unless the comment explicitly says ownership moved.

---

## 5. Return a new allocation, or fill a caller-provided buffer?

| Approach | Signature | Pros | Cons |
|---|---|---|---|
| Return allocated pointer | `double *softmax_new(const double *x, size_t n)` | Simple call site | One `malloc` per call (slow in hot loops); caller must remember to free |
| Fill caller buffer | `void softmax(double *out, const double *x, size_t n)` | No allocation; caller controls memory; can reuse buffers across iterations | Caller must size the buffer correctly |

For hot loops (a forward pass called millions of times), prefer the caller-provided buffer. Allocate once outside the loop, reuse inside. This is how BLAS works: `dgemm` writes into a `C` you pass in. Return-allocated is fine for one-off setup code (loading a dataset).

---

## 6. The four memory bugs and what ASan says about them

Compile with the sanitizer flags to catch these at runtime:

```
cc -g -fsanitize=address,undefined -Wall -Wextra -std=c11 -o prog prog.c -lm
```

`-g` keeps line numbers so the report points at your source. AddressSanitizer (ASan) adds "red zones" around every allocation and poisons freed memory, so bad accesses are caught the instant they happen — not thousands of instructions later when the corrupted data finally causes a crash.

### 6.1 Memory leak

```c
void leak(void) {
    double *p = malloc(100 * sizeof *p);
    p[0] = 1.0;
    // forgot free(p) -> 800 bytes lost every call
}
```

On Linux, ASan's LeakSanitizer would print this at exit:

```
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 800 byte(s) in 1 object(s) allocated from:
    #0 0x... in malloc
    #1 0x... in leak prog.c:3
    #2 0x... in main prog.c:10

SUMMARY: AddressSanitizer: 800 byte(s) leaked in 1 allocation(s).
```

**On Apple Silicon, LeakSanitizer is not supported** (`ASAN_OPTIONS=detect_leaks=1` prints `detect_leaks is not supported on this platform`). Use Apple's `leaks` tool on a normal build instead:

```
cc -g -O0 -Wall -Wextra -std=c11 -o prog prog.c -lm
MallocStackLogging=1 leaks --atExit -- ./prog
```

Output:

```
STACK OF 1 INSTANCE OF 'ROOT LEAK: <malloc in leak>':
    1 (896 bytes) ROOT LEAK: <malloc in leak 0x...> [896]
      ...
      main  prog.c:10
      leak  prog.c:3
```

(896, not 800: malloc rounds requests up to a size class.) Two practical notes: build with `-O0` for leak checks — at `-O2` the dead pointer may linger in a register or stack slot, and `leaks` conservatively scans those and considers the block still reachable; and `MallocStackLogging=1` is what gives you the allocation call stack instead of just an address.

A leak does not crash. It just eats memory. In a 10-minute training run, a leak in the inner loop will exhaust RAM and the OS will kill the process.

### 6.2 Double free

```c
void double_free(void) {
    int *p = malloc(sizeof *p);
    free(p);
    free(p);        // UB: allocator metadata corrupted
}
```

```
==12345==ERROR: AddressSanitizer: attempting double-free on 0x602000000010 in thread T0:
    #0 0x... in free
    #1 0x... in double_free prog.c:4
0x602000000010 is located 0 bytes inside of 4-byte region [0x602000000010,0x602000000014)
freed by thread T0 here:
    #0 0x... in free
    #1 0x... in double_free prog.c:3
previously allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in double_free prog.c:2
```

ASan tells you *both* the line of the second free and the line of the first. Without ASan, macOS prints `malloc: *** error for object 0x...: pointer being freed was not allocated` and aborts — or silently corrupts the heap.

### 6.3 Use-after-free

```c
void use_after_free(void) {
    int *p = malloc(4 * sizeof *p);
    p[0] = 42;
    free(p);
    printf("%d\n", p[0]);   // UB: reading freed memory
}
```

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x604000000010
READ of size 4 at 0x604000000010 thread T0
    #0 0x... in use_after_free prog.c:5
0x604000000010 is located 0 bytes inside of 16-byte region
freed by thread T0 here:
    #1 0x... in use_after_free prog.c:4
previously allocated by thread T0 here:
    #1 0x... in use_after_free prog.c:2
```

Without ASan this usually prints `42` — the memory has not been reused *yet*. Then one day the allocator hands that block to something else and your value silently changes. This is the most insidious of the four.

### 6.4 Heap buffer overflow

```c
void overflow(void) {
    double *v = malloc(10 * sizeof *v);
    for (int i = 0; i <= 10; i++)    // <= should be <
        v[i] = 0.0;                  // v[10] writes 8 bytes past the end
    free(v);
}
```

```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x606000000090
WRITE of size 8 at 0x606000000090 thread T0
    #0 0x... in overflow prog.c:4
0x606000000090 is located 0 bytes after 80-byte region [0x606000000040,0x606000000090)
allocated by thread T0 here:
    #1 0x... in overflow prog.c:2
```

"0 bytes after 80-byte region" is the giveaway: you wrote at exactly `end`. Off-by-one. Without ASan, this overwrites allocator metadata or the neighboring block, and you get a crash in an unrelated `free` much later.

### Summary table

| Bug | Symptom without ASan | ASan message |
|---|---|---|
| Leak | memory grows; eventual OOM kill | `LeakSanitizer: detected memory leaks` |
| Double free | `pointer being freed was not allocated`, abort | `attempting double-free` |
| Use-after-free | wrong values, random crashes | `heap-use-after-free` |
| Buffer overflow | corrupted neighbors, crash in `free` | `heap-buffer-overflow` |

**Valgrind is not available on Apple Silicon.** Use ASan for double free / use-after-free / overflow and `leaks --atExit -- ./prog` (on a `-O0 -g` build) for leak detection. Use `-fsanitize=undefined` (UBSan) alongside ASan to catch signed overflow, misaligned access, and shifts out of range.

---

## 7. NULL-ing pointers after free

```c
free(p);
p = NULL;
```

This converts a potential double free into a harmless `free(NULL)` and converts a use-after-free into a guaranteed, immediate segfault (dereferencing NULL) instead of silent corruption. It costs nothing. Do it whenever the pointer variable stays in scope after the free. It does *not* help if another copy of the pointer exists elsewhere — that is why ownership must be clear.

---

## 8. Growable array (dynamic array / "vector")

Fixed-size arrays force you to know `n` up front. A growable array stores three things: the data pointer, how many elements are in use (`len`), and how many fit before reallocating (`cap`).

```
len = 3, cap = 4
data -> [ 1.0 | 2.0 | 3.0 |  ?  ]
          used  used  used  free
```

When `len == cap`, double `cap` and `realloc`. Doubling gives **amortized O(1)** append: to reach `n` elements you copy `1 + 2 + 4 + ... + n/2 < n` elements total, so the average cost per push is constant. Growing by a fixed amount (+16) would be O(n) per push amortized — O(n²) to fill.

```c
typedef struct {
    double *data;
    size_t  len;
    size_t  cap;
} Vec;

/* Returns 0 on success, -1 on allocation failure (vec unchanged). */
int vec_push(Vec *v, double x) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 8;
        double *tmp = realloc(v->data, ncap * sizeof *tmp);  // realloc(NULL, n) == malloc(n)
        if (!tmp) return -1;
        v->data = tmp;
        v->cap  = ncap;
    }
    v->data[v->len++] = x;
    return 0;
}

void vec_free(Vec *v) { free(v->data); v->data = NULL; v->len = v->cap = 0; }

// Usage:
//   Vec v = {0};                  // data=NULL, len=0, cap=0 -- valid empty vector
//   for (int i = 0; i < 100; i++) vec_push(&v, i * 0.5);
//   printf("%zu %zu\n", v.len, v.cap);   // 100 128
//   vec_free(&v);
```

**Python equivalent:** `list.append` does exactly this (CPython over-allocates by ~12.5%, not 2x, but same idea). `np.append` does *not* — it copies the whole array every time, which is why it is slow in loops.

Danger: a pointer `double *first = &v.data[0]` becomes dangling after any `vec_push` that reallocates. Store indices, not pointers, into growable arrays.

---

## 9. 2D arrays on the heap

Two layouts. Know both; use the first.

### 9.1 Flat block (preferred)

```c
size_t rows = 3, cols = 4;
double *m = calloc(rows * cols, sizeof *m);
// element (i, j):
m[i * cols + j] = 1.0;
free(m);   // one free
```

```
memory:  [ r0c0 r0c1 r0c2 r0c3 | r1c0 r1c1 r1c2 r1c3 | r2c0 r2c1 r2c2 r2c3 ]
          ^ one contiguous block, row-major (same as NumPy default, C order)
```

Pros: one allocation, one free, contiguous (cache-friendly; sequential access streams through memory), trivially passed to BLAS / written to a file with one `fwrite`. Cons: you write `m[i*cols + j]` instead of `m[i][j]` — wrap it in a macro or inline function.

### 9.2 Array of pointers (`double **`, jagged)

```c
double **m = malloc(rows * sizeof *m);
for (size_t i = 0; i < rows; i++)
    m[i] = malloc(cols * sizeof *m[i]);
m[1][2] = 5.0;                          // natural syntax
for (size_t i = 0; i < rows; i++) free(m[i]);
free(m);                                // rows+1 frees
```

```
m -> [ ptr0 | ptr1 | ptr2 ]
        |      |      |
        v      v      v
      [....] [....] [....]   <- three separate heap blocks, anywhere in memory
```

Pros: `m[i][j]` syntax; rows can have different lengths (jagged). Cons: `rows+1` allocations, pointer chasing on every access (extra memory load, cache misses), cannot be passed to code expecting a contiguous block, error handling on partial allocation failure is annoying.

| | Flat | `double**` |
|---|---|---|
| Allocations | 1 | rows + 1 |
| Cache behavior | excellent | poor (rows scattered) |
| Syntax | `m[i*cols+j]` | `m[i][j]` |
| Interop (BLAS, fwrite) | direct | must copy |
| Jagged rows | no | yes |

Use `double**` only when rows genuinely have different lengths (e.g. a list of variable-length token sequences). For matrices, always flat. Chapter 07 wraps the flat layout in a `Matrix` struct.

---

## 10. `alloca` and VLAs — know them, avoid them

C99 introduced variable-length arrays: `double buf[n];` where `n` is a runtime value. `alloca(n)` (non-standard, `<alloca.h>`) does the same thing manually. Both allocate on the **stack**.

```c
void f(size_t n) {
    double vla[n];          // VLA: stack space, freed on return
    double *a = alloca(n * sizeof *a);  // same idea, non-standard
    (void)vla; (void)a;
}
```

Problems: if `n` is large you overflow the stack with no error checking (no NULL to test — just a crash); `sizeof` becomes a runtime operation; VLAs are optional in C11 (`__STDC_NO_VLA__`) and were removed from C++. Use `malloc` for anything whose size you do not control, or a fixed-size stack array with a sanity check when the bound is small and known.

---

## 11. Memory alignment basics

Every type has an alignment requirement: a `double` must live at an address divisible by 8, an `int` at one divisible by 4. `malloc` returns memory aligned for *any* standard type (16 bytes on macOS arm64). Misaligned access is UB in C; on arm64 it may work slowly or trap depending on the instruction.

Why you care: SIMD (NEON, AVX) instructions want 16/32/64-byte alignment for full speed. For that, C11 gives `aligned_alloc(alignment, size)` — `size` must be a multiple of `alignment`; free with ordinary `free`.

```c
double *v = aligned_alloc(64, 64 * sizeof *v);   // 512 bytes, 64-byte aligned (one cache line)
printf("%d\n", (int)((uintptr_t)v % 64));         // 0
free(v);
```

Chapter 07 covers alignment *inside* structs (padding).

---

## 12. Arena (bump) allocator

An arena is one big block you `malloc` once; you then hand out pieces by bumping an offset. You never free individual pieces — you free (or reset) the whole arena at once.

```
arena.base -> [ alloc1 | alloc2 | alloc3 |        free space          ]
                                          ^ arena.used
```

```c
typedef struct {
    unsigned char *base;
    size_t used, cap;
} Arena;

/* Returns 0 on success. Arena owns `base`; arena_free releases it. */
int arena_init(Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return -1;
    a->used = 0; a->cap = cap;
    return 0;
}

/* Returns a pointer to `n` bytes aligned to 16, or NULL if the arena is full.
 * The arena owns the memory; do NOT free() the result. */
void *arena_alloc(Arena *a, size_t n) {
    size_t start = (a->used + 15) & ~(size_t)15;   // round up to multiple of 16
    if (start + n > a->cap) return NULL;
    a->used = start + n;
    return a->base + start;
}

void arena_reset(Arena *a) { a->used = 0; }       // "free everything" in O(1)
void arena_free(Arena *a)  { free(a->base); a->base = NULL; a->used = a->cap = 0; }
```

Why real ML runtimes (llama.cpp, ggml, ONNX Runtime) use this: a forward pass allocates dozens of temporary tensors whose lifetimes all end when the pass ends. With an arena: compute the total size once, allocate once, run the pass, `arena_reset`, repeat. Zero `malloc` calls in the hot loop, zero fragmentation, zero leaks by construction, and every tensor is contiguous with its neighbors. The same applies to a per-frame arena in a simulation.

Limitation: you cannot free one thing in the middle. If lifetimes differ wildly, use `malloc`.

---

## 13. Tooling recap on macOS / Apple Silicon

| Tool | Command | Finds |
|---|---|---|
| ASan + UBSan | `cc -g -fsanitize=address,undefined ...` then run | overflow, UAF, double free, signed overflow, misalignment |
| LeakSanitizer | `ASAN_OPTIONS=detect_leaks=1 ./prog` | leaks — **Linux only; not supported on Apple Silicon** |
| `leaks` | `MallocStackLogging=1 leaks --atExit -- ./prog` on a `-O0 -g` build | leaks (Xcode CLI tools) |
| `-Wall -Wextra` | always | returning address of local, unused results, etc. |

Valgrind does not run on Apple Silicon. Do not waste time trying to install it.

---

## Gotchas and undefined behavior

- **Reading `malloc`'d memory before writing it** is UB. It often looks like zeros in a fresh process, then not. Use `calloc` or write first.
- **`p = realloc(p, n)`** loses the block if `realloc` returns NULL. Use a temporary.
- **Pointers into a block across `realloc`** dangle if the block moved. Recompute from the new base.
- **`free` on stack memory or a pointer offset into a block** is UB and usually aborts.
- **Returning the address of a local variable** is UB. The compiler warns (`-Wreturn-local-addr` / `address of stack memory`). Heed it.
- **`sizeof(p)` where `p` is a pointer** is 8, not the array size. `sizeof *p` is the element size. Neither tells you how many elements were allocated — you must track `n` yourself.
- **`int` overflow in size math**: `rows * cols * sizeof(double)` with `int rows, cols` can overflow before widening. Cast to `size_t` first.
- **Freeing the same block from two places** because two structs both hold the pointer. Decide who owns it; the other holds a borrowed reference.
- **VLAs with large or untrusted `n`** overflow the stack silently.
- **`memset(p, 0, n)` to "initialize" pointers or floats** is fine in practice on every platform you will use, but strictly only guaranteed for integer types. `calloc` has the same caveat. For `double`, all-zero bytes is `0.0` on every IEEE-754 machine.

---

## Common mistakes checklist

- [ ] Every `malloc`/`calloc`/`realloc` result checked for NULL.
- [ ] Every allocation has exactly one matching `free`, and you can point to it.
- [ ] `realloc` result stored in a temporary before overwriting the original pointer.
- [ ] Sizes computed as `n * sizeof *p`, with `n` already `size_t`.
- [ ] Pointer set to NULL after `free` when the variable remains in scope.
- [ ] No pointers held into a growable array across a push.
- [ ] Matrices are one flat block, indexed `i*cols + j`.
- [ ] Every function comment says whether it returns ownership, takes ownership, or borrows.
- [ ] You have run the program under `-fsanitize=address,undefined` at least once.
- [ ] No large or runtime-sized arrays on the stack.

---

## You can move on when...

- You can draw the stack/heap diagram from memory and say which of your variables lives where.
- You can state, without looking, what `realloc(NULL, n)`, `free(NULL)`, and `realloc` returning NULL each mean.
- You can write `vec_push` with doubling from scratch and explain why it is amortized O(1).
- You can allocate a `rows x cols` matrix as one block, index it, and free it — and explain why this beats `double**`.
- You can write the four memory bugs deliberately, compile with ASan, and match each report to its bug.
- You can write an arena allocator and explain why an ML inference engine would use one.
- You can tell from a function's comment who is responsible for freeing its result.

Next: `../07_structs_unions_enums/lesson.md` wraps `rows, cols, data*` into a proper `Matrix` type.
