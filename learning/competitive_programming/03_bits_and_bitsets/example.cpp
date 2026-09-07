// Chapter 03 — Bits and bitsets: reference library.
//
// TRAINING RULE: this file is a snippet library, not a solution. Re-type every
// function from memory (close the file, write it, diff). If you cannot reproduce
// a snippet in under two minutes, you do not own it yet.
//
// Build & run:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
// Every section ends with asserts, most against a brute force on random input.

// On CSES/Codeforces (GCC) a single `#include <bits/stdc++.h>` replaces all of
// these. Apple clang / libc++ has no such header, so the file lists what it uses.
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <vector>
using namespace std;
using ll  = long long;
using ull = unsigned long long;

static mt19937_64 rng(20260905);
static ll rnd(ll lo, ll hi) { return lo + (ll)(rng() % (ull)(hi - lo + 1)); }

// ───────────────────────── 1. Single-bit operations ─────────────────────────
// Always shift an UNSIGNED 64-bit 1 (1ULL << k). `1 << k` is int: k >= 31 is UB.
inline bool test_bit  (ull x, int k) { return (x >> k) & 1ULL; }
inline ull  set_bit   (ull x, int k) { return x |  (1ULL << k); }
inline ull  clear_bit (ull x, int k) { return x & ~(1ULL << k); }
inline ull  toggle_bit(ull x, int k) { return x ^  (1ULL << k); }

// Two's complement: -x = ~x + 1, so x & -x keeps exactly the lowest set bit,
// and x-1 flips the lowest set bit and every zero below it, so x & (x-1) clears it.
inline ll   lowbit     (ll x) { return x & -x; }
inline ll   drop_lowbit(ll x) { return x & (x - 1); }
inline bool is_pow2    (ll x) { return x > 0 && (x & (x - 1)) == 0; }

// Builtins. All three are UB on 0 for ctz/clz — wrap them.
inline int popcnt(ull x) { return __builtin_popcountll(x); }
inline int ctz(ull x)    { return x ? __builtin_ctzll(x) : 64; }   // index of lowest set bit
inline int clz(ull x)    { return x ? __builtin_clzll(x) : 64; }
inline int bit_length(ull x) { return 64 - clz(x); }                 // #bits needed; 0 for x=0
inline int floor_log2(ull x) { assert(x); return 63 - __builtin_clzll(x); }
inline ull next_pow2(ull x)  { return x <= 1 ? 1 : 1ULL << bit_length(x - 1); }

// Iterate over set bits: O(popcount), not O(64).
template <class F> void for_each_bit(ull m, F f) {
    while (m) { int b = __builtin_ctzll(m); f(b); m &= m - 1; }
}

// Sum of set bits in binary representations of 1..n, O(log n).
// Bit b has period 2^(b+1): 2^b zeros then 2^b ones, over the range 0..n.
ll count_ones_upto(ll n) {
    ll res = 0;
    for (int b = 0; b < 62 && (1LL << b) <= n; b++) {
        ll half = 1LL << b, period = half << 1;
        ll full = (n + 1) / period, rem = (n + 1) % period;
        res += full * half + max(0LL, rem - half);
    }
    return res;
}

void test_basic() {
    ull x = 0;
    x = set_bit(x, 5); x = set_bit(x, 63);
    assert(test_bit(x, 5) && test_bit(x, 63) && !test_bit(x, 4));
    x = toggle_bit(x, 5); assert(!test_bit(x, 5));
    x = clear_bit(x, 63); assert(x == 0);
    assert(lowbit(40) == 8 && drop_lowbit(40) == 32 && lowbit(12) == 4);
    assert(is_pow2(1) && is_pow2(1LL << 40) && !is_pow2(0) && !is_pow2(12));
    assert(popcnt(2024) == 7 && ctz(40) == 3 && floor_log2(40) == 5 && bit_length(0) == 0);
    assert(ctz(0) == 64 && clz(0) == 64);
    assert(next_pow2(5) == 8 && next_pow2(8) == 8 && next_pow2(0) == 1);
    vector<int> bits; for_each_bit(0b101001ULL, [&](int b) { bits.push_back(b); });
    assert((bits == vector<int>{0, 3, 5}));
    // brute-force check of count_ones_upto
    ll acc = 0;
    for (ll n = 1; n <= 2000; n++) { acc += popcnt(n); assert(count_ones_upto(n) == acc); }
    assert(count_ones_upto(5) == 7);
}

