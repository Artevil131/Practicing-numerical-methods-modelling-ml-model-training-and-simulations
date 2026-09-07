// Chapter 08 — DSU, offline techniques, sqrt techniques: the reference snippet library.
//
// TRAINING RULE: do not copy this file into a contest. Re-type every structure
// here from memory until it comes out bug-free in one go. Every technique is
// asserted against a brute force on random data in main().
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Contents
//   1. DSU: union by size, iterative path compression, extra info (size, sum, min);
//      Kruskal MST as the canonical use
//   2. Parity DSU (DSU with potentials): online bipartiteness / "same or different side"
//   3. Rollback DSU: union by size, no compression, stack of changes, O(log n) per op
//   4. Offline dynamic connectivity: segment tree over time + rollback DSU
//   5. Time-stamped union forest ("when did a and b become connected?") — depth <= log n
//   6. Parallel binary search for the same question
//   7. DSU on tree (Sack) and small-to-large set merging: distinct colours per subtree
//   8. Offline sweeps with a Fenwick: distinct values in range, 2D dominance counting
//   9. Sqrt techniques: triangle counting by degree threshold, sqrt-rebuild buffer
//
// On CSES/Codeforces (GCC) you would write `#include <bits/stdc++.h>`; Apple
// clang has no such header, so the standard headers are listed explicitly.
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <vector>
using namespace std;
using ll = long long;

static mt19937_64 rng(8080);
static ll rnd(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rng); }

// ============================================================================
// 1. DSU with union by size + path compression (iterative find).
//    Invariant: p[r] == r iff r is a root; sz/sum/mn are valid only at roots.
// ============================================================================
struct DSU {
    vector<int> p, sz, mn;
    vector<ll> sum;
    int comps;
    explicit DSU(int n, const vector<ll>& val = {}) : p(n), sz(n, 1), mn(n), sum(n, 0), comps(n) {
        iota(p.begin(), p.end(), 0);
        iota(mn.begin(), mn.end(), 0);            // min vertex id in component
        if (!val.empty()) sum = val;               // per-component sum of values
    }
    int find(int x) {
        int r = x;
        while (p[r] != r) r = p[r];               // 1st pass: find the root
        while (p[x] != r) { int nx = p[x]; p[x] = r; x = nx; }  // 2nd pass: compress
        return r;
    }
    bool same(int a, int b) { return find(a) == find(b); }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (sz[a] < sz[b]) swap(a, b);            // a is the larger root
        p[b] = a; sz[a] += sz[b]; sum[a] += sum[b]; mn[a] = min(mn[a], mn[b]);
        comps--;
        return true;
    }
    int size(int x) { return sz[find(x)]; }
};

// Kruskal: O(m log m); returns {total weight, number of edges taken}.
pair<ll, int> kruskal(int n, vector<tuple<ll, int, int>> edges) {
    sort(edges.begin(), edges.end());
    DSU d(n);
    ll total = 0; int taken = 0;
    for (auto& [w, u, v] : edges)
        if (d.unite(u, v)) { total += w; taken++; }
    return {total, taken};
}

// ============================================================================
// 2. Parity DSU. par[x] = parity of the path x -> p[x]. find returns (root,
//    parity of x relative to the root). Union by size bounds the depth by
//    log2 n BEFORE compression, so the recursive find is safe.
// ============================================================================
struct ParityDSU {
    vector<int> p, sz, par;
    explicit ParityDSU(int n) : p(n), sz(n, 1), par(n, 0) { iota(p.begin(), p.end(), 0); }
    pair<int, int> find(int x) {
        if (p[x] == x) return {x, 0};
        auto [r, q] = find(p[x]);
        p[x] = r; par[x] ^= q;                    // compress: parity to the root
        return {r, par[x]};
    }
    // Constraint parity(a) ^ parity(b) == d. Returns false on contradiction.
    bool unite(int a, int b, int d) {
        auto [ra, pa] = find(a);
        auto [rb, pb] = find(b);
        if (ra == rb) return (pa ^ pb) == d;
        if (sz[ra] < sz[rb]) { swap(ra, rb); swap(pa, pb); }
        p[rb] = ra; sz[ra] += sz[rb];
        par[rb] = pa ^ pb ^ d;                    // so that pa ^ (par[rb] ^ pb)... = d
        return true;
    }
};

