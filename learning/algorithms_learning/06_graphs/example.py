"""Chapter 06 — Graphs. Python port of example.c: same skeletons, same tiny neutral inputs.
Run: python3 example.py
"""

from collections import deque, defaultdict
import heapq

# ------------------------------------------------------------------------
# 0. Graph representation
# ------------------------------------------------------------------------

def graph_from_edges(n, edges):
    adj = defaultdict(list)
    for u, v in edges:
        adj[u].append(v)
        adj[v].append(u)
    return adj

# ------------------------------------------------------------------------
# 1. Grid DFS / flood fill
# ------------------------------------------------------------------------

DIRS = [(-1, 0), (1, 0), (0, -1), (0, 1)]


def flood(grid, r, c):
    rows, cols = len(grid), len(grid[0])
    if not (0 <= r < rows and 0 <= c < cols):
        return 0
    if grid[r][c] != "1":
        return 0
    grid[r][c] = "0"
    size = 1
    for dr, dc in DIRS:
        size += flood(grid, r + dr, c + dc)
    return size


def count_islands(grid):
    count = 0
    for r in range(len(grid)):
        for c in range(len(grid[0])):
            if grid[r][c] == "1":
                count += 1
                flood(grid, r, c)
    return count

# ------------------------------------------------------------------------
# 2. Grid BFS shortest path (multi-source)
# ------------------------------------------------------------------------

def bfs_grid(grid, sources):
    rows, cols = len(grid), len(grid[0])
    dist = [[-1] * cols for _ in range(rows)]
    q = deque()
    for r, c in sources:
        dist[r][c] = 0
        q.append((r, c))
    while q:
        r, c = q.popleft()
        for dr, dc in DIRS:
            nr, nc = r + dr, c + dc
            if not (0 <= nr < rows and 0 <= nc < cols):
                continue
            if grid[nr][nc] == 1 or dist[nr][nc] != -1:
                continue
            dist[nr][nc] = dist[r][c] + 1
            q.append((nr, nc))
    return dist

# ------------------------------------------------------------------------
# 3. Connected components
# ------------------------------------------------------------------------

def components(adj, n):
    comp = {}
    ncomp = 0
    for s in range(n):
        if s in comp:
            continue
        st = [s]
        comp[s] = ncomp
        while st:
            u = st.pop()
            for v in adj.get(u, []):
                if v not in comp:
                    comp[v] = ncomp
                    st.append(v)
        ncomp += 1
    return ncomp, comp


def is_bipartite(adj, n):
    color = {}
    for s in range(n):
        if s in color:
            continue
        color[s] = 0
        st = [s]
        while st:
            u = st.pop()
            for v in adj.get(u, []):
                if v not in color:
                    color[v] = 1 - color[u]
                    st.append(v)
                elif color[v] == color[u]:
                    return False
    return True

# ------------------------------------------------------------------------
# 4. Cycle detection and topological sort
# ------------------------------------------------------------------------

def kahn(n, edges):
    adj = defaultdict(list)
    indeg = [0] * n
    for u, v in edges:
        adj[u].append(v)
        indeg[v] += 1
    q = deque(u for u in range(n) if indeg[u] == 0)
    order = []
    while q:
        u = q.popleft()
        order.append(u)
        for v in adj[u]:
            indeg[v] -= 1
            if indeg[v] == 0:
                q.append(v)
    return order if len(order) == n else []


def topo_dfs(n, edges):
    adj = defaultdict(list)
    for u, v in edges:
        adj[u].append(v)
    color = [0] * n
    order = []
    has_cycle = [False]

    def dfs(u):
        color[u] = 1
        for v in adj[u]:
            if color[v] == 1:
                has_cycle[0] = True
                return
            if color[v] == 0:
                dfs(v)
                if has_cycle[0]:
                    return
        color[u] = 2
        order.append(u)

    for s in range(n):
        if color[s] == 0:
            dfs(s)
        if has_cycle[0]:
            return []
    return order[::-1]

# ------------------------------------------------------------------------
# 5. Union-Find
# ------------------------------------------------------------------------

class DSU:
    def __init__(self, n):
        self.parent = list(range(n))
        self.size = [1] * n
        self.count = n

    def find(self, x):
        root = x
        while self.parent[root] != root:
            root = self.parent[root]
        while self.parent[x] != root:
            self.parent[x], x = root, self.parent[x]
        return root

    def union(self, x, y):
        rx, ry = self.find(x), self.find(y)
        if rx == ry:
            return False
        if self.size[rx] < self.size[ry]:
            rx, ry = ry, rx
        self.parent[ry] = rx
        self.size[rx] += self.size[ry]
        self.count -= 1
        return True


def find_redundant_connection(edges, n):
    """1-indexed nodes 1..n; returns the last edge (in input order) that closes a cycle."""
    dsu = DSU(n + 1)
    redundant = None
    for u, v in edges:
        if not dsu.union(u, v):
            redundant = (u, v)
    return redundant

