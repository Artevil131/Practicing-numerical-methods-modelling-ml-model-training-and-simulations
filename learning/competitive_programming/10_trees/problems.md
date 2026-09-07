# Chapter 10 — Problems

How to work this ladder:

1. Re-type `Rooted`, `LCA`, `EulerTour + Fenwick`, `HLD`, `CentroidDecomp`, the rerooting sweeps and `KruskalTree` from memory first (see `example.cpp`). Do not open the file while solving.
2. One file per problem: `competitive_programming/10_trees/solutions/<source>_<id>.cpp` (e.g. `cses_1135.cpp`, `cf_342E.cpp`, `ioi2011_race.cpp`).
3. Stress-test against brute force on random trees of size ≤ 50 (`randomTree` from `example.cpp`) before the first submission — including a path and a star.
4. Time yourself: ★1-2 → 15 min, ★3 → 30 min, ★4 → 60 min, ★5 → 90 min. Then hint → approach → implement anyway. Upsolve within 48 hours.
5. The CSES Tree Algorithms section (10.1-10.16) is the backbone: finish it completely before the extras.

Difficulty: ★1 (CF ~1200) … ★5 (CF ~2400+).

---

## A. Tree DP, diameter, rerooting

### 10.1  Subordinates  ·  CSES 1674  ·  ★1
https://cses.fi/problemset/task/1674
**Technique:** subtree sizes, reverse-preorder sweep.
<details><summary>Hint</summary>The boss of employee `i` has a smaller index. What does that say about the order `1..n`?</details>
<details><summary>Approach sketch</summary>Since `boss[i] < i`, the sequence `1..n` is already a valid preorder. `sub[i] = 1`; for `i = n..2`: `sub[boss[i]] += sub[i]`. Answer `sub[i] − 1`. No DFS at all. `O(n)`.</details>

### 10.2  Tree Matching  ·  CSES 1130  ·  ★2
https://cses.fi/problemset/task/1130
**Technique:** tree DP with two states, or leaf-first greedy.
<details><summary>Hint</summary>A leaf should always be matched with its parent if the parent is still free. Why is that never wrong?</details>
<details><summary>Approach sketch</summary>Greedy in reverse preorder: if `v` is unmatched and its parent is unmatched, match them. Exchange argument: any maximum matching that leaves leaf `v` unmatched can be modified to match `v` with its parent without losing size. Or DP: `dp[v][0/1]` (v free / v matched to a child) with the subtract-per-child trick. `O(n)`.</details>

### 10.3  Tree Diameter  ·  CSES 1131  ·  ★2
https://cses.fi/problemset/task/1131
**Technique:** two BFS, or two-best-depths DP.
<details><summary>Hint</summary>Farthest vertex from an arbitrary start is a diameter endpoint (lesson §3 proof).</details>
<details><summary>Approach sketch</summary>BFS from 0, take the farthest `a`; BFS from `a`, the maximum distance is the answer. Implement the DP version too — it is what you extend in Tree Distances I. `O(n)`.</details>

### 10.4  Tree Distances I  ·  CSES 1132  ·  ★3
https://cses.fi/problemset/task/1132
**Technique:** rerooting with two best depths, or three BFS from the diameter endpoints.
<details><summary>Hint</summary>The farthest vertex from any `v` is one of the two endpoints of a diameter.</details>
<details><summary>Approach sketch</summary>Find a diameter `(a, b)` with two BFS; run a third BFS from `b`; `ans[v] = max(dist_a[v], dist_b[v])`. Then do it again with rerooting: `down1/down2/who` bottom-up, `up[u] = 1 + max(up[p], down1[p] or down2[p] if who[p] == u)` top-down. `O(n)`.</details>

### 10.5  Tree Distances II  ·  CSES 1133  ·  ★3
https://cses.fi/problemset/task/1133
**Technique:** rerooting DP, invertible combine.
<details><summary>Hint</summary>Moving the root across an edge changes the sum of distances by `(n − sub[v]) − sub[v]`.</details>
<details><summary>Approach sketch</summary>`down[p] += down[v] + sub[v]` bottom-up; `ans[root] = down[root]`; `ans[v] = ans[par] − sub[v] + (n − sub[v])` top-down. `long long`. `O(n)`.</details>