// ───────────────────────── 2. Submasks / supermasks / Gray code ─────────────
// All submasks of m in decreasing order. Total over all m of 2^popcount(m) = 3^n.
template <class F> void for_each_submask(int m, F f) {
    for (int s = m;; s = (s - 1) & m) { f(s); if (s == 0) break; }
}
// All supermasks of m inside n bits, increasing order. (s+1)|m: adding 1 then
// restoring m's bits jumps straight to the next superset.
template <class F> void for_each_supermask(int m, int n, F f) {
    for (int s = m; s < (1 << n); s = (s + 1) | m) f(s);
}
inline unsigned gray(unsigned i) { return i ^ (i >> 1); }
inline unsigned inv_gray(unsigned g) { unsigned i = 0; for (; g; g >>= 1) i ^= g; return i; }

void test_submasks() {
    for (int n = 0; n <= 10; n++) {
        ll total = 0, pow3 = 1;
        for (int m = 0; m < (1 << n); m++) {
            int prev = -1; ll cnt = 0;
            for_each_submask(m, [&](int s) {
                assert((s & m) == s); assert(prev == -1 || s < prev); prev = s; cnt++;
            });
            assert(cnt == (1LL << popcnt(m)));
            total += cnt;
        }
        for (int i = 0; i < n; i++) pow3 *= 3;
        assert(total == pow3);
    }
    int n = 6, m = 0b100101, cnt = 0;
    for_each_supermask(m, n, [&](int s) { assert((s & m) == m); cnt++; });
    assert(cnt == (1 << (n - popcnt(m))));
    for (unsigned i = 0; i < (1u << 10); i++) {
        assert(inv_gray(gray(i)) == i);
        if (i) assert(popcnt(gray(i) ^ gray(i - 1)) == 1);
    }
}

// ───────────────────────── 3. Bitmask DP ────────────────────────────────────
// Count Hamiltonian paths s -> t visiting every vertex once (directed graph,
// adjacency matrix adj[u][v]). dp[mask][v] = #paths from s covering `mask`,
// ending at v. O(2^n * n^2) time, O(2^n * n) memory.  n <= 20 in contests.
ll hamiltonian_paths(int n, const vector<vector<int>>& adj, int s, int t, ll mod) {
    vector<vector<ll>> dp(1 << n, vector<ll>(n, 0));
    dp[1 << s][s] = 1;
    for (int mask = 0; mask < (1 << n); mask++) {
        if (!(mask >> s & 1)) continue;
        for (int v = 0; v < n; v++) {
            if (!(mask >> v & 1) || dp[mask][v] == 0) continue;
            if (v == t && mask != (1 << n) - 1) continue;   // t must be last
            ll cur = dp[mask][v];
            for (int w = 0; w < n; w++)
                if (!(mask >> w & 1) && adj[v][w])
                    dp[mask | 1 << w][w] = (dp[mask | 1 << w][w] + cur * adj[v][w]) % mod;
        }
    }
    return dp[(1 << n) - 1][t];
}

// TSP: shortest closed tour through all vertices starting/ending at 0.
// dp[mask][v] = min cost of a path from 0 through `mask`, ending at v.
const ll INF = (ll)4e18;
ll tsp(int n, const vector<vector<ll>>& w) {
    vector<vector<ll>> dp(1 << n, vector<ll>(n, INF));
    dp[1][0] = 0;
    for (int mask = 1; mask < (1 << n); mask++)
        for (int v = 0; v < n; v++) {
            if (dp[mask][v] == INF) continue;
            for (int u = 0; u < n; u++)
                if (!(mask >> u & 1))
                    dp[mask | 1 << u][u] = min(dp[mask | 1 << u][u], dp[mask][v] + w[v][u]);
        }
    ll best = INF;
    for (int v = 1; v < n; v++) if (dp[(1 << n) - 1][v] < INF) best = min(best, dp[(1 << n) - 1][v] + w[v][0]);
    return best;
}

// Elevator Rides pattern: dp[mask] = (rides, weight of last ride), minimise
// lexicographically. Greedy "add to current ride if it fits" inside the DP is
// correct because any optimal packing can be reordered so that the last ride is
// filled by the last element.
pair<int, ll> elevator(const vector<ll>& wts, ll cap) {
    int n = wts.size();
    vector<pair<int, ll>> dp(1 << n, {n + 1, 0});
    dp[0] = {1, 0};
    for (int mask = 1; mask < (1 << n); mask++)
        for (int i = 0; i < n; i++) if (mask >> i & 1) {
            auto [r, wgt] = dp[mask ^ (1 << i)];
            if (wgt + wts[i] <= cap) wgt += wts[i]; else { r++; wgt = wts[i]; }
            dp[mask] = min(dp[mask], make_pair(r, wgt));
        }
    return dp[(1 << n) - 1];
}

