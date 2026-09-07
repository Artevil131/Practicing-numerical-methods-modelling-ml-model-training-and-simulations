# Chapter 12 — Problems

How to work this ladder:

1. Before each problem, re-type the algorithm it needs (Dinic / Hopcroft–Karp / MCMF) from memory
   into a fresh file; compile; only then open the statement.
2. Solutions go to `learning/competitive_programming/12_flows_and_matching/solutions/<source>_<id>.cpp`.
3. Draw the network on paper first: what is a vertex, an edge, a unit of flow, a cut. Write the
   count of vertices you will allocate. Then code.
4. Stress-test: for max flow, brute force = min over all vertex subsets (n ≤ 8) of cut capacity;
   for matching, enumerate; for assignment, permutations. `example.cpp` has all three harnesses.
5. Time budget: ★2 < 20 min, ★3 < 45 min, ★4 up to 90 min. If the *model* is not clear after 20
   minutes, read the hint — modeling is the skill being trained here, not typing.

CSES has relatively few flow tasks (the four in Graph Algorithms plus five in Advanced
Techniques / Additional Problems). They are all here; the ladder is completed with classic,
well-known external problems (AtCoder Library Practice Contest, SPOJ, Kattis, POJ classics, a few
Codeforces) that are safe to cite. For everything else use the "Practice more" lines.

## A. Maximum flow and minimum cut

### 12.1  Download Speed  ·  CSES 1694  ·  ★2
https://cses.fi/problemset/task/1694
**Technique:** plain max flow (Dinic or Edmonds–Karp), `long long`.
<details><summary>Hint</summary>n ≤ 500, m ≤ 1000, capacities up to 10⁹: the answer needs 64 bits; any polynomial algorithm passes.</details>
<details><summary>Approach sketch</summary>Directed edges with the given capacities (parallel edges are fine as separate edges). Max flow 1→n. This is your Dinic correctness test; also submit an Edmonds–Karp once to see both pass.</details>

### 12.2  Police Chase  ·  CSES 1695  ·  ★3
https://cses.fi/problemset/task/1695
**Technique:** min cut recovery from the residual graph.
<details><summary>Hint</summary>Undirected streets: capacity 1 in both directions. After max flow, the vertices reachable from 1 in the residual graph form one side of a minimum cut.</details>
<details><summary>Approach sketch</summary>Add each street as an undirected unit edge (`add(a,b,1,1)`). Run Dinic 1→n; the flow value is the number of streets to close. BFS from 1 over edges with residual > 0; print every input edge with exactly one endpoint reached (orient the output as given).</details>

### 12.3  Distinct Routes  ·  CSES 1711  ·  ★3
https://cses.fi/problemset/task/1711
**Technique:** edge-disjoint paths = unit-capacity max flow; flow decomposition to print them.
<details><summary>Hint</summary>Each directed edge capacity 1. To print paths, walk from 1 along edges carrying flow 1, marking them used; each walk ends at n.</details>
<details><summary>Approach sketch</summary>Max flow with unit capacities gives k. Then repeat k times: from vertex 1 follow an unused forward edge with flow 1 (flow = reverse residual), mark it, continue until n. Flow cycles cannot occur on a path that reaches n unless you revisit a vertex; if a vertex repeats, cut the loop out of the printed path (or cancel cycles first). Print length as the number of vertices.</details>

### 12.4  Maxflow  ·  AtCoder Library Practice Contest D  ·  ★3
https://atcoder.jp/contests/practice2/tasks/practice2_d
**Technique:** bipartite matching on a grid (chessboard colouring) + print the matching.
<details><summary>Hint</summary>Dominoes cover one black and one white cell; adjacent cells always have different colours.</details>
<details><summary>Approach sketch</summary>Left = free black cells, right = free white cells, edge for adjacency. Max matching = max dominoes. Read the matched pairs and paint the grid with `<>` or `v^`. Good exercise in mapping cells to vertex ids and back.</details>

### 12.5  Fast Maximum Flow  ·  SPOJ FASTFLOW  ·  ★3
https://www.spoj.com/problems/FASTFLOW/
**Technique:** Dinic performance check.
<details><summary>Hint</summary>n ≤ 5000, m ≤ 30000, undirected, capacities up to 10⁹: Edmonds–Karp will not pass; Dinic will.</details>
<details><summary>Approach sketch</summary>Undirected edges with equal reverse capacity; `long long`; skip self-loops. If your Dinic times out here, check that `it[]` is a reference and that you are not clearing vectors inside the DFS.</details>

