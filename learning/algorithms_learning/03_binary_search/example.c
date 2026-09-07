/*
 * example.c — Chapter 03: Binary search pattern skeletons.
 *
 * Compile and run:
 *     cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * These are the PATTERN SKELETONS from lesson.md on neutral demo inputs, each
 * checked with assert() against a brute-force answer. They are not solutions to
 * the LeetCode problems in problems.md; they are the pieces you assemble those from.
 *
 *   1. Closed-interval exact search                    [lo, hi],  while (lo <= hi)
 *   2. lower_bound / upper_bound, count, last <= t     [lo, hi),  while (lo <  hi)
 *   3. Boundary search with a non-trivial predicate    (peak, weighted pick)
 *   4. Search on a computed value in 64-bit            (integer sqrt)
 *   5. Rotated array: minimum, target, duplicates
 *   6. Binary search on the ANSWER                     (smallest feasible / largest feasible)
 *   7. 2D matrix: staircase walk, count <= x, k-th smallest by value
 *   8. Threshold + BFS reachability on a grid          (explicit queue, no recursion)
 *
 * Every mid is written lo + (hi - lo) / 2. Closed-interval code uses signed
 * indices (long) so that hi = mid - 1 can go to -1; half-open code uses size_t
 * because it never subtracts.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* 1. Closed-interval exact-match search                                    */
/* ------------------------------------------------------------------------ */

/* Index of t in ascending a[0..n), or -1. Invariant: if t exists it is in [lo, hi]. */
static long search_closed(const int *a, long n, int t)
{
    long lo = 0, hi = n - 1;                 /* both ends are candidates            */
    while (lo <= hi) {                       /* closed form: empty when lo > hi     */
        long mid = lo + (hi - lo) / 2;       /* never (lo + hi) / 2                 */
        if (a[mid] == t) return mid;
        if (a[mid] < t) lo = mid + 1;        /* a[0..mid] are all < t: skip past    */
        else            hi = mid - 1;        /* a[mid..] are all > t: skip past     */
    }
    return -1;
}

/* ------------------------------------------------------------------------ */
/* 2. Half-open boundary searches                                           */
/* ------------------------------------------------------------------------ */

/* First index with a[i] >= t, or n. Invariant: a[0..lo) < t, a[hi..n) >= t. */
static size_t lower_bound(const int *a, size_t n, int t)
{
    size_t lo = 0, hi = n;                   /* hi = n: "not found" is a normal result */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;     /* mid < hi always, so hi = mid shrinks   */
        if (a[mid] >= t) hi = mid;           /* mid may BE the boundary: keep it       */
        else             lo = mid + 1;       /* mid is definitely left of it           */
    }
    return lo;
}

/* First index with a[i] > t, or n. Same skeleton, one comparison changed. */
static size_t upper_bound(const int *a, size_t n, int t)
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] > t) hi = mid;
        else            lo = mid + 1;
    }
    return lo;
}

/* Brute-force reference for the two above. */
static size_t lower_bound_slow(const int *a, size_t n, int t)
{
    size_t i = 0;
    while (i < n && a[i] < t) i++;
    return i;
}
static size_t upper_bound_slow(const int *a, size_t n, int t)
{
    size_t i = 0;
    while (i < n && a[i] <= t) i++;
    return i;
}

/* ------------------------------------------------------------------------ */
/* 3. Boundary search with a non-trivial predicate                          */
/* ------------------------------------------------------------------------ */

/* Index of some peak (a[i] > neighbours, edges count as -inf). n >= 1.
   Predicate P(mid) = "already on a descending slope" = a[mid] > a[mid + 1].
   Range [0, n - 1) guarantees a[mid + 1] is in bounds. */
static size_t find_peak(const int *a, size_t n)
{
    size_t lo = 0, hi = n - 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] > a[mid + 1]) hi = mid;   /* descending: a peak is at mid or left   */
        else                     lo = mid + 1; /* ascending: a peak is strictly right  */
    }
    return lo;
}

