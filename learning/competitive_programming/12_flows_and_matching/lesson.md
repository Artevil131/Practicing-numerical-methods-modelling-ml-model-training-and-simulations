# Chapter 12 — Flows and Matching

Prerequisites: Chapter 11 (BFS/DFS, Dijkstra, Bellman–Ford, DAGs, bipartite check) and DSU from
`../../algorithms_learning/06_graphs/lesson.md`. Reference code: `example.cpp` — Dinic, Edmonds–Karp,
Kuhn, Hopcroft–Karp, König cover, min cost flow (SPFA and Dijkstra+potentials), Hungarian,
lower bounds, project selection, all brute-force verified.

Flow problems are recognised, not derived: the statement never says "flow". It says *assign*,
*disjoint*, *cut*, *cover*, *at most one*, *two kinds of objects*, *minimum number of removals to
disconnect*. This chapter is about (a) three algorithms you can type in five minutes each and
(b) the catalogue of reductions that turn statements into them.

## What you'll be able to do after this chapter

- State and prove max-flow = min-cut, and read a minimum cut off the residual graph.
- Implement Dinic from memory (level graph + blocking flow with iterator pointers) and know its
  bounds: O(V²E) general, O(E√V) unit capacities, O(E√V) bipartite matching.
- Decompose a flow into paths; compute edge-disjoint and vertex-disjoint paths (Menger).
- Maximum bipartite matching via Kuhn (O(VE)) and Hopcroft–Karp (O(E√V)); recover a minimum
  vertex cover (König) and a maximum independent set; minimum path cover in a DAG (Dilworth).
- Model project selection / closure problems as min cut; handle lower bounds and circulations.
- Min cost max flow with successive shortest paths (SPFA or Dijkstra with Johnson potentials);
  the assignment problem via Hungarian O(n³) and know when each is appropriate.
- Recognise a flow problem from the statement shape and pick the modeling pattern.

## Where this shows up in contests

| Signal | Placement |
|---|---|
| "maximum number of edge-disjoint / vertex-disjoint paths", "minimum roads to close" | CF Div2 E–F (2100–2400), CSES Police Chase / Distinct Routes |
| "assign tasks to workers, each at most one, maximise pairs" | bipartite matching; CF Div2 D–E, BOI |
| "choose a set of cells so that no two share a row/column" on a grid | matching on rows × columns; König; CF 1900–2200 |
| "projects with profit, some require others; maximise profit" | closure / project selection min cut; CF 2300+ |
| "minimum total cost to assign n workers to n jobs" | Hungarian or MCMF; ICPC regionals |
| "k paths total minimum cost" | MCMF with flow limit k |
| "minimum number of chains covering a poset" | Dilworth = matching |
| Small V (≤ 500–1000), moderate E, exact optimisation with "each at most once" | almost always flow |

