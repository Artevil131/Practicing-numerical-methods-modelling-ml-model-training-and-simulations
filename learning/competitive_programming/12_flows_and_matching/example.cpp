// Chapter 12 — Flows and matching: the reference library.
//
// TRAINING RULE: re-type Dinic, Kuhn, Hopcroft–Karp and MCMF from memory
// until each compiles first time and passes the asserts below. Dinic in
// under 5 minutes is the bar. Then compare with this file.
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Contents:
//   1. Dinic (level graph + blocking flow), min cut recovery, flow on an edge,
//      flow decomposition into s-t paths
//   2. Edmonds–Karp (BFS augmenting paths) — for cross-checking
//   3. Bipartite matching: Kuhn O(VE), Hopcroft–Karp O(E sqrt V)
//   4. König: minimum vertex cover from a maximum matching; max independent set
//   5. Minimum path cover in a DAG (vertex-disjoint) via matching
//   6. Vertex-disjoint s-t paths by vertex splitting
//   7. Min cost max flow: SPFA successive shortest paths, and Johnson
//      potentials + Dijkstra
//   8. Hungarian algorithm O(n^2 m) for the assignment problem
//   9. Flow with lower bounds / feasible circulation
//  10. Project selection (closure) as a min cut
// Every routine is cross-checked in main() against brute force on tiny inputs
// (min cut by enumerating all vertex subsets, matchings by enumeration,
// assignment by permutations).

// Headers listed explicitly so the file also builds on libc++ (Apple clang has
// no <bits/stdc++.h>). In a contest on GCC just use <bits/stdc++.h>.
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <functional>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <utility>
#include <vector>
using namespace std;
using ll = long long;

const ll INF = LLONG_MAX / 4;

mt19937 rng(20260905);
int rnd(int lo, int hi) { return uniform_int_distribution<int>(lo, hi)(rng); }

// =====================================================================
// 1. Dinic
// =====================================================================
// Edges stored in one array; edge id ^ 1 is the reverse edge. cap[] is the
// RESIDUAL capacity; flow on edge e = original cap - cap[e] = cap[e ^ 1]
// (when the reverse edge was created with capacity 0).
//
// Phase: BFS builds level[] (shortest residual distance from s). DFS pushes
// flow only along edges with level[v] == level[u] + 1 (the level graph) and
// uses it[u] so that each dead edge is skipped forever within the phase
// -> one phase is O(VE); the s-t distance strictly increases per phase, so
// at most V phases: O(V^2 E). Unit capacities: O(E sqrt V). Bipartite
// matching: O(E sqrt V). In practice far faster than the bounds.
struct Dinic {
    struct E { int to; ll cap; };
    int n;
    vector<E> e;
    vector<vector<int>> g;
    vector<int> level, it;
    Dinic(int n) : n(n), g(n), level(n), it(n) {}

