// Chapter 05 — Combinatorics and Probability: reference library + self-tests.
//
// TRAINING RULE: you must be able to re-type every snippet in this file from memory.
// Close the file, write `power`, `Comb`, `lucas`, `ballot`, `necklaces`, `stirling2`,
// `partitions`, `grid_paths_with_obstacles`, `fib` on a blank page, then diff against this.
//
// Build & run:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
// (on macOS/libc++ add  -I ../include  so that <bits/stdc++.h> resolves to the local shim)
// Every section is cross-checked against a brute force on small inputs; the program prints
// "all tests passed" if every assert holds.

#include <bits/stdc++.h>
using namespace std;
using ll = long long;

const ll MOD = 1'000'000'007;

// ---------------------------------------------------------------------------------------------
// 1. Modular basics
// ---------------------------------------------------------------------------------------------
ll power(ll b, ll e, ll m = MOD) {
    ll r = 1;
    b %= m; if (b < 0) b += m;
    for (; e > 0; e >>= 1, b = b * b % m) if (e & 1) r = r * b % m;
    return r;
}
ll inv(ll a, ll m = MOD) { return power(a, m - 2, m); }          // m prime, a != 0 mod m
ll norm(ll a, ll m = MOD) { a %= m; return a < 0 ? a + m : a; }   // fix negative remainders

// ---------------------------------------------------------------------------------------------
// 2. Binomial coefficients: factorial tables (default), Pascal (any modulus), Lucas (p <= n),
//    falling product (huge n, small k)
// ---------------------------------------------------------------------------------------------
struct Comb {
    int N; vector<ll> f, fi;                    // f[i] = i!, fi[i] = (i!)^{-1}
    explicit Comb(int n) : N(n), f(n + 1), fi(n + 1) {
        f[0] = 1;
        for (int i = 1; i <= n; i++) f[i] = f[i - 1] * i % MOD;
        fi[n] = inv(f[n]);
        for (int i = n; i > 0; i--) fi[i - 1] = fi[i] * i % MOD;    // 1/(i-1)! = i / i!
    }
    ll C(int n, int k) const { return (k < 0 || k > n) ? 0 : f[n] * fi[k] % MOD * fi[n - k] % MOD; }
    ll P(int n, int k) const { return (k < 0 || k > n) ? 0 : f[n] * fi[n - k] % MOD; }
    ll invi(int i) const { return fi[i] * f[i - 1] % MOD; }              // 1/i for 1 <= i <= N
    ll catalan(int n) const { return C(2 * n, n) * invi(n + 1) % MOD; }
    ll multinomial(const vector<int>& c) const {
        int n = 0; ll r = 1;
        for (int x : c) { n += x; r = r * fi[x] % MOD; }
        return r * f[n] % MOD;
    }
    // stars and bars: non-negative solutions of x_1 + ... + x_k = n
    ll stars_and_bars(int n, int k) const { return C(n + k - 1, k - 1); }
};

vector<vector<ll>> pascal(int n, ll mod) {           // O(n^2), works for any modulus
    vector<vector<ll>> C(n + 1, vector<ll>(n + 1, 0));
    for (int i = 0; i <= n; i++) {
        C[i][0] = 1;
        for (int j = 1; j <= i; j++) C[i][j] = (C[i - 1][j - 1] + C[i - 1][j]) % mod;
    }
    return C;
}

// C(n,k) mod p for prime p, with factorial tables mod p of size p (so p must be small).
struct Lucas {
    ll p; vector<ll> f, fi;
    explicit Lucas(ll prime) : p(prime), f(prime), fi(prime) {
        f[0] = 1;
        for (ll i = 1; i < p; i++) f[i] = f[i - 1] * i % p;
        fi[p - 1] = power(f[p - 1], p - 2, p);
        for (ll i = p - 1; i > 0; i--) fi[i - 1] = fi[i] * i % p;
    }
    ll C(ll n, ll k) const {                        // digit-wise product, O(log_p n)
        if (k < 0 || k > n) return 0;
        ll r = 1;
        while (n > 0 || k > 0) {
            ll a = n % p, b = k % p;
            if (b > a) return 0;
            r = r * f[a] % p * fi[b] % p * fi[a - b] % p;
            n /= p; k /= p;
        }
        return r;
    }
};