/* Weighted pick: prefix[i] = w[0] + ... + w[i]. For r in [1, total], the chosen
   index is the first i with prefix[i] >= r (lower_bound on a strictly increasing array). */
static size_t weighted_pick(const int *prefix, size_t n, int r)
{
    return lower_bound(prefix, n, r);
}

/* ------------------------------------------------------------------------ */
/* 4. Search on a computed value, 64-bit arithmetic                         */
/* ------------------------------------------------------------------------ */

/* Largest r with r*r <= x, x >= 0. Closed interval on VALUES; hi is the answer at exit. */
static long long isqrt_ll(long long x)
{
    long long lo = 0, hi = x < 2 ? x : x / 2 + 1;   /* sqrt(x) <= x/2 + 1 for all x >= 0 */
    while (lo <= hi) {
        long long mid = lo + (hi - lo) / 2;
        if (mid <= x / mid || mid == 0) lo = mid + 1;  /* mid*mid <= x without overflow */
        else                            hi = mid - 1;
    }
    return hi;                               /* last value that satisfied the predicate */
}

/* ------------------------------------------------------------------------ */
/* 5. Rotated sorted array                                                  */
/* ------------------------------------------------------------------------ */

/* Index of the minimum (rotation offset) in a rotated ascending array with distinct
   values. Compare against a[hi], not a[lo]: it gives a clean two-way predicate. */
static long rotated_min_index(const int *a, long n)
{
    long lo = 0, hi = n - 1;                 /* half-open on [0, n-1): a[hi] always valid */
    while (lo < hi) {
        long mid = lo + (hi - lo) / 2;
        if (a[mid] > a[hi]) lo = mid + 1;    /* mid sits on the high plateau           */
        else                hi = mid;        /* mid may be the minimum: keep it         */
    }
    return lo;
}

/* Index of t in a rotated ascending array with distinct values, or -1.
   Two-stage predicate: which half is sorted, then is t inside that half. */
static long rotated_search(const int *a, long n, int t)
{
    long lo = 0, hi = n - 1;
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        if (a[mid] == t) return mid;
        if (a[lo] <= a[mid]) {                        /* left half [lo, mid] is sorted  */
            if (a[lo] <= t && t < a[mid]) hi = mid - 1;
            else                          lo = mid + 1;
        } else {                                      /* right half [mid, hi] is sorted */
            if (a[mid] < t && t <= a[hi]) lo = mid + 1;
            else                          hi = mid - 1;
        }
    }
    return -1;
}

/* Same, allowing duplicates. When a[lo] == a[mid] == a[hi] nothing can be inferred:
   shrink from both ends (O(n) worst case, unavoidable). Returns 1 if present. */
