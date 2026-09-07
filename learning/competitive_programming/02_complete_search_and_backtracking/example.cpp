// Chapter 02 — Complete Search, Backtracking, Search on the Answer, Greedy: reference library.
//
// RE-TYPE THIS FILE FROM MEMORY — that is the training. Each block is a snippet you must be able
// to produce in a contest in under two minutes: subset/permutation generation, backtracking with
// pruning (N-queens, sudoku, grid paths), meet in the middle, IDDFS, bidirectional BFS, branch
// and bound, ternary/golden-section search, binary search on reals and on the answer, and the
// greedy classics with their exchange arguments in comments. main() cross-checks every fast
// method against a brute force on random or classic inputs with assert.
//
// Build & run (must be warning-free):
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
// Explicit standard headers so Apple clang compiles it; on a GCC judge use <bits/stdc++.h>.

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using ll = long long;
#define all(x) begin(x), end(x)

mt19937_64 rng(20260905);
ll rnd(ll lo, ll hi) { return (ll)(rng() % (unsigned long long)(hi - lo + 1)) + lo; }

// ═════════════════════════════ 1. Generating subsets ═════════════════════════════════════
// (a) recursive: at element k decide "skip" or "take". O(2^n) leaves, depth n.
void subsets_rec(int k, int n, vector<int>& cur, vector<vector<int>>& out) {
    if (k == n) { out.push_back(cur); return; }
    subsets_rec(k + 1, n, cur, out);              // element k not in subset
    cur.push_back(k);
    subsets_rec(k + 1, n, cur, out);              // element k in subset
    cur.pop_back();
}
// (b) bitmask: subset ↔ integer 0..2^n-1, bit i set ⇔ element i chosen. O(2^n · n) to expand.
vector<vector<int>> subsets_mask(int n) {
    vector<vector<int>> out;
    for (int m = 0; m < (1 << n); m++) {
        vector<int> s;
        for (int i = 0; i < n; i++) if (m >> i & 1) s.push_back(i);
        out.push_back(s);
    }
    return out;
}
// (c) all subset sums in O(2^n) total: sum[m] = sum[m without lowest bit] + w[lowest bit].
vector<ll> subset_sums(const vector<ll>& w) {
    int n = (int)w.size();
    vector<ll> s(1 << n, 0);
    for (int m = 1; m < (1 << n); m++) s[m] = s[m & (m - 1)] + w[__builtin_ctz(m)];
    return s;
}
// (d) Gray code order: consecutive subsets differ in exactly one element → O(1) update per step.
int gray(int i) { return i ^ (i >> 1); }
// (e) all submasks of `mask`, descending. Total over all masks of n bits: 3^n.
vector<int> submasks(int mask) {
    vector<int> out;
    for (int s = mask; ; s = (s - 1) & mask) { out.push_back(s); if (s == 0) break; }
    return out;
}

// ═════════════════════════════ 2. Generating permutations ════════════════════════════════
// (a) recursive with a used[] array: O(n!) leaves. With duplicates, produces duplicates too.
void perms_rec(int n, vector<int>& cur, vector<bool>& used, vector<vector<int>>& out) {
    if ((int)cur.size() == n) { out.push_back(cur); return; }
    for (int i = 0; i < n; i++) {
        if (used[i]) continue;
        used[i] = true; cur.push_back(i);
        perms_rec(n, cur, used, out);
        cur.pop_back(); used[i] = false;
    }
}
// (b) std::next_permutation: MUST start sorted; with duplicates yields each DISTINCT order once,
// in lexicographic order. Amortised O(1) per step.
template <class T> vector<vector<T>> perms_next(vector<T> v) {
    sort(all(v));
    vector<vector<T>> out;
    do out.push_back(v); while (next_permutation(all(v)));
    return out;
}