// ============================================================================
// 3. Rollback DSU: NO path compression (it would break the LIFO undo), union by
//    size keeps find at O(log n). Every unite pushes one record; rollback pops it.
// ============================================================================
struct RollbackDSU {
    vector<int> p, sz;
    vector<pair<int, int>> st;                    // (attached root b, its new parent a); (-1,-1) = no-op
    int comps;
    explicit RollbackDSU(int n) : p(n), sz(n, 1), comps(n) { iota(p.begin(), p.end(), 0); }
    int find(int x) const { while (p[x] != x) x = p[x]; return x; }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) { st.push_back({-1, -1}); return false; }
        if (sz[a] < sz[b]) swap(a, b);
        p[b] = a; sz[a] += sz[b]; comps--;
        st.push_back({b, a});
        return true;
    }
    int snapshot() const { return st.size(); }
    void rollback(int snap) {
        while ((int)st.size() > snap) {
            auto [b, a] = st.back(); st.pop_back();
            if (b < 0) continue;
            p[b] = b; sz[a] -= sz[b]; comps++;
        }
    }
};

// ============================================================================
// 4. Offline dynamic connectivity. Operations at times 0..q-1:
//    {0,u,v} add edge, {1,u,v} remove edge, {2,u,v} query "connected?",
//    {3,0,0} query "number of components". Each edge lives on a time interval;
//    insert it into every canonical segment-tree node of that interval; DFS the
//    tree applying unions on entry and rolling back on exit. O(q log q log n).
// ============================================================================
vector<int> offline_dynamic_connectivity(int n, const vector<array<int, 3>>& ops) {
    int q = ops.size();
    vector<vector<pair<int, int>>> seg(4 * max(q, 1));
    function<void(int, int, int, int, int, pair<int, int>)> insert =
        [&](int v, int lo, int hi, int l, int r, pair<int, int> e) {
            if (r < lo || hi < l) return;
            if (l <= lo && hi <= r) { seg[v].push_back(e); return; }
            int mid = (lo + hi) / 2;
            insert(2 * v, lo, mid, l, r, e);
            insert(2 * v + 1, mid + 1, hi, l, r, e);
        };
    map<pair<int, int>, int> start;                // edge -> time it was added
    for (int t = 0; t < q; t++) {
        auto [type, u, v] = ops[t];
        if (u > v) swap(u, v);
        if (type == 0) start[{u, v}] = t;
        else if (type == 1) { insert(1, 0, q - 1, start[{u, v}], t - 1, {u, v}); start.erase({u, v}); }
    }
    for (auto& [e, t0] : start) insert(1, 0, q - 1, t0, q - 1, e);  // still alive at the end

    RollbackDSU d(n);
    vector<int> ans(q, -1);
    function<void(int, int, int)> dfs = [&](int v, int lo, int hi) {
        int snap = d.snapshot();
        for (auto [a, b] : seg[v]) d.unite(a, b);
        if (lo == hi) {
            auto [type, a, b] = ops[lo];
            if (type == 2) ans[lo] = d.find(a) == d.find(b);
            else if (type == 3) ans[lo] = d.comps;
        } else {
            int mid = (lo + hi) / 2;
            dfs(2 * v, lo, mid);
            dfs(2 * v + 1, mid + 1, hi);
        }
        d.rollback(snap);
    };
    if (q) dfs(1, 0, q - 1);
    return ans;
}

