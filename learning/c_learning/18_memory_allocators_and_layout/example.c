/*
 * Chapter 18 — Memory Allocators and Layout
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 *
 * Also try:
 *   cc -Wall -Wextra -std=c11 -O3 -march=native -o ex_demo example.c -lm      (SoA loops vectorize)
 *   cc -Wall -Wextra -std=c11 -g -O1 -fsanitize=address,undefined -o ex_demo example.c -lm
 *   leaks --atExit -- ./ex_demo             (macOS; LeakSanitizer is not available on Apple Silicon)
 *   MallocStackLogging=1 leaks --atExit -- ./ex_demo   (leaks with allocation backtraces)
 *
 * Contents (each section is a function called from main, in this order):
 *   1. arena_*   — bump allocator with alignment, mark/release, reset (memory per frame/epoch)
 *   2. pool_*    — fixed-size pool with an intrusive free list (tree nodes, particles)
 *   3. fl_*      — general-purpose allocator: implicit block list with boundary tags,
 *                  first-fit or best-fit, split on alloc, coalesce on free, a heap checker
 *                  that asserts every invariant, and fragmentation statistics
 *   4. layout    — struct padding, offsetof, reordering fields to shrink a struct,
 *                  _Alignof, aligned_alloc / posix_memalign, over-aligned structs
 *   5. AoS vs SoA— an N-body force pass and a streaming integrate pass, timed both ways
 *   6. prefetch  — random gather with and without __builtin_prefetch, timed
 *   7. pages     — an anonymous mmap arena: first touch (page faults) vs second touch, timed
 *   8. Matrix    — allocator callbacks (Allocator vtable) and zero-copy strided views
 *   9. limits    — stack size from getrlimit, malloc statistics from the system allocator
 *
 * Every timed section consumes its results (checksum printed) so -O2 cannot delete the work.
 * Times are wall clock on this machine and will differ on yours; the ratios are the point.
 *
 * Sandbox note: section 7 uses anonymous mmap (no file). If your environment forbids mmap the
 * section prints a message and continues; everything else is plain malloc.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include <assert.h>
#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>                 /* sysconf */

#if defined(__APPLE__)
#include <malloc/malloc.h>          /* malloc_zone_statistics, malloc_size */
#elif defined(__GLIBC__)
#include <malloc.h>                 /* mallinfo2, malloc_usable_size */
#endif

/* ------------------------------------------------------------------------------------------ */
/* timing helper (chapter 13): monotonic clock, seconds as double                             */
/* ------------------------------------------------------------------------------------------ */
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* round x up to a multiple of `align`, which must be a power of two */
static inline size_t align_up(size_t x, size_t align) {
    assert(align != 0 && (align & (align - 1)) == 0);   /* power of two */
    return (x + align - 1) & ~(align - 1);
}

/* ========================================================================================== */
/* 1. ARENA (bump) ALLOCATOR                                                                  */
/*                                                                                            */
/*   base                       off                                        cap                */
/*   |=========used=============|...............free........................|                */
/*   alloc: round `off` up to the requested alignment, hand out [off, off+size), bump off.    */
/*   free:  there is no per-object free. arena_reset() frees everything in O(1).              */
/*   mark/release: a stack discipline — release() rewinds to a saved offset.                  */
/* ========================================================================================== */
typedef struct {
    unsigned char *base;
    size_t cap;
    size_t off;
    size_t high_water;      /* statistics: maximum `off` ever reached */
} Arena;

static bool arena_init(Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return false;
    a->cap = cap;
    a->off = 0;
    a->high_water = 0;
    return true;
}

static void arena_destroy(Arena *a) {
    free(a->base);
    *a = (Arena){0};
}

/* returns NULL when the arena is exhausted — callers decide whether that is fatal */
static void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    size_t start = align_up(a->off, align);
    if (start > a->cap || size > a->cap - start) return NULL;   /* overflow-safe check */
    a->off = start + size;
    if (a->off > a->high_water) a->high_water = a->off;
    return a->base + start;
}

/* the macro every caller uses: type-correct size and alignment in one place */
#define ARENA_NEW(a, T, n) ((T *)arena_alloc_aligned((a), sizeof(T) * (size_t)(n), _Alignof(T)))

static size_t arena_mark(const Arena *a) { return a->off; }
static void   arena_release(Arena *a, size_t mark) { assert(mark <= a->off); a->off = mark; }
static void   arena_reset(Arena *a) { a->off = 0; }

static void demo_arena(void) {
    puts("== 1. arena (bump) allocator ==");
    Arena a;
    if (!arena_init(&a, 1 << 20)) { perror("arena_init"); return; }

    /* a "frame": allocate a bunch of temporaries, then throw all of them away at once */
    for (int frame = 0; frame < 3; frame++) {
        size_t m = arena_mark(&a);
        double *scratch = ARENA_NEW(&a, double, 1000);
        char   *name    = ARENA_NEW(&a, char, 17);
        int    *ids     = ARENA_NEW(&a, int, 250);
        assert(scratch && name && ids);
        /* alignment is a property of the address: check it with a cast to uintptr_t */
        assert(((uintptr_t)scratch % _Alignof(double)) == 0);
        assert(((uintptr_t)ids % _Alignof(int)) == 0);
        snprintf(name, 17, "frame-%d", frame);
        for (int i = 0; i < 1000; i++) scratch[i] = i * 0.5;
        printf("  frame %d: %s, used %zu bytes (ids at offset %td)\n",
               frame, name, a.off, (unsigned char *)ids - a.base);
        arena_release(&a, m);                     /* O(1) free of everything in the frame */
        assert(a.off == m);
    }
    /* over-aligned request: 64-byte alignment for a SIMD/cache-line buffer */
    float *simd_buf = arena_alloc_aligned(&a, 256 * sizeof(float), 64);
    assert(simd_buf && ((uintptr_t)simd_buf % 64) == 0);
    /* exhaustion returns NULL instead of writing past the end */
    assert(arena_alloc_aligned(&a, a.cap, 1) == NULL);
    printf("  high water mark: %zu bytes of %zu\n", a.high_water, a.cap);
    arena_reset(&a);                              /* end of epoch: everything gone, memory kept */
    assert(a.off == 0 && ARENA_NEW(&a, double, 1) == (double *)a.base);
    arena_destroy(&a);
}