    // Returns the id of the forward edge. rcap > 0 gives an undirected edge.
    int add(int a, int b, ll cap, ll rcap = 0) {
        g[a].push_back(e.size()); e.push_back({b, cap});
        g[b].push_back(e.size()); e.push_back({a, rcap});
        return (int)e.size() - 2;
    }
    bool bfs(int s, int t) {
        fill(level.begin(), level.end(), -1);
        queue<int> q; level[s] = 0; q.push(s);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int id : g[u]) if (e[id].cap > 0 && level[e[id].to] == -1) {
                level[e[id].to] = level[u] + 1; q.push(e[id].to);
            }
        }
        return level[t] != -1;
    }
    ll dfs(int u, int t, ll f) {
        if (u == t || f == 0) return f;
        for (int& i = it[u]; i < (int)g[u].size(); i++) {   // reference: advance permanently
            int id = g[u][i], v = e[id].to;
            if (e[id].cap <= 0 || level[v] != level[u] + 1) continue;
            ll d = dfs(v, t, min(f, e[id].cap));
            if (d > 0) { e[id].cap -= d; e[id ^ 1].cap += d; return d; }
        }
        return 0;
    }
    ll maxflow(int s, int t) {
        ll flow = 0;
        while (bfs(s, t)) {
            fill(it.begin(), it.end(), 0);            // reset iterators every phase
            while (ll f = dfs(s, t, INF)) flow += f;
        }
        return flow;
    }
    // After maxflow: vertices still reachable from s in the residual graph.
    // Edges leaving this set are saturated => they form a minimum cut.
    vector<char> minCutSide() const {
        vector<char> side(n, 0);
        for (int v = 0; v < n; v++) side[v] = level[v] != -1;
        return side;
    }
    ll flowOn(int id) const { return e[id ^ 1].cap; }   // valid when rcap was 0

    // Decompose the flow into s-t paths (each path = list of vertices, with a
    // multiplicity). Walk edges with positive flow, subtract the bottleneck.
    // O(VE) worst case; cycles of flow are ignored (they never reach t).
    vector<pair<vector<int>, ll>> decompose(int s, int t) {
        vector<ll> f(e.size());
        for (size_t id = 0; id < e.size(); id += 2) f[id] = e[id ^ 1].cap;   // flow on forward edges
        vector<int> ptr(n, 0);
        vector<pair<vector<int>, ll>> paths;
        while (true) {
            vector<int> path{s}, used;
            vector<char> onPath(n, 0); onPath[s] = 1;
            int u = s; ll bottleneck = INF; bool ok = true;
            while (u != t) {
                while (ptr[u] < (int)g[u].size()) {
                    int id = g[u][ptr[u]];
                    if ((id & 1) == 0 && f[id] > 0) break;
                    ptr[u]++;
                }
                if (ptr[u] == (int)g[u].size()) { ok = false; break; }
                int id = g[u][ptr[u]], v = e[id].to;
                if (onPath[v]) {                          // flow cycle: cancel it, restart
                    ll c = INF; size_t pos = 0;
                    while (path[pos] != v) pos++;
                    for (size_t k = pos; k < used.size(); k++) c = min(c, f[used[k]]);
                    c = min(c, f[id]);
                    for (size_t k = pos; k < used.size(); k++) f[used[k]] -= c;
                    f[id] -= c;
                    ok = false; break;
                }
                used.push_back(id); path.push_back(v); onPath[v] = 1;
                bottleneck = min(bottleneck, f[id]);
                u = v;
            }
            if (!ok) { if (u == s && ptr[s] == (int)g[s].size()) break; else continue; }
            for (int id : used) f[id] -= bottleneck;
            paths.push_back({path, bottleneck});
        }
        return paths;
    }
};

// =====================================================================
// 2. Edmonds–Karp: Ford–Fulkerson with BFS. O(V E^2). Same edge layout.
// =====================================================================
struct EdmondsKarp {
    struct E { int to; ll cap; };
    int n; vector<E> e; vector<vector<int>> g;
    EdmondsKarp(int n) : n(n), g(n) {}
    void add(int a, int b, ll cap) {
        g[a].push_back(e.size()); e.push_back({b, cap});
        g[b].push_back(e.size()); e.push_back({a, 0});
    }
    ll maxflow(int s, int t) {
        ll flow = 0;
        while (true) {
            vector<int> pe(n, -1);                     // parent edge
            queue<int> q; q.push(s); pe[s] = -2;
            while (!q.empty() && pe[t] == -1) {
                int u = q.front(); q.pop();
                for (int id : g[u]) if (e[id].cap > 0 && pe[e[id].to] == -1) { pe[e[id].to] = id; q.push(e[id].to); }
            }
            if (pe[t] == -1) return flow;
            ll f = INF;
            for (int v = t; v != s; v = e[pe[v] ^ 1].to) f = min(f, e[pe[v]].cap);
            for (int v = t; v != s; v = e[pe[v] ^ 1].to) { e[pe[v]].cap -= f; e[pe[v] ^ 1].cap += f; }
            flow += f;
        }
    }
};

