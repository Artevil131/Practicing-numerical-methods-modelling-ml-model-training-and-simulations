// Chapter 10 — Trees: reference library.
//
// TRAINING RULE: do not copy this file. Read it, close it, re-type each block
// from memory until it compiles and passes the same asserts. Binary-lifting
// LCA and HLD must come out of your fingers in < 5 minutes each.
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Contents (each cross-checked against a brute force on random trees):
//   1. Rooting a tree iteratively: parent / depth / preorder / subtree sizes
//   2. Diameter: two BFS, and DP with two best depths
//   3. Centroid(s) — vertex whose removal leaves components of size <= n/2
//   4. Euler tour (tin/tout) + Fenwick: subtree sum with point updates
//   5. LCA by binary lifting (+ k-th ancestor, jump to depth, distance)
//   6. LCA by Euler tour + sparse table (O(1) query)
//   7. Binary lifting with aggregated edge values (max edge on path)
//   8. Heavy-light decomposition: path max / point update / subtree via segtree
//   9. Centroid decomposition: count pairs at distance exactly k
//  10. Rerooting DP: sum of distances from every vertex
//  11. AHU canonical hashing: rooted / unrooted tree isomorphism
//  12. Kruskal reconstruction tree: minimax (bottleneck) path weight = LCA weight
//
// Headers listed explicitly so the file also builds on libc++ (Apple clang has
// no <bits/stdc++.h>). In a contest on GCC just use <bits/stdc++.h>.
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;
using ll = long long;

// ───────────────────────────────────────────────────────────────────────────
// 1. Rooting a tree without recursion.
//    order = preorder (parents before children) so a REVERSE sweep over order
//    computes any bottom-up DP (subtree sizes, sums...) and a FORWARD sweep
//    computes any top-down DP (depth, prefix sums to root).
// ───────────────────────────────────────────────────────────────────────────
struct Rooted {
    int n; vector<vector<int>> adj; vector<int> par, depth, order, sub;
    Rooted(const vector<vector<int>>& g, int root = 0) : n((int)g.size()), adj(g), par(n, -1), depth(n, 0), sub(n, 1) {
        vector<int> st = {root}; par[root] = -1;
        order.reserve(n);
        while (!st.empty()) {
            int v = st.back(); st.pop_back();
            order.push_back(v);
            for (int u : adj[v]) if (u != par[v]) { par[u] = v; depth[u] = depth[v] + 1; st.push_back(u); }
        }
        for (int i = n - 1; i > 0; --i) sub[par[order[i]]] += sub[order[i]];   // reverse preorder
    }
};

static vector<int> bfsDist(const vector<vector<int>>& g, int s) {
    vector<int> d(g.size(), -1); d[s] = 0; queue<int> q; q.push(s);
    while (!q.empty()) { int v = q.front(); q.pop(); for (int u : g[v]) if (d[u] < 0) { d[u] = d[v] + 1; q.push(u); } }
    return d;
}

// ───────────────────────────────────────────────────────────────────────────
// 2. Diameter.
//    (a) two BFS: farthest vertex from any vertex is an endpoint of a diameter.
//    (b) DP: for each v keep the two deepest child-depths; diameter through v
//        is their sum. Also returns the pair of endpoints for (a).
// ───────────────────────────────────────────────────────────────────────────
static tuple<int, int, int> diameterTwoBFS(const vector<vector<int>>& g) {
    auto d0 = bfsDist(g, 0);
    int a = (int)(max_element(d0.begin(), d0.end()) - d0.begin());
    auto da = bfsDist(g, a);
    int b = (int)(max_element(da.begin(), da.end()) - da.begin());
    return {da[b], a, b};
}
static int diameterDP(const Rooted& T) {
    int n = T.n; vector<int> best1(n, 0), best2(n, 0);   // two largest (child depth + 1)
    int diam = 0;
    for (int i = n - 1; i >= 0; --i) {
        int v = T.order[i];
        diam = max(diam, best1[v] + best2[v]);
        if (v == 0) break;
        int p = T.par[v], cand = best1[v] + 1;
        if (cand > best1[p]) { best2[p] = best1[p]; best1[p] = cand; }
        else if (cand > best2[p]) best2[p] = cand;
    }
    return diam;
}

