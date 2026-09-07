// Chapter 01 — Contest Fundamentals: reference snippet library.
//
// RE-TYPE THIS FILE FROM MEMORY. That is the exercise. Every block below is something you will
// need in a contest with no internet and no notes: the template, fast input, the dbg() macro,
// overflow-safe modular arithmetic, a hack-proof hash, bit builtins, bitset tricks, both forms
// of recursive lambda, and an in-process stress test (brute force vs fast solution on random
// data). main() asserts that each piece behaves as claimed.
//
// Build & run (must be warning-free):
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//   c++ -Wall -Wextra -std=c++17 -O2 -DLOCAL -o ex_demo example.cpp && ./ex_demo   # dbg() on
//
// Explicit standard headers are used instead of <bits/stdc++.h> so the file compiles with Apple
// clang out of the box. In submissions (GCC judges) you write the one-liner.
// libstdc++-only features (pbds, bitset::_Find_next) are guarded by #ifdef __GLIBCXX__.

#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef __GLIBCXX__
#include <ext/pb_ds/assoc_container.hpp>
#endif
using namespace std;

// ───────────────────────────── 1. Template pieces ────────────────────────────────────────
using ll = long long;
using ull = unsigned long long;
#define all(x) begin(x), end(x)
const int INF = 1e9;         // fits int; INF + INF fits int too (2e9 < 2^31-1)
const ll LINF = 1e18;        // LINF + LINF fits ll; 4e18 would not
const ll MOD = 1000000007;

// One global RNG, seeded from the clock. NEVER rand(): RAND_MAX is 32767 on Windows judges.
mt19937_64 rng((ull)chrono::steady_clock::now().time_since_epoch().count());
ll rnd(ll lo, ll hi) { return (ll)(rng() % (ull)(hi - lo + 1)) + lo; }  // uniform in [lo, hi]

// ───────────────────────────── 2. Fast input ─────────────────────────────────────────────
// fread-based reader: ~10x faster than cin without sync, ~5x faster than scanf. Reads from
// `in` (stdin in contests). Returns false at EOF. Handles a leading '-'.
struct FastReader {
    static const int BUF = 1 << 16;
    char buf[BUF];
    int len = 0, pos = 0;
    FILE* in;
    explicit FastReader(FILE* f = stdin) : in(f) {}
    int gc() {
        if (pos == len) {
            len = (int)fread(buf, 1, BUF, in);
            pos = 0;
            if (len == 0) return -1;
        }
        return buf[pos++];
    }
    bool readInt(ll& x) {
        int c = gc();
        while (c != '-' && (c < '0' || c > '9')) {
            if (c == -1) return false;
            c = gc();
        }
        bool neg = false;
        if (c == '-') { neg = true; c = gc(); }
        x = 0;
        while (c >= '0' && c <= '9') { x = x * 10 + (c - '0'); c = gc(); }
        if (neg) x = -x;
        return true;
    }
};

// ───────────────────────────── 3. dbg() macro ────────────────────────────────────────────
// Compiled out unless -DLOCAL. Prints "[expr1, expr2] = v1, v2" to stderr.
// Supports scalars, pairs, strings and anything iterable (nested too).
template <class T> void dbg_print(const T& x);
template <class A, class B> void dbg_print(const pair<A, B>& p) {
    cerr << '('; dbg_print(p.first); cerr << ", "; dbg_print(p.second); cerr << ')';
}
template <class T, class = void> struct is_iterable : false_type {};
template <class T>
struct is_iterable<T, void_t<decltype(begin(declval<T&>())), decltype(end(declval<T&>()))>>
    : true_type {};
template <class T> void dbg_print(const T& x) {
    if constexpr (is_iterable<T>::value && !is_same<T, string>::value) {
        cerr << '{';
        bool first = true;
        for (auto& e : x) { if (!first) cerr << ", "; first = false; dbg_print(e); }
        cerr << '}';
    } else {
        cerr << x;
    }
}
void dbg_out() { cerr << '\n'; }
template <class H, class... T> void dbg_out(const H& h, const T&... t) {
    dbg_print(h);
    if (sizeof...(t)) cerr << ", ";
    dbg_out(t...);
}
#ifdef LOCAL
#define dbg(...) (cerr << "[" << #__VA_ARGS__ << "] = ", dbg_out(__VA_ARGS__))
#else
#define dbg(...) ((void)0)
#endif

