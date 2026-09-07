// Chapter 07 — Range Queries: the reference snippet library.
//
// TRAINING RULE: do not copy this file into a contest. Re-type every structure
// here from memory until you can write it in under 3 minutes with zero bugs.
// Every structure is asserted against a brute force on random data in main().
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Contents
//   1. Prefix sums 1D / 2D, difference array
//   2. Sparse table (idempotent ops, O(1) query) with log table
//   3. Fenwick / BIT: point add + prefix sum, k-th element (binary lifting),
//      range add + point query, range add + range sum (two BITs), 2D BIT
//   4. Iterative (bottom-up) segment tree with arbitrary associative combine,
//      including a non-commutative "best subarray sum" node
//   5. Recursive segment tree with lazy propagation: range assign + range add,
//      range sum; plus the "walk" (first index with prefix sum >= k, first
//      index >= x in a max tree)
//   6. Merge sort tree ("count elements < x in [l, r]")
//   7. Persistent segment tree (k-th smallest in a subarray)
//   8. Dynamic / sparse segment tree (coordinates up to 1e9)
//   9. Sqrt decomposition (blocks with lazy add) and Mo's algorithm (distinct values)
// On CSES/Codeforces (GCC) you would write `#include <bits/stdc++.h>`; Apple
// clang has no such header, so the standard headers are listed explicitly.
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <vector>
using namespace std;
using ll = long long;

static mt19937_64 rng(20260905);
static ll rnd(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rng); }

// ============================================================================
// 1. Prefix sums
// ============================================================================
// Invariant: p[i] = a[0] + ... + a[i-1]; p[0] = 0. sum(l..r) = p[r+1] - p[l].
struct Prefix1D {
    vector<ll> p;
    explicit Prefix1D(const vector<ll>& a) : p(a.size() + 1, 0) {
        for (size_t i = 0; i < a.size(); i++) p[i + 1] = p[i] + a[i];
    }
    ll sum(int l, int r) const { return p[r + 1] - p[l]; }  // inclusive [l, r]
};

// Invariant: p[i][j] = sum of a[x][y] for x < i, y < j (inclusion–exclusion).
struct Prefix2D {
    int n, m;
    vector<vector<ll>> p;
    explicit Prefix2D(const vector<vector<ll>>& a)
        : n(a.size()), m(a[0].size()), p(n + 1, vector<ll>(m + 1, 0)) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < m; j++)
                p[i + 1][j + 1] = a[i][j] + p[i][j + 1] + p[i + 1][j] - p[i][j];
    }
    // sum over rows r1..r2, cols c1..c2 (inclusive)
    ll sum(int r1, int c1, int r2, int c2) const {
        return p[r2 + 1][c2 + 1] - p[r1][c2 + 1] - p[r2 + 1][c1] + p[r1][c1];
    }
};

// Difference array: k offline range adds in O(n + k).
// d[l] += v, d[r+1] -= v; final a = prefix sums of d.
vector<ll> apply_range_adds(int n, const vector<array<ll, 3>>& ops) {  // {l, r, v}
    vector<ll> d(n + 1, 0);
    for (auto& [l, r, v] : ops) { d[l] += v; d[r + 1] -= v; }
    vector<ll> a(n);
    ll run = 0;
    for (int i = 0; i < n; i++) { run += d[i]; a[i] = run; }
    return a;
}

// ============================================================================
// 2. Sparse table — for idempotent, associative ops (min, max, gcd, and, or).
// ============================================================================
// t[k][i] = op(a[i .. i + 2^k - 1]). Query [l, r]: k = floor(log2(r-l+1)),
// answer = op(t[k][l], t[k][r - 2^k + 1]) — the two blocks overlap, which is
// harmless only because op is idempotent (min(x,x) = x).
struct SparseTable {
    int n, K;
    vector<vector<ll>> t;
    vector<int> lg;  // lg[i] = floor(log2 i); avoids a __lg call per query
    explicit SparseTable(const vector<ll>& a) : n(a.size()), lg(n + 1, 0) {
        for (int i = 2; i <= n; i++) lg[i] = lg[i / 2] + 1;
        K = lg[n] + 1;
        t.assign(K, vector<ll>(n));
        t[0] = a;
        for (int k = 1; k < K; k++)
            for (int i = 0; i + (1 << k) <= n; i++)
                t[k][i] = min(t[k - 1][i], t[k - 1][i + (1 << (k - 1))]);
    }
    ll query(int l, int r) const {  // inclusive, O(1)
        int k = lg[r - l + 1];
        return min(t[k][l], t[k][r - (1 << k) + 1]);
    }
};

