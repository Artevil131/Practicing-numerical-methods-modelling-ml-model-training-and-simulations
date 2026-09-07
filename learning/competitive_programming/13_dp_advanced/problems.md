# Chapter 13 — Problems

How to work this ladder:

1. Re-type the relevant snippet of `example.cpp` from memory *before* opening a problem that uses it;
   diff against the file; fix your version, not the file.
2. One file per problem: `competitive_programming/13_dp_advanced/solutions/<source>_<id>.cpp`
   (e.g. `cses_1653.cpp`, `cf_319C.cpp`, `atcoder_dp_z.cpp`).
3. For every optimised DP (sections 9–13 of the lesson) keep the O(n²) version in the same file
   under `#ifdef BRUTE` and stress-test on random inputs before submitting.
4. Time yourself: ★1–2 → 15 min, ★3 → 30 min, ★4 → 60 min, ★5 → 90 min, then read the hint, then
   the sketch, then upsolve. Never skip the upsolve.
5. The CSES Dynamic Programming section is the backbone; every one of its 23 tasks is here.

Difficulty: ★1 = warm-up, ★3 = Div2 D, ★5 = Div1 D / IOI subtask 5. CF ratings are estimates.

---

## A. Warm-up: the basic loops, done fast

### 13.1  Dice Combinations  ·  CSES 1633  ·  ★1
https://cses.fi/problemset/task/1633
**Technique:** unbounded, ordered sequences (sums ascending, faces inside).
<details><summary>Hint</summary>`dp[s] = Σ_{f=1..6} dp[s-f]`, `dp[0] = 1`.</details>
<details><summary>Approach sketch</summary>One array of n+1 values, six terms each, mod 10⁹+7. This is a 3-minute task; use it to fix your I/O template.</details>

### 13.2  Minimizing Coins  ·  CSES 1634  ·  ★1
https://cses.fi/problemset/task/1634
**Technique:** unbounded min-count.
<details><summary>Hint</summary>`INF` sentinel; answer −1 if `dp[x]` still `INF`.</details>
<details><summary>Approach sketch</summary>`dp[s] = 1 + min_c dp[s-c]`. O(n·x) = 100·10⁶ — fine, but write the inner loop tight (`int`, no bounds checks).</details>

### 13.3  Coin Combinations I  ·  CSES 1635  ·  ★1
https://cses.fi/problemset/task/1635
**Technique:** unbounded, ordered count.
<details><summary>Hint</summary>Same loop order as Dice Combinations.</details>
<details><summary>Approach sketch</summary>Sums outer, coins inner. Compare with the next task to burn the loop-order distinction into memory.</details>

### 13.4  Coin Combinations II  ·  CSES 1636  ·  ★2
https://cses.fi/problemset/task/1636
**Technique:** unbounded, unordered count.
<details><summary>Hint</summary>Coins outer, sums inner ascending.</details>
<details><summary>Approach sketch</summary>Processing coin types one at a time makes the choice "how many of coin c" happen once, so {1,3} and {3,1} become one path. 10⁸ operations: use `int` and reduce with a conditional subtract instead of `%`.</details>

### 13.5  Removing Digits  ·  CSES 1637  ·  ★1
https://cses.fi/problemset/task/1637
**Technique:** 1D DP over n with digit transitions.
<details><summary>Hint</summary>`dp[x] = 1 + min over digits d of x: dp[x-d]`.</details>
<details><summary>Approach sketch</summary>Bottom-up from 0 to n; the greedy "subtract the largest digit" also happens to be optimal, but write the DP.</details>

### 13.6  Grid Paths I  ·  CSES 1638  ·  ★1
https://cses.fi/problemset/task/1638
**Technique:** grid counting DP with obstacles.
<details><summary>Hint</summary>Traps are zeros.</details>
<details><summary>Approach sketch</summary>Row-major fill; the first row/column need the obstacle check too (a trap blocks everything after it in that line).</details>

