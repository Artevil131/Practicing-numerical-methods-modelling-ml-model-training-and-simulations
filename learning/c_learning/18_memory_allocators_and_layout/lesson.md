# Chapter 18 — Memory Allocators and Layout

## What you'll be able to do after this chapter

- Explain what `malloc` actually does on macOS and Linux (size classes, headers, `mmap` vs `brk`), and predict when it is slow, when it fragments, and when it is the wrong tool.
- Write three allocators from scratch — an arena, a fixed-size pool, and a coalescing free-list allocator with boundary tags — with a heap checker that asserts every invariant, and know when each one is the right choice.
- Read `sizeof`/`offsetof` output and reorder struct fields to remove padding; request over-aligned memory correctly for SIMD and cache lines.
- Reason about the memory hierarchy with numbers: cache line size, L1/L2 sizes, DRAM latency vs bandwidth, TLB reach, page-fault cost — and restructure data (AoS → SoA) to hit those numbers.
- Use `mmap` for arenas and checkpoints, detect leaks and fragmentation with `leaks`, `malloc_zone_statistics`, and ASan, and put allocator callbacks and zero-copy strided views into your Matrix library.

## Why this matters for ML / numerics / sims

Every training step of an MLP allocates activations, gradients, and temporaries; every N-body frame allocates tree nodes; every tokenizer run allocates strings. With plain `malloc`/`free` each of those is a hash-table lookup, a lock, and a scattered heap — and a leak waiting for one missed `free`. Game engines and ML runtimes (PyTorch's caching allocator, ggml's context arena, every frame-based renderer) solve this the same way: allocate from an arena per frame/epoch, free everything at once. Memory *layout* is the other half: a matmul at 1 GFLOP/s and the same matmul at 50 GFLOP/s differ only in how the bytes are arranged and visited. Chapter 19 does the SIMD; this chapter is the memory it needs.

---

## 1. What `malloc` really does

`malloc(n)` (C11 §7.22.3) is a library function, not a system call. The C library keeps a *heap*: memory it obtained from the kernel in big chunks and hands out in small pieces.

```
   your program                 libc malloc                        kernel
   ─────────────                ───────────────────────            ──────────────────
   p = malloc(24)  ─────►  size class "32 B": pop a free slot
                            from a bucket of 32-byte slots        (nothing happens)
   q = malloc(40 MB) ────►  too big for the heap ─────────────►  mmap(): fresh pages
   free(q)          ────►  ────────────────────────────────────►  munmap(): returned
   free(p)          ────►  push slot back on the 32-B bucket      (nothing happens)
```

- **Getting memory from the kernel.** Linux glibc grows the heap with `brk`/`sbrk` (moves the end of the data segment) and uses `mmap` for large requests (default threshold 128 KB, `M_MMAP_THRESHOLD`). macOS `libmalloc` uses `mmap`/`vm_allocate` for everything, in regions of a few MB. Both keep freed memory around for reuse instead of returning it, so your process's RSS does not shrink when you `free`.
- **Size classes.** Small requests are rounded up to a class: on macOS 16-byte steps up to 1 KB ("tiny"), then 512-byte steps up to 32 KB ("small"), then "medium/large". `malloc_size(p)` (macOS) / `malloc_usable_size(p)` (glibc) shows what you really got. Example 9 in `example.c` prints: asked 24, got 32; asked 124, got 128; asked 724, got 768. The difference is *internal fragmentation*.
- **Headers.** glibc stores an 8/16-byte header before every block (size + flags); `free(p)` reads `p[-1]` to learn the size. That is why writing one byte before your allocation corrupts the heap and why `free` of a pointer you did not get from `malloc` is undefined behaviour (§7.22.3.3) that usually crashes *later*. macOS tiny/small allocators keep metadata out-of-line in a per-region bitmap, which is why heap overflows on macOS are sometimes silent until much later — run ASan (`../13_debugging_testing_perf/lesson.md`).
- **Thread safety.** `malloc` takes a lock or uses per-thread caches (glibc "arenas" — a different meaning of the word, `tcache`; macOS per-CPU magazines). It is thread-safe but not free: a tight loop calling `malloc` from 8 threads can spend most of its time in the allocator.
- **Cost.** A small `malloc`/`free` pair is roughly 20-50 ns when the fast path hits, hundreds of ns when it doesn't, plus the cache misses of touching cold heap metadata. Your arena below is ~1 ns.

Python equivalent: `PyObject_Malloc` is exactly this — pymalloc keeps size classes ("pools") of 8-byte steps up to 512 B and falls back to the system `malloc` above that. NumPy arrays are one `malloc` for the data buffer plus a small object header.

### 1.1 Seeing it on your machine

```c
#include <malloc/malloc.h>      // macOS. Linux: <malloc.h>, malloc_usable_size()
void *p = malloc(24), *q = malloc(1000), *r = malloc(40 << 20);
printf("%zu %zu %zu\n", malloc_size(p), malloc_size(q), malloc_size(r));   // 32 1024 41943040
printf("%p %p %p\n", p, q, r);
// 0x600000c0c020 0x6000021ac000 0x120008000   <- tiny and small regions vs a fresh mmap
```

The tiny region address (`0x6000...`) and the large one (`0x1200...`) are far apart: they came from different mechanisms. `vmmap <pid>` (macOS) / `cat /proc/<pid>/maps` (Linux) lists every region with its origin. `calloc(n, size)` for a large block is cheaper than `malloc` + `memset` because fresh `mmap` pages are already zero — the kernel supplies them lazily (§8) and libc knows it does not have to touch them.

### 1.2 What `free` costs, and why RSS does not shrink

`free(p)` for a small block pushes the slot onto the size class's free list — it does not return memory to the kernel. Large blocks *are* unmapped. So after allocating and freeing 1e6 small objects, `size_allocated` in `malloc_zone_statistics` stays high while `size_in_use` drops. That is by design: the next `malloc` is fast. Force a trim with `malloc_zone_pressure_relief(NULL, 0)` (macOS) or `malloc_trim(0)` (glibc) — rarely worth it.

## 2. Fragmentation

