# Chapter 08 — DSU, Offline Techniques and Sqrt Techniques

Prerequisites: `../../algorithms_learning/06_graphs/lesson.md` §5 (basic union-find — this chapter
starts where that stopped), `../07_range_queries/lesson.md` (Fenwick tree, segment tree,
Mo's algorithm — used here as black boxes). The unifying theme: **if the queries are known in
advance, reorder them.** Sort by time, by right endpoint, by weight, by subtree size; bucket by
`√n`; binary search on all of them at once. Most "hard data structure" problems at BOI/IOI level
are an easy structure plus the right reordering.

## What you'll be able to do after this chapter

- Write a DSU with union by size + path compression from memory, explain why the amortised cost
  is `O(α(n))`, and extend it with component aggregates and **parity/potentials** for
  "same side / different side" constraints.
- Write a **rollback DSU** and use it inside a **segment tree over time** to answer connectivity
  queries with edge deletions offline in `O(q log q log n)`.
- Answer "when did `a` and `b` become connected" for `2·10^5` queries by three methods:
  time-stamped union forest, Kruskal reconstruction tree, and **parallel binary search**.
- Apply **DSU on tree (Sack)** and **small-to-large merging** to subtree queries in
  `O(n log n)` / `O(n log² n)`.
- Recognise offline sweep patterns (sort by `r` + BIT, 2D dominance counting, reverse-time
  deletions) and sqrt patterns (bucket rebuild, heavy/light thresholds) and bound their cost.

## Where this shows up in contests

| Statement shape | Technique |
|---|---|
| "add road, then ask if connected / size of component" | DSU (online) |
| "for each query, the first moment `a` and `b` are connected" | union forest / Kruskal tree / PBS |
| "roads are **removed** one by one; count components after each" | reverse time + DSU |
| "edges added **and** removed, connectivity queries", `q ≤ 10^5` | segment tree over time + rollback DSU |
| "is the graph bipartite after each edge" | parity DSU (+ rollback if deletions) |
| "for every vertex, number of distinct colours in its subtree" | Sack / small-to-large |
| "for `k` meteor showers, when does each state get enough" | parallel binary search |
| "number of distinct values in `[l, r]`", static | sort by `r` + BIT |
| "count points with `x ≤ X, y ≤ Y`" for many `(X, Y)` | sweep + BIT (dominance) |
| `n ≤ 10^5` and an `O(n√n)` bound is obviously intended | threshold / block techniques |
| "count triangles", `m ≤ 2·10^5` | degree orientation, `O(m√m)` |

Placements: DSU basics are Div2 B/C; rollback + segment tree over time is Div1 D / CF 2300+;
DSU on tree is a standard Div1 C; parallel binary search appears in BOI/JOI/POI as the intended
solution for one subtask and as the whole solution in ICPC regionals. Time budget with
`n = q = 2·10^5`: `O((n + q) log n)` ≈ `7·10^6` — trivial; `O(q log q log n)` ≈ `1.3·10^8` — fine;
`O(n√n)` ≈ `9·10^7` — fine with arrays, risky with `std::set`.

---

## 1. DSU deep dive

### 1.1 Representation and the two heuristics

`p[x]` = parent; roots satisfy `p[r] = r`. Two independent optimisations:

- **Union by size (or rank)**: attach the smaller tree under the larger root. Invariant: a tree of
  height `h` has at least `2^h` vertices ⇒ height `≤ log₂ n`. Proof: the height increases only when
  two trees of equal height merge, doubling the size (induction).
- **Path compression**: after finding the root, point every vertex on the path directly to it.

```cpp
struct DSU {
    vector<int> p, sz; int comps;
    explicit DSU(int n) : p(n), sz(n, 1), comps(n) { iota(p.begin(), p.end(), 0); }
    int find(int x) {
        int r = x;
        while (p[r] != r) r = p[r];                              // pass 1: root
        while (p[x] != r) { int nx = p[x]; p[x] = r; x = nx; }  // pass 2: compress
        return r;
    }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (sz[a] < sz[b]) swap(a, b);
        p[b] = a; sz[a] += sz[b]; comps--;
        return true;
    }
};
```

The iterative `find` is preferred over `return p[x] = find(p[x])` because a compressed-only DSU
(no union by size) can have a path of length `n` before the first compression — `2·10^5` deep
recursion segfaults on a 1 MB stack. With union by size the depth is `≤ log₂ n` even without
compression, so recursion is also fine; know **why** you are safe rather than hoping.

Trace: `unite(0,1), unite(2,3), unite(1,3), find(2)`:

```
after unite(0,1):  0        after unite(2,3):  0    2       after unite(1,3): sizes 2 vs 2,
                   |                           |    |        a=0 (root of 1), b=2:   0
                   1                           1    3                              / \
                                                                                   1   2
find(2): path 2 -> 0, already root child.                                              |
find(3): pass 1: 3 -> 2 -> 0; pass 2: p[3] = 0.                                        3
```

### 1.2 Complexity — proof sketches

- Union by size alone: every `find` is `O(log n)` (height bound).
- Path compression alone: `m` operations cost `O((n + m) log n)` amortised (Tarjan–van Leeuwen).
- Both: `O((n + m) α(n))`, `α` the inverse Ackermann function, `≤ 4` for any `n` that fits in
  the universe. A weaker but short argument gives `O(m log* n)`:

  *Sketch.* Assign each vertex the rank it had as a root (`rank ≤ log n`; ranks strictly increase
  along any parent path and never change after the vertex stops being a root). Put ranks into
  buckets `[k+1, 2^k]`: `{1}, {2}, {3,4}, {5..16}, {17..65536}, …` — only `log* n` buckets. A
  `find` walks a path; charge each step either to the *query* (if the step crosses a bucket
  boundary — at most `log* n` such steps per `find`) or to the *vertex* (if parent and child are
  in the same bucket). A vertex charged this way gets, after compression, a parent of strictly
  larger rank; within a bucket `[k+1, 2^k]` this can happen at most `2^k − k − 1 < 2^k` times, and
  vertices of rank in that bucket number at most `n / 2^k` (there are at most `n/2^r` vertices of
  rank `r`). So each bucket contributes `O(n)` vertex charges, total `O(n log* n + m log* n)`. ∎

  For contests: "practically constant". `10^7` DSU operations run in well under a second.

### 1.3 Extra information at the root

Anything that is a **commutative monoid** over the members can be kept at the root and merged in
`O(1)`: size, sum, min/max vertex, number of edges (add one per `unite` call, including the
failing ones — that gives "component has a cycle iff edges ≥ vertices"), xor of labels, a
`bitset`. Non-mergeable data (a set of colours) needs small-to-large (§7).

```cpp
// inside unite, after choosing a as the larger root:
sum[a] += sum[b]; mn[a] = min(mn[a], mn[b]); edges[a] += edges[b] + 1;
// failed unite (same root): edges[a] += 1
```

CSES Road Construction (1676): after each `unite`, output `comps` and a running `max` of `sz`
(the maximum never decreases, so one variable suffices).

### 1.4 Parity DSU / DSU with potentials

Constraint type "`x` and `y` are on the **same** side / **different** sides" (bipartiteness,
enemies/friends, xor equations `a_x ⊕ a_y = d`). Store for each vertex `par[x]` = parity of the
path `x → p[x]`. Then `find` returns `(root, parity of x relative to root)` and compresses both
pointer and parity:

```cpp
struct ParityDSU {
    vector<int> p, sz, par;
    pair<int,int> find(int x) {                       // depth <= log n by union by size
        if (p[x] == x) return {x, 0};
        auto [r, q] = find(p[x]);
        p[x] = r; par[x] ^= q;
        return {r, par[x]};
    }
    bool unite(int a, int b, int d) {                 // want par(a) ^ par(b) == d
        auto [ra, pa] = find(a); auto [rb, pb] = find(b);
        if (ra == rb) return (pa ^ pb) == d;          // false = contradiction (odd cycle)
        if (sz[ra] < sz[rb]) { swap(ra, rb); swap(pa, pb); }
        p[rb] = ra; sz[ra] += sz[rb];
        par[rb] = pa ^ pb ^ d;                        // so that (pb ^ par[rb]) ^ pa == d
        return true;
    }
};
```

Why `par[rb] = pa ^ pb ^ d`: after linking, `b`'s parity to the new root is `pb ^ par[rb]`, and we
need `pa ^ (pb ^ par[rb]) = d`. Replace xor by `+`/`−` in `Z` and you get the **weighted DSU**
for constraints `w[y] − w[x] = d` (potential differences); by any group (e.g. permutations) for
"relative rotation" puzzles. Generalisation: the potential is a group element, `find` composes
along the path, `unite` solves for the root-to-root offset.

---

## 2. DSU with rollback

Path compression destroys the information needed to undo; **union by size without compression**
keeps `find` at `O(log n)` worst case and makes every `unite` a single reversible pointer change:

```cpp
struct RollbackDSU {
    vector<int> p, sz; vector<pair<int,int>> st; int comps;
    int find(int x) const { while (p[x] != x) x = p[x]; return x; }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) { st.push_back({-1, -1}); return false; }   // record even no-ops
        if (sz[a] < sz[b]) swap(a, b);
        p[b] = a; sz[a] += sz[b]; comps--; st.push_back({b, a});
        return true;
    }
    int snapshot() const { return st.size(); }
    void rollback(int snap) {
        while ((int)st.size() > snap) {
            auto [b, a] = st.back(); st.pop_back();
            if (b >= 0) { p[b] = b; sz[a] -= sz[b]; comps++; }
        }
    }
};
```

Recording no-op unions as `(-1,-1)` lets callers roll back "the last `k` unites" without counting
which succeeded. Extra root data (sum, parity — `par[rb]` is set once and simply becomes
irrelevant when `p[rb] = rb` again) rolls back the same way. Cost: `O(log n)` per `unite`/`find`,
`O(1)` per undo. Rollback only works **LIFO** — you cannot delete an arbitrary old edge; that is
exactly what the next section fixes.

---

## 3. Offline dynamic connectivity: segment tree over time + rollback DSU

Problem (CSES Dynamic Connectivity, 2133): `q` operations — add edge, remove edge, query
(connected? / number of components). Online deletions need link-cut or Holm–de Lichtenberg–
Thorup; **offline** the following is simple and fast.

1. Every edge is alive on a set of disjoint **time intervals** `[t_add, t_remove − 1]` (or to
   `q − 1` if never removed). Compute them with a `map<edge, t_add>`.
2. Build a segment tree over time `[0, q − 1]`; insert each interval into its `O(log q)` canonical
   nodes (a `vector<edge>` per node).
3. DFS the tree. Entering node `v`: `snap = snapshot()`, `unite` all edges stored at `v`. At a
   leaf `t`: answer the query at time `t` from the current DSU. Leaving `v`: `rollback(snap)`.

```
time:      0     1     2     3     4     5     6     7
ops:     +ab   +bc   ?ac   -ab   ?ac   +ab   -bc   ?ac
edge ab alive: [0,2] and [5,7]      edge bc alive: [1,5]

segment tree over [0,7]:      [0,7]
                        [0,3]            [4,7]
                     [0,1] [2,3]      [4,5]  [6,7]
insert ab [0,2] -> nodes [0,1], [2,2];  ab [5,7] -> [5,5], [6,7];  bc [1,5] -> [1,1],[2,3],[4,5]
DFS: unite on entry, answer at leaves 2, 4, 7, rollback on exit.
```

Correctness: on the root-to-leaf path to time `t`, the union of the edge lists is exactly the
set of edges alive at `t` (each interval containing `t` is stored on exactly one node of the
path). Rollback restores the DSU to the state on entry, so siblings see the same state.
Complexity: every edge interval is applied `O(log q)` times, each `O(log n)`: `O(q log q log n)`
total; memory `O(q log q)` edge copies. `q = 10^5`: `1.7·10^6` unites, instantaneous.

The same skeleton answers **anything a rollback DSU can maintain**: bipartiteness with a parity
rollback DSU (CF 813F), component sizes, number of components, "sum of `f(size)`". Recursion depth
is `log q`. A `std::function` DFS is fine; hand-written recursion is faster.

---

## 4. "When did `a` and `b` become connected?" — three tools

CSES New Roads Queries (2101): `m` roads added at times `1..m`; for each query `(a, b)` output the
first time they are connected, or `−1`.

### 4.1 Time-stamped union forest (no compression)

Do unions by size without compression and store `when[b] = t` on the edge `b → p[b]`. Because
every union links two **roots**, `when` is strictly increasing along any upward path. The answer
is the maximum `when` on the path `a → LCA → b` — which is the `when` of the LCA's child on the
later side, i.e. the moment the last merge happened. Depth `≤ log₂ n` ⇒ lift the endpoint with the
smaller `when` until they meet: `O(log n)` per query, no preprocessing.

```cpp
int connected_at(int a, int b) const {           // -1 if never
    int best = 0;
    while (a != b) {
        if (p[a] == a && p[b] == b) return -1;   // two different roots
        if (p[b] == b || (p[a] != a && when[a] < when[b])) { best = max(best, when[a]); a = p[a]; }
        else { best = max(best, when[b]); b = p[b]; }
    }
    return best;
}
```

Lifting the smaller `when` cannot overshoot the LCA: the LCA's own `when` (its edge to *its*
parent) is larger than every `when` inside its subtree, so a vertex sitting at the LCA is never
lifted while the other endpoint is still below.

