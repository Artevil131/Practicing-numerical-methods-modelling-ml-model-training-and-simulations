# Chapter 10 — Data Structures

## What you'll be able to do after this chapter

- Implement a generic dynamic array (via `void *` + element size) and a type-safe macro version, and know when each is appropriate.
- Build linked lists, an array-backed stack, and a ring-buffer queue, and state precisely when a list beats an array (rarely).
- Implement a hash table two ways (chaining, open addressing), with string keys copied correctly, load-factor resizing, and tombstones — the structure behind a tokenizer's vocabulary and a BPE pair counter.
- Implement a binary search tree and a binary heap / priority queue, and explain their Big-O.
- Use `qsort` and `bsearch` with comparator functions.
- Choose a structure for a problem from a decision guide, and state who owns the elements inside a container.

## Why this matters for ML / numerics / sims

Python hands you `list`, `dict`, `set`, `heapq`, `collections.deque` and you never think about them. In C you build them, once, as modules in your `src/` (Chapter 09) — then reuse them everywhere. The BPE tokenizer counts adjacent-pair frequencies: a hash table from `(int,int)` to `int`. The vocabulary maps strings to IDs: a hash table from `char*` to `int`. Barnes-Hut and k-nearest-neighbors pull the closest item repeatedly: a priority queue. Dijkstra on a grid: the same heap. An event-driven collision sim: a heap of events keyed by time. A parser for expressions in an autograd DSL: a stack. Knowing the Big-O of each operation is the difference between a tokenizer that processes a corpus in 2 seconds and one that takes 2 hours.

---

## 1. Big-O summary (reference for the whole chapter)

| Structure | Access by index | Search | Insert | Delete | Min/Max | Notes |
|---|---|---|---|---|---|---|
| Dynamic array | O(1) | O(n) (O(log n) if sorted) | O(1) amortized at end; O(n) middle | O(n) middle; O(1) at end | O(n) | cache-friendly; default choice |
| Singly linked list | O(n) | O(n) | O(1) at head (given node) | O(1) given previous node | O(n) | pointer chasing; poor cache |
| Doubly linked list | O(n) | O(n) | O(1) given node | O(1) given node | O(n) | 2 pointers per node |
| Stack (array) | — | — | push O(1) | pop O(1) | — | LIFO |
| Queue (ring buffer) | — | — | enqueue O(1) | dequeue O(1) | — | FIFO, fixed or growable |
| Hash table | — | O(1) avg, O(n) worst | O(1) avg | O(1) avg | O(n) | unordered; needs good hash |
| BST (unbalanced) | — | O(h): O(log n) avg, O(n) worst | O(h) | O(h) | O(h) | ordered iteration; degenerates on sorted input |
| Binary heap | — | O(n) | O(log n) | O(log n) pop-min | O(1) peek | array-backed; not for search |
| Sorted array + `bsearch` | O(1) | O(log n) | O(n) | O(n) | O(1) | best when built once, queried often |

The single most useful rule: **default to the dynamic array**. Its constant factors (contiguous memory, no allocation per element, prefetch-friendly) beat asymptotically better structures until `n` is large. Reach for a hash table when you need lookup by key, a heap when you need repeated min/max, and a linked list almost never.

---

## 2. Dynamic array, generic

Chapter 06 built `Vec` for `double`. A generic version stores the element size and moves bytes with `memcpy`:

```c
typedef struct {
    void  *data;
    size_t len, cap, elem_size;
} DynArray;

void da_init(DynArray *a, size_t elem_size) { a->data = NULL; a->len = a->cap = 0; a->elem_size = elem_size; }

/* Appends a copy of *elem (elem_size bytes). Returns 0, or -1 on allocation failure. */
int da_push(DynArray *a, const void *elem) {
    if (a->len == a->cap) {
        size_t ncap = a->cap ? a->cap * 2 : 8;
        void *t = realloc(a->data, ncap * a->elem_size);
        if (!t) return -1;
        a->data = t; a->cap = ncap;
    }
    memcpy((char *)a->data + a->len * a->elem_size, elem, a->elem_size);
    a->len++;
    return 0;
}
/* Returns a pointer to element i (valid until the next push). */
void *da_at(const DynArray *a, size_t i) { return (char *)a->data + i * a->elem_size; }
void  da_free(DynArray *a) { free(a->data); da_init(a, a->elem_size); }

// Usage:
//   DynArray pts; da_init(&pts, sizeof(Point));
//   Point p = {1, 2}; da_push(&pts, &p);
//   Point *q = da_at(&pts, 0);      // cast needed: void* -> Point* (implicit in C, explicit in C++)
```

Pros: one implementation for every type. Cons: `void *` loses type checking — pushing a `double` into an array of `int` compiles fine. `(char *)a->data + i * a->elem_size` is the idiom for byte-offset arithmetic; you cannot do arithmetic on `void *` in standard C.

### Type-specific via macro

For type safety, generate a concrete array type per element type with a macro:

```c
#define DEFINE_VEC(T, Name)                                                        \
    typedef struct { T *data; size_t len, cap; } Name;                             \
    static int Name##_push(Name *v, T x) {                                         \
        if (v->len == v->cap) {                                                    \
            size_t ncap = v->cap ? v->cap * 2 : 8;                                 \
            T *t = realloc(v->data, ncap * sizeof *t);                             \
            if (!t) return -1;                                                     \
            v->data = t; v->cap = ncap;                                            \
        }                                                                          \
        v->data[v->len++] = x;                                                     \
        return 0;                                                                  \
    }                                                                              \
    static void Name##_free(Name *v) { free(v->data); v->data = NULL; v->len = v->cap = 0; }

DEFINE_VEC(int, IntVec)       // now IntVec, IntVec_push, IntVec_free exist
DEFINE_VEC(double, DblVec)
```

`##` pastes tokens, so `Name##_push` with `Name = IntVec` becomes `IntVec_push`. The compiler now rejects `IntVec_push(&v, 3.5)` with a warning and indexes `v.data[i]` as `int` directly. The cost is one copy of the code per type (fine) and harder-to-read compiler errors inside macros (annoying). C++ templates are this idea done properly.

**Python equivalent:** `list`. Both versions above are `list.append`.

---

## 3. Linked lists

A node holds a value and a pointer to the next node. The list is a pointer to the first node.

```
head -> [3 | ●] -> [7 | ●] -> [1 | NULL]
```

```c
typedef struct Node { int value; struct Node *next; } Node;

/* Pushes at the head. Returns the new head (== old head on allocation failure). */
Node *list_push_front(Node *head, int v) {
    Node *n = malloc(sizeof *n);
    if (!n) return head;
    n->value = v; n->next = head;
    return n;
}
/* Removes the first node with value v. Returns the (possibly new) head. */
Node *list_remove(Node *head, int v) {
    Node **pp = &head;                       /* pointer to the pointer we may rewrite */
    while (*pp && (*pp)->value != v) pp = &(*pp)->next;
    if (*pp) { Node *dead = *pp; *pp = dead->next; free(dead); }
    return head;
}
void list_free(Node *head) {
    while (head) { Node *next = head->next; free(head); head = next; }   /* save next BEFORE freeing */
}
// traverse:  for (Node *n = head; n; n = n->next) printf("%d ", n->value);
```

The `Node **pp` trick in `list_remove` avoids a special case for removing the head: `pp` points either at `head` itself or at some node's `next` field, and `*pp = dead->next` splices around the dead node in both cases.

A **doubly linked list** adds `prev`. Deletion given a node becomes O(1) without walking from the head; the cost is one more pointer per node and twice as many pointers to keep consistent on every insert/delete.

### When lists beat arrays — rarely

