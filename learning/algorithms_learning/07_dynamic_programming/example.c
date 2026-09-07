/*
 * example.c — Chapter 07: Dynamic programming — pattern skeletons in C11
 *
 * Compile & run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * This file is NOT a set of solutions to the chapter's LeetCode problems.
 * It implements the seven DP *pattern skeletons* on tiny neutral inputs so you
 * can see the memory layout, the fill order and the base cases in real C:
 *
 *   1. 1D DP with a fixed backward window  (rolling variables, O(1) space)
 *      + the same recurrence top-down with memoization and a sentinel
 *      + "dp ending at i" with a separate running best
 *   2. Decision DP / state machine          (two states per step, temporaries)
 *   3. Knapsack: 0/1 vs unbounded            (loop direction; combos vs perms;
 *                                            2D table + reconstruction)
 *   4. Grid DP                              (flat row-major table, sentinel
 *                                            border, single-row rolling)
 *   5. String DP                            ((n+1)x(m+1) table, dp[i] <-> s[i-1])
 *   6. Interval DP                          (fill by increasing length, split k)
 *   7. Bitmask DP                           (dp[mask], popcount, dp[mask][last],
 *                                            submask enumeration)
 *
 * Every table is heap-allocated with malloc/calloc and freed — no VLAs — so the
 * same code works for large n.  2D tables are one flat block indexed
 * t[y * W + x]; see ../../c_learning/06_dynamic_memory/lesson.md.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A "negative infinity"/"infinity" that can still be added to without
 * overflowing: INT_MAX/2 + INT_MAX/2 fits in int.  Never use INT_MAX itself
 * as INF if you ever compute INF + something. */
#define INF (INT_MAX / 2)

static int  imax(int a, int b) { return a > b ? a : b; }
static int  imin(int a, int b) { return a < b ? a : b; }
static void die(const char *msg) { fprintf(stderr, "%s\n", msg); exit(1); }

/* Portable popcount (compilers turn this into a single instruction anyway;
 * __builtin_popcount is GCC/clang-only). */
static int popcount32(unsigned x) {
    int c = 0;
    while (x) { x &= x - 1; c++; }
    return c;
}

/* ========================================================================
 * 1. 1D DP: state, transition, base case
 * ======================================================================== */

/* Number of ways to write n as an ordered sum of 1s and 3s.
 *   state:      ways[i] = ordered sums of i using parts {1,3}
 *   transition: ways[i] = ways[i-1] + ways[i-3]     (last part is 1 or 3)
 *   base:       ways[0] = 1 (the empty sum), ways[<0] = 0
 * The window looks back at most 3, so three rolling variables replace the
 * whole array.  Counts grow fast -> long long. */
static long long ways_1_3_rolling(int n) {
    long long w0 = 1, w1 = 0, w2 = 0;   /* ways[i-1], ways[i-2], ways[i-3] */
    for (int i = 1; i <= n; i++) {
        long long cur = w0 + w2;         /* ways[i-1] + ways[i-3] */
        w2 = w1; w1 = w0; w0 = cur;      /* slide the window */
    }
    return w0;
}

/* The same recurrence top-down: memo[] filled lazily; -1 = "not computed".
 * memset with 0xFF sets every byte to 0xFF, which is -1 for two's-complement
 * ints — a common trick for int sentinels (works for 0 and -1 only). */
static long long ways_1_3_memo(int n, long long *memo) {
    if (n < 0) return 0;                 /* base: no way to reach a negative total */
    if (n == 0) return 1;                /* base: the empty sum — forget this and
                                            every value collapses to 0 */
    if (memo[n] != -1) return memo[n];
    memo[n] = ways_1_3_memo(n - 1, memo) + ways_1_3_memo(n - 3, memo);
    return memo[n];
}

/* "dp ending at i" pattern: length of the longest strictly increasing
 * CONTIGUOUS run.  end[i] = 1 + end[i-1] if a[i] > a[i-1], else 1.
 * The answer is max over all i — not end[n-1] — so keep a separate best. */