/* ========================================================================================== */
/* 2. FIXED-SIZE POOL with an intrusive free list                                             */
/*                                                                                            */
/*   slots: [ 0 ][ 1 ][ 2 ][ 3 ]...   every slot is `stride` bytes, stride >= sizeof(void*)   */
/*   free slots store a pointer to the next free slot IN THEIR OWN FIRST BYTES (intrusive):   */
/*        head -> slot3 -> slot0 -> slot7 -> NULL                                             */
/*   alloc = pop head (O(1)), free = push (O(1)). Zero per-object overhead, zero fragmentation.*/
/* ========================================================================================== */
typedef struct FreeSlot { struct FreeSlot *next; } FreeSlot;

typedef struct {
    unsigned char *slots;
    size_t stride;          /* bytes per slot, rounded up for alignment */
    size_t count;
    FreeSlot *head;
    size_t in_use;
} Pool;

static bool pool_init(Pool *p, size_t obj_size, size_t obj_align, size_t count) {
    size_t align = obj_align > _Alignof(FreeSlot) ? obj_align : _Alignof(FreeSlot);
    size_t stride = align_up(obj_size < sizeof(FreeSlot) ? sizeof(FreeSlot) : obj_size, align);
    p->slots = aligned_alloc(align, align_up(stride * count, align));
    if (!p->slots) return false;
    p->stride = stride;
    p->count = count;
    p->in_use = 0;
    /* thread every slot onto the free list, in address order */
    p->head = NULL;
    for (size_t i = count; i-- > 0;) {
        FreeSlot *s = (FreeSlot *)(p->slots + i * stride);
        s->next = p->head;
        p->head = s;
    }
    return true;
}

static void *pool_alloc(Pool *p) {
    FreeSlot *s = p->head;
    if (!s) return NULL;                       /* pool exhausted */
    p->head = s->next;
    p->in_use++;
    return s;
}

static void pool_free(Pool *p, void *ptr) {
    if (!ptr) return;
    /* sanity: pointer must be inside the slab and on a slot boundary */
    unsigned char *b = ptr;
    assert(b >= p->slots && b < p->slots + p->stride * p->count);
    assert(((size_t)(b - p->slots)) % p->stride == 0);
    FreeSlot *s = ptr;
    s->next = p->head;
    p->head = s;
    p->in_use--;
}

static void pool_destroy(Pool *p) { free(p->slots); *p = (Pool){0}; }

typedef struct TreeNode {
    double key;
    struct TreeNode *left, *right;
} TreeNode;

static TreeNode *tree_insert(Pool *pool, TreeNode *root, double key) {
    if (!root) {
        TreeNode *n = pool_alloc(pool);
        if (!n) return NULL;
        *n = (TreeNode){ .key = key };
        return n;
    }
    if (key < root->key) root->left = tree_insert(pool, root->left, key);
    else                 root->right = tree_insert(pool, root->right, key);
    return root;
}

static size_t tree_count(const TreeNode *n) {
    return n ? 1 + tree_count(n->left) + tree_count(n->right) : 0;
}

static void demo_pool(void) {
    puts("== 2. fixed-size pool ==");
    Pool pool;
    if (!pool_init(&pool, sizeof(TreeNode), _Alignof(TreeNode), 1024)) { perror("pool_init"); return; }
    printf("  slot stride %zu bytes (sizeof TreeNode = %zu), %zu slots\n",
           pool.stride, sizeof(TreeNode), pool.count);

    TreeNode *root = NULL;
    unsigned s = 12345;
    for (int i = 0; i < 500; i++) {
        s = s * 1103515245u + 12345u;
        root = tree_insert(&pool, root, (double)(s >> 8) / 16777216.0);
    }
    assert(tree_count(root) == 500 && pool.in_use == 500);
    printf("  built a 500-node BST from the pool; in_use = %zu\n", pool.in_use);

    /* freeing the whole tree: no recursion needed — the pool owns every node */
    pool_destroy(&pool);
    puts("  pool_destroy: all 500 nodes released in one free()");

    /* LIFO reuse: the slot you just freed is the next one you get (hot in cache) */
    pool_init(&pool, 32, 16, 8);
    void *p1 = pool_alloc(&pool), *p2 = pool_alloc(&pool);
    pool_free(&pool, p1);
    void *p3 = pool_alloc(&pool);
    assert(p3 == p1);
    assert(((uintptr_t)p2 % 16) == 0);
    /* exhaustion: 8 slots total, 2 in use, so 6 more succeed and the 7th is NULL */
    for (int i = 0; i < 6; i++) assert(pool_alloc(&pool) != NULL);
    assert(pool_alloc(&pool) == NULL);
    pool_destroy(&pool);
    puts("  LIFO reuse and exhaustion behave as asserted");
}