- You hold a pointer to a node and need O(1) insert/delete *right there*, and you never need index access. (LRU cache eviction, free lists inside an allocator, an intrusive queue of "ready" tasks.)
- Elements are huge and moving them is costly. (But then store *pointers* in an array.)
- You need stable addresses: pointers into a linked list survive insertions; pointers into a dynamic array do not.

For everything else, an array wins: O(n) traversal of a list is 10–100x slower than of an array on modern hardware because every `->next` is a likely cache miss. Bjarne Stroustrup's benchmark of "insert in sorted position, then remove random elements" — the textbook linked-list use case — has the array winning for every `n` up to 100,000+.

**Python equivalent:** `collections.deque` is a doubly linked list of blocks. Plain `list` is a dynamic array.

---

## 4. Stack and queue

### Stack (array-backed)

```c
typedef struct { double *data; size_t len, cap; } Stack;
/* push = vec_push from ch06; pop: */
int stack_pop(Stack *s, double *out) {
    if (s->len == 0) return -1;
    *out = s->data[--s->len];
    return 0;
}
```

LIFO. Uses: expression evaluation (shunting-yard), undo, DFS without recursion, matching brackets, the call stack itself.

### Queue (ring buffer)

Enqueue at the tail, dequeue at the head. A naive array queue shifts everything on dequeue (O(n)). A **ring buffer** wraps indices modulo capacity so both ends are O(1):

```
cap = 8, head = 6, len = 4:   indices 6,7,0,1 hold the queue (in order)
 [c][d][ ][ ][ ][ ][a][b]
        ^tail=(6+4)%8=2        ^head
```

```c
typedef struct { int *data; size_t head, len, cap; } Queue;

int queue_push(Queue *q, int v) {                 /* returns -1 when full (fixed capacity) */
    if (q->len == q->cap) return -1;
    q->data[(q->head + q->len) % q->cap] = v;
    q->len++;
    return 0;
}
int queue_pop(Queue *q, int *out) {
    if (q->len == 0) return -1;
    *out = q->data[q->head];
    q->head = (q->head + 1) % q->cap;
    q->len--;
    return 0;
}
```

To make it growable: when full, allocate `2*cap`, copy the `len` elements starting from `head` into positions `0..len-1` of the new buffer, set `head = 0`. Uses: BFS on a grid (flood fill, shortest path in a maze), producer/consumer buffers, moving-average windows in signal processing, a fixed-size history of the last `k` frames.

---

## 5. Hash table, in depth

A hash table stores key→value pairs and finds a key in O(1) average by computing an index from the key.

```
key "cat" --hash--> 0x8f2c...  --% capacity-->  index 5
buckets: [0][1][2][3][4][5: ("cat",17)][6][7]
```

### 5.1 Hash functions

A hash function maps a key to a well-distributed integer. Two classics for byte strings:

```c
/* djb2 */
uint64_t hash_djb2(const char *s) {
    uint64_t h = 5381;
    for (; *s; s++) h = h * 33 + (unsigned char)*s;
    return h;
}
/* FNV-1a: better distribution, still tiny */
uint64_t hash_fnv1a(const void *key, size_t len) {
    const unsigned char *p = key;
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
/* integer keys: mix the bits so sequential keys do not land in sequential buckets */
uint64_t hash_u64(uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}
```

For a pair key `(a, b)` (BPE merges), hash `((uint64_t)a << 32) | (uint32_t)b` with `hash_u64`. Never use `key % capacity` directly on raw integer keys — patterns in the keys (all even, multiples of 8) create collisions.

Reduce to an index with `h % cap` or, if `cap` is a power of two, `h & (cap - 1)` (faster).

### 5.2 Chaining

Each bucket is the head of a linked list of entries that hashed there.

```
buckets[0] -> NULL
buckets[1] -> ("dog",3) -> ("god",9) -> NULL      (collision: two keys, one bucket)
buckets[2] -> ("cat",17) -> NULL
```