Two kinds, both measured in `example.c` section 3:

| Kind | What it is | Who causes it |
|------|------------|---------------|
| Internal | bytes inside a block the caller did not ask for (rounding to a size class, minimum block size) | the allocator's policy |
| External | enough free bytes in total, but no single hole big enough | the *program's* alloc/free pattern |

```
 heap:  [A 380][free 660][C 940][free 1220][E 1500][free 380]...   34 KB free in total
 request 17 KB  -> NULL: the largest hole is 3.3 KB.        (measured: fragmentation 90%)
```

A useful metric: `1 - largest_free_block / total_free_bytes` (0 = one big hole, →1 = many tiny holes). Long-running programs with mixed allocation lifetimes (a server, a simulation with dynamic particle counts) fragment; short-lived batch jobs rarely care. The cure is not a smarter general-purpose allocator — it is giving objects with the same lifetime the same allocator (section 4).

## 3. Alignment

Every type has an *alignment requirement* (C11 §6.2.8): an address a `T` may live at must be a multiple of `_Alignof(T)`. On arm64 and x86-64: `char` 1, `short` 2, `int`/`float` 4, `double`/pointers 8, `long double` 8 on arm64 macOS (16 on x86-64 Linux). `max_align_t` (`<stddef.h>`) is the strictest fundamental alignment — 8 on arm64 macOS, 16 on x86-64 — and `malloc` guarantees at least that (§7.22.3: "suitably aligned so that it may be assigned to a pointer to any type of object with a fundamental alignment requirement"). In practice macOS `malloc` returns 16-byte aligned memory.

Misaligned access: arm64 and x86 tolerate misaligned scalar loads (slower, and crossing a cache line costs two accesses), but it is still **undefined behaviour** (§6.3.2.3p7: converting to a pointer type whose alignment is not satisfied), and UBSan reports it as `misaligned address`. SIMD loads and atomics may fault outright. Never do `*(double *)(buf + 3)`; use `memcpy` (chapter 15).

```c
#include <stdalign.h>
printf("%zu %zu %zu\n", _Alignof(double), alignof(max_align_t), _Alignof(long double)); // 8 8 8 on arm64 macOS

typedef struct { _Alignas(64) double v[8]; } CacheLineVec;  // over-aligned: sizeof 64, alignof 64
double *a = aligned_alloc(64, 64 * 16);       // C11 §7.22.3.1: size should be a multiple of alignment
void *b; posix_memalign(&b, 4096, 1 << 16);   // POSIX: page-aligned
free(a); free(b);                              // both freed with free()
```

Over-alignment matters for: SIMD (NEON wants 16 B, AVX 32 B, AVX-512 64 B; unaligned loads work but aligned ones are cheaper and cross fewer cache lines), avoiding *false sharing* between threads (two counters in the same 64/128-byte line ping-pong between cores — `../16_concurrency_pthreads_and_atomics/lesson.md`), and DMA/page-granular I/O.

`aligned_alloc` with a size that is not a multiple of the alignment is undefined in C11 (relaxed in C17 to "may fail"); always round the size up. Objects with an alignment greater than `max_align_t` allocated on the stack (`_Alignas(64)` locals) and in static storage are fine; via `malloc` they are *not* guaranteed — use `aligned_alloc`.

## 4. Struct layout and padding

The compiler inserts padding so every member is aligned, and pads the tail so arrays of the struct keep the alignment (§6.7.2.1p15-17). Members are laid out in declaration order — the compiler will **not** reorder them for you (that would break ABI and `offsetof` expectations).

```c
typedef struct { char tag; double mass; int id; double radius; char alive; } BodyBad;   // 40 bytes
typedef struct { double mass; double radius; int id; char tag; char alive; } BodyGood;  // 24 bytes
//  BodyBad:  tag@0 [7 pad] mass@8 id@16 [4 pad] radius@24 alive@32 [7 pad]
//  BodyGood: mass@0 radius@8 id@16 tag@20 alive@21 [2 pad]
```

Rules of thumb: sort members by alignment, largest first; group `char`/`bool` flags together; put the fields the hot loop touches together (the *hot* part of a struct should fit in one cache line); consider bitfields or `uint8_t` enums for flags. `offsetof(T, member)` (`<stddef.h>`) tells you exactly where things landed — print it when in doubt (`example.c` section 4). `-Wpadded` (clang) warns on every padding byte inserted; useful once, too noisy to leave on. For 1e6 bodies, `BodyBad` → `BodyGood` is 38 MB → 23 MB and 40% less memory traffic in every loop.

`#pragma pack(1)` / `__attribute__((packed))` remove padding so the struct matches a file or wire format byte-for-byte — but then members are misaligned and taking their address is UB-adjacent; only use packed structs as an I/O staging buffer and `memcpy` fields out (`../08_file_io/lesson.md` did this by hand for the MNIST header — that is the safer approach).

## 5. The memory hierarchy in numbers

Measured/spec numbers for Apple M-series performance cores (this machine, M5; M1-M4 are similar within 2x). Check yours with `sysctl hw.cachelinesize hw.perflevel0.l1dcachesize hw.perflevel0.l2cachesize hw.pagesize`.

| Level | Size | Latency (approx) | Bandwidth (approx) | Line / unit |
|-------|------|------------------|--------------------|-------------|
| Registers | 31 GP + 32 NEON | 0 | — | 8 / 16 B |
| L1 data | 128 KB per P-core (64 KB E-core) | 3-4 cycles ≈ 1 ns | > 200 GB/s per core | 128 B line on Apple Silicon, 64 B on x86 |
| L2 | 16 MB shared by a 4-P-core cluster (6 MB E-cluster) | ~15 cycles ≈ 4-5 ns | ~100+ GB/s per core | 128 B |
| System-level cache (SLC) | 8-48 MB depending on chip | ~40 ns | | |
| DRAM (LPDDR5X) | 16-128 GB | ~100 ns | 100-200 GB/s (M1: 68, M1 Max: 400, M5: ~150) shared by everything; one core gets ~60-100 GB/s | page 16 KB (4 KB on x86 Linux) |
| SSD | | ~100 µs | 5-7 GB/s | |