### 10.6  Finding a Centroid  ·  CSES 2079  ·  ★2
https://cses.fi/problemset/task/2079
**Technique:** centroid by walking into the heavy child.
<details><summary>Hint</summary>From the root, while a child has `sub > n/2`, move into it.</details>
<details><summary>Approach sketch</summary>Compute `sub` with an iterative DFS; walk from the root as in lesson §4. Any centroid is accepted. `O(n)`.</details>

### 10.7  Tree Isomorphism I  ·  CSES 1700  ·  ★3
https://cses.fi/problemset/task/1700
**Technique:** AHU canonical ids for rooted trees.
<details><summary>Hint</summary>The id of a vertex is the sorted multiset of its children's ids; intern it in a `map<vector<int>, int>` shared by both trees.</details>
<details><summary>Approach sketch</summary>Root both trees at 1, compute ids in reverse preorder, compare `id[root1] == id[root2]`. Sharing one dictionary between both trees is what makes ids comparable. `O(n log n)`.</details>

### 10.8  Tree Isomorphism II  ·  CSES 1701  ·  ★3
https://cses.fi/problemset/task/1701
**Technique:** AHU at the centroid(s).
<details><summary>Hint</summary>Unrooted trees: where is the canonical place to root? What if there are two candidates?</details>
<details><summary>Approach sketch</summary>Root each tree at its centroid; if a tree has two centroids compute both ids. Isomorphic iff some id of tree 1 equals some id of tree 2 (the mapping sends centroids to centroids). `O(n log n)`.</details>

### 10.9  Prüfer Code  ·  CSES 1134  ·  ★3
https://cses.fi/problemset/task/1134
**Technique:** decoding a Prüfer sequence with a `set`/heap of leaves (Chapter 09 tools on a tree).
<details><summary>Hint</summary>The degree of vertex `v` in the tree is `1 + (occurrences of v in the code)`. Repeatedly connect the smallest leaf to the next code element.</details>
<details><summary>Approach sketch</summary>Compute degrees; keep the smallest current leaf (min-heap or a pointer trick for `O(n)`); for each code element `c`: connect `leaf` to `c`, decrement `deg[c]`, if it became 1 it is a new leaf. Finally connect the last two remaining vertices. `O(n log n)`.</details>

### 10.10  Creating Offices  ·  CSES 1752  ·  ★4
https://cses.fi/problemset/task/1752
**Technique:** greedy on a tree processed deepest-first (rerooting flavour: cover every vertex within distance `d`).
<details><summary>Hint</summary>Process vertices by decreasing depth. If a vertex is not yet covered, where should the office serving it be placed to cover as much as possible?</details>
<details><summary>Approach sketch</summary>Sort by depth descending. For an uncovered vertex `v`, place an office at its `d`-th ancestor (or the root if shallower) — the placement that reaches as high as possible while still covering `v` — and mark everything within distance `d` of that office covered (BFS bounded by `d`; each vertex is marked a bounded number of times if you stop at already-covered vertices with a closer-or-equal office). Track `cover[u]` = remaining radius to prune. `O(n)` amortised with the pruning. Prove optimality by exchange on the deepest uncovered vertex.</details>

### 10.11  Network Renovation  ·  CSES 1704  ·  ★4
https://cses.fi/problemset/task/1704
**Technique:** leaves in DFS order, pairing across the halves.
<details><summary>Hint</summary>Making a tree 2-edge-connected needs `⌈L/2⌉` edges where `L` is the number of leaves. Which leaves do you pair?</details>
<details><summary>Approach sketch</summary>Root at a non-leaf, list leaves in preorder `ℓ_0..ℓ_{L−1}`, connect `ℓ_i` with `ℓ_{i+⌈L/2⌉}` (and the middle one with any other leaf when `L` is odd). Every edge then lies on a cycle because each subtree's leaves form a contiguous block of the order and the pairing crosses every such block. Output count and pairs. `O(n)`.</details>

