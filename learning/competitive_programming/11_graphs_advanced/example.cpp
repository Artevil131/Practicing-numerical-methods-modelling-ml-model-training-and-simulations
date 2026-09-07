// Chapter 11 — Advanced graph algorithms: the reference library.
//
// TRAINING RULE: this file is not for copy-paste. Re-type every struct from
// memory, compile, and make the asserts in main() pass. Only then compare with
// this version. If a snippet takes you more than ~5 minutes to reproduce, you
// do not own it yet.
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Contents (in order):
//   1. Dijkstra with parents + path reconstruction; k shortest walks;
//      "Flight Discount" state expansion; 0-1 BFS
//   2. Bellman–Ford (distances / negative-cycle extraction), SPFA
//   3. Floyd–Warshall with path reconstruction; transitive closure via bitset
//   4. Topological sort (Kahn, lexicographic), DAG DP (count / longest path)
//   5. Cycle detection: directed (colors), undirected (DFS parent)
//   6. Eulerian path: Hierholzer, directed and undirected
//   7. Hamiltonian path count via bitmask DP
//   8. SCC: Tarjan (one pass) and Kosaraju; condensation
//   9. 2-SAT
//  10. Bridges, articulation points, 2-edge-connected components, bridge tree
//  11. MST: Kruskal, Prim, Borůvka; second-best MST; Kruskal reconstruction tree
//  12. Functional graphs: binary lifting, cycle structure
//  13. Bipartite check
// Every section has an assert-based test in main(); several are cross-checked
// against a brute force on random graphs.

// Headers listed explicitly so the file also builds on libc++ (Apple clang has
// no <bits/stdc++.h>). In a contest on GCC just use <bits/stdc++.h>.
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <climits>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <vector>
using namespace std;
using ll = long long;

const ll INF = LLONG_MAX / 4;   // safe to add two of these without overflow

mt19937 rng(20260905);
int rnd(int lo, int hi) { return uniform_int_distribution<int>(lo, hi)(rng); }

// =====================================================================
// 1. Dijkstra family
// =====================================================================
struct WEdge { int to; ll w; };

struct Dijkstra {
    int n;
    vector<vector<WEdge>> g;
    vector<ll> dist;
    vector<int> par;
    Dijkstra(int n) : n(n), g(n) {}
    void add(int a, int b, ll w) { g[a].push_back({b, w}); }

    // Invariant: when (d,u) is popped and d == dist[u], dist[u] is final
    // (all edge weights >= 0). Stale heap entries are skipped, so the heap
    // holds at most m entries -> O((n + m) log m).
    void run(int s) {
        dist.assign(n, INF); par.assign(n, -1);
        priority_queue<pair<ll,int>, vector<pair<ll,int>>, greater<>> pq;
        dist[s] = 0; pq.push({0, s});
        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist[u]) continue;                 // stale
            for (auto [v, w] : g[u])
                if (d + w < dist[v]) { dist[v] = d + w; par[v] = u; pq.push({dist[v], v}); }
        }
    }
    vector<int> path(int t) const {
        if (dist[t] >= INF) return {};
        vector<int> p;
        for (int v = t; v != -1; v = par[v]) p.push_back(v);
        reverse(p.begin(), p.end());
        return p;
    }
};

// k shortest walks s -> t (walks may repeat vertices; CSES "Flight Routes").
// Each vertex is allowed to be popped at most k times; the i-th pop of v is
// the i-th smallest walk length to v. Heap size O(k*m).
vector<ll> kShortestWalks(const vector<vector<WEdge>>& g, int s, int t, int k) {
    int n = g.size();
    vector<int> cnt(n, 0);
    vector<ll> res;
    priority_queue<pair<ll,int>, vector<pair<ll,int>>, greater<>> pq;
    pq.push({0, s});
    while (!pq.empty() && cnt[t] < k) {
        auto [d, u] = pq.top(); pq.pop();
        if (cnt[u] >= k) continue;
        cnt[u]++;
        if (u == t) res.push_back(d);
        for (auto [v, w] : g[u]) pq.push({d + w, v});
    }
    return res;
}

// "Flight Discount": exactly one edge may be taken at half price (floor).
// State = (vertex, used). 2n states, 3m transitions, still non-negative.
ll flightDiscount(const vector<vector<WEdge>>& g, int s, int t) {
    int n = g.size();
    vector<array<ll,2>> dist(n, {INF, INF});
    using S = tuple<ll,int,int>;
    priority_queue<S, vector<S>, greater<>> pq;
    dist[s][0] = 0; pq.push({0, s, 0});
    while (!pq.empty()) {
        auto [d, u, k] = pq.top(); pq.pop();
        if (d > dist[u][k]) continue;
        for (auto [v, w] : g[u]) {
            if (d + w < dist[v][k]) { dist[v][k] = d + w; pq.push({dist[v][k], v, k}); }
            if (k == 0 && d + w / 2 < dist[v][1]) { dist[v][1] = d + w / 2; pq.push({dist[v][1], v, 1}); }
        }
    }
    return dist[t][1];
}

// 0-1 BFS: weights in {0,1}. Deque keeps the invariant "front block has
// distance d, back block d+1"; O(n + m) without a heap.
vector<int> zeroOneBFS(const vector<vector<pair<int,int>>>& g, int s) {
    int n = g.size();
    vector<int> dist(n, INT_MAX);
    deque<int> dq;
    dist[s] = 0; dq.push_back(s);
    while (!dq.empty()) {
        int u = dq.front(); dq.pop_front();
        for (auto [v, w] : g[u])
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                if (w == 0) dq.push_front(v); else dq.push_back(v);
            }
    }
    return dist;
}

// =====================================================================
// 2. Bellman–Ford / SPFA
// =====================================================================
struct Edge3 { int a, b; ll w; };

// Single-source distances with negative edges, no negative cycle assumed
// reachable. n-1 rounds suffice: a shortest path has <= n-1 edges and after
// round i all shortest paths with <= i edges are correct.
vector<ll> bellmanFord(int n, const vector<Edge3>& es, int s) {
    vector<ll> d(n, INF);
    d[s] = 0;
    for (int i = 0; i < n - 1; i++) {
        bool any = false;
        for (auto& e : es)
            if (d[e.a] < INF && d[e.a] + e.w < d[e.b]) { d[e.b] = d[e.a] + e.w; any = true; }
        if (!any) break;                               // early exit
    }
    return d;
}

