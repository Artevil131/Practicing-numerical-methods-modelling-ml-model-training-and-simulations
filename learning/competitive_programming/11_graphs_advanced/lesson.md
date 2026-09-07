# Chapter 11 — Advanced Graph Algorithms

Prerequisite: `../../algorithms_learning/06_graphs/lesson.md` (adjacency lists, BFS, DFS, DSU,
plain Dijkstra, grid BFS). None of that is repeated here. This chapter is the full CPH-level
graph toolkit: every shortest-path algorithm and when each is *wrong*, DAG DP, Eulerian and
Hamiltonian paths, SCC/2-SAT, bridges/articulation points, the MST family, functional graphs, and
the modeling patterns that turn a statement into one of those.

The reference implementations are in `example.cpp`; every snippet in this text is a trimmed copy of
what is tested there.

## What you'll be able to do after this chapter

- Pick the right shortest-path algorithm from constraints in under 10 seconds, and know why
  Dijkstra fails on negative edges, why Bellman–Ford's n-th round proves a negative cycle, and why
  SPFA is a trap on adversarial tests.
- Run Dijkstra on *state graphs* (vertex × small extra state), on implicit graphs, and extract
  the path, the k best walks, and the number of shortest paths.
- Do DP on a DAG in topological order (count paths, longest path, "Investigation" style combined
  with Dijkstra's shortest-path DAG).
- Output a cycle (directed / undirected / negative), an Eulerian path, a Hamiltonian path count,
  a De Bruijn sequence.
- Write Tarjan's SCC in one pass, build the condensation, solve 2-SAT and read off an assignment.
- Find bridges and articulation points with a proof of `low[]`, handle multi-edges correctly,
  build the bridge tree / 2-edge-connected components, and know what a block-cut tree is for.
- Kruskal / Prim / Borůvka, MST uniqueness, second-best MST in O(m log n), must-have edges, and
  the Kruskal reconstruction tree for bottleneck queries.
- Handle successor graphs with binary lifting and cycle decomposition.
- Model: layered graphs, reversed edges, super source, edge-vertex splitting, grid as graph.

## Where this shows up in contests

| Signal in the statement | Typical placement |
|---|---|
| Weighted shortest path with one twist ("one free edge", "at most k discounts", "must visit x") | CF Div2 D–E (1700–2100), Datatähti final 4–5, BOI day problems |
| "Is it possible to reach every city from every city" / "minimum roads to add so that ..." | SCC + condensation; CF Div2 D, IOI-style subtask 3 |
| "Each of n people has two wishes, satisfy at least one" | 2-SAT; CF Div1 C, CSES Giant Pizza |
| "Which roads, if closed, disconnect the network" | Bridges; CF Div2 E |
| "Route using every road/flight exactly once" | Euler path; CF Div2 D, Datatähti |
| n ≤ 20 with "visit every city exactly once" | Hamiltonian bitmask DP |
| "Minimum total cost to connect all cities" plus a twist (must include edge e, second cheapest) | MST variants; CF Div2 D–E |
| "Each planet has exactly one teleporter" + queries of the form "where after k steps" | Successor graph + binary lifting |
| n ≤ 500 and all-pairs distances, or n ≤ 5000 and reachability | Floyd–Warshall / bitset closure |
| DAG + "number of routes" / "longest route" | Topological DP; Div2 C–D |

In IOI-level problems graphs are rarely the whole task; they are the substrate on which the real
difficulty (a DP over the condensation, a data structure over the bridge tree, binary search over
a bottleneck) lives. Owning these primitives at typing speed is the point.

---

## 1. Shortest paths — the map

| Situation | Algorithm | Time | Memory |
|---|---|---|---|
| Unweighted | BFS | O(n+m) | O(n+m) |
| Weights ∈ {0,1} | 0-1 BFS | O(n+m) | O(n+m) |
| Weights ≥ 0, one source | Dijkstra (binary heap, lazy deletion) | O((n+m) log m) | O(n+m) |
| Negative edges, one source, n·m ≤ ~1e8 | Bellman–Ford | O(nm) | O(n+m) |
| Need a negative cycle | Bellman–Ford, n rounds | O(nm) | O(n+m) |
| All pairs, n ≤ ~500 | Floyd–Warshall | O(n³) | O(n²) |
| All pairs, sparse, weights ≥ 0 | n × Dijkstra | O(n(n+m) log m) | O(n²) for output |
| DAG, any weights | Topological order + DP | O(n+m) | O(n+m) |

Budget guide for 1 s: Dijkstra with n = m = 2·10⁵ is ~50 ms. Bellman–Ford with n = 2500,
m = 5000 is ~12·10⁶ relaxations — fine; n = m = 10⁵ is 10¹⁰ — dead. Floyd–Warshall n = 500 is
1.25·10⁸ trivial ops — ~150 ms; n = 1000 is borderline (1 s with `int`, tight loop), n = 2000 no.

### 1.1 Dijkstra revisited: what the proof needs, and the parents array

Invariant: when `(d,u)` is popped and `d == dist[u]`, `dist[u]` is final. Proof: any other path to
`u` leaves the settled set through a vertex `x` with `dist[x] ≥ d` (heap order) and then adds
non-negative weights, so it is ≥ `d`. A single negative edge breaks "adds non-negative weights",
and Dijkstra never revisits a settled vertex, so the improvement is lost forever. Not "slow" —
*wrong*. Example: edges 0→1 (1), 1→3 (1), 0→2 (3), 2→3 (−3). Dijkstra reports dist[3] = 2, truth
is 0.

Lazy deletion (push duplicates, skip stale pops) keeps the heap ≤ m entries: O((n+m) log m).
`std::priority_queue` with `greater<>` on `pair<long long,int>`; do not use `set` with erase —
slower and no gain.

```cpp
struct Dijkstra {
    int n; vector<vector<pair<int,ll>>> g; vector<ll> dist; vector<int> par;
    Dijkstra(int n) : n(n), g(n) {}
    void add(int a, int b, ll w) { g[a].push_back({b, w}); }
    void run(int s) {
        dist.assign(n, INF); par.assign(n, -1);
        priority_queue<pair<ll,int>, vector<pair<ll,int>>, greater<>> pq;
        dist[s] = 0; pq.push({0, s});
        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist[u]) continue;                 // stale entry
            for (auto [v, w] : g[u])
                if (d + w < dist[v]) { dist[v] = d + w; par[v] = u; pq.push({dist[v], v}); }
        }
    }
    vector<int> path(int t) {                          // follow par[] back from t
        if (dist[t] >= INF) return {};
        vector<int> p; for (int v = t; v != -1; v = par[v]) p.push_back(v);
        reverse(p.begin(), p.end()); return p;
    }
};
```

`INF = LLONG_MAX / 4`: you can add two INFs and still not overflow; `LLONG_MAX` itself overflows
on `d + w`. `1e18` also works. Never `INT_MAX` with `int` distances: 2·10⁵ edges × 10⁹ = 2·10¹⁴.

Trace on edges (0,1,4) (0,2,2) (1,2,1) (1,3,5) (2,3,8) (2,4,10) (3,4,2), undirected, source 0:

```
pop (0,0): relax 1->4, 2->2            heap: (2,2) (4,1)
pop (2,2): relax 1->3 (better), 3->10, 4->12   heap: (3,1) (4,1)stale (10,3) (12,4)
pop (3,1): relax 3->8                  heap: (4,1)stale (8,3) (10,3)stale (12,4)
pop (4,1): stale, skip
pop (8,3): relax 4->10                 heap: (10,3)stale (10,4) (12,4)stale
pop (10,4): done.  dist = [0,3,2,8,10],  par = [-1,2,0,1,3]  path(4) = 0 2 1 3 4
```

Note vertex 1 improved from 4 to 3 *after* vertex 2 was processed — the greedy "take the
lightest edge" is not the algorithm; relaxation is.

**Counting shortest paths / min-max edges along shortest paths.** Maintain `cnt[v]` (mod p) and on
`d + w == dist[v]` do `cnt[v] += cnt[u]`; on strict improvement `cnt[v] = cnt[u]`. Same for
`minEdges[v]` and `maxEdges[v]` (CSES *Investigation*, §4). This is correct because Dijkstra
finalises `u` before any relaxation *from* `u`, so `cnt[u]` is complete when used. With
zero-weight edges this argument breaks (two vertices at equal distance can be popped in either
order) — then build the shortest-path DAG first and DP over it.

### 1.2 k shortest walks (CSES *Flight Routes*)

Want the k smallest lengths of walks s→t, repetition allowed. Let every vertex be popped at most
k times; the i-th pop of `v` is the i-th smallest walk length to `v` (same proof as Dijkstra, by
induction on pops). No `dist[]` array, no stale check — just a counter:

```cpp
vector<ll> kShortestWalks(const vector<vector<pair<int,ll>>>& g, int s, int t, int k) {
    vector<int> cnt(g.size(), 0); vector<ll> res;
    priority_queue<pair<ll,int>, vector<pair<ll,int>>, greater<>> pq;
    pq.push({0, s});
    while (!pq.empty() && cnt[t] < k) {
        auto [d, u] = pq.top(); pq.pop();
        if (cnt[u]++ >= k) continue;
        if (u == t) res.push_back(d);
        for (auto [v, w] : g[u]) pq.push({d + w, v});
    }
    return res;
}
```

Heap size O(k·m): k = 10, m = 2·10⁵ → 2·10⁶ entries, fine. For k *simple* paths (no repeats)
you need Yen's algorithm — rare in contests.

**Second-best (strictly longer) distance** — IOI 2011 *Crocodile* flavour: keep `dist1[v] <
dist2[v]`, push both; a vertex's second value is final on its second pop, exactly the k = 2 case
with "strictly different" enforced.

### 1.3 State expansion: "Flight Discount" and friends

When the cheapest way to arrive at `v` depends on something other than `v` (a coupon used or not,
number of teleports left, parity of steps, which key you hold), the *state* is `(v, extra)`. Build
the product graph implicitly: 2n states, 3m transitions for one coupon. All weights stay ≥ 0, so
Dijkstra applies.

```cpp
// dist[v][k], k = 0: coupon unused, k = 1: used. Answer dist[t][1].
for (auto [v, w] : g[u]) {
    relax(v, k, d + w);                    // don't use coupon on this edge
    if (k == 0) relax(v, 1, d + w / 2);    // use it here
}
```

General rule: multiply the graph by the small extra state. "At most k reversed edges" → k+1
layers. "Parity of path length" → 2 layers. "Collected subset of ≤ 15 items" → 2¹⁵ layers with
BFS/Dijkstra over `(cell, mask)`. Memory is the constraint: states × sizeof, plus heap.

### 1.4 Implicit graphs

Never build `vector<vector<...>>` when neighbours are computable: grid cells (4 or 8 directions),
`(x, y)` positions of a knight, numbers with "×2 / +1" moves, strings differing by one character.
Dijkstra/BFS only need a `for each neighbour` loop. Encode the state as an integer (`r*W + c`, or
`v*K + k`) to use flat arrays — a `map<state, ll>` is 10–50× slower and usually TLE.

### 1.5 0-1 BFS

Weights in {0,1}: a deque replaces the heap. Invariant: the deque holds vertices with distance d
(front block) then d+1 (back block), so it is sorted; a 0-edge pushes front, a 1-edge pushes back.
O(n+m). Common as "cost 1 to break a wall, 0 to walk" grids up to 1000×1000 where a heap would be
~4·10⁶ log operations.

```cpp
deque<int> dq; dist[s] = 0; dq.push_back(s);
while (!dq.empty()) {
    int u = dq.front(); dq.pop_front();
    for (auto [v, w] : g[u]) if (dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
        if (w == 0) dq.push_front(v); else dq.push_back(v);
    }
}
```

A vertex can be pushed several times; that is fine (bounded by its degree). Generalisation:
weights in {0..K} with K small → Dial's algorithm (K+1 buckets).

### 1.6 Bellman–Ford, negative cycles, SPFA

Relax every edge, n−1 rounds. After round i, every shortest path with ≤ i edges is correct
(induction on the last edge). A shortest path without negative cycles is simple, hence ≤ n−1
edges, so n−1 rounds suffice — and if round n still improves something, a negative cycle is
reachable.

```cpp
vector<ll> d(n, INF); d[s] = 0;
for (int i = 0; i < n - 1; i++) {
    bool any = false;
    for (auto [a, b, w] : edges)
        if (d[a] < INF && d[a] + w < d[b]) { d[b] = d[a] + w; any = true; }
    if (!any) break;                                   // early exit: often 3-5 rounds
}
```

Guard `d[a] < INF` — otherwise INF + negative w becomes a finite garbage distance.

**Extracting a negative cycle anywhere in the graph** (CSES *Cycle Finding*): initialise all
`d = 0` (a virtual source with 0-edges to everyone, so unreachable-from-s cycles are found too),
run exactly n rounds recording `par[]`, remember the last relaxed vertex `x`. Then walk `par` n
times from `x` — you are now guaranteed inside the cycle (a parent chain longer than n−1 must
loop) — and read the cycle until `x` repeats.

```cpp
int x = -1;
for (int i = 0; i < n; i++) { x = -1;
    for (auto [a, b, w] : edges) if (d[a] + w < d[b]) { d[b] = d[a] + w; par[b] = a; x = b; } }
if (x == -1) { /* no negative cycle */ }
for (int i = 0; i < n; i++) x = par[x];
vector<int> cyc; for (int v = x;; v = par[v]) { cyc.push_back(v); if (v == x && cyc.size() > 1) break; }
reverse(cyc.begin(), cyc.end());                        // cyc.front() == cyc.back() == x
```

Why the walk is necessary: `x` was relaxed in round n but may hang *off* the cycle (a vertex
downstream of it). The `par` chain from any relaxed vertex eventually enters the cycle.

**Longest path / "High Score"**: negate weights, find the shortest path 1→n; answer −dist[n]
unless a negative cycle exists on some path 1→…→cycle→…→n. Check: after n rounds, for every
vertex `v` still improving, if `v` is reachable from 1 *and* n is reachable from `v` (two
BFS/DFS: forward from 1, backward from n), answer is unbounded. Improving vertices that cannot
reach n are irrelevant.

**SPFA** = Bellman–Ford with a queue of vertices whose distance just changed. Usually fast;
worst case still O(nm) and Codeforces has anti-SPFA generators (grid-like graphs with crafted
weights). Use it only for min-cost flow inner loops or when negative edges force it and the
graph is random; otherwise Dijkstra with potentials or plain Bellman–Ford. Detect a negative
cycle by counting relaxations per vertex (≥ n ⇒ cycle).

### 1.7 Floyd–Warshall

```cpp
for (int k = 0; k < n; k++)
    for (int i = 0; i < n; i++) { if (d[i][k] >= INF) continue;
        for (int j = 0; j < n; j++)
            if (d[k][j] < INF && d[i][k] + d[k][j] < d[i][j]) { d[i][j] = d[i][k] + d[k][j]; nxt[i][j] = nxt[i][k]; } }
```

Invariant after iteration k: `d[i][j]` is the shortest path whose intermediate vertices are all in
{0..k}. The k loop MUST be outermost. Negative weights fine; negative cycle ⇔ some `d[i][i] < 0`.
Path reconstruction via `nxt[i][j]` = first hop of the best i→j path (initialised to `j` for a
direct edge, updated to `nxt[i][k]`). Multi-edges: `d[a][b] = min(d[a][b], w)` on input.

The `continue` on `d[i][k] >= INF` roughly halves running time on sparse graphs and also prevents
INF+INF. n = 500 with `long long`: 1.25·10⁸ adds, ~0.15 s.

**Reverse Floyd trick** (CF 295B *Greg and Graph*): vertices deleted one by one → add them in
reverse order; adding vertex k is exactly the k-th outer iteration.

**Transitive closure with bitsets**: `reach[i]` is a `bitset<N>`; `if (reach[i][k]) reach[i] |=
reach[k]`. O(n³/64): n = 5000 ~ 2·10⁹ word ops ≈ 1 s — actually fine because it is pure OR on
contiguous memory. For DAGs prefer the topological-order version: process vertices in reverse
topological order, `reach[u] |= reach[v]` for each edge — O(nm/64) (CSES *Reachable Nodes*).
For general graphs: SCC first, then closure on the condensation (CSES *Reachability Queries*).

---

## 2. Cycle detection

**Directed — DFS colors.** WHITE (0) unvisited, GRAY (1) on the current stack, BLACK (2) done. An
edge to a GRAY vertex is a back edge and closes a cycle; the cycle is `par` chain from the current
vertex back to the gray one. O(n+m).

```cpp
bool dfs(int u) {
    color[u] = 1;
    for (int v : g[u]) {
        if (color[v] == 0) { par[v] = u; if (dfs(v)) return true; }
        else if (color[v] == 1) { start = v; end = u; return true; }
    }
    color[u] = 2; return false;
}
// cycle: start, ..., par[par[end]], par[end], end, start  (walk par from end to start)
```

Alternative: Kahn's algorithm — if `order.size() < n` there is a cycle, but it does not give you
the cycle; the vertices with `indeg > 0` at the end all lie on or downstream of cycles.

**Undirected — DFS with parent EDGE.** An edge to a visited vertex that is not the edge you
arrived by closes a cycle. Track the parent *edge id*, not parent vertex: with parallel edges
(a,b),(a,b) the second edge is a legitimate 2-cycle, and a self-loop is a 1-cycle (CSES *Round
Trip* has no multi-edges, but CF problems do). Or DSU: an edge whose endpoints are already
connected closes a cycle; to output it, run DFS on the tree formed so far between its endpoints.

**Cycle *lengths*.** Shortest cycle (girth) in an undirected unweighted graph: BFS from every
vertex, on a non-tree edge (u,v) with depths, candidate `d[u] + d[v] + 1`; O(nm) — CSES *Graph
Girth* (n ≤ 2500). Shortest cycle through a given edge: remove the edge, BFS between its
endpoints. Even/odd cycle existence = bipartiteness (§9).

---

## 3. Topological sort and DAG DP

Kahn (queue of in-degree-0 vertices) or DFS finish order reversed. Kahn is preferred: iterative,
detects cycles, and swapping the queue for a heap gives the lexicographically smallest order (CSES
*Course Schedule II* wants the order where vertex 1 is as early as possible, *then* 2, … — that is
NOT the lexicographically smallest sequence; it is: reverse all edges, take the lexicographically
*largest* order with a max-heap, then reverse the result).

```cpp
vector<int> topo(int n, const vector<vector<int>>& g) {
    vector<int> indeg(n), order; for (int u = 0; u < n; u++) for (int v : g[u]) indeg[v]++;
    queue<int> q; for (int u = 0; u < n; u++) if (!indeg[u]) q.push(u);
    while (!q.empty()) { int u = q.front(); q.pop(); order.push_back(u);
        for (int v : g[u]) if (--indeg[v] == 0) q.push(v); }
    return order;                                      // size < n  <=>  cycle
}
```

Once you have the order, any recurrence `f(v) = ⊕ over predecessors u of (f(u) ∘ w(u,v))` is
computed in O(n+m) by pushing values forward:

```cpp
ways[s] = 1;
for (int u : order) for (int v : g[u]) ways[v] = (ways[v] + ways[u]) % MOD;      // count paths s->v
best[s] = 0; // others = -INF
for (int u : order) { if (best[u] == -INF) continue;
    for (auto [v, w] : g[u]) if (best[u] + w > best[v]) { best[v] = best[u] + w; par[v] = u; } }
```

Pitfall: unreachable vertices must stay at −INF / 0 ways; if you initialise `best` to 0 everywhere,
unreachable vertices leak fake paths into `t`.

Applications: *Game Routes* (count), *Longest Flight Route* (max + reconstruct), *Course
Schedule* (any order or IMPOSSIBLE), "minimum number of colours = longest path + 1", "number of
vertices reachable" (bitset over reverse topological order), Dilworth on DAG = matching (ch12).

**Investigation (CSES 1202)**: Dijkstra from 1 gives `dist[]`; the edges with `dist[u] + w ==
dist[v]` form the shortest-path DAG. Number of shortest paths, min and max edge count along
shortest paths = three DAG DPs on that DAG, or — since weights are positive — merged into
Dijkstra as in §1.1. Positive weights guarantee the shortest-path DAG has no 0-cycles.

**Longest path in a general graph is NP-hard**; in a DAG it is linear. When a statement asks for
longest path with n ≤ 10⁵ the graph is a DAG or you must use SCC condensation (§5) and it asks
about walks, or it is a tree (two BFS diameter, ch10).

---

## 4. Eulerian paths and circuits

Euler path = uses every *edge* exactly once. Existence:

| | Circuit (start = end) | Path (start ≠ end) |
|---|---|---|
| Undirected | all degrees even | exactly two odd vertices: the endpoints |
| Directed | in(v) = out(v) ∀v | out(s) = in(s)+1, in(t) = out(t)+1, others balanced |
| plus | all edges in one connected component (isolated vertices are fine) | |

Proof sketch (undirected circuit): necessity — each visit enters and leaves. Sufficiency — walk
greedily from s never reusing edges; you can only get stuck at s (every other vertex you enter has
an unused exit by even degree). The walk is a closed trail; remove it, degrees stay even, recurse
on the remaining components (each touches the trail by connectivity) and splice. Hierholzer's
algorithm does exactly this splicing with a stack:

```cpp
// directed; edges[i] = (a, b); g[a] holds edge ids; ptr[u] = next unused out-edge
vector<int> stk{s}, path;
while (!stk.empty()) {
    int u = stk.back();
    if (ptr[u] < (int)g[u].size()) stk.push_back(edges[g[u][ptr[u]++]].second);
    else { path.push_back(u); stk.pop_back(); }
}
reverse(path.begin(), path.end());
if ((int)path.size() != m + 1) /* edges unreachable from s */ ;
```

When stuck at `u` (no unused out-edges), `u` goes to the output — vertices come out in reverse
order of *finishing*, which is a valid Euler path reversed. O(n+m). Undirected: store `(to, id)`
and a `used[id]` array; advance `ptr[u]` past used edges.

Check degrees first, then run, then check `path.size() == m+1` (catches disconnected edges).
Recursive Hierholzer dies at m = 2·10⁵ (stack depth = m) — use the iterative version above.

**Mail Delivery** (undirected circuit from 1), **Teleporters Path** (directed path 1→n): both are
exactly this. **De Bruijn sequence** of order n over {0,1}: vertices = binary strings of length
n−1, edges = strings of length n (from prefix to suffix); every vertex has in = out = 2, so an
Euler circuit exists and reading the first character of each edge in order gives a cyclic string
of length 2ⁿ containing every n-string once; print the start vertex then the last char of every
edge. **Knight's Tour** is Hamiltonian, not Eulerian — Warnsdorff's heuristic (always move to
the square with fewest onward moves, tie-break arbitrarily) finds one on 8×8 almost instantly
with backtracking as a fallback.

**Counting Eulerian subgraphs** (CSES 2078): an edge subset where every degree is even = an
element of the cycle space; its size is 2^(m − n + c) with c connected components.

---

## 5. Hamiltonian paths

Visit every *vertex* exactly once. NP-hard; in contests n ≤ 20 ⇒ bitmask DP over
`dp[mask][v]` = number of (or min cost of) paths starting at 0 visiting exactly `mask`, ending at
`v`. O(2ⁿ · m) with adjacency lists, O(2ⁿ n²) with a matrix. 2²⁰ · 20 longs = 160 MB with `long
long` — use `int` mod p (80 MB) or roll the DP by popcount layers when memory is 64 MB.

```cpp
dp[1][0] = 1;
for (int mask = 1; mask < (1 << n); mask++)
    for (int v = 0; v < n; v++) { if (!dp[mask][v]) continue;
        if (v == n - 1 && mask != full) continue;      // Hamiltonian Flights: n-1 must be last
        for (int w : g[v]) if (!(mask >> w & 1)) dp[mask | 1 << w][w] += dp[mask][v]; }
```

Iterating `mask` in increasing order is a valid topological order because `mask | bit > mask`.
Multi-edges count as distinct routes automatically. Full chapter treatment in ch13 (bitmask DP).

---

## 6. Strongly connected components

u ~ v iff each reaches the other. Equivalence classes = SCCs; the condensation (one vertex per
SCC) is a DAG (a cycle of SCCs would merge them). This is the universal trick for "any directed
graph → DAG, then DP".

### 6.1 Tarjan — one DFS

`idx[u]` = DFS entry time. `low[u]` = min entry time reachable from u's subtree using tree edges
plus at most one back/cross edge *to a vertex still on the stack*. `u` is the root of its SCC iff
`low[u] == idx[u]`: then nothing in the subtree escapes to an earlier vertex, and everything on the
stack above `u` is in u's SCC (they all reach `u` — they are descendants that did not close their
own SCC — and `u` reaches them). Pop them.