Three consequences you will keep running into:

1. **Latency vs bandwidth.** A dependent chain of DRAM loads (linked list, tree walk, `a[idx[i]]`) runs at 1 access per ~100 ns = 80 MB/s. A streaming loop over an array runs at 60+ GB/s. Same bytes, 1000x difference. Pointer-chasing data structures are slow *because of latency*, not because of the pointers.
2. **Cache lines.** Memory moves in whole lines. Reading one `double` (8 B) of an 88-byte struct costs the whole 128-byte line. The `drift` pass in `example.c` section 5 moves 88 MB through the cache to update 48 MB of positions with AoS, and only the 6 arrays it touches with SoA — measured 19 ms vs 8 ms.
3. **Spatial and temporal locality.** Spatial: after you touch `a[i]`, `a[i+1]…a[i+15]` are already in L1 — walk memory in order (row-major `i, j` with `j` inner: `../04_arrays_and_strings/lesson.md`). Temporal: reuse data while it is still in cache — the whole point of matmul blocking (chapter 19). The hardware prefetcher detects sequential and constant-stride streams and runs ahead; it cannot predict random or pointer-dependent addresses.

Python equivalent: the reason `a.sum()` is fast and `sum(a)` over a Python list is 100x slower is exactly this — contiguous 8-byte doubles vs pointer-chasing to scattered `PyFloatObject`s.

## 6. Array-of-structs vs struct-of-arrays

```c
typedef struct { double x, y, z, vx, vy, vz, ax, ay, az, mass; int id; } BodyAoS;   // 88 B each
BodyAoS *bodies;                                            // AoS: bodies[i].x

typedef struct { size_t n; double *x, *y, *z, *vx, *vy, *vz, *ax, *ay, *az, *m; } BodiesSoA;  // SoA: s.x[i]
```

| | AoS | SoA |
|--|-----|-----|
| Loop that touches all fields of one body | good (one line holds the body) | scattered (10 streams) |
| Loop that touches 3 of 11 fields over all bodies | wastes 70% of every cache line | perfect: only 3 streams |
| Auto-vectorization of `x[j] - xi` | stride-88 gathers: usually not vectorized | contiguous loads: vectorized |
| Adding a field | free | one more array, one more `malloc` |
| Sorting / removing bodies | swap structs | swap in every array |

The measured force pass in `example.c` (N = 2048, all data in L2) shows SoA 1.3x faster at `-O2` — the gain there is the compiler's ability to vectorize, not bandwidth. The streaming `drift` pass shows 2.4x from bandwidth alone. Real codes often use **AoSoA** (arrays of small SIMD-width structs) to get both locality and vector loads:

```c
typedef struct { double x[4], y[4], z[4], m[4]; } Chunk4;     /* 128 bytes = one Apple cache line */
Chunk4 *chunks = aligned_alloc(128, nchunks * sizeof(Chunk4));
for (size_t c = 0; c < nchunks; c++)                          /* j-loop over chunks */
    for (int l = 0; l < 4; l++) {                             /* fixed 4: fully unrolled + vectorized */
        double dx = chunks[c].x[l] - xi, dy = chunks[c].y[l] - yi, dz = chunks[c].z[l] - zi;
        /* ... */
    }
```

One chunk holds four bodies' positions and masses in exactly one cache line, and the inner fixed-4 loop maps onto two `float64x2` NEON registers per coordinate (chapter 19). Sorting bodies spatially (Morton/Z-order) before chunking makes neighbours in memory neighbours in space — a Barnes–Hut tree then walks memory almost sequentially.

NumPy: a `(n, 3)` array is AoS; three `(n,)` arrays or a `(3, n)` array is SoA — `a[:, 0]` is the strided view that makes AoS-style access slow. `np.ascontiguousarray(a[:, 0])` is the copy that fixes it.

### 6.1 False sharing (the multi-thread layout bug)

Two threads writing two *different* variables that share a cache line force the line to bounce between cores on every write (the coherence protocol allows one writer at a time). `int counters[8]` updated by 8 threads is a 10-50x slowdown that looks like a scheduling problem. Fix: pad each thread's data to its own line — `struct { _Alignas(128) long count; }` — or accumulate in locals and combine once. Chapter 16 has the measurement; this is where the 128-byte number comes from.

## 7. Software prefetching

`__builtin_prefetch(addr, rw, locality)` (GCC/clang extension, emits `prfm` on arm64, `prefetcht0/1/2` on x86) asks the cache to start fetching a line you will need soon without stalling. It only helps when (a) the hardware prefetcher cannot predict the address — indirect indexing, tree nodes — and (b) you can compute the address far enough ahead (≥ latency / time-per-iteration iterations, typically 8-64).

```c
for (size_t i = 0; i < n; i++) {
    if (i + 32 < n) __builtin_prefetch(&a[idx[i + 32]], 0, 0);   // rw=0 read, locality=0 no reuse
    s += a[idx[i]];
}
// measured, 8M random reads over 512 MB: 51 ms plain, 27 ms with prefetch (-O2): 1.9x
```

Do not sprinkle prefetches through streaming loops — the hardware already does that and the extra instructions cost throughput. Measure; remove it if it does not help.

## 8. Pages, TLB, page faults, huge pages, NUMA

Virtual memory is mapped in **pages**: 16 KB on Apple Silicon (`sysconf(_SC_PAGESIZE)`), 4 KB on x86 Linux. The **TLB** caches virtual→physical translations for a few thousand pages (L1 dTLB ~ 128-256 entries, L2 TLB ~ 2-3k); a loop that touches one `double` per page (a column walk of a large row-major matrix, stride = row length) misses the TLB on every access: that is why the naive `ijk` matmul with a large N collapses (chapter 13 §10) — it is not just cache lines, it is translations.

The stride experiment that exposes both the cache line and the TLB (exercise 18.7):

