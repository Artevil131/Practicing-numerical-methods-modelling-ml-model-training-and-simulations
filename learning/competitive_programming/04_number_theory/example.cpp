// Chapter 04 — Number theory: reference library.
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
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <numeric>
#include <random>
#include <tuple>
#include <unordered_map>
#include <vector>
using namespace std;
using ll  = long long;
using ull = unsigned long long;
using u128 = unsigned __int128;

static mt19937_64 rng(20260905);
static ll rnd(ll lo, ll hi) { return lo + (ll)(rng() % (ull)(hi - lo + 1)); }

// ───────────────────────── 1. gcd / lcm / extended Euclid ───────────────────
ll gcd_euclid(ll a, ll b) { while (b) { a %= b; swap(a, b); } return a; }   // O(log min(a,b))

// Binary gcd: only shifts and subtractions; ~2x faster than % on 64-bit.
ull binary_gcd(ull a, ull b) {
    if (!a) return b; if (!b) return a;
    int sh = __builtin_ctzll(a | b);
    a >>= __builtin_ctzll(a);
    do { b >>= __builtin_ctzll(b); if (a > b) swap(a, b); b -= a; } while (b);
    return a << sh;
}
ll lcm_safe(ll a, ll b) { return a / gcd_euclid(a, b) * b; }   // divide first: avoids a*b overflow

// Returns g = gcd(a,b) and x,y with a*x + b*y = g (Bézout). |x| <= b/g, |y| <= a/g.
ll extgcd(ll a, ll b, ll& x, ll& y) {
    if (b == 0) { x = 1; y = 0; return a; }
    ll x1, y1, g = extgcd(b, a % b, x1, y1);
    x = y1; y = x1 - (a / b) * y1;
    return g;
}
// Same, iterative (no recursion, same bounds on x,y).
ll extgcd_iter(ll a, ll b, ll& x, ll& y) {
    ll x0 = 1, y0 = 0, x1 = 0, y1 = 1;
    while (b) {
        ll q = a / b;
        tie(a, b)   = make_pair(b, a - q * b);
        tie(x0, x1) = make_pair(x1, x0 - q * x1);
        tie(y0, y1) = make_pair(y1, y0 - q * y1);
    }
    x = x0; y = y0; return a;
}

// Linear Diophantine a*x + b*y = c. Returns false if no solution. Otherwise one
// solution (x0,y0); all solutions: x0 + k*(b/g), y0 - k*(a/g), k in Z.
bool diophantine(ll a, ll b, ll c, ll& x0, ll& y0, ll& g) {
    g = extgcd(llabs(a), llabs(b), x0, y0);
    if (g == 0) return c == 0;             // a = b = 0
    if (c % g) return false;
    x0 *= c / g; y0 *= c / g;
    if (a < 0) x0 = -x0;
    if (b < 0) y0 = -y0;
    return true;
}

void test_gcd() {
    for (int it = 0; it < 20000; it++) {
        ll a = rnd(0, 1000000), b = rnd(0, 1000000);
        ll g = gcd_euclid(a, b);
        assert(g == (ll)binary_gcd(a, b) && g == std::gcd(a, b));
        ll x, y, x2, y2;
        ll g1 = extgcd(a, b, x, y), g2 = extgcd_iter(a, b, x2, y2);
        assert(g1 == g && g2 == g && a * x + b * y == g && a * x2 + b * y2 == g);
        if (g) assert(llabs(x) <= max(1LL, b / g) && llabs(y) <= max(1LL, a / g));
        if (a && b) assert(lcm_safe(a, b) == a / g * b);
    }
    for (int it = 0; it < 5000; it++) {
        ll a = rnd(-50, 50), b = rnd(-50, 50), c = rnd(-200, 200), x0, y0, g;
        bool ok = diophantine(a, b, c, x0, y0, g);
        bool brute = false;
        for (ll x = -200; x <= 200 && !brute; x++) for (ll y = -200; y <= 200; y++) if (a * x + b * y == c) { brute = true; break; }
        assert(ok == brute);
        if (ok) assert(a * x0 + b * y0 == c);
    }
}

