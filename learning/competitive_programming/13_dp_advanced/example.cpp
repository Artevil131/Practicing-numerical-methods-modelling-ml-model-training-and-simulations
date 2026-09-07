// Chapter 13 — Advanced dynamic programming: reference library.
//
// Every technique of lesson.md as a self-contained snippet, each cross-checked in main()
// against a brute force on random data. This file is TRAINING MATERIAL: after reading the
// lesson, close it and re-type each snippet from memory, then diff against this file.
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Sections (search for "// ==="):
//   1. knapsack variants: unbounded, bounded via binary splitting, 0/1 with bitset
//   2. LIS in O(n log n) with reconstruction
//   3. edit distance (rolling rows) and LCS (parent pointers)
//   4. tree DP: weighted independent set, tree matching, subtree knapsack merging
//   5. digit DP: count numbers <= N with no two equal adjacent digits
//   6. bitmask DP: TSP path, Elevator Rides (pair state), broken-profile tilings
//   7. Knuth optimization (interval DP with monotone opt)
//   8. divide & conquer optimization (layered DP with monotone opt)
//   9. convex hull trick: monotone deque + Li Chao tree
//  10. slope trick: make array non-decreasing with minimum |change|
//  11. Aliens trick (Lagrangian relaxation) for an exactly-k constraint
//  12. SOS DP
//  13. DP with a Fenwick tree: counting increasing subsequences
//  14. probability DP: dice sum distribution
//  15. DP over a KMP automaton: strings containing a pattern
// <bits/stdc++.h> is GCC-only (not available with Apple clang); on a judge it is fine,
// here we list the headers so the file builds everywhere.
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using ll = long long;

static const ll MOD = 1'000'000'007LL;
static mt19937_64 rng(20260905);
static ll rnd(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rng); }

// =====================================================================================
// 1. Knapsack variants
// =====================================================================================

// Unbounded: minimum number of coins for every sum 0..X (INF = unreachable).
// Invariant after processing sum s: dp[s] is exact (all coins may be reused, so a coin
// can be applied to a sum that already used it -> iterate sums ascending, coins inside).
vector<ll> min_coins(const vector<ll>& c, int X) {
    const ll INF = LLONG_MAX / 4;
    vector<ll> dp(X + 1, INF);
    dp[0] = 0;
    for (int s = 1; s <= X; s++)
        for (ll v : c)
            if (v <= s && dp[s - v] + 1 < dp[s]) dp[s] = dp[s - v] + 1;
    return dp;
}

// Unbounded, count UNORDERED combinations (Coin Combinations II): coin loop outside
// so each coin type is decided once and orderings are not distinguished.
vector<ll> count_unordered(const vector<ll>& c, int X) {
    vector<ll> dp(X + 1, 0);
    dp[0] = 1;
    for (ll v : c)
        for (int s = (int)v; s <= X; s++) dp[s] = (dp[s] + dp[s - v]) % MOD;
    return dp;
}

// Bounded knapsack via binary splitting: item (w, v, cnt) -> items with multiplicities
// 1,2,4,...,rest; every count 0..cnt is a sum of a subset of those. O(W * sum log cnt).
ll bounded_knapsack(const vector<array<ll, 3>>& items, int W) {
    vector<pair<ll, ll>> split;  // (weight, value) 0/1 items
    for (auto [w, v, cnt] : items) {
        for (ll k = 1; cnt > 0; k <<= 1) {
            ll take = min(k, cnt);
            split.push_back({w * take, v * take});
            cnt -= take;
        }
    }
    vector<ll> dp(W + 1, 0);
    for (auto [w, v] : split)
        for (int cap = W; cap >= w; cap--)  // 0/1: capacity descending
            dp[cap] = max(dp[cap], dp[cap - w] + v);
    return dp[W];
}

// 0/1 subset sum with std::bitset: reach |= reach << w. 64x faster than bool DP.
// N must be a compile-time bound on the maximum sum.
template <size_t N>
bitset<N> subset_sums(const vector<int>& a) {
    bitset<N> reach;
    reach[0] = 1;
    for (int w : a) reach |= reach << w;
    return reach;
}

// =====================================================================================
// 2. LIS in O(n log n) with reconstruction (strictly increasing)
// =====================================================================================
// tailVal[k] = smallest possible last value of an increasing subsequence of length k+1
// (over the prefix scanned so far); tailIdx[k] = index of that element.
// tailVal is strictly increasing -> lower_bound finds where a[i] extends/replaces.
// prv[i] = predecessor of i in the best subsequence ending at i (set at insertion time).
vector<int> lis_indices(const vector<ll>& a) {
    int n = (int)a.size();
    vector<ll> tailVal;
    vector<int> tailIdx, prv(n, -1);
    for (int i = 0; i < n; i++) {
        int k = int(lower_bound(tailVal.begin(), tailVal.end(), a[i]) - tailVal.begin());
        if (k == (int)tailVal.size()) { tailVal.push_back(a[i]); tailIdx.push_back(i); }
        else { tailVal[k] = a[i]; tailIdx[k] = i; }
        prv[i] = k ? tailIdx[k - 1] : -1;
    }
    vector<int> res;
    if (n) for (int i = tailIdx.back(); i != -1; i = prv[i]) res.push_back(i);
    reverse(res.begin(), res.end());
    return res;
}

int lis_quadratic(const vector<ll>& a) {
    int n = (int)a.size(), best = 0;
    vector<int> dp(n, 1);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < i; j++)
            if (a[j] < a[i]) dp[i] = max(dp[i], dp[j] + 1);
        best = max(best, dp[i]);
    }
    return best;
}