### 13.7  Book Shop  ·  CSES 1158  ·  ★2
https://cses.fi/problemset/task/1158
**Technique:** 0/1 knapsack, capacity descending.
<details><summary>Hint</summary>One row of size x+1.</details>
<details><summary>Approach sketch</summary>`dp[cap] = max(dp[cap], dp[cap-h] + s)` for cap from x down to h. 10³·10⁵ = 10⁸ updates in `int` — passes in ~0.3 s.</details>

### 13.8  Array Description  ·  CSES 1746  ·  ★2
https://cses.fi/problemset/task/1746
**Technique:** counting DP with a small second dimension.
<details><summary>Hint</summary>`dp[i][v]` for `v` in 1..m, three predecessors.</details>
<details><summary>Approach sketch</summary>If `a[i] ≠ 0` only `v = a[i]` is allowed. Rolling rows. Watch the boundaries `v = 1` and `v = m`.</details>

### 13.9  Counting Towers  ·  CSES 2413  ·  ★3
https://cses.fi/problemset/task/2413
**Technique:** counting DP with a two-value state (profile of the top row).
<details><summary>Hint</summary>State: is the top layer one 2-wide block or two 1-wide blocks? Count how each extends.</details>
<details><summary>Approach sketch</summary>From a wide top you can continue the block (1 way) or start a new wide block (1) or start two narrow blocks (1); from a split top you can continue both (1), continue one and start the other (2), start two new (1), or start a wide block (1). Precompute up to 10⁶ once, answer all tests in O(1).</details>

### 13.10  Edit Distance  ·  CSES 1639  ·  ★2
https://cses.fi/problemset/task/1639
**Technique:** two-string DP, rolling rows.
<details><summary>Hint</summary>Three transitions; match costs 0.</details>
<details><summary>Approach sketch</summary>5000×5000 = 2.5·10⁷ cells; two `int` rows. Print the answer only.</details>

### 13.11  Longest Common Subsequence  ·  CSES 3403  ·  ★2
https://cses.fi/problemset/task/3403
**Technique:** two-string DP with reconstruction.
<details><summary>Hint</summary>Keep the full table (or direction bits) to walk back.</details>
<details><summary>Approach sketch</summary>Standard LCS table, then backtrack from (n, m): equal → diagonal, else move to the larger of up/left. Output length and the sequence.</details>

### 13.12  Rectangle Cutting  ·  CSES 1744  ·  ★2
https://cses.fi/problemset/task/1744
**Technique:** 2D DP over dimensions (interval-DP flavour).
<details><summary>Hint</summary>`dp[a][b] = 0` if `a == b`, else 1 + min over horizontal and vertical cuts.</details>
<details><summary>Approach sketch</summary>O(ab·(a+b)) = 500²·1000 = 2.5·10⁸ light operations — passes with `int` and tight loops; symmetric `dp[a][b] = dp[b][a]` halves the work.</details>

### 13.13  Minimal Grid Path  ·  CSES 3359  ·  ★3
https://cses.fi/problemset/task/3359
**Technique:** DP by anti-diagonals with lexicographic minimisation.
<details><summary>Hint</summary>All paths have the same length; decide the path letter by letter, keeping the set of cells reachable with the current minimal prefix.</details>
<details><summary>Approach sketch</summary>Process anti-diagonals: from the current frontier set, look at the letters of the right/down neighbours, take the minimum letter, and keep only the neighbours with that letter as the new frontier. O(n²) total since each cell enters a frontier at most once.</details>

### 13.14  Money Sums  ·  CSES 1745  ·  ★2
https://cses.fi/problemset/task/1745
**Technique:** subset-sum reachability; `bitset`.
<details><summary>Hint</summary>`reach |= reach << x`.</details>
<details><summary>Approach sketch</summary>Sum ≤ 10⁵; a `bitset<100001>` and 100 shift-ors. Print popcount−1 (exclude 0) and the set bits.</details>

