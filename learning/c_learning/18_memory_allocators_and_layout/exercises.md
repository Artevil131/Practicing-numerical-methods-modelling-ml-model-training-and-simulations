# Chapter 18 — Exercises

Write each exercise as `ex18_K.c` in this folder (`ex18_1.c`, `ex18_2.c`, ...). Default compile:
`cc -Wall -Wextra -std=c11 -O2 -o ex18_K ex18_K.c -lm`. For every allocator you write, also build and
run with `-g -O1 -fsanitize=address,undefined` and, on macOS, `leaks --atExit -- ./ex18_K`. Every
allocator must have a `check()` function that asserts its invariants; call it after every operation
in your tests. Timed exercises: warm up, repeat, report the median (chapter 13).

---

### 18.1 — **Padding detective**

Define five structs mixing `char`, `short`, `int`, `double`, `float`, pointers and a `char[3]` array, in deliberately bad member orders. For each, print `sizeof`, `_Alignof`, and `offsetof` of every member in a table, then write the reordered version and print the same. Report total padding bytes saved for 1e6 elements. Compile once with `-Wpadded` and confirm it reports every padding byte you found by hand.

Example:
```
struct A (bad):  size 40 align 8   tag@0 mass@8 id@16 radius@24 alive@32   padding 18
struct A (good): size 24 align 8   mass@0 radius@8 id@16 tag@20 alive@21   padding 2
```

<details><summary>Hint</summary>
Padding = sizeof − sum of member sizes. Members sorted by descending alignment never need interior padding; only tail padding can remain.
</details>

### 18.2 — **Arena with mark/release, growth, and poisoning**

Implement `Arena` with `arena_alloc_aligned`, `ARENA_NEW`, `arena_mark`, `arena_release`, `arena_reset`, and two extras: (a) when a block is full, allocate a new block (at least 2x the request) and link it — `release` must handle marks that point into earlier blocks; (b) under `-DARENA_DEBUG`, fill released memory with `0xDD` and assert in `alloc` that the bytes about to be handed out are still `0xDD` (catches writes after release). Test: 1e6 allocations of random sizes 1-200 across 1000 frames; total `malloc` calls must be < 30.

Example:
```
frames 1000, allocs 1000000, blocks allocated 12, high water 1.9 MB, malloc calls 12
```

<details><summary>Hint</summary>
Store `{base, cap, off, prev_block}` per block; a mark is `{block*, off}`. Release walks back freeing (or caching) blocks newer than the mark's block.
</details>

### 18.3 — **Pool with double-free detection and generation handles**

Write a fixed-size `Pool` with an intrusive free list. Add: a magic value `0xFEEDFACE` written in the second word of every free slot and checked in `pool_free` (assert on double free); a `uint16_t generation` per slot incremented on free; and 32-bit handles `(slot_index << 16) | generation` with `pool_get(handle)` returning `NULL` for a stale handle. Test all three failure modes deliberately and show the assertion / NULL.

Example:
```
h1 = alloc -> slot 0 gen 0; free(h1); h2 = alloc -> slot 0 gen 1
pool_get(h1) -> NULL (stale), pool_get(h2) -> valid
free(h1) again -> Assertion failed: (slot_is_used(...)), double free
```

<details><summary>Hint</summary>
A slot needs to be at least 8 bytes for {next, magic} — round `stride` up. Generation must live outside the object bytes (a parallel `uint16_t` array) so live objects can use all their bytes.
</details>

### 18.4 — **Boundary-tag allocator, first-fit vs best-fit**

Implement the implicit-list allocator from the lesson (16-byte tags, prologue/epilogue, split, coalesce both ways, `fl_check`). Run the same random workload (10,000 ops: alloc 1-2000 bytes with probability 0.6, free a random live block with 0.4) under first-fit and best-fit and print, at the end: number of live blocks, total free bytes, largest free block, fragmentation ratio, and total nanoseconds spent inside the allocator. Then inject a bug (skip coalescing with the previous block) and show `fl_check` catching it on which operation number.

Example:
```
first-fit: live 2388  free 1.3 MB  largest 88 KB  frag 0.93  time 41 ms
best-fit : live 2388  free 1.3 MB  largest 176 KB frag 0.86  time 118 ms
bug injected: Assertion failed: (!(prev_free && !h->used)) at op 17
```

<details><summary>Hint</summary>
Give the allocator a 4 MB static `_Alignas(16)` array. Keep a `void *live[]` array of outstanding pointers so you can free a random one. Time with `clock_gettime` around each call and sum.
</details>

### 18.5 — **Explicit and segregated free lists**

Extend 18.4: (a) an explicit doubly-linked free list threaded through free blocks' payloads (allocation walks only free blocks); (b) segregated lists — 8 size classes (≤48, ≤64, ≤128, ≤256, ≤512, ≤1024, ≤4096, larger) so allocation is O(1) for small requests. Keep `fl_check` and add a check that every free block is on exactly one list. Re-run the 18.4 workload and report the speedup.

Example:
```
implicit first-fit: 41 ms   explicit: 9 ms   segregated: 2.1 ms   (all pass check after every op)
```

<details><summary>Hint</summary>
Minimum block becomes 16 + 16 (prev/next) + 16 = 48 — the same as before, conveniently. On coalesce, unlink both neighbours before merging, then link the merged block.
</details>

### 18.6 — **Buddy allocator**

Implement a binary buddy allocator over a 1 MB region with minimum block 32 bytes (orders 5..20): free lists per order, split on demand, merge with the buddy (`offset ^ (1 << order)`) on free, one bit per block to record free/used at each order. Provide `buddy_check` (every free block is on the list for its order; no two free buddies coexist). Measure internal fragmentation for requests of 33, 65, 129, ... bytes and for the workload from 18.4.