// ═════════════════════════════ 3. Backtracking with pruning ══════════════════════════════
// N-queens: one queen per column; rows and both diagonals tracked in O(1) arrays.
// Diagonal ids: x+y (constant along ↙↗), x-y+n-1 (constant along ↖↘). Undo after recursion!
struct NQueens {
    int n; ll count = 0;
    vector<char> row, d1, d2, blocked;             // blocked[x*n+y]: forbidden squares (CSES 1624)
    explicit NQueens(int n_) : n(n_), row(n_), d1(2 * n_), d2(2 * n_), blocked(n_ * n_, 0) {}
    void go(int y) {
        if (y == n) { count++; return; }
        for (int x = 0; x < n; x++) {
            if (blocked[x * n + y] || row[x] || d1[x + y] || d2[x - y + n - 1]) continue;
            row[x] = d1[x + y] = d2[x - y + n - 1] = 1;
            go(y + 1);
            row[x] = d1[x + y] = d2[x - y + n - 1] = 0;   // the "back" in backtracking
        }
    }
    ll solve() { count = 0; go(0); return count; }
};

// Sudoku: choose the empty cell with the FEWEST candidates (most-constrained-first ordering),
// try each, recurse. Bitmask candidate sets make the check O(1).
struct Sudoku {
    array<array<int, 9>, 9> g{};
    int rowm[9] = {}, colm[9] = {}, boxm[9] = {};   // bit d set ⇔ digit d used
    static int box(int r, int c) { return r / 3 * 3 + c / 3; }
    bool solve() {
        int br = -1, bc = -1, bestCnt = 10, bestCand = 0;
        for (int r = 0; r < 9; r++) for (int c = 0; c < 9; c++) {
            if (g[r][c]) continue;
            int cand = ~(rowm[r] | colm[c] | boxm[box(r, c)]) & 0x3FE;   // digits 1..9
            int cnt = __builtin_popcount(cand);
            if (cnt < bestCnt) { bestCnt = cnt; br = r; bc = c; bestCand = cand; }
            if (cnt == 0) return false;                // dead cell: prune immediately
        }
        if (br == -1) return true;                     // no empty cell → solved
        for (int cand = bestCand; cand; cand &= cand - 1) {
            int d = __builtin_ctz(cand);
            g[br][bc] = d; rowm[br] |= 1 << d; colm[bc] |= 1 << d; boxm[box(br, bc)] |= 1 << d;
            if (solve()) return true;
            g[br][bc] = 0; rowm[br] ^= 1 << d; colm[bc] ^= 1 << d; boxm[box(br, bc)] ^= 1 << d;
        }
        return false;
    }
    void load(const vector<string>& s) {
        for (int r = 0; r < 9; r++) for (int c = 0; c < 9; c++) {
            int d = s[r][c] == '.' ? 0 : s[r][c] - '0';
            g[r][c] = d;
            if (d) { rowm[r] |= 1 << d; colm[c] |= 1 << d; boxm[box(r, c)] |= 1 << d; }
        }
    }
    bool valid() const {                              // full grid, every row/col/box a permutation
        for (int i = 0; i < 9; i++) {
            int rm = 0, cm = 0, bm = 0;
            for (int j = 0; j < 9; j++) {
                rm |= 1 << g[i][j]; cm |= 1 << g[j][i];
                bm |= 1 << g[i / 3 * 3 + j / 3][i % 3 * 3 + j % 3];
            }
            if (rm != 0x3FE || cm != 0x3FE || bm != 0x3FE) return false;
        }
        return true;
    }
};

