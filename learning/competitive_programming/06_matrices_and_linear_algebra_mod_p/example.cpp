// Chapter 06 — Matrices and Linear Algebra mod p: reference library + self-tests.
//
// TRAINING RULE: re-type every snippet here from memory. Minimum set for a contest:
// `Mat` + `mpow`, `kth_term_matrix` (companion matrix), `MinPlus`, `gauss` (reals),
// `det_mod`, `gauss_gf2`, `XorBasis`, `kirchhoff`. Then `linear_rec` and `berlekamp_massey`.
//
// Build & run:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
// (on macOS/libc++ add  -I ../include  so that <bits/stdc++.h> resolves to the local shim)
// Every technique is cross-checked against a naive DP or brute force; prints "all tests passed".

#include <bits/stdc++.h>
using namespace std;
using ll = long long;

const ll MOD = 1'000'000'007;

ll power(ll b, ll e, ll m = MOD) {
    ll r = 1; b %= m; if (b < 0) b += m;
    for (; e > 0; e >>= 1, b = b * b % m) if (e & 1) r = r * b % m;
    return r;
}
ll norm(ll a) { a %= MOD; return a < 0 ? a + MOD : a; }

// ---------------------------------------------------------------------------------------------
// 1. Matrices mod p, fast exponentiation
// ---------------------------------------------------------------------------------------------
struct Mat {
    int n; vector<vector<ll>> a;
    explicit Mat(int n) : n(n), a(n, vector<ll>(n, 0)) {}
    static Mat identity(int n) { Mat I(n); for (int i = 0; i < n; i++) I.a[i][i] = 1; return I; }
    Mat operator*(const Mat& o) const {                 // i,k,j loop order: cache friendly
        Mat r(n);
        for (int i = 0; i < n; i++)
            for (int k = 0; k < n; k++) {
                if (a[i][k] == 0) continue;
                for (int j = 0; j < n; j++) r.a[i][j] = (r.a[i][j] + a[i][k] * o.a[k][j]) % MOD;
            }
        return r;
    }
};
Mat mpow(Mat b, ll e) {                                  // invariant: r * b^e == original b^e
    Mat r = Mat::identity(b.n);
    for (; e > 0; e >>= 1, b = b * b) if (e & 1) r = r * b;
    return r;
}
// NOTE: named mat_apply, not apply -- with `using namespace std;` a call to apply() is ambiguous with std::apply.
vector<ll> mat_apply(const Mat& m, const vector<ll>& v) {  // m * v, v a column vector
    vector<ll> r(m.n, 0);
    for (int i = 0; i < m.n; i++) for (int j = 0; j < m.n; j++) r[i] = (r[i] + m.a[i][j] * v[j]) % MOD;
    return r;
}

// ---------------------------------------------------------------------------------------------
// 2. Linear recurrence a_n = sum_{j<k} c[j] a_{n-1-j} via companion matrix, O(k^3 log n)
// ---------------------------------------------------------------------------------------------
ll kth_term_matrix(const vector<ll>& c, const vector<ll>& init, ll n) {
    int k = c.size();
    if (n < k) return norm(init[n]);
    Mat T(k);
    for (int j = 0; j < k; j++) T.a[0][j] = norm(c[j]);
    for (int i = 1; i < k; i++) T.a[i][i - 1] = 1;
    vector<ll> v(k);                                     // (a_{k-1}, ..., a_0)
    for (int i = 0; i < k; i++) v[i] = norm(init[k - 1 - i]);
    return mat_apply(mpow(T, n - (k - 1)), v)[0];
}
ll kth_term_naive(const vector<ll>& c, const vector<ll>& init, ll n) {   // O(n k) reference
    int k = c.size();
    vector<ll> a(init.begin(), init.end());
    for (ll& x : a) x = norm(x);
    for (ll i = k; i <= n; i++) {
        ll s = 0;
        for (int j = 0; j < k; j++) s = (s + norm(c[j]) * a[i - 1 - j]) % MOD;
        a.push_back(s);
    }
    return a[n];
}

