# Chapter 06 — Graphs

## What you'll be able to do after this chapter

- Treat a 2-D grid as an implicit graph and run DFS / flood fill on it to mark, count or transform connected regions — with the three checks (bounds, visited, target) in the right place.
- Run BFS for shortest paths in unweighted graphs, including multi-source BFS and BFS over an abstract state space (strings, bitmasks, `(node, extra)` pairs), and explain *why* the first discovery of a node is its final distance.
- Build an adjacency list in C (CSR layout or `malloc`'d per-node arrays), count connected components with the outer-loop-plus-search formula, and recognise reachability / grouping / 2-colouring problems as the same formula in disguise.
- Detect cycles in a directed graph and produce a topological order two ways (Kahn's in-degree BFS, three-colour DFS post-order), and state exactly when a plain `visited` flag is not enough.
- Implement union-find with path compression and union by size, and recognise the "edges arrive in a stream / count components as you go / Kruskal" signal.
- Implement Dijkstra with a binary min-heap and lazy deletion, adapt the relaxation rule (sum, max, product, count-of-paths), and know when to use 0-1 BFS or bounded Bellman-Ford instead.

## Why this matters for ML / numerics / sims

Almost every "spatial" or "dependency" computation in numerics is a graph algorithm wearing a domain costume. A mesh's connected components (is this triangle soup one body or three?) is flood fill / union-find. Pathfinding for agents in a simulation on a grid is BFS (unit cost) or Dijkstra (terrain cost). Segmenting a binary image into blobs is Number of Islands. Computing a distance transform (distance from every pixel to the nearest foreground pixel) is multi-source BFS — the 01 Matrix problem, verbatim. An autograd engine executes a computation graph in **topological order** and checks it for cycles; PyTorch's backward pass is a reverse topological traversal. Build systems, dependency resolvers and scheduler DAGs are Kahn's algorithm. Kruskal's MST over Euclidean distances is single-linkage clustering. Dijkstra with a "max edge on path" relaxation (Path With Minimum Effort) is a minimax path — the same thing as the bottleneck criterion in percolation simulations. Learning these in C means you control the memory layout: a CSR adjacency array is contiguous and cache-friendly, which is why sparse-matrix libraries and GNN frameworks store graphs exactly that way.

---

## 0. Representing a graph in C

Before the patterns, the plumbing. You have no `dict[int, list[int]]`; you build one of these.

| Representation | Memory | Neighbours of `u` | Edge lookup `(u,v)`? | Use when |
|---|---|---|---|---|
| Implicit grid | none | 4 or 8 coordinate offsets | O(1) | input *is* a grid |
| Adjacency matrix `int adj[n][n]` | O(n²) | O(n) scan of a row | O(1) | n ≤ ~2000, dense, or input is already a matrix |
| Adjacency list, CSR | O(n + m) | O(deg u), contiguous | O(deg u) | default for sparse graphs |
| Adjacency list, per-node dynamic arrays | O(n + m) + n allocations | O(deg u) | O(deg u) | edges arrive incrementally |
| Edge list `(u, v, w)[m]` | O(m) | no | no | Kruskal, Bellman-Ford |

**CSR (compressed sparse row)** — the layout you want in C. Two arrays: `off[n+1]` and `adj[m]` (`2m` for undirected, since each edge is stored twice). The neighbours of `u` are `adj[off[u] .. off[u+1])`, a half-open range exactly like Chapter 04's array slices.

```c
/* Build CSR from an undirected edge list. Two passes: count degrees, then fill. */
typedef struct { int n, m; int *off, *adj; } Graph;

Graph graph_from_edges(int n, int m, const int (*edges)[2]) {
    Graph g = { n, m, calloc(n + 1, sizeof *g.off), malloc(2 * (size_t)m * sizeof *g.adj) };
    for (int i = 0; i < m; i++) { g.off[edges[i][0] + 1]++; g.off[edges[i][1] + 1]++; }
    for (int u = 0; u < n; u++) g.off[u + 1] += g.off[u];        /* prefix sum → row starts */
    int *fill = malloc((n + 1) * sizeof *fill);                    /* per-row write cursor */
    memcpy(fill, g.off, (n + 1) * sizeof *fill);
    for (int i = 0; i < m; i++) {
        int u = edges[i][0], v = edges[i][1];
        g.adj[fill[u]++] = v;
        g.adj[fill[v]++] = u;
    }
    free(fill);
    return g;
}
/* iterate: for (int k = g.off[u]; k < g.off[u+1]; k++) { int v = g.adj[k]; ... } */
```

Trace for `n=4`, edges `(0,1) (0,2) (1,2) (2,3)`:

```
degrees           : [2, 2, 3, 1]
off (prefix sums) : [0, 2, 4, 7, 8]
adj               : [1 2 | 0 2 | 0 1 3 | 2]
                     u=0   u=1   u=2     u=3
```

Weighted graphs: add a parallel `int *w` (or `double *w`) array indexed the same as `adj`. The prefix-sum step is Chapter 01's prefix sums used as a bucket-offset computation — the same trick as counting sort.

**Python equivalent:** `adj = defaultdict(list); adj[u].append(v)`. CSR is what `scipy.sparse.csr_matrix` stores.

---

## 1. Grid DFS and flood fill

### The idea

A grid is a graph in disguise: each cell is a node and its four (sometimes eight) neighbouring cells are its edges. You never build the graph — neighbours come straight from the coordinates `(y±1, x)` and `(y, x±1)`. DFS is the right tool when the task asks you to **mark, count or transform** a connected region: fill an area, measure its size, or count how many separate areas there are.

### The three checks, every step

Before you step into a neighbour cell, check — in any order, but all three:

1. **In bounds:** `0 <= y < n && 0 <= x < m`.
2. **Not already visited.**
3. **Belongs to the target region:** e.g. `grid[y][x] == '1'`, not `'0'`.

Forgetting any one of these is, in practice, the cause of nearly every grid-DFS bug: out-of-bounds reads (undefined behaviour in C — no `IndexError` to save you), infinite recursion, or bleeding into the wrong region.

### When DFS rather than BFS

When the problem does **not** ask for a distance or a shortest path — only "is it connected" or "how big is the region". The order in which cells are visited does not matter, so a stack (recursion) works as well as a queue, and recursion is usually shorter to write. As soon as the word *shortest* or *minimum steps* appears, switch to Section 2.

### Timing of the visited mark

Mark a cell visited **the moment it is judged valid**, not after the recursive call returns. Otherwise the same cell can be pushed several times from several neighbours before the first processing marks it. An alternative to a separate `visited` array is to mark the cell directly in the grid (e.g. `'1'` → `'0'`, or write the new colour): saves memory but mutates the input — fine on LeetCode, think twice in library code.

### C layout

- Grid as a flat `char *grid` or `int *grid` of `n*m` with index `y*m + x` (row-major, like NumPy's default). 2-D `grid[y][x]` also works when the dimensions are compile-time known or you use VLAs / pointer-to-row arrays (LeetCode's `char** grid`).
- Visited: `unsigned char *vis = calloc(n*m, 1)` — or reuse the grid.
- Direction table: `static const int DY[4] = {-1, 1, 0, 0}, DX[4] = {0, 0, -1, 1};` — loop `for d in 0..4` instead of writing four calls. For 8 directions, add the diagonals.

### Skeleton

```c
static const int DY[4] = {-1, 1, 0, 0}, DX[4] = {0, 0, -1, 1};

/* Marks the whole component containing (y,x) as '0' and returns its size. */
static int flood(char *g, int n, int m, int y, int x) {
    if (y < 0 || y >= n || x < 0 || x >= m) return 0;   /* check 1: bounds   */
    if (g[y * m + x] != '1') return 0;                    /* checks 2+3: land, unvisited (we sink it) */
    g[y * m + x] = '0';                                   /* mark NOW, before recursing */
    int size = 1;
    for (int d = 0; d < 4; d++) size += flood(g, n, m, y + DY[d], x + DX[d]);
    return size;
}

int count_islands(char *g, int n, int m) {
    int count = 0;
    for (int y = 0; y < n; y++)          /* the outer loop is not optional */
        for (int x = 0; x < m; x++)
            if (g[y * m + x] == '1') { count++; flood(g, n, m, y, x); }
    return count;
}
```

### Worked example

Grid 3×4 (`.` = water, `#` = land):

```
y\x 0 1 2 3
 0  # # . .
 1  . # . #
 2  . . . #
```

Outer loop reaches (0,0): land → `count=1`, `flood(0,0)`. It sinks (0,0), recurses: up/left out of bounds, down (1,0) is water, right (0,1) is land → sink, recurse: down (1,1) land → sink; its neighbours are all water or sunk. `flood` returns 3. Loop continues: (0,1),(1,1) are now `'0'`, skipped. Reaches (1,3): land → `count=2`, sinks (1,3),(2,3). Result: 2 islands, sizes 3 and 2. Every cell was entered at most once as a valid cell → O(nm).

### The "border first" trick

Two problems in this unit (Closed Islands, Surrounded Regions) flip the question: regions touching the border are special. Do not try to decide "touches the border?" *during* a single search — you'd have to finish the whole region before knowing. Instead run a **first pass** of DFS from every border cell of the target type to mark/sink those regions, then a **second pass** over the interior with the normal counting formula. Pacific Atlantic uses the same shape, plus **reversed direction**: instead of "where can water from this cell go", ask "which cells can reach this ocean", which means searching *uphill* (neighbour height ≥ current). Reversing a search flips the inequality — the classic bug is forgetting to.

### Complexity

O(nm) time — each cell is validated and marked at most once; each cell is *examined* at most 4 (or 8) times as someone's neighbour. O(nm) extra space for `visited` (or O(1) if you mutate the grid) plus recursion depth up to O(nm) in the worst case (a snake-shaped region).

### Pitfalls

- Recursion depth: a 1000×1000 all-land grid recurses a million deep and blows the default 8 MB stack. Use an explicit stack (an `int` array of `n*m` packed indices) when grids are large — see Section 2's queue; a stack is the same array with `pop = --top`.
- Flood fill where the new colour equals the old: "recolour" no longer changes anything, so it no longer acts as a visited mark → infinite loop. Check this special case first.
- Off-grid neighbours of a border cell count as "water" for perimeter-type counting — handle the bounds check before indexing, never after.
- Returning a size from the recursion (Max Area) is correct; accumulating into a shared global is how island sizes get mixed together.

**Python equivalent:** the same recursion, or `sys.setrecursionlimit` plus a `set` of visited tuples. In C the visited set is a byte array — 1 byte per cell, not a 100-byte hashed tuple.

---

## 2. Grid BFS and shortest paths in unweighted graphs

### The idea

BFS is the **mandatory** choice whenever the problem asks for a shortest path or a minimum number of steps in an unweighted graph. DFS would find *some* path, not the shortest. The reason is structural: BFS processes nodes via a queue in exact order of distance from the source, so when a node is **first** enqueued (or dequeued, depending on where you assign the distance) its distance is already final and cannot be improved by a route found later.

### The invariant

At every moment the queue contains nodes of at most two consecutive distances `d, d, d, d+1, d+1` — never `d+2` before all `d+1` are there. Everything already dequeued has distance ≤ everything still in the queue. This is what makes "first time seen = shortest distance" true, and it is the same property Dijkstra generalises with a heap (Section 6).

### On a grid

Identical neighbour generation to DFS (the four coordinate offsets), but a queue instead of a stack, and each cell stores its distance (or step count) from the source the moment it is first discovered.

### Multi-source BFS

Important extension: when several start points are active *simultaneously* (all rotten oranges, all zeros in a 0/1 matrix), push **all** sources into the queue at distance 0 before the first round. This is one BFS, cost O(nm), versus one BFS per source, O(n²m²). Conceptually it's BFS from a virtual super-source connected to every real source by a zero-length edge. The resulting `dist` array *is* a distance transform.

### Generalised BFS

BFS is not about grids — it's about any state space with unit-cost transitions and a "fewest steps" goal:

| Problem | Node (state) | Neighbours | State count |
|---|---|---|---|
| Word Ladder | a word | change one letter to any of 26, keep if in dictionary | n words |
| Open the Lock | 4-digit string | turn one of 4 wheels ±1 | 10⁴ |
| Visit-all-nodes | `(node, visited-bitmask)` | move to adjacent node, OR its bit into the mask | n · 2ⁿ |

The key question is always "what is the *complete* state?" — if the same node can be reached with different useful histories (which nodes are already visited, how many stops used), the history goes into the state, and `visited` becomes 2-D.

### C layout: the queue

A BFS queue never needs to be a ring buffer: each node is enqueued at most once, so an array of size `n` (or `n*m` for a grid) with `head` and `tail` indices suffices — push `q[tail++] = v`, pop `v = q[head++]`, empty when `head == tail`. Pack a grid cell as `y*m + x` into one `int` to avoid a struct queue. Distances: `int *dist` initialised to `-1` (meaning unvisited) — one array serves as both `visited` and the answer.

### Skeleton

```c
/* Multi-source BFS on an n×m grid. Cells with grid[i]==1 are blocked.
   dist[i] = -1 for blocked/unreached, else steps from the nearest source. */
void bfs_grid(const int *grid, int n, int m, const int *sources, int ns, int *dist) {
    int *q = malloc((size_t)n * m * sizeof *q);
    int head = 0, tail = 0;
    for (int i = 0; i < n * m; i++) dist[i] = -1;
    for (int s = 0; s < ns; s++) { dist[sources[s]] = 0; q[tail++] = sources[s]; } /* all at d=0 */
    while (head < tail) {
        int cur = q[head++], y = cur / m, x = cur % m;
        for (int d = 0; d < 4; d++) {
            int ny = y + DY[d], nx = x + DX[d];
            if (ny < 0 || ny >= n || nx < 0 || nx >= m) continue;
            int nb = ny * m + nx;
            if (grid[nb] == 1 || dist[nb] != -1) continue;   /* blocked or already reached */
            dist[nb] = dist[cur] + 1;                          /* mark ON ENQUEUE */
            q[tail++] = nb;
        }
    }
    free(q);
}
```

If you need "how many rounds until everything is reached" (Rotting Oranges), the answer is `max(dist)` over reached cells; if some fresh cell still has `dist == -1`, the answer is `-1`. Level-by-level processing (`for (int k = tail - head; k > 0; k--)` inside the loop) gives you explicit rounds when you need to do something per level.

### Worked example (multi-source, 01 Matrix)

```
input        dist after BFS
1 1 1        2 1 2
1 0 1   ->   1 0 1
1 1 1        2 1 2
```

Queue starts `[(1,1)]` at d=0. Pop it: neighbours (0,1),(2,1),(1,0),(1,2) all get d=1 and are enqueued. Pop (0,1): (0,0),(0,2) get d=2. Pop (2,1): (2,0),(2,2) get d=2. Pop (1,0): (0,0),(2,0) already set — skipped. And so on; nothing is ever overwritten. Nine cells, nine enqueues.

### Complexity

O(V + E): each node enqueued once, each edge examined once. On a grid V = nm, E ≤ 4nm → O(nm). For an abstract state space it's O(states × branching), e.g. O(n · L · 26) for Word Ladder, O(n · 2ⁿ · n) for bitmask BFS.

### Pitfalls

- Marking visited on **dequeue** instead of enqueue: still correct, but the queue can hold the same cell many times, blowing the "size n array is enough" assumption and costing time. Mark on enqueue.
- Forgetting to check that source and target are themselves passable before starting (Shortest Path in Binary Matrix) — if either is blocked the answer is `-1` immediately.
- Excluding the start from goal candidates when the problem demands it (Nearest Exit: the entrance is on the border but is not an exit).
- Running a separate BFS per source instead of one multi-source BFS — wrong *and* slow for "simultaneous spreading" semantics.
- Word-ladder-type: generating neighbours by trying all 26 letters per position and checking a hash set (Chapter 10) is O(L·26) per word; comparing every pair of words is O(n²L). Delete a word from the set when discovered — that's your visited mark.
- A grid with more than two islands (Shortest Bridge): the DFS marking phase must stop after the *first* island (return once found), or it merges two islands into the multi-source set.

**Python equivalent:** `collections.deque`, `popleft()`. In C: a plain array and two indices.

---

## 3. Connected components on general graphs

### The idea

A connected component is a maximal set of nodes each reachable from every other by some path. Counting components is always the same formula: loop over **all** nodes in an outer loop, and each time you meet an unvisited node, launch a DFS or BFS that marks its whole component. The number of launches is the number of components.

### Why the outer loop is mandatory

One search from one node finds only that node's component. If the graph is not guaranteed connected, other nodes stay unvisited unless the outer loop reaches them. This is the single most common bug in this unit: "I ran DFS from node 0 and got 1 component."

### DFS or BFS?

Either gives the same result, because the question is reachability, not distance. It's a matter of taste: DFS is shorter as recursion; BFS avoids stack overflow on deep graphs (a path graph of 10⁵ nodes recurses 10⁵ deep). In C with an unknown input size, prefer an explicit stack or BFS.

### Recognising the pattern in disguise

Many apparently different problems are this formula:

| Question | What it is |
|---|---|
| "Is there a path from A to B?" | one search from A; is B marked? |
| "How many groups / provinces / islands?" | component count |
| "Can you reach every room from room 0?" | one search; is `visited_count == n`? |
| "Merge records that share a field" (Accounts Merge) | build implicit edges between shared fields, collect each component |
| "Can the nodes be 2-coloured?" (Bipartite) | search carrying a colour; conflict = not bipartite |
| "Copy this graph" (Clone Graph) | search + a hash map original → copy |
| "a/b = k, what is c/d?" (Evaluate Division) | search accumulating an edge-weight product |

The search body is what changes: the "visited" side-effect can carry a colour, a running product, a pointer to a copied node, or a list of members.

### C layout

- CSR adjacency (Section 0) for an edge-list input; the matrix directly when the input is `isConnected[i][j]` (then a node's neighbour scan is O(n), total O(n²)).
- `int *comp` of size n, initialised to `-1`, storing the component id — this is both the visited mark *and* a useful output (Python's `dict` of node → label).
- Explicit stack: `int *stack = malloc(n * sizeof *stack)` — each node is pushed once if you mark on push.
- String-keyed nodes (emails, variable names): map each string to an integer id via a hash table (Chapter 10) *first*, then run everything on ints. Never DFS over `char*` directly.

### Skeleton

```c
/* Labels every node with its component id; returns the number of components. */
int components(const Graph *g, int *comp) {
    int *stack = malloc((size_t)g->n * sizeof *stack), ncomp = 0;
    for (int i = 0; i < g->n; i++) comp[i] = -1;
    for (int s = 0; s < g->n; s++) {                    /* outer loop */
        if (comp[s] != -1) continue;
        int top = 0; stack[top++] = s; comp[s] = ncomp;  /* mark on push */
        while (top > 0) {
            int u = stack[--top];
            for (int k = g->off[u]; k < g->off[u + 1]; k++) {
                int v = g->adj[k];
                if (comp[v] == -1) { comp[v] = ncomp; stack[top++] = v; }
            }
        }
        ncomp++;
    }
    free(stack);
    return ncomp;
}
```

Bipartite check is this loop with `color[v] = 1 - color[u]` on discovery and a `color[v] == color[u]` test on every already-coloured neighbour — including across the outer loop, since one component may be bipartite while another is not.

### Worked example

n = 6, edges `(0,1) (1,2) (3,4)`. Outer loop: s=0 unvisited → comp 0, stack visits 0,1,2. s=1,2 skipped. s=3 → comp 1: 3,4. s=4 skipped. s=5 → comp 2 (isolated node, zero edges — still a component). Answer 3. Without the outer loop you'd report 1.

### Complexity

O(n + m) with an adjacency list: every node is pushed once, every adjacency entry scanned once. O(n²) with an adjacency matrix. Accounts Merge adds an O(k log k) sort per component.

### Pitfalls

- Source == target in a path query: trivially true; make sure the search handles it (marking the source before checking the target does).
- Clone Graph: insert `original → copy` into the map **before** recursing into neighbours, else a cycle recurses forever.
- Evaluate Division: every equation `a/b = k` is *two* directed edges, `a→b` weight `k` and `b→a` weight `1/k`; forgetting the reverse edge makes queries one-directional. Query nodes not in the graph → `-1.0`.
- Merging records by *name* instead of by shared field — two different people can share a name.
- Isolated nodes count as components.

**Python equivalent:** `visited = set()`, iterate `for s in range(n): if s not in visited: ...`. `networkx.connected_components` if you're cheating.

---

## 4. Cycle detection and topological sort

### The idea

A topological order of a directed graph is an ordering of its nodes such that every edge points from earlier to later. It exists **exactly when** the graph has no directed cycle (it's a DAG). Hence topological sorting and cycle detection are one algorithm viewed from two sides: if the sort manages to place every node, there is no cycle; if some nodes can never be placed, they lie on (or downstream of) a cycle.

### Kahn's algorithm (BFS flavour)

1. Compute `indeg[v]` = number of edges pointing into `v`.
2. Enqueue every node with `indeg == 0`.
3. Pop `u`, append it to the order, and for each edge `u → v` decrement `indeg[v]`; if it hits 0, enqueue `v`.
4. If the order has fewer than `n` nodes at the end, the leftovers form/depend on a cycle.

The order comes out in the sequence nodes are popped. Using a min-heap instead of a queue gives the lexicographically smallest topological order.

### DFS flavour (three colours)

Each node is in one of three states: **white** (unvisited), **grey** (on the current recursion path, "in progress"), **black** (finished). If DFS reaches a **grey** node, you have found a cycle — a back edge to something on the current path. A plain two-state `visited` flag is **not enough**: it cannot distinguish "on my current path" (cycle) from "already fully finished in another branch" (a DAG with two routes to the same node, perfectly fine). The topological order is the **post-order** (append a node when it turns black, after all its successors) **reversed**.

```
    0 → 1 → 3
    0 → 2 → 3         two routes to 3, no cycle. With 2 states, visiting 3 the second
                       time looks like a cycle. With 3 states it's black → fine.
```

### Which to choose

| | Kahn | Three-colour DFS |
|---|---|---|
| Order of output | direct | post-order, then reverse |
| Cycle detection | count < n | grey hit |
| Recursion | none | depth up to n (use explicit stack for large n) |
| Extras | easy to get lexicographic order via heap; natural for "peel layers" (Minimum Height Trees) | natural for "is this node safe / memoise per node" (Eventual Safe States), Hierholzer |

### C layout

CSR adjacency for the *directed* graph (each edge stored once). `int *indeg` (Kahn), or `unsigned char *color` (DFS). Output order as an `int` array with a write index; for DFS post-order write forward then reverse in place, or write from the back (`order[--pos] = u`) to skip the reverse.

### Skeleton (Kahn)

```c
/* Returns count of ordered nodes (== n iff acyclic). order[] receives a topological order. */
int kahn(const Graph *g, int *order) {
    int n = g->n, *indeg = calloc(n, sizeof *indeg), *q = malloc(n * sizeof *q);
    for (int k = 0; k < g->off[n]; k++) indeg[g->adj[k]]++;
    int head = 0, tail = 0;
    for (int u = 0; u < n; u++) if (indeg[u] == 0) q[tail++] = u;
    while (head < tail) {
        int u = q[head++];
        order[head - 1] = u;                              /* q itself IS the order */
        for (int k = g->off[u]; k < g->off[u + 1]; k++)
            if (--indeg[g->adj[k]] == 0) q[tail++] = g->adj[k];
    }
    free(indeg); free(q);
    return head;                                          /* < n means a cycle exists */
}
```

Skeleton (DFS, iterative-free version for clarity):

```c
/* 0 = white, 1 = grey, 2 = black. Returns 1 if a cycle is reachable from u. */
static int dfs_cycle(const Graph *g, int u, unsigned char *color, int *order, int *pos) {
    color[u] = 1;
    for (int k = g->off[u]; k < g->off[u + 1]; k++) {
        int v = g->adj[k];
        if (color[v] == 1) return 1;                            /* back edge → cycle */
        if (color[v] == 0 && dfs_cycle(g, v, color, order, pos)) return 1;
    }
    color[u] = 2;
    order[--*pos] = u;                                          /* post-order, filled from the back = reversed */
    return 0;
}
```

### Worked example (Kahn)

Courses: 0→1, 0→2, 1→3, 2→3, 3→4.

```
indeg      : [0, 1, 1, 2, 1]
queue      : [0]                 order: 0
pop 0      : indeg[1]=0,[2]=0    queue [1,2]
pop 1      : indeg[3]=1          order: 0 1
pop 2      : indeg[3]=0          order: 0 1 2   queue [3]
pop 3      : indeg[4]=0          order: 0 1 2 3 queue [4]
pop 4      :                     order: 0 1 2 3 4   → 5 == n, acyclic
```

Add edge 4→1: `indeg[1]` starts at 2, never reaches 0 after pop 0; only node 0 is ever ordered → count 1 < 5 → cycle.

### Variants in this unit

- **Transitive closure over a DAG** (Course Schedule IV): process nodes in topological order and give each node the *union* of its predecessors' ancestor sets plus the predecessors themselves; a predecessor is always finished before anyone depending on it. In C: a bitset of `n` bits per node (`uint64_t words[(n+63)/64]`), union = word-wise OR. O(n · (n+m)/64).
- **Eventual Safe States**: three-colour DFS with memoisation — a node is safe iff it's not grey and all successors are safe; store the verdict so each node is resolved once. Without the memo: exponential.
- **Peeling leaves** (Minimum Height Trees): Kahn on an *undirected* tree, "in-degree" = degree, start from degree-1 nodes, stop when ≤ 2 remain — the centre(s).
- **Hierholzer** (Reconstruct Itinerary): consume edges one at a time; a node is appended to the result only once it has *no unused outgoing edges* (post-order), then reverse. Greedy-without-backtracking fails; post-order fixes it automatically.

### Complexity

O(n + m) for both flavours. Sorting adjacency lists (lexicographic order, Hierholzer) adds O(m log m).

### Pitfalls

- Two-state visited in DFS cycle detection → false positives on any DAG with a diamond.
- Forgetting to reverse DFS post-order (or to fill from the back).
- Returning a partial order when a cycle exists; the problems want an empty result.
- Edge direction: "a depends on b" is `b → a` (b first). Pick a convention and be consistent in both building and reading.
- Undirected cycle detection is a *different* problem (any back edge to a non-parent) — and is what union-find does best (Section 5).

**Python equivalent:** `graphlib.TopologicalSorter` in the stdlib does Kahn.

---

## 5. Union-Find (disjoint set union)

### The idea

Union-find maintains a family of disjoint sets with two operations: `find(x)` returns the set's representative (root), and `union(x, y)` merges two sets. It is the right tool when **edges arrive as a stream** and you must repeatedly answer "are these two nodes in the same component?" or "how many components remain?" without rebuilding the whole graph per question. DFS/BFS answers the same question for one fixed edge set; union-find wins when edges are added dynamically and component information is needed continuously.

### The two optimisations

Together they make each operation almost O(1) — precisely O(α(n)) amortised, α being the inverse Ackermann function (≤ 4 for any n you'll ever see):

1. **Path compression:** `find` flattens the path to the root — every node it passes is repointed directly to the root.
2. **Union by size (or rank):** always hang the smaller tree under the larger, never the other way; tree height stays O(log n) even without compression.

Skipping both gives O(n) worst-case per op (a chain). Either one alone gives O(log n). Both: α(n).

### Recognising it

- component count while edges are being added;
- "how many edges can be removed / how many extra cables are there before the network falls apart";
- the first edge that closes a cycle in an undirected graph;
- Kruskal's MST: add edges in weight order, skip any whose endpoints are already connected — exactly the cycle check;
- "threshold" problems: process cells/edges sorted by some value and stop when two specific nodes become connected (Swim in Rising Water) — Kruskal in disguise, replacing binary-search-on-answer + BFS.
- Equality constraints with transitivity (`a==b`, `b==c` ⇒ `a==c`): union all `==` first, then test every `!=`.

### C layout

Two `int` arrays: `parent[n]` (initially `parent[i] = i`) and `size[n]` (initially 1). Optional `int count` of live components. Node ids must be small ints — map strings/coordinates to ids first. Combining two id spaces (rows and columns in Most Stones Removed, or four triangles per cell in Regions Cut by Slashes) is just an offset: `col_id = n_rows + c`, `tri_id = 4*(y*m + x) + which`.

### Skeleton

```c
typedef struct { int *parent, *size, count; } DSU;

void dsu_init(DSU *d, int n) {
    d->parent = malloc(n * sizeof *d->parent); d->size = malloc(n * sizeof *d->size); d->count = n;
    for (int i = 0; i < n; i++) { d->parent[i] = i; d->size[i] = 1; }
}
int dsu_find(DSU *d, int x) {                 /* iterative two-pass path compression */
    int root = x;
    while (d->parent[root] != root) root = d->parent[root];
    while (d->parent[x] != root) { int next = d->parent[x]; d->parent[x] = root; x = next; }
    return root;
}
/* Returns 1 if a merge happened, 0 if x and y were already in the same set. */
int dsu_union(DSU *d, int x, int y) {
    int rx = dsu_find(d, x), ry = dsu_find(d, y);
    if (rx == ry) return 0;
    if (d->size[rx] < d->size[ry]) { int t = rx; rx = ry; ry = t; }   /* rx is the bigger root */
    d->parent[ry] = rx; d->size[rx] += d->size[ry]; d->count--;
    return 1;
}
```

Iterative `find` avoids recursion entirely — no stack depth concerns. The return value of `dsu_union` is the whole point: `0` means "this edge closes a cycle / is redundant".

### Worked example (Redundant Connection)

n = 5, edges in order `(1,2) (2,3) (3,4) (1,4) (4,5)` (1-indexed → subtract 1 or allocate n+1).

```
after (1,2): parent [1,1,3,4,5]  size[1]=2   count 4
after (2,3): find(2)=1, find(3)=3 → parent[3]=1, size[1]=3   count 3
after (3,4): find(3)=1, find(4)=4 → parent[4]=1               count 2
try   (1,4): find(1)=1, find(4)=1 → SAME ROOT → redundant edge = (1,4)
```

`(4,5)` is never looked at; the answer is the *last* edge in input order that closes a cycle, and since a tree plus one edge has exactly one cycle, the first `union` failure in input order is it — process in the given order, do **not** sort.

### Kruskal (Min Cost to Connect All Points)

Generate all edges (all n(n-1)/2 point pairs, weight = Manhattan distance), `qsort` by weight, walk them: `if (dsu_union(u, v)) total += w, accepted++`; stop when `accepted == n-1`. Greedy is correct *only because* union-find refuses cycle-closing edges. O(n² log n) dominated by the sort.

### Complexity

O((n + m) α(n)) for m operations. Sorting edges for Kruskal: O(m log m). Space O(n).

### Pitfalls

- Doing the `!=` checks before *all* `==` unions are done — transitive equalities not yet visible, contradiction missed.
- Forgetting the size/rank swap → still correct, but O(log n) instead of α(n); forgetting compression too → O(n) chains.
- Network Connected: check `m >= n-1` first (else `-1`); the answer is `components - 1`, and the redundant cables (failed unions) are guaranteed sufficient once `m >= n-1`.
- Most Stones Removed: unioning stone pairs directly is O(n²); union each stone's row-node with its column-node instead — answer `n - components`, counting only components that contain at least one stone.
- Swim in Rising Water: the answer is at least `grid[0][0]` — the start cell itself is a lower bound.
- 1-indexed inputs: allocate `n+1` or subtract 1 everywhere; mixing the two is a classic off-by-one.

**Python equivalent:** there is none in the stdlib; people write the same 15 lines. `scipy.sparse.csgraph.connected_components` is the batch version.

---

## 6. Dijkstra and weighted shortest paths

### The idea

When edges have non-negative weights, BFS is no longer enough — you need Dijkstra. The idea is greedy: keep a priority queue (min-heap) of nodes keyed by their best known distance so far, and each round extract the node with the **smallest** distance. When a node is popped for the first time, its distance is final — exactly the same guarantee BFS gives, but now "distance" is a sum of weights rather than a step count, so a priority queue rather than a FIFO must pick the next node.

### Why non-negative weights are required

The finality argument: any route discovered later to the popped node `u` would pass through some node `x` still in the heap with `dist[x] ≥ dist[u]`, and then add non-negative weights — so it cannot be shorter. A negative edge breaks this directly (going through the "farther" `x` could get cheaper). With negative edges you need Bellman-Ford (or, with a hop limit, the bounded Bellman-Ford below).

### Relaxation — the core operation

For edge `(u, v, w)`: if `dist[u] + w < dist[v]`, set `dist[v] = dist[u] + w` and push `(dist[v], v)` onto the heap. You do **not** delete the older, now-stale heap entry for `v` (heaps don't support that cheaply); instead, when popping `(d, v)`, check `d > dist[v]` — if so, it's stale, skip it. This is **lazy deletion**, and it means the heap can hold up to O(m) entries, hence the complexity below.

### Complexity

O((n + m) log n) with a binary heap (the heap holds O(m) entries; log m = O(log n) for simple graphs). Grid: O(nm log(nm)). Running from every source (Find the City): O(n (n+m) log n), versus Floyd-Warshall's O(n³) — better for sparse, worse for dense small graphs.

### Variants change only "what is best" and "how to combine"

| Problem | dist meaning | relax rule | heap order |
|---|---|---|---|
| Network Delay Time | sum of weights | `d[u] + w < d[v]` | min |
| Path with Maximum Probability | product of probs | `d[u] * w > d[v]` | max (or min on negated) |
| Path With Minimum Effort | max edge on path | `max(d[u], |h[u]-h[v]|) < d[v]` | min |
| Number of Ways to Arrive | sum + count | `<` : copy count; `==` : add count | min |
| Cheapest Flights ≤ K stops | sum, state = `(node, stops)` | bounded Bellman-Ford, k+1 rounds | — |
| Min Obstacle Removal | sum of 0/1 weights | 0-1 BFS with a deque | deque, no heap |

Greedy finality holds for the product variant because probabilities are in [0, 1] (multiplying never increases), and for the minimax variant because `max` is monotone — "non-negative and monotone combine" is the real precondition.

### C layout

- Adjacency: CSR with a parallel weight array (`int *w` or `double *w`).
- `dist`: `long long` (sums overflow `int` faster than you think) or `double`, initialised to a sentinel `INF` (e.g. `LLONG_MAX / 4` so `INF + w` can't overflow, or `INFINITY`/`DBL_MAX` for doubles).
- Heap: an array of `struct { long long d; int v; }` with sift-up/sift-down from Chapter 10 — capacity `m + 1` for lazy deletion, or `n` if you implement decrease-key with a position index (not worth it in contests).
- Optional `unsigned char *done` — redundant with the stale check, but makes intent explicit.

### Skeleton

```c
typedef struct { long long d; int v; } HItem;
typedef struct { HItem *a; int len; } Heap;   /* min-heap by d; push/pop as in ../../c_learning/10_data_structures/lesson.md */

void dijkstra(const Graph *g, const int *w, int src, long long *dist) {
    const long long INF = LLONG_MAX / 4;
    for (int i = 0; i < g->n; i++) dist[i] = INF;
    Heap h = heap_new(g->off[g->n] + 1);            /* capacity = number of edges + 1 (lazy deletion) */
    dist[src] = 0; heap_push(&h, (HItem){0, src});
    while (h.len > 0) {
        HItem top = heap_pop(&h);
        if (top.d > dist[top.v]) continue;           /* stale entry → skip */
        int u = top.v;
        for (int k = g->off[u]; k < g->off[u + 1]; k++) {
            int v = g->adj[k]; long long nd = top.d + w[k];
            if (nd < dist[v]) { dist[v] = nd; heap_push(&h, (HItem){nd, v}); }
        }
    }
    heap_free(&h);
}
```

### Worked example

Nodes 0..3, directed edges `0→1 (4), 0→2 (1), 2→1 (2), 1→3 (1), 2→3 (5)`, source 0.

```
dist = [0, ∞, ∞, ∞]      heap: (0,0)
pop (0,0): relax 1→4, 2→1          dist [0,4,1,∞]   heap (1,2) (4,1)
pop (1,2): relax 1→3 (better), 3→6 dist [0,3,1,6]   heap (3,1) (4,1) (6,3)
pop (3,1): relax 3→4 (better)      dist [0,3,1,4]   heap (4,1) (4,3) (6,3)
pop (4,1): 4 > dist[1]=3 → STALE, skip
pop (4,3): relax nothing           final
pop (6,3): stale, skip
```

Node 1 was pushed twice; the stale `(4,1)` did no harm. Answer `[0, 3, 1, 4]`.

### 0-1 BFS

When every weight is 0 or 1, replace the heap with a deque: push weight-0 neighbours to the **front**, weight-1 neighbours to the **back**. The deque stays sorted by distance (it holds only values `d` and `d+1`), the finality guarantee is preserved, and every operation is O(1) → O(V + E) total. In C: a ring buffer of size `2·(n·m)` (front pushes need room), or an array of size `2·(n·m)+1` with the initial head in the middle.

### Bounded Bellman-Ford (Cheapest Flights Within K Stops)

Plain Dijkstra **fails** here: it finalises a node the first time it pops, but the cheapest route within ≤ k stops may pass through a *more expensive* intermediate state that saves stops later. Two fixes: (1) Bellman-Ford limited to exactly `k+1` rounds over the edge list, each round relaxing from a **copy** of the previous round's distances (so a round corresponds to exactly one more edge) — O(k · m); (2) Dijkstra whose state is `(node, stops_used)`, so the same node may be processed again with a different stop count. Either way, the lesson is: when a constraint changes what "same state" means, put it into the state.

### Pitfalls

- Forgetting the stale check: still correct, but the same node is re-expanded for every stale entry — quadratic blow-up on dense graphs.
- `int` overflow in `dist[u] + w`; using `INT_MAX` as INF and then adding to it (UB). Use `long long` and `LLONG_MAX / 4`.
- Max-probability with an unmodified min-heap → you find the *least* probable path.
- Summing height differences in Minimum Effort → answers a different question (total climb, not worst step).
- Number of Ways: on a strictly better distance, *replace* the count (don't add); on an equal distance, *add*. Keep counts modulo 10⁹+7 in `long long`.
- Find the City tiebreak: the larger city index wins.
- Plain BFS on a 0/1-weight grid — it ignores weights and is simply wrong.

**Python equivalent:** `heapq.heappush(h, (d, v))`, tuples compare lexicographically so the pair just works. In C you write the comparison on `.d` explicitly; if you also need tie-breaking on `v`, say so in the comparator.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Complexity |
|---|---|---|
| grid, "connected region", count islands, fill, "surrounded", "closed" | grid DFS (flood fill), border-first for boundary conditions | O(nm) |
| "minimum steps", "shortest path", unweighted, "minutes until", "nearest" | BFS; multi-source if several starts | O(V+E) = O(nm) |
| "shortest transformation", lock, words, "visit all nodes" | BFS over a state space; state = everything that matters | O(states × branching) |
| "how many groups", "is there a path", "can you reach every", merge records | components: outer loop + DFS/BFS | O(n+m) |
| "two groups so that no edge...", "split into two teams" | bipartite 2-colouring DFS/BFS | O(n+m) |
| copy a graph, `a/b = k` queries | DFS/BFS with a hash map / weight product | O(n+m), O(q(n+m)) |
| prerequisites, "can all be finished", order of tasks, build order | Kahn or 3-colour DFS topological sort | O(n+m) |
| "safe nodes", "no path leads to a cycle" | 3-colour DFS with memo | O(n+m) |
| "peel leaves", "centre of a tree", minimum height | Kahn on an undirected tree | O(n) |
| use every edge once, itinerary | Hierholzer (post-order + reverse) | O(m log m) |
| edges arrive over time, "redundant edge", "extra cables", `==`/`!=` constraints | union-find | O((n+m) α(n)) |
| minimum cost to connect all, sorted by weight/height/threshold | Kruskal (sort + union-find) | O(m log m) |
| weighted, non-negative, shortest / cheapest / fastest | Dijkstra with lazy deletion | O((n+m) log n) |
| maximise probability / minimise worst step / count shortest paths | Dijkstra with a changed relax rule | O((n+m) log n) |
| weights only 0 and 1 | 0-1 BFS with a deque | O(V+E) |
| "at most k stops / edges" | bounded Bellman-Ford or `(node, k)` state | O(k·m) |
| all-pairs, small n, threshold distance | n × Dijkstra, or Floyd-Warshall | O(n(n+m) log n) / O(n³) |

---

## Gotchas in C specifically

- **Out-of-bounds is silent.** `grid[y][x]` with `y == n` reads whatever is next in memory; no exception. Do the bounds check *before* indexing, every time, and run with `-fsanitize=address` while developing (Chapter 13).
- **Recursion depth.** Default stack is 8 MB (macOS main thread) — a million-deep flood fill or a 10⁵-node DFS on a path graph crashes. Convert to an explicit stack array when the input can be large. Iterative `find` in union-find for the same reason.
- **No built-in queue/deque/heap/set.** Queue = `int` array with `head`/`tail` (each node enqueued once → size n suffices). Deque = ring buffer with `(i + cap) % cap` indexing. Heap = Chapter 10's sift-up/down. Hash set for string states = Chapter 10's open addressing — write it once, keep it in `src/`.
- **LeetCode's C signatures.** `char** grid, int gridSize, int* gridColSize` — the grid is an array of row pointers, not contiguous. `int** edges, int edgesSize, int* edgesColSize` for edge lists. Return arrays via `malloc` and set `*returnSize`. Never return a pointer to a local array.
- **Overflow in distances.** `int` sums of `10⁵` edges of weight `10⁴` overflow. Use `long long`; pick `INF = LLONG_MAX / 4` so `INF + w` stays representable. Never compare against `INT_MAX` after adding to it.
- **Sentinel visited values.** `dist[i] = -1` as "unvisited" collapses two arrays into one — but then a genuine distance of `-1` cannot exist; fine for BFS, not for graphs with negative weights.
- **qsort comparator for edges.** `int cmp(const void *a, const void *b) { const Edge *x = a, *y = b; return (x->w > y->w) - (x->w < y->w); }` — never `return x->w - y->w` (overflows on large weights). For `double` weights the subtraction trick doesn't even compile to an `int` correctly.
- **`calloc` for zero-init arrays** (`indeg`, `visited`, `off`), `malloc` + explicit loop for `-1`/`INF` init. `memset` to `-1` works for `int` (all bytes 0xFF) but *not* to set `INF`.
- **Packing 2-D indices.** `idx = y * m + x` with `m` = number of *columns*; decode `y = idx / m, x = idx % m`. Swapping n and m here is a silent bug on non-square grids — test on a 2×5 grid, not 4×4.
- **Half-open ranges everywhere.** CSR row `u` is `[off[u], off[u+1])`; the BFS queue is `[head, tail)`; the topological order fills `[0, count)`. Consistency with Chapter 04 pays off.
- **Direction tables as `static const`** at file scope — not re-created per call, and the compiler can fold them.
- **Freeing.** Every `malloc` in a helper needs a matching `free` before return, including early returns on "cycle found" — or structure the code so cleanup is at a single exit.

---

## Common mistakes checklist

- [ ] Grid DFS: all three checks (bounds, visited, target) present, bounds check *first*.
- [ ] Visited marked on discovery (push/enqueue), not on pop.
- [ ] Outer loop over all nodes/cells when counting components — one search finds one component.
- [ ] Border-first pass before counting "closed" / "surrounded" regions; reverse inequality when searching from the sink.
- [ ] BFS, not DFS, whenever "shortest"/"minimum steps" appears; multi-source when spread is simultaneous.
- [ ] State-space BFS: is the state complete? (bitmask, stops used, ...) — `visited` dimensioned accordingly.
- [ ] Start/target passable? Start excluded from goals when the problem says so? Source == target handled?
- [ ] Cycle detection in a *directed* graph uses three colours, not two.
- [ ] DFS topological order reversed (or filled from the back); empty output on cycle.
- [ ] Clone/DFS-with-map: insert into the map *before* recursing into neighbours.
- [ ] Union-find: path compression **and** union by size; process edges in the order the problem specifies; `==` unions before `!=` checks.
- [ ] Dijkstra: stale-entry check on pop; `long long` distances; the relax rule matches the objective (sum / max / product / count).
- [ ] Non-negative weights confirmed before using Dijkstra; hop-limit → bounded Bellman-Ford or `(node, stops)` state.
- [ ] Ties and special cases from the statement (largest index wins, answer ≥ `grid[0][0]`, no fresh oranges → 0).
- [ ] Every `malloc` freed; tested on a non-square grid and on a disconnected graph.

---

## You can move on when...

- You can write flood fill and multi-source grid BFS from memory in C, with a flat array and packed indices, and explain in one sentence why BFS's first discovery is the shortest distance.
- Given an edge list, you can build a CSR adjacency structure without looking anything up, and iterate a node's neighbours as a half-open range.
- You can state the difference between two-state and three-state DFS cycle detection and draw the diamond DAG that breaks the two-state version.
- You can implement `find` with path compression iteratively and `union` by size, and explain what the return value of `union` tells you.
- You can write Dijkstra with a binary heap and lazy deletion, then modify the relax rule to minimise the maximum edge on the path — without restructuring anything else.
- You have solved at least the level-2 and level-3 problems of every unit in `problems.md` in C, with your own `main()` tests, and know which of them are BFS-in-disguise, components-in-disguise, and Kruskal-in-disguise.