static int rotated_contains_dup(const int *a, long n, int t)
{
    long lo = 0, hi = n - 1;
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        if (a[mid] == t) return 1;
        if (a[lo] == a[mid] && a[mid] == a[hi]) { lo++; hi--; continue; }
        if (a[lo] <= a[mid]) {
            if (a[lo] <= t && t < a[mid]) hi = mid - 1;
            else                          lo = mid + 1;
        } else {
            if (a[mid] < t && t <= a[hi]) lo = mid + 1;
            else                          hi = mid - 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* 6. Binary search on the answer                                           */
/* ------------------------------------------------------------------------ */

/* feasible(cap): can a[0..n) (kept in order) be cut into <= parts contiguous chunks,
   each with sum <= cap?  Greedy: start a new chunk only when forced. O(n). */
static int fits_in_parts(const int *a, size_t n, long long cap, int parts)
{
    int used = 1;
    long long cur = 0;
    for (size_t i = 0; i < n; i++) {
        if (a[i] > cap) return 0;            /* a single item exceeds the cap        */
        if (cur + a[i] > cap) { used++; cur = 0; }
        cur += a[i];
    }
    return used <= parts;
}

/* MINIMISE THE MAXIMUM: smallest cap such that fits_in_parts is true.
   Predicate is false...false true...true; want the first true. Value range [max, sum]. */
static long long min_max_chunk_sum(const int *a, size_t n, int parts)
{
    long long lo = 0, hi = 0;
    for (size_t i = 0; i < n; i++) { if (a[i] > lo) lo = a[i]; hi += a[i]; }
    while (lo < hi) {                        /* half-open on values; hi (= sum) is feasible */
        long long mid = lo + (hi - lo) / 2;
        if (fits_in_parts(a, n, mid, parts)) hi = mid;
        else                                 lo = mid + 1;
    }
    return lo;
}

/* Brute force for the check above: try every cap from max to sum. */
static long long min_max_chunk_sum_slow(const int *a, size_t n, int parts)
{
    long long mx = 0, sum = 0;
    for (size_t i = 0; i < n; i++) { if (a[i] > mx) mx = a[i]; sum += a[i]; }
    for (long long cap = mx; cap <= sum; cap++)
        if (fits_in_parts(a, n, cap, parts)) return cap;
    return sum;
}

/* feasible(gap): with positions sorted ascending, can we place `balls` of them so that
   every adjacent pair is >= gap apart? Greedy left-to-right placement. O(n). */
static int can_place(const int *pos, size_t n, int gap, int balls)
{
    int placed = 1, last = pos[0];
    for (size_t i = 1; i < n && placed < balls; i++)
        if (pos[i] - last >= gap) { placed++; last = pos[i]; }
    return placed >= balls;
}

/* MAXIMISE THE MINIMUM: largest gap such that can_place is true.
   Predicate is true...true false...false; want the LAST true. Round mid UP so that
   lo = mid still makes progress. Value range [1, pos[n-1] - pos[0]]. */
static int max_min_gap(const int *pos, size_t n, int balls)
{
    int lo = 1, hi = pos[n - 1] - pos[0];
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;    /* upper mid: mid > lo always           */
        if (can_place(pos, n, mid, balls)) lo = mid;      /* mid works: answer >= mid */
        else                               hi = mid - 1;  /* mid fails: answer < mid  */
    }
    return lo;
}

/* Integer comparator for qsort — (a > b) - (a < b), never a - b (overflow). */
static int cmp_int(const void *pa, const void *pb)
{
    int a = *(const int *)pa, b = *(const int *)pb;
    return (a > b) - (a < b);
}

/* ------------------------------------------------------------------------ */
/* 7. 2D matrix, row-major m x n, rows and columns each ascending           */
/* ------------------------------------------------------------------------ */

#define AT(a, n, r, c) ((a)[(size_t)(r) * (size_t)(n) + (size_t)(c)])

/* Staircase from the top-right: too big -> left, too small -> down. O(m + n). */
static int staircase_find(const int *a, int m, int n, int t)
{
    int r = 0, c = n - 1;
    while (r < m && c >= 0) {
        int v = AT(a, n, r, c);
        if (v == t) return 1;
        if (v > t) c--;                      /* whole column below is also > t        */
        else       r++;                      /* whole row to the left is also < t     */
    }
    return 0;
}

/* Number of entries <= x, staircase from the bottom-left. O(m + n). */
static long count_le(const int *a, int m, int n, int x)
{
    int r = m - 1, c = 0;
    long cnt = 0;
    while (r >= 0 && c < n) {
        if (AT(a, n, r, c) <= x) { cnt += r + 1; c++; }   /* rows 0..r of column c   */
        else                       r--;
    }
    return cnt;
}

/* k-th smallest (1-based) by binary search on the VALUE with count_le as the predicate.
   Range [a[0][0], a[m-1][n-1]]; the first x with count_le(x) >= k is always a real entry. */
static int kth_smallest(const int *a, int m, int n, long k)
{
    long long lo = AT(a, n, 0, 0), hi = AT(a, n, m - 1, n - 1);
    while (lo < hi) {
        long long mid = lo + (hi - lo) / 2;
        if (count_le(a, m, n, (int)mid) >= k) hi = mid;
        else                                  lo = mid + 1;
    }
    return (int)lo;
}

/* ------------------------------------------------------------------------ */
/* 8. Threshold + BFS reachability on a grid                                */
/* ------------------------------------------------------------------------ */

/* Can (0,0) reach (m-1,n-1) stepping only on cells with h <= t? 4-neighbour BFS with an
   explicit queue; each cell is enqueued at most once so a plain array suffices. */
static int reachable_under(const int *h, int m, int n, int t,
                           unsigned char *seen, int *queue)
{
    size_t cells = (size_t)m * (size_t)n;
    if (h[0] > t || h[cells - 1] > t) return 0;
    memset(seen, 0, cells);
    static const int dr[4] = {1, -1, 0, 0}, dc[4] = {0, 0, 1, -1};
    int head = 0, tail = 0;
    queue[tail++] = 0;
    seen[0] = 1;
    while (head < tail) {
        int cur = queue[head++];
        if (cur == m * n - 1) return 1;
        int r = cur / n, c = cur % n;
        for (int k = 0; k < 4; k++) {
            int nr = r + dr[k], nc = c + dc[k];
            if (nr < 0 || nr >= m || nc < 0 || nc >= n) continue;
            int id = nr * n + nc;
            if (seen[id] || h[id] > t) continue;
            seen[id] = 1;
            queue[tail++] = id;
        }
    }
    return 0;
}

/* Smallest t such that reachable_under(t) is true. Buffers allocated once, reused per check. */
static int min_threshold_to_cross(const int *h, int m, int n)
{
    size_t cells = (size_t)m * (size_t)n;
    unsigned char *seen = malloc(cells);
    int *queue = malloc(cells * sizeof *queue);
    if (!seen || !queue) { free(seen); free(queue); return -1; }

    int lo = 0, hi = 0;
    for (size_t i = 0; i < cells; i++) if (h[i] > hi) hi = h[i];   /* hi = max height is feasible */
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (reachable_under(h, m, n, mid, seen, queue)) hi = mid;
        else                                             lo = mid + 1;
    }
    free(seen);
    free(queue);
    return lo;
}