// ---------------------------------------------------------------------------------------------
// 3. Kitamasa: a_n in O(k^2 log n) (KACTL convention: S = a_0..a_{k-1}, a_i = sum_j tr[j] a_{i-1-j})
// ---------------------------------------------------------------------------------------------
ll linear_rec(const vector<ll>& S, const vector<ll>& tr, ll n) {
    int k = tr.size();
    auto combine = [&](vector<ll> a, vector<ll> b) {
        vector<ll> res(2 * k + 1, 0);
        for (int i = 0; i <= k; i++) for (int j = 0; j <= k; j++)
            res[i + j] = (res[i + j] + a[i] * b[j]) % MOD;
        for (int i = 2 * k; i > k; i--)                  // x^i -> sum_j tr[j] x^{i-1-j}
            for (int j = 0; j < k; j++) res[i - 1 - j] = (res[i - 1 - j] + res[i] * tr[j]) % MOD;
        res.resize(k + 1);
        return res;
    };
    vector<ll> pol(k + 1, 0), e(pol);
    pol[0] = 1; e[1] = 1;
    for (++n; n; n /= 2) {
        if (n % 2) pol = combine(pol, e);
        e = combine(e, e);
    }
    ll res = 0;
    for (int i = 0; i < k; i++) res = (res + pol[i + 1] * S[i]) % MOD;
    return res;
}

// ---------------------------------------------------------------------------------------------
// 4. Berlekamp–Massey: shortest recurrence c with s[i] = sum_j c[j] s[i-1-j], O(n^2)
// ---------------------------------------------------------------------------------------------
vector<ll> berlekamp_massey(const vector<ll>& s) {
    int n = s.size(), L = 0, m = 0;
    vector<ll> C(n, 0), B(n, 0), T;
    C[0] = B[0] = 1;
    ll b = 1;
    for (int i = 0; i < n; i++) { ++m;
        ll d = s[i] % MOD;
        for (int j = 1; j <= L; j++) d = (d + C[j] * s[i - j]) % MOD;
        if (d == 0) continue;
        T = C;
        ll coef = d * power(b, MOD - 2) % MOD;
        for (int j = m; j < n; j++) C[j] = norm(C[j] - coef * B[j - m]);
        if (2 * L > i) continue;
        L = i + 1 - L; B = T; b = d; m = 0;
    }
    C.resize(L + 1); C.erase(C.begin());
    for (ll& x : C) x = (MOD - x) % MOD;
    return C;
}

// ---------------------------------------------------------------------------------------------
// 5. (min,+) semiring: cheapest walk with exactly k edges
// ---------------------------------------------------------------------------------------------
const ll INF = LLONG_MAX / 4;
struct MinPlus {
    int n; vector<vector<ll>> a;
    explicit MinPlus(int n) : n(n), a(n, vector<ll>(n, INF)) {}
    static MinPlus identity(int n) { MinPlus I(n); for (int i = 0; i < n; i++) I.a[i][i] = 0; return I; }
    MinPlus operator*(const MinPlus& o) const {
        MinPlus r(n);
        for (int i = 0; i < n; i++) for (int k = 0; k < n; k++) {
            if (a[i][k] == INF) continue;
            for (int j = 0; j < n; j++)
                if (o.a[k][j] != INF) r.a[i][j] = min(r.a[i][j], a[i][k] + o.a[k][j]);
        }
        return r;
    }
};
MinPlus mpow(MinPlus b, ll e) {
    MinPlus r = MinPlus::identity(b.n);
    for (; e > 0; e >>= 1, b = b * b) if (e & 1) r = r * b;
    return r;
}

// (OR, AND) semiring with bitset rows: reachability in exactly k steps, O(n^3 / 64)
const int NB = 64;
struct BoolMat {
    int n; vector<bitset<NB>> a;
    explicit BoolMat(int n) : n(n), a(n) {}
    static BoolMat identity(int n) { BoolMat I(n); for (int i = 0; i < n; i++) I.a[i][i] = 1; return I; }
    BoolMat operator*(const BoolMat& o) const {
        BoolMat r(n);
        for (int i = 0; i < n; i++) for (int k = 0; k < n; k++) if (a[i][k]) r.a[i] |= o.a[k];
        return r;
    }
};
BoolMat mpow(BoolMat b, ll e) {
    BoolMat r = BoolMat::identity(b.n);
    for (; e > 0; e >>= 1, b = b * b) if (e & 1) r = r * b;
    return r;
}

