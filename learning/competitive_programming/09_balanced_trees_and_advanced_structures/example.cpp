// Chapter 09 — Balanced trees and advanced structures: reference library.
//
// TRAINING RULE: do not copy this file. Read it, close it, and re-type every
// struct from memory until it compiles and passes the same asserts. In a
// contest you will have ~5 minutes to write an implicit treap; that speed
// only comes from having typed it 10+ times.
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Contents (each cross-checked against a brute force in main()):
//   1. Treap with explicit keys  (multiset: insert / erase / k-th / rank)
//   2. Implicit treap ("rope")   (insert at pos / erase range / reverse / sum,min / cut-paste)
//   3. pbds ordered set          (only if <ext/pb_ds> exists — libstdc++/GCC only)
//   4. Set of intervals          (assign a value to a range, "Chtholly")
//   5. priority_queue with lazy deletion
//   6. Two-heaps running median
//   7. Li Chao tree              (insert line, max at integer x)
//   8. Monotone convex hull trick (min, slopes decreasing, queries increasing)
//   9. Sparse (dynamic) segment tree over [0, 1e9)
//
// Headers are listed explicitly so the file also builds on libc++ (Apple
// clang has no <bits/stdc++.h>). In a contest on GCC just use <bits/stdc++.h>.
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <utility>
#include <vector>

using namespace std;
using ll = long long;

// ───────────────────────────────────────────────────────────────────────────
// 1. Treap with explicit keys (ordered multiset with order statistics)
//    Invariants: BST order on key, max-heap order on pri. Node 0 is "null".
//    Expected depth O(log n) because pri is random: node v is an ancestor of
//    u iff v has the max priority among all keys between key(u) and key(v),
//    which has probability 1/(|rank(u)-rank(v)|+1); summing gives ~2 ln n.
// ───────────────────────────────────────────────────────────────────────────
struct Treap {
    struct Node { ll key; unsigned pri; int l, r, sz; };
    vector<Node> t;
    mt19937 rng{12345};
    int root = 0;
    Treap() { t.push_back({0, 0, 0, 0, 0}); }               // index 0 = null
    int newNode(ll k) { t.push_back({k, (unsigned)rng(), 0, 0, 1}); return (int)t.size() - 1; }
    int sz(int v) const { return t[v].sz; }
    void upd(int v) { if (v) t[v].sz = 1 + sz(t[v].l) + sz(t[v].r); }

    // split(v, k): l = all nodes with key <  k,  r = all nodes with key >= k
    void split(int v, ll k, int& l, int& r) {
        if (!v) { l = r = 0; return; }
        if (t[v].key < k) { split(t[v].r, k, t[v].r, r); l = v; }
        else              { split(t[v].l, k, l, t[v].l); r = v; }
        upd(v);
    }
    // merge(a, b): requires every key in a <= every key in b
    int merge(int a, int b) {
        if (!a || !b) return a ? a : b;
        if (t[a].pri > t[b].pri) { t[a].r = merge(t[a].r, b); upd(a); return a; }
        else                     { t[b].l = merge(a, t[b].l); upd(b); return b; }
    }
    void insert(ll k) { int l, r; split(root, k, l, r); root = merge(merge(l, newNode(k)), r); }
    bool erase(ll k) {                                       // erase ONE copy of k
        int l, m, r;
        split(root, k, l, m); split(m, k + 1, m, r);         // m = all copies of k
        if (!m) { root = merge(l, r); return false; }
        m = merge(t[m].l, t[m].r);                           // drop the root of m
        root = merge(merge(l, m), r);
        return true;
    }
    ll kth(int k) const {                                    // 0-indexed, k < size()
        int v = root;
        for (;;) {
            int ls = sz(t[v].l);
            if (k < ls) v = t[v].l;
            else if (k == ls) return t[v].key;
            else { k -= ls + 1; v = t[v].r; }
        }
    }
    int countLess(ll k) const {                              // rank of k = #keys < k
        int v = root, res = 0;
        while (v) {
            if (t[v].key < k) { res += sz(t[v].l) + 1; v = t[v].r; }
            else v = t[v].l;
        }
        return res;
    }
    int size() const { return sz(root); }
};

