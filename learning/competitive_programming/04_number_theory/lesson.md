# Chapter 04 — Number Theory

Reference library: `example.cpp` (re-type it from memory; that is the exercise).
Prerequisites: chapter 03 (`__int128`, mulmod, bit tricks), `../../c_learning/12_numbers_bits_floats/lesson.md`
(integer widths, overflow). Combinatorics (binomials, inclusion–exclusion on sets, Burnside) is
chapter 05; this chapter supplies the modular machinery it uses.

## What you'll be able to do after this chapter

- Compute gcd/lcm/Bézout coefficients and solve `ax + by = c`, `ax ≡ b (mod m)` correctly with
  negative inputs and near-64-bit magnitudes.
- Do every modular operation (`+ − × ÷ ^`) safely under 10⁹+7 and 998244353, including inverses
  of 1..n in O(n) and all-at-once factorial tables.
- Combine congruences with the Chinese Remainder Theorem, coprime or not.
- Sieve primes to 10⁷ in 50 ms (Eratosthenes / linear), sieve a window near 10¹² (segmented), and
  read off smallest prime factors for O(log n) factorization.
- Test primality of any 64-bit integer deterministically (Miller–Rabin) and factor it in
  microseconds (Pollard's rho with Brent).
- Compute φ, μ, d(n), σ(n) for all n ≤ 10⁷ with a sieve and apply Möbius inversion to
  "count pairs with gcd = 1 / gcd = k" questions.
- Find primitive roots and solve discrete logarithms in O(√m) (baby-step giant-step).
- Recognise the pitfalls: `a*b` overflow, negative `%`, `pow` with doubles, `1e9+7` vs `998244353`.

## Where this shows up in contests

| Signal in the statement                                        | Technique                                   | Placement                    |
|----------------------------------------------------------------|---------------------------------------------|------------------------------|
| "print the answer modulo 10⁹+7 / 998244353"                    | modular arithmetic, inverses, fast power    | everywhere                   |
| "number of divisors / sum of divisors" for many queries        | spf sieve or trial division to √n           | CSES Math, CF Div2 C         |
| `n ≤ 10⁶–10⁷` and "for every k count …"                        | multiplicative-function sieve, harmonic Σ n/d| CF 1500–2000                 |
| "pairs with gcd = 1", "gcd of subset = 1", "lcm of array"       | Möbius inversion / inclusion–exclusion      | CF Div2 E, CSES 2417         |
| a single `n ≤ 10¹⁸` "is it prime / factor it"                  | Miller–Rabin + Pollard rho                  | CF Div1 B–C, CSES Next Prime |
| `x ≡ aᵢ (mod mᵢ)` conditions, periodic events aligning         | CRT                                         | CF Div2 D                    |
| `a^x ≡ b (mod p)`, "smallest exponent", `p ≤ 10¹²`             | baby-step giant-step                        | CF Div1 C, CEOI-level        |
| `k ≤ 20` primes and "divisible by at least one"                | inclusion–exclusion over 2^k subsets        | CSES 2185                    |
| `n ≤ 10¹⁸` Fibonacci / linear recurrence mod m                 | fast doubling / matrix power (ch05)         | CSES 1722                    |

IOI itself contains little pure number theory; BOI/CEOI/JOI use gcd/sieve/inversion as
ingredients (typically subtask 2–3 of a counting problem). Codeforces uses it constantly.

Time budget for 1 s: sieve to 10⁷ (60 ms bitset-Eratosthenes, 40 ms linear), 10⁶ Miller–Rabin
tests on 64-bit numbers (≈ 1.5 s with `__int128` — too slow; sieve or precheck instead), 10⁵
Pollard factorizations of 10¹⁸ numbers (≈ 1 s), 10⁷ modular multiplications with `%` on 64-bit
(≈ 30 ms), 10⁶ `power()` calls (≈ 60 ms).

---

## 1. Divisibility, gcd, lcm

`a | b` ("a divides b") ⇔ `b % a == 0`. `gcd(a, b)` = largest d with `d | a` and `d | b`;
`gcd(a, 0) = a`; `lcm(a, b) · gcd(a, b) = a · b`.

**Euclid.** `gcd(a, b) = gcd(b, a mod b)`, because every common divisor of `a, b` divides
`a − qb = a mod b` and vice versa. Termination in O(log min(a,b)) steps: after two steps the
smaller argument at least halves (if `b > a/2` then `a mod b = a − b < a/2`; otherwise
`a mod b < b ≤ a/2`). Worst case is consecutive Fibonacci numbers: ~1.44 · log₂(min) steps —
about 90 steps for 64-bit inputs.

```cpp
ll gcd_euclid(ll a, ll b) { while (b) { a %= b; swap(a, b); } return a; }
ll lcm_safe(ll a, ll b) { return a / gcd_euclid(a, b) * b; }     // divide first
```

`std::gcd` and `std::lcm` exist in `<numeric>` (C++17); they return the type of the arguments
and `std::gcd(-4, 6) == 2` (absolute value). Note `lcm(a,b)` overflows `ll` whenever the true
value exceeds 9.2·10¹⁸ — check with `__int128` or by `a / g > LLONG_MAX / b` if the statement
allows large lcms.

**Binary gcd** (Stein): remove common factors of two, then repeatedly subtract and strip factors
of two. ~2× faster than `%`-based Euclid on 64-bit because division is the slow instruction. Use it
in Pollard's rho or when gcd is the hot loop (10⁸ calls).

```cpp
ull binary_gcd(ull a, ull b) {
    if (!a) return b; if (!b) return a;
    int sh = __builtin_ctzll(a | b);
    a >>= __builtin_ctzll(a);
    do { b >>= __builtin_ctzll(b); if (a > b) swap(a, b); b -= a; } while (b);
    return a << sh;
}
```

Facts you will use: `gcd(a, b) = gcd(a, b − a)` → gcd of an arithmetic-like sequence is gcd of
differences (CF "Row GCD"); `gcd(a₁..aₙ)` of a range via sparse table / segment tree since gcd is
associative and idempotent; the sequence `gcd(a₁), gcd(a₁,a₂), …` takes at most log₂(a₁)+1
distinct values — so "number of distinct gcds of subarrays ending at r" is O(log) per r
(CSES-style *distinct subarray gcds* trick: maintain the set of (gcd, count) pairs).

## 2. Extended Euclid, Bézout, Diophantine equations

**Bézout:** for all integers a, b there are x, y with `ax + by = gcd(a, b)`, and gcd(a,b) is the
smallest positive value of `ax + by`. Extended Euclid computes x, y along with the gcd.

```cpp
ll extgcd(ll a, ll b, ll& x, ll& y) {          // a*x + b*y = g = gcd(a,b), a,b >= 0
    if (b == 0) { x = 1; y = 0; return a; }
    ll x1, y1, g = extgcd(b, a % b, x1, y1);   // b*x1 + (a - (a/b)*b)*y1 = g
    x = y1; y = x1 - (a / b) * y1;             // => a*y1 + b*(x1 - (a/b)*y1) = g
    return g;
}
```

Correctness is the one-line rewrite in the comment. Bounds: the returned `|x| ≤ b/g`, `|y| ≤ a/g`
(provable by induction), so no intermediate overflows for 63-bit inputs. Depth ≤ ~90. The
iterative version in `example.cpp` keeps two rows `(x0, y0), (x1, y1)` and updates them with the
quotient, exactly like the remainders.

Trace `extgcd(240, 46)`:

```
(240,46) q=5 → (46,10) q=4 → (10,6) q=1 → (6,4) q=1 → (4,2) q=2 → (2,0): x=1,y=0
unwind: (4,2): x=0, y=1-2·0=1        4·0 + 2·1 = 2
        (6,4): x=1, y=0-1·1=-1       6·1 + 4·(-1) = 2
        (10,6): x=-1, y=1-1·(-1)=2   10·(-1) + 6·2 = 2
        (46,10): x=2, y=-1-4·2=-9    46·2 + 10·(-9) = 2
        (240,46): x=-9, y=2-5·(-9)=47   240·(-9) + 46·47 = -2160 + 2162 = 2 ✓
```

**Linear Diophantine `ax + by = c`.** Solvable ⇔ `g = gcd(a,b) | c`. One solution:
`(x₀, y₀) · (c/g)` from Bézout; all solutions `x = x₀ + k·(b/g)`, `y = y₀ − k·(a/g)`, k ∈ ℤ
(the homogeneous solutions of `ax + by = 0` are exactly multiples of `(b/g, −a/g)` because
`a/g` and `b/g` are coprime). To find the solution with smallest `x ≥ L`: `k = ⌈(L − x₀) / (b/g)⌉`
with a floor-division helper that is correct for negatives. To count solutions with `x ∈ [x₁,x₂]`,
`y ∈ [y₁,y₂]`: intersect the two ranges of k. Handle negative a or b by solving with |a|, |b| and
flipping the signs of x or y (`diophantine()` in `example.cpp`).

**Linear congruence `ax ≡ b (mod m)`:** this is `ax + my = b`. Solvable ⇔ `g = gcd(a, m) | b`;
then exactly g solutions mod m: `x₀ = (b/g) · inv(a/g mod m/g)`, and `x₀ + k·(m/g)` for
`k = 0..g−1`.

## 3. Modular arithmetic

Rules (all for a fixed modulus m ≥ 1): `(a ± b) mod m = ((a mod m) ± (b mod m)) mod m`,
`(a · b) mod m = ((a mod m)(b mod m)) mod m`, and division by b works iff `gcd(b, m) = 1`, via the
modular inverse. Exponents are **not** reduced mod m; they are reduced mod φ(m) (Euler) or `m−1`
(prime m, Fermat) — and only when the base is coprime to m.

C++ `%` takes the sign of the dividend: `-7 % 5 == -2`. Idioms:

```cpp
inline ll norm(ll a, ll m) { a %= m; if (a < 0) a += m; return a; }   // canonical [0, m)
// or in one expression: ((a % m) + m) % m
// add/sub without % when both operands are already in [0, m):
x += y; if (x >= m) x -= m;
x -= y; if (x < 0)  x += m;
```

Multiplication of two reduced residues < 10⁹+7 is < 1.0·10¹⁸ — fits `ll` (max 9.22·10¹⁸). That
is precisely why contest moduli are ≈ 10⁹: `a * b % m` is safe with `long long` operands. For
`m` up to 2⁶³ use `mulmod` from chapter 03 (`__int128`).

**Fast exponentiation** (binary / square-and-multiply), O(log e):

```cpp
ll power(ll b, ll e, ll m) {                 // e >= 0; power(x, 0, 1) == 0 correctly
    ll r = 1 % m; b = norm(b, m);
    for (; e > 0; e >>= 1) { if (e & 1) r = mulmod(r, b, m); b = mulmod(b, b, m); }
    return r;
}
```

Invariant: before each iteration `result · b^e` equals the original power. For 30-bit moduli
replace `mulmod` with `r * b % m`. Never use `std::pow` for integers: it returns `double` and
`pow(10, 15)` may print as `999999999999999`.

**Exponent towers** (CSES 1712 Exponentiation II, `a^(b^c) mod p`): reduce the exponent modulo
`p − 1` — valid when `p ∤ a` by Fermat; when `p | a` the answer is 0 unless the exponent is 0.
General m: `a^e ≡ a^(φ(m) + e mod φ(m))` for `e ≥ log₂ m` (Euler's theorem generalized) — the
`+φ(m)` is what makes it correct for bases not coprime to m.

## 4. Modular inverses

`a⁻¹ mod m` exists ⇔ `gcd(a, m) = 1`. Three ways:

| Method                        | Requirement           | Cost               | Code                                     |
|-------------------------------|-----------------------|--------------------|------------------------------------------|
| Fermat: `a^(m−2)`             | m prime               | O(log m) mults     | `power(a, m-2, m)`                       |
| Extended Euclid               | any m, gcd = 1        | O(log m) divisions | `extgcd(a, m, x, y); norm(x, m)`          |
| Euler: `a^(φ(m)−1)`           | any m, know φ(m)      | O(log m)           | rarely needed                            |
| All of 1..n at once           | m prime, n < m        | O(n)               | below                                    |

```cpp
vector<ll> all_inverses(int n, ll p) {       // inv[i] = i^{-1} mod p for 1 <= i <= n < p
    vector<ll> inv(n + 1, 1);
    for (int i = 2; i <= n; i++) inv[i] = (p - (p / i) * inv[p % i] % p) % p;
    return inv;
}
```

**Proof.** Write `p = q·i + r` with `q = ⌊p/i⌋`, `r = p mod i < i`. Mod p: `q·i + r ≡ 0`, so
`i ≡ −r · q⁻¹`, hence `i⁻¹ ≡ −q · r⁻¹`. Since `r < i`, `inv[r]` is already computed. ∎

Factorial tables (chapter 05 needs them): `fact[i] = fact[i−1]·i`, `inv_fact[n] = power(fact[n],
p−2)`, then `inv_fact[i−1] = inv_fact[i]·i` downward — one modular exponentiation for the whole
table instead of n. Trace `all_inverses(4, 7)`: inv[1]=1; i=2: p/i=3, p%i=1 → 7 − 3·1 = 4 (2·4=8≡1 ✓);
i=3: p/i=2, p%i=1 → 7−2·1 = 5 (3·5=15≡1 ✓); i=4: p/i=1, p%i=3 → 7 − 1·5 = 2 (4·2 = 8 ≡ 1 ✓).

## 5. Chinese Remainder Theorem

**Statement (coprime).** If `m₁..m_k` are pairwise coprime, the system `x ≡ rᵢ (mod mᵢ)` has
exactly one solution modulo `M = ∏ mᵢ`.

**Proof sketch.** The map `ℤ/M → ℤ/m₁ × … × ℤ/m_k`, `x ↦ (x mod mᵢ)` is a ring homomorphism.
It is injective: if `x ≡ y` mod every `mᵢ` then every `mᵢ | x − y`, and coprime moduli imply
`M | x − y`. Both sides have M elements, so it is a bijection. Constructively, with
`Mᵢ = M / mᵢ` and `eᵢ = Mᵢ · (Mᵢ⁻¹ mod mᵢ)`, `eᵢ ≡ 1 (mod mᵢ)` and `≡ 0 (mod mⱼ, j ≠ i)`, so
`x = Σ rᵢ eᵢ` works. ∎

**Non-coprime moduli.** Merge one congruence at a time. Suppose `x ≡ r (mod M)` so far and the
new one is `x ≡ s (mod m)`. Write `x = r + M·t`; need `M·t ≡ s − r (mod m)`. With `g = gcd(M, m)`
this is solvable iff `g | (s − r)`, and then `t ≡ (s−r)/g · (M/g)⁻¹ (mod m/g)`. The new modulus
is `lcm(M, m) = M · (m/g)`. Do the multiplication of `M` by `t` in `__int128` if `lcm` approaches
10¹⁸.

```cpp
pair<ll, ll> crt(const vector<ll>& r, const vector<ll>& m) {    // {x, lcm} or {-1, 0}
    ll x = 0, M = 1;
    for (size_t i = 0; i < r.size(); i++) {
        ll p, q, g = extgcd(M, m[i], p, q);              // M*p + m[i]*q = g  ⇒ p = (M/g)^{-1} mod m[i]/g
        ll diff = norm(r[i], m[i]) - x;
        if (diff % g != 0) return {-1, 0};
        ll m2 = m[i] / g;
        ll t = mulmod(norm(diff / g, m2), norm(p, m2), m2);
        x += M * t;  M *= m2;  x %= M;
    }
    return {x, M};
}
```

Trace Sun Tzu: `x ≡ 2 (3), 3 (5), 2 (7)`. Start x=0,M=1. i=0: g=1, diff=2, t=2 → x=2, M=3.
i=1: extgcd(3,5): 3·2 + 5·(−1) = 1 → p=2; diff = 3−2 = 1; t = 1·2 mod 5 = 2 → x = 2+3·2 = 8, M=15.
i=2: extgcd(15,7): 15·1 + 7·(−2) = 1 → p=1; diff = 2−8 = −6; t = norm(−6,7)·1 = 1 → x = 8+15 = 23,
M = 105. Check 23 mod 3,5,7 = 2,3,2 ✓.

Uses: combining answers computed mod several small primes (Garner's algorithm for big
integers / large-modulus NTT); "the k-th time all planets align"; problems with `x mod a = b`
constraints (CF 687B "Remainders Game": can `x mod k` be deduced ⇔ `k | lcm(cᵢ)`).

## 6. Sieves

### 6.1 Eratosthenes, O(n log log n)

```cpp
vector<char> sieve(int n) {                    // is[i] == 1 iff i prime
    vector<char> is(n + 1, 1); is[0] = 0; if (n >= 1) is[1] = 0;
    for (ll i = 2; i * i <= n; i++) if (is[i]) for (ll j = i * i; j <= n; j += i) is[j] = 0;
    return is;
}
```

Start at `i·i` (smaller multiples already have a smaller prime factor); stop the outer loop at √n.
Σ over primes p ≤ n of n/p = n · log log n (Mertens). `vector<char>` (not `vector<bool>`: bit
packing costs speed here) to 10⁷: 10 MB, ~60 ms. To 10⁸: use `bitset` / only odd numbers, ~1 s.

The same loop shape computes a multiplicative function for all i: replace the inner marking with
"apply factor p to i" — see §8.

### 6.2 Linear sieve with smallest prime factor, O(n)

```cpp
vector<int> linear_sieve(int n, vector<int>& primes) {   // spf[i] = smallest prime factor
    vector<int> spf(n + 1, 0); primes.clear();
    for (int i = 2; i <= n; i++) {
        if (spf[i] == 0) { spf[i] = i; primes.push_back(i); }
        for (int p : primes) { if (p > spf[i] || (ll)p * i > n) break; spf[p * i] = p; }
    }
    return spf;
}
```

**Why linear.** Composite `c` is written uniquely as `c = p · i` with `p = spf(c)` and
`p ≤ spf(i)`. The inner loop generates exactly these pairs (`p` runs over primes up to `spf(i)`),
so each composite is assigned once. Total work = #composites + #primes = O(n). In practice it is
only slightly faster than Eratosthenes, but it yields `spf` for free, which gives O(log n)
factorization of any `n` in range and O(1) recurrences for multiplicative functions.

Trace `n = 10`: i=2 prime, mark 4. i=3 prime, mark 6, 9. i=4 (spf 2): mark 8; p=3 > spf(4) → break.
i=5 prime: mark 10. i=6 (spf 2): 12 > n → break. i=7 prime. i=8,9,10: nothing in range.
Each of 4,6,8,9,10 marked exactly once.

### 6.3 Segmented sieve for a window [L, R]

Primes in `[L, R]` with `R ≤ 10¹²–10¹⁴`, `R − L ≤ 10⁷`: sieve primes up to √R, then for each
prime `p` mark multiples in the window starting at `max(p², ⌈L/p⌉·p)`. Cost O((R−L) log log R
+ √R). Memory O(R − L). Also the tool for "primes in [L, R]" and for sieving `10⁸..10⁸+10⁶`
without allocating 10⁸.

```cpp
vector<char> segmented_sieve(ll L, ll R) {
    int s = (int)sqrtl((long double)R) + 1;
    vector<int> primes; linear_sieve(s, primes);
    vector<char> is(R - L + 1, 1);
    for (ll p : primes) {
        if (p * p > R) break;
        for (ll j = max(p * p, (L + p - 1) / p * p); j <= R; j += p) is[j - L] = 0;
    }
    for (ll v = L; v <= min(R, 1LL); v++) is[v - L] = 0;
    return is;
}
```

### 6.4 How many primes? (intuition for constants)

`π(n) ≈ n / ln n`: 78498 below 10⁶, 664579 below 10⁷, 5.08·10⁷ below 10⁹, 2.5·10¹⁷ below 10¹⁸.
The n-th prime ≈ n ln n. Gaps between consecutive primes near 10¹⁸ average 41 and never exceed
~1500 — so "next prime after n" (CSES 3396) tests only a few hundred candidates with Miller–Rabin.
Density of numbers ≤ n with no prime factor > √n is bounded; a random 10¹⁸ number has ~3 prime
factors on average and d(n) ≤ 103680 for n ≤ 10¹⁸ (max divisor count), ≤ 1344 for n ≤ 10⁹,
≤ 6720 for n ≤ 10¹².

## 7. Factorization

| Method                     | Range          | Cost                           |
|----------------------------|----------------|--------------------------------|
| trial division to √n       | n ≤ 10¹²       | O(√n) ≈ 10⁶ per number         |
| `spf` after linear sieve   | n ≤ 10⁷–10⁸    | O(log n) per number            |
| trial by primes to √n only | n ≤ 10¹⁴       | O(√n / ln √n) ≈ 10⁶ per number |
| Miller–Rabin + Pollard rho | n < 2⁶⁴        | O(n^{1/4}) ≈ 10⁵ mulmods worst |

```cpp
using Fact = vector<pair<ll, int>>;            // (prime, exponent), increasing primes
Fact factor_trial(ll n) {
    Fact f;
    for (ll p = 2; p * p <= n; p++) if (n % p == 0) { int e = 0; while (n % p == 0) { n /= p; e++; } f.push_back({p, e}); }
    if (n > 1) f.push_back({n, 1});            // leftover is prime
    return f;
}
Fact factor_spf(int n, const vector<int>& spf) {
    Fact f;
    while (n > 1) { int p = spf[n], e = 0; while (n % p == 0) { n /= p; e++; } f.push_back({p, e}); }
    return f;
}
```

Write `p * p <= n` (not `p <= sqrt(n)` with doubles) and use `ll` for `p` so `p*p` cannot overflow
`int` at `p ≈ 46341`.

### 7.1 Miller–Rabin, deterministic for 64-bit

Write `n − 1 = 2^s · d` with `d` odd. If n is prime then for every `a` not divisible by n, the
sequence `a^d, a^{2d}, …, a^{2^s d} = a^{n−1} ≡ 1` either starts at 1 or contains −1 (because
the only square roots of 1 mod a prime are ±1). A base `a` for which this fails is a *witness*
of compositeness. For composite n at most 1/4 of the bases fail to witness, so random bases give
error 4^{−k}; better, testing the first 12 primes `2, 3, 5, …, 37` is **deterministic for all
n < 2⁶⁴** (Jaeschke / Sorenson–Webster), and `{2, 3, 5, 7}` suffices for n < 3.2·10⁹,
`{2, 3, 5, 7, 11, 13, 17}` for n < 3.4·10¹⁴.

```cpp
bool is_prime(ull n) {
    if (n < 2) return false;
    for (ull p : {2,3,5,7,11,13,17,19,23,29,31,37}) if (n % p == 0) return n == p;
    ull d = n - 1; int s = 0;
    while (!(d & 1)) { d >>= 1; s++; }
    for (ull a : {2,3,5,7,11,13,17,19,23,29,31,37}) {
        ull x = powmod(a, d, n);                     // mulmod via unsigned __int128
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (int r = 1; r < s && composite; r++) { x = mulmod(x, x, n); if (x == n - 1) composite = false; }
        if (composite) return false;
    }
    return true;
}
```

Cost: 12 exponentiations of 64 squarings each ≈ 800 mulmods ≈ 20–40 µs with `__int128`. The
small-prime pre-check makes `a < n` automatically for the remaining n. Carmichael numbers (561,
1105, 1729, …) fool Fermat's test but not Miller–Rabin; strong pseudoprimes to base 2 (2047,
3215031751) are the reason for multiple bases — `example.cpp` asserts on them.

### 7.2 Pollard's rho (Brent variant)

Iterate `x ↦ x² + c mod n`. Modulo a prime factor `p` of n the sequence enters a cycle after
≈ √p steps (birthday paradox), and when `xᵢ ≡ xⱼ (mod p)` but `xᵢ ≠ xⱼ (mod n)`, `gcd(|xᵢ − xⱼ|, n)`
is a nontrivial factor. Brent's cycle finding compares against a saved point `x` at power-of-two
positions; multiplying 128 differences together before one gcd amortizes the expensive gcd. If
the batched product becomes 0 mod n (overshoot), redo that block step by step. If `g = n`
anyway, restart with a new `c`. Expected O(n^{1/4}) mulmods per factor — ≈ 3·10⁴ for `n ≈ 10¹⁸`,
≈ 1 ms worst case; practically 50–100 µs.

```cpp
ull pollard_rho(ull n) {                      // n composite, n < 2^63; returns a nontrivial factor
    if (n % 2 == 0) return 2;
    auto mul = [&](ull a, ull b) { return (ull)((unsigned __int128)a * b % n); };
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
        if (g == n) do { ys = f(ys, c); g = std::gcd(x > ys ? x - ys : ys - x, n); } while (g == 1);
        if (g != n) return g;
    }
}
void factor_rec(ull n, vector<ull>& out) {
    if (n == 1) return;
    if (is_prime(n)) { out.push_back(n); return; }
    ull d = pollard_rho(n); factor_rec(d, out); factor_rec(n / d, out);
}
```

Always test `is_prime` before calling rho (rho loops forever on primes) and strip small primes
first (rho is slow on `n = 4`, and `x² + c` mod 4 can cycle badly). `std::gcd` on `unsigned long
long` is fine; use `binary_gcd` if profiling says so.

### 7.3 Divisor functions from a factorization

`n = ∏ pᵢ^{eᵢ}` ⇒ `d(n) = ∏ (eᵢ + 1)`, `σ(n) = ∏ (pᵢ^{eᵢ+1} − 1)/(pᵢ − 1)` (geometric series; mod
a prime use the inverse of `pᵢ − 1` — CSES 2182 Divisor Analysis, where exponents are up to 10⁹
so compute `pᵢ^{eᵢ+1}` by fast power), product of divisors `= n^{d(n)/2}` (if `d(n)` is odd, n is
a square and `√n^{d(n)}`; mod p compute as `∏ pᵢ^{eᵢ · d(n) / 2}` with the exponent reduced mod
`p − 1`, halving carefully: divide `d(n)` by 2 if even, else divide `eᵢ` — exactly one of them
is even). Enumerating all divisors: multiply out prime powers, O(d(n)).

## 8. Multiplicative functions, φ, μ, Möbius inversion

`f` is *multiplicative* if `f(ab) = f(a) f(b)` for coprime a, b. Then `f` is determined by its
values on prime powers, and a linear sieve computes it for all `1..n` in O(n): for `i = j · p`
with `p = spf(i)`, either `p ∤ j` (coprime split: `f(i) = f(j) f(p)`) or `p | j` (need the
prime-power rule for the specific function).

| Function        | on `p^e`                     | linear-sieve rule when `p \| j`                  | identity                           |
|-----------------|------------------------------|--------------------------------------------------|------------------------------------|
| φ (totient)     | `p^{e−1}(p−1)`               | `φ(jp) = φ(j) · p`                               | `Σ_{d\|n} φ(d) = n`                |
| μ (Möbius)      | `−1` if e=1, `0` if e≥2      | `μ(jp) = 0`                                      | `Σ_{d\|n} μ(d) = [n = 1]`          |
| d (divisors)    | `e + 1`                      | track the exponent of `spf`: `d(jp) = d(j)/(e+1)·(e+2)` | `Σ d(k) = Σ_d ⌊n/d⌋`         |
| σ (divisor sum) | `(p^{e+1}−1)/(p−1)`          | track `p^e` part: `σ(jp) = σ(j)·p + σ(j / p^e)`  | `Σ σ(k) = Σ_d d·⌊n/d⌋`             |

```cpp
void phi_mu_sieve(int n, vector<int>& phi, vector<int>& mu) {
    vector<int> primes, spf(n + 1, 0);
    phi.assign(n + 1, 0); mu.assign(n + 1, 0); phi[1] = mu[1] = 1;
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
```

**Euler's totient** `φ(n) = #{1 ≤ k ≤ n : gcd(k, n) = 1} = n ∏_{p | n} (1 − 1/p)`. From a
factorization: `n = n / p * (p − 1)` for each prime. Euler's theorem: `a^{φ(m)} ≡ 1 (mod m)` for
`gcd(a, m) = 1`. Also `Σ_{k ≤ n, gcd(k,n)=1} k = n φ(n) / 2` for n > 1.

**Harmonic sieve** for any "for each d, for each multiple" quantity: `for d: for m = d, 2d, …`
costs `Σ n/d = O(n log n)`; ≈ 1.6·10⁸ steps at n = 10⁷ — fine. This is the lazy way to get
d(k), σ(k), or `cnt[d] = #aᵢ divisible by d` for all d ≤ V.

**Möbius inversion.** If `g(n) = Σ_{d | n} f(d)` then `f(n) = Σ_{d | n} μ(d) g(n/d)`. The
"sum over multiples" form used in counting: if `G(d) = Σ_{d | k} F(k)` (F over multiples) then
`F(d) = Σ_{d | k} μ(k/d) G(k)`. Both follow from `Σ_{d | n} μ(d) = [n = 1]`, which holds because
for `n = p₁^{e₁}…p_r^{e_r}` only squarefree d contribute: `Σ_{S ⊆ primes} (−1)^{|S|} = (1−1)^r = 0`
for r ≥ 1.

**Worked example — Counting Coprime Pairs (CSES 2417).** Given `a₁..aₙ ≤ V = 10⁶`, count pairs
`i < j` with `gcd(aᵢ, aⱼ) = 1`. Let `c_d` = #elements divisible by d (harmonic sum over
`freq`). `G(d) = C(c_d, 2)` counts pairs whose gcd is a *multiple* of d. Want `F(1)` = pairs with
gcd exactly 1:

```
F(1) = Σ_d μ(d) · G(d) = Σ_d μ(d) · c_d (c_d − 1) / 2
```

Read it as inclusion–exclusion: subtract pairs both divisible by 2, by 3, by 5, …; add back
pairs divisible by 6, 10, 15, …; the coefficient of "divisible by d" is exactly μ(d). Complexity
O(V log V) for the harmonic loop after an O(V) sieve. Generalization: pairs with gcd exactly g —
`Σ_d μ(d) G(g·d)`; "number of subsets with gcd 1" — `Σ_d μ(d) (2^{c_d} − 1)` (CF 803F "Coprime
Subsequences"; CSES 3161 GCD Subsets asks for every gcd value: compute `H(d) = 2^{c_d} − 1` for
"gcd multiple of d", then `F(d) = Σ_{d | k} μ(k/d) H(k)` or simply subtract multiples top-down).

```cpp
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
```

**Floor-division blocks.** `Σ_{k=1}^{n} ⌊n/k⌋` (= Σ d(k), CSES 1082 Sum of Divisors uses
`Σ k⌊n/k⌋`) takes only O(√n) distinct values of `⌊n/k⌋`: for `k` in a block `[l, r]` with
`r = n / (n / l)` the quotient is constant. Loop `l = 1; while l ≤ n: q = n/l; r = n/q; add
(r−l+1)·q (or (l+r)(r−l+1)/2 · q mod p); l = r+1` — O(√n) even for n = 10¹²; mind the `/2` mod p
(multiply by inv(2)).

