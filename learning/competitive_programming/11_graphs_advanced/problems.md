# Chapter 11 — Problems

How to work this ladder:

1. Re-type the relevant struct from `example.cpp` from memory *before* opening the problem. If it
   does not compile first time, fix it, then do it again tomorrow.
2. Solve each task in `learning/competitive_programming/11_graphs_advanced/solutions/<source>_<id>.cpp`
   (e.g. `cses_1195.cpp`, `cf_20C.cpp`).
3. For anything non-trivial, write a brute force (Floyd for shortest paths, edge-removal loops for
   bridges, enumeration for 2-SAT) and stress-test on random graphs with n ≤ 8 before submitting.
4. Time yourself: ★1–★2 should take < 15 min including typing; ★3 < 40 min; ★4 up to 90 min; if
   stuck on a ★4/★5 for more than 90 min, read the hint, then the sketch, then upsolve without
   looking again.
5. After an AC, write one line in the Progress section about what the key observation was.

The whole CSES **Graph Algorithms** section (36 tasks) is mapped below. The four flow tasks
(Download Speed, Police Chase, School Dance, Distinct Routes) are listed in the map but worked in
Chapter 12.

## Map of the CSES Graph Algorithms section

| ID | Task | Technique | Section |
|---|---|---|---|
| 1192 | Counting Rooms | grid DFS/BFS components | algorithms_learning ch06 |
| 1193 | Labyrinth | grid BFS + parent reconstruction | §1.1 |
| 1666 | Building Roads | components (DSU/DFS) | ch06 |
| 1667 | Message Route | BFS + path | §1.1 |
| 1668 | Building Teams | bipartite check | §9 |
| 1669 | Round Trip | undirected cycle (DFS parent) | §2 |
| 1194 | Monsters | multi-source BFS then BFS | §12 |
| 1671 | Shortest Routes I | Dijkstra | §1.1 |
| 1672 | Shortest Routes II | Floyd–Warshall | §1.7 |
| 1673 | High Score | Bellman–Ford + reachable positive cycle | §1.6 |
| 1195 | Flight Discount | Dijkstra on (v, used) | §1.3 |
| 1197 | Cycle Finding | Bellman–Ford negative cycle extraction | §1.6 |
| 1196 | Flight Routes | k shortest walks | §1.2 |
| 1678 | Round Trip II | directed cycle (colors) | §2 |
| 1679 | Course Schedule | topological sort | §3 |
| 1680 | Longest Flight Route | DAG DP longest path | §3 |
| 1681 | Game Routes | DAG DP count paths | §3 |
| 1202 | Investigation | Dijkstra + counting on shortest-path DAG | §3 |
| 1750 | Planets Queries I | binary lifting | §11 |
| 1160 | Planets Queries II | functional graph structure | §11 |
| 1751 | Planets Cycles | cycle decomposition | §11 |
| 1675 | Road Reparation | Kruskal | §10.1 |
| 1676 | Road Construction | online DSU | §10.1 |
| 1682 | Flight Routes Check | SCC / two DFS | §6.3 |
| 1683 | Planets and Kingdoms | SCC | §6 |
| 1684 | Giant Pizza | 2-SAT | §7 |
| 1686 | Coin Collector | SCC + DAG DP | §6.3 |
| 1691 | Mail Delivery | Euler circuit, undirected | §4 |
| 1692 | De Bruijn Sequence | Euler circuit on shift graph | §4 |
| 1693 | Teleporters Path | Euler path, directed | §4 |
| 1690 | Hamiltonian Flights | bitmask DP | §5 |
| 1689 | Knight's Tour | Warnsdorff backtracking | §4 |
| 1694 | Download Speed | max flow | ch12 |
| 1695 | Police Chase | min cut | ch12 |
| 1696 | School Dance | bipartite matching | ch12 |
| 1711 | Distinct Routes | edge-disjoint paths | ch12 |

---

## A. Shortest paths

### 11.1  Message Route  ·  CSES 1667  ·  ★1
https://cses.fi/problemset/task/1667
**Technique:** BFS with parent array, path reconstruction.
<details><summary>Hint</summary>Store `par[v]` when you first reach `v`; walk back from n.</details>
<details><summary>Approach sketch</summary>Unweighted, so BFS from 1 gives shortest distances. Record the parent at discovery time, then follow parents from n to 1 and reverse. Print the vertex count (edges + 1). IMPOSSIBLE if n is never reached.</details>

### 11.2  Labyrinth  ·  CSES 1193  ·  ★1
https://cses.fi/problemset/task/1193
**Technique:** grid BFS, implicit graph, parent direction array.
<details><summary>Hint</summary>Store the direction letter you arrived by, not the parent cell — reconstruction then prints itself.</details>
<details><summary>Approach sketch</summary>BFS from A over free cells; keep `from[r][c]` = index of the direction used. From B walk backwards, pushing letters, reverse. Grid 1000×1000 means 10⁶ cells: flat arrays, no `std::map`, no recursion.</details>

### 11.3  Monsters  ·  CSES 1194  ·  ★2
https://cses.fi/problemset/task/1194
**Technique:** multi-source BFS (super source pattern) + second BFS with a comparison.
<details><summary>Hint</summary>Compute for every cell the earliest time a monster can be there. You may step on a cell only if you arrive strictly earlier.</details>
<details><summary>Approach sketch</summary>BFS #1 from all monsters simultaneously gives `dm[cell]`. BFS #2 from A moves to a neighbour only if `dA + 1 < dm[neighbour]`. Any boundary cell reached is an exit; reconstruct as in Labyrinth. Being strictly earlier at each cell is sufficient because the monster's earliest arrival is monotone along your path.</details>

