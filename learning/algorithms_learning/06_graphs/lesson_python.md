# Chapter 06 — Graphs — Python

## What you'll be able to do after this chapter

- Treat a 2-D grid as an implicit graph and run DFS / flood fill on it to mark, count or transform connected regions — with the three checks (bounds, visited, target) in the right place.
- Run BFS for shortest paths in unweighted graphs, including multi-source BFS and BFS over an abstract state space (strings, bitmasks, `(node, extra)` tuples), and explain *why* the first discovery of a node is its final distance.
- Build an adjacency list with `defaultdict(list)`, count connected components with the outer-loop-plus-search formula, and recognise reachability / grouping / 2-colouring problems as the same formula in disguise.
- Detect cycles in a directed graph and produce a topological order two ways (Kahn's in-degree BFS, three-colour DFS post-order, or `graphlib.TopologicalSorter`), and state exactly when a plain `visited` flag is not enough.
- Implement union-find with path compression and union by size, and recognise the "edges arrive in a stream / count components as you go / Kruskal" signal.
- Implement Dijkstra with `heapq` and lazy deletion, adapt the relaxation rule (sum, max, product, count-of-paths), and know when to use 0-1 BFS or bounded Bellman-Ford instead.

## Why this matters for ML / numerics / sims

Almost every "spatial" or "dependency" computation in numerics is a graph algorithm wearing a domain costume. A mesh's connected components (is this triangle soup one body or three?) is flood fill / union-find. Pathfinding for agents in a simulation on a grid is BFS (unit cost) or Dijkstra (terrain cost). Segmenting a binary image into blobs is Number of Islands. Computing a distance transform (distance from every pixel to the nearest foreground pixel) is multi-source BFS — the 01 Matrix problem, verbatim. An autograd engine executes a computation graph in **topological order** and checks it for cycles; PyTorch's backward pass is a reverse topological traversal. Build systems, dependency resolvers and scheduler DAGs are Kahn's algorithm. Kruskal's MST over Euclidean distances is single-linkage clustering. Dijkstra with a "max edge on path" relaxation (Path With Minimum Effort) is a minimax path — the same thing as the bottleneck criterion in percolation simulations.

**Python vs C, once for the chapter:** there is no CSR to hand-build, no `malloc`, no recursion-depth-by-hand-with-an-explicit-stack unless you choose to (Python's own recursion limit, ~1000, forces the same choice C makes at 10^5+). `collections.deque` replaces the ring buffer / array-with-head-tail. `heapq` replaces the hand-rolled binary heap. `defaultdict(list)` replaces per-node dynamic arrays. Ints never overflow, so `long long` and `INF = LLONG_MAX / 4` become `float("inf")` with no risk of wraparound. `graphlib.TopologicalSorter` is Kahn's algorithm already written for you. The *ideas* — three checks, mark-on-discover, outer loop, three colours, lazy deletion — are identical; only the plumbing shrinks.

---

## 0. Representing a graph in Python

Before the patterns, the plumbing.

| Representation | Build | Neighbours of `u` | Use when |
|---|---|---|---|
| Implicit grid | none | 4 or 8 coordinate offsets | input *is* a grid |
| Adjacency matrix `list[list[int]]` | O(n^2) | O(n) scan of a row | n small, dense, or input already a matrix |
| Adjacency list, `dict`/`defaultdict(list)` | O(n + m) | O(deg u) | default for sparse graphs |
| Edge list `list[(u, v, w)]` | O(m) | none directly | Kruskal, Bellman-Ford |

```python
from collections import defaultdict

def graph_from_edges(edges: list[tuple[int, int]]) -> dict[int, list[int]]:
    adj: dict[int, list[int]] = defaultdict(list)
    for u, v in edges:
        adj[u].append(v)
        adj[v].append(u)                 # omit for a directed graph
    return adj
```

This is exactly `scipy.sparse.csr_matrix`'s adjacency in spirit, minus the contiguous-memory guarantee — the C chapter builds CSR by hand precisely because that contiguity is what makes sparse-matrix libraries and GNN frameworks fast. In Python you almost never hand-roll CSR; `defaultdict(list)` is idiomatic and fast enough for LeetCode-scale inputs. If you need the C-style array-of-arrays for a NumPy pipeline, `scipy.sparse.csr_matrix((data, (rows, cols)))` builds it for you.

Weighted graphs: store `(neighbour, weight)` tuples, `adj[u].append((v, w))`.

---

## 1. Grid DFS and flood fill

### The idea

A grid is a graph in disguise: each cell is a node and its four (sometimes eight) neighbouring cells are its edges. Neighbours come straight from the coordinates `(r+dr, c+dc)`. DFS is the right tool when the task asks you to **mark, count or transform** a connected region: fill an area, measure its size, or count how many separate areas there are.

### The three checks, every step

Before stepping into a neighbour cell, check — in any order, but all three:

1. **In bounds:** `0 <= r < rows and 0 <= c < cols`.
2. **Not already visited.**
3. **Belongs to the target region:** e.g. `grid[r][c] == "1"`, not `"0"`.

Forgetting any one is the cause of nearly every grid-DFS bug: an `IndexError` (Python at least raises one, unlike C's silent out-of-bounds read), infinite recursion, or bleeding into the wrong region.

### When DFS rather than BFS

When the problem does **not** ask for a distance or shortest path — only "is it connected" or "how big is the region". Order doesn't matter, so a stack (recursion, or an explicit `list`) works as well as a queue. As soon as *shortest* or *minimum steps* appears, switch to Section 2.

### Timing of the visited mark

Mark a cell visited **the moment it is judged valid**, not after the recursive call returns — otherwise the same cell gets pushed multiple times before the first processing marks it. Mutating the grid directly (`"1"` -> `"0"`) is the idiomatic LeetCode shortcut and saves a separate `visited` set.

### Python layout

```python
DIRS = [(-1, 0), (1, 0), (0, -1), (0, 1)]

def flood(grid: list[list[str]], r: int, c: int) -> int:
    """Marks the whole component containing (r,c) as '0' and returns its size."""
    rows, cols = len(grid), len(grid[0])
    if not (0 <= r < rows and 0 <= c < cols):      # check 1: bounds
        return 0
    if grid[r][c] != "1":                           # checks 2+3: land, unvisited (we sink it)
        return 0
    grid[r][c] = "0"                                # mark NOW, before recursing
    size = 1
    for dr, dc in DIRS:
        size += flood(grid, r + dr, c + dc)
    return size

def count_islands(grid: list[list[str]]) -> int:
    count = 0
    for r in range(len(grid)):                      # the outer loop is not optional
        for c in range(len(grid[0])):
            if grid[r][c] == "1":
                count += 1
                flood(grid, r, c)
    return count
```

Recursion is fine at LeetCode grid sizes for typical (non-pathological) inputs, but a snake-shaped 1000x1000 all-land grid will blow Python's ~1000-frame limit well before it would trouble C's 8 MB stack. The iterative version replaces the call stack with a `list`:

```python
def flood_iter(grid: list[list[str]], r: int, c: int) -> int:
    rows, cols = len(grid), len(grid[0])
    st = [(r, c)]
    size = 0
    while st:
        y, x = st.pop()
        if not (0 <= y < rows and 0 <= x < cols) or grid[y][x] != "1":
            continue
        grid[y][x] = "0"
        size += 1
        for dy, dx in DIRS:
            st.append((y + dy, x + dx))
    return size
```

### Worked example

Grid 3x4 (`.` = water, `#` = land):

```
r\c 0 1 2 3
 0  # # . .
 1  . # . #
 2  . . . #
```

Outer loop reaches (0,0): land -> `count=1`, `flood(0,0)`. It sinks (0,0), recurses: up/left out of bounds, down (1,0) water, right (0,1) land -> sink, recurse: down (1,1) land -> sink; its neighbours are all water or sunk. `flood` returns 3. Loop continues: (0,1),(1,1) now `"0"`, skipped. Reaches (1,3): land -> `count=2`, sinks (1,3),(2,3). Result: 2 islands, sizes 3 and 2. Every cell entered at most once as a valid cell -> O(rows*cols).

### The "border first" trick

Two problems in this unit (Closed Islands, Surrounded Regions) flip the question: regions touching the border are special. Do not try to decide "touches the border?" *during* a single search — finish the whole region first. Instead run a **first pass** of DFS from every border cell of the target type to mark/sink those regions, then a **second pass** over the interior with the normal counting formula. Pacific Atlantic uses the same shape plus **reversed direction**: search *uphill* (neighbour height >= current) instead of downhill. Reversing a search flips the inequality — forgetting to is the classic bug.

### Complexity

O(rows*cols) time — each cell validated and marked at most once, examined at most 4 (or 8) times as someone's neighbour. O(rows*cols) extra space for `visited` (or O(1) if you mutate the grid) plus recursion/stack depth up to O(rows*cols) worst case.

### Pitfalls

- Recursion depth in Python is the tighter constraint than in C — prefer the iterative stack form for grids that could be large or adversarially shaped.
- Flood fill where the new colour equals the old: "recolour" no longer changes anything, so it stops acting as a visited mark -> infinite loop. Check this special case first.
- Off-grid neighbours of a border cell count as "water" for perimeter-type counting — bounds-check *before* indexing, never after (Python raises `IndexError` immediately on a negative-index wraparound surprise, or worse, silently accepts `grid[-1][x]` as a valid Python negative index into the *last* row — a bug C cannot even have the same way).
- Returning a size from the recursion (Max Area) is correct; accumulating into a shared variable is how island sizes get mixed together.

---

## 2. Grid BFS and shortest paths in unweighted graphs

### The idea

BFS is the **mandatory** choice whenever the problem asks for a shortest path or minimum number of steps in an unweighted graph. DFS would find *some* path, not the shortest. BFS processes nodes via a queue in exact order of distance from the source, so when a node is **first** enqueued its distance is already final.

### The invariant

At every moment the queue contains nodes of at most two consecutive distances `d, d, d, d+1, d+1` — never `d+2` before all `d+1` are there. This is what makes "first time seen = shortest distance" true, and it's the property Dijkstra generalises with a heap (Section 6).

### On a grid

Identical neighbour generation to DFS, but a `deque` instead of a stack, and each cell stores its distance from the source the moment it is first discovered.

### Multi-source BFS

When several start points are active *simultaneously* (all rotten oranges, all zeros in a 0/1 matrix), push **all** sources into the queue at distance 0 before the first round. This is one BFS, O(rows*cols), versus one BFS per source. Conceptually it's BFS from a virtual super-source connected to every real source by a zero-length edge. The resulting `dist` grid *is* a distance transform.

### Generalised BFS

BFS is not about grids — it's about any state space with unit-cost transitions and a "fewest steps" goal:

| Problem | Node (state) | Neighbours | State count |
|---|---|---|---|
| Word Ladder | a word | change one letter to any of 26, keep if in dictionary | n words |
| Open the Lock | 4-digit string | turn one of 4 wheels +-1 | 10^4 |
| Visit-all-nodes | `(node, visited_bitmask)` | move to adjacent node, OR its bit into the mask | n * 2^n |

The key question is always "what is the *complete* state?" — if the same node can be reached with different useful histories, the history goes into the state (as a tuple), and `visited` becomes a `set` of tuples.

### Python layout

```python
from collections import deque

def bfs_grid(grid: list[list[int]], sources: list[tuple[int, int]]) -> list[list[int]]:
    """Multi-source BFS. grid[r][c] == 1 is a wall.
    dist[r][c] == -1 for walls/unreached, else steps from the nearest source."""
    rows, cols = len(grid), len(grid[0])
    dist = [[-1] * cols for _ in range(rows)]
    q: deque[tuple[int, int]] = deque()
    for r, c in sources:
        dist[r][c] = 0
        q.append((r, c))                             # all sources start at d=0
    while q:
        r, c = q.popleft()
        for dr, dc in DIRS:
            nr, nc = r + dr, c + dc
            if not (0 <= nr < rows and 0 <= nc < cols):
                continue
            if grid[nr][nc] == 1 or dist[nr][nc] != -1:   # wall or already reached
                continue
            dist[nr][nc] = dist[r][c] + 1                  # mark ON ENQUEUE
            q.append((nr, nc))
    return dist
```

If you need "how many rounds until everything is reached" (Rotting Oranges), the answer is `max` of `dist` over reached cells; if some fresh cell still has `dist == -1`, the answer is `-1`. Level-by-level processing (snapshot `len(q)` before draining it) gives explicit rounds when you need to do something per level — the same `level_size` idiom as Chapter 05's tree BFS.

### Worked example (multi-source, 01 Matrix)

```
input        dist after BFS
1 1 1        2 1 2
1 0 1   ->   1 0 1
1 1 1        2 1 2
```

Queue starts `[(1,1)]` at d=0. Pop it: neighbours (0,1),(2,1),(1,0),(1,2) all get d=1 and are enqueued. Pop (0,1): (0,0),(0,2) get d=2. Pop (2,1): (2,0),(2,2) get d=2. Pop (1,0): (0,0),(2,0) already set — skipped. Nine cells, nine enqueues.

### Complexity

O(V + E): each node enqueued once, each edge examined once. On a grid V = rows*cols, E <= 4*V -> O(rows*cols). For an abstract state space it's O(states * branching), e.g. O(n * L * 26) for Word Ladder, O(n * 2^n * n) for bitmask BFS.

### Pitfalls

- Marking visited on **dequeue** instead of enqueue: still correct, but the queue can hold the same cell many times, costing extra time. Mark on enqueue.
- Forgetting to check that source and target are themselves passable before starting (Shortest Path in Binary Matrix) — if either is blocked the answer is `-1` immediately.
- Excluding the start from goal candidates when the problem demands it (Nearest Exit: the entrance is on the border but is not an exit).
- Running a separate BFS per source instead of one multi-source BFS — wrong *and* slow for "simultaneous spreading" semantics.
- Word-ladder-type: generating neighbours by trying all 26 letters per position and checking a `set` is O(L*26) per word; comparing every pair of words is O(n^2*L). Remove a word from the set when discovered — that's your visited mark.
- `deque.popleft()` is O(1); `list.pop(0)` is O(n) — never use a plain `list` as a BFS queue.

---

## 3. Connected components on general graphs

### The idea

A connected component is a maximal set of nodes each reachable from every other by some path. Counting components is always the same formula: loop over **all** nodes in an outer loop, and each time you meet an unvisited node, launch a DFS or BFS that marks its whole component. The number of launches is the number of components.

### Why the outer loop is mandatory

One search from one node finds only that node's component. If the graph is not guaranteed connected, other nodes stay unvisited unless the outer loop reaches them. This is the single most common bug in this unit: "I ran DFS from node 0 and got 1 component."

### DFS or BFS?

Either gives the same result — the question is reachability, not distance. DFS is shorter as recursion; BFS avoids Python's recursion limit on deep graphs (a path graph of 10^5 nodes recurses 10^5 deep and will raise `RecursionError`). Prefer an explicit stack or `deque`-BFS when input size is unknown.

### Recognising the pattern in disguise

| Question | What it is |
|---|---|
| "Is there a path from A to B?" | one search from A; is B marked? |
| "How many groups / provinces / islands?" | component count |
| "Can you reach every room from room 0?" | one search; is `len(visited) == n`? |
| "Merge records that share a field" (Accounts Merge) | build implicit edges between shared fields, collect each component |
| "Can the nodes be 2-coloured?" (Bipartite) | search carrying a colour; conflict = not bipartite |
| "Copy this graph" (Clone Graph) | search + a `dict` original -> copy |
| "a/b = k, what is c/d?" (Evaluate Division) | search accumulating an edge-weight product |

The search body is what changes: the "visited" side effect can carry a colour, a running product, a reference to a copied node, or a list of members.

### Python layout

```python
def components(adj: dict[int, list[int]], n: int) -> tuple[int, dict[int, int]]:
    """Labels every node with its component id; returns (count, comp)."""
    comp: dict[int, int] = {}
    ncomp = 0
    for s in range(n):                              # outer loop
        if s in comp:
            continue
        st = [s]
        comp[s] = ncomp                              # mark on push
        while st:
            u = st.pop()
            for v in adj.get(u, []):
                if v not in comp:
                    comp[v] = ncomp
                    st.append(v)
        ncomp += 1
    return ncomp, comp
```

`comp` doubles as the visited mark and the answer, exactly like the C `int comp[]` array — here a `dict` because node ids may not be a dense `0..n-1` range (e.g. after mapping strings to ids). Bipartite check is this loop with `color[v] = 1 - color[u]` on discovery and `color[v] == color[u]` as the conflict test on every already-coloured neighbour — including across the outer loop, since one component may be bipartite while another is not.

### Worked example

n = 6, edges `(0,1) (1,2) (3,4)`. Outer loop: s=0 unvisited -> comp 0, stack visits 0,1,2. s=1,2 skipped. s=3 -> comp 1: 3,4. s=4 skipped. s=5 -> comp 2 (isolated node, zero edges — still a component). Answer 3. Without the outer loop you'd report 1.

### Complexity

O(n + m) with an adjacency list: every node pushed once, every adjacency entry scanned once. O(n^2) with an adjacency matrix. Accounts Merge adds an O(k log k) sort per component.

### Pitfalls

- Source == target in a path query: trivially true; make sure the search handles it (marking the source before checking the target does).
- Clone Graph: insert `original -> copy` into the map **before** recursing into neighbours, else a cycle recurses forever.
- Evaluate Division: every equation `a/b = k` is *two* directed edges, `a->b` weight `k` and `b->a` weight `1/k`; forgetting the reverse edge makes queries one-directional. Query nodes not in the graph -> `-1.0`.
- Merging records by *name* instead of by shared field — two different people can share a name.
- Isolated nodes count as components.
- `networkx.connected_components` exists if you want the batch version, but write it yourself here — that's the point of the chapter.

---

## 4. Cycle detection and topological sort

### The idea

A topological order of a directed graph is an ordering of its nodes such that every edge points from earlier to later. It exists **exactly when** the graph has no directed cycle (it's a DAG). Topological sorting and cycle detection are one algorithm viewed from two sides: if the sort manages to place every node, there is no cycle; if some nodes can never be placed, they lie on (or downstream of) a cycle.

### Kahn's algorithm (BFS flavour)

1. Compute `indeg[v]` = number of edges pointing into `v`.
2. Enqueue every node with `indeg == 0`.
3. Pop `u`, append it to the order, and for each edge `u -> v` decrement `indeg[v]`; if it hits 0, enqueue `v`.
4. If the order has fewer than `n` nodes at the end, the leftovers form/depend on a cycle.

Using a min-heap (`heapq`) instead of a `deque` gives the lexicographically smallest topological order.

### DFS flavour (three colours)

Each node is **white** (unvisited), **grey** (on the current recursion path), or **black** (finished). If DFS reaches a **grey** node, that's a cycle. A plain two-state `visited` set is **not enough**: it cannot distinguish "on my current path" (cycle) from "already fully finished via another branch" (a DAG with two routes to the same node, perfectly fine). The topological order is the **post-order** reversed.

```
    0 -> 1 -> 3
    0 -> 2 -> 3         two routes to 3, no cycle. With 2 states, visiting 3 the second
                        time looks like a cycle. With 3 states it's black -> fine.
```

### Which to choose

| | Kahn | Three-colour DFS |
|---|---|---|
| Order of output | direct | post-order, then reverse |
| Cycle detection | count < n | grey hit |
| Recursion | none (deque loop) | depth up to n (RecursionError risk on skewed graphs) |
| Extras | lexicographic order via `heapq`; natural for "peel layers" (Minimum Height Trees) | natural for "is this node safe / memoise per node" (Eventual Safe States), Hierholzer |

### Python layout (Kahn)

```python
from collections import deque, defaultdict

def kahn(n: int, edges: list[tuple[int, int]]) -> list[int]:
    """Returns a topological order, or [] if a cycle exists."""
    adj: dict[int, list[int]] = defaultdict(list)
    indeg = [0] * n
    for u, v in edges:
        adj[u].append(v)
        indeg[v] += 1
    q = deque(u for u in range(n) if indeg[u] == 0)
    order: list[int] = []
    while q:
        u = q.popleft()
        order.append(u)
        for v in adj[u]:
            indeg[v] -= 1
            if indeg[v] == 0:
                q.append(v)
    return order if len(order) == n else []       # shorter than n => a cycle exists
```

Python layout (three-colour DFS):

```python
def topo_dfs(n: int, edges: list[tuple[int, int]]) -> list[int]:
    adj: dict[int, list[int]] = defaultdict(list)
    for u, v in edges:
        adj[u].append(v)
    color = [0] * n                                 # 0 white, 1 grey, 2 black
    order: list[int] = []
    has_cycle = False

    def dfs(u: int) -> None:
        nonlocal has_cycle
        color[u] = 1
        for v in adj[u]:
            if color[v] == 1:
                has_cycle = True                     # back edge -> cycle
                return
            if color[v] == 0:
                dfs(v)
                if has_cycle:
                    return
        color[u] = 2
        order.append(u)                              # post-order

    for s in range(n):
        if color[s] == 0:
            dfs(s)
        if has_cycle:
            return []
    return order[::-1]                               # reverse once at the end
```

The stdlib has this built in for the common case: `graphlib.TopologicalSorter(graph).static_order()` runs Kahn's algorithm and raises `graphlib.CycleError` on a cycle — reach for it when the problem is exactly "give me a valid order", and write your own when you need the count, the lexicographically smallest order, or DFS-specific extras (memoised safety, Hierholzer).

### Worked example (Kahn)

Courses: 0->1, 0->2, 1->3, 2->3, 3->4.

```
indeg      : [0, 1, 1, 2, 1]
queue      : [0]                 order: 0
pop 0      : indeg[1]=0,[2]=0    queue [1,2]
pop 1      : indeg[3]=1          order: 0 1
pop 2      : indeg[3]=0          order: 0 1 2   queue [3]
pop 3      : indeg[4]=0          order: 0 1 2 3 queue [4]
pop 4      :                     order: 0 1 2 3 4   -> 5 == n, acyclic
```

Add edge 4->1: `indeg[1]` starts at 2, never reaches 0 after pop 0; only node 0 is ever ordered -> `len(order) == 1 < 5` -> cycle.

### Variants in this unit

- **Transitive closure over a DAG** (Course Schedule IV): process nodes in topological order and give each node the *union* of its predecessors' ancestor sets plus the predecessors themselves — a `set` per node, unioned with `|=`, or a Python `int` used as a bitset (`ancestors[v] |= ancestors[u] | (1 << u)`) when `n` is small.
- **Eventual Safe States**: three-colour DFS with memoisation — a node is safe iff it's not grey and all successors are safe; cache the verdict (a `list[bool | None]`) so each node is resolved once.
- **Peeling leaves** (Minimum Height Trees): Kahn on an *undirected* tree, "in-degree" = degree, start from degree-1 nodes, stop when <= 2 remain — the centre(s).
- **Hierholzer** (Reconstruct Itinerary): consume edges one at a time (a `list` per node, sorted, popped from the end for O(1) removal); a node is appended to the result only once it has *no unused outgoing edges* (post-order), then reverse.

### Complexity

O(n + m) for both flavours. Sorting adjacency lists (lexicographic order, Hierholzer) adds O(m log m).

### Pitfalls

- Two-state visited in DFS cycle detection -> false positives on any DAG with a diamond.
- Forgetting to reverse the DFS post-order.
- Returning a partial order when a cycle exists; the problems want an empty result.
- Edge direction: "a depends on b" is `b -> a` (b first). Pick a convention and be consistent in both building and reading.
- Undirected cycle detection is a *different* problem (any back edge to a non-parent) — and is what union-find does best (Section 5).

---

## 5. Union-Find (disjoint set union)

### The idea

Union-find maintains a family of disjoint sets with two operations: `find(x)` returns the set's representative (root), and `union(x, y)` merges two sets. It is the right tool when **edges arrive as a stream** and you must repeatedly answer "are these two nodes in the same component?" or "how many components remain?" without rebuilding the whole graph per question.

### The two optimisations

Together they make each operation almost O(1) — O(alpha(n)) amortised, alpha being the inverse Ackermann function (<= 4 for any n you'll ever see):

1. **Path compression:** `find` flattens the path to the root — every node it passes is repointed directly to the root.
2. **Union by size (or rank):** always hang the smaller tree under the larger, never the other way.

Skipping both gives O(n) worst-case per op (a chain). Either one alone gives O(log n). Both: alpha(n).

### Recognising it

- component count while edges are being added;
- "how many edges can be removed / how many extra cables are there before the network falls apart";
- the first edge that closes a cycle in an undirected graph;
- Kruskal's MST: add edges in weight order, skip any whose endpoints are already connected — exactly the cycle check;
- "threshold" problems: process cells/edges sorted by some value and stop when two specific nodes become connected (Swim in Rising Water) — Kruskal in disguise;
- Equality constraints with transitivity (`a==b`, `b==c` => `a==c`): union all `==` first, then test every `!=`.

### Python layout

```python
class DSU:
    def __init__(self, n: int) -> None:
        self.parent = list(range(n))
        self.size = [1] * n
        self.count = n                              # number of live components

    def find(self, x: int) -> int:                  # iterative two-pass path compression
        root = x
        while self.parent[root] != root:
            root = self.parent[root]
        while self.parent[x] != root:
            self.parent[x], x = root, self.parent[x]
        return root

    def union(self, x: int, y: int) -> bool:
        """Returns True if a merge happened, False if x, y were already together."""
        rx, ry = self.find(x), self.find(y)
        if rx == ry:
            return False
        if self.size[rx] < self.size[ry]:
            rx, ry = ry, rx                          # rx is the bigger root
        self.parent[ry] = rx
        self.size[rx] += self.size[ry]
        self.count -= 1
        return True
```

Iterative `find` avoids recursion entirely, matching the C version — there is no reason to write it recursively here either (a chain of `n` unioned-in-order nodes without compression would recurse `n` deep). The return value of `union` is the whole point: `False` means "this edge closes a cycle / is redundant".

### Worked example (Redundant Connection)

n = 5, edges in order `(1,2) (2,3) (3,4) (1,4) (4,5)` (0-indexed internally: subtract 1, or allocate size `n+1` and ignore index 0).

```
after (1,2): parent [1,1,3,4,5]  size[1]=2   count 4
after (2,3): find(2)=1, find(3)=3 -> parent[3]=1, size[1]=3   count 3
after (3,4): find(3)=1, find(4)=4 -> parent[4]=1               count 2
try   (1,4): find(1)=1, find(4)=1 -> SAME ROOT -> redundant edge = (1,4)
```

`(4,5)` is never looked at; the answer is the *last* edge in input order that closes a cycle — process in the given order, do **not** sort.

### Kruskal (Min Cost to Connect All Points)

Generate all edges (all n(n-1)/2 point pairs, weight = Manhattan distance), `sorted(edges, key=lambda e: e[2])`, walk them: `if dsu.union(u, v): total += w; accepted += 1`; stop when `accepted == n - 1`. Greedy is correct *only because* union-find refuses cycle-closing edges. O(n^2 log n), dominated by the sort.

### Complexity

O((n + m) alpha(n)) for m operations. Sorting edges for Kruskal: O(m log m). Space O(n).

### Pitfalls

- Doing the `!=` checks before *all* `==` unions are done — transitive equalities not yet visible, contradiction missed.
- Forgetting the size/rank swap -> still correct, but O(log n) instead of alpha(n); forgetting compression too -> O(n) chains.
- Network Connected: check `m >= n-1` first (else `-1`); the answer is `components - 1`.
- Most Stones Removed: unioning stone pairs directly is O(n^2); union each stone's row-node with its column-node instead (offset the column ids by `n_rows`) — answer `n - components`, counting only components with at least one stone.
- Swim in Rising Water: the answer is at least `grid[0][0]` — the start cell itself is a lower bound.
- There is no DSU in the stdlib — you write these ~15 lines yourself, same as in C.

---

## 6. Dijkstra and weighted shortest paths

### The idea

When edges have non-negative weights, BFS is no longer enough — you need Dijkstra. Keep a priority queue (`heapq`) of nodes keyed by their best known distance so far, and each round extract the node with the **smallest** distance. When a node is popped for the first time, its distance is final — the same guarantee BFS gives, but now "distance" is a sum of weights, so a priority queue rather than a FIFO must pick the next node.

### Why non-negative weights are required

The finality argument: any route discovered later to the popped node `u` would pass through some node `x` still in the heap with `dist[x] >= dist[u]`, then add non-negative weights — so it cannot be shorter. A negative edge breaks this directly. With negative edges you need Bellman-Ford (or, with a hop limit, bounded Bellman-Ford below).

### Relaxation — the core operation

For edge `(u, v, w)`: if `dist[u] + w < dist[v]`, set `dist[v] = dist[u] + w` and push `(dist[v], v)` onto the heap. You do **not** delete the older, now-stale heap entry for `v`; instead, when popping `(d, v)`, check `d > dist[v]` — if so, skip it. This is **lazy deletion**, and it means the heap can hold up to O(m) entries.

### Complexity

O((n + m) log n) with a binary heap. Grid: O(rows*cols * log(rows*cols)). Running from every source (Find the City): O(n (n+m) log n), versus Floyd-Warshall's O(n^3) — better for sparse, worse for dense small graphs.

### Variants change only "what is best" and "how to combine"

| Problem | dist meaning | relax rule | heap order |
|---|---|---|---|
| Network Delay Time | sum of weights | `d[u] + w < d[v]` | min |
| Path with Maximum Probability | product of probs | `d[u] * w > d[v]` | max (negate for `heapq`) |
| Path With Minimum Effort | max edge on path | `max(d[u], abs(h[u]-h[v])) < d[v]` | min |
| Number of Ways to Arrive | sum + count | `<`: copy count; `==`: add count | min |
| Cheapest Flights <= K stops | sum, state = `(node, stops)` | bounded Bellman-Ford, k+1 rounds | -- |
| Min Obstacle Removal | sum of 0/1 weights | 0-1 BFS with a deque | deque, no heap |

Greedy finality holds for the product variant because probabilities are in [0, 1] (multiplying never increases), and for the minimax variant because `max` is monotone.

### Python layout

```python
import heapq

def dijkstra(n: int, adj: dict[int, list[tuple[int, int]]], src: int) -> list[float]:
    """adj[u] = list of (v, weight). Non-negative weights."""
    dist = [float("inf")] * n
    dist[src] = 0
    heap: list[tuple[float, int]] = [(0, src)]      # heapq sorts tuples lexicographically
    while heap:
        d, u = heapq.heappop(heap)
        if d > dist[u]:                              # stale entry -> skip
            continue
        for v, w in adj.get(u, []):
            nd = d + w
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(heap, (nd, v))
    return dist
```

No overflow risk with `float("inf")` or plain Python ints — the C chapter's `long long` / `LLONG_MAX / 4` dance is unnecessary here; `float("inf") + w` behaves exactly as expected for any finite `w`.

### Worked example

Nodes 0..3, directed edges `0->1 (4), 0->2 (1), 2->1 (2), 1->3 (1), 2->3 (5)`, source 0.

```
dist = [0, inf, inf, inf]      heap: (0,0)
pop (0,0): relax 1->4, 2->1          dist [0,4,1,inf]   heap (1,2) (4,1)
pop (1,2): relax 1->3 (better), 3->6 dist [0,3,1,6]     heap (3,1) (4,1) (6,3)
pop (3,1): relax 3->4 (better)       dist [0,3,1,4]     heap (4,1) (4,3) (6,3)
pop (4,1): 4 > dist[1]=3 -> STALE, skip
pop (4,3): relax nothing             final
pop (6,3): stale, skip
```

Node 1 was pushed twice; the stale `(4,1)` did no harm. Answer `[0, 3, 1, 4]`.

### 0-1 BFS

When every weight is 0 or 1, replace the heap with a `deque`: push weight-0 neighbours to the **front** (`appendleft`), weight-1 neighbours to the **back** (`append`). The deque stays sorted by distance (it holds only values `d` and `d+1`), and every operation is O(1) -> O(V + E) total.

### Bounded Bellman-Ford (Cheapest Flights Within K Stops)

Plain Dijkstra **fails** here: it finalises a node the first time it pops, but the cheapest route within <= k stops may pass through a *more expensive* intermediate state that saves stops later. Two fixes: (1) Bellman-Ford limited to exactly `k+1` rounds over the edge list, each round relaxing from a **copy** of the previous round's distances (`prev = dist[:]`) so a round corresponds to exactly one more edge — O(k * m); (2) Dijkstra whose state is `(node, stops_used)`, so the same node may be processed again with a different stop count. When a constraint changes what "same state" means, put it into the state.

### Pitfalls

- Forgetting the stale check: still correct, but the same node is re-expanded for every stale entry — quadratic blow-up on dense graphs.
- `heapq` is a **min-heap only**. Max-probability needs either negated weights (`heapq.heappush(h, (-prob, v))`) or a custom comparable wrapper.
- Summing height differences in Minimum Effort -> answers a different question (total climb, not worst step).
- Number of Ways: on a strictly better distance, *replace* the count (don't add); on an equal distance, *add*. Keep counts modulo `10**9 + 7`.
- Find the City tiebreak: the larger city index wins.
- Plain BFS on a 0/1-weight grid — it ignores weights and is simply wrong.
- `heapq.heappush(h, (d, v))` — tuples compare lexicographically, so ties on `d` fall back to comparing `v`; if `v` is itself unorderable (e.g. a custom object with no `__lt__`), wrap it or add a tiebreak field.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Complexity |
|---|---|---|
| grid, "connected region", count islands, fill, "surrounded", "closed" | grid DFS (flood fill), border-first for boundary conditions | O(rows*cols) |
| "minimum steps", "shortest path", unweighted, "minutes until", "nearest" | BFS; multi-source if several starts | O(V+E) |
| "shortest transformation", lock, words, "visit all nodes" | BFS over a state space; state = everything that matters | O(states x branching) |
| "how many groups", "is there a path", "can you reach every", merge records | components: outer loop + DFS/BFS | O(n+m) |
| "two groups so that no edge...", "split into two teams" | bipartite 2-colouring DFS/BFS | O(n+m) |
| copy a graph, `a/b = k` queries | DFS/BFS with a `dict` / weight product | O(n+m), O(q(n+m)) |
| prerequisites, "can all be finished", order of tasks, build order | Kahn or 3-colour DFS topological sort (or `graphlib.TopologicalSorter`) | O(n+m) |
| "safe nodes", "no path leads to a cycle" | 3-colour DFS with memo | O(n+m) |
| "peel leaves", "centre of a tree", minimum height | Kahn on an undirected tree | O(n) |
| use every edge once, itinerary | Hierholzer (post-order + reverse) | O(m log m) |
| edges arrive over time, "redundant edge", "extra cables", `==`/`!=` constraints | union-find | O((n+m) alpha(n)) |
| minimum cost to connect all, sorted by weight/height/threshold | Kruskal (sort + union-find) | O(m log m) |
| weighted, non-negative, shortest / cheapest / fastest | Dijkstra with lazy deletion | O((n+m) log n) |
| maximise probability / minimise worst step / count shortest paths | Dijkstra with a changed relax rule | O((n+m) log n) |
| weights only 0 and 1 | 0-1 BFS with a deque | O(V+E) |
| "at most k stops / edges" | bounded Bellman-Ford or `(node, k)` state | O(k*m) |
| all-pairs, small n, threshold distance | n x Dijkstra, or Floyd-Warshall | O(n(n+m) log n) / O(n^3) |

---

## Gotchas in Python specifically

- **Recursion limit ~1000.** A million-deep flood fill or a 10^5-node DFS on a path graph raises `RecursionError` well before it would trouble C's 8 MB stack. Convert to an explicit `list`-as-stack, or BFS with `deque`, whenever the input could be large or adversarially shaped (a straight-line "path" graph is the worst case for DFS recursion depth).
- **`list.pop(0)` / `insert(0, x)` are O(n).** Never use a plain `list` as a BFS queue or a 0-1 BFS deque's front — `collections.deque` gives O(1) at both ends.
- **`heapq` is a min-heap only.** Negate values for a max-heap (max-probability Dijkstra); ties between equal keys fall back to comparing the next tuple element, so make sure that element is comparable or add an explicit tiebreak (e.g. insertion order) if it might not be.
- **Mutable default arguments.** `def dfs(u, visited={})` keeps state across calls — always default to `None` and create fresh inside, or thread the state explicitly through parameters like the examples above do.
- **Dict iteration and mutation.** Never add or remove keys from a `dict` you're iterating (`for v in adj:` while inserting into `adj`) — `RuntimeError: dictionary changed size during iteration`. Iterate over a snapshot (`list(adj)`) if you must mutate.
- **String/tuple states in BFS need a `set`, not a `list`, for `visited`.** `state in visited_list` is O(n) per check; `state in visited_set` is O(1) amortised. This matters enormously for Word Ladder / Open the Lock scale state spaces.
- **No overflow, but no silent wraparound either.** `float("inf") + w` and big-int sums are exact; the C chapter's `long long`/`INF = LLONG_MAX/4` dance simply is not needed. The one real trap is mixing `float("inf")` with modular arithmetic (`% mod`) — infinity has no valid modulus, so keep sentinel and real distances in separate comparisons.
- **`graphlib.TopologicalSorter`** exists in the stdlib (Python 3.9+) and is the right tool when you only need *a* valid order or cycle detection, not the specific Kahn/DFS mechanics the problem is testing.
- **`==` vs `is` for graph nodes.** When nodes are custom objects (Clone Graph's `Node` class) rather than plain ints, use `is`/identity in a `dict` key context carefully — Python `dict`/`set` hash objects by `id()` by default unless `__hash__`/`__eq__` are overridden, which is usually what you want for "same object" checks, but surprises when two structurally-equal-but-distinct nodes get treated as different keys (or vice versa if you did override `__eq__`).
- **Tuple-as-state hashing.** `(node, stops)`, `(r, c)`, `(node, bitmask)` all hash and compare correctly out of the box as `dict`/`set` keys — no manual struct-equality code needed, unlike C where you'd write your own hash/equality for a composite key.
- **Sentinel visited values.** `dist[i] = -1` as "unvisited" is fine and idiomatic; just remember it collapses two arrays into one, so it only works when a genuine distance of `-1` can never occur (true for BFS step counts, false for signed-weight graphs).

---

## Common mistakes checklist

- [ ] Grid DFS: all three checks (bounds, visited, target) present, bounds check *first*.
- [ ] Visited marked on discovery (push/enqueue), not on pop.
- [ ] Outer loop over all nodes/cells when counting components — one search finds one component.
- [ ] Border-first pass before counting "closed" / "surrounded" regions; reverse inequality when searching from the sink.
- [ ] BFS, not DFS, whenever "shortest"/"minimum steps" appears; multi-source when spread is simultaneous.
- [ ] State-space BFS: is the state complete? (bitmask, stops used, ...) — `visited` is a `set` of the full state tuple.
- [ ] Start/target passable? Start excluded from goals when the problem says so? Source == target handled?
- [ ] Cycle detection in a *directed* graph uses three colours, not two (or `graphlib.TopologicalSorter` + catch `CycleError`).
- [ ] DFS topological order reversed; empty output on cycle.
- [ ] Clone/DFS-with-map: insert into the map *before* recursing into neighbours.
- [ ] Union-find: path compression **and** union by size; process edges in the order the problem specifies; `==` unions before `!=` checks.
- [ ] Dijkstra: stale-entry check on pop; relax rule matches the objective (sum / max / product / count); `heapq` negated for max-heap use.
- [ ] Non-negative weights confirmed before using Dijkstra; hop-limit -> bounded Bellman-Ford or `(node, stops)` state.
- [ ] Ties and special cases from the statement (largest index wins, answer >= `grid[0][0]`, no fresh oranges -> 0).
- [ ] `deque` used for every queue; `set`/`dict` used for every visited-mark on non-small-int states; recursion depth considered for skewed inputs.

---

## You can move on when...

- You can write flood fill and multi-source grid BFS from memory in Python, with `deque` and tuple coordinates, and explain in one sentence why BFS's first discovery is the shortest distance.
- Given an edge list, you can build a `defaultdict(list)` adjacency structure without looking anything up, and explain how it maps onto `scipy.sparse.csr_matrix`.
- You can state the difference between two-state and three-state DFS cycle detection and draw the diamond DAG that breaks the two-state version.
- You can implement `find` with path compression iteratively and `union` by size, and explain what the return value of `union` tells you.
- You can write Dijkstra with `heapq` and lazy deletion, then modify the relax rule to minimise the maximum edge on the path — without restructuring anything else.
- You have solved at least the level-2 and level-3 problems of every unit in `problems.md` in Python, with your own `assert` tests, and know which of them are BFS-in-disguise, components-in-disguise, and Kruskal-in-disguise.