## 9. Primitive roots and discrete logarithm

The multiplicative group mod a prime p is cyclic of order `p − 1`: some `g` (primitive root) has
`{g⁰, g¹, …, g^{p−2}} = {1, …, p−1}`. There are `φ(p − 1)` primitive roots, so a random candidate
succeeds with probability ≥ ~1/(ln ln p), and the smallest one is tiny in practice (2, 3, 5 …).
Test: `g` is a primitive root ⇔ `g^{(p−1)/q} ≠ 1` for every prime `q | p − 1` (otherwise the
order of g divides `(p−1)/q`). Requires factoring `p − 1` — Pollard if p is large.

```cpp
ll primitive_root(ll p) {
    if (p == 2) return 1;
    Fact f = factor_fast(p - 1);
    for (ll g = 2; g < p; g++) {
        bool ok = true;
        for (auto [q, e] : f) if (power(g, (p - 1) / q, p) == 1) { ok = false; break; }
        if (ok) return g;
    }
    return -1;
}
```

`998244353 = 119 · 2²³ + 1` has primitive root 3; `10⁹+7` has 5. Primitive roots exist mod m
exactly for `m = 1, 2, 4, p^k, 2p^k` (odd p).

**Discrete logarithm — baby-step giant-step.** Solve `a^x ≡ b (mod m)`, `gcd(a, m) = 1`, in
O(√m) time and memory. Let `k = ⌈√m⌉` and write `x = i·k − j` with `0 ≤ j < k`, `1 ≤ i ≤ k`.
Then `a^{ik} ≡ b · a^j`. Baby steps: store `b·a^j → j` for all j in a hash map (keep the largest
j per value so the found x is minimal for that i). Giant steps: for `i = 1..k` look up `a^{ik}`;
the first hit gives the smallest x ≥ 1 (later i give larger x because `ik − j ≥ (i−1)k + 1`).
Check `b == 1` → x = 0 separately. Every x in `[0, m)` is covered because `i·k ≥ x+1 > x` for
some i ≤ k.