static int longest_increasing_run(const int *a, int n) {
    if (n == 0) return 0;
    int end_here = 1, best = 1;
    for (int i = 1; i < n; i++) {
        end_here = (a[i] > a[i - 1]) ? end_here + 1 : 1;
        best = imax(best, end_here);
    }
    return best;
}

static void demo_1d(void) {
    puts("== 1. 1D DP ==");
    long long memo[16];
    memset(memo, 0xFF, sizeof memo);                 /* all -1 */
    for (int n = 0; n <= 8; n++)
        printf("  ways(%d) rolling=%lld memo=%lld\n", n,
               ways_1_3_rolling(n), ways_1_3_memo(n, memo));
    int a[] = {5, 1, 2, 3, 2, 4, 6, 7, 1};
    printf("  longest increasing run of {5,1,2,3,2,4,6,7,1} = %d (expect 4)\n",
           longest_increasing_run(a, 9));
}

/* ========================================================================
 * 2. Decision DP: a state machine per step
 * ======================================================================== */

/* A machine processes a sequence.  At step i it is either ON or OFF.
 *   - Being ON at step i costs run[i]; being OFF costs 0 but incurs a
 *     penalty miss[i] (work not done).
 *   - Switching state between two consecutive steps costs `sw`.
 * Minimize total cost.  Two states per step -> two variables.
 *
 *   state:      on[i]  = min cost of steps 0..i ending ON
 *               off[i] = min cost of steps 0..i ending OFF
 *   transition: on[i]  = run[i]  + min(on[i-1], off[i-1] + sw)
 *               off[i] = miss[i] + min(off[i-1], on[i-1] + sw)
 *   base:       on[0] = run[0], off[0] = miss[0]  (no switch before step 0)
 *
 * Both new values depend on BOTH old values -> compute into temporaries and
 * assign afterwards.  Updating `on` in place first would feed the new `on`
 * into `off`'s transition, which is a different (wrong) machine. */
static int two_state_machine(const int *run, const int *miss, int n, int sw) {
    int on = run[0], off = miss[0];
    for (int i = 1; i < n; i++) {
        int new_on  = run[i]  + imin(on, off + sw);
        int new_off = miss[i] + imin(off, on + sw);
        on = new_on; off = new_off;
    }
    return imin(on, off);
}

static void demo_state_machine(void) {
    puts("== 2. Decision DP / state machine ==");
    int run[]  = {3, 3, 3, 3, 3};
    int miss[] = {1, 1, 9, 9, 1};
    /* switch cost 1: OFF,OFF,ON,ON,OFF = 1+1+3+3+1 + 2 switches = 11
     * switch cost 10: never switch; all ON = 15, all OFF = 21 -> 15 */
    printf("  sw=1  -> %d (expect 11)\n", two_state_machine(run, miss, 5, 1));
    printf("  sw=10 -> %d (expect 15)\n", two_state_machine(run, miss, 5, 10));
}

/* ========================================================================
 * 3. Knapsack & subset sums: loop direction is the whole game
 * ======================================================================== */

/* 0/1 subset-sum feasibility.  reach[s] = can some subset sum to exactly s.
 * Each item may be used ONCE, so s runs from high to low: reach[s-w] is then
 * still the value from BEFORE this item was considered. */
static bool subset_sum_01(const int *w, int n, int target, bool *reach) {
    memset(reach, 0, (size_t)(target + 1) * sizeof *reach);
    reach[0] = true;                                  /* empty subset */
    for (int i = 0; i < n; i++)
        for (int s = target; s >= w[i]; s--)          /* DESCENDING */
            reach[s] = reach[s] || reach[s - w[i]];
    return reach[target];
}

/* Same loops ASCENDING = unbounded knapsack: an item may be reused because
 * reach[s-w] may already include this very item. */
static bool subset_sum_unbounded(const int *w, int n, int target, bool *reach) {
    memset(reach, 0, (size_t)(target + 1) * sizeof *reach);
    reach[0] = true;
    for (int i = 0; i < n; i++)
        for (int s = w[i]; s <= target; s++)          /* ASCENDING */
            reach[s] = reach[s] || reach[s - w[i]];
    return reach[target];
}

