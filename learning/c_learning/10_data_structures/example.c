/*
 * Chapter 10 — Data Structures: worked example
 *
 * Compile + run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * Check for leaks (Apple Silicon: no valgrind, no LeakSanitizer):
 *   cc -g -fsanitize=address,undefined -Wall -Wextra -std=c11 -o ex_demo example.c -lm && ./ex_demo
 *   cc -g -O0 -Wall -Wextra -std=c11 -o ex_demo example.c -lm && leaks --atExit -- ./ex_demo
 *
 * Demonstrates, each as a small self-contained module you could move to src/:
 *   1. generic dynamic array (void* + elem_size) and the DEFINE_VEC macro
 *   2. singly linked list with Node** removal
 *   3. array stack and ring-buffer queue
 *   4. hash functions (djb2, FNV-1a, integer mixer)
 *   5. string->int hash table, chaining, copies keys, resizes
 *   6. int->int hash table, open addressing, tombstones, resizes  (BPE pair counter)
 *   7. binary search tree: insert / contains / inorder / free
 *   8. binary min-heap: push / pop; k smallest
 *   9. qsort + bsearch with comparators; argsort
 *  10. iteration with a callback + void* context
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ================================================================== 1a. generic array */
typedef struct { void *data; size_t len, cap, elem_size; } DynArray;   /* stores elements BY VALUE */

static void da_init(DynArray *a, size_t elem_size) { a->data = NULL; a->len = a->cap = 0; a->elem_size = elem_size; }
/* Appends a copy of *elem. Returns 0 or -1 on allocation failure. */
static int da_push(DynArray *a, const void *elem) {
    if (a->len == a->cap) {
        size_t ncap = a->cap ? a->cap * 2 : 8;
        void *t = realloc(a->data, ncap * a->elem_size);
        if (!t) return -1;
        a->data = t; a->cap = ncap;
    }
    memcpy((char *)a->data + a->len * a->elem_size, elem, a->elem_size);   /* char* for byte math */
    a->len++;
    return 0;
}
static void *da_at(const DynArray *a, size_t i) { return (char *)a->data + i * a->elem_size; }
static void  da_free(DynArray *a) { free(a->data); da_init(a, a->elem_size); }

/* ================================================================== 1b. macro-generated typed vec */
#define DEFINE_VEC(T, Name)                                                     \
    typedef struct { T *data; size_t len, cap; } Name;                          \
    static int Name##_push(Name *v, T x) {                                      \
        if (v->len == v->cap) {                                                 \
            size_t ncap = v->cap ? v->cap * 2 : 8;                              \
            T *t = realloc(v->data, ncap * sizeof *t);                          \
            if (!t) return -1;                                                  \
            v->data = t; v->cap = ncap;                                         \
        }                                                                       \
        v->data[v->len++] = x;                                                  \
        return 0;                                                               \
    }                                                                           \
    static void Name##_free(Name *v) { free(v->data); v->data = NULL; v->len = v->cap = 0; }

DEFINE_VEC(int, IntVec)
DEFINE_VEC(double, DblVec)

/* ================================================================== 2. linked list */
typedef struct Node { int value; struct Node *next; } Node;

static Node *list_push_front(Node *head, int v) {
    Node *n = malloc(sizeof *n);
    if (!n) return head;
    n->value = v; n->next = head;
    return n;
}
/* Removes every node with value v in one pass. Returns the new head. */
static Node *list_remove_all(Node *head, int v) {
    Node **pp = &head;                          /* points at head, or at some node's ->next */
    while (*pp) {
        if ((*pp)->value == v) { Node *dead = *pp; *pp = dead->next; free(dead); }   /* splice; do not advance */
        else pp = &(*pp)->next;
    }
    return head;
}
static void list_print(const Node *head) { for (; head; head = head->next) printf("%d ", head->value); printf("\n"); }
static void list_free(Node *head) { while (head) { Node *nx = head->next; free(head); head = nx; } }

/* ================================================================== 3. stack + ring queue */
/* stack: just IntVec + pop */
static int stack_pop(IntVec *s, int *out) { if (!s->len) return -1; *out = s->data[--s->len]; return 0; }