---

## B. Euler tour, LCA, path decomposition

### 10.12  Company Queries I  ·  CSES 1687  ·  ★2
https://cses.fi/problemset/task/1687
**Technique:** binary lifting, k-th ancestor.
<details><summary>Hint</summary>`up[j][v]` for `j < 18`; decompose `k` in binary; `k > depth` → −1.</details>
<details><summary>Approach sketch</summary>Parents are given (`boss[i] < i`), so build `up[0]` directly and the table in `O(n log n)`; each query jumps over the set bits of `k`. Return −1 when you reach the root sentinel. `O((n + q) log n)`.</details>

### 10.13  Company Queries II  ·  CSES 1688  ·  ★3
https://cses.fi/problemset/task/1688
**Technique:** LCA by binary lifting.
<details><summary>Hint</summary>Equalise depths, then jump both from the top bit while `up[k][a] != up[k][b]`.</details>
<details><summary>Approach sketch</summary>Lesson §6 verbatim. Also implement the Euler-tour + sparse-table version and compare speeds on `q = 2e5`. `O((n + q) log n)` or `O(n log n + q)`.</details>

### 10.14  Distance Queries  ·  CSES 1135  ·  ★3
https://cses.fi/problemset/task/1135
**Technique:** LCA + depths.
<details><summary>Hint</summary>`dist = depth[a] + depth[b] − 2·depth[lca]`.</details>
<details><summary>Approach sketch</summary>Edge list this time → iterative DFS for depth and parent, then lifting. Check the two-vertex case and `a == b`. `O((n + q) log n)`.</details>

### 10.15  Counting Paths  ·  CSES 1136  ·  ★3
https://cses.fi/problemset/task/1136
**Technique:** LCA + difference on the tree.
<details><summary>Hint</summary>Add `+1` at both endpoints, `−1` at the LCA and `−1` at the LCA's parent; then subtree sums.</details>
<details><summary>Approach sketch</summary>For each path, apply the four point increments; then `cnt[v] = Σ` over the subtree in reverse preorder. Each vertex of the path is counted once: it is in the subtree of exactly one endpoint but not of the parent-of-LCA (unless it is above the LCA, where the −1's cancel the +1's). `O((n + m) log n)`.</details>

### 10.16  Subtree Queries  ·  CSES 1137  ·  ★3
https://cses.fi/problemset/task/1137
**Technique:** Euler tour + Fenwick.
<details><summary>Hint</summary>Subtree of `v` = positions `[tin[v], tin[v] + sub[v])`.</details>
<details><summary>Approach sketch</summary>Point set = add the difference at `tin[v]`; subtree sum = Fenwick range sum. `long long`. `O((n + q) log n)`.</details>

### 10.17  Path Queries  ·  CSES 1138  ·  ★3
https://cses.fi/problemset/task/1138
**Technique:** Euler tour + Fenwick with range add / point query.
<details><summary>Hint</summary>Changing `val[v]` changes the root-path sum of every vertex in `v`'s subtree by the same delta.</details>
<details><summary>Approach sketch</summary>Keep `S[u]` = sum from root to `u` as a Fenwick supporting range add (add `delta` on `[tin[v], tout[v])`) and point query at `tin[u]`. Initialise by adding each vertex's value on its subtree range. `O((n + q) log n)`.</details>

### 10.18  Path Queries II  ·  CSES 2134  ·  ★4
https://cses.fi/problemset/task/2134
**Technique:** heavy-light decomposition + max segment tree, point updates.
<details><summary>Hint</summary>Vertex values, max on a path, updates → HLD. Iterative segment tree keeps the constant small.</details>
<details><summary>Approach sketch</summary>`HLD` from the lesson with `SegMax`; `pathMax(a, b)` walks chains until the heads coincide. `O(q log² n)` ≈ 6e7 simple steps — fine in 1 s if the segment tree is iterative and I/O is fast.</details>