// ============================================================================
// 3. Fenwick tree (binary indexed tree)
// ============================================================================
// Internally 1-indexed: t[i] stores the sum of a[i - lowbit(i) + 1 .. i].
// Public API is 0-indexed. Never call the internal loops with index 0:
// 0 & -0 == 0 would loop forever.
struct Fenwick {
    int n;
    vector<ll> t;
    explicit Fenwick(int n) : n(n), t(n + 1, 0) {}
    void add(int i, ll v) {                 // a[i] += v
        for (i++; i <= n; i += i & -i) t[i] += v;
    }
    ll prefix(int i) const {                // a[0] + ... + a[i]; prefix(-1) == 0
        ll s = 0;
        for (i++; i > 0; i -= i & -i) s += t[i];
        return s;
    }
    ll sum(int l, int r) const { return prefix(r) - prefix(l - 1); }
    // Smallest 0-indexed idx with prefix(idx) >= k, assuming all a[i] >= 0
    // and 1 <= k <= total. Binary lifting on the implicit tree: O(log n).
    int kth(ll k) const {
        int pos = 0, LOG = 1;
        while ((1 << LOG) <= n) LOG++;
        for (int pw = 1 << LOG; pw; pw >>= 1)
            if (pos + pw <= n && t[pos + pw] < k) { pos += pw; k -= t[pos]; }
        return pos;  // pos is the last 1-indexed position with prefix < k -> 0-indexed answer
    }
};

// Range add, point query: store the difference array in a Fenwick.
struct RangeAddPointQuery {
    Fenwick f;
    explicit RangeAddPointQuery(int n) : f(n + 1) {}
    void range_add(int l, int r, ll v) { f.add(l, v); f.add(r + 1, -v); }
    ll point(int i) const { return f.prefix(i); }
};

// Range add, range sum with two Fenwicks (0-indexed derivation):
// after range_add(l, r, v), the contribution to prefix(i) is
//   0                for i <  l
//   v * (i - l + 1)  for l <= i <= r
//   v * (r - l + 1)  for i >  r
// which equals (i + 1) * B1.prefix(i) - B2.prefix(i) with
//   B1: +v at l, -v at r+1        B2: +v*l at l, -v*(r+1) at r+1.
struct RangeAddRangeSum {
    Fenwick b1, b2;
    explicit RangeAddRangeSum(int n) : b1(n + 1), b2(n + 1) {}
    void range_add(int l, int r, ll v) {
        b1.add(l, v);      b1.add(r + 1, -v);
        b2.add(l, v * l);  b2.add(r + 1, -v * (r + 1));
    }
    ll prefix(int i) const { return (i + 1) * b1.prefix(i) - b2.prefix(i); }
    ll sum(int l, int r) const { return prefix(r) - (l ? prefix(l - 1) : 0); }
};

// 2D Fenwick: point add, prefix-rectangle sum. O(log n log m) per op.
struct Fenwick2D {
    int n, m;
    vector<vector<ll>> t;
    Fenwick2D(int n, int m) : n(n), m(m), t(n + 1, vector<ll>(m + 1, 0)) {}
    void add(int x, int y, ll v) {
        for (int i = x + 1; i <= n; i += i & -i)
            for (int j = y + 1; j <= m; j += j & -j) t[i][j] += v;
    }
    ll prefix(int x, int y) const {  // sum over [0..x] x [0..y]
        ll s = 0;
        for (int i = x + 1; i > 0; i -= i & -i)
            for (int j = y + 1; j > 0; j -= j & -j) s += t[i][j];
        return s;
    }
    ll sum(int x1, int y1, int x2, int y2) const {
        return prefix(x2, y2) - prefix(x1 - 1, y2) - prefix(x2, y1 - 1) + prefix(x1 - 1, y1 - 1);
    }
};