// Grid paths (CPH ch. 5 showcase, generalised to n×n): count Hamiltonian paths from (0,0) to the
// opposite corner (n-1,n-1). Pruning rules: (1) if we hit the target before visiting every cell,
// stop; (2) if we cannot continue straight but can turn both left and right, the path + walls
// form a closed barrier and the unvisited region is split in two — no Hamiltonian completion
// exists; (3) symmetry: reflecting in the main diagonal fixes both corners and swaps "first move
// down" with "first move right", so count one and double. (CSES 1625 ends at the lower-LEFT
// corner and gives a pattern string, so rule 3 does not apply there; rules 1–2 do.)
// `prune` toggles the rules to compare with brute force. Even n → 0 paths (checkerboard parity).
struct GridPaths {
    int n; bool prune; ll cnt = 0;
    vector<char> vis;
    static constexpr int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    GridPaths(int n_, bool p) : n(n_), prune(p), vis((n_ + 2) * (n_ + 2), 0) {
        // 1-cell wall around the board makes bounds checks disappear
        for (int i = 0; i < n + 2; i++) vis[i] = vis[(n + 1) * (n + 2) + i] = vis[i * (n + 2)] = vis[i * (n + 2) + n + 1] = 1;
    }
    int id(int x, int y) const { return (x + 1) * (n + 2) + (y + 1); }
    void go(int x, int y, int steps, int dir) {
        if (prune) {
            if (x == n - 1 && y == n - 1 && steps != n * n - 1) return;         // rule 1
            int fx = x + dx[dir], fy = y + dy[dir];
            int lx = x + dx[(dir + 1) & 3], ly = y + dy[(dir + 1) & 3];
            int rx = x + dx[(dir + 3) & 3], ry = y + dy[(dir + 3) & 3];
            if (vis[id(fx, fy)] && !vis[id(lx, ly)] && !vis[id(rx, ry)]) return;  // rule 2
        }
        if (steps == n * n - 1) { if (x == n - 1 && y == n - 1) cnt++; return; }
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (vis[id(nx, ny)]) continue;
            vis[id(nx, ny)] = 1;
            go(nx, ny, steps + 1, d);
            vis[id(nx, ny)] = 0;
        }
    }
    ll solve() {
        cnt = 0; vis[id(0, 0)] = 1;
        if (prune) {                                   // rule 3: only the first move "down" (x+1)
            vis[id(1, 0)] = 1; go(1, 0, 1, 0); vis[id(1, 0)] = 0;
            return cnt * 2;
        }
        go(0, 0, 0, 0);
        return cnt;
    }
};

// ═════════════════════════════ 4. Meet in the middle ═════════════════════════════════════
// Count subsets with sum exactly x, n ≤ 40. Split into halves A|B, enumerate 2^{n/2} sums each,
// sort B, for each a count b == x - a with equal_range. O(2^{n/2} · n). Every subset of the
// whole set splits uniquely into (A-part, B-part) → each counted exactly once.
ll count_subsets_with_sum_mitm(const vector<ll>& w, ll x) {
    int n = (int)w.size(), h = n / 2;
    vector<ll> A(w.begin(), w.begin() + h), B(w.begin() + h, w.end());
    vector<ll> sa = subset_sums(A), sb = subset_sums(B);
    sort(all(sb));
    ll ans = 0;
    for (ll a : sa) {
        auto r = equal_range(all(sb), x - a);
        ans += r.second - r.first;
    }
    return ans;
}
ll count_subsets_with_sum_brute(const vector<ll>& w, ll x) {
    vector<ll> s = subset_sums(w);
    return count(all(s), x);
}