// ───────────────────────── 2. Modular arithmetic ────────────────────────────
inline ll norm(ll a, ll m) { a %= m; if (a < 0) a += m; return a; }        // C++ % keeps the sign of a
ll mulmod(ll a, ll b, ll m) { return (ll)((u128)a * b % m); }               // any m < 2^63
ll power(ll b, ll e, ll m) {                                                // O(log e); needs e >= 0
    ll r = 1 % m; b = norm(b, m);
    for (; e > 0; e >>= 1) { if (e & 1) r = mulmod(r, b, m); b = mulmod(b, b, m); }
    return r;
}
ll inv_fermat(ll a, ll p) { return power(a, p - 2, p); }                     // p prime, a % p != 0
ll inv_ext(ll a, ll m) {                                                    // gcd(a,m) = 1, any m
    ll x, y, g = extgcd(norm(a, m), m, x, y);
    assert(g == 1);
    return norm(x, m);
}
// inv[i] for all 1 <= i <= n in O(n), p prime, n < p.
// Proof: p = q*i + r with q = p/i, r = p%i.  q*i + r ≡ 0  ⇒  i^{-1} ≡ -q * r^{-1}.
vector<ll> all_inverses(int n, ll p) {
    vector<ll> inv(n + 1, 1);
    for (int i = 2; i <= n; i++) inv[i] = (p - (p / i) * inv[p % i] % p) % p;
    return inv;
}

// Chinese Remainder Theorem, moduli need NOT be coprime. Solves x ≡ r[i] (mod m[i]).
// Returns {x, M} with 0 <= x < M = lcm(m), or {-1, 0} if inconsistent.
// Merge step: x + M*t ≡ r (mod m)  ⇔  M*t ≡ r - x (mod m); solvable iff g | (r-x)
// with g = gcd(M, m); then t is unique mod m/g. New modulus lcm(M, m) = M * (m/g).
pair<ll, ll> crt(const vector<ll>& r, const vector<ll>& m) {
    ll x = 0, M = 1;
    for (size_t i = 0; i < r.size(); i++) {
        ll p, q, g = extgcd(M, m[i], p, q);                 // M*p + m[i]*q = g
        ll diff = norm(r[i], m[i]) - x;                     // may be negative
        if (diff % g != 0) return {-1, 0};
        ll m2 = m[i] / g;
        ll t = mulmod(norm(diff / g, m2), norm(p, m2), m2); // t = (diff/g) * (M/g)^{-1} mod m2
        x += M * t;                                          // < M * m2 = new M
        M *= m2;
        x %= M;
    }
    return {x, M};
}

void test_modular() {
    const ll P = 1000000007;
    assert(norm(-7, 5) == 3 && norm(7, 5) == 2 && (-7 % 5) == -2);
    assert(power(2, 10, P) == 1024 && power(2, 0, 1) == 0 && power(0, 0, P) == 1);
    for (int it = 0; it < 2000; it++) {
        ll a = rnd(1, P - 1);
        ll i1 = inv_fermat(a, P), i2 = inv_ext(a, P);
        assert(i1 == i2 && mulmod(a, i1, P) == 1);
        ll b = rnd(0, 1000), e = rnd(0, 60), brute = 1;
        for (int k = 0; k < e; k++) brute = brute * b % P;
        assert(power(b, e, P) == brute);
    }
    auto inv = all_inverses(3000, P);
    for (int i = 1; i <= 3000; i++) assert(inv[i] == inv_fermat(i, P));
    // composite modulus: inverse exists iff coprime
    assert(inv_ext(3, 10) == 7 && inv_ext(7, 10) == 3);
    // CRT vs brute force (small moduli, non-coprime allowed)
    for (int it = 0; it < 3000; it++) {
        int k = (int)rnd(1, 4);
        vector<ll> m(k), r(k);
        for (int i = 0; i < k; i++) { m[i] = rnd(1, 12); r[i] = rnd(-20, 20); }
        ll L = 1; for (ll mi : m) L = lcm_safe(L, mi);
        ll brute = -1;
        for (ll x = 0; x < L && brute < 0; x++) { bool ok = true; for (int i = 0; i < k; i++) ok &= (norm(x - r[i], m[i]) == 0); if (ok) brute = x; }
        auto [x, M] = crt(r, m);
        if (brute < 0) assert(x == -1);
        else { assert(M == L && x == brute); }
    }
    assert(crt({2, 3, 2}, {3, 5, 7}).first == 23);       // Sun Tzu
    assert(crt({1, 3}, {4, 6}).first == 9 && crt({1, 3}, {4, 6}).second == 12);
    assert(crt({1, 2}, {4, 6}).first == -1);
}