// Find ANY negative cycle in the whole graph (CSES "Cycle Finding").
// Trick: start all distances at 0 = virtual source with 0-edges to every
// vertex. If round n still relaxes vertex x, walk par[] n times from x to
// be guaranteed inside the cycle, then read it off.
vector<int> negativeCycle(int n, const vector<Edge3>& es) {
    vector<ll> d(n, 0);
    vector<int> par(n, -1);
    int x = -1;
    for (int i = 0; i < n; i++) {
        x = -1;
        for (auto& e : es)
            if (d[e.a] + e.w < d[e.b]) { d[e.b] = d[e.a] + e.w; par[e.b] = e.a; x = e.b; }
    }
    if (x == -1) return {};
    for (int i = 0; i < n; i++) x = par[x];
    vector<int> cyc;
    for (int v = x;; v = par[v]) { cyc.push_back(v); if (v == x && cyc.size() > 1) break; }
    reverse(cyc.begin(), cyc.end());                   // cyc.front() == cyc.back() == x
    return cyc;
}

// SPFA: Bellman–Ford with a queue of "dirty" vertices. Average fast, worst
// case still O(nm) and easily broken by adversarial tests. Returns false if a
// vertex is relaxed n times (negative cycle reachable from s).
bool spfa(int n, const vector<vector<WEdge>>& g, int s, vector<ll>& d) {
    d.assign(n, INF);
    vector<int> cnt(n, 0);
    vector<char> inq(n, 0);
    queue<int> q;
    d[s] = 0; q.push(s); inq[s] = 1;
    while (!q.empty()) {
        int u = q.front(); q.pop(); inq[u] = 0;
        for (auto [v, w] : g[u])
            if (d[u] + w < d[v]) {
                d[v] = d[u] + w;
                if (!inq[v]) {
                    if (++cnt[v] >= n) return false;
                    q.push(v); inq[v] = 1;
                }
            }
    }
    return true;
}

// =====================================================================
// 3. Floyd–Warshall + transitive closure
// =====================================================================
struct Floyd {
    int n;
    vector<vector<ll>> d;
    vector<vector<int>> nxt;      // nxt[i][j] = first hop on a shortest i->j path
    Floyd(int n) : n(n), d(n, vector<ll>(n, INF)), nxt(n, vector<int>(n, -1)) {
        for (int i = 0; i < n; i++) d[i][i] = 0;
    }
    void add(int a, int b, ll w) { if (w < d[a][b]) { d[a][b] = w; nxt[a][b] = b; } }
    // Invariant after outer iteration k: d[i][j] = shortest path using only
    // intermediate vertices from {0..k}.
    void run() {
        for (int k = 0; k < n; k++)
            for (int i = 0; i < n; i++) {
                if (d[i][k] >= INF) continue;
                for (int j = 0; j < n; j++)
                    if (d[k][j] < INF && d[i][k] + d[k][j] < d[i][j]) {
                        d[i][j] = d[i][k] + d[k][j];
                        nxt[i][j] = nxt[i][k];
                    }
            }
    }
    vector<int> path(int a, int b) const {
        if (d[a][b] >= INF) return {};
        vector<int> p{a};
        while (a != b) { a = nxt[a][b]; p.push_back(a); }
        return p;
    }
    // Negative cycle through some vertex <=> d[i][i] < 0 after run().
};

// Reachability for n <= 64 here (use bitset<MAXN> and a compile-time bound in
// contests). O(n^3 / 64): n = 5000 is ~2e9 bit-ops -> fine.
template<size_t B>
vector<bitset<B>> transitiveClosure(vector<bitset<B>> reach) {
    int n = reach.size();
    for (int i = 0; i < n; i++) reach[i][i] = 1;
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            if (reach[i][k]) reach[i] |= reach[k];
    return reach;
}

// =====================================================================
// 4. Topological sort and DAG DP
// =====================================================================
// Kahn's algorithm. Returns order of size < n iff the graph has a cycle.
vector<int> topoKahn(int n, const vector<vector<int>>& g) {
    vector<int> indeg(n, 0), order;
    for (int u = 0; u < n; u++) for (int v : g[u]) indeg[v]++;
    queue<int> q;
    for (int u = 0; u < n; u++) if (indeg[u] == 0) q.push(u);
    while (!q.empty()) {
        int u = q.front(); q.pop(); order.push_back(u);
        for (int v : g[u]) if (--indeg[v] == 0) q.push(v);
    }
    return order;
}

// Lexicographically smallest topological order: replace the queue by a
// min-heap. (For "smallest position of vertex 1" type constraints the trick
// is reversed graph + max-heap + reverse the result.)
vector<int> topoLexSmallest(int n, const vector<vector<int>>& g) {
    vector<int> indeg(n, 0), order;
    for (int u = 0; u < n; u++) for (int v : g[u]) indeg[v]++;
    priority_queue<int, vector<int>, greater<>> pq;
    for (int u = 0; u < n; u++) if (indeg[u] == 0) pq.push(u);
    while (!pq.empty()) {
        int u = pq.top(); pq.pop(); order.push_back(u);
        for (int v : g[u]) if (--indeg[v] == 0) pq.push(v);
    }
    return order;
}

const ll MOD = 1'000'000'007LL;

// Number of paths s -> t in a DAG modulo MOD (CSES "Game Routes").
ll countDagPaths(int n, const vector<vector<int>>& g, int s, int t) {
    vector<int> order = topoKahn(n, g);
    vector<ll> ways(n, 0);
    ways[s] = 1;
    for (int u : order) for (int v : g[u]) ways[v] = (ways[v] + ways[u]) % MOD;
    return ways[t];
}

// Longest path (edge count) s -> t in a DAG with reconstruction; -1 if none.
// Pitfall: unreachable vertices must stay at -INF, otherwise they leak 0s.
pair<int, vector<int>> longestDagPath(int n, const vector<vector<int>>& g, int s, int t) {
    vector<int> order = topoKahn(n, g);
    vector<int> best(n, INT_MIN), par(n, -1);
    best[s] = 0;
    for (int u : order) {
        if (best[u] == INT_MIN) continue;
        for (int v : g[u]) if (best[u] + 1 > best[v]) { best[v] = best[u] + 1; par[v] = u; }
    }
    if (best[t] == INT_MIN) return {-1, {}};
    vector<int> p;
    for (int v = t; v != -1; v = par[v]) p.push_back(v);
    reverse(p.begin(), p.end());
    return {best[t], p};
}

// =====================================================================
// 5. Cycle detection
// =====================================================================
// Directed: DFS with 3 colors. A back edge (to a GRAY vertex) closes a cycle.
// Returns the cycle as v0 v1 ... v0, or empty.
vector<int> directedCycle(int n, const vector<vector<int>>& g) {
    vector<int> color(n, 0), par(n, -1);
    int start = -1, end = -1;
    function<bool(int)> dfs = [&](int u) {
        color[u] = 1;
        for (int v : g[u]) {
            if (color[v] == 0) { par[v] = u; if (dfs(v)) return true; }
            else if (color[v] == 1) { start = v; end = u; return true; }
        }
        color[u] = 2;
        return false;
    };
    for (int u = 0; u < n && start == -1; u++) if (color[u] == 0) dfs(u);
    if (start == -1) return {};
    vector<int> cyc{start};
    for (int v = end; v != start; v = par[v]) cyc.push_back(v);
    cyc.push_back(start);
    reverse(cyc.begin(), cyc.end());
    return cyc;
}