// ───────────────────────────────────────────────────────────────────────────
// 2. Implicit treap: the key is the POSITION (= size of left subtree + 1 along
//    the root path). Supports a dynamic sequence with reversal (lazy flip),
//    range sum/min, insert/erase at position, cut-and-paste.
//    Lazy invariant: node.rev == true means "this whole subtree must still be
//    reversed"; the node's own aggregates (sum, mn, sz) are already correct
//    (they are symmetric), only the child ORDER is stale. push() before
//    reading children in split/merge/inorder.
// ───────────────────────────────────────────────────────────────────────────
struct ImplicitTreap {
    struct Node { ll val, sum, mn; unsigned pri; int l, r, sz; bool rev; };
    vector<Node> t;
    mt19937 rng{777};
    int root = 0;
    ImplicitTreap() { t.push_back({0, 0, LLONG_MAX, 0, 0, 0, 0, false}); }
    int newNode(ll v) { t.push_back({v, v, v, (unsigned)rng(), 0, 0, 1, false}); return (int)t.size() - 1; }
    int sz(int v) const { return t[v].sz; }
    void push(int v) {
        if (v && t[v].rev) {
            swap(t[v].l, t[v].r);
            if (t[v].l) t[t[v].l].rev ^= 1;
            if (t[v].r) t[t[v].r].rev ^= 1;
            t[v].rev = false;
        }
    }
    void upd(int v) {
        if (!v) return;
        const Node& L = t[t[v].l]; const Node& R = t[t[v].r];
        t[v].sz = 1 + L.sz + R.sz;
        t[v].sum = t[v].val + L.sum + R.sum;
        t[v].mn = min({t[v].val, L.mn, R.mn});
    }
    // split(v, k): l = first k elements, r = the rest
    void split(int v, int k, int& l, int& r) {
        if (!v) { l = r = 0; return; }
        push(v);
        if (sz(t[v].l) < k) { split(t[v].r, k - sz(t[v].l) - 1, t[v].r, r); l = v; }
        else                { split(t[v].l, k, l, t[v].l); r = v; }
        upd(v);
    }
    int merge(int a, int b) {
        if (!a || !b) return a ? a : b;
        push(a); push(b);
        if (t[a].pri > t[b].pri) { t[a].r = merge(t[a].r, b); upd(a); return a; }
        else                     { t[b].l = merge(a, t[b].l); upd(b); return b; }
    }
    int size() const { return sz(root); }
    void insert(int pos, ll v) {                             // new element becomes index pos
        int l, r; split(root, pos, l, r);
        root = merge(merge(l, newNode(v)), r);
    }
    void erase(int l, int r) {                               // erase [l, r)
        int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
        root = merge(a, c);
    }
    void reverse(int l, int r) {                             // reverse [l, r)
        int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
        if (b) t[b].rev ^= 1;
        root = merge(merge(a, b), c);
    }
    pair<ll, ll> query(int l, int r) {                       // (sum, min) of [l, r)
        int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
        pair<ll, ll> res = {t[b].sum, t[b].mn};
        root = merge(merge(a, b), c);
        return res;
    }
    // Cut [l, r) out and paste it so that it starts at index pos of the REMAINING sequence.
    void cutPaste(int l, int r, int pos) {
        int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
        int rest = merge(a, c);
        int x, y; split(rest, pos, x, y);
        root = merge(merge(x, b), y);
    }
    ll get(int pos) {                                        // element at index pos
        int v = root;
        for (;;) {
            push(v);
            int ls = sz(t[v].l);
            if (pos < ls) v = t[v].l;
            else if (pos == ls) return t[v].val;
            else { pos -= ls + 1; v = t[v].r; }
        }
    }
    void toVector(int v, vector<ll>& out) {                  // inorder, O(n)
        if (!v) return;
        push(v);
        toVector(t[v].l, out); out.push_back(t[v].val); toVector(t[v].r, out);
    }
    vector<ll> toVector() { vector<ll> out; toVector(root, out); return out; }
};

// ───────────────────────────────────────────────────────────────────────────
// 3. pbds ordered set — GCC/libstdc++ only. Guarded so the file also builds
//    on libc++ (Apple clang, some online judges). Never rely on it blindly.
// ───────────────────────────────────────────────────────────────────────────
#if __has_include(<ext/pb_ds/assoc_container.hpp>)
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
template <class T>
using ordered_set = __gnu_pbds::tree<T, __gnu_pbds::null_type, less<T>, __gnu_pbds::rb_tree_tag,
                                     __gnu_pbds::tree_order_statistics_node_update>;