// ============================================================================
// 5. Time-stamped union forest. Edges arrive at times 1..m. Union by size, no
//    compression: the forest has depth <= log2 n, so walking both vertices to
//    their LCA is O(log n). Answer = max edge time on the path (the moment the
//    two components merged), or -1 if never connected.
// ============================================================================
struct UnionForest {
    vector<int> p, sz, when;                      // when[x] = time x's root edge was created
    explicit UnionForest(int n) : p(n), sz(n, 1), when(n, 0) { iota(p.begin(), p.end(), 0); }
    void unite(int a, int b, int t) {
        while (p[a] != a) a = p[a];
        while (p[b] != b) b = p[b];
        if (a == b) return;
        if (sz[a] < sz[b]) swap(a, b);
        p[b] = a; sz[a] += sz[b]; when[b] = t;
    }
    int connected_at(int a, int b) const {
        // Walk up alternately from the deeper-in-time side. Since times increase
        // towards the root along any path, always lift the endpoint with the
        // smaller "when"; they meet at the LCA (or never).
        int best = 0;
        while (a != b) {
            if (p[a] == a && p[b] == b) return -1;
            if (p[b] == b || (p[a] != a && when[a] < when[b])) { best = max(best, when[a]); a = p[a]; }
            else { best = max(best, when[b]); b = p[b]; }
        }
        return best;
    }
};

// ============================================================================
// 6. Parallel binary search. Same question: for each query (a, b), the first
//    time t in [1, m] at which a and b are connected, or -1.
//    Each round: sort queries by mid, sweep edges once, halve every interval.
//    O((m + q) log m * alpha) total instead of O(q * m).
// ============================================================================
vector<int> parallel_binary_search(int n, const vector<pair<int, int>>& edges,
                                   const vector<pair<int, int>>& qs) {
    int m = edges.size(), q = qs.size();
    vector<int> lo(q, 1), hi(q, m + 1);           // answer in [lo, hi); hi == m+1 means "never"
    vector<int> ord(q);
    for (int round = 0; round < 20 && (1 << round) <= 2 * (m + 1); round++) {
        bool any = false;
        for (int i = 0; i < q; i++) if (lo[i] < hi[i]) any = true;
        if (!any) break;
        iota(ord.begin(), ord.end(), 0);
        sort(ord.begin(), ord.end(), [&](int x, int y) { return lo[x] + hi[x] < lo[y] + hi[y]; });
        DSU d(n);
        int t = 0;                                // edges [0, t) applied  <=> time t
        for (int id : ord) {
            if (lo[id] >= hi[id]) continue;
            int mid = (lo[id] + hi[id]) / 2;      // "connected after `mid` edges?"
            while (t < mid && t < m) { d.unite(edges[t].first, edges[t].second); t++; }
            if (d.same(qs[id].first, qs[id].second)) hi[id] = mid; else lo[id] = mid + 1;
        }
    }
    vector<int> ans(q);
    for (int i = 0; i < q; i++) ans[i] = (lo[i] <= m) ? lo[i] : -1;
    return ans;
}