```c
for (size_t stride = 1; stride <= 4096; stride *= 2) {        /* in doubles */
    double t0 = now(), s = 0;
    for (size_t i = 0; i < n; i += stride) s += a[i];          /* n = 32M doubles = 256 MB */
    printf("stride %5zu: %.2f ns/access\n", stride, (now() - t0) / (n / stride) * 1e9); sink(s);
}
// stride 1..16: ~0.3-0.6 ns   (same 128-byte line, prefetcher streaming)
// stride 16+:   jumps: every access is a new line
// stride 2048:  jumps again: every access is a new 16 KB page -> a TLB miss on top of the cache miss
```

**Page faults.** `malloc`/`mmap` of a large region does not give you memory — it gives you *address space*. The kernel maps a physical page the first time you touch it ("demand paging"), at ~0.1-1 µs per page plus zeroing. `example.c` section 7 measures `memset` of 256 MB twice: the first pass pays 16384 faults. This is why the first iteration of a benchmark is slow and why `calloc` of a huge block is "free" (the kernel gives zero pages lazily) while the first pass over it is not. Lesson: allocate once, reuse; touch memory in the thread that will use it.

**Huge pages.** 2 MB (x86) or 32 MB/1 GB pages cut TLB misses 512x for large arrays. Linux: `madvise(p, len, MADV_HUGEPAGE)` with transparent huge pages, or `mmap(..., MAP_HUGETLB)`. macOS: `mmap` with `VM_FLAGS_SUPERPAGE_SIZE_2MB` on Intel only; on Apple Silicon the 16 KB base page already gives 4x the TLB reach of x86, and the kernel manages large mappings itself. Know the concept; measure before caring.

**NUMA** (Non-Uniform Memory Access): on multi-socket servers, memory attached to the other socket is ~1.5-2x slower. Allocate and first-touch memory on the thread that will use it (`numactl --membind`, `libnuma`). Apple Silicon and laptops are single-node — irrelevant there, essential on a cluster node.

## 9. Allocator zoo — which one when

| Allocator | alloc / free cost | per-object overhead | frees individual objects? | fragmentation | use for |
|-----------|-------------------|---------------------|---------------------------|---------------|---------|
| system `malloc` | 20-200 ns | 0-16 B + size-class rounding | yes | internal + external | default; long-lived, mixed sizes |
| **arena / bump** | ~1 ns / free is a no-op | 0 | no — reset all at once | none (only alignment gaps) | per-frame temporaries, parsers/AST, request handlers, activations for one forward pass |
| **pool / free list of one size** | ~2 ns push/pop | 0 (intrusive) | yes | none | tree nodes, particles, hash-table entries, linked-list nodes |
| **general free list** (boundary tags) | 50-500 ns (search) | 32 B (hdr+ftr) | yes | both | teaching; embedded systems without libc |
| **buddy** | O(log n) split/merge | 1 bit per block | yes | internal (power-of-two rounding, up to 50%) | kernels (Linux page allocator), GPU memory managers |
| **slab** | ~pool | small | yes | none | kernel objects: a pool per type, with constructors/destructors, per-CPU caches |

The rest of this section walks through the three you build in `example.c`.

### 9.1 Arena (bump allocator)

```c
typedef struct { unsigned char *base; size_t cap, off; } Arena;

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    size_t start = (a->off + align - 1) & ~(align - 1);      // round up: align is a power of two
    if (start > a->cap || size > a->cap - start) return NULL; // overflow-safe bounds check
    a->off = start + size;
    return a->base + start;
}
#define ARENA_NEW(a, T, n) ((T *)arena_alloc_aligned((a), sizeof(T) * (size_t)(n), _Alignof(T)))
size_t arena_mark(const Arena *a)            { return a->off; }
void   arena_release(Arena *a, size_t mark)  { a->off = mark; }   // free everything since mark
void   arena_reset(Arena *a)                 { a->off = 0; }
```

Properties: allocation is three instructions; there is no `free`; memory is released in LIFO groups (`mark`/`release`) or all at once (`reset`); allocations are contiguous, which is itself a locality win.

A trace, 1 KB arena, three allocations then a release:

```
arena_init(1024)               off=0     |................................|
ARENA_NEW(double, 3)  -> +0    off=24    |dddddddddddddddddddddddd........|   24 bytes, align 8 ok
ARENA_NEW(char, 5)    -> +24   off=29    |ddd...dddccccc..................|   align 1: no gap
ARENA_NEW(int, 2)     -> +32   off=40    |ddd...dddccccc...iiiiiiii.......|   29 rounded up to 32: 3-byte gap
m = arena_mark()      -> 40
ARENA_NEW(double, 50) -> +40   off=440
arena_release(m)               off=40    the 400 bytes are "free" — nothing else happened
```

Three extensions worth writing (exercise 18.2):

```c
/* growable: when the block is full, chain a new one. A mark is then {block, off}. */
typedef struct ArenaBlock { struct ArenaBlock *prev; size_t cap, off; unsigned char mem[]; } ArenaBlock;

/* realloc in place if p was the most recent allocation — string builders love this */
void *arena_realloc_last(Arena *a, void *p, size_t old, size_t new_size) {
    if ((unsigned char *)p + old == a->base + a->off && a->off - old + new_size <= a->cap) {
        a->off += new_size - old; return p;               /* extend in place */
    }
    void *q = arena_alloc_aligned(a, new_size, 16); if (q) memcpy(q, p, old); return q;
}

/* debug poisoning: released memory is filled with 0xDD; reading it later is obvious in lldb */
#ifdef ARENA_DEBUG
#  define ARENA_POISON(a, from) memset((a)->base + (from), 0xDD, (a)->off - (from))
#else
#  define ARENA_POISON(a, from) ((void)0)
#endif
```

**How engines use it.** A game engine has a *frame arena*: everything allocated while rendering frame k is reset at the start of frame k+1 — no per-object frees, no leaks possible. A compiler has an arena per translation unit for the AST. ggml (the tensor library under llama.cpp) creates a `ggml_context` over one big buffer and bump-allocates every tensor and graph node in it; "freeing" the context frees the whole model. PyTorch's CUDA caching allocator keeps freed blocks in size-class pools per stream instead of calling `cudaFree`. For your MLP: allocate all activations for one forward/backward pass from an arena and `reset` after the optimizer step — the same buffers are reused every step, warm in cache, zero `malloc` calls in the training loop.

