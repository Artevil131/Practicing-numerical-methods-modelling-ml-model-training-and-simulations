// Chapter 16 — Game theory, constructive problems, randomization and contest misc: reference library.
//
// Compile:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// TRAINING RULE: this is your snippet library. Read once, close, re-type from memory.
// The things you must be able to produce in under 5 minutes each: Nim xor + winning move,
// Grundy numbers by mex, retrograde analysis on a game graph, misere Nim rule, staircase Nim,
// Zobrist/random hashing of sets, meet-in-the-middle subset sums, Gray code, alpha-beta skeleton,
// and an interactive-binary-search skeleton with correct flushing.
//
// Every non-trivial claim is cross-checked in main() against brute-force minimax.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>
using namespace std;
typedef long long ll;
typedef unsigned long long ull;

// ============================================================================
// 1. Nim (Bouton) — normal play: last move wins.  Losing positions <=> xor of piles == 0.
// ============================================================================
// Proof sketch: (i) xor==0 -> every move changes exactly one pile, hence some bit of the xor
// -> xor != 0.  (ii) xor = s != 0 -> take the pile p with the top bit of s set; p ^ s < p, so
// reducing p to p ^ s is legal and makes the xor 0.  Terminal (all empty) has xor 0 = losing.
bool nimFirstWins(const vector<ll>& a) { ll x = 0; for (ll v : a) x ^= v; return x != 0; }
// Winning move: returns {pile index, new size}, or {-1,-1} when losing.
pair<int, ll> nimMove(const vector<ll>& a) {
    ll s = 0; for (ll v : a) s ^= v;
    if (!s) return {-1, -1};
    for (int i = 0; i < (int)a.size(); i++) if ((a[i] ^ s) < a[i]) return {i, a[i] ^ s};
    return {-1, -1};   // unreachable
}

// Misere Nim (last move LOSES): identical to normal Nim unless every pile is <= 1;
// then the first player wins iff the number of 1-piles is EVEN.
bool misereNimFirstWins(const vector<ll>& a) {
    bool allSmall = true; ll x = 0; int ones = 0;
    for (ll v : a) { if (v > 1) allSmall = false; x ^= v; ones += v == 1; }
    return allSmall ? ones % 2 == 0 : x != 0;
}

// Staircase Nim (CSES "Stair Game" shape): balls on stairs 1..n, a move takes >=1 balls from
// stair k>=2 to stair k-1; balls on stair 1 are dead. Only stairs at ODD distance from the
// sink matter (stairs 2,4,6,... when the sink is stair 1): xor of those counts.
// Reason: a move on an even-distance stair can be mirrored (push the same balls one further),
// a move on an odd-distance stair is exactly a Nim move on that pile.
bool stairGameFirstWins(const vector<ll>& balls) {   // balls[0] = stair 1 (sink), balls[1] = stair 2, ...
    ll x = 0; for (int i = 1; i < (int)balls.size(); i += 2) x ^= balls[i]; return x != 0;
}

// "Another Game" (CSES 2208): take one coin from any non-empty SUBSET of heaps. First player
// wins iff some heap is odd  (all-even -> every move creates an odd heap; some-odd -> take one
// from every odd heap and hand back an all-even position).
bool anotherGameFirstWins(const vector<ll>& a) { for (ll v : a) if (v & 1) return true; return false; }

