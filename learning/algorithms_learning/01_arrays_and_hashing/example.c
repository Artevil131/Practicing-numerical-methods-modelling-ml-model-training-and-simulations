/*
 * Chapter 01 — Arrays & hashing: pattern skeletons.
 *
 * Compile + run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * This file is NOT a set of LeetCode solutions. It is the toolbox the chapter's
 * problems are built from, each demonstrated on a tiny neutral input:
 *
 *   1. IntSet  — open-addressing hash set for int keys (linear probing)
 *   2. LLMap   — open-addressing hash map, long long key -> long long value
 *   3. Array as its own set: sign marking, cyclic sort
 *   4. Frequency counting: int[26] histogram, bucket-by-frequency, Boyer-Moore
 *   5. Hash map as memory: complement lookup (query BEFORE insert)
 *   6. Prefix sums: P[] with half-open ranges, prefix + map, negative modulo
 *   7. qsort with a safe comparator: interval merge sweep
 *   8. Matrix index arithmetic: transpose, rotate, spiral, two-bit stencil
 *
 * Every helper is small enough to paste into a solution file. Copy, do not
 * #include — the learner's solutions must stay single-file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ------------------------------------------------------------------------- */
/* 1. IntSet: open addressing, linear probing, power-of-two capacity          */
/* ------------------------------------------------------------------------- */

typedef struct {
    int           *keys;   /* key stored in slot i (valid only if used[i])   */
    unsigned char *used;   /* 1 = occupied, 0 = empty                       */
    size_t         cap;    /* power of two, >= 2 * expected element count   */
    size_t         len;
} IntSet;

/* Round up to a power of two, at least 8. */
static size_t pow2_at_least(size_t n) {
    size_t c = 8;
    while (c < n) c <<= 1;
    return c;
}

/* Knuth multiplicative hash. Cast to unsigned FIRST so negative keys hash fine. */
static size_t hash_int(int x, size_t cap) {
    unsigned h = (unsigned)x * 2654435761u;
    h ^= h >> 16;                  /* mix the high bits down */
    return h & (cap - 1);          /* cap is a power of two -> cheap modulo */
}

static void set_init(IntSet *s, size_t expected) {
    s->cap  = pow2_at_least(2 * expected);         /* load factor <= 0.5 */
    s->keys = malloc(s->cap * sizeof *s->keys);
    s->used = calloc(s->cap, 1);
    s->len  = 0;
    if (!s->keys || !s->used) { fputs("out of memory\n", stderr); exit(1); }
}
static void set_free(IntSet *s) { free(s->keys); free(s->used); s->cap = s->len = 0; }

static int set_contains(const IntSet *s, int x) {
    size_t i = hash_int(x, s->cap);
    while (s->used[i]) {                 /* stop at the first empty slot   */
        if (s->keys[i] == x) return 1;
        i = (i + 1) & (s->cap - 1);      /* linear probe, wrap around      */
    }
    return 0;
}

/* Returns 1 if x was already present, 0 if it was inserted now. */
static int set_insert(IntSet *s, int x) {
    size_t i = hash_int(x, s->cap);
    while (s->used[i]) {
        if (s->keys[i] == x) return 1;
        i = (i + 1) & (s->cap - 1);
    }
    s->used[i] = 1; s->keys[i] = x; s->len++;
    return 0;
}

/* Pattern: first duplicate in one pass. Returns the duplicate or INT_MIN. */
static int first_duplicate(const int *a, int n) {
    IntSet s; set_init(&s, (size_t)n);
    int result = INT_MIN;
    for (int i = 0; i < n; i++)
        if (set_insert(&s, a[i])) { result = a[i]; break; }   /* invariant: set == a[0..i-1] */
    set_free(&s);
    return result;
}

/* Pattern: longest run of consecutive integers. Start only from run heads. */
static int longest_consecutive(const int *a, int n) {
    IntSet s; set_init(&s, (size_t)n);
    for (int i = 0; i < n; i++) set_insert(&s, a[i]);
    int best = 0;
    for (int i = 0; i < n; i++) {
        if (set_contains(&s, a[i] - 1)) continue;   /* not a run head: skip */
        int len = 1;
        while (set_contains(&s, a[i] + len)) len++; /* each value extended at most once overall */
        if (len > best) best = len;
    }
    set_free(&s);
    return best;
}

