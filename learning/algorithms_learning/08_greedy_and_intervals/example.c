/*
 * example.c — Chapter 08: Greedy & intervals. Pattern skeletons on tiny neutral inputs.
 *
 * Compile + run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * What is demonstrated (one section per unit of lesson.md):
 *   1. Sort-then-scan greedy: qsort on ints with a safe comparator, greedy budget fill,
 *      and a two-pointer smallest-sufficient matching of two sorted arrays.
 *   2. Exchange-argument greedy: a comparator DERIVED from a swap inequality (Smith's rule
 *      for weighted completion time, cross-multiplied to avoid doubles), and a
 *      concatenation comparator on strings (a before b iff a+b > b+a).
 *   3. Intervals: merge (sort by start), activity selection (sort by end), group counting
 *      (strict '>'), and two-sorted-list intersection with the "advance earliest end" rule.
 *   4. Greedy with a heap: a {key,payload} min-heap driving "admit everything that has
 *      arrived, then pop the best" event simulation with idle-time jumps.
 *   5. Reach/jump greedy: farthest-reach test, layered min-jumps (BFS without a queue),
 *      and the circular net-sum start finder.
 *   6. Where greedy fails: largest-piece-first vs a 1-D DP on a tiny unbounded-knapsack
 *      instance, and a brute-force cross-check harness for a ratio greedy on 0/1 knapsack.
 *
 * These are skeletons of the PATTERNS, not solutions to the problems in problems.md.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------------------- */
/* 1. Sort-then-scan greedy                                                              */
/* ------------------------------------------------------------------------------------- */

/* Three-way comparison without overflow. `return x - y` is WRONG for large magnitudes. */
static int cmp_int_asc(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* How many items fit into `budget` when we always take the cheapest remaining one?
   Sort ascending, walk once, stop at the first that does not fit. O(n log n).
   Exchange argument: skipping a cheaper item for a dearer one can be swapped back without
   losing feasibility or count, so cheapest-first is never worse. */
static int greedy_fill(int *cost, int n, long long budget) {
    qsort(cost, (size_t)n, sizeof cost[0], cmp_int_asc);
    int taken = 0;
    for (int i = 0; i < n && cost[i] <= budget; i++) {
        budget -= cost[i];            /* irrevocable: never revisit index i */
        taken++;
    }
    return taken;
}

/* Two-pointer matching: each demand gets the smallest supply that is >= it.
   Both arrays MUST be sorted ascending; j (supply) always advances, i (demand) only on a match.
   Returns the number of satisfied demands. O(n + m) after the sorts. */
static int match_smallest_sufficient(const int *demand, int n, const int *supply, int m) {
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (supply[j] >= demand[i]) i++;   /* matched: this demand is done */
        j++;                               /* this supply is consumed either way */
    }
    return i;
}

static void demo_sort_then_scan(void) {
    puts("== 1. Sort-then-scan greedy ==");
    int cost[] = {7, 2, 9, 4, 3};
    int n = (int)(sizeof cost / sizeof cost[0]);
    int taken = greedy_fill(cost, n, 12);
    printf("costs sorted:");
    for (int i = 0; i < n; i++) printf(" %d", cost[i]);
    printf("\nbudget 12 -> %d items fit (2+3+4=9, next 7 does not fit)\n", taken);

    int demand[] = {3, 1, 4};           /* what each consumer needs, at least */
    int supply[] = {2, 3, 5, 1};        /* what is on offer */
    qsort(demand, 3, sizeof demand[0], cmp_int_asc);
    qsort(supply, 4, sizeof supply[0], cmp_int_asc);
    printf("two-pointer match: %d of 3 demands satisfied "
           "(demand 1<-1, 3<-3, 4<-5; supply 2 wasted)\n\n",
           match_smallest_sufficient(demand, 3, supply, 4));
}

/* ------------------------------------------------------------------------------------- */
/* 2. Exchange-argument greedy                                                           */
/* ------------------------------------------------------------------------------------- */