// ============================================================================
// 7. Distinct colours in every subtree.
//    (a) DSU on tree / Sack: keep the heavy child's data, re-add light subtrees.
//        Each vertex is added O(log n) times => O(n log n).
//    (b) Small-to-large: merge each child's set into the largest; each element
//        moves O(log n) times => O(n log^2 n) with std::set.
// ============================================================================
struct Tree {
    int n;
    vector<vector<int>> adj;
    vector<int> par, sz, heavy, tin, tout, order;
    explicit Tree(int n) : n(n), adj(n), par(n, -1), sz(n, 1), heavy(n, -1), tin(n), tout(n) {}
    void add_edge(int a, int b) { adj[a].push_back(b); adj[b].push_back(a); }
    void prepare(int root = 0) {                  // iterative DFS: sizes, heavy child, Euler tour
        vector<int> st = {root}, seen;
        par[root] = -1;
        while (!st.empty()) {                     // preorder
            int v = st.back(); st.pop_back();
            seen.push_back(v);
            for (int u : adj[v]) if (u != par[v]) { par[u] = v; st.push_back(u); }
        }
        for (int i = n - 1; i >= 0; i--) {        // reverse preorder = children before parents
            int v = seen[i];
            if (par[v] != -1) {
                sz[par[v]] += sz[v];
                if (heavy[par[v]] == -1 || sz[v] > sz[heavy[par[v]]]) heavy[par[v]] = v;
            }
        }
        // Euler tour with heavy child first so its tour range is contiguous at tin[v]+1
        order.clear();
        vector<pair<int, int>> s2 = {{root, 0}};
        while (!s2.empty()) {
            auto [v, phase] = s2.back(); s2.pop_back();
            if (phase == 0) {
                tin[v] = order.size(); order.push_back(v);
                s2.push_back({v, 1});
                for (int u : adj[v]) if (u != par[v] && u != heavy[v]) s2.push_back({u, 0});
                if (heavy[v] != -1) s2.push_back({heavy[v], 0});   // popped first
            } else tout[v] = order.size();        // [tin, tout) is the subtree
        }
    }
};

vector<int> sack_distinct_colours(const Tree& T, const vector<int>& colour) {
    int n = T.n;
    int maxc = *max_element(colour.begin(), colour.end());
    vector<int> cnt(maxc + 1, 0), ans(n);         // size by max colour, NOT by n
    int distinct = 0;
    auto add = [&](int v) { if (cnt[colour[v]]++ == 0) distinct++; };
    auto rem = [&](int v) { if (--cnt[colour[v]] == 0) distinct--; };
    // Process vertices so that a vertex is handled after its heavy child (post-order
    // over the heavy chain). Recursive formulation kept small for clarity.
    function<void(int, bool)> dfs = [&](int v, bool keep) {
        for (int u : T.adj[v]) if (u != T.par[v] && u != T.heavy[v]) dfs(u, false);
        if (T.heavy[v] != -1) dfs(T.heavy[v], true);
        for (int u : T.adj[v]) if (u != T.par[v] && u != T.heavy[v])
            for (int i = T.tin[u]; i < T.tout[u]; i++) add(T.order[i]);   // re-add light subtrees
        add(v);
        ans[v] = distinct;
        if (!keep) for (int i = T.tin[v]; i < T.tout[v]; i++) rem(T.order[i]);
    };
    dfs(0, true);
    return ans;
}

vector<int> small_to_large_distinct_colours(const Tree& T, const vector<int>& colour) {
    int n = T.n;
    vector<set<int>> s(n);
    vector<int> ans(n);
    // children before parents: reverse Euler order works since tin[child] > tin[parent]
    for (int i = n - 1; i >= 0; i--) {
        int v = T.order[i];
        s[v].insert(colour[v]);
        for (int u : T.adj[v]) if (u != T.par[v]) {
            if (s[u].size() > s[v].size()) swap(s[u], s[v]);   // O(1) swap, then merge the smaller
            for (int c : s[u]) s[v].insert(c);
            s[u].clear();
        }
        ans[v] = s[v].size();
    }
    return ans;
}

// ============================================================================
// 8. Offline sweeps with a Fenwick.
// ============================================================================
struct Fenwick {
    int n; vector<ll> t;
    explicit Fenwick(int n) : n(n), t(n + 1, 0) {}
    void add(int i, ll v) { for (i++; i <= n; i += i & -i) t[i] += v; }
    ll prefix(int i) const { ll s = 0; for (i++; i > 0; i -= i & -i) s += t[i]; return s; }
    ll sum(int l, int r) const { return prefix(r) - prefix(l - 1); }
};