void test_bitdp() {
    for (int iter = 0; iter < 30; iter++) {
        int n = (int)rnd(2, 7);
        vector<vector<int>> adj(n, vector<int>(n, 0));
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (i != j) adj[i][j] = (int)rnd(0, 1);
        ll brute = 0;
        vector<int> p(n); iota(p.begin(), p.end(), 0);
        do {
            if (p[0] != 0 || p[n - 1] != n - 1) continue;
            bool ok = true;
            for (int i = 0; i + 1 < n && ok; i++) ok = adj[p[i]][p[i + 1]];
            brute += ok;
        } while (next_permutation(p.begin(), p.end()));
        assert(hamiltonian_paths(n, adj, 0, n - 1, 1000000007) == brute);
    }
    for (int iter = 0; iter < 20; iter++) {
        int n = (int)rnd(2, 7);
        vector<vector<ll>> w(n, vector<ll>(n));
        for (auto& r : w) for (auto& x : r) x = rnd(1, 50);
        vector<int> p(n - 1); iota(p.begin(), p.end(), 1);
        ll brute = INF;
        do {
            ll c = w[0][p[0]];
            for (int i = 0; i + 1 < n - 1; i++) c += w[p[i]][p[i + 1]];
            c += w[p[n - 2]][0];
            brute = min(brute, c);
        } while (next_permutation(p.begin(), p.end()));
        assert(tsp(n, w) == brute);
    }
    // elevator: brute over all orderings with greedy filling
    for (int iter = 0; iter < 20; iter++) {
        int n = (int)rnd(1, 6); ll cap = rnd(5, 15);
        vector<ll> wts(n); for (auto& x : wts) x = rnd(1, cap);
        vector<int> p(n); iota(p.begin(), p.end(), 0);
        int best = n + 1;
        do {
            int rides = 1; ll cur = 0;
            for (int i : p) { if (cur + wts[i] <= cap) cur += wts[i]; else { rides++; cur = wts[i]; } }
            best = min(best, rides);
        } while (next_permutation(p.begin(), p.end()));
        assert(elevator(wts, cap).first == best);
    }
}

// ───────────────────────── 4. SOS DP (sum over subsets) ─────────────────────
// f[mask] = sum over sub ⊆ mask of a[sub]. O(2^n * n).
// Invariant after processing bit i: f[mask] = sum of a[sub] over sub that agree
// with mask on bits > i and are ⊆ mask on bits <= i.
vector<ll> sos_subsets(vector<ll> f, int n) {
    for (int i = 0; i < n; i++)
        for (int mask = 0; mask < (1 << n); mask++)
            if (mask >> i & 1) f[mask] += f[mask ^ (1 << i)];
    return f;
}
// g[mask] = sum over sup ⊇ mask of a[sup]. Same loop, opposite direction of flow.
vector<ll> sos_supersets(vector<ll> f, int n) {
    for (int i = 0; i < n; i++)
        for (int mask = 0; mask < (1 << n); mask++)
            if (!(mask >> i & 1)) f[mask] += f[mask | (1 << i)];
    return f;
}
// Inverse (Möbius on the subset lattice): recover a from f. Subtract instead.
vector<ll> sos_subsets_inverse(vector<ll> f, int n) {
    for (int i = 0; i < n; i++)
        for (int mask = 0; mask < (1 << n); mask++)
            if (mask >> i & 1) f[mask] -= f[mask ^ (1 << i)];
    return f;
}

void test_sos() {
    for (int n = 0; n <= 10; n++) {
        vector<ll> a(1 << n); for (auto& x : a) x = rnd(-100, 100);
        auto f = sos_subsets(a, n), g = sos_supersets(a, n);
        for (int m = 0; m < (1 << n); m++) {
            ll bs = 0, bg = 0;
            for (int s = 0; s < (1 << n); s++) { if ((s & m) == s) bs += a[s]; if ((s & m) == m) bg += a[s]; }
            assert(f[m] == bs && g[m] == bg);
        }
        assert(sos_subsets_inverse(f, n) == a);
    }
}