# ------------------------------------------------------------------------
# 6. Dijkstra with a binary heap (heapq) and lazy deletion
# ------------------------------------------------------------------------

def dijkstra(n, adj, src):
    dist = [float("inf")] * n
    dist[src] = 0
    heap = [(0, src)]
    while heap:
        d, u = heapq.heappop(heap)
        if d > dist[u]:
            continue
        for v, w in adj.get(u, []):
            nd = d + w
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(heap, (nd, v))
    return dist


def dijkstra_minimax(n, adj, src):
    """Minimises the largest edge weight on the path (Path With Minimum Effort style)."""
    dist = [float("inf")] * n
    dist[src] = 0
    heap = [(0, src)]
    while heap:
        d, u = heapq.heappop(heap)
        if d > dist[u]:
            continue
        for v, w in adj.get(u, []):
            nd = max(d, w)
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(heap, (nd, v))
    return dist

# ------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------

def main():
    # 0. representation
    adj0 = graph_from_edges(4, [(0, 1), (0, 2), (1, 2), (2, 3)])
    assert sorted(adj0[2]) == [0, 1, 3]

    # 1. grid DFS / flood fill
    grid = [list(row) for row in ["11..", ".1.1", "...1"]]
    assert count_islands(grid) == 2
    assert all(cell != "1" for row in grid for cell in row)   # every '1' sunk to '0'

    # 2. multi-source BFS (01 Matrix)
    mat = [[1, 1, 1], [1, 0, 1], [1, 1, 1]]
    sources = [(r, c) for r in range(3) for c in range(3) if mat[r][c] == 0]
    no_walls = [[0, 0, 0], [0, 0, 0], [0, 0, 0]]      # every cell passable; only `sources` seed dist=0
    dist = bfs_grid(no_walls, sources)
    assert dist == [[2, 1, 2], [1, 0, 1], [2, 1, 2]]

    # 3. connected components + bipartite
    adj3 = graph_from_edges(6, [(0, 1), (1, 2), (3, 4)])
    ncomp, comp = components(adj3, 6)
    assert ncomp == 3
    assert comp[0] == comp[1] == comp[2]
    assert comp[3] == comp[4]
    assert comp[5] not in (comp[0], comp[3])

    tri_adj = graph_from_edges(3, [(0, 1), (1, 2), (2, 0)])       # odd cycle
    assert not is_bipartite(tri_adj, 3)
    square_adj = graph_from_edges(4, [(0, 1), (1, 2), (2, 3), (3, 0)])  # even cycle
    assert is_bipartite(square_adj, 4)

    # 4. cycle detection / topological sort
    course_edges = [(0, 1), (0, 2), (1, 3), (2, 3), (3, 4)]
    order_kahn = kahn(5, course_edges)
    assert order_kahn[0] == 0 and order_kahn[-1] == 4 and len(order_kahn) == 5
    order_dfs = topo_dfs(5, course_edges)
    assert len(order_dfs) == 5 and order_dfs[0] == 0

    cyclic_edges = course_edges + [(4, 1)]
    assert kahn(5, cyclic_edges) == []
    assert topo_dfs(5, cyclic_edges) == []

    def valid_topo_order(order, edges):
        pos = {u: i for i, u in enumerate(order)}
        return all(pos[u] < pos[v] for u, v in edges)

    assert valid_topo_order(order_kahn, course_edges)
    assert valid_topo_order(order_dfs, course_edges)

    # 5. union-find
    redundant = find_redundant_connection([(1, 2), (2, 3), (3, 4), (1, 4), (4, 5)], 5)
    assert redundant == (1, 4)

    dsu = DSU(5)
    assert dsu.union(0, 1) and dsu.union(1, 2)
    assert dsu.find(0) == dsu.find(2)
    assert not dsu.union(0, 2)                     # already connected
    assert dsu.count == 3                          # {0,1,2}, {3}, {4}

    # 6. Dijkstra
    dij_adj = defaultdict(list)
    for u, v, w in [(0, 1, 4), (0, 2, 1), (2, 1, 2), (1, 3, 1), (2, 3, 5)]:
        dij_adj[u].append((v, w))
    dists = dijkstra(4, dij_adj, 0)
    assert dists == [0, 3, 1, 4]

    minimax_adj = defaultdict(list)
    for u, v, w in [(0, 1, 4), (0, 2, 1), (2, 1, 2), (1, 3, 1), (2, 3, 5)]:
        minimax_adj[u].append((v, w))
        minimax_adj[v].append((u, w))
    mdist = dijkstra_minimax(4, minimax_adj, 0)
    assert mdist[3] == 2                           # 0-2 (1) -2-1 (2) -1-3 (1): worst edge 2

    print("chapter 06 graphs: all asserts passed")


if __name__ == "__main__":
    main()