// ============================================================================
// 2. Sprague–Grundy: Grundy numbers via mex; a sum of games has grundy = xor of components.
// ============================================================================
int mex(vector<int> v) {          // smallest non-negative integer not in v
    sort(v.begin(), v.end()); int m = 0;
    for (int x : v) { if (x == m) m++; else if (x > m) break; }
    return m;
}
// Subtraction game: from a pile of n you may remove any amount in S. grundy[0] = 0.
// ("Stick Game" and "Nim Game II"-style single-pile rules reduce to this.)
vector<int> grundySubtraction(int N, const vector<int>& S) {
    vector<int> g(N + 1, 0);
    for (int n = 1; n <= N; n++) {
        vector<int> opts;
        for (int s : S) if (s <= n) opts.push_back(g[n - s]);
        g[n] = mex(opts);
    }
    return g;
}
// Grundy's Game: split one pile into two UNEQUAL non-empty piles; a pile of size <= 2 is dead.
// grundy(n) = mex over a+b=n, a<b of grundy(a) ^ grundy(b).  O(n^2) — fine to n ~ 3000.
vector<int> grundyGrundysGame(int N) {
    vector<int> g(N + 1, 0);
    for (int n = 3; n <= N; n++) {
        vector<int> opts;
        for (int a = 1; 2 * a < n; a++) opts.push_back(g[a] ^ g[n - a]);
        g[n] = mex(opts);
    }
    return g;
}

// ============================================================================
// 3. Retrograde analysis on a game graph: WIN / LOSE / DRAW for every node, O(V+E)
// ============================================================================
// Position u, mover picks an out-edge. No out-edge = mover loses. Cycles => draws possible.
// Reverse-BFS from terminal losers: a node is WIN if ANY successor is LOSE; LOSE if ALL
// successors are WIN (tracked with a remaining out-degree counter). Untouched nodes are DRAW.
enum Result { DRAW = 0, WIN = 1, LOSE = 2 };
vector<int> retrograde(const vector<vector<int>>& adj) {
    int n = (int)adj.size();
    vector<vector<int>> radj(n); vector<int> deg(n), res(n, DRAW);
    for (int u = 0; u < n; u++) { deg[u] = (int)adj[u].size(); for (int v : adj[u]) radj[v].push_back(u); }
    vector<int> q;
    for (int u = 0; u < n; u++) if (deg[u] == 0) { res[u] = LOSE; q.push_back(u); }
    for (size_t i = 0; i < q.size(); i++) {
        int v = q[i];
        for (int u : radj[v]) {
            if (res[u] != DRAW) continue;
            if (res[v] == LOSE) { res[u] = WIN; q.push_back(u); }             // u can move to a losing position
            else if (--deg[u] == 0) { res[u] = LOSE; q.push_back(u); }        // every successor of u is winning
        }
    }
    return res;
}

// ============================================================================
// 4. Minimax with alpha-beta pruning (partisan / scored games)
// ============================================================================
// Generic skeleton over an explicit tree: leaf values, internal nodes alternate max/min.
struct GameTree { vector<vector<int>> ch; vector<ll> leafVal; };
ll minimax(const GameTree& t, int u, bool maxTurn) {
    if (t.ch[u].empty()) return t.leafVal[u];
    ll best = maxTurn ? LLONG_MIN : LLONG_MAX;
    for (int v : t.ch[u]) { ll r = minimax(t, v, !maxTurn); best = maxTurn ? max(best, r) : min(best, r); }
    return best;
}
ll alphabeta(const GameTree& t, int u, bool maxTurn, ll alpha, ll beta, ll& visited) {
    visited++;
    if (t.ch[u].empty()) return t.leafVal[u];
    if (maxTurn) {
        ll best = LLONG_MIN;
        for (int v : t.ch[u]) { best = max(best, alphabeta(t, v, false, alpha, beta, visited)); alpha = max(alpha, best); if (alpha >= beta) break; }
        return best;
    } else {
        ll best = LLONG_MAX;
        for (int v : t.ch[u]) { best = min(best, alphabeta(t, v, true, alpha, beta, visited)); beta = min(beta, best); if (alpha >= beta) break; }
        return best;
    }
}