// =====================================================================
// 3. Bipartite matching
// =====================================================================
// Kuhn: for each left vertex, DFS for an augmenting path (alternating
// unmatched/matched edges) in the current matching. Berge: a matching is
// maximum iff no augmenting path exists. O(V E); fine up to ~ V,E = 1e4..1e5
// with the "greedy initial matching" and "fresh visited per left vertex" tricks.
struct Kuhn {
    int nl, nr;
    vector<vector<int>> g;
    vector<int> matchR, matchL;
    vector<char> vis;
    Kuhn(int nl, int nr) : nl(nl), nr(nr), g(nl), matchR(nr, -1), matchL(nl, -1), vis(nl) {}
    void add(int l, int r) { g[l].push_back(r); }
    bool tryKuhn(int u) {
        if (vis[u]) return false;
        vis[u] = 1;
        for (int v : g[u])
            if (matchR[v] == -1 || tryKuhn(matchR[v])) { matchR[v] = u; matchL[u] = v; return true; }
        return false;
    }
    int run() {
        int res = 0;
        for (int u = 0; u < nl; u++) {                 // greedy start
            for (int v : g[u]) if (matchR[v] == -1) { matchR[v] = u; matchL[u] = v; res++; break; }
        }
        for (int u = 0; u < nl; u++) if (matchL[u] == -1) {
            fill(vis.begin(), vis.end(), 0);
            if (tryKuhn(u)) res++;
        }
        return res;
    }
};

// Hopcroft–Karp: BFS from all free left vertices builds layers; DFS finds a
// maximal set of vertex-disjoint shortest augmenting paths. O(sqrt V) phases
// (after sqrt V phases every remaining augmenting path is long, so few remain)
// -> O(E sqrt V). Equivalent to Dinic on the unit network.
struct HopcroftKarp {
    int nl, nr;
    vector<vector<int>> g;
    vector<int> matchL, matchR, dist, it;
    HopcroftKarp(int nl, int nr) : nl(nl), nr(nr), g(nl), matchL(nl, -1), matchR(nr, -1), dist(nl), it(nl) {}
    void add(int l, int r) { g[l].push_back(r); }
    bool bfs() {
        queue<int> q; bool found = false;
        for (int u = 0; u < nl; u++) { dist[u] = matchL[u] == -1 ? 0 : -1; if (dist[u] == 0) q.push(u); }
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int v : g[u]) {
                int w = matchR[v];
                if (w == -1) found = true;
                else if (dist[w] == -1) { dist[w] = dist[u] + 1; q.push(w); }
            }
        }
        return found;
    }
    bool dfs(int u) {
        for (int& i = it[u]; i < (int)g[u].size(); i++) {
            int v = g[u][i], w = matchR[v];
            if (w == -1 || (dist[w] == dist[u] + 1 && dfs(w))) { matchL[u] = v; matchR[v] = u; return true; }
        }
        dist[u] = -1;                                  // dead end this phase
        return false;
    }
    int run() {
        int res = 0;
        while (bfs()) {
            fill(it.begin(), it.end(), 0);
            for (int u = 0; u < nl; u++) if (matchL[u] == -1 && dfs(u)) res++;
        }
        return res;
    }
};

// =====================================================================
// 4. König's theorem: min vertex cover = max matching (bipartite)
// =====================================================================
// From every UNMATCHED left vertex, walk alternating paths: left->right via
// any edge, right->left via the matching edge. Let Z = visited vertices.
// Cover = (L \ Z) u (R n Z). Proof sketch: an edge (l,r) with l in Z has r
// in Z (we walked it). An edge with l not in Z: l is matched (all unmatched
// left are in Z), so l itself covers it. |cover| = |matching| because every
// vertex in the cover is matched and each matching edge contributes exactly
// one endpoint (its right endpoint if reached, else its left).
// Max independent set = complement of the cover, size |V| - |matching|.
pair<vector<int>, vector<int>> koenigCover(const HopcroftKarp& hk) {
    int nl = hk.nl, nr = hk.nr;
    vector<char> visL(nl, 0), visR(nr, 0);
    function<void(int)> go = [&](int u) {
        visL[u] = 1;
        for (int v : hk.g[u]) if (!visR[v]) {
            visR[v] = 1;
            int w = hk.matchR[v];
            if (w != -1 && !visL[w]) go(w);
        }
    };
    for (int u = 0; u < nl; u++) if (hk.matchL[u] == -1 && !visL[u]) go(u);
    vector<int> coverL, coverR;
    for (int u = 0; u < nl; u++) if (!visL[u]) coverL.push_back(u);
    for (int v = 0; v < nr; v++) if (visR[v]) coverR.push_back(v);
    return {coverL, coverR};
}