/* Counting ways with unlimited reuse.
 *   items outer, sum inner  -> COMBINATIONS (order irrelevant)
 *   sum outer, items inner  -> PERMUTATIONS (ordered sequences)
 * Same recurrence dp[s] += dp[s - item]; only the loop nesting differs. */
static long long count_combinations(const int *items, int n, int target) {
    long long *dp = calloc((size_t)target + 1, sizeof *dp);
    if (!dp) die("oom");
    dp[0] = 1;
    for (int i = 0; i < n; i++)
        for (int s = items[i]; s <= target; s++)
            dp[s] += dp[s - items[i]];
    long long r = dp[target];
    free(dp);
    return r;
}

static long long count_permutations(const int *items, int n, int target) {
    long long *dp = calloc((size_t)target + 1, sizeof *dp);
    if (!dp) die("oom");
    dp[0] = 1;
    for (int s = 1; s <= target; s++)
        for (int i = 0; i < n; i++)
            if (items[i] <= s) dp[s] += dp[s - items[i]];
    long long r = dp[target];
    free(dp);
    return r;
}

/* 0/1 knapsack with the full 2D table so we can RECONSTRUCT the choice.
 *   T[i][c] = best value using items 0..i-1 with capacity c   (flat, W+1 wide)
 *   T[i][c] = max(T[i-1][c], T[i-1][c-wt[i-1]] + val[i-1])
 * Walking back from T[n][W]: if T[i][c] != T[i-1][c], item i-1 was taken. */
static int knapsack_01_with_items(const int *wt, const int *val, int n, int W,
                                  bool *taken) {
    int cols = W + 1;
    int *T = calloc((size_t)(n + 1) * (size_t)cols, sizeof *T);  /* row 0 = 0 */
    if (!T) die("oom");
    for (int i = 1; i <= n; i++)
        for (int c = 0; c <= W; c++) {
            int skip = T[(i - 1) * cols + c];
            int take = (c >= wt[i - 1]) ? T[(i - 1) * cols + (c - wt[i - 1])] + val[i - 1]
                                        : INT_MIN;
            T[i * cols + c] = imax(skip, take);
        }
    int best = T[n * cols + W];
    /* reconstruction */
    for (int i = n, c = W; i >= 1; i--) {
        taken[i - 1] = (T[i * cols + c] != T[(i - 1) * cols + c]);
        if (taken[i - 1]) c -= wt[i - 1];
    }
    free(T);
    return best;
}

static void demo_knapsack(void) {
    puts("== 3. Knapsack & subset sums ==");
    int w[] = {3, 4, 5};
    bool reach[10];
    printf("  {3,4,5} 0/1 reaches 9?        %s (expect yes: 4+5)\n",
           subset_sum_01(w, 3, 9, reach) ? "yes" : "no");
    printf("  {3,4,5} 0/1 reaches 6?        %s (expect no)\n",
           subset_sum_01(w, 3, 6, reach) ? "yes" : "no");
    printf("  {3,4,5} unbounded reaches 6?  %s (expect yes: 3+3)\n",
           subset_sum_unbounded(w, 3, 6, reach) ? "yes" : "no");

    int coins[] = {1, 2};
    printf("  ways to make 4 from {1,2}: combinations=%lld (expect 3), "
           "permutations=%lld (expect 5)\n",
           count_combinations(coins, 2, 4), count_permutations(coins, 2, 4));

    int wt[]  = {1, 3, 4, 5};
    int val[] = {1, 4, 5, 7};
    bool taken[4];
    int best = knapsack_01_with_items(wt, val, 4, 7, taken);
    printf("  0/1 knapsack W=7 -> value %d (expect 9), items taken:", best);
    for (int i = 0; i < 4; i++) if (taken[i]) printf(" (w%d,v%d)", wt[i], val[i]);
    putchar('\n');
}

/* ========================================================================
 * 4. Grid DP: flat row-major table with a sentinel border
 * ======================================================================== */

/* Min-cost path from top-left to bottom-right; moves: down, right, diagonal
 * down-right.  Cells with cost < 0 are walls.
 *
 * Trick: allocate (H+1) x (W+1) and use row 0 / col 0 as an INF border, so
 * the transition never has to special-case "neighbour outside the grid".
 * Cell (y,x) of the grid lives at D[(y+1)*(W+1) + (x+1)]. */
