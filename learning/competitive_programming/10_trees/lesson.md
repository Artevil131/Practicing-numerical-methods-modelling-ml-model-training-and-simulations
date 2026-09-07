# Chapter 10 — Trees

## What you'll be able to do after this chapter

- Root a tree iteratively (no recursion-depth risk at `n = 2e5`), produce parent / depth / preorder, and run any bottom-up or top-down DP as a linear sweep over the preorder.
- Compute the diameter two ways, find the centroid(s) and the center, and prove why each works.
- Flatten a tree with an Euler tour so that "subtree of `v`" is a contiguous range and "path to root" is a prefix difference — then answer subtree/path queries with a Fenwick or segment tree.
- Write LCA by binary lifting from memory (build `O(n log n)`, query `O(log n)`), with k-th ancestor, "jump to depth", distance, and the k-th vertex on a path; and the `O(1)`-query Euler-tour + sparse-table version.
- Aggregate along paths with binary lifting (max edge on a path), and do full **heavy-light decomposition** with path updates/queries in `O(log² n)`.
- Do **centroid decomposition** and use the "every path is counted at its highest centroid" argument to count paths of length `k` and to answer nearest-marked-vertex queries.
- Do **rerooting DP** to compute an answer for every root in `O(n)`.
- Hash rooted/unrooted trees (AHU), know what virtual trees are for, and build a Kruskal reconstruction tree for bottleneck queries.

Prerequisites: `../../algorithms_learning/05_trees_and_bsts/lesson.md` (traversals, recursion shape) and `../../algorithms_learning/06_graphs/lesson.md` (BFS/DFS, DSU). Segment tree / Fenwick from the range-queries chapter of this course. Not re-taught.

## Where this shows up in contests

| Signal | Technique | Placement |
|---|---|---|
| `n−1` edges, "connected", employees/subordinates, folders | it is a tree: root it, DP over subtrees | CSES Tree Algorithms; CF Div2 C-D |
| "longest path", "farthest node from every node" | diameter, rerooting | CF Div2 D, BOI |
| "for every vertex, sum/max over all others" | rerooting DP | CF Div2 D-E |
| "sum over subtree of `v`", "change value of a node" | Euler tour + Fenwick/segtree | CSES Subtree Queries, CF Div2 D |
| "distance between `a` and `b`", "k-th ancestor", "is `a` an ancestor of `b`" | LCA (lifting / Euler+RMQ), tin/tout | CSES Company Queries, CF Div2 C-E |
| "max/sum on the path `a→b`" with point updates | HLD, or lifting if static | CSES Path Queries II, CF Div1 B-C, IOI subtasks |
| "count paths with property P (length k, sum ≤ s, …)" | centroid decomposition | CSES Fixed-Length Paths, IOI 2011 Race, CF Div1 C-D |
| "nearest marked vertex", online marking | centroid decomposition with ancestor-centroid arrays | CF Div1 C (Xenia and Tree) |
| "are two trees the same up to relabeling" | AHU hashing | CSES Tree Isomorphism |
| queries on `k` specified vertices, `Σk ≤ 2e5` | virtual (auxiliary) tree | CF Div1 D-E, IOI 2018-type subtasks |
| "min over paths of max edge", "reachable with edges ≤ w" | Kruskal reconstruction tree | CF Div1 C-D, BOI |

---

## 1. Representation, rooting, and the preorder sweep

Adjacency lists `vector<vector<int>> adj(n)`; edges stored both ways. Rooted vs unrooted is a matter of *which vertex you call parent*: an unrooted tree becomes rooted the moment you run a traversal from some `root`. Problems phrase trees as "boss → subordinate" (already rooted, parent given for `i = 2..n`) or as an edge list (root at 0 or 1 yourself).

**Iterative rooting.** A stack-based DFS produces a `preorder` in which every vertex appears after its parent. That ordering is all any tree DP needs:

- bottom-up (subtree sizes, sums, DP values): `for i = n−1 .. 1: contribute order[i] into par[order[i]]`
- top-down (depth, prefix sums to the root, "answer from parent"): `for i = 1 .. n−1: read from par[order[i]]`

```cpp
struct Rooted {
    int n; vector<vector<int>> adj; vector<int> par, depth, order, sub;
    Rooted(const vector<vector<int>>& g, int root = 0) : n(g.size()), adj(g), par(n, -1), depth(n, 0), sub(n, 1) {
        vector<int> st = {root};
        while (!st.empty()) {
            int v = st.back(); st.pop_back(); order.push_back(v);
            for (int u : adj[v]) if (u != par[v]) { par[u] = v; depth[u] = depth[v] + 1; st.push_back(u); }
        }
        for (int i = n - 1; i > 0; --i) sub[par[order[i]]] += sub[order[i]];
    }
};
```