// ═════════════════════════════ 5. IDDFS and bidirectional BFS ════════════════════════════
// Toy state space: integers, moves v → v+1, v-1, 2v, within [0, LIM]. All three algorithms must
// return the same shortest distance. IDDFS: DFS with depth cap 0,1,2,…; memory O(depth), time
// O(b^d) like BFS but without the queue — useful when states are huge and d is small.
const int LIM = 5000;
vector<int> moves(int v) {
    vector<int> r;
    if (v + 1 <= LIM) r.push_back(v + 1);
    if (v - 1 >= 0) r.push_back(v - 1);
    if (2 * v <= LIM) r.push_back(2 * v);
    return r;
}
vector<int> rmoves(int v) {                         // predecessors: u such that v ∈ moves(u)
    vector<int> r;
    if (v - 1 >= 0) r.push_back(v - 1);             // (v-1)+1 = v
    if (v + 1 <= LIM) r.push_back(v + 1);           // (v+1)-1 = v
    if (v % 2 == 0 && v / 2 != v) r.push_back(v / 2);   // 2·(v/2) = v  (v=0: 2·0=0 self-loop, skip)
    return r;
}
int bfs_dist(int s, int t) {
    vector<int> d(LIM + 1, -1);
    queue<int> q; q.push(s); d[s] = 0;
    while (!q.empty()) {
        int v = q.front(); q.pop();
        if (v == t) return d[v];
        for (int u : moves(v)) if (d[u] < 0) { d[u] = d[v] + 1; q.push(u); }
    }
    return -1;
}
bool dls(int v, int t, int depth) {                 // depth-limited search
    if (v == t) return true;
    if (depth == 0) return false;
    for (int u : moves(v)) if (dls(u, t, depth - 1)) return true;
    return false;
}
int iddfs_dist(int s, int t, int maxDepth = 30) {
    for (int d = 0; d <= maxDepth; d++) if (dls(s, t, d)) return d;
    return -1;
}
// Bidirectional BFS: expand the smaller frontier each round; meeting point gives the answer.
// Explores O(2·b^{d/2}) states instead of O(b^d). Needs the reverse move set.
int bidir_bfs_dist(int s, int t) {
    if (s == t) return 0;
    vector<int> ds(LIM + 1, -1), dt(LIM + 1, -1);
    vector<int> fs = {s}, ft = {t};
    ds[s] = 0; dt[t] = 0;
    while (!fs.empty() && !ft.empty()) {
        bool fromS = fs.size() <= ft.size();
        vector<int>& fr = fromS ? fs : ft;
        vector<int>& mine = fromS ? ds : dt;
        vector<int>& other = fromS ? dt : ds;
        vector<int> nxt;
        for (int v : fr) {
            for (int u : (fromS ? moves(v) : rmoves(v))) {
                if (mine[u] >= 0) continue;
                mine[u] = mine[v] + 1;
                if (other[u] >= 0) return mine[u] + other[u];
                nxt.push_back(u);
            }
        }
        fr.swap(nxt);
    }
    return -1;
}

// ═════════════════════════════ 6. Branch and bound ═══════════════════════════════════════
// 0/1 knapsack: DFS over items sorted by value density; bound = current value + fractional
// (greedy) relaxation of the rest. Prune a branch whose bound ≤ best found. Exact, exponential
// worst case, but typically explores a tiny fraction of the 2^n leaves.
struct KnapsackBB {
    int n; ll cap; vector<pair<ll, ll>> it;         // (weight, value), sorted by value/weight desc
    ll best = 0;
    KnapsackBB(vector<pair<ll, ll>> items, ll c) : n((int)items.size()), cap(c), it(std::move(items)) {
        sort(all(it), [](auto& a, auto& b) { return a.second * b.first > b.second * a.first; });
    }
    double bound(int k, ll w, ll v) const {         // optimistic: fill remaining capacity fractionally
        double b = (double)v; ll room = cap - w;
        for (int i = k; i < n && room > 0; i++) {
            ll take = min(room, it[i].first);
            b += (double)it[i].second * take / it[i].first;
            room -= take;
        }
        return b;
    }
    void go(int k, ll w, ll v) {
        best = max(best, v);
        if (k == n) return;
        if (bound(k, w, v) <= (double)best + 1e-9) return;        // cannot beat best: prune
        if (w + it[k].first <= cap) go(k + 1, w + it[k].first, v + it[k].second);  // take first (greedy order)
        go(k + 1, w, v);                                           // skip
    }
    ll solve() { best = 0; go(0, 0, 0); return best; }
};
ll knapsack_dp(const vector<pair<ll, ll>>& items, ll cap) {
    vector<ll> dp(cap + 1, 0);
    for (auto [w, v] : items) for (ll c = cap; c >= w; c--) dp[c] = max(dp[c], dp[c - w] + v);
    return dp[cap];
}

