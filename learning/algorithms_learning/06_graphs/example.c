/*
 * Chapter 06 — Graphs: pattern skeletons on tiny neutral inputs.
 *
 * Compile + run:
 *     cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * What is demonstrated (one section per unit of lesson.md):
 *   0. Adjacency list built from an edge list with per-node dynamic arrays,
 *      plus a CSR (compressed sparse row) version of the same graph.
 *   1. Grid DFS / flood fill with an explicit stack (no recursion depth risk).
 *   2. Grid BFS shortest path with a ring-buffer queue, multi-source variant.
 *   3. Connected components on a general graph (outer loop + iterative DFS).
 *   4. Cycle detection + topological sort: Kahn's algorithm and 3-colour DFS.
 *   5. Union-Find with path compression and union by size (Kruskal-style use).
 *   6. Dijkstra with a binary min-heap and lazy deletion; a minimax variant.
 *
 * These are NOT solutions to the LeetCode problems in problems.md — they are the
 * reusable building blocks you will assemble into those solutions.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* 0. Graph representations                                                   */
/* ------------------------------------------------------------------------- */

/* (a) Per-node dynamic arrays: good when edges arrive one at a time.
 *     Each node owns a growable int array of neighbour ids (Chapter 10's Vec). */
typedef struct { int *data; int len, cap; } IntVec;

static void vec_push(IntVec *v, int x) {
    if (v->len == v->cap) {
        int ncap = v->cap ? 2 * v->cap : 4;
        int *t = realloc(v->data, (size_t)ncap * sizeof *t);
        if (!t) { perror("realloc"); exit(1); }
        v->data = t; v->cap = ncap;
    }
    v->data[v->len++] = x;
}

typedef struct { int n; IntVec *adj; } ListGraph;

static ListGraph lg_new(int n) {
    ListGraph g = { n, calloc((size_t)n, sizeof(IntVec)) };   /* calloc → data=NULL,len=cap=0 */
    return g;
}
static void lg_add_undirected(ListGraph *g, int u, int v) { vec_push(&g->adj[u], v); vec_push(&g->adj[v], u); }
static void lg_add_directed(ListGraph *g, int u, int v)   { vec_push(&g->adj[u], v); }
static void lg_free(ListGraph *g) {
    for (int i = 0; i < g->n; i++) free(g->adj[i].data);
    free(g->adj);
}

/* (b) CSR: two flat arrays. off[u]..off[u+1] is the half-open neighbour range of u.
 *     Built in two passes from an edge list: count degrees, prefix-sum, fill.
 *     Contiguous memory → the layout scipy.sparse and GNN libraries use. */
typedef struct { int n, nnz; int *off, *adj, *w; } CSR;

static CSR csr_from_edges(int n, int m, const int (*e)[3], int directed) {
    CSR g; g.n = n; g.nnz = directed ? m : 2 * m;
    g.off = calloc((size_t)n + 1, sizeof *g.off);
    g.adj = malloc((size_t)g.nnz * sizeof *g.adj);
    g.w   = malloc((size_t)g.nnz * sizeof *g.w);
    for (int i = 0; i < m; i++) { g.off[e[i][0] + 1]++; if (!directed) g.off[e[i][1] + 1]++; }
    for (int u = 0; u < n; u++) g.off[u + 1] += g.off[u];          /* prefix sum = row starts */
    int *fill = malloc(((size_t)n + 1) * sizeof *fill);
    memcpy(fill, g.off, ((size_t)n + 1) * sizeof *fill);
    for (int i = 0; i < m; i++) {
        int u = e[i][0], v = e[i][1], w = e[i][2];
        g.adj[fill[u]] = v; g.w[fill[u]++] = w;
        if (!directed) { g.adj[fill[v]] = u; g.w[fill[v]++] = w; }
    }
    free(fill);
    return g;
}
static void csr_free(CSR *g) { free(g->off); free(g->adj); free(g->w); }