// ============================================================================
// 5. Constructive formulas (CSES Introductory) — each verified against brute force below
// ============================================================================
// Gray code: g(i) = i ^ (i >> 1); consecutive codes differ in exactly one bit.
vector<ull> grayCode(int n) { vector<ull> g(1ULL << n); for (ull i = 0; i < g.size(); i++) g[i] = i ^ (i >> 1); return g; }
// "Permutations": arrange 1..n so that adjacent numbers never differ by 1. Evens first, then odds.
// Impossible only for n = 2, 3.
vector<int> beautifulPermutation(int n) {
    if (n == 2 || n == 3) return {};
    vector<int> p; for (int i = 2; i <= n; i += 2) p.push_back(i); for (int i = 1; i <= n; i += 2) p.push_back(i);
    return p;
}
// "Two Knights": ways to place two knights on a k x k board so they don't attack:
// C(k^2, 2) - 4 (k-1)(k-2)   [each 2x3 / 3x2 sub-rectangle holds exactly 2 attacking pairs;
// there are (k-1)(k-2) placements of each orientation].
ll twoKnights(ll k) { ll c = k * k; return c * (c - 1) / 2 - 4 * (k - 1) * (k - 2); }
// "Number Spiral": value at row y, column x (1-indexed). Layer m = max(x,y); the layer holds
// numbers (m-1)^2+1 .. m^2. Even layers descend along the top row, odd layers along the left column.
ll numberSpiral(ll y, ll x) {
    ll m = max(x, y), base = (m - 1) * (m - 1);
    if (m % 2 == 0) return (y == m) ? base + x : m * m - (y - 1);        // even m: row m increasing in x
    else            return (x == m) ? base + y : m * m - (x - 1);        // odd m:  column m increasing in y
}

// ============================================================================
// 6. Randomization
// ============================================================================
// Seed from the clock so anti-hash / anti-quicksort tests cannot target a fixed seed.
mt19937_64 rng((ull)chrono::steady_clock::now().time_since_epoch().count());
ll rnd(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rng); }

// Zobrist hashing: h(set) = xor of random 64-bit keys of its elements -> O(1) update on
// insert/erase, set equality with collision probability ~ 2^-64 per comparison.
// Multisets: use SUM (mod 2^64) instead of xor so duplicates count.
struct ZobristSet {
    unordered_map<ll, ull>& key; ull h = 0;
    explicit ZobristSet(unordered_map<ll, ull>& k) : key(k) {}
    ull keyOf(ll v) { auto it = key.find(v); if (it == key.end()) it = key.emplace(v, rng()).first; return it->second; }
    void toggle(ll v) { h ^= keyOf(v); }         // insert or erase (set semantics)
};
// Randomized quickselect: expected O(n), the random pivot defeats adversarial inputs.
ll quickselect(vector<ll> a, int k) {           // k-th smallest, 0-indexed
    int lo = 0, hi = (int)a.size() - 1;
    while (lo < hi) {
        ll piv = a[rnd(lo, hi)]; int i = lo, j = hi;
        while (i <= j) { while (a[i] < piv) i++; while (a[j] > piv) j--; if (i <= j) swap(a[i++], a[j--]); }
        if (k <= j) hi = j; else if (k >= i) lo = i; else return piv;
    }
    return a[lo];
}
// Freivalds' check: is A*B == C ?  Pick random vector r; test A(Br) == Cr in O(n^2).
// Wrong answer slips through with probability <= 1/2 per round (Schwartz–Zippel flavour); repeat.
bool freivalds(const vector<vector<ll>>& A, const vector<vector<ll>>& B, const vector<vector<ll>>& C, int rounds = 20) {
    int n = (int)A.size(); const ll MOD = (1LL << 61) - 1;
    for (int t = 0; t < rounds; t++) {
        vector<ll> r(n), Br(n, 0), ABr(n, 0), Cr(n, 0);
        for (auto& v : r) v = rnd(0, MOD - 1);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) Br[i] = (Br[i] + (__int128)B[i][j] * r[j]) % MOD;
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { ABr[i] = (ABr[i] + (__int128)A[i][j] * Br[j]) % MOD; Cr[i] = (Cr[i] + (__int128)C[i][j] * r[j]) % MOD; }
        if (ABr != Cr) return false;
    }
    return true;
}