// Distinct values in a[l..r], queries sorted by r. Keep a 1 at the LAST occurrence
// (so far) of every value; distinct(l, r) = number of ones in [l, r].
vector<int> distinct_values_offline(const vector<int>& a, const vector<pair<int, int>>& qs) {
    int n = a.size(), q = qs.size();
    vector<int> ord(q), ans(q);
    iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int x, int y) { return qs[x].second < qs[y].second; });
    map<int, int> last;                           // value -> last position (compress in contests)
    Fenwick f(n);
    int r = -1;
    for (int id : ord) {
        while (r < qs[id].second) {
            r++;
            auto it = last.find(a[r]);
            if (it != last.end()) f.add(it->second, -1);
            f.add(r, +1); last[a[r]] = r;
        }
        ans[id] = f.sum(qs[id].first, qs[id].second);
    }
    return ans;
}

// 2D dominance: for each query (qx, qy) count points with x <= qx and y <= qy.
// Sort points and queries by x, sweep, Fenwick over compressed y.
vector<int> dominance_count(vector<pair<int, int>> pts, const vector<pair<int, int>>& qs) {
    vector<int> ys;
    for (auto& [x, y] : pts) ys.push_back(y);
    sort(ys.begin(), ys.end()); ys.erase(unique(ys.begin(), ys.end()), ys.end());
    sort(pts.begin(), pts.end());
    int q = qs.size();
    vector<int> ord(q), ans(q);
    iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int a, int b) { return qs[a].first < qs[b].first; });
    Fenwick f(ys.size());
    size_t i = 0;
    for (int id : ord) {
        while (i < pts.size() && pts[i].first <= qs[id].first) {
            f.add(lower_bound(ys.begin(), ys.end(), pts[i].second) - ys.begin(), 1);
            i++;
        }
        int k = upper_bound(ys.begin(), ys.end(), qs[id].second) - ys.begin();  // # of ys <= qy
        ans[id] = k ? f.prefix(k - 1) : 0;
    }
    return ans;
}

// ============================================================================
// 9. Sqrt techniques.
// ============================================================================
// Triangle counting in O(m sqrt m): orient each edge from the endpoint with
// smaller (degree, id) to the larger. Every vertex then has out-degree
// O(sqrt m), and each triangle is counted exactly once from its "smallest" vertex.
ll count_triangles(int n, const vector<pair<int, int>>& edges) {
    vector<int> deg(n, 0);
    for (auto& [u, v] : edges) { deg[u]++; deg[v]++; }
    auto less = [&](int a, int b) { return make_pair(deg[a], a) < make_pair(deg[b], b); };
    vector<vector<int>> out(n);
    for (auto& [u, v] : edges) { if (less(u, v)) out[u].push_back(v); else out[v].push_back(u); }
    vector<int> mark(n, 0);
    ll tri = 0;
    for (int u = 0; u < n; u++) {
        for (int v : out[u]) mark[v] = 1;
        for (int v : out[u]) for (int w : out[v]) tri += mark[w];
        for (int v : out[u]) mark[v] = 0;
    }
    return tri;
}

// Sqrt-rebuild buffer: insert(x) and count_leq(x). Pending inserts sit in a small
// buffer scanned linearly; every B inserts the sorted base is rebuilt in O(n).
// With B = sqrt(q): O(sqrt q) per insert amortised... O(sqrt q + log n) per query.
struct SqrtRebuild {
    vector<ll> base, buf;
    size_t B;
    explicit SqrtRebuild(size_t B) : B(B) {}
    void insert(ll x) {
        buf.push_back(x);
        if (buf.size() >= B) {
            sort(buf.begin(), buf.end());
            vector<ll> merged(base.size() + buf.size());
            merge(base.begin(), base.end(), buf.begin(), buf.end(), merged.begin());
            base.swap(merged); buf.clear();
        }
    }
    ll count_leq(ll x) const {
        ll c = upper_bound(base.begin(), base.end(), x) - base.begin();
        for (ll y : buf) c += (y <= x);
        return c;
    }
};