### 9.2 Fixed-size pool with an intrusive free list

```c
typedef struct FreeSlot { struct FreeSlot *next; } FreeSlot;
typedef struct { unsigned char *slots; size_t stride, count; FreeSlot *head; } Pool;

void *pool_alloc(Pool *p) { FreeSlot *s = p->head; if (!s) return NULL; p->head = s->next; return s; }
void  pool_free(Pool *p, void *ptr) { FreeSlot *s = ptr; s->next = p->head; p->head = s; }
```

"Intrusive" means the free-list `next` pointer lives *inside* the free slot itself — a slot is either an object or a link, never both, so the list costs zero extra memory. `stride` is `sizeof(T)` rounded up to `max(_Alignof(T), _Alignof(void *))` and at least `sizeof(void *)`. LIFO reuse means the object you just freed is the next one handed out — still hot in L1.

```
pool_init(4 slots)      head -> [0]->[1]->[2]->[3]->NULL      (links live in the slots' first 8 bytes)
a = pool_alloc()  = [0] head -> [1]->[2]->[3]->NULL
b = pool_alloc()  = [1] head -> [2]->[3]->NULL
pool_free(a)            head -> [0]->[2]->[3]->NULL           a's bytes now hold the link to [2]
c = pool_alloc()  = [0] head -> [2]->[3]->NULL                LIFO: c == a
```

Initialization cost is O(count) to thread the list. For huge pools, do it lazily: keep a `bump` index and take `slots[bump++]` while the free list is empty — the pool then costs nothing until used, and untouched pages are never faulted in (§8).

Where it fits: `TreeNode`s of a BST or Barnes–Hut quadtree (allocate the node pool once for the maximum N, rebuild the tree every frame by resetting), particles in a PIC simulation with birth/death, hash-table entries. Debug additions: a "magic" word written on free and checked on alloc (double-free detection), and `pool_free` asserting the pointer lies inside the slab on a slot boundary (`example.c` does this).

### 9.3 General-purpose free-list allocator with boundary tags