// =====================================================================================
// 3. Edit distance with two rolling rows; LCS with parent pointers
// =====================================================================================
int edit_distance(const string& a, const string& b) {
    int n = (int)a.size(), m = (int)b.size();
    vector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; j++) prev[j] = j;  // a[0..0) -> b[0..j): j insertions
    for (int i = 1; i <= n; i++) {
        cur[0] = i;
        for (int j = 1; j <= m; j++)
            cur[j] = min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1])});
        swap(prev, cur);
    }
    return prev[m];
}

int edit_distance_full(const string& a, const string& b) {
    int n = (int)a.size(), m = (int)b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (int i = 0; i <= n; i++) dp[i][0] = i;
    for (int j = 0; j <= m; j++) dp[0][j] = j;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                            dp[i - 1][j - 1] + (a[i - 1] != b[j - 1])});
    return dp[n][m];
}

// LCS: full table kept because reconstruction walks it backwards.
string lcs(const string& a, const string& b) {
    int n = (int)a.size(), m = (int)b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            dp[i][j] = a[i - 1] == b[j - 1] ? dp[i - 1][j - 1] + 1 : max(dp[i - 1][j], dp[i][j - 1]);
    string res;
    for (int i = n, j = m; i > 0 && j > 0;) {
        if (a[i - 1] == b[j - 1]) { res += a[i - 1]; i--; j--; }
        else if (dp[i - 1][j] >= dp[i][j - 1]) i--;
        else j--;
    }
    reverse(res.begin(), res.end());
    return res;
}

// =====================================================================================
// 4. Tree DP
// =====================================================================================
struct Tree {
    int n;
    vector<vector<int>> adj;
    explicit Tree(int n_) : n(n_), adj(n_) {}
    void add(int a, int b) { adj[a].push_back(b); adj[b].push_back(a); }
};

// Maximum weight independent set. dp[v][0] = v not taken, dp[v][1] = v taken.
ll tree_mwis(const Tree& t, const vector<ll>& w) {
    vector<array<ll, 2>> dp(t.n);
    function<void(int, int)> dfs = [&](int v, int p) {
        dp[v] = {0, w[v]};
        for (int c : t.adj[v]) if (c != p) {
            dfs(c, v);
            dp[v][0] += max(dp[c][0], dp[c][1]);
            dp[v][1] += dp[c][0];
        }
    };
    dfs(0, -1);
    return max(dp[0][0], dp[0][1]);
}

// Maximum matching on a tree. dp[v][0] = v unmatched to a child, dp[v][1] = v matched
// to exactly one child c (then c must be unmatched below: dp[c][0]).
int tree_matching(const Tree& t) {
    vector<array<int, 2>> dp(t.n);
    function<void(int, int)> dfs = [&](int v, int p) {
        dp[v] = {0, INT_MIN / 2};
        for (int c : t.adj[v]) if (c != p) { dfs(c, v); dp[v][0] += max(dp[c][0], dp[c][1]); }
        for (int c : t.adj[v]) if (c != p)
            dp[v][1] = max(dp[v][1], dp[v][0] - max(dp[c][0], dp[c][1]) + dp[c][0] + 1);
    };
    dfs(0, -1);
    return max(dp[0][0], dp[0][1]);
}

// Subtree knapsack merging: f[v][k] = number of connected vertex sets of size k whose
// topmost vertex is v. Merge child c: new[i+j] += f[v][i] * f[c][j]  (f[c][0] := 1 =
// "take nothing from c's subtree"). The double loop is bounded by |f[v]| * |f[c]|;
// summed over the whole tree this is O(n^2) (every pair of vertices is charged once,
// at their LCA when their subtrees are merged).
vector<vector<ll>> connected_subsets_by_top(const Tree& t) {
    vector<vector<ll>> f(t.n);
    function<void(int, int)> dfs = [&](int v, int p) {
        f[v] = {0, 1};  // sizes 0 (unused) and 1 (just v)
        for (int c : t.adj[v]) if (c != p) {
            dfs(c, v);
            vector<ll> g(f[v].size() + f[c].size() - 1, 0);
            for (size_t i = 1; i < f[v].size(); i++) {
                g[i] += f[v][i];  // child contributes nothing
                for (size_t j = 1; j < f[c].size(); j++) g[i + j] += f[v][i] * f[c][j];
            }
            f[v] = std::move(g);
        }
    };
    dfs(0, -1);
    return f;
}

// =====================================================================================
// 5. Digit DP: count integers in [0, N] with no two adjacent equal digits
// =====================================================================================
// State: (pos, prev digit or 10 = "nothing yet", started). tight is not memoised: the
// tight branch is followed exactly once per position, all other branches are free.
ll count_no_adjacent_equal(ll N) {
    if (N < 0) return 0;
    string s = to_string(N);
    int L = (int)s.size();
    // memo[pos][prev][started] for the non-tight case
    vector<vector<array<ll, 2>>> memo(L, vector<array<ll, 2>>(11, {-1, -1}));
    function<ll(int, int, bool, bool)> go = [&](int pos, int prev, bool started, bool tight) -> ll {
        if (pos == L) return 1;  // the number 0 (never started) also counts
        if (!tight && memo[pos][prev][started] != -1) return memo[pos][prev][started];
        int hi = tight ? s[pos] - '0' : 9;
        ll res = 0;
        for (int d = 0; d <= hi; d++) {
            bool nstarted = started || d != 0;
            if (nstarted && d == prev) continue;
            int nprev = nstarted ? d : 10;
            res += go(pos + 1, nprev, nstarted, tight && d == hi);
        }
        if (!tight) memo[pos][prev][started] = res;
        return res;
    };
    return go(0, 10, false, true);
}