// ============================================================================
// Tests
// ============================================================================
static void test_dsu_and_kruskal() {
    int n = 30;
    vector<ll> val(n);
    for (auto& x : val) x = rnd(-10, 10);
    DSU d(n, val);
    vector<int> label(n);                         // brute: explicit component labels
    iota(label.begin(), label.end(), 0);
    for (int it = 0; it < 200; it++) {
        int a = rnd(0, n - 1), b = rnd(0, n - 1);
        bool merged = d.unite(a, b);
        assert(merged == (label[a] != label[b]));
        if (merged) { int la = label[a], lb = label[b]; for (auto& x : label) if (x == lb) x = la; }
        int c = rnd(0, n - 1);
        int cnt = 0, mn = INT_MAX; ll s = 0;
        for (int i = 0; i < n; i++) if (label[i] == label[c]) { cnt++; mn = min(mn, i); s += val[i]; }
        int r = d.find(c);
        assert(d.sz[r] == cnt && d.mn[r] == mn && d.sum[r] == s);
        set<int> labs(label.begin(), label.end());
        assert(d.comps == (int)labs.size());
    }
    // Kruskal vs Prim O(n^2) on a random connected graph
    for (int it = 0; it < 30; it++) {
        int m = 12;
        vector<vector<ll>> w(m, vector<ll>(m, LLONG_MAX));
        vector<tuple<ll, int, int>> edges;
        for (int i = 1; i < m; i++) { int j = rnd(0, i - 1); ll c = rnd(1, 50); edges.push_back({c, i, j}); w[i][j] = w[j][i] = min(w[i][j], c); }
        for (int k = 0; k < 15; k++) { int i = rnd(0, m - 1), j = rnd(0, m - 1); if (i == j) continue; ll c = rnd(1, 50); edges.push_back({c, i, j}); w[i][j] = w[j][i] = min(w[i][j], c); }
        vector<ll> dist(m, LLONG_MAX); vector<char> in(m, 0);
        dist[0] = 0; ll prim = 0;
        for (int k = 0; k < m; k++) {
            int u = -1;
            for (int i = 0; i < m; i++) if (!in[i] && (u == -1 || dist[i] < dist[u])) u = i;
            in[u] = 1; prim += dist[u];
            for (int v = 0; v < m; v++) if (!in[v] && w[u][v] < dist[v]) dist[v] = w[u][v];
        }
        auto [kw, taken] = kruskal(m, edges);
        assert(kw == prim && taken == m - 1);
    }
}

static void test_parity_dsu() {
    for (int it = 0; it < 100; it++) {
        int n = 12;
        vector<int> side(n);                      // hidden 2-colouring
        for (auto& s : side) s = rnd(0, 1);
        ParityDSU d(n);
        vector<array<int, 3>> constraints;
        bool contradiction = false;
        for (int k = 0; k < 25; k++) {
            int a = rnd(0, n - 1), b = rnd(0, n - 1);
            int dd = (rnd(0, 6) == 0) ? rnd(0, 1) : (side[a] ^ side[b]);  // mostly consistent
            constraints.push_back({a, b, dd});
            bool ok = d.unite(a, b, dd);
            // brute: is the constraint set 2-satisfiable? BFS 2-colouring on constraint graph
            vector<vector<pair<int, int>>> g(n);
            for (auto& [x, y, z] : constraints) { g[x].push_back({y, z}); g[y].push_back({x, z}); }
            vector<int> col(n, -1); bool sat = true;
            for (int s = 0; s < n && sat; s++) {
                if (col[s] != -1) continue;
                col[s] = 0; vector<int> stk = {s};
                while (!stk.empty() && sat) {
                    int v = stk.back(); stk.pop_back();
                    for (auto [u, z] : g[v]) {
                        if (col[u] == -1) { col[u] = col[v] ^ z; stk.push_back(u); }
                        else if (col[u] != (col[v] ^ z)) sat = false;
                    }
                }
            }
            if (!ok) { contradiction = true; assert(!sat); break; }
            assert(sat);
        }
        (void)contradiction;
    }
}