The textbook design (Knuth's boundary tags; CS:APP chapter 9). Every block carries a header and an identical footer; sizes are multiples of 16 so payloads are 16-byte aligned:

```
 heap: [prologue HDR][prologue FTR] [HDR|payload........|FTR] [HDR|payload|FTR] ... [epilogue HDR size=0]
 tag = { size_t size /* whole block, hdr+payload+ftr */ ; size_t used /* 0 or 1 */ }   16 bytes each

 next block  = blk + hdr(blk)->size
 prev block  = blk - ((FLTag *)(blk - 16))->size      <- the footer of the previous block makes this O(1)
```

- **malloc(n)**: `need = round_up(n + 32, 16)`, at least 48. Walk blocks from the first to the epilogue; **first-fit** returns the first free block with `size >= need`; **best-fit** keeps walking for the smallest one that fits (less external fragmentation, more time). If the chosen block has ≥ 48 spare bytes, **split** it: mark the front `need` bytes used and write a new free header/footer on the remainder.
- **free(p)**: header is at `p - 16`. Assert `used == 1` (double free) and `hdr->size == ftr->size` (buffer overflow corrupted the tags). Mark free, then **coalesce**: if the next block is free, absorb it (`size += next->size`); if the previous block is free (read its footer), let it absorb us. The prologue and epilogue are permanently "used" so no bounds checks are needed at the ends.
- **check()**: walk the heap asserting every block is 16-aligned, ≥ 48, header equals footer, no two adjacent free blocks (else coalescing is broken), and the sizes tile the heap exactly. Call it after *every* operation in tests. This is how you develop an allocator without losing your mind.

A trace on a 512-byte heap (sizes are whole blocks including 32 bytes of tags; `*` = used):

```
init                  [P][ free 464                                   ][E]
a = malloc(100) 144   [P][a*144][ free 320                           ][E]    split
b = malloc(200) 240   [P][a*144][b*240        ][ free 80             ][E]    split
free(a)               [P][ free 144][b*240    ][ free 80             ][E]    neighbours used: no merge
c = malloc(50)   96   [P][c*96][free 48][b*240][ free 80             ][E]    first-fit: a's hole, split (144-96=48 >= 48)
                      best-fit would pick the 80-byte tail hole instead and leave 144 intact
free(b)               [P][c*96][ free 368                            ][E]    merges with BOTH neighbours (48 + 240 + 80)
free(c)               [P][ free 464                                   ][E]    back to one block: check() passes
```

The core of `fl_free` is eight lines once the helpers exist:

```c
void fl_free(FreeList *fl, void *p) {
    unsigned char *b = (unsigned char *)p - 16;                       /* header */
    assert(hdr(b)->used && hdr(b)->size == ftr(b)->size);            /* double free / overflow */
    set_tags(b, hdr(b)->size, 0);
    unsigned char *nx = b + hdr(b)->size;                             /* next header */
    if (!hdr(nx)->used) set_tags(b, hdr(b)->size + hdr(nx)->size, 0);
    unsigned char *pv = b - ((FLTag *)(b - 16))->size;                /* previous header via its footer */
    if (!hdr(pv)->used) set_tags(pv, hdr(pv)->size + hdr(b)->size, 0);
}
```

Note the order: absorb the next block *first* (so its footer is now our footer), then let the previous block absorb the merged result. `set_tags` writes the footer at `b + size - 16`, which is why it must know the final size.

Measured in `example.c`: 64 allocations of 380-1500 bytes, free every other one → 33 holes, 34 KB free, largest hole 3.3 KB → a 17 KB request fails (external fragmentation 90%). Free the rest → exactly one free block spanning the heap (coalescing proven). Best-fit left 2 more holes intact than first-fit under the same small requests.

Making it production-grade means: an *explicit* free list (doubly-linked, only free blocks: allocation stops walking used blocks), then *segregated* free lists by size class (what real allocators do: O(1) allocation for small sizes), a footer only on free blocks (saves 16 B per used block), and growing the heap with `mmap` when the search fails. Each step is one exercise.

### 9.4 Buddy allocator (sketch)

Manage a region of size 2^K. A request for n bytes is rounded up to 2^k. If no free block of order k exists, take a block of order k+1 and split it into two "buddies" — the buddy of block at offset `o` (order k) is at `o ^ (1 << k)`. On free, if the buddy is also free, merge back to order k+1, and repeat. Free lists per order; one bit per block records "free". Allocation and free are O(log n) with **no external fragmentation among same-order blocks** and up to 50% internal fragmentation from the power-of-two rounding. Linux's physical page allocator is a buddy system (orders 0-10, 4 KB to 4 MB); CUDA's memory pools use it. Exercise 18.6 builds it.

### 9.5 Slab (idea)

A slab allocator (Bonwick 1994, Solaris/Linux kernel) is a pool per *type* with extras: objects are kept initialized between uses (constructor runs once, not per alloc), slabs are page-sized so the containing slab of any object is `addr & ~(PAGE-1)`, per-CPU "magazines" avoid locking, and coloring offsets objects in successive slabs so they do not all map to the same cache set. Your pool is a one-slab slab allocator.

## 10. Ownership patterns at scale

Chapter 06 taught "every `malloc` has one owner who calls `free`". At 10,000 allocations per frame that rule does not scale — humans cannot track it and `malloc` cannot keep up. The scalable patterns:

| Pattern | Rule | Example |
|---------|------|---------|
| Arena per lifetime | objects that die together are allocated together; free = reset | frame arena, per-request arena, per-epoch activations, per-parse AST |
| Pool per type | one allocator per hot type; objects returned individually or the pool dropped whole | tree nodes, particles, tokens |
| Handles instead of pointers | store `uint32_t` indices into a pool array; a generation counter detects stale handles | entity systems, autograd graph nodes |
| Owner struct | one `struct Model { Arena params; Arena acts; ... }` owns every buffer; `model_free` frees them all | your MLP / transformer |
| Allocator as a parameter | library code takes an `Allocator *` and never calls `malloc` directly | your Matrix library (§12) |

Handles deserve a note: `Node *` pointers into a growing `realloc`'d array are invalidated on growth; indices are not. An autograd tape is naturally a vector of nodes indexed by `uint32_t`; a Barnes–Hut tree is a pool of nodes with `uint32_t child[4]`. Indices are also half the size of pointers and trivially serializable (checkpointing!).

```c
typedef struct { uint32_t idx, gen; } Handle;                 /* 8 bytes, never dangles silently */
typedef struct { Vec nodes; Vec gens; Vec free_idx; } NodeStore;

Handle store_new(NodeStore *s) {
    uint32_t i = s->free_idx.len ? vec_pop_u32(&s->free_idx) : (vec_push_zero(&s->nodes), s->nodes.len - 1);
    return (Handle){ i, vec_at_u32(&s->gens, i) };
}
Node *store_get(NodeStore *s, Handle h) {                    /* NULL for a stale handle */
    return h.idx < s->nodes.len && vec_at_u32(&s->gens, h.idx) == h.gen ? vec_at_node(&s->nodes, h.idx) : NULL;
}
void store_free(NodeStore *s, Handle h) { vec_at_u32(&s->gens, h.idx)++; vec_push_u32(&s->free_idx, h.idx); }
```

How the big systems do it, so you recognise the pattern when reading their code:

| System | Pattern | Where to look |
|--------|---------|---------------|
| ggml / llama.cpp | one `ggml_context` = one arena; every tensor and graph node bump-allocated; `ggml_gallocr` plans reuse of activation buffers across the graph | `ggml.c`: `ggml_new_object`, `ggml-alloc.c` |
| PyTorch (CUDA) | caching allocator: freed blocks kept in per-stream size-class pools; `empty_cache()` is the "reset" | `c10/cuda/CUDACachingAllocator.cpp` |
| Game engines | frame arena + per-system pools; handles (index+generation) for entities | any ECS write-up; Unity's `NativeArray`/`Allocator.Temp` |
| Compilers (clang, Lua) | per-translation-unit / per-state arenas; Lua's `lua_Alloc` callback lets you plug in your own | `lstate.c`, `lmem.c` |
| SQLite | pluggable `sqlite3_mem_methods`; lookaside pools per connection for small objects | `mem1.c`, `malloc.c` |

## 11. Memory-mapped arenas and checkpointing

`mmap` (`<sys/mman.h>`, POSIX; `../17_posix_systems_programming/lesson.md`) maps file contents or anonymous zero pages into your address space:

```c
int fd = open("weights.bin", O_RDWR | O_CREAT, 0644);
ftruncate(fd, nbytes);                                            // size the file
double *w = mmap(NULL, nbytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
// ... train: w[] IS the file. The kernel pages it in on first touch and writes dirty pages back.
msync(w, nbytes, MS_SYNC);                                        // force the write-back now (checkpoint)
munmap(w, nbytes); close(fd);
```

Why this beats `fwrite`: loading a 4 GB model is instant (nothing is read until touched); only the pages you use are resident; several processes can share the same physical pages read-only (`MAP_PRIVATE`/`PROT_READ` — how llama.cpp loads GGUF files); a crash leaves at most the un-synced pages stale. An **anonymous** mapping (`MAP_ANONYMOUS`, `fd = -1`) is how to get a huge arena cheaply: reserve 64 GB of address space, pay only for pages you touch; `madvise(p, len, MADV_FREE)`/`MADV_DONTNEED` gives pages back after a `reset`. `example.c` section 7 uses an anonymous mapping to measure the page-fault cost.

Alignment bonus: `mmap` returns page-aligned memory, so any SIMD alignment you want is free.

## 12. Allocator hooks and zero-copy views in your Matrix library

Library code should not call `malloc` directly. Take an allocator as a vtable of callbacks (`../11_function_pointers_and_generics/lesson.md`):

```c
typedef struct {
    void *(*alloc)(void *ctx, size_t size, size_t align);
    void  (*free)(void *ctx, void *p, size_t size);       // size lets pools/arenas verify or no-op
    void  *ctx;                                           // the Arena*, Pool*, or NULL for heap
} Allocator;

typedef struct {
    size_t rows, cols;
    ptrdiff_t rs, cs;          // row stride, column stride in ELEMENTS (NumPy strides are in bytes)
    double *data;
    bool owns;                 // false for views
    const Allocator *al;
} Mat;
static inline double *mat_at(const Mat *m, size_t i, size_t j) { return &m->data[(ptrdiff_t)i * m->rs + (ptrdiff_t)j * m->cs]; }
```

With strides, these are O(1) and copy nothing: transpose (swap `rs`/`cs`), a row range (`data += r0 * rs`, `rows = r1 - r0`), a column (`cols = 1`, `data += j * cs`), a sub-block, every k-th row (`rs *= k`). A view has `owns = false` and its `release` is a no-op; writing through it writes the owner. This is exactly NumPy's model: `a.T`, `a[1:3]`, `a[:, 2]` are views with modified `shape`/`strides`; `a.flags['C_CONTIGUOUS']` is `cs == 1 && rs == cols`. Two rules NumPy also has: fast kernels (matmul, `memcpy`) require contiguity — check `mat_is_contiguous` and fall back or copy; and a view must not outlive its owner (dangling — chapter 06's use-after-free in a new costume). `example.c` section 8 shows a heap-backed and an arena-backed `Mat` behind the same interface, and a transposed view being written through.