IOI itself rarely requires a flow algorithm (it is on the syllabus as "not required" in recent
years), but BOI/CEOI/JOI do use bipartite matching and min cut, and Codeforces uses them
constantly from ~2100 upward. Matching also appears as a subroutine inside harder problems (e.g.
verifying Hall's condition, or a greedy matching in a checker).

---

## 1. Definitions and the max-flow min-cut theorem

Flow network: directed graph, capacity c(e) ≥ 0, source s, sink t. A flow f satisfies
0 ≤ f(e) ≤ c(e) and conservation (in = out) at every vertex except s, t. Value |f| = net out of s.
An s-t cut (S, T) is a vertex partition with s ∈ S, t ∈ T; capacity = sum of c(e) over edges from S
to T.

**Theorem.** max |f| = min cap(S,T).

*Easy direction:* for any f and any cut, |f| = (flow S→T) − (flow T→S) ≤ cap(S,T). (Sum
conservation over S.)

*Hard direction:* take a maximum flow f. Residual graph G_f has edge u→v with capacity
c(u,v) − f(u,v) (leftover) and v→u with capacity f(u,v) (cancel). Let S = vertices reachable
from s in G_f. t ∉ S (else an augmenting path exists and f is not maximum). Every original edge
S→T is saturated (else its residual would put the endpoint in S) and every edge T→S carries zero
flow (else the reverse residual edge would). So |f| = cap(S,T). Since |f| ≤ every cut, this cut
is minimum and this flow is maximum. ∎

Consequences you use daily:
- **Min cut recovery**: after any max-flow algorithm terminates, BFS from s in the residual
  graph; the reachable set is the S side of a minimum cut. The saturated edges leaving it are the
  cut edges. (Dinic's last BFS already computed exactly this: `level[v] != -1`.)
- **Integrality**: with integer capacities, augmenting-path algorithms produce integer flows.
  That is why "each edge used at most once" becomes capacity 1 and the answer is a count.
- **Menger**: max number of edge-disjoint s-t paths = min number of edges whose removal
  disconnects t from s (capacities 1). Vertex version via splitting (§4).

---

## 2. Augmenting-path algorithms

**Ford–Fulkerson**: while there is an s→t path in the residual graph, push the bottleneck along
it. Terminates for integer capacities (each step adds ≥ 1) in O(E·|f|) — exponential in the input
size in the worst case, and with irrational capacities may not terminate at all. The reverse edge
is the whole point: it lets a later path *undo* an earlier greedy choice.

**Edmonds–Karp**: choose the *shortest* augmenting path (BFS). Key lemma: residual distances from
s never decrease, and each edge becomes a bottleneck at most V/2 times (between two saturations
of (u,v) the distance to v must grow by 2). Hence O(VE) augmentations, O(VE²) total, independent
of capacities. Fine for V ≤ 500, E ≤ 5000 (CSES limits), too slow beyond.

**Dinic**: Edmonds–Karp on steroids — one BFS builds the *level graph* (edges with level[v] =
level[u]+1), then one DFS pass finds a *blocking flow* (a set of shortest augmenting paths after
which no shortest path remains). The s-t distance strictly increases per phase, so ≤ V phases.
Within a phase, the pointer `it[u]` never moves backwards: an edge that failed to carry flow is
dead for the rest of the phase, so the phase costs O(VE) (each augmentation walks ≤ V edges and
there are ≤ E dead ends). Total O(V²E).

```cpp
struct Dinic {
    struct E { int to; ll cap; };
    int n; vector<E> e; vector<vector<int>> g; vector<int> level, it;
    Dinic(int n) : n(n), g(n), level(n), it(n) {}
    int add(int a, int b, ll cap, ll rcap = 0) {      // returns forward edge id; id^1 = reverse
        g[a].push_back(e.size()); e.push_back({b, cap});
        g[b].push_back(e.size()); e.push_back({a, rcap});
        return e.size() - 2;
    }
    bool bfs(int s, int t) {
        fill(level.begin(), level.end(), -1); level[s] = 0;
        queue<int> q; q.push(s);
        while (!q.empty()) { int u = q.front(); q.pop();
            for (int id : g[u]) if (e[id].cap > 0 && level[e[id].to] == -1) { level[e[id].to] = level[u] + 1; q.push(e[id].to); } }
        return level[t] != -1;
    }
    ll dfs(int u, int t, ll f) {
        if (u == t || f == 0) return f;
        for (int& i = it[u]; i < (int)g[u].size(); i++) {       // reference! advances permanently
            int id = g[u][i], v = e[id].to;
            if (e[id].cap <= 0 || level[v] != level[u] + 1) continue;
            ll d = dfs(v, t, min(f, e[id].cap));
            if (d > 0) { e[id].cap -= d; e[id ^ 1].cap += d; return d; }
        }
        return 0;
    }
    ll maxflow(int s, int t) {
        ll flow = 0;
        while (bfs(s, t)) { fill(it.begin(), it.end(), 0); while (ll f = dfs(s, t, INF)) flow += f; }
        return flow;
    }
};
```

Edge storage: one flat `vector<E>`, edge ids in adjacency lists, `id ^ 1` is the reverse. Add an
undirected edge with `rcap = cap`. Flow on forward edge `id` (created with rcap 0) = `e[id^1].cap`.

Trace (s=0, t=3; edges 0→1 (3), 0→2 (2), 1→2 (1), 1→3 (2), 2→3 (3)):

```
phase 1: BFS levels: 0:0  1:1  2:1  3:2
  dfs 0 -> 1 -> 3 : push 2      (1->3 saturated)
  dfs 0 -> 1 -> 2 : level[2] != level[1]+1, skip ; it[1] exhausted -> back to 0, it[0]++
  dfs 0 -> 2 -> 3 : push 2      (0->2 saturated)
  blocking flow found, flow = 4
phase 2: BFS: 0:0, 1:1 (cap 1 left), 2:2 (via 1->2), 3:3 (via 2->3, cap 1 left)
  dfs 0 -> 1 -> 2 -> 3 : push 1.  flow = 5
phase 3: BFS cannot reach 3.  Answer 5. Reachable from s: {0, 1}. Cut edges 0->2 (2), 1->2 (1), 1->3 (2) = 5.
```

**Complexity notes.**
- General: O(V²E). In practice Dinic handles V = 10⁴, E = 10⁵ with mixed capacities well below
  a second on typical tests; the bound is very pessimistic.
- Unit capacities (all edges 1): O(E·min(V^{2/3}, E^{1/2})). With unit capacity *and* every
  vertex having in-degree 1 or out-degree 1 (bipartite matching networks): O(E√V) — the number
  of phases is O(√V) because after √V phases every augmenting path is long, hence few remain.
- **Capacity scaling** (mention): process bit by bit from the highest, only using edges with
  residual ≥ 2^k; O(E² log C) for Ford–Fulkerson. Rarely needed once you have Dinic.
- **Push–relabel** (mention): O(V³) or O(V²√E) with highest-label; faster on dense graphs;
  not worth memorising for contests.

**Pitfalls in Dinic.** Forgetting `fill(it, 0)` per phase (wrong answer); using `int` for
capacities when sums reach 10¹⁴ (overflow: INF should be `LLONG_MAX/4` or `1e18`, and edge
capacity "infinite" should be ≥ sum of all finite capacities, not `INT_MAX` mixed with `ll`);
`it[u]` not a reference (O(E) per augmentation instead of amortised — TLE); recursion depth = path
length ≤ V (fine up to 10⁵). A DFS that pushes *multiple* paths per call (accumulate `f -= d` and
continue instead of returning) is a constant factor faster; the single-return version above is
the one to memorise.

---

## 3. Working with the result

### 3.1 Minimum cut edges (CSES *Police Chase*)

After `maxflow`, `level[v] != -1` ⇔ v reachable in the residual graph. Output every original
edge (u,v) with u reachable and v not. Count equals the flow. For an undirected road network
add both directions with capacity 1 (or `add(a,b,1,1)`).

### 3.2 Flow decomposition into paths (CSES *Distinct Routes*)

Any s-t flow decomposes into ≤ E paths plus cycles. Greedy: from s follow an edge with positive
remaining flow until t; subtract the bottleneck along it; repeat while s has outgoing flow. Use a
per-vertex pointer that only advances (an edge whose flow hits zero is skipped forever) — O(VE)
total. If you encounter a vertex already on the current path you found a flow cycle: cancel it
and restart. With unit capacities each path has multiplicity 1 and cycles cannot reach t, so
"walk edges with flow 1, marking them used" is enough.

### 3.3 Edge-disjoint and vertex-disjoint paths

Edge-disjoint: capacity 1 on every edge, max flow = number of paths (Menger). Vertex-disjoint:
split each v into v_in → v_out with capacity 1 (∞ for s and t); original edge (u,v) becomes
u_out → v_in with capacity 1 (or ∞ — the vertex edge already limits). Also the pattern for "each
city can be visited by at most k routes": internal capacity k.

### 3.4 Multiple sources / sinks, undirected edges, vertex capacities

Super source S with edges S→sᵢ of capacity supply(sᵢ); super sink likewise. Undirected edge =
two antiparallel edges of the same capacity (net flow is what matters). Vertex capacity = split.
Lower bounds — §7.

---

## 4. Bipartite matching

Matching = edge set with no shared vertices. Maximum bipartite matching = max flow on
s → L (cap 1), L → R (cap 1), R → t (cap 1). Dinic gives O(E√V) directly. Two specialised
algorithms are shorter and faster in practice:

### 4.1 Kuhn — O(VE)

Berge's lemma: a matching is maximum ⇔ no augmenting path (alternating path between two free
vertices). Kuhn tries to find one from each free left vertex by DFS: go to a right neighbour r;
if r is free, match; else recurse into r's current partner to re-match it elsewhere.

```cpp
bool tryKuhn(int u) {
    if (vis[u]) return false; vis[u] = 1;
    for (int v : g[u]) if (matchR[v] == -1 || tryKuhn(matchR[v])) { matchR[v] = u; matchL[u] = v; return true; }
    return false;
}
int run() { int res = 0;
    for (int u = 0; u < nl; u++) for (int v : g[u]) if (matchR[v] == -1) { matchR[v] = u; matchL[u] = v; res++; break; }   // greedy seed
    for (int u = 0; u < nl; u++) if (matchL[u] == -1) { fill(vis.begin(), vis.end(), 0); if (tryKuhn(u)) res++; }
    return res; }
```

`vis` is reset per *starting* vertex, not per recursive call. The greedy seed and iterating only
unmatched left vertices typically make Kuhn fast up to V ≈ 10⁴, E ≈ 10⁵ despite the bound.
Another constant-factor trick: reset `vis` lazily with a timestamp. Kuhn is the algorithm to
write when the graph is small or the bipartite graph is implicit and awkward to store.

### 4.2 Hopcroft–Karp — O(E√V)

Phase = BFS from all free left vertices building layers (left vertex → right neighbour →
its partner), then DFS finds a *maximal* set of vertex-disjoint shortest augmenting paths, using
iterator pointers exactly like Dinic. Shortest augmenting path length strictly increases per
phase; after √V phases all remaining augmenting paths have length > √V, so at most √V more
augmentations exist (they are vertex-disjoint) — O(√V) phases, O(E) each.

```cpp
bool bfs() { queue<int> q; bool found = false;
    for (int u = 0; u < nl; u++) { dist[u] = matchL[u] == -1 ? 0 : -1; if (!dist[u]) q.push(u); }
    while (!q.empty()) { int u = q.front(); q.pop();
        for (int v : g[u]) { int w = matchR[v];
            if (w == -1) found = true; else if (dist[w] == -1) { dist[w] = dist[u] + 1; q.push(w); } } }
    return found; }
bool dfs(int u) {
    for (int& i = it[u]; i < (int)g[u].size(); i++) { int v = g[u][i], w = matchR[v];
        if (w == -1 || (dist[w] == dist[u] + 1 && dfs(w))) { matchL[u] = v; matchR[v] = u; return true; } }
    dist[u] = -1; return false; }
int run() { int res = 0;
    while (bfs()) { fill(it.begin(), it.end(), 0); for (int u = 0; u < nl; u++) if (matchL[u] == -1 && dfs(u)) res++; }
    return res; }
```

`example.cpp` asserts Kuhn == Hopcroft–Karp == Dinic == brute force on random graphs; do the
same when you re-type them.

### 4.3 Hall's theorem

A bipartite graph has a matching saturating L ⇔ for every subset X ⊆ L, |N(X)| ≥ |X|. Rarely used
algorithmically (exponential subsets), often used to *prove* a greedy or to convert "does a perfect
matching exist" into a counting condition on structured graphs (intervals, grids, "each left has
degree ≥ k and each right ≤ k" ⇒ perfect matching exists by Hall). Deficiency version: max matching
= |L| − max_X (|X| − |N(X)|).

### 4.4 König's theorem and the minimum vertex cover

In a bipartite graph, min vertex cover = max matching. Construction from a maximum matching M:
from every unmatched left vertex, follow alternating paths (left→right via any edge, right→left
via the matching edge); let Z be the visited vertices. Cover C = (L ∖ Z) ∪ (R ∩ Z).

- C covers every edge (l,r): if l ∈ Z then r ∈ Z (we walk every edge out of l); if l ∉ Z then
  l ∈ C.
- |C| = |M|: every vertex in C is matched (unmatched left vertices are in Z; a right vertex in Z
  that is unmatched would end an augmenting path, contradicting maximality), and no matching edge
  has both endpoints in C (if r ∈ Z its partner l was reached through r, so l ∈ Z, l ∉ C).

```cpp
// after Hopcroft-Karp: visL/visR via alternating DFS from unmatched left vertices
for (int u = 0; u < nl; u++) if (matchL[u] == -1 && !visL[u]) go(u);   // go: visL[u]=1; for v in g[u] if !visR[v]: visR[v]=1, go(matchR[v]) if matched
cover = { u : !visL[u] } ∪ { v : visR[v] }
```

**Maximum independent set** in a bipartite graph = complement of a minimum vertex cover, size
|V| − |M|. Typical shape: "place as many pieces as possible on free cells so that no two attack
along a row/column/diagonal" → build the conflict graph as bipartite (rows vs columns; or two
colour classes of a chessboard) → independent set (CSES *Coin Grid* is the cover version:
minimum lines to remove all coins = min vertex cover of rows × columns).