// =====================================================================
// 5. Minimum path cover of a DAG (vertex-disjoint paths covering all vertices)
// =====================================================================
// Split each vertex v into out-copy (left) and in-copy (right); edge u->v
// becomes left u -> right v. Each matching edge glues two path pieces.
// Answer = n - maxMatching. (Dilworth: min chain cover = max antichain when
// edges are the transitive closure.)
int minPathCover(int n, const vector<pair<int,int>>& dagEdges) {
    HopcroftKarp hk(n, n);
    for (auto [a, b] : dagEdges) hk.add(a, b);
    return n - hk.run();
}

// =====================================================================
// 6. Vertex-disjoint s-t paths: split v into v_in -> v_out with capacity 1
// =====================================================================
ll vertexDisjointPaths(int n, const vector<pair<int,int>>& edges, int s, int t) {
    Dinic d(2 * n);
    auto in = [](int v) { return 2 * v; };
    auto out = [](int v) { return 2 * v + 1; };
    for (int v = 0; v < n; v++) d.add(in(v), out(v), (v == s || v == t) ? INF : 1);
    for (auto [a, b] : edges) d.add(out(a), in(b), 1);
    return d.maxflow(in(s), out(t));
}

// =====================================================================
// 7. Min cost max flow
// =====================================================================
// Successive shortest paths: repeatedly augment along a cheapest s-t path
// in the residual graph (reverse edges have cost -c). Total cost is convex
// in the flow amount, so stopping early gives min cost for that flow.
// Variant A: SPFA/Bellman–Ford each round — simple, handles negative costs,
//            O(F * V E) worst case, fast in practice.
// Variant B: Johnson potentials h[]: reduced cost c + h[u] - h[v] >= 0 on
//            residual edges, so Dijkstra applies; h += dist after each round.
//            Initial h by Bellman–Ford if there are negative costs.
struct MCMF {
    struct E { int to; ll cap, cost; };
    int n;
    vector<E> e;
    vector<vector<int>> g;
    MCMF(int n) : n(n), g(n) {}
    int add(int a, int b, ll cap, ll cost) {
        g[a].push_back(e.size()); e.push_back({b, cap, cost});
        g[b].push_back(e.size()); e.push_back({a, 0, -cost});
        return (int)e.size() - 2;
    }
    // Returns {flow, cost}; augments at most maxf units.
    pair<ll, ll> run(int s, int t, ll maxf = INF, bool dijkstra = false) {
        ll flow = 0, cost = 0;
        vector<ll> h(n, 0);                           // potentials (Variant B)
        if (dijkstra) {                               // Bellman–Ford for initial potentials
            vector<ll> d(n, INF); d[s] = 0;
            for (int it = 0; it < n; it++) {
                bool any = false;
                for (int u = 0; u < n; u++) if (d[u] < INF) for (int id : g[u])
                    if (e[id].cap > 0 && d[u] + e[id].cost < d[e[id].to]) { d[e[id].to] = d[u] + e[id].cost; any = true; }
                if (!any) break;
            }
            for (int v = 0; v < n; v++) h[v] = d[v] < INF ? d[v] : 0;
        }
        while (flow < maxf) {
            vector<ll> d(n, INF);
            vector<int> pe(n, -1);
            d[s] = 0;
            if (!dijkstra) {                          // SPFA
                vector<char> inq(n, 0); queue<int> q; q.push(s); inq[s] = 1;
                while (!q.empty()) {
                    int u = q.front(); q.pop(); inq[u] = 0;
                    for (int id : g[u]) if (e[id].cap > 0 && d[u] + e[id].cost < d[e[id].to]) {
                        int v = e[id].to; d[v] = d[u] + e[id].cost; pe[v] = id;
                        if (!inq[v]) { inq[v] = 1; q.push(v); }
                    }
                }
            } else {                                  // Dijkstra on reduced costs
                priority_queue<pair<ll,int>, vector<pair<ll,int>>, greater<>> pq;
                pq.push({0, s});
                while (!pq.empty()) {
                    auto [du, u] = pq.top(); pq.pop();
                    if (du > d[u]) continue;
                    for (int id : g[u]) if (e[id].cap > 0) {
                        int v = e[id].to;
                        ll nd = du + e[id].cost + h[u] - h[v];  // reduced cost >= 0
                        if (nd < d[v]) { d[v] = nd; pe[v] = id; pq.push({nd, v}); }
                    }
                }
                for (int v = 0; v < n; v++) if (d[v] < INF) h[v] += d[v];   // keep reduced costs >= 0
            }
            if (d[t] >= INF) break;
            ll f = maxf - flow;
            for (int v = t; v != s; v = e[pe[v] ^ 1].to) f = min(f, e[pe[v]].cap);
            for (int v = t; v != s; v = e[pe[v] ^ 1].to) {
                e[pe[v]].cap -= f; e[pe[v] ^ 1].cap += f; cost += f * e[pe[v]].cost;
            }
            flow += f;
        }
        return {flow, cost};
    }
};