Python equivalent: `np.ndarray(shape, strides=..., buffer=...)`, `torch.as_strided`. PyTorch's `Tensor` is exactly `{storage*, offset, sizes, strides}`.

The views themselves, to show how little code they are:

```c
Mat mat_transpose_view(const Mat *m) { Mat t = *m; t.rows = m->cols; t.cols = m->rows; t.rs = m->cs; t.cs = m->rs; t.owns = false; return t; }
Mat mat_rows_view(const Mat *m, size_t r0, size_t r1) { Mat v = *m; v.rows = r1 - r0; v.data = mat_at(m, r0, 0); v.owns = false; return v; }
Mat mat_block_view(const Mat *m, size_t r0, size_t c0, size_t h, size_t w) { Mat v = *m; v.rows = h; v.cols = w; v.data = mat_at(m, r0, c0); v.owns = false; return v; }
Mat mat_every_kth_row(const Mat *m, size_t k) { Mat v = *m; v.rows = (m->rows + k - 1) / k; v.rs = m->rs * (ptrdiff_t)k; v.owns = false; return v; }

/* a: 4x3 strides (3,1) contiguous   aT: 3x4 strides (1,3) not contiguous   a[:,2]: 4x1 strides (3,1) */
```

Two things fall out of strides for free. First, a **batch dimension**: a `(batch, rows, cols)` tensor is `Mat` plus a batch stride, and `mat_rows_view` over a flattened `(batch*rows, cols)` matrix gives one sample without copying — your minibatch loop never copies data. Second, **in-place transposed matmul** (`C = A^T B`) needs no transpose pass: hand `mat_matmul` the view; only when you need the fast contiguous kernel do you materialise a copy. That is what `torch.Tensor.contiguous()` does and why it exists.

A counting allocator is the simplest way to make "zero allocations in the training loop" a *test*:

```c
typedef struct { const Allocator *inner; size_t calls, bytes, live; } Counting;
static void *counting_alloc(void *ctx, size_t sz, size_t al) {
    Counting *c = ctx; c->calls++; c->bytes += sz; c->live += sz;
    return c->inner->alloc(c->inner->ctx, sz, al);
}
/* ... in the test: run one training step; assert(counting.calls == 0); */
```

## 13. Stack size and deep recursion

The main thread's stack is 8 MB by default on macOS and Linux (`ulimit -s` → 8192 KB; `getrlimit(RLIMIT_STACK)` in code). Other pthreads get 512 KB on macOS (8 MB on Linux) unless you set `pthread_attr_setstacksize`. A recursive function with a 64-byte frame overflows 8 MB after ~130k calls; quicksort on sorted input, a naive recursive tree free on a degenerate tree, or a recursive descent over a long token list will hit it. The symptom is `EXC_BAD_ACCESS` / `SIGSEGV` with a huge backtrace in lldb; ASan reports `stack-overflow`.

```c
static size_t depth(const TreeNode *n) { return n ? 1 + depth(n->left) : 0; }   /* degenerate tree: depth == N */
// N = 1e6 nodes inserted in sorted order -> a linked list -> ~1e6 frames * 48 B = 48 MB > 8 MB
// $ ./prog
// zsh: segmentation fault  ./prog          (ASan: ==ERROR: AddressSanitizer: stack-overflow on address ...)
// $ ulimit -s 65532 && ./prog             (macOS hard cap 64 MB) — works, this time
```

Fixes, in order of preference: make the algorithm iterative with an explicit stack (a `Vec` of frames — chapter 10); bound the recursion (quicksort: recurse on the smaller half, loop on the larger → O(log n) depth); run the work on a thread with a bigger stack (`pthread_attr_setstacksize(&attr, 256 << 20)`); raise the limit — `ulimit -s unlimited` in the shell (Linux; macOS caps at 64 MB), or at link time `cc ... -Wl,-stack_size,0x10000000` (macOS, 256 MB main-thread stack; Linux equivalent `-Wl,-z,stacksize=...` is honoured only for some loaders — prefer `ulimit`). Never put large arrays on the stack (`double buf[1 << 20]` = 8 MB = instant overflow): heap or `static`.

## 14. Detecting leaks and fragmentation