static void demo_representations(void) {
    puts("== 0. Graph representations ==");
    /* Undirected, unweighted 5-node graph:  0-1, 0-2, 1-2, 2-3   (4 isolated) */
    int edges[][3] = { {0,1,0}, {0,2,0}, {1,2,0}, {2,3,0} };
    int m = (int)(sizeof edges / sizeof edges[0]);

    ListGraph lg = lg_new(5);
    for (int i = 0; i < m; i++) lg_add_undirected(&lg, edges[i][0], edges[i][1]);
    puts("per-node dynamic arrays:");
    for (int u = 0; u < lg.n; u++) {
        printf("  %d:", u);
        for (int k = 0; k < lg.adj[u].len; k++) printf(" %d", lg.adj[u].data[k]);
        putchar('\n');
    }
    lg_free(&lg);

    /* Directed version: only the out-neighbours are stored (edges arriving one by one). */
    ListGraph dg = lg_new(3);
    lg_add_directed(&dg, 0, 1); lg_add_directed(&dg, 0, 2); lg_add_directed(&dg, 2, 1);
    printf("directed out-lists: 0->{");
    for (int k = 0; k < dg.adj[0].len; k++) printf("%d%s", dg.adj[0].data[k], k + 1 < dg.adj[0].len ? "," : "");
    printf("}  1->{}  2->{%d}\n", dg.adj[2].data[0]);
    lg_free(&dg);

    CSR g = csr_from_edges(5, m, edges, 0);
    printf("CSR off: [");
    for (int u = 0; u <= g.n; u++) printf("%d%s", g.off[u], u < g.n ? " " : "]\n");
    printf("CSR adj: [");
    for (int k = 0; k < g.nnz; k++) printf("%d%s", g.adj[k], k + 1 < g.nnz ? " " : "]\n");
    printf("neighbours of 2 via CSR range [%d,%d):", g.off[2], g.off[3]);
    for (int k = g.off[2]; k < g.off[3]; k++) printf(" %d", g.adj[k]);
    putchar('\n');
    csr_free(&g);
}

/* ------------------------------------------------------------------------- */
/* 1. Grid DFS / flood fill                                                   */
/* ------------------------------------------------------------------------- */

/* Shared direction table: up, down, left, right. static const → one copy, foldable. */
static const int DY[4] = { -1, 1, 0, 0 };
static const int DX[4] = { 0, 0, -1, 1 };

/* Flood fill from (sy,sx): every 4-connected cell holding `from` becomes `to`.
 * Uses an explicit stack of packed indices (y*m + x) — no recursion, so a
 * 1000x1000 all-one-colour grid cannot overflow the call stack.
 * Returns the number of recoloured cells. */
static int flood_fill(char *grid, int n, int m, int sy, int sx, char from, char to) {
    if (from == to) return 0;                    /* pitfall: recolouring would never "mark" anything */
    if (grid[sy * m + sx] != from) return 0;
    int *stack = malloc((size_t)n * m * sizeof *stack), top = 0, filled = 0;
    grid[sy * m + sx] = to;                      /* mark ON PUSH, before anything else sees it */
    stack[top++] = sy * m + sx;
    while (top > 0) {
        int cur = stack[--top], y = cur / m, x = cur % m;
        filled++;
        for (int d = 0; d < 4; d++) {
            int ny = y + DY[d], nx = x + DX[d];
            if (ny < 0 || ny >= n || nx < 0 || nx >= m) continue;   /* check 1: bounds (FIRST) */
            if (grid[ny * m + nx] != from) continue;                /* checks 2+3: target & unvisited */
            grid[ny * m + nx] = to;                                 /* mark now */
            stack[top++] = ny * m + nx;
        }
    }
    free(stack);
    return filled;
}

/* Component count on a grid = outer loop over all cells + one flood per new region. */
static int count_regions(char *grid, int n, int m, char land, char sunk) {
    int count = 0;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < m; x++)
            if (grid[y * m + x] == land) { count++; flood_fill(grid, n, m, y, x, land, sunk); }
    return count;
}