// Undirected: DFS, an edge to a visited vertex that is not the parent EDGE
// closes a cycle. Track the parent edge id, not the parent vertex, so that
// multi-edges (a,b),(a,b) count as a cycle of length 2.
vector<int> undirectedCycle(int n, const vector<vector<pair<int,int>>>& g /* (to, edgeId) */) {
    vector<int> vis(n, 0), par(n, -1);
    int start = -1, end = -1;
    function<bool(int,int)> dfs = [&](int u, int pe) {
        vis[u] = 1;
        for (auto [v, id] : g[u]) {
            if (id == pe) continue;
            if (!vis[v]) { par[v] = u; if (dfs(v, id)) return true; }
            else { start = v; end = u; return true; }
        }
        return false;
    };
    for (int u = 0; u < n && start == -1; u++) if (!vis[u]) dfs(u, -1);
    if (start == -1) return {};
    vector<int> cyc{start};
    for (int v = end; v != start; v = par[v]) cyc.push_back(v);
    cyc.push_back(start);
    return cyc;
}

// =====================================================================
// 6. Eulerian path (Hierholzer)
// =====================================================================
// Directed. Existence: every vertex in/out balanced, except s (out = in + 1)
// and t (in = out + 1); all edges in one weakly connected piece.
// Iterative Hierholzer: walk greedily, when stuck pop into the answer.
// The answer is produced in reverse. O(n + m).
vector<int> eulerPathDirected(int n, const vector<pair<int,int>>& edges, int s, int t) {
    int m = edges.size();
    vector<vector<int>> g(n);
    vector<int> in(n, 0), out(n, 0);
    for (int i = 0; i < m; i++) { g[edges[i].first].push_back(i); out[edges[i].first]++; in[edges[i].second]++; }
    for (int v = 0; v < n; v++) {
        int need = (v == s) - (v == t);          // s: +1, t: -1, s==t: 0
        if (out[v] - in[v] != need) return {};
    }
    vector<int> ptr(n, 0), stk{s}, path;
    while (!stk.empty()) {
        int u = stk.back();
        if (ptr[u] < (int)g[u].size()) { int id = g[u][ptr[u]++]; stk.push_back(edges[id].second); }
        else { path.push_back(u); stk.pop_back(); }
    }
    reverse(path.begin(), path.end());
    if ((int)path.size() != m + 1) return {};   // edges not all reachable from s
    return path;
}

// Undirected. Existence: 0 odd vertices (circuit, s == t) or exactly 2 (they
// must be s and t); every vertex with an edge is connected to s.
vector<int> eulerPathUndirected(int n, const vector<pair<int,int>>& edges, int s, int t) {
    int m = edges.size();
    vector<vector<pair<int,int>>> g(n);
    vector<int> deg(n, 0);
    for (int i = 0; i < m; i++) {
        auto [a, b] = edges[i];
        g[a].push_back({b, i}); g[b].push_back({a, i}); deg[a]++; deg[b]++;
    }
    for (int v = 0; v < n; v++) {
        bool odd = deg[v] & 1;
        bool endpoint = (s != t) && (v == s || v == t);
        if (odd != endpoint) return {};
    }
    vector<char> used(m, 0);
    vector<int> ptr(n, 0), stk{s}, path;
    while (!stk.empty()) {
        int u = stk.back();
        while (ptr[u] < (int)g[u].size() && used[g[u][ptr[u]].second]) ptr[u]++;
        if (ptr[u] == (int)g[u].size()) { path.push_back(u); stk.pop_back(); }
        else { auto [v, id] = g[u][ptr[u]]; used[id] = 1; stk.push_back(v); }
    }
    if ((int)path.size() != m + 1) return {};
    reverse(path.begin(), path.end());
    return path;
}

// =====================================================================
// 7. Hamiltonian paths via bitmask DP (CSES "Hamiltonian Flights")
// =====================================================================
// dp[mask][v] = number of paths starting at 0, visiting exactly mask, ending
// at v. O(2^n * m). n <= 20. Force ending at n-1 only when mask is full.
ll countHamiltonianPaths(int n, const vector<vector<int>>& g) {
    int full = (1 << n) - 1;
    vector<vector<ll>> dp(1 << n, vector<ll>(n, 0));
    dp[1][0] = 1;
    for (int mask = 1; mask <= full; mask++)
        for (int v = 0; v < n; v++) {
            if (!dp[mask][v]) continue;
            if (v == n - 1 && mask != full) continue;   // may not pass through target
            for (int w : g[v]) if (!(mask >> w & 1))
                dp[mask | 1 << w][w] = (dp[mask | 1 << w][w] + dp[mask][v]) % MOD;
        }
    return dp[full][n - 1];
}

// =====================================================================
// 8. Strongly connected components
// =====================================================================
// Tarjan: one DFS. low[u] = smallest index reachable from u's subtree via at
// most one back edge. u is an SCC root iff low[u] == idx[u]; the SCC is the
// stack segment above u. Components are numbered in REVERSE topological
// order of the condensation (comp 0 is a sink).
struct TarjanSCC {
    int n, timer = 0, ncomp = 0;
    vector<vector<int>> g;
    vector<int> idx, low, comp, stk;
    vector<char> onstk;
    TarjanSCC(int n) : n(n), g(n), idx(n, -1), low(n, 0), comp(n, -1), onstk(n, 0) {}
    void add(int a, int b) { g[a].push_back(b); }
    void dfs(int u) {
        idx[u] = low[u] = timer++;
        stk.push_back(u); onstk[u] = 1;
        for (int v : g[u]) {
            if (idx[v] == -1) { dfs(v); low[u] = min(low[u], low[v]); }
            else if (onstk[v]) low[u] = min(low[u], idx[v]);
        }
        if (low[u] == idx[u]) {
            while (true) {
                int v = stk.back(); stk.pop_back(); onstk[v] = 0;
                comp[v] = ncomp;
                if (v == u) break;
            }
            ncomp++;
        }
    }
    void run() { for (int u = 0; u < n; u++) if (idx[u] == -1) dfs(u); }
    // Condensation DAG (edges deduplicated). Edge comp[a] -> comp[b] always
    // goes from a higher comp id to a lower one.
    vector<vector<int>> condensation() const {
        vector<set<int>> s(ncomp);
        for (int u = 0; u < n; u++) for (int v : g[u]) if (comp[u] != comp[v]) s[comp[u]].insert(comp[v]);
        vector<vector<int>> c(ncomp);
        for (int i = 0; i < ncomp; i++) c[i].assign(s[i].begin(), s[i].end());
        return c;
    }
};