/* ========================================================================================== */
/* 3. GENERAL-PURPOSE FREE-LIST ALLOCATOR: implicit list + boundary tags                       */
/*                                                                                            */
/*  heap: [prologue HDR][prologue FTR] [HDR|payload......|FTR] [HDR|payload|FTR] ... [epilogue HDR] */
/*  Every block: 16-byte header, payload, 16-byte footer (a copy of the header).               */
/*  Block sizes are multiples of 16, so payloads are 16-byte aligned (max_align_t on arm64).   */
/*  The footer lets free() find the PREVIOUS block's header in O(1): that is what makes        */
/*  coalescing with both neighbours possible without a doubly-linked list.                     */
/*  Minimum block = 48 bytes (HDR + 16-byte payload + FTR).                                    */
/* ========================================================================================== */
typedef struct { size_t size; size_t used; } FLTag;        /* 16 bytes, both hdr and ftr */

#define FL_TAG    sizeof(FLTag)          /* 16 */
#define FL_MIN    (3 * FL_TAG)           /* 48: header + 16 payload + footer */

typedef struct {
    unsigned char *heap;      /* the raw memory we manage */
    size_t cap;
    unsigned char *first;     /* header of the first real block */
    bool best_fit;            /* policy switch */
    /* statistics */
    size_t n_alloc, n_free, n_split, n_coalesce;
} FreeList;

static FLTag *fl_hdr(unsigned char *blk)            { return (FLTag *)blk; }
static FLTag *fl_ftr(unsigned char *blk)            { return (FLTag *)(blk + fl_hdr(blk)->size - FL_TAG); }
static unsigned char *fl_next(unsigned char *blk)   { return blk + fl_hdr(blk)->size; }
static unsigned char *fl_prev(unsigned char *blk)   { FLTag *pf = (FLTag *)(blk - FL_TAG); return blk - pf->size; }
static void *fl_payload(unsigned char *blk)         { return blk + FL_TAG; }
static unsigned char *fl_block_of(void *payload)    { return (unsigned char *)payload - FL_TAG; }

static void fl_set(unsigned char *blk, size_t size, size_t used) {
    fl_hdr(blk)->size = size; fl_hdr(blk)->used = used;
    fl_ftr(blk)->size = size; fl_ftr(blk)->used = used;
}

static bool fl_init(FreeList *fl, void *mem, size_t cap, bool best_fit) {
    assert(((uintptr_t)mem % 16) == 0 && cap % 16 == 0 && cap >= 2 * FL_TAG + FL_MIN + FL_TAG);
    memset(fl, 0, sizeof *fl);
    fl->heap = mem; fl->cap = cap; fl->best_fit = best_fit;
    /* prologue: a used block with no payload (HDR+FTR = 32 bytes) so fl_prev(first) is valid */
    fl_set(fl->heap, 2 * FL_TAG, 1);
    fl->first = fl->heap + 2 * FL_TAG;
    /* epilogue: a header-only used block of size 0 at the very end — stops the walk */
    unsigned char *epi = fl->heap + cap - FL_TAG;
    fl_hdr(epi)->size = 0; fl_hdr(epi)->used = 1;
    /* one giant free block in between */
    fl_set(fl->first, (size_t)(epi - fl->first), 0);
    return true;
}

static unsigned char *fl_find(FreeList *fl, size_t need) {
    unsigned char *best = NULL;
    for (unsigned char *b = fl->first; fl_hdr(b)->size != 0; b = fl_next(b)) {
        if (fl_hdr(b)->used || fl_hdr(b)->size < need) continue;
        if (!fl->best_fit) return b;                                    /* first fit */
        if (!best || fl_hdr(b)->size < fl_hdr(best)->size) best = b;    /* best fit */
        if (fl_hdr(b)->size == need) break;                             /* cannot do better */
    }
    return best;
}

static void *fl_malloc(FreeList *fl, size_t n) {
    if (n == 0) n = 1;
    size_t need = align_up(n + 2 * FL_TAG, 16);
    if (need < FL_MIN) need = FL_MIN;
    unsigned char *b = fl_find(fl, need);
    if (!b) return NULL;                       /* out of memory: caller handles it */
    size_t have = fl_hdr(b)->size;
    if (have - need >= FL_MIN) {               /* split: remainder becomes a free block */
        fl_set(b, need, 1);
        fl_set(fl_next(b), have - need, 0);
        fl->n_split++;
    } else {
        fl_set(b, have, 1);                    /* internal fragmentation: hand out the slack */
    }
    fl->n_alloc++;
    return fl_payload(b);
}

/* merge with free neighbours. Because of the prologue/epilogue no bounds checks are needed. */
static unsigned char *fl_coalesce(FreeList *fl, unsigned char *b) {
    unsigned char *nx = fl_next(b);
    if (!fl_hdr(nx)->used) {                   /* absorb the next block */
        fl_set(b, fl_hdr(b)->size + fl_hdr(nx)->size, 0);
        fl->n_coalesce++;
    }
    unsigned char *pv = fl_prev(b);
    if (!fl_hdr(pv)->used) {                   /* let the previous block absorb us */
        fl_set(pv, fl_hdr(pv)->size + fl_hdr(b)->size, 0);
        fl->n_coalesce++;
        b = pv;
    }
    return b;
}