static void print_grid(const char *grid, int n, int m) {
    for (int y = 0; y < n; y++) printf("  %.*s\n", m, grid + y * m);
}

static void demo_grid_dfs(void) {
    puts("\n== 1. Grid DFS / flood fill ==");
    /* 3x5 grid (non-square on purpose: catches n/m swaps). */
    char grid[] = "##..#"
                  ".#..#"
                  "....#";
    int n = 3, m = 5;
    puts("input:"); print_grid(grid, n, m);
    char work[sizeof grid]; memcpy(work, grid, sizeof grid);
    int sz = flood_fill(work, n, m, 0, 0, '#', 'A');
    printf("flood from (0,0) recoloured %d cells:\n", sz); print_grid(work, n, m);
    memcpy(work, grid, sizeof grid);
    printf("regions of '#': %d\n", count_regions(work, n, m, '#', '.'));
}

/* ------------------------------------------------------------------------- */
/* 2. Grid BFS shortest path (ring-buffer queue, multi-source)                */
/* ------------------------------------------------------------------------- */

/* A ring buffer queue of ints. For plain BFS a linear array of size n suffices
 * (each node enqueued once), but the ring buffer is the general tool — and the
 * same struct becomes a deque for 0-1 BFS if you add push_front. */
typedef struct { int *buf; int cap, head, len; } Queue;

static Queue q_new(int cap) { Queue q = { malloc((size_t)cap * sizeof(int)), cap, 0, 0 }; return q; }
static void  q_free(Queue *q) { free(q->buf); }
static int   q_empty(const Queue *q) { return q->len == 0; }
static void  q_push(Queue *q, int x) {                    /* caller guarantees len < cap */
    q->buf[(q->head + q->len) % q->cap] = x; q->len++;
}
static int   q_pop(Queue *q) {
    int x = q->buf[q->head]; q->head = (q->head + 1) % q->cap; q->len--; return x;
}

/* Multi-source BFS on an n x m grid. grid[i]=='#' is a wall.
 * dist[i] = -1 for walls / unreachable, else steps from the nearest source.
 * dist doubles as the visited array. */
static void bfs_grid(const char *grid, int n, int m, const int *sources, int ns, int *dist) {
    for (int i = 0; i < n * m; i++) dist[i] = -1;
    Queue q = q_new(n * m);                                   /* each cell enqueued at most once */
    for (int s = 0; s < ns; s++) { dist[sources[s]] = 0; q_push(&q, sources[s]); }  /* all at d=0 */
    while (!q_empty(&q)) {
        int cur = q_pop(&q), y = cur / m, x = cur % m;
        for (int d = 0; d < 4; d++) {
            int ny = y + DY[d], nx = x + DX[d];
            if (ny < 0 || ny >= n || nx < 0 || nx >= m) continue;
            int nb = ny * m + nx;
            if (grid[nb] == '#' || dist[nb] != -1) continue;    /* wall or already discovered */
            dist[nb] = dist[cur] + 1;                            /* mark ON ENQUEUE → final */
            q_push(&q, nb);
        }
    }
    q_free(&q);
}

static void print_dist(const int *dist, int n, int m) {
    for (int y = 0; y < n; y++) {
        printf("  ");
        for (int x = 0; x < m; x++) {
            int d = dist[y * m + x];
            if (d < 0) printf(" ##"); else printf(" %2d", d);
        }
        putchar('\n');
    }
}