// C(n,k) mod MOD for n up to ~1e18 and k small (k < MOD): product of k terms / k!
ll binom_small_k(ll n, int k) {
    if (k < 0 || k > n) return 0;
    ll num = 1, den = 1;
    for (int i = 0; i < k; i++) {
        num = num * norm(n - i) % MOD;
        den = den * (i + 1) % MOD;
    }
    return num * inv(den) % MOD;
}

// ---------------------------------------------------------------------------------------------
// 3. Inclusion–exclusion: derangements, surjections
// ---------------------------------------------------------------------------------------------
vector<ll> derangements(int n) {                     // D(0..n) via D(n) = (n-1)(D(n-1)+D(n-2))
    vector<ll> D(max(n + 1, 2));
    D[0] = 1; D[1] = 0;
    for (int i = 2; i <= n; i++) D[i] = (i - 1) * ((D[i - 1] + D[i - 2]) % MOD) % MOD;
    D.resize(n + 1);
    return D;
}
ll derangement_ie(int n, const Comb& cb) {           // sum_j (-1)^j C(n,j) (n-j)!
    ll r = 0;
    for (int j = 0; j <= n; j++) {
        ll t = cb.C(n, j) * cb.f[n - j] % MOD;
        r = (j & 1) ? norm(r - t) : (r + t) % MOD;
    }
    return r;
}
ll surjections(int n, int k, const Comb& cb) {       // functions [n] -> [k] that hit every value
    ll r = 0;
    for (int j = 0; j <= k; j++) {
        ll t = cb.C(k, j) * power(k - j, n) % MOD;
        r = (j & 1) ? norm(r - t) : (r + t) % MOD;
    }
    return r;
}

// ---------------------------------------------------------------------------------------------
// 4. Catalan / ballot numbers (reflection principle)
//    paths with a up-steps and b down-steps starting at height h >= 0 that never go below 0
// ---------------------------------------------------------------------------------------------
ll ballot(int a, int b, int h, const Comb& cb) {
    if (a < 0 || b < 0 || h < 0 || h + a - b < 0) return 0;
    return norm(cb.C(a + b, b) - cb.C(a + b, b - h - 1));   // C(.., negative) is 0
}
// CSES Bracket Sequences II style: completions of a valid prefix with balance h using m more symbols
ll bracket_completions(int m, int h, const Comb& cb) {
    if ((m - h) % 2 != 0 || m - h < 0) return 0;
    int a = (m - h) / 2, b = (m + h) / 2;                     // ups, downs
    return ballot(a, b, h, cb);
}

// ---------------------------------------------------------------------------------------------
// 5. Burnside: necklaces under rotation, n x n grids under 90-degree rotations
// ---------------------------------------------------------------------------------------------
ll necklaces(ll n, ll k) {                            // (1/n) sum_i k^{gcd(i,n)}
    ll s = 0;
    for (ll i = 0; i < n; i++) s = (s + power(k, gcd(i, n))) % MOD;   // std::gcd (C++17)
    return s * inv(n) % MOD;
}
ll grids_under_rotation(ll n, ll k) {                 // (k^{n^2} + 2 k^{ceil(n^2/4)} + k^{ceil(n^2/2)}) / 4
    ll n2 = n * n;                                    // exponents reduced mod (MOD-1); k != 0 mod MOD
    ll e0 = n2 % (MOD - 1), e1 = ((n2 + 3) / 4) % (MOD - 1), e2 = ((n2 + 1) / 2) % (MOD - 1);
    ll s = (power(k, e0) + 2 * power(k, e1) + power(k, e2)) % MOD;
    return s * inv(4) % MOD;
}

