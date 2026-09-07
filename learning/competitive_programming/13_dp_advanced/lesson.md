# Chapter 13 — Advanced dynamic programming

Prerequisite: `../../algorithms_learning/07_dynamic_programming/lesson.md` (1D recurrences, take/skip
state machines, 0/1 and unbounded knapsack, grid DP, two-string DP, interval DP, `dp[mask]`). That
chapter taught the *shapes*. This one teaches the contest layer: how to design a state under
pressure, the classic families at full depth (tree, digit, broken profile), and the five
optimizations that turn an O(n²) or O(n³) recurrence into something that passes — Knuth, divide &
conquer, convex hull trick, slope trick, Aliens. Reference library: `example.cpp` (every section
has a cross-checked snippet there).

## What you'll be able to do after this chapter

- Write a DP as five lines of English (state, transition, order, base, answer) before touching the
  keyboard, and shrink a state that is too big by dropping a coordinate that is derivable or by
  moving it into the value.
- Implement from memory: bounded knapsack in O(W · Σ log cᵢ), 0/1 subset-sum with `bitset`, LIS in
  O(n log n) with reconstruction, edit distance in O(min(n,m)) memory, LCS with parent pointers.
- Solve tree DPs with per-vertex tables and *prove* that subtree merging is O(n²) total.
- Write a digit DP with `tight`/`started` flags in ten minutes; write a broken-profile DP for
  tilings; write a pair-state bitmask DP (Elevator Rides).
- Recognise when `opt[i][j]` is monotone and apply Knuth (O(n²)) or divide & conquer
  (O(k n log n)); recognise `dp[j] + b[j]·a[i]` and apply the convex hull trick (deque or Li Chao);
  recognise convex piecewise-linear cost functions and apply slope trick; recognise an "exactly k"
  constraint over a convex cost and apply the Aliens trick.
- Speed up a transition with a Fenwick/segment tree, a KMP/Aho–Corasick automaton, or SOS.
- Debug a DP by printing the table on a tiny input and by stress-testing against brute force.

## Where this shows up in contests

| Signal in the statement | Family | Typical placement |
|---|---|---|
| n ≤ 5000 with two indices, or n ≤ 10⁶ with one | plain 1D/2D DP | Div2 C–D, IOI subtask 1–3 |
| "count the number of ways … modulo 10⁹+7" | counting DP | Div2 C–E |
| n ≤ 20 or "each element is chosen once, order matters" | bitmask DP | Div2 D, IOI subtask 2–3 |
| grid with one dimension ≤ 10 | broken-profile DP | Div2 E, BOI |
| "numbers between a and b whose digits …" | digit DP | Div2 D–E |
| a tree and a per-vertex choice | tree DP / rerooting | Div2 D–E, IOI subtask 3–4 |
| n ≤ 5000 interval merging with a cost that is a sum | Knuth | Div1 C |
| "split into exactly k groups", n ≤ 10⁵, k ≤ 100 | D&C optimization | Div1 C–D, JOI |
| "split into exactly k groups", n ≤ 10⁶, k arbitrary | Aliens trick | IOI 2016 Aliens, Div1 D–E |
| transition of the form `dp[j] + a[i]·b[j]` | convex hull trick | Div1 C–D, BOI/CEOI |
| "minimum total change to make the array non-decreasing / satisfy …" | slope trick | Div1 D, JOI |
| transition is a range query over earlier states | DP + segment tree | Div2 E, Div1 B–C |

IOI problems rarely say "DP"; they give a state space through a story and a subtask ladder where
subtask 1 is brute force, subtask 3 is the O(n²) DP and subtask 5 is the optimised one. Train
yourself to write the O(n²) version *first*, keep it as the checker, and optimise second.

---

## 0. The design method

Every DP is the same object: a directed acyclic graph of states, values on states, values computed
from predecessor values. Five questions, in this order:

| # | Question | Written form |
|---|---|---|
| 1 | **State** — what does `dp[...]` mean, in a full sentence including what is fixed? | "`dp[i][j]` = min cost to process the first `i` items with exactly `j` groups closed" |
| 2 | **Transition** — from which states, combined how? | `dp[i][j] = min_{k<i} dp[k][j-1] + cost(k, i)` |
| 3 | **Order** — which iteration order guarantees dependencies are final? | outer `j` ascending, inner `i` ascending |
| 4 | **Base** — which states cannot use the formula? | `dp[0][0] = 0`, everything else `+INF` |
| 5 | **Answer** — which cell(s)? | `dp[n][k]` (not `min` over `j`, since exactly k) |