// ───────────────────────── 3. Sieves ────────────────────────────────────────
// Eratosthenes, O(n log log n). is_prime[0] = is_prime[1] = false.
vector<char> sieve(int n) {
    vector<char> is(n + 1, 1); is[0] = 0; if (n >= 1) is[1] = 0;
    for (ll i = 2; i * i <= n; i++) if (is[i]) for (ll j = i * i; j <= n; j += i) is[j] = 0;
    return is;
}
// Linear sieve: O(n), also gives smallest prime factor spf[i]. Each composite
// j = i * p is crossed out exactly once, by its smallest prime p (p <= spf[i]).
vector<int> linear_sieve(int n, vector<int>& primes) {
    vector<int> spf(n + 1, 0); primes.clear();
    for (int i = 2; i <= n; i++) {
        if (spf[i] == 0) { spf[i] = i; primes.push_back(i); }
        for (int p : primes) { if (p > spf[i] || (ll)p * i > n) break; spf[p * i] = p; }
    }
    return spf;
}
// Segmented sieve over [L, R] with R - L <= ~1e7, R <= ~1e14: sieve primes to
// sqrt(R), cross multiples inside the window. Result[i] <=> L+i prime.
vector<char> segmented_sieve(ll L, ll R) {
    int s = (int)sqrtl((long double)R) + 1;
    vector<int> primes; linear_sieve(s, primes);
    vector<char> is(R - L + 1, 1);
    for (ll p : primes) {
        if (p * p > R) break;
        for (ll j = max(p * p, (L + p - 1) / p * p); j <= R; j += p) is[j - L] = 0;
    }
    for (ll v = L; v <= min(R, 1LL); v++) is[v - L] = 0;   // 0 and 1 are not prime
    return is;
}

void test_sieves() {
    const int N = 200000;
    auto is = sieve(N);
    vector<int> primes; auto spf = linear_sieve(N, primes);
    int cnt = 0;
    for (int i = 0; i <= N; i++) {
        assert(is[i] == (i >= 2 && spf[i] == i));
        if (is[i]) cnt++;
        if (i >= 2) { assert(i % spf[i] == 0); for (int d = 2; d < spf[i]; d++) assert(i % d != 0); }
    }
    assert(cnt == (int)primes.size() && cnt == 17984);       // pi(200000)
    assert(primes[0] == 2 && primes[24] == 97);              // 25 primes below 100
    for (int it = 0; it < 30; it++) {
        ll L = rnd(0, N - 5000), R = L + rnd(0, 5000);
        auto seg = segmented_sieve(L, R);
        for (ll v = L; v <= R; v++) assert(seg[v - L] == is[v]);
    }
    auto seg = segmented_sieve(1000000000000LL, 1000000000200LL);   // near 1e12
    int c12 = 0; for (char c : seg) c12 += c;
    assert(seg[39] == 1 && c12 > 0);                                 // 1e12 + 39 is prime
}

// ───────────────────────── 4. Factorization ─────────────────────────────────
using Fact = vector<pair<ll, int>>;                 // (prime, exponent), primes increasing

Fact factor_trial(ll n) {                            // O(sqrt n); fine to ~1e12
    Fact f;
    for (ll p = 2; p * p <= n; p++) if (n % p == 0) { int e = 0; while (n % p == 0) { n /= p; e++; } f.push_back({p, e}); }
    if (n > 1) f.push_back({n, 1});
    return f;
}
Fact factor_spf(int n, const vector<int>& spf) {     // O(log n) after a linear sieve
    Fact f;
    while (n > 1) { int p = spf[n], e = 0; while (n % p == 0) { n /= p; e++; } f.push_back({p, e}); }
    return f;
}