// ───────────────────────────────────────────────────────────────────────────
// 3. Centroid: every component after removal has size <= n/2. There are 1 or 2
//    of them; walking from the root to the heavy child until the heavy child
//    has size <= n/2 finds one in O(n).
// ───────────────────────────────────────────────────────────────────────────
static int findCentroid(const Rooted& T) {
    int v = T.order[0];                                                    // root
    for (;;) {
        int heavy = -1;
        for (int u : T.adj[v]) if (u != T.par[v] && T.sub[u] > T.n / 2) heavy = u;
        if (heavy < 0) return v;
        v = heavy;
    }
}

// ───────────────────────────────────────────────────────────────────────────
// 4. Euler tour: tin[v] = index of v in preorder, tout[v] = tin[v] + sub[v].
//    Subtree of v == positions [tin[v], tout[v]). Any array structure on the
//    positions (Fenwick here) answers subtree queries.
// ───────────────────────────────────────────────────────────────────────────
struct Fenwick {
    int n; vector<ll> f;
    Fenwick(int n_) : n(n_), f(n_ + 1, 0) {}
    void add(int i, ll d) { for (++i; i <= n; i += i & -i) f[i] += d; }      // 0-indexed i
    ll prefix(int i) const { ll s = 0; for (; i > 0; i -= i & -i) s += f[i]; return s; } // sum [0, i)
    ll sum(int l, int r) const { return prefix(r) - prefix(l); }            // [l, r)
};
struct EulerTour {
    vector<int> tin, tout;
    EulerTour(const Rooted& T) : tin(T.n), tout(T.n) {
        for (int i = 0; i < T.n; ++i) tin[T.order[i]] = i;
        for (int v = 0; v < T.n; ++v) tout[v] = tin[v] + T.sub[v];
    }
    bool isAncestor(int a, int b) const { return tin[a] <= tin[b] && tout[b] <= tout[a]; }
};