// ---------------------------------------------------------------------------------------------
// 6. Gaussian elimination over the reals (Gauss–Jordan, partial pivoting)
//    returns 0: no solution, 1: unique, 2: infinitely many (x = one particular solution)
// ---------------------------------------------------------------------------------------------
const double EPS = 1e-9;
int gauss(vector<vector<double>> a, vector<double> b, vector<double>& x) {
    int n = a.size(), m = a[0].size();
    vector<int> where(m, -1);
    for (int col = 0, row = 0; col < m && row < n; col++) {
        int sel = row;
        for (int i = row; i < n; i++) if (fabs(a[i][col]) > fabs(a[sel][col])) sel = i;
        if (fabs(a[sel][col]) < EPS) continue;
        swap(a[sel], a[row]); swap(b[sel], b[row]);
        where[col] = row;
        for (int i = 0; i < n; i++) if (i != row) {
            double f = a[i][col] / a[row][col];
            if (fabs(f) < EPS) continue;
            for (int j = col; j < m; j++) a[i][j] -= f * a[row][j];
            b[i] -= f * b[row];
        }
        row++;
    }
    x.assign(m, 0.0);
    for (int j = 0; j < m; j++) if (where[j] != -1) x[j] = b[where[j]] / a[where[j]][j];
    for (int i = 0; i < n; i++) {
        double s = 0;
        for (int j = 0; j < m; j++) s += a[i][j] * x[j];
        if (fabs(s - b[i]) > EPS) return 0;
    }
    for (int j = 0; j < m; j++) if (where[j] == -1) return 2;
    return 1;
}
double det_real(vector<vector<double>> a) {                 // forward elimination, product of pivots
    int n = a.size(); double det = 1;
    for (int col = 0; col < n; col++) {
        int sel = col;
        for (int i = col; i < n; i++) if (fabs(a[i][col]) > fabs(a[sel][col])) sel = i;
        if (fabs(a[sel][col]) < EPS) return 0;
        if (sel != col) { swap(a[sel], a[col]); det = -det; }
        det *= a[col][col];
        for (int i = col + 1; i < n; i++) {
            double f = a[i][col] / a[col][col];
            for (int j = col; j < n; j++) a[i][j] -= f * a[col][j];
        }
    }
    return det;
}
bool inverse_real(vector<vector<double>> a, vector<vector<double>>& inv) {   // Gauss–Jordan on [A | I]
    int n = a.size();
    inv.assign(n, vector<double>(n, 0)); for (int i = 0; i < n; i++) inv[i][i] = 1;
    for (int col = 0; col < n; col++) {
        int sel = col;
        for (int i = col; i < n; i++) if (fabs(a[i][col]) > fabs(a[sel][col])) sel = i;
        if (fabs(a[sel][col]) < EPS) return false;             // singular
        swap(a[sel], a[col]); swap(inv[sel], inv[col]);
        double p = a[col][col];
        for (int j = 0; j < n; j++) { a[col][j] /= p; inv[col][j] /= p; }
        for (int i = 0; i < n; i++) if (i != col) {
            double f = a[i][col];
            if (fabs(f) < EPS) continue;
            for (int j = 0; j < n; j++) { a[i][j] -= f * a[col][j]; inv[i][j] -= f * inv[col][j]; }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// 7. Gaussian elimination over Z_p: determinant, and solve (returns 0/1/2 like the real version)
// ---------------------------------------------------------------------------------------------
ll det_mod(vector<vector<ll>> a) {
    int n = a.size(); ll det = 1;
    for (int col = 0; col < n; col++) {
        int sel = -1;
        for (int i = col; i < n; i++) if (a[i][col] != 0) { sel = i; break; }
        if (sel == -1) return 0;
        if (sel != col) { swap(a[sel], a[col]); det = (MOD - det) % MOD; }
        det = det * a[col][col] % MOD;
        ll iv = power(a[col][col], MOD - 2);
        for (int i = col + 1; i < n; i++) if (a[i][col] != 0) {
            ll f = a[i][col] * iv % MOD;
            for (int j = col; j < n; j++) a[i][j] = norm(a[i][j] - f * a[col][j]);
        }
    }
    return det;
}
int gauss_mod(vector<vector<ll>> a, vector<ll> b, vector<ll>& x) {
    int n = a.size(), m = a[0].size();
    vector<int> where(m, -1);
    for (int col = 0, row = 0; col < m && row < n; col++) {
        int sel = -1;
        for (int i = row; i < n; i++) if (a[i][col] != 0) { sel = i; break; }
        if (sel == -1) continue;
        swap(a[sel], a[row]); swap(b[sel], b[row]);
        where[col] = row;
        ll iv = power(a[row][col], MOD - 2);
        for (int j = 0; j < m; j++) a[row][j] = a[row][j] * iv % MOD;      // normalize pivot row to 1
        b[row] = b[row] * iv % MOD;
        for (int i = 0; i < n; i++) if (i != row && a[i][col] != 0) {
            ll f = a[i][col];
            for (int j = 0; j < m; j++) a[i][j] = norm(a[i][j] - f * a[row][j]);
            b[i] = norm(b[i] - f * b[row]);
        }
        row++;
    }
    x.assign(m, 0);
    for (int j = 0; j < m; j++) if (where[j] != -1) x[j] = b[where[j]];
    for (int i = 0; i < n; i++) {                               // zero row with non-zero rhs -> none
        bool zero = true;
        for (int j = 0; j < m; j++) if (a[i][j]) zero = false;
        if (zero && b[i] != 0) return 0;
    }
    for (int j = 0; j < m; j++) if (where[j] == -1) return 2;
    return 1;
}

// ---------------------------------------------------------------------------------------------
// 8. GF(2): bitset Gauss (rank / solve) and the incremental XOR basis
// ---------------------------------------------------------------------------------------------
const int MB = 128;                                          // columns; last one may hold the rhs
int gauss_gf2(vector<bitset<MB>>& rows, int cols) {          // RREF in place over the first `cols` columns
    int rank = 0, n = rows.size();
    for (int col = 0; col < cols && rank < n; col++) {
        int sel = -1;
        for (int i = rank; i < n; i++) if (rows[i][col]) { sel = i; break; }
        if (sel == -1) continue;
        swap(rows[sel], rows[rank]);
        for (int i = 0; i < n; i++) if (i != rank && rows[i][col]) rows[i] ^= rows[rank];
        rank++;
    }
    return rank;
}
// Solve A x = b over GF(2): rows = [A | b] with b at column `vars`. Returns 0/1/2 like gauss().
int solve_gf2(vector<bitset<MB>> rows, int vars, bitset<MB>& x) {
    int n = rows.size();
    vector<int> where(vars, -1);
    int rank = 0;
    for (int col = 0; col < vars && rank < n; col++) {
        int sel = -1;
        for (int i = rank; i < n; i++) if (rows[i][col]) { sel = i; break; }
        if (sel == -1) continue;
        swap(rows[sel], rows[rank]);
        for (int i = 0; i < n; i++) if (i != rank && rows[i][col]) rows[i] ^= rows[rank];
        where[col] = rank++;
    }
    x.reset();
    for (int j = 0; j < vars; j++) if (where[j] != -1) x[j] = rows[where[j]][vars];
    for (int i = rank; i < n; i++) if (rows[i][vars]) return 0;        // 0 = 1 row
    for (int j = 0; j < vars; j++) if (where[j] == -1) return 2;
    return 1;
}

struct XorBasis {
    static const int B = 60;
    ll basis[B] = {}; int rank = 0;
    bool insert(ll x) {
        for (int b = B - 1; b >= 0; b--) {
            if (!(x >> b & 1)) continue;
            if (!basis[b]) { basis[b] = x; rank++; return true; }
            x ^= basis[b];
        }
        return false;
    }
    bool can_make(ll x) const {
        for (int b = B - 1; b >= 0; b--) if (x >> b & 1) { if (!basis[b]) return false; x ^= basis[b]; }
        return true;
    }
    ll max_xor(ll start = 0) const {
        ll r = start;
        for (int b = B - 1; b >= 0; b--) if (basis[b] && !(r >> b & 1)) r ^= basis[b];
        return r;
    }
    ll kth(ll k) const {                                     // k-th smallest distinct value, 1-indexed
        vector<ll> red;
        for (int b = 0; b < B; b++) if (basis[b]) {
            ll v = basis[b];
            for (ll u : red) if (v >> (63 - __builtin_clzll(u)) & 1) v ^= u;
            red.push_back(v);
        }
        ll r = 0; k--;
        for (int i = 0; i < (int)red.size(); i++) if (k >> i & 1) r ^= red[i];
        return r;
    }
};

// ---------------------------------------------------------------------------------------------
// 9. Kirchhoff: number of spanning trees of an undirected multigraph (no self-loops), mod p
// ---------------------------------------------------------------------------------------------
ll kirchhoff(int n, const vector<pair<int,int>>& edges) {
    if (n == 1) return 1;
    vector<vector<ll>> L(n, vector<ll>(n, 0));
    for (auto [u, v] : edges) {
        if (u == v) continue;
        L[u][u]++; L[v][v]++;
        L[u][v] = norm(L[u][v] - 1); L[v][u] = norm(L[v][u] - 1);
    }
    vector<vector<ll>> M(n - 1, vector<ll>(n - 1));         // delete row 0 and column 0
    for (int i = 1; i < n; i++) for (int j = 1; j < n; j++) M[i - 1][j - 1] = L[i][j];
    return det_mod(M);
}

// ---------------------------------------------------------------------------------------------
// 10. Expected steps to absorption: (I - Q) E = 1 solved by Gauss
//     P: t x t transition matrix among transient states (rows may sum to < 1: mass to absorbing)
// ---------------------------------------------------------------------------------------------
vector<double> expected_steps(const vector<vector<double>>& P) {
    int t = P.size();
    vector<vector<double>> A(t, vector<double>(t, 0));
    vector<double> b(t, 1.0), E;
    for (int i = 0; i < t; i++) for (int j = 0; j < t; j++) A[i][j] = (i == j) - P[i][j];
    int st = gauss(A, b, E);
    assert(st == 1);
    return E;
}

// =============================================================================================
// Tests
// =============================================================================================
static mt19937_64 rng(2024);
static ll rnd(ll lo, ll hi) { return lo + (ll)(rng() % (unsigned long long)(hi - lo + 1)); }

static void test_matrix_power() {
    // Fibonacci via T^n vs iterative
    vector<ll> F(300); F[0] = 0; F[1] = 1;
    for (int i = 2; i < 300; i++) F[i] = (F[i - 1] + F[i - 2]) % MOD;
    Mat T(2); T.a = {{1, 1}, {1, 0}};
    for (int n = 0; n < 300; n++) { Mat P = mpow(T, n); assert(P.a[0][1] == F[n]); }
    assert(mpow(T, 1'000'000'000'000'000'000LL).a[0][1] == 209783453);   // F_{1e18} mod 1e9+7
    // Throwing Dice: f(n) = sum_{i=1}^{6} f(n-i), f(0) = 1, via companion matrix vs DP
    {
        vector<ll> c(6, 1), init = {1, 1, 2, 4, 8, 16};        // f(0..5)
        for (int n = 0; n <= 60; n++) assert(kth_term_matrix(c, init, n) == kth_term_naive(c, init, n));
        assert(kth_term_matrix(c, init, 7) == 63);
    }
    // random k-term recurrences with negative coefficients
    for (int it = 0; it < 30; it++) {
        int k = rnd(1, 6);
        vector<ll> c(k), init(k);
        for (auto& x : c) x = rnd(-5, 5);
        for (auto& x : init) x = rnd(0, 100);
        ll n = rnd(0, 80);
        assert(kth_term_matrix(c, init, n) == kth_term_naive(c, init, n));
    }
    // Counting Towers transition [[2,1],[1,4]] — matrix vs linear DP, and the CSES sample n=1,2
    {
        Mat W(2); W.a = {{2, 1}, {1, 4}};
        ll wide = 1, narrow = 1;
        for (int n = 1; n <= 200; n++) {
            vector<ll> v = mat_apply(mpow(W, n - 1), {1, 1});
            assert(v[0] == wide && v[1] == narrow);
            if (n == 1) assert((wide + narrow) % MOD == 2);
            if (n == 2) assert((wide + narrow) % MOD == 8);
            ll nw = (2 * wide + narrow) % MOD, nn = (wide + 4 * narrow) % MOD;
            wide = nw; narrow = nn;
        }
    }
    puts("matrix power ok");
}

static void test_kitamasa_bm() {
    for (int it = 0; it < 40; it++) {
        int k = rnd(1, 7);
        vector<ll> tr(k), S(k);
        for (auto& x : tr) x = norm(rnd(-9, 9));
        for (auto& x : S) x = rnd(0, 1000);
        ll n = rnd(0, 120);
        ll want = kth_term_naive(tr, S, n);
        assert(linear_rec(S, tr, n) == want);
        assert(kth_term_matrix(tr, S, n) == want);
        // Berlekamp–Massey from 2k+6 terms recovers a recurrence that reproduces the sequence
        int m = 2 * k + 6;
        vector<ll> seq(m);
        for (int i = 0; i < m; i++) seq[i] = kth_term_naive(tr, S, i);
        vector<ll> rec = berlekamp_massey(seq);
        assert((int)rec.size() <= k);
        for (int i = rec.size(); i < m; i++) {
            ll s = 0;
            for (int j = 0; j < (int)rec.size(); j++) s = (s + rec[j] * seq[i - 1 - j]) % MOD;
            assert(s == seq[i]);
        }
        // and gives the same far term through Kitamasa
        vector<ll> S2(seq.begin(), seq.begin() + rec.size());
        if (!rec.empty()) assert(linear_rec(S2, rec, n) == want);
    }
    // Fibonacci: BM must return exactly (1, 1)
    {
        vector<ll> f = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
        vector<ll> rec = berlekamp_massey(f);
        assert(rec == vector<ll>({1, 1}));
        assert(linear_rec({0, 1}, rec, 1'000'000'000'000'000'000LL) == 209783453);
    }
    puts("kitamasa / berlekamp-massey ok");
}

static void test_semirings() {
    for (int it = 0; it < 20; it++) {
        int n = rnd(1, 6);
        Mat A(n); MinPlus M(n); BoolMat Bm(n);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (rng() % 3 == 0) {
            ll w = rnd(1, 9);
            A.a[i][j] = 1; M.a[i][j] = w; Bm.a[i][j] = 1;
        }
        int K = rnd(0, 8);
        Mat Ak = mpow(A, K); MinPlus Mk = mpow(M, K); BoolMat Bk = mpow(Bm, K);
        // DP over steps: count[s][v], best[s][v], reach[s][v] from every source
        for (int src = 0; src < n; src++) {
            vector<ll> cnt(n, 0), best(n, INF); cnt[src] = 1; best[src] = 0;
            for (int s = 0; s < K; s++) {
                vector<ll> nc(n, 0), nb(n, INF);
                for (int u = 0; u < n; u++) for (int v = 0; v < n; v++) if (A.a[u][v]) {
                    nc[v] = (nc[v] + cnt[u]) % MOD;
                    if (best[u] != INF) nb[v] = min(nb[v], best[u] + M.a[u][v]);
                }
                cnt = nc; best = nb;
            }
            for (int v = 0; v < n; v++) {
                assert(Ak.a[src][v] == cnt[v]);
                assert(Mk.a[src][v] == best[v]);
                assert(Bk.a[src][v] == (cnt[v] != 0 || (K == 0 && v == src)));
            }
        }
    }
    puts("semirings (count / min-plus / boolean) ok");
}

static ll det_brute_mod(const vector<vector<ll>>& a) {      // Leibniz expansion, n <= 6
    int n = a.size(); vector<int> p(n); iota(p.begin(), p.end(), 0);
    ll det = 0;
    do {
        int inv = 0;
        for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) inv += p[i] > p[j];
        ll term = 1;
        for (int i = 0; i < n; i++) term = term * a[i][p[i]] % MOD;
        det = (inv & 1) ? norm(det - term) : (det + term) % MOD;
    } while (next_permutation(p.begin(), p.end()));
    return det;
}

static void test_gauss_real() {
    for (int it = 0; it < 200; it++) {
        int n = rnd(1, 7);
        vector<vector<double>> A(n, vector<double>(n));
        vector<double> x(n), b(n, 0);
        for (auto& r : A) for (auto& v : r) v = rnd(-6, 6);
        for (auto& v : x) v = rnd(-9, 9);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) b[i] += A[i][j] * x[j];
        vector<double> sol;
        int st = gauss(A, b, sol);
        // determinant vs exact integer brute force (entries small -> exact in mod arithmetic too)
        vector<vector<ll>> Am(n, vector<ll>(n));
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) Am[i][j] = norm((ll)A[i][j]);
        ll dm = det_brute_mod(Am);
        double dr = det_real(A);
        assert(norm(llround(dr)) == dm);
        if (dm != 0) {
            assert(st == 1);
            for (int i = 0; i < n; i++) assert(fabs(sol[i] - x[i]) < 1e-6);
            vector<vector<double>> inv;
            assert(inverse_real(A, inv));
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) {
                double s = 0;
                for (int k = 0; k < n; k++) s += A[i][k] * inv[k][j];
                assert(fabs(s - (i == j)) < 1e-7);
            }
        } else {
            assert(st == 2);                                  // consistent by construction, singular
            vector<double> chk(n, 0);
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) chk[i] += A[i][j] * sol[j];
            for (int i = 0; i < n; i++) assert(fabs(chk[i] - b[i]) < 1e-6);
        }
    }
    // inconsistent system: x + y = 1, x + y = 2
    {
        vector<double> sol;
        assert(gauss({{1, 1}, {1, 1}}, {1, 2}, sol) == 0);
    }
    puts("gauss (reals) ok");
}