static void fl_free(FreeList *fl, void *p) {
    if (!p) return;
    unsigned char *b = fl_block_of(p);
    assert(b >= fl->first && b < fl->heap + fl->cap);
    assert(fl_hdr(b)->used == 1 && "double free or bad pointer");
    assert(fl_hdr(b)->size == fl_ftr(b)->size && "header/footer mismatch: heap overflow?");
    fl_set(b, fl_hdr(b)->size, 0);
    fl_coalesce(fl, b);
    fl->n_free++;
}

/* heap checker: walk every block and assert every invariant. Call it in tests after every op. */
static void fl_check(const FreeList *fl) {
    size_t total = 2 * FL_TAG;
    bool prev_free = false;
    for (unsigned char *b = fl->first; fl_hdr(b)->size != 0; b = fl_next(b)) {
        FLTag *h = fl_hdr(b), *f = fl_ftr(b);
        assert(h->size % 16 == 0 && h->size >= FL_MIN);
        assert(h->size == f->size && h->used == f->used);          /* boundary tags agree */
        assert(((uintptr_t)fl_payload(b) % 16) == 0);              /* payload alignment */
        assert(!(prev_free && !h->used) && "two adjacent free blocks: coalescing failed");
        prev_free = !h->used;
        total += h->size;
    }
    assert(total + FL_TAG == fl->cap && "block sizes do not tile the heap");
}

typedef struct { size_t free_bytes, largest_free, n_free_blocks, used_bytes; } FLStats;

static FLStats fl_stats(const FreeList *fl) {
    FLStats s = {0};
    for (unsigned char *b = fl->first; fl_hdr(b)->size != 0; b = fl_next(b)) {
        size_t sz = fl_hdr(b)->size;
        if (fl_hdr(b)->used) s.used_bytes += sz;
        else {
            s.free_bytes += sz; s.n_free_blocks++;
            if (sz > s.largest_free) s.largest_free = sz;
        }
    }
    return s;
}

static void demo_freelist(bool best_fit) {
    printf("== 3. free-list allocator (%s) ==\n", best_fit ? "best-fit" : "first-fit");
    enum { CAP = 64 * 1024 };
    static _Alignas(16) unsigned char heap_mem[CAP];
    FreeList fl;
    fl_init(&fl, heap_mem, CAP, best_fit);
    fl_check(&fl);

    /* 1) nearly fill the heap with blocks of varying size, then free every other one: holes */
    void *p[64] = {0};
    size_t sizes[64];
    for (int i = 0; i < 64; i++) {
        sizes[i] = 380 + (size_t)(i % 5) * 280;                 /* 380, 660, 940, 1220, 1500 bytes */
        p[i] = fl_malloc(&fl, sizes[i]);
        assert(p[i] != NULL);
        memset(p[i], i, sizes[i]);                             /* the payload really is ours */
        fl_check(&fl);
    }
    for (int i = 0; i < 64; i += 2) { fl_free(&fl, p[i]); p[i] = NULL; fl_check(&fl); }
    FLStats s1 = fl_stats(&fl);
    printf("  after freeing every other block: %zu free blocks, %zu free bytes, largest hole %zu "
           "(fragmentation %.0f%%)\n", s1.n_free_blocks, s1.free_bytes, s1.largest_free,
           100.0 * (1.0 - (double)s1.largest_free / (double)s1.free_bytes));

    /* 2) a request smaller than the total free space but bigger than any hole must fail */
    void *big = fl_malloc(&fl, s1.free_bytes / 2);
    printf("  request for half the free bytes (%zu): %s  <- external fragmentation\n",
           s1.free_bytes / 2, big ? "SUCCEEDED" : "NULL");
    assert(big == NULL);

    /* 3) policy: 8 small requests. first-fit splits the first hole that fits, best-fit the     */
    /*    tightest one; count how many holes survive intact for larger requests later.           */
    void *small[8];
    for (int i = 0; i < 8; i++) { small[i] = fl_malloc(&fl, 150); assert(small[i]); fl_check(&fl); }
    FLStats s3 = fl_stats(&fl);
    printf("  after 8 x 150-byte requests: %zu free blocks, largest hole %zu, %zu splits so far\n",
           s3.n_free_blocks, s3.largest_free, fl.n_split);
    for (int i = 0; i < 8; i++) fl_free(&fl, small[i]);

    /* 4) free the rest: coalescing must give back exactly one free block spanning the heap */
    for (int i = 1; i < 64; i += 2) { fl_free(&fl, p[i]); fl_check(&fl); }
    FLStats s2 = fl_stats(&fl);
    assert(s2.n_free_blocks == 1 && s2.used_bytes == 0);
    assert(s2.largest_free == CAP - 3 * FL_TAG);
    printf("  after freeing everything: %zu free block of %zu bytes  [coalescing verified]\n",
           s2.n_free_blocks, s2.largest_free);

    /* 5) reuse: the whole heap is available again */
    void *q = fl_malloc(&fl, CAP - 3 * FL_TAG - 2 * FL_TAG);
    assert(q != NULL);
    fl_free(&fl, q);
    fl_check(&fl);
    printf("  stats: %zu allocs, %zu frees, %zu splits, %zu coalesces\n",
           fl.n_alloc, fl.n_free, fl.n_split, fl.n_coalesce);
}