/* Jobs (processing time p, weight w) run back to back. Minimise sum_i w_i * C_i, where C_i
   is the completion time of job i.
   Swap adjacent a,b:   a-first cost = w_a p_a + w_b (p_a + p_b)
                        b-first cost = w_b p_b + w_a (p_a + p_b)
   a-first is no worse  <=>  w_b p_a <= w_a p_b  <=>  p_a/w_a <= p_b/w_b   (Smith's rule).
   The comparator is a transcription of that inequality, cross-multiplied so we stay in
   integer arithmetic (all products fit in long long for |p|,|w| < 2^31). */
typedef struct { long long p, w; const char *name; } Job;

static int cmp_job_smith(const void *x, const void *y) {
    const Job *a = x, *b = y;
    long long lhs = a->p * b->w, rhs = b->p * a->w;
    return (lhs > rhs) - (lhs < rhs);
}

static long long weighted_completion(const Job *jobs, int n) {
    long long t = 0, total = 0;
    for (int i = 0; i < n; i++) { t += jobs[i].p; total += jobs[i].w * t; }
    return total;
}

/* Concatenation comparator: a goes before b iff the string a+b is lexicographically GREATER
   than b+a. Both concatenations have equal length, so string order == numeric order.
   Elements are `const char *`, so qsort hands us `const char *const *`. */
static int cmp_concat_desc(const void *x, const void *y) {
    const char *a = *(const char *const *)x, *b = *(const char *const *)y;
    char ab[64], ba[64];
    snprintf(ab, sizeof ab, "%s%s", a, b);
    snprintf(ba, sizeof ba, "%s%s", b, a);
    return strcmp(ba, ab);             /* reversed arguments: larger concatenation first */
}

static void demo_exchange_argument(void) {
    puts("== 2. Exchange-argument greedy ==");
    Job jobs[] = { {3, 1, "A"}, {1, 2, "B"}, {2, 2, "C"}, {4, 4, "D"} };
    int n = (int)(sizeof jobs / sizeof jobs[0]);

    /* A naive "shortest processing time first" order, for contrast. */
    Job naive[4]; memcpy(naive, jobs, sizeof jobs);
    for (int i = 1; i < n; i++)         /* tiny insertion sort by p */
        for (int j = i; j > 0 && naive[j].p < naive[j - 1].p; j--) {
            Job t = naive[j]; naive[j] = naive[j - 1]; naive[j - 1] = t;
        }
    printf("shortest-first order:");
    for (int i = 0; i < n; i++) printf(" %s", naive[i].name);
    printf("  weighted completion = %lld\n", weighted_completion(naive, n));

    qsort(jobs, (size_t)n, sizeof jobs[0], cmp_job_smith);
    printf("Smith's rule (p/w asc):");
    for (int i = 0; i < n; i++) printf(" %s", jobs[i].name);
    printf("  weighted completion = %lld  (optimal)\n", weighted_completion(jobs, n));

    const char *pieces[] = {"5", "52", "9", "34", "3"};
    int m = (int)(sizeof pieces / sizeof pieces[0]);
    qsort(pieces, (size_t)m, sizeof pieces[0], cmp_concat_desc);
    printf("concatenation order: ");
    for (int i = 0; i < m; i++) printf("%s", pieces[i]);
    printf("   (note 5 before 52 because \"552\" > \"525\")\n\n");
}

/* ------------------------------------------------------------------------------------- */
/* 3. Intervals                                                                          */
/* ------------------------------------------------------------------------------------- */

typedef struct { int s, e; } Interval;      /* closed [s, e] unless stated otherwise */

static int cmp_by_start(const void *a, const void *b) {
    const Interval *x = a, *y = b;
    if (x->s != y->s) return (x->s > y->s) - (x->s < y->s);
    return (y->e > x->e) - (y->e < x->e);   /* tie: longer interval first */
}
static int cmp_by_end(const void *a, const void *b) {
    const Interval *x = a, *y = b;
    return (x->e > y->e) - (x->e < y->e);
}