### 11.4  Shortest Routes I  ·  CSES 1671  ·  ★1
https://cses.fi/problemset/task/1671
**Technique:** Dijkstra, `long long`.
<details><summary>Hint</summary>Distances reach 2·10¹⁴.</details>
<details><summary>Approach sketch</summary>Textbook Dijkstra from 1 with lazy deletion. The only trap is `int` overflow and forgetting the stale-pop `continue` (without it the heap explodes on dense inputs).</details>

### 11.5  Shortest Routes II  ·  CSES 1672  ·  ★2
https://cses.fi/problemset/task/1672
**Technique:** Floyd–Warshall.
<details><summary>Hint</summary>n ≤ 500, q ≤ 10⁵: all pairs once, O(1) per query.</details>
<details><summary>Approach sketch</summary>Initialise the matrix with the minimum over parallel edges, run the k-i-j triple loop, answer queries from the matrix (−1 for INF). Use `long long` and skip `d[i][k] == INF` rows for speed.</details>

### 11.6  Flight Discount  ·  CSES 1195  ·  ★3
https://cses.fi/problemset/task/1195
**Technique:** Dijkstra on state (vertex, coupon used).
<details><summary>Hint</summary>The best way to reach a city depends on whether you still hold the coupon. Double the graph.</details>
<details><summary>Approach sketch</summary>States (v,0) and (v,1). From (u,0) an edge of weight w leads to (v,0) at cost w and to (v,1) at cost ⌊w/2⌋; from (u,1) only to (v,1) at cost w. Answer dist[(n,1)] (there is always at least one edge on a path from 1 to n, so using the coupon never hurts). Alternative: forward Dijkstra from 1, backward from n, min over edges of d1[u] + w/2 + dn[v] — same idea with two runs.</details>

### 11.7  Flight Routes  ·  CSES 1196  ·  ★3
https://cses.fi/problemset/task/1196
**Technique:** k shortest walks (Dijkstra with k pops per vertex).
<details><summary>Hint</summary>Routes may repeat cities. Let every vertex be popped up to k times.</details>
<details><summary>Approach sketch</summary>Heap of (distance, vertex); pop; if `cnt[v] == k` skip, else `cnt[v]++`, and if `v == n` record the distance; push all neighbours. Stop when n has been popped k times. The i-th pop of a vertex is its i-th smallest walk length by the same exchange argument as Dijkstra.</details>

### 11.8  High Score  ·  CSES 1673  ·  ★3
https://cses.fi/problemset/task/1673
**Technique:** Bellman–Ford on negated weights + reachability filtering of positive cycles.
<details><summary>Hint</summary>A positive cycle only matters if it lies on some walk from 1 to n.</details>
<details><summary>Approach sketch</summary>Negate weights; run n−1 rounds from 1; in round n, every vertex that still improves is on or after a negative cycle. If any such vertex is reachable from 1 (it is, since its distance is finite) and can reach n (precompute with DFS on the reversed graph from n), print −1. Otherwise print −dist[n]. Guard `d[a] < INF`.</details>

### 11.9  Cycle Finding  ·  CSES 1197  ·  ★3
https://cses.fi/problemset/task/1197
**Technique:** Bellman–Ford negative cycle extraction anywhere in the graph.
<details><summary>Hint</summary>Start all distances at 0 and walk `par[]` n times from the last relaxed vertex.</details>
<details><summary>Approach sketch</summary>All-zero initialisation simulates a virtual source connected to every vertex, so any negative cycle is found regardless of reachability. Run exactly n rounds recording parents; if round n relaxes vertex x, follow `par` n times to land inside the cycle, then output it until the start repeats. Print with the first vertex repeated at the end.</details>

### 11.10  Investigation  ·  CSES 1202  ·  ★3
https://cses.fi/problemset/task/1202
**Technique:** Dijkstra with counters merged in, or shortest-path DAG + DP.
<details><summary>Hint</summary>All weights are positive, so when `u` is popped its `cnt/min/max` are final and may be pushed to neighbours.</details>
<details><summary>Approach sketch</summary>On strict improvement copy `cnt[u]`, `mn[u]+1`, `mx[u]+1` into v; on equality add `cnt` (mod 10⁹+7), take min/max. Correctness needs positive weights (a zero edge could pop v before u finishes). If you distrust this, extract the DAG of tight edges after Dijkstra and run three topological DPs.</details>

### 11.11  Nearest Shops  ·  CSES 3303  ·  ★4
https://cses.fi/problemset/task/3303
**Technique:** multi-source Dijkstra keeping the two best (distance, origin) with distinct origins.
<details><summary>Hint</summary>Each vertex needs the nearest shop *other than itself*. Two labels per vertex suffice.</details>
<details><summary>Approach sketch</summary>Run Dijkstra from all shops at once but store per vertex up to two entries (dist, shopId) with different shopIds; a vertex is finalised for a label the first two times distinct labels pop. Relax neighbours with the popped label. Each vertex is processed at most twice, so O((n+m) log). Answer for shop s = its second label's distance (the first is itself at 0).</details>