/* ========================================================================================== */
/* 4. STRUCT LAYOUT, PADDING, ALIGNMENT                                                       */
/* ========================================================================================== */
typedef struct {            /* declared in a "natural" order — 40 bytes */
    char   tag;             /* offset 0, then 7 bytes of padding */
    double mass;            /* offset 8 */
    int    id;              /* offset 16, then 4 bytes of padding */
    double radius;          /* offset 24 */
    char   alive;           /* offset 32, then 7 bytes of tail padding (struct align = 8) */
} BodyBad;

typedef struct {            /* largest members first — 24 bytes, same information */
    double mass;            /* 0 */
    double radius;          /* 8 */
    int    id;              /* 16 */
    char   tag;             /* 20 */
    char   alive;           /* 21, then 2 bytes of tail padding */
} BodyGood;

typedef struct { _Alignas(64) double v[8]; } CacheLineVec;   /* over-aligned: one full 64-B line */

static void demo_layout(void) {
    puts("== 4. struct layout and alignment ==");
    printf("  BodyBad : size %zu, align %zu  (tag@%zu mass@%zu id@%zu radius@%zu alive@%zu)\n",
           sizeof(BodyBad), _Alignof(BodyBad), offsetof(BodyBad, tag), offsetof(BodyBad, mass),
           offsetof(BodyBad, id), offsetof(BodyBad, radius), offsetof(BodyBad, alive));
    printf("  BodyGood: size %zu, align %zu  (mass@%zu radius@%zu id@%zu tag@%zu alive@%zu)\n",
           sizeof(BodyGood), _Alignof(BodyGood), offsetof(BodyGood, mass), offsetof(BodyGood, radius),
           offsetof(BodyGood, id), offsetof(BodyGood, tag), offsetof(BodyGood, alive));
    printf("  1e6 bodies: %.1f MB vs %.1f MB\n",
           1e6 * sizeof(BodyBad) / 1048576.0, 1e6 * sizeof(BodyGood) / 1048576.0);
    printf("  _Alignof: char %zu short %zu int %zu double %zu long double %zu max_align_t %zu\n",
           _Alignof(char), _Alignof(short), _Alignof(int), _Alignof(double),
           _Alignof(long double), _Alignof(max_align_t));
    printf("  CacheLineVec: size %zu align %zu\n", sizeof(CacheLineVec), _Alignof(CacheLineVec));

    /* aligned heap memory: size must be a multiple of the alignment for aligned_alloc (C11) */
    double *a64 = aligned_alloc(64, align_up(1000 * sizeof(double), 64));
    void *a4k = NULL;
    int rc = posix_memalign(&a4k, 4096, 1 << 16);            /* POSIX alternative, page-aligned */
    assert(a64 && rc == 0 && a4k);
    printf("  aligned_alloc(64): %p -> %% 64 = %zu;  posix_memalign(4096): %p -> %% 4096 = %zu\n",
           (void *)a64, (size_t)((uintptr_t)a64 % 64), a4k, (size_t)((uintptr_t)a4k % 4096));
    CacheLineVec *clv = aligned_alloc(_Alignof(CacheLineVec), sizeof(CacheLineVec) * 4);
    assert(clv && ((uintptr_t)clv % 64) == 0);
    free(a64); free(a4k); free(clv);     /* all three are freed with plain free() */
}

/* ========================================================================================== */
/* 5. ARRAY-OF-STRUCTS vs STRUCT-OF-ARRAYS: N-body                                            */
/* ========================================================================================== */
typedef struct {
    double x, y, z;         /* position */
    double vx, vy, vz;      /* velocity */
    double ax, ay, az;      /* acceleration */
    double mass;
    int    id;              /* + padding: sizeof == 88 (80 doubles + 4 int + 4 pad) */
} BodyAoS;

typedef struct {
    size_t n;
    double *x, *y, *z, *vx, *vy, *vz, *ax, *ay, *az, *m;
} BodiesSoA;

/* direct O(N^2) softened gravity, AoS */
static void forces_aos(BodyAoS *b, size_t n, double eps2) {
    for (size_t i = 0; i < n; i++) {
        double ax = 0, ay = 0, az = 0;
        const double xi = b[i].x, yi = b[i].y, zi = b[i].z;
        for (size_t j = 0; j < n; j++) {
            double dx = b[j].x - xi, dy = b[j].y - yi, dz = b[j].z - zi;
            double d2 = dx * dx + dy * dy + dz * dz + eps2;
            double inv = 1.0 / sqrt(d2);
            double s = b[j].mass * inv * inv * inv;
            ax += dx * s; ay += dy * s; az += dz * s;
        }
        b[i].ax = ax; b[i].ay = ay; b[i].az = az;
    }
}

/* same math, SoA: the inner loop streams four contiguous arrays; the compiler can vectorize */
static void forces_soa(const BodiesSoA *s, double eps2) {
    const size_t n = s->n;
    const double *restrict x = s->x, *restrict y = s->y, *restrict z = s->z, *restrict m = s->m;
    for (size_t i = 0; i < n; i++) {
        double ax = 0, ay = 0, az = 0;
        const double xi = x[i], yi = y[i], zi = z[i];
        for (size_t j = 0; j < n; j++) {
            double dx = x[j] - xi, dy = y[j] - yi, dz = z[j] - zi;
            double d2 = dx * dx + dy * dy + dz * dz + eps2;
            double inv = 1.0 / sqrt(d2);
            double sc = m[j] * inv * inv * inv;
            ax += dx * sc; ay += dy * sc; az += dz * sc;
        }
        s->ax[i] = ax; s->ay[i] = ay; s->az[i] = az;
    }
}