```cpp
ll bsgs(ll a, ll b, ll m) {                   // smallest x >= 0 with a^x = b mod m, or -1
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
```

`m ≤ 10¹²` → `k = 10⁶` hash entries, ~0.3 s; use a sorted vector + binary search or a custom hash
if the map is too slow. When `gcd(a, m) ≠ 1`: repeatedly divide out `g = gcd(a, m)` (if `g ∤ b` no
solution unless b = 1), reducing to the coprime case plus an offset — the cp-algorithms page has
the details. Uses: `x^k ≡ b` (discrete root: log base a primitive root, then a linear congruence
in the exponent), order of an element, "which term of the sequence 3^i mod p equals y" (CF 1106F).

## 10. Fibonacci, Pisano, Josephus

**Fast doubling** (CSES 1722 Fibonacci Numbers, n ≤ 10¹⁸): from `(F(k), F(k+1))` get
`F(2k) = F(k)(2F(k+1) − F(k))`, `F(2k+1) = F(k)² + F(k+1)²` — O(log n), no matrices. (Matrix
power `[[1,1],[1,0]]^n` is the general tool for linear recurrences — chapter 05.)

**Pisano period.** `F(n) mod m` is periodic with period `π(m)`; `π(10) = 60`, `π(10^k) = 15·10^{k−1}`
for k ≥ 3, `π(p) | p − 1` when `p ≡ ±1 (mod 5)`, `π(p) | 2(p+1)` when `p ≡ ±2 (mod 5)`, and
`π(m) ≤ 6m`. So `F(n) mod m` for huge n can be reduced to `n mod π(m)` — rarely needed when fast
doubling exists, but it explains "last digits of F(n)" puzzles and periodicity of `a^n mod m`
type sequences generally (Pigeonhole: any deterministic map on a finite set is eventually
periodic — the basis of Pollard rho, of "functional graph" problems, and of Planets Cycles).

