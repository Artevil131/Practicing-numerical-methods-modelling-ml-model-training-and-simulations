# Chapter 07 — Dynamic programming

## What you'll be able to do after this chapter

- State a DP in three sentences — **state**, **transition**, **base case** — before writing any code, and recognise that a vague state definition is the root of most DP bugs.
- Pick the right table shape (1D rolling variables, 2D flat row-major, 3D with an extra "budget" dimension, `dp[mask]`) and lay it out in C with `malloc`/`calloc`, no VLAs.
- Choose the fill order that makes every dependency ready before it is read: index ascending, capacity descending (0/1) vs ascending (unbounded), interval length ascending, mask numerically ascending, or backwards from the goal.
- Reduce space from O(n) or O(n·m) to O(1) or O(m) when the transition only looks a fixed distance back, and know when you *cannot* (LIS, interval DP, reconstruction).
- Recognise seven families from the problem text: 1D recurrence, state machine, knapsack/subset sum, grid, two-string / interval-over-one-string, interval split, bitmask.
- Debug a DP by printing the table on a 5-element input and checking it by hand.

## Why this matters for ML / numerics / sims

DP is the exact-inference workhorse hiding inside many things you already use:

- **Viterbi decoding** of an HMM / CRF is a 2D DP `dp[t][state] = max over prev (dp[t-1][prev] + log A[prev][state]) + log B[state][obs_t]` — a state machine (Unit 2) laid out over time. **Forward-backward** is the same table with `logsumexp` instead of `max`.
- **CTC loss** (speech, OCR) is a forward DP over `(time, position in extended label)`; the backward pass through it is the same table read in reverse (Unit 4's "count paths through a grid" with a blank-skip rule).
- **Edit distance** = WER / CER metrics; **Needleman–Wunsch / Smith–Waterman** = the two-string DP of Unit 5 with a scoring matrix; **Dynamic Time Warping** = grid DP with `min` of three neighbours (Unit 4) on a distance matrix.
- **Value iteration / Bellman equation** in RL is DP over states with the discount as the "cost" — the same fixed-point structure as `dp[i] = best over choices (reward + dp[next])`.
- **Optimal segmentation** of a time series into piecewise-constant/linear pieces (changepoint detection, Bellman's segmentation) is `dp[i] = min over j<i (dp[j] + cost(j,i))` — exactly the palindrome-partitioning shape in Unit 5.
- **Knapsack** = packing batches under a memory budget, choosing which activations to recompute vs store (checkpointing), selecting features under a cost budget (Unit 3).
- **Interval DP** = optimal matrix-chain parenthesisation (the order in which you multiply `A B C D` changes the FLOP count by orders of magnitude) and optimal binary-tree / triangulation problems in mesh generation (Unit 6).
- **Bitmask DP** = exact TSP-style scheduling of a small number of simulation jobs, exact subset selection when `n ≤ 20` — the brute force you run to *validate* a heuristic (Unit 7).

Python gives you `functools.lru_cache` and a `[[0]*m for _ in range(n)]` table. In C you decide the layout, the fill order and the sentinels yourself — which is exactly what makes a DP kernel fast.

---

## 0. The recipe (applies to every unit)

Every DP in this chapter is written down the same way before any code:

| Part | Question it answers | Typical bug when missing |
|---|---|---|
| **State** | What does `dp[...]` mean, *in words*, including what is fixed ("ending exactly at i", "using items 0..i-1", "prefix of length i")? | Off-by-one, mixing "up to i" with "exactly at i" |
| **Transition** | Which smaller states does it depend on, and how are they combined (`+`, `min`, `max`, `||`)? | Wrong operator, missing a case |
| **Base case** | Which states cannot be computed from the formula and must be given directly? | `dp[0]` = 0 vs 1, empty prefix, `-INF` for impossible |
| **Order** | In what order are states computed so every dependency is already final? | Reading a not-yet-computed or already-overwritten cell |
| **Answer** | Which cell(s) hold the answer — the last one, or the max over all? | Returning `dp[n-1]` when the answer is `max(dp)` |

The fill order is what separates the two implementation styles:

- **Bottom-up (tabulation):** loops in the dependency order, arrays, no recursion. Default in C.
- **Top-down (memoisation):** recursion with a `memo[]` initialised to a sentinel (`-1`, or `INF`). Easier when the reachable state space is sparse or the order is awkward (Unit 7's Can-I-Win), but it costs stack depth — see "Gotchas in C".

```c
/* Top-down skeleton: sentinel + recursion. memo must be sized to the state space. */
static long long solve(int i, long long *memo) {
    if (i < 0) return 0;                 /* impossible / empty  -> base */
    if (i == 0) return 1;                /* base case given directly */
    if (memo[i] != -1) return memo[i];   /* already computed */
    memo[i] = solve(i - 1, memo) + solve(i - 3, memo);
    return memo[i];
}
/* caller: memset(memo, 0xFF, n * sizeof memo[0]);  -> every int/long long == -1 */
```

Forget the `i == 0` base and every value silently becomes 0 — `example.c` had exactly that bug on the first compile.

---

## 1. 1D DP: state, transition, base case

One-dimensional DP is the simplest form: the state is a single index `i`, and `dp[i]` depends only on smaller indices. The entire difficulty is defining the state **precisely** in words — a vague state is the most common source of errors.

**State.** `dp[i]` = the best (or only) answer when considering only the prefix up to length `i` — for example "largest sum of a subarray that **ends exactly at** index i" or "number of ways to reach step i".

**Transition.** `dp[i]` is computed from one or a few preceding states, e.g. `dp[i] = dp[i-1] + dp[i-2]` (stairs) or `dp[i] = max(dp[i-1], 0) + a[i]` (maximum subarray). The shape of the transition tells you how many previous states you have to keep in memory.

**Base case.** The smallest indices, which cannot be computed by the formula — they must be given directly, e.g. `dp[0] = 1`, `dp[-1]` interpreted as the empty path.

**Complexity.** Typically O(n): every state is computed in constant time from a constant number of predecessors. Space can often be reduced to O(1) because only the last couple of values are ever needed — **but only if the transition uses a fixed backward window, not the whole history** (LIS's O(n²) form looks at *all* `j < i`, so it keeps the full array).

**Two sub-shapes to tell apart:**

| Shape | State means | Answer | Example transitions |
|---|---|---|---|
| "prefix" | best over the whole prefix `0..i` | `dp[n-1]` (or `dp[n]`) | `dp[i] = max(dp[i-1], dp[i-2] + a[i])` |
| "ending at i" | best among things that **end exactly at i** | `max(dp)` over all `i` | `dp[i] = max(a[i], dp[i-1] + a[i])` |

Mixing them up is the classic Kadane bug: returning the last cell instead of the running best.

**C layout.** Either a `long long dp[n+1]` allocated with `calloc` (counts overflow `int` fast — the stairs sequence passes 2³¹ at n = 46), or two/three rolling scalars. Use `long long` for counts, `int` for lengths, and take `% MOD` on every addition when a modulus is specified.

```c
/* Rolling-window skeleton: dp[i] = dp[i-1] + dp[i-3], dp[0] = 1, dp[<0] = 0 */
long long w0 = 1, w1 = 0, w2 = 0;          /* dp[i-1], dp[i-2], dp[i-3] */
for (int i = 1; i <= n; i++) {
    long long cur = w0 + w2;
    w2 = w1; w1 = w0; w0 = cur;             /* slide */
}
/* answer: w0 */

/* "Ending at i" skeleton with a separate best: */
int end_here = a[0], best = a[0];
for (int i = 1; i < n; i++) {
    end_here = imax(a[i], end_here + a[i]);  /* start fresh at i, or extend */
    best = imax(best, end_here);
}
```

**Worked mini-example** — `dp[i] = max(a[i], dp[i-1] + a[i])` on `a = [2, -3, 4, -1, 2, -5, 3]`:

```
i     : 0    1    2    3    4    5    6
a[i]  : 2   -3    4   -1    2   -5    3
dp[i] : 2   -1    4    3    5    0    3      (dp[1] = max(-3, 2-3) = -1; dp[2] = max(4, -1+4) = 4 ...)
best  : 2    2    4    4    5    5    5
```

`dp[6] = 3` but the answer is `best = 5` (subarray `[4, -1, 2]`). Notice that `dp[5] = 0` is "the empty-ish tail": from here on the previous history cannot help, which is why `max(a[i], dp[i-1] + a[i])` is the same as the hint "reset when the running sum goes negative".

**When you recognise it:** "number of ways to reach", "maximum/minimum over a sequence with a local constraint" (no two adjacent), "decode a string left to right", "longest ... ending here". The input is a single sequence and the answer for a prefix depends on a few earlier prefixes.

**Pitfalls.**
- `dp[0]` and `dp[1]` confused as base cases (stairs); the "empty" base `dp[0] = 1` for counting problems.
- Cost paid at the *source* step vs the *destination* step (min-cost stairs) — write the words down.
- Greedy "take the bigger neighbour" instead of DP (house robber) — locally better choices exclude a better pair later.
- A character `'0'` in a decoding problem: a lone zero encodes nothing; it only works as part of `10` or `20`, and a dead prefix (`dp[i] = 0`) propagates forward.
- Re-indexing by *value* instead of *position* (Delete and Earn): the "neighbour" is `v±1`, not `i±1` — build `points[v]` first.

**Python equivalent:** `functools.lru_cache` on a recursive function is the top-down form; a plain list is the bottom-up form. NumPy has no DP primitive — the loop is inherently sequential.

---

## 2. Decision DP: take or skip → state machines

Decision DP generalises "take or skip" to states where a choice has a **lasting effect** on the future — not just "used / not used" but e.g. "I currently hold a share" or "I cannot buy right now because I sold yesterday".

**State** is no longer just the index `i` but a pair `(index, extra state)`: `dp[i][hold]` = best result up to day `i` given that `hold` says whether we own the share now. This is a **state machine**: each extra state is a node, and the transitions are edges (buy, sell, wait).

**Transition** iterates over all allowed edges out of each state: `dp[i][hold] = max(dp[i-1][hold], dp[i-1][!hold] ± price[i])`, with the sign depending on whether we buy or sell. Cooldown and transaction-fee variants add a third state or a subtracted term on an edge.

**Base case:** day 0 before any trade — `dp[0][not holding] = 0`, `dp[0][holding] = -price[0]`.

House Robber II is structurally the same idea from another direction: a circular constraint (house 0 and house n-1 cannot both be taken) is solved by running the linear house-robber twice — once without house 0, once without house n-1 — and taking the better result. The constraint is converted into two separate subproblems.

**Draw the machine before coding.** Two-state (unlimited trades):

```
          buy (-price)
   cash  ----------->  hold
     ^                  |
     +------------------+
          sell (+price)
   (each node also has a self-loop "wait")
```

Three-state with cooldown: `hold --sell--> sold --wait--> rest --buy--> hold`, `rest --wait--> rest`. The `sold` node exists only to forbid `sold -> hold` in one step.

**Complexity.** O(n · S · E) where S = number of extra states and E = edges per state — for S ≤ 3 this is O(n) with O(1) space. Adding a "transactions used" counter multiplies by K: O(nK) time, O(K) space (Stock IV).

**C layout.** One scalar per machine state; if S grows (Stock IV), `int buy[K+1], sell[K+1]` from `calloc`. All new values must be computed from the **old** values, so either use temporaries or order the assignments so that each read happens before the write it depends on.

```c
/* Two-state machine skeleton. `on`/`off` are dp[i-1][*]; temporaries hold dp[i][*]. */
int on = run[0], off = miss[0];                     /* base: day 0 */
for (int i = 1; i < n; i++) {
    int new_on  = run[i]  + imin(on,  off + sw);    /* stay on, or switch on */
    int new_off = miss[i] + imin(off, on  + sw);    /* stay off, or switch off */
    on = new_on; off = new_off;                     /* commit together */
}
int answer = imin(on, off);
```

**Worked mini-example** — unlimited trades, `prices = [3, 1, 4, 2, 5]`, `cash[i] = max(cash, hold + p)`, `hold[i] = max(hold, cash - p)`:

```
day  price   cash                      hold
 0     3      0                        -3
 1     1      max(0, -3+1) = 0         max(-3, 0-1) = -1
 2     4      max(0, -1+4) = 3         max(-1, 0-4) = -1
 3     2      max(3, -1+2) = 3         max(-1, 3-2) =  1
 4     5      max(3,  1+5) = 6         max( 1, 3-5) =  1
```

Answer `cash[4] = 6` = (4−1) + (5−2). With unlimited trades the order of the two updates happens not to matter (buying and selling on the same day nets 0), but with a fee or a cooldown it does — which is why the skeleton always uses temporaries.

**When you recognise it:** a sequence plus a small set of "modes" you can be in (holding / not, on / off, k trades left, cooling down), and the allowed moves depend on the mode. Also: a circular array constraint that splits into two linear runs.

**Pitfalls.**
- Updating `minPrice` *before* computing `best` on the same day (Stock I) → you "sell" on the day you bought.
- Collapsing `sold` and `rest` into one node → the cooldown constraint silently disappears.
- Subtracting the fee on *both* buy and sell → every trade double-charged.
- `buy2` must read `sell1` of the **same** day before `sell1` is updated (Stock III) — assignment order inside the loop is semantic.
- Stock IV with huge K: if `K >= n/2` the constraint is vacuous → fall back to the unlimited version, or your `dp[i][k]` table will not fit in memory.
- Greedy works for exactly one variant (unlimited, no fee/cooldown: sum all positive daily deltas). Do not generalise it.

---

## 3. Knapsack & subset sums

In knapsack DP the state is (items processed, remaining capacity), and the most important distinction is **0/1 knapsack** (each item at most once) versus **unbounded knapsack** (an item may repeat, e.g. coins).

**State.** `dp[w]` = best value (or number of ways, or feasibility) at capacity `w`, once all permitted items have been considered.

**The transition and the iteration order are the whole story.** In the 0/1 knapsack, every item is processed once and the capacity is updated **from large to small** (`for w = W .. item.weight`), so that the same item is not chosen twice within the same row. In the unbounded knapsack the capacity is traversed **from small to large**, so the just-updated value may influence itself — the item may repeat.

**Combinations vs permutations when counting ways:** if the loop order is (item outer, sum inner), you count *combinations* (order irrelevant); if it is (sum outer, item inner), you count *permutations* (order matters). This is the most common cause of wrong answers in counting problems.

**Subset sum** is the special case of 0/1 knapsack where "value" = "weight": is there a subset summing to exactly `T`? The base case is always `dp[0] = true` or `dp[0] = 1` — the empty subset always sums to zero.

**Why the direction works.** Think of the 2D table `T[i][w]` = best using items `0..i-1` at capacity `w`; the 1D array is row `i` overwriting row `i-1` in place. Going downward in `w`, the cell `dp[w - wt]` you read is still from row `i-1` (not yet overwritten) → item `i` used at most once. Going upward, `dp[w - wt]` may already be row `i` → item `i` reused → unbounded.

| Variant | Outer loop | Inner loop | Reads | Counts |
|---|---|---|---|---|
| 0/1 (max value / feasibility / #subsets) | items | `w` **descending** | previous item row | each item ≤ 1 time |
| Unbounded, combinations | items | `w` **ascending** | current item row | multiset, order-free |
| Unbounded, permutations | `w` ascending | items | any item at every `w` | ordered sequences |
| 2D capacities (zeros & ones) | items | both capacities **descending** | previous row in both dims | each item ≤ 1 |

**Complexity.** O(n · W) time, O(W) space (O(n · W) if you keep the 2D table to reconstruct *which* items). This is *pseudo-polynomial*: W is a number, not a count, so W = 10⁹ is infeasible.

**C layout.** `bool *reach = calloc(W + 1, 1)` or `long long *dp = calloc(W + 1, sizeof *dp)`; `dp[0] = 1`. For min-count problems (coins) initialise to `INF = INT_MAX / 2` — never `INT_MAX`, because you will compute `dp[w - c] + 1`.

```c
/* 0/1 subset-sum feasibility: descending capacity */
memset(reach, 0, (W + 1) * sizeof *reach);  reach[0] = true;
for (int i = 0; i < n; i++)
    for (int s = W; s >= wt[i]; s--)               /* DESCENDING */
        reach[s] = reach[s] || reach[s - wt[i]];

/* Unbounded min-count (coins): ascending capacity, INF sentinel */
for (int s = 0; s <= W; s++) dp[s] = INF;  dp[0] = 0;
for (int i = 0; i < n; i++)
    for (int s = wt[i]; s <= W; s++)               /* ASCENDING */
        if (dp[s - wt[i]] + 1 < dp[s]) dp[s] = dp[s - wt[i]] + 1;
/* answer: dp[W] >= INF ? -1 : dp[W] */
```

**Worked mini-example** — items `{3, 4, 5}`, target 9, `reach[]` as a set of reachable sums:

```
start        : {0}
after 3 (s=9..3, desc): {0, 3}
after 4      : {0, 3, 4, 7}
after 5      : {0, 3, 4, 5, 7, 8, 9}      -> reach[9] = true  (4+5)
```

Same input, item 3 processed **ascending** by mistake: s=3: reach[3] |= reach[0] → {0,3}; s=6: reach[6] |= reach[3] → {0,3,6}; s=9: → {0,3,6,9}. The single 3 has been used three times — the bug is invisible on inputs where the answer is `true` anyway.

Combinations vs permutations on `{1, 2}`, target 4: item-outer gives `dp = [1,1,2,2,3]` → 3 ways (`1111, 112, 22`); sum-outer gives `[1,1,2,3,5]` → 5 ordered sequences (`112, 121, 211` counted separately).

**When you recognise it:** "choose a subset with total ≤/= capacity", "fewest coins / squares summing to n", "can the array be split into two equal halves", "assign + / − signs to hit a target", "minimum difference between two piles". Any "budget" that is a number, not a count.

**Pitfalls.**
- Wrong loop direction turns 0/1 into unbounded (Partition Equal Subset Sum) — tests pass on small inputs.
- Wrong loop nesting turns combinations into permutations (Coin Change II vs Combination Sum IV) — solve those two side by side.
- Greedy (largest coin first) fails on non-canonical coin sets; greedy (largest square first) fails: `12 = 4+4+4` beats `9+1+1+1`.
- Boundary checks before the DP: odd total → no equal partition; `(sum + target)` odd or `|target| > sum` → 0 ways (Target Sum).
- Disguised problems: "smash stones" (Last Stone Weight II) and "±signs" (Target Sum) are subset sums after an algebraic transform: `P − N = target`, `P + N = sum` ⇒ `P = (sum + target) / 2`.
- Two capacities (Ones and Zeroes) → both inner loops descending.

**Python equivalent:** `reach = {0}; for x in items: reach |= {s + x for s in reach}` is the 0/1 subset sum; note that building a *new* set each iteration is exactly what "descending" achieves in place.

---

## 4. Grid DP

In grid DP the state is a cell `(y, x)`, and the transition combines the value of one or more neighbouring cells. The idea is the same as in 1D DP, but the "predecessors" are now above and to the left (or the opposite direction, depending on the direction of movement).

**State.** `dp[y][x]` = best (or number of ways, or feasibility) of reaching cell `(y, x)` from the start, or symmetrically the best result from cell `(y, x)` to the goal.

**Transition.** Typically `dp[y][x] = f(dp[y-1][x], dp[y][x-1])`, where `f` is `+`, `min` or `max` depending on the problem.

**Base case.** The first row and column, which can only be reached along a single route — often a separately handled boundary condition, because the neighbour would fall outside the grid.

Two extensions appear in the harder problems: a **third dimension** (e.g. the number of steps remaining) brings the path length into the state, and a **reversed direction of computation** (goal to start) is needed when the best result depends on the future, not the past. The routes of two simultaneous walkers are merged into one state `dp[step][pos1][pos2]`, because the step count ties the walkers' rows together.

**Complexity.** O(H · W) for the basic form; × K for a step-count dimension; O(n³) for two walkers on an n×n grid (`t`, `x1`, `x2` — the `y`s are implied by `t`).

**C layout.** A 2D array in C is either a flat block `int *D = malloc(H * W * sizeof *D)` indexed `D[y * W + x]` (preferred: one allocation, contiguous, cache-friendly, passes to functions without knowing W at compile time), or a pointer-to-pointer, which you should avoid for DP tables. Row `y` needs only row `y-1` in the basic form, so a single row of length `W` suffices: `row[x] = row[x] + row[x-1]` reads "above" (old value at `x`) and "left" (new value at `x-1`) in place.

**Sentinel border trick.** Allocate `(H+1) × (W+1)` and fill row 0 and column 0 with the neutral element (`0` for counting, `INF` for min, `-INF` for max). Then the transition never special-cases the first row/column: cell `(y, x)` lives at `D[(y+1)*(W+1) + (x+1)]`.

```c
/* Min-cost path, moves down/right, with a sentinel border of INF */
int cols = W + 1;
int *D = malloc((size_t)(H + 1) * cols * sizeof *D);
for (int i = 0; i < (H + 1) * cols; i++) D[i] = INF;
for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
        int *cell = &D[(y + 1) * cols + (x + 1)];
        if (y == 0 && x == 0) { *cell = cost[0]; continue; }        /* start */
        int m = imin(D[y * cols + (x + 1)], D[(y + 1) * cols + x]);  /* up, left */
        *cell = (m >= INF) ? INF : m + cost[y * W + x];              /* keep INF as INF */
    }
/* answer: D[H * cols + W] */
```

**Worked mini-example** — count monotone paths in a 3×3 grid with a wall at the centre (`dp = up + left`, wall ⇒ 0):

```
grid        dp
. . .       1 1 1
. # .       1 0 1
. . .       1 1 2      -> 2 paths (all-right-then-down, all-down-then-right)
```

Row by row with one rolling array: start `[1,1,1]`; row 1: `x=0` stays 1 (from above), `x=1` wall → 0, `x=2`: 1 (above) + 0 (left) = 1 → `[1,0,1]`; row 2: `x=0` 1, `x=1`: 0 + 1 = 1, `x=2`: 1 + 1 = 2 → `[1,1,2]`. Without the wall the first row and column would all be 1; *with* a wall on the first row every cell after it is unreachable, so "initialise the border to 1" is no longer valid — which is why the sentinel-border form is safer.

**Reversed direction.** When `dp[y][x]` = "what I need *from here on* to survive to the goal" (Dungeon Game), the value at `(y, x)` depends on `(y+1, x)` and `(y, x+1)`, so you fill from the bottom-right corner to the top-left, with the fictitious cells beyond the goal set to the neutral value (health 1). The Triangle problem uses the same idea for a different reason: bottom-up, every node has exactly two children, so edge cases vanish.

**When you recognise it:** a matrix, movement restricted to right/down (or 4-directional with a step budget), "number of paths", "minimum path sum", "largest all-ones square" (`1 + min(up, left, up-left)`), "minimum initial health", "two robots / round trip".

**Pitfalls.**
- Forgetting that the first row/column are *cumulative* for min-sum problems (`min` compares against a nonexistent neighbour otherwise).
- Assuming the first row/column are all 1 when obstacles exist.
- Confusing "largest square" (3-neighbour DP) with "largest rectangle" (a different, stack-based algorithm).
- Same cell reachable with different step counts (Out of Boundary Paths) → `k` must be in the state; take `% (1e9+7)` on every sum.
- Modelling two trips sequentially instead of simultaneously (Cherry Pickup): `dp[t][x1][x2]`, cherry counted once when `x1 == x2`, obstacles as `-INF`.

**Python equivalent:** `np.zeros((H, W))` and two nested `for` loops. `np.cumsum` gives you the first row/column of a min-path table for free.

---

## 5. String DP

In string DP the state is most often indexed by **two** variables: either the prefix lengths `(i, j)` of two strings (comparing two sequences, as in LCS and edit distance), or a sub-interval `[i, j]` of one string (palindromes, interval-based properties).

**Two-sequence DP.** `dp[i][j]` = best result when considering the prefix of `A` of length `i` and the prefix of `B` of length `j`. The transition branches on whether `A[i-1]` and `B[j-1]` are equal: if they are, they are usually merged for free (`dp[i-1][j-1]`); if not, every allowed operation is tried. The base case is the empty prefixes: `dp[0][j]` and `dp[i][0]`.

**Interval DP for palindromes.** `dp[i][j]` = is `s[i..j]` a palindrome. Transition: `dp[i][j] = (s[i] == s[j]) && dp[i+1][j-1]`. Base case: intervals of one and two characters are checked directly. The fill order is **increasing interval length**, because `dp[i][j]` depends on the narrower interval `dp[i+1][j-1]`.

The shared pitfall: off-by-one errors when the `dp` table has size `(n+1) × (m+1)` but the string is 0-indexed — `dp[i][j]` always corresponds to character `s[i-1]`, not `s[i]`.

**The two-sequence family in one table.** Every problem below is the same `(n+1) × (m+1)` table with a different operator on the three neighbours `dp[i-1][j-1]` (diagonal), `dp[i-1][j]` (drop from A), `dp[i][j-1]` (drop from B):

| Problem | match `A[i-1]==B[j-1]` | mismatch | base |
|---|---|---|---|
| LCS length | `diag + 1` | `max(up, left)` | 0 |
| Edit distance | `diag` | `1 + min(diag, up, left)` (replace, delete, insert) | `dp[i][0]=i`, `dp[0][j]=j` |
| Distinct subsequences (count T in S) | `up + diag` | `up` | `dp[i][0]=1`, `dp[0][j>0]=0` |
| Interleaving (bool, third string C) | — | `(up && A[i-1]==C[i+j-1]) \|\| (left && B[j-1]==C[i+j-1])` | `dp[0][0]=true` |
| Longest common *substring* | `diag + 1` | `0` | 0; answer = max over table |

**Complexity.** O(n · m) time and space; O(min(n, m)) space if you keep two rows and do not need to reconstruct. Interval-over-one-string: O(n²) time and space.

**C layout.** Flat `(n+1) * (m+1)` block from `calloc` (so the zero base cases come for free), indexed `dp[i * (m+1) + j]`. Strings are `const char *` with `strlen` computed **once** (it is O(n)). For the palindrome table, `bool *P = calloc(n * n, 1)`, indexed `P[i * n + j]`, `i <= j`.

```c
/* Two-sequence skeleton (LCS length); dp[i][j] talks about s[i-1], t[j-1] */
int cols = m + 1;
int *dp = calloc((size_t)(n + 1) * cols, sizeof *dp);   /* row 0, col 0 == 0 */
for (int i = 1; i <= n; i++)
    for (int j = 1; j <= m; j++)
        dp[i*cols + j] = (s[i-1] == t[j-1])
                       ? dp[(i-1)*cols + (j-1)] + 1
                       : imax(dp[(i-1)*cols + j], dp[i*cols + (j-1)]);
/* answer: dp[n*cols + m] */

/* Palindrome table by increasing length */
for (int len = 1; len <= n; len++)
    for (int i = 0; i + len - 1 < n; i++) {
        int j = i + len - 1;
        P[i*n + j] = (s[i] == s[j]) && (len < 3 || P[(i+1)*n + (j-1)]);
    }
```

**Worked mini-example** — LCS of `A = "ABC"`, `B = "AC"`:

```
          j=0 ""   j=1 "A"   j=2 "C"
i=0 ""      0        0         0
i=1 "A"     0        1 (diag+1) 1 (max)
i=2 "B"     0        1         1
i=3 "C"     0        1         2 (diag+1)     -> LCS = 2 ("AC")
```

Palindrome table for `"abba"` (rows = `i`, cols = `j`, `.` = not computed):

```
len 1: [0][0]=T [1][1]=T [2][2]=T [3][3]=T
len 2: [0][1] a≠b F   [1][2] b=b T   [2][3] b≠a F
len 3: [0][2] a≠b F   [1][3] b≠a F
len 4: [0][3] a=a && [1][2]=T  -> T
```

Filling row by row (`i` ascending) would read `[1][2]` before it exists when computing `[0][3]`; filling `i` **descending** with `j` ascending also works and is a common alternative to the length loop.

**Reconstruction.** Shortest Common Supersequence and "print the LCS" walk the finished table backwards from `(n, m)`: on a match take the character and go diagonal; otherwise step towards the neighbour that produced the value. The result comes out reversed — reverse it at the end.

**Two-phase DPs.** Palindrome Partitioning II precomputes `isPal[i][j]` (O(n²)), then runs a 1D DP `cuts[i] = min over j<i with isPal[j][i-1] of cuts[j] + 1`, base `cuts[0] = -1` so that a whole-prefix palindrome yields 0. Without the precomputed table the palindrome check inside the inner loop makes it O(n³).

**When you recognise it:** two strings/arrays and "longest common", "edit", "subsequence count", "can they interleave"; one string and "palindrome", "partition into palindromes", "minimum cuts".

**Pitfalls.**
- `s[i]` where `s[i-1]` was meant (table is 1-based, string is 0-based).
- LCS ≠ longest common *substring* — the subsequence may skip characters, which is the whole reason for the `max(up, left)` branch.
- Which string is being edited into which: `up` and `left` are *different* operations (delete vs insert); fix the direction once and keep it.
- Counting (`+`) vs boolean (`||`): Distinct Subsequences adds both branches on a match; Interleaving ORs them.
- `C[i+j-1]` index when `i` or `j` is 0 (Interleaving); quick reject `len(A)+len(B) != len(C)`.
- Expand-around-centre is O(1) space and faster in practice for the *longest* palindrome, but the table generalises to counting and partitioning.

---

## 6. Interval DP

In interval DP the state is a sub-interval `[i, j]` of the original sequence, and the transition splits the interval into two parts according to some **last event** `k ∈ [i, j]` — the last balloon burst in the burst order, or the last diagonal drawn in a polygon triangulation.

**State.** `dp[i][j]` = best (min or max) result when only the range `[i, j]` is treated as an independent subproblem.

**Transition.** `dp[i][j] = best over k of (dp[i][k-1] + cost(i, j, k) + dp[k+1][j])`, where `k` is the "last" choice on this interval and `cost` depends on the problem.

**Base case.** Single-element intervals are often trivial, and the empty interval is the neutral element (cost 0).

**Fill order** is always increasing interval length, because `dp[i][j]` depends on narrower inner intervals. Time complexity is typically O(n³): O(n²) intervals × O(n) split points.

Boundary padding (e.g. `nums[-1] = nums[n] = 1` in balloon bursting) is a common trick: it turns edge cases into ordinary cases without a separate `if` branch. In game-theoretic versions (who wins), the state stores the **difference** between the two players' scores, not either absolute score — this simplifies the transition to a single maximisation instead of two minimisations.

**Why "last", not "first".** If you split on the *first* balloon burst, the two remaining sides are no longer independent — their neighbours change as more balloons go. If `k` is the *last* one burst on the open interval `(i, j)`, then at that moment its neighbours are exactly `i` and `j`, and everything inside `(i, k)` and `(k, j)` was resolved independently beforehand. Choosing the pivot as the last event is what makes the subproblems independent.

**Three flavours of the split:**

| Flavour | Transition | Example |
|---|---|---|
| min/max over a split point | `min_k dp[i][k] + dp[k+1][j] + cost` | matrix chain, triangulation, merge stones |
| "last element" on an open interval | `max_k dp[i][k] + a[i]*a[k]*a[j] + dp[k][j]` | burst balloons |
| two-player difference | `max(a[i] − dp[i+1][j], a[j] − dp[i][j−1])` | predict the winner, stone game |
| minimax worst case | `min_k (k + max(dp[i][k−1], dp[k+1][j]))` | guess number II |

**C layout.** Flat `n × n` (or `(n+2) × (n+2)` with padding) block; only `i <= j` is used, but allocate the full square — the wasted half is worth the simpler indexing. `calloc` gives `dp[i][i] = 0` or the empty-interval 0 for free. For min problems initialise the cells you compute to `INF` *before* the `k` loop, not the whole table.

```c
/* Interval skeleton: fill by length, split at k */
for (int len = 2; len <= n; len++)                 /* len 1 = base case, set before */
    for (int i = 0; i + len - 1 < n; i++) {
        int j = i + len - 1;
        long long best = LLONG_MAX;
        for (int k = i; k < j; k++) {                /* split [i..k] | [k+1..j] */
            long long c = C[i*n + k] + C[(k+1)*n + j] + cost(i, k, j);
            if (c < best) best = c;
        }
        C[i*n + j] = best;
    }
/* answer: C[0*n + (n-1)] */
```

**Worked mini-example** — two-player difference on `a = [3, 9, 1, 2]`, `D[i][j] = max(a[i] − D[i+1][j], a[j] − D[i][j−1])`:

```
len 1: D[0][0]=3  D[1][1]=9  D[2][2]=1  D[3][3]=2
len 2: D[0][1]=max(3-9, 9-3)=6   D[1][2]=max(9-1, 1-9)=8   D[2][3]=max(1-2, 2-1)=1
len 3: D[0][2]=max(3-D[1][2], 1-D[0][1]) = max(3-8, 1-6) = -5
       D[1][3]=max(9-D[2][3], 2-D[1][2]) = max(9-1, 2-8) =  8
len 4: D[0][3]=max(3-D[1][3], 2-D[0][2]) = max(3-8, 2-(-5)) = 7
```

The first player wins by 7 — by taking the **2** first, not the 3. Greedy ("take the larger end") takes 3, the opponent takes 9, and the greedy player loses 5 to 10. This is the whole point of the difference formulation: the `−D[...]` term *is* the opponent's best reply.

**Extra dimensions.** When `[i, j]` alone does not determine the sub-state, add one: `dp[i][j][m]` = cost to merge `[i, j]` into exactly `m` piles (Merge Stones; feasible only if `(n−1) mod (k−1) == 0`), or `dp[i][j][k]` = best score removing `[i, j]` with `k` extra same-coloured boxes already glued to the left of `i` (Remove Boxes, O(n⁴)). These are the hardest problems in the chapter; the extra index encodes *memory* of what was skipped or merged.

**When you recognise it:** "remove/merge/burst elements one at a time, score depends on neighbours", "two players take from either end", "parenthesise / triangulate", "minimum guaranteed cost", "print/paint a string in the fewest strokes". Always: the answer for a range depends on how the range is split.

**Pitfalls.**
- Wrong split bounds: triangulation needs `i < k < j` or a side is counted twice; `k` from `i` to `j−1` for a two-way split.
- Forgetting the padding, then special-casing the ends inside the loop.
- Taking the average or the sum where the problem asks for the *worst case* (`max`) — minimax, not expectation.
- Checking only the *first* `k` with `s[k] == s[i]` (Strange Printer) — every matching `k` must be tried.
- Trusting a mathematical shortcut ("first player always wins") instead of implementing the DP — it does not generalise to the variants.

---

## 7. Bitmask DP: DP over subsets

Bitmask DP uses an integer as the state: if there are at most about 20 elements, every subset fits in one n-bit mask, and `dp[mask]` = best result when exactly the bits in the mask correspond to "used" or "visited" elements.

**State.** `dp[mask]` (or `dp[mask][last]` in route problems) = best value for the subset that `mask` represents. Bit `i` set usually means "element `i` has already been handled".

**Transition.** Iterate over all masks (or add bits one at a time): `dp[mask | (1 << i)] = best(dp[mask] + cost(i, mask))` for every bit `i` not yet set. In route problems the state expands to `dp[mask][last]`, because the cost of the next step depends also on where we are **now**.

**Base case.** `dp[0] = 0` (or `dp[1 << start][start] = 0`) — the empty subset costs nothing.

The number of states is 2ⁿ, and the transition often iterates over all n bits, so the time is typically O(2ⁿ · n). This limits n in practice to about 15–20. The tell-tale sign in a problem statement: a small upper bound on the number of elements combined with each element's state being binary — that is a strong hint towards a bitmask.

**Order.** Iterating `mask` from 0 to `2ⁿ − 1` numerically is a valid topological order for "push" transitions, because `mask | bit > mask` whenever `bit` was clear. For "pull" transitions (`dp[mask] = best over i in mask of dp[mask ^ (1<<i)] + ...`) the same order works because `mask ^ bit < mask`.

**Free information in the mask.** `popcount(mask)` = how many elements have been placed = the *position* currently being filled (Beautiful Arrangement, Maximum Compatibility). Sum of the elements in the mask = the remaining target (Can I Win). Do not store what you can derive — it multiplies memory by n for nothing.

**Which set to mask.** Mask the *small* set. With 60 people and 16 skills, mask the skills (Smallest Sufficient Team); with 12 points in group 1 and 12 in group 2, mask one group and iterate the other with an ordinary index (Connect Two Groups). Same problem, wrong set → 2⁶⁰ states.

**Bit operations you need** (see `../../c_learning/12_numbers_bits_floats/lesson.md`):

| Operation | C | Note |
|---|---|---|
| test bit i | `mask & (1u << i)` | use `1u`, never shift a signed 1 into bit 31 |
| set bit i | `mask \| (1u << i)` | |
| full set | `(1u << n) - 1u` | n ≤ 31 for `unsigned`, use `1ull` for n ≤ 63 |
| popcount | loop `x &= x - 1` or `__builtin_popcount` | builtin is GCC/clang; portable loop in `example.c` |
| lowest set bit | `mask & -mask` | |
| all submasks of `m` | `for (s = m; ; s = (s - 1) & m) { ...; if (!s) break; }` | O(2^popcount(m)); over all `m` it totals O(3ⁿ) |

**C layout.** `int *dp = malloc((1u << n) * sizeof *dp)`; for `dp[mask][last]` flatten to `dp[mask * n + last]`. Initialise to `INF`/`-INF` and skip unreachable masks (`if (dp[mask] >= INF) continue;`). For a boolean "can the mover win" with sparse reachability, memoised recursion over `mask` with a `signed char memo[1 << n]` (0 = unknown, 1 = win, −1 = lose) is simpler than tabulation.

```c
/* dp[mask] assignment skeleton: slot = popcount(mask), item i not yet used */
unsigned full = (1u << n) - 1u;
for (unsigned m = 0; m <= full; m++) dp[m] = INF;
dp[0] = 0;
for (unsigned mask = 0; mask < full; mask++) {
    if (dp[mask] >= INF) continue;
    int slot = popcount32(mask);
    for (int i = 0; i < n; i++) {
        if (mask & (1u << i)) continue;
        unsigned nm = mask | (1u << i);
        dp[nm] = imin(dp[nm], dp[mask] + cost[slot * n + i]);
    }
}
/* answer: dp[full] */
```

**Worked mini-example** — 3 slots × 3 items, `cost[slot][item]` = `[[4,2,8],[4,3,7],[3,1,9]]`:

```
mask 000 (slot 0): dp[001]=4  dp[010]=2  dp[100]=8
mask 001 (slot 1): dp[011]=4+3=7      dp[101]=4+7=11
mask 010 (slot 1): dp[011]=min(7,2+4)=6  dp[110]=2+7=9
mask 100 (slot 1): dp[101]=min(11,8+4)=11 dp[110]=min(9,8+3)=9
mask 011 (slot 2): dp[111]=6+9=15
mask 101 (slot 2): dp[111]=min(15,11+1)=12
mask 110 (slot 2): dp[111]=min(12,9+3)=12        -> answer 12
```

3! = 6 permutations were covered with 2³ = 8 states — the saving grows to 20! vs 2²⁰ ≈ 10⁶ at n = 20.

**BFS over `(mask, node)`.** When edges are unit weight (Shortest Path Visiting All Nodes), BFS layer by layer over the `(mask, node)` state space is the DP: seed the queue with *every* `(1 << i, i)` at distance 0, stop at the first state with `mask == full`. The `node` dimension is required because the next step's cost depends on where you stand — exactly the TSP `dp[mask][last]` idea with an unweighted graph.

**When you recognise it:** n ≤ ~20 and each element is either used or not; "visit all nodes", "assign each X to a distinct Y", "partition into k equal groups", "cover all required skills", "game where numbers are used once". Also: your backtracking solution revisits the same "set of used items" from many different orders — that is the memoisation hint.

**Pitfalls.**
- `1 << 31` on a signed `int` is undefined; `1 << n` with `n = 32` is undefined. Use `1u`/`1ull` and check the bound.
- Storing a derivable quantity (position, remaining sum) as a table dimension.
- Masking the wrong (large) set.
- Seeding the BFS from node 0 only instead of from every node.
- "Is there a subset with sum `side`" is *not* enough for four equal sides (Matchsticks): the four sides must be filled simultaneously from disjoint sticks, so `dp[mask]` = remaining room in the *current* side, `(dp[mask] − stick) mod side`.
- Backtracking without memo on `mask` explores the same state once per arrival order — exponential in n! instead of 2ⁿ.

**Python equivalent:** `@lru_cache(None) def f(mask): ...`; `bin(mask).count("1")` or `mask.bit_count()` for popcount. Python ints are unbounded, so the `1u` shift issue does not exist there — it does in C.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | State | Time / space |
|---|---|---|---|
| "number of ways to reach step / decode", "no two adjacent", "max sum subarray" | 1D DP | `dp[i]` over prefix or ending-at-i | O(n) / O(1) |
| "longest increasing subsequence" | 1D DP (all `j<i`) or patience sorting | `dp[i]` ending at i / `tails[]` | O(n²) or O(n log n) / O(n) |
| "buy/sell", "hold", "cooldown", "fee", "at most k transactions", "circular houses" | State machine | `dp[i][mode]`, `dp[i][k][mode]` | O(n·S) or O(nK) / O(S) or O(K) |
| "fewest coins / squares", "can you make exactly", "equal partition", "± signs", "two piles" | Knapsack / subset sum | `dp[w]` | O(n·W) / O(W) |
| "count combinations" vs "count ordered sequences" | Unbounded knapsack, loop nesting | `dp[w]` | O(n·W) / O(W) |
| "two budgets (zeros and ones)" | 2D 0/1 knapsack | `dp[a][b]` | O(L·m·n) / O(m·n) |
| "grid, move right/down", "obstacles", "min path sum", "largest square of 1s" | Grid DP | `dp[y][x]` | O(HW) / O(W) |
| "exactly k moves", "leave the grid" | Grid DP + step dimension | `dp[k][y][x]` | O(kHW) / O(HW) |
| "minimum initial health", "triangle bottom to top" | Grid DP backwards | `dp[y][x]` from goal | O(HW) / O(W) |
| "go there and back", "two robots" | Grid DP two walkers | `dp[t][x1][x2]` | O(n³) / O(n²) |
| "two strings, longest common / edit / interleave / count subsequences" | Two-sequence DP | `dp[i][j]` prefixes | O(nm) / O(nm) or O(m) |
| "palindromic substring(s)", "minimum palindrome cuts" | Interval over one string (+1D) | `P[i][j]`, `cuts[i]` | O(n²) / O(n²) |
| "burst / merge / remove with neighbour-dependent score", "parenthesise", "triangulate" | Interval DP, last event k | `dp[i][j]` | O(n³) / O(n²) |
| "two players take from ends", "guaranteed cost" | Interval DP difference / minimax | `dp[i][j]` | O(n²) or O(n³) / O(n²) |
| "exactly k consecutive piles", "boxes with same colour glued" | Interval DP + extra dimension | `dp[i][j][m]` | O(n³/k), O(n⁴) / O(n²·k) |
| "n ≤ 16–20", "visit all", "assign each to a distinct", "used once", "k equal groups", "cover all skills" | Bitmask DP | `dp[mask]`, `dp[mask][last]` | O(2ⁿ·n) or O(2ⁿ·n²) / O(2ⁿ) |

---

## Gotchas in C specifically

- **Table allocation.** No VLAs for DP tables (`int dp[n][m]` with runtime `n, m` blows the 8 MB main-thread stack at ~1.4 M ints, and 512 KB on secondary threads much sooner). Use `malloc`/`calloc` and a **flat** index `dp[y * W + x]`; free it. `calloc` zeroes the memory — which is the base case for counting and LCS tables, but *not* for min tables (`INF`) or `-1` sentinels. See `../../c_learning/06_dynamic_memory/lesson.md`.
- **`memset` only works for 0 and −1.** `memset(dp, 0xFF, bytes)` sets every `int`/`long long` to −1 (two's complement); `memset(dp, 1, bytes)` gives `0x01010101`, not 1. Use a loop for `INF`.
- **INF that survives addition.** `INT_MAX + 1` is undefined behaviour and in practice wraps negative. Use `INF = INT_MAX / 2` (or `LLONG_MAX / 4`) and guard `if (dp[prev] >= INF) continue;` before adding costs. Symmetrically `-INF` for max problems.
- **Counts overflow `int`.** Ways-to-reach sequences pass 2³¹ around n ≈ 46 (Fibonacci-like) and much sooner for grids/paths. Use `long long`, and when a modulus `MOD = 1000000007` is given, reduce after **every** addition: `dp[i] = (dp[i-1] + dp[i-2]) % MOD`. Two values `< MOD` sum to `< 2·MOD < 2³¹`, so `int` is fine for sums but a product needs `long long` before `%`.
- **Negative modulo.** In C, `-7 % 3 == -1`. When subtracting under a modulus write `((a - b) % MOD + MOD) % MOD`.
- **`s[i-1]` not `s[i]`.** A `(n+1) × (m+1)` table is 1-based; the string is 0-based. Write the invariant as a comment above the loop.
- **Bit shifts.** `1 << 31` on `int` is UB; `1 << n` with `n >= 32` is UB. Use `1u << i` for masks up to 32 bits and `1ull` for 64; `unsigned` loop variables for masks (`for (unsigned mask = 0; mask < full; mask++)`), and `full = (1u << n) - 1u`. See `../../c_learning/12_numbers_bits_floats/lesson.md`.
- **`__builtin_popcount`** is GCC/clang only; a portable `while (x) { x &= x - 1; c++; }` is in `example.c`. C23 has `stdc_count_ones` in `<stdbit.h>`; C11 does not.
- **Recursion depth for memoisation.** Each frame is tens to hundreds of bytes; 10⁵ nested calls is borderline, 10⁶ will segfault. If the recursion depth is O(n) with n ≥ 10⁵, convert to bottom-up. Top-down over `mask` (Can I Win) has depth ≤ n, so it is safe.
- **Temporaries in state machines.** `hold = max(hold, cash - p); cash = max(cash, hold + p);` uses the *new* `hold` in the second line. Compute into `new_hold`, `new_cash`, then assign — or prove the order is harmless for that specific problem.
- **In-place rolling rows.** For grid DP, `row[x] += row[x-1]` is correct only because "above" is the old `row[x]` and "left" is the new `row[x-1]`. For knapsack, the same in-place trick is *why* the loop direction matters.
- **`bool` from `<stdbool.h>`**; a `bool` array uses one byte per cell — for `n·n` palindrome tables at n = 10⁴ that is 100 MB. Either pack bits or switch to expand-around-centre.
- **Reconstruction needs the full table.** If the problem asks *which* items / *which* string, keep the 2D table (or a separate parent array) — the O(W) rolled version has thrown the information away.
- **`strlen` in a loop condition** is O(n) each time. Compute `n = (int)strlen(s)` once.
- **Debug by printing the table.** On a 5–8 element input, print `dp` row by row and check it against your hand trace. Most DP bugs are visible in the first two rows. See `../../c_learning/13_debugging_testing_perf/lesson.md`.

---

## Common mistakes checklist

- [ ] The state is written down in one precise sentence, including "ending exactly at" vs "anywhere in the prefix".
- [ ] Base cases are set *before* the loop, and `dp[0]` is 1 for counting / `true` for feasibility / 0 for min-cost / `-INF` for "impossible" max.
- [ ] The answer cell is correct: last cell vs `max` over all cells vs `dp[full_mask]`.
- [ ] 0/1 knapsack: capacity loop **descending**. Unbounded: **ascending**. Two capacities: both descending.
- [ ] Counting ways: item-outer for combinations, sum-outer for permutations — and the problem statement was read to decide which one it wants.
- [ ] Interval DP: filled by increasing length; split bounds `i ≤ k < j` (or `i < k < j` for triangulation); padding added for open intervals.
- [ ] String DP: `dp[i][j]` ↔ `s[i-1]`, `t[j-1]`; table is `(n+1) × (m+1)`.
- [ ] State machine: new values computed from old values via temporaries; fee/cooldown counted exactly once.
- [ ] Grid DP: first row/column handled (cumulative, obstacle-aware) or replaced by a sentinel border; direction reversed when the value depends on the future.
- [ ] Bitmask: `1u <<`, `unsigned` masks, small set masked, derivable quantities (popcount, sum) not stored.
- [ ] `long long` for counts; `% MOD` on every addition; `INF = INT_MAX / 2`.
- [ ] Quick rejections before the DP (odd sum, `|target| > sum`, `(n−1) mod (k−1) != 0`, `len(A)+len(B) != len(C)`).
- [ ] Space reduction only applied when the transition has a fixed backward window and no reconstruction is needed.
- [ ] Tested on: empty input, `n = 1`, all-negative / all-zero arrays, an input where greedy is wrong (e.g. `[2, 7, 9, 3, 1]`, coins `{1, 3, 4}` for 6, squares for 12).
- [ ] Every `malloc`/`calloc` checked and freed; `cc -Wall -Wextra` clean.

---

## You can move on when...

- You can, for any problem in `problems.md`, write the state / transition / base / order / answer in five lines *before* opening the hint, and your table on a 5-element input matches a hand trace.
- You can explain in one sentence each why: 0/1 knapsack goes descending; combinations need item-outer; interval DP splits on the *last* event; the stock machine needs a `sold` node for cooldown; `dp[mask][last]` needs `last`.
- You have implemented, in C, with `cc -Wall -Wextra -std=c11` clean: one rolling 1D DP, one 2-state machine, both knapsack directions, one flat-2D grid DP, one `(n+1)×(m+1)` string DP with reconstruction, one interval DP by length, and one `dp[mask]` — and you can point to each of them in `example.c` and say what would break if the loop order were flipped.
- You know which of your DPs can be rolled to O(1)/O(W) space and which cannot (LIS O(n²), interval, anything that reconstructs).
- You can look at a Viterbi, DTW or edit-distance implementation in a library and name its state, transition and base case.