// Kosaraju: DFS #1 records finish order; DFS #2 on the reversed graph in
// decreasing finish time; each DFS #2 tree is exactly one SCC. Components
// come out in topological order of the condensation (comp 0 is a source).
struct KosarajuSCC {
    int n, ncomp = 0;
    vector<vector<int>> g, rg;
    vector<int> comp, order;
    vector<char> vis;
    KosarajuSCC(int n) : n(n), g(n), rg(n), comp(n, -1), vis(n, 0) {}
    void add(int a, int b) { g[a].push_back(b); rg[b].push_back(a); }
    void dfs1(int u) { vis[u] = 1; for (int v : g[u]) if (!vis[v]) dfs1(v); order.push_back(u); }
    void dfs2(int u, int c) { comp[u] = c; for (int v : rg[u]) if (comp[v] == -1) dfs2(v, c); }
    void run() {
        for (int u = 0; u < n; u++) if (!vis[u]) dfs1(u);
        for (int i = n - 1; i >= 0; i--) if (comp[order[i]] == -1) dfs2(order[i], ncomp++);
    }
};

// =====================================================================
// 9. 2-SAT
// =====================================================================
// Variable i has literals 2i (x_i) and 2i+1 (not x_i); lit ^ 1 negates.
// Clause (a or b) adds implications (not a -> b) and (not b -> a).
// Satisfiable iff no variable shares an SCC with its negation. With Tarjan
// numbering (reverse topological), set x true iff comp[x] < comp[not x],
// i.e. x's component comes LATER in topological order.
struct TwoSAT {
    int n;                       // number of variables
    TarjanSCC scc;
    vector<char> value;
    TwoSAT(int n) : n(n), scc(2 * n) {}
    void addClause(int a, int b) { scc.add(a ^ 1, b); scc.add(b ^ 1, a); }   // a or b
    void addImplication(int a, int b) { scc.add(a, b); scc.add(b ^ 1, a ^ 1); }
    void setTrue(int a) { scc.add(a ^ 1, a); }
    bool solve() {
        scc.run();
        value.assign(n, 0);
        for (int i = 0; i < n; i++) {
            if (scc.comp[2 * i] == scc.comp[2 * i + 1]) return false;
            value[i] = scc.comp[2 * i] < scc.comp[2 * i + 1];
        }
        return true;
    }
};

// =====================================================================
// 10. Bridges, articulation points, 2-edge-connected components
// =====================================================================
// low[u] = min over tin[u], tin of back-edge targets from the subtree of u.
// Tree edge (p,u) is a bridge iff low[u] > tin[p]: nothing in u's subtree
// reaches p or above except through this edge.
// Non-root p is an articulation point iff some child u has low[u] >= tin[p].
// Root is an articulation point iff it has >= 2 DFS children.
// Multi-edges: skip only the parent EDGE (by id), not the parent vertex.
struct BridgesAP {
    int n, timer = 0;
    vector<vector<pair<int,int>>> g;    // (to, edgeId)
    vector<pair<int,int>> edges;
    vector<int> tin, low, comp;          // comp = 2-edge-connected component id
    vector<char> isBridge, isAP;
    int ncomp = 0;
    BridgesAP(int n) : n(n), g(n), tin(n, -1), low(n, 0), comp(n, -1), isAP(n, 0) {}
    void add(int a, int b) {
        int id = edges.size();
        edges.push_back({a, b});
        g[a].push_back({b, id}); g[b].push_back({a, id});
    }
    void dfs(int u, int pe) {
        tin[u] = low[u] = timer++;
        int children = 0;
        for (auto [v, id] : g[u]) {
            if (id == pe) continue;
            if (tin[v] != -1) low[u] = min(low[u], tin[v]);        // back edge
            else {
                dfs(v, id);
                low[u] = min(low[u], low[v]);
                if (low[v] > tin[u]) isBridge[id] = 1;
                if (low[v] >= tin[u] && pe != -1) isAP[u] = 1;
                children++;
            }
        }
        if (pe == -1 && children >= 2) isAP[u] = 1;
    }
    void run() {
        isBridge.assign(edges.size(), 0);
        for (int u = 0; u < n; u++) if (tin[u] == -1) dfs(u, -1);
        // 2-edge-connected components: flood fill without crossing bridges.
        for (int u = 0; u < n; u++) if (comp[u] == -1) {
            vector<int> st{u}; comp[u] = ncomp;
            while (!st.empty()) {
                int x = st.back(); st.pop_back();
                for (auto [v, id] : g[x]) if (!isBridge[id] && comp[v] == -1) { comp[v] = ncomp; st.push_back(v); }
            }
            ncomp++;
        }
    }
    // Bridge tree: one vertex per 2-edge-connected component, one edge per bridge.
    vector<vector<int>> bridgeTree() const {
        vector<vector<int>> t(ncomp);
        for (size_t i = 0; i < edges.size(); i++) if (isBridge[i]) {
            int a = comp[edges[i].first], b = comp[edges[i].second];
            t[a].push_back(b); t[b].push_back(a);
        }
        return t;
    }
};

// =====================================================================
// 11. Minimum spanning trees
// =====================================================================
struct DSU {
    vector<int> p, sz;
    DSU(int n) : p(n), sz(n, 1) { iota(p.begin(), p.end(), 0); }
    int find(int x) { return p[x] == x ? x : p[x] = find(p[x]); }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (sz[a] < sz[b]) swap(a, b);
        p[b] = a; sz[a] += sz[b];
        return true;
    }
};

struct MSTEdge { int a, b; ll w; };

// Kruskal: sort edges, take an edge iff it joins two components (cut
// property). Returns {weight, chosen edge indices}; size < n-1 => disconnected.
pair<ll, vector<int>> kruskal(int n, vector<MSTEdge> es) {
    vector<int> id(es.size());
    iota(id.begin(), id.end(), 0);
    sort(id.begin(), id.end(), [&](int x, int y) { return es[x].w < es[y].w; });
    DSU d(n);
    ll total = 0; vector<int> chosen;
    for (int i : id) if (d.unite(es[i].a, es[i].b)) { total += es[i].w; chosen.push_back(i); }
    return {total, chosen};
}

// Prim: grow one tree, always take the lightest edge leaving it. Same shape as
// Dijkstra but the key is the edge weight, not the path length. Returns INF
// if disconnected.
ll prim(int n, const vector<vector<WEdge>>& g) {
    vector<char> in(n, 0);
    priority_queue<pair<ll,int>, vector<pair<ll,int>>, greater<>> pq;
    pq.push({0, 0});
    ll total = 0; int taken = 0;
    while (!pq.empty()) {
        auto [w, u] = pq.top(); pq.pop();
        if (in[u]) continue;
        in[u] = 1; total += w; taken++;
        for (auto [v, ww] : g[u]) if (!in[v]) pq.push({ww, v});
    }
    return taken == n ? total : INF;
}