bool ok_no_adjacent_equal(ll x) {
    string s = to_string(x);
    for (size_t i = 1; i < s.size(); i++) if (s[i] == s[i - 1]) return false;
    return true;
}

// =====================================================================================
// 6. Bitmask DP
// =====================================================================================
// Shortest Hamiltonian path starting at vertex 0. dp[mask][v] = min cost to visit
// exactly the set mask, ending at v. O(2^n n^2).
ll tsp_path(const vector<vector<ll>>& d) {
    int n = (int)d.size();
    const ll INF = LLONG_MAX / 4;
    vector<vector<ll>> dp(1 << n, vector<ll>(n, INF));
    dp[1][0] = 0;
    for (int mask = 1; mask < (1 << n); mask++)
        for (int v = 0; v < n; v++) {
            if (dp[mask][v] == INF) continue;
            for (int u = 0; u < n; u++) if (!(mask >> u & 1))
                dp[mask | 1 << u][u] = min(dp[mask | 1 << u][u], dp[mask][v] + d[v][u]);
        }
    ll best = INF;
    for (int v = 0; v < n; v++) best = min(best, dp[(1 << n) - 1][v]);
    return best;
}

// Elevator Rides: dp[mask] = (rides used, weight in the last ride), minimised
// lexicographically. Adding person p: fits in the current ride or starts a new one.
int elevator_rides(const vector<ll>& w, ll cap) {
    int n = (int)w.size();
    vector<pair<int, ll>> dp(1 << n, {INT_MAX, 0});
    dp[0] = {1, 0};
    for (int mask = 1; mask < (1 << n); mask++)
        for (int p = 0; p < n; p++) if (mask >> p & 1) {
            auto [r, last] = dp[mask ^ (1 << p)];
            pair<int, ll> cand = last + w[p] <= cap ? make_pair(r, last + w[p]) : make_pair(r + 1, w[p]);
            dp[mask] = min(dp[mask], cand);
        }
    return dp[(1 << n) - 1].first;
}

// Broken-profile DP for domino tilings of an n x m grid (n <= ~10 rows, m columns).
// Cells are processed column by column, top to bottom. mask bit i = cell (i, current
// column) is already covered by a horizontal domino started in the previous column.
// At cell (i, j): if covered, skip; else place vertical (needs i+1 free in same column)
// or horizontal (sets bit i for the next column).
ll count_tilings(int n, int m) {
    vector<ll> dp(1 << n, 0), nxt(1 << n);
    dp[0] = 1;
    for (int j = 0; j < m; j++) {
        for (int i = 0; i < n; i++) {
            fill(nxt.begin(), nxt.end(), 0);
            for (int mask = 0; mask < (1 << n); mask++) {
                if (!dp[mask]) continue;
                if (mask >> i & 1) {  // already covered: clear the bit, move on
                    nxt[mask ^ (1 << i)] = (nxt[mask ^ (1 << i)] + dp[mask]) % MOD;
                } else {
                    if (j + 1 < m)  // horizontal domino into the next column
                        nxt[mask | 1 << i] = (nxt[mask | 1 << i] + dp[mask]) % MOD;
                    if (i + 1 < n && !(mask >> (i + 1) & 1)) {  // vertical domino
                        // covers (i,j) and (i+1,j): mark i+1 as "covered" so the next
                        // step skips it. Represented by setting bit i+1 (cleared at i+1).
                        nxt[mask | 1 << (i + 1)] = (nxt[mask | 1 << (i + 1)] + dp[mask]) % MOD;
                    }
                }
            }
            swap(dp, nxt);
        }
    }
    return dp[0];
}

ll brute_tilings(int n, int m) {
    vector<vector<int>> g(n, vector<int>(m, 0));
    function<ll()> rec = [&]() -> ll {
        int fi = -1, fj = -1;
        for (int j = 0; j < m && fi < 0; j++)
            for (int i = 0; i < n; i++) if (!g[i][j]) { fi = i; fj = j; break; }
        if (fi < 0) return 1;
        ll r = 0;
        if (fi + 1 < n && !g[fi + 1][fj]) { g[fi][fj] = g[fi + 1][fj] = 1; r += rec(); g[fi][fj] = g[fi + 1][fj] = 0; }
        if (fj + 1 < m && !g[fi][fj + 1]) { g[fi][fj] = g[fi][fj + 1] = 1; r += rec(); g[fi][fj] = g[fi][fj + 1] = 0; }
        return r;
    };
    return rec();
}

// =====================================================================================
// 7. Knuth optimization
// =====================================================================================
// dp[l][r] = min_{l<=k<r} dp[l][k] + dp[k+1][r] + w(l, r) with w = range sum
// (merging adjacent piles). w satisfies the quadrangle inequality and is monotone on
// nested intervals, hence opt[l][r-1] <= opt[l][r] <= opt[l+1][r]  ->  O(n^2) total.
ll knuth(const vector<ll>& a) {
    int n = (int)a.size();
    vector<ll> pre(n + 1, 0);
    for (int i = 0; i < n; i++) pre[i + 1] = pre[i] + a[i];
    const ll INF = LLONG_MAX / 4;
    vector<vector<ll>> dp(n, vector<ll>(n, 0));
    vector<vector<int>> opt(n, vector<int>(n, 0));
    for (int i = 0; i < n; i++) opt[i][i] = i;
    for (int len = 2; len <= n; len++)
        for (int l = 0; l + len - 1 < n; l++) {
            int r = l + len - 1;
            dp[l][r] = INF;
            int lo = opt[l][r - 1], hi = min(opt[l + 1][r], r - 1);
            for (int k = lo; k <= hi; k++) {
                ll cand = dp[l][k] + dp[k + 1][r] + pre[r + 1] - pre[l];
                if (cand < dp[l][r]) { dp[l][r] = cand; opt[l][r] = k; }
            }
        }
    return dp[0][n - 1];
}