```cpp
void dfs(int u) {
    idx[u] = low[u] = timer++; stk.push_back(u); onstk[u] = 1;
    for (int v : g[u]) {
        if (idx[v] == -1) { dfs(v); low[u] = min(low[u], low[v]); }
        else if (onstk[v]) low[u] = min(low[u], idx[v]);
    }
    if (low[u] == idx[u]) {
        while (true) { int v = stk.back(); stk.pop_back(); onstk[v] = 0; comp[v] = ncomp; if (v == u) break; }
        ncomp++;
    }
}
```

Why `onstk` matters: a cross edge to an already-completed SCC must not lower `low` (that SCC is
downstream and closed). Components are numbered in **reverse topological order** (the first
component completed is a sink of the condensation). Every condensation edge goes from a higher
comp id to a lower one — handy: iterate `comp = 0..ncomp-1` and you process sinks first.

Trace on 0→1, 1→2, 2→0, 2→3, 3→4, 4→3, 4→5:

```
dfs 0 (idx0) -> 1 (idx1) -> 2 (idx2): back edge 2->0 : low[2]=0
   2 -> 3 (idx3) -> 4 (idx4): back edge 4->3 : low[4]=3 ; 4 -> 5 (idx5): leaf, low=idx -> SCC0={5}
   back at 4: low[4]=3 != 4 ; back at 3: low[3]=3 == idx -> pop 4,3 -> SCC1={3,4}
   back at 2: low[2]=0 ; at 1: low[1]=0 ; at 0: low[0]=0 -> pop 2,1,0 -> SCC2={0,1,2}
condensation: SCC2 -> SCC1 -> SCC0   (ids decrease along edges)
```