// ============================================================================
// 4. Iterative bottom-up segment tree (Al.Cash style). Works for any n, any
//    associative combine, even non-commutative — keep separate left/right
//    accumulators and fold them in the correct order.
// ============================================================================
template <class T, class F>
struct SegTree {
    int n;
    vector<T> t;
    T id;
    F f;
    SegTree(int n, T id, F f) : n(n), t(2 * n, id), id(id), f(f) {}
    void build(const vector<T>& a) {
        for (int i = 0; i < n; i++) t[n + i] = a[i];
        for (int i = n - 1; i >= 1; i--) t[i] = f(t[2 * i], t[2 * i + 1]);
    }
    void set(int p, T v) {
        for (t[p += n] = v; p > 1; p >>= 1) t[p >> 1] = f(t[p & ~1], t[p | 1]);
    }
    T query(int l, int r) const {  // half-open [l, r)
        T resl = id, resr = id;
        for (l += n, r += n; l < r; l >>= 1, r >>= 1) {
            if (l & 1) resl = f(resl, t[l++]);
            if (r & 1) resr = f(t[--r], resr);
        }
        return f(resl, resr);
    }
};

// Node for "maximum subarray sum in [l, r]" (CSES Subarray Sum Queries).
// Combine is associative but NOT commutative (pref/suf depend on order).
struct BestNode {
    ll sum, pref, suf, best;  // best = max subarray sum (empty allowed -> >= 0)
};
static BestNode best_leaf(ll v) { return {v, max(0LL, v), max(0LL, v), max(0LL, v)}; }
static const BestNode BEST_ID{0, 0, 0, 0};
static BestNode best_combine(const BestNode& a, const BestNode& b) {
    return {a.sum + b.sum,
            max(a.pref, a.sum + b.pref),
            max(b.suf, b.sum + a.suf),
            max({a.best, b.best, a.suf + b.pref})};
}

// ============================================================================
// 5. Recursive segment tree with lazy propagation.
//    Operations: range assign x, range add d, range sum.
//    Lazy = (has_assign, assign_value, add). Meaning: "first assign (if set),
//    then add". Composition when a new op arrives on top of a pending lazy:
//      new add d   : add += d                       (assign stays)
//      new assign x: has_assign = 1, val = x, add = 0  (kills everything below)
//    Push-down ORDER: apply the parent's lazy to both children BEFORE
//    descending, and before reading child values. Then clear the parent lazy.
// ============================================================================
struct LazySeg {
    int n;
    vector<ll> sum, add, asg;
    vector<char> has;
    explicit LazySeg(int n) : n(n), sum(4 * n, 0), add(4 * n, 0), asg(4 * n, 0), has(4 * n, 0) {}
    void build(const vector<ll>& a) { build(1, 0, n - 1, a); }
    void build(int v, int lo, int hi, const vector<ll>& a) {
        if (lo == hi) { sum[v] = a[lo]; return; }
        int mid = (lo + hi) / 2;
        build(2 * v, lo, mid, a); build(2 * v + 1, mid + 1, hi, a);
        sum[v] = sum[2 * v] + sum[2 * v + 1];
    }
    // apply "assign x" to node v covering len elements
    void apply_assign(int v, int len, ll x) { has[v] = 1; asg[v] = x; add[v] = 0; sum[v] = x * len; }
    void apply_add(int v, int len, ll d) { add[v] += d; sum[v] += d * len; }
    void push(int v, int lo, int hi) {
        if (!has[v] && add[v] == 0) return;
        int mid = (lo + hi) / 2;
        for (int c : {2 * v, 2 * v + 1}) {
            int len = (c == 2 * v) ? mid - lo + 1 : hi - mid;
            if (has[v]) apply_assign(c, len, asg[v]);  // assign first...
            if (add[v]) apply_add(c, len, add[v]);     // ...then add
        }
        has[v] = 0; add[v] = 0;
    }
    void range_assign(int l, int r, ll x) { update(1, 0, n - 1, l, r, x, true); }
    void range_add(int l, int r, ll d) { update(1, 0, n - 1, l, r, d, false); }
    void update(int v, int lo, int hi, int l, int r, ll x, bool assign) {
        if (r < lo || hi < l) return;
        if (l <= lo && hi <= r) {
            if (assign) apply_assign(v, hi - lo + 1, x); else apply_add(v, hi - lo + 1, x);
            return;
        }
        push(v, lo, hi);
        int mid = (lo + hi) / 2;
        update(2 * v, lo, mid, l, r, x, assign);
        update(2 * v + 1, mid + 1, hi, l, r, x, assign);
        sum[v] = sum[2 * v] + sum[2 * v + 1];
    }
    ll query(int l, int r) { return query(1, 0, n - 1, l, r); }
    ll query(int v, int lo, int hi, int l, int r) {
        if (r < lo || hi < l) return 0;
        if (l <= lo && hi <= r) return sum[v];
        push(v, lo, hi);
        int mid = (lo + hi) / 2;
        return query(2 * v, lo, mid, l, r) + query(2 * v + 1, mid + 1, hi, l, r);
    }
    // WALK: smallest index i with a[0] + ... + a[i] >= k (all a >= 0, k <= total).
    int first_prefix_geq(ll k) { return walk(1, 0, n - 1, k); }
    int walk(int v, int lo, int hi, ll k) {
        if (lo == hi) return lo;
        push(v, lo, hi);
        int mid = (lo + hi) / 2;
        if (sum[2 * v] >= k) return walk(2 * v, lo, mid, k);
        return walk(2 * v + 1, mid + 1, hi, k - sum[2 * v]);
    }
};