// ---------------------------------------------------------------------------------------------
// 6. Stirling numbers (both kinds), Bell numbers, integer partitions
// ---------------------------------------------------------------------------------------------
vector<vector<ll>> stirling2(int n) {                 // S(i,j): partitions of i-set into j blocks
    vector<vector<ll>> S(n + 1, vector<ll>(n + 1, 0));
    S[0][0] = 1;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= i; j++) S[i][j] = (j * S[i - 1][j] + S[i - 1][j - 1]) % MOD;
    return S;
}
vector<vector<ll>> stirling1(int n) {                 // c(i,j): permutations of i with j cycles (unsigned)
    vector<vector<ll>> c(n + 1, vector<ll>(n + 1, 0));
    c[0][0] = 1;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= i; j++) c[i][j] = ((i - 1) * c[i - 1][j] + c[i - 1][j - 1]) % MOD;
    return c;
}
vector<ll> bell(int n, const Comb& cb) {              // B(i+1) = sum_k C(i,k) B(k)
    vector<ll> B(n + 1, 0); B[0] = 1;
    for (int i = 0; i < n; i++) {
        ll s = 0;
        for (int k = 0; k <= i; k++) s = (s + cb.C(i, k) * B[k]) % MOD;
        B[i + 1] = s;
    }
    return B;
}
vector<ll> partitions(int n) {                        // p(0..n): unbounded knapsack over part sizes
    vector<ll> p(n + 1, 0); p[0] = 1;
    for (int part = 1; part <= n; part++)
        for (int s = part; s <= n; s++) p[s] = (p[s] + p[s - part]) % MOD;
    return p;
}
vector<ll> partitions_pentagonal(int n) {             // Euler: O(n sqrt n)
    vector<ll> p(n + 1, 0); p[0] = 1;
    for (int i = 1; i <= n; i++) {
        ll s = 0;
        for (ll k = 1;; k++) {
            ll g1 = k * (3 * k - 1) / 2, g2 = k * (3 * k + 1) / 2;
            if (g1 > i) break;
            ll sign = (k & 1) ? 1 : -1;
            s += sign * p[i - g1];
            if (g2 <= i) s += sign * p[i - g2];
            s %= MOD;
        }
        p[i] = norm(s);
    }
    return p;
}

// ---------------------------------------------------------------------------------------------
// 7. Grid paths with obstacles: huge grid, few obstacles (first-obstacle inclusion–exclusion)
// ---------------------------------------------------------------------------------------------
ll paths(int r1, int c1, int r2, int c2, const Comb& cb) {       // monotone paths (r1,c1)->(r2,c2)
    if (r2 < r1 || c2 < c1) return 0;
    return cb.C(r2 - r1 + c2 - c1, r2 - r1);
}
ll grid_paths_with_obstacles(vector<pair<int,int>> obs, int R, int C, const Comb& cb) {
    obs.push_back({R, C});                            // treat the target as the final "obstacle"
    sort(obs.begin(), obs.end());
    int k = obs.size();
    vector<ll> bad(k);                                // paths from (0,0) reaching obs[i] first
    for (int i = 0; i < k; i++) {
        bad[i] = paths(0, 0, obs[i].first, obs[i].second, cb);
        for (int j = 0; j < i; j++)
            bad[i] = norm(bad[i] - bad[j] * paths(obs[j].first, obs[j].second,
                                                  obs[i].first, obs[i].second, cb) % MOD);
    }
    return bad[k - 1];
}
ll grid_paths_dp(const vector<string>& g) {           // small grid brute force, '#' = obstacle
    int R = g.size(), C = g[0].size();
    vector<vector<ll>> dp(R, vector<ll>(C, 0));
    dp[0][0] = g[0][0] == '#' ? 0 : 1;
    for (int i = 0; i < R; i++) for (int j = 0; j < C; j++) {
        if (g[i][j] == '#') { dp[i][j] = 0; continue; }
        if (i) dp[i][j] = (dp[i][j] + dp[i - 1][j]) % MOD;
        if (j) dp[i][j] = (dp[i][j] + dp[i][j - 1]) % MOD;
    }
    return dp[R - 1][C - 1];
}

// ---------------------------------------------------------------------------------------------
// 8. Expected values by linearity, probability DP
// ---------------------------------------------------------------------------------------------
// E[#inversions] for a_i uniform in [1, r_i], independent (CSES Inversion Probability)
double expected_inversions(const vector<int>& r) {
    int n = r.size(); double e = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
        double x = r[i], y = r[j];
        double favorable = (x <= y) ? x * (x - 1) / 2 : x * y - y * (y + 1) / 2;   // #(u > v)
        e += favorable / (x * y);
    }
    return e;
}
// E[max of n i.i.d. uniform{1..k}] = sum_v v (P(max<=v) - P(max<=v-1))  (CSES Candy Lottery)
double expected_max_uniform(int n, int k) {
    double e = 0;
    for (int v = 1; v <= k; v++) e += v * (pow((double)v / k, n) - pow((double)(v - 1) / k, n));
    return e;
}
// P(sum of n fair dice in [a,b])  (CSES Dice Probability)
double dice_sum_probability(int n, int a, int b) {
    vector<double> dp(6 * n + 1, 0.0); dp[0] = 1;
    for (int i = 1; i <= n; i++) {
        vector<double> nx(6 * n + 1, 0.0);
        for (int s = 0; s <= 6 * (i - 1); s++) if (dp[s] > 0)
            for (int f = 1; f <= 6; f++) nx[s + f] += dp[s] / 6.0;
        dp.swap(nx);
    }
    double p = 0;
    for (int s = max(a, n); s <= min(b, 6 * n); s++) p += dp[s];
    return p;
}