```c
typedef struct Entry { char *key; int value; struct Entry *next; } Entry;
typedef struct { Entry **buckets; size_t cap, count; } StrMap;

/* Returns a pointer to the value slot for key, inserting (value 0) if absent. NULL on alloc failure. */
int *strmap_get_or_insert(StrMap *m, const char *key) {
    if (m->count + 1 > m->cap * 3 / 4) strmap_grow(m);            /* keep load factor <= 0.75 */
    size_t idx = hash_djb2(key) % m->cap;
    for (Entry *e = m->buckets[idx]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return &e->value;           /* found */
    Entry *e = malloc(sizeof *e);
    if (!e) return NULL;
    e->key = strdup_own(key);                                     /* COPY the key: the map owns it */
    if (!e->key) { free(e); return NULL; }
    e->value = 0;
    e->next = m->buckets[idx];                                    /* push at head of chain */
    m->buckets[idx] = e;
    m->count++;
    return &e->value;
}
```

Simple, deletion is easy (unlink the node), performance degrades gracefully when overloaded. Cost: one `malloc` per entry and pointer-chasing along chains.

### 5.3 Open addressing with linear probing

All entries live in one flat array. On collision, step to the next slot (`idx+1`, wrapping) until an empty one is found. Lookup follows the same path until it finds the key or an empty slot.

```
insert "cat" -> idx 5 (empty): store at 5
insert "dog" -> idx 5 (taken): probe 6 (empty): store at 6
lookup "dog" -> idx 5 ("cat" != "dog"): probe 6 ("dog"): found
lookup "emu" -> idx 5, 6 (no match), 7 (EMPTY): not present
```

```c
typedef struct { char *key; int value; } Slot;       /* key == NULL: empty; key == TOMBSTONE: deleted */
typedef struct { Slot *slots; size_t cap, count, tombstones; } OpenMap;
static char TOMBSTONE_STORAGE; #define TOMBSTONE (&TOMBSTONE_STORAGE)

Slot *openmap_find(OpenMap *m, const char *key, int for_insert) {
    size_t idx = hash_fnv1a(key, strlen(key)) & (m->cap - 1);    /* cap is a power of two */
    Slot *first_tomb = NULL;
    for (;;) {
        Slot *s = &m->slots[idx];
        if (s->key == NULL) return for_insert && first_tomb ? first_tomb : s;   /* empty ends the probe */
        if (s->key == TOMBSTONE) { if (!first_tomb) first_tomb = s; }
        else if (strcmp(s->key, key) == 0) return s;
        idx = (idx + 1) & (m->cap - 1);
    }
}
```

**Tombstones.** You cannot simply set a deleted slot's key back to NULL: a later key that probed *past* this slot would now hit "empty" and be reported missing. Instead mark it with a tombstone that lookups skip over and inserts may reuse. Count tombstones toward the load factor and rebuild (rehash without tombstones) when `count + tombstones` gets high.

Open addressing has no per-entry allocation and excellent cache behavior (probing walks contiguous memory), so it is usually faster than chaining. It is what CPython's `dict`, Rust's `HashMap`, and Go's `map` use.

### 5.4 Load factor and resizing

Load factor = `count / cap`. As it approaches 1, chains lengthen (chaining) or probe sequences explode (open addressing — at 0.9 the expected probe length for a miss is ~50). Resize at 0.7–0.75 for open addressing, up to ~1.0 is tolerable for chaining. Resizing means allocating a table of `2*cap`, re-inserting every live entry (their indices change because `cap` changed), and freeing the old table. This costs O(n) but happens O(log n) times, so amortized insert remains O(1) — the same argument as the dynamic array.

### 5.5 String keys — copy them!

```c
char buf[64];
while (fgets(buf, sizeof buf, f))
    (*strmap_get_or_insert(&vocab, buf))++;     // if the map stored `buf` itself, every key would be the same pointer
```