static void print_intervals(const char *label, const Interval *v, int n) {
    printf("%s", label);
    for (int i = 0; i < n; i++) printf(" [%d,%d]", v[i].s, v[i].e);
    putchar('\n');
}

/* Merge overlapping intervals IN PLACE. Returns the merged count m; v[0..m) is the result.
   Safe because the write index m never overtakes the read index i. */
static int merge_intervals(Interval *v, int n) {
    if (n == 0) return 0;
    qsort(v, (size_t)n, sizeof v[0], cmp_by_start);
    int m = 0;
    for (int i = 1; i < n; i++) {
        if (v[i].s <= v[m].e) {                    /* overlap (touching counts): extend */
            if (v[i].e > v[m].e) v[m].e = v[i].e;
        } else {
            v[++m] = v[i];                         /* gap: emit a new merged interval */
        }
    }
    return m + 1;
}

/* Activity selection: maximum number of pairwise non-overlapping intervals.
   Sort by END; keep an interval iff it starts at or after the last kept one's end.
   Exchange argument: the earliest-ending candidate leaves the most room for the rest. */
static int max_non_overlapping(Interval *v, int n) {
    qsort(v, (size_t)n, sizeof v[0], cmp_by_end);
    int kept = 0;
    long long last_end = LLONG_MIN;
    for (int i = 0; i < n; i++)
        if (v[i].s >= last_end) { kept++; last_end = v[i].e; }
    return kept;
}

/* Minimum number of points needed so that every interval contains at least one point.
   Same end order; a new point only when the interval starts STRICTLY after the last point
   (an interval that merely touches the point is still covered). */
static int min_points_to_cover(Interval *v, int n) {
    if (n == 0) return 0;
    qsort(v, (size_t)n, sizeof v[0], cmp_by_end);
    int points = 1;
    int pos = v[0].e;
    for (int i = 1; i < n; i++)
        if (v[i].s > pos) { points++; pos = v[i].e; }
    return points;
}

/* Intersection of two lists that are each sorted and internally disjoint. O(n + m).
   Emit [max(s), min(e)] when non-empty, then advance the interval that ENDS FIRST:
   it can meet nothing further, while the other may still meet the next one. */
static int intersect_sorted(const Interval *a, int n, const Interval *b, int m, Interval *out) {
    int i = 0, j = 0, k = 0;
    while (i < n && j < m) {
        int lo = a[i].s > b[j].s ? a[i].s : b[j].s;
        int hi = a[i].e < b[j].e ? a[i].e : b[j].e;
        if (lo <= hi) out[k++] = (Interval){lo, hi};
        if (a[i].e < b[j].e) i++; else j++;
    }
    return k;
}

static void demo_intervals(void) {
    puts("== 3. Intervals ==");
    Interval v[] = { {1, 3}, {8, 10}, {2, 6}, {15, 18}, {9, 12} };
    int n = (int)(sizeof v / sizeof v[0]);
    print_intervals("input:        ", v, n);

    Interval w[5]; memcpy(w, v, sizeof v);
    int m = merge_intervals(w, n);
    print_intervals("merged:       ", w, m);

    memcpy(w, v, sizeof v);
    printf("max non-overlapping (sort by end): %d\n", max_non_overlapping(w, n));

    /* Why NOT sort by start for selection: */
    Interval trap[] = { {1, 100}, {2, 3}, {4, 5} };
    memcpy(w, trap, sizeof trap);
    printf("trap [1,100][2,3][4,5]: by end keeps %d; sorting by start would keep only 1\n",
           max_non_overlapping(w, 3));

    memcpy(w, v, sizeof v);
    printf("min points to hit all: %d\n", min_points_to_cover(w, n));

    Interval a[] = { {0, 2}, {5, 10}, {13, 23}, {24, 25} };
    Interval b[] = { {1, 5}, {8, 12}, {15, 24}, {25, 26} };
    Interval out[8];
    int k = intersect_sorted(a, 4, b, 4, out);
    print_intervals("A:            ", a, 4);
    print_intervals("B:            ", b, 4);
    print_intervals("intersection: ", out, k);
    putchar('\n');
}