// Max tree with the "first index >= x in [l, n)" walk (Hotel Queries pattern).
struct MaxSeg {
    int n;
    vector<ll> t;
    explicit MaxSeg(const vector<ll>& a) : n(a.size()), t(4 * n, LLONG_MIN) { build(1, 0, n - 1, a); }
    void build(int v, int lo, int hi, const vector<ll>& a) {
        if (lo == hi) { t[v] = a[lo]; return; }
        int mid = (lo + hi) / 2;
        build(2 * v, lo, mid, a); build(2 * v + 1, mid + 1, hi, a);
        t[v] = max(t[2 * v], t[2 * v + 1]);
    }
    void set(int p, ll x) { set(1, 0, n - 1, p, x); }
    void set(int v, int lo, int hi, int p, ll x) {
        if (lo == hi) { t[v] = x; return; }
        int mid = (lo + hi) / 2;
        if (p <= mid) set(2 * v, lo, mid, p, x); else set(2 * v + 1, mid + 1, hi, p, x);
        t[v] = max(t[2 * v], t[2 * v + 1]);
    }
    // leftmost i >= l with a[i] >= x, or -1. O(log n) amortised over the walk:
    // at most one "failed" left branch per level.
    int first_geq(int l, ll x) { return first(1, 0, n - 1, l, x); }
    int first(int v, int lo, int hi, int l, ll x) {
        if (hi < l || t[v] < x) return -1;
        if (lo == hi) return lo;
        int mid = (lo + hi) / 2;
        int res = first(2 * v, lo, mid, l, x);
        if (res != -1) return res;
        return first(2 * v + 1, mid + 1, hi, l, x);
    }
};

// ============================================================================
// 6. Merge sort tree: node stores the sorted multiset of its segment.
//    count_less(l, r, x) in O(log^2 n); memory O(n log n). No updates.
// ============================================================================
struct MergeSortTree {
    int n;
    vector<vector<ll>> t;
    explicit MergeSortTree(const vector<ll>& a) : n(a.size()), t(2 * n) {
        for (int i = 0; i < n; i++) t[n + i] = {a[i]};
        for (int i = n - 1; i >= 1; i--) {
            t[i].resize(t[2 * i].size() + t[2 * i + 1].size());
            merge(t[2 * i].begin(), t[2 * i].end(), t[2 * i + 1].begin(), t[2 * i + 1].end(), t[i].begin());
        }
    }
    int count_less(int l, int r, ll x) const {  // # of a[i] < x for i in [l, r)
        int res = 0;
        for (l += n, r += n; l < r; l >>= 1, r >>= 1) {
            if (l & 1) { res += lower_bound(t[l].begin(), t[l].end(), x) - t[l].begin(); l++; }
            if (r & 1) { --r; res += lower_bound(t[r].begin(), t[r].end(), x) - t[r].begin(); }
        }
        return res;
    }
};