// ───────────────────────── 5. XOR linear basis over GF(2) ───────────────────
// b[i] is either 0 or a vector whose highest set bit is i. Span = set of all
// subset xors of inserted values; |span| = 2^sz.
struct XorBasis {
    static const int B = 60;
    ll b[B]; int sz;
    XorBasis() : sz(0) { memset(b, 0, sizeof b); }
    bool insert(ll x) {                     // false if x already in span
        for (int i = B - 1; i >= 0 && x; i--) {
            if (!(x >> i & 1)) continue;
            if (!b[i]) { b[i] = x; sz++; return true; }
            x ^= b[i];
        }
        return false;
    }
    bool contains(ll x) const {
        for (int i = B - 1; i >= 0 && x; i--) if (x >> i & 1) { if (!b[i]) return false; x ^= b[i]; }
        return true;
    }
    ll max_xor(ll x = 0) const {            // max of x ^ (element of span)
        for (int i = B - 1; i >= 0; i--) if ((x ^ b[i]) > x) x ^= b[i];
        return x;
    }
    // Reduced echelon form: each pivot bit appears in exactly one vector. After
    // this, the k-th smallest span element (k 0-indexed, 0 <= k < 2^sz) is the
    // xor of the pivot vectors chosen by k's bits (pivots sorted ascending).
    void reduce() {
        for (int i = 0; i < B; i++) if (b[i])
            for (int j = i + 1; j < B; j++) if (b[j] && (b[j] >> i & 1)) b[j] ^= b[i];
    }
    ll kth(ll k) const {                    // call reduce() first
        ll res = 0; int j = 0;
        for (int i = 0; i < B; i++) if (b[i]) { if (k >> j & 1) res ^= b[i]; j++; }
        return res;
    }
};

void test_xor_basis() {
    for (int iter = 0; iter < 40; iter++) {
        int n = (int)rnd(1, 9);
        vector<ll> a(n); for (auto& x : a) x = rnd(0, 255);
        set<ll> span;
        for (int m = 0; m < (1 << n); m++) { ll x = 0; for (int i = 0; i < n; i++) if (m >> i & 1) x ^= a[i]; span.insert(x); }
        XorBasis xb; for (ll x : a) xb.insert(x);
        assert((1LL << xb.sz) == (ll)span.size());
        assert(xb.max_xor() == *span.rbegin());
        for (ll q = 0; q < 256; q++) {
            ll best = 0; for (ll s : span) best = max(best, s ^ q);
            assert(xb.max_xor(q) == best);
            assert(xb.contains(q) == (span.count(q) > 0));
        }
        xb.reduce();
        vector<ll> sorted(span.begin(), span.end());
        for (ll k = 0; k < (ll)sorted.size(); k++) assert(xb.kth(k) == sorted[k]);
    }
}

// ───────────────────────── 6. std::bitset speedups ──────────────────────────
// Subset-sum reachability (Money Sums): bs |= bs << w. O(n * S / 64).
template <size_t S>
bitset<S> subset_sums(const vector<int>& coins) {
    bitset<S> bs; bs[0] = 1;
    for (int c : coins) bs |= bs << c;
    return bs;
}

// Transitive closure of a directed graph with bitset rows: O(n^3 / 64).
// After the k-loop, reach[i] = set of vertices reachable from i (i included).
template <size_t N>
vector<bitset<N>> transitive_closure(int n, vector<bitset<N>> reach) {
    for (int i = 0; i < n; i++) reach[i][i] = 1;
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            if (reach[i][k]) reach[i] |= reach[k];
    return reach;
}

// libstdc++ (GCC — what CSES/CF/oj.uz run) has the extensions
// bs._Find_first() and bs._Find_next(i): index of the first set bit / the first
// set bit after i, or N if none, in O(N/64). libc++ (Apple clang) lacks them, so
// these wrappers fall back to a linear scan there; on the judge they are O(N/64).
template <size_t N> int find_first(const bitset<N>& b) {
#ifdef __GLIBCXX__
    return (int)b._Find_first();
#else
    for (size_t i = 0; i < N; i++) if (b[i]) return (int)i;
    return (int)N;
#endif
}
template <size_t N> int find_next(const bitset<N>& b, int i) {
#ifdef __GLIBCXX__
    return (int)b._Find_next(i);
#else
    for (size_t j = i + 1; j < N; j++) if (b[j]) return (int)j;
    return (int)N;
#endif
}