// ---------------------------------------------------------------------------------------------
// 9. Fibonacci by fast doubling: returns (F_n, F_{n+1}) mod MOD
// ---------------------------------------------------------------------------------------------
pair<ll,ll> fib(ll n) {
    if (n == 0) return {0, 1};
    auto [a, b] = fib(n >> 1);                        // a = F_k, b = F_{k+1}
    ll c = a * norm(2 * b - a) % MOD;                 // F_{2k} = F_k (2F_{k+1} - F_k)
    ll d = (a * a + b * b) % MOD;                     // F_{2k+1} = F_k^2 + F_{k+1}^2
    if (n & 1) return {d, (c + d) % MOD};
    return {c, d};
}

// =============================================================================================
// Tests (brute forces live here; they are part of the training too)
// =============================================================================================
static ll brute_nCr(int n, int k) {                   // exact, small n
    if (k < 0 || k > n) return 0;
    ll r = 1;
    for (int i = 0; i < k; i++) r = r * (n - i) / (i + 1);
    return r;
}

static void test_binomials() {
    Comb cb(2000);
    auto P = pascal(60, MOD);
    for (int n = 0; n <= 60; n++) for (int k = 0; k <= n; k++) {
        assert(cb.C(n, k) == P[n][k]);
        assert(cb.C(n, k) == brute_nCr(n, k) % MOD);
    }
    assert(cb.C(5, 7) == 0 && cb.C(5, -1) == 0);
    assert(cb.P(5, 2) == 20 && cb.P(5, 5) == 120);
    assert(cb.multinomial({2, 1, 1}) == 12);           // AABC arrangements
    // absorption, Vandermonde, hockey stick
    for (int n = 1; n <= 30; n++) for (int k = 1; k <= n; k++)
        assert(k * cb.C(n, k) % MOD == n * cb.C(n - 1, k - 1) % MOD);
    for (int k = 0; k <= 10; k++) {
        ll s = 0;
        for (int i = 0; i <= k; i++) s = (s + cb.C(7, i) * cb.C(9, k - i)) % MOD;
        assert(s == cb.C(16, k));
    }
    for (int k = 0; k <= 6; k++) {
        ll s = 0;
        for (int i = k; i <= 12; i++) s = (s + cb.C(i, k)) % MOD;
        assert(s == cb.C(13, k + 1));
    }
    // Lucas vs Pascal mod 7 and mod 2
    for (ll p : {2LL, 7LL, 13LL}) {
        Lucas L(p);
        auto Pp = pascal(120, p);
        for (int n = 0; n <= 120; n++) for (int k = 0; k <= n; k++) assert(L.C(n, k) == Pp[n][k]);
    }
    {   // parity of C(n,k): odd iff (k & n) == k
        Lucas L2(2);
        for (int n = 0; n < 64; n++) for (int k = 0; k <= n; k++) assert(L2.C(n, k) == ((k & n) == k));
    }
    // small-k formula vs tables
    for (int n = 0; n <= 200; n++) for (int k = 0; k <= 8; k++) assert(binom_small_k(n, k) == cb.C(n, k));
    assert(binom_small_k(1'000'000'000'000LL, 0) == 1);
    // stars and bars vs brute enumeration of solutions
    for (int k = 1; k <= 4; k++) for (int n = 0; n <= 7; n++) {
        ll cnt = 0;
        function<void(int,int)> rec = [&](int idx, int left) {
            if (idx == k - 1) { cnt++; return; }
            for (int x = 0; x <= left; x++) rec(idx + 1, left - x);
        };
        rec(0, n);
        assert(cb.stars_and_bars(n, k) == cnt);
    }
    puts("binomials ok");
}

static void test_inclusion_exclusion() {
    Comb cb(100);
    auto D = derangements(9);
    for (int n = 0; n <= 8; n++) {
        vector<int> p(n); iota(p.begin(), p.end(), 0);
        ll cnt = 0;
        do { bool ok = true; for (int i = 0; i < n; i++) if (p[i] == i) ok = false; cnt += ok; }
        while (next_permutation(p.begin(), p.end()));
        assert(D[n] == cnt && derangement_ie(n, cb) == cnt);
    }
    assert(D[9] == 133496);
    // surjections vs brute enumeration of all functions
    for (int n = 0; n <= 6; n++) for (int k = 1; k <= 4; k++) {
        ll total = 1; for (int i = 0; i < n; i++) total *= k;
        ll cnt = 0;
        for (ll code = 0; code < total; code++) {
            int seen = 0; ll c = code;
            for (int i = 0; i < n; i++) { seen |= 1 << (c % k); c /= k; }
            cnt += (seen == (1 << k) - 1);
        }
        assert(surjections(n, k, cb) == cnt);
    }
    puts("inclusion-exclusion ok");
}