ll interval_cubic(const vector<ll>& a) {
    int n = (int)a.size();
    vector<ll> pre(n + 1, 0);
    for (int i = 0; i < n; i++) pre[i + 1] = pre[i] + a[i];
    const ll INF = LLONG_MAX / 4;
    vector<vector<ll>> dp(n, vector<ll>(n, 0));
    for (int len = 2; len <= n; len++)
        for (int l = 0; l + len - 1 < n; l++) {
            int r = l + len - 1;
            dp[l][r] = INF;
            for (int k = l; k < r; k++)
                dp[l][r] = min(dp[l][r], dp[l][k] + dp[k + 1][r] + pre[r + 1] - pre[l]);
        }
    return dp[0][n - 1];
}

// =====================================================================================
// 8. Divide & conquer optimization
// =====================================================================================
// Partition a[0..n) into exactly K contiguous segments; cost of segment (j, i] is
// (pre[i]-pre[j])^2. dp[k][i] = min_{j<i} dp[k-1][j] + C(j, i).
// C satisfies the quadrangle inequality -> opt(i) is non-decreasing in i for fixed k,
// so compute the middle i, find its opt, recurse on the halves with restricted ranges.
// Each layer O(n log n); total O(K n log n).
struct DnC {
    int n;
    vector<ll> pre;
    vector<ll> prevLayer, curLayer;
    ll C(int j, int i) const { ll s = pre[i] - pre[j]; return s * s; }
    void compute(int lo, int hi, int optlo, int opthi) {
        if (lo > hi) return;
        int mid = (lo + hi) / 2;
        ll best = LLONG_MAX / 4;
        int bestj = optlo;
        for (int j = optlo; j <= min(mid - 1, opthi); j++) {
            ll cand = prevLayer[j] + C(j, mid);
            if (cand < best) { best = cand; bestj = j; }
        }
        curLayer[mid] = best;
        compute(lo, mid - 1, optlo, bestj);
        compute(mid + 1, hi, bestj, opthi);
    }
    ll solve(const vector<ll>& a, int K) {
        n = (int)a.size();
        pre.assign(n + 1, 0);
        for (int i = 0; i < n; i++) pre[i + 1] = pre[i] + a[i];
        const ll INF = LLONG_MAX / 4;
        prevLayer.assign(n + 1, INF);
        curLayer.assign(n + 1, INF);
        prevLayer[0] = 0;  // 0 segments cover prefix of length 0
        for (int k = 1; k <= K; k++) {
            fill(curLayer.begin(), curLayer.end(), INF);
            compute(k, n, k - 1, n - 1);  // i in [k, n], j in [k-1, i-1]
            swap(prevLayer, curLayer);
        }
        return prevLayer[n];
    }
};

ll partition_quadratic(const vector<ll>& a, int K) {
    int n = (int)a.size();
    vector<ll> pre(n + 1, 0);
    for (int i = 0; i < n; i++) pre[i + 1] = pre[i] + a[i];
    const ll INF = LLONG_MAX / 4;
    vector<vector<ll>> dp(K + 1, vector<ll>(n + 1, INF));
    dp[0][0] = 0;
    for (int k = 1; k <= K; k++)
        for (int i = k; i <= n; i++)
            for (int j = k - 1; j < i; j++)
                if (dp[k - 1][j] < INF)
                    dp[k][i] = min(dp[k][i], dp[k - 1][j] + (pre[i] - pre[j]) * (pre[i] - pre[j]));
    return dp[K][n];
}

// =====================================================================================
// 9. Convex hull trick
// =====================================================================================
// Minimum over lines y = m x + c. Monotone version: lines are added with NON-INCREASING
// slopes, queries come with NON-DECREASING x. Deque front holds the optimal line for the
// current x; lines that can never be optimal again are popped. Amortised O(1) per op.
struct MonoCHT {
    deque<pair<ll, ll>> q;  // (m, c)
    // line l2 is useless between l1 and l3 (slopes m1 >= m2 >= m3) iff
    // intersection(l1,l3) is left of intersection(l1,l2):
    // (c3-c1)/(m1-m3) <= (c2-c1)/(m1-m2)  <=>  (c3-c1)(m1-m2) <= (c2-c1)(m1-m3)
    static bool bad(pair<ll, ll> l1, pair<ll, ll> l2, pair<ll, ll> l3) {
        return (__int128)(l3.second - l1.second) * (l1.first - l2.first) <=
               (__int128)(l2.second - l1.second) * (l1.first - l3.first);
    }
    void add(ll m, ll c) {  // requires m <= slope of previously added line
        pair<ll, ll> nl{m, c};
        if (!q.empty() && q.back().first == m) {  // parallel: keep the lower one
            if (q.back().second <= c) return;
            q.pop_back();
        }
        while (q.size() >= 2 && bad(q[q.size() - 2], q.back(), nl)) q.pop_back();
        q.push_back(nl);
    }
    ll query(ll x) {  // requires x >= previous query x
        while (q.size() >= 2 && q[1].first * x + q[1].second <= q[0].first * x + q[0].second) q.pop_front();
        return q[0].first * x + q[0].second;
    }
};