static int grid_min_path(const int *cost, int H, int W) {
    int cols = W + 1;
    int *D = malloc((size_t)(H + 1) * (size_t)cols * sizeof *D);
    if (!D) die("oom");
    for (int i = 0; i < (H + 1) * cols; i++) D[i] = INF;   /* border + init */

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int c = cost[y * W + x];
            int *cell = &D[(y + 1) * cols + (x + 1)];
            if (c < 0) { *cell = INF; continue; }           /* wall */
            if (y == 0 && x == 0) { *cell = c; continue; }  /* start */
            int up   = D[y * cols + (x + 1)];
            int left = D[(y + 1) * cols + x];
            int diag = D[y * cols + x];
            int m = imin(up, imin(left, diag));
            *cell = (m >= INF) ? INF : m + c;               /* keep INF as INF */
        }
    int r = D[H * cols + W];
    free(D);
    return r >= INF ? -1 : r;
}

/* Same idea with a single rolling row: counts monotone (down/right) paths
 * avoiding walls.  Row y depends only on row y-1, and within the row, on the
 * cell to the left which we have just written -> in-place works. */
static long long grid_count_paths_rolling(const bool *wall, int H, int W) {
    long long *row = calloc((size_t)W, sizeof *row);
    if (!row) die("oom");
    row[0] = wall[0] ? 0 : 1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (wall[y * W + x]) { row[x] = 0; continue; }
            if (y == 0 && x == 0) continue;
            long long from_left = (x > 0) ? row[x - 1] : 0;
            /* row[x] currently holds the value from row y-1 (= "from above") */
            row[x] = row[x] + from_left;
        }
    long long r = row[W - 1];
    free(row);
    return r;
}

static void demo_grid(void) {
    puts("== 4. Grid DP ==");
    /* 3x4 grid; -1 is a wall */
    int cost[] = { 1, 3, 1, 2,
                   2,-1, 1, 9,
                   4, 2, 1, 1 };
    /* Best: (0,0)->(0,1)->diag (1,2)->diag (2,3) = 1+3+1+1 = 6.
     * Without the diagonal move the best would be 7. */
    printf("  min-cost path (down/right/diag) = %d (expect 6)\n",
           grid_min_path(cost, 3, 4));
    bool wall[] = { 0,0,0,
                    0,1,0,
                    0,0,0 };
    printf("  monotone paths in 3x3 with centre wall = %lld (expect 2)\n",
           grid_count_paths_rolling(wall, 3, 3));
}

/* ========================================================================
 * 5. String DP: (n+1) x (m+1) table, dp[i][j] talks about s[i-1], t[j-1]
 * ======================================================================== */

/* Longest common SUBSTRING (contiguous) of two strings.
 *   L[i][j] = length of the longest common suffix of s[0..i) and t[0..j)
 *   L[i][j] = L[i-1][j-1] + 1 if s[i-1] == t[j-1], else 0
 *   base:     L[0][*] = L[*][0] = 0    (empty prefix)
 * Answer = max over the table (a substring can end anywhere), and we remember
 * where it ended so we can print it.  Row i only needs row i-1, but we keep
 * the full table here because it is the layout you will use most often. */
static int longest_common_substring(const char *s, const char *t, int *end_in_s) {
    int n = (int)strlen(s), m = (int)strlen(t), cols = m + 1;
    int *L = calloc((size_t)(n + 1) * (size_t)cols, sizeof *L);
    if (!L) die("oom");
    int best = 0; *end_in_s = 0;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++) {
            if (s[i - 1] == t[j - 1]) {          /* NOTE: i-1, j-1 */
                L[i * cols + j] = L[(i - 1) * cols + (j - 1)] + 1;
                if (L[i * cols + j] > best) { best = L[i * cols + j]; *end_in_s = i; }
            }                                   /* else stays 0 from calloc */
        }
    free(L);
    return best;
}

/* Interval-style boolean table over ONE string: is_pal[i][j] = s[i..j] is a
 * palindrome.  Fill by increasing length because [i][j] needs [i+1][j-1].
 * Returned as a flat n*n array of bool; caller frees. */