// Borůvka: each round every component picks its cheapest outgoing edge; all
// picks are added (ties broken by edge index so no cycles). Components at
// least halve per round -> O(m log n). Basis of parallel MST and of
// "MST on implicit graphs" (e.g. XOR-MST with a trie).
ll boruvka(int n, const vector<MSTEdge>& es) {
    DSU d(n);
    ll total = 0; int comps = n;
    while (comps > 1) {
        vector<int> best(n, -1);
        for (size_t i = 0; i < es.size(); i++) {
            int a = d.find(es[i].a), b = d.find(es[i].b);
            if (a == b) continue;
            auto better = [&](int cur) {
                return cur == -1 || es[i].w < es[cur].w || (es[i].w == es[cur].w && (int)i < cur);
            };
            if (better(best[a])) best[a] = i;
            if (better(best[b])) best[b] = i;
        }
        bool any = false;
        for (int c = 0; c < n; c++) if (best[c] != -1 && d.unite(es[best[c]].a, es[best[c]].b)) {
            total += es[best[c]].w; comps--; any = true;
        }
        if (!any) return INF;                          // disconnected
    }
    return total;
}

// Binary-lifting LCA on a tree that also stores (max, second max) edge weight
// on the upward path. Used for second-best MST and bottleneck queries.
struct TreeLift {
    int n, LOG;
    vector<vector<int>> up;
    vector<vector<pair<ll,ll>>> mx;     // (largest, second largest distinct) on 2^k edges
    vector<int> depth;
    TreeLift(int n, const vector<vector<WEdge>>& t, int root = 0) : n(n) {
        LOG = 1; while ((1 << LOG) < n) LOG++;
        up.assign(LOG, vector<int>(n, root));
        mx.assign(LOG, vector<pair<ll,ll>>(n, {-INF, -INF}));
        depth.assign(n, 0);
        vector<int> st{root}; vector<char> vis(n, 0); vis[root] = 1;
        while (!st.empty()) {
            int u = st.back(); st.pop_back();
            for (auto [v, w] : t[u]) if (!vis[v]) {
                vis[v] = 1; depth[v] = depth[u] + 1; up[0][v] = u; mx[0][v] = {w, -INF}; st.push_back(v);
            }
        }
        for (int k = 1; k < LOG; k++)
            for (int v = 0; v < n; v++) {
                up[k][v] = up[k - 1][up[k - 1][v]];
                mx[k][v] = merge(mx[k - 1][v], mx[k - 1][up[k - 1][v]]);
            }
    }
    static pair<ll,ll> merge(pair<ll,ll> a, pair<ll,ll> b) {
        ll m1 = max(a.first, b.first);
        ll m2 = -INF;
        for (ll x : {a.first, a.second, b.first, b.second}) if (x != m1) m2 = max(m2, x);
        return {m1, m2};
    }
    // (max, second max) edge weight on the path u-v.
    pair<ll,ll> query(int u, int v) const {
        pair<ll,ll> res{-INF, -INF};
        if (depth[u] < depth[v]) swap(u, v);
        for (int k = LOG - 1; k >= 0; k--) if (depth[u] - (1 << k) >= depth[v]) { res = merge(res, mx[k][u]); u = up[k][u]; }
        if (u == v) return res;
        for (int k = LOG - 1; k >= 0; k--) if (up[k][u] != up[k][v]) {
            res = merge(res, merge(mx[k][u], mx[k][v])); u = up[k][u]; v = up[k][v];
        }
        return merge(res, merge(mx[0][u], mx[0][v]));
    }
};

// Second-best MST. Theorem: some second-best tree differs from ANY fixed MST T
// by exactly one exchange (add non-tree edge e, drop a tree edge on the cycle).
// So: for every non-tree edge (u,v,w) replace the heaviest tree edge on the
// tree path u-v. O(m log n) with binary lifting.
//   strict = false : cheapest spanning tree whose edge set differs from T
//                    (weight may equal the MST weight).
//   strict = true  : cheapest spanning tree with weight > MST weight; if the
//                    path maximum equals w, use the second maximum instead.
// Returns INF if no such tree exists.
ll secondBestMST(int n, const vector<MSTEdge>& es, bool strict) {
    auto [mst, chosen] = kruskal(n, es);
    if ((int)chosen.size() != n - 1) return INF;
    vector<char> inTree(es.size(), 0);
    vector<vector<WEdge>> t(n);
    for (int i : chosen) { inTree[i] = 1; t[es[i].a].push_back({es[i].b, es[i].w}); t[es[i].b].push_back({es[i].a, es[i].w}); }
    TreeLift lift(n, t);
    ll best = INF;
    for (size_t i = 0; i < es.size(); i++) if (!inTree[i]) {
        if (es[i].a == es[i].b) continue;              // self-loop never in a tree
        auto [m1, m2] = lift.query(es[i].a, es[i].b);
        ll rep = (strict && m1 == es[i].w) ? m2 : m1;
        if (rep > -INF) best = min(best, mst - rep + es[i].w);
    }
    return best;
}

// Kruskal reconstruction tree: process edges by weight; when an edge joins
// two components create a new node whose weight is the edge weight and whose
// children are the two component roots. Then bottleneck(u,v) = weight of
// LCA(u,v), and "vertices reachable from u using edges <= w" is a subtree.
struct KruskalTree {
    int n, cnt;
    vector<ll> w;                 // w[node] for internal nodes, -INF for leaves
    vector<int> par, depth;
    vector<vector<int>> up;
    KruskalTree(int n, vector<MSTEdge> es) : n(n), cnt(n), w(2 * n, -INF), par(2 * n, -1) {
        sort(es.begin(), es.end(), [](const MSTEdge& x, const MSTEdge& y) { return x.w < y.w; });
        DSU d(2 * n);
        for (auto& e : es) {
            int a = d.find(e.a), b = d.find(e.b);
            if (a == b) continue;
            int c = cnt++;
            w[c] = e.w; par[a] = par[b] = c;
            d.p[a] = d.p[b] = c;                       // c is the new representative
        }
        int LOG = 1; while ((1 << LOG) < cnt) LOG++;
        up.assign(LOG, vector<int>(cnt));
        depth.assign(cnt, 0);
        // roots first: nodes are created in increasing order, parent id > child id
        for (int v = cnt - 1; v >= 0; v--) {
            int p = par[v] == -1 ? v : par[v];
            up[0][v] = p;
            depth[v] = par[v] == -1 ? 0 : depth[par[v]] + 1;
        }
        for (int k = 1; k < LOG; k++) for (int v = 0; v < cnt; v++) up[k][v] = up[k - 1][up[k - 1][v]];
    }
    int lca(int u, int v) const {
        if (depth[u] < depth[v]) swap(u, v);
        int LOG = up.size();
        for (int k = LOG - 1; k >= 0; k--) if (depth[u] - (1 << k) >= depth[v]) u = up[k][u];
        if (u == v) return u;
        for (int k = LOG - 1; k >= 0; k--) if (up[k][u] != up[k][v]) { u = up[k][u]; v = up[k][v]; }
        return up[0][u];
    }
    // Min over paths of the max edge; INF if disconnected.
    ll bottleneck(int u, int v) const {
        int a = u, b = v;
        while (par[a] != -1) a = par[a];
        while (par[b] != -1) b = par[b];
        if (a != b) return INF;
        return u == v ? 0 : w[lca(u, v)];
    }
};