Complexity = (#states) × (cost of one transition). Write both numbers down before coding. If the
product exceeds ~10⁸ simple operations, one of the two must shrink.

**Shrinking a state.** Techniques, most common first:

1. *Drop a derivable coordinate.* If `j` is always `i - something(i)`, remove `j`. If the total
   sum is fixed, one of the two partial sums is redundant.
2. *Move a coordinate into the value.* Instead of `dp[i][j] = feasible?` (bool, O(n·m) states),
   use `dp[i] = min j such that feasible` (O(n) states). Works when feasibility is monotone in `j`.
   Example: "Elevator Rides" stores `(rides, weight-of-last-ride)` as the value instead of putting
   the weight into the state.
3. *Exploit small parameters.* If one parameter is ≤ 20, it is a bitmask; if ≤ 10, it is a profile.
4. *Reparametrize by the answer.* Binary search the answer and turn the DP into a feasibility
   check (often removes a dimension).
5. *Recognise that the transition only looks a fixed distance back* → rolling arrays (memory,
   not time).

**Pull vs push.** *Pull*: `dp[i] = f(dp[j] for j in preds(i))` — compute a state from its
predecessors; natural when predecessors are easy to enumerate. *Push*: for each finished state
`j`, update all successors `dp[i] = min(dp[i], dp[j] + w)` — natural when successors are easy to
enumerate (knapsack "add this item", automaton "read this letter"). Push requires that `j` is
final when pushed; in a layered DP that is automatic. Counting DP in push form is the same
thing as multiplying by a transition matrix one row at a time.

**Top-down vs bottom-up.** Memoised recursion visits only reachable states (digit DP, sparse state
spaces) and gets the order for free; it costs recursion depth (≤ ~10⁵ frames safe on most judges,
more with `ulimit -s`/pragma) and a constant factor. Bottom-up is faster and rolling-array friendly.
Default: bottom-up when the state space is a dense box, top-down when it is sparse or the order
is unclear.

---

## 1. Coin and knapsack variants

The four basic loops (from ch07, one line each so you never confuse them again):

| Problem | Loop order | Why |
|---|---|---|
| unbounded, min coins / count *ordered* sequences (Dice Combinations, Coin Combinations I) | sums ascending, coins inside | a coin may be reused on a sum that already used it |
| unbounded, count *unordered* combinations (Coin Combinations II) | coins outside, sums ascending inside | each coin type is "decided" once, so `{1,3}` and `{3,1}` are one state path |
| 0/1 (Book Shop, Two Sets II, Money Sums) | items outside, capacity **descending** | reading `dp[cap-w]` must see the row *before* this item |
| bounded (cᵢ copies) | binary-split, then 0/1 | see below |

**Bounded knapsack via binary splitting.** An item with count `c` becomes items with
multiplicities `1, 2, 4, …, 2^(t-1), c - (2^t - 1)`. Every quantity `0..c` is a subset sum of these
(the powers of two represent `0..2^t-1`, the remainder shifts the range to reach `c`), and no
subset exceeds `c`. Items: Σ ⌈log cᵢ⌉; total O(W · Σ log cᵢ). This replaces O(W · Σ cᵢ) and is
what you reach for when counts are ≤ 10⁹. (The monotone-deque O(nW) method exists; binary
splitting is what you'll actually write in a contest.)

```cpp
ll bounded_knapsack(const vector<array<ll,3>>& items /*w,v,cnt*/, int W) {
    vector<pair<ll,ll>> split;
    for (auto [w, v, cnt] : items)
        for (ll k = 1; cnt > 0; k <<= 1) { ll take = min(k, cnt); split.push_back({w*take, v*take}); cnt -= take; }
    vector<ll> dp(W + 1, 0);
    for (auto [w, v] : split)
        for (int cap = W; cap >= w; cap--) dp[cap] = max(dp[cap], dp[cap - w] + v);
    return dp[W];
}
```

**0/1 subset sum with `bitset`.** Reachability, not value: `reach |= reach << w`. Each shift-or is
O(W/64). Money Sums (n ≤ 100, sum ≤ 10⁵) becomes 100 × 1600 word operations. Two Sets II asks for
the *count* modulo a prime, so bitset does not apply — that is the plain 0/1 counting DP over
sums up to n(n+1)/4, with the answer halved (each partition is counted twice) via the modular
inverse of 2.

```cpp
bitset<100001> reach; reach[0] = 1;
for (int w : a) reach |= reach << w;
```

Pitfalls: `bitset` size is compile-time — declare it at the maximum constraint; `reach << w` with
`w ≥ N` is defined (all zeros), fine. For the value version (Book Shop, n·x = 10³·10⁵ = 10⁸) use
`int` not `long long` in the row and iterate capacity descending; 10⁸ simple ops pass in ~0.3 s.

---

## 2. LIS in O(n log n) with reconstruction

State compression by *moving the index into the value*: instead of `dp[i]` = LIS ending at `i`
(O(n²) to fill), keep `tail[k]` = the smallest possible last element of an increasing subsequence
of length `k+1` among the prefix seen so far.

**Invariant.** `tail` is strictly increasing. Proof: if `tail[k] ≥ tail[k+1]`, take the length-
`k+2` subsequence ending at `tail[k+1]` and drop its last element: a length-`k+1` subsequence ending
at a value `< tail[k+1] ≤ tail[k]`, contradicting minimality of `tail[k]`.

**Step.** For `a[i]`, `k = lower_bound(tail, a[i])`: the longest subsequence that `a[i]` can extend
has length `k` (its tail `< a[i]`), so `a[i]` produces a length-`k+1` subsequence ending at `a[i]`,
which is better than or equal to the current `tail[k]` (which is `≥ a[i]`). Set `tail[k] = a[i]`. If
`k == size`, append. The predecessor of `i` is whatever index currently owns `tail[k-1]` — record
it at this moment, it is never revised.

```cpp
vector<int> lis_indices(const vector<ll>& a) {
    int n = a.size();
    vector<ll> tailVal; vector<int> tailIdx, prv(n, -1);
    for (int i = 0; i < n; i++) {
        int k = lower_bound(tailVal.begin(), tailVal.end(), a[i]) - tailVal.begin();
        if (k == (int)tailVal.size()) { tailVal.push_back(a[i]); tailIdx.push_back(i); }
        else { tailVal[k] = a[i]; tailIdx[k] = i; }
        prv[i] = k ? tailIdx[k-1] : -1;
    }
    vector<int> res;
    for (int i = tailIdx.back(); i != -1; i = prv[i]) res.push_back(i);
    reverse(res.begin(), res.end());
    return res;
}
```

Trace on `a = 3 1 4 1 5 9 2 6`:

```
i  a[i]  k   tailVal after      prv[i]
0   3    0   [3]                 -
1   1    0   [1]                 -
2   4    1   [1 4]               1
3   1    0   [1 4]               -      (replaces itself, harmless)
4   5    2   [1 4 5]             2
5   9    3   [1 4 5 9]           4
6   2    1   [1 2 5 9]           1      (2 replaces 4 -- future 3's can extend)
7   6    3   [1 2 5 6]           4      (prv = index owning tail[2] = 4)
answer: from tailIdx.back()=7: 7 -> 4 -> 2 -> 1  => indices 1 2 4 7 = values 1 4 5 6
```

Variants: non-decreasing → `upper_bound`. "Number of LIS" → `dp[i]` counts with a Fenwick over
values (section 15). Minimum number of decreasing subsequences to cover the array = LIS length
(Dilworth). Longest chain of pairs `(x,y)` with both strictly increasing: sort by `x` asc, `y`
**desc** for ties, LIS on `y`.

Pitfall: `tailVal` values are not the subsequence; only `prv` gives the subsequence.

---

## 3. Two-string DP: edit distance, LCS, and memory

Edit distance `dp[i][j]` over prefixes `a[0..i)`, `b[0..j)` depends only on row `i-1` and the
current row → two rolling rows, O(min(n,m)) memory (swap the strings so the shorter one is the
column). Time stays O(nm): n = m = 5000 → 2.5·10⁷, fine.

```cpp
int edit_distance(const string& a, const string& b) {
    int n = a.size(), m = b.size();
    vector<int> prev(m + 1), cur(m + 1);
    iota(prev.begin(), prev.end(), 0);
    for (int i = 1; i <= n; i++) {
        cur[0] = i;
        for (int j = 1; j <= m; j++)
            cur[j] = min({prev[j] + 1, cur[j-1] + 1, prev[j-1] + (a[i-1] != b[j-1])});
        swap(prev, cur);
    }
    return prev[m];
}
```

LCS with reconstruction keeps the full table (`n·m` ints; 5000² × 4 B = 100 MB — too much; use
`short`/`uint16_t` if lengths ≤ 65535, or store the table as the *direction* only in 2 bits). Walk
back from `(n,m)`: match → diagonal; else move toward the larger neighbour.

**Hirschberg's trick** (mention; rarely needed but the idea is a classic): compute LCS length of
the top half forward and of the bottom half backward in O(m) memory each, find the column where
their sum is maximal, recurse on the two sub-rectangles. O(nm) time, O(n+m) memory,
reconstruction included. Same idea underlies "find where an optimal path crosses the middle
row" in many grid DPs.

Pitfall: when rolling, the `cur[j-1]` term must be read from the *current* row and `prev[j-1]` from
the previous — writing `dp[j-1]` into a single array before it is read for `dp[j]` is the classic
bug; the two-array version avoids it.

---

## 4. Counting DP with modular arithmetic

The recurrence is the same as for optimisation with `+` replacing `min` and `×` replacing `+`
along a path. Rules that actually cause WA:

- Reduce after every addition, `dp[s] = (dp[s] + dp[s-c]) % MOD` — with `long long` you may skip
  one reduction after a sum of two reduced values, never after a product of two.
- Subtraction: `(x - y + MOD) % MOD`.
- Division: multiply by the modular inverse (`pow(y, MOD-2)`), only for prime MOD.
- Products of two values `< 2³¹` need `long long`.
- "Count ordered vs unordered" is a loop-order question (section 1), not a formula question.

**Array Description** (values `0..m`, `0` = unknown, adjacent differ by ≤ 1): `dp[i][v]` = number
of ways for prefix `i` with `a[i] = v`; `dp[i][v] = dp[i-1][v-1] + dp[i-1][v] + dp[i-1][v+1]` if
`a[i] ∈ {0, v}` else 0. O(nm) = 10⁵ · 100. **Counting Towers** (2×n tower of blocks): state = whether
the top row is one 2-wide block or two 1-wide blocks; 2 states, transitions counted by hand
(`dp[n][wide] = 2·dp[n-1][wide] + dp[n-1][split]`, `dp[n][split] = dp[n-1][wide] + 4·dp[n-1][split]`).
**Removal Game** is a game DP, not a counting DP: `dp[l][r]` = best score difference for the player
to move on `[l, r]`; `dp[l][r] = max(a[l] - dp[l+1][r], a[r] - dp[l][r-1])`; the answer is
`(total + dp[0][n-1]) / 2`. **Mountain Range**: longest path in the DAG "jump to a higher peak
that sees me" — a DP over sorted heights, O(n²) with a monotonic stack to find visibility.

---

## 5. Tree DP

A tree DP is a bottom-up DFS where `dp[v]` is a small table computed from the children's tables.
The state almost always has the form `dp[v][what v does]`; the parent's choice is resolved at the
parent.

**Maximum weight independent set.** `dp[v][0]` = best in subtree with `v` not taken, `dp[v][1]` =
taken. `dp[v][0] = Σ max(dp[c][0], dp[c][1])`, `dp[v][1] = w[v] + Σ dp[c][0]`.

**Tree Matching** (CSES 1130): `dp[v][0]` = max matching in subtree with `v` unmatched to a child;
`dp[v][1]` = `v` matched to some child. `dp[v][0] = Σ best(c)`, and for `dp[v][1]` try each child:
`dp[v][0] - best(c) + dp[c][0] + 1`. O(n). (Greedy — match every leaf to its parent, bottom-up —
also works; the DP shape generalises.)

**Subtree knapsack merging and the O(n²) proof.** `f[v]` is a vector indexed by "how many vertices
chosen in v's subtree" (or any size-bounded quantity). Merging child `c` into `v` costs
`|f[v]| · |f[c]|` where sizes are the current subtree sizes. Claim: Σ over all merges = O(n²).
Proof: charge each pair of vertices `(x, y)` with `x` in the already-merged part of `v` and `y` in
`c`'s subtree to the merge step; every unordered pair of vertices is charged exactly once, at the
step where their subtrees are joined (i.e. at their LCA). There are n(n-1)/2 pairs. ∎ The same
argument gives O(n·k) when tables are truncated at size `k` (min(size, k) bounds on each side —
the charging then counts pairs with rank ≤ k, still ≤ n·k).

```cpp
// f[v][k] = number of connected vertex sets of size k whose topmost vertex is v
void dfs(int v, int p) {
    f[v] = {0, 1};
    for (int c : adj[v]) if (c != p) {
        dfs(c, v);
        vector<ll> g(f[v].size() + f[c].size() - 1, 0);
        for (size_t i = 1; i < f[v].size(); i++) {
            g[i] += f[v][i];                                     // take nothing from c
            for (size_t j = 1; j < f[c].size(); j++) g[i + j] += f[v][i] * f[c][j];
        }
        f[v] = move(g);
    }
}
```

Note the merge writes into a fresh vector `g` of the combined size — merging in place would
double-count. Pitfall: `f[v].size()` grows; keep loops over `size_t` or cast, and reserve nothing
(the proof already bounds the copying).

**Rerooting** (all-roots answers in O(n)): compute `down[v]` bottom-up, then `up[v]` top-down with
prefix/suffix aggregates over siblings. Covered with the tree chapter; here just remember the
shape — if the problem asks the tree DP value "for every root", it is rerooting.

Pitfalls: recursion depth n = 2·10⁵ on a path graph — either raise the stack or write iterative
DFS (compute an order, then process in reverse). Use `long long` for sums of weights.

---

## 6. Digit DP

Count integers in `[0, N]` whose decimal string has a property that can be checked left to right
with bounded memory. State: `(pos, memory, tight, started)`.

- `tight` = the prefix built so far equals `N`'s prefix; then the next digit is bounded by `N[pos]`.
  Once a digit is strictly smaller, `tight` is false forever. Only *one* tight path exists per
  position, so the memo may ignore `tight` entirely (memoise only non-tight states).
- `started` = a nonzero digit has been placed; before that, leading zeros must not count as digits
  (they would break "no two adjacent equal digits" and "digit sum" rules alike).
- `memory` = whatever the property needs: previous digit, digit sum mod k, bitmask of used digits.

`[a, b]` = `count(b) - count(a-1)`.

```cpp
ll count_no_adjacent_equal(ll N) {           // Counting Numbers (CSES 2220), one bound
    string s = to_string(N); int L = s.size();
    vector<vector<array<ll,2>>> memo(L, vector<array<ll,2>>(11, {-1,-1}));
    function<ll(int,int,bool,bool)> go = [&](int pos, int prev, bool started, bool tight) -> ll {
        if (pos == L) return 1;
        if (!tight && memo[pos][prev][started] != -1) return memo[pos][prev][started];
        int hi = tight ? s[pos]-'0' : 9; ll res = 0;
        for (int d = 0; d <= hi; d++) {
            bool ns = started || d != 0;
            if (ns && d == prev) continue;
            res += go(pos+1, ns ? d : 10, ns, tight && d == hi);
        }
        if (!tight) memo[pos][prev][started] = res;
        return res;
    };
    return go(0, 10, false, true);
}
```

Trace, `N = 21`, digits `2 1`:

```
go(0, prev=none, started=0, tight=1): d in 0..2
  d=0 -> go(1, none, 0, 0): d in 0..9, all allowed (not started or started with prev=none) -> 10  [numbers 0..9]
  d=1 -> go(1, 1, 1, 0):   d in 0..9 except 1 -> 9                                              [10,12..19]
  d=2 -> go(1, 2, 1, 1):   d in 0..1, neither equals 2 -> 2                                      [20, 21]
total 21  (the only excluded number <= 21 is 11)
```

States: `L × 11 × 2 = 19 × 22` for N < 10¹⁸; transitions ×10. Trivially fast; the difficulty is
always in the state design. Pitfalls: `N = 0` and negative lower bound (`a - 1` when `a = 0`);
counting the number 0 (the all-zeros path) — decide whether the statement includes it; `long
long` for N up to 10¹⁸.

---

## 7. Bitmask DP

`dp[mask]` over subsets of ≤ 20–22 elements. 2²⁰ ≈ 10⁶ states; with an O(n) transition, 2·10⁷.
Iterating `mask` in increasing numeric order is a valid topological order because every proper
subset is numerically smaller.

**TSP / Hamiltonian Flights** (CSES 1690): `dp[mask][v]` = number of (or min-cost) paths visiting
exactly `mask`, ending at `v`. O(2ⁿ · n²) with dense edges, O(2ⁿ · m) pushing along edges.
Hamiltonian Flights wants paths from 1 to n visiting all: forbid reaching `n` before `mask` is
full.

**Assignment** (n tasks to n workers): `dp[mask]` = min cost after assigning workers `0..popcount-1`
to the task set `mask`; the worker index is derivable from the mask → 1D state.

**Elevator Rides** (CSES 1653) — the pair-state trick: the natural state `dp[mask][weight]` is too
big; instead `dp[mask] = (rides, weight of the last ride)`, minimised lexicographically. Fewer
rides is always better; among equal rides, less weight in the last ride is always better
(exchange argument: any continuation from the heavier state is available from the lighter one).

```cpp
vector<pair<int,ll>> dp(1 << n, {INT_MAX, 0}); dp[0] = {1, 0};
for (int mask = 1; mask < (1 << n); mask++)
    for (int p = 0; p < n; p++) if (mask >> p & 1) {
        auto [r, last] = dp[mask ^ (1 << p)];
        auto cand = last + w[p] <= cap ? make_pair(r, last + w[p]) : make_pair(r + 1, w[p]);
        dp[mask] = min(dp[mask], cand);
    }
```

**Broken profile / plug DP — Counting Tilings** (CSES 2181, n ≤ 10 rows, m ≤ 1000 columns).
Process cells one at a time in column-major order. The profile is the set of cells in the *next*
n positions that are already covered by a domino that started earlier. At cell `(i, j)`:

```
mask bit i set  -> cell already covered: clear bit i, move on
else            -> horizontal domino (i,j)-(i,j+1): set bit i (the cell in the next column is pre-covered)
                -> vertical domino (i,j)-(i+1,j): needs i+1 < n and bit i+1 clear; set bit i+1
```

One `dp` array of size 2ⁿ per *cell*, so O(n·m·2ⁿ) = 10 · 1000 · 1024 ≈ 10⁷. The equivalent
column-at-a-time formulation (`dp[j][mask]`, transition over compatible mask pairs) is O(m · 4ⁿ)
naive or O(m · 3ⁿ) with submask enumeration — the cell-at-a-time version is both simpler and
faster.

```
2x3 grid, tilings = 3.  Profile bits refer to the column being entered.
col0 cell0: mask 00 -> horiz => 01 ;  vert => 10 (bit1 marks (1,0) covered)
col0 cell1: 01: bit1 clear -> horiz => 11        10: bit1 set -> clear => 00
col1 cell0: 11: bit0 set -> clear => 10          00: horiz => 01 | vert => 10
... dp[0] at the end = 3
```

Pitfalls: mixing the two formulations; forgetting that a vertical domino also needs the *next*
cell free; using `int` for 2ⁿ·n arrays (memory: `dp[1<<20][20]` of `long long` = 160 MB — use
`int` or reduce).

---

## 8. DP on DAGs (pointer)

Longest/shortest/counting paths on a DAG is a DP in topological order (`../../algorithms_learning/06_graphs/lesson.md`
§4 and the graph chapter of this course). The reverse view matters here: **every DP is a DAG
shortest path**, so when a DP has an unclear order, compute a topological order of its state graph
(or memoise). "Game Routes", "Longest Flight Route", and every layered DP are this.

---

## 9. Interval DP and the Knuth optimization

Interval DP `dp[l][r] = min_{l ≤ k < r} dp[l][k] + dp[k+1][r] + w(l, r)` is O(n³). Knuth's
optimization brings it to O(n²) when `w` satisfies

1. **Quadrangle inequality (QI):** `w(a,c) + w(b,d) ≤ w(a,d) + w(b,c)` for `a ≤ b ≤ c ≤ d`.
2. **Monotonicity on nested intervals:** `w(b,c) ≤ w(a,d)` for `a ≤ b ≤ c ≤ d`.

Then (Yao 1980) `dp` itself satisfies QI, and the optimal split point is monotone:
`opt[l][r-1] ≤ opt[l][r] ≤ opt[l+1][r]`. Iterating `k` only over that window makes the total work
telescope: for fixed length, Σ_l (opt[l+1][r] − opt[l][r−1] + 1) ≤ n + (opt[n−len+1][n] − opt[0][len−1]) ≤ 2n.
Σ over lengths = O(n²).

`w(l, r) = Σ a[l..r]` (merging adjacent piles, optimal BST, Knuth Division CSES 2088) satisfies both
with equality in QI. Squared range sums do **not** satisfy monotonicity in general — check both
conditions or stress-test.

```cpp
for (int i = 0; i < n; i++) opt[i][i] = i;
for (int len = 2; len <= n; len++)
    for (int l = 0; l + len - 1 < n; l++) {
        int r = l + len - 1; dp[l][r] = INF;
        for (int k = opt[l][r-1]; k <= min(opt[l+1][r], r-1); k++) {
            ll cand = dp[l][k] + dp[k+1][r] + pre[r+1] - pre[l];
            if (cand < dp[l][r]) { dp[l][r] = cand; opt[l][r] = k; }
        }
    }
```

Fill order: by length ascending, so `opt[l][r-1]` and `opt[l+1][r]` (length `len-1`) are ready.
`opt[l][l] = l` seeds it. Memory: two n×n tables; n = 5000 → 25·10⁶ `long long` = 200 MB — store
`dp` as `long long` and `opt` as `int`/`short`, or notice that only two lengths are needed at a time.

---

## 10. Divide & conquer optimization

Layered DP `dp[k][i] = min_{j < i} dp[k-1][j] + C(j, i)` (split a prefix of length `i` into `k`
groups; the last group is `(j, i]`). Naive O(k n²). If the argmin `opt(i)` (smallest optimal `j`) is
**non-decreasing in `i`** for every layer, compute the layer by divide & conquer: take the middle
`i`, find `opt(mid)` by scanning its allowed range, then the left half's optima lie in
`[optlo, opt(mid)]` and the right half's in `[opt(mid), opthi]`. Each recursion level scans
O(n) candidates in total (the ranges overlap only at endpoints), depth log n → O(n log n) per
layer, O(k n log n) overall.

**Sufficient condition:** `C` satisfies the quadrangle inequality `C(a,c) + C(b,d) ≤ C(a,d) + C(b,c)`
for `a ≤ b ≤ c ≤ d` (equivalently, it is "Monge"). Proof of monotonicity: suppose `i < i'` and
`opt(i') < opt(i)`; put `a = opt(i')`, `b = opt(i)`, `c = i`, `d = i'`. QI gives
`C(a,c) + C(b,d) ≤ C(a,d) + C(b,c)`; adding `dp[a] + dp[b]` to both sides and using optimality of
`b` at `c` (`dp[b] + C(b,c) ≤ dp[a] + C(a,c)`) yields `dp[b] + C(b,d) ≤ dp[a] + C(a,d)`, so `b` is at
least as good for `i'` — contradiction with `a` being the *smallest* optimum unless equal. ∎

Costs satisfying QI: `(Σ segment)²`, `Σ pairwise products inside a segment` (Subarray Squares
CSES 2086, Houses and Schools CSES 2087 with a bit more work), anything of the form
`f(pre[i] − pre[j])` with `f` convex.

```cpp
void compute(int lo, int hi, int optlo, int opthi) {      // fills cur[lo..hi] from prev
    if (lo > hi) return;
    int mid = (lo + hi) / 2; ll best = INF; int bestj = optlo;
    for (int j = optlo; j <= min(mid - 1, opthi); j++) {
        ll cand = prev[j] + C(j, mid);
        if (cand < best) { best = cand; bestj = j; }
    }
    cur[mid] = best;
    compute(lo, mid - 1, optlo, bestj);
    compute(mid + 1, hi, bestj, opthi);
}
// per layer k: compute(k, n, k-1, n-1)
```

Pitfalls: the `j < mid` bound (`min(mid-1, opthi)`) — otherwise you allow empty groups; ties must
be broken consistently (strict `<` keeps the smallest `j`); memory is two rows, not `k` rows.

---

## 11. Convex hull trick

**Derivation.** Many recurrences have the shape `dp[i] = min_{j<i} ( dp[j] + b[j] · a[i] ) + c[i]`.
Fix `j`: `y = b[j] · x + dp[j]` is a *line* with slope `b[j]` and intercept `dp[j]`. `dp[i]` is the
minimum over a set of lines evaluated at `x = a[i]`, i.e. the lower envelope of the lines. Lines
are inserted as `dp[j]` becomes known; queries come as `a[i]`. The task is a data structure
problem: *insert line, query min at x*.

Typical origins: `dp[i] = min_j dp[j] + (pre[i] − pre[j])²` expands to
`dp[j] + pre[j]² − 2·pre[j]·pre[i]` + `pre[i]²`: slope `−2 pre[j]`, intercept `dp[j] + pre[j]²`,
query `x = pre[i]`. Any cost `f(i) g(j) + h(j)` works.

**Monotone version (deque).** If slopes arrive in non-increasing order *and* query `x` is
non-decreasing, the envelope is a deque: new lines are appended at the back (popping lines made
useless), queries advance a pointer at the front (popping lines that are no longer optimal for
any future `x`). Amortised O(1) per operation.

Line `l2` (between `l1` and `l3`, slopes `m1 ≥ m2 ≥ m3`) is useless iff the intersection of
`l1, l3` is not to the right of the intersection of `l1, l2`:
`(c3 − c1)/(m1 − m3) ≤ (c2 − c1)/(m1 − m2)`  ⟺  `(c3 − c1)(m1 − m2) ≤ (c2 − c1)(m1 − m3)`.
Cross-multiply only with positive denominators (slopes strictly decreasing after dedup), and use
`__int128` — products of two 10¹²-scale values overflow `long long`.

```cpp
struct MonoCHT {                      // min; slopes non-increasing; queries non-decreasing
    deque<pair<ll,ll>> q;             // (m, c)
    static bool bad(pair<ll,ll> l1, pair<ll,ll> l2, pair<ll,ll> l3) {
        return (__int128)(l3.second - l1.second) * (l1.first - l2.first) <=
               (__int128)(l2.second - l1.second) * (l1.first - l3.first);
    }
    void add(ll m, ll c) {
        if (!q.empty() && q.back().first == m) { if (q.back().second <= c) return; q.pop_back(); }
        while (q.size() >= 2 && bad(q[q.size()-2], q.back(), {m, c})) q.pop_back();
        q.push_back({m, c});
    }
    ll query(ll x) {
        while (q.size() >= 2 && q[1].first*x + q[1].second <= q[0].first*x + q[0].second) q.pop_front();
        return q[0].first*x + q[0].second;
    }
};
```

If slopes are monotone but queries are not: keep the deque, binary search the query (O(log n)).
If neither is monotone: **Li Chao tree** over the query coordinate range (integers, or compress
the query points). Each node stores one line; inserting a new line, keep at the node the one that
is better at the node's midpoint and push the other down the side where it can still win (it wins
on at most one side because two lines cross once). O(log C) per insert and query, no monotonicity
assumptions, ~20 lines. Prefer it whenever unsure; the deque version is a constant-factor
optimisation.

```cpp
struct LiChao {                                   // min over integer x in [lo, hi]
    struct Line { ll m, c; ll at(ll x) const { return m*x + c; } };
    int lo, hi; vector<Line> tr; vector<char> has;
    void add(int node, int l, int r, Line nl) {
        if (!has[node]) { tr[node] = nl; has[node] = 1; return; }
        int mid = (l + r) >> 1;
        bool L = nl.at(l) < tr[node].at(l), M = nl.at(mid) < tr[node].at(mid);
        if (M) swap(tr[node], nl);
        if (l == r) return;
        if (L != M) add(2*node, l, mid, nl); else add(2*node+1, mid+1, r, nl);
    }
    ll query(ll x) const { /* walk root->leaf, min of tr[node].at(x) over nodes with has */ }
};
```

Pitfalls: for `max`, negate slopes and intercepts (or flip comparisons everywhere — pick one); `x`
range for Li Chao must contain every query; when `dp[j]` can be `INF`, do not insert that line.

---

## 12. Slope trick

For DPs whose value as a function of one continuous parameter is **convex piecewise linear**,
represent the function by its breakpoints instead of its values. Two multisets: `L` (breakpoints
where slope increases from negative side — a max-heap) and `R` (a min-heap), plus the current
minimum value. Operations that preserve convexity and are O(log n): add `|x − a|` (push `a` into
both heaps, then fix the crossing), take prefix-minimum `g(x) = min_{y ≤ x} f(y)` (drop `R`), shift.

**"Make the array non-decreasing with minimum Σ|aᵢ − bᵢ|"** (Increasing Array II / classic).
`f_i(x)` = min cost of the prefix `i` with `b_i ≤ x`. `f_i(x) = prefmin( f_{i−1}(x) + |x − a_i| )`.
`f_{i-1}` is non-increasing (it is a prefix-min) with slopes `−(i−1) … 0`; adding `|x − a_i|` adds
slope `−1` left of `a_i` and `+1` right of it; the prefix-min then flattens the part to the right of
the new minimum. Only the left breakpoints matter → one max-heap. The minimum value increases by
`max(L) − a_i` exactly when `a_i < max(L)` (the new `+1` slope segment from `a_i` to `max(L)` is
flattened, at cost equal to its height). That is the whole algorithm:

```cpp
ll make_nondecreasing_cost(const vector<ll>& a) {
    priority_queue<ll> L; ll cost = 0;
    for (ll x : a) {
        L.push(x);
        if (L.top() > x) { cost += L.top() - x; L.pop(); L.push(x); }
    }
    return cost;
}
```

Trace `a = 3 1 2`: push 3 → L={3}, cost 0. push 1: top 3 > 1 → cost 2, L={1,1}. push 2: top 1 ≤ 2 →
L={1,1,2}, cost 2. Answer 2 (`b = 2 2 2` or `1 1 2`... either costs 2). Reconstruction: the
optimal `b_i` is `max(L)` after step `i`, taken as a suffix-min at the end.

Variants: "strictly increasing" → subtract `i` from `a_i` first. Costs with slopes ±k → push `k`
copies. Both-sided constraints keep both heaps and a lazy shift per heap. Slope trick appears in
JOI/Div1 D as the intended solution for anything "minimise Σ|change| subject to a monotone
constraint" with n up to 10⁶.

---

## 13. Aliens trick (Lagrangian relaxation / WQS binary search)

Problem: minimise `cost` subject to using **exactly k** items/segments, where `f(k)` = optimal cost
with exactly `k` is **convex** in `k`. The exactly-`k` DP has a `k` dimension (O(nk)); we remove it.

Relax: add a penalty `λ` per item, solve the unconstrained problem `g(λ) = min_k ( f(k) + λk )`
with a plain DP that also reports the count `cnt(λ)` it used. Because `f` is convex, the
minimising `k` for a given `λ` is the point where the slope `f(k) − f(k−1)` crosses `−λ`; `cnt(λ)`
is non-increasing in `λ`; and for every `k` there is a `λ` making `k` optimal. Binary search `λ`
for the smallest value with `cnt(λ) ≤ k` (breaking ties in the DP toward the **fewest** items).
Then `k` is among the optimal counts at that `λ` (the argument in the box), and
`f(k) = g(λ) − λk`.

```
convex f(k): slopes s_k = f(k-1) - f(k) >= 0 are non-increasing in k.
At penalty λ, k is optimal  <=>  s_{k+1} <= λ <= s_k.
cnt(λ) with fewest-tiebreak = smallest optimal k  <= k   <=>  λ >= s_{k+1}.
Smallest such λ is s_{k+1}, and s_{k+1} <= s_k, so k is optimal there. g(λ) = f(k) + λk.
```

Ties are the whole difficulty: when several `k` are optimal for `λ` (collinear points of `f`), the
DP returns one of them; consistent tie-breaking plus the formula `g(λ) − λk` still gives the right
value for the requested `k` even if `cnt ≠ k`. Integer `λ` suffices when `f` is integer-valued
(slopes are integers). Range of `λ`: `[0, max possible cost]`. Total: O(log C) runs of the relaxed
DP, which must itself be fast (CHT / D&C / greedy) — that is why the trick pairs with section 11.

```cpp
// relaxed DP returns (cost incl. penalties, #segments); fewest segments on ties
ll aliens(const vector<ll>& a, int K) {
    ll lo = 0, hi = HIGH;
    while (lo < hi) { ll mid = (lo + hi) / 2; if (penalized(mid).second <= K) hi = mid; else lo = mid + 1; }
    return penalized(lo).first - lo * K;
}
```

**Convexity requirement.** Without convexity the trick silently returns a wrong value (some `k`
are never optimal for any `λ`). Sufficient conditions: the cost is a min-cost-flow value as a
function of flow (always convex), or the segment cost satisfies QI (partition problems), or
matroid-like exchange structure. When unsure: compute `f(k)` by the O(nk) DP for small n and plot
the differences. IOI 2016 "Aliens" is the eponym: cover n points with exactly k squares of
minimum total area, n ≤ 10⁵, k ≤ n, and the relaxed DP is a CHT.

---

## 14. SOS DP (recap)

`F[mask] = Σ_{sub ⊆ mask} A[sub]` for all masks in O(n 2ⁿ) instead of O(3ⁿ): process bit by bit,
`if (mask >> b & 1) A[mask] += A[mask ^ (1 << b)]`. After processing bits `0..b`, `A[mask]` sums
over all `sub` that agree with `mask` on bits `> b` and are ⊆ on bits `≤ b`. Superset sums:
flip the condition. Inverse (Möbius): same loops with `−=`. Bitwise-operations chapter covers
applications (SOS Bit Problem CSES 1654, counting pairs with `a & b = 0`).

---

## 15. DP with data structures

When the transition is `dp[i] = best over j in some range/condition of dp[j]`, the inner loop is a
range query; replace it with a Fenwick/segment tree on the right key.

**Increasing Subsequence II** (CSES 1748): number of strictly increasing subsequences.
`dp[i] = 1 + Σ_{j<i, a_j<a_i} dp[j]`. Key = value rank (compress), Fenwick prefix sum over ranks
`< rank(a_i)`, then add `dp[i]` at `rank(a_i)`. O(n log n). For non-strict, query `≤`.

```cpp
for (ll x : a) {
    int r = lower_bound(vals.begin(), vals.end(), x) - vals.begin();
    ll dp = (1 + (r ? fw.sum(r - 1) : 0)) % MOD;
    fw.add(r, dp); total = (total + dp) % MOD;
}
```

Other shapes: `dp[i] = min_{j ∈ [i−k, i−1]} dp[j] + c[i]` → monotone deque (sliding window min,
O(1) amortised) or segment tree; `dp[i] = min_{j : a_j ≤ a_i − d} dp[j]` → segment tree on
value with point-min updates; **Projects** (CSES 1140): sort by end, `dp[i] = max(dp[i−1],
reward_i + dp[last project ending before start_i])` with binary search — a DP with a `lower_bound`
as the data structure; **Elevator Rides** alternatives don't need it, but **Increasing Array
Queries** (CSES 2416) and **Movie Festival II** do.

---

## 16. Expected value and probability DP

Same recurrences with `double` values and probabilities as transition weights. Expected value is
linear, so `E[sum] = Σ E[parts]` even for dependent parts — most "expected number of X" problems
are Σ P(each X). Where a DP is needed: **Dice Probability** (CSES 1725), `dp[t][s]` = P(sum of `t`
dice = `s`), transition `dp[t][s] += dp[t−1][s−f] / 6`. **Moving Robots**, **Candy Lottery**,
**Inversion Probability**: independence across robots/pairs lets you multiply marginals.

Expected steps with self-loops (e.g. "expected throws until …"): the recurrence
`E[s] = 1 + Σ p_i E[next_i]` may reference `E[s]` itself; solve algebraically:
`E[s](1 − p_self) = 1 + Σ_{i≠self} p_i E[next_i]`. Cycles across states → Gaussian elimination.

Precision: `double` relative error 10⁻¹⁶ per op; printing with `printf("%.6f")` is fine for ~10⁶
operations. Use `long double` only when subtracting nearly equal probabilities.

---

## 17. Bitset DP

Beyond subset sum: any boolean DP whose transition is a shift/and/or over the second index.
`dp[i]` as a `bitset<N>`: reachability in a grid row ("Two Sets"/"Grid Path" style), string
matching with wildcards (`match |= (match << 1) & occ[c]` — the Shift-And algorithm), LCS of
many-valued strings in O(nm/64) (bit-parallel LCS). Speed-up factor ≈ 64 with `-O2`; `bitset<N>`
needs compile-time `N` — allocate the maximum.

---

## 18. DP on strings with automata

"Count strings of length n that contain / avoid pattern(s)" = DP over the states of an automaton
that recognises the pattern(s): `dp[i][state]` = number of prefixes of length `i` ending in
`state`. For one pattern the automaton is KMP's (`aut[s][c]` = next matched length; `m+1` states,
O(mΣ) to build); for many patterns it is Aho–Corasick's. Answer for "contains" =
`Σ^n − avoiding`. **Required Substring** (CSES 1112) is exactly this. Build and proofs are in
chapter 14; the DP is:

```cpp
for (int i = 0; i < n; i++) {          // dp over states < m (not yet matched)
    fill(nd.begin(), nd.end(), 0);
    for (int s = 0; s < m; s++) if (dp[s])
        for (int c = 0; c < k; c++) { int t = aut[s][c]; if (t < m) nd[t] = (nd[t] + dp[s]) % MOD; }
    dp = nd;
}
```

O(n · m · Σ) — with `n, m ≤ 1000` and Σ = 26, 2.6·10⁷.

---

## 19. Memory tricks

| Trick | When | Cost |
|---|---|---|
| Rolling arrays (`prev`/`cur`, swap) | transition uses only the previous layer | loses reconstruction |
| `dp[i & 1]` index | same, when you prefer one 2D array | same |
| Store parent pointers (`opt[i]`, direction bits) | need the solution, not just the value | one extra table (use `int`/`char`) |
| Recompute instead of store | reconstruction of a layered DP: recompute layer `k−1` on the way back | ×2 time |
| Hirschberg | two-string DP with O(n+m) memory and reconstruction | ×2 time |
| Smaller types | `int` vs `long long`, `short`, `uint8_t` for bounded values | none |
| `vector<vector>` → flat array | 2D tables with many small rows | fewer allocations, better cache |

Memory limit arithmetic: 256 MB = 6.4·10⁷ `int` = 3.2·10⁷ `long long`. `dp[5000][5000]` of
`long long` = 200 MB — over; of `int` = 100 MB — fine.