// Li Chao tree over integer x in [lo, hi]: arbitrary insertion order, min queries.
// Each node keeps the line that is best at the node's midpoint; the other line can be
// better on at most one half -> push it down. O(log range) per op.
struct LiChao {
    struct Line { ll m, c; ll at(ll x) const { return m * x + c; } };
    int lo, hi;
    vector<Line> tr;
    vector<char> has;
    LiChao(int lo_, int hi_) : lo(lo_), hi(hi_) {
        int sz = 1; while (sz < hi - lo + 1) sz <<= 1;
        tr.assign(2 * sz, {0, 0}); has.assign(2 * sz, 0);
    }
    void add(Line nl) { add(1, lo, hi, nl); }
    void add(int node, int l, int r, Line nl) {
        if (!has[node]) { tr[node] = nl; has[node] = 1; return; }
        int mid = (l + r) >> 1;
        bool leftBetter = nl.at(l) < tr[node].at(l);
        bool midBetter = nl.at(mid) < tr[node].at(mid);
        if (midBetter) swap(tr[node], nl);  // node keeps the winner at mid
        if (l == r) return;
        if (leftBetter != midBetter) add(2 * node, l, mid, nl);
        else add(2 * node + 1, mid + 1, r, nl);
    }
    ll query(ll x) const {
        int node = 1, l = lo, r = hi;
        ll res = LLONG_MAX / 4;
        while (true) {
            if (has[node]) res = min(res, tr[node].at(x));
            if (l == r) break;
            int mid = (l + r) >> 1;
            if (x <= mid) { node = 2 * node; r = mid; } else { node = 2 * node + 1; l = mid + 1; }
        }
        return res;
    }
};

// Test recurrence: dp[0] = 0; dp[i] = min_{j<i} dp[j] + b[j] * a[i] + c[i].
// Line j: slope b[j], intercept dp[j]; query at x = a[i].
vector<ll> cht_dp_brute(const vector<ll>& a, const vector<ll>& b, const vector<ll>& c) {
    int n = (int)a.size();
    vector<ll> dp(n, 0);
    for (int i = 1; i < n; i++) {
        dp[i] = LLONG_MAX / 4;
        for (int j = 0; j < i; j++) dp[i] = min(dp[i], dp[j] + b[j] * a[i] + c[i]);
    }
    return dp;
}

// =====================================================================================
// 10. Slope trick: min sum |a_i - b_i| with b non-decreasing
// =====================================================================================
// f_i(x) = min cost for prefix i with b_i <= x; convex piecewise linear, slopes -k..0.
// The breakpoints of the left (negative-slope) part live in a max-heap L. Adding a_i:
// push a_i; if max(L) > a_i, the minimum must move to a_i: cost += max(L) - a_i, replace
// max by a_i (the "prefix min" of f_i + |x - a_i|).
ll make_nondecreasing_cost(const vector<ll>& a) {
    priority_queue<ll> L;
    ll cost = 0;
    for (ll x : a) {
        L.push(x);
        if (L.top() > x) { cost += L.top() - x; L.pop(); L.push(x); }
    }
    return cost;
}

ll make_nondecreasing_brute(const vector<ll>& a, ll V) {  // values in [0, V]
    // dp[v] = min cost so far with b_i = v; prefix-min gives "b_i <= v"
    vector<ll> dp(V + 1, 0);
    for (ll x : a) {
        vector<ll> nd(V + 1);
        ll run = LLONG_MAX / 4;
        for (ll v = 0; v <= V; v++) { run = min(run, dp[v]); nd[v] = run + llabs(x - v); }
        dp = nd;
    }
    return *min_element(dp.begin(), dp.end());
}

// =====================================================================================
// 11. Aliens trick (Lagrangian relaxation)
// =====================================================================================
// Same problem as section 8 (min total squared segment sums with EXACTLY K segments).
// f(K) is convex in K (cost satisfies the quadrangle inequality). Relax: pay lambda per
// segment, no count constraint -> g(lambda) = min_k f(k) + lambda k, solved by a plain
// 1D DP (O(n^2) here; in a contest use CHT/D&C for O(n log n)). Track the smallest
// segment count among optima. Binary search the smallest lambda with cnt <= K; then
// f(K) = g(lambda) - lambda K.
pair<ll, int> penalized(const vector<ll>& pre, ll lambda) {
    int n = (int)pre.size() - 1;
    const ll INF = LLONG_MAX / 4;
    vector<pair<ll, int>> dp(n + 1, {INF, 0});  // (cost incl. penalties, #segments)
    dp[0] = {0, 0};
    for (int i = 1; i <= n; i++)
        for (int j = 0; j < i; j++) {
            ll s = pre[i] - pre[j];
            pair<ll, int> cand{dp[j].first + s * s + lambda, dp[j].second + 1};
            dp[i] = min(dp[i], cand);  // lexicographic: min cost, then fewest segments
        }
    return dp[n];
}

ll aliens_partition(const vector<ll>& a, int K) {
    int n = (int)a.size();
    vector<ll> pre(n + 1, 0);
    for (int i = 0; i < n; i++) pre[i + 1] = pre[i] + a[i];
    ll lo = 0, hi = pre[n] * pre[n] + 1;  // at hi, one segment is always optimal
    while (lo < hi) {  // smallest lambda with cnt(lambda) <= K
        ll mid = lo + (hi - lo) / 2;
        if (penalized(pre, mid).second <= K) hi = mid; else lo = mid + 1;
    }
    auto [g, cnt] = penalized(pre, lo);
    (void)cnt;
    return g - lo * K;
}