/* ------------------------------------------------------------------------ */
/* Demo driver                                                              */
/* ------------------------------------------------------------------------ */

static void print_arr(const char *label, const int *a, size_t n)
{
    printf("%s[", label);
    for (size_t i = 0; i < n; i++) printf("%d%s", a[i], i + 1 < n ? ", " : "");
    printf("]\n");
}

int main(void)
{
    /* 1. closed-interval exact search ------------------------------------ */
    puts("== 1. closed-interval exact search ==");
    int s[] = {1, 3, 4, 7, 9, 12, 15};
    long sn = (long)(sizeof s / sizeof s[0]);
    print_arr("a = ", s, (size_t)sn);
    for (int t = 0; t <= 16; t++) {
        long got = search_closed(s, sn, t);
        long want = -1;
        for (long i = 0; i < sn; i++) if (s[i] == t) want = i;
        assert(got == want);
    }
    printf("search 9 -> %ld, search 8 -> %ld, on empty -> %ld\n",
           search_closed(s, sn, 9), search_closed(s, sn, 8), search_closed(s, 0, 9));

    /* 2. lower_bound / upper_bound --------------------------------------- */
    puts("\n== 2. lower_bound / upper_bound (half-open) ==");
    int d[] = {2, 4, 4, 4, 7, 9};
    size_t dn = sizeof d / sizeof d[0];
    print_arr("a = ", d, dn);
    for (int t = 0; t <= 10; t++) {
        assert(lower_bound(d, dn, t) == lower_bound_slow(d, dn, t));
        assert(upper_bound(d, dn, t) == upper_bound_slow(d, dn, t));
    }
    assert(lower_bound(d, 0, 4) == 0 && upper_bound(d, 0, 4) == 0);   /* empty array */
    size_t lb = lower_bound(d, dn, 4), ub = upper_bound(d, dn, 4);
    printf("lower_bound(4) = %zu  upper_bound(4) = %zu  count(4) = %zu\n", lb, ub, ub - lb);
    printf("lower_bound(5) = %zu (insertion point)  lower_bound(10) = %zu (== n: absent)\n",
           lower_bound(d, dn, 5), lower_bound(d, dn, 10));
    size_t ub7 = upper_bound(d, dn, 7);
    printf("last index with a[i] <= 7 = %zu (upper_bound(7) - 1)\n", ub7 - 1);

    /* 3. non-trivial predicates ------------------------------------------ */
    puts("\n== 3. boundary search with a non-trivial predicate ==");
    int pk[] = {1, 3, 8, 12, 9, 4, 2};
    size_t pkn = sizeof pk / sizeof pk[0];
    size_t p = find_peak(pk, pkn);
    print_arr("a = ", pk, pkn);
    printf("peak at index %zu (value %d)\n", p, pk[p]);
    assert(p == 3);
    int single[] = {42};
    assert(find_peak(single, 1) == 0);
    int asc[] = {1, 2, 3, 4};
    assert(find_peak(asc, 4) == 3);          /* edge counts as -inf: last index is a peak */

    int w[] = {1, 3, 2, 4};                  /* weights */
    int prefix[4];
    int total = 0;
    for (size_t i = 0; i < 4; i++) { total += w[i]; prefix[i] = total; }
    print_arr("weights = ", w, 4);
    print_arr("prefix  = ", prefix, 4);
    int hist[4] = {0};
    for (int r = 1; r <= total; r++) hist[weighted_pick(prefix, 4, r)]++;
    print_arr("draws r=1..total land on index (histogram) = ", hist, 4);
    for (size_t i = 0; i < 4; i++) assert(hist[i] == w[i]);   /* each index hit w[i] times */

    /* 4. computed value in 64-bit ---------------------------------------- */
    puts("\n== 4. integer sqrt by binary search on a computed value ==");
    long long sq_tests[] = {0, 1, 2, 3, 4, 15, 16, 17, 99, 100, 2147395599LL, 2147483647LL,
                            1000000000000000000LL};
    for (size_t i = 0; i < sizeof sq_tests / sizeof sq_tests[0]; i++) {
        long long x = sq_tests[i], r = isqrt_ll(x);
        assert(r >= 0 && r <= x / (r ? r : 1));        /* r*r <= x, no overflow      */
        assert((r + 1) > x / (r + 1) || (r + 1) * (r + 1) > x);   /* (r+1)^2 > x    */
    }
    printf("isqrt(2147395599) = %lld  isqrt(2147483647) = %lld  isqrt(10^18) = %lld\n",
           isqrt_ll(2147395599LL), isqrt_ll(2147483647LL), isqrt_ll(1000000000000000000LL));

    /* 5. rotated arrays --------------------------------------------------- */
    puts("\n== 5. rotated sorted array ==");
    int base[] = {0, 1, 2, 4, 5, 6, 7};
    long bn = 7;
    for (long rot = 0; rot < bn; rot++) {
        int rr[7];
        for (long i = 0; i < bn; i++) rr[i] = base[(i + rot) % bn];
        long mi = rotated_min_index(rr, bn);
        assert(rr[mi] == 0);
        assert(mi == (bn - rot) % bn);
        for (int t = -1; t <= 8; t++) {
            long got = rotated_search(rr, bn, t);
            long want = -1;
            for (long i = 0; i < bn; i++) if (rr[i] == t) want = i;
            assert(got == want);
        }
        if (rot == 4) {
            print_arr("a = ", rr, 7);
            printf("min index = %ld, search 1 -> %ld, search 3 -> %ld\n",
                   mi, rotated_search(rr, bn, 1), rotated_search(rr, bn, 3));
        }
    }
    int dup1[] = {1, 0, 1, 1, 1}, dup2[] = {1, 1, 1, 0, 1}, dup3[] = {1, 1, 1, 1, 1};
    assert(rotated_contains_dup(dup1, 5, 0) == 1);
    assert(rotated_contains_dup(dup2, 5, 0) == 1);
    assert(rotated_contains_dup(dup3, 5, 0) == 0);
    assert(rotated_contains_dup(dup3, 5, 1) == 1);
    puts("duplicate collapse (lo++, hi--) verified on {1,0,1,1,1}, {1,1,1,0,1}, {1,1,1,1,1}");

    /* 6. binary search on the answer -------------------------------------- */
    puts("\n== 6. binary search on the answer ==");
    int items[] = {7, 2, 5, 10, 8};
    size_t in = sizeof items / sizeof items[0];
    print_arr("items = ", items, in);
    for (int parts = 1; parts <= 5; parts++) {
        long long got = min_max_chunk_sum(items, in, parts);
        assert(got == min_max_chunk_sum_slow(items, in, parts));
        printf("  minimise-the-maximum: %d part(s) -> smallest feasible cap = %lld\n", parts, got);
    }

    int pos[] = {5, 1, 4, 2, 8, 12};
    size_t pn = sizeof pos / sizeof pos[0];
    qsort(pos, pn, sizeof pos[0], cmp_int);  /* sorting is a prerequisite for the greedy */
    print_arr("positions (sorted) = ", pos, pn);
    for (int balls = 2; balls <= 4; balls++) {
        int got = max_min_gap(pos, pn, balls);
        /* brute force: largest gap g for which the greedy can place `balls` */
        int want = 0;
        for (int g = 1; g <= pos[pn - 1] - pos[0]; g++) if (can_place(pos, pn, g, balls)) want = g;
        assert(got == want);
        printf("  maximise-the-minimum: %d balls -> largest feasible gap = %d\n", balls, got);
    }

    /* 7. 2D matrix -------------------------------------------------------- */
    puts("\n== 7. 2D matrix (rows and columns ascending) ==");
    enum { M = 4, N = 4 };
    int mat[M * N] = { 1,  4,  7, 11,
                       2,  5,  8, 12,
                       3,  6,  9, 16,
                      10, 13, 14, 17 };
    for (int r = 0; r < M; r++) {
        printf("  ");
        for (int c = 0; c < N; c++) printf("%3d", AT(mat, N, r, c));
        printf("\n");
    }
    for (int t = 0; t <= 18; t++) {
        int want = 0;
        for (int i = 0; i < M * N; i++) if (mat[i] == t) want = 1;
        assert(staircase_find(mat, M, N, t) == want);
        long wc = 0;
        for (int i = 0; i < M * N; i++) if (mat[i] <= t) wc++;
        assert(count_le(mat, M, N, t) == wc);
    }
    int sorted_copy[M * N];
    memcpy(sorted_copy, mat, sizeof mat);
    qsort(sorted_copy, M * N, sizeof sorted_copy[0], cmp_int);
    for (long k = 1; k <= M * N; k++) assert(kth_smallest(mat, M, N, k) == sorted_copy[k - 1]);
    printf("staircase_find(9) = %d, count_le(8) = %ld, kth_smallest(8) = %d\n",
           staircase_find(mat, M, N, 9), count_le(mat, M, N, 8), kth_smallest(mat, M, N, 8));

    /* 8. threshold + BFS --------------------------------------------------- */
    puts("\n== 8. threshold + BFS feasibility on a grid ==");
    int grid[9] = { 0, 2, 6,
                    5, 3, 7,
                    8, 4, 1 };
    for (int r = 0; r < 3; r++) printf("  %d %d %d\n", grid[r * 3], grid[r * 3 + 1], grid[r * 3 + 2]);
    int thr = min_threshold_to_cross(grid, 3, 3);
    printf("smallest level t with a (0,0)->(2,2) path over cells <= t: %d\n", thr);
    assert(thr == 4);
    {
        unsigned char seen[9];
        int q[9];
        assert(reachable_under(grid, 3, 3, 3, seen, q) == 0);
        assert(reachable_under(grid, 3, 3, 4, seen, q) == 1);
    }
    int wall[9] = { 0, 9, 9,
                    9, 9, 9,
                    9, 9, 0 };
    assert(min_threshold_to_cross(wall, 3, 3) == 9);   /* must flood the wall itself */

    puts("\nall assertions passed");
    return 0;
}