---

## 20. Debugging a DP

1. **Print the table** on a 4–6 element input and fill it by hand. Most bugs are a wrong base
   case or a transition reading a cell that is not yet final (order bug).
2. **Brute force cross-check**: write the exponential recursion (10 lines), generate random small
   inputs, compare; this is what every `assert` in `example.cpp` does. Keep the O(n²) version
   alive when you write the optimised one — it is the checker.
3. **Sentinels**: use `INF = LLONG_MAX/4` (so `INF + INF` does not overflow) and check `dp[j] < INF`
   before using it; for counting use exact 0.
4. **Off-by-one audit**: prefix `[0, i)` vs index `i`; `dp[i]` "ending at i" vs "among first i".
   Choose "prefix of length i" as the default and stick to it.
5. **Modular**: every `+` and `*` followed by `% MOD`; negative results after subtraction.
6. **Recursion depth**: memoised DP with depth > ~10⁵ → convert to bottom-up or raise the stack.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| unbounded items, min count / count ordered | 1D DP sums asc, items inner | O(X·k) |
| unbounded items, count unordered | items outer, sums asc | O(X·k) |
| each item once, value | 0/1, capacity desc | O(nW) |
| each item `cᵢ` times | binary split + 0/1 | O(W Σ log cᵢ) |
| each item once, reachability only | `bitset` shift-or | O(nW/64) |
| longest increasing subsequence, n ≤ 10⁶ | tails + `lower_bound` | O(n log n) |
| two strings, prefix alignment | 2D DP, rolling rows | O(nm), O(min) memory |
| "modulo 10⁹+7" | counting DP | same as optimisation |
| tree, per-vertex 0/1 choice | `dp[v][0/1]` | O(n) |
| tree, "choose k in subtree" | subtree merging | O(n²) or O(nk) |
| tree, answer for every root | rerooting | O(n) |
| "numbers ≤ N whose digits …" | digit DP `(pos, mem, tight, started)` | O(L · states · 10) |
| n ≤ 20, permutations/subsets | `dp[mask]` / `dp[mask][last]` | O(2ⁿ n) |
| grid, one side ≤ 10, tilings/paths | broken profile | O(nm 2ⁿ) |
| interval merge, cost = range sum | Knuth | O(n²) |
| exactly k groups, cost convex in segment sum, k ≤ ~100 | D&C optimization | O(k n log n) |
| exactly k groups, k large, f(k) convex | Aliens trick + fast relaxed DP | O(n log n · log C) |
| `dp[j] + a[i]·b[j]` | CHT deque (monotone) / Li Chao | O(n) / O(n log C) |
| minimise Σ|change| under monotone constraint | slope trick (heaps) | O(n log n) |
| transition = range query over earlier states | Fenwick / segment tree / deque | O(n log n) |
| sum over submasks for every mask | SOS DP | O(n 2ⁿ) |
| count strings containing/avoiding patterns | DP over KMP / Aho–Corasick automaton | O(n · states · Σ) |
| probabilities / expected values | same DP with doubles; linearity | — |