### 11.12  Dijkstra?  ·  Codeforces 20C  ·  CF 1900
https://codeforces.com/problemset/problem/20/C
**Technique:** Dijkstra with path reconstruction and `long long`.
<details><summary>Hint</summary>The title is the hint; the traps are overflow and printing the path.</details>
<details><summary>Approach sketch</summary>Standard Dijkstra 1→n with parents. Weights up to 10⁶ over 10⁵ edges: `int` overflows. Print −1 if unreachable.</details>

### 11.13  Jzzhu and Cities  ·  Codeforces 449B  ·  CF 2000
https://codeforces.com/problemset/problem/449/B
**Technique:** Dijkstra with tie-aware counting of "how many ways to reach v at distance dist[v]".
<details><summary>Hint</summary>A train route to v is removable if dist[v] < its length, or if dist[v] equals it and some other tight edge also reaches v.</details>
<details><summary>Approach sketch</summary>Add trains as edges from the capital. Run Dijkstra; for each vertex count the number of tight incoming edges (edges with dist[u] + w == dist[v]). A train of length exactly dist[v] can be dropped if the count of tight edges into v exceeds the number of such trains kept — remove all but one, or all if a road is tight.</details>

### 11.14  Greg and Graph  ·  Codeforces 295B  ·  CF 1700
https://codeforces.com/problemset/problem/295/B
**Technique:** Floyd–Warshall in reverse deletion order.
<details><summary>Hint</summary>Deleting vertices forward = adding them backward, and adding vertex k is one outer Floyd iteration.</details>
<details><summary>Approach sketch</summary>Process the deletion list from the end; after adding vertex k as an intermediate (outer loop body for k), sum d[i][j] over already-added i, j. Output the sums reversed. O(n³), n ≤ 500.</details>

### 11.15  Crocodile  ·  IOI 2011 (oj.uz)  ·  ★4
**Technique:** Dijkstra variant — a vertex is settled when it is popped the *second* time (second-best distance).
<details><summary>Hint</summary>The crocodile blocks your best exit from each chamber; you can always take the second best.</details>
<details><summary>Approach sketch</summary>Run Dijkstra backwards from all exits (super source). For each vertex, the value that matters is its second smallest tentative distance over distinct incoming edges; relax neighbours only when a vertex is popped for the second time. That value is the guaranteed escape time from that chamber.</details>

Practice more: Codeforces problemset, tag `shortest paths`, rating 1600–2200.

## B. Cycles, DAGs, topological order

### 11.16  Building Teams  ·  CSES 1668  ·  ★1
https://cses.fi/problemset/task/1668
**Technique:** bipartite 2-colouring.
<details><summary>Hint</summary>Components are independent; a same-colour edge means IMPOSSIBLE.</details>
<details><summary>Approach sketch</summary>BFS from every uncoloured vertex assigning alternating colours; conflict ⇒ odd cycle. Print colours + 1.</details>

### 11.17  Round Trip  ·  CSES 1669  ·  ★2
https://cses.fi/problemset/task/1669
**Technique:** undirected cycle via DFS with parent tracking.
<details><summary>Hint</summary>An edge to an already-visited vertex that is not the one you came from closes the cycle.</details>
<details><summary>Approach sketch</summary>DFS with `par[]`; on such an edge (u, v) output v, u, par[u], …, v. The graph has no multi-edges here so skipping the parent vertex is safe; in general skip the parent edge id. Iterative DFS or trust the stack (n ≤ 10⁵ is fine on CSES).</details>

### 11.18  Round Trip II  ·  CSES 1678  ·  ★2
https://cses.fi/problemset/task/1678
**Technique:** directed cycle via 3-colour DFS.
<details><summary>Hint</summary>A back edge to a GRAY vertex.</details>
<details><summary>Approach sketch</summary>Colour DFS; on edge u→v with v GRAY the cycle is v … u v via parents. Start from every WHITE vertex.</details>

### 11.19  Course Schedule  ·  CSES 1679  ·  ★2
https://cses.fi/problemset/task/1679
**Technique:** Kahn's algorithm.
<details><summary>Hint</summary>Order shorter than n ⇒ cycle.</details>
<details><summary>Approach sketch</summary>In-degree queue; output the order or IMPOSSIBLE.</details>

### 11.20  Course Schedule II  ·  CSES 1757  ·  ★3
https://cses.fi/problemset/task/1757
**Technique:** topological order with "vertex 1 as early as possible, then 2, …" — reverse-graph max-heap trick.
<details><summary>Hint</summary>The lexicographically smallest order is NOT the answer. Think about which vertex should go *last*.</details>
<details><summary>Approach sketch</summary>Reverse every edge, run Kahn with a max-heap (always take the largest available vertex), then reverse the produced order. Placing the largest possible vertex as late as possible is exactly what makes small vertices as early as possible. Prove it by exchange on the last position.</details>

### 11.21  Longest Flight Route  ·  CSES 1680  ·  ★2
https://cses.fi/problemset/task/1680
**Technique:** DAG DP longest path with reconstruction.
<details><summary>Hint</summary>Unreachable cities must not contribute — initialise them to −INF.</details>
<details><summary>Approach sketch</summary>Topological order; `best[1] = 0`, push `best[u] + 1` forward with parent pointers. Reconstruct from n; IMPOSSIBLE if `best[n]` is −INF.</details>

### 11.22  Game Routes  ·  CSES 1681  ·  ★2
https://cses.fi/problemset/task/1681
**Technique:** DAG DP path counting mod 10⁹+7.
<details><summary>Hint</summary>Same skeleton as the previous task with `+` instead of `max`.</details>
<details><summary>Approach sketch</summary>`ways[1] = 1`, propagate in topological order.</details>