// ═════════════════════════════ 7. Ternary search, golden section, BS on reals ════════════
// Unimodal f on integers [lo, hi] (strictly increasing then strictly decreasing, or with a
// plateau ONLY at the maximum). Compare f(m1) < f(m2) → max is right of m1. ~2·log_{1.5} n calls.
template <class F> ll ternary_max_int(ll lo, ll hi, F f) {
    while (hi - lo > 2) {
        ll m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
        if (f(m1) < f(m2)) lo = m1 + 1; else hi = m2;   // for a plateau at the max, both are safe
    }
    ll bestx = lo;
    for (ll x = lo + 1; x <= hi; x++) if (f(x) > f(bestx)) bestx = x;
    return bestx;
}
// Real ternary search: 100 iterations shrink the interval by (2/3)^100 ≈ 2.5e-18 — fixed count,
// never `while (hi - lo > eps)` (can loop forever when eps is below double resolution).
template <class F> double ternary_max_real(double lo, double hi, F f, int iters = 100) {
    for (int i = 0; i < iters; i++) {
        double m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
        if (f(m1) < f(m2)) lo = m1; else hi = m2;
    }
    return (lo + hi) / 2;
}
// Golden-section search: same idea, reuses one evaluation per step (1 call/iter instead of 2),
// interval shrinks by 0.618 per call vs 0.816 per call for ternary. Use when f is expensive.
template <class F> double golden_max(double lo, double hi, F f, int iters = 100) {
    const double r = (sqrt(5.0) - 1) / 2;           // 0.618…
    double x1 = hi - r * (hi - lo), x2 = lo + r * (hi - lo);
    double f1 = f(x1), f2 = f(x2);
    for (int i = 0; i < iters; i++) {
        if (f1 < f2) { lo = x1; x1 = x2; f1 = f2; x2 = lo + r * (hi - lo); f2 = f(x2); }
        else         { hi = x2; x2 = x1; f2 = f1; x1 = hi - r * (hi - lo); f1 = f(x1); }
    }
    return (lo + hi) / 2;
}
// Binary search on reals for a monotone predicate: fixed 100 halvings → 2^-100 relative width.
template <class P> double bisect_real(double lo, double hi, P ok, int iters = 100) {
    for (int i = 0; i < iters; i++) {
        double mid = (lo + hi) / 2;
        if (ok(mid)) hi = mid; else lo = mid;
    }
    return hi;                                       // smallest x with ok(x), to ~1e-15 relative
}

// ═════════════════════════════ 8. Binary search on the answer ════════════════════════════
// Invariant: ok(x) is monotone (false…false true…true). Loop keeps ok(hi) == true (or hi is the
// sentinel), ok(lo-1) == false. Returns the smallest x in [lo, hi] with ok(x); hi if none.
template <class P> ll first_true(ll lo, ll hi, P ok) {
    while (lo < hi) {
        ll mid = lo + (hi - lo) / 2;                 // never (lo+hi)/2: overflow at 1e18
        if (ok(mid)) hi = mid; else lo = mid + 1;
    }
    return lo;
}
// Factory Machines (CSES 1620): k[i] = seconds per product on machine i; min time for t products.
// ok(T) = Σ floor(T / k_i) ≥ t. Monotone: more time → more products. Cap the sum to avoid overflow.
ll factory_min_time(const vector<ll>& k, ll t) {
    return first_true(1, (ll)1e18, [&](ll T) {
        ll made = 0;
        for (ll x : k) { made += T / x; if (made >= t) return true; }
        return false;
    });
}
ll factory_brute(const vector<ll>& k, ll t) {       // simulate second by second (tiny inputs)
    for (ll T = 1; ; T++) {
        ll made = 0;
        for (ll x : k) made += T / x;
        if (made >= t) return T;
    }
}
// Array Division (CSES 1085): split into k contiguous parts minimising the max part sum.
// ok(S) = greedy scan needs ≤ k parts with every part sum ≤ S. Monotone in S.
ll array_division(const vector<ll>& a, int k) {
    ll lo = *max_element(all(a)), hi = accumulate(all(a), 0LL);
    return first_true(lo, hi, [&](ll S) {
        int parts = 1; ll cur = 0;
        for (ll x : a) { if (cur + x > S) { parts++; cur = 0; } cur += x; }
        return parts <= k;
    });
}
ll array_division_brute(const vector<ll>& a, int k) {   // try every set of k-1 cut positions
    int n = (int)a.size(); ll best = LLONG_MAX;
    for (int m = 0; m < (1 << (n - 1)); m++) {
        if (__builtin_popcount(m) != k - 1) continue;
        ll cur = 0, mx = 0;
        for (int i = 0; i < n; i++) { cur += a[i]; if (i == n - 1 || (m >> i & 1)) { mx = max(mx, cur); cur = 0; } }
        best = min(best, mx);
    }
    return best;
}