static void test_gauss_mod() {
    for (int it = 0; it < 200; it++) {
        int n = rnd(1, 6);
        vector<vector<ll>> A(n, vector<ll>(n));
        for (auto& r : A) for (auto& v : r) v = rnd(0, MOD - 1);
        if (it % 4 == 0) A[n - 1] = A[0];                        // force singular sometimes
        assert(det_mod(A) == det_brute_mod(A));
        vector<ll> x(n), b(n, 0);
        for (auto& v : x) v = rnd(0, MOD - 1);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) b[i] = (b[i] + A[i][j] * x[j]) % MOD;
        vector<ll> sol;
        int st = gauss_mod(A, b, sol);
        assert(st != 0);
        for (int i = 0; i < n; i++) {                            // any returned solution must satisfy A sol = b
            ll s = 0;
            for (int j = 0; j < n; j++) s = (s + A[i][j] * sol[j]) % MOD;
            assert(s == b[i]);
        }
        if (det_mod(A) != 0) { assert(st == 1); assert(sol == x); }
    }
    puts("gauss (mod p) ok");
}

static void test_gf2() {
    for (int it = 0; it < 200; it++) {
        int n = rnd(1, 9), bits = rnd(1, 7);
        vector<ll> v(n);
        for (auto& x : v) x = rnd(0, (1LL << bits) - 1);
        // brute: all subset xors (as a set, and as multiset for kth with multiplicity)
        set<ll> span; vector<ll> all;
        for (int mask = 0; mask < (1 << n); mask++) {
            ll x = 0; for (int i = 0; i < n; i++) if (mask >> i & 1) x ^= v[i];
            span.insert(x); all.push_back(x);
        }
        // bitset gauss rank
        vector<bitset<MB>> rows(n);
        for (int i = 0; i < n; i++) for (int b = 0; b < bits; b++) rows[i][b] = v[i] >> b & 1;
        int rank = gauss_gf2(rows, bits);
        assert((1LL << rank) == (ll)span.size());
        // xor basis
        XorBasis xb;
        for (ll x : v) xb.insert(x);
        assert(xb.rank == rank);
        assert(xb.max_xor() == *span.rbegin());
        for (ll x = 0; x < (1LL << bits); x++) assert(xb.can_make(x) == (span.count(x) > 0));
        ll k = 1;
        for (ll x : span) assert(xb.kth(k++) == x);              // set iterates in increasing order
        // K Subset Xors with multiplicity: each value appears 2^(n-rank) times
        sort(all.begin(), all.end());
        for (int idx = 0; idx < (int)all.size(); idx++) {
            ll j = idx >> (n - rank);
            assert(xb.kth(j + 1) == all[idx]);
        }
        // linear system over GF(2): random A (n x n), b = A x
        int m = rnd(1, 8);
        vector<bitset<MB>> sys(m); bitset<MB> xs;
        for (int j = 0; j < m; j++) xs[j] = rng() & 1;
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < m; j++) sys[i][j] = rng() & 1;
            int rhs = 0; for (int j = 0; j < m; j++) rhs ^= (int(sys[i][j]) & int(xs[j]));
            sys[i][m] = rhs;
        }
        bitset<MB> sol;
        int st = solve_gf2(sys, m, sol);
        assert(st != 0);
        for (int i = 0; i < m; i++) {
            int s = 0; for (int j = 0; j < m; j++) s ^= (int(sys[i][j]) & int(sol[j]));
            assert(s == sys[i][m]);
        }
    }
    {   // inconsistent GF(2) system: x = 0, x = 1
        vector<bitset<MB>> sys(2); sys[0][0] = 1; sys[1][0] = 1; sys[1][1] = 1;
        bitset<MB> sol; assert(solve_gf2(sys, 1, sol) == 0);
    }
    puts("gf(2) gauss / xor basis ok");
}