`u != par[v]` is a valid "visited" test in a tree (no cycles, single parent). This is not a real DFS order (children are popped in reverse push order and siblings' subtrees are still contiguous), but it is a valid *preorder*: the subtree of `v` is exactly `order[tin[v] .. tin[v]+sub[v])`. That is all you need for Euler-tour techniques.

**Recursion depth.** A path graph with `n = 2e5` recurses 2e5 deep: ~100 bytes per frame → 20 MB. Linux judges (CSES, Codeforces with default 256 MB stack, AtCoder) survive; Windows judges and macOS default 8 MB stacks crash (segfault with no message). Fixes: iterative traversal as above; or `-Wl,-z,stacksize=268435456` (GCC/Linux) / `-Wl,-stack_size,0x10000000` (macOS); or run your solve in a `std::thread` with a big stack. Recursion of depth `O(log n)` (treap, centroid decomposition levels) is fine.

**Reading trees.** Edge list `a b` (1-indexed → subtract 1); parent list `p_2..p_n`; weighted edges: store `adj[v].push_back({u, w})` or keep `wpar[v]` = weight of the edge to the parent once rooted (there is exactly one such edge per non-root vertex — edge-weighted problems become vertex-weighted on non-roots).

---

## 2. Tree DP basics

`sub[v]` (subtree size), `depth[v]`, `down[v]` = sum of depths inside the subtree, `cnt[v]` = leaves in subtree, `dp[v]` = best independent set in subtree — all are "combine children's values" computed in reverse preorder. Two DP shapes:

- **Value flows up**: `f(v) = g(f(children))`. Reverse preorder sweep, `O(n)`.
- **Choice at v depends on children's states**: `dp[v][state]`, e.g. maximum matching: `dp[v][0]` (v unmatched) `= Σ max(dp[u][0], dp[u][1])`; `dp[v][1]` (v matched to some child `u`) `= max_u ( dp[u][0] + 1 + Σ_{w≠u} max(dp[w][0], dp[w][1]) )` — compute the total once and subtract per child, avoiding the `O(deg²)`. Greedy alternative for Tree Matching: match every leaf to its parent when the parent is free, processing in reverse preorder.

Counting shape: number of vertices at depth exactly `d` in the subtree of `v` — a `vector` per vertex merged small-to-large, or offline via Euler tour: vertices at depth `d` inside `[tin, tout)` = binary search in the sorted `tin` list of depth `d`. That second idea generalises: **any subtree query about a class of vertices = range count over Euler positions of that class**.

---

## 3. Diameter

**Two BFS/DFS.** From any vertex `s`, take the farthest vertex `a`; from `a` take the farthest `b`; `dist(a, b)` is the diameter.

*Proof.* Let `(p, q)` be a diameter. Suppose the farthest vertex from `s` is `a`. Case 1: the path `s→a` shares a vertex `x` with the path `p→q`. Then `dist(x, a) ≥ dist(x, p)` and `≥ dist(x, q)` (else a farther vertex than `a` exists), so `dist(a, q) = dist(a, x) + dist(x, q) ≥ dist(p, x) + dist(x, q) = diam` — `a` is a diameter endpoint. Case 2: disjoint; let `x` on `p→q` be closest to the `s→a` path at vertex `y`. Then `dist(y, a) ≥ dist(y, q) ⇒ dist(a, p) = dist(a, y) + dist(y, x) + dist(x, p) > dist(q, p)`, contradiction with maximality. Works for non-negative weights too.

**DP with two best depths.** Root anywhere; for each `v` keep the largest two values of `1 + downDepth(child)`; the diameter through `v` is their sum; take the max over `v`. `O(n)`, one pass, and it generalises to weighted edges and to "count diameters" / "second best".

```cpp
int diam = 0; vector<int> b1(n), b2(n);              // two largest child depths + 1
for (int i = n - 1; i >= 0; --i) {
    int v = order[i]; diam = max(diam, b1[v] + b2[v]);
    if (i == 0) break;
    int p = par[v], c = b1[v] + 1;
    if (c > b1[p]) { b2[p] = b1[p]; b1[p] = c; } else if (c > b2[p]) b2[p] = c;
}
```

Trace on the tree `0-1, 1-2, 1-3, 3-4` rooted at 0: leaves 2, 4 have `b1=b2=0`; vertex 3 gets `b1=1`; vertex 1 gets `b1=2` (from 3), `b2=1` (from 2) → diameter through 1 is 3 (path 2-1-3-4). Vertex 0: `b1=3`. Answer 3.

**Center.** The middle vertex (or two) of any diameter path; all diameters pass through the center. Distinct from the centroid (§4). For "minimise the maximum distance to a chosen vertex" the center is the answer: `eccentricity(center) = ⌈diam/2⌉`.

---

## 4. Centroid

A **centroid** is a vertex whose removal leaves every component with size `≤ n/2`. Every tree has one or two (two iff `n` is even and an edge splits it into two halves).

*Existence.* Root the tree; start at the root; while some child `u` has `sub[u] > n/2`, move to `u` (there is at most one such child, since two would sum to `> n`). When you stop at `v`: all children have `sub ≤ n/2`, and the component above `v` has size `n − sub[v] < n − n/2` because `sub[v] > n/2` (we moved into `v` because it was heavy; or `v` is the root and the upper component is empty). `O(n)`.

```cpp
int v = root;
for (;;) { int heavy = -1;
    for (int u : adj[v]) if (u != par[v] && sub[u] > n / 2) heavy = u;
    if (heavy < 0) return v; v = heavy; }
```

Uses: CSES Finding a Centroid directly; the root of a canonical form for unrooted isomorphism (§14); the recursion pivot for centroid decomposition (§12), where the `≤ n/2` guarantee gives `O(log n)` depth.

---

## 5. Euler tour: subtree = range, path = prefix difference

Assign `tin[v]` = index of `v` in preorder, `tout[v] = tin[v] + sub[v]`. Then

- `u` is in the subtree of `v`  ⇔  `tin[v] ≤ tin[u] < tout[v]`  (an `O(1)` ancestor test!)
- subtree of `v` = positions `[tin[v], tout[v])` → **subtree sum/min/max with point updates**: any array structure over the positions (Fenwick for sums). CSES Subtree Queries.
- **path to root sum with point updates** (CSES Path Queries): "add `x` to vertex `v`" affects the root-path sum of every vertex in `v`'s subtree → range add `[tin[v], tout[v])`, point query at `tin[u]`. Fenwick with range-add/point-query (difference array), or lazy segment tree.
- **path `a→b` = `root→a + root→b − 2·root→lca + value(lca)`** for sums: reduce path queries to root-path queries plus one LCA (§8).

```
tree:        0                 preorder (stack DFS):  0 2 1 4 3    (order depends on push order)
           /   \               tin:  0→0  2→1  1→2  4→3  3→4
          1     2              sub:  0:5  1:3  2:1  3:1  4:1
         / \                   subtree(1) = positions [2, 5) = {1, 4, 3}   ✓
        3   4
```

Fenwick reminder (0-indexed API over 1-indexed storage): `add(i, d)`: `for (++i; i <= n; i += i & -i)`; `prefix(i)` = sum of `[0, i)`: `for (; i > 0; i -= i & -i)`. Chapter on range queries has the k-th descent and 2D variants.

A second, longer Euler tour — append `v` every time the DFS *returns* to it (length `2n − 1`) — is what the `O(1)` LCA of §7 uses.

---

## 6. LCA by binary lifting

`up[k][v]` = the `2^k`-th ancestor of `v` (with `up[·][root] = root`). Build: `up[0][v] = par[v]`; `up[k][v] = up[k−1][ up[k−1][v] ]`. `O(n log n)` time and memory (`LOG = 18` for `2e5`: 3.6e6 ints = 14 MB).

- **k-th ancestor**: decompose `k` in binary, jump for each set bit. `O(log n)`. CSES Company Queries I.
- **jump to depth `d`**: k-th ancestor with `k = depth[v] − d`.
- **lca(a, b)**: lift the deeper one to the other's depth; if equal, done; otherwise for `k = LOG−1 .. 0`, if `up[k][a] ≠ up[k][b]` jump both. Afterwards `a` and `b` are distinct children of the LCA → answer `up[0][a]`. *Invariant:* after processing bit `k`, `a` and `b` are still below the LCA and their distance to it is `< 2^k`. CSES Company Queries II.
- **dist(a, b)** `= depth[a] + depth[b] − 2·depth[lca]`. CSES Distance Queries.
- **k-th vertex on the path a→b**: if `k ≤ depth[a] − depth[lca]` it is `kth(a, k)`, else `kth(b, dist − k)`.

```cpp
struct LCA {
    int n, LOG; vector<vector<int>> up; vector<int> depth;
    LCA(const Rooted& T) : n(T.n), depth(T.depth) {
        LOG = 1; while ((1 << LOG) < n) ++LOG;
        up.assign(LOG, vector<int>(n));
        for (int v = 0; v < n; ++v) up[0][v] = T.par[v] < 0 ? v : T.par[v];
        for (int k = 1; k < LOG; ++k) for (int v = 0; v < n; ++v) up[k][v] = up[k-1][up[k-1][v]];
    }
    int kth(int v, int k) const { if (k > depth[v]) return -1;
        for (int i = 0; k; ++i, k >>= 1) if (k & 1) v = up[i][v]; return v; }
    int lca(int a, int b) const {
        if (depth[a] < depth[b]) swap(a, b);
        a = kth(a, depth[a] - depth[b]);
        if (a == b) return a;
        for (int k = LOG - 1; k >= 0; --k) if (up[k][a] != up[k][b]) { a = up[k][a]; b = up[k][b]; }
        return up[0][a];
    }
    int dist(int a, int b) const { int c = lca(a, b); return depth[a] + depth[b] - 2 * depth[c]; }
};
```

Trace: path `0-1-2-3-4-5` rooted at 0, plus `2-6`. `lca(5, 6)`: depths 5 and 3 → lift 5 by 2 → vertex 3. `3 ≠ 6`. `k=2`: `up[2][3]=0`, `up[2][6]=0` equal → skip. `k=1`: `up[1][3]=1`, `up[1][6]=1` → skip. `k=0`: `up[0][3]=2`, `up[0][6]=2` → skip. Return `up[0][3] = 2`. ✓

**Memory layout.** `up[k][v]` (k outer) makes the build loop cache-friendly; `up[v][k]` makes queries slightly faster. Either is fine at `2e5`. For `n = 1e6` with tight memory, use the Euler+RMQ method or the `O(n)`-memory "jump pointers" trick (`jump[v]` = ancestor at depth chosen so that the ladder halves — skip unless needed).

**Binary lifting on any functional graph** (`succ[v]`, not necessarily a tree): the same `up` table answers "where am I after `k` steps" — CSES Planets Queries I, Movie Festival Queries ("after choosing this movie, which is the next?" is a successor function).

---

## 7. LCA in O(1): Euler tour + sparse table

Write the `2n−1` Euler tour (vertex appended on every visit, i.e., before and after each child). `first[v]` = first index of `v`. For `a, b`: the shallowest vertex in `euler[first[a] .. first[b]]` is the LCA — the walk between the two first-visits never leaves the LCA's subtree and passes through the LCA when it switches child subtrees. Range-min by depth with a sparse table: build `O(n log n)`, query `O(1)`. Same memory as lifting, faster queries, but no k-th ancestor. Use when queries dominate (`q = 1e6`) or as the LCA inside a virtual-tree build.

The iterative construction in `example.cpp` (`LCA_RMQ`) keeps a stack of `(vertex, next child index)` and appends the top vertex on every loop iteration — this yields exactly the `2n−1` sequence.

**Tarjan's offline LCA** (mention): process queries in DFS order with a DSU that unites each finished child into its parent; `lca(a, b)` for a query is `find(b)` at the moment `a` is finished if `b` is already finished. `O((n + q) α)`, needs all queries in advance. Rarely worth it over lifting.

---

## 8. Distance and path decomposition with LCA

Every path `a→b` is `a → lca → b`. Consequences:

- `dist(a, b)` as above.
- "Does the path `a→b` pass through `c`?" ⇔ `dist(a,c) + dist(c,b) == dist(a,b)` (or: `c` is an ancestor of `a` or `b`, and `lca(a,b)` is an ancestor of `c`).
- **Counting Paths (CSES 1136)**: "for each of `m` paths, add 1 to every vertex on it; report all counts": difference on the tree — `+1` at `a`, `+1` at `b`, `−1` at `lca`, `−1` at `par[lca]`; then `cnt[v] = Σ` over the subtree of `v` (reverse preorder accumulation). `O((n + m) log n)`.
- Path *sum* queries with point updates: `S(v)` = sum from root to `v`, maintained via Euler range-add; answer `S(a) + S(b) − 2 S(lca) + val[lca]`.
- Path *max* with **no** updates: binary lifting with aggregates (§9). With updates: HLD (§10).

---

## 9. Binary lifting with aggregated values

Store beside `up[k][v]` the aggregate `mx[k][v]` of the `2^k` edges above `v`. `mx[k][v] = op(mx[k−1][v], mx[k−1][up[k−1][v]])`. Query `pathMax(a, b)`: lift the deeper vertex, aggregating; then do the LCA descent aggregating both sides; finally include the two last edges into the LCA. `O(log n)` per query, `O(n log n)` memory, **static** edge values only. Works for any associative operator (max, min, gcd, sum, "and"; also non-commutative ones like function composition if you keep direction in mind — e.g. "Company Queries II"-style lifting over a `succ` function with costs, "Distance Queries").

Classic uses: *min-max edge on the path* → "is edge `e` in some MST" / "MST for each edge" (CF 609E); *sum of edge weights* on a path in a weighted tree; *Planets Queries II* (distance in a functional graph, lifting plus cycle handling).

```cpp
ll pathMax(int a, int b) const {                      // mx[0][v] = w(v, par v)
    ll res = LLONG_MIN; if (depth[a] < depth[b]) swap(a, b);
    for (int k = LOG - 1; k >= 0; --k) if (depth[a] - (1 << k) >= depth[b]) { res = max(res, mx[k][a]); a = up[k][a]; }
    if (a == b) return res;
    for (int k = LOG - 1; k >= 0; --k) if (up[k][a] != up[k][b]) { res = max({res, mx[k][a], mx[k][b]}); a = up[k][a]; b = up[k][b]; }
    return max({res, mx[0][a], mx[0][b]});
}
```

---

## 10. Heavy-light decomposition

**Definition.** For each non-leaf `v`, its **heavy child** is the child with the largest subtree; the edge to it is *heavy*, all others *light*. Maximal chains of heavy edges are **heavy paths**. Lemma: on any root-to-vertex path there are at most `log₂ n` light edges. Proof: if `u` is a light child of `p` then `sub[u] ≤ sub[p]/2` — otherwise `sub[u] > sub[p]/2` would make `u` strictly larger than every sibling, i.e. the heavy child. So the subtree size at least halves at every light edge, and it starts at `n`. Hence every path `a→b` is the union of `O(log n)` chain segments.

**Positions.** Do a DFS that visits the heavy child *first*. Then (a) each heavy path occupies a contiguous block of positions, and (b) each subtree is still contiguous (`[pos[v], pos[v] + sub[v])`). Store `head[v]` = top vertex of `v`'s chain. Now a segment tree over positions supports:

- `pathQuery(a, b)`: while `head[a] ≠ head[b]`: take the one whose head is deeper (say `a`), query `[pos[head[a]], pos[a]]`, set `a = par[head[a]]`. Finally both are on one chain: query `[pos[a], pos[b]]` with `a` the shallower. `O(log n)` segments × `O(log n)` per segtree query = `O(log² n)`. Path *updates* are identical with `update` instead of `query` (lazy segtree for range add).
- `subtreeQuery(v)`: one segtree query on `[pos[v], pos[v] + sub[v])`.
- **Edge weights**: store `w(v, par v)` at `pos[v]`; in the final same-chain step start at `pos[a] + 1` to skip the LCA's own (upper) edge; skip entirely when `a == b`.

```cpp
struct HLD {
    int n; vector<int> par, depth, heavy, head, pos, sub; SegMax seg;   // SegMax: iterative point-set / range-max
    HLD(const Rooted& T) : n(T.n), par(T.par), depth(T.depth), heavy(n, -1), head(n), pos(n), sub(T.sub), seg(n) {
        for (int v : T.order) for (int u : T.adj[v])
            if (u != par[v] && (heavy[v] < 0 || sub[u] > sub[heavy[v]])) heavy[v] = u;
        int cur = 0; vector<int> st = {T.order[0]};
        while (!st.empty()) {                          // each stack entry starts a new chain
            int v = st.back(); st.pop_back();
            for (int x = v; x != -1; x = heavy[x]) {   // walk the chain, positions consecutive
                head[x] = v; pos[x] = cur++;
                for (int u : T.adj[x]) if (u != par[x] && u != heavy[x]) st.push_back(u);
            }
        }
    }
    void setVertex(int v, ll val) { seg.set(pos[v], val); }
    ll pathMax(int a, int b) const {
        ll res = LLONG_MIN;
        while (head[a] != head[b]) {
            if (depth[head[a]] < depth[head[b]]) swap(a, b);
            res = max(res, seg.query(pos[head[a]], pos[a] + 1)); a = par[head[a]];
        }
        if (depth[a] > depth[b]) swap(a, b);
        return max(res, seg.query(pos[a], pos[b] + 1));
    }
    ll subtreeMax(int v) const { return seg.query(pos[v], pos[v] + sub[v]); }
};
```

Why this stack-based assignment keeps both invariants: chains are contiguous by construction (the inner `for` walks a whole chain). Subtrees are contiguous because when `v` is popped, `v`'s subtree receives `pos[v]`, then the rest of `v`'s chain, then the light subtrees hanging off that chain — all popped (LIFO) before anything that was pushed earlier. The test in `example.cpp` checks `subtreeMax` and `pos[heavy[v]] == pos[v] + 1` against brute force on random trees.

```
    0                 heavy edges: 0-1 (sub 4 vs 1), 1-3 (sub 2 vs 1), 3-5
   / \                chains:  [0 1 3 5]  [2]  [4]
  1   2               pos:     0 1 2 3     4    5      head[5]=head[3]=head[1]=0
 / \
4   3                 pathMax(4, 5): head[4]=4 ≠ head[5]=0, depth[4]=2 > depth[0] → query [pos 5,5]=[5,5], a = par[4] = 1
    |                 now head[1]=head[5]=0: query [pos[1], pos[5]] = [1, 3]. Two segtree calls.
    5
```

Constants: `n = q = 2e5` → `2e5 · 18 · 18 ≈ 6.5e7` segtree steps worst case — ~0.3 s with an iterative segment tree; a recursive lazy segtree may reach 1 s. Use iterative when the operation allows.

HLD vs lifting vs Euler: lifting = static path aggregates, `O(log n)`, simplest; Euler tour = subtree ops and root-path sums with updates; HLD = arbitrary path *and* subtree ops with updates, `O(log² n)`; link-cut = plus edge insert/delete (Chapter 09 §12).

---

## 11. Rerooting DP

Goal: an answer `ans[v]` for *every* root in `O(n)` total, when `ans[root]` is a bottom-up DP. Two sweeps:

1. `down[v]` = DP over the subtree of `v` (reverse preorder).
2. `up[v]` = DP over "everything outside the subtree of `v`, viewed from `par[v]`", computed top-down: `up[u] = combine( up[v], down of v's other children )`. Then `ans[u] = combine(down[u], up[u])`.

When "other children" requires excluding one child, either use an invertible combine (sum: `total − down[u]`) or prefix/suffix aggregates over the children list (max: keep the two best, as in the diameter DP — if the best came from `u`, use the second best).

**Sum of distances (Tree Distances II).** `down[v] = Σ_{children u} (down[u] + sub[u])`. Moving the root from `p` to child `v`: the `sub[v]` vertices in `v`'s subtree get 1 closer, the other `n − sub[v]` get 1 farther: `ans[v] = ans[p] − sub[v] + (n − sub[v])`. Two linear sweeps.

**Max distance from each vertex (Tree Distances I).** `down1[v], down2[v]` = two largest child depths (+1), with `who[v]` the child achieving `down1`. `up[u]` for child `u` of `p`: `1 + max(up[p], down1[p] if who[p] ≠ u else down2[p])`. `ans[u] = max(down1[u], up[u])`. Equivalent trick: `ans[v] = max(dist(v, a), dist(v, b))` for a diameter `(a, b)` — three BFS, no DP.

```cpp
// sum of distances for every root
for (int i = n - 1; i > 0; --i) { int v = order[i], p = par[v]; down[p] += down[v] + sub[v]; }
ans[root] = down[root];
for (int i = 1; i < n; ++i) { int v = order[i], p = par[v]; ans[v] = ans[p] - sub[v] + (n - sub[v]); }
```

The general pattern is "answer for a root = combine over neighbours of a *directed-edge* DP `f(u → v)`"; there are `2(n−1)` directed edges and each is computed once. That framing (`dp[edge]`) is what solves counting problems like "number of independent sets containing `v`" for all `v` (multiplication needs modular inverses or prefix/suffix products — prefer prefix/suffix).

---

## 12. Centroid decomposition

**Build.** Find the centroid `c` of the current component, record it, *remove* it (`removed[c] = true`), recurse into each remaining component. The **centroid tree** has `c` as parent of the centroids of the pieces. Depth `≤ log₂ n` because each piece has `≤ half` the vertices. Total build `O(n log n)` (each level touches every vertex once for size computation and centroid search).

**The key property.** For any two vertices `a, b`, let `c` be their LCA in the centroid tree (= the first centroid, in removal order, that lies on the path `a→b` — the first removed centroid whose component contained both). Then `c` lies on the path `a→b` and `dist(a, b) = dist(a, c) + dist(c, b)`, with both distances measured *inside `c`'s component*. Hence **every path is counted exactly once if, at each centroid `c`, we count paths that pass through `c` and have both ends in `c`'s component**, and each vertex belongs to only `O(log n)` components — so a vertex's distances to all its ancestor-centroids fit in `O(n log n)` memory.

**Count paths of length exactly `k`** (CSES Fixed-Length Paths I; CF 161D): at centroid `c`, walk each child subtree, collect the depths of its vertices; for depth `d`, add `cnt[k − d]` where `cnt[]` counts depths in *previously processed* child subtrees (and `cnt[0] = 1` for `c` itself); then add this subtree's depths into `cnt`. Processing subtrees one at a time ensures both endpoints are in *different* subtrees (or one endpoint is `c`). Reset `cnt` in `O(component)` (only the touched entries). Total `O(n log n)`.

```cpp
ll build(int entry, int k) {                            // returns #paths of length k inside this component
    int total = calcSize(entry, -1); int c = centroid(entry, -1, total);
    removed[c] = true;
    vector<ll> cnt(total + 1, 0); cnt[0] = 1; ll res = 0;
    for (int u : g[c]) if (!removed[u]) {
        vector<int> ds; collect(u, c, 1, ds);           // depths from c inside this child subtree
        for (int d : ds) if (k - d >= 0 && k - d <= total) res += cnt[k - d];
        for (int d : ds) cnt[d]++;
    }
    for (int u : g[c]) if (!removed[u]) res += build(u, k);
    return res;
}
```

**Paths with length in `[k1, k2]`** (Fixed-Length Paths II): replace `cnt[k − d]` by a prefix-sum query `Σ_{j=k1−d}^{k2−d} cnt[j]` — maintain a Fenwick over depths, or note depths in one subtree are added in bulk, so a prefix-sum array rebuilt per subtree is `O(size)` amortised... the simplest correct approach is a Fenwick over `cnt` with `O(log n)` per operation → `O(n log² n)`; an `O(n log n)` version sorts subtrees by max depth so that the prefix array is rebuilt cheaply.

**Nearest marked vertex, online** (CF 342E "Xenia and Tree"): store for each vertex `v` its distance to each of its `O(log n)` ancestor-centroids (computed at build). `best[c]` = min distance from centroid `c` to any marked vertex *in its component*. Mark `v`: for each ancestor-centroid `c`, `best[c] = min(best[c], dist(v, c))`. Query `v`: `min over ancestor-centroids c of best[c] + dist(v, c)`. Correctness: the nearest marked `u` and `v` have some common ancestor-centroid `c` on their path, and `dist(v, c) + dist(c, u) = dist(v, u)`; every other term is `≥` a real distance. `O(log n)` per operation.

**IOI 2011 Race**: shortest (fewest edges) path with total weight exactly `K` → at each centroid, for each child subtree, query a global `bestEdges[weight]` table for `K − w`, then insert; reset touched entries. `O(n log n)` with `K ≤ 1e6` as the table size.

Pitfalls: forgetting to reset `cnt` between centroids (use only the touched indices or a per-component `vector`); recursion for `calcSize/collect` has depth up to the component size — iterate with an explicit stack for `n = 2e5` paths on strict judges; `removed[c]` must be set *before* recursing.

---

## 13. Tree isomorphism (AHU) and tree hashing

**Rooted trees.** Assign each vertex a canonical id = the sorted list of its children's ids, interned in a global `map<vector<int>, int>`. Two rooted trees are isomorphic iff their roots get the same id (Aho–Hopcroft–Ullman). `O(n log n)` with the sort. **Unrooted**: root at the centroid; if there are two centroids, try both (or root at the middle of the edge between them by inserting a virtual vertex). CSES Tree Isomorphism I (rooted) / II (unrooted).

**Hashing alternative** for speed/simplicity: `h(v) = Π_{children} (R_{depth} + h(u)) mod p` or `h(v) = Σ (h(u) · something)` — multiplicative with a *depth-dependent random constant* is collision-resistant in practice; plain sums are not (anti-hash tests exist on Codeforces). Use two moduli or `unsigned long long` overflow with random bases. The `map`-based AHU is collision-free and fast enough for `2e5`.

---

## 14. Virtual (auxiliary) trees — mention

Given `k` marked vertices, the *virtual tree* is the tree on the marked vertices plus all pairwise LCAs, preserving ancestor relations; it has `≤ 2k − 1` vertices. Build: sort marked vertices by `tin`, add LCAs of adjacent pairs, sort again, dedupe, connect each vertex to the nearest preceding ancestor using a stack. `O(k log n)`. Use it when a problem asks `q` questions about sets of vertices with `Σk ≤ 2e5`: run the tree DP on the virtual tree only (edges carry compressed lengths). Signal: "a query gives `k_i` vertices; sum of `k_i` is bounded". Needs LCA (§6/§7) and `tin/tout`.

---

## 15. Kruskal reconstruction tree

Run Kruskal on the edges in increasing weight. When an edge `(a, b, w)` merges two components, create a **new vertex** with weight `w` whose children are the two component roots (DSU roots now point to the new vertex). Result: a binary tree with `2n − 1` vertices (if connected), leaves = original vertices, internal weights non-decreasing towards the root. Properties:

- `minimax(a, b)` — the minimum over paths of the maximum edge — is the weight of `lca(a, b)` in the KRT (with the LCA structure of §6 built on the KRT).
- "Set of vertices reachable from `a` using edges of weight `≤ w`" = the subtree of the highest ancestor of `a` with weight `≤ w` — find it by binary lifting on the KRT (weights are monotone along root paths), then use Euler-tour ranges on the leaves.
- Swap min/max (run Kruskal in decreasing order) for maximin / "bottleneck bandwidth" (CSES Transfer Speeds Sum: sum over pairs of the bottleneck = Σ over merges of `w · sizeL · sizeR`, which does not even need the tree — but the tree gives per-pair queries).
- "Earliest time `a` and `b` are connected" when edges arrive over time = KRT with time as weight (CSES New Roads Queries), or DSU with small-to-large and per-vertex timestamps.

Bridge to Chapter 11 (MST): a KRT is the MST plus the merge history; when a problem asks path-min/max queries on an MST, build the KRT instead of running HLD on the MST.

---

## 16. Pitfalls

- **Recursion depth** at `n = 2e5` on a path: iterative DFS (`Rooted` above) or a bigger stack. Test on a path graph.
- **0/1 indexing**: CSES is 1-indexed; convert on input. `par[root] = -1` vs `root` — pick one and be consistent (`up[0][root] = root` for lifting).
- **Parent arrays given as input** (`Subordinates`, `Company Queries`): the tree is already rooted at 1 and parents are `< child`, so the preorder is just `1..n` — no DFS needed.
- `int` overflow in sums of distances (`2e5 × 2e5 = 4e10`), in path sums of weights (`2e5 × 1e9`), and in `Σ w · sizeL · sizeR`.
- **LCA when one vertex is the ancestor of the other**: the early `if (a == b) return a` after lifting is mandatory; otherwise the descent loop returns the parent of the LCA.
- HLD edge version: skip `pos[lca]`; vertex version: include it exactly once.
- Centroid decomposition: reset arrays per component; mark `removed` before recursing; the `k − d` index can be negative — check bounds.
- Rerooting with `max`: keep the two best children or use prefix/suffix — never "recompute without `u`" in `O(deg)` per child (`O(deg²)` on stars).
- `vector<vector<int>>` of size `LOG × n` costs `LOG` allocations — fine; but `vector<vector<int>> adj(n)` on `n = 1e6` with many tiny vectors is slow: use CSR (`head/next` arrays or a sorted edge array with offsets) for `n ≥ 1e6`.
- Output volume: `2e5` answers with `endl` = TLE. `'\n'` and `sync_with_stdio(false)`.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| "number of subordinates / subtree size" | reverse-preorder DP | `O(n)` |
| "longest path in the tree" | 2×BFS or two-best-depths DP | `O(n)` |
| "for each node, the farthest node / sum of distances" | rerooting (or 3×BFS for max) | `O(n)` |
| "k-th boss / ancestor" | binary lifting | `O(n log n + q log n)` |
| "LCA / distance between two nodes" | lifting or Euler+RMQ | `O(q log n)` / `O(q)` |
| "how many paths cover each node" | LCA + difference on tree | `O((n+m) log n)` |
| "subtree sum, point updates" | Euler tour + Fenwick | `O((n+q) log n)` |
| "root-path sum, point updates" | Euler tour range-add / point-query | `O((n+q) log n)` |
| "path max/sum, point updates" | HLD + segtree | `O(q log² n)` |
| "path max, no updates" | lifting with aggregates | `O(q log n)` |
| "number of distinct values in each subtree" | Euler tour + offline distinct / small-to-large sets | `O(n log n)`–`O(n log² n)` |
| "count paths of length k / in `[k1,k2]`" | centroid decomposition | `O(n log n)` / `O(n log² n)` |
| "nearest marked vertex with online marking" | centroid decomposition + ancestor distances | `O(log n)` per op |
| "are these trees isomorphic" | AHU canonical ids at centroid | `O(n log n)` |
| "queries on sets of vertices, Σk bounded" | virtual tree | `O(Σk log n)` |
| "min possible max edge between a and b" | Kruskal reconstruction tree + LCA | `O((m log m) + q log n)` |
| "earliest time a,b connected" | KRT with time weights | `O(m log m + q log n)` |

## Implementation checklist for contests

- [ ] Rooting: iterative; `par[root] = -1`; `order` is a preorder (parents before children); `sub` computed in reverse.
- [ ] Euler tour: `tout = tin + sub`; ranges half-open `[tin, tout)`; Fenwick over `n` positions 0-indexed API.
- [ ] Lifting: `LOG` such that `2^LOG ≥ n` (**≥**, and at least 1); `up[0][root] = root`; `kth` returns `-1` when `k > depth`; `if (a == b)` after equalising depths.
- [ ] Path aggregates: include the two final edges into the LCA (`mx[0][a]`, `mx[0][b]`); identity `LLONG_MIN` / `LLONG_MAX` / `0`.
- [ ] HLD: heavy child = max `sub`; positions assigned heavy-first; loop condition `head[a] != head[b]`; swap so the deeper *head* moves; edge variant skips the LCA.
- [ ] Centroid decomposition: `removed` set before recursion; `cnt`/`best` arrays reset per component; depth of internal DFS ≤ component size.
- [ ] Rerooting: two sweeps, second one top-down over `order`; use two-best or prefix/suffix for non-invertible ops.
- [ ] `long long` for every sum of distances/weights; `int` for indices.
- [ ] Test on: a path (depth), a star (degree), `n = 1`, `n = 2`, two centroids (even `n` path).
- [ ] Stress-test every structure against brute force on random trees of size ≤ 50 before the first submission.

## Further reading

- CPH: Ch. 14 (Tree algorithms: traversal, diameter, all longest paths, binary trees), Ch. 15 (Spanning trees — for §15), Ch. 18 (Tree queries: ancestors, subtrees and paths, LCA, offline algorithms).
- cp-algorithms.com: "Lowest Common Ancestor — Binary Lifting", "LCA — Farach-Colton and Bender", "LCA — Tarjan's off-line", "Heavy-light decomposition", "Centroid Decomposition" (in the tree section), "Kruskal reconstruction tree" (sometimes under "MST — second best / properties"), "Tree isomorphism (AHU)".
- Codeforces blogs: "Centroid Decomposition" tutorials (search `centroid decomposition tutorial`), "Rerooting technique" tutorials, "Virtual tree / auxiliary tree" tutorials.
- Papers: Aho, Hopcroft, Ullman, *The Design and Analysis of Computer Algorithms* (1974) — tree isomorphism; Harel & Tarjan, *Fast algorithms for finding nearest common ancestors* (1984); Sleator & Tarjan (1983) for the heavy/light path idea; Bender & Farach-Colton, *The LCA Problem Revisited* (2000).
- IOI 2011 "Race" solution notes (official) for centroid decomposition with a global table; BOI 2017 "Railway" for LCA + difference on trees.

## You can move on when...

- You can write `Rooted`, `LCA` (lifting) and `HLD` from memory in under 20 minutes total, all passing random stress tests.
- You solve the whole CSES Tree Algorithms section (16 tasks) without editorials; Fixed-Length Paths I/II in under 45 minutes each.
- You can explain in two sentences why centroid decomposition counts every path exactly once, and why HLD paths cross `O(log n)` chains.
- Given a "for every vertex compute…" statement, you can write the two rerooting sweeps on paper before coding.
- You can describe what a virtual tree and a Kruskal reconstruction tree are for and name a query each one answers.