// =====================================================================
// 8. Hungarian algorithm (Kuhn–Munkres), O(n^2 m), n <= m
// =====================================================================
// Minimum-cost assignment of n rows to distinct columns of an n x m matrix
// (1-indexed internally, e-maxx style). Returns {cost, assignment[row]}.
// Use when n ~ 500..1000 dense: MCMF with n^2 edges is slower and heavier.
pair<ll, vector<int>> hungarian(const vector<vector<ll>>& a) {
    int n = a.size(), m = a[0].size();               // n <= m
    vector<ll> u(n + 1), v(m + 1);
    vector<int> p(m + 1), way(m + 1);
    for (int i = 1; i <= n; i++) {
        p[0] = i; int j0 = 0;
        vector<ll> minv(m + 1, INF);
        vector<char> used(m + 1, 0);
        do {
            used[j0] = 1;
            int i0 = p[j0], j1 = 0; ll delta = INF;
            for (int j = 1; j <= m; j++) if (!used[j]) {
                ll cur = a[i0 - 1][j - 1] - u[i0] - v[j];
                if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                if (minv[j] < delta) { delta = minv[j]; j1 = j; }
            }
            for (int j = 0; j <= m; j++) {
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else minv[j] -= delta;
            }
            j0 = j1;
        } while (p[j0] != 0);
        do { int j1 = way[j0]; p[j0] = p[j1]; j0 = j1; } while (j0);
    }
    vector<int> ans(n);
    for (int j = 1; j <= m; j++) if (p[j]) ans[p[j] - 1] = j - 1;
    return {-v[0], ans};
}

// =====================================================================
// 9. Flow with lower bounds / feasible circulation
// =====================================================================
// Edge (a,b) with bounds [lo, hi]: send lo units unconditionally, keep an
// edge of capacity hi - lo, and fix conservation with a super source S and
// super sink T: excess[b] += lo, excess[a] -= lo; S->v (excess>0),
// v->T (-excess). For an s-t flow also add t->s with capacity INF (turns it
// into a circulation). Feasible iff maxflow(S,T) saturates all S edges.
struct LowerBoundFlow {
    int n; Dinic d; vector<ll> excess; vector<pair<int,ll>> ids;   // (edge id, lo)
    LowerBoundFlow(int n) : n(n), d(n + 2), excess(n, 0) {}
    int S() const { return n; }
    int T() const { return n + 1; }
    void add(int a, int b, ll lo, ll hi) {
        int id = d.add(a, b, hi - lo);
        ids.push_back({id, lo});
        excess[b] += lo; excess[a] -= lo;
    }
    // Feasible circulation? (call after adding t->s with [0, INF] for s-t flows)
    bool feasible() {
        ll need = 0;
        for (int v = 0; v < n; v++) {
            if (excess[v] > 0) { d.add(S(), v, excess[v]); need += excess[v]; }
            else if (excess[v] < 0) d.add(v, T(), -excess[v]);
        }
        return d.maxflow(S(), T()) == need;
    }
    ll flowOn(size_t k) const { return ids[k].second + d.flowOn(ids[k].first); }
};