### 10.19  Distinct Colors  ·  CSES 1139  ·  ★4
https://cses.fi/problemset/task/1139
**Technique:** Euler tour → "distinct values in range" offline (Chapter 09 problem 09.12), or small-to-large merging of `set`s.
<details><summary>Hint</summary>After flattening, the question "distinct colours in the subtree of `v`" is exactly "distinct values in `[tin[v], tout[v])`".</details>
<details><summary>Approach sketch</summary>Method 1: Euler tour, then the offline sweep with a Fenwick over last occurrences, queries sorted by right endpoint. Method 2: each vertex owns a `set<int>` of colours; merge children into the largest child's set (`swap` to keep the big one), record the size. Both `O(n log n)`–`O(n log² n)`. Do both; note that method 2 is the "DSU on tree" idea.</details>

### 10.20  Movie Festival Queries  ·  CSES 1664  ·  ★4
https://cses.fi/problemset/task/1664
**Technique:** binary lifting on a successor function (functional graph, not a tree).
<details><summary>Hint</summary>From time `t`, the best next movie is the one with the smallest end time among those starting `≥ t`; `next[t]` defined on compressed times is a function — lift over it.</details>
<details><summary>Approach sketch</summary>Compress all times; `nxt[t]` = min end time over movies with start `≥ t` (suffix minimum). Build `up[k][t]`. For a query `[a, b]`: greedily jump from `a` by the largest powers of two while `up[k][t] ≤ b`, counting movies. `O((n + q) log n)`.</details>

### 10.21  Planets Queries I  ·  CSES 1750  ·  ★2
https://cses.fi/problemset/task/1750
**Technique:** binary lifting on a functional graph (k up to `1e9`).
<details><summary>Hint</summary>`up[k][v]` for `k < 30`; `k` in binary.</details>
<details><summary>Approach sketch</summary>Exactly `kth` without the depth check — every vertex has a successor so the jump always exists. `O((n + q) · 30)`.</details>

### 10.22  Planets Queries II  ·  CSES 1160  ·  ★4
https://cses.fi/problemset/task/1160
**Technique:** functional graph = cycles with in-trees; distance via depth-to-cycle, cycle positions, and lifting/ancestor checks.
<details><summary>Hint</summary>Decompose into cycles and trees hanging off them. Three cases: `b` on the tree path of `a`, `b` on `a`'s cycle, otherwise unreachable.</details>
<details><summary>Approach sketch</summary>Find cycles (colouring walk), assign each vertex its cycle id, its cycle position, and its distance to the cycle entry. If `a` and `b` are in the same in-tree part and `b` is an ancestor of `a` (check with lifting: `kth(a, depth[a] − depth[b]) == b`), answer the depth difference. If `b` is on the cycle of `a`'s component, answer `dist_to_cycle[a] + (pos[b] − pos[entry(a)] mod len)`. Else −1. `O((n + q) log n)`.</details>

---

## C. Centroid decomposition

### 10.23  Fixed-Length Paths I  ·  CSES 2080  ·  ★4
https://cses.fi/problemset/task/2080
**Technique:** centroid decomposition, count pairs at distance exactly `k`.
<details><summary>Hint</summary>At a centroid, process child subtrees one at a time so both ends land in different subtrees.</details>
<details><summary>Approach sketch</summary>Lesson §12 `build`. Use a global `cnt` array and clear only the touched depths, or a per-component vector. Test on a path with `k = 1` (answer `n − 1`). `O(n log n)`.</details>

### 10.24  Fixed-Length Paths II  ·  CSES 2081  ·  ★5
https://cses.fi/problemset/task/2081
**Technique:** centroid decomposition + prefix sums over depths (range `[k1, k2]`).
<details><summary>Hint</summary>For a vertex at depth `d`, you need `Σ_{j = k1 − d}^{k2 − d} cnt[j]` over previous subtrees.</details>
<details><summary>Approach sketch</summary>Keep `cnt` as a prefix-sum-able array. Simple version: Fenwick over depths, `O(n log² n)`. Faster: process child subtrees in increasing order of their max depth and maintain a plain prefix-sum array rebuilt only up to the current max depth — total `O(n log n)`. Watch the `k1 − d` lower bound clamping to 0.</details>

