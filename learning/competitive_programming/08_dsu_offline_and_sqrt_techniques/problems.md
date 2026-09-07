# Chapter 08 — Problems

How to work this ladder:

1. Before each section, re-type the structure from `example.cpp` **from memory** (DSU, parity
   DSU, rollback DSU, segment tree over time, Sack) and stress-test it against a brute force.
2. Solutions go to `competitive_programming/08_dsu_offline_and_sqrt_techniques/solutions/<source>_<id>.cpp`
   (e.g. `cses_2133.cpp`, `cf_600E.cpp`).
3. Time yourself: ★1–2 ≤ 15 min, ★3 ≤ 40 min, ★4–5 up to 2 h before the hint. Record the time.
4. On a wrong answer: random generator + brute force + minimal failing case. Do not guess-fix.
5. Upsolve everything; write down the reordering trick that made the problem easy.

Difficulty: ★1 (warm-up) … ★5 (IOI-day hard). CF problems carry a rating estimate.

---

## A. DSU fundamentals and Kruskal

### 08.1  Building Roads  ·  CSES 1666  ·  ★1
https://cses.fi/problemset/task/1666
**Technique:** DSU component count.
<details><summary>Hint</summary>`comps - 1` roads; connect any representative of each component to the next.</details>
<details><summary>Approach sketch</summary>Unite all given roads. Collect one vertex per root (`find(v) == v`). Output consecutive pairs of those representatives. O((n + m) α).</details>

### 08.2  Road Construction  ·  CSES 1676  ·  ★2
https://cses.fi/problemset/task/1676
**Technique:** DSU with sizes, online.
<details><summary>Hint</summary>The maximum component size never decreases.</details>
<details><summary>Approach sketch</summary>After each `unite`, print `comps` and `mx = max(mx, sz[root])`. This is the "type the DSU in 60 seconds" problem.</details>

### 08.3  Road Reparation  ·  CSES 1675  ·  ★2
https://cses.fi/problemset/task/1675
**Technique:** Kruskal.
<details><summary>Hint</summary>Sort edges, take those that unite; check that `n - 1` were taken.</details>
<details><summary>Approach sketch</summary>Standard Kruskal with the DSU; if fewer than `n - 1` edges are accepted the graph is disconnected — print `IMPOSSIBLE`. `long long` for the total.</details>

### 08.4  Building Teams  ·  CSES 1668  ·  ★2
https://cses.fi/problemset/task/1668
**Technique:** Parity DSU (or BFS 2-colouring — do it with parity DSU here).
<details><summary>Hint</summary>Each edge is the constraint `team(a) ≠ team(b)`, i.e. parity difference 1.</details>
<details><summary>Approach sketch</summary>`unite(a, b, 1)` for every edge; a `false` return means an odd cycle ⇒ `IMPOSSIBLE`. Otherwise the team of `v` is `1 + parity(v relative to root)`. Note that the DSU version also works when edges arrive online, unlike BFS.</details>

### 08.5  MST Edge Check  ·  CSES 3407  ·  ★3
https://cses.fi/problemset/task/3407
**Technique:** Kruskal by weight groups.
<details><summary>Hint</summary>An edge of weight `w` is in some MST iff its endpoints are in different components after all edges of weight `< w` are united.</details>
<details><summary>Approach sketch</summary>Sort edges by weight. For each group of equal weight: first test every edge in the group against the DSU (record yes/no), then unite the whole group. Testing before uniting within a group is what makes equal weights correct.</details>

### 08.6  MST Edge Cost  ·  CSES 3409  ·  ★4
https://cses.fi/problemset/task/3409
**Technique:** MST + maximum edge on tree path (binary lifting / Kruskal tree).
<details><summary>Hint</summary>Forcing edge `(u, v, w)` into the tree removes the heaviest edge on the MST path between `u` and `v`.</details>
<details><summary>Approach sketch</summary>Build one MST with Kruskal. For every input edge, answer `MST − maxEdge(u, v) + w` (for tree edges the answer is `MST`). Path maximum: binary lifting with `up[k][v]` and `mx[k][v]` (ch. 10), or the Kruskal reconstruction tree where the answer is the value at `LCA(u, v)`.</details>

### 08.7  Path Queries  ·  Codeforces 1213G  ·  ~1800
https://codeforces.com/problemset/problem/1213/G
**Technique:** Sort edges and queries by weight; DSU with sizes; count pairs.
<details><summary>Hint</summary>Uniting components of sizes `s` and `t` creates `s · t` new pairs.</details>
<details><summary>Approach sketch</summary>Sort queries by `q_i`; sweep edges in weight order, maintaining `pairs += sz[a] · sz[b]` on every successful union; answer each query when all edges `≤ q_i` are in. `long long`.</details>