static void test_catalan_ballot() {
    Comb cb(100);
    ll cat[] = {1, 1, 2, 5, 14, 42, 132, 429, 1430, 4862};
    for (int n = 0; n < 10; n++) assert(cb.catalan(n) == cat[n]);
    // ballot vs brute: enumerate all sequences of a ups and b downs from height h
    for (int h = 0; h <= 3; h++) for (int a = 0; a <= 6; a++) for (int b = 0; b <= 6; b++) {
        int m = a + b; ll cnt = 0;
        for (int mask = 0; mask < (1 << m); mask++) {
            if (__builtin_popcount(mask) != a) continue;
            int height = h; bool ok = true;
            for (int i = 0; i < m; i++) { height += (mask >> i & 1) ? 1 : -1; if (height < 0) ok = false; }
            cnt += ok;
        }
        assert(ballot(a, b, h, cb) == cnt);
    }
    for (int n = 0; n <= 8; n++) assert(bracket_completions(2 * n, 0, cb) == cb.catalan(n));
    assert(bracket_completions(3, 1, cb) == 2);        // prefix "(" + 3 symbols: "(())" and "()()"
    puts("catalan/ballot ok");
}

static void test_burnside() {
    // necklaces: brute canonical form = lexicographically minimal rotation
    for (int n = 1; n <= 6; n++) for (int k = 1; k <= 3; k++) {
        set<vector<int>> classes;
        ll total = 1; for (int i = 0; i < n; i++) total *= k;
        for (ll code = 0; code < total; code++) {
            vector<int> v(n); ll c = code;
            for (int i = 0; i < n; i++) { v[i] = c % k; c /= k; }
            vector<int> best = v;
            for (int r = 1; r < n; r++) { rotate(v.begin(), v.begin() + 1, v.end()); best = min(best, v); }
            classes.insert(best);
        }
        assert(necklaces(n, k) == (ll)classes.size());
    }
    assert(necklaces(4, 2) == 6);
    // grids: brute over all 2-colorings for n = 1..3
    for (int n = 1; n <= 3; n++) {
        int cells = n * n; set<vector<int>> classes;
        auto rot = [&](const vector<int>& g) {          // 90 degrees clockwise
            vector<int> r(cells);
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) r[j * n + (n - 1 - i)] = g[i * n + j];
            return r;
        };
        for (int mask = 0; mask < (1 << cells); mask++) {
            vector<int> g(cells); for (int i = 0; i < cells; i++) g[i] = mask >> i & 1;
            vector<int> best = g;
            for (int t = 0; t < 3; t++) { g = rot(g); best = min(best, g); }
            classes.insert(best);
        }
        assert(grids_under_rotation(n, 2) == (ll)classes.size());
    }
    assert(grids_under_rotation(2, 2) == 6);
    puts("burnside ok");
}

static void test_stirling_bell_partitions() {
    Comb cb(50);
    auto S = stirling2(12), c = stirling1(12);
    auto B = bell(12, cb);
    for (int n = 0; n <= 8; n++) for (int k = 0; k <= n; k++)
        assert(cb.f[k] * S[n][k] % MOD == surjections(n, k, cb));   // k! S(n,k) = #surjections
    for (int n = 0; n <= 12; n++) {
        ll s1 = 0, s2 = 0;
        for (int k = 0; k <= n; k++) { s1 = (s1 + c[n][k]) % MOD; s2 = (s2 + S[n][k]) % MOD; }
        assert(s1 == cb.f[n] && s2 == B[n]);                          // sum c(n,k) = n!, sum S(n,k) = B(n)
    }
    ll bellv[] = {1, 1, 2, 5, 15, 52, 203, 877, 4140};
    for (int i = 0; i < 9; i++) assert(B[i] == bellv[i]);
    assert(S[5][2] == 15 && S[5][3] == 25 && c[4][2] == 11);
    // partitions: brute by enumerating non-increasing sequences
    auto p = partitions(60), q = partitions_pentagonal(60);
    for (int n = 0; n <= 60; n++) assert(p[n] == q[n]);
    for (int n = 0; n <= 12; n++) {
        ll cnt = 0;
        function<void(int,int)> rec = [&](int left, int maxpart) {
            if (left == 0) { cnt++; return; }
            for (int x = min(left, maxpart); x >= 1; x--) rec(left - x, x);
        };
        rec(n, n);
        assert(p[n] == cnt);
    }
    assert(p[10] == 42 && p[50] == 204226);
    puts("stirling/bell/partitions ok");
}