static void test_rollback_and_dynamic_connectivity() {
    for (int it = 0; it < 40; it++) {
        int n = rnd(2, 9), q = rnd(1, 40);
        vector<array<int, 3>> ops;
        set<pair<int, int>> alive;
        for (int t = 0; t < q; t++) {
            int type = rnd(0, 3);
            if (type == 1 && alive.empty()) type = 2;
            if (type == 0) {
                int u = rnd(0, n - 1), v = rnd(0, n - 1);
                if (u == v || alive.count({min(u, v), max(u, v)})) { type = 3; ops.push_back({3, 0, 0}); continue; }
                alive.insert({min(u, v), max(u, v)}); ops.push_back({0, u, v});
            } else if (type == 1) {
                auto it2 = alive.begin(); advance(it2, rnd(0, alive.size() - 1));
                ops.push_back({1, it2->first, it2->second}); alive.erase(it2);
            } else if (type == 2) ops.push_back({2, (int)rnd(0, n - 1), (int)rnd(0, n - 1)});
            else ops.push_back({3, 0, 0});
        }
        auto ans = offline_dynamic_connectivity(n, ops);
        // brute
        set<pair<int, int>> cur;
        for (int t = 0; t < q; t++) {
            auto [type, u, v] = ops[t];
            if (u > v) swap(u, v);
            if (type == 0) cur.insert({u, v});
            else if (type == 1) cur.erase({u, v});
            else {
                DSU d(n);
                for (auto& [a, b] : cur) d.unite(a, b);
                if (type == 2) assert(ans[t] == (int)d.same(u, v));
                else assert(ans[t] == d.comps);
            }
        }
    }
    // rollback DSU alone: random unite / snapshot / rollback against a copy-based brute
    int n = 15;
    RollbackDSU d(n);
    vector<vector<int>> history;                  // brute labels at each snapshot
    vector<int> snaps, label(n);
    iota(label.begin(), label.end(), 0);
    for (int it = 0; it < 300; it++) {
        int op = rnd(0, 2);
        if (op == 0) {
            int a = rnd(0, n - 1), b = rnd(0, n - 1);
            d.unite(a, b);
            int la = label[a], lb = label[b];
            for (auto& x : label) if (x == lb) x = la;
        } else if (op == 1) { snaps.push_back(d.snapshot()); history.push_back(label); }
        else if (!snaps.empty()) { d.rollback(snaps.back()); label = history.back(); snaps.pop_back(); history.pop_back(); }
        for (int k = 0; k < 10; k++) {
            int a = rnd(0, n - 1), b = rnd(0, n - 1);
            assert((d.find(a) == d.find(b)) == (label[a] == label[b]));
        }
    }
}

static void test_when_connected() {
    for (int it = 0; it < 40; it++) {
        int n = rnd(2, 12), m = rnd(0, 20);
        vector<pair<int, int>> edges(m);
        for (auto& e : edges) e = {(int)rnd(0, n - 1), (int)rnd(0, n - 1)};
        vector<pair<int, int>> qs(25);
        for (auto& qq : qs) qq = {(int)rnd(0, n - 1), (int)rnd(0, n - 1)};
        UnionForest uf(n);
        for (int t = 0; t < m; t++) uf.unite(edges[t].first, edges[t].second, t + 1);
        auto pbs = parallel_binary_search(n, edges, qs);
        for (size_t i = 0; i < qs.size(); i++) {
            // brute: simulate
            DSU d(n); int expect = d.same(qs[i].first, qs[i].second) ? 0 : -1;
            for (int t = 0; t < m && expect == -1; t++) { d.unite(edges[t].first, edges[t].second); if (d.same(qs[i].first, qs[i].second)) expect = t + 1; }
            int f = uf.connected_at(qs[i].first, qs[i].second);
            // a == b: both report 0 (connected at time 0); PBS reports 1 (first time in [1,m]) or -1 if m==0
            if (qs[i].first == qs[i].second) { assert(f == 0 && expect == 0); assert(pbs[i] == (m ? 1 : -1)); continue; }
            assert(f == expect);
            assert(pbs[i] == expect);
        }
    }
}