---

## B. Deletions, time, and rollback

### 08.8  Network Breakdown  ·  CSES 1677  ·  ★3
https://cses.fi/problemset/task/1677
**Technique:** Reverse time + DSU.
<details><summary>Hint</summary>Deleting is hard; start from the graph with all deleted edges removed and add them back in reverse order.</details>
<details><summary>Approach sketch</summary>Mark the `k` removed edges. Build a DSU from the surviving edges. Process removals backwards: record `comps`, then unite the edge. Print the recorded counts in forward order.</details>

### 08.9  New Roads Queries  ·  CSES 2101  ·  ★4
https://cses.fi/problemset/task/2101
**Technique:** Time-stamped union forest (O(log n) per query), or Kruskal reconstruction tree + LCA, or parallel binary search. Implement at least two.
<details><summary>Hint</summary>With union by size and no compression, the union forest has depth ≤ log n and edge timestamps increase upwards.</details>
<details><summary>Approach sketch</summary>Union forest: lift the endpoint with the smaller timestamp until the two meet; the answer is the largest timestamp seen (−1 if they end at different roots). PBS: for each round, sort queries by mid, rebuild the DSU sweeping the edges once, and halve the intervals; `log m` rounds.</details>

### 08.10  Dynamic Connectivity  ·  CSES 2133  ·  ★4
https://cses.fi/problemset/task/2133
**Technique:** Segment tree over time + rollback DSU.
<details><summary>Hint</summary>Each edge's lifetime is a union of time intervals; put each interval on O(log q) tree nodes.</details>
<details><summary>Approach sketch</summary>Compute intervals with a `map<edge, add_time>`. DFS the time tree: unite the node's edges, recurse, answer `comps` at leaves that are queries, roll back. O(q log q log n). Write the brute force first (rebuild DSU per query) to test on random inputs.</details>

### 08.11  Bipartite Checking  ·  Codeforces 813F  ·  ~2500
https://codeforces.com/problemset/problem/813/F
**Technique:** Segment tree over time + parity DSU with rollback.
<details><summary>Hint</summary>Toggle = each edge alive on intervals between consecutive toggles; the DSU must detect odd cycles and undo.</details>
<details><summary>Approach sketch</summary>Same skeleton as 08.10 with a parity rollback DSU (store the old `par[rb]` implicitly — it becomes irrelevant when `p[rb] = rb`). If a union at node `v` finds a contradiction, every leaf below `v` answers NO; skip the recursion but still roll back what was applied.</details>

### 08.12  Extending Set of Points  ·  Codeforces 1140F  ·  ~2600
https://codeforces.com/problemset/problem/1140/F
**Technique:** Offline dynamic connectivity on a bipartite "x-coordinates vs y-coordinates" graph, maintaining `Σ (#x in component) · (#y in component)`.
<details><summary>Hint</summary>A point `(x, y)` is an edge between vertex `x` and vertex `n + y`; the closure of a component is the full bipartite product.</details>
<details><summary>Approach sketch</summary>Each toggle defines lifetime intervals. Rollback DSU keeps per-root counts `cx, cy`; the answer changes by `(cx_a + cx_b)(cy_a + cy_b) − cx_a cy_a − cx_b cy_b` per union, undone on rollback. Read the answer at every leaf.</details>

### 08.13  Graph and Queries  ·  Codeforces 1416D  ·  ~2600
https://codeforces.com/problemset/problem/1416/D
**Technique:** Reverse-time DSU to build the Kruskal reconstruction tree; then Euler tour + max segment tree.
<details><summary>Hint</summary>Deletions in reverse are unions; each union creates a "component vertex" whose subtree is the component at that time.</details>
<details><summary>Approach sketch</summary>Build the reconstruction tree offline. Every query "max in component of v at time t" becomes "max over the Euler-tour range of the appropriate ancestor of v" — find that ancestor by comparing creation times (binary lifting). Point-set the taken value to 0. Hard; do it after ch. 10.</details>