**Josephus.** n people in a circle, every k-th removed; `J(1) = 0`, `J(n) = (J(n−1) + k) mod n`
(0-indexed survivor): after the first removal the circle is the same problem with n−1 people,
renumbered by a shift of k. O(n) per query. For k = 2, `J(n) = 2(n − 2^{⌊log₂ n⌋})` (0-indexed);
CSES 2164 Josephus Queries asks the position of the j-th removed person for n up to 10⁹: use the
recursion "the j-th removal with n people: if `j ≤ n/2` it is person `2j`; else it is the
`(j − n/2)`-th removal in the remaining `⌈n/2⌉` people, mapped back through the parity of n" —
O(log n) per query. CSES 2162/2163 Josephus Problem I/II (the full removal order) are simulation
with an order-statistics structure (Fenwick "find k-th" or `pb_ds`, chapter 07/09) in O(n log n).

## 11. Pitfalls

| Pitfall                                   | Fix                                                                 |
|-------------------------------------------|---------------------------------------------------------------------|
| `a * b % m` with `a, b` up to 10¹⁸ or `m > 3·10⁹` | `__int128` mulmod                                                |
| `(a − b) % m` negative                    | `norm()` or `((a-b) % m + m) % m`                                   |
| `%` on `int` intermediate: `int x = a * b % m` where `a*b` computed in `int` | make operands `ll` before multiplying |
| `pow(2, n)` with doubles                  | integer `power()`                                                   |
| exponent reduced mod m instead of `φ(m)`  | `e mod (p−1)` for prime p, `φ(m) + e mod φ(m)` otherwise            |
| inverse of a multiple of p                | doesn't exist; detect (e.g. `C(n, k) mod p` with `n ≥ p` needs Lucas) |
| `1 << 31` / `i * i` overflow in sieve loops | `ll` loop variables, `p * p <= n` with `ll p`                      |
| `vector<bool>` sieve is slow              | `vector<char>` or `bitset`                                          |
| Pollard on a prime / on n = 1             | check `is_prime` first, handle n = 1                                |
| `sqrt(n)` rounding (`(ll)sqrt(1e18)` off by one) | integer sqrt with correction: `while (r*r > n) r--; while ((r+1)*(r+1) <= n) r++;` |
| forgetting `1` is not prime, `0` in sieve | set both explicitly                                                 |
| lcm overflow                              | check `a / g > LLONG_MAX / b` or use `__int128`                     |
| `unordered_map` in BSGS hacked/slow       | custom hash (splitmix64) or sorted vector                           |