### 4.5 Dilworth and minimum path cover

Poset: min number of chains covering all elements = max antichain (Dilworth). For a DAG whose
edges are the full transitive closure, chains = paths, so min *vertex-disjoint* path cover =
n − max matching in the split bipartite graph (out-copy u → in-copy v for each edge u→v). Each
matching edge glues two path segments, so paths = n − |M|.

If paths may share vertices ("cover with paths, vertices may be reused") take the transitive
closure first (bitsets, n ≤ ~2000) then the same formula — then it is exactly Dilworth and the
answer equals the maximum antichain (largest set of mutually unreachable vertices).

### 4.6 Other matching shapes

- **b-matching**: left vertex l may be matched up to b(l) times → capacity b(l) on s→l. Flow
  handles it; Kuhn does not.
- **Weighted**: min/max weight perfect matching → Hungarian (§6) or MCMF.
- **General (non-bipartite) matching**: Edmonds' blossom algorithm O(V³), or randomised
  Tutte-matrix rank. Very rare in contests; know it exists, do not memorise it. For *unweighted*
  general matching on small graphs (V ≤ 20) use bitmask DP.

---

## 5. Min cut as a modeling tool

Whenever a problem asks to partition into two sides with penalties for "wrong side" choices and
penalties for cut pairs, think min cut.