### 11.23  Acyclic Graph Edges  ·  CSES 1756  ·  ★2
https://cses.fi/problemset/task/1756
**Technique:** orientation by any total order.
<details><summary>Hint</summary>Orient every edge from the smaller index to the larger one.</details>
<details><summary>Approach sketch</summary>Any orientation that follows a fixed linear order of the vertices is acyclic (a cycle would need a decreasing edge). Constant-time per edge.</details>

### 11.24  Reachable Nodes  ·  CSES 2138  ·  ★3
https://cses.fi/problemset/task/2138
**Technique:** bitset reachability on a DAG in reverse topological order.
<details><summary>Hint</summary>n ≤ 5·10⁴: 5·10⁴ bitsets of 5·10⁴ bits is 312 MB — process source vertices in blocks of 64·B.</details>
<details><summary>Approach sketch</summary>For a block of B·64 vertices, give each of them a bitset<B·64> with only its own bit; process vertices in reverse topological order doing `reach[u] |= reach[v]`; count bits for the block's vertices. O(n·m/64) total with n/(64B) passes over the edges. Alternatively compute, for each vertex, the OR over children in reverse topo order with bitsets sized by the number of *targets* in the current block.</details>

### 11.25  Graph Girth  ·  CSES 1707  ·  ★3
https://cses.fi/problemset/task/1707
**Technique:** BFS from every vertex, shortest cycle via non-tree edges.
<details><summary>Hint</summary>n ≤ 2500, m ≤ 5000: O(nm) BFS is 1.25·10⁷.</details>
<details><summary>Approach sketch</summary>For each start s BFS; when an edge (u,v) reaches an already visited v that is not u's parent, candidate d[u]+d[v]+1. The minimum over all starts is the girth (the BFS from a vertex on the shortest cycle finds it exactly).</details>

### 11.26  Hamiltonian Flights  ·  CSES 1690  ·  ★3
https://cses.fi/problemset/task/1690
**Technique:** bitmask DP over (visited set, last city).
<details><summary>Hint</summary>City n may only be entered last; multi-edges count separately.</details>
<details><summary>Approach sketch</summary>`dp[1][0] = 1`; for each mask in increasing order and each `v` with `dp[mask][v] > 0`, skip if `v == n−1` and the mask is not full, else push along every out-edge to an unvisited city. Answer `dp[full][n−1]`. Use `int` mod p or a layered array if memory is tight (2²⁰·20 values).</details>

### 11.27  Knight's Tour  ·  CSES 1689  ·  ★3
https://cses.fi/problemset/task/1689
**Technique:** Warnsdorff's heuristic with backtracking.
<details><summary>Hint</summary>Always jump to the unvisited square with the fewest unvisited onward moves.</details>
<details><summary>Approach sketch</summary>Recursive backtracking over 64 squares; at each step sort candidate moves by their onward-move count. With this ordering the first attempt almost always succeeds from any start on 8×8; the backtracking is a safety net.</details>

Practice more: Codeforces problemset, tags `dfs and similar`, `graphs`, rating 1500–2000 (cycles,
topological order, DAG DP).

## C. Eulerian paths

### 11.28  Mail Delivery  ·  CSES 1691  ·  ★3
https://cses.fi/problemset/task/1691
**Technique:** undirected Euler circuit, iterative Hierholzer.
<details><summary>Hint</summary>All degrees even AND all edges reachable from 1; the second condition is checked by the path length.</details>
<details><summary>Approach sketch</summary>Degree check ⇒ IMPOSSIBLE. Run Hierholzer from 1 with a `used[edge]` array and per-vertex pointer. If the produced sequence has fewer than m+1 vertices some edges were in another component ⇒ IMPOSSIBLE. Recursion depth m: iterative.</details>

### 11.29  Teleporters Path  ·  CSES 1693  ·  ★3
https://cses.fi/problemset/task/1693
**Technique:** directed Euler path from 1 to n.
<details><summary>Hint</summary>out(1) = in(1) + 1, in(n) = out(n) + 1, all others balanced; then the same length check.</details>
<details><summary>Approach sketch</summary>Degree conditions; Hierholzer from 1; verify length m+1 and that the path ends at n (it must, given the degrees).</details>

### 11.30  De Bruijn Sequence  ·  CSES 1692  ·  ★3
https://cses.fi/problemset/task/1692
**Technique:** Euler circuit on the shift graph of (n−1)-bit strings.
<details><summary>Hint</summary>Vertices are the 2ⁿ⁻¹ strings of length n−1; string s has edges to s[1:]+'0' and s[1:]+'1'.</details>
<details><summary>Approach sketch</summary>Every vertex has in = out = 2, so an Euler circuit exists. Take vertex 0…0 as the start, output its n−1 bits, then the last bit of each of the 2ⁿ edges traversed. Total length 2ⁿ + n − 1. n = 1 is a special case ("01").</details>

### 11.31  Eulerian Subgraphs  ·  CSES 2078  ·  ★3
https://cses.fi/problemset/task/2078
**Technique:** cycle space dimension.
<details><summary>Hint</summary>Edge subsets with all degrees even form a vector space over GF(2).</details>
<details><summary>Approach sketch</summary>Count components c with DSU; answer 2^(m − n + c) mod p. (Basis: fundamental cycles of a spanning forest, one per non-tree edge.)</details>