### 4.2 Kruskal reconstruction tree

Process edges in time (or weight) order; for each successful union create a **new vertex** with
value `t` whose two children are the old roots. Result: a binary tree with `2n − 1` vertices where
the answer for `(a, b)` is the value of `LCA(a, b)` (ch. 10: binary lifting). Beyond this problem
the reconstruction tree turns "minimum bottleneck path", "vertices reachable using edges
`≤ w`" (= a subtree) and "k-th vertex reachable" into tree queries — a heavier but more powerful
tool than §4.1.

### 4.3 Parallel binary search (PBS)

General pattern: `q` queries, each asks for the smallest `t` such that a **monotone** predicate
`P_i(t)` holds, and evaluating `P_i(t)` for *all* `i` at their current midpoints can be done by
**one sweep** over `t`. Each round halves every query's interval; `O(log T)` rounds.

```cpp
vector<int> lo(q, 1), hi(q, m + 1);                        // answer in [lo, hi); hi = m+1 means never
for (int round = 0; round < LOG; round++) {
    order queries by mid = (lo + hi) / 2;                   // counting sort or std::sort
    DSU d(n); int t = 0;                                    // rebuild the structure from scratch
    for (int id : ord) {
        if (lo[id] >= hi[id]) continue;
        int mid = (lo[id] + hi[id]) / 2;
        while (t < mid) d.unite(edges[t].first, edges[t].second), t++;   // sweep to time mid
        if (d.same(a[id], b[id])) hi[id] = mid; else lo[id] = mid + 1;
    }
}
```