// Deterministic Miller–Rabin for all n < 2^64 with the first 12 primes as bases.
// n = 2^s * d + 1. a is a witness of compositeness unless a^d ≡ 1 or a^(2^r d) ≡ -1.
bool is_prime(ull n) {
    if (n < 2) return false;
    for (ull p : {2ULL, 3ULL, 5ULL, 7ULL, 11ULL, 13ULL, 17ULL, 19ULL, 23ULL, 29ULL, 31ULL, 37ULL})
        if (n % p == 0) return n == p;
    ull d = n - 1; int s = 0;
    while (!(d & 1)) { d >>= 1; s++; }
    auto mul = [&](ull a, ull b) { return (ull)((u128)a * b % n); };
    auto pw  = [&](ull b, ull e) { ull r = 1; for (; e; e >>= 1, b = mul(b, b)) if (e & 1) r = mul(r, b); return r; };
    for (ull a : {2ULL, 3ULL, 5ULL, 7ULL, 11ULL, 13ULL, 17ULL, 19ULL, 23ULL, 29ULL, 31ULL, 37ULL}) {
        ull x = pw(a, d);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (int r = 1; r < s && composite; r++) { x = mul(x, x); if (x == n - 1) composite = false; }
        if (composite) return false;
    }
    return true;
}

// Pollard's rho with Brent's cycle detection and batched gcd (product of 128
// differences, one gcd). Expected O(n^{1/4}) per factor. n must be composite,
// n < 2^63 (so mul + c does not overflow). Returns a nontrivial factor.
ull pollard_rho(ull n) {
    if (n % 2 == 0) return 2;
    auto mul = [&](ull a, ull b) { return (ull)((u128)a * b % n); };
    auto f   = [&](ull x, ull c) { ull r = mul(x, x) + c; return r >= n ? r - n : r; };
    while (true) {
        ull x = rnd(0, n - 1), y = x, c = rnd(1, n - 1), g = 1, q = 1, ys = 0;
        for (ull r = 1; g == 1; r <<= 1) {
            x = y;
            for (ull i = 0; i < r; i++) y = f(y, c);
            for (ull k = 0; k < r && g == 1; k += 128) {
                ys = y;
                for (ull i = 0; i < min<ull>(128, r - k); i++) { y = f(y, c); q = mul(q, x > y ? x - y : y - x); }
                g = std::gcd(q, n);
            }
        }
        if (g == n)                                   // batch overshot: redo step by step
            do { ys = f(ys, c); g = std::gcd(x > ys ? x - ys : ys - x, n); } while (g == 1);
        if (g != n) return g;
    }
}
void factor_rec(ull n, vector<ull>& out) {
    if (n == 1) return;
    if (is_prime(n)) { out.push_back(n); return; }
    ull d = pollard_rho(n);
    factor_rec(d, out); factor_rec(n / d, out);
}
Fact factor_fast(ull n) {                              // any n < 2^63, ~1e-5 s worst case
    vector<ull> ps; factor_rec(n, ps); sort(ps.begin(), ps.end());
    Fact f;
    for (ull p : ps) { if (!f.empty() && f.back().first == (ll)p) f.back().second++; else f.push_back({(ll)p, 1}); }
    return f;
}

// Divisor functions from a factorization.
ll num_divisors(const Fact& f) { ll d = 1; for (auto [p, e] : f) d *= e + 1; return d; }
ll sum_divisors_mod(const Fact& f, ll mod) {          // prod (p^(e+1)-1)/(p-1), mod prime, p-1 invertible
    ll s = 1;
    for (auto [p, e] : f) {
        ll num = (power(p, e + 1, mod) - 1 + mod) % mod;
        s = mulmod(s, mulmod(num, inv_fermat(norm(p - 1, mod), mod), mod), mod);
    }
    return s;
}
vector<ll> divisors(const Fact& f) {                   // unsorted, O(d(n))
    vector<ll> ds = {1};
    for (auto [p, e] : f) {
        size_t sz = ds.size(); ll pk = 1;
        for (int k = 1; k <= e; k++) { pk *= p; for (size_t i = 0; i < sz; i++) ds.push_back(ds[i] * pk); }
    }
    return ds;
}