struct DSU {
    vector<int> p;
    explicit DSU(int n) : p(n) { iota(p.begin(), p.end(), 0); }
    int find(int x) { return p[x] == x ? x : p[x] = find(p[x]); }
    bool unite(int a, int b) { a = find(a); b = find(b); if (a == b) return false; p[a] = b; return true; }
};
static ll spanning_trees_brute(int n, const vector<pair<int,int>>& e) {
    int m = e.size(); ll cnt = 0;
    for (int mask = 0; mask < (1 << m); mask++) {
        if (__builtin_popcount(mask) != n - 1) continue;
        DSU d(n); bool ok = true;
        for (int i = 0; i < m && ok; i++) if (mask >> i & 1) ok = d.unite(e[i].first, e[i].second);
        cnt += ok;
    }
    return cnt;
}
static void test_kirchhoff_markov() {
    for (int n = 2; n <= 8; n++) {                          // K_n: Cayley n^{n-2}
        vector<pair<int,int>> e;
        for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) e.push_back({i, j});
        assert(kirchhoff(n, e) == power(n, n - 2));
    }
    for (int n = 3; n <= 10; n++) {                         // cycle C_n: n trees
        vector<pair<int,int>> e;
        for (int i = 0; i < n; i++) e.push_back({i, (i + 1) % n});
        assert(kirchhoff(n, e) == n);
    }
    for (int it = 0; it < 100; it++) {                      // random multigraphs vs brute force
        int n = rnd(1, 6), m = rnd(0, 9);
        vector<pair<int,int>> e;
        for (int i = 0; i < m; i++) { int u = rnd(0, n - 1), v = rnd(0, n - 1); if (u != v) e.push_back({u, v}); }
        assert(kirchhoff(n, e) == spanning_trees_brute(n, e));
    }
    // Markov: walk on {0..n}, reflect at 0, absorb at n: E_0 = n^2
    for (int n = 1; n <= 40; n++) {
        vector<vector<double>> P(n, vector<double>(n, 0));    // transient states 0..n-1
        P[0][1 % n] += 1.0;                                    // from 0 always to 1 (if n == 1, to absorbing)
        if (n == 1) P[0][0] = 0;
        for (int i = 1; i < n; i++) { P[i][i - 1] += 0.5; if (i + 1 < n) P[i][i + 1] += 0.5; }
        vector<double> E = expected_steps(P);
        assert(fabs(E[0] - (double)n * n) < 1e-6);
    }
    puts("kirchhoff / markov expected steps ok");
}

int main() {
    test_matrix_power();
    test_kitamasa_bm();
    test_semirings();
    test_gauss_real();
    test_gauss_mod();
    test_gf2();
    test_kirchhoff_markov();
    puts("all tests passed");
    return 0;
}