Cost `O((m + q) log m · α)` — the sweep is redone `log m` times, but each sweep is a plain linear
pass. PBS is the tool when the structure is **easy to build forward but impossible to roll back
or to persist**: POI 2011 "Meteors" (range adds on a circular array, for each state find the first
shower after which its total ≥ need — one Fenwick sweep per round), "first time a vertex's
degree reaches `k`", "first version in which the k-th smallest ≤ x". Contrast: the union forest
answers this specific question in `O(log n)` online; PBS is `O(log m)` slower but works for any
sweepable predicate.

---

## 5. DSU on tree (Sack) and small-to-large merging

### 5.1 Small-to-large for sets

To merge two sets `A`, `B`: move the elements of the **smaller** into the larger
(`if (a.size() < b.size()) swap(a, b); for (x : b) a.insert(x);`). Every element that moves ends
up in a set at least **twice** as large as its previous one ⇒ each element moves `≤ log₂ n` times
⇒ `O(n log n)` moves, `O(n log² n)` with `std::set`, `O(n log n)` expected with a hash map.
The `swap` of two `std::set`s is `O(1)` (pointer swap) — never copy.

Applies to any per-component collection: sets of colours, maps `value → count`, sorted vectors of
depths (merge with offset), priority queues (Meldable heaps do it in `O(log n)` per meld).