// ═════════════════════════════ 9. Greedy ═════════════════════════════════════════════════
// Interval scheduling (Movie Festival, CSES 1629): max number of pairwise non-overlapping
// intervals. Sort by END, take whenever start ≥ last end.
// Exchange argument: the greedy first choice ends no later than the optimum's first choice, so
// swapping it in keeps every later interval of the optimum feasible; induct on the rest.
int max_nonoverlap(vector<pair<ll, ll>> iv) {       // (start, end); touching allowed (start ≥ end)
    sort(all(iv), [](auto& a, auto& b) { return a.second < b.second; });
    int cnt = 0; ll last = LLONG_MIN;
    for (auto [s, e] : iv) if (s >= last) { cnt++; last = e; }
    return cnt;
}
int max_nonoverlap_brute(const vector<pair<ll, ll>>& iv) {
    int n = (int)iv.size(), best = 0;
    for (int m = 0; m < (1 << n); m++) {
        bool ok = true;
        for (int i = 0; i < n && ok; i++) for (int j = i + 1; j < n && ok; j++)
            if ((m >> i & 1) && (m >> j & 1))
                if (iv[i].first < iv[j].second && iv[j].first < iv[i].second) ok = false;
        if (ok) best = max(best, __builtin_popcount(m));
    }
    return best;
}
// Tasks and Deadlines (CSES 1630): reward Σ(d_i − finish_i) = Σd_i − Σfinish_i; the first sum is
// constant, so minimise Σ finish times → shortest duration first (adjacent swap of a longer task
// before a shorter one decreases the sum by (long − short) > 0).
ll tasks_reward(vector<pair<ll, ll>> t) {           // (duration, deadline)
    sort(all(t));
    ll time = 0, r = 0;
    for (auto [d, dl] : t) { time += d; r += dl - time; }
    return r;
}
ll tasks_reward_brute(vector<pair<ll, ll>> t) {
    sort(all(t)); ll best = LLONG_MIN;
    do { ll time = 0, r = 0; for (auto [d, dl] : t) { time += d; r += dl - time; } best = max(best, r); }
    while (next_permutation(all(t)));
    return best;
}
// Coin change: greedy (largest coin that fits) is optimal for canonical systems (euro: each coin
// ≥ 2× previous with the right structure) and WRONG in general — {1,3,4}, x=6: greedy 4+1+1,
// optimum 3+3. DP is the safe fallback.
int coins_greedy(const vector<int>& c, int x) {     // c sorted ascending
    int cnt = 0;
    for (int i = (int)c.size() - 1; i >= 0; i--) { cnt += x / c[i]; x %= c[i]; }
    return x == 0 ? cnt : -1;
}
int coins_dp(const vector<int>& c, int x) {
    vector<int> dp(x + 1, INT_MAX / 2); dp[0] = 0;
    for (int s = 1; s <= x; s++) for (int v : c) if (v <= s) dp[s] = min(dp[s], dp[s - v] + 1);
    return dp[x] >= INT_MAX / 2 ? -1 : dp[x];
}
// Huffman: merge the two smallest weights repeatedly; total cost = Σ merged weights = Σ w_i·depth_i.
// Greedy is optimal: the two least frequent symbols are siblings at max depth in some optimal tree.
ll huffman_cost(vector<ll> w) {
    priority_queue<ll, vector<ll>, greater<ll>> pq(all(w));
    ll cost = 0;
    while (pq.size() > 1) {
        ll a = pq.top(); pq.pop(); ll b = pq.top(); pq.pop();
        cost += a + b; pq.push(a + b);
    }
    return cost;
}