### 10.25  Race  ·  IOI 2011  ·  ★5
https://oj.uz/problem/view/IOI11_race
**Technique:** centroid decomposition with a global table indexed by path weight.
<details><summary>Hint</summary>Fewest edges on a path of total weight exactly `K`: at a centroid, for each child subtree, query `bestEdges[K − w]` for every (weight `w`, edges `e`) in the subtree, then insert the subtree's pairs.</details>
<details><summary>Approach sketch</summary>`best[w]` = min number of edges of a path from the centroid with weight `w` among processed subtrees (`best[0] = 0`). For each child subtree collect `(w, e)` with `w ≤ K`; update the answer with `e + best[K − w]`; then insert `best[w] = min(best[w], e)`. Reset the touched entries after the centroid. `O(n log n)` with `K ≤ 1e6` array. Distances up to `1e6` → fine in `int`; prune subtrees paths with `w > K` during collection.</details>

### 10.26  Distance in Tree  ·  Codeforces 161D  ·  ★3 (CF 1800)
https://codeforces.com/contest/161/problem/D
**Technique:** count pairs at distance `k` — small `k ≤ 500` allows `dp[v][d]`; also solvable by centroid decomposition.
<details><summary>Hint</summary>`dp[v][d]` = number of vertices at depth `d` in the subtree of `v`; combine children pairwise via the running total.</details>
<details><summary>Approach sketch</summary>For each `v`, for each child `u`, add `Σ_d dp[v][d] · dp[u][k − 1 − d]` (pairs across previously merged children plus `v` itself), then merge `dp[u]` shifted by 1 into `dp[v]`. `O(n · k)`. Then solve it again with centroid decomposition as a warm-up for 10.23.</details>

### 10.27  Xenia and Tree  ·  Codeforces 342E  ·  ★4 (CF 2400)
https://codeforces.com/contest/342/problem/E
**Technique:** centroid decomposition, nearest marked vertex online.
<details><summary>Hint</summary>Store each vertex's distance to its `O(log n)` ancestor-centroids; mark = update `best[c]` for those; query = min over them of `best[c] + dist(v, c)`.</details>
<details><summary>Approach sketch</summary>Lesson §12 "nearest marked vertex". Precompute `dist(v, c)` during the build (BFS from each centroid inside its component) into a per-vertex list of `(centroid, distance)`. `O((n + q) log n)`. Alternative: sqrt-decomposition over queries with BFS rebuilds — slower, know both.</details>

---

## D. Lifting with aggregates, HLD, Kruskal tree, virtual trees

### 10.28  Minimum spanning tree for each edge  ·  Codeforces 609E  ·  ★4 (CF 2100)
https://codeforces.com/contest/609/problem/E
**Technique:** MST + max edge on a tree path (binary lifting with aggregates).
<details><summary>Hint</summary>Forcing edge `e = (a, b, w)` into the MST costs `MST − maxEdgeOnPath(a, b) + w`.</details>
<details><summary>Approach sketch</summary>Kruskal for the MST, root it, `LiftMax` over MST edge weights; answer each edge with the formula (edges already in the MST give `MST`). `long long`. `O(m log m + m log n)`.</details>