### 5.2 Sack: keep the heavy child

For subtree queries on a rooted tree, `O(n log n)` **without** sets: process vertex `v` by

1. recursively solving all **light** children and clearing their data afterwards,
2. recursively solving the **heavy** child (largest subtree) and **keeping** its data,
3. re-adding every vertex of the light subtrees (and `v` itself) into the global counters,
4. answering the query for `v`; if `v` is itself a light child, remove its whole subtree.

Each vertex is re-added once per **light edge** on its path to the root, and there are
`≤ log₂ n` of those (a light child has at most half its parent's size). `O(n log n)` `add` calls.

```cpp
// cnt[colour], distinct maintained by add/rem; tin/tout/order = Euler tour with heavy child first
void dfs(int v, bool keep) {
    for (int u : adj[v]) if (u != par[v] && u != heavy[v]) dfs(u, false);
    if (heavy[v] != -1) dfs(heavy[v], true);
    for (int u : adj[v]) if (u != par[v] && u != heavy[v])
        for (int i = tin[u]; i < tout[u]; i++) add(order[i]);        // light subtrees back in
    add(v);
    ans[v] = distinct;
    if (!keep) for (int i = tin[v]; i < tout[v]; i++) rem(order[i]);
}
```

```
        1            sizes: 1:7  2:4  5:2  ...   heavy(1) = 2, heavy(2) = 3? (3:1, 4:1 -> tie ok)
      / | \          dfs(1): light 5,7 solved & cleared; heavy 2 kept (its 4 vertices stay in cnt);
     2  5  7                 re-add 5,6,7 -> add 1 -> answer for 1 with all 7 present.
    / \  \
   3   4  6          vertex 6 is re-added at 5 (light child of 1): 1 light edge on its path.
```

Recursion depth `= n` in the worst case (a path). On CSES the stack is large; on Codeforces
add `#pragma comment(linker, "/STACK:...")`-equivalents or run the DFS from a thread with a big
stack, or use an explicit-stack post-order (as `Tree::prepare` in `example.cpp` does for sizes
and the Euler tour). Use the Euler tour: with the heavy child visited first, subtree `u` is the
contiguous range `[tin[u], tout[u])` in `order`, so "re-add the subtree" is a plain loop.

Sack answers: distinct colours per subtree (CSES Distinct Colors 1139), most frequent colour sum
(CF 600E), "vertices at depth `d` in subtree with property" (CF 570D — keep per-depth counters),
number of paths of length `k` through `v` (with depth counters, CSES Fixed-Length Paths I 2080 —
also solvable by small-to-large depth vectors or centroid decomposition, ch. 10).

Mo's on trees (Euler tour + Mo's) covers **path** queries with the same add/remove interface but
in `O(n√q)`; Sack is subtree-only and `O(n log n)`.