/* streaming pass: touches only positions and velocities. AoS drags the whole 88-byte struct  */
/* through the cache for the 48 bytes it needs; SoA streams exactly the 6 arrays it uses.      */
static void drift_aos(BodyAoS *b, size_t n, double dt) {
    for (size_t i = 0; i < n; i++) { b[i].x += dt * b[i].vx; b[i].y += dt * b[i].vy; b[i].z += dt * b[i].vz; }
}
static void drift_soa(BodiesSoA *s, double dt) {
    for (size_t i = 0; i < s->n; i++) { s->x[i] += dt * s->vx[i]; s->y[i] += dt * s->vy[i]; s->z[i] += dt * s->vz[i]; }
}

static double *soa_col(Arena *a, size_t n) { return arena_alloc_aligned(a, n * sizeof(double), 64); }

static void demo_aos_soa(void) {
    puts("== 5. AoS vs SoA (N-body) ==");
    const size_t n_force = 2048, n_stream = 1u << 20;
    const double eps2 = 1e-3;

    /* one arena holds everything for the demo: freed with a single call at the end */
    Arena a;
    size_t need = n_stream * sizeof(BodyAoS) + 10 * n_stream * sizeof(double) + 4096;
    if (!arena_init(&a, need)) { perror("arena"); return; }

    BodyAoS *aos = ARENA_NEW(&a, BodyAoS, n_stream);
    BodiesSoA soa = { .n = n_stream };
    double **cols[] = { &soa.x, &soa.y, &soa.z, &soa.vx, &soa.vy, &soa.vz, &soa.ax, &soa.ay, &soa.az, &soa.m };
    for (size_t c = 0; c < 10; c++) *cols[c] = soa_col(&a, n_stream);

    unsigned s = 7;
    for (size_t i = 0; i < n_stream; i++) {
        double v[7];
        for (int k = 0; k < 7; k++) { s = s * 1103515245u + 12345u; v[k] = (double)(s >> 8) / 16777216.0 - 0.5; }
        aos[i] = (BodyAoS){ v[0], v[1], v[2], v[3], v[4], v[5], 0, 0, 0, 1.0 + v[6], (int)i };
        soa.x[i] = v[0]; soa.y[i] = v[1]; soa.z[i] = v[2];
        soa.vx[i] = v[3]; soa.vy[i] = v[4]; soa.vz[i] = v[5];
        soa.ax[i] = soa.ay[i] = soa.az[i] = 0; soa.m[i] = 1.0 + v[6];
    }

    /* force pass on the first n_force bodies */
    BodiesSoA soa_small = soa; soa_small.n = n_force;
    forces_aos(aos, n_force, eps2);                     /* warm up */
    double t0 = now_sec(); forces_aos(aos, n_force, eps2); double t_aos = now_sec() - t0;
    forces_soa(&soa_small, eps2);
    t0 = now_sec(); forces_soa(&soa_small, eps2);       double t_soa = now_sec() - t0;
    double chk = 0;
    for (size_t i = 0; i < n_force; i++) chk += fabs(aos[i].ax - soa.ax[i]) + fabs(aos[i].ay - soa.ay[i]);
    double pairs = (double)n_force * (double)n_force;
    printf("  forces N=%zu: AoS %.1f ms (%.2f G pair/s)  SoA %.1f ms (%.2f G pair/s)  |diff| = %.1e\n",
           n_force, t_aos * 1e3, pairs / t_aos * 1e-9, t_soa * 1e3, pairs / t_soa * 1e-9, chk);

    /* streaming pass over all bodies, repeated */
    const int reps = 10;
    drift_aos(aos, n_stream, 1e-3);
    t0 = now_sec(); for (int r = 0; r < reps; r++) drift_aos(aos, n_stream, 1e-3); t_aos = now_sec() - t0;
    drift_soa(&soa, 1e-3);
    t0 = now_sec(); for (int r = 0; r < reps; r++) drift_soa(&soa, 1e-3);          t_soa = now_sec() - t0;
    double bytes_aos = (double)reps * n_stream * sizeof(BodyAoS);   /* whole struct streamed */
    double bytes_soa = (double)reps * n_stream * 6 * sizeof(double);
    chk = 0; for (size_t i = 0; i < n_stream; i += 4096) chk += aos[i].x - soa.x[i];
    printf("  drift  N=%zu x%d: AoS %.1f ms (%.1f GB/s touched)  SoA %.1f ms (%.1f GB/s)  |diff| = %.1e\n",
           n_stream, reps, t_aos * 1e3, bytes_aos / t_aos * 1e-9, t_soa * 1e3, bytes_soa / t_soa * 1e-9, chk);
    printf("  AoS moves %.0f MB per pass for %.0f MB of useful data; SoA moves only what it uses\n",
           n_stream * sizeof(BodyAoS) / 1048576.0, n_stream * 6 * sizeof(double) / 1048576.0);
    arena_destroy(&a);
}

/* ========================================================================================== */
/* 6. SOFTWARE PREFETCH on a random gather                                                    */
/* ========================================================================================== */
static double gather(const double *a, const uint32_t *idx, size_t n, bool prefetch) {
    double s = 0;
    const size_t ahead = 32;
    for (size_t i = 0; i < n; i++) {
        if (prefetch && i + ahead < n) __builtin_prefetch(&a[idx[i + ahead]], 0 /*read*/, 0 /*no reuse*/);
        s += a[idx[i]];
    }
    return s;
}

