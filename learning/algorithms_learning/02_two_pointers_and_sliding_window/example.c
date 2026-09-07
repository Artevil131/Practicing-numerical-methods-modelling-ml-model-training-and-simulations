/*
 * example.c — Chapter 02: Two pointers & sliding window
 *
 * Pattern skeletons on tiny, neutral inputs (NOT solutions to the listed
 * LeetCode problems). Each demo prints its trace so you can follow the
 * pointer / window state step by step and compare with the lesson.
 *
 *   1. Opposite ends        — pair with a given sum in a sorted array
 *   2. Fast & slow          — cycle detection + cycle start, on a linked list
 *                             and on a function graph (array of indices)
 *   3. Fixed window         — running sum, and window max via monotonic deque
 *   4. Variable window      — longest subarray with sum <= bound (positive ints)
 *                             and shortest subarray with sum >= bound
 *   5. Window + histogram   — longest substring with at most k distinct bytes,
 *                             and the atMost(k) - atMost(k-1) counting trick
 *   6. Partition in place   — write/read stable partition, Dutch national flag,
 *                             quickselect built on the same partition
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static void print_arr(const char *label, const int *a, int n) {
    printf("%s[", label);
    for (int i = 0; i < n; i++) printf("%d%s", a[i], i + 1 < n ? " " : "");
    printf("]\n");
}

static void swap_int(int *x, int *y) { int t = *x; *x = *y; *y = t; }

/* ================================================================== */
/* 1. OPPOSITE ENDS                                                    */
/* ================================================================== */

/*
 * Precondition: a[] sorted ascending.
 * Invariant: no valid pair has both indices in the discarded regions
 *            [0, left) or (right, n-1].
 * Each step discards a whole set of pairs at once, hence O(n).
 */
static int pair_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) {
    int left = 0, right = n - 1;                      /* int, not size_t: right is decremented */
    while (left < right) {
        long long s = (long long)a[left] + a[right];  /* widen before adding: no int overflow */
        printf("  l=%d r=%d  %d+%d=%lld ", left, right, a[left], a[right], s);
        if (s == target) { printf("== %d  found\n", target); *i_out = left; *j_out = right; return 1; }
        if (s < target)  { printf("<  %d  left++\n", target);  left++;  }   /* only left can raise it */
        else             { printf(">  %d  right--\n", target); right--; }   /* only right can lower it */
    }
    return 0;
}

static void demo_opposite_ends(void) {
    puts("== 1. Opposite ends: pair with sum 10 in a sorted array ==");
    int a[] = {1, 3, 4, 6, 8, 11};
    int n = (int)(sizeof a / sizeof a[0]);
    print_arr("  a = ", a, n);
    int i, j;
    if (pair_sum_sorted(a, n, 10, &i, &j)) printf("  -> indices (%d, %d)\n", i, j);
    else puts("  -> no pair");

    /* Same skeleton used for an in-place reverse: swap ends, move inward. */
    int r[] = {1, 2, 3, 4, 5};
    int rn = (int)(sizeof r / sizeof r[0]);
    for (int l = 0, h = rn - 1; l < h; l++, h--) swap_int(&r[l], &r[h]);   /* l < h handles odd and even n */
    print_arr("  reversed [1 2 3 4 5] -> ", r, rn);
    puts("");
}

/* ================================================================== */
/* 2. FAST & SLOW POINTERS                                             */
/* ================================================================== */

typedef struct Node { int val; struct Node *next; } Node;

/*
 * Floyd on a linked list. Returns the first node of the cycle or NULL.
 * Phase 1: slow 1 step, fast 2 steps -> they meet inside the cycle if any.
 * Phase 2: reset one pointer to head, advance both 1 step -> meet at entry
 *          (distance head->entry == distance meeting->entry, see lesson).
 */
static Node *cycle_start(Node *head) {
    Node *slow = head, *fast = head;
    while (fast && fast->next) {                      /* guard BOTH hops */
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            Node *p = head;
            while (p != slow) { p = p->next; slow = slow->next; }
            return p;
        }
    }
    return NULL;
}

/* Same idea on a function graph: successor of i is nums[i]. Values must be
 * valid indices (here in [1, n-1] so index 0 is never a successor and acts
 * as the head of the chain). Returns the entry of the cycle. */