/* ------------------------------------------------------------------------- */
/* 2. LLMap: long long key -> long long value (prefix sums overflow int)       */
/* ------------------------------------------------------------------------- */

typedef struct {
    long long     *keys;
    long long     *vals;
    unsigned char *used;
    size_t         cap, len;
} LLMap;

static size_t hash_ll(long long x, size_t cap) {
    unsigned long long h = (unsigned long long)x * 0x9E3779B97F4A7C15ull;  /* golden-ratio constant */
    h ^= h >> 32;
    return (size_t)(h & (cap - 1));
}

static void map_init(LLMap *m, size_t expected) {
    m->cap  = pow2_at_least(2 * expected + 2);
    m->keys = malloc(m->cap * sizeof *m->keys);
    m->vals = malloc(m->cap * sizeof *m->vals);
    m->used = calloc(m->cap, 1);
    m->len  = 0;
    if (!m->keys || !m->vals || !m->used) { fputs("out of memory\n", stderr); exit(1); }
}
static void map_free(LLMap *m) { free(m->keys); free(m->vals); free(m->used); m->cap = m->len = 0; }

/* Slot where key lives, or the empty slot where it would go. */
static size_t map_slot(const LLMap *m, long long key) {
    size_t i = hash_ll(key, m->cap);
    while (m->used[i] && m->keys[i] != key) i = (i + 1) & (m->cap - 1);
    return i;
}

/* Returns 1 and writes *out if key present, else 0. */
static int map_get(const LLMap *m, long long key, long long *out) {
    size_t i = map_slot(m, key);
    if (!m->used[i]) return 0;
    *out = m->vals[i];
    return 1;
}

static void map_put(LLMap *m, long long key, long long val) {
    size_t i = map_slot(m, key);
    if (!m->used[i]) { m->used[i] = 1; m->keys[i] = key; m->len++; }
    m->vals[i] = val;
}

/* map[key] += delta (creating key with value 0 first). The frequency-count primitive. */
static void map_add(LLMap *m, long long key, long long delta) {
    size_t i = map_slot(m, key);
    if (!m->used[i]) { m->used[i] = 1; m->keys[i] = key; m->vals[i] = 0; m->len++; }
    m->vals[i] += delta;
}

/* ------------------------------------------------------------------------- */
/* 3. Array as its own hash set (values bounded to 1..n)                      */
/* ------------------------------------------------------------------------- */

/* Sign marking: print every value that occurs twice. Reads abs(a[i]) always. */
static void demo_sign_marking(int *a, int n) {
    printf("  sign marking on [");
    for (int i = 0; i < n; i++) printf("%d%s", a[i], i + 1 < n ? " " : "");
    printf("]: duplicates =");
    for (int i = 0; i < n; i++) {
        int v   = abs(a[i]);        /* earlier marks may have flipped this sign */
        int idx = v - 1;            /* value v "owns" index v-1                  */
        if (a[idx] < 0) printf(" %d", v);
        else            a[idx] = -a[idx];
    }
    printf("\n");
    for (int i = 0; i < n; i++) a[i] = abs(a[i]);   /* restore */
}

/* Cyclic sort: put every v in [1,n] at index v-1; then the first mismatch is the gap. */
static int first_missing_positive(int *a, int n) {
    for (int i = 0; i < n; i++) {
        /* keep swapping until a[i] is correct, out of range, or a duplicate */
        while (a[i] >= 1 && a[i] <= n && a[a[i] - 1] != a[i]) {
            int j = a[i] - 1;
            int t = a[i]; a[i] = a[j]; a[j] = t;     /* each swap fixes >= 1 slot */
        }
    }
    for (int i = 0; i < n; i++)
        if (a[i] != i + 1) return i + 1;
    return n + 1;
}

/* ------------------------------------------------------------------------- */
/* 4. Frequency counting                                                       */
/* ------------------------------------------------------------------------- */