### 13.15  Removal Game  ·  CSES 1097  ·  ★3
https://cses.fi/problemset/task/1097
**Technique:** interval game DP (score difference).
<details><summary>Hint</summary>`dp[l][r]` = best (mine − theirs) for the player to move on [l, r].</details>
<details><summary>Approach sketch</summary>`dp[l][r] = max(a[l] − dp[l+1][r], a[r] − dp[l][r−1])`; answer `(total + dp[0][n−1]) / 2`. n = 5000 → 2.5·10⁷ `long long` cells = 200 MB: fill by length and keep only two diagonals, or use the fact that only `dp[l][r]` with `r−l` parity fixed by n matters — or just use `int` if sums fit (they do not: 5000·10⁹ — so roll by length).</details>

### 13.16  Two Sets II  ·  CSES 1093  ·  ★3
https://cses.fi/problemset/task/1093
**Technique:** 0/1 counting DP over sums.
<details><summary>Hint</summary>Target `n(n+1)/4`; if the total is odd, answer 0. Each partition is counted twice.</details>
<details><summary>Approach sketch</summary>Count subsets with sum `total/2` via 0/1 counting (items outer, sum descending), multiply by the modular inverse of 2. 500 · 62500 ≈ 3·10⁷ operations.</details>

### 13.17  Mountain Range  ·  CSES 3314  ·  ★3
https://cses.fi/problemset/task/3314
**Technique:** DP over a DAG defined by visibility (nearest-higher structure).
<details><summary>Hint</summary>Which mountains can you jump to from a given one? They form a set that the nearest-higher-to-the-left / right (monotonic stack) describes; jumps only go to strictly higher mountains, so the jump graph is a DAG ordered by height.</details>
<details><summary>Approach sketch</summary>Sort mountains by height and compute `dp[v]` = longest jump chain starting at `v` from higher mountains first; the candidate targets are found with a monotonic stack / sparse table on the "all in between are lower" condition. Verify the exact jump rule in the statement before coding — that rule determines whether the reachable set is O(1) or O(n) per mountain.</details>

### 13.18  Increasing Subsequence  ·  CSES 1145  ·  ★2
https://cses.fi/problemset/task/1145
**Technique:** LIS in O(n log n).
<details><summary>Hint</summary>`lower_bound` into `tail`.</details>
<details><summary>Approach sketch</summary>Exactly section 2 of the lesson without reconstruction. n = 2·10⁵.</details>

### 13.19  Projects  ·  CSES 1140  ·  ★3
https://cses.fi/problemset/task/1140
**Technique:** interval scheduling DP with binary search.
<details><summary>Hint</summary>Sort by end day; `dp[i] = max(dp[i−1], reward_i + dp[j])` where `j` is the last project ending before `start_i`.</details>
<details><summary>Approach sketch</summary>`upper_bound` on end days gives `j`. O(n log n). Weighted interval scheduling is the general name; "DP where the data structure is `lower_bound`".</details>

---

## B. Bitmask, profile and digit DP

### 13.20  Elevator Rides  ·  CSES 1653  ·  ★3
https://cses.fi/problemset/task/1653
**Technique:** bitmask DP with a pair value.
<details><summary>Hint</summary>Store `(rides, weight of last ride)` per mask, not weight in the state.</details>
<details><summary>Approach sketch</summary>Lesson §7 verbatim. 2²⁰ · 20 = 2·10⁷ transitions; pairs of `int` keep the table at 8 MB.</details>

### 13.21  Counting Tilings  ·  CSES 2181  ·  ★4
https://cses.fi/problemset/task/2181
**Technique:** broken-profile DP.
<details><summary>Hint</summary>Process cell by cell; the mask says which of the next n cells are pre-covered.</details>
<details><summary>Approach sketch</summary>Lesson §7 cell-at-a-time transition: covered → clear bit; else horizontal → set bit i (if not last column); vertical → set bit i+1 (if free). O(n·m·2ⁿ) ≈ 10⁷. Cross-check with the brute-force domino placement in `example.cpp` for tiny grids.</details>

### 13.22  Counting Numbers  ·  CSES 2220  ·  ★3
https://cses.fi/problemset/task/2220
**Technique:** digit DP.
<details><summary>Hint</summary>`count(b) − count(a−1)`; state `(pos, prev, started)`, tight handled by iteration.</details>
<details><summary>Approach sketch</summary>Lesson §6. Careful with `a = 0` (then `count(−1) = 0`) and with the number 0 itself, which is valid (a single digit).</details>