// ============================================================================
// 7. Persistent segment tree over VALUE domain [0, m): version i = multiset of
//    a[0..i-1]. k-th smallest in a[l..r] = descend on cnt(version r+1) - cnt(version l).
//    Each insert creates O(log m) new nodes; total memory O((n + q) log m).
// ============================================================================
struct PersistentSeg {
    struct Node { int l, r, cnt; };
    vector<Node> t;
    int m;
    explicit PersistentSeg(int m) : m(m) { t.push_back({0, 0, 0}); }  // node 0 = empty tree
    int insert(int prev, int pos) { return insert(prev, 0, m, pos); }
    int insert(int prev, int lo, int hi, int pos) {
        int cur = t.size();
        t.push_back(t[prev]);
        t[cur].cnt++;
        if (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            if (pos < mid) { int c = insert(t[cur].l, lo, mid, pos); t[cur].l = c; }
            else           { int c = insert(t[cur].r, mid, hi, pos); t[cur].r = c; }
        }
        return cur;
    }
    // k-th smallest (1-indexed k) among elements present in version b but not a.
    int kth(int ra, int rb, int k) const {
        int lo = 0, hi = m;
        while (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            int left = t[t[rb].l].cnt - t[t[ra].l].cnt;
            if (k <= left) { ra = t[ra].l; rb = t[rb].l; hi = mid; }
            else { k -= left; ra = t[ra].r; rb = t[rb].r; lo = mid; }
        }
        return lo;
    }
};

// ============================================================================
// 8. Dynamic (sparse) segment tree over [0, N) with N up to ~1e18: nodes are
//    created on demand; O(log N) nodes per update. Point add, range sum.
// ============================================================================
struct DynSeg {
    struct Node { int l = 0, r = 0; ll sum = 0; };
    vector<Node> t;
    ll N;
    explicit DynSeg(ll N) : N(N) { t.emplace_back(); t.emplace_back(); }  // 0 = null, 1 = root
    void add(ll pos, ll val) { add(1, 0, N, pos, val); }
    void add(int v, ll lo, ll hi, ll pos, ll val) {
        t[v].sum += val;
        if (hi - lo == 1) return;
        ll mid = lo + (hi - lo) / 2;
        if (pos < mid) {
            if (!t[v].l) { int c = t.size(); t.emplace_back(); t[v].l = c; }
            add(t[v].l, lo, mid, pos, val);
        } else {
            if (!t[v].r) { int c = t.size(); t.emplace_back(); t[v].r = c; }
            add(t[v].r, mid, hi, pos, val);
        }
    }
    ll sum(ll l, ll r) const { return sum(1, 0, N, l, r + 1); }  // inclusive [l, r]
    ll sum(int v, ll lo, ll hi, ll l, ll r) const {              // half-open [l, r)
        if (!v || r <= lo || hi <= l) return 0;
        if (l <= lo && hi <= r) return t[v].sum;
        ll mid = lo + (hi - lo) / 2;
        return sum(t[v].l, lo, mid, l, r) + sum(t[v].r, mid, hi, l, r);
    }
};

// ============================================================================
// 9. Sqrt decomposition: blocks of size B with a per-block lazy add.
//    range add / range sum in O(sqrt n) each.
// ============================================================================
struct SqrtDecomp {
    int n, B;
    vector<ll> a, bsum, blazy;
    explicit SqrtDecomp(const vector<ll>& v) : n(v.size()), B(max(1, (int)sqrt(n))), a(v) {
        int nb = (n + B - 1) / B;
        bsum.assign(nb, 0); blazy.assign(nb, 0);
        for (int i = 0; i < n; i++) bsum[i / B] += a[i];
    }
    void range_add(int l, int r, ll d) {
        for (int i = l; i <= r;) {
            if (i % B == 0 && i + B - 1 <= r) { blazy[i / B] += d; bsum[i / B] += d * B; i += B; }
            else { a[i] += d; bsum[i / B] += d; i++; }
        }
    }
    ll sum(int l, int r) const {
        ll s = 0;
        for (int i = l; i <= r;) {
            if (i % B == 0 && i + B - 1 <= r) { s += bsum[i / B]; i += B; }
            else { s += a[i] + blazy[i / B]; i++; }
        }
        return s;
    }
};