static void demo_grid_bfs(void) {
    puts("\n== 2. Grid BFS shortest path ==");
    const char grid[] = "....#"
                        ".##.#"
                        "....."
                        "#.#..";
    int n = 4, m = 5, dist[20];
    puts("maze ('#' = wall):"); print_grid(grid, n, m);

    int src = 0;                                          /* single source: (0,0) */
    bfs_grid(grid, n, m, &src, 1, dist);
    puts("single-source distances from (0,0):"); print_dist(dist, n, m);
    printf("shortest path (0,0)->(3,4): %d steps\n", dist[3 * m + 4]);

    int srcs[2] = { 0 * m + 3, 3 * m + 1 };               /* multi-source: (0,3) and (3,1) */
    bfs_grid(grid, n, m, srcs, 2, dist);
    puts("multi-source distances (distance transform) from (0,3) and (3,1):");
    print_dist(dist, n, m);
    int rounds = 0;
    for (int i = 0; i < n * m; i++) if (dist[i] > rounds) rounds = dist[i];
    printf("rounds until every open cell is reached: %d\n", rounds);
}

/* ------------------------------------------------------------------------- */
/* 3. Connected components on a general graph                                 */
/* ------------------------------------------------------------------------- */

/* Labels comp[u] with a component id in [0, k); returns k.
 * comp[] initialised to -1 is the visited mark; explicit stack = iterative DFS. */
static int components(const CSR *g, int *comp) {
    int *stack = malloc((size_t)g->n * sizeof *stack), k = 0;
    for (int i = 0; i < g->n; i++) comp[i] = -1;
    for (int s = 0; s < g->n; s++) {                     /* outer loop: not optional */
        if (comp[s] != -1) continue;
        int top = 0;
        stack[top++] = s; comp[s] = k;                    /* mark on push */
        while (top > 0) {
            int u = stack[--top];
            for (int e = g->off[u]; e < g->off[u + 1]; e++) {
                int v = g->adj[e];
                if (comp[v] == -1) { comp[v] = k; stack[top++] = v; }
            }
        }
        k++;
    }
    free(stack);
    return k;
}

/* 2-colouring: returns 1 if bipartite. Same loop, the "visited" mark carries a colour. */
static int bipartite(const CSR *g, int *color) {
    int *stack = malloc((size_t)g->n * sizeof *stack), ok = 1;
    for (int i = 0; i < g->n; i++) color[i] = -1;
    for (int s = 0; s < g->n && ok; s++) {
        if (color[s] != -1) continue;
        int top = 0; stack[top++] = s; color[s] = 0;
        while (top > 0 && ok) {
            int u = stack[--top];
            for (int e = g->off[u]; e < g->off[u + 1]; e++) {
                int v = g->adj[e];
                if (color[v] == -1) { color[v] = 1 - color[u]; stack[top++] = v; }
                else if (color[v] == color[u]) { ok = 0; break; }     /* same colour across an edge */
            }
        }
    }
    free(stack);
    return ok;
}

static void demo_components(void) {
    puts("\n== 3. Connected components ==");
    /* 7 nodes: 0-1-2 triangle, 3-4, 5-6 path; nothing else. */
    int edges[][3] = { {0,1,0}, {1,2,0}, {2,0,0}, {3,4,0}, {5,6,0} };
    CSR g = csr_from_edges(7, 5, edges, 0);
    int comp[7], color[7];
    int k = components(&g, comp);
    printf("components: %d   labels:", k);
    for (int u = 0; u < g.n; u++) printf(" %d:%d", u, comp[u]);
    putchar('\n');
    printf("path 0->2 exists: %s   path 0->4 exists: %s\n",
           comp[0] == comp[2] ? "yes" : "no", comp[0] == comp[4] ? "yes" : "no");
    printf("bipartite (has a triangle): %s\n", bipartite(&g, color) ? "yes" : "no");
    csr_free(&g);

    int edges2[][3] = { {0,1,0}, {1,2,0}, {2,3,0}, {3,0,0} };     /* 4-cycle: even → bipartite */
    CSR g2 = csr_from_edges(4, 4, edges2, 0);
    printf("bipartite (4-cycle): %s   colours:", bipartite(&g2, color) ? "yes" : "no");
    for (int u = 0; u < g2.n; u++) printf(" %d", color[u]);
    putchar('\n');
    csr_free(&g2);
}