// =====================================================================
// 12. Functional graphs (every vertex has exactly one out-edge)
// =====================================================================
struct Successor {
    int n, LOG;
    vector<vector<int>> up;      // up[k][v] = 2^k-th successor
    Successor(const vector<int>& nxt, ll maxSteps) {
        n = nxt.size();
        LOG = 1; while ((1LL << LOG) <= maxSteps) LOG++;
        up.assign(LOG, nxt);
        for (int k = 1; k < LOG; k++) for (int v = 0; v < n; v++) up[k][v] = up[k - 1][up[k - 1][v]];
    }
    int kth(int v, ll k) const {
        for (int b = 0; b < LOG; b++) if (k >> b & 1) v = up[b][v];
        return v;
    }
};

// For each vertex: number of steps until the walk first repeats a vertex
// (CSES "Planets Cycles"): distance to its cycle + cycle length.
// state 0 = unvisited, 1 = on current path, 2 = done. Iterative, O(n).
vector<int> planetsCycles(const vector<int>& nxt) {
    int n = nxt.size();
    vector<int> ans(n, 0), state(n, 0);
    for (int s = 0; s < n; s++) {
        if (state[s]) continue;
        vector<int> path;
        int v = s;
        while (state[v] == 0) { state[v] = 1; path.push_back(v); v = nxt[v]; }
        if (state[v] == 1) {                            // closed a new cycle at v
            int len = 0;
            for (int i = path.size() - 1; ; i--) { len++; if (path[i] == v) break; }
            for (int i = path.size() - 1; ; i--) { ans[path[i]] = len; state[path[i]] = 2; if (path[i] == v) { path.resize(i); break; } }
        }
        // remaining path vertices lead into an already-finished vertex v
        for (int i = path.size() - 1; i >= 0; i--) { ans[path[i]] = ans[nxt[path[i]]] + 1; state[path[i]] = 2; }
    }
    return ans;
}

// =====================================================================
// 13. Bipartite check (2-coloring by BFS)
// =====================================================================
// Returns colors or empty if an odd cycle exists.
vector<int> bipartiteColor(int n, const vector<vector<int>>& g) {
    vector<int> col(n, -1);
    for (int s = 0; s < n; s++) {
        if (col[s] != -1) continue;
        col[s] = 0; queue<int> q; q.push(s);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int v : g[u]) {
                if (col[v] == -1) { col[v] = col[u] ^ 1; q.push(v); }
                else if (col[v] == col[u]) return {};
            }
        }
    }
    return col;
}

// =====================================================================
// Tests
// =====================================================================
static void testShortestPaths() {
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(2, 8), m = rnd(1, 16);
        Dijkstra dj(n); Floyd fl(n); vector<Edge3> es; vector<vector<WEdge>> g(n);
        for (int i = 0; i < m; i++) {
            int a = rnd(0, n - 1), b = rnd(0, n - 1); ll w = rnd(0, 10);
            dj.add(a, b, w); fl.add(a, b, w); es.push_back({a, b, w}); g[a].push_back({b, w});
        }
        dj.run(0); fl.run();
        auto bf = bellmanFord(n, es, 0);
        vector<ll> sp; assert(spfa(n, g, 0, sp));
        for (int v = 0; v < n; v++) {
            assert(dj.dist[v] == bf[v]);
            assert(dj.dist[v] == fl.d[0][v]);
            assert(dj.dist[v] == sp[v]);
            // path reconstruction: verify the paths are real and have the claimed length
            for (auto p : {dj.path(v), fl.path(0, v)}) {
                if (dj.dist[v] >= INF) { assert(p.empty()); continue; }
                assert(!p.empty() && p.front() == 0 && p.back() == v);
                ll len = 0;
                for (size_t i = 1; i < p.size(); i++) {
                    ll best = INF;
                    for (auto [to, w] : g[p[i - 1]]) if (to == p[i]) best = min(best, w);
                    assert(best < INF); len += best;
                }
                assert(len == dj.dist[v]);
            }
        }
        // k shortest walks: the 1st is the Dijkstra distance, sequence is non-decreasing
        auto ks = kShortestWalks(g, 0, n - 1, 3);
        if (dj.dist[n - 1] < INF) { assert(!ks.empty() && ks[0] == dj.dist[n - 1]); assert(is_sorted(ks.begin(), ks.end())); }
        else assert(ks.empty());
        // flight discount: brute force by halving each edge in turn
        ll brute = INF;
        for (int i = 0; i < m; i++) {
            Dijkstra d2(n);
            for (int j = 0; j < m; j++) d2.add(es[j].a, es[j].b, j == i ? es[j].w / 2 : es[j].w);
            d2.run(0); brute = min(brute, d2.dist[n - 1]);
        }
        assert(flightDiscount(g, 0, n - 1) == brute);
    }
    // 0-1 BFS vs Dijkstra
    for (int iter = 0; iter < 100; iter++) {
        int n = rnd(2, 10), m = rnd(1, 25);
        vector<vector<pair<int,int>>> g(n); Dijkstra dj(n);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1), w = rnd(0, 1); g[a].push_back({b, w}); dj.add(a, b, w); }
        dj.run(0); auto z = zeroOneBFS(g, 0);
        for (int v = 0; v < n; v++) assert((dj.dist[v] >= INF) == (z[v] == INT_MAX) && (z[v] == INT_MAX || z[v] == dj.dist[v]));
    }
    // negative cycle: brute check with Floyd's diagonal
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 6), m = rnd(1, 10);
        vector<Edge3> es; Floyd fl(n);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); ll w = rnd(-5, 8); es.push_back({a, b, w}); fl.add(a, b, w); }
        fl.run();
        bool hasNeg = false;
        for (int v = 0; v < n; v++) if (fl.d[v][v] < 0) hasNeg = true;
        auto cyc = negativeCycle(n, es);
        assert(hasNeg == !cyc.empty());
        if (!cyc.empty()) {                            // verify the cycle is real and negative
            assert(cyc.front() == cyc.back());
            ll total = 0;
            for (size_t i = 1; i < cyc.size(); i++) {
                ll best = INF;
                for (auto& e : es) if (e.a == cyc[i - 1] && e.b == cyc[i]) best = min(best, e.w);
                assert(best < INF); total += best;
            }
            assert(total < 0);
        }
    }
    // transitive closure vs Floyd reachability
    for (int iter = 0; iter < 50; iter++) {
        int n = rnd(1, 12), m = rnd(0, 30);
        vector<bitset<64>> r(n); Floyd fl(n);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); r[a][b] = 1; fl.add(a, b, 1); }
        fl.run(); auto tc = transitiveClosure<64>(r);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) assert(tc[i][j] == (fl.d[i][j] < INF));
    }
}