// ============================================================================
// 7. Meet in the middle: count subsets with sum == target, O(2^(n/2) * n)
// ============================================================================
ll countSubsetSum(const vector<ll>& a, ll target) {
    int n = (int)a.size(), h = n / 2;
    auto sums = [&](int lo, int hi) { vector<ll> s{0}; for (int i = lo; i < hi; i++) { int m = (int)s.size(); for (int j = 0; j < m; j++) s.push_back(s[j] + a[i]); } return s; };
    vector<ll> L = sums(0, h), R = sums(h, n);
    sort(R.begin(), R.end()); ll cnt = 0;
    for (ll x : L) cnt += upper_bound(R.begin(), R.end(), target - x) - lower_bound(R.begin(), R.end(), target - x);
    return cnt;
}

// ============================================================================
// 8. Interactive skeleton: binary search against a judge.  In a real contest the judge is
//    stdin/stdout; EVERY query must end with a flush:  cout << "? " << x << endl;  (endl flushes)
//    or  cout << ... << '\n' << flush;  Read the answer with cin >> s;  Never buffer queries.
// ============================================================================
ll interactiveGuess(ll lo, ll hi, const function<int(ll)>& ask, int& queries) {   // ask(x): -1 if secret < x, 0 if ==, +1 if >
    while (lo < hi) {
        ll mid = lo + (hi - lo) / 2; queries++;
        int r = ask(mid);                          // contest: cout << "? " << mid << endl; cin >> r;
        if (r == 0) return mid;
        if (r < 0) hi = mid - 1; else lo = mid + 1;
    }
    return lo;                                     // contest: cout << "! " << lo << endl;
}

// ============================================================================
// 9. Contest math helpers
// ============================================================================
ll floorDiv(ll a, ll b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }   // floor for negatives too
ll ceilDiv(ll a, ll b) { return -floorDiv(-a, b); }
// Harmonic-sum argument: sum_{i=1..n} floor(n/i) = O(n log n); the loop below runs that many steps.
ll harmonicWork(ll n) { ll w = 0; for (ll i = 1; i <= n; i++) w += n / i; return w; }

// ============================================================================
// Brute-force checkers
// ============================================================================
// Generic memoized win/lose for an impartial game given by a move generator on sorted pile vectors.
map<vector<ll>, bool> memoWin;
bool bruteWin(vector<ll> st, const function<vector<vector<ll>>(const vector<ll>&)>& moves) {
    sort(st.begin(), st.end());
    auto it = memoWin.find(st); if (it != memoWin.end()) return it->second;
    bool win = false;
    for (auto& nx : moves(st)) if (!bruteWin(nx, moves)) { win = true; break; }
    return memoWin[st] = win;
}
vector<vector<ll>> nimMoves(const vector<ll>& s) {
    vector<vector<ll>> r;
    for (int i = 0; i < (int)s.size(); i++) for (ll k = 1; k <= s[i]; k++) { auto t = s; t[i] -= k; r.push_back(t); }
    return r;
}