### 6.2 Kosaraju — two passes

DFS #1 on G records finish order. DFS #2 on the *reversed* graph, starting vertices in decreasing
finish time; each DFS tree is exactly one SCC. Proof idea: the last-finishing vertex lies in a
source SCC of the condensation; in the reversed graph it reaches exactly its own SCC. Components
come out in **topological order** (comp 0 is a source). Two adjacency lists, two passes, but no
`low` bookkeeping — some people find it harder to get wrong.

### 6.3 Using the condensation

- **Planets and Kingdoms**: just `comp[]`.
- **Flight Routes Check** ("can everyone reach everyone?"): `ncomp == 1`. Simpler: DFS from 1 in
  G and in reverse G; if either misses `x`, print the failing pair.
- **Coin Collector**: DAG DP over the condensation; component value = sum of coins; longest
  path in the DAG by comp id order (Tarjan: iterate ids increasing, push to predecessors — or just
  reverse).
- **New Flight Routes** (min edges to make strongly connected): on the condensation with c > 1
  components, answer = max(#sources, #sinks). Construction: pair the sinks with the sources along
  a DFS so each added edge sink→source links different "chains"; then attach the leftovers.
- **Reachability Queries**: SCC, then bitset closure over the condensation in reverse topological
  order (n ≤ 5·10⁴ ⇒ 5·10⁴ × 5·10⁴ bits = 300 MB — too much; do it in chunks of 64·k source
  vertices, or use the fact that queries are offline).

### 6.4 Recursion depth

Tarjan/Kosaraju recurse to depth n. On CSES (GCC, large stack) n = 10⁵ is fine; on Codeforces
Windows the stack is 256 MB — also fine; on some judges 8 MB stack means ~10⁵ frames of ~80 bytes
is borderline. Iterative Tarjan exists (explicit stack of (vertex, edge index)) — write it once,
keep it in your library.

---

## 7. 2-SAT

Variables x₁..xₙ, clauses (a ∨ b) with literals. Implication graph: 2n vertices (x and ¬x);
clause (a ∨ b) ⇒ edges ¬a→b and ¬b→a. A path means "if the first literal is true the last must
be". Satisfiable ⇔ no xᵢ is in the same SCC as ¬xᵢ.

Assignment: set literal `l` true iff comp[l] comes *later* in topological order than comp[¬l].
With Tarjan numbering (reverse topo) that is `comp[l] < comp[¬l]`. Why consistent: the graph is
symmetric under (negate all literals, reverse all edges), so comp(¬l) ordering is the mirror of
comp(l); and if l is set true and l → l' then comp[l'] ≤ comp[l] < comp[¬l] ≤ comp[¬l'] hence l'
is also true.

```cpp
// literal encoding: variable i -> 2i (true), 2i+1 (false); l ^ 1 negates
void addClause(int a, int b) { scc.add(a ^ 1, b); scc.add(b ^ 1, a); }   // a or b
void addImplication(int a, int b) { scc.add(a, b); scc.add(b ^ 1, a ^ 1); }
void setTrue(int a) { scc.add(a ^ 1, a); }                                // (a or a)
bool solve() { scc.run();
    for (int i = 0; i < n; i++) { if (scc.comp[2*i] == scc.comp[2*i+1]) return false;
        value[i] = scc.comp[2*i] < scc.comp[2*i+1]; }
    return true; }
```

Clause shapes to memorise: "at least one" = (a ∨ b); "not both" = (¬a ∨ ¬b); "a ⇒ b" = (¬a ∨ b);
"exactly one of a, b" = (a ∨ b) ∧ (¬a ∨ ¬b); "a is forced" = (a ∨ a). "At most one of k literals"
naively needs k² clauses — use prefix variables pᵢ = "some of the first i is true": pᵢ₋₁ ⇒ pᵢ,
xᵢ ⇒ pᵢ, pᵢ₋₁ ⇒ ¬xᵢ; O(k) clauses.

*Giant Pizza*: m toppings, n people each with two literals; O(n + m). Typical CF: place
rectangles/segments in one of two positions without overlap (each pair of conflicting positions
gives a "not both" clause).

---

## 8. Bridges, articulation points, 2-edge-connectivity

Undirected graph. Bridge = edge whose removal increases the number of components. Articulation
point (AP) = vertex whose removal does.

`tin[u]` = DFS entry time; `low[u]` = min of `tin[u]`, `tin[x]` over back edges from the subtree
of `u` to `x`, and `low[child]`. Tree edge (p, u) is a bridge ⇔ `low[u] > tin[p]`: the subtree of
`u` has no back edge to `p` or above, so removing the edge cuts the subtree off. Conversely, if
`low[u] ≤ tin[p]` some back edge from the subtree reaches an ancestor, giving a second route.
Non-tree edges are never bridges (they lie on a cycle with tree edges).

Non-root `p` is an AP ⇔ some child `u` has `low[u] ≥ tin[p]` (that child's subtree cannot bypass
`p`). The root is an AP ⇔ it has ≥ 2 DFS children. Note `≥` for APs vs `>` for bridges: a back
edge to `p` itself saves the edge (p,u) but not the vertex `p`.

```cpp
void dfs(int u, int pe) {                              // pe = parent EDGE id
    tin[u] = low[u] = timer++; int children = 0;
    for (auto [v, id] : g[u]) {
        if (id == pe) continue;                        // skip only the edge we came by
        if (tin[v] != -1) low[u] = min(low[u], tin[v]);          // back edge
        else { dfs(v, id); low[u] = min(low[u], low[v]); children++;
            if (low[v] > tin[u]) isBridge[id] = 1;
            if (low[v] >= tin[u] && pe != -1) isAP[u] = 1; }
    }
    if (pe == -1 && children >= 2) isAP[u] = 1;
}
```

**Multi-edges and self-loops.** Skipping the parent *vertex* is the classic bug: with two parallel
edges a–b neither is a bridge, but "skip parent vertex" ignores the second edge and reports a
bridge. Skip the parent edge *id*. Self-loops: `tin[v] != -1` with v = u lowers `low[u]` to
`tin[u]` — harmless.

Trace: edges 0–1, 1–2, 2–0, 2–3, 3–4:

```
dfs 0(t0) -> 1(t1) -> 2(t2): back edge 2-0: low[2]=0 ; 2 -> 3(t3) -> 4(t4): leaf low[4]=4
  at 3: low[4]=4 > tin[3]=3 -> edge 3-4 bridge ; low[4] >= tin[3] -> 3 is AP
  at 2: low[3]=3 > tin[2]=2 -> edge 2-3 bridge ; 3 >= 2 -> 2 is AP ; low[2]=0
  at 1: low[2]=0 <= tin[1]=1 -> not bridge, not AP ; at 0 (root): 1 child -> not AP
bridges {2-3, 3-4}; APs {2, 3}; 2-edge-connected components {0,1,2} {3} {4}
```

**2-edge-connected components**: remove bridges, connected components of the rest. **Bridge
tree**: one vertex per 2ECC, one edge per bridge — a tree (forest). Queries "is there a path from
a to b avoiding edge e" or "how many bridges on every a–b path" become tree problems (LCA,
distance). CSES *Necessary Roads* (bridges), *Necessary Cities* (APs), *Strongly Connected Edges*
(orient an undirected graph to be strongly connected: possible ⇔ no bridges; orient DFS tree edges
down and back edges up — Robbins' theorem).

**Biconnected components / block-cut tree.** Vertex analogue: maximal subgraphs with no AP.
Found with the same DFS by keeping an edge stack and popping when `low[v] ≥ tin[u]`. Block-cut
tree: one vertex per block and per AP, AP joined to the blocks containing it — a tree. Used for
"path from a to b avoiding vertex c" (CSES *Forbidden Cities*: c blocks a–b ⇔ c is an AP on the
block-cut-tree path between a's and b's blocks) and "number of vertices on some simple path from
a to b". Write it when you need it; know that it exists and what it answers.

---

## 9. Bipartite check

BFS 2-colouring; a same-colour edge ⇒ odd cycle ⇒ not bipartite. O(n+m). Equivalently DSU with
parity, which also handles online edge additions. Bipartite ⇒ matching/König machinery of ch12
applies; non-bipartite ⇒ general matching (blossom, rarely needed).

*Building Teams*: 2-colour each component, IMPOSSIBLE on conflict. Note the graph may be
disconnected — start BFS from every uncoloured vertex.

---

## 10. Minimum spanning trees

**Cut property**: for any partition (S, V∖S), the lightest crossing edge is in some MST (exchange
argument: an MST without it has another crossing edge on the cycle created by adding it; swap,
weight does not increase). **Cycle property**: the heaviest edge on any cycle is in no MST unless
tied. Both algorithms are the cut property applied repeatedly.

### 10.1 Kruskal — O(m log m)

Sort edges; take an edge iff it joins two DSU components. When (a,b) is considered, all lighter
edges inside a's component were already handled, so (a,b) is the lightest edge crossing the cut
"a's component vs rest" — safe. Sorting dominates; DSU is O(m α(n)).

```cpp
sort(edges.begin(), edges.end(), [](auto& x, auto& y) { return x.w < y.w; });
DSU d(n); ll total = 0; int taken = 0;
for (auto [a, b, w] : edges) if (d.unite(a, b)) { total += w; taken++; }
if (taken < n - 1) /* disconnected: IMPOSSIBLE */ ;
```

Check `taken == n−1` — the standard forgotten case (*Road Reparation*). Maximum spanning tree:
sort descending. *Road Construction*: online Kruskal — after each union print components count
and max size, both O(1) to maintain.

### 10.2 Prim — O(m log m) with heap, O(n²) with a matrix

Grow one tree from a vertex; heap keyed by edge weight (not path length — the only difference from
Dijkstra); skip popped vertices already in the tree. Prefer Prim on dense graphs given as an n×n
matrix (n = 5000 ⇒ 2.5·10⁷ ops vs sorting 1.25·10⁷ edges).

### 10.3 Borůvka — O(m log n)

Each round every component picks its cheapest outgoing edge; add all of them (ties broken by a
total order on edges so no cycle forms). Components at least halve per round ⇒ ≤ log n rounds.
Why care: the "cheapest outgoing edge per component" step can often be done *without listing
edges* — e.g. XOR-MST (edge weight = aᵢ xor aⱼ, cheapest via binary trie), or MST on points with
Manhattan/Euclidean distance. Borůvka is the MST algorithm for implicit dense graphs.

### 10.4 Uniqueness, must-have edges, edge classification

- MST is unique iff for every edge not in the MST, the max edge on its tree path is strictly
  lighter. Equivalently: in Kruskal, within a group of equal-weight edges, no edge that gets
  rejected could have been accepted (check with DSU before uniting the group).
- **MST that must contain given edges**: union the mandatory edges first (cost added; if they
  form a cycle, impossible), then Kruskal on the rest.
- **Is edge e in some MST?** (*MST Edge Check*) e = (a,b,w): among edges with weight < w, are a
  and b already connected? If no ⇒ e in some MST. Process weight groups: union all edges lighter,
  then answer queries of that weight, then union the group. **In every MST?** Additionally e is a
  bridge among edges of weight ≤ w (bridge-finding on the "critical" subgraph per weight group,
  or Tarjan on the DSU-compressed graph per group — CF 160D).
- **MST for each edge** (*MST Edge Cost*, CF 609E): cost of the cheapest spanning tree containing
  e = MST − maxEdge(path a–b in MST) + w. Binary lifting with max (ch10), O(m log n).
- **Second-best MST**: theorem — some second-best tree differs from any fixed MST T by one
  exchange. So `min over non-tree e=(a,b,w) of MST − maxOnPath(a,b) + w`. If "second-best" must
  have strictly larger weight, use the second maximum when the maximum equals w (store (max1,
  max2 distinct) in the lifting table). O(m log n). Both variants are verified against brute force
  in `example.cpp`.

### 10.5 Kruskal reconstruction tree

Run Kruskal; each time an edge (a,b,w) unites two components create a new node with weight w whose
children are the two component roots. You get a binary tree with 2n−1 nodes, leaves = original
vertices, node weights non-decreasing towards the root. Then:

- bottleneck (minimax) path weight between u and v = weight of LCA(u,v);
- "vertices reachable from u using edges of weight ≤ w" = leaves of the highest ancestor of u with
  weight ≤ w (binary lift up while weight ≤ w) — a contiguous range in the leaf order, so subtree
  queries become range queries;
- for maximum spanning tree version: "max over paths of min edge" (*Transfer Speeds Sum* wants
  the sum over all pairs — do the DSU merge counting `size[a]·size[b]·w` instead).

*New Roads Queries* ("first moment a and b are connected") = LCA weight when edge weight = time.
The offline alternative is DSU with a merge time on parent pointers (no path compression, union
by size, walk up).

---

## 11. Functional (successor) graphs

Every vertex has exactly one out-edge. Structure: each component is one cycle with trees hanging
off it (in-trees pointing towards the cycle).

**k-th successor** (*Planets Queries I*): binary lifting `up[j][v] = up[j-1][up[j-1][v]]`,
O(n log K) memory/time, query O(log K). K ≤ 10⁹ ⇒ 30 levels × 2·10⁵ ints = 24 MB.

**Cycle decomposition** (*Planets Cycles*): walk from each unvisited vertex marking "on current
path"; when you hit a vertex on the current path you found a new cycle — assign its length to all
cycle vertices; then unwind the tail assigning `ans[v] = ans[next[v]] + 1`. When you hit a
finished vertex, just unwind. O(n), iterative.

**Distance queries** (*Planets Queries II*, "how many steps from a to b, or −1"): both on the same
cycle ⇒ (pos[b] − pos[a]) mod len; a in a tree, b in a tree ⇒ b must be an ancestor of a on the
path to the cycle: depth difference + check `kth(a, depth[a]−depth[b]) == b`; a in a tree, b on
the cycle ⇒ depth[a] + cycle distance from a's entry point. Floyd's tortoise-and-hare finds the
cycle from one start with O(1) memory — needed only when n is huge and you cannot store visited
marks.

---

## 12. Modeling patterns

| Pattern | How | Example |
|---|---|---|
| Small extra state | vertex × state, implicit product graph | Flight Discount, "k free edges", parity |
| Layered / time-expanded graph | copy graph per time step t ≤ T, edges go t→t+1 | trains with timetables, "wait or move" |
| Reverse edges | shortest paths *to* t from all v = Dijkstra from t on reversed graph | Investigation-style "to n", "all reach 1" |
| Super source / sink | extra vertex with 0-edges to all sources | multi-source Dijkstra / BFS (nearest of many) |
| Edge splitting | vertex per edge when the constraint is on consecutive edges | "no two consecutive roads of same colour" |
| Vertex with capacity/cost | split into in/out with an internal edge | vertex-weighted shortest path, vertex-disjoint paths (ch12) |
| Grid | cell = r·W + c, 4/8 neighbours computed | Labyrinth, Monsters, Counting Rooms |
| Complement graph BFS | keep set of unvisited, remove reached ones | when m = "all pairs except k" |
| Binary search on answer + graph check | monotone predicate "connected using edges ≤ x" | bottleneck problems if no reconstruction tree |
| Second-nearest source | keep two best (dist, source) per vertex with distinct sources | *Nearest Shops* ("nearest shop of another type") |

---

## 13. Pitfalls checklist

- `int` distances overflow: 2·10⁵ × 10⁹. Use `long long` and `INF = LLONG_MAX/4` (or `1e18`).
- `INF + w` with negative `w` becomes a finite value — guard `d[a] < INF` in Bellman–Ford/Floyd.
- Dijkstra on negative edges: wrong, not slow. Dijkstra without the stale check: O(nm) TLE.
- Floyd with `k` not outermost: wrong.
- Bridges skipping parent vertex instead of parent edge: wrong on multi-edges.
- Euler path: check connectivity of the *edges* (path length m+1), and degrees; recursion depth
  m in recursive Hierholzer — go iterative.
- DAG DP with unreachable vertices initialised to 0: fake paths.
- Tarjan: forgot `onstk` check → cross edges corrupt `low`. Kosaraju: DFS #2 must run in
  *decreasing* finish time on the *reversed* graph.
- 2-SAT: literal encoding `2i`, `2i+1`, negation `^1`; clause (a ∨ b) adds ¬a→b AND ¬b→a.
- Recursion depth 2·10⁵ in DFS-based algorithms: on Codeforces fine; on judges with 8 MB stacks
  consider iterative versions or `#pragma comment(linker, "/STACK:...")` (MSVC only).
- Reading a graph with 1-indexed vertices: subtract 1 once at input and never think about it
  again; add 1 once at output.
- `endl` in a loop printing 2·10⁵ lines: flushes each time, TLE. Use `'\n'` and
  `ios::sync_with_stdio(false); cin.tie(nullptr);`.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| shortest, weights ≥ 0 | Dijkstra | O((n+m) log m) |
| shortest, one coupon / k modifications | Dijkstra on (v, state) | O(k(n+m) log) |
| shortest with negative edges, n·m ≤ 10⁸ | Bellman–Ford | O(nm) |
| "find a negative cycle" / "arbitrage" | Bellman–Ford n rounds + par walk | O(nm) |
| all pairs, n ≤ 500 | Floyd–Warshall | O(n³) |
| reachability all pairs, n ≤ 5000 | bitset closure (topo order if DAG) | O(nm/64) or O(n³/64) |
| weights {0,1} | 0-1 BFS | O(n+m) |
| k-th shortest walk | Dijkstra with k pops per vertex | O(km log) |
| count / longest path, DAG | topo order DP | O(n+m) |
| "order courses", lexicographic tie-break | Kahn with heap (reverse trick) | O((n+m) log n) |
| "use every road once" | Euler path (Hierholzer) | O(n+m) |
| "visit every city once", n ≤ 20 | bitmask DP | O(2ⁿ n²) |
| "everyone reaches everyone", "kingdoms" | SCC | O(n+m) |
| "at least one of two wishes" | 2-SAT via SCC | O(n+m) |
| "roads whose closure disconnects" | bridges | O(n+m) |
| "cities whose closure disconnects" | articulation points | O(n+m) |
| "avoid city c between a and b" | block-cut tree + LCA | O((n+m) log n) |
| "cheapest network connecting all" | Kruskal / Prim | O(m log m) |
| "cheapest tree containing edge e" / "second cheapest" | MST + path max (lifting) | O(m log n) |
| "min over paths of max edge", many queries | Kruskal reconstruction tree | O((n+q) log n) |
| "teleporter to exactly one planet" + k steps | binary lifting | O(n log k) |
| "two teams, friends in different teams" | bipartite check | O(n+m) |

## Implementation checklist for contests

1. `long long` for distances/weights sums; `INF = LLONG_MAX/4`; check the answer can be INF.
2. Directed or undirected? Multi-edges? Self-loops? Read the statement twice.
3. 0/1-indexing converted once at I/O.
4. Dijkstra: stale-pop check present; heap comparator is `greater<>`.
5. Bellman–Ford: `d[a] < INF` guard; n rounds for cycle detection; par-walk n times.
6. DAG DP: unreachable sentinel; topological order computed once; cycle ⇒ IMPOSSIBLE branch.
7. Euler: degree check → Hierholzer → length check → output; iterative.
8. Tarjan: `onstk`; comp numbering direction (reverse topo) used consistently in DP / 2-SAT.
9. Bridges: parent edge id; root AP rule; multi-edge test in your stress test.
10. Kruskal: `taken == n−1`; sort by weight only (stable order irrelevant).
11. Stress-test against brute force (Floyd for all shortest path variants; removal loops for
    bridges/APs; enumeration for 2-SAT) on n ≤ 8 random graphs before submitting anything
    non-trivial — `example.cpp` shows the harness.
12. Output format: "IMPOSSIBLE" spelled exactly; paths printed with count first when asked.

## Further reading

- CPH (Laaksonen) chapters 11–20: graph traversal, shortest paths, spanning trees, directed
  graphs, strong connectivity, tree queries, paths and circuits, flows (next chapter).
- cp-algorithms.com: "Dijkstra", "0-1 BFS", "Bellman-Ford", "Finding a negative cycle",
  "Floyd-Warshall", "Topological sorting", "Finding bridges in O(N+M)", "Finding articulation
  points", "Strongly connected components and condensation graph", "2-SAT", "Kruskal with DSU",
  "Prim", "Second best MST", "Eulerian path", "Finding the shortest cycle", "Kirchhoff's theorem"
  (for counting spanning trees — ch06).
- Tarjan, "Depth-first search and linear graph algorithms" (1972) — the original low-link paper;
  Hopcroft–Tarjan for biconnectivity (1973).
- Robbins' theorem (1939) — strong orientation ⇔ bridgeless.

## You can move on when...

- You can write Dijkstra, Tarjan SCC, bridges and Kruskal from memory, each compiling first time,
  in under 5 minutes each.
- You can explain in two sentences why Dijkstra fails on negative edges and why `low[v] > tin[u]`
  characterises bridges.
- You have solved every ★1–★3 problem in `problems.md` and at least half of the ★4s, including
  Flight Discount, Investigation, Giant Pizza, Mail Delivery, Necessary Roads, Planets Queries II
  and New Flight Routes.
- Your stress-test harness for graphs (random small graph + brute force) takes you < 10 minutes
  to set up during a contest.