The caller's string may be a reused buffer, a stack array, or freed later. The map must own its keys: `malloc(strlen(key)+1)` + `memcpy` on insert, `free` on delete and on `map_free`. This is the number-one hash-table bug. If keys are guaranteed to outlive the map (string literals, an interned pool), you may store the pointer — document that.

### 5.6 `int -> int` and `string -> int` versions

The `int → int` version (BPE pair counts, `id → frequency`) needs no key copying and no `strcmp`; use `hash_u64` and a sentinel key (e.g. `INT64_MIN`) for "empty". The `string → int` version (vocab: `token → id`) copies keys and compares with `strcmp`. Write both as separate modules rather than one generic version: the generic one needs function pointers for hash/compare/copy/free and is slower and harder to read. In the tokenizer you will use `StrMap` for the vocabulary and `IntMap` keyed by `(left << 32) | right` for pair frequencies.

**Python equivalent:** `dict` and `collections.Counter`. `d[k] = d.get(k, 0) + 1` is `(*map_get_or_insert(&m, k))++`.

---

## 6. Binary search tree

Each node has a key, a left subtree with smaller keys, a right subtree with larger keys.

```
        8
      /   \
     3     10
    / \      \
   1   6      14
```

```c
typedef struct TNode { int key; struct TNode *left, *right; } TNode;

/* Inserts key (no duplicates). Returns the (possibly new) root. */
TNode *bst_insert(TNode *root, int key) {
    if (!root) {
        TNode *n = malloc(sizeof *n);
        if (n) { n->key = key; n->left = n->right = NULL; }
        return n;
    }
    if (key < root->key)      root->left  = bst_insert(root->left, key);
    else if (key > root->key) root->right = bst_insert(root->right, key);
    return root;
}
int bst_contains(const TNode *root, int key) {
    while (root && root->key != key) root = key < root->key ? root->left : root->right;
    return root != NULL;
}
void bst_inorder(const TNode *root, void (*visit)(int)) {      /* visits keys in sorted order */
    if (!root) return;
    bst_inorder(root->left, visit);
    visit(root->key);
    bst_inorder(root->right, visit);
}
void bst_free(TNode *root) {
    if (!root) return;
    bst_free(root->left); bst_free(root->right); free(root);    /* post-order: children first */
}
```

Recursion is natural here: a tree is defined recursively. Every operation is O(h) where `h` is the height — O(log n) for a bushy tree, O(n) if you insert sorted keys (the tree becomes a linked list). Balanced variants (AVL, red-black) fix that with rotations; in practice, when you need ordered keys, sort an array once or use a heap. A BST is mainly worth knowing because so many other things (k-d trees for nearest-neighbor, the Barnes-Hut quadtree/octree) are trees with the same recursive shape.

**Python equivalent:** none built in (`sortedcontainers` is a third-party library). Python people use `sorted()` or `bisect`.

---

## 7. Binary heap / priority queue

A binary heap is a complete binary tree stored in an array, with the **heap property**: every parent ≤ its children (min-heap). The minimum is always at index 0.

```
array:  [1, 3, 2, 7, 4, 5]
tree:        1
           /   \
          3     2
         / \   /
        7   4 5
parent(i) = (i-1)/2     left(i) = 2i+1     right(i) = 2i+2
```

```c
typedef struct { double *keys; size_t len, cap; } MinHeap;

static void swap(double *a, double *b) { double t = *a; *a = *b; *b = t; }

/* Push: append, then sift UP while smaller than parent. O(log n). */
int heap_push(MinHeap *h, double k) {
    if (h->len == h->cap) { /* grow with realloc as in Vec */ }
    size_t i = h->len++;
    h->keys[i] = k;
    while (i > 0 && h->keys[(i - 1) / 2] > h->keys[i]) { swap(&h->keys[i], &h->keys[(i - 1) / 2]); i = (i - 1) / 2; }
    return 0;
}
/* Pop min: move last to root, then sift DOWN toward the smaller child. O(log n). */
int heap_pop(MinHeap *h, double *out) {
    if (h->len == 0) return -1;
    *out = h->keys[0];
    h->keys[0] = h->keys[--h->len];
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->len && h->keys[l] < h->keys[m]) m = l;
        if (r < h->len && h->keys[r] < h->keys[m]) m = r;
        if (m == i) break;
        swap(&h->keys[i], &h->keys[m]); i = m;
    }
    return 0;
}
```