/* ------------------------------------------------------------------------------------- */
/* 4. Greedy with a heap                                                                 */
/* ------------------------------------------------------------------------------------- */

/* Array-backed binary min-heap of {key, payload}. See ../../c_learning/10_data_structures.
   Ties broken by payload so the order is deterministic (Python does this for free with
   tuples; in C you write the lexicographic compare into the sift loops). */
typedef struct { long long key; int payload; } HItem;
typedef struct { HItem *a; int len, cap; } MinHeap;

static int hitem_less(HItem x, HItem y) {
    return x.key < y.key || (x.key == y.key && x.payload < y.payload);
}
static void heap_swap(HItem *x, HItem *y) { HItem t = *x; *x = *y; *y = t; }

static int heap_push(MinHeap *h, HItem it) {
    if (h->len == h->cap) return -1;
    int i = h->len++;
    h->a[i] = it;
    while (i > 0 && hitem_less(h->a[i], h->a[(i - 1) / 2])) {   /* sift up */
        heap_swap(&h->a[i], &h->a[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
    return 0;
}
static HItem heap_pop(MinHeap *h) {              /* caller guarantees len > 0 */
    HItem top = h->a[0];
    h->a[0] = h->a[--h->len];
    for (int i = 0;;) {                          /* sift down toward the smaller child */
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->len && hitem_less(h->a[l], h->a[m])) m = l;
        if (r < h->len && hitem_less(h->a[r], h->a[m])) m = r;
        if (m == i) break;
        heap_swap(&h->a[i], &h->a[m]);
        i = m;
    }
    return top;
}

typedef struct { long long arrival, duration; } Task;

static int cmp_task_arrival(const void *x, const void *y) {
    const Task *a = x, *b = y;
    return (a->arrival > b->arrival) - (a->arrival < b->arrival);
}

/* "Admit everything that has arrived, then run the shortest" on one machine.
   Two phases per step; if nothing is admissible, jump time to the next arrival. */
static void demo_heap_greedy(void) {
    puts("== 4. Greedy with a heap ==");
    Task tasks[] = { {0, 5}, {1, 2}, {2, 1}, {9, 3}, {10, 1} };   /* gap 8..9 forces an idle jump */
    int n = (int)(sizeof tasks / sizeof tasks[0]);

    /* Sort an index array so the payload stays the original task id (argsort). */
    int order[5];
    for (int i = 0; i < n; i++) order[i] = i;
    Task sorted[5]; memcpy(sorted, tasks, sizeof tasks);
    qsort(sorted, (size_t)n, sizeof sorted[0], cmp_task_arrival);
    /* map back to ids (inputs here are distinct, so a linear search is fine for a demo) */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (tasks[j].arrival == sorted[i].arrival && tasks[j].duration == sorted[i].duration)
                order[i] = j;

    HItem storage[5];
    MinHeap heap = { storage, 0, n };
    long long t = 0;
    int next = 0, done = 0;
    printf("tasks (arrival,duration):");
    for (int i = 0; i < n; i++) printf(" T%d=(%lld,%lld)", i, tasks[i].arrival, tasks[i].duration);
    putchar('\n');
    while (done < n) {
        if (heap.len == 0 && sorted[next].arrival > t) {
            t = sorted[next].arrival;                       /* idle: jump to next arrival */
            printf("  t=%-3lld idle -> jump\n", t);
        }
        while (next < n && sorted[next].arrival <= t) {     /* ADMIT phase */
            heap_push(&heap, (HItem){ sorted[next].duration, order[next] });
            next++;
        }
        HItem run = heap_pop(&heap);                        /* CHOOSE phase: shortest */
        printf("  t=%-3lld run T%d (dur %lld) -> ends %lld   heap left %d\n",
               t, run.payload, run.key, t + run.key, heap.len);
        t += run.key;
        done++;
    }
    putchar('\n');
}

/* ------------------------------------------------------------------------------------- */
/* 5. Reach/jump greedy                                                                  */
/* ------------------------------------------------------------------------------------- */

/* Can index n-1 be reached from 0 when a[i] is the max forward step from i?
   One variable: farthest reach using elements 0..i. Fail the moment i > farthest. */
static int can_reach_end(const int *a, int n) {
    long long farthest = 0;
    for (int i = 0; i < n; i++) {
        if (i > farthest) return 0;
        if (i + (long long)a[i] > farthest) farthest = i + (long long)a[i];
    }
    return 1;
}

/* Minimum number of steps to reach n-1, or -1. Layers: current_end is the boundary of the
   current layer; when i reaches it we must jump and the next layer's boundary is `farthest`.
   This is BFS on an implicit graph with no queue; `jumps` is the depth. */
static int min_jumps(const int *a, int n) {
    int jumps = 0;
    long long current_end = 0, farthest = 0;
    for (int i = 0; i < n - 1; i++) {                 /* never jump FROM the last index */
        if (i + (long long)a[i] > farthest) farthest = i + (long long)a[i];
        if (i == current_end) {
            if (farthest <= i) return -1;             /* no progress: stuck */
            jumps++;
            current_end = farthest;
            if (current_end >= n - 1) break;
        }
    }
    return jumps;
}

/* Circular route with per-station net delta. If the total is negative there is no start.
   Otherwise one pass: when the running tank goes negative at i, no start in [start..i]
   can work, so the candidate becomes i+1. The surviving candidate is guaranteed. */
static int circular_start(const int *delta, int n) {
    long long total = 0, tank = 0;
    int start = 0;
    for (int i = 0; i < n; i++) {
        total += delta[i];
        tank += delta[i];
        if (tank < 0) { start = i + 1; tank = 0; }
    }
    return total < 0 ? -1 : start;
}

static void demo_reach(void) {
    puts("== 5. Reach/jump greedy ==");
    int a[] = {1, 3, 0, 0, 4, 1, 0};
    int n = (int)(sizeof a / sizeof a[0]);
    printf("a = [1,3,0,0,4,1,0]: reachable=%d  min_jumps=%d  (0->1->4->6)\n",
           can_reach_end(a, n), min_jumps(a, n));
    int b[] = {2, 0, 0, 5};
    printf("b = [2,0,0,5]:       reachable=%d  min_jumps=%d  (stuck at index 2)\n",
           can_reach_end(b, 4), min_jumps(b, 4));
    int d[] = {-2, 3, -1, -3, 4};
    printf("circular deltas [-2,3,-1,-3,4]: start = %d (total = +1)\n", circular_start(d, 5));
    int e[] = {-2, 1, -1};
    printf("circular deltas [-2,1,-1]:      start = %d (total < 0)\n\n", circular_start(e, 3));
}

/* ------------------------------------------------------------------------------------- */
/* 6. Where greedy fails: side-by-side with DP, and a brute-force cross-check harness     */
/* ------------------------------------------------------------------------------------- */

/* Fewest pieces from `sizes` (unlimited copies) summing exactly to `total`.
   Greedy: largest piece that fits, repeat. DP: dp[x] = 1 + min over sizes of dp[x - size]. */
static int pieces_greedy(const int *sizes, int k, int total) {   /* sizes sorted ascending */
    int count = 0;
    for (int i = k - 1; i >= 0 && total > 0; i--) {
        count += total / sizes[i];
        total %= sizes[i];
    }
    return total == 0 ? count : -1;
}
static int pieces_dp(const int *sizes, int k, int total) {
    const int INF = INT_MAX / 2;                    /* /2 so INF + 1 cannot overflow */
    int *dp = malloc(((size_t)total + 1) * sizeof *dp);
    if (!dp) return -1;
    dp[0] = 0;
    for (int x = 1; x <= total; x++) {
        dp[x] = INF;
        for (int i = 0; i < k; i++)                 /* try EVERY first piece, keep the best */
            if (sizes[i] <= x && dp[x - sizes[i]] + 1 < dp[x]) dp[x] = dp[x - sizes[i]] + 1;
    }
    int ans = dp[total] >= INF ? -1 : dp[total];
    free(dp);
    return ans;
}

/* 0/1 knapsack (each item at most once): the "best value/weight ratio first" greedy is
   optimal for the FRACTIONAL knapsack but not for 0/1. Cross-check against brute force. */
typedef struct { int w, v; } Item;

static int cmp_item_ratio_desc(const void *x, const void *y) {
    const Item *a = x, *b = y;
    long long lhs = (long long)a->v * b->w, rhs = (long long)b->v * a->w;   /* a.v/a.w vs b.v/b.w */
    return (lhs < rhs) - (lhs > rhs);               /* descending ratio */
}
static int knapsack_greedy(Item *items, int n, int cap) {
    qsort(items, (size_t)n, sizeof items[0], cmp_item_ratio_desc);
    int value = 0;
    for (int i = 0; i < n; i++)
        if (items[i].w <= cap) { cap -= items[i].w; value += items[i].v; }
    return value;
}
static int knapsack_brute(const Item *items, int n, int cap) {  /* all 2^n subsets, n <= 12 */
    int best = 0;
    for (unsigned mask = 0; mask < (1u << n); mask++) {
        int w = 0, v = 0;
        for (int i = 0; i < n; i++)
            if (mask & (1u << i)) { w += items[i].w; v += items[i].v; }
        if (w <= cap && v > best) best = v;
    }
    return best;
}

/* Deterministic tiny PRNG so the demo output is reproducible (no <time.h>). */
static unsigned rng_state = 12345u;
static unsigned rng_next(void) { rng_state = rng_state * 1103515245u + 12345u; return rng_state >> 16; }

static void demo_greedy_vs_dp(void) {
    puts("== 6. Where greedy fails ==");
    int sizes[] = {1, 3, 4};
    printf("pieces {1,3,4} for total 6:  greedy=%d (4+1+1)   dp=%d (3+3)\n",
           pieces_greedy(sizes, 3, 6), pieces_dp(sizes, 3, 6));
    int euro[] = {1, 2, 5, 10, 20, 50};
    printf("pieces euro for total 68:    greedy=%d           dp=%d   (canonical set: agree)\n",
           pieces_greedy(euro, 6, 68), pieces_dp(euro, 6, 68));

    /* Brute-force harness: the tool you reach for BEFORE trusting a greedy. */
    int trials = 2000, mismatches = 0, shown = 0;
    for (int t = 0; t < trials; t++) {
        int n = 3 + (int)(rng_next() % 4);          /* 3..6 items */
        Item items[6], copy[6];
        for (int i = 0; i < n; i++) { items[i].w = 1 + (int)(rng_next() % 9); items[i].v = 1 + (int)(rng_next() % 20); }
        int cap = 5 + (int)(rng_next() % 15);
        memcpy(copy, items, sizeof items);
        int g = knapsack_greedy(copy, n, cap), b = knapsack_brute(items, n, cap);
        if (g != b) {
            mismatches++;
            if (shown < 2) {                        /* print the first two counterexamples */
                shown++;
                printf("  counterexample: cap=%d items(w,v):", cap);
                for (int i = 0; i < n; i++) printf(" (%d,%d)", items[i].w, items[i].v);
                printf("  greedy=%d brute=%d\n", g, b);
            }
        }
    }
    printf("0/1 knapsack, ratio greedy vs brute force: %d/%d random instances disagree "
           "-> greedy is NOT optimal here\n", mismatches, trials);
}

/* ------------------------------------------------------------------------------------- */

int main(void) {
    demo_sort_then_scan();
    demo_exchange_argument();
    demo_intervals();
    demo_heap_greedy();
    demo_reach();
    demo_greedy_vs_dp();
    return 0;
}