// ───────────────────────────── 4. Overflow-safe integer arithmetic ────────────────────────
ll normmod(ll x, ll m) { return ((x % m) + m) % m; }             // C++ % truncates toward 0
ll mulmod(ll a, ll b, ll m) { return (ll)((__int128)a * b % m); }  // safe for m up to ~9e18
ll powmod(ll b, ll e, ll m) {                                       // O(log e)
    ll r = 1 % m;
    b = normmod(b, m);
    while (e > 0) {
        if (e & 1) r = mulmod(r, b, m);
        b = mulmod(b, b, m);
        e >>= 1;
    }
    return r;
}
// Print an __int128 (no iostream support for it). Only needed when the answer itself is huge.
string to_string128(__int128 x) {
    if (x == 0) return "0";
    bool neg = x < 0;
    if (neg) x = -x;
    string s;
    while (x > 0) { s.push_back(char('0' + (int)(x % 10))); x /= 10; }
    if (neg) s.push_back('-');
    reverse(all(s));
    return s;
}

// ───────────────────────────── 5. Hack-proof hashing ─────────────────────────────────────
// splitmix64: a bijective mixer; the default libstdc++ hash for integers is the identity, and
// anti-hash tests on Codeforces put 2e5 keys into one bucket. Random per-run offset defeats
// tests prepared against a fixed function.
struct SplitMix64 {
    static ull splitmix64(ull x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
    size_t operator()(ull x) const {
        static const ull FIXED = (ull)chrono::steady_clock::now().time_since_epoch().count();
        return (size_t)splitmix64(x + FIXED);
    }
};
// usage: unordered_map<ll, int, SplitMix64> cnt;   (also works as gp_hash_table's hash)

// Polynomial string hash modulo 2^61-1 (a Mersenne prime): fast reduction, no known anti-tests
// unless the base is fixed — so the base is random. Compare with hashing mod 2^64 via unsigned
// wraparound (defined behaviour, but broken by the Thue–Morse construction).
struct StrHash {
    static const ull M = (1ULL << 61) - 1;
    static ull mulM(ull a, ull b) {                     // (a*b) mod (2^61-1) without __int128 division
        __int128 p = (__int128)a * b;
        ull lo = (ull)(p & M), hi = (ull)(p >> 61);
        ull r = lo + hi;
        if (r >= M) r -= M;
        return r;
    }
    ull B;
    vector<ull> h, pw;                                 // h[i] = hash of s[0..i), pw[i] = B^i
    explicit StrHash(const string& s) : B(rnd(300, (ll)1e9)), h(s.size() + 1, 0), pw(s.size() + 1, 1) {
        for (size_t i = 0; i < s.size(); i++) {
            h[i + 1] = (mulM(h[i], B) + (ull)s[i]) % M;
            pw[i + 1] = mulM(pw[i], B);
        }
    }
    ull get(int l, int r) const {                      // hash of s[l..r)
        ull v = h[r] + M - mulM(h[l], pw[r - l]);
        return v >= M ? v - M : v;
    }
};

// ───────────────────────────── 6. Bit builtins ───────────────────────────────────────────
int popcnt(ull x) { return __builtin_popcountll(x); }
int lowbit_index(ull x) { assert(x != 0); return __builtin_ctzll(x); }      // UB for x == 0
int floor_log2(ull x) { assert(x != 0); return 63 - __builtin_clzll(x); }   // UB for x == 0
bool is_pow2(ull x) { return x && !(x & (x - 1)); }

// ───────────────────────────── 7. Recursive lambdas ──────────────────────────────────────
// Both compute the subtree sizes of a tree given as adjacency lists.
vector<int> subtree_sizes_stdfunction(const vector<vector<int>>& adj, int root) {
    vector<int> sz(adj.size(), 1);
    function<void(int, int)> dfs = [&](int v, int p) {   // type-erased: readable, 2-3x slower
        for (int u : adj[v]) if (u != p) { dfs(u, v); sz[v] += sz[u]; }
    };
    dfs(root, -1);
    return sz;
}
vector<int> subtree_sizes_self(const vector<vector<int>>& adj, int root) {
    vector<int> sz(adj.size(), 1);
    auto dfs = [&](auto&& self, int v, int p) -> void {  // zero-overhead; explicit return type
        for (int u : adj[v]) if (u != p) { self(self, u, v); sz[v] += sz[u]; }
    };
    dfs(dfs, root, -1);
    return sz;
}

// ───────────────────────────── 8. Stress-test pattern (in-process) ───────────────────────
// Problem: maximum subarray sum. Fast = Kadane O(n); brute = all O(n^2) subarrays.
// In a contest these are two programs + gen + a bash loop (lesson §11); the logic is identical.
ll max_subarray_fast(const vector<ll>& a) {
    ll best = a[0], cur = 0;
    for (ll x : a) { cur = max(cur + x, x); best = max(best, cur); }
    return best;
}
ll max_subarray_brute(const vector<ll>& a) {
    ll best = LLONG_MIN;
    for (size_t i = 0; i < a.size(); i++) {
        ll s = 0;
        for (size_t j = i; j < a.size(); j++) { s += a[j]; best = max(best, s); }
    }
    return best;
}

// ───────────────────────────── 9. Timer ──────────────────────────────────────────────────
struct Timer {
    chrono::steady_clock::time_point t0 = chrono::steady_clock::now();
    double ms() const {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
    }
};

// ═════════════════════════════════════ tests ═════════════════════════════════════════════
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ── 2. fast reader on a temp file (stdin stands in for it in contests)
    {
        FILE* f = tmpfile();
        assert(f);
        fputs("3\n-17 42\n  1000000000000 \n", f);
        rewind(f);
        FastReader fr(f);
        ll v;
        vector<ll> got;
        while (fr.readInt(v)) got.push_back(v);
        fclose(f);
        assert((got == vector<ll>{3, -17, 42, 1000000000000LL}));
    }