### 11.32  Bertown roads  ·  Codeforces 118E  ·  CF 2000
https://codeforces.com/problemset/problem/118/E
**Technique:** bridges + strong orientation (Robbins).
<details><summary>Hint</summary>A bridge can never be oriented both ways; otherwise orient DFS tree edges downward and back edges upward.</details>
<details><summary>Approach sketch</summary>Find bridges; if any, print 0. Otherwise output every edge in the direction it was first traversed by the DFS (tree edge parent→child, back edge descendant→ancestor). Every vertex reaches the root via back edges and the root reaches everything via tree edges.</details>

Practice more: Codeforces problemset, tag `graphs` with "Euler", rating 1800–2300.

## D. Strong connectivity and 2-SAT

### 11.33  Flight Routes Check  ·  CSES 1682  ·  ★2
https://cses.fi/problemset/task/1682
**Technique:** two DFS (forward, reversed) or SCC count.
<details><summary>Hint</summary>Strongly connected ⇔ vertex 1 reaches everything and everything reaches 1.</details>
<details><summary>Approach sketch</summary>DFS from 1 in G and in Gᵀ. If some x is unreached in G print "1 x"; if unreached in Gᵀ print "x 1".</details>

### 11.34  Planets and Kingdoms  ·  CSES 1683  ·  ★2
https://cses.fi/problemset/task/1683
**Technique:** SCC (Tarjan or Kosaraju).
<details><summary>Hint</summary>Print the component id + 1.</details>
<details><summary>Approach sketch</summary>Run the SCC routine; output `comp[v]+1` and the count.</details>

### 11.35  Coin Collector  ·  CSES 1686  ·  ★3
https://cses.fi/problemset/task/1686
**Technique:** SCC condensation + longest path DP.
<details><summary>Hint</summary>Inside an SCC you can take every coin; the condensation is a DAG.</details>
<details><summary>Approach sketch</summary>Component weight = sum of coins. Build condensation edges (deduplicated or not — duplicates do not hurt a max DP). With Tarjan ids (reverse topological), iterate ids from 0 upward pushing `best[c] = w[c] + max over out-neighbours` — since out-neighbours have smaller ids they are already final. Answer = max over components.</details>

### 11.36  Giant Pizza  ·  CSES 1684  ·  ★4
https://cses.fi/problemset/task/1684
**Technique:** 2-SAT.
<details><summary>Hint</summary>Each person's pair of wishes is a clause (a ∨ b): add ¬a→b and ¬b→a.</details>
<details><summary>Approach sketch</summary>2m literal vertices, 2n edges, SCC. IMPOSSIBLE if some topping shares a component with its negation. Otherwise topping true iff its literal's component comes later in topological order (Tarjan: smaller id). Print + / −.</details>