static void testDagAndCycles() {
    // Hand DAG: 0->1, 0->2, 1->3, 2->3, 3->4 : 2 paths 0->4, longest = 3 edges
    vector<vector<int>> g = {{1, 2}, {3}, {3}, {4}, {}};
    assert(topoKahn(5, g).size() == 5);
    assert(topoLexSmallest(5, g) == vector<int>({0, 1, 2, 3, 4}));
    assert(countDagPaths(5, g, 0, 4) == 2);
    auto [len, p] = longestDagPath(5, g, 0, 4);
    assert(len == 3 && p.front() == 0 && p.back() == 4 && (int)p.size() == 4);
    assert(directedCycle(5, g).empty());
    // random directed graphs: Kahn detects a cycle iff DFS coloring does; returned cycle is valid
    for (int iter = 0; iter < 300; iter++) {
        int n = rnd(1, 7), m = rnd(0, 10);
        vector<vector<int>> g2(n);
        for (int i = 0; i < m; i++) g2[rnd(0, n - 1)].push_back(rnd(0, n - 1));
        auto cyc = directedCycle(n, g2);
        assert(((int)topoKahn(n, g2).size() < n) == !cyc.empty());
        if (!cyc.empty()) {
            assert(cyc.size() >= 2 && cyc.front() == cyc.back());
            for (size_t i = 1; i < cyc.size(); i++) assert(count(g2[cyc[i - 1]].begin(), g2[cyc[i - 1]].end(), cyc[i]) > 0);
        }
    }
    // undirected cycle: exists iff m > n - components (tree test via DSU); check multi-edge handling
    for (int iter = 0; iter < 300; iter++) {
        int n = rnd(1, 7), m = rnd(0, 9);
        vector<vector<pair<int,int>>> g2(n); DSU d(n); bool hasCycle = false;
        for (int i = 0; i < m; i++) {
            int a = rnd(0, n - 1), b = rnd(0, n - 1);
            g2[a].push_back({b, i}); g2[b].push_back({a, i});
            if (!d.unite(a, b)) hasCycle = true;       // self-loops and multi-edges are cycles
        }
        auto cyc = undirectedCycle(n, g2);
        assert(hasCycle == !cyc.empty());
    }
    // Hamiltonian paths brute force (permutations)
    for (int iter = 0; iter < 30; iter++) {
        int n = rnd(2, 6);
        vector<vector<int>> g2(n); vector<vector<char>> adj(n, vector<char>(n, 0));
        int m = rnd(0, n * n);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); if (a != b) { g2[a].push_back(b); adj[a][b] = 1; } }
        // dedupe for the brute force: multi-edges count as separate routes in the DP too, so keep them
        vector<int> perm(n); iota(perm.begin(), perm.end(), 0);
        ll brute = 0;
        do {
            if (perm[0] != 0 || perm[n - 1] != n - 1) continue;
            ll ways = 1;
            for (int i = 1; i < n; i++) ways *= count(g2[perm[i - 1]].begin(), g2[perm[i - 1]].end(), perm[i]);
            brute += ways;
        } while (next_permutation(perm.begin(), perm.end()));
        assert(countHamiltonianPaths(n, g2) == brute % MOD);
    }
}

static void testEuler() {
    // directed: random Eulerian circuit built by a random closed walk, plus random non-Eulerian ones
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 6), m = rnd(1, 12);
        vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) edges.push_back({rnd(0, n - 1), rnd(0, n - 1)});
        int s = edges[0].first;
        for (int t = 0; t < n; t++) {
            auto p = eulerPathDirected(n, edges, s, t);
            // verify: if a path is returned it uses every edge exactly once
            if (!p.empty()) {
                assert((int)p.size() == m + 1 && p.front() == s && p.back() == t);
                multiset<pair<int,int>> ms(edges.begin(), edges.end());
                for (size_t i = 1; i < p.size(); i++) { auto it = ms.find({p[i - 1], p[i]}); assert(it != ms.end()); ms.erase(it); }
                assert(ms.empty());
            } else {
                // brute: an Euler path exists iff degrees balance AND all edges reachable from s
                vector<int> in(n, 0), out(n, 0); bool ok = true;
                for (auto [a, b] : edges) { out[a]++; in[b]++; }
                for (int v = 0; v < n; v++) if (out[v] - in[v] != (v == s) - (v == t)) ok = false;
                if (ok) {                                // check weak connectivity of edge set to s
                    DSU d(n); for (auto [a, b] : edges) d.unite(a, b);
                    for (auto [a, b] : edges) if (d.find(a) != d.find(s)) ok = false;
                }
                assert(!ok);
            }
        }
    }
    // undirected
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 6), m = rnd(1, 10);
        vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) edges.push_back({rnd(0, n - 1), rnd(0, n - 1)});
        int s = edges[0].first;
        for (int t = 0; t < n; t++) {
            auto p = eulerPathUndirected(n, edges, s, t);
            if (!p.empty()) {
                assert((int)p.size() == m + 1 && p.front() == s && p.back() == t);
                multiset<pair<int,int>> ms;
                for (auto [a, b] : edges) ms.insert(minmax(a, b));
                for (size_t i = 1; i < p.size(); i++) { auto it = ms.find(minmax(p[i - 1], p[i])); assert(it != ms.end()); ms.erase(it); }
                assert(ms.empty());
            } else {
                vector<int> deg(n, 0); bool ok = true;
                for (auto [a, b] : edges) { deg[a]++; deg[b]++; }
                for (int v = 0; v < n; v++) if ((deg[v] & 1) != ((s != t) && (v == s || v == t))) ok = false;
                if (ok) { DSU d(n); for (auto [a, b] : edges) d.unite(a, b); for (auto [a, b] : edges) if (d.find(a) != d.find(s)) ok = false; }
                assert(!ok);
            }
        }
    }
}