    // ── 3. dbg() — visible only with -DLOCAL; must compile either way
    {
        vector<pair<int, string>> vp = {{1, "a"}, {2, "b"}};
        vector<vector<int>> vv = {{1, 2}, {3}};
        dbg(vp, vv, string("plain"), 42);
    }

    // ── 4. overflow discipline
    {
        int a = 100000, b = 100000;
        ll wrong_way_would_overflow = (ll)a * b;       // the right way; a*b as int would be UB
        assert(wrong_way_would_overflow == 10000000000LL);
        assert(INF + INF > 0 && LINF + LINF > 0);      // sentinels chosen so that sums don't overflow
        assert(normmod(-7, 5) == 3);                   // C++ gives -7 % 5 == -2
        assert(-7 % 5 == -2);
        assert(mulmod(MOD - 1, MOD - 1, MOD) == 1);    // (M-1)^2 ≡ (-1)^2
        ll big = (1LL << 62) + 12345;                  // ~4.6e18: plain a*b%m would overflow
        assert(mulmod(big, big, big + 1) == 1);        // big ≡ -1 (mod big+1)
        assert(powmod(2, 10, MOD) == 1024);
        assert(powmod(2, MOD - 1, MOD) == 1);          // Fermat
        assert(powmod(3, 0, 1) == 0);                  // everything is 0 mod 1
        __int128 p = (__int128)LLONG_MAX * LLONG_MAX;
        assert(to_string128(p) == "85070591730234615847396907784232501249");
        assert(to_string128(-5) == "-5" && to_string128(0) == "0");
        assert((1LL << 40) == 1099511627776LL);        // 1 << 40 with int would be UB
        // unsigned wraparound is defined: exactly mod 2^64
        ull u = ULLONG_MAX; u += 2;
        assert(u == 1);
    }

    // ── 5. hashing
    {
        unordered_map<ll, int, SplitMix64> cnt;
        for (int i = 0; i < 1000; i++) cnt[(ll)i * 1000003] += 1;
        assert((int)cnt.size() == 1000 && cnt[999 * 1000003LL] == 1);

        string s = "abracadabra";
        StrHash H(s);
        assert(H.get(0, 4) == H.get(7, 11));           // "abra" == "abra"
        assert(H.get(0, 4) != H.get(1, 5));            // "abra" != "brac" (overwhelmingly likely)
        assert(H.get(3, 3) == 0);                      // empty substring
        // brute check: equal substrings ⇔ equal hashes on all pairs of length 3
        for (int i = 0; i + 3 <= (int)s.size(); i++)
            for (int j = 0; j + 3 <= (int)s.size(); j++)
                assert((s.substr(i, 3) == s.substr(j, 3)) == (H.get(i, i + 3) == H.get(j, j + 3)));
    }