static int cycle_start_func_graph(const int *nums) {
    int slow = 0, fast = 0, step = 0;
    do {
        slow = nums[slow];
        fast = nums[nums[fast]];
        printf("    step %d: slow=%d fast=%d\n", ++step, slow, fast);
    } while (slow != fast);
    int p = 0;
    printf("    phase 2: p=%d slow=%d\n", p, slow);
    while (p != slow) {
        p = nums[p]; slow = nums[slow];
        printf("    phase 2: p=%d slow=%d\n", p, slow);
    }
    return p;
}

static void demo_fast_slow(void) {
    puts("== 2. Fast & slow: cycle detection ==");

    /* Linked list 1 -> 2 -> 3 -> 4 -> 5 -> back to 3   (cycle entry = node 3) */
    Node nodes[5];
    for (int i = 0; i < 5; i++) { nodes[i].val = i + 1; nodes[i].next = (i + 1 < 5) ? &nodes[i + 1] : NULL; }
    nodes[4].next = &nodes[2];
    Node *entry = cycle_start(&nodes[0]);
    printf("  list 1->2->3->4->5->(3): cycle entry = node %d\n", entry ? entry->val : -1);

    nodes[4].next = NULL;                             /* break the cycle */
    entry = cycle_start(&nodes[0]);
    printf("  list 1->2->3->4->5->NULL: cycle entry = %s\n", entry ? "??" : "NULL (acyclic)");

    /* Middle of the list: slow ends at the middle when fast hits the end. */
    Node *slow = &nodes[0], *fast = &nodes[0];
    while (fast && fast->next) { slow = slow->next; fast = fast->next->next; }
    printf("  middle of 1..5 = %d\n", slow->val);

    /* Function graph: 6 values in [1,5], so one value repeats -> guaranteed cycle. */
    int nums[] = {3, 1, 3, 4, 2, 5};
    int n = (int)(sizeof nums / sizeof nums[0]);
    print_arr("  function graph nums = ", nums, n);
    puts("    (successor of i is nums[i]; start at 0)");
    int e = cycle_start_func_graph(nums);
    printf("  -> cycle entry = %d (the value with two incoming edges)\n\n", e);
}

/* ================================================================== */
/* 3. FIXED-SIZE WINDOW                                                */
/* ================================================================== */

/* Invariant: sum == sum of a[right-k+1 .. right]. */
static void fixed_window_sums(const int *a, int n, int k) {
    long long sum = 0, best = LLONG_MIN;
    for (int i = 0; i < k; i++) sum += a[i];          /* first window directly */
    printf("    window [0,%d] sum=%lld\n", k - 1, sum);
    best = sum;
    for (int right = k; right < n; right++) {
        sum += a[right] - a[right - k];               /* add entering, remove leaving: O(1) */
        printf("    window [%d,%d] sum=%lld\n", right - k + 1, right, sum);
        if (sum > best) best = sum;
    }
    printf("  max window sum = %lld, max average = %.3f (divide ONCE at the end)\n",
           best, (double)best / k);
}

/*
 * Sliding window maximum with a monotonic deque of INDICES.
 * dq[head..tail) holds indices whose values are strictly decreasing.
 * Every index is pushed once and popped at most once -> O(n) total.
 */
static void fixed_window_max(const int *a, int n, int k) {
    int *dq = malloc((size_t)n * sizeof *dq);         /* n slots: never wraps, no ring buffer needed */
    if (!dq) { perror("malloc"); exit(1); }
    int head = 0, tail = 0;
    for (int r = 0; r < n; r++) {
        while (tail > head && a[dq[tail - 1]] <= a[r]) tail--;   /* smaller-or-equal can never be max again */
        dq[tail++] = r;
        if (dq[head] <= r - k) head++;                            /* front expired */
        if (r >= k - 1) {
            printf("    r=%d  dq=", r);
            for (int q = head; q < tail; q++) printf("%d(%d)%s", dq[q], a[dq[q]], q + 1 < tail ? "," : "");
            printf("  window [%d,%d] max=%d\n", r - k + 1, r, a[dq[head]]);
        }
    }
    free(dq);
}

static void demo_fixed_window(void) {
    puts("== 3. Fixed window (k = 3) ==");
    int a[] = {2, 1, 5, 1, 3, 2};
    int n = (int)(sizeof a / sizeof a[0]);
    print_arr("  a = ", a, n);
    puts("  running sums:");
    fixed_window_sums(a, n, 3);
    puts("  window max via monotonic deque (index(value)):");
    fixed_window_max(a, n, 3);
    puts("");
}