**Why 10⁹+7 and 998244353.** Both are primes below 2³⁰ (products of residues fit `ll`; sums of
two residues fit 32-bit unsigned). `998244353 = 119·2²³ + 1` has `2²³` dividing `p − 1`, so
the group has a primitive `2²³`-th root of unity (`3^{119}`), which is what the Number-Theoretic
Transform needs to convolve arrays of length up to 2²³ ≈ 8·10⁶ exactly. `10⁹+7 − 1 = 2 · 500000003`
has only one factor of two — no NTT. Other NTT primes: `7340033 = 7·2²⁰+1`, `167772161 = 5·2²⁵+1`,
`469762049 = 7·2²⁶+1`; three of them + CRT gives exact convolution with arbitrary modulus.
Anti-hash: with a known modulus, adversaries construct hash collisions; for string hashing use a
random base or the mod `2⁶¹−1` (Mersenne — fast reduction via shifts).

## Recognition cheatsheet

| Statement signal                                       | Technique                         | Complexity                |
|--------------------------------------------------------|-----------------------------------|---------------------------|
| answer mod prime, fractions appear                     | modular inverse (Fermat)          | O(log p)                  |
| `a^b^c mod p`                                          | exponent mod `p−1`                | O(log)                    |
| many "d(n), σ(n)" queries, n ≤ 10⁶                     | spf sieve + factor                | O(n) + O(log n) per query |
| single n ≤ 10¹² factor/divisors                        | trial division                    | O(√n)                     |
| n ≤ 10¹⁸ prime? / factor                               | Miller–Rabin / Pollard rho        | O(log³ n) / O(n^{1/4})    |
| primes in `[L, R]`, R huge, R−L ≤ 10⁷                  | segmented sieve                   | O((R−L) log log R)        |
| pairs/subsets with gcd = 1 or = g                      | Möbius over multiples             | O(V log V)                |
| `Σ_{k≤n} f(⌊n/k⌋)` with n ≤ 10¹²                       | floor blocks                      | O(√n)                     |
| `x ≡ rᵢ (mod mᵢ)` conditions                           | CRT (general)                     | O(k log M)                |
| `ax + by = c` integer solutions in a box               | extgcd + range of k               | O(log)                    |
| `a^x ≡ b (mod m)`, m ≤ 10¹²                            | BSGS                              | O(√m)                     |
| `x^k ≡ b (mod p)`                                      | primitive root + BSGS + lin. congruence | O(√p)               |
| F(n) mod m, n ≤ 10¹⁸                                   | fast doubling                     | O(log n)                  |
| "every k-th removed", n ≤ 10⁹, many queries            | Josephus recursion                | O(log n) per query        |
| "divisible by at least one of k ≤ 20 primes"           | inclusion–exclusion over subsets  | O(2^k)                    |
| lcm/gcd of a range, many queries                       | sparse table (gcd idempotent)     | O(n log n + q)            |