| Tool | Command (macOS) | Linux equivalent |
|------|-----------------|------------------|
| Leak check at exit | `leaks --atExit -- ./prog` (add `MallocStackLogging=1` env for allocation backtraces) | `-fsanitize=leak` or `valgrind --leak-check=full` |
| Heap overflow / UAF | `cc -g -O1 -fsanitize=address -fno-omit-frame-pointer` | same |
| Heap statistics in code | `malloc_zone_statistics(NULL, &st)` → `blocks_in_use`, `size_in_use`, `size_allocated` (`<malloc/malloc.h>`) | `mallinfo2()` (`<malloc.h>`), `malloc_stats()` |
| Snapshot / diff over time | `heap ./prog` or `heap <pid>`; Instruments → Allocations template (`xcrun xctrace record --template 'Allocations' --launch ./prog`) | `valgrind --tool=massif`, `heaptrack` |
| Debug malloc | `MallocScribble=1` (fill freed memory with 0x55), `MallocPreScribble=1` (0xAA on alloc), `MallocGuardEdges=1` | `MALLOC_PERTURB_=165`, `MALLOC_CHECK_=3` |
| Per-block size | `malloc_size(p)` | `malloc_usable_size(p)` |

What a `leaks` report looks like, and how to read it:

```
$ MallocStackLogging=1 leaks --atExit -- ./ex_demo
Process 4242: 190 nodes malloced for 28 KB
Process 4242: 1 leak for 8192 total leaked bytes.
    1 (8.00K) ROOT LEAK: 0x13a808200 [8192]
        Call stack: ... | main | demo_matrix | mat_new | heap_alloc | aligned_alloc
```

"ROOT LEAK" = nothing points to it any more; the call stack (only with `MallocStackLogging=1`) is the allocation site, which is what you need — the *missing* `free` has no line number. `leaks` finds blocks unreachable from roots; a block that is still referenced by a global you forgot to clear is not a leak to `leaks` but is to you — that is what a growing `size_in_use` over time reveals.

LeakSanitizer is **not available on Apple Silicon**; `leaks` is the tool. Fragmentation shows up as `size_allocated` (held from the OS) growing while `size_in_use` stays flat — log the two every N steps in a long simulation. For your own allocators, expose the statistics struct (`example.c`'s `FLStats`) and assert on them in tests.

## 15. A note on `realloc` and growth

`realloc(p, n)` (§7.22.3.5) may move the block: every pointer into the old block is dangling afterwards, and on failure it returns `NULL` and leaves the old block valid (do not do `p = realloc(p, n)` without a temporary). Growth factor 1.5-2x gives amortized O(1) appends (chapter 10). With an arena, `realloc` of the *most recent* allocation can extend in place for free — worth implementing for string builders and token buffers.

---

## Gotchas and undefined behavior

- Reading padding bytes is *unspecified* (they have indeterminate values); `memcmp` of two structs is therefore wrong even when all members are equal. Compare members, or `memset(&s, 0, sizeof s)` before filling and document it.
- `aligned_alloc(align, size)` with `size` not a multiple of `align`: UB in C11, "may fail" in C17. `align` must be a power of two supported by the implementation. Free with `free()`, never with a custom free that assumes a header layout.
- Casting a `char *` at an arbitrary offset to `double *` and dereferencing: UB (§6.3.2.3p7) even on hardware that tolerates it; UBSan flags it. Use `memcpy` or ensure alignment (your arena's `align_up` exists for this).
- Pointer comparison across different allocations (`p < q` where they come from different `malloc`s) is UB (§6.5.8p5); use `uintptr_t` if you must, and know it is implementation-defined. Inside one arena/pool slab it is fine — that is why the pool's bounds assert is legal.
- `mark`/`release` discipline: releasing to a mark while a *later* mark is live corrupts everything allocated after it. Marks are a stack; release in reverse order.
- A pool of `stride < sizeof(void *)` cannot store the free-list link — round up. A pool alignment smaller than `_Alignof(FreeSlot)` misaligns the link.
- Arena memory is not zeroed on `reset`. If callers assume zeroed memory (they will), `memset` in `ARENA_NEW` or provide `arena_calloc`.
- Views outliving owners; `realloc` invalidating views; a view into an arena after `reset`: all use-after-free. ASan catches the heap case; for arenas, poison on release in debug builds.
- `mmap` failure returns `MAP_FAILED` (`(void *)-1`), **not** `NULL`. `munmap` needs the same length. Writing past a `ftruncate`d file's mapping raises `SIGBUS`, not `SIGSEGV`.
- Signed `int` for sizes and offsets: 2 GB arrays overflow it (`../12_numbers_bits_floats/lesson.md`); use `size_t`/`ptrdiff_t`. Strides can legitimately be negative (reversed view) — `ptrdiff_t`, not `size_t`.

## Common mistakes checklist

- [ ] Calling `malloc` inside a hot loop (per node, per particle, per token) — allocate from a pool/arena.
- [ ] Freeing arena- or pool-owned memory with `free()` (heap corruption) — the `Allocator` struct exists so the right `free` is always paired.
- [ ] Struct members in "logical" order instead of alignment order — check `sizeof`, aim for no padding.
- [ ] `-O2` benchmark where the first pass includes page faults — warm up, then time.
- [ ] Measuring "AoS vs SoA" on data that fits in L1 and concluding layout doesn't matter — use realistic N.
- [ ] Assuming `free` shrinks RSS — it usually does not; measure with `malloc_zone_statistics`.
- [ ] Missing `fl_check()` after every operation while developing an allocator — you cannot debug heap corruption after the fact.
- [ ] Deep recursion on user-sized inputs (tree free, quicksort, parser) without a depth bound.
- [ ] Keeping raw pointers into a growable array instead of indices.
- [ ] Forgetting `msync` before assuming a checkpoint is on disk.

## You can move on when...

- You can draw the boundary-tag heap after `malloc(100); malloc(200); free(first); malloc(50)` and say which block each policy picks and what coalesces on the next `free`.
- Your `fl_check()` catches a deliberately injected bug (skip coalescing with the previous block) on the first test.
- You can state your machine's cache line size, L1/L2 sizes, page size, and DRAM latency, and explain why a linked list of 1e7 nodes is 100x slower to sum than an array of 1e7 doubles.
- You can turn a `BodyAoS` loop into SoA, measure the difference, and explain which loops benefit and which do not.
- Your Matrix library takes an `Allocator *`, has `transpose`/`rows`/`col` views with strides, and its tests pass with the heap allocator *and* with an arena.
- You can build a 2 GB anonymous `mmap` arena, explain why it costs nothing until touched, and show the page-fault cost with a timer.