/* ================================================================== */
/* 4. VARIABLE WINDOW                                                  */
/* ================================================================== */

/*
 * LONGEST subarray with sum <= bound (all a[i] > 0 so the predicate is monotone).
 * Rhythm: grow always; when broken, shrink until JUST valid; record after each right.
 */
static int longest_sum_at_most(const int *a, int n, long long bound) {
    int left = 0, best = 0;
    long long sum = 0;
    for (int right = 0; right < n; right++) {
        sum += a[right];
        while (sum > bound) { sum -= a[left]; left++; }     /* shrink until just valid */
        int len = right - left + 1;
        printf("    r=%d  window [%d,%d] sum=%lld len=%d\n", right, left, right, sum, len);
        if (len > best) best = len;
    }
    return best;
}

/*
 * SHORTEST subarray with sum >= bound (all a[i] > 0).
 * Rhythm: grow; once valid, shrink AS FAR AS POSSIBLE, recording at each valid shrink.
 * Returns 0 if no window qualifies (not the sentinel).
 */
static int shortest_sum_at_least(const int *a, int n, long long bound) {
    int left = 0, best = INT_MAX;
    long long sum = 0;
    for (int right = 0; right < n; right++) {
        sum += a[right];
        while (sum >= bound) {                                /* valid: record, then try shorter */
            int len = right - left + 1;
            printf("    valid [%d,%d] sum=%lld len=%d\n", left, right, sum, len);
            if (len < best) best = len;
            sum -= a[left]; left++;
        }
    }
    return best == INT_MAX ? 0 : best;
}

static void demo_variable_window(void) {
    puts("== 4. Variable window ==");
    int a[] = {2, 3, 1, 2, 4, 3};
    int n = (int)(sizeof a / sizeof a[0]);
    print_arr("  a = ", a, n);
    puts("  longest subarray with sum <= 6:");
    printf("  -> %d\n", longest_sum_at_most(a, n, 6));
    puts("  shortest subarray with sum >= 7:");
    printf("  -> %d\n\n", shortest_sum_at_least(a, n, 7));
}

/* ================================================================== */
/* 5. WINDOW WITH A FREQUENCY TABLE                                    */
/* ================================================================== */

/*
 * Longest substring with at most k distinct bytes.
 * State: have[256] (window histogram) + distinct (number of keys with have>0).
 * Index with (unsigned char): plain char may be negative.
 */
static int longest_k_distinct(const char *s, int k) {
    int have[256] = {0}, distinct = 0, left = 0, best = 0;
    int n = (int)strlen(s);                            /* strlen once, not in the loop condition */
    for (int right = 0; right < n; right++) {
        unsigned char c = (unsigned char)s[right];
        if (have[c]++ == 0) distinct++;                /* 0 -> 1: a new key entered */
        while (distinct > k) {
            unsigned char d = (unsigned char)s[left++];
            if (--have[d] == 0) distinct--;            /* 1 -> 0: key left the window */
        }
        printf("    r=%d '%c'  window \"%.*s\" distinct=%d\n", right, s[right], right - left + 1, s + left, distinct);
        if (right - left + 1 > best) best = right - left + 1;
    }
    return best;
}

/*
 * Counting trick: number of subarrays with AT MOST k odd numbers.
 * For each right, [left,right] is the longest valid window ending at right,
 * so every one of its right-anchored sub-windows is valid too: add right-left+1.
 * exactly(k) = atMost(k) - atMost(k-1). atMost(-1) must be 0.
 */
static long long at_most_k_odd(const int *a, int n, int k) {
    if (k < 0) return 0;
    int left = 0, odd = 0;
    long long count = 0;
    for (int right = 0; right < n; right++) {
        if (a[right] % 2 != 0) odd++;
        while (odd > k) { if (a[left] % 2 != 0) odd--; left++; }
        count += right - left + 1;
    }
    return count;
}

static void demo_window_histogram(void) {
    puts("== 5. Window with a frequency table ==");
    const char *s = "eceba";
    printf("  longest substring of \"%s\" with at most 2 distinct bytes:\n", s);
    printf("  -> %d\n", longest_k_distinct(s, 2));

    int a[] = {1, 1, 2, 1, 1};
    int n = (int)(sizeof a / sizeof a[0]);
    print_arr("  a = ", a, n);
    int k = 3;
    long long am_k = at_most_k_odd(a, n, k), am_k1 = at_most_k_odd(a, n, k - 1);
    printf("  subarrays with at most %d odd = %lld, at most %d odd = %lld\n", k, am_k, k - 1, am_k1);
    printf("  -> exactly %d odd = %lld - %lld = %lld\n\n", k, am_k, am_k1, am_k - am_k1);
}