// Mo's algorithm: offline, answers q range queries with O((n + q) sqrt n)
// add/remove calls. Here: number of distinct values in [l, r].
// Sort key: (block of l, r), with r reversed in odd blocks (halves pointer travel).
vector<int> mo_distinct(const vector<int>& a, const vector<pair<int, int>>& qs) {
    int n = a.size(), q = qs.size();
    int B = max(1, (int)(n / sqrt((double)q + 1)));
    vector<int> ord(q);
    iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int x, int y) {
        int bx = qs[x].first / B, by = qs[y].first / B;
        if (bx != by) return bx < by;
        return (bx & 1) ? qs[x].second > qs[y].second : qs[x].second < qs[y].second;
    });
    int maxv = *max_element(a.begin(), a.end());
    vector<int> cnt(maxv + 1, 0), ans(q);
    int curL = 0, curR = -1, distinct = 0;
    auto add = [&](int i) { if (cnt[a[i]]++ == 0) distinct++; };
    auto rem = [&](int i) { if (--cnt[a[i]] == 0) distinct--; };
    for (int id : ord) {
        auto [l, r] = qs[id];
        while (curL > l) add(--curL);
        while (curR < r) add(++curR);
        while (curL < l) rem(curL++);
        while (curR > r) rem(curR--);
        ans[id] = distinct;
    }
    return ans;
}

// ============================================================================
// Tests
// ============================================================================
static void test_prefix() {
    int n = 40;
    vector<ll> a(n);
    for (auto& x : a) x = rnd(-50, 50);
    Prefix1D p(a);
    for (int it = 0; it < 300; it++) {
        int l = rnd(0, n - 1), r = rnd(l, n - 1);
        ll s = 0;
        for (int i = l; i <= r; i++) s += a[i];
        assert(p.sum(l, r) == s);
    }
    int R = 7, C = 9;
    vector<vector<ll>> g(R, vector<ll>(C));
    for (auto& row : g) for (auto& x : row) x = rnd(-9, 9);
    Prefix2D p2(g);
    for (int it = 0; it < 300; it++) {
        int r1 = rnd(0, R - 1), r2 = rnd(r1, R - 1), c1 = rnd(0, C - 1), c2 = rnd(c1, C - 1);
        ll s = 0;
        for (int i = r1; i <= r2; i++) for (int j = c1; j <= c2; j++) s += g[i][j];
        assert(p2.sum(r1, c1, r2, c2) == s);
    }
    vector<array<ll, 3>> ops;
    vector<ll> brute(n, 0);
    for (int k = 0; k < 30; k++) {
        ll l = rnd(0, n - 1), r = rnd(l, n - 1), v = rnd(-5, 5);
        ops.push_back({l, r, v});
        for (int i = l; i <= r; i++) brute[i] += v;
    }
    assert(apply_range_adds(n, ops) == brute);
}

static void test_sparse() {
    int n = 57;
    vector<ll> a(n);
    for (auto& x : a) x = rnd(-100, 100);
    SparseTable st(a);
    for (int it = 0; it < 500; it++) {
        int l = rnd(0, n - 1), r = rnd(l, n - 1);
        assert(st.query(l, r) == *min_element(a.begin() + l, a.begin() + r + 1));
    }
}