static void demo_prefetch(void) {
    puts("== 6. software prefetch (random gather over 512 MB) ==");
    const size_t n = 1u << 26;                 /* 64M doubles = 512 MB: far beyond any cache */
    const size_t m = 1u << 23;                 /* 8M random reads */
    double *a = malloc(n * sizeof *a);
    uint32_t *idx = malloc(m * sizeof *idx);
    if (!a || !idx) { perror("malloc"); free(a); free(idx); return; }
    for (size_t i = 0; i < n; i++) a[i] = (double)(i & 1023);
    uint64_t s = 88172645463325252ull;
    for (size_t i = 0; i < m; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; idx[i] = (uint32_t)(s % n); }

    double t0 = now_sec(); double r1 = gather(a, idx, m, false); double t_plain = now_sec() - t0;
    t0 = now_sec();        double r2 = gather(a, idx, m, true);  double t_pf = now_sec() - t0;
    assert(r1 == r2);
    printf("  plain: %.1f ms (%.0f ns/access)   prefetch 32 ahead: %.1f ms (%.0f ns/access)   x%.2f\n",
           t_plain * 1e3, t_plain / m * 1e9, t_pf * 1e3, t_pf / m * 1e9, t_plain / t_pf);
    printf("  (DRAM latency is ~100 ns; prefetching keeps many misses in flight at once)  chk=%.0f\n", r1);
    free(a); free(idx);
}

/* ========================================================================================== */
/* 7. PAGES: an anonymous mmap arena, page faults on first touch                              */
/* ========================================================================================== */
static void demo_pages(void) {
    puts("== 7. mmap arena and page faults ==");
    const size_t len = 256u << 20;             /* 256 MB reserved, nothing committed yet */
    void *mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { perror("  mmap (skipping section)"); return; }
    long page = sysconf(_SC_PAGESIZE);
    unsigned char *p = mem;
    size_t npages = len / (size_t)page;

    double t0 = now_sec();
    memset(p, 1, len);                          /* first write: one page fault per page + the writes */
    double t_first = now_sec() - t0;
    t0 = now_sec();
    memset(p, 2, len);                          /* second write: pages are mapped, just the writes */
    double t_second = now_sec() - t0;
    double fault_cost = (t_first - t_second) / (double)npages;
    printf("  page size %ld B, %zu pages. memset 256 MB: first %.1f ms, second %.1f ms "
           "-> ~%.2f us per page fault (x%.1f slower)\n",
           page, npages, t_first * 1e3, t_second * 1e3, fault_cost * 1e6, t_first / t_second);
    printf("  second pass bandwidth: %.1f GB/s   chk=%d\n", (double)len / t_second * 1e-9, p[len / 2]);
    puts("  a file-backed mmap of a checkpoint works the same way: the OS pages it in on demand,");
    puts("  and msync()/munmap() write it back — zero explicit fwrite calls.");
    if (munmap(mem, len) != 0) perror("munmap");
}

/* ========================================================================================== */
/* 8. MATRIX with allocator callbacks and strided views                                       */
/* ========================================================================================== */
typedef struct {
    void *(*alloc)(void *ctx, size_t size, size_t align);
    void  (*free)(void *ctx, void *p, size_t size);
    void  *ctx;
} Allocator;

/* the default allocator: plain aligned malloc */
static void *heap_alloc(void *ctx, size_t size, size_t align) {
    (void)ctx;
    size_t a = align < sizeof(void *) ? sizeof(void *) : align;
    return aligned_alloc(a, align_up(size, a));
}
static void heap_free(void *ctx, void *p, size_t size) { (void)ctx; (void)size; free(p); }
static const Allocator heap_allocator = { heap_alloc, heap_free, NULL };

/* an arena allocator behind the same interface: free is a no-op */
static void *arena_cb_alloc(void *ctx, size_t size, size_t align) { return arena_alloc_aligned(ctx, size, align); }
static void  arena_cb_free(void *ctx, void *p, size_t size) { (void)ctx; (void)p; (void)size; }

typedef struct {
    size_t rows, cols;
    ptrdiff_t rs, cs;       /* row stride, column stride, in elements (NumPy strides / 8) */
    double *data;
    bool owns;              /* views do not own their data */
    const Allocator *al;
} Mat;

static double *mat_at(const Mat *m, size_t i, size_t j) {
    assert(i < m->rows && j < m->cols);
    return &m->data[(ptrdiff_t)i * m->rs + (ptrdiff_t)j * m->cs];
}