/* Fixed alphabet: +1 for s, -1 for t, all zero <=> anagram. */
static int is_anagram(const char *s, const char *t) {
    int cnt[26] = {0};
    size_t i;
    for (i = 0; s[i] && t[i]; i++) {
        cnt[(unsigned char)s[i] - 'a']++;         /* cast: char may be signed */
        cnt[(unsigned char)t[i] - 'a']--;
    }
    if (s[i] || t[i]) return 0;                   /* lengths differ */
    for (int c = 0; c < 26; c++) if (cnt[c]) return 0;
    return 1;
}

/* Bucket by frequency: counts live in [1,n], so buckets[f] replaces sorting.
 * Prints the k most frequent values. Uses LLMap for arbitrary int values.   */
static void top_k_frequent(const int *a, int n, int k) {
    LLMap freq; map_init(&freq, (size_t)n);
    for (int i = 0; i < n; i++) map_add(&freq, a[i], 1);

    /* buckets: bucket_head[f] = index into a flat list; store (value) per bucket
       via a simple "next" linked array to avoid n small mallocs.               */
    int *bucket_head = malloc((size_t)(n + 1) * sizeof *bucket_head);
    int *node_val    = malloc(freq.cap * sizeof *node_val);
    int *node_next   = malloc(freq.cap * sizeof *node_next);
    if (!bucket_head || !node_val || !node_next) { fputs("out of memory\n", stderr); exit(1); }
    for (int f = 0; f <= n; f++) bucket_head[f] = -1;

    int nodes = 0;
    for (size_t s = 0; s < freq.cap; s++) {
        if (!freq.used[s]) continue;
        int f = (int)freq.vals[s];                 /* 1 <= f <= n */
        node_val[nodes]  = (int)freq.keys[s];
        node_next[nodes] = bucket_head[f];
        bucket_head[f]   = nodes++;
    }

    printf("  top-%d frequent:", k);
    int taken = 0;
    for (int f = n; f >= 1 && taken < k; f--)      /* scan buckets high -> low */
        for (int node = bucket_head[f]; node != -1 && taken < k; node = node_next[node]) {
            printf(" %d(x%d)", node_val[node], f);
            taken++;
        }
    printf("\n");

    free(bucket_head); free(node_val); free(node_next);
    map_free(&freq);
}

/* Boyer-Moore voting: O(1) space majority element (assumes one exists). */
static int majority_element(const int *a, int n) {
    int cand = 0, cnt = 0;
    for (int i = 0; i < n; i++) {
        if (cnt == 0) cand = a[i];
        cnt += (a[i] == cand) ? 1 : -1;            /* majority can never be fully cancelled */
    }
    return cand;
}

/* ------------------------------------------------------------------------- */
/* 5. Hash map as memory: complement lookup                                   */
/* ------------------------------------------------------------------------- */

/* Find i<j with a[i]+a[j]==target. Query BEFORE insert so an element never pairs with itself. */
static int pair_with_sum(const int *a, int n, int target, int *i_out, int *j_out) {
    LLMap m; map_init(&m, (size_t)n);
    int found = 0;
    for (int j = 0; j < n && !found; j++) {
        long long idx;
        if (map_get(&m, (long long)target - a[j], &idx)) {  /* invariant: map == a[0..j-1] */
            *i_out = (int)idx; *j_out = j; found = 1;
        } else {
            map_put(&m, a[j], j);                          /* value -> its index */
        }
    }
    map_free(&m);
    return found;
}