### 12.6  Drainage Ditches  ·  POJ 1273 (classic)  ·  ★2
**Technique:** plain max flow, tiny input; parallel edges must be summed or kept separate.
<details><summary>Hint</summary>Classic first flow problem; multiple edges between the same pair are allowed.</details>
<details><summary>Approach sketch</summary>Read edges as given, add each separately, max flow 1→m. Serves as a warm-up if you have never submitted a flow anywhere.</details>

### 12.7  Sabotage  ·  UVa 10480 (classic)  ·  ★3
**Technique:** min cut with output of cut edges.
<details><summary>Hint</summary>Same as Police Chase with capacities; print every edge from the reachable side to the unreachable side.</details>
<details><summary>Approach sketch</summary>Max flow between cities 1 and 2 on the undirected capacitated graph; residual BFS from 1; output crossing edges in either orientation as the judge accepts.</details>

Practice more: Codeforces problemset, tag `flows`, rating 1900–2300 (pure max flow / min cut
modeling); Kattis `maxflow`, `mincut`.

## B. Bipartite matching, König, path cover

### 12.8  School Dance  ·  CSES 1696  ·  ★3
https://cses.fi/problemset/task/1696
**Technique:** maximum bipartite matching (Hopcroft–Karp or Kuhn), print the pairs.
<details><summary>Hint</summary>n, m ≤ 500, k ≤ 1000: even Kuhn passes; use it to test your Hopcroft–Karp against it.</details>
<details><summary>Approach sketch</summary>Boys left, girls right. Run the matching; print `matchL[b]` for each matched boy. Also solve it once with Dinic on the unit network to see it is the same thing.</details>

### 12.9  Coin Grid  ·  CSES 1709  ·  ★4
https://cses.fi/problemset/task/1709
**Technique:** König — minimum vertex cover in the rows × columns bipartite graph, with the cover recovered.
<details><summary>Hint</summary>A coin at (r,c) is an edge between row r and column c; removing rows/columns = choosing vertices covering all edges.</details>
<details><summary>Approach sketch</summary>Max matching between rows and columns. Then alternating DFS from unmatched rows: visited rows are *not* in the cover, visited columns *are*. Output rows not visited and columns visited; the count equals the matching size. Check on a 2×2 example by hand before trusting the sides.</details>

### 12.10  Asteroids  ·  POJ 3041 (classic)  ·  ★3
**Technique:** minimum vertex cover of rows × columns = matching size (no reconstruction).
<details><summary>Hint</summary>Coin Grid without printing the cover.</details>
<details><summary>Approach sketch</summary>Edge per asteroid (row, column); answer = maximum matching by König.</details>

### 12.11  SAM I AM  ·  UVa 11419 (classic)  ·  ★4
**Technique:** König with cover reconstruction (rows and columns to shoot).
<details><summary>Hint</summary>Exactly Coin Grid with a different story.</details>
<details><summary>Approach sketch</summary>Same as 12.9; useful as a second implementation of the alternating-path cover to make sure you can reproduce it.</details>

### 12.12  Array and Operations  ·  Codeforces 498C  ·  CF 2100
https://codeforces.com/problemset/problem/498/C
**Technique:** matching per prime factor between odd-index and even-index elements (the given pairs always join opposite parities).
<details><summary>Hint</summary>Each operation divides one odd-indexed and one even-indexed element by a common prime. Primes are independent.</details>
<details><summary>Approach sketch</summary>For each prime p dividing some element, build a flow network: source → odd-index element i with capacity = exponent of p in aᵢ, i → j (for allowed pairs) infinite, even-index j → sink with capacity = exponent. Sum the max flows over primes. n ≤ 100 so anything works; the modeling is the point.</details>

### 12.13  Minimum path cover in a DAG  ·  (technique drill, no fixed judge)  ·  ★3
**Technique:** n − max matching on the split graph.
<details><summary>Hint</summary>Write the function `minPathCover` from `example.cpp` blind and verify it against the brute force there.</details>
<details><summary>Approach sketch</summary>Left copy = "out" of each vertex, right copy = "in"; edge u→v as left u → right v. Paths = n − |M|. For the version where paths may share vertices, take the transitive closure first (bitsets) — this is Dilworth: the answer equals the maximum antichain.</details>

Practice more: Codeforces problemset, tag `graph matchings`, rating 1800–2300; Kattis
`bipartitematching`, `maxflow`; SPOJ MATCHING (Hopcroft–Karp speed test).

## C. Min cut modeling