## Implementation checklist for contests

- Every modular multiplication has `ll` operands (or `mulmod`), every subtraction is followed by
  `+ m` normalization, every division is an inverse and the divisor is nonzero mod p.
- Exponents: reduced mod `p − 1`, never mod `p`; `power(x, 0) = 1`; `power(_, _, 1) = 0`.
- Sieve arrays sized `n + 1`; loops over `i * i <= n` in `ll`; `is[0] = is[1] = 0`.
- Factorization: leftover `n > 1` after the loop is prime — push it.
- Miller–Rabin before Pollard; handle `n ≤ 3`, even n, perfect powers only if the statement
  demands distinct treatment (rho handles `p²` fine).
- Möbius: skip `μ = 0` terms; `c_d (c_d − 1) / 2` in `ll`; result may be negative in intermediate
  steps — do not take mod too early with unsigned types.
- CRT: check `diff % g == 0`, keep `M` as `ll` only if `lcm ≤ 9·10¹⁸`, multiply with `__int128`.
- BSGS: `b == 1` → 0; reserve the hash map; use largest j on collisions.
- Output: the answer to "count" problems is often > 2³¹ even when mod is not requested (pairs of
  10⁵ → 5·10⁹): `long long`.
- Stress-test every function in this chapter against a brute force — `example.cpp` shows the
  pattern: random small inputs, exhaustive checks, 10³–10⁴ iterations.