// ═════════════════════════════════════ tests ═════════════════════════════════════════════
int main() {
    // ── 1. subsets
    {
        vector<vector<int>> rec; vector<int> cur;
        subsets_rec(0, 4, cur, rec);
        auto msk = subsets_mask(4);
        assert(rec.size() == 16 && msk.size() == 16);
        sort(all(rec)); sort(all(msk));
        assert(rec == msk);
        vector<ll> w = {3, 5, 11, 2};
        auto s = subset_sums(w);
        for (int m = 0; m < 16; m++) {
            ll t = 0; for (int i = 0; i < 4; i++) if (m >> i & 1) t += w[i];
            assert(s[m] == t);
        }
        for (int i = 0; i + 1 < (1 << 5); i++) assert(__builtin_popcount(gray(i) ^ gray(i + 1)) == 1);
        set<int> seen; for (int i = 0; i < 32; i++) seen.insert(gray(i));
        assert(seen.size() == 32);                     // Gray code is a permutation of 0..2^n-1
        auto sm = submasks(0b1011);
        assert((sm == vector<int>{11, 10, 9, 8, 3, 2, 1, 0}));
        int total = 0; for (int m = 0; m < (1 << 6); m++) total += (int)submasks(m).size();
        assert(total == 729);                          // 3^6
    }

    // ── 2. permutations
    {
        vector<vector<int>> pr; vector<int> cur; vector<bool> used(5, false);
        perms_rec(5, cur, used, pr);
        auto pn = perms_next(vector<int>{0, 1, 2, 3, 4});
        assert(pr.size() == 120 && pn.size() == 120);
        sort(all(pr)); assert(pr == pn);               // next_permutation is already lexicographic
        auto dup = perms_next(vector<char>{'a', 'a', 'b', 'c'});
        assert(dup.size() == 12);                      // 4!/2! distinct strings
        assert(is_sorted(all(dup)));
    }

    // ── 3. backtracking
    {
        assert(NQueens(4).solve() == 2 && NQueens(6).solve() == 4 && NQueens(8).solve() == 92);
        NQueens q(8); q.blocked[0 * 8 + 0] = 1;        // block a1: fewer solutions
        assert(q.solve() < 92 && q.solve() > 0);

        Sudoku sd;
        sd.load({"53..7....", "6..195...", ".98....6.", "8...6...3", "4..8.3..1",
                 "7...2...6", ".6....28.", "...419..5", "....8..79"});
        assert(sd.solve() && sd.valid());
        assert(sd.g[0][2] == 4 && sd.g[8][8] == 9);    // known solution cells

        for (int n = 3; n <= 5; n++) {
            ll pruned = GridPaths(n, true).solve(), brute = GridPaths(n, false).solve();
            assert(pruned == brute);
        }
        assert(GridPaths(3, false).solve() == 2);      // hand-checkable: 3x3 corner to corner
        assert(GridPaths(4, true).solve() == 0);       // parity: even n has no such path
    }

    // ── 4. meet in the middle vs brute
    {
        for (int it = 0; it < 200; it++) {
            int n = (int)rnd(1, 14);
            vector<ll> w(n); for (auto& v : w) v = rnd(0, 12);
            ll x = rnd(0, 40);
            assert(count_subsets_with_sum_mitm(w, x) == count_subsets_with_sum_brute(w, x));
        }
        vector<ll> big(40); for (auto& v : big) v = rnd(1, (ll)1e9);
        ll target = big[0] + big[7] + big[39];
        assert(count_subsets_with_sum_mitm(big, target) >= 1);   // 2^40 brute is impossible; MitM ~1e6·40
    }

    // ── 5. IDDFS / BFS / bidirectional BFS agree
    {
        for (int it = 0; it < 30; it++) {
            int s = (int)rnd(0, 200), t = (int)rnd(0, 400);
            int d = bfs_dist(s, t);
            assert(d == bidir_bfs_dist(s, t));
            if (d <= 12) assert(d == iddfs_dist(s, t));   // IDDFS is exponential; keep it shallow
        }
        assert(bfs_dist(1, 1024) == 10 && bidir_bfs_dist(1, 1024) == 10 && iddfs_dist(1, 1024) == 10);
    }

    // ── 6. branch and bound vs DP
    {
        for (int it = 0; it < 100; it++) {
            int n = (int)rnd(1, 15);
            vector<pair<ll, ll>> items(n);
            for (auto& [w, v] : items) { w = rnd(1, 20); v = rnd(1, 50); }
            ll cap = rnd(1, 60);
            assert(KnapsackBB(items, cap).solve() == knapsack_dp(items, cap));
        }
    }

    // ── 7. ternary / golden / bisection
    {
        auto f = [](ll x) { return -(x - 137) * (x - 137); };        // max at 137
        assert(ternary_max_int(-1000, 1000, f) == 137);
        auto g = [](ll x) { return min(x, 50LL); };                   // plateau at the max → any x ≥ 50 ok
        assert(g(ternary_max_int(0, 100, g)) == 50);
        auto h = [](double x) { return -(x - M_PI) * (x - M_PI) + 1; };
        // Pitfall: near a smooth maximum f is flat, so f(m1) vs f(m2) is decided by rounding once
        // |m1-m2| < ~sqrt(machine eps) ≈ 1e-8. The ARGMAX is accurate to ~1e-7, the MAX VALUE
        // to ~1e-15. Problems asking for the position need this in mind (or exact arithmetic).
        double tx = ternary_max_real(0, 10, h), gx = golden_max(0, 10, h);
        assert(fabs(tx - M_PI) < 1e-6 && fabs(gx - M_PI) < 1e-6);
        assert(fabs(h(tx) - 1) < 1e-12 && fabs(h(gx) - 1) < 1e-12);
        double r2 = bisect_real(0, 2, [](double x) { return x * x >= 2; });
        assert(fabs(r2 - sqrt(2.0)) < 1e-12);
    }

    // ── 8. binary search on the answer vs brute
    {
        assert(first_true(0, 100, [](ll x) { return x >= 37; }) == 37);
        assert(first_true(0, 100, [](ll) { return false; }) == 100);      // sentinel returned
        assert(first_true(0, 100, [](ll) { return true; }) == 0);
        for (int it = 0; it < 200; it++) {
            int n = (int)rnd(1, 5); vector<ll> k(n); for (auto& v : k) v = rnd(1, 6);
            ll t = rnd(1, 30);
            assert(factory_min_time(k, t) == factory_brute(k, t));
        }
        vector<ll> huge(200000, 1e9);
        assert(factory_min_time(huge, (ll)1e9) == 5000000000000LL);  // Σ T/1e9 ≥ 1e9 → T = 5e12
        for (int it = 0; it < 200; it++) {
            int n = (int)rnd(1, 8), kk = (int)rnd(1, n);
            vector<ll> a(n); for (auto& v : a) v = rnd(1, 20);
            assert(array_division(a, kk) == array_division_brute(a, kk));
        }
    }

    // ── 9. greedy vs brute, and where greedy fails
    {
        for (int it = 0; it < 300; it++) {
            int n = (int)rnd(1, 9); vector<pair<ll, ll>> iv(n);
            for (auto& [s, e] : iv) { s = rnd(0, 15); e = s + rnd(1, 6); }
            assert(max_nonoverlap(iv) == max_nonoverlap_brute(iv));
        }
        for (int it = 0; it < 200; it++) {
            int n = (int)rnd(1, 6); vector<pair<ll, ll>> t(n);
            for (auto& [d, dl] : t) { d = rnd(1, 10); dl = rnd(1, 30); }
            assert(tasks_reward(t) == tasks_reward_brute(t));
        }
        vector<int> euro = {1, 2, 5, 10, 20, 50, 100, 200};
        for (int x = 0; x <= 500; x++) assert(coins_greedy(euro, x) == coins_dp(euro, x));
        vector<int> bad = {1, 3, 4};
        assert(coins_greedy(bad, 6) == 3 && coins_dp(bad, 6) == 2);   // greedy fails
        assert(huffman_cost({5, 9, 12, 13, 16, 45}) == 224);           // textbook example
        assert(huffman_cost({1, 1, 1, 1}) == 8);                        // balanced tree, depth 2 each
    }

    cout << "all chapter 02 checks passed\n";
    return 0;
}