static void test_grid_paths() {
    Comb cb(100);
    mt19937 rng(12345);
    for (int it = 0; it < 300; it++) {
        int R = 1 + rng() % 7, C = 1 + rng() % 7;
        vector<string> g(R, string(C, '.'));
        vector<pair<int,int>> obs;
        int cnt = rng() % 6;
        for (int t = 0; t < cnt; t++) {
            int r = rng() % R, c = rng() % C;
            if ((r == 0 && c == 0) || (r == R - 1 && c == C - 1) || g[r][c] == '#') continue;
            g[r][c] = '#'; obs.push_back({r, c});
        }
        assert(grid_paths_with_obstacles(obs, R - 1, C - 1, cb) == grid_paths_dp(g));
    }
    assert(grid_paths_with_obstacles({}, 3, 3, cb) == 20);
    puts("grid paths ok");
}

static void test_probability() {
    // expected inversions vs full enumeration
    mt19937 rng(7);
    for (int it = 0; it < 50; it++) {
        int n = 1 + rng() % 4;
        vector<int> r(n); for (int& x : r) x = 1 + rng() % 4;
        vector<int> a(n, 1); double total = 0, cnt = 0;
        function<void(int)> rec = [&](int i) {
            if (i == n) {
                int invs = 0;
                for (int x = 0; x < n; x++) for (int y = x + 1; y < n; y++) invs += a[x] > a[y];
                total += invs; cnt += 1; return;
            }
            for (a[i] = 1; a[i] <= r[i]; a[i]++) rec(i + 1);
        };
        rec(0);
        assert(fabs(expected_inversions(r) - total / cnt) < 1e-9);
    }
    // expected max vs enumeration
    for (int n = 1; n <= 4; n++) for (int k = 1; k <= 5; k++) {
        double total = 0; ll cnt = 1; for (int i = 0; i < n; i++) cnt *= k;
        for (ll code = 0; code < cnt; code++) {
            ll c = code; int mx = 0;
            for (int i = 0; i < n; i++) { mx = max<int>(mx, 1 + c % k); c /= k; }
            total += mx;
        }
        assert(fabs(expected_max_uniform(n, k) - total / cnt) < 1e-9);
    }
    // dice sum probability vs enumeration (n = 3 dice)
    {
        vector<int> hist(19, 0);
        for (int a = 1; a <= 6; a++) for (int b = 1; b <= 6; b++) for (int c = 1; c <= 6; c++) hist[a + b + c]++;
        for (int lo = 3; lo <= 18; lo++) for (int hi = lo; hi <= 18; hi++) {
            int s = 0; for (int v = lo; v <= hi; v++) s += hist[v];
            assert(fabs(dice_sum_probability(3, lo, hi) - s / 216.0) < 1e-12);
        }
    }
    puts("probability ok");
}

static void test_fibonacci() {
    vector<ll> F(200); F[0] = 0; F[1] = 1;
    for (int i = 2; i < 200; i++) F[i] = (F[i - 1] + F[i - 2]) % MOD;
    for (int n = 0; n < 199; n++) { auto [a, b] = fib(n); assert(a == F[n] && b == F[n + 1]); }
    // Cassini: F_{n-1} F_{n+1} - F_n^2 = (-1)^n
    for (int n = 1; n < 100; n++)
        assert(norm(F[n - 1] * F[n + 1] % MOD - F[n] * F[n] % MOD) == ((n & 1) ? MOD - 1 : 1));
    assert(fib(1'000'000'000'000'000'000LL).first == 209783453);   // F_{1e18} mod 1e9+7 (well-known value)
    puts("fibonacci ok");
}

int main() {
    test_binomials();
    test_inclusion_exclusion();
    test_catalan_ballot();
    test_burnside();
    test_stirling_bell_partitions();
    test_grid_paths();
    test_probability();
    test_fibonacci();
    puts("all tests passed");
    return 0;
}