static void test_fenwick() {
    int n = 33;
    vector<ll> a(n, 0);
    Fenwick f(n);
    for (int it = 0; it < 500; it++) {
        int i = rnd(0, n - 1); ll v = rnd(0, 5);  // non-negative so kth is defined
        a[i] += v; f.add(i, v);
        int l = rnd(0, n - 1), r = rnd(l, n - 1);
        ll s = 0;
        for (int j = l; j <= r; j++) s += a[j];
        assert(f.sum(l, r) == s);
        ll total = accumulate(a.begin(), a.end(), 0LL);
        if (total > 0) {
            ll k = rnd(1, total);
            int expect = 0; ll run = 0;
            while (run + a[expect] < k) run += a[expect++];
            assert(f.kth(k) == expect);
        }
    }
    RangeAddPointQuery rp(n);
    RangeAddRangeSum rr(n);
    vector<ll> b(n, 0);
    for (int it = 0; it < 500; it++) {
        int l = rnd(0, n - 1), r = rnd(l, n - 1); ll v = rnd(-7, 7);
        rp.range_add(l, r, v); rr.range_add(l, r, v);
        for (int j = l; j <= r; j++) b[j] += v;
        int i = rnd(0, n - 1);
        assert(rp.point(i) == b[i]);
        int ql = rnd(0, n - 1), qr = rnd(ql, n - 1);
        ll s = 0;
        for (int j = ql; j <= qr; j++) s += b[j];
        assert(rr.sum(ql, qr) == s);
    }
    int R = 6, C = 11;
    vector<vector<ll>> g(R, vector<ll>(C, 0));
    Fenwick2D f2(R, C);
    for (int it = 0; it < 500; it++) {
        int x = rnd(0, R - 1), y = rnd(0, C - 1); ll v = rnd(-9, 9);
        g[x][y] += v; f2.add(x, y, v);
        int r1 = rnd(0, R - 1), r2 = rnd(r1, R - 1), c1 = rnd(0, C - 1), c2 = rnd(c1, C - 1);
        ll s = 0;
        for (int i = r1; i <= r2; i++) for (int j = c1; j <= c2; j++) s += g[i][j];
        assert(f2.sum(r1, c1, r2, c2) == s);
    }
}

static void test_iterative_segtree() {
    int n = 37;  // deliberately not a power of two
    vector<ll> a(n);
    for (auto& x : a) x = rnd(-20, 20);
    auto mn = [](ll x, ll y) { return min(x, y); };
    SegTree<ll, decltype(mn)> st(n, LLONG_MAX, mn);
    st.build(a);
    auto gc = [](ll x, ll y) { return std::gcd(x, y); };
    SegTree<ll, decltype(gc)> sg(n, 0LL, gc);
    vector<ll> g(n);
    for (auto& x : g) x = rnd(1, 60);
    sg.build(g);
    SegTree<BestNode, decltype(&best_combine)> sb(n, BEST_ID, &best_combine);
    {
        vector<BestNode> leaves(n);
        for (int i = 0; i < n; i++) leaves[i] = best_leaf(a[i]);
        sb.build(leaves);
    }
    for (int it = 0; it < 800; it++) {
        int p = rnd(0, n - 1); ll v = rnd(-20, 20);
        a[p] = v; st.set(p, v); sb.set(p, best_leaf(v));
        int gp = rnd(0, n - 1); ll gv = rnd(1, 60);
        g[gp] = gv; sg.set(gp, gv);
        int l = rnd(0, n - 1), r = rnd(l + 1, n);  // half-open
        assert(st.query(l, r) == *min_element(a.begin() + l, a.begin() + r));
        ll gg = 0;
        for (int i = l; i < r; i++) gg = std::gcd(gg, g[i]);
        assert(sg.query(l, r) == gg);
        // brute best subarray sum (empty allowed)
        ll best = 0, cur = 0;
        for (int i = l; i < r; i++) { cur = max(0LL, cur + a[i]); best = max(best, cur); }
        assert(sb.query(l, r).best == best);
    }
}

static void test_lazy() {
    int n = 45;
    vector<ll> a(n);
    for (auto& x : a) x = rnd(0, 10);
    LazySeg ls(n);
    ls.build(a);
    for (int it = 0; it < 1500; it++) {
        int op = rnd(0, 3);
        int l = rnd(0, n - 1), r = rnd(l, n - 1);
        if (op == 0) { ll x = rnd(0, 10); ls.range_assign(l, r, x); for (int i = l; i <= r; i++) a[i] = x; }
        else if (op == 1) { ll d = rnd(0, 5); ls.range_add(l, r, d); for (int i = l; i <= r; i++) a[i] += d; }
        else if (op == 2) {
            ll s = 0;
            for (int i = l; i <= r; i++) s += a[i];
            assert(ls.query(l, r) == s);
        } else {
            ll total = accumulate(a.begin(), a.end(), 0LL);
            if (total == 0) continue;
            ll k = rnd(1, total);
            int expect = 0; ll run = 0;
            while (run + a[expect] < k) run += a[expect++];
            assert(ls.first_prefix_geq(k) == expect);
        }
    }
    vector<ll> h(n);
    for (auto& x : h) x = rnd(0, 30);
    MaxSeg ms(h);
    for (int it = 0; it < 800; it++) {
        int p = rnd(0, n - 1); ll v = rnd(0, 30);
        h[p] = v; ms.set(p, v);
        int l = rnd(0, n - 1); ll x = rnd(0, 32);
        int expect = -1;
        for (int i = l; i < n; i++) if (h[i] >= x) { expect = i; break; }
        assert(ms.first_geq(l, x) == expect);
    }
}