// BFS on a dense graph, iterating only over UNVISITED neighbours.
// Total O(n^2 / 64 + n) per source. The same trick solves "BFS on the
// complement graph": use (~adj[v]) & unvisited.
template <size_t N>
vector<int> bfs_dense(int n, const vector<bitset<N>>& adj, int src) {
    vector<int> dist(n, -1);
    bitset<N> unvisited; for (int i = 0; i < n; i++) unvisited[i] = 1;
    unvisited[src] = 0; dist[src] = 0;
    vector<int> q = {src};
    for (size_t qi = 0; qi < q.size(); qi++) {
        int v = q[qi];
        bitset<N> nxt = adj[v] & unvisited;
        for (int u = find_first(nxt); u < (int)N; u = find_next(nxt, u)) {
            unvisited[u] = 0; dist[u] = dist[v] + 1; q.push_back(u);
        }
    }
    return dist;
}

// Hand-rolled dynamic bitset: size known only at run time, and find_next is
// portable and word-parallel. Reachable Nodes / Reachability Queries use this
// (or bitset<N> with N = max n) as the row type.
struct Bits {
    int n; vector<ull> w;
    explicit Bits(int n_ = 0) : n(n_), w((n_ + 63) / 64, 0) {}
    void set(int i)        { w[i >> 6] |=  1ULL << (i & 63); }
    void reset(int i)      { w[i >> 6] &= ~(1ULL << (i & 63)); }
    bool test(int i) const { return w[i >> 6] >> (i & 63) & 1; }
    int  count() const     { int c = 0; for (ull x : w) c += __builtin_popcountll(x); return c; }
    Bits& operator|=(const Bits& o) { for (size_t i = 0; i < w.size(); i++) w[i] |= o.w[i]; return *this; }
    Bits& operator&=(const Bits& o) { for (size_t i = 0; i < w.size(); i++) w[i] &= o.w[i]; return *this; }
    Bits& operator^=(const Bits& o) { for (size_t i = 0; i < w.size(); i++) w[i] ^= o.w[i]; return *this; }
    // first set index >= i, or n if none. O(#words scanned).
    int find_next(int i) const {
        if (i >= n) return n;
        size_t k = i >> 6;
        ull cur = w[k] & (~0ULL << (i & 63));
        while (true) {
            if (cur) return min(n, (int)(k * 64 + __builtin_ctzll(cur)));
            if (++k == w.size()) return n;
            cur = w[k];
        }
    }
};

void test_bitset() {
    // knapsack vs bool DP
    const size_t S = 1024;
    for (int iter = 0; iter < 20; iter++) {
        int n = (int)rnd(1, 12);
        vector<int> coins(n); for (auto& c : coins) c = (int)rnd(1, 60);
        auto bs = subset_sums<S>(coins);
        vector<char> dp(S, 0); dp[0] = 1;
        for (int c : coins) for (int s = (int)S - 1; s >= c; s--) if (dp[s - c]) dp[s] = 1;
        for (size_t s = 0; s < S; s++) assert(bs[s] == (bool)dp[s]);
    }
    // closure & dense BFS vs plain BFS
    const size_t N = 64;
    for (int iter = 0; iter < 15; iter++) {
        int n = (int)rnd(2, 40);
        vector<bitset<N>> adj(n);
        vector<vector<int>> g(n);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++)
            if (i != j && rnd(0, 9) < 2) { adj[i][j] = 1; g[i].push_back(j); }
        auto reach = transitive_closure<N>(n, adj);
        for (int s = 0; s < n; s++) {
            vector<int> d(n, -1); d[s] = 0; queue<int> q; q.push(s);
            while (!q.empty()) { int v = q.front(); q.pop(); for (int u : g[v]) if (d[u] < 0) { d[u] = d[v] + 1; q.push(u); } }
            auto dd = bfs_dense<N>(n, adj, s);
            for (int v = 0; v < n; v++) { assert(reach[s][v] == (d[v] >= 0)); assert(dd[v] == d[v]); }
        }
    }
    // find_first / find_next basics
    bitset<100> b; b[3] = b[64] = b[99] = 1;
    assert(find_first(b) == 3 && find_next(b, 3) == 64 && find_next(b, 64) == 99 && find_next(b, 99) == 100);
    assert(b.count() == 3 && b.any() && !b.none());
    // Bits vs bitset on random data
    for (int iter = 0; iter < 20; iter++) {
        int n = (int)rnd(1, 300);
        Bits x(n); vector<char> ref(n, 0);
        for (int i = 0; i < n; i++) if (rnd(0, 3) == 0) { x.set(i); ref[i] = 1; }
        assert(x.count() == count(ref.begin(), ref.end(), 1));
        for (int i = 0; i <= n; i++) {
            int expect = n; for (int j = i; j < n; j++) if (ref[j]) { expect = j; break; }
            assert(x.find_next(i) == expect);
        }
        int cnt = 0; for (int i = x.find_next(0); i < n; i = x.find_next(i + 1)) { assert(ref[i]); cnt++; }
        assert(cnt == x.count());
    }
}