---

## 6. Offline processing patterns

### 6.1 Sort queries by right endpoint + Fenwick

"Number of distinct values in `a[l..r]`" (CSES 1734). Sweep `r` left to right, maintaining a
Fenwick with a `1` at the **last occurrence** of every value seen so far (when `a[r]` appears
again, move its `1`). For a query `(l, r)` processed when the sweep reaches `r`,
`distinct = ones in [l, r]`: every value present in `[l, r]` has its last occurrence in `[l, r]`,
and exactly one `1`. `O((n + q) log n)`, beats Mo's by a factor ~10.

The same "last occurrence" sweep answers: "sum of distinct values in range", "number of values
occurring exactly once" (also track previous-previous), "xor of values with even count"
(CF 703D: xor of all minus xor of distinct), and — with a `(min, argmin)` segment tree instead of a
Fenwick — CF 1000F One Occurrence (ch. 07).

### 6.2 2D dominance counting

Points `(x_i, y_i)`, queries "how many points with `x ≤ X` and `y ≤ Y`". Sort points and queries
by `x`; sweep; insert `y` into a Fenwick over compressed `y`; answer `prefix(Y)`. `O((n + q) log n)`.
General rectangle counts `[x1, x2] × [y1, y2]` = four dominance queries (inclusion–exclusion),
or two sweeps at `x1 − 1` and `x2`. Any "count pairs `(i, j)` with `i < j` and `a_i < a_j`" is
dominance counting with `x = index` (inversions: CSES Sliding Window Inversions 3223,
Nested Ranges Count 2169). Three dimensions: CDQ divide and conquer or a BIT of BITs — rare at IOI.

### 6.3 Sweep line for geometry-shaped counting