// ───────────────────────────────────────────────────────────────────────────
// 5. LCA by binary lifting. up[k][v] = 2^k-th ancestor (root's parent = root).
//    Build O(n log n), query O(log n). Also k-th ancestor and jump to depth.
// ───────────────────────────────────────────────────────────────────────────
struct LCA {
    int n, LOG; vector<vector<int>> up; vector<int> depth;
    LCA(const Rooted& T) : n(T.n), depth(T.depth) {
        LOG = 1; while ((1 << LOG) < n) ++LOG;
        up.assign(LOG, vector<int>(n));
        for (int v = 0; v < n; ++v) up[0][v] = T.par[v] < 0 ? v : T.par[v];
        for (int k = 1; k < LOG; ++k) for (int v = 0; v < n; ++v) up[k][v] = up[k - 1][up[k - 1][v]];
    }
    int kth(int v, int k) const {                      // k-th ancestor, or -1 if it does not exist
        if (k > depth[v]) return -1;
        for (int i = 0; k; ++i, k >>= 1) if (k & 1) v = up[i][v];
        return v;
    }
    int lca(int a, int b) const {
        if (depth[a] < depth[b]) swap(a, b);
        a = kth(a, depth[a] - depth[b]);               // jump a to depth[b]
        if (a == b) return a;
        for (int k = LOG - 1; k >= 0; --k) if (up[k][a] != up[k][b]) { a = up[k][a]; b = up[k][b]; }
        return up[0][a];                               // a and b are now children of the LCA
    }
    int dist(int a, int b) const { int c = lca(a, b); return depth[a] + depth[b] - 2 * depth[c]; }
    // vertex at distance k from a along the path a -> b (k <= dist(a,b))
    int jump(int a, int b, int k) const {
        int c = lca(a, b), da = depth[a] - depth[c];
        if (k <= da) return kth(a, k);
        int total = da + depth[b] - depth[c];
        return kth(b, total - k);
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 6. LCA by Euler tour (enter every time we come back to v) + sparse table
//    over depths: lca(a,b) = shallowest vertex in euler[first[a]..first[b]].
//    Build O(n log n), query O(1).
// ───────────────────────────────────────────────────────────────────────────
struct LCA_RMQ {
    vector<int> euler, first, dep, lg; vector<vector<int>> sp;   // sp stores vertex with min depth
    LCA_RMQ(const Rooted& T) : first(T.n, -1), dep(T.depth) {
        // iterative DFS producing the 2n-1 length Euler tour
        vector<pair<int, int>> st = {{T.order[0], 0}};          // (vertex, next child index)
        while (!st.empty()) {
            auto& [v, i] = st.back();
            if (first[v] < 0) first[v] = (int)euler.size();
            euler.push_back(v);                                  // v is appended on EVERY visit (2n-1 total)
            while (i < (int)T.adj[v].size() && T.adj[v][i] == T.par[v]) ++i;
            if (i < (int)T.adj[v].size()) { int u = T.adj[v][i++]; st.push_back({u, 0}); }
            else st.pop_back();                                  // parent is re-appended by the next iteration
        }
        int m = (int)euler.size();
        lg.assign(m + 1, 0); for (int i = 2; i <= m; ++i) lg[i] = lg[i / 2] + 1;
        sp.assign(lg[m] + 1, vector<int>(m));
        sp[0] = euler;
        for (int k = 1; (1 << k) <= m; ++k)
            for (int i = 0; i + (1 << k) <= m; ++i) sp[k][i] = better(sp[k - 1][i], sp[k - 1][i + (1 << (k - 1))]);
    }
    int better(int a, int b) const { return dep[a] < dep[b] ? a : b; }
    int lca(int a, int b) const {
        int l = first[a], r = first[b]; if (l > r) swap(l, r);
        int k = lg[r - l + 1];
        return better(sp[k][l], sp[k][r - (1 << k) + 1]);
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 7. Binary lifting with aggregates: mx[k][v] = max edge weight on the 2^k
//    edges above v. Path max(a,b) = combine the two climbs to the LCA.
//    Works for any associative, commutative op (max/min/gcd/sum).
// ───────────────────────────────────────────────────────────────────────────
struct LiftMax {
    int n, LOG; vector<vector<int>> up; vector<vector<ll>> mx; vector<int> depth;
    LiftMax(const Rooted& T, const vector<ll>& wpar) : n(T.n), depth(T.depth) {   // wpar[v] = weight of edge (v, par[v])
        LOG = 1; while ((1 << LOG) < n) ++LOG;
        up.assign(LOG, vector<int>(n)); mx.assign(LOG, vector<ll>(n, LLONG_MIN));
        for (int v = 0; v < n; ++v) { up[0][v] = T.par[v] < 0 ? v : T.par[v]; mx[0][v] = T.par[v] < 0 ? LLONG_MIN : wpar[v]; }
        for (int k = 1; k < LOG; ++k) for (int v = 0; v < n; ++v) {
            int mid = up[k - 1][v];
            up[k][v] = up[k - 1][mid]; mx[k][v] = max(mx[k - 1][v], mx[k - 1][mid]);
        }
    }
    ll pathMax(int a, int b) const {
        ll res = LLONG_MIN;
        if (depth[a] < depth[b]) swap(a, b);
        for (int k = LOG - 1; k >= 0; --k) if (depth[a] - (1 << k) >= depth[b]) { res = max(res, mx[k][a]); a = up[k][a]; }
        if (a == b) return res;
        for (int k = LOG - 1; k >= 0; --k) if (up[k][a] != up[k][b]) { res = max({res, mx[k][a], mx[k][b]}); a = up[k][a]; b = up[k][b]; }
        return max({res, mx[0][a], mx[0][b]});
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 8. Heavy-light decomposition. heavy[v] = child with the largest subtree.
//    Positions are assigned so every heavy chain is contiguous AND every
//    subtree is contiguous (subtree of v = [pos[v], pos[v]+sub[v])).
//    A root-to-vertex path crosses O(log n) light edges => O(log n) chains,
//    each a contiguous segment => path query O(log^2 n) with a segment tree.
//    Vertex values here; for EDGE values store w(v,par v) at v and skip the
//    LCA (start the last segment at pos[lca]+1).
// ───────────────────────────────────────────────────────────────────────────
struct SegMax {
    int n; vector<ll> t;
    SegMax(int n_) : n(n_), t(2 * n_, LLONG_MIN) {}
    void set(int i, ll v) { for (t[i += n] = v, i >>= 1; i >= 1; i >>= 1) t[i] = max(t[2 * i], t[2 * i + 1]); }
    ll query(int l, int r) const {                            // [l, r)
        ll res = LLONG_MIN;
        for (l += n, r += n; l < r; l >>= 1, r >>= 1) { if (l & 1) res = max(res, t[l++]); if (r & 1) res = max(res, t[--r]); }
        return res;
    }
};
struct HLD {
    int n; vector<int> par, depth, heavy, head, pos, sub; SegMax seg;
    HLD(const Rooted& T) : n(T.n), par(T.par), depth(T.depth), heavy(n, -1), head(n), pos(n), sub(T.sub), seg(n) {
        for (int v : T.order) for (int u : T.adj[v]) if (u != par[v] && (heavy[v] < 0 || sub[u] > sub[heavy[v]])) heavy[v] = u;
        // assign positions: DFS that always enters the heavy child first (iterative)
        int cur = 0; vector<int> st = {T.order[0]}; head[T.order[0]] = T.order[0];
        while (!st.empty()) {
            int v = st.back(); st.pop_back();
            // walk the whole heavy chain starting at v
            for (int x = v; x != -1; x = heavy[x]) {
                head[x] = (x == v) ? v : head[par[x]];
                pos[x] = cur++;
                for (int u : T.adj[x]) if (u != par[x] && u != heavy[x]) { head[u] = u; st.push_back(u); }
            }
        }
    }
    void setVertex(int v, ll val) { seg.set(pos[v], val); }
    ll pathMax(int a, int b) const {
        ll res = LLONG_MIN;
        while (head[a] != head[b]) {
            if (depth[head[a]] < depth[head[b]]) swap(a, b);
            res = max(res, seg.query(pos[head[a]], pos[a] + 1));    // whole chain segment above a
            a = par[head[a]];
        }
        if (depth[a] > depth[b]) swap(a, b);
        return max(res, seg.query(pos[a], pos[b] + 1));            // same chain: a is the LCA
    }
    ll subtreeMax(int v) const { return seg.query(pos[v], pos[v] + sub[v]); }
};

// ───────────────────────────────────────────────────────────────────────────
// 9. Centroid decomposition. Each vertex is in O(log n) centroid components
//    (component size halves per level). Any path a-b is counted exactly once:
//    at the highest centroid c on the path (the first centroid that separates
//    or contains it), where the path = (a..c) + (c..b) with both halves inside
//    c's component and in DIFFERENT child subtrees (or one half empty).
//    Application: count unordered pairs at distance exactly k.
// ───────────────────────────────────────────────────────────────────────────
struct CentroidDecomp {
    int n; const vector<vector<int>>& g; vector<char> removed; vector<int> sz;
    vector<int> cpar;                                          // centroid tree parent
    CentroidDecomp(const vector<vector<int>>& g_) : n((int)g_.size()), g(g_), removed(n, 0), sz(n, 0), cpar(n, -1) {}
    int calcSize(int v, int p) { sz[v] = 1; for (int u : g[v]) if (u != p && !removed[u]) sz[v] += calcSize(u, v); return sz[v]; }
    int centroid(int v, int p, int total) {
        for (int u : g[v]) if (u != p && !removed[u] && sz[u] * 2 > total) return centroid(u, v, total);
        return v;
    }
    void collect(int v, int p, int d, vector<int>& out) { out.push_back(d); for (int u : g[v]) if (u != p && !removed[u]) collect(u, v, d + 1, out); }

    ll countPairsAtDistance(int k) { return build(0, -1, k); }
    ll build(int entry, int cp, int k) {
        int total = calcSize(entry, -1);
        int c = centroid(entry, -1, total);
        cpar[c] = cp; removed[c] = 1;
        // cnt[d] = number of vertices at distance d from c among already-processed child subtrees
        vector<ll> cnt(total + 1, 0); cnt[0] = 1;
        ll res = 0;
        for (int u : g[c]) if (!removed[u]) {
            vector<int> ds; collect(u, c, 1, ds);
            for (int d : ds) if (k - d >= 0 && k - d <= total) res += cnt[k - d];   // pair with earlier subtrees / c itself
            for (int d : ds) cnt[d]++;
        }
        for (int u : g[c]) if (!removed[u]) res += build(u, c, k);
        return res;
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 10. Rerooting DP: sum of distances from every vertex in O(n).
//     down[v] = sum of dist(v, u) over u in subtree(v)   (bottom-up)
//     ans[v] = ans[par] - sub[v] + (n - sub[v])            (top-down: moving the
//     root from par to v makes sub[v] vertices closer by 1 and n-sub[v] farther by 1)
// ───────────────────────────────────────────────────────────────────────────
static vector<ll> sumOfDistancesAllRoots(const Rooted& T) {
    int n = T.n; vector<ll> down(n, 0), ans(n, 0);
    for (int i = n - 1; i > 0; --i) { int v = T.order[i], p = T.par[v]; down[p] += down[v] + T.sub[v]; }
    ans[T.order[0]] = down[T.order[0]];
    for (int i = 1; i < n; ++i) { int v = T.order[i], p = T.par[v]; ans[v] = ans[p] - T.sub[v] + (n - T.sub[v]); }
    return ans;
}

// ───────────────────────────────────────────────────────────────────────────
// 11. AHU canonical ids. id(v) = index of the SORTED multiset of children ids
//     in a global dictionary; equal ids <=> isomorphic rooted subtrees.
//     Unrooted: compare over centroids (1 or 2). O(n log n) total.
// ───────────────────────────────────────────────────────────────────────────
struct AHU {
    map<vector<int>, int> dict;
    int rootedId(const vector<vector<int>>& g, int root) {
        Rooted T(g, root); int n = T.n; vector<vector<int>> ch(n); vector<int> id(n);
        for (int i = n - 1; i >= 0; --i) {
            int v = T.order[i];
            sort(ch[v].begin(), ch[v].end());
            auto it = dict.find(ch[v]);
            id[v] = (it == dict.end()) ? (dict[ch[v]] = (int)dict.size()) : it->second;
            if (T.par[v] >= 0) ch[T.par[v]].push_back(id[v]);
        }
        return id[root];
    }
    vector<int> unrootedIds(const vector<vector<int>>& g) {   // ids at centroid(s), sorted
        Rooted T(g); int c1 = findCentroid(T); vector<int> res = {rootedId(g, c1)};
        for (int u : g[c1]) if (T.sub[u] * 2 == T.n || (u == T.par[c1] && (T.n - T.sub[c1]) * 2 == T.n)) res.push_back(rootedId(g, u));
        sort(res.begin(), res.end()); return res;
    }
    bool isomorphic(const vector<vector<int>>& a, const vector<vector<int>>& b) {
        if (a.size() != b.size()) return false;
        auto ia = unrootedIds(a), ib = unrootedIds(b);
        for (int x : ia) for (int y : ib) if (x == y) return true;
        return false;
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 12. Kruskal reconstruction tree (KRT). Process edges by increasing weight;
//     when an edge joins two components, create a new node with that weight
//     whose children are the two component roots. Then:
//       minimax path weight(a,b)  (min over paths of max edge) = weight[lca(a,b)]
//       "vertices reachable from a using edges <= w" = subtree of the highest
//       ancestor of a with weight <= w (binary lifting on the KRT).
// ───────────────────────────────────────────────────────────────────────────
struct DSU {
    vector<int> p;
    DSU(int n) : p(n) { iota(p.begin(), p.end(), 0); }
    int find(int x) { while (p[x] != x) x = p[x] = p[p[x]]; return x; }
    bool unite(int a, int b) { a = find(a); b = find(b); if (a == b) return false; p[a] = b; return true; }
};
struct KruskalTree {
    int n, cnt; vector<vector<int>> adj; vector<ll> w;        // nodes 0..n-1 leaves, n.. internal
    KruskalTree(int n_, vector<tuple<ll, int, int>> edges) : n(n_), cnt(n_), adj(2 * n_ - 1), w(2 * n_ - 1, LLONG_MIN) {
        sort(edges.begin(), edges.end());
        DSU d(2 * n_ - 1);
        for (auto [c, a, b] : edges) {
            int ra = d.find(a), rb = d.find(b);
            if (ra == rb) continue;
            int x = cnt++; w[x] = c;
            adj[x].push_back(ra); adj[ra].push_back(x); adj[x].push_back(rb); adj[rb].push_back(x);
            d.p[ra] = x; d.p[rb] = x;
        }
        adj.resize(cnt); w.resize(cnt);                          // cnt == 2n-1 iff the graph is connected
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════════════
static mt19937 rnd(4242);
static ll rnd_ll(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rnd); }

static vector<vector<int>> randomTree(int n) {                 // random parent, then shuffle labels
    vector<int> perm(n); iota(perm.begin(), perm.end(), 0); shuffle(perm.begin(), perm.end(), rnd);
    vector<vector<int>> g(n);
    for (int i = 1; i < n; ++i) {
        int p = (rnd_ll(0, 2) == 0) ? i - 1 : (int)rnd_ll(0, i - 1);   // mix of paths and bushes
        g[perm[i]].push_back(perm[p]); g[perm[p]].push_back(perm[i]);
    }
    return g;
}
static int naiveLCA(const Rooted& T, int a, int b) {
    while (T.depth[a] > T.depth[b]) a = T.par[a];
    while (T.depth[b] > T.depth[a]) b = T.par[b];
    while (a != b) { a = T.par[a]; b = T.par[b]; }
    return a;
}

static void test_basics_diameter_centroid() {
    for (int round = 0; round < 200; ++round) {
        int n = (int)rnd_ll(1, 40);
        auto g = randomTree(n); Rooted T(g);
        // subtree sizes vs brute (count descendants by parent chain)
        vector<int> cnt(n, 0);
        for (int v = 0; v < n; ++v) for (int x = v; x != -1; x = T.par[x]) cnt[x]++;
        assert(cnt == T.sub);
        // diameter: brute = max over all BFS
        int brute = 0; for (int s = 0; s < n; ++s) { auto d = bfsDist(g, s); brute = max(brute, *max_element(d.begin(), d.end())); }
        auto [d1, a, b] = diameterTwoBFS(g);
        assert(d1 == brute && bfsDist(g, a)[b] == brute);
        assert(diameterDP(T) == brute);
        // centroid property
        int c = findCentroid(T);
        Rooted Tc(g, c);
        for (int u : g[c]) assert(Tc.sub[u] * 2 <= n);
    }
    puts("rooting / diameter / centroid      OK");
}

static void test_euler_tour() {
    for (int round = 0; round < 100; ++round) {
        int n = (int)rnd_ll(1, 50); auto g = randomTree(n); Rooted T(g); EulerTour E(T);
        vector<ll> val(n); Fenwick fw(n);
        for (int v = 0; v < n; ++v) { val[v] = rnd_ll(-10, 10); fw.add(E.tin[v], val[v]); }
        for (int q = 0; q < 50; ++q) {
            int v = (int)rnd_ll(0, n - 1);
            if (rnd_ll(0, 1)) { ll nv = rnd_ll(-10, 10); fw.add(E.tin[v], nv - val[v]); val[v] = nv; }
            ll brute = 0; for (int u = 0; u < n; ++u) { bool anc = false; for (int x = u; x != -1; x = T.par[x]) if (x == v) anc = true; if (anc) brute += val[u]; assert(anc == E.isAncestor(v, u)); }
            assert(fw.sum(E.tin[v], E.tout[v]) == brute);
        }
    }
    puts("Euler tour + Fenwick subtree sums  OK");
}

static void test_lca() {
    for (int round = 0; round < 100; ++round) {
        int n = (int)rnd_ll(1, 60); auto g = randomTree(n); Rooted T(g); LCA L(T); LCA_RMQ R(T);
        for (int q = 0; q < 100; ++q) {
            int a = (int)rnd_ll(0, n - 1), b = (int)rnd_ll(0, n - 1);
            int c = naiveLCA(T, a, b);
            assert(L.lca(a, b) == c && R.lca(a, b) == c);
            assert(L.dist(a, b) == bfsDist(g, a)[b]);
            int k = (int)rnd_ll(0, n); int x = a; for (int i = 0; i < k && x != -1; ++i) x = T.par[x];
            assert(L.kth(a, k) == x);
            int d = L.dist(a, b), j = (int)rnd_ll(0, d);
            int v = L.jump(a, b, j);
            assert(bfsDist(g, a)[v] == j && bfsDist(g, v)[b] == d - j);
        }
    }
    puts("LCA lifting / Euler+RMQ / kth/jump OK");
}

static void test_path_max_lifting_and_hld() {
    for (int round = 0; round < 100; ++round) {
        int n = (int)rnd_ll(1, 60); auto g = randomTree(n); Rooted T(g);
        vector<ll> wpar(n, 0); for (int v = 0; v < n; ++v) if (T.par[v] >= 0) wpar[v] = rnd_ll(-100, 100);
        LiftMax LM(T, wpar);
        HLD H(T); vector<ll> vval(n); for (int v = 0; v < n; ++v) { vval[v] = rnd_ll(-100, 100); H.setVertex(v, vval[v]); }
        for (int q = 0; q < 100; ++q) {
            int a = (int)rnd_ll(0, n - 1), b = (int)rnd_ll(0, n - 1);
            if (rnd_ll(0, 3) == 0) { int v = (int)rnd_ll(0, n - 1); vval[v] = rnd_ll(-100, 100); H.setVertex(v, vval[v]); }
            // brute: walk both up to the LCA
            int c = naiveLCA(T, a, b); ll em = LLONG_MIN, vm = LLONG_MIN;
            for (int x = a; x != c; x = T.par[x]) { em = max(em, wpar[x]); vm = max(vm, vval[x]); }
            for (int x = b; x != c; x = T.par[x]) { em = max(em, wpar[x]); vm = max(vm, vval[x]); }
            vm = max(vm, vval[c]);
            assert(LM.pathMax(a, b) == em);
            assert(H.pathMax(a, b) == vm);
            ll sm = LLONG_MIN; for (int u = 0; u < n; ++u) { bool anc = false; for (int x = u; x != -1; x = T.par[x]) if (x == a) anc = true; if (anc) sm = max(sm, vval[u]); }
            assert(H.subtreeMax(a) == sm);
        }
        // HLD structural invariants: subtree contiguous, chain contiguous
        for (int v = 0; v < n; ++v) if (H.heavy[v] >= 0) assert(H.pos[H.heavy[v]] == H.pos[v] + 1 && H.head[H.heavy[v]] == H.head[v]);
    }
    puts("lifting path max / HLD             OK");
}

static void test_centroid_decomposition_and_rerooting() {
    for (int round = 0; round < 60; ++round) {
        int n = (int)rnd_ll(1, 50); auto g = randomTree(n); Rooted T(g);
        vector<vector<int>> D(n); for (int s = 0; s < n; ++s) D[s] = bfsDist(g, s);
        for (int k = 1; k <= 6; ++k) {
            ll brute = 0; for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b) if (D[a][b] == k) ++brute;
            CentroidDecomp cd(g);
            assert(cd.countPairsAtDistance(k) == brute);
        }
        auto ans = sumOfDistancesAllRoots(T);
        for (int v = 0; v < n; ++v) { ll s = 0; for (int u = 0; u < n; ++u) s += D[v][u]; assert(ans[v] == s); }
    }
    puts("centroid decomposition / rerooting OK");
}

static void test_ahu_and_krt() {
    // isomorphism: relabel a tree and compare; also a deliberately different tree
    for (int round = 0; round < 50; ++round) {
        int n = (int)rnd_ll(2, 30); auto g = randomTree(n);
        vector<int> perm(n); iota(perm.begin(), perm.end(), 0); shuffle(perm.begin(), perm.end(), rnd);
        vector<vector<int>> h(n); for (int v = 0; v < n; ++v) for (int u : g[v]) h[perm[v]].push_back(perm[u]);
        AHU ahu; assert(ahu.isomorphic(g, h));
        // path vs star are never isomorphic for n >= 4
        if (n >= 4) {
            vector<vector<int>> path(n), star(n);
            for (int i = 1; i < n; ++i) { path[i - 1].push_back(i); path[i].push_back(i - 1); star[0].push_back(i); star[i].push_back(0); }
            AHU a2; assert(!a2.isomorphic(path, star));
        }
    }
    // KRT: minimax path on a random connected weighted graph vs Floyd-style brute
    for (int round = 0; round < 50; ++round) {
        int n = (int)rnd_ll(1, 25);
        vector<tuple<ll, int, int>> edges;
        for (int i = 1; i < n; ++i) edges.push_back({rnd_ll(1, 50), i, (int)rnd_ll(0, i - 1)});   // spanning tree
        for (int e = 0; e < n; ++e) { int a = (int)rnd_ll(0, n - 1), b = (int)rnd_ll(0, n - 1); if (a != b) edges.push_back({rnd_ll(1, 50), a, b}); }
        vector<vector<ll>> mm(n, vector<ll>(n, LLONG_MAX)); for (int i = 0; i < n; ++i) mm[i][i] = 0;
        for (auto [c, a, b] : edges) { mm[a][b] = min(mm[a][b], c); mm[b][a] = min(mm[b][a], c); }
        for (int k = 0; k < n; ++k) for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) mm[i][j] = min(mm[i][j], max(mm[i][k], mm[k][j]));
        KruskalTree K(n, edges);
        assert(K.cnt == 2 * n - 1);
        Rooted T(K.adj, K.cnt - 1); LCA L(T);
        for (int a = 0; a < n; ++a) for (int b = 0; b < n; ++b) if (a != b) assert(K.w[L.lca(a, b)] == mm[a][b]);
    }
    puts("AHU isomorphism / Kruskal tree     OK");
}

int main() {
    test_basics_diameter_centroid();
    test_euler_tour();
    test_lca();
    test_path_max_lifting_and_hld();
    test_centroid_decomposition_and_rerooting();
    test_ahu_and_krt();
    puts("all chapter 10 tests passed");
    return 0;
}