// Multiset trick: use pair<T,int> with a unique tie-breaker, or less_equal<T>
// (then find/erase by value break — erase via find_by_order(order_of_key(x))).
static bool HAVE_PBDS = true;
#else
static bool HAVE_PBDS = false;
#endif

// ───────────────────────────────────────────────────────────────────────────
// 4. Set of intervals covering [0, n): map l -> (r, value). "Assign v on [l,r)"
//    splits at most 2 intervals and erases everything strictly inside.
//    Amortised: each assign creates <= 3 intervals, and every interval erased
//    was created earlier, so total work is O((n + q) log n).
// ───────────────────────────────────────────────────────────────────────────
struct IntervalSet {
    map<int, pair<int, ll>> m;                               // l -> (r, val), disjoint, cover [0,n)
    IntervalSet(int n, ll v) { m[0] = {n, v}; }
    // make x a boundary; return iterator to the interval starting at x
    map<int, pair<int, ll>>::iterator split(int x) {
        auto it = prev(m.upper_bound(x));                     // interval containing x
        if (it->first == x) return it;
        auto [r, v] = it->second;
        it->second.first = x;                                // [l, x)
        return m.emplace(x, make_pair(r, v)).first;          // [x, r)
    }
    void assign(int l, int r, ll v) {                        // set [l, r) := v
        auto itr = split(r), itl = split(l);                 // split r first, then l
        m.erase(itl, itr);
        m[l] = {r, v};
    }
    ll sum(int l, int r) {                                   // sum of values on [l, r)
        auto itr = split(r), itl = split(l);
        ll s = 0;
        for (auto it = itl; it != itr; ++it) s += (ll)(it->second.first - it->first) * it->second.second;
        return s;
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 5. priority_queue with lazy deletion: "erase(x)" is recorded; the real
//    removal happens when x reaches the top. top() amortised O(log n).
// ───────────────────────────────────────────────────────────────────────────
struct LazyMaxPQ {
    priority_queue<ll> q, del;                               // del = pending deletions
    void push(ll x) { q.push(x); }
    void erase(ll x) { del.push(x); }                        // x must currently be in the multiset
    void clean() { while (!del.empty() && !q.empty() && q.top() == del.top()) { q.pop(); del.pop(); } }
    bool empty() { clean(); return q.empty(); }
    ll top() { clean(); return q.top(); }
    void pop() { clean(); q.pop(); }
};

// ───────────────────────────────────────────────────────────────────────────
// 6. Two heaps: lower half in a max-heap, upper half in a min-heap,
//    |lo| == |hi| or |lo| == |hi| + 1. Median = lo.top() (lower median).
// ───────────────────────────────────────────────────────────────────────────
struct RunningMedian {
    priority_queue<ll> lo;                                   // max-heap of the smaller half
    priority_queue<ll, vector<ll>, greater<ll>> hi;          // min-heap of the larger half
    void add(ll x) {
        if (lo.empty() || x <= lo.top()) lo.push(x); else hi.push(x);
        if (lo.size() > hi.size() + 1) { hi.push(lo.top()); lo.pop(); }
        else if (hi.size() > lo.size()) { lo.push(hi.top()); hi.pop(); }
    }
    ll median() const { return lo.top(); }                   // lower median for even count
};

// ───────────────────────────────────────────────────────────────────────────
// 7. Li Chao tree over integer x in [lo, hi]: insert line y = a*x + b, query
//    max at x. Each node keeps the line that wins at the node's midpoint; the
//    loser can only win on one half, so it is pushed down: O(log C) per op.
// ───────────────────────────────────────────────────────────────────────────
struct LiChao {
    struct Line { ll a, b; ll operator()(ll x) const { return a * x + b; } };
    int lo, hi;
    vector<Line> line; vector<char> has;                     // array-indexed over [lo,hi]
    LiChao(int lo_, int hi_) : lo(lo_), hi(hi_) {
        int sz = 1; while (sz < hi - lo + 1) sz <<= 1;
        line.assign(2 * sz, {0, 0}); has.assign(2 * sz, 0);
    }
    void insert(Line nw) { insert(1, lo, hi, nw); }
    void insert(int v, int l, int r, Line nw) {
        if (!has[v]) { line[v] = nw; has[v] = 1; return; }
        int m = (l + r) >> 1;                                // l <= m < r when l < r
        bool leftBetter = nw(l) > line[v](l), midBetter = nw(m) > line[v](m);
        if (midBetter) swap(line[v], nw);                    // node keeps the winner at m
        if (l == r) return;
        if (leftBetter != midBetter) insert(2 * v, l, m, nw); // loser wins somewhere on the left
        else insert(2 * v + 1, m + 1, r, nw);
    }
    ll query(ll x) const {                                   // returns LLONG_MIN if empty
        int v = 1, l = lo, r = hi; ll res = LLONG_MIN;
        for (;;) {
            if (has[v]) res = max(res, line[v](x));
            if (l == r) return res;
            int m = (l + r) >> 1;
            if (x <= m) { v = 2 * v; r = m; } else { v = 2 * v + 1; l = m + 1; }
        }
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 8. Monotone convex hull trick (MIN version): lines added with strictly
//    DECREASING slopes, queries at NON-DECREASING x. Deque pointer never moves
//    back, so everything is amortised O(1).  Line (a,b): y = a*x + b.
//    bad(l1,l2,l3): l2 is useless if intersection(l1,l3) is left of intersection(l1,l2).
// ───────────────────────────────────────────────────────────────────────────
struct MonotoneCHT {
    vector<pair<ll, ll>> h;                                  // (a, b)
    size_t ptr = 0;
    // uses __int128 to avoid overflow in the cross-multiplication
    static bool bad(pair<ll, ll> l1, pair<ll, ll> l2, pair<ll, ll> l3) {
        // x12 = (b2-b1)/(a1-a2), x13 = (b3-b1)/(a1-a3); l2 useless iff x13 <= x12
        return (__int128)(l3.second - l1.second) * (l1.first - l2.first) <=
               (__int128)(l2.second - l1.second) * (l1.first - l3.first);
    }
    void add(ll a, ll b) {                                   // a strictly less than all previous a
        pair<ll, ll> nw{a, b};
        while (h.size() >= 2 && bad(h[h.size() - 2], h.back(), nw)) h.pop_back();
        h.push_back(nw);
        if (ptr >= h.size()) ptr = h.size() - 1;
    }
    ll query(ll x) {                                         // x non-decreasing across calls
        while (ptr + 1 < h.size() && h[ptr + 1].first * x + h[ptr + 1].second <= h[ptr].first * x + h[ptr].second) ++ptr;
        return h[ptr].first * x + h[ptr].second;
    }
};

// ───────────────────────────────────────────────────────────────────────────
// 9. Sparse segment tree: nodes created on demand, coordinates up to 1e9,
//    memory O(q log C). Point add, range sum.
// ───────────────────────────────────────────────────────────────────────────
struct SparseSeg {
    struct Node { ll sum = 0; int l = 0, r = 0; };
    vector<Node> t; int LO, HI;                              // range [LO, HI)
    SparseSeg(int lo, int hi) : t(2), LO(lo), HI(hi) {}      // node 0 = null, node 1 = root
    // PITFALL: emplace_back may reallocate t, invalidating the reference c —
    // so compute the index into a local, write it, and never touch c afterwards.
    int child(int& c) { if (!c) { int id = (int)t.size(); c = id; t.emplace_back(); return id; } return c; }
    void add(int pos, ll delta) { add(1, LO, HI, pos, delta); }
    void add(int v, int l, int r, int pos, ll delta) {
        t[v].sum += delta;
        if (r - l == 1) return;
        int m = l + (r - l) / 2;
        if (pos < m) { int c = child(t[v].l); add(c, l, m, pos, delta); }   // child() may realloc t: take c first
        else         { int c = child(t[v].r); add(c, m, r, pos, delta); }
    }
    ll sum(int ql, int qr) const { return sum(1, LO, HI, ql, qr); }
    ll sum(int v, int l, int r, int ql, int qr) const {
        if (!v || qr <= l || r <= ql) return 0;
        if (ql <= l && r <= qr) return t[v].sum;
        int m = l + (r - l) / 2;
        return sum(t[v].l, l, m, ql, qr) + sum(t[v].r, m, r, ql, qr);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════════════
static mt19937 rnd(2024);
static ll rnd_ll(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rnd); }

static void test_treap() {
    Treap tr; multiset<ll> ref;
    for (int it = 0; it < 20000; ++it) {
        int op = (int)rnd_ll(0, 3);
        ll k = rnd_ll(-50, 50);
        if (op == 0) { tr.insert(k); ref.insert(k); }
        else if (op == 1) {
            bool a = tr.erase(k);
            auto f = ref.find(k); bool b = f != ref.end(); if (b) ref.erase(f);
            assert(a == b);
        } else if (op == 2 && !ref.empty()) {
            int idx = (int)rnd_ll(0, (ll)ref.size() - 1);
            assert(tr.kth(idx) == *next(ref.begin(), idx));
        } else {
            assert(tr.countLess(k) == (int)distance(ref.begin(), ref.lower_bound(k)));
        }
        assert(tr.size() == (int)ref.size());
    }
    puts("treap (explicit keys)      OK");
}

static void test_implicit_treap() {
    ImplicitTreap tr; vector<ll> ref;
    for (int it = 0; it < 20000; ++it) {
        int n = (int)ref.size();
        int op = (int)rnd_ll(0, 5);
        if (op == 0 || n == 0) {                             // insert
            int pos = (int)rnd_ll(0, n); ll v = rnd_ll(-1000, 1000);
            tr.insert(pos, v); ref.insert(ref.begin() + pos, v);
        } else if (op == 1) {                                // erase range
            int l = (int)rnd_ll(0, n - 1), r = (int)rnd_ll(l + 1, n);
            tr.erase(l, r); ref.erase(ref.begin() + l, ref.begin() + r);
        } else if (op == 2) {                                // reverse range
            int l = (int)rnd_ll(0, n - 1), r = (int)rnd_ll(l + 1, n);
            tr.reverse(l, r); std::reverse(ref.begin() + l, ref.begin() + r);
        } else if (op == 3) {                                // sum / min
            int l = (int)rnd_ll(0, n - 1), r = (int)rnd_ll(l + 1, n);
            auto [s, mn] = tr.query(l, r);
            ll rs = 0, rm = LLONG_MAX;
            for (int i = l; i < r; ++i) { rs += ref[i]; rm = min(rm, ref[i]); }
            assert(s == rs && mn == rm);
        } else if (op == 4) {                                // cut & paste
            int l = (int)rnd_ll(0, n - 1), r = (int)rnd_ll(l + 1, n);
            int pos = (int)rnd_ll(0, n - (r - l));
            tr.cutPaste(l, r, pos);
            vector<ll> blk(ref.begin() + l, ref.begin() + r);
            ref.erase(ref.begin() + l, ref.begin() + r);
            ref.insert(ref.begin() + pos, blk.begin(), blk.end());
        } else {                                             // point read
            int p = (int)rnd_ll(0, n - 1);
            assert(tr.get(p) == ref[p]);
        }
        if (it % 500 == 0) assert(tr.toVector() == ref);
    }
    assert(tr.toVector() == ref);
    puts("implicit treap (rope)      OK");
}

static void test_pbds() {
#if __has_include(<ext/pb_ds/assoc_container.hpp>)
    ordered_set<int> s; set<int> ref;
    for (int it = 0; it < 5000; ++it) {
        int x = (int)rnd_ll(0, 200);
        if (rnd_ll(0, 1)) { s.insert(x); ref.insert(x); } else { s.erase(x); ref.erase(x); }
        assert((int)s.order_of_key(x) == (int)distance(ref.begin(), ref.lower_bound(x)));
        if (!ref.empty()) { int k = (int)rnd_ll(0, (ll)ref.size() - 1); assert(*s.find_by_order(k) == *next(ref.begin(), k)); }
    }
    puts("pbds ordered_set           OK");
#else
    puts("pbds ordered_set           skipped (no <ext/pb_ds> on this toolchain — that is the caveat)");
#endif
}

static void test_interval_set() {
    const int n = 60;
    IntervalSet is(n, 0); vector<ll> ref(n, 0);
    for (int it = 0; it < 5000; ++it) {
        int l = (int)rnd_ll(0, n - 1), r = (int)rnd_ll(l + 1, n);
        if (rnd_ll(0, 1)) { ll v = rnd_ll(-9, 9); is.assign(l, r, v); fill(ref.begin() + l, ref.begin() + r, v); }
        else { ll s = 0; for (int i = l; i < r; ++i) s += ref[i]; assert(is.sum(l, r) == s); }
        // every interval boundary set must be disjoint & covering
        int prevr = 0; for (auto& [a, pr] : is.m) { assert(a == prevr); prevr = pr.first; } assert(prevr == n);
    }
    puts("set of intervals           OK");
}

static void test_lazy_pq_and_median() {
    LazyMaxPQ pq; multiset<ll> ref;
    for (int it = 0; it < 5000; ++it) {
        ll x = rnd_ll(0, 30);
        int op = (int)rnd_ll(0, 2);
        if (op == 0) { pq.push(x); ref.insert(x); }
        else if (op == 1 && ref.count(x)) { pq.erase(x); ref.erase(ref.find(x)); }
        else if (!ref.empty()) { assert(pq.top() == *ref.rbegin()); if (rnd_ll(0, 1)) { pq.pop(); ref.erase(prev(ref.end())); } }
        assert(pq.empty() == ref.empty());
    }
    RunningMedian rm; vector<ll> all;
    for (int i = 0; i < 3000; ++i) {
        ll x = rnd_ll(-1000, 1000); rm.add(x); all.push_back(x);
        vector<ll> s = all; nth_element(s.begin(), s.begin() + ((int)s.size() - 1) / 2, s.end());
        assert(rm.median() == s[((int)s.size() - 1) / 2]);
    }
    puts("lazy PQ + running median   OK");
}

static void test_lichao_and_cht() {
    LiChao lc(-100, 100); vector<pair<ll, ll>> lines;
    for (int it = 0; it < 3000; ++it) {
        if (rnd_ll(0, 1) || lines.empty()) { ll a = rnd_ll(-1000, 1000), b = rnd_ll(-1000000, 1000000); lc.insert({a, b}); lines.push_back({a, b}); }
        else { ll x = rnd_ll(-100, 100); ll best = LLONG_MIN; for (auto [a, b] : lines) best = max(best, a * x + b); assert(lc.query(x) == best); }
    }
    // monotone CHT: slopes strictly decreasing, queries non-decreasing
    for (int round = 0; round < 50; ++round) {
        MonotoneCHT cht; vector<pair<ll, ll>> ls;
        ll a = rnd_ll(500, 1000);
        for (int i = 0; i < 40; ++i) { a -= rnd_ll(1, 20); ll b = rnd_ll(-100000, 100000); cht.add(a, b); ls.push_back({a, b}); }
        ll x = -1000;
        for (int q = 0; q < 200; ++q) {
            x += rnd_ll(0, 15);
            ll best = LLONG_MAX; for (auto [aa, bb] : ls) best = min(best, aa * x + bb);
            assert(cht.query(x) == best);
        }
    }
    puts("Li Chao + monotone CHT     OK");
}

static void test_sparse_seg() {
    SparseSeg st(0, 1000000000); map<int, ll> ref;
    for (int it = 0; it < 5000; ++it) {
        if (rnd_ll(0, 1)) { int p = (int)rnd_ll(0, 999999999); ll d = rnd_ll(-100, 100); st.add(p, d); ref[p] += d; }
        else {
            int l = (int)rnd_ll(0, 999999999), r = (int)rnd_ll(l, 1000000000);
            ll s = 0; for (auto it2 = ref.lower_bound(l); it2 != ref.end() && it2->first < r; ++it2) s += it2->second;
            assert(st.sum(l, r) == s);
        }
    }
    puts("sparse segment tree        OK");
}

int main() {
    test_treap();
    test_implicit_treap();
    test_pbds();
    test_interval_set();
    test_lazy_pq_and_median();
    test_lichao_and_cht();
    test_sparse_seg();
    printf("pbds available on this toolchain: %s\n", HAVE_PBDS ? "yes" : "no");
    puts("all chapter 09 tests passed");
    return 0;
}