### 08.14  Qpwoeirut and Vertices  ·  Codeforces 1706E  ·  ~2300
https://codeforces.com/problemset/problem/1706/E
**Technique:** Kruskal reconstruction tree over edge indices + range max over consecutive pairs.
<details><summary>Hint</summary>Vertices `l..r` are all connected iff every consecutive pair `(i, i+1)` is; "first edge index connecting `i` and `i+1`" is an LCA value.</details>
<details><summary>Approach sketch</summary>Compute `f(i) = time when i and i+1 become connected` via the union forest or reconstruction tree LCA (n − 1 queries). A query `(l, r)` is `max f(l..r-1)` — sparse table. Answer 0 if `l = r`.</details>

### 08.15  Meteors  ·  POI 2011 (SPOJ METEORS)  ·  ★4
https://www.spoj.com/problems/METEORS/
**Technique:** Parallel binary search + Fenwick range add / point query.
<details><summary>Hint</summary>For each state, "has it collected enough after the first `t` showers" is monotone in `t`; evaluate all mids with one sweep of showers.</details>
<details><summary>Approach sketch</summary>Each round: sort states by mid, sweep showers applying circular range adds to a Fenwick, and for each state sum its sectors (point queries) when the sweep reaches its mid — cap the sum to avoid overflow. `log k` rounds × O((k + n + m) log m).</details>

---

## C. Subtree queries: Sack and small-to-large

### 08.16  Distinct Colors  ·  CSES 1139  ·  ★3
https://cses.fi/problemset/task/1139
**Technique:** DSU on tree (Sack) — and once more with small-to-large sets; compare runtimes.
<details><summary>Hint</summary>Keep the heavy child's counters; re-add light subtrees via Euler-tour ranges.</details>
<details><summary>Approach sketch</summary>Sack as in the lesson: `cnt[colour]`, `distinct`. Colours up to 10^9 ⇒ compress first (or size `cnt` by the max compressed colour). Small-to-large: `set<int>` per vertex, swap-then-merge, O(n log² n). Iterative Euler tour avoids stack issues on a path graph.</details>