static bool *palindrome_table(const char *s, int n) {
    bool *P = calloc((size_t)n * (size_t)n, sizeof *P);
    if (!P) die("oom");
    for (int len = 1; len <= n; len++)
        for (int i = 0; i + len - 1 < n; i++) {
            int j = i + len - 1;
            bool ends = (s[i] == s[j]);
            P[i * n + j] = ends && (len < 3 || P[(i + 1) * n + (j - 1)]);
        }
    return P;
}

static void demo_string(void) {
    puts("== 5. String DP ==");
    const char *s = "xabcdy", *t = "zbcdw";
    int end;
    int len = longest_common_substring(s, t, &end);
    printf("  LCSubstring(\"%s\",\"%s\") = %d -> \"%.*s\" (expect 3 \"bcd\")\n",
           s, t, len, len, s + end - len);
    const char *p = "abcba";
    int n = (int)strlen(p);
    bool *P = palindrome_table(p, n);
    printf("  palindromic substrings of \"%s\": ", p);
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++)
            if (P[i * n + j]) printf("%.*s ", j - i + 1, p + i);
    puts("(expect 7: a abcba b bcb c b a)");
    free(P);
}

/* ========================================================================
 * 6. Interval DP: dp[i][j] over [i,j], split at the LAST/chosen k
 * ======================================================================== */

/* Matrix-chain multiplication: dims[] has n+1 entries; matrix k is
 * dims[k] x dims[k+1].  Minimal scalar multiplications to multiply the chain.
 *   C[i][j] = min over i<=k<j of C[i][k] + C[k+1][j] + dims[i]*dims[k+1]*dims[j+1]
 *   base:     C[i][i] = 0   (single matrix — nothing to multiply)
 * Fill by increasing interval length; every sub-interval is then ready.
 * O(n^2) intervals x O(n) split points = O(n^3). */
static long long matrix_chain(const int *dims, int n) {
    long long *C = calloc((size_t)n * (size_t)n, sizeof *C);   /* C[i][i]=0 */
    if (!C) die("oom");
    for (int len = 2; len <= n; len++)
        for (int i = 0; i + len - 1 < n; i++) {
            int j = i + len - 1;
            long long best = LLONG_MAX;
            for (int k = i; k < j; k++) {
                long long c = C[i * n + k] + C[(k + 1) * n + j]
                            + (long long)dims[i] * dims[k + 1] * dims[j + 1];
                if (c < best) best = c;
            }
            C[i * n + j] = best;
        }
    long long r = C[0 * n + (n - 1)];
    free(C);
    return r;
}

/* Game-difference interval DP: two players alternately take the left or
 * right end of a[]; D[i][j] = best (mover - opponent) score on a[i..j].
 *   D[i][j] = max(a[i] - D[i+1][j], a[j] - D[i][j-1]);  D[i][i] = a[i]
 * Storing the DIFFERENCE turns a two-player minimax into one max. */
static int end_game_difference(const int *a, int n) {
    int *D = calloc((size_t)n * (size_t)n, sizeof *D);
    if (!D) die("oom");
    for (int i = 0; i < n; i++) D[i * n + i] = a[i];
    for (int len = 2; len <= n; len++)
        for (int i = 0; i + len - 1 < n; i++) {
            int j = i + len - 1;
            D[i * n + j] = imax(a[i] - D[(i + 1) * n + j],
                                a[j] - D[i * n + (j - 1)]);
        }
    int r = D[0 * n + (n - 1)];
    free(D);
    return r;
}

static void demo_interval(void) {
    puts("== 6. Interval DP ==");
    int dims[] = {10, 30, 5, 60};   /* A:10x30, B:30x5, C:5x60 */
    printf("  matrix chain (10x30)(30x5)(5x60) -> %lld mults (expect 4500)\n",
           matrix_chain(dims, 3));
    int a[] = {3, 9, 1, 2};
    printf("  end-game difference on {3,9,1,2} = %d (expect 7)\n",
           end_game_difference(a, 4));
}

/* ========================================================================
 * 7. Bitmask DP: an integer is the set
 * ======================================================================== */