// =====================================================================================
// 12. SOS DP: F[mask] = sum over submasks s of A[s]
// =====================================================================================
vector<ll> sos(vector<ll> A, int bits) {
    for (int b = 0; b < bits; b++)
        for (int mask = 0; mask < (1 << bits); mask++)
            if (mask >> b & 1) A[mask] += A[mask ^ (1 << b)];
    return A;
}

// =====================================================================================
// 13. DP with a Fenwick tree: number of strictly increasing subsequences (mod)
// =====================================================================================
struct Fenwick {
    int n; vector<ll> t;
    explicit Fenwick(int n_) : n(n_), t(n_ + 1, 0) {}
    void add(int i, ll v) { for (i++; i <= n; i += i & -i) t[i] = (t[i] + v) % MOD; }
    ll sum(int i) { ll s = 0; for (i++; i > 0; i -= i & -i) s = (s + t[i]) % MOD; return s; }  // [0, i]
};

ll count_increasing_subsequences(const vector<ll>& a) {
    // dp[i] = 1 + sum_{j<i, a_j<a_i} dp[j]; answer = sum dp[i]. Fenwick indexed by rank.
    vector<ll> vals(a);
    sort(vals.begin(), vals.end());
    vals.erase(unique(vals.begin(), vals.end()), vals.end());
    Fenwick fw((int)vals.size());
    ll total = 0;
    for (ll x : a) {
        int r = int(lower_bound(vals.begin(), vals.end(), x) - vals.begin());
        ll dp = (1 + (r ? fw.sum(r - 1) : 0)) % MOD;
        fw.add(r, dp);
        total = (total + dp) % MOD;
    }
    return total;
}

ll count_increasing_subsequences_brute(const vector<ll>& a) {
    int n = (int)a.size();
    vector<ll> dp(n);
    ll total = 0;
    for (int i = 0; i < n; i++) {
        dp[i] = 1;
        for (int j = 0; j < i; j++) if (a[j] < a[i]) dp[i] = (dp[i] + dp[j]) % MOD;
        total = (total + dp[i]) % MOD;
    }
    return total;
}

// =====================================================================================
// 14. Probability DP: distribution of the sum of n fair dice
// =====================================================================================
vector<double> dice_sum_distribution(int n) {
    vector<double> dp(6 * n + 1, 0.0);
    dp[0] = 1.0;
    for (int t = 1; t <= n; t++) {
        vector<double> nd(6 * n + 1, 0.0);
        for (int s = 0; s <= 6 * (t - 1); s++)
            if (dp[s] > 0) for (int f = 1; f <= 6; f++) nd[s + f] += dp[s] / 6.0;
        dp = nd;
    }
    return dp;
}

// =====================================================================================
// 15. DP over a KMP automaton: number of strings of length n over an alphabet of size k
//     that contain pattern p (= total - avoiding)
// =====================================================================================
vector<int> prefix_function(const string& p) {
    int m = (int)p.size();
    vector<int> pi(m, 0);
    for (int i = 1; i < m; i++) {
        int k = pi[i - 1];
        while (k > 0 && p[i] != p[k]) k = pi[k - 1];
        if (p[i] == p[k]) k++;
        pi[i] = k;
    }
    return pi;
}

// aut[state][c] = next matched length after reading character c in state "state".
vector<vector<int>> kmp_automaton(const string& p, int k) {
    int m = (int)p.size();
    vector<int> pi = prefix_function(p);
    vector<vector<int>> aut(m + 1, vector<int>(k, 0));
    for (int s = 0; s <= m; s++)
        for (int c = 0; c < k; c++) {
            if (s < m && p[s] - 'a' == c) aut[s][c] = s + 1;
            else aut[s][c] = s == 0 ? 0 : aut[pi[s - 1]][c];
        }
    return aut;
}

ll count_containing(int n, int k, const string& p) {
    int m = (int)p.size();
    auto aut = kmp_automaton(p, k);
    vector<ll> dp(m, 0), nd(m);  // dp[s] = #strings so far in state s < m (not yet matched)
    dp[0] = 1;
    ll total = 1;
    for (int i = 0; i < n; i++) {
        fill(nd.begin(), nd.end(), 0);
        for (int s = 0; s < m; s++) if (dp[s])
            for (int c = 0; c < k; c++) {
                int t = aut[s][c];
                if (t < m) nd[t] = (nd[t] + dp[s]) % MOD;
            }
        dp = nd;
        total = total * k % MOD;
    }
    ll avoiding = 0;
    for (ll v : dp) avoiding = (avoiding + v) % MOD;
    return ((total - avoiding) % MOD + MOD) % MOD;
}