static void testSCCand2SAT() {
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 8), m = rnd(0, 16);
        TarjanSCC t(n); KosarajuSCC k(n); Floyd fl(n);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); t.add(a, b); k.add(a, b); fl.add(a, b, 1); }
        t.run(); k.run(); fl.run();
        assert(t.ncomp == k.ncomp);
        for (int u = 0; u < n; u++) for (int v = 0; v < n; v++) {
            bool same = fl.d[u][v] < INF && fl.d[v][u] < INF;   // mutual reachability
            assert((t.comp[u] == t.comp[v]) == same);
            assert((k.comp[u] == k.comp[v]) == same);
        }
        // Tarjan numbering: edges go from higher comp id to lower (reverse topological)
        for (int u = 0; u < n; u++) for (int v : t.g[u]) assert(t.comp[u] >= t.comp[v]);
        // Kosaraju numbering: edges go from lower comp id to higher (topological)
        for (int u = 0; u < n; u++) for (int v : k.g[u]) assert(k.comp[u] <= k.comp[v]);
        auto c = t.condensation();
        assert(topoKahn(t.ncomp, c).size() == (size_t)t.ncomp);  // condensation is a DAG
    }
    // 2-SAT vs brute force over all assignments
    for (int iter = 0; iter < 300; iter++) {
        int n = rnd(1, 5), m = rnd(0, 10);
        vector<pair<int,int>> clauses;
        TwoSAT ts(n);
        for (int i = 0; i < m; i++) { int a = rnd(0, 2 * n - 1), b = rnd(0, 2 * n - 1); clauses.push_back({a, b}); ts.addClause(a, b); }
        bool bruteSat = false;
        for (int mask = 0; mask < (1 << n); mask++) {
            auto lit = [&](int l) { return ((mask >> (l / 2)) & 1) ^ (l & 1); };
            bool ok = true;
            for (auto [a, b] : clauses) if (!lit(a) && !lit(b)) ok = false;
            if (ok) bruteSat = true;
        }
        bool sat = ts.solve();
        assert(sat == bruteSat);
        if (sat) {
            auto lit = [&](int l) { return (int)ts.value[l / 2] ^ (l & 1); };
            for (auto [a, b] : clauses) assert(lit(a) || lit(b));
        }
    }
}

static void testBridges() {
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 8), m = rnd(0, 12);
        BridgesAP br(n);
        vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); br.add(a, b); edges.push_back({a, b}); }
        br.run();
        auto components = [&](int skipEdge, int skipVertex) {
            DSU d(n); int c = n - (skipVertex != -1);
            for (int i = 0; i < m; i++) {
                if (i == skipEdge) continue;
                auto [a, b] = edges[i];
                if (a == skipVertex || b == skipVertex) continue;
                if (d.unite(a, b)) c--;
            }
            return c;
        };
        int base = components(-1, -1);
        for (int i = 0; i < m; i++) assert((bool)br.isBridge[i] == (components(i, -1) > base));
        for (int v = 0; v < n; v++) assert((bool)br.isAP[v] == (components(-1, v) > base));
        // 2-edge-connected components: same comp iff still connected after removing any single edge
        for (int u = 0; u < n; u++) for (int v = 0; v < n; v++) {
            bool always = true;
            for (int i = -1; i < m; i++) {
                DSU d(n);
                for (int j = 0; j < m; j++) if (j != i) d.unite(edges[j].first, edges[j].second);
                if (d.find(u) != d.find(v)) always = false;
            }
            assert((br.comp[u] == br.comp[v]) == always);
        }
        auto bt = br.bridgeTree();
        int bridges = count(br.isBridge.begin(), br.isBridge.end(), 1);
        size_t deg = 0; for (auto& v : bt) deg += v.size();
        assert((int)deg == 2 * bridges);
    }
}

static void testMST() {
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 8), m = rnd(0, 14);
        vector<MSTEdge> es; vector<vector<WEdge>> g(n);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); ll w = rnd(1, 20); es.push_back({a, b, w}); g[a].push_back({b, w}); g[b].push_back({a, w}); }
        auto [kw, chosen] = kruskal(n, es);
        ll pw = prim(n, g), bw = boruvka(n, es);
        bool connected = (int)chosen.size() == n - 1;
        if (connected) assert(kw == pw && kw == bw);
        else assert(pw == INF && bw == INF);
        // second-best MST vs brute force over all spanning trees (n small)
        if (connected && m <= 10) {
            int chosenMask = 0;
            for (int i : chosen) chosenMask |= 1 << i;
            ll bestOther = INF, bestStrict = INF, minW = INF;
            for (int mask = 0; mask < (1 << m); mask++) {
                if (__builtin_popcount(mask) != n - 1) continue;
                DSU d(n); ll w = 0; bool ok = true;
                for (int i = 0; i < m; i++) if (mask >> i & 1) { if (!d.unite(es[i].a, es[i].b)) { ok = false; break; } w += es[i].w; }
                if (!ok) continue;
                minW = min(minW, w);
                if (mask != chosenMask) bestOther = min(bestOther, w);
                if (w > kw) bestStrict = min(bestStrict, w);
            }
            assert(minW == kw);
            assert(secondBestMST(n, es, false) == bestOther);
            assert(secondBestMST(n, es, true) == bestStrict);
        }
        // Kruskal reconstruction tree: bottleneck vs brute (Floyd with max instead of sum)
        KruskalTree kt(n, es);
        vector<vector<ll>> mm(n, vector<ll>(n, INF));
        for (int i = 0; i < n; i++) mm[i][i] = 0;
        for (auto& e : es) { mm[e.a][e.b] = min(mm[e.a][e.b], e.w); mm[e.b][e.a] = min(mm[e.b][e.a], e.w); }
        for (int k = 0; k < n; k++) for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) mm[i][j] = min(mm[i][j], max(mm[i][k], mm[k][j]));
        for (int u = 0; u < n; u++) for (int v = 0; v < n; v++) assert(kt.bottleneck(u, v) == mm[u][v]);
    }
}

static void testFunctionalAndBipartite() {
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 10);
        vector<int> nxt(n);
        for (int& x : nxt) x = rnd(0, n - 1);
        Successor s(nxt, 1000);
        for (int v = 0; v < n; v++) {
            int k = rnd(0, 1000), u = v;
            for (int i = 0; i < k; i++) u = nxt[u];
            assert(s.kth(v, k) == u);
        }
        auto pc = planetsCycles(nxt);
        for (int v = 0; v < n; v++) {
            vector<char> seen(n, 0); int u = v, steps = 0;
            while (!seen[u]) { seen[u] = 1; u = nxt[u]; steps++; }
            assert(pc[v] == steps);
        }
    }
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 8), m = rnd(0, 12);
        vector<vector<int>> g(n); vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); g[a].push_back(b); g[b].push_back(a); edges.push_back({a, b}); }
        auto col = bipartiteColor(n, g);
        bool brute = false;
        for (int mask = 0; mask < (1 << n) && !brute; mask++) {
            bool ok = true;
            for (auto [a, b] : edges) if (((mask >> a) & 1) == ((mask >> b) & 1)) ok = false;
            if (ok) brute = true;
        }
        assert(brute == !col.empty() || (n == 0));
        if (!col.empty()) for (auto [a, b] : edges) assert(col[a] != col[b]);
    }
}

int main() {
    testShortestPaths();
    testDagAndCycles();
    testEuler();
    testSCCand2SAT();
    testBridges();
    testMST();
    testFunctionalAndBipartite();
    cout << "all chapter 11 tests passed\n";
    return 0;
}