### 13.23  Hamiltonian Flights  ·  CSES 1690  ·  ★3
https://cses.fi/problemset/task/1690
**Technique:** bitmask DP over paths (counting).
<details><summary>Hint</summary>`dp[mask][v]`; do not allow visiting city n before the mask is full.</details>
<details><summary>Approach sketch</summary>Push along each edge from `(mask, u)`; count mod 10⁹+7. 2²⁰ · 20 states, m ≤ 2·10⁵ edges: iterate over the adjacency list of `u` — O(2ⁿ · m / n) ≈ 10⁷.</details>

### 13.24  Classy Numbers  ·  Codeforces 1036C  ·  ★2 (CF ~1500)
https://codeforces.com/contest/1036/problem/C
**Technique:** digit DP with a small counter.
<details><summary>Hint</summary>State = number of nonzero digits used (≤ 3).</details>
<details><summary>Approach sketch</summary>Standard `[l, r]` difference; memoise `(pos, nonzeros)` for non-tight states. Alternatively enumerate all classy numbers (there are few) and binary search — but do the DP for practice.</details>

### 13.25  Magic Numbers  ·  Codeforces 628D  ·  ★3 (CF ~2100)
https://codeforces.com/contest/628/problem/D
**Technique:** digit DP with a modulus and position parity constraint.
<details><summary>Hint</summary>State `(pos, value mod m, tight)`; even positions are forced to digit d, odd positions forbidden d.</details>
<details><summary>Approach sketch</summary>Count for `b` and for `a − 1` (big integers as strings — subtract 1 by hand). 2000 digits × m ≤ 2000 states → 4·10⁶ per bound, transitions ×10.</details>

### 13.26  Kefa and Dishes  ·  Codeforces 580D  ·  ★3 (CF ~1800)
https://codeforces.com/contest/580/problem/D
**Technique:** `dp[mask][last]` with pairwise bonuses.
<details><summary>Hint</summary>Only masks with popcount ≤ m matter; answer = max over masks of popcount exactly m.</details>
<details><summary>Approach sketch</summary>2¹⁸ · 18 · 18 ≈ 8.5·10⁷ transitions — fine. `long long` values.</details>

### 13.27  Digit Sum  ·  AtCoder Educational DP Contest S  ·  ★3
https://atcoder.jp/contests/dp/tasks/dp_s
**Technique:** digit DP with a sum-mod-D state on a 10⁴-digit bound.
<details><summary>Hint</summary>State `(pos, sum mod D, tight)`; iterate positions, not recursion (10⁴ depth × 10 is fine, but iterative is cleaner).</details>
<details><summary>Approach sketch</summary>Bottom-up over positions with two arrays of size D for the loose branch and a scalar for the tight branch. Exclude 0 at the end.</details>

### 13.28  Matching  ·  AtCoder Educational DP Contest O  ·  ★2
https://atcoder.jp/contests/dp/tasks/dp_o
**Technique:** assignment counting over `dp[mask]` with derivable index.
<details><summary>Hint</summary>The number of men assigned = popcount(mask) of women assigned.</details>
<details><summary>Approach sketch</summary>For each mask, the next man is `popcount(mask)`; try each compatible unassigned woman. 2²¹ · 21 ≈ 4·10⁷.</details>

---

## C. Tree DP

### 13.29  Tree Matching  ·  CSES 1130  ·  ★2
https://cses.fi/problemset/task/1130
**Technique:** `dp[v][matched-to-child?]`.
<details><summary>Hint</summary>Lesson §5 formula, or greedy leaf-to-parent.</details>
<details><summary>Approach sketch</summary>Iterative DFS order to avoid deep recursion on a path of 2·10⁵ vertices; compute `dp[v][0]` first, then the best child swap for `dp[v][1]`.</details>