void test_factorization() {
    const int N = 100000;
    vector<int> primes; auto spf = linear_sieve(N, primes);
    auto is = sieve(N);
    for (int n = 1; n <= N; n++) {
        assert(is_prime(n) == (bool)is[n]);
        if (n >= 2) assert(factor_spf(n, spf) == factor_trial(n));
    }
    // Miller–Rabin + Pollard rho vs trial division on random numbers up to 1e12
    for (int it = 0; it < 300; it++) {
        ll n = rnd(2, 1000000000000LL);
        Fact t = factor_trial(n);
        assert(factor_fast(n) == t);
        assert(is_prime(n) == (t.size() == 1 && t[0].second == 1));
    }
    // Semiprimes of two ~1e9 primes (trial division would need ~1e9 steps): known primes
    ll p1 = 1000000007, p2 = 998244353, p3 = 1000000009;
    Fact f = factor_fast((ull)p1 * p2);
    assert((f == Fact{{p2, 1}, {p1, 1}}));
    f = factor_fast((ull)p1 * p1);
    assert((f == Fact{{p1, 2}}));
    f = factor_fast((ull)p3 * 999999937);              // 999999937 is the largest prime < 1e9
    assert((f == Fact{{999999937, 1}, {p3, 1}}));
    assert(is_prime(p1) && is_prime(p2) && is_prime(p3) && !is_prime((ull)p1 * p2));
    // Carmichael numbers and strong pseudoprimes to base 2 must be rejected
    assert(!is_prime(561) && !is_prime(2047) && !is_prime(3215031751ULL) && !is_prime(4759123141ULL));
    assert(is_prime(18446744073709551557ULL));           // largest prime below 2^64
    assert(is_prime((1ULL << 61) - 1));                  // Mersenne prime M61
    // divisor functions
    for (int it = 0; it < 300; it++) {
        ll n = rnd(1, 100000);
        Fact ft = factor_trial(n);
        ll bd = 0, bs = 0; for (ll d = 1; d <= n; d++) if (n % d == 0) { bd++; bs += d; }
        assert(num_divisors(ft) == bd && sum_divisors_mod(ft, 1000000007) == bs);
        auto ds = divisors(ft); sort(ds.begin(), ds.end());
        assert((ll)ds.size() == bd); for (ll d : ds) assert(n % d == 0);
    }
}

// ───────────────────────── 5. Multiplicative functions ──────────────────────
ll phi_from_fact(ll n, const Fact& f) { for (auto [p, e] : f) { (void)e; n = n / p * (p - 1); } return n; }

// phi and mu for all 1..n via the linear sieve, O(n). For i = j * p with p = spf:
//   p | j  : phi(i) = phi(j) * p,     mu(i) = 0
//   p ∤ j  : phi(i) = phi(j) * (p-1), mu(i) = -mu(j)
void phi_mu_sieve(int n, vector<int>& phi, vector<int>& mu) {
    vector<int> primes, spf(n + 1, 0);
    phi.assign(n + 1, 0); mu.assign(n + 1, 0);
    phi[1] = mu[1] = 1;
    for (int i = 2; i <= n; i++) {
        if (!spf[i]) { spf[i] = i; primes.push_back(i); phi[i] = i - 1; mu[i] = -1; }
        for (int p : primes) {
            if (p > spf[i] || (ll)p * i > n) break;
            spf[p * i] = p;
            if (i % p == 0) { phi[p * i] = phi[i] * p;       mu[p * i] = 0; }
            else            { phi[p * i] = phi[i] * (p - 1); mu[p * i] = -mu[i]; }
        }
    }
}
// Divisor count and sum for all 1..n by the harmonic double loop, O(n log n).
void divisor_sieve(int n, vector<int>& cnt, vector<ll>& sum) {
    cnt.assign(n + 1, 0); sum.assign(n + 1, 0);
    for (int d = 1; d <= n; d++) for (int m = d; m <= n; m += d) { cnt[m]++; sum[m] += d; }
}