### 10.29  A and B and Lecture Rooms  ·  Codeforces 519E  ·  ★4 (CF 2100)
https://codeforces.com/contest/519/problem/E
**Technique:** LCA, k-th ancestor, subtree sizes — counting equidistant vertices.
<details><summary>Hint</summary>If `dist(a, b)` is odd, answer 0. Otherwise the midpoint `m` of the path is the vertex equidistant from both; count vertices whose nearest-of-{a,b} entry point is `m`.</details>
<details><summary>Approach sketch</summary>Find `m` with `jump(a, b, dist/2)`. If `m` is the LCA, answer `n − sub[childTowardA] − sub[childTowardB]` where the children are found by `kth(a, da − 1)` and `kth(b, db − 1)`; otherwise (say `m` is on `a`'s side) answer `sub[m] − sub[childOfMTowardA]`. Handle `a == b` (answer `n`). `O((n + q) log n)`.</details>

### 10.30  Blood Cousins  ·  Codeforces 208E  ·  ★4 (CF 2100)
https://codeforces.com/contest/208/problem/E
**Technique:** k-th ancestor + Euler tour + counting vertices of a given depth in a range.
<details><summary>Hint</summary>`p`-th cousins of `v` = vertices at `depth[v]` in the subtree of the `p`-th ancestor of `v`, minus `v` itself.</details>
<details><summary>Approach sketch</summary>Forest: add a virtual root or handle each tree. For query `(v, p)`: `u = kth(v, p)`; if none, 0. Else count vertices at `depth[v]` with `tin ∈ [tin[u], tout[u])` — binary search in the sorted `tin` list of each depth. Subtract 1. `O((n + q) log n)`.</details>

### 10.31  Tree Queries  ·  Codeforces 1328E  ·  ★3 (CF 1900)
https://codeforces.com/contest/1328/problem/E
**Technique:** ancestor test via `tin/tout`, deepest vertex trick.
<details><summary>Hint</summary>A set of vertices lies within distance 1 of one root path iff, taking the deepest given vertex `d`, every other vertex `v` (or its parent) is an ancestor of `d`.</details>
<details><summary>Approach sketch</summary>For each query find the deepest vertex `d`; for every `v` check `isAncestor(par[v], d)` (treat `par[root] = root`). `O(Σk)` after the Euler tour. Illustrates that `tin/tout` alone often replaces LCA.</details>

### 10.32  Lomsat gelral  ·  Codeforces 600E  ·  ★4 (CF 2300)
https://codeforces.com/contest/600/problem/E
**Technique:** DSU on tree (small-to-large / Sack) — the general form of Distinct Colors.
<details><summary>Hint</summary>Keep the heavy child's colour counts, re-add the light children's subtrees; each vertex is re-added `O(log n)` times.</details>
<details><summary>Approach sketch</summary>Process the heavy child last and keep its data; for each light child compute and then discard; then re-insert all light subtrees' vertices and `v` itself into the count map, tracking the max count and the sum of colours achieving it. `O(n log n)` with arrays over colours. This is HLD's heavy-child idea used for offline subtree queries.</details>

### 10.33  New Roads Queries  ·  CSES 2101  ·  ★4
https://cses.fi/problemset/task/2101
**Technique:** Kruskal reconstruction tree with time as weight (or DSU small-to-large with timestamps).
<details><summary>Hint</summary>"Earliest time `a` and `b` are connected" is `minimax(a, b)` when each road has weight = its index.</details>
<details><summary>Approach sketch</summary>Process roads in input order; each successful union creates a KRT node with weight = day index. Answer `w[lca(a, b)]` in the KRT with lifting; if `a` and `b` end up in different components, −1 (add a virtual super-root with weight −1 for each final component). `O((n + m) log n + q log n)`.</details>

### 10.34  Transfer Speeds Sum  ·  CSES 3111  ·  ★3
https://cses.fi/problemset/task/3111
**Technique:** Kruskal merge history — sum over pairs of the bottleneck (min edge) on the tree path.
<details><summary>Hint</summary>Process edges in *decreasing* speed. When an edge of speed `s` joins components of sizes `p` and `q`, it is the bottleneck for exactly `p·q` pairs.</details>
<details><summary>Approach sketch</summary>The input is a tree, so every edge merges; sort edges by speed descending, DSU with sizes, accumulate `s · size(a) · size(b)`. `long long` (`1e9 · (2e5)²/2` fits in 64 bits). `O(n log n)`. This is the KRT argument without building the tree.</details>

### 10.35  Railway  ·  BOI 2017  ·  ★4
https://oj.uz/problem/view/BOI17_railway
**Technique:** LCA + difference on the tree (Counting Paths generalised) or a virtual tree per query.
<details><summary>Hint</summary>Sort each ministry's vertices by `tin`; the edges used by the minimal connecting subtree are those on paths between consecutive vertices (cyclically) — each such edge is counted twice, so add `+1` at each vertex, `−1` at each consecutive-pair LCA, take subtree sums, and an edge is used iff the sum on its lower endpoint is positive.</details>
<details><summary>Approach sketch</summary>For each ministry with `k` vertices sorted by `tin`: `+1` at each vertex, `−1` at `lca(v_i, v_{i+1})` for the `k−1` consecutive pairs... precisely: `+1` at every vertex and `−1` at the LCA of each consecutive pair *including* the wrap-around pair, then halve — or the simpler variant: `+1` at each vertex, `−1` at each of the `k − 1` consecutive LCAs, `−1` at the overall LCA; any vertex with positive subtree sum has its parent edge in the Steiner tree. Accumulate per-edge counts over ministries, output edges with count `≥ k`. `O(Σk log n)`.</details>

### 10.36  Practice more
- Codeforces problemset, tag `trees`, rating 1800-2200 (LCA, Euler tour, rerooting).
- Codeforces problemset, tag `trees` + `data structures`, rating 2200-2500 (HLD, centroid decomposition, DSU on tree).
- Codeforces problemset, tag `dp` + `trees`, rating 1900-2300 (rerooting, "answer for every vertex").
- oj.uz: BOI / CEOI / JOI "tree" tagged problems as olympiad-format practice once the CSES section is complete.

---

## Progress

- [ ] 10.1 Subordinates (CSES 1674)
- [ ] 10.2 Tree Matching (CSES 1130)
- [ ] 10.3 Tree Diameter (CSES 1131)
- [ ] 10.4 Tree Distances I (CSES 1132)
- [ ] 10.5 Tree Distances II (CSES 1133)
- [ ] 10.6 Finding a Centroid (CSES 2079)
- [ ] 10.7 Tree Isomorphism I (CSES 1700)
- [ ] 10.8 Tree Isomorphism II (CSES 1701)
- [ ] 10.9 Prüfer Code (CSES 1134)
- [ ] 10.10 Creating Offices (CSES 1752)
- [ ] 10.11 Network Renovation (CSES 1704)
- [ ] 10.12 Company Queries I (CSES 1687)
- [ ] 10.13 Company Queries II (CSES 1688)
- [ ] 10.14 Distance Queries (CSES 1135)
- [ ] 10.15 Counting Paths (CSES 1136)
- [ ] 10.16 Subtree Queries (CSES 1137)
- [ ] 10.17 Path Queries (CSES 1138)
- [ ] 10.18 Path Queries II (CSES 2134)
- [ ] 10.19 Distinct Colors (CSES 1139)
- [ ] 10.20 Movie Festival Queries (CSES 1664)
- [ ] 10.21 Planets Queries I (CSES 1750)
- [ ] 10.22 Planets Queries II (CSES 1160)
- [ ] 10.23 Fixed-Length Paths I (CSES 2080)
- [ ] 10.24 Fixed-Length Paths II (CSES 2081)
- [ ] 10.25 Race (IOI 2011)
- [ ] 10.26 Distance in Tree (CF 161D)
- [ ] 10.27 Xenia and Tree (CF 342E)
- [ ] 10.28 Minimum spanning tree for each edge (CF 609E)
- [ ] 10.29 A and B and Lecture Rooms (CF 519E)
- [ ] 10.30 Blood Cousins (CF 208E)
- [ ] 10.31 Tree Queries (CF 1328E)
- [ ] 10.32 Lomsat gelral (CF 600E)
- [ ] 10.33 New Roads Queries (CSES 2101)
- [ ] 10.34 Transfer Speeds Sum (CSES 3111)
- [ ] 10.35 Railway (BOI 2017)