/* ------------------------------------------------------------------------- */
/* 4. Cycle detection and topological sort                                    */
/* ------------------------------------------------------------------------- */

/* Kahn's algorithm. Fills order[] and returns how many nodes were ordered;
 * a return value < n means the leftover nodes lie on / behind a cycle. */
static int kahn(const CSR *g, int *order) {
    int n = g->n;
    int *indeg = calloc((size_t)n, sizeof *indeg);
    for (int e = 0; e < g->off[n]; e++) indeg[g->adj[e]]++;
    Queue q = q_new(n);
    for (int u = 0; u < n; u++) if (indeg[u] == 0) q_push(&q, u);
    int cnt = 0;
    while (!q_empty(&q)) {
        int u = q_pop(&q);
        order[cnt++] = u;                                  /* pop order IS the topological order */
        for (int e = g->off[u]; e < g->off[u + 1]; e++)
            if (--indeg[g->adj[e]] == 0) q_push(&q, g->adj[e]);
    }
    q_free(&q); free(indeg);
    return cnt;
}

/* Three-colour DFS: 0 white (unseen), 1 grey (on current path), 2 black (done).
 * Returns 1 if a cycle is reachable from u. Post-order written from the back of
 * order[] so the final array is already in topological order (no reverse pass). */
static int dfs_color(const CSR *g, int u, unsigned char *color, int *order, int *pos) {
    color[u] = 1;
    for (int e = g->off[u]; e < g->off[u + 1]; e++) {
        int v = g->adj[e];
        if (color[v] == 1) return 1;                       /* back edge to the current path */
        if (color[v] == 0 && dfs_color(g, v, color, order, pos)) return 1;
        /* color[v] == 2: already finished in another branch — NOT a cycle */
    }
    color[u] = 2;
    order[--*pos] = u;
    return 0;
}

/* Returns 1 if acyclic (order[] filled), 0 if a cycle exists. */
static int toposort_dfs(const CSR *g, int *order) {
    unsigned char *color = calloc((size_t)g->n, 1);
    int pos = g->n, acyclic = 1;
    for (int s = 0; s < g->n && acyclic; s++)
        if (color[s] == 0 && dfs_color(g, s, color, order, &pos)) acyclic = 0;
    free(color);
    return acyclic;
}

static void print_order(const char *label, const int *order, int cnt) {
    printf("%s", label);
    for (int i = 0; i < cnt; i++) printf(" %d", order[i]);
    putchar('\n');
}

static void demo_toposort(void) {
    puts("\n== 4. Cycle detection / topological sort ==");
    /* DAG with a diamond: 0->1, 0->2, 1->3, 2->3, 3->4.
     * The diamond (two routes to 3) is exactly what breaks a 2-state visited flag. */
    int dag[][3] = { {0,1,0}, {0,2,0}, {1,3,0}, {2,3,0}, {3,4,0} };
    CSR g = csr_from_edges(5, 5, dag, 1);
    int order[5];
    int cnt = kahn(&g, order);
    printf("Kahn ordered %d/%d nodes → %s\n", cnt, g.n, cnt == g.n ? "acyclic" : "CYCLE");
    print_order("  Kahn order:    ", order, cnt);
    if (toposort_dfs(&g, order)) print_order("  DFS post-order:", order, g.n);
    csr_free(&g);

    /* Same graph plus 4->1: a directed cycle 1->3->4->1. */
    int cyc[][3] = { {0,1,0}, {0,2,0}, {1,3,0}, {2,3,0}, {3,4,0}, {4,1,0} };
    CSR gc = csr_from_edges(5, 6, cyc, 1);
    cnt = kahn(&gc, order);
    printf("with edge 4->1: Kahn ordered %d/%d nodes → %s\n", cnt, gc.n, cnt == gc.n ? "acyclic" : "CYCLE");
    printf("with edge 4->1: 3-colour DFS says %s\n", toposort_dfs(&gc, order) ? "acyclic" : "CYCLE");
    csr_free(&gc);
}