### 12.14  Petya and Graph  ·  Codeforces 1082G  ·  CF 2400
https://codeforces.com/problemset/problem/1082/G
**Technique:** project selection / closure: edges are profitable projects that require both endpoint vertices (costs).
<details><summary>Hint</summary>Taking an edge yields its weight but forces you to pay for both endpoints. Sum of edge weights minus a min cut.</details>
<details><summary>Approach sketch</summary>s → edge-node with capacity w(e); edge-node → each endpoint with INF; vertex → t with capacity a(v). Answer = Σ w(e) − maxflow. The S side of the cut is the chosen set.</details>

### 12.15  Project selection drill  ·  (technique drill)  ·  ★3
**Technique:** maximum-weight closure.
<details><summary>Hint</summary>Re-derive the network from the profit signs; verify with `projectSelection` in `example.cpp` against subset enumeration.</details>
<details><summary>Approach sketch</summary>Positive profit ⇒ s→i with p; negative ⇒ i→t with −p; requirement i⇒j as i→j INF. Chosen = reachable side. Be able to explain why every finite cut corresponds to a closed set.</details>

Practice more: Codeforces problemset, tag `flows`, rating 2300–2600 (closure, image
segmentation, "two teams with penalties"). Kleinberg–Tardos chapter 7 exercises for models.

## D. Min cost flow and assignment

### 12.16  Task Assignment  ·  CSES 2129  ·  ★4
https://cses.fi/problemset/task/2129
**Technique:** assignment problem — Hungarian or MCMF, and print the assignment.
<details><summary>Hint</summary>n ≤ 200: MCMF on the complete bipartite graph (F = n, E = n²) or Hungarian both pass; do both and compare.</details>
<details><summary>Approach sketch</summary>Hungarian on the n×n matrix returns the cost and `assign[row]`. For MCMF: s→worker (1, 0), worker→task (1, cost), task→t (1, 0); read matched edges by residual flow.</details>

### 12.17  Distinct Routes II  ·  CSES 2130  ·  ★4
https://cses.fi/problemset/task/2130
**Technique:** MCMF with flow limit k: capacity 1, cost 1 per edge (edge count as length), print the paths.
<details><summary>Hint</summary>Successive shortest paths gives the minimum total cost for exactly k units; if the k-th augmentation fails, IMPOSSIBLE.</details>
<details><summary>Approach sketch</summary>Unit capacities, unit costs (or given lengths), augment k times from 1 to n; the residual graph may contain reversed flow that cancels — decomposition as in Distinct Routes still works since final flow on each edge is 0 or 1. Print the k paths.</details>

### 12.18  Parcel Delivery  ·  CSES 2121  ·  ★4
https://cses.fi/problemset/task/2121
**Technique:** min cost flow with capacities and costs, exactly k units.
<details><summary>Hint</summary>Capacities up to given limits, cost per parcel per road; augment until k parcels are sent or no path remains.</details>
<details><summary>Approach sketch</summary>Directly MCMF 1→n with flow limit k; answer the cost, or −1 if the maximum flow is below k. With k ≤ 500ish and n, m small, SPFA-based SSP is enough; try the Dijkstra+potentials variant too.</details>

### 12.19  MinCostFlow  ·  AtCoder Library Practice Contest E  ·  ★4
https://atcoder.jp/contests/practice2/tasks/practice2_e
**Technique:** MCMF modeling with per-row/per-column limits and "skip" edges.
<details><summary>Hint</summary>Maximise sum ⇒ negate values (or use `BIG − a`); each row and each column may be used at most k times; a free zero-cost bypass edge from the row hub to the column hub lets you not fill.</details>
<details><summary>Approach sketch</summary>s → rowᵢ (k, 0), rowᵢ → colⱼ (1, −aᵢⱼ), colⱼ → t (k, 0), plus s→t (n·k, 0) bypass so that only profitable cells are taken. Stop when the shortest path cost becomes non-negative (or use the bypass). Print the chosen cells from residual flows.</details>

### 12.20  Going Home  ·  POJ 2195 (classic)  ·  ★3
**Technique:** min cost perfect bipartite matching (men ↔ houses, Manhattan distance).
<details><summary>Hint</summary>Complete bipartite graph, unit capacities, cost = distance.</details>
<details><summary>Approach sketch</summary>MCMF or Hungarian; n ≤ 100 per side.</details>