### 08.17  Fixed-Length Paths I  ·  CSES 2080  ·  ★4
https://cses.fi/problemset/task/2080
**Technique:** Small-to-large merging of depth-count vectors (or centroid decomposition, ch. 10).
<details><summary>Hint</summary>A path of length `k` through `v` pairs a vertex at depth `d` in one child subtree with one at depth `k − d` in the already-merged part.</details>
<details><summary>Approach sketch</summary>For each vertex keep `cnt[depth]` as a vector indexed from the deepest child (the "long path" trick makes the heavy child's vector reusable with an offset). Merge smaller vectors into the larger, counting pairs summing to `k` before merging. O(n log n) or O(n) with the long-path decomposition; centroid decomposition is the safer general tool.</details>

### 08.18  Lomsat gelral  ·  Codeforces 600E  ·  ~2300
https://codeforces.com/problemset/problem/600/E
**Technique:** Sack with `cnt[colour]`, `freq[count]` and running "sum of colours with maximum count".
<details><summary>Hint</summary>When a colour's count rises to a new maximum, the sum resets to that colour; when it ties, add it.</details>
<details><summary>Approach sketch</summary>Maintain `mx` and `sumAtMx` during `add`; removal happens only when clearing a light subtree, so reset `mx = sumAtMx = 0` when the counters become empty (no need to support decreasing `mx` gracefully). `long long`.</details>

### 08.19  Tree and Queries  ·  Codeforces 375D  ·  ~2400
https://codeforces.com/problemset/problem/375/D
**Technique:** Sack (or Mo's on the Euler tour) with `cnt[colour]` and `ge[k] = number of colours with count ≥ k`.
<details><summary>Hint</summary>Increasing a colour's count from `c` to `c+1` increments `ge[c+1]` only.</details>
<details><summary>Approach sketch</summary>Attach queries to vertices; answer `ge[k]` when the subtree of `v` is fully present. `add`/`rem` update `ge` in O(1). Sack: O((n + q) log n).</details>

### 08.20  Tree Requests  ·  Codeforces 570D  ·  ~2200
https://codeforces.com/problemset/problem/570/D
**Technique:** Sack with per-depth letter masks (xor of bits), or offline by depth with Euler tour ranges.
<details><summary>Hint</summary>A multiset of letters forms a palindrome iff at most one letter has odd count — xor of `1 << letter` over the depth has ≤ 1 bit set.</details>
<details><summary>Approach sketch</summary>Keep `mask[depth]` under Sack add/remove (xor is its own inverse). Alternative without Sack: for each depth store vertices in Euler order with prefix xors; a query is one xor of two prefix values. Both O((n + q) log n) or better.</details>

---

## D. Offline sweeps with a Fenwick

### 08.21  Distinct Values Queries  ·  CSES 1734  ·  ★3
https://cses.fi/problemset/task/1734
**Technique:** Sort by `r`, Fenwick with a 1 at each value's last occurrence. (You solved it with Mo's in ch. 07 — compare the runtimes.)
<details><summary>Hint</summary>Every value present in `[l, r]` has exactly one last occurrence inside `[l, r]`.</details>
<details><summary>Approach sketch</summary>Sweep `r`; when `a[r]` was seen before at `p`, `add(p, −1)`; `add(r, +1)`. Answer `sum(l, r)` for all queries with this `r`. Compress values into an array instead of `map` for speed.</details>

### 08.22  Nested Ranges Check  ·  CSES 2168  ·  ★2
https://cses.fi/problemset/task/2168
**Technique:** Sort + running min/max (a degenerate sweep).
<details><summary>Hint</summary>Sort by `l` ascending, `r` descending; range `i` contains something after it iff the minimum `r` among later ranges is `≤ r_i`.</details>
<details><summary>Approach sketch</summary>One backward pass with a suffix minimum for "contains", one forward pass with a prefix maximum for "is contained". The tie-break makes equal ranges count. The counting version (CSES 2169) needs the Fenwick — ch. 07.</details>

### 08.23  Intersection Points  ·  CSES 1740  ·  ★3
https://cses.fi/problemset/task/1740
**Technique:** Sweep line over `x` + Fenwick over `y`.
<details><summary>Hint</summary>Horizontal segments are `+1`/`−1` events at their endpoints; vertical segments are range-count queries.</details>
<details><summary>Approach sketch</summary>Compress `y`. Events sorted by `x` with order insert < query < delete at equal `x`. A vertical segment at `x` with `[y1, y2]` adds `sum(y1, y2)` to the answer. O((n + m) log n), `long long` answer.</details>

### 08.24  Area of Rectangles  ·  CSES 1741  ·  ★4
https://cses.fi/problemset/task/1741
**Technique:** Sweep over `x` + segment tree over compressed `y` with `(cover count, covered length)`.
<details><summary>Hint</summary>Between two consecutive `x` events, the covered `y`-length is constant.</details>
<details><summary>Approach sketch</summary>Node stores `cnt` (how many rectangles cover the entire node range) and `len` (covered length within it): `len = cnt > 0 ? node_range : len[left] + len[right]`. Range `+1`/`−1` without push-down (the cover count is only ever queried at the root). Add `len[root] · Δx` per event gap.</details>

### 08.25  Moving Points  ·  Codeforces 1311F  ·  ~1900
https://codeforces.com/problemset/problem/1311/F
**Technique:** Sort by velocity, Fenwick over positions (2D dominance counting with weights).
<details><summary>Hint</summary>Two points never get closer if the left one is not faster; their minimal distance is the initial one.</details>
<details><summary>Approach sketch</summary>Sort by `v` (ties by `x`). For each point, the answer accumulates `x_i · (count of earlier points with x ≤ x_i) − (sum of their x)` — two Fenwicks (count, sum) over compressed `x`. `long long`.</details>

---

## E. Sqrt techniques

### 08.26  Sliding Window Distinct Values  ·  CSES 3222  ·  ★1
https://cses.fi/problemset/task/3222
**Technique:** Frequency array with add/remove (the primitive behind Mo's).
<details><summary>Hint</summary>`distinct` changes only when a count crosses zero.</details>
<details><summary>Approach sketch</summary>Compress values; slide the window with `add(a[i+k])`, `rem(a[i])`. O(n). Keep this exact add/remove pair — it is reused verbatim in Mo's and Sack.</details>

### 08.27  Sliding Window Mode  ·  CSES 3224  ·  ★3
https://cses.fi/problemset/task/3224
**Technique:** Frequency buckets: `cnt[value]`, `freq[c]`, and a running maximum count that changes by ±1.
<details><summary>Hint</summary>When the element leaving the window was the only one at the maximum count, the maximum drops by exactly one.</details>
<details><summary>Approach sketch</summary>Maintain `mx = max count`. On add: `if (++cnt[v] > mx) mx++`. On remove: `if (cnt[v] == mx && freq[mx] == 1) mx--` before decrementing. The mode value itself (smallest among ties) needs a per-count structure — a `set` per `freq` bucket or a smarter ordering; read the statement for the tie rule.</details>

### 08.28  XOR and Favorite Number  ·  Codeforces 617E  ·  ~2200
https://codeforces.com/problemset/problem/617/E
**Technique:** Mo's algorithm on prefix xors.
<details><summary>Hint</summary>Subarray xor `= px[r] ^ px[l-1]`; count pairs in the window of prefix values with xor `k`.</details>
<details><summary>Approach sketch</summary>Window over prefix indices `[l-1, r]`. `add(i)`: `ans += cnt[px[i] ^ k]; cnt[px[i]]++`; `rem` symmetric. `cnt` array of size `2^20`. Odd-even sorting order.</details>

### 08.29  Machine Learning  ·  Codeforces 940F  ·  ~2600
https://codeforces.com/problemset/problem/940/F
**Technique:** Mo's with updates + mex of counts.
<details><summary>Hint</summary>The mex of occurrence counts is at most O(√n) because counts summing to `n` cannot all be distinct beyond that.</details>
<details><summary>Approach sketch</summary>Three-pointer Mo's with `B = n^{2/3}`. Maintain `cnt[value]` and `freq[count]`; the mex of `freq` is found by scanning from 1 — O(√n) per query, which is dominated by pointer movement. Compress values including all update values.</details>

### 08.30  Park  ·  BOI 2016  ·  ★4
BOI 2016 Day 1 "Park" (available on oj.uz).
**Technique:** Offline DSU over obstacles sorted by "blocking radius"; queries sorted by visitor size.
<details><summary>Hint</summary>Two trees (or a tree and a wall) block a visitor of diameter `d` iff their gap is `< d`; process gaps in increasing order and unite obstacles; the four walls are vertices.</details>
<details><summary>Approach sketch</summary>Compute all pairwise gaps (n ≤ 2000 ⇒ 2·10^6 pairs) and sort them together with the queries by size. Sweep: unite obstacles whose gap is too small for the current visitor. A corner is reachable from another iff no united chain of obstacles connects the walls separating them — check which wall pairs are connected.</details>

---

Practice more: Codeforces problemset, tag `dsu`, rating 1500–2000 (basic and parity DSU), then
`dsu` + `data structures` at 2000–2300 (rollback, offline connectivity, Kruskal tree).
Practice more: Codeforces problemset, tag `data structures` + `trees`, rating 2100–2400 for Sack /
small-to-large.
Practice more: Codeforces problemset, tag `binary search` + `data structures`, rating 2200–2500
for parallel binary search; search the problemset for "sqrt decomposition" educational rounds
for heavy/light threshold problems.

## Progress

- [ ] 08.1 Building Roads (CSES 1666)
- [ ] 08.2 Road Construction (CSES 1676)
- [ ] 08.3 Road Reparation (CSES 1675)
- [ ] 08.4 Building Teams (CSES 1668)
- [ ] 08.5 MST Edge Check (CSES 3407)
- [ ] 08.6 MST Edge Cost (CSES 3409)
- [ ] 08.7 Path Queries (CF 1213G)
- [ ] 08.8 Network Breakdown (CSES 1677)
- [ ] 08.9 New Roads Queries (CSES 2101)
- [ ] 08.10 Dynamic Connectivity (CSES 2133)
- [ ] 08.11 Bipartite Checking (CF 813F)
- [ ] 08.12 Extending Set of Points (CF 1140F)
- [ ] 08.13 Graph and Queries (CF 1416D)
- [ ] 08.14 Qpwoeirut and Vertices (CF 1706E)
- [ ] 08.15 Meteors (POI 2011 / SPOJ METEORS)
- [ ] 08.16 Distinct Colors (CSES 1139)
- [ ] 08.17 Fixed-Length Paths I (CSES 2080)
- [ ] 08.18 Lomsat gelral (CF 600E)
- [ ] 08.19 Tree and Queries (CF 375D)
- [ ] 08.20 Tree Requests (CF 570D)
- [ ] 08.21 Distinct Values Queries (CSES 1734)
- [ ] 08.22 Nested Ranges Check (CSES 2168)
- [ ] 08.23 Intersection Points (CSES 1740)
- [ ] 08.24 Area of Rectangles (CSES 1741)
- [ ] 08.25 Moving Points (CF 1311F)
- [ ] 08.26 Sliding Window Distinct Values (CSES 3222)
- [ ] 08.27 Sliding Window Mode (CSES 3224)
- [ ] 08.28 XOR and Favorite Number (CF 617E)
- [ ] 08.29 Machine Learning (CF 940F)
- [ ] 08.30 Park (BOI 2016)