Horizontal/vertical segment intersections (CSES Intersection Points, 1740): sweep `x`; a
horizontal segment inserts `+1` at its `y` when it starts and `−1` when it ends; a vertical segment
at `x` queries the Fenwick on `[y1, y2]`. Order events at equal `x` as *insert, query, delete* so
that touching endpoints count. Union area of rectangles (CSES 1741): sweep `x`, segment tree over
compressed `y` with `(cover count, covered length)` lazy node; add `covered_length · Δx` between
consecutive events.

### 6.4 Reverse time

Deletions are hard, insertions are easy: read all operations, start from the **final** state, and
process deletions backwards as insertions (CSES Network Breakdown 1677: components after each
removal ⇒ add edges in reverse and record `comps` before each). Works whenever the queries do not
depend on the earlier answers and the "final state" is computable.

### 6.5 Sort by weight

Kruskal itself is an offline sort. Queries about "paths using only edges of weight `≤ w`" (number
of pairs connected, CF 1213G; is `(u, v)` connected with weight bound `w`) — sort queries by `w`
too, interleave. MST edge questions: an edge `e = (u, v, w)` belongs to **some** MST iff, after
uniting all edges of weight `< w`, `u` and `v` are in different components (CSES MST Edge Check
3407); the min spanning tree forced to contain `e` costs `MST + w − max_edge_on_MST_path(u, v)`
(CSES MST Edge Cost 3409 — path maximum via binary lifting or the Kruskal tree, ch. 10).

---

## 7. Sqrt techniques

### 7.1 Why √n appears

Two costs `A·x` and `B·n/x` balance at `x = √(Bn/A)`: blocks of size `B` cost `O(B)` inside a block
and `O(n/B)` across blocks; a threshold `T` on frequency/degree splits objects into `≤ n/T`
"heavy" ones handled individually and "light" ones each of cost `≤ T`. Target complexity
`O(n√n) ≈ 10^8` for `n = 2·10^5`, `≈ 3·10^7` for `n = 10^5`. Only attempt it when the constraints
scream it (`n ≤ 10^5`, 2–3 s limit) or when no polylog solution is known.

### 7.2 Block decomposition with lazy tags

Covered in ch. 07 §5.1 (range add / range sum in `O(√n)`). Its real value is supporting
operations a segment tree cannot: per block keep a **sorted copy** ⇒ "count of values `< x` in
`[l, r]` with range add" in `O(√n log n)` (partial blocks rebuilt by re-sorting, `O(B log B)`, or
by merging, `O(B)`); per block keep a **frequency map** ⇒ mode queries in `O(√n)` per pointer
move. Rule: whole blocks answer in `O(1)`–`O(log)`, boundary elements are handled by brute force.

### 7.3 Sqrt on queries: periodic rebuild

Maintain a static structure `S` (sorted array, prefix sums, sparse table) plus a **buffer** of the
last `< B` updates. Queries: answer from `S` in `O(log n)`, then correct by scanning the buffer in
`O(B)`. Every `B` updates, rebuild `S` in `O(n)`. Total `O(q · (B + log n) + (q/B) · n)`,
minimised at `B = √n` ⇒ `O(q√n)`. Pattern for "insert `x`, count `≤ x`" without a BIT (e.g. when
the value domain is huge and online), for "point update, arbitrary static query" where the static
query is `O(1)` (sparse table) but the update is `O(n log n)`.

```cpp
struct SqrtRebuild {                         // insert(x), count_leq(x)
    vector<ll> base, buf; size_t B;
    void insert(ll x) {
        buf.push_back(x);
        if (buf.size() >= B) { sort(buf); base = merge(base, buf); buf.clear(); }   // O(n)
    }
    ll count_leq(ll x) const {               // O(log n + B)
        ll c = upper_bound(base, x) - base.begin();
        for (ll y : buf) c += (y <= x);
        return c;
    }
};
```

### 7.4 Heavy / light thresholds

**Triangle counting in `O(m√m)`.** Orient each edge from the endpoint with smaller `(deg, id)`
to the larger. Claim: every out-degree is `O(√m)`. If `deg⁺(u) > √(2m)` then `u` has more than
`√(2m)` neighbours of degree `≥ deg(u) > √(2m)`, giving `> 2m` degree sum — contradiction. Count
each triangle from its smallest vertex: for `u`, mark `out[u]`, then for `v ∈ out[u]`,
`w ∈ out[v]`, count marked `w`. Work `Σ_u Σ_{v∈out[u]} deg⁺(v) ≤ m · √(2m)`.