    // ── 6. builtins vs naive loops
    {
        for (int it = 0; it < 1000; it++) {
            ull x = rng();
            if (x == 0) continue;
            int pc = 0, ctz = 0, lg = -1;
            for (int i = 0; i < 64; i++) { if ((x >> i) & 1) { pc++; lg = i; } }
            while (!((x >> ctz) & 1)) ctz++;
            assert(popcnt(x) == pc && lowbit_index(x) == ctz && floor_log2(x) == lg);
            assert((x & -x) == (1ULL << ctz));
        }
        assert(is_pow2(1) && is_pow2(1ULL << 63) && !is_pow2(0) && !is_pow2(12));
    }

    // ── 7. bitset: subset sums 64x faster than a bool DP
    {
        vector<int> w = {3, 5, 11, 2, 8};
        const int S = 64;
        bitset<S> dp; dp[0] = 1;
        vector<char> ok(S, 0); ok[0] = 1;
        for (int x : w) {
            dp |= dp << x;                              // all sums reachable using x once more
            for (int s = S - 1; s >= x; s--) ok[s] |= ok[s - x];
        }
        for (int s = 0; s < S; s++) assert(dp[s] == (bool)ok[s]);
        assert(dp.count() == (size_t)count(all(ok), 1));
#ifdef __GLIBCXX__
        // libstdc++ extension: iterate set bits in O(popcount) word steps
        int seen = 0;
        for (size_t i = dp._Find_first(); i < S; i = dp._Find_next(i)) { assert(dp[i]); seen++; }
        assert(seen == (int)dp.count());
#endif
    }

    // ── 8. pbds ordered set (GCC only)
#ifdef __GLIBCXX__
    {
        using namespace __gnu_pbds;
        tree<int, null_type, less<int>, rb_tree_tag, tree_order_statistics_node_update> os;
        for (int x : {5, 1, 9, 3}) os.insert(x);
        assert(os.order_of_key(4) == 2);               // {1,3} are < 4
        assert(*os.find_by_order(2) == 5);             // 0-indexed k-th smallest
    }
#endif

    // ── 9. floating point
    {
        double x = 0.1 + 0.2;
        assert(x != 0.3);                              // the reason for EPS
        assert(fabs(x - 0.3) < 1e-9);
        char buf[64];
        snprintf(buf, sizeof buf, "%.6f", 2.0 / 3.0);
        assert(string(buf) == "0.666667");
        // long double is 80-bit on x86 Linux judges, 64-bit on Apple Silicon and MSVC:
        cout << "sizeof(long double) = " << sizeof(long double) << " bytes ("
             << (sizeof(long double) > 8 ? "extended" : "same as double") << ")\n";
        cout << fixed << setprecision(10) << 1.0 / 7 << '\n';   // contest-style output
    }

    // ── 10. recursive lambdas agree
    {
        //      0
        //    / | \
        //   1  2  3
        //  / \
        // 4   5
        vector<vector<int>> adj(6);
        auto add = [&](int a, int b) { adj[a].push_back(b); adj[b].push_back(a); };
        add(0, 1); add(0, 2); add(0, 3); add(1, 4); add(1, 5);
        vector<int> expect = {6, 3, 1, 1, 1, 1};
        assert(subtree_sizes_stdfunction(adj, 0) == expect);
        assert(subtree_sizes_self(adj, 0) == expect);
    }

    // ── 11. in-process stress test: small n, small values, many seeds
    {
        for (int seed = 1; seed <= 2000; seed++) {
            mt19937 g(seed);                           // reproducible per seed, like gen.cpp
            int n = (int)(g() % 6) + 1;
            vector<ll> a(n);
            for (auto& v : a) v = (ll)(g() % 21) - 10;
            ll f = max_subarray_fast(a), b = max_subarray_brute(a);
            if (f != b) {
                dbg(seed, a, f, b);
                assert(false && "mismatch — the input above is your 6-element failing case");
            }
        }
    }

    // ── 12. the 1e8 budget, measured
    {
        Timer t;
        volatile ll sink = 0;                          // volatile: keep the loop honest
        ll s = 0;
        for (int i = 0; i < 100000000; i++) s += i & 7;
        sink = s;
        (void)sink;
        cout << "1e8 simple ops: " << (int)t.ms() << " ms (judge ~1s budget ≈ this × 5–10)\n";
    }

    // ── 13. rnd() range sanity
    {
        for (int i = 0; i < 1000; i++) { ll v = rnd(-3, 3); assert(v >= -3 && v <= 3); }
    }

    cout << "all chapter 01 checks passed\n";
    return 0;
}