### 5.1 Project selection / closure

Items i with profit pᵢ (positive or negative); constraints "if i is chosen then j must be chosen".
Maximise total profit of a closed set. Network:

```
s -> i   cap  p_i     (p_i > 0)   cutting = giving up the profit
i -> t   cap -p_i     (p_i < 0)   cutting = paying the cost
i -> j   cap  INF     for each "i requires j"
answer = sum of positive p_i - mincut ; chosen set = S side
```

Proof: any finite cut has S closed under requirements (an INF edge from S to T would be cut).
Cut cost = Σ_{i∈T, pᵢ>0} pᵢ + Σ_{i∈S, pᵢ<0} (−pᵢ) = (Σ positive) − profit(S). Minimising the cut
maximises profit(S). `example.cpp` verifies this against subset enumeration.

Variants: "choose a subset of vertices maximising Σ profit − Σ (penalty for each edge with exactly
one endpoint chosen)" — penalty edges become undirected capacity edges. "Two teams, each person
prefers a team, friends want to be together" = image segmentation min cut. "Choose closed set in a
DAG with maximum weight" = the maximum-weight closure above.

### 5.2 Other min-cut patterns

- Minimum number of vertices to remove to disconnect s from t → vertex splitting with cap 1.
- "Every path from s to t must pass a chosen edge, minimise chosen weight" → literally min cut.
- Binary labelling with pairwise costs where c(0,1)+c(1,0) ≥ c(0,0)+c(1,1) (submodular) →
  representable as a cut; otherwise not.