### 13.30  Independent Set  ·  AtCoder Educational DP Contest P  ·  ★2
https://atcoder.jp/contests/dp/tasks/dp_p
**Technique:** tree DP counting colourings (black not adjacent to black).
<details><summary>Hint</summary>`dp[v][black] = Π dp[c][white]`, `dp[v][white] = Π (dp[c][black] + dp[c][white])`.</details>
<details><summary>Approach sketch</summary>Products mod 10⁹+7 over children; n = 10⁵ so use an explicit stack or raise the recursion limit.</details>

### 13.31  Subtree  ·  AtCoder Educational DP Contest V  ·  ★4
https://atcoder.jp/contests/dp/tasks/dp_v
**Technique:** rerooting with a non-invertible combine (arbitrary modulus).
<details><summary>Hint</summary>Prefix and suffix products over children, because you cannot divide modulo a composite M.</details>
<details><summary>Approach sketch</summary>`down[v] = Π (down[c] + 1)`; for each child `c`, the value "everything except c" is `prefix × suffix × (up[v] + 1)`. Two passes, O(n).</details>

### 13.32  Distance in Tree  ·  Codeforces 161D  ·  ★3 (CF ~1800)
https://codeforces.com/contest/161/problem/D
**Technique:** tree DP with a small depth dimension (k ≤ 500).
<details><summary>Hint</summary>`cnt[v][d]` = number of vertices at depth d below v; pairs through v combine `cnt[c1][d1]` with `cnt[c2][k−d1−2]`.</details>
<details><summary>Approach sketch</summary>Merging children with the "running total" trick: for each child, pair its counts with the accumulated counts of previous children, then add it. O(n·k) = 5·10⁷.</details>

### 13.33  Tree with Maximum Cost  ·  Codeforces 1092F  ·  ★3 (CF ~1900)
https://codeforces.com/contest/1092/problem/F
**Technique:** rerooting.
<details><summary>Hint</summary>Moving the root across edge (v, c) changes the answer by `total − 2·subtreeSum[c]`.</details>
<details><summary>Approach sketch</summary>Compute the answer for root 0 and subtree sums bottom-up; then a top-down pass applies the O(1) delta per edge. `long long`.</details>

---

## D. DP with data structures, counting, probability, automata

### 13.34  Increasing Subsequence II  ·  CSES 1748  ·  ★3
https://cses.fi/problemset/task/1748
**Technique:** counting DP with a Fenwick tree over value ranks.
<details><summary>Hint</summary>`dp[i] = 1 + Σ_{a_j < a_i, j < i} dp[j]`.</details>
<details><summary>Approach sketch</summary>Lesson §15 verbatim. Compress values; strict inequality means query prefix `rank − 1`.</details>

### 13.35  Consecutive Subsequence  ·  Codeforces 977F  ·  ★2 (CF ~1700)
https://codeforces.com/contest/977/problem/F
**Technique:** DP with a hash map as the "data structure" plus reconstruction.
<details><summary>Hint</summary>`best[x] = best[x−1] + 1` where `best` is keyed by value.</details>
<details><summary>Approach sketch</summary>One pass with `unordered_map<int,int>`; remember the end value of the maximum, then a second pass outputs indices whose value matches the expected running value.</details>

### 13.36  Dice Probability  ·  CSES 1725  ·  ★2
https://cses.fi/problemset/task/1725
**Technique:** probability DP.
<details><summary>Hint</summary>`dp[t][s]` with weight 1/6; answer Σ over `[a, b]`.</details>
<details><summary>Approach sketch</summary>100 dice → sums up to 600; rolling array of `double`s; print with 6 decimals.</details>

### 13.37  Let's Play Osu!  ·  Codeforces 235B  ·  ★3 (CF ~2000)
https://codeforces.com/contest/235/problem/B
**Technique:** expected value by linearity with a running expectation.
<details><summary>Hint</summary>A run of length L scores L²; `L² = Σ_{i<j in run} 2 + L`, so count expected pairs and singles.</details>
<details><summary>Approach sketch</summary>Maintain `E[current run length]` (`e = p_i·(e + 1)`); the increase in expected score from position i is `p_i·(2·e_prev + 1)`. O(n), `double`.</details>