Example:
```
request 33 -> block 64 (48% waste)   request 1025 -> block 2048 (50% waste)
workload: 10,000 ops, requested 2.1 MB total, granted 3.3 MB (internal frag 36%)
```

<details><summary>Hint</summary>
Index blocks by `(order, offset >> order)`. A block's buddy has the same order and its offset differs in exactly bit `order`. Merge only if the buddy is free *and* the same order (not already split).
</details>

### 18.7 — **Memory hierarchy measurements**

Measure and print, in a table: (a) random-access latency vs working-set size (pointer chase through a shuffled cycle of 1 KB … 1 GB — the "latency staircase"; identify L1, L2, SLC, DRAM); (b) sequential read bandwidth for the same sizes; (c) stride experiment: sum every k-th `double` of a 256 MB array for k = 1, 2, 4, … 1024 and show the knee at the cache line and again at the page size (TLB); (d) `__builtin_prefetch` on a random gather with look-ahead 0, 4, 16, 64. Compare with `sysctl hw.cachelinesize hw.perflevel0.l1dcachesize hw.perflevel0.l2cachesize hw.pagesize`.

Example:
```
size      latency   bandwidth
 32 KB     1.1 ns    180 GB/s
  4 MB     4.8 ns     95 GB/s
 64 MB    38.2 ns     70 GB/s
  1 GB   105.7 ns     62 GB/s
stride: 1 x1.0 | 8 x1.1 | 16 x2.0 (128 B line) | 2048 x9 (16 KB page: TLB) ...
```

<details><summary>Hint</summary>
For latency, build a random permutation cycle `next[i]` and run `i = next[i]` 1e7 times — one dependent load per step. For bandwidth, sum an array; consume the sum. Repeat and take the median.
</details>

### 18.8 — **N-body: AoS, SoA, AoSoA (sim goal)**

Implement the softened direct-sum force kernel three ways: AoS (`Body` struct with 11 fields), SoA (separate arrays), and AoSoA (`struct { double x[4], y[4], z[4], m[4]; }` chunks). For N = 512, 2048, 8192, time one force evaluation of each, compute pair-interactions/s, and verify the three produce identical accelerations to 1e-9. Compile also with `-O3 -march=native -Rpass=loop-vectorize` and note which inner loops the compiler vectorized in each layout. Then time the leapfrog *drift* pass over 4M bodies in AoS vs SoA and compute effective GB/s from the bytes each layout must touch.

Example:
```
N=8192  AoS 91 ms (0.74 Gpair/s)  SoA 62 ms (1.08)  AoSoA 55 ms (1.22)  max|diff| 3e-12
drift 4M x10: AoS 82 ms (43 GB/s)  SoA 31 ms (62 GB/s)
```

<details><summary>Hint</summary>
Allocate all arrays 64-byte aligned from one arena. In AoSoA the j-loop runs over chunks and an inner fixed-4 loop the compiler unrolls fully.
</details>

### 18.9 — **Matrix library: allocator callbacks and strided views (ML goal)**

Add to your P02 `matrix.h`: an `Allocator` vtable (`alloc(ctx,size,align)`, `free(ctx,p,size)`, `ctx`), `mat_new(al, rows, cols)` (64-byte aligned), strides `rs`/`cs` in elements, and views: `mat_t(view)`, `mat_rows(m, r0, r1)`, `mat_col(m, j)`, `mat_block(m, r0, c0, h, w)`, `mat_every_kth_row(m, k)`. `mat_matmul` must accept strided inputs (use `mat_at`) and take a fast contiguous path when both are contiguous (assert the two paths agree). Provide three allocators — heap, arena, and a counting allocator that records bytes/calls — and run your whole P02 test suite once with each. Print the counting allocator's totals: a training step of a 784-256-10 MLP must make zero heap allocations when backed by an arena that is reset every step.

Example:
```
tests: 41 passed (heap)  41 passed (arena)  41 passed (counting: 128 allocs, 9.4 MB)
mlp step with arena: 0 malloc calls, arena high water 2.3 MB, reset per step
```

<details><summary>Hint</summary>
A view copies the `Mat` struct, sets `owns=false`, and adjusts `data`/`rows`/`cols`/`rs`/`cs`. `mat_release` on a view must be a no-op. Contiguity: `cs == 1 && rs == cols`.
</details>

### 18.10 — **mmap-backed checkpointing and a Barnes–Hut node pool (sim + ML goal)**

Two parts. (a) Checkpoint: store your MLP's parameters in a struct-of-arrays layout inside one file-backed `mmap` (`MAP_SHARED`), with a 64-byte header (magic, version, layer count, shapes). Train for 10 steps, `msync`, kill the process with `raise(SIGKILL)` after step 15 without syncing, restart, and show the loaded parameters equal the step-10 checkpoint (print a checksum). Measure load time for a 100 MB checkpoint via `mmap` vs `fread`. (b) Node pool: build the quadtree for a Barnes–Hut N-body step (P15) from a `Pool` of nodes sized 4N, reset per frame; compare frames/s against `malloc`/`free` per node for N = 50,000, and report `malloc_zone_statistics` (or `mallinfo2`) before and after 100 frames for both to show the pool version's heap does not grow.

Example:
```
checkpoint: 100 MB  mmap load 0.4 ms (lazy)  fread load 38 ms  checksum after crash-restart matches step 10
quadtree N=50000: malloc/free 21 fps, heap +14 MB after 100 frames; pool 34 fps, heap +0 MB
```

<details><summary>Hint</summary>
`ftruncate` the file to the right size before mapping; write the header last so a torn file is detectable. For the pool, store children as `uint32_t` indices, not pointers — the checkpoint format for the tree becomes trivial.
</details>