// =====================================================================================
// main: tests
// =====================================================================================
int main() {
    // ---- 1. knapsack variants
    {
        vector<ll> coins = {1, 3, 4};
        auto mc = min_coins(coins, 10);
        assert(mc[6] == 2 && mc[10] == 3 && mc[0] == 0);
        auto cu = count_unordered(coins, 10);
        assert(cu[4] == 3);  // 4 = 1+1+1+1 = 1+3 = 4
        for (int it = 0; it < 200; it++) {
            int k = (int)rnd(1, 4), W = (int)rnd(0, 40);
            vector<array<ll, 3>> items;
            vector<pair<ll, ll>> expanded;
            for (int i = 0; i < k; i++) {
                ll w = rnd(1, 8), v = rnd(0, 20), c = rnd(1, 9);
                items.push_back({w, v, c});
                for (ll t = 0; t < c; t++) expanded.push_back({w, v});
            }
            vector<ll> dp(W + 1, 0);
            for (auto [w, v] : expanded)
                for (int cap = W; cap >= w; cap--) dp[cap] = max(dp[cap], dp[cap - w] + v);
            assert(bounded_knapsack(items, W) == dp[W]);
        }
        for (int it = 0; it < 100; it++) {
            int n = (int)rnd(1, 12);
            vector<int> a(n);
            for (int& x : a) x = (int)rnd(1, 30);
            auto bs = subset_sums<512>(a);
            vector<char> can(512, 0);
            can[0] = 1;
            for (int x : a) for (int s = 511; s >= x; s--) if (can[s - x]) can[s] = 1;
            for (int s = 0; s < 512; s++) assert(bs[s] == (bool)can[s]);
        }
    }
    // ---- 2. LIS
    for (int it = 0; it < 300; it++) {
        int n = (int)rnd(0, 40);
        vector<ll> a(n);
        for (ll& x : a) x = rnd(0, 15);
        auto idx = lis_indices(a);
        assert((int)idx.size() == lis_quadratic(a));
        for (size_t i = 1; i < idx.size(); i++) assert(idx[i - 1] < idx[i] && a[idx[i - 1]] < a[idx[i]]);
    }
    // ---- 3. edit distance & LCS
    for (int it = 0; it < 300; it++) {
        auto rs = [&](int n) { string s; for (int i = 0; i < n; i++) s += char('a' + rnd(0, 2)); return s; };
        string a = rs((int)rnd(0, 12)), b = rs((int)rnd(0, 12));
        assert(edit_distance(a, b) == edit_distance_full(a, b));
        string l = lcs(a, b);
        // l must be a subsequence of both, and |l| = |a|+|b|-(edit distance with only ins/del)
        auto isSub = [](const string& s, const string& t) {
            size_t i = 0;
            for (char c : t) if (i < s.size() && s[i] == c) i++;
            return i == s.size();
        };
        assert(isSub(l, a) && isSub(l, b));
        // LCS length via O(nm) table check against a brute recursion for tiny sizes
        if (a.size() <= 8 && b.size() <= 8) {
            function<int(int, int)> rec = [&](int i, int j) -> int {
                if (i == (int)a.size() || j == (int)b.size()) return 0;
                if (a[i] == b[j]) return 1 + rec(i + 1, j + 1);
                return max(rec(i + 1, j), rec(i, j + 1));
            };
            assert((int)l.size() == rec(0, 0));
        }
    }
    // ---- 4. tree DP
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(1, 11);
        Tree t(n);
        vector<pair<int, int>> edges;
        for (int v = 1; v < n; v++) { int p = (int)rnd(0, v - 1); t.add(p, v); edges.push_back({p, v}); }
        vector<ll> w(n);
        for (ll& x : w) x = rnd(0, 10);
        // brute over vertex subsets: independent set, connected subsets by size
        ll bestIS = 0;
        vector<ll> connBySize(n + 1, 0);
        for (int mask = 0; mask < (1 << n); mask++) {
            bool indep = true;
            for (auto [a, b] : edges) if ((mask >> a & 1) && (mask >> b & 1)) indep = false;
            if (indep) { ll s = 0; for (int v = 0; v < n; v++) if (mask >> v & 1) s += w[v]; bestIS = max(bestIS, s); }
            if (mask) {
                int start = __builtin_ctz(mask), seen = 1 << start;
                vector<int> st = {start};
                while (!st.empty()) {
                    int v = st.back(); st.pop_back();
                    for (int u : t.adj[v]) if ((mask >> u & 1) && !(seen >> u & 1)) { seen |= 1 << u; st.push_back(u); }
                }
                if (seen == mask) connBySize[__builtin_popcount(mask)]++;
            }
        }
        assert(tree_mwis(t, w) == bestIS);
        auto f = connected_subsets_by_top(t);
        for (int k = 1; k <= n; k++) {
            ll s = 0;
            for (int v = 0; v < n; v++) if (k < (int)f[v].size()) s += f[v][k];
            assert(s == connBySize[k]);
        }
        // brute matching over edge subsets
        int m = (int)edges.size(), bestM = 0;
        for (int em = 0; em < (1 << m); em++) {
            vector<int> deg(n, 0);
            bool ok = true;
            for (int e = 0; e < m; e++) if (em >> e & 1) { if (++deg[edges[e].first] > 1 || ++deg[edges[e].second] > 1) ok = false; }
            if (ok) bestM = max(bestM, __builtin_popcount(em));
        }
        assert(tree_matching(t) == bestM);
    }
    // ---- 5. digit DP
    {
        ll cnt = 0;
        for (ll x = 0; x <= 30000; x++) {
            if (ok_no_adjacent_equal(x)) cnt++;
            if (x % 997 == 0 || x == 30000) assert(count_no_adjacent_equal(x) == cnt);
        }
        assert(count_no_adjacent_equal(9) == 10 && count_no_adjacent_equal(99) == 91);
    }
    // ---- 6. bitmask DP
    for (int it = 0; it < 30; it++) {
        int n = (int)rnd(1, 7);
        vector<vector<ll>> d(n, vector<ll>(n));
        for (auto& row : d) for (ll& x : row) x = rnd(0, 50);
        vector<int> perm(n);
        iota(perm.begin(), perm.end(), 0);
        ll best = LLONG_MAX;
        do {
            if (perm[0] != 0) continue;
            ll c = 0;
            for (int i = 1; i < n; i++) c += d[perm[i - 1]][perm[i]];
            best = min(best, c);
        } while (next_permutation(perm.begin(), perm.end()));
        assert(tsp_path(d) == best);
    }
    for (int it = 0; it < 60; it++) {
        int n = (int)rnd(1, 7);
        ll cap = rnd(5, 20);
        vector<ll> w(n);
        for (ll& x : w) x = rnd(1, cap);
        // brute: assign each person to a group id in [0, groups]
        int best = n;
        vector<ll> load;
        function<void(int)> rec = [&](int i) {
            if ((int)load.size() >= best) return;
            if (i == n) { best = min(best, (int)load.size()); return; }
            for (size_t g = 0; g < load.size(); g++) if (load[g] + w[i] <= cap) { load[g] += w[i]; rec(i + 1); load[g] -= w[i]; }
            load.push_back(w[i]); rec(i + 1); load.pop_back();
        };
        rec(0);
        assert(elevator_rides(w, cap) == best);
    }
    for (int n = 1; n <= 5; n++)
        for (int m = 1; m <= 5; m++) assert(count_tilings(n, m) == brute_tilings(n, m));
    assert(count_tilings(2, 3) == 3 && count_tilings(4, 4) == 36 && count_tilings(8, 8) == 12988816);
    // ---- 7. Knuth
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(1, 25);
        vector<ll> a(n);
        for (ll& x : a) x = rnd(0, 100);
        assert(knuth(a) == interval_cubic(a));
    }
    // ---- 8. divide & conquer optimization
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(1, 30), K = (int)rnd(1, n);
        vector<ll> a(n);
        for (ll& x : a) x = rnd(0, 20);
        DnC solver;
        assert(solver.solve(a, K) == partition_quadratic(a, K));
        // ---- 11. Aliens on the same instance
        assert(aliens_partition(a, K) == partition_quadratic(a, K));
    }
    // ---- 9. convex hull trick
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(1, 60);
        vector<ll> a(n), b(n), c(n);
        for (ll& x : a) x = rnd(0, 1000);
        for (ll& x : b) x = rnd(-1000, 1000);
        for (ll& x : c) x = rnd(-1000, 1000);
        sort(a.begin(), a.end());                     // queries non-decreasing
        sort(b.rbegin(), b.rend());                   // slopes non-increasing
        auto want = cht_dp_brute(a, b, c);
        MonoCHT hull;
        vector<ll> dp(n, 0);
        hull.add(b[0], dp[0]);
        for (int i = 1; i < n; i++) {
            dp[i] = hull.query(a[i]) + c[i];
            hull.add(b[i], dp[i]);
        }
        assert(dp == want);
        // Li Chao: arbitrary slopes and query order
        shuffle(b.begin(), b.end(), rng);
        shuffle(a.begin(), a.end(), rng);
        want = cht_dp_brute(a, b, c);
        LiChao lc(0, 1000);
        vector<ll> dp2(n, 0);
        lc.add({b[0], dp2[0]});
        for (int i = 1; i < n; i++) {
            dp2[i] = lc.query(a[i]) + c[i];
            lc.add({b[i], dp2[i]});
        }
        assert(dp2 == want);
    }
    // ---- 10. slope trick
    for (int it = 0; it < 200; it++) {
        int n = (int)rnd(1, 12);
        vector<ll> a(n);
        for (ll& x : a) x = rnd(0, 15);
        assert(make_nondecreasing_cost(a) == make_nondecreasing_brute(a, 15));
    }
    // ---- 12. SOS
    {
        int bits = 6;
        vector<ll> A(1 << bits);
        for (ll& x : A) x = rnd(0, 100);
        auto F = sos(A, bits);
        for (int mask = 0; mask < (1 << bits); mask++) {
            ll s = 0;
            for (int sub = mask;; sub = (sub - 1) & mask) { s += A[sub]; if (sub == 0) break; }
            assert(F[mask] == s);
        }
    }
    // ---- 13. Fenwick-accelerated DP
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(0, 40);
        vector<ll> a(n);
        for (ll& x : a) x = rnd(0, 10);
        assert(count_increasing_subsequences(a) == count_increasing_subsequences_brute(a));
    }
    // ---- 14. probability DP
    {
        auto p = dice_sum_distribution(3);
        double total = 0;
        for (double x : p) total += x;
        assert(fabs(total - 1.0) < 1e-12);
        vector<int> cnt(19, 0);
        for (int x = 1; x <= 6; x++) for (int y = 1; y <= 6; y++) for (int z = 1; z <= 6; z++) cnt[x + y + z]++;
        for (int s = 3; s <= 18; s++) assert(fabs(p[s] - cnt[s] / 216.0) < 1e-12);
    }
    // ---- 15. KMP automaton DP
    for (int it = 0; it < 60; it++) {
        int k = (int)rnd(2, 3), n = (int)rnd(1, 8), m = (int)rnd(1, min(n, 4));
        string p;
        for (int i = 0; i < m; i++) p += char('a' + rnd(0, k - 1));
        ll brute = 0, total = 1;
        for (int i = 0; i < n; i++) total *= k;
        for (ll code = 0; code < total; code++) {
            string s; ll c = code;
            for (int i = 0; i < n; i++) { s += char('a' + c % k); c /= k; }
            if (s.find(p) != string::npos) brute++;
        }
        assert(count_containing(n, k, p) == brute % MOD);
    }
    puts("all chapter 13 tests passed");
    return 0;
}