/* Bijection check between two equal-length byte strings: both directions. */
static int is_bijection(const char *s, const char *t) {
    int st[256], ts[256];
    for (int i = 0; i < 256; i++) st[i] = ts[i] = -1;
    for (size_t i = 0; s[i] || t[i]; i++) {
        if (!s[i] || !t[i]) return 0;             /* different lengths */
        unsigned char a = (unsigned char)s[i], b = (unsigned char)t[i];
        if (st[a] == -1 && ts[b] == -1) { st[a] = b; ts[b] = a; }   /* add both at once */
        else if (st[a] != b || ts[b] != a) return 0;                 /* must agree both ways */
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* 6. Prefix sums                                                              */
/* ------------------------------------------------------------------------- */

/* Build P with n+1 entries, P[0] = 0, P[i] = sum a[0..i-1]. Caller frees. */
static long long *build_prefix(const int *a, int n) {
    long long *P = malloc((size_t)(n + 1) * sizeof *P);
    if (!P) { fputs("out of memory\n", stderr); exit(1); }
    P[0] = 0;
    for (int i = 0; i < n; i++) P[i + 1] = P[i] + a[i];
    return P;
}

/* Sum of the INCLUSIVE range a[l..r] in O(1). */
static long long range_sum(const long long *P, int l, int r) { return P[r + 1] - P[l]; }

/* Count subarrays with sum == k: prefix + map (value -> count), init {0:1}. */
static long long count_subarrays_with_sum(const int *a, int n, long long k) {
    LLMap m; map_init(&m, (size_t)n);
    map_put(&m, 0, 1);                             /* the empty prefix */
    long long prefix = 0, ans = 0, c;
    for (int i = 0; i < n; i++) {
        prefix += a[i];
        if (map_get(&m, prefix - k, &c)) ans += c; /* query ... */
        map_add(&m, prefix, 1);                    /* ... then record */
    }
    map_free(&m);
    return ans;
}

/* Longest subarray with sum == 0: prefix + map (value -> FIRST index), init {0:-1}. */
static int longest_zero_sum_subarray(const int *a, int n) {
    LLMap m; map_init(&m, (size_t)n);
    map_put(&m, 0, -1);
    long long prefix = 0, first;
    int best = 0;
    for (int i = 0; i < n; i++) {
        prefix += a[i];
        if (map_get(&m, prefix, &first)) {         /* seen before: a[first+1..i] sums to 0 */
            if (i - (int)first > best) best = i - (int)first;
        } else {
            map_put(&m, prefix, i);                /* store only the FIRST occurrence */
        }
    }
    map_free(&m);
    return best;
}

/* C's % truncates toward zero: -7 % 5 == -2. Normalise into [0, k). */
static long long mod_norm(long long x, long long k) { return ((x % k) + k) % k; }

/* ------------------------------------------------------------------------- */
/* 7. Sorting as preprocessing: qsort + interval sweep                         */
/* ------------------------------------------------------------------------- */

typedef struct { int start, end; } Interval;

/* Safe three-way comparison; never `return a - b` (overflow). */
static int cmp_interval_start(const void *pa, const void *pb) {
    const Interval *a = pa, *b = pb;
    return (a->start > b->start) - (a->start < b->start);
}

/* Merge overlapping (or touching) intervals in place; returns new count. */
static int merge_intervals(Interval *v, int n) {
    if (n == 0) return 0;
    qsort(v, (size_t)n, sizeof v[0], cmp_interval_start);
    int w = 0;                                     /* index of the last merged interval */
    for (int i = 1; i < n; i++) {
        if (v[i].start <= v[w].end) {              /* overlap can only be with the neighbour */
            if (v[i].end > v[w].end) v[w].end = v[i].end;   /* max, not overwrite */
        } else {
            v[++w] = v[i];
        }
    }
    return w + 1;
}

/* ------------------------------------------------------------------------- */
/* 8. Matrix index arithmetic on flat row-major buffers                       */
/* ------------------------------------------------------------------------- */

/* (i,j) of an m x n matrix lives at a[i*n + j]. */
#define AT(a, n, i, j) ((a)[(i) * (n) + (j)])

static void print_matrix(const char *label, const int *a, int m, int n) {
    printf("  %s\n", label);
    for (int i = 0; i < m; i++) {
        printf("   ");
        for (int j = 0; j < n; j++) printf(" %2d", AT(a, n, i, j));
        printf("\n");
    }
}

/* out is n x m. */
static void transpose(const int *a, int m, int n, int *out) {
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            AT(out, m, j, i) = AT(a, n, i, j);
}

/* 90 degrees clockwise, square, in place: transpose upper triangle, reverse rows. */
static void rotate_cw_inplace(int *a, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {          /* i<j only, or you undo the swap */
            int t = AT(a, n, i, j); AT(a, n, i, j) = AT(a, n, j, i); AT(a, n, j, i) = t;
        }
    for (int i = 0; i < n; i++)
        for (int l = 0, r = n - 1; l < r; l++, r--) {
            int t = AT(a, n, i, l); AT(a, n, i, l) = AT(a, n, i, r); AT(a, n, i, r) = t;
        }
}

/* Spiral order: four shrinking bounds; re-check bounds before sides 3 and 4. */
static void print_spiral(const int *a, int m, int n) {
    int top = 0, bottom = m - 1, left = 0, right = n - 1;
    printf("  spiral:");
    while (top <= bottom && left <= right) {
        for (int j = left; j <= right; j++) printf(" %d", AT(a, n, top, j));
        top++;
        for (int i = top; i <= bottom; i++) printf(" %d", AT(a, n, i, right));
        right--;
        if (top <= bottom) {                       /* a single remaining row would repeat */
            for (int j = right; j >= left; j--) printf(" %d", AT(a, n, bottom, j));
            bottom--;
        }
        if (left <= right) {                       /* a single remaining column would repeat */
            for (int i = bottom; i >= top; i--) printf(" %d", AT(a, n, i, left));
            left++;
        }
    }
    printf("\n");
}

/* Simultaneous grid update in place: bit0 = old state, bit1 = new state.
 * Rule here is Conway's: live with 2-3 live neighbours survives, dead with exactly 3 is born. */
static void life_step_inplace(int *g, int m, int n) {
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++) {
            int live = 0;
            for (int di = -1; di <= 1; di++)
                for (int dj = -1; dj <= 1; dj++) {
                    if (!di && !dj) continue;
                    int ii = i + di, jj = j + dj;
                    if (ii < 0 || ii >= m || jj < 0 || jj >= n) continue;  /* clamp */
                    live += AT(g, n, ii, jj) & 1;      /* always the OLD state */
                }
            int old = AT(g, n, i, j) & 1;
            int nw  = old ? (live == 2 || live == 3) : (live == 3);
            AT(g, n, i, j) = old | (nw << 1);          /* pack both states */
        }
    for (int k = 0; k < m * n; k++) g[k] >>= 1;        /* second pass: commit */
}

/* ------------------------------------------------------------------------- */
/* main: tiny demos of every skeleton                                          */
/* ------------------------------------------------------------------------- */

int main(void) {
    puts("== 1. IntSet (open addressing) ==");
    {
        int a[] = {3, 1, 4, 1, 5, 9, 2, 6};
        int dup = first_duplicate(a, 8);
        printf("  first duplicate in [3 1 4 1 5 9 2 6]: %d\n", dup);
        int b[] = {100, 4, 200, 1, 3, 2, -1, 0};
        printf("  longest consecutive run in [100 4 200 1 3 2 -1 0]: %d\n", longest_consecutive(b, 8));
        IntSet s; set_init(&s, 4);
        set_insert(&s, -7); set_insert(&s, INT_MIN); set_insert(&s, 0);
        printf("  negative/extreme keys: contains(-7)=%d contains(INT_MIN)=%d contains(7)=%d\n",
               set_contains(&s, -7), set_contains(&s, INT_MIN), set_contains(&s, 7));
        set_free(&s);
    }

    puts("== 3. Array as its own set ==");
    {
        int a[] = {4, 3, 2, 7, 8, 2, 3, 1};
        demo_sign_marking(a, 8);
        int b[] = {3, 4, -1, 1};
        int c[] = {1, 2, 0};
        int d[] = {7, 8, 9, 11, 12};
        printf("  first missing positive: [3 4 -1 1] -> %d, [1 2 0] -> %d, [7 8 9 11 12] -> %d\n",
               first_missing_positive(b, 4), first_missing_positive(c, 3), first_missing_positive(d, 5));
    }

    puts("== 4. Frequency counting ==");
    {
        printf("  is_anagram(\"listen\",\"silent\") = %d, (\"rat\",\"car\") = %d, (\"ab\",\"abc\") = %d\n",
               is_anagram("listen", "silent"), is_anagram("rat", "car"), is_anagram("ab", "abc"));
        int a[] = {1, 1, 1, 2, 2, 3, -5, -5, -5, -5};
        top_k_frequent(a, 10, 2);
        int m[] = {2, 2, 1, 1, 1, 2, 2};
        printf("  Boyer-Moore majority of [2 2 1 1 1 2 2]: %d\n", majority_element(m, 7));
    }

    puts("== 5. Hash map as memory ==");
    {
        int a[] = {2, 7, 11, 15, 3};
        int i, j;
        if (pair_with_sum(a, 5, 10, &i, &j))
            printf("  pair summing to 10 in [2 7 11 15 3]: indices (%d, %d) -> %d + %d\n", i, j, a[i], a[j]);
        int b[] = {5, 5};
        printf("  pair summing to 10 in [5 5]: %s\n", pair_with_sum(b, 2, 10, &i, &j) ? "found (0,1)" : "none");
        printf("  is_bijection(\"egg\",\"add\") = %d, (\"foo\",\"bar\") = %d, (\"badc\",\"baba\") = %d\n",
               is_bijection("egg", "add"), is_bijection("foo", "bar"), is_bijection("badc", "baba"));
    }

    puts("== 6. Prefix sums ==");
    {
        int a[] = {3, -1, 4, 1, -5, 9};
        long long *P = build_prefix(a, 6);
        printf("  a = [3 -1 4 1 -5 9]\n  P = [");
        for (int i = 0; i <= 6; i++) printf("%lld%s", P[i], i < 6 ? " " : "");
        printf("]   (n+1 entries)\n");
        printf("  sum a[1..3] = P[4]-P[1] = %lld;  sum a[0..5] = %lld\n", range_sum(P, 1, 3), range_sum(P, 0, 5));
        free(P);

        int b[] = {1, 1, 1, -1, 2};
        printf("  subarrays of [1 1 1 -1 2] with sum 2: %lld\n", count_subarrays_with_sum(b, 5, 2));
        int c[] = {1, -1, 3, -3, 4, 0, -4};          /* 0s->-1 balance problems look like this */
        printf("  longest zero-sum subarray of [1 -1 3 -3 4 0 -4]: %d\n", longest_zero_sum_subarray(c, 7));
        printf("  -7 %% 5 in C = %lld, normalised = %lld\n", -7LL % 5LL, mod_norm(-7, 5));
    }

    puts("== 7. qsort + interval sweep ==");
    {
        Interval v[] = {{8, 10}, {1, 3}, {2, 6}, {15, 18}, {6, 7}};
        int k = merge_intervals(v, 5);
        printf("  merged:");
        for (int i = 0; i < k; i++) printf(" [%d,%d]", v[i].start, v[i].end);
        printf("\n");
    }

    puts("== 8. Matrix index arithmetic ==");
    {
        int a[] = {1, 2, 3,
                   4, 5, 6};                       /* 2 x 3 */
        int t[6];
        transpose(a, 2, 3, t);
        print_matrix("2x3 input:", a, 2, 3);
        print_matrix("transpose (3x2):", t, 3, 2);

        int sq[] = {1, 2, 3,
                    4, 5, 6,
                    7, 8, 9};
        print_spiral(sq, 3, 3);
        rotate_cw_inplace(sq, 3);
        print_matrix("3x3 rotated 90 cw:", sq, 3, 3);

        int r[] = {1, 2, 3, 4,
                   5, 6, 7, 8,
                   9, 10, 11, 12};                  /* 3 x 4: non-square spiral */
        print_spiral(r, 3, 4);

        int g[] = {0, 1, 0,
                   0, 0, 1,
                   1, 1, 1,
                   0, 0, 0};                       /* 4 x 3 glider */
        print_matrix("life before:", g, 4, 3);
        life_step_inplace(g, 4, 3);
        print_matrix("life after one step (in place, two-bit encoding):", g, 4, 3);
    }

    return 0;
}