static Mat mat_new(const Allocator *al, size_t rows, size_t cols) {
    Mat m = { rows, cols, (ptrdiff_t)cols, 1, NULL, true, al };
    m.data = al->alloc(al->ctx, rows * cols * sizeof(double), 64);
    if (m.data) memset(m.data, 0, rows * cols * sizeof(double));
    return m;
}
static void mat_release(Mat *m) {
    if (m->owns && m->data) m->al->free(m->al->ctx, m->data, m->rows * m->cols * sizeof(double));
    *m = (Mat){0};
}
/* views: same memory, different shape/strides. O(1), no copy, no ownership. */
static Mat mat_transpose_view(const Mat *m) { Mat t = *m; t.rows = m->cols; t.cols = m->rows; t.rs = m->cs; t.cs = m->rs; t.owns = false; return t; }
static Mat mat_rows_view(const Mat *m, size_t r0, size_t r1) {
    assert(r0 <= r1 && r1 <= m->rows);
    Mat v = *m; v.rows = r1 - r0; v.data = mat_at(m, r0, 0); v.owns = false; return v;
}
static Mat mat_col_view(const Mat *m, size_t j) { Mat v = *m; v.cols = 1; v.data = mat_at(m, 0, j); v.owns = false; return v; }
static bool mat_is_contiguous(const Mat *m) { return m->cs == 1 && m->rs == (ptrdiff_t)m->cols; }

static double mat_sum(const Mat *m) {
    double s = 0;
    for (size_t i = 0; i < m->rows; i++) for (size_t j = 0; j < m->cols; j++) s += *mat_at(m, i, j);
    return s;
}

static void demo_matrix(void) {
    puts("== 8. Matrix: allocator callbacks and strided views ==");
    Arena arena; arena_init(&arena, 1 << 16);
    const Allocator arena_allocator = { arena_cb_alloc, arena_cb_free, &arena };

    Mat a = mat_new(&heap_allocator, 4, 3);
    Mat b = mat_new(&arena_allocator, 4, 3);
    for (size_t i = 0; i < 4; i++) for (size_t j = 0; j < 3; j++) { *mat_at(&a, i, j) = (double)(i * 3 + j); *mat_at(&b, i, j) = 1.0; }

    Mat at = mat_transpose_view(&a);                /* 3x4, strides swapped, shares a.data */
    Mat mid = mat_rows_view(&a, 1, 3);              /* rows 1..2 — a[1:3] in NumPy */
    Mat c2 = mat_col_view(&a, 2);                   /* a[:, 2] — column stride 3 */
    printf("  a: %zux%zu strides (%td,%td) contiguous=%d  aT: %zux%zu strides (%td,%td) contiguous=%d\n",
           a.rows, a.cols, a.rs, a.cs, mat_is_contiguous(&a), at.rows, at.cols, at.rs, at.cs, mat_is_contiguous(&at));
    printf("  aT[2][3] = %g (== a[3][2] = %g)   a[1:3] sum = %g   a[:,2] sum = %g\n",
           *mat_at(&at, 2, 3), *mat_at(&a, 3, 2), mat_sum(&mid), mat_sum(&c2));
    *mat_at(&at, 0, 0) = 100.0;                    /* writing through a view changes the owner */
    assert(*mat_at(&a, 0, 0) == 100.0);
    printf("  wrote 100 through the transpose view; a[0][0] = %g   b (arena) sum = %g\n", *mat_at(&a, 0, 0), mat_sum(&b));
    printf("  arena used %zu bytes for b (64-byte aligned: %d)\n", arena.off, ((uintptr_t)b.data % 64) == 0);

    mat_release(&at);          /* view: no-op on the data */
    mat_release(&a);           /* heap allocator: free() */
    mat_release(&b);           /* arena allocator: no-op; the arena is released as a whole */
    arena_destroy(&arena);
}

/* ========================================================================================== */
/* 9. LIMITS AND STATISTICS FROM THE SYSTEM                                                   */
/* ========================================================================================== */
static void demo_limits(void) {
    puts("== 9. stack limit and system allocator statistics ==");
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) == 0)
        printf("  main-thread stack limit: %.1f MB soft, %s hard  (ulimit -s / -Wl,-stack_size,0x...)\n",
               (double)rl.rlim_cur / 1048576.0, rl.rlim_max == RLIM_INFINITY ? "unlimited" : "finite");
    double frame_guess = 64.0;   /* a small recursive function with a few locals */
    printf("  a %g-byte frame overflows an 8 MB stack after ~%.0fk calls: recursion depth is a resource\n",
           frame_guess, 8.0 * 1048576.0 / frame_guess / 1000.0);

    void *blocks[8];
    for (int i = 0; i < 8; i++) blocks[i] = malloc(24 + 100 * (size_t)i);
#if defined(__APPLE__)
    printf("  malloc_size(): asked 24 got %zu; asked 124 got %zu; asked 724 got %zu  (size classes)\n",
           malloc_size(blocks[0]), malloc_size(blocks[1]), malloc_size(blocks[7]));
    malloc_statistics_t st;
    malloc_zone_statistics(NULL, &st);
    printf("  malloc_zone_statistics: %u blocks in use, %.1f KB in use, %.1f MB held from the OS\n",
           st.blocks_in_use, st.size_in_use / 1024.0, st.size_allocated / 1048576.0);
#elif defined(__GLIBC__)
    printf("  malloc_usable_size(): asked 24 got %zu; asked 124 got %zu\n",
           malloc_usable_size(blocks[0]), malloc_usable_size(blocks[1]));
    struct mallinfo2 mi = mallinfo2();
    printf("  mallinfo2: %zu bytes in use, %zu bytes from the OS\n", mi.uordblks, mi.arena + mi.hblkhd);
#endif
    for (int i = 0; i < 8; i++) free(blocks[i]);
}

int main(void) {
    demo_arena();
    demo_pool();
    demo_freelist(false);
    demo_freelist(true);
    demo_layout();
    demo_aos_soa();
    demo_prefetch();
    demo_pages();
    demo_matrix();
    demo_limits();
    return 0;
}