// Count pairs i<j with gcd(a_i, a_j) = 1, values <= V, via Möbius inversion:
//   #coprime pairs = sum_d mu(d) * C(c_d, 2), c_d = #elements divisible by d.
// O(V log V) after the sieve. (CSES Counting Coprime Pairs.)
ll coprime_pairs(const vector<int>& a, int V) {
    vector<int> phi, mu; phi_mu_sieve(V, phi, mu);
    vector<ll> freq(V + 1, 0); for (int x : a) freq[x]++;
    ll res = 0;
    for (int d = 1; d <= V; d++) {
        if (!mu[d]) continue;
        ll c = 0; for (int m = d; m <= V; m += d) c += freq[m];
        res += mu[d] * (c * (c - 1) / 2);
    }
    return res;
}

void test_multiplicative() {
    const int N = 20000;
    vector<int> phi, mu; phi_mu_sieve(N, phi, mu);
    vector<int> dc; vector<ll> dsum; divisor_sieve(N, dc, dsum);
    for (int n = 1; n <= N; n++) {
        Fact f = factor_trial(n);
        assert(phi[n] == phi_from_fact(n, f));
        bool sqfree = true; for (auto [p, e] : f) { (void)p; if (e > 1) sqfree = false; }
        assert(mu[n] == (sqfree ? (f.size() % 2 ? -1 : 1) : 0));
        assert(dc[n] == num_divisors(f));
        if (n <= 3000) { ll bp = 0; for (int k = 1; k <= n; k++) bp += (std::gcd(k, n) == 1); assert(bp == phi[n]); }
    }
    ll ds = 0; for (int n = 1; n <= N; n++) ds += dsum[n];
    ll dbrute = 0; for (int d = 1; d <= N; d++) dbrute += (ll)d * (N / d);
    assert(ds == dbrute);
    // sum_{d|n} phi(d) = n   and   sum_{d|n} mu(d) = [n == 1]
    for (int n = 1; n <= 2000; n++) {
        ll sp = 0, sm = 0; for (int d = 1; d <= n; d++) if (n % d == 0) { sp += phi[d]; sm += mu[d]; }
        assert(sp == n && sm == (n == 1));
    }
    for (int it = 0; it < 50; it++) {
        int n = (int)rnd(1, 60), V = (int)rnd(1, 100);
        vector<int> a(n); for (int& x : a) x = (int)rnd(1, V);
        ll brute = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) brute += (std::gcd(a[i], a[j]) == 1);
        assert(coprime_pairs(a, V) == brute);
    }
}

// ───────────────────────── 6. Primitive roots and discrete log ──────────────
// g is a primitive root mod prime p iff g^((p-1)/q) != 1 for every prime q | p-1.
ll primitive_root(ll p) {
    if (p == 2) return 1;
    Fact f = factor_fast(p - 1);
    for (ll g = 2; g < p; g++) {
        bool ok = true;
        for (auto [q, e] : f) { (void)e; if (power(g, (p - 1) / q, p) == 1) { ok = false; break; } }
        if (ok) return g;
    }
    return -1;
}
// Baby-step giant-step: smallest x >= 0 with a^x ≡ b (mod m), gcd(a, m) = 1.
// Write x = i*k - j (k = ceil sqrt m, 0 <= j < k, 1 <= i <= k): a^(ik) = b * a^j.
// Baby steps store b*a^j (keep the largest j); giant steps look up a^(ik). O(sqrt m).
ll bsgs(ll a, ll b, ll m) {
    a = norm(a, m); b = norm(b, m);
    if (m == 1 || b == 1 % m) return 0;
    ll k = (ll)sqrtl((long double)m) + 1;
    unordered_map<ll, ll> baby; baby.reserve(k * 2);
    ll cur = b;
    for (ll j = 0; j < k; j++) { baby[cur] = j; cur = mulmod(cur, a, m); }
    ll ak = power(a, k, m); cur = ak;
    for (ll i = 1; i <= k; i++) {
        auto it = baby.find(cur);
        if (it != baby.end()) return i * k - it->second;
        cur = mulmod(cur, ak, m);
    }
    return -1;
}