### 11.37  New Flight Routes  ·  CSES 1685  ·  ★5
https://cses.fi/problemset/task/1685
**Technique:** SCC condensation; min edges to make strongly connected = max(sources, sinks), constructive.
<details><summary>Hint</summary>Pair each sink with a source it cannot reach "back"; a DFS from sources that stops at the first unseen sink produces such pairs.</details>
<details><summary>Approach sketch</summary>Condense. If one component, answer 0. Otherwise, for each source component run a DFS in the condensation that marks vertices globally and returns the first sink found that has not been claimed yet; this yields k pairs (sink_i, source_{i+1}) forming a chain that links k sources and k sinks into one cycle. Leftover sources get an edge from any sink and leftover sinks an edge to any source. Total max(#sources, #sinks). Pick any representative vertex per component for the output.</details>

### 11.38  Reachability Queries  ·  CSES 2143  ·  ★4
https://cses.fi/problemset/task/2143
**Technique:** SCC + bitset closure on the condensation (blocked over sources).
<details><summary>Hint</summary>Same as Reachable Nodes but first make the graph a DAG.</details>
<details><summary>Approach sketch</summary>Condense (n ≤ 5·10⁴ components in the worst case). Process the condensation in reverse topological order in blocks of 64·B target components as in 11.24; answer offline queries per block, or store reach bits for the queried targets only.</details>

### 11.39  Strongly Connected Edges  ·  CSES 2177  ·  ★4
https://cses.fi/problemset/task/2177
**Technique:** bridge check + DFS orientation.
<details><summary>Hint</summary>Same as Bertown roads: bridgeless ⇔ strong orientation exists.</details>
<details><summary>Approach sketch</summary>If the graph is disconnected or has a bridge, IMPOSSIBLE. Otherwise orient tree edges downward and back edges upward as traversed by DFS.</details>

### 11.40  Checkposts  ·  Codeforces 427C  ·  CF 1900
https://codeforces.com/problemset/problem/427/C
**Technique:** SCC, min cost per component and number of ways.
<details><summary>Hint</summary>A checkpost in an SCC protects exactly that SCC.</details>
<details><summary>Approach sketch</summary>Per component take the minimum cost and count how many vertices attain it; sum the minima and multiply the counts mod 10⁹+7.</details>

Practice more: Codeforces problemset, tag `2-sat`, rating 1900–2400; tag `graphs` + "strongly
connected", 1800–2300.

## E. Bridges and articulation points

### 11.41  Necessary Roads  ·  CSES 2076  ·  ★3
https://cses.fi/problemset/task/2076
**Technique:** bridges (low-link), multi-edge safe.
<details><summary>Hint</summary>Skip the parent *edge*, not the parent vertex.</details>
<details><summary>Approach sketch</summary>Standard bridge DFS over all components; output the bridges. Test on a graph with a duplicated edge before submitting.</details>

### 11.42  Necessary Cities  ·  CSES 2077  ·  ★3
https://cses.fi/problemset/task/2077
**Technique:** articulation points.
<details><summary>Hint</summary>`low[child] ≥ tin[u]` for non-root u; root: ≥ 2 DFS children.</details>
<details><summary>Approach sketch</summary>Same DFS with the AP rule. Remember the `≥` (vs `>` for bridges) and the root special case.</details>

### 11.43  Network Renovation  ·  CSES 1704  ·  ★4
https://cses.fi/problemset/task/1704
**Technique:** make a tree 2-edge-connected: ⌈leaves/2⌉ edges, pair leaves in DFS order.
<details><summary>Hint</summary>List leaves in DFS visiting order; connect leaf i with leaf i + ⌈L/2⌉.</details>
<details><summary>Approach sketch</summary>Every leaf needs a new edge, so ≥ ⌈L/2⌉. Root the tree at a non-leaf, list leaves in DFS order l₀..l_{L−1}, add edges (lᵢ, l_{i+⌈L/2⌉}) for i < ⌊L/2⌋ and, if L odd, (l_{L−1}, l₀). Pairing leaves that are "opposite" in the DFS order makes every tree edge lie on a cycle. n = 2 is a special case.</details>

### 11.44  Forbidden Cities  ·  CSES 1705  ·  ★5
https://cses.fi/problemset/task/1705
**Technique:** biconnected components, block-cut tree, LCA.
<details><summary>Hint</summary>c separates a and b ⇔ c is an articulation point lying on the block-cut-tree path between (a block of) a and (a block of) b.</details>
<details><summary>Approach sketch</summary>Build the block-cut tree (blocks + articulation points as tree vertices). Map a, b, c to tree vertices (a non-AP vertex maps to its unique block, an AP to itself). The answer is NO iff c is an AP and its tree vertex lies on the path between the mapped a and b — check with LCA and depths; handle a or b equal to c (NO) and a == b (YES).</details>

Practice more: Codeforces problemset, tag `graphs` + "bridges", rating 1900–2400.

## F. Spanning trees

### 11.45  Road Reparation  ·  CSES 1675  ·  ★2
https://cses.fi/problemset/task/1675
**Technique:** Kruskal + DSU.
<details><summary>Hint</summary>Fewer than n−1 accepted edges ⇒ IMPOSSIBLE.</details>
<details><summary>Approach sketch</summary>Sort, unite, sum as `long long`.</details>

### 11.46  Road Construction  ·  CSES 1676  ·  ★2
https://cses.fi/problemset/task/1676
**Technique:** online DSU with component count and max size.
<details><summary>Hint</summary>Both statistics update in O(1) per successful union.</details>
<details><summary>Approach sketch</summary>Union by size; on a real merge decrement the count and update the running maximum.</details>

### 11.47  MST Edge Check  ·  CSES 3407  ·  ★3
https://cses.fi/problemset/task/3407
**Technique:** Kruskal by weight groups, query before uniting the group.
<details><summary>Hint</summary>Edge (a,b,w) is in some MST iff a and b are disconnected using only edges lighter than w.</details>
<details><summary>Approach sketch</summary>Sort edges by weight; for each group of equal weight, first answer for every edge in the group whether `find(a) != find(b)`, then unite all edges of the group. O(m log m).</details>

### 11.48  MST Edge Set Check  ·  CSES 3408  ·  ★4
https://cses.fi/problemset/task/3408
**Technique:** Kruskal by weight groups with a temporary DSU per group (rollback or re-init on touched vertices).
<details><summary>Hint</summary>A set of edges lies in a common MST iff, within every weight group, the chosen edges of that weight form a forest over the components of the lighter edges.</details>
<details><summary>Approach sketch</summary>Process weight groups in order. For each group: on the current DSU (lighter edges united), test the group's *chosen* edges: each must connect two different components and must not close a cycle among themselves — check with a scratch DSU keyed by component ids (reset only the touched representatives). Then unite all edges of the group. Any failure ⇒ NO.</details>

### 11.49  MST Edge Cost  ·  CSES 3409  ·  ★4
https://cses.fi/problemset/task/3409
**Technique:** MST + max edge on tree path (binary lifting).
<details><summary>Hint</summary>Cheapest spanning tree containing e = MST − heaviest edge on the MST path between its endpoints + w(e). For tree edges it is just MST.</details>
<details><summary>Approach sketch</summary>Kruskal, root the MST, binary lifting storing the max edge weight per jump; answer each edge in O(log n).</details>

### 11.50  Transfer Speeds Sum  ·  CSES 3111  ·  ★3
https://cses.fi/problemset/task/3111
**Technique:** maximum spanning tree Kruskal with size products (Kruskal reconstruction tree idea).
<details><summary>Hint</summary>When an edge of weight w merges components of sizes p and q, it is the bottleneck for exactly p·q pairs.</details>
<details><summary>Approach sketch</summary>Sort edges descending; on each successful union add `w · size[a] · size[b]`. The pair (u,v)'s max-min path value is the weight of the edge that first connected them in this order. `long long`.</details>

### 11.51  New Roads Queries  ·  CSES 2101  ·  ★4
https://cses.fi/problemset/task/2101
**Technique:** Kruskal reconstruction tree with edge index as weight, or DSU with merge timestamps and no path compression.
<details><summary>Hint</summary>"First day a and b are connected" = weight of their LCA in the reconstruction tree built with day numbers as weights.</details>
<details><summary>Approach sketch</summary>Process roads in input order as Kruskal with weight = day; build the reconstruction tree; binary-lifting LCA; answer w[LCA] or −1 if different trees. Alternative: union by size without compression, store the union day on the parent pointer; the answer is the max day on the path from a and from b to their meeting point (walk up in O(log n)).</details>

### 11.52  Network Breakdown  ·  CSES 1677  ·  ★3
https://cses.fi/problemset/task/1677
**Technique:** offline reverse DSU.
<details><summary>Hint</summary>Deleting edges is hard; adding them backwards is Road Construction.</details>
<details><summary>Approach sketch</summary>Mark edges that get deleted; union all never-deleted edges; process deletions in reverse, uniting and recording the component count before each; output reversed.</details>

### 11.53  MST Unification  ·  Codeforces 1108F  ·  CF 2100
https://codeforces.com/problemset/problem/1108/F
**Technique:** MST uniqueness by weight groups.
<details><summary>Hint</summary>Within a weight group, count edges that *could* be added (endpoints in different components before the group is processed) minus the number actually added.</details>
<details><summary>Approach sketch</summary>Kruskal by groups; for each group first count candidates with `find(a) != find(b)`, then unite and count successes; the difference summed over groups is the number of edges whose weight must be increased.</details>

### 11.54  Minimum spanning tree for each edge  ·  Codeforces 609E  ·  CF 2100
https://codeforces.com/problemset/problem/609/E
**Technique:** identical to MST Edge Cost (11.49).
<details><summary>Hint</summary>Same formula; `long long` answers.</details>
<details><summary>Approach sketch</summary>See 11.49.</details>

### 11.55  Path Queries  ·  Codeforces 1213G  ·  CF 1900
https://codeforces.com/problemset/problem/1213/G
**Technique:** offline Kruskal with size products (as 11.50) and sorted queries.
<details><summary>Hint</summary>Sort edges and queries by weight; maintain the number of connected pairs.</details>
<details><summary>Approach sketch</summary>Two-pointer over sorted edges and sorted queries; on each union add size[a]·size[b] to a running total; answer each query with the total at that point.</details>

Practice more: Codeforces problemset, tag `dsu` + `graphs`, rating 1700–2300 (MST variants).

## G. Functional graphs

### 11.56  Planets Queries I  ·  CSES 1750  ·  ★2
https://cses.fi/problemset/task/1750
**Technique:** binary lifting on a successor function.
<details><summary>Hint</summary>k ≤ 10⁹ ⇒ 30 levels.</details>
<details><summary>Approach sketch</summary>`up[j][v]`; decompose k in binary. 30 × 2·10⁵ ints fits.</details>

### 11.57  Planets Cycles  ·  CSES 1751  ·  ★3
https://cses.fi/problemset/task/1751
**Technique:** cycle decomposition of a functional graph.
<details><summary>Hint</summary>Three states: unvisited, on current path, finished.</details>
<details><summary>Approach sketch</summary>Walk from each unvisited vertex; on returning to the current path, assign the cycle length to the cycle vertices, then unwind the tail with `ans[v] = ans[next] + 1`; on hitting a finished vertex just unwind. Iterative, O(n).</details>

### 11.58  Planets Queries II  ·  CSES 1160  ·  ★4
https://cses.fi/problemset/task/1160
**Technique:** functional graph structure: cycle position, depth to cycle, binary lifting for ancestor checks.
<details><summary>Hint</summary>Three cases: both on the same cycle; b an ancestor of a in the same in-tree; a in a tree whose root lies on b's cycle.</details>
<details><summary>Approach sketch</summary>Decompose into cycles (position index, cycle id, length) and trees (depth, entry vertex on the cycle). Query (a,b): if same cycle → modular difference. If b in a tree: need `depth[a] ≥ depth[b]` and `kth(a, depth[a]−depth[b]) == b`. If b on a cycle and a in a tree hanging off that cycle: `depth[a]` + cyclic distance from entry(a) to b. Otherwise −1.</details>

Practice more: Codeforces problemset, tag `graphs` + "functional", rating 1600–2100; AtCoder
ABC problems on "doubling" (binary lifting).

## H. Extras (mixed technique, harder)

### 11.59  Visiting Cities  ·  CSES 1203  ·  ★4
https://cses.fi/problemset/task/1203
**Technique:** shortest-path DAG + "vertices on every shortest path".
<details><summary>Hint</summary>Count shortest paths from 1 and to n (mod a random large prime); v is on every shortest path iff d1[v] + dn[v] = D and cnt1[v]·cntn[v] = total.</details>
<details><summary>Approach sketch</summary>Dijkstra from 1 and from n (reversed graph). Path counts via DP on the tight-edge DAG; hashing with two random moduli or a 64-bit modulus avoids collisions. Alternatively: dominators on the shortest-path DAG via the "levels" trick — sort tight-DAG vertices by d1 and find levels crossed by exactly one vertex.</details>

### 11.60  Critical Cities  ·  CSES 1703  ·  ★5
https://cses.fi/problemset/task/1703
**Technique:** dominators (Lengauer–Tarjan) or a path-skipping argument.
<details><summary>Hint</summary>Take one path P from 1 to n; a critical city is on P. A vertex of P is *not* critical if some edge from before it (or from a vertex reachable while avoiding P) lands after it.</details>
<details><summary>Approach sketch</summary>Find any path P = p₀..p_k. BFS from p₀ over vertices not on P, recording for each reached off-path vertex the smallest P-index it can hit; for each P-vertex compute the furthest P-index reachable by one hop (directly or via off-path vertices). Sweep i along P keeping `maxReach`; p_i is critical iff no j < i reaches beyond i. Equivalent to computing the dominator tree of n restricted to P; the general tool is Lengauer–Tarjan.</details>

### 11.61  Flight Route Requests  ·  CSES 1699  ·  ★5
https://cses.fi/problemset/task/1699
**Technique:** SCC + transitive reduction of the condensation DAG.
<details><summary>Hint</summary>Inside each SCC of the request graph a cycle of k edges is optimal; between SCCs you need exactly the edges of the transitive reduction of the condensation.</details>
<details><summary>Approach sketch</summary>Condense the request graph. Each component of size k > 1 costs k edges (a Hamiltonian cycle). The condensation is a DAG; its transitive reduction is unique and its edge count is the number of inter-component edges needed. Compute it by processing vertices in topological order with bitsets of reachable descendants (blocked as in 11.24 if memory forces it), keeping an edge u→v only if v is not reachable from another out-neighbour of u.</details>

### 11.62  The Shortest Statement  ·  Codeforces 1051F  ·  CF 2400
https://codeforces.com/problemset/problem/1051/F
**Technique:** m − n ≤ 20 ⇒ spanning tree + Dijkstra from ≤ 42 special vertices.
<details><summary>Hint</summary>Any shortest path either stays in the spanning tree or passes through an endpoint of a non-tree edge.</details>
<details><summary>Approach sketch</summary>Take a spanning tree; the ≤ 21 non-tree edges have ≤ 42 endpoints. Dijkstra from each. Query (u,v) = min(tree distance via LCA, min over special x of d_x[u] + d_x[v]). O(42·(n+m) log + q·42).</details>

### 11.63  School Excursion  ·  CSES 1706  ·  ★4
https://cses.fi/problemset/task/1706
**Technique:** components (DSU) + subset-sum bitset.
<details><summary>Hint</summary>Component sizes sum to n, so there are O(√n) distinct sizes.</details>
<details><summary>Approach sketch</summary>Collect component sizes; subset-sum over them with a `bitset<n+1>`; group equal sizes and use binary splitting (1,2,4,… copies) so the bitset work is O(n√n/64).</details>

Practice more: Codeforces problemset, tag `graphs`, rating 2100–2500 for combined-technique
problems; BOI/CEOI graph tasks on oj.uz (filter "graphs").

## Progress

- [ ] 11.1 Message Route
- [ ] 11.2 Labyrinth
- [ ] 11.3 Monsters
- [ ] 11.4 Shortest Routes I
- [ ] 11.5 Shortest Routes II
- [ ] 11.6 Flight Discount
- [ ] 11.7 Flight Routes
- [ ] 11.8 High Score
- [ ] 11.9 Cycle Finding
- [ ] 11.10 Investigation
- [ ] 11.11 Nearest Shops
- [ ] 11.12 CF 20C Dijkstra?
- [ ] 11.13 CF 449B Jzzhu and Cities
- [ ] 11.14 CF 295B Greg and Graph
- [ ] 11.15 IOI 2011 Crocodile
- [ ] 11.16 Building Teams
- [ ] 11.17 Round Trip
- [ ] 11.18 Round Trip II
- [ ] 11.19 Course Schedule
- [ ] 11.20 Course Schedule II
- [ ] 11.21 Longest Flight Route
- [ ] 11.22 Game Routes
- [ ] 11.23 Acyclic Graph Edges
- [ ] 11.24 Reachable Nodes
- [ ] 11.25 Graph Girth
- [ ] 11.26 Hamiltonian Flights
- [ ] 11.27 Knight's Tour
- [ ] 11.28 Mail Delivery
- [ ] 11.29 Teleporters Path
- [ ] 11.30 De Bruijn Sequence
- [ ] 11.31 Eulerian Subgraphs
- [ ] 11.32 CF 118E Bertown roads
- [ ] 11.33 Flight Routes Check
- [ ] 11.34 Planets and Kingdoms
- [ ] 11.35 Coin Collector
- [ ] 11.36 Giant Pizza
- [ ] 11.37 New Flight Routes
- [ ] 11.38 Reachability Queries
- [ ] 11.39 Strongly Connected Edges
- [ ] 11.40 CF 427C Checkposts
- [ ] 11.41 Necessary Roads
- [ ] 11.42 Necessary Cities
- [ ] 11.43 Network Renovation
- [ ] 11.44 Forbidden Cities
- [ ] 11.45 Road Reparation
- [ ] 11.46 Road Construction
- [ ] 11.47 MST Edge Check
- [ ] 11.48 MST Edge Set Check
- [ ] 11.49 MST Edge Cost
- [ ] 11.50 Transfer Speeds Sum
- [ ] 11.51 New Roads Queries
- [ ] 11.52 Network Breakdown
- [ ] 11.53 CF 1108F MST Unification
- [ ] 11.54 CF 609E Minimum spanning tree for each edge
- [ ] 11.55 CF 1213G Path Queries
- [ ] 11.56 Planets Queries I
- [ ] 11.57 Planets Cycles
- [ ] 11.58 Planets Queries II
- [ ] 11.59 Visiting Cities
- [ ] 11.60 Critical Cities
- [ ] 11.61 Flight Route Requests
- [ ] 11.62 CF 1051F The Shortest Statement
- [ ] 11.63 School Excursion