### 12.21  Farm Tour  ·  POJ 2135 (classic)  ·  ★3
**Technique:** two edge-disjoint paths of minimum total length = MCMF with flow 2.
<details><summary>Hint</summary>Going 1→n and back without reusing a road = two edge-disjoint 1→n paths (undirected ⇒ both directions capacity 1).</details>
<details><summary>Approach sketch</summary>Each undirected road as two directed unit edges with cost = length; send exactly 2 units 1→n at minimum cost.</details>

### 12.22  Trash  ·  Timus 1076 (classic)  ·  ★3
https://acm.timus.ru/problem.aspx?space=1&num=1076
**Technique:** assignment problem via Hungarian O(n³).
<details><summary>Hint</summary>Cost to assign basket i to colour j = total in basket i minus what is already colour j.</details>
<details><summary>Approach sketch</summary>Build the n×n matrix, run Hungarian. A clean test of your Hungarian against MCMF.</details>

Practice more: Codeforces problemset, tag `flows` with "min cost", rating 2200–2500; Kattis
`mincostmaxflow`; AtCoder ABC/ARC problems tagged "min cost flow".

## E. Vertex splitting, lower bounds, harder modeling

### 12.23  Dining  ·  POJ 3281 (classic)  ·  ★3
**Technique:** vertex splitting so each cow is used once between two resource classes.
<details><summary>Hint</summary>Food → cow_in → cow_out → drink: the cow edge has capacity 1.</details>
<details><summary>Approach sketch</summary>s → food (1), food → cow_in, cow_in → cow_out (1), cow_out → drink, drink → t (1). Max flow = number of satisfied cows. The canonical example of "a vertex with capacity between two bipartitions".</details>

### 12.24  Vertex-disjoint paths drill  ·  (technique drill)  ·  ★3
**Technique:** vertex splitting; Menger's vertex version.
<details><summary>Hint</summary>Re-type `vertexDisjointPaths` blind; verify against the separator brute force in `example.cpp`.</details>
<details><summary>Approach sketch</summary>v_in → v_out with capacity 1 (INF for s, t); original edge u→v as u_out → v_in. Max flow = max vertex-disjoint paths = min vertex separator (when no direct s–t edge).</details>

### 12.25  Lower bounds drill  ·  (technique drill)  ·  ★4
**Technique:** feasible circulation with [lo, hi] bounds.
<details><summary>Hint</summary>Reduce capacities by lo, track excess per vertex, super source/sink, check saturation. Verify with the brute force in `example.cpp`.</details>
<details><summary>Approach sketch</summary>See lesson §7. Then extend: max s-t flow with lower bounds = feasibility with a t→s INF edge, followed by continued augmentation s→t on the same residual graph.</details>

### 12.26  Software Allocation  ·  UVa 259 (classic)  ·  ★3
**Technique:** applications → computers with capacities, plus assignment output.
<details><summary>Hint</summary>Each application letter appears k times (k units), each computer takes one unit.</details>
<details><summary>Approach sketch</summary>s → app (count), app → allowed computer (1), computer → t (1). If max flow equals total requests print the assignment from residual flows, else `!`.</details>

Practice more: Codeforces problemset, tag `flows`, rating 2100–2500 (splitting, lower bounds,
"each at least once"); BOI/CEOI tasks tagged flows on oj.uz.

## Progress

- [ ] 12.1 Download Speed
- [ ] 12.2 Police Chase
- [ ] 12.3 Distinct Routes
- [ ] 12.4 ACL Practice D Maxflow
- [ ] 12.5 SPOJ FASTFLOW
- [ ] 12.6 POJ 1273 Drainage Ditches
- [ ] 12.7 UVa 10480 Sabotage
- [ ] 12.8 School Dance
- [ ] 12.9 Coin Grid
- [ ] 12.10 POJ 3041 Asteroids
- [ ] 12.11 UVa 11419 SAM I AM
- [ ] 12.12 CF 498C Array and Operations
- [ ] 12.13 Minimum path cover drill
- [ ] 12.14 CF 1082G Petya and Graph
- [ ] 12.15 Project selection drill
- [ ] 12.16 Task Assignment
- [ ] 12.17 Distinct Routes II
- [ ] 12.18 Parcel Delivery
- [ ] 12.19 ACL Practice E MinCostFlow
- [ ] 12.20 POJ 2195 Going Home
- [ ] 12.21 POJ 2135 Farm Tour
- [ ] 12.22 Timus 1076 Trash
- [ ] 12.23 POJ 3281 Dining
- [ ] 12.24 Vertex-disjoint paths drill
- [ ] 12.25 Lower bounds drill
- [ ] 12.26 UVa 259 Software Allocation