**Frequent elements.** A value occurring `> T` times is *heavy*; at most `n/T` heavy values. Queries
like "most frequent value in `[l, r]`" or "is there a value occurring more than half the time":
heavy values are checked individually with prefix counts (`O(n/T)` per query); light values are
checked by brute force over the range if `r − l < T`, or cannot be the majority otherwise.
`T = √n` gives `O(√n)` per query. **Frequency buckets**: maintain `cnt[value]` and
`freq[c] = #values with count c`; when the window slides by one, the maximum count changes by
`±1`, so the mode's frequency is maintained in `O(1)` (CSES Sliding Window Mode, 3224).

**Small vs. large weights / degrees** in general: process objects with parameter `≤ T` by one
method (`O(T)` each, `O(nT)` total) and the `≤ n/T` objects with parameter `> T` by another
(`O(n)` each). E.g. "for each vertex, sum over neighbours" with point updates: heavy vertices keep
a cached sum that is patched by neighbours' updates; light vertices are summed directly.

### 7.5 Mo's algorithm with updates (mention)

Add a third coordinate `t` (number of updates applied). Sort queries by
`(l / B, r / B, t)` with `B = n^{2/3}`; the three pointers move `O(n^{5/3})` in total
(`l, r`: `O(q · B)`, `t`: `O((n/B)² · n)`). Applying an update at position `p` toggles the element
if `p` is inside the current window. Only worth it when there is no polylog solution (CF 940F).

### 7.6 Complexity table

| Technique | Per op | Total (`n = q = 2·10^5`) | Notes |
|---|---|---|---|
| DSU (size + compression) | `O(α)` | `~10^6` | online, no deletions |
| Rollback DSU | `O(log n)` | `~4·10^6` | LIFO undo only |
| Segment tree over time + rollback | `O(log q log n)` | `~1.3·10^8` | offline, arbitrary deletions |
| Union forest query | `O(log n)` | `~4·10^6` | no preprocessing |
| Kruskal tree + LCA | `O(log n)` | `~4·10^6` | needs LCA, gives subtree queries |
| Parallel binary search | `O((m+q) log m)` | `~7·10^6` unions | rebuild per round |
| Small-to-large (`std::set`) | `O(log² n)` amortised/elem | `~7·10^7` | any per-component set |
| Sack | `O(log n)` adds/vertex | `~4·10^6` | subtree queries only |
| Sort by `r` + BIT | `O(log n)` | `~7·10^6` | static array |
| Sqrt rebuild | `O(√n)` | `~10^8` | when no BIT fits |
| Triangle counting | — | `O(m√m) ≈ 10^8` | `m = 2·10^5` |
| Mo's | `O(√q)` moves | `~10^8` | offline, add/remove only |
| Mo's with updates | — | `O(n^{5/3}) ≈ 6·10^8` | `n = 10^5` |

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| "connect `a` and `b`", "same component?", "component size" | DSU with root data | `O(α)` per op |
| "minimum total road cost" | Kruskal + DSU | `O(m log m)` |
| "friends/enemies", "same or opposite team", xor constraints | parity DSU | `O(α)` per op |
| "distance between `x` and `y` is `d`" constraints | weighted DSU (potentials) | `O(α)` per op |
| "edges added and removed", connectivity, all input given | segment tree over time + rollback DSU | `O(q log q log n)` |
| "bipartite after each edge / on each interval of time" | parity DSU + rollback (+ segtree over time) | `O(q log q log n)` |
| "roads destroyed one by one" | reverse time + DSU | `O((n + m) α)` |
| "first day `a` and `b` are connected" | union forest / Kruskal tree / PBS | `O(log n)` per query |
| "when does each of `q` things first satisfy a monotone condition" | parallel binary search | `O((n + q) log T · cost)` |
| "distinct colours / most frequent / count at depth in subtree" | Sack or small-to-large | `O(n log n)` / `O(n log² n)` |
| "distinct values in `[l, r]`", static | sort by `r` + BIT | `O((n + q) log n)` |
| "points with `x ≤ X, y ≤ Y`", "pairs with `i < j, a_i < a_j`" | sweep + BIT | `O((n + q) log n)` |
| "is edge in some MST", "MST forced to include edge" | Kruskal with grouped weights, path max | `O(m log m)` |
| "count triangles", `m ≤ 2·10^5` | degree orientation | `O(m√m)` |
| "mode / majority of range", `n ≤ 10^5` | heavy values + brute force on light | `O(n√n)` |
| "point update + query only a static structure can answer" | sqrt rebuild | `O(q√n)` |
| range queries with add/remove and no combine, **with** point updates | Mo's with updates | `O(n^{5/3})` |