- Densest subgraph, ratio problems → binary search + min cut (parametric flow).

---

## 6. Min cost flow

Edge has capacity and cost per unit. Find a maximum flow of minimum total cost (or min cost for
exactly k units). Reverse residual edges have cost −c.

**Successive shortest paths.** Repeatedly augment along a *cheapest* s→t path in the residual
graph. Invariant: after augmenting k units this way the current flow is min-cost among all flows
of value k (a min-cost flow has no negative cycle in its residual graph; augmenting along a
shortest path preserves that). So the cost is convex in the flow amount and you may stop at any
k. Complexity O(F · SP) where SP is one shortest-path computation; F = total flow. Fine when F is
small (≤ 10³–10⁴) or the graph is small.

Two ways to compute the shortest path with negative residual costs:

- **SPFA / Bellman–Ford** each iteration. Simplest; O(F·VE) worst, much faster in practice; this
  is what most contestants write. Negative costs on original edges are fine.
- **Dijkstra with Johnson potentials.** Keep h[v]; reduced cost c'(u,v) = c(u,v) + h[u] − h[v] ≥ 0
  on all residual edges. Initialise h by Bellman–Ford (or zeros if all costs ≥ 0); after each
  Dijkstra set h[v] += dist[v]. Edges on the shortest path have reduced cost 0, so their newly
  created reverse edges also get reduced cost 0 — nonnegativity is preserved. O(F·E log V).