### 13.38  Compatible Numbers  ·  Codeforces 165E  ·  ★3 (CF ~2200)
https://codeforces.com/contest/165/problem/E
**Technique:** SOS DP (any element that is a submask of the complement).
<details><summary>Hint</summary>`any[mask]` = some input element ⊆ mask; propagate from submasks.</details>
<details><summary>Approach sketch</summary>2²² masks × 22 bits ≈ 10⁸ simple operations; answer for `a` is `any[~a & FULL]`.</details>

### 13.39  Required Substring  ·  CSES 1112  ·  ★4
https://cses.fi/problemset/task/1112
**Technique:** DP over the KMP automaton (lesson §18; automaton construction in chapter 14).
<details><summary>Hint</summary>Count strings that avoid the pattern; subtract from 26ⁿ.</details>
<details><summary>Approach sketch</summary>`aut[state][c]` via the prefix function, states 0..m−1 for "not yet matched", transitions ×26. O(n·m·26).</details>

---

## E. DP optimizations

### 13.40  Knuth Division  ·  CSES 2088  ·  ★4
https://cses.fi/problemset/task/2088
**Technique:** interval DP with Knuth optimization.
<details><summary>Hint</summary>Cost of splitting a range is its sum → QI and monotonicity hold; `opt[l][r-1] ≤ opt[l][r] ≤ opt[l+1][r]`.</details>
<details><summary>Approach sketch</summary>n = 5000: two 5000×5000 tables (`long long` dp, `short`/`int` opt) — 100 MB + 100 MB with `int` opt is too much; use `short` for opt (values < 5000) or store `dp` only for the two lengths in flight. O(n²) ≈ 2.5·10⁷ transitions.</details>

### 13.41  Subarray Squares  ·  CSES 2086  ·  ★4
https://cses.fi/problemset/task/2086
**Technique:** divide & conquer optimization (or CHT per layer).
<details><summary>Hint</summary>`C(j, i) = (pre[i] − pre[j])²` satisfies QI.</details>
<details><summary>Approach sketch</summary>`dp[k][i] = min_j dp[k−1][j] + C(j, i)`, each layer by D&C in O(n log n); n, k ≤ 3000 → 3000·3000·12 ≈ 10⁸ — fine. Alternative: for each layer the transition is `dp[j] + pre[j]² − 2 pre[j] pre[i]` → monotone CHT (slopes −2pre[j] decreasing, queries pre[i] increasing) in O(n) per layer.</details>

### 13.42  Houses and Schools  ·  CSES 2087  ·  ★5
https://cses.fi/problemset/task/2087
**Technique:** D&C optimization with a segment cost that is not a simple formula.
<details><summary>Hint</summary>Placing one school in a segment optimally puts it at the weighted median; cost(j, i) can be computed in O(1) with prefix sums of `x·w` and `w` after locating the median with binary search (or a moving pointer inside the D&C).</details>
<details><summary>Approach sketch</summary>Layered DP over the number of schools; cost of serving houses `(j, i]` by one school satisfies QI (it is a sum of convex distance costs). D&C per layer, O(k n log n · log n) with binary-searched medians, or O(k n log n) with a two-pointer median.</details>

### 13.43  Frog 3  ·  AtCoder Educational DP Contest Z  ·  ★4
https://atcoder.jp/contests/dp/tasks/dp_z
**Technique:** convex hull trick (monotone deque).
<details><summary>Hint</summary>`dp[i] = min_j dp[j] + (h_i − h_j)² + C` → lines with slope `−2h_j`, intercept `dp[j] + h_j²`, query at `h_i`; heights are strictly increasing.</details>
<details><summary>Approach sketch</summary>Slopes decrease and queries increase → deque CHT, O(n). Verify against the O(n²) version on random data before submitting.</details>

### 13.44  Kalila and Dimna in the Logging Industry  ·  Codeforces 319C  ·  ★4 (CF ~2400)
https://codeforces.com/contest/319/problem/C
**Technique:** CHT with the classic `dp[j] + b[j]·a[i]` shape.
<details><summary>Hint</summary>`dp[i] = min_j dp[j] + b[j]·a[i]` with `a` increasing and `b` decreasing — the textbook monotone case.</details>
<details><summary>Approach sketch</summary>Deque CHT with `__int128` (or careful `long double`) in the "bad line" test since `a, b ≤ 10⁹`. Answer `dp[n−1]`.</details>