In practice the heap stores `{double priority; int payload;}` pairs — the payload is an event, a particle index, a graph node. Uses in this course: Barnes-Hut (process cells by distance), k-NN (keep the `k` nearest seen so far in a *max*-heap of size `k`; pop when a closer one arrives), Dijkstra / A* on a grid, event-driven simulation (next collision time), Huffman coding, and "top-k tokens" in sampling from a language model. Popping all `n` elements gives you heapsort: O(n log n).

**Python equivalent:** `heapq.heappush` / `heapq.heappop`, which operate on a plain list exactly like this.

---

## 8. `qsort` and `bsearch`

You do not need to write sorting. `<stdlib.h>` has a generic sort and binary search that take a comparator function:

```c
int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);             /* -1, 0, 1 — avoids the overflow of `return x - y` for ints and works for doubles */
}
double arr[] = {3.5, -1, 2, 9, 0};
qsort(arr, 5, sizeof arr[0], cmp_double);            // -1 0 2 3.5 9

double key = 2;
double *found = bsearch(&key, arr, 5, sizeof arr[0], cmp_double);   // pointer into arr, or NULL
```

The comparator receives `const void *` pointers *to elements*; cast them to the element type. To sort structs by a field, compare that field. To sort indices by the values they point to (argsort — `np.argsort`), compare `vals[*(int*)a]` against `vals[*(int*)b]` via a file-scope pointer or by sorting an array of `{value, index}` pairs. `qsort` is not stable; if equal elements must keep their order, add the original index as a tiebreaker.

`bsearch` requires the array to be sorted with the *same* comparator. A sorted array plus `bsearch` is the right structure for "build once, look up many times" — e.g. a sorted vocabulary of string pointers, or the breakpoints of a piecewise interpolation table.

---

## 9. Memory ownership in containers

Every container must answer: **does the container own the elements, or just refer to them?**

| Policy | Container stores | On `container_free` | Example |
|---|---|---|---|
| By value | copies of the elements (`memcpy`) | free the storage only | `IntVec`, `MinHeap` of doubles |
| Owning pointers | pointers it allocated (or was given) | free each element, then the storage | `StrMap` (owns copied keys), a `Vec` of `Matrix` |
| Borrowed pointers | pointers to things owned elsewhere | free the storage only | a `Vec<Particle*>` sorted by x for a sweep |

Mixing these is how leaks and double frees happen. Write the policy in the struct's comment. For owning containers of non-trivial elements, take a destructor callback: `void vec_free_with(Vec *v, void (*free_elem)(void *))`. When an element is a struct with heap fields (a `Matrix`), storing it *by value* in a `Vec` is fine — but then `vec_free` must call `mat_free` on each element before freeing the array.

---

## 10. Iteration patterns without iterators

C has no `for x in container`. Patterns:

```c
/* arrays: index */
for (size_t i = 0; i < v.len; i++) use(v.data[i]);

/* linked list: pointer walk */
for (Node *n = head; n; n = n->next) use(n->value);

/* hash table: scan every slot, skip empties */
for (size_t i = 0; i < m.cap; i++)
    if (m.slots[i].key && m.slots[i].key != TOMBSTONE) use(m.slots[i].key, m.slots[i].value);

/* tree: recursion with a callback, or an explicit stack */
bst_inorder(root, print_key);

/* callback with user context: the void* passes state through without globals */
void map_foreach(const StrMap *m, void (*fn)(const char *k, int v, void *ctx), void *ctx);
```