```cpp
// one iteration of the Dijkstra variant (see example.cpp for the full struct)
for each popped u, edge id (u->v) with cap > 0:
    nd = d[u] + cost + h[u] - h[v];  if (nd < d[v]) { d[v] = nd; pe[v] = id; push }
after Dijkstra: for all v with d[v] < INF: h[v] += d[v];
augment along pe[] from t: f = min residual; cost += f * (real cost of each edge)
```

When F is large (e.g. capacities 10⁹), successive shortest paths is too slow; use cost scaling or
network simplex — essentially never needed in olympiad-style contests, and then the intended
solution is a different model.

**Negative cycles in the initial graph** (costs negative and cycles present): saturate negative
edges first or use the lower-bound trick. Rare.

### 6.1 Assignment problem and the Hungarian algorithm

n workers, m ≥ n jobs, cost matrix; assign each worker a distinct job minimising total cost. MCMF
on the complete bipartite graph: F = n, E = nm → O(n · nm log) ≈ O(n³ log) — OK for n ≤ 300. The
**Hungarian algorithm** (Kuhn–Munkres, the O(n²m) potential-based version from e-maxx) handles
n = m = 1000 in well under a second and is only ~30 lines; it also yields the dual potentials.
Use Hungarian for dense complete assignment; MCMF when the bipartite graph is sparse, capacities
exceed 1, or you need "at most k assignments".

The e-maxx implementation (1-indexed, rows n ≤ columns m) is in `example.cpp`, asserted against
permutation brute force. Maximise by negating costs. Rectangular n < m is allowed as-is; n > m:
transpose.

### 6.2 Typical MCMF models

- k edge-disjoint paths of minimum total length (CSES *Distinct Routes II*): capacity 1, cost =
  length, flow limit k.
- Minimum cost bipartite matching of size k ("Task Assignment" is n = m perfect).
- Transportation with supplies and demands: super source/sink with the amounts.
- "Send k units, each edge can be used once, cost per use" — same.

---

## 7. Lower bounds and circulations

Edge with bounds [lo, hi]. Replace by capacity hi − lo and account for the forced lo units:
`excess[b] += lo; excess[a] −= lo`. Add super source S → v with capacity excess[v] for excess > 0
and v → T with −excess[v] for excess < 0. A feasible circulation exists ⇔ max flow S→T saturates
all S edges. For a feasible s-t flow (not circulation) add an edge t→s with [0, ∞) first. For a
maximum s-t flow with lower bounds: after establishing feasibility, remove the S/T edges' effect
by continuing to augment from s to t on the same residual graph. Real flow on an edge = lo + flow
on its reduced edge.

Where it shows up: "each worker must be assigned at least 1 and at most 3 jobs", "each row must
contain between a and b chosen cells" (rounding matrices), "every vertex visited at least once".

---

## 8. Modeling checklist: recognising a flow problem

Words that should trigger the thought:

| Word | Model |
|---|---|
| "at most one", "each ... exactly once" | unit capacity |
| "assign", "pair up", "match", two kinds of objects | bipartite matching (weights ⇒ MCMF/Hungarian) |
| "disjoint paths", "cannot share a road/city" | edge/vertex-disjoint (splitting) |
| "minimum number of roads/cities to close so that ..." | min cut (edges / split vertices) |
| "choose rows or columns to cover all marked cells" | min vertex cover = matching (König) |
| "maximum set with no two conflicting", conflicts bipartite | max independent set = V − matching |
| "projects with profits and prerequisites" | closure / project selection |
| "minimum number of chains / paths covering everything" | Dilworth / path cover |
| "between lo and hi of something per row" | lower bounds |
| "cost per unit", "minimum total cost for k paths" | MCMF |
| small V (≤ 1000), moderate E, exact optimum, no obvious greedy/DP | suspect flow |

Sanity: check the size. Flow on V = 10⁵, E = 10⁶ with arbitrary capacities is not intended; on
V ≤ 500 (CSES) anything goes; bipartite matching on 10⁵ edges with Hopcroft–Karp is fine.

If the graph is bipartite and unit, prefer Hopcroft–Karp/Kuhn over Dinic only for brevity — Dinic
on the flow network is the same complexity and lets you add capacities later.

---

## 9. Pitfalls

- Capacities summing beyond `int`: use `long long` everywhere in the flow struct; INF = 1e18 or
  `LLONG_MAX/4`, and "infinite" edge capacities should be a *separate* large constant (e.g. 1e15)
  so that summing several INF edges in a cut does not overflow.