### 13.45  Yet Another Minimization Problem  ·  Codeforces 868F  ·  ★5 (CF ~2500)
https://codeforces.com/contest/868/problem/F
**Technique:** D&C optimization with a cost evaluated by a moving window (Mo-style).
<details><summary>Hint</summary>Cost of a segment = number of equal pairs inside; maintain it with add/remove pointers while the D&C moves `j` and `mid`.</details>
<details><summary>Approach sketch</summary>QI holds (adding an element to a longer segment costs at least as much). The D&C pattern guarantees the window pointers move O(n log n) per layer in total. k ≤ 20 layers.</details>

### 13.46  Sonya and Problem Wihtout a Legend  ·  Codeforces 713C  ·  ★4 (CF ~2300)
https://codeforces.com/contest/713/problem/C
**Technique:** slope trick (strictly increasing → subtract index).
<details><summary>Hint</summary>Replace `a_i` by `a_i − i`; then "non-decreasing" with cost Σ|change|.</details>
<details><summary>Approach sketch</summary>Lesson §12 one-heap algorithm, O(n log n). The O(n²) alternative (values compressed to the input set) is also acceptable for n = 3000 — write both and compare.</details>

### 13.47  Increasing Array II  ·  CSES 2132  ·  ★4
https://cses.fi/problemset/task/2132
**Technique:** slope trick / convex cost minimisation.
<details><summary>Hint</summary>Read the exact cost model in the statement; then map it to "convex piecewise-linear function updated by adding |x − aᵢ| and taking a prefix minimum".</details>
<details><summary>Approach sketch</summary>Same heap structure as §12; if the statement's cost is per-unit change with a monotone target, the one-heap version applies directly. Stress-test against the O(n·V) DP on small values.</details>

### 13.48  Gosha is hunting  ·  Codeforces 739E  ·  ★5 (CF ~2900)
https://codeforces.com/contest/739/problem/E
**Technique:** Aliens trick with two penalties (or one penalty plus a DP dimension).
<details><summary>Hint</summary>Relax "exactly a Poké Balls" with penalty λ; the inner problem with "exactly b Ultra Balls" is a DP or a second relaxation.</details>
<details><summary>Approach sketch</summary>The expected-catch function is concave in both counts; a nested binary search on two real λ's with a greedy inner solution, or one λ with an O(n·b) inner DP. Precision: use `double` and 60–100 iterations of bisection.</details>

### 13.49  Aliens  ·  IOI 2016 (oj.uz IOI16_aliens)  ·  ★5
https://oj.uz/problem/view/IOI16_aliens
**Technique:** the Aliens trick, eponymously.
<details><summary>Hint</summary>Reduce points to those on/under the diagonal, drop dominated ones, then `dp[i] = min_j dp[j] + area(j, i) − overlap` is a CHT; make the number of squares free with a penalty λ.</details>
<details><summary>Approach sketch</summary>Subtasks 1–4 are the O(n²k) and O(nk) DPs (write them: they are the checkers). Subtask 5: f(k) is convex; binary search λ with the CHT inner DP in O(n) each, ~40 iterations. Tie-breaking toward fewer squares and the formula `g(λ) − λk` finish it.</details>

### 13.50  Book Shop II  ·  CSES 1159  ·  ★3
https://cses.fi/problemset/task/1159
**Technique:** bounded knapsack via binary splitting.
<details><summary>Hint</summary>Copies up to 10⁵ → split into log pieces.</details>
<details><summary>Approach sketch</summary>Lesson §1: each (price, pages, count) becomes ≤ 17 0/1 items; 0/1 knapsack over x ≤ 10⁵. 100·17·10⁵ ≈ 1.7·10⁸ `int` updates — tight but passes with `-O2`.</details>

---

## Practice more