/* ------------------------------------------------------------------------- */
/* 5. Union-Find (disjoint set union)                                         */
/* ------------------------------------------------------------------------- */

typedef struct { int *parent, *size, count; } DSU;

static DSU dsu_new(int n) {
    DSU d = { malloc((size_t)n * sizeof(int)), malloc((size_t)n * sizeof(int)), n };
    for (int i = 0; i < n; i++) { d.parent[i] = i; d.size[i] = 1; }
    return d;
}
static void dsu_free(DSU *d) { free(d->parent); free(d->size); }

/* Iterative find with two-pass path compression: locate the root, then repoint
 * every node on the path straight at it. No recursion → no stack depth issue. */
static int dsu_find(DSU *d, int x) {
    int root = x;
    while (d->parent[root] != root) root = d->parent[root];
    while (d->parent[x] != root) { int next = d->parent[x]; d->parent[x] = root; x = next; }
    return root;
}

/* Union by size. Returns 1 if two sets were merged, 0 if x,y were already
 * together — i.e. the edge (x,y) would close a cycle. */
static int dsu_union(DSU *d, int x, int y) {
    int rx = dsu_find(d, x), ry = dsu_find(d, y);
    if (rx == ry) return 0;
    if (d->size[rx] < d->size[ry]) { int t = rx; rx = ry; ry = t; }   /* rx = bigger root */
    d->parent[ry] = rx;
    d->size[rx] += d->size[ry];
    d->count--;
    return 1;
}

/* Edge for Kruskal. qsort comparator uses the (a>b)-(a<b) idiom — never a-b. */
typedef struct { int u, v, w; } Edge;
static int cmp_edge_w(const void *a, const void *b) {
    const Edge *x = a, *y = b;
    return (x->w > y->w) - (x->w < y->w);
}

static void demo_union_find(void) {
    puts("\n== 5. Union-Find ==");
    /* Edge stream on 6 nodes; the 4th edge closes a cycle. */
    int stream[][2] = { {0,1}, {1,2}, {3,4}, {2,0}, {4,5} };
    DSU d = dsu_new(6);
    for (int i = 0; i < 5; i++) {
        int merged = dsu_union(&d, stream[i][0], stream[i][1]);
        printf("  add (%d,%d): %-18s components now %d\n", stream[i][0], stream[i][1],
               merged ? "merged" : "REDUNDANT (cycle)", d.count);
    }
    printf("same set 0,2: %s   same set 0,5: %s\n",
           dsu_find(&d, 0) == dsu_find(&d, 2) ? "yes" : "no",
           dsu_find(&d, 0) == dsu_find(&d, 5) ? "yes" : "no");
    dsu_free(&d);

    /* Kruskal MST on a small weighted graph (5 nodes, 7 edges). */
    Edge es[] = { {0,1,4}, {0,2,1}, {1,2,2}, {1,3,5}, {2,3,8}, {3,4,3}, {2,4,9} };
    int m = (int)(sizeof es / sizeof es[0]), n = 5;
    qsort(es, (size_t)m, sizeof es[0], cmp_edge_w);
    DSU k = dsu_new(n);
    long long total = 0; int accepted = 0;
    printf("Kruskal picks:");
    for (int i = 0; i < m && accepted < n - 1; i++)
        if (dsu_union(&k, es[i].u, es[i].v)) {         /* refuse cycle-closing edges */
            printf(" (%d-%d w%d)", es[i].u, es[i].v, es[i].w);
            total += es[i].w; accepted++;
        }
    printf("\nMST weight: %lld with %d edges\n", total, accepted);
    dsu_free(&k);
}

/* ------------------------------------------------------------------------- */
/* 6. Dijkstra with a binary min-heap and lazy deletion                       */
/* ------------------------------------------------------------------------- */

typedef struct { long long d; int v; } HItem;
typedef struct { HItem *a; int len, cap; } Heap;   /* array-backed min-heap on .d */