- Forgetting to reset `it[]` each Dinic phase; forgetting `level` reset in BFS.
- Reverse edge capacity: 0 for directed, cap for undirected. With `add(a,b,c)` twice for an
  undirected edge you get two edges — correct but doubles E; `add(a,b,c,c)` is one pair.
- Reading flow on an edge as `orig − cap` requires storing `orig`; with rcap = 0 use `e[id^1].cap`.
- Vertex splitting: forget to make s and t's internal edge infinite ⇒ answer capped at 1.
- Kuhn: `vis` reset per start vertex; recursion depth ≤ |L|.
- Hopcroft–Karp: `dist[u] = -1` on DFS dead end (without it the phase is O(VE)).
- MCMF: reverse edge cost must be −c; potentials update only for reached vertices; the final
  cost accumulates `f × real cost`, not reduced cost.
- Hungarian: rows ≤ columns; 1-indexing internally; returns −v[0] as the cost.
- Min cut side: BFS on the *residual* graph (cap > 0), not on the original edges.
- Decomposition on non-unit flows: paths carry a multiplicity; on unit flows just mark edges.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| max throughput s→t with capacities | Dinic | O(V²E), fast in practice |
| min roads to close to separate a from b | max flow, residual reachable set | + O(V+E) |
| max edge-disjoint paths (+ print them) | unit-cap flow + decomposition | O(E√V) |
| max vertex-disjoint paths | split vertices | O(E√V) |
| max pairs boy–girl / task–worker | Hopcroft–Karp or Kuhn | O(E√V) / O(VE) |
| min rows+columns to cover cells | König: min vertex cover = matching | O(E√V) |
| max non-attacking placements (bipartite conflicts) | V − max matching | O(E√V) |
| min paths covering DAG vertices | n − matching on split graph | O(E√V) |
| projects, profits, prerequisites | closure min cut | Dinic |
| k paths, min total length | MCMF with flow limit k | O(k · E log V) |
| n×n assignment, dense | Hungarian | O(n³) |
| per-row [lo,hi] constraints | lower bounds + circulation | Dinic |

## Implementation checklist for contests

1. Draw the network on paper: what is a vertex, what is an edge, what does a unit of flow mean,
   what does a cut mean. Only then type.
2. Count vertices (including split copies and super source/sink) and allocate exactly.
3. `long long` capacities; INF constants distinct and non-overflowing; check the answer bound.
4. Dinic: `it` reset per phase; `dfs` uses `int&`; BFS resets `level`.
5. Undirected edges: reverse capacity = capacity.
6. Vertex splitting: infinite internal capacity at s and t.
7. Output stage: min cut from residual reachability; paths via decomposition with used marks.
8. Matching: verify `matchL`/`matchR` consistency in debug; Kuhn `vis` reset per start.
9. König: alternating DFS from *unmatched left* vertices only.
10. MCMF: negative costs? If yes and Dijkstra variant, initialise potentials with Bellman–Ford.
11. Stress test against brute force on tiny instances (cut enumeration for max flow, matching
    enumeration, permutations for assignment) — `example.cpp` shows all three.

## Further reading

- CPH chapter 20 (Flows and cuts): Ford–Fulkerson, min cut, disjoint paths, matching, König,
  path covers.
- cp-algorithms.com: "Maximum flow — Ford-Fulkerson and Edmonds-Karp", "Maximum flow — Dinic",
  "Maximum flow — Push-relabel", "Flows with demands", "Minimum-cost flow", "Assignment problem
  (Hungarian algorithm)", "Kuhn's algorithm for maximum bipartite matching", "Hopcroft-Karp".
- Dinic (1970); Edmonds–Karp (1972); Hopcroft–Karp (1973); Goldberg–Tarjan push–relabel (1988).
- Kleinberg & Tardos, *Algorithm Design*, chapter 7 — the best set of flow reductions with proofs
  (project selection, image segmentation, baseball elimination, survey design).

## You can move on when...

- Dinic, Hopcroft–Karp and the SPFA-based MCMF come out of your fingers in ≤ 5 minutes each and
  pass the `example.cpp` asserts when you re-type them blind.
- You can prove max-flow = min-cut and König in two minutes on paper.
- Given a statement, you can write the network on paper (vertices, edges, capacities, what a
  unit of flow means) before coding, for each of: disjoint paths, path cover, project selection,
  lower-bound rows.
- You have solved all CSES tasks in `problems.md` plus at least four of the external ones.