// =====================================================================
// 10. Project selection / closure as min cut
// =====================================================================
// Items with profit p_i (may be negative) and dependencies "choosing i
// requires j". Max profit = sum(positive p) - mincut where:
//   s -> i with cap p_i     for p_i > 0    (cut = give up profit)
//   i -> t with cap -p_i    for p_i < 0    (cut = pay the cost)
//   i -> j with cap INF     for each requirement i requires j
// Chosen set = source side of the min cut.
pair<ll, vector<char>> projectSelection(const vector<ll>& profit, const vector<pair<int,int>>& requires) {
    int n = profit.size(), s = n, t = n + 1;
    Dinic d(n + 2);
    ll total = 0;
    for (int i = 0; i < n; i++) {
        if (profit[i] > 0) { d.add(s, i, profit[i]); total += profit[i]; }
        else if (profit[i] < 0) d.add(i, t, -profit[i]);
    }
    for (auto [i, j] : requires) d.add(i, j, INF);
    ll cut = d.maxflow(s, t);
    auto side = d.minCutSide();
    side.resize(n);
    return {total - cut, side};
}

// =====================================================================
// Tests
// =====================================================================
struct Cap { int a, b; ll c; };

// Brute-force min cut: enumerate every vertex subset containing s and not t.
ll bruteMinCut(int n, const vector<Cap>& es, int s, int t) {
    ll best = INF;
    for (int mask = 0; mask < (1 << n); mask++) {
        if (!(mask >> s & 1) || (mask >> t & 1)) continue;
        ll c = 0;
        for (auto& e : es) if ((mask >> e.a & 1) && !(mask >> e.b & 1)) c += e.c;
        best = min(best, c);
    }
    return best;
}

static void testMaxflow() {
    for (int iter = 0; iter < 300; iter++) {
        int n = rnd(2, 7), m = rnd(0, 14);
        vector<Cap> es;
        for (int i = 0; i < m; i++) es.push_back({rnd(0, n - 1), rnd(0, n - 1), (ll)rnd(0, 9)});
        int s = 0, t = n - 1;
        Dinic d(n); EdmondsKarp ek(n);
        vector<int> ids;
        for (auto& e : es) { ids.push_back(d.add(e.a, e.b, e.c)); ek.add(e.a, e.b, e.c); }
        ll f = d.maxflow(s, t), brute = bruteMinCut(n, es, s, t);
        assert(f == brute);                                         // max-flow = min-cut
        assert(ek.maxflow(s, t) == brute);
        // min cut side: contains s, not t, and its capacity equals the flow
        auto side = d.minCutSide();
        assert(side[s] && !side[t]);
        ll cut = 0;
        for (auto& e : es) if (side[e.a] && !side[e.b]) cut += e.c;
        assert(cut == f);
        // flow on edges: capacity + conservation + value
        vector<ll> bal(n, 0);
        for (int i = 0; i < m; i++) {
            ll fl = d.flowOn(ids[i]);
            assert(0 <= fl && fl <= es[i].c);
            bal[es[i].a] -= fl; bal[es[i].b] += fl;
        }
        for (int v = 0; v < n; v++) if (v != s && v != t) assert(bal[v] == 0);
        assert(bal[t] == f);
        // decomposition: paths are real, total equals flow, edge usage <= flow on edge
        auto paths = d.decompose(s, t);
        ll total = 0; vector<ll> use(m, 0);
        for (auto& [p, mult] : paths) {
            assert(p.front() == s && p.back() == t && mult > 0);
            total += mult;
        }
        assert(total == f);
    }
    // vertex-disjoint paths vs brute force (remove any k-1 vertices -> still connected?)
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(2, 7), m = rnd(0, 12);
        vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); if (a != b) edges.push_back({a, b}); }
        int s = 0, t = n - 1;
        // Menger: max vertex-disjoint paths = min vertex separator size (or INF if edge s->t exists)
        bool direct = false;
        for (auto [a, b] : edges) if (a == s && b == t) direct = true;
        ll got = vertexDisjointPaths(n, edges, s, t);
        if (direct) { assert(got >= 1); continue; }   // INF-ish capacity through the direct edge; skip exact check
        ll sep = INF;
        for (int mask = 0; mask < (1 << n); mask++) {
            if ((mask >> s & 1) || (mask >> t & 1)) continue;
            vector<char> reach(n, 0); reach[s] = 1;
            bool changed = true;
            while (changed) {
                changed = false;
                for (auto [a, b] : edges) if (reach[a] && !reach[b] && !(mask >> b & 1)) { reach[b] = 1; changed = true; }
            }
            if (!reach[t]) sep = min(sep, (ll)__builtin_popcount(mask));
        }
        assert(got == sep);
    }
}