- Codeforces problemset, tag `dp`, rating 1900–2400 (aim for two per week; upsolve every one).
- Codeforces problemset, tag `dp` + `bitmasks`, rating 1800–2200.
- Codeforces problemset, tag `dp` + `trees`, rating 1900–2300.
- AtCoder Educational DP Contest (https://atcoder.jp/contests/dp) — all 26 tasks A–Z; the ones not
  listed above (K Stones, L Deque, M Candies, N Slimes, Q Flowers, R Walk, T Permutation, U
  Grouping, W Intervals, X Tower, Y Grid 2) are each a 20–40 minute exercise on one lesson section.
- Codeforces problemset, tag `dp` + `data structures`, rating 2000–2400 for segment-tree DPs.

## Progress

- [ ] 13.1 Dice Combinations (CSES 1633)
- [ ] 13.2 Minimizing Coins (CSES 1634)
- [ ] 13.3 Coin Combinations I (CSES 1635)
- [ ] 13.4 Coin Combinations II (CSES 1636)
- [ ] 13.5 Removing Digits (CSES 1637)
- [ ] 13.6 Grid Paths I (CSES 1638)
- [ ] 13.7 Book Shop (CSES 1158)
- [ ] 13.8 Array Description (CSES 1746)
- [ ] 13.9 Counting Towers (CSES 2413)
- [ ] 13.10 Edit Distance (CSES 1639)
- [ ] 13.11 Longest Common Subsequence (CSES 3403)
- [ ] 13.12 Rectangle Cutting (CSES 1744)
- [ ] 13.13 Minimal Grid Path (CSES 3359)
- [ ] 13.14 Money Sums (CSES 1745)
- [ ] 13.15 Removal Game (CSES 1097)
- [ ] 13.16 Two Sets II (CSES 1093)
- [ ] 13.17 Mountain Range (CSES 3314)
- [ ] 13.18 Increasing Subsequence (CSES 1145)
- [ ] 13.19 Projects (CSES 1140)
- [ ] 13.20 Elevator Rides (CSES 1653)
- [ ] 13.21 Counting Tilings (CSES 2181)
- [ ] 13.22 Counting Numbers (CSES 2220)
- [ ] 13.23 Hamiltonian Flights (CSES 1690)
- [ ] 13.24 Classy Numbers (CF 1036C)
- [ ] 13.25 Magic Numbers (CF 628D)
- [ ] 13.26 Kefa and Dishes (CF 580D)
- [ ] 13.27 Digit Sum (AtCoder dp_s)
- [ ] 13.28 Matching (AtCoder dp_o)
- [ ] 13.29 Tree Matching (CSES 1130)
- [ ] 13.30 Independent Set (AtCoder dp_p)
- [ ] 13.31 Subtree (AtCoder dp_v)
- [ ] 13.32 Distance in Tree (CF 161D)
- [ ] 13.33 Tree with Maximum Cost (CF 1092F)
- [ ] 13.34 Increasing Subsequence II (CSES 1748)
- [ ] 13.35 Consecutive Subsequence (CF 977F)
- [ ] 13.36 Dice Probability (CSES 1725)
- [ ] 13.37 Let's Play Osu! (CF 235B)
- [ ] 13.38 Compatible Numbers (CF 165E)
- [ ] 13.39 Required Substring (CSES 1112)
- [ ] 13.40 Knuth Division (CSES 2088)
- [ ] 13.41 Subarray Squares (CSES 2086)
- [ ] 13.42 Houses and Schools (CSES 2087)
- [ ] 13.43 Frog 3 (AtCoder dp_z)
- [ ] 13.44 Kalila and Dimna in the Logging Industry (CF 319C)
- [ ] 13.45 Yet Another Minimization Problem (CF 868F)
- [ ] 13.46 Sonya and Problem Wihtout a Legend (CF 713C)
- [ ] 13.47 Increasing Array II (CSES 2132)
- [ ] 13.48 Gosha is hunting (CF 739E)
- [ ] 13.49 Aliens (IOI 2016)
- [ ] 13.50 Book Shop II (CSES 1159)