## Implementation checklist for contests

- [ ] DSU: `find` on both endpoints **before** comparing; `iota` the parent array; sizes start
      at 1; keep the failed-`unite` branch in mind for edge counters.
- [ ] Iterative `find`, or a proof (union by size) that recursion depth is `≤ log n`.
- [ ] Parity DSU: compress parity **together** with the pointer; the new root offset is
      `pa ^ pb ^ d` — verify with a 3-vertex example before submitting.
- [ ] Rollback DSU: **no** path compression; record no-op unions; roll back to a snapshot, not
      "pop once".
- [ ] Segment tree over time: edges alive at the end get interval `[t_add, q − 1]`; an edge
      added and removed multiple times gets multiple intervals; queries live at leaves.
- [ ] PBS: `hi = m + 1` means "never"; skip finished queries; **reset the structure** every round;
      sort by `mid` (counting sort if `m` small).
- [ ] Sack: heavy child chosen by subtree size; Euler tour with heavy child first; `cnt` array
      sized by **max colour**, not by `n` (the bug found while writing `example.cpp`).
- [ ] Small-to-large: `swap` the containers (O(1)), then insert the smaller into the larger;
      clear the smaller if memory matters.
- [ ] Offline sweeps: sort queries **and** keep their original indices; handle ties in event
      order deliberately (insert before query before delete for closed intervals).
- [ ] Sqrt: pick `B` from the actual `n`, `q` (`B = n / √q` for Mo's, `√n` for blocks); measure —
      constants matter more than the exponent here.
- [ ] `long long` for sums over components and for `size · size` pair counts.
- [ ] Fast IO; avoid `std::set` inside `O(n√n)` loops (use arrays / sorted vectors).

## Further reading

- CPH: ch. 15 "Spanning trees" (union-find), ch. 18 "Tree queries" (merging data structures =
  small-to-large), ch. 27 "Square root algorithms" (batch processing, sub-algorithms, Mo's).
- cp-algorithms.com: "Disjoint Set Union" (incl. "DSU with parity / bipartiteness", "offline RMQ
  with DSU", "storing the DSU as an explicit list", proofs of the amortised bounds), "Deleting from
  a data structure in O(T(n) log n)" (the segment tree over time technique), "Sqrt Decomposition".
- Codeforces blogs: "Sack (dsu on tree)" (Arpa), "Offline dynamic connectivity" (various),
  "Parallel binary search" (several tutorials; POI 2011 Meteors is the canonical example),
  "Kruskal reconstruction tree".
- Tarjan, "Efficiency of a good but not linear set union algorithm" (1975) — the `α(n)` bound;
  Tarjan & van Leeuwen, "Worst-case analysis of set union algorithms" (1984) — all heuristic
  combinations.
- Holm, de Lichtenberg, Thorup, "Poly-logarithmic deterministic fully-dynamic algorithms for
  connectivity…" (2001) — what "online dynamic connectivity" really costs (not needed for IOI).

## You can move on when...

- You can write DSU, parity DSU and rollback DSU from memory in under 8 minutes total, each
  passing a random brute-force stress test on the first compile.
- You can implement segment-tree-over-time dynamic connectivity in under 25 minutes and explain
  why each edge interval is applied `O(log q)` times.
- You have solved CSES 1676, 1675, 1668, 1677, 2101, 2133, 1734 (offline BIT version), 1139 and
  at least two of the Codeforces DSU-on-tree problems in `problems.md`.
- Given a statement, you can say within a minute which reordering (by time, by `r`, by weight,
  by subtree size, by `√`) makes it easy — and estimate the resulting complexity.