typedef struct { int *data; size_t head, len, cap; } Queue;   /* fixed capacity ring buffer */
static int queue_init(Queue *q, size_t cap) { q->data = malloc(cap * sizeof *q->data); q->head = q->len = 0; q->cap = cap; return q->data ? 0 : -1; }
static int queue_push(Queue *q, int v) { if (q->len == q->cap) return -1; q->data[(q->head + q->len) % q->cap] = v; q->len++; return 0; }
static int queue_pop(Queue *q, int *out) { if (!q->len) return -1; *out = q->data[q->head]; q->head = (q->head + 1) % q->cap; q->len--; return 0; }
static void queue_free(Queue *q) { free(q->data); q->data = NULL; q->len = q->cap = 0; }

/* ================================================================== 4. hash functions */
static uint64_t hash_djb2(const char *s) { uint64_t h = 5381; for (; *s; s++) h = h * 33 + (unsigned char)*s; return h; }
static uint64_t hash_fnv1a(const void *key, size_t len) {
    const unsigned char *p = key; uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
static uint64_t hash_u64(uint64_t x) {           /* bit mixer for integer keys */
    x ^= x >> 33; x *= 0xff51afd7ed558ccdull; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ull; x ^= x >> 33;
    return x;
}

/* ================================================================== 5. StrMap: chaining */
typedef struct Entry { char *key; int value; struct Entry *next; } Entry;   /* key is OWNED (copied) */
typedef struct { Entry **buckets; size_t cap, count; } StrMap;

static int strmap_init(StrMap *m, size_t cap) { m->buckets = calloc(cap, sizeof *m->buckets); m->cap = cap; m->count = 0; return m->buckets ? 0 : -1; }

static int strmap_grow(StrMap *m) {
    size_t ncap = m->cap * 2;
    Entry **nb = calloc(ncap, sizeof *nb);
    if (!nb) return -1;
    for (size_t i = 0; i < m->cap; i++)                          /* re-link every node into the new table */
        for (Entry *e = m->buckets[i], *nx; e; e = nx) {
            nx = e->next;
            size_t idx = hash_djb2(e->key) % ncap;
            e->next = nb[idx]; nb[idx] = e;
        }
    free(m->buckets); m->buckets = nb; m->cap = ncap;
    return 0;
}
/* Returns pointer to the value slot for key, inserting value 0 if absent. NULL on alloc failure.
 * The pointer is valid until the next insert (which may resize). */
static int *strmap_get_or_insert(StrMap *m, const char *key) {
    if ((m->count + 1) * 4 > m->cap * 3 && strmap_grow(m) != 0) return NULL;   /* load factor 0.75 */
    size_t idx = hash_djb2(key) % m->cap;
    for (Entry *e = m->buckets[idx]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return &e->value;
    Entry *e = malloc(sizeof *e);
    if (!e) return NULL;
    size_t n = strlen(key) + 1;
    e->key = malloc(n);                                          /* COPY the key */
    if (!e->key) { free(e); return NULL; }
    memcpy(e->key, key, n);
    e->value = 0; e->next = m->buckets[idx]; m->buckets[idx] = e; m->count++;
    return &e->value;
}
/* Returns 0 and sets *out if found, else -1. */
static int strmap_get(const StrMap *m, const char *key, int *out) {
    for (Entry *e = m->buckets[hash_djb2(key) % m->cap]; e; e = e->next)
        if (strcmp(e->key, key) == 0) { *out = e->value; return 0; }
    return -1;
}
static int strmap_remove(StrMap *m, const char *key) {
    Entry **pp = &m->buckets[hash_djb2(key) % m->cap];
    while (*pp && strcmp((*pp)->key, key) != 0) pp = &(*pp)->next;
    if (!*pp) return -1;
    Entry *dead = *pp; *pp = dead->next;
    free(dead->key); free(dead); m->count--;
    return 0;
}
/* Iteration via callback + user context (replaces closures). */
static void strmap_foreach(const StrMap *m, void (*fn)(const char *k, int v, void *ctx), void *ctx) {
    for (size_t i = 0; i < m->cap; i++)
        for (Entry *e = m->buckets[i]; e; e = e->next) fn(e->key, e->value, ctx);
}
static void strmap_free(StrMap *m) {
    for (size_t i = 0; i < m->cap; i++)
        for (Entry *e = m->buckets[i], *nx; e; e = nx) { nx = e->next; free(e->key); free(e); }
    free(m->buckets); m->buckets = NULL; m->cap = m->count = 0;
}

/* ================================================================== 6. IntMap: open addressing */
#define IM_EMPTY INT64_MIN
#define IM_TOMB  (INT64_MIN + 1)
typedef struct { int64_t key, value; } ISlot;
typedef struct { ISlot *slots; size_t cap, count, tombs; } IntMap;   /* cap is a power of two */

static int intmap_init(IntMap *m, size_t cap) {
    m->slots = malloc(cap * sizeof *m->slots);
    if (!m->slots) return -1;
    for (size_t i = 0; i < cap; i++) m->slots[i].key = IM_EMPTY;
    m->cap = cap; m->count = m->tombs = 0;
    return 0;
}
static ISlot *intmap_probe(IntMap *m, int64_t key, int for_insert) {
    size_t idx = hash_u64((uint64_t)key) & (m->cap - 1);
    ISlot *first_tomb = NULL;
    for (;;) {
        ISlot *s = &m->slots[idx];
        if (s->key == IM_EMPTY) return (for_insert && first_tomb) ? first_tomb : s;
        if (s->key == IM_TOMB) { if (!first_tomb) first_tomb = s; }
        else if (s->key == key) return s;
        idx = (idx + 1) & (m->cap - 1);                  /* linear probing */
    }
}
static int intmap_put(IntMap *m, int64_t key, int64_t value);
static int intmap_grow(IntMap *m) {
    IntMap n;
    if (intmap_init(&n, m->cap * 2) != 0) return -1;
    for (size_t i = 0; i < m->cap; i++)                 /* rehash live slots only: tombstones vanish */
        if (m->slots[i].key != IM_EMPTY && m->slots[i].key != IM_TOMB) intmap_put(&n, m->slots[i].key, m->slots[i].value);
    free(m->slots); *m = n;
    return 0;
}
static int intmap_put(IntMap *m, int64_t key, int64_t value) {
    if ((m->count + m->tombs + 1) * 10 > m->cap * 7 && intmap_grow(m) != 0) return -1;   /* load 0.7 incl. tombstones */
    ISlot *s = intmap_probe(m, key, 1);
    if (s->key == IM_TOMB) m->tombs--;
    if (s->key != key) { s->key = key; m->count++; }
    s->value = value;
    return 0;
}
static int64_t *intmap_get(IntMap *m, int64_t key) { ISlot *s = intmap_probe(m, key, 0); return s->key == key ? &s->value : NULL; }
static int intmap_remove(IntMap *m, int64_t key) {
    ISlot *s = intmap_probe(m, key, 0);
    if (s->key != key) return -1;
    s->key = IM_TOMB; m->count--; m->tombs++;          /* NOT IM_EMPTY: that would break later probes */
    return 0;
}
static void intmap_free(IntMap *m) { free(m->slots); m->slots = NULL; m->cap = m->count = m->tombs = 0; }

/* ================================================================== 7. BST */
typedef struct TNode { int key; struct TNode *left, *right; } TNode;
static TNode *bst_insert(TNode *root, int key) {
    if (!root) { TNode *n = malloc(sizeof *n); if (n) { n->key = key; n->left = n->right = NULL; } return n; }
    if (key < root->key)      root->left  = bst_insert(root->left, key);
    else if (key > root->key) root->right = bst_insert(root->right, key);
    return root;
}
static int bst_contains(const TNode *r, int key) { while (r && r->key != key) r = key < r->key ? r->left : r->right; return r != NULL; }
static void bst_inorder(const TNode *r, IntVec *out) { if (!r) return; bst_inorder(r->left, out); IntVec_push(out, r->key); bst_inorder(r->right, out); }
static int  bst_height(const TNode *r) { if (!r) return 0; int l = bst_height(r->left), h = bst_height(r->right); return 1 + (l > h ? l : h); }
static void bst_free(TNode *r) { if (!r) return; bst_free(r->left); bst_free(r->right); free(r); }   /* post-order */

/* ================================================================== 8. min-heap */
typedef struct { double *keys; size_t len, cap; } MinHeap;
static void hswap(double *a, double *b) { double t = *a; *a = *b; *b = t; }
static int heap_push(MinHeap *h, double k) {
    if (h->len == h->cap) {
        size_t ncap = h->cap ? h->cap * 2 : 8;
        double *t = realloc(h->keys, ncap * sizeof *t);
        if (!t) return -1;
        h->keys = t; h->cap = ncap;
    }
    size_t i = h->len++;
    h->keys[i] = k;
    while (i > 0 && h->keys[(i - 1) / 2] > h->keys[i]) { hswap(&h->keys[i], &h->keys[(i - 1) / 2]); i = (i - 1) / 2; }  /* sift up */
    return 0;
}
static int heap_pop(MinHeap *h, double *out) {
    if (!h->len) return -1;
    *out = h->keys[0];
    h->keys[0] = h->keys[--h->len];
    size_t i = 0;
    for (;;) {                                                     /* sift down */
        size_t l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->len && h->keys[l] < h->keys[m]) m = l;
        if (r < h->len && h->keys[r] < h->keys[m]) m = r;
        if (m == i) break;
        hswap(&h->keys[i], &h->keys[m]); i = m;
    }
    return 0;
}
static void heap_free(MinHeap *h) { free(h->keys); h->keys = NULL; h->len = h->cap = 0; }

/* ================================================================== 9. comparators */
static int cmp_double(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
typedef struct { double value; int index; } Indexed;             /* for argsort */
static int cmp_indexed(const void *a, const void *b) { const Indexed *x = a, *y = b; return (x->value > y->value) - (x->value < y->value); }
static int cmp_str(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

/* ================================================================== 10. callback context */
typedef struct { const char *best; int best_count; int total; } WordStats;
static void collect_stats(const char *k, int v, void *ctx) {
    WordStats *s = ctx;
    s->total += v;
    if (v > s->best_count || (v == s->best_count && strcmp(k, s->best) < 0)) { s->best = k; s->best_count = v; }
}

/* ================================================================== main */
int main(void) {
    printf("=== 1. dynamic arrays ===\n");
    typedef struct { double x, y; } Pt;
    DynArray pts; da_init(&pts, sizeof(Pt));
    for (int i = 0; i < 100; i++) { Pt p = {i, 2.0 * i}; if (da_push(&pts, &p) != 0) return 1; }
    Pt *last = da_at(&pts, pts.len - 1);
    printf("generic: len=%zu cap=%zu elem_size=%zu last=(%.0f,%.0f)\n", pts.len, pts.cap, pts.elem_size, last->x, last->y);
    da_free(&pts);
    IntVec iv = {0};
    for (int i = 0; i < 20; i++) IntVec_push(&iv, i * i);
    printf("IntVec (macro): len=%zu cap=%zu data[19]=%d (typed: v.data[i] is an int)\n", iv.len, iv.cap, iv.data[19]);
    IntVec_free(&iv);
    DblVec dv = {0};                                           /* second instantiation of the same macro */
    for (int i = 0; i < 5; i++) DblVec_push(&dv, i * 0.25);
    printf("DblVec (macro): len=%zu data[4]=%.2f (same code, generated for double)\n", dv.len, dv.data[4]);
    DblVec_free(&dv);

    printf("\n=== 2. linked list ===\n");
    Node *head = NULL;
    int vals[] = {2, 4, 2, 3, 2, 1};
    for (int i = 5; i >= 0; i--) head = list_push_front(head, vals[i]);
    printf("list:              "); list_print(head);
    head = list_remove_all(head, 2);
    printf("remove_all(2):     "); list_print(head);
    head = list_remove_all(head, 1);                          /* removing the head works via Node** */
    printf("remove_all(1):     "); list_print(head);
    list_free(head);

    printf("\n=== 3. stack and ring queue ===\n");
    IntVec st = {0};
    for (int i = 1; i <= 4; i++) IntVec_push(&st, i * 10);
    int x;
    printf("stack pops (LIFO): ");
    while (stack_pop(&st, &x) == 0) printf("%d ", x);
    printf("\n");
    IntVec_free(&st);
    Queue q;
    if (queue_init(&q, 4) != 0) return 1;
    for (int i = 1; i <= 3; i++) queue_push(&q, i);
    queue_pop(&q, &x); queue_pop(&q, &x);                     /* head advances to 2 */
    for (int i = 4; i <= 6; i++) queue_push(&q, i);           /* wraps around: slots 3,0,1 */
    printf("queue after wrap: head=%zu len=%zu cap=%zu; pops (FIFO): ", q.head, q.len, q.cap);
    while (queue_pop(&q, &x) == 0) printf("%d ", x);
    printf("\n");
    queue_free(&q);

    printf("\n=== 4. hash functions ===\n");
    printf("djb2(\"cat\")=%016llx  fnv1a(\"cat\")=%016llx\n",
           (unsigned long long)hash_djb2("cat"), (unsigned long long)hash_fnv1a("cat", 3));
    printf("hash_u64(1)=%016llx hash_u64(2)=%016llx  (sequential keys -> scattered buckets)\n",
           (unsigned long long)hash_u64(1), (unsigned long long)hash_u64(2));

    printf("\n=== 5. StrMap (chaining, owns copied keys, resizes) ===\n");
    StrMap vocab;
    if (strmap_init(&vocab, 4) != 0) return 1;
    const char *text = "the cat sat on the mat the end";
    char buf[128];
    strncpy(buf, text, sizeof buf - 1); buf[sizeof buf - 1] = '\0';
    for (char *w = strtok(buf, " "); w; w = strtok(NULL, " ")) {  /* w points into buf: the map MUST copy */
        int *slot = strmap_get_or_insert(&vocab, w);
        if (!slot) return 1;
        (*slot)++;
    }
    memset(buf, 'X', sizeof buf - 1);                          /* clobber buf: keys survive because they were copied */
    int c;
    printf("count(\"the\")=%d  count(\"cat\")=%d  has(\"dog\")=%s\n",
           (strmap_get(&vocab, "the", &c), c), (strmap_get(&vocab, "cat", &c), c),
           strmap_get(&vocab, "dog", &c) == 0 ? "yes" : "no");
    printf("unique=%zu cap=%zu (grew from 4)\n", vocab.count, vocab.cap);
    WordStats ws = {"", 0, 0};
    strmap_foreach(&vocab, collect_stats, &ws);                /* callback + void* ctx */
    printf("foreach: total tokens=%d, most frequent=\"%s\" x%d\n", ws.total, ws.best, ws.best_count);
    strmap_remove(&vocab, "cat");
    printf("after remove(\"cat\"): has(\"cat\")=%s unique=%zu\n", strmap_get(&vocab, "cat", &c) == 0 ? "yes" : "no", vocab.count);
    strmap_free(&vocab);

    printf("\n=== 6. IntMap (open addressing, tombstones): BPE pair counting ===\n");
    IntMap pairs;
    if (intmap_init(&pairs, 8) != 0) return 1;
    const char *corpus = "aaabdaaabac";                         /* the classic BPE example */
    size_t n = strlen(corpus);
    for (size_t i = 0; i + 1 < n; i++) {
        int64_t key = ((int64_t)(unsigned char)corpus[i] << 32) | (unsigned char)corpus[i + 1];
        int64_t *v = intmap_get(&pairs, key);
        if (v) (*v)++; else if (intmap_put(&pairs, key, 1) != 0) return 1;
    }
    int64_t best_key = 0, best = 0;
    for (size_t i = 0; i < pairs.cap; i++)
        if (pairs.slots[i].key != IM_EMPTY && pairs.slots[i].key != IM_TOMB && pairs.slots[i].value > best) { best = pairs.slots[i].value; best_key = pairs.slots[i].key; }
    printf("corpus \"%s\": %zu distinct pairs, cap=%zu; most frequent pair ('%c','%c') x%lld -> first BPE merge\n",
           corpus, pairs.count, pairs.cap, (char)(best_key >> 32), (char)(best_key & 0xff), (long long)best);
    intmap_remove(&pairs, best_key);
    printf("after remove: count=%zu tombstones=%zu; lookup of removed key -> %s; other keys still found: %s\n",
           pairs.count, pairs.tombs, intmap_get(&pairs, best_key) ? "found" : "absent",
           intmap_get(&pairs, ((int64_t)'a' << 32) | 'b') ? "yes" : "no");
    intmap_free(&pairs);

    printf("\n=== 7. BST ===\n");
    TNode *root = NULL;
    int keys[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) root = bst_insert(root, keys[i]);
    IntVec sorted = {0};
    bst_inorder(root, &sorted);
    printf("inorder: ");
    for (size_t i = 0; i < sorted.len; i++) printf("%d ", sorted.data[i]);
    printf(" (sorted)   contains(6)=%d contains(5)=%d height=%d\n", bst_contains(root, 6), bst_contains(root, 5), bst_height(root));
    IntVec_free(&sorted);
    bst_free(root);
    TNode *chain = NULL;
    for (int i = 0; i < 50; i++) chain = bst_insert(chain, i);  /* sorted input -> degenerate */
    printf("50 sorted inserts -> height %d (a linked list in disguise)\n", bst_height(chain));
    bst_free(chain);

    printf("\n=== 8. min-heap: k smallest of n ===\n");
    MinHeap h = {0};
    double nums[] = {9.5, 2.0, 7.7, 1.1, 8.8, 3.3, 6.6, 0.4, 5.5, 4.4};
    for (size_t i = 0; i < 10; i++) heap_push(&h, nums[i]);
    printf("heap array (parent <= children): ");
    for (size_t i = 0; i < h.len; i++) printf("%.1f ", h.keys[i]);
    printf("\n3 smallest via pop: ");
    double m;
    for (int i = 0; i < 3 && heap_pop(&h, &m) == 0; i++) printf("%.1f ", m);
    printf("  (each pop O(log n))\n");
    heap_free(&h);

    printf("\n=== 9. qsort / bsearch / argsort ===\n");
    double arr[] = {3.5, -1.0, 2.0, 9.0, 0.0};
    qsort(arr, 5, sizeof arr[0], cmp_double);
    printf("qsort: ");
    for (int i = 0; i < 5; i++) printf("%.1f ", arr[i]);
    double key = 2.0;
    double *found = bsearch(&key, arr, 5, sizeof arr[0], cmp_double);
    printf("\nbsearch(2.0) -> index %td;  bsearch(4.0) -> %s\n", found ? found - arr : -1,
           (key = 4.0, bsearch(&key, arr, 5, sizeof arr[0], cmp_double)) ? "found" : "NULL");
    Indexed idx[] = {{0.9, 0}, {0.1, 1}, {0.5, 2}, {0.3, 3}};   /* argsort: sort (value,index) pairs */
    qsort(idx, 4, sizeof idx[0], cmp_indexed);
    printf("argsort of [0.9 0.1 0.5 0.3] = ");
    for (int i = 0; i < 4; i++) printf("%d ", idx[i].index);
    const char *words[] = {"tanh", "relu", "gelu", "sigmoid"};
    qsort(words, 4, sizeof words[0], cmp_str);
    printf("\nsorted strings: %s %s %s %s\n", words[0], words[1], words[2], words[3]);

    printf("\nAll containers freed. Ownership: DynArray/IntVec/MinHeap by value; StrMap owns copied keys;\n"
           "IntMap by value; list/BST own their nodes. Run under ASan and `leaks` to confirm.\n");
    return 0;
}