static void test_subtree_colours() {
    for (int it = 0; it < 40; it++) {
        int n = rnd(1, 60);
        Tree T(n);
        for (int v = 1; v < n; v++) T.add_edge(v, rnd(0, v - 1));
        T.prepare(0);
        vector<int> colour(n);
        for (auto& c : colour) c = rnd(1, 8);
        auto a1 = sack_distinct_colours(T, colour);
        auto a2 = small_to_large_distinct_colours(T, colour);
        for (int v = 0; v < n; v++) {
            set<int> s;
            for (int i = T.tin[v]; i < T.tout[v]; i++) s.insert(colour[T.order[i]]);
            assert(a1[v] == (int)s.size() && a2[v] == (int)s.size());
        }
    }
}

static void test_offline_sweeps() {
    int n = 80;
    vector<int> a(n);
    for (auto& x : a) x = rnd(0, 15);
    vector<pair<int, int>> qs(200);
    for (auto& qq : qs) { int l = rnd(0, n - 1), r = rnd(l, n - 1); qq = {l, r}; }
    auto ans = distinct_values_offline(a, qs);
    for (size_t i = 0; i < qs.size(); i++) {
        set<int> s(a.begin() + qs[i].first, a.begin() + qs[i].second + 1);
        assert(ans[i] == (int)s.size());
    }
    vector<pair<int, int>> pts(70);
    for (auto& p : pts) p = {(int)rnd(-20, 20), (int)rnd(-20, 20)};
    vector<pair<int, int>> dq(150);
    for (auto& qq : dq) qq = {(int)rnd(-25, 25), (int)rnd(-25, 25)};
    auto dc = dominance_count(pts, dq);
    for (size_t i = 0; i < dq.size(); i++) {
        int c = 0;
        for (auto& [x, y] : pts) c += (x <= dq[i].first && y <= dq[i].second);
        assert(dc[i] == c);
    }
}

static void test_sqrt() {
    for (int it = 0; it < 30; it++) {
        int n = rnd(3, 14);
        set<pair<int, int>> es;
        int m = rnd(0, n * (n - 1) / 2);
        while ((int)es.size() < m) { int u = rnd(0, n - 1), v = rnd(0, n - 1); if (u != v) es.insert({min(u, v), max(u, v)}); }
        vector<pair<int, int>> edges(es.begin(), es.end());
        vector<vector<char>> adj(n, vector<char>(n, 0));
        for (auto& [u, v] : edges) adj[u][v] = adj[v][u] = 1;
        ll brute = 0;
        for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) for (int c = b + 1; c < n; c++)
            brute += adj[a][b] && adj[b][c] && adj[a][c];
        assert(count_triangles(n, edges) == brute);
    }
    SqrtRebuild sr(7);
    multiset<ll> ms;
    for (int it = 0; it < 500; it++) {
        if (rnd(0, 1)) { ll x = rnd(-30, 30); sr.insert(x); ms.insert(x); }
        else { ll x = rnd(-35, 35); assert(sr.count_leq(x) == (ll)distance(ms.begin(), ms.upper_bound(x))); }
    }
}

int main() {
    test_dsu_and_kruskal();
    test_parity_dsu();
    test_rollback_and_dynamic_connectivity();
    test_when_connected();
    test_subtree_colours();
    test_offline_sweeps();
    test_sqrt();
    cout << "All chapter 08 tests passed.\n";
    return 0;
}