// ───────────────────────── 7. Meet in the middle ────────────────────────────
// Number of subsets with sum exactly x, n <= 40: split, enumerate 2^(n/2) sums
// per half, sort one side, binary-search (or two-pointer) for x - s.
// O(2^(n/2) * n). Enumerate sums incrementally: sums[m] = sums[m ^ lowbit] + a[ctz].
vector<ll> all_subset_sums(const vector<ll>& a) {
    int n = a.size(); vector<ll> s(1 << n); s[0] = 0;
    for (int m = 1; m < (1 << n); m++) s[m] = s[m & (m - 1)] + a[__builtin_ctz(m)];
    return s;
}
ll count_subsets_with_sum(const vector<ll>& a, ll x) {
    int n = a.size(), h = n / 2;
    vector<ll> L(a.begin(), a.begin() + h), R(a.begin() + h, a.end());
    vector<ll> sl = all_subset_sums(L), sr = all_subset_sums(R);
    sort(sr.begin(), sr.end());
    ll cnt = 0;
    for (ll s : sl) cnt += upper_bound(sr.begin(), sr.end(), x - s) - lower_bound(sr.begin(), sr.end(), x - s);
    return cnt;
}

void test_mitm() {
    for (int iter = 0; iter < 30; iter++) {
        int n = (int)rnd(1, 14); ll x = rnd(0, 30);
        vector<ll> a(n); for (auto& v : a) v = rnd(-5, 10);
        ll brute = 0;
        for (int m = 0; m < (1 << n); m++) { ll s = 0; for (int i = 0; i < n; i++) if (m >> i & 1) s += a[i]; brute += (s == x); }
        assert(count_subsets_with_sum(a, x) == brute);
    }
}

// ───────────────────────── 8. Overflow-safe multiplication mod m ────────────
ull mulmod128(ull a, ull b, ull m) { return (ull)((unsigned __int128)a * b % m); }

// Without __int128 (a, b < m < 2^63): compute q ≈ a*b/m in long double (64-bit
// mantissa on x86, error at most a few units), then a*b - q*m modulo 2^64 is the
// exact small remainder up to a multiple of m; fix the sign.
// PITFALL: on arm64 (Apple Silicon, some ARM judges) long double == double
// (53-bit mantissa) and this is only safe for m < 2^52. Check
// numeric_limits<long double>::digits; prefer __int128 when the judge has it.
ll mulmod_ld(ll a, ll b, ll m) {
    ll q = (ll)((long double)a * b / m);
    ll r = (ll)((ull)a * (ull)b - (ull)q * (ull)m);   // unsigned wrap is defined
    r %= m; if (r < 0) r += m;
    return r;
}

void test_mulmod() {
    const int LD_BITS = numeric_limits<long double>::digits;         // 64 on x86, 53 on arm64
    const ll  M_MAX   = LD_BITS >= 64 ? (1LL << 62) - 1 : (1LL << 51) - 1;
    for (int iter = 0; iter < 20000; iter++) {
        ll m = rnd(2, M_MAX);
        ll a = rnd(0, m - 1), b = rnd(0, m - 1);
        ll ref = (ll)mulmod128(a, b, m);
        assert(mulmod_ld(a, b, m) == ref);
        if (m < (1LL << 31)) assert(a * b % m == ref);
    }
    assert(mulmod_ld(3, 4, 5) == 2);
}

int main() {
    test_basic();      puts("basic bit ops         ok");
    test_submasks();   puts("submask/gray          ok");
    test_bitdp();      puts("bitmask DP            ok");
    test_sos();        puts("SOS DP                ok");
    test_xor_basis();  puts("xor basis             ok");
    test_bitset();     puts("bitset                ok");
    test_mitm();       puts("meet in the middle    ok");
    test_mulmod();     puts("mulmod                ok");
    puts("all tests passed");
    return 0;
}