/* Assignment: n slots, n items, cost[slot][item]; each item used once.
 *   dp[mask] = min cost of filling the first popcount(mask) slots with
 *              exactly the items in mask
 *   dp[mask | 1<<i] = min(., dp[mask] + cost[popcount(mask)][i])  for i not in mask
 *   base: dp[0] = 0
 * Iterating masks in increasing numeric order is a valid topological order,
 * because mask | bit > mask whenever the bit was not set. */
static int assignment_bitmask(const int *cost, int n) {
    unsigned full = (1u << n) - 1u;        /* 1u: never shift a signed 1 by 31 */
    int *dp = malloc(((size_t)full + 1) * sizeof *dp);
    if (!dp) die("oom");
    for (unsigned m = 0; m <= full; m++) dp[m] = INF;
    dp[0] = 0;
    for (unsigned mask = 0; mask < full; mask++) {
        if (dp[mask] >= INF) continue;           /* unreachable */
        int slot = popcount32(mask);
        for (int i = 0; i < n; i++) {
            if (mask & (1u << i)) continue;      /* item already used */
            unsigned nm = mask | (1u << i);
            dp[nm] = imin(dp[nm], dp[mask] + cost[slot * n + i]);
        }
    }
    int r = dp[full];
    free(dp);
    return r;
}

/* Shortest Hamiltonian path in a small complete weighted graph (TSP path).
 *   dp[mask][last] = shortest walk visiting exactly `mask`, ending at `last`
 *   dp[1<<s][s] = 0 for every start s
 *   dp[mask|1<<v][v] = min(., dp[mask][u] + w[u][v])
 * The extra "last" dimension is needed because the next edge's cost depends
 * on where we are NOW, not only on what we have visited. Flat: dp[mask*n+last]. */
static int tsp_path(const int *w, int n) {
    unsigned full = (1u << n) - 1u;
    int *dp = malloc(((size_t)full + 1) * (size_t)n * sizeof *dp);
    if (!dp) die("oom");
    for (size_t i = 0; i < ((size_t)full + 1) * (size_t)n; i++) dp[i] = INF;
    for (int s = 0; s < n; s++) dp[(1u << s) * n + s] = 0;
    for (unsigned mask = 1; mask <= full; mask++)
        for (int u = 0; u < n; u++) {
            if (!(mask & (1u << u))) continue;
            int cur = dp[mask * n + u];
            if (cur >= INF) continue;
            for (int v = 0; v < n; v++) {
                if (mask & (1u << v)) continue;
                unsigned nm = mask | (1u << v);
                dp[nm * n + v] = imin(dp[nm * n + v], cur + w[u * n + v]);
            }
        }
    int best = INF;
    for (int u = 0; u < n; u++) best = imin(best, dp[full * n + u]);
    free(dp);
    return best;
}

/* Enumerating all submasks of a mask in O(2^k), k = popcount(mask):
 *   for (sub = mask; ; sub = (sub - 1) & mask) { ...; if (sub == 0) break; }
 * Used when a transition "picks any subset of the remaining elements". */
static int count_submasks(unsigned mask) {
    int c = 0;
    for (unsigned sub = mask;; sub = (sub - 1) & mask) {
        c++;
        if (sub == 0) break;
    }
    return c;
}

static void demo_bitmask(void) {
    puts("== 7. Bitmask DP ==");
    int cost[] = { 4, 2, 8,     /* slot 0 */
                   4, 3, 7,     /* slot 1 */
                   3, 1, 9 };   /* slot 2 */
    printf("  min-cost assignment 3x3 = %d (expect 12)\n", assignment_bitmask(cost, 3));
    int w[] = { 0, 2, 9, 10,
                1, 0, 6,  4,
                15, 7, 0, 8,
                6, 3, 12, 0 };
    printf("  shortest Hamiltonian path on 4 nodes = %d (expect 12)\n", tsp_path(w, 4));
    printf("  submasks of 0b1011 = %d (expect 8)\n", count_submasks(0xBu));
}

/* ======================================================================== */

int main(void) {
    demo_1d();
    demo_state_machine();
    demo_knapsack();
    demo_grid();
    demo_string();
    demo_interval();
    demo_bitmask();
    return 0;
}