static void test_merge_sort_tree() {
    int n = 50;
    vector<ll> a(n);
    for (auto& x : a) x = rnd(0, 40);
    MergeSortTree mt(a);
    for (int it = 0; it < 800; it++) {
        int l = rnd(0, n - 1), r = rnd(l + 1, n); ll x = rnd(-1, 42);
        int c = 0;
        for (int i = l; i < r; i++) c += a[i] < x;
        assert(mt.count_less(l, r, x) == c);
    }
}

static void test_persistent() {
    int n = 60;
    vector<ll> a(n);
    for (auto& x : a) x = rnd(-1000, 1000);
    // coordinate compression
    vector<ll> vals(a);
    sort(vals.begin(), vals.end());
    vals.erase(unique(vals.begin(), vals.end()), vals.end());
    int m = vals.size();
    PersistentSeg ps(m);
    vector<int> root(n + 1, 0);
    for (int i = 0; i < n; i++) {
        int pos = lower_bound(vals.begin(), vals.end(), a[i]) - vals.begin();
        root[i + 1] = ps.insert(root[i], pos);
    }
    for (int it = 0; it < 800; it++) {
        int l = rnd(0, n - 1), r = rnd(l, n - 1); int k = rnd(1, r - l + 1);
        vector<ll> sub(a.begin() + l, a.begin() + r + 1);
        nth_element(sub.begin(), sub.begin() + k - 1, sub.end());
        assert(vals[ps.kth(root[l], root[r + 1], k)] == sub[k - 1]);
    }
}

static void test_dynamic() {
    const ll N = 1000000000LL;
    DynSeg ds(N);
    map<ll, ll> brute;
    vector<ll> coords;
    for (int i = 0; i < 40; i++) coords.push_back(rnd(0, N - 1));
    coords.push_back(0); coords.push_back(N - 1);
    for (int it = 0; it < 600; it++) {
        ll pos = coords[rnd(0, coords.size() - 1)]; ll v = rnd(-9, 9);
        ds.add(pos, v); brute[pos] += v;
        ll l = rnd(0, N - 1), r = rnd(l, N - 1);
        if (rnd(0, 1)) { l = coords[rnd(0, coords.size() - 1)]; r = coords[rnd(0, coords.size() - 1)]; if (l > r) swap(l, r); }
        ll s = 0;
        for (auto& [p, x] : brute) if (l <= p && p <= r) s += x;
        assert(ds.sum(l, r) == s);
    }
}

static void test_sqrt_and_mo() {
    int n = 70;
    vector<ll> a(n);
    for (auto& x : a) x = rnd(-10, 10);
    SqrtDecomp sd(a);
    for (int it = 0; it < 800; it++) {
        int l = rnd(0, n - 1), r = rnd(l, n - 1);
        if (rnd(0, 1)) { ll d = rnd(-5, 5); sd.range_add(l, r, d); for (int i = l; i <= r; i++) a[i] += d; }
        else {
            ll s = 0;
            for (int i = l; i <= r; i++) s += a[i];
            assert(sd.sum(l, r) == s);
        }
    }
    vector<int> b(n);
    for (auto& x : b) x = rnd(0, 12);
    vector<pair<int, int>> qs;
    for (int i = 0; i < 150; i++) { int l = rnd(0, n - 1), r = rnd(l, n - 1); qs.push_back({l, r}); }
    auto ans = mo_distinct(b, qs);
    for (size_t i = 0; i < qs.size(); i++) {
        set<int> s(b.begin() + qs[i].first, b.begin() + qs[i].second + 1);
        assert(ans[i] == (int)s.size());
    }
}

int main() {
    test_prefix();
    test_sparse();
    test_fenwick();
    test_iterative_segtree();
    test_lazy();
    test_merge_sort_tree();
    test_persistent();
    test_dynamic();
    test_sqrt_and_mo();
    cout << "All chapter 07 tests passed.\n";
    return 0;
}