## Further reading

- CPH: Ch. 21 Number theory (21.1 primes and factors, 21.2 modular arithmetic, 21.3 solving
  equations — CRT, extended Euclid, 21.4 other results: Lagrange, Zeckendorf, Pythagorean triples,
  Wilson).
- cp-algorithms.com: "Euclidean algorithm", "Extended Euclidean Algorithm", "Linear Diophantine
  Equations", "Modular Inverse", "Chinese Remainder Theorem", "Sieve of Eratosthenes", "Linear
  Sieve", "Primality tests", "Integer factorization" (Pollard rho, Brent), "Euler's totient
  function", "Number of divisors / sum of divisors", "Möbius function" (in the inclusion–exclusion
  article), "Primitive Root", "Discrete Logarithm", "Discrete Root", "Fibonacci Numbers" (fast
  doubling, Pisano), "Josephus Problem".
- Codeforces blog "Möbius inversion / Dirichlet convolution" tutorials for the `Σ_{d|n}` calculus;
  "Sum over floor(n/i)" (the O(√n) block trick).
- Deterministic Miller–Rabin base sets: Jaeschke (1993); the 12-prime set for 2⁶⁴ from Sorenson &
  Webster (2015). Brent, "An improved Monte Carlo factorization algorithm" (1980).
- Hardy & Wright, *An Introduction to the Theory of Numbers*, Ch. 5–6 (congruences, Fermat/Euler),
  Ch. 16–17 (arithmetic functions, Möbius) for proofs at full rigor.

## You can move on when...

- You can write `extgcd`, `power`, `all_inverses`, the linear sieve with spf, `phi_mu_sieve`,
  Miller–Rabin and Pollard rho from memory, each compiling first try and passing the brute-force
  tests in `example.cpp`.
- You can state and prove: Euclid's termination bound, the O(n) inverse recurrence, the
  linear-sieve "each composite once" argument, `Σ_{d|n} μ(d) = [n = 1]`, the CRT merge step.
- You've solved the CSES Mathematics number-theory tasks (Exponentiation I/II, Counting Divisors,
  Common Divisors, Sum of Divisors, Divisor Analysis, Prime Multiples, Counting Coprime Pairs, Next
  Prime, Fibonacci Numbers, Josephus Queries) and at least four Codeforces problems from
  `problems.md` at rating ≥ 1900.
- Given "count pairs / subsets with gcd = k" you reach for Möbius over multiples without hesitation,
  and given `n ≤ 10¹⁸` you know whether Pollard, trial division to 10⁶, or a sieve is the tool.