## Implementation checklist for contests

- [ ] State written in one sentence; #states × transition cost computed and ≤ ~10⁸.
- [ ] Base cases and `INF`/`0` sentinels set explicitly; `INF = LLONG_MAX/4`.
- [ ] Loop order matches dependencies (0/1: capacity descending; unbounded: ascending; interval:
      by length; mask: increasing).
- [ ] `long long` where sums/products can exceed 2³¹; `% MOD` after every op in counting DPs.
- [ ] Reconstruction needed? Then keep parents or the full table; otherwise roll.
- [ ] Optimisation preconditions verified (QI for Knuth/D&C, monotone slopes+queries for deque
      CHT, convexity for Aliens) — or stress-tested against the O(n²) version on random data.
- [ ] Recursion depth < 10⁵ or iterative.
- [ ] Memory: table bytes computed against the limit.
- [ ] Tested on: minimum n (0 or 1), all-equal input, strictly decreasing input, max n for time.

## Further reading

- CPH (Laaksonen, *Competitive Programmer's Handbook*): ch. 7 (DP), ch. 10.5 (Elevator Rides,
  Hamiltonian paths, tilings), ch. 14.3 (rerooting).
- cp-algorithms.com: "Divide and Conquer DP", "Knuth's Optimization", "Convex hull trick and Li
  Chao tree", "Sum over Subsets DP", "Longest increasing subsequence", "Edit distance".
- Yao, "Efficient dynamic programming using quadrangle inequalities" (STOC 1980) — the Knuth/D&C
  monotonicity theorem.
- Codeforces blogs: "Slope trick" (by kuroni), "Aliens trick / WQS binary search" write-ups, "DP
  optimizations" (the standard list: Knuth, D&C, CHT, Li Chao, SMAWK mention).
- IOI 2016 "Aliens" official analysis (the trick's origin in olympiad folklore).

## You can move on when...

- You can write, from memory and without a bug on the first compile: bounded knapsack, LIS with
  reconstruction, Elevator Rides, Counting Tilings, Counting Numbers, Knuth, D&C optimization,
  monotone CHT, Li Chao, slope trick, Aliens on the squared-partition problem.
- You can state the preconditions of each optimisation and give a cost function that violates each.
- All 23 CSES Dynamic Programming tasks and the Advanced-Techniques DP tasks in `problems.md` are
  solved, with at least three of the hard ones stress-tested against a brute force.
- Given an unlabeled Div1 C with an obvious O(n²) DP, you identify within five minutes which of
  sections 9–13 (if any) applies.