The callback-with-`void *ctx` idiom replaces closures: pack whatever the callback needs into a struct, pass its address as `ctx`, cast it back inside the callback. Modifying a container while iterating it (deleting from a hash table you are scanning, pushing to a `Vec` you are indexing) is a bug in every language; in C it is a use-after-free when the `realloc` moves the data.

---

## 11. Choosing a structure — decision guide

1. **Do you need lookup by key?** → hash table. Ordered keys or range queries too? → sorted array (static) or BST (dynamic).
2. **Do you repeatedly need the smallest/largest?** → binary heap. Only once at the end? → `qsort`.
3. **FIFO?** → ring buffer. **LIFO?** → array stack.
4. **Do you hold a node and need O(1) insert/delete *there*, with no index access?** → linked list. (Ask twice.)
5. **Otherwise** → dynamic array. Sort it with `qsort` if you need order; `bsearch` if you then need lookup.

For sizes under ~100 elements, a linear scan of an array beats every structure above. Measure before optimizing.

---

## Gotchas and undefined behavior

- **Storing the caller's string pointer as a hash key** instead of copying: all keys alias one buffer. Copy on insert, free on remove.
- **Pointer arithmetic on `void *`** is a GNU extension, not C. Cast to `char *` first.
- **Holding a pointer into a dynamic array across a push** — `realloc` may move it. Store indices.
- **Deleting from an open-addressing table by setting the key to NULL** breaks later lookups. Use tombstones.
- **`return a - b` in an `int` comparator** overflows for large magnitudes; `return (a > b) - (a < b)`. For doubles, `a - b` cast to `int` truncates `0.5` to `0` — wrong.
- **Comparator not a strict weak ordering** (e.g. inconsistent on NaN) → `qsort` behavior is undefined; it may read out of bounds.
- **`bsearch` on an unsorted array** or with a different comparator than the sort: garbage result.
- **Freeing a list node before reading `->next`** → use-after-free. Save `next` first.
- **Recursive BST/tree functions on a degenerate 1,000,000-node chain** → stack overflow. Balance the tree or use an explicit stack.
- **Heap index math with `size_t`:** `(i - 1) / 2` when `i == 0` underflows to a huge number. Guard with `i > 0` first (as in `heap_push`).
- **Iterating a hash table while inserting** may trigger a resize and move every slot.
- **`hash % cap` when `cap` is 0** → division by zero. Initialize capacity before the first insert.

---

## Common mistakes checklist

- [ ] Default to a dynamic array; justify anything else with a Big-O need.
- [ ] Every container struct comment states its ownership policy (by value / owns / borrows).
- [ ] Hash tables copy string keys on insert and free them on removal and on `free`.
- [ ] Open-addressing deletion uses tombstones; resizing rehashes without them.
- [ ] Load factor checked before insert; capacity doubles.
- [ ] Comparators return `(a > b) - (a < b)`, never `a - b`.
- [ ] Heap sift-up guards `i > 0` before computing the parent.
- [ ] Linked-list free saves `next` before `free(node)`.
- [ ] Tree free is post-order (children before parent).
- [ ] No pointers held into a container across a mutation.
- [ ] `void *` arithmetic done through `char *`.

---

## You can move on when...

- You can write `da_push` for a generic `void *` array and the `DEFINE_VEC` macro, and say what each gains and loses.
- You can implement a string→int hash table with chaining *and* one with linear probing + tombstones, both with resizing, and run them under ASan without leaks.
- You can explain why hash tables must copy string keys, with a concrete failure scenario.
- You can write `heap_push`/`heap_pop` and use them to pop the `k` smallest of `n` numbers in O(n log k).
- You can implement BST insert/search/inorder recursively and explain when the height degenerates.
- You can write a `qsort` comparator for a struct field and an argsort via index pairs.
- You can fill in the Big-O table from memory and pick the structure for: token vocabulary, BPE pair counting, k-NN, BFS on a grid, event scheduling.

Next: the projects in `../projects/` — start with the matrix library and the BPE tokenizer, which use everything from Chapters 06–10.