/* ================================================================== */
/* 6. PARTITIONING IN PLACE                                            */
/* ================================================================== */

/*
 * Stable two-region partition with write/read pointers: keep elements
 * satisfying keep(x) at the front in original order, fill the rest with `filler`.
 * Invariant: [0, write) = accepted elements so far, in order; read >= write.
 */
static int is_nonzero(int x) { return x != 0; }

static void stable_partition_front(int *a, int n, int (*keep)(int), int filler) {
    int write = 0;
    for (int read = 0; read < n; read++)
        if (keep(a[read])) a[write++] = a[read];       /* never overwrites unread data */
    for (int i = write; i < n; i++) a[i] = filler;
}

/*
 * Dutch national flag for values {0,1,2}.
 * [0,low) = 0s, [low,mid) = 1s, [mid,high] = unknown, (high,n-1] = 2s.
 * After a high-swap, mid does NOT advance: the incoming element is unknown.
 */
static void dutch_flag(int *a, int n) {
    int low = 0, mid = 0, high = n - 1;
    while (mid <= high) {
        printf("    low=%d mid=%d high=%d  ", low, mid, high);
        print_arr("", a, n);
        if (a[mid] == 0)      { swap_int(&a[low], &a[mid]); low++; mid++; }
        else if (a[mid] == 2) { swap_int(&a[mid], &a[high]); high--; }
        else                  { mid++; }
    }
}

/*
 * Lomuto partition around a[pivot_idx]; returns the pivot's final index.
 * Postcondition: a[lo..p-1] <= a[p] < a[p+1..hi]  (ties go left).
 */
static int partition_lomuto(int *a, int lo, int hi, int pivot_idx) {
    swap_int(&a[pivot_idx], &a[hi]);                   /* park pivot at the end */
    int pivot = a[hi], store = lo;
    for (int i = lo; i < hi; i++)
        if (a[i] <= pivot) swap_int(&a[i], &a[store++]);
    swap_int(&a[store], &a[hi]);
    return store;
}

/* Quickselect: k-th smallest (0-based rank). Iterative -> no recursion depth issue. */
static int quickselect(int *a, int n, int k) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int pivot_idx = lo + rand() % (hi - lo + 1);   /* random pivot: expected O(n) */
        int p = partition_lomuto(a, lo, hi, pivot_idx);
        printf("    partition [%d,%d] pivot=%d landed at %d  ", lo, hi, a[p], p);
        print_arr("", a, n);
        if (p == k) return a[p];
        if (k < p) hi = p - 1; else lo = p + 1;        /* recurse into ONE side only */
    }
    return a[lo];
}

static void demo_partition(void) {
    puts("== 6. Partitioning in place ==");

    int z[] = {0, 1, 0, 3, 12};
    int zn = (int)(sizeof z / sizeof z[0]);
    print_arr("  stable partition (non-zero to front): ", z, zn);
    stable_partition_front(z, zn, is_nonzero, 0);
    print_arr("  -> ", z, zn);

    int c[] = {2, 0, 1, 2, 0};
    int cn = (int)(sizeof c / sizeof c[0]);
    puts("  Dutch national flag:");
    dutch_flag(c, cn);
    print_arr("  -> ", c, cn);

    int q[] = {7, 2, 9, 4, 1, 8, 5};
    int qn = (int)(sizeof q / sizeof q[0]);
    int k = 2;                                         /* rank 2 = third smallest (0-based) */
    print_arr("  quickselect on ", q, qn);
    int v = quickselect(q, qn, k);
    printf("  -> %d-th smallest (0-based) = %d\n", k, v);
    printf("  (k-th LARGEST would be rank n-k = %d in ascending order)\n", qn - k);
}

/* ================================================================== */

int main(void) {
    srand(12345u);                                     /* seed once, in main; fixed seed = reproducible trace */
    demo_opposite_ends();
    demo_fast_slow();
    demo_fixed_window();
    demo_variable_window();
    demo_window_histogram();
    demo_partition();
    return 0;
}