static Heap heap_new(int cap) { Heap h = { malloc((size_t)cap * sizeof(HItem)), 0, cap }; return h; }
static void heap_free(Heap *h) { free(h->a); }
static void heap_swap(HItem *x, HItem *y) { HItem t = *x; *x = *y; *y = t; }

static void heap_push(Heap *h, HItem it) {          /* caller guarantees len < cap */
    int i = h->len++;
    h->a[i] = it;
    while (i > 0) {                                  /* sift up */
        int p = (i - 1) / 2;
        if (h->a[p].d <= h->a[i].d) break;
        heap_swap(&h->a[p], &h->a[i]); i = p;
    }
}
static HItem heap_pop(Heap *h) {
    HItem top = h->a[0];
    h->a[0] = h->a[--h->len];
    int i = 0;
    for (;;) {                                       /* sift down */
        int l = 2 * i + 1, r = l + 1, s = i;
        if (l < h->len && h->a[l].d < h->a[s].d) s = l;
        if (r < h->len && h->a[r].d < h->a[s].d) s = r;
        if (s == i) break;
        heap_swap(&h->a[s], &h->a[i]); i = s;
    }
    return top;
}

#define INF (LLONG_MAX / 4)     /* INF + w cannot overflow; never use LLONG_MAX itself */

/* mode 0: classic (sum of weights). mode 1: minimax (minimise the largest edge on the path).
 * Only the relaxation line differs — everything else is the same skeleton. */
static void dijkstra(const CSR *g, int src, long long *dist, int mode) {
    for (int i = 0; i < g->n; i++) dist[i] = INF;
    Heap h = heap_new(g->nnz + 1);                   /* lazy deletion → up to one entry per edge */
    dist[src] = 0;
    heap_push(&h, (HItem){ 0, src });
    while (h.len > 0) {
        HItem top = heap_pop(&h);
        if (top.d > dist[top.v]) continue;           /* stale entry: a better one was popped earlier */
        int u = top.v;
        for (int e = g->off[u]; e < g->off[u + 1]; e++) {
            int v = g->adj[e];
            long long w = g->w[e];
            long long nd = mode == 0 ? top.d + w : (top.d > w ? top.d : w);
            if (nd < dist[v]) { dist[v] = nd; heap_push(&h, (HItem){ nd, v }); }
        }
    }
    heap_free(&h);
}

static void demo_dijkstra(void) {
    puts("\n== 6. Dijkstra ==");
    /* Directed weighted graph, 5 nodes. Node 4 is unreachable from 0. */
    int edges[][3] = { {0,1,4}, {0,2,1}, {2,1,2}, {1,3,1}, {2,3,5}, {0,3,3}, {4,0,7} };
    CSR g = csr_from_edges(5, 7, edges, 1);
    long long dist[5];

    dijkstra(&g, 0, dist, 0);
    printf("shortest sums from 0:   ");
    for (int v = 0; v < g.n; v++) {
        if (dist[v] == INF) printf(" %d:unreachable", v); else printf(" %d:%lld", v, dist[v]);
    }
    putchar('\n');

    dijkstra(&g, 0, dist, 1);
    printf("minimax (worst edge) from 0:");
    for (int v = 0; v < g.n; v++) {
        if (dist[v] == INF) printf(" %d:unreachable", v); else printf(" %d:%lld", v, dist[v]);
    }
    putchar('\n');
    puts("(node 3: cheapest SUM is the direct edge 0->3 = 3, but the MINIMAX path is 0->2->1->3"
         "\n whose worst edge is 2 — same skeleton, different relax rule, different path)");
    csr_free(&g);
}

/* ------------------------------------------------------------------------- */

int main(void) {
    demo_representations();
    demo_grid_dfs();
    demo_grid_bfs();
    demo_components();
    demo_toposort();
    demo_union_find();
    demo_dijkstra();
    return 0;
}