void test_dlog() {
    for (ll p : {2LL, 3LL, 5LL, 7LL, 101LL, 998244353LL, 1000000007LL}) {
        ll g = primitive_root(p);
        assert(g >= 1);
        if (p <= 101) {                                   // brute: powers of g hit every residue
            vector<char> seen(p, 0); ll cur = 1; int distinct = 0;
            for (ll e = 0; e < p - 1; e++) { if (!seen[cur]) { seen[cur] = 1; distinct++; } cur = cur * g % p; }
            assert(distinct == p - 1);
        }
    }
    assert(primitive_root(998244353) == 3);               // the NTT generator
    assert(primitive_root(1000000007) == 5);
    for (int it = 0; it < 300; it++) {
        ll m = rnd(2, 500), a = rnd(1, m - 1);
        if (std::gcd(a, m) != 1) continue;
        ll b = rnd(0, m - 1);
        ll brute = -1, cur = 1 % m;
        for (ll x = 0; x < m && brute < 0; x++) { if (cur == b) brute = x; cur = cur * a % m; }
        assert(bsgs(a, b, m) == brute);
    }
    assert(bsgs(3, power(3, 123456789, 998244353), 998244353) == 123456789);
    assert(bsgs(2, 1, 7) == 0 && bsgs(2, 3, 7) == -1);   // 2 has order 3 mod 7: {1,2,4}
}

// ───────────────────────── 7. Fibonacci, Pisano, Josephus ───────────────────
// Fast doubling: returns (F(n), F(n+1)) mod m, O(log n). F(0)=0, F(1)=1.
//   F(2k)   = F(k) * (2F(k+1) - F(k))
//   F(2k+1) = F(k)^2 + F(k+1)^2
pair<ll, ll> fib(ll n, ll m) {
    if (n == 0) return {0, 1 % m};
    auto [a, b] = fib(n >> 1, m);
    ll c = mulmod(a, norm(2 * b - a, m), m);
    ll d = (mulmod(a, a, m) + mulmod(b, b, m)) % m;
    if (n & 1) return {d, (c + d) % m};
    return {c, d};
}
// Pisano period pi(m): F mod m is periodic; pi(10) = 60, pi(10^k) = 15*10^(k-1) for k>=3.
ll pisano(ll m) {
    if (m == 1) return 1;
    ll a = 0, b = 1;
    for (ll i = 1;; i++) { tie(a, b) = make_pair(b, (a + b) % m); if (a == 0 && b == 1) return i; }
}
// Josephus: n people in a circle, every k-th is removed; 0-indexed survivor.
// J(1) = 0, J(n) = (J(n-1) + k) mod n.  O(n).
ll josephus(ll n, ll k) { ll j = 0; for (ll i = 2; i <= n; i++) j = (j + k) % i; return j; }
// k = 2 closed form: with 2^m <= n < 2^(m+1), survivor (1-indexed) is 2(n - 2^m) + 1.
ll josephus2(ll n) { ll m = 63 - __builtin_clzll(n); return 2 * (n - (1LL << m)) + 1; }

void test_fib_josephus() {
    const ll P = 1000000007;
    ll a = 0, b = 1;
    for (int n = 0; n <= 5000; n++) { assert(fib(n, P).first == a); tie(a, b) = make_pair(b, (a + b) % P); }
    assert(fib(10, 1000).first == 55 && fib(100, P).first == 687995182);
    assert(pisano(10) == 60 && pisano(2) == 3 && pisano(1000) == 1500);
    for (ll n = 1; n <= 200; n++) {
        for (ll k = 1; k <= 5; k++) {
            vector<int> circle(n); iota(circle.begin(), circle.end(), 0);
            int idx = 0;
            while (circle.size() > 1) { idx = (idx + k - 1) % circle.size(); circle.erase(circle.begin() + idx); idx %= circle.size(); }
            assert(josephus(n, k) == circle[0]);
        }
        assert(josephus2(n) == josephus(n, 2) + 1);
    }
}

int main() {
    test_gcd();            puts("gcd / extgcd / diophantine  ok");
    test_modular();        puts("modular / inverses / CRT    ok");
    test_sieves();         puts("sieves                      ok");
    test_factorization();  puts("Miller-Rabin / Pollard rho  ok");
    test_multiplicative(); puts("phi / mu / divisor sieves   ok");
    test_dlog();           puts("primitive root / BSGS       ok");
    test_fib_josephus();   puts("fibonacci / josephus        ok");
    puts("all tests passed");
    return 0;
}