int main() {
    // ---- Nim vs brute minimax ----
    for (int it = 0; it < 300; it++) {
        int n = (int)rnd(1, 3); vector<ll> a(n); for (auto& v : a) v = rnd(0, 5);
        memoWin.clear();
        assert(nimFirstWins(a) == bruteWin(a, nimMoves));
        auto [i, nv] = nimMove(a);
        if (nimFirstWins(a)) { assert(i >= 0 && nv < a[i]); auto b = a; b[i] = nv; assert(!nimFirstWins(b)); } else assert(i == -1);
    }
    assert(nimFirstWins({3, 4, 5}) && !nimFirstWins({1, 2, 3}) && !nimFirstWins({0}) && nimFirstWins({7}));

    // ---- Misere Nim vs brute (last move loses) ----
    for (int it = 0; it < 300; it++) {
        int n = (int)rnd(1, 3); vector<ll> a(n); for (auto& v : a) v = rnd(0, 4);
        memoWin.clear();
        // in misere play the player who faces an empty board has WON (the opponent took the last stick)
        function<bool(vector<ll>)> mis = [&](vector<ll> st) -> bool {
            sort(st.begin(), st.end());
            if (accumulate(st.begin(), st.end(), 0LL) == 0) return true;
            auto it2 = memoWin.find(st); if (it2 != memoWin.end()) return it2->second;
            bool win = false; for (auto& nx : nimMoves(st)) if (!mis(nx)) { win = true; break; }
            return memoWin[st] = win;
        };
        assert(misereNimFirstWins(a) == mis(a));
    }

    // ---- Staircase Nim vs brute ----
    for (int it = 0; it < 200; it++) {
        int n = (int)rnd(1, 4); vector<ll> st(n); for (auto& v : st) v = rnd(0, 3);
        memoWin.clear();
        auto moves = [](const vector<ll>& s) {   // NOT sorted here: position matters -> bypass sorting by a wrapper
            vector<vector<ll>> r;
            for (int i = 1; i < (int)s.size(); i++) for (ll k = 1; k <= s[i]; k++) { auto t = s; t[i] -= k; t[i - 1] += k; r.push_back(t); }
            return r;
        };
        map<vector<ll>, bool> memo;
        function<bool(const vector<ll>&)> win = [&](const vector<ll>& s) -> bool {
            auto f = memo.find(s); if (f != memo.end()) return f->second;
            bool w = false; for (auto& nx : moves(s)) if (!win(nx)) { w = true; break; }
            return memo[s] = w;
        };
        assert(stairGameFirstWins(st) == win(st));
    }

    // ---- Another Game vs brute ----
    for (int it = 0; it < 200; it++) {
        int n = (int)rnd(1, 3); vector<ll> a(n); for (auto& v : a) v = rnd(0, 4);
        memoWin.clear();
        auto moves = [](const vector<ll>& s) {
            vector<vector<ll>> r; int n = (int)s.size();
            for (int mask = 1; mask < (1 << n); mask++) {
                bool ok = true; auto t = s;
                for (int i = 0; i < n; i++) if (mask >> i & 1) { if (t[i] == 0) ok = false; else t[i]--; }
                if (ok) r.push_back(t);
            }
            return r;
        };
        assert(anotherGameFirstWins(a) == bruteWin(a, moves));
    }

    // ---- Grundy numbers of a subtraction game vs brute minimax; SG theorem on sums ----
    {
        vector<int> S = {1, 3, 4};
        vector<int> g = grundySubtraction(30, S);
        assert(g[0] == 0 && g[1] == 1 && g[2] == 0 && g[3] == 1 && g[4] == 2 && g[5] == 3 && g[6] == 2 && g[7] == 0);  // period 7: 0101232
        for (int n = 0; n <= 30; n++) assert(g[n] == g[n % 7]);
        auto moves = [&](const vector<ll>& s) {
            vector<vector<ll>> r;
            for (int i = 0; i < (int)s.size(); i++) for (int k : S) if (k <= s[i]) { auto t = s; t[i] -= k; r.push_back(t); }
            return r;
        };
        for (int n = 0; n <= 30; n++) { memoWin.clear(); assert((g[n] != 0) == bruteWin({n}, moves)); }
        for (int it = 0; it < 300; it++) {           // sums of 2-3 piles: xor of grundy values decides
            int m = (int)rnd(2, 3); vector<ll> a(m); int x = 0; for (auto& v : a) { v = rnd(0, 12); x ^= g[v]; }
            memoWin.clear(); assert((x != 0) == bruteWin(a, moves));
        }
        // Nim Game II shape: remove 1..3 sticks -> grundy = n mod 4
        vector<int> g3 = grundySubtraction(50, {1, 2, 3});
        for (int n = 0; n <= 50; n++) assert(g3[n] == n % 4);
    }
    // ---- Grundy's Game vs brute on multi-pile states ----
    {
        vector<int> gg = grundyGrundysGame(60);
        assert(gg[1] == 0 && gg[2] == 0 && gg[3] == 1 && gg[4] == 0 && gg[5] == 2 && gg[6] == 1 && gg[7] == 0 && gg[8] == 2);
        auto moves = [](const vector<ll>& s) {
            vector<vector<ll>> r;
            for (int i = 0; i < (int)s.size(); i++) for (ll a = 1; 2 * a < s[i]; a++) { auto t = s; t[i] = a; t.push_back(s[i] - a); r.push_back(t); }
            return r;
        };
        for (int n = 1; n <= 14; n++) { memoWin.clear(); assert((gg[n] != 0) == bruteWin({n}, moves)); }
        for (int it = 0; it < 100; it++) {
            int m = (int)rnd(2, 3); vector<ll> a(m); int x = 0; for (auto& v : a) { v = rnd(1, 9); x ^= gg[v]; }
            memoWin.clear(); assert((x != 0) == bruteWin(a, moves));
        }
    }

    // ---- Retrograde analysis on a tiny graph (with a cycle) vs hand analysis and DAG recursion ----
    {
        // 0->1, 0->2, 1->3, 2->3, 3 terminal; 4->5, 5->4 (cycle), 4->3, 6->4
        vector<vector<int>> adj = {{1, 2}, {3}, {3}, {}, {5, 3}, {4}, {4}};
        vector<int> r = retrograde(adj);
        assert(r[3] == LOSE && r[1] == WIN && r[2] == WIN && r[0] == LOSE);
        assert(r[4] == WIN);            // 4 can move to 3 (LOSE)
        assert(r[5] == LOSE);           // 5's only successor 4 is WIN -> deg hits 0 -> LOSE (the cycle does not save it)
        assert(r[6] == LOSE);           // same for 6
        // a true draw: 7 <-> 8 with no exit
        adj.push_back({8}); adj.push_back({7});
        r = retrograde(adj); assert(r[7] == DRAW && r[8] == DRAW);
        // random DAGs: compare with recursive DP
        for (int it = 0; it < 200; it++) {
            int n = (int)rnd(1, 9); vector<vector<int>> g(n);
            for (int u = 0; u < n; u++) for (int v = u + 1; v < n; v++) if (rnd(0, 2) == 0) g[u].push_back(v);
            vector<int> dp(n, -1);
            function<int(int)> f = [&](int u) { if (dp[u] != -1) return dp[u]; int res = LOSE; for (int v : g[u]) if (f(v) == LOSE) res = WIN; return dp[u] = res; };
            vector<int> rr = retrograde(g);
            for (int u = 0; u < n; u++) assert(rr[u] == f(u));
        }
    }

    // ---- alpha-beta == minimax, fewer visits ----
    for (int it = 0; it < 100; it++) {
        GameTree t; int n = 1; t.ch.push_back({}); t.leafVal.push_back(0);
        vector<int> frontier = {0};
        for (int depth = 0; depth < 4; depth++) {
            vector<int> nf;
            for (int u : frontier) for (int k = 0; k < 3; k++) { t.ch.push_back({}); t.leafVal.push_back(rnd(-50, 50)); t.ch[u].push_back(n); nf.push_back(n); n++; }
            frontier = nf;
        }
        ll visited = 0;
        assert(alphabeta(t, 0, true, LLONG_MIN, LLONG_MAX, visited) == minimax(t, 0, true));
        assert(visited <= n);
    }

    // ---- constructive formulas vs brute ----
    for (int n = 1; n <= 10; n++) {
        auto g = grayCode(n);
        set<ull> seen(g.begin(), g.end()); assert(seen.size() == g.size());
        for (size_t i = 0; i + 1 < g.size(); i++) assert(__builtin_popcountll(g[i] ^ g[i + 1]) == 1);
    }
    for (int n = 1; n <= 12; n++) {
        auto p = beautifulPermutation(n);
        if (n == 2 || n == 3) { assert(p.empty()); continue; }
        assert((int)p.size() == n);
        vector<int> s = p; sort(s.begin(), s.end()); for (int i = 0; i < n; i++) assert(s[i] == i + 1);
        for (int i = 0; i + 1 < n; i++) assert(abs(p[i] - p[i + 1]) != 1);
    }
    for (int k = 1; k <= 7; k++) {
        ll cnt = 0; int c = k * k;
        for (int a = 0; a < c; a++) for (int b = a + 1; b < c; b++) {
            int dx = abs(a / k - b / k), dy = abs(a % k - b % k);
            if (!((dx == 1 && dy == 2) || (dx == 2 && dy == 1))) cnt++;
        }
        assert(twoKnights(k) == cnt);
    }
    {   // build the spiral by simulation: layer m fills column m (rows 1..m) and row m (cols 1..m)
        const int N = 8; ll grid[N + 1][N + 1]; ll v = 1;
        for (int m = 1; m <= N; m++) {
            if (m % 2 == 1) { for (int y = 1; y <= m; y++) grid[y][m] = v++; for (int x = m - 1; x >= 1; x--) grid[m][x] = v++; }
            else            { for (int x = 1; x <= m; x++) grid[m][x] = v++; for (int y = m - 1; y >= 1; y--) grid[y][m] = v++; }
        }
        assert(grid[1][1] == 1 && grid[2][1] == 2 && grid[2][2] == 3 && grid[1][2] == 4 && grid[1][3] == 5 && grid[3][3] == 7 && grid[3][1] == 9);
        for (int y = 1; y <= N; y++) for (int x = 1; x <= N; x++) assert(numberSpiral(y, x) == grid[y][x]);
    }

    // ---- randomization ----
    {
        unordered_map<ll, ull> keys; ZobristSet A(keys), B(keys);
        for (ll v : {5, 9, 12}) A.toggle(v);
        for (ll v : {12, 5, 9}) B.toggle(v);
        assert(A.h == B.h); B.toggle(7); assert(A.h != B.h); B.toggle(7); assert(A.h == B.h);
        for (int it = 0; it < 100; it++) {
            int n = (int)rnd(1, 30); vector<ll> a(n); for (auto& x : a) x = rnd(-20, 20);
            int k = (int)rnd(0, n - 1); vector<ll> s = a; sort(s.begin(), s.end());
            assert(quickselect(a, k) == s[k]);
        }
        int n = 5; vector<vector<ll>> A2(n, vector<ll>(n)), B2 = A2, C2 = A2;
        for (auto& r : A2) for (auto& x : r) x = rnd(0, 100);
        for (auto& r : B2) for (auto& x : r) x = rnd(0, 100);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) for (int k = 0; k < n; k++) C2[i][j] += A2[i][k] * B2[k][j];
        assert(freivalds(A2, B2, C2)); C2[2][3] += 1; assert(!freivalds(A2, B2, C2));
    }

    // ---- meet in the middle vs brute ----
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(1, 12); vector<ll> a(n); for (auto& x : a) x = rnd(-5, 9); ll target = rnd(-5, 20);
        ll b = 0; for (int m = 0; m < (1 << n); m++) { ll s = 0; for (int i = 0; i < n; i++) if (m >> i & 1) s += a[i]; b += s == target; }
        assert(countSubsetSum(a, target) == b);
    }

    // ---- interactive skeleton: log2 queries ----
    for (int it = 0; it < 100; it++) {
        ll secret = rnd(1, 1000000); int q = 0;
        ll got = interactiveGuess(1, 1000000, [&](ll x) { return secret < x ? -1 : secret > x ? 1 : 0; }, q);
        assert(got == secret && q <= 21);
    }

    // ---- contest math ----
    assert(floorDiv(7, 2) == 3 && floorDiv(-7, 2) == -4 && ceilDiv(7, 2) == 4 && ceilDiv(-7, 2) == -3);
    for (ll n : {10LL, 1000LL, 100000LL}) assert(harmonicWork(n) <= (ll)(n * (log((double)n) + 1)));
    // sum_{i=1}^{n} i = n(n+1)/2 ; sum i^2 = n(n+1)(2n+1)/6
    for (ll n = 1; n <= 50; n++) { ll s = 0, s2 = 0; for (ll i = 1; i <= n; i++) { s += i; s2 += i * i; } assert(s == n * (n + 1) / 2 && s2 == n * (n + 1) * (2 * n + 1) / 6); }

    puts("all game-theory / constructive / misc tests passed");
    return 0;
}