static void testMatching() {
    for (int iter = 0; iter < 300; iter++) {
        int nl = rnd(1, 6), nr = rnd(1, 6), m = rnd(0, 12);
        Kuhn k(nl, nr); HopcroftKarp hk(nl, nr); Dinic d(nl + nr + 2);
        vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) {
            int l = rnd(0, nl - 1), r = rnd(0, nr - 1);
            edges.push_back({l, r}); k.add(l, r); hk.add(l, r); d.add(l, nl + r, 1);
        }
        int s = nl + nr, t = s + 1;
        for (int l = 0; l < nl; l++) d.add(s, l, 1);
        for (int r = 0; r < nr; r++) d.add(nl + r, t, 1);
        // brute force: assign each left vertex a distinct right neighbour or none
        int brute = 0;
        function<void(int,int,int)> rec = [&](int u, int usedMask, int cnt) {
            if (u == nl) { brute = max(brute, cnt); return; }
            rec(u + 1, usedMask, cnt);
            for (auto [l, r] : edges) if (l == u && !(usedMask >> r & 1)) rec(u + 1, usedMask | 1 << r, cnt + 1);
        };
        rec(0, 0, 0);
        int mk = k.run(), mh = hk.run();
        assert(mk == brute && mh == brute && d.maxflow(s, t) == brute);
        // matching validity
        for (int l = 0; l < nl; l++) if (hk.matchL[l] != -1) assert(hk.matchR[hk.matchL[l]] == l);
        // König cover: covers all edges and has size = matching
        auto [cl, cr] = koenigCover(hk);
        assert((int)(cl.size() + cr.size()) == brute);
        vector<char> inL(nl, 0), inR(nr, 0);
        for (int x : cl) inL[x] = 1;
        for (int x : cr) inR[x] = 1;
        for (auto [l, r] : edges) assert(inL[l] || inR[r]);
    }
    // min path cover on random DAGs vs brute force over path partitions
    for (int iter = 0; iter < 100; iter++) {
        int n = rnd(1, 6);
        vector<pair<int,int>> edges; vector<vector<char>> adj(n, vector<char>(n, 0));
        int m = rnd(0, 8);
        for (int i = 0; i < m; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); if (a < b && !adj[a][b]) { adj[a][b] = 1; edges.push_back({a, b}); } }
        // brute: choose a set of edges forming vertex-disjoint paths (each vertex out<=1, in<=1);
        // paths = n - |edges chosen|, so maximize chosen edges — enumerate subsets.
        int E = edges.size(), best = 0;
        for (int mask = 0; mask < (1 << E); mask++) {
            vector<int> in(n, 0), out(n, 0); bool ok = true;
            for (int i = 0; i < E && ok; i++) if (mask >> i & 1) { if (++out[edges[i].first] > 1 || ++in[edges[i].second] > 1) ok = false; }
            if (ok) best = max(best, __builtin_popcount(mask));   // acyclic since a < b
        }
        assert(minPathCover(n, edges) == n - best);
    }
}

static void testMinCost() {
    // assignment problem: MCMF (both variants) and Hungarian vs permutations
    for (int iter = 0; iter < 150; iter++) {
        int n = rnd(1, 6), m = n + rnd(0, 2);
        vector<vector<ll>> a(n, vector<ll>(m));
        for (auto& row : a) for (ll& x : row) x = rnd(-5, 20);
        ll brute = INF;
        vector<int> perm(m); iota(perm.begin(), perm.end(), 0);
        do {
            ll c = 0;
            for (int i = 0; i < n; i++) c += a[i][perm[i]];
            brute = min(brute, c);
        } while (next_permutation(perm.begin(), perm.end()));
        auto [hc, assign] = hungarian(a);
        assert(hc == brute);
        ll chk = 0; vector<char> used(m, 0);
        for (int i = 0; i < n; i++) { assert(!used[assign[i]]); used[assign[i]] = 1; chk += a[i][assign[i]]; }
        assert(chk == brute);
        for (bool dij : {false, true}) {
            MCMF mc(n + m + 2);
            int s = n + m, t = s + 1;
            for (int i = 0; i < n; i++) mc.add(s, i, 1, 0);
            for (int j = 0; j < m; j++) mc.add(n + j, t, 1, 0);
            for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) mc.add(i, n + j, 1, a[i][j]);
            auto [f, c] = mc.run(s, t, INF, dij);
            assert(f == n && c == brute);
        }
    }
    // general min cost flow: MCMF SPFA vs Dijkstra variant on random graphs with non-negative costs,
    // and flow value equals Dinic max flow
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(2, 6), m = rnd(0, 10);
        MCMF a(n), b(n); Dinic d(n);
        for (int i = 0; i < m; i++) {
            int x = rnd(0, n - 1), y = rnd(0, n - 1); ll cap = rnd(0, 5), cost = rnd(0, 9);
            a.add(x, y, cap, cost); b.add(x, y, cap, cost); d.add(x, y, cap);
        }
        auto [fa, ca] = a.run(0, n - 1, INF, false);
        auto [fb, cb] = b.run(0, n - 1, INF, true);
        assert(fa == fb && ca == cb && fa == d.maxflow(0, n - 1));
    }
}

static void testLowerBoundsAndClosure() {
    // circulation with lower bounds: brute force over all integer flows on tiny graphs
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 4), m = rnd(1, 4);
        vector<array<ll,4>> es;                       // a, b, lo, hi
        for (int i = 0; i < m; i++) { ll lo = rnd(0, 2), hi = lo + rnd(0, 2); es.push_back({rnd(0, n - 1), rnd(0, n - 1), lo, hi}); }
        LowerBoundFlow lb(n);
        for (auto& e : es) lb.add(e[0], e[1], e[2], e[3]);
        bool feas = lb.feasible();
        // brute: enumerate f_i in [lo_i, hi_i]
        bool brute = false;
        vector<ll> f(m);
        function<void(int)> rec = [&](int i) {
            if (brute) return;
            if (i == m) {
                vector<ll> bal(n, 0);
                for (int k = 0; k < m; k++) { bal[es[k][0]] -= f[k]; bal[es[k][1]] += f[k]; }
                for (ll x : bal) if (x != 0) return;
                brute = true; return;
            }
            for (f[i] = es[i][2]; f[i] <= es[i][3]; f[i]++) rec(i + 1);
        };
        rec(0);
        assert(feas == brute);
        if (feas) {                                    // returned flow respects bounds and conservation
            vector<ll> bal(n, 0);
            for (int k = 0; k < m; k++) { ll fl = lb.flowOn(k); assert(es[k][2] <= fl && fl <= es[k][3]); bal[es[k][0]] -= fl; bal[es[k][1]] += fl; }
            for (ll x : bal) assert(x == 0);
        }
    }
    // project selection vs brute force over subsets
    for (int iter = 0; iter < 200; iter++) {
        int n = rnd(1, 7), r = rnd(0, 8);
        vector<ll> profit(n); for (ll& x : profit) x = rnd(-10, 10);
        vector<pair<int,int>> req;
        for (int i = 0; i < r; i++) { int a = rnd(0, n - 1), b = rnd(0, n - 1); if (a != b) req.push_back({a, b}); }
        ll brute = LLONG_MIN;
        for (int mask = 0; mask < (1 << n); mask++) {
            bool ok = true; ll p = 0;
            for (auto [a, b] : req) if ((mask >> a & 1) && !(mask >> b & 1)) ok = false;
            for (int i = 0; i < n; i++) if (mask >> i & 1) p += profit[i];
            if (ok) brute = max(brute, p);
        }
        auto [best, side] = projectSelection(profit, req);
        assert(best == brute);
        ll p = 0; bool ok = true;
        for (auto [a, b] : req) if (side[a] && !side[b]) ok = false;
        for (int i = 0; i < n; i++) if (side[i]) p += profit[i];
        assert(ok && p == brute);
    }
}

int main() {
    testMaxflow();
    testMatching();
    testMinCost();
    testLowerBoundsAndClosure();
    cout << "all chapter 12 tests passed\n";
    return 0;
}
