# Chapter 05 — Combinatorics and Probability

Prerequisites: modular arithmetic, fast exponentiation and modular inverses (chapter 04 of this
course), DP fundamentals (`../../algorithms_learning/07_dynamic_programming/lesson.md`). Everything
below is computed modulo a prime `p` (usually `1e9+7` or `998244353`) unless stated otherwise.

## What you'll be able to do after this chapter

- Turn a counting statement into a closed formula or a DP, and know which one the constraints ask for.
- Compute `C(n,k) mod p` for `n ≤ 1e7` in O(1) per query, for `n` astronomically large with Lucas, and
  for `k` small with the falling-product formula.
- Apply stars and bars, inclusion–exclusion, Catalan/ballot counting, Burnside's lemma, Stirling/Bell
  numbers and integer partitions from memory, with the proof in your head so you can adapt them.
- Compute expected values with indicator variables and linearity, run probability DPs, and know when a
  Markov chain must be raised to a power (chapter 06).
- Read a generating function argument and know when the coefficient extraction needs NTT (chapter 14).
- Avoid the standard modular-arithmetic traps that turn a correct formula into a wrong answer.

## Where this shows up in contests

| Signal in the statement | Usually means |
|---|---|
| "count the number of ways ... modulo 1e9+7", `n ≤ 1e6` | closed form with factorials / one-dimensional DP |
| "count ... modulo p" with `n ≤ 1e18` | formula with `pow`, matrix power (ch06) or Lucas |
| "at least one of each", "no element in its original place", "avoids all forbidden ..." | inclusion–exclusion |
| balanced brackets, non-crossing, "never goes below zero", binary trees | Catalan / ballot numbers |
| "rotations (and reflections) are considered the same" | Burnside / Pólya |
| "split into k non-empty groups" | Stirling numbers of the second kind |
| "expected value", "probability that", print with `1e-6` precision or as `P·Q⁻¹ mod p` | linearity of expectation, probability DP |
| grid `1e6 × 1e6` with `≤ 2000` blocked cells | lattice paths + inclusion–exclusion over obstacles |

Placement: CSES Mathematics section (the whole ladder below), Codeforces Div2 C–E (counting with a
twist), AtCoder ABC D–F (almost every round has one), IOI subtasks that are "just counting" before
the real structure appears (e.g. counting valid configurations mod p as a warm-up subtask), BOI/CEOI
problems whose final step is "how many labelings does this structure have".

---

## 1. Counting principles

**Rule of sum.** Disjoint choices add: `|A ∪ B| = |A| + |B|` when `A ∩ B = ∅`.
**Rule of product.** Independent sequential choices multiply: `|A × B| = |A|·|B|`.
**Bijection.** If you can map set `X` one-to-one onto set `Y`, `|X| = |Y|`. Almost every formula below
is a bijection plus the product rule.
**Complement.** `|good| = |all| − |bad|` — often `bad` is easier (CSES Two Knights: all placements
`C(n²,2)` minus attacking pairs `4(n−1)(n−2)`).
**Double counting.** Count the same set two ways to get an identity, e.g. the handshake lemma
`Σ deg(v) = 2|E|`, or `Σ_v C(deg(v),2)` = number of paths of length 2.
**Pigeonhole.** `n+1` objects in `n` boxes ⇒ some box has ≥ 2. In contests: among any `n` integers some
non-empty subset has sum divisible by `n` (prefix sums mod `n` collide); among `n+1` numbers in `[1,2n]`
two are coprime (adjacent); Erdős–Szekeres: a sequence of `n²+1` distinct numbers has a monotone
subsequence of length `n+1`.

Complexity lens: the answer to "how many" is either a formula (O(1) after O(n) precomputation), a
1D/2D DP (O(n), O(n·k)), or a convolution (O(n log n) with NTT). Recognize which before coding.

## 2. Permutations, combinations, multinomials

| Object | Count | Notes |
|---|---|---|
| orderings of `n` distinct items | `n!` | |
| ordered `k`-subsets (`k`-permutations) | `P(n,k) = n!/(n−k)!` | |
| unordered `k`-subsets | `C(n,k) = n!/(k!(n−k)!)` | `C(n,k) = C(n,n−k)` |
| orderings of a multiset with counts `c_1..c_m` | `n!/(c_1!·…·c_m!)` | multinomial; CSES Creating Strings II |
| ways to fill `m` labeled boxes with sizes `c_i`, `Σc_i = n` | same multinomial | `C(n,c_1)·C(n−c_1,c_2)·…` |
| subsets of an `n`-set | `2^n` | `Σ_k C(n,k) = 2^n` |
| binary strings of length `n` with `k` ones | `C(n,k)` | |

Identities you must know cold (each has a one-line bijective proof):

- Pascal: `C(n,k) = C(n−1,k−1) + C(n−1,k)` (does element `n` belong to the subset?).
- Symmetry: `C(n,k) = C(n,n−k)` (complement).
- Absorption: `k·C(n,k) = n·C(n−1,k−1)` (choose a committee and its chair two ways).
- Vandermonde: `Σ_i C(m,i)·C(n,k−i) = C(m+n,k)` (split the `m+n` items into two groups).
- Hockey stick: `Σ_{i=k}^{n} C(i,k) = C(n+1,k+1)` (classify `(k+1)`-subsets by their largest element).
- `Σ_k k·C(n,k) = n·2^{n−1}`, `Σ_k C(n,k)² = C(2n,n)`.
- Binomial theorem: `(1+x)^n = Σ C(n,k) x^k`; with `x = −1`: `Σ (−1)^k C(n,k) = [n = 0]` — this is the
  engine of inclusion–exclusion.

## 3. Binomial coefficients mod p

### 3.1 Factorials and inverse factorials — the default

Precompute `fact[i]` and `inv_fact[i]` for `i ≤ N`. One modular exponentiation for `inv_fact[N]`, then
walk down: `inv_fact[i−1] = inv_fact[i]·i`. Total O(N + log p); each `C(n,k)` is then O(1).

```cpp
#include <bits/stdc++.h>
using namespace std;
using ll = long long;
const ll MOD = 1'000'000'007;

ll power(ll b, ll e, ll m = MOD) {
    ll r = 1; b %= m; if (b < 0) b += m;
    for (; e > 0; e >>= 1, b = b * b % m) if (e & 1) r = r * b % m;
    return r;
}

struct Comb {                       // C(n,k) mod prime MOD for 0 <= n <= N
    int N; vector<ll> f, fi;
    explicit Comb(int n) : N(n), f(n + 1), fi(n + 1) {
        f[0] = 1;
        for (int i = 1; i <= n; i++) f[i] = f[i - 1] * i % MOD;
        fi[n] = power(f[n], MOD - 2);
        for (int i = n; i > 0; i--) fi[i - 1] = fi[i] * i % MOD;
    }
    ll C(int n, int k) const { return (k < 0 || k > n) ? 0 : f[n] * fi[k] % MOD * fi[n - k] % MOD; }
    ll P(int n, int k) const { return (k < 0 || k > n) ? 0 : f[n] * fi[n - k] % MOD; }
    ll inv(int i) const { return fi[i] * f[i - 1] % MOD; }          // 1/i, i >= 1
    ll catalan(int n) const { return C(2 * n, n) * inv(n + 1) % MOD; }
    ll multinomial(const vector<int>& c) const {                     // n!/(c1!...cm!)
        int n = 0; ll r = 1;
        for (int x : c) n += x, r = r * fi[x] % MOD;
        return r * f[n] % MOD;
    }
};
```

Why `inv_fact[i−1] = inv_fact[i]·i`: `1/(i−1)! = i/i!`. The trick saves `N` exponentiations
(O(N log p) → O(N)), which matters at `N = 1e7`.

Memory: two `long long` arrays of `1e7` = 160 MB — too much on 256 MB judges. Use `int` (mod < 2³¹ fits)
or compute only `fact` and one `inv_fact` on the fly if queries are few. `N = 1e6` is 16 MB, fine.

Requirements: `p` prime and `p > n` (otherwise `n!` ≡ 0 and has no inverse). If `p ≤ n`, use Lucas
(3.3). If `p` is not prime, see 3.5.

### 3.2 Pascal's triangle — small n, any modulus

`C[i][j] = C[i−1][j−1] + C[i−1][j]` in O(n²) time and memory. Works for **any** modulus (no division),
and for exact values in `long long` up to `n ≤ 66` (`C(66,33) ≈ 7.2e18 < 9.2e18`; `C(67,33)` overflows).
Use when `n ≤ ~5000` or when the modulus is composite.

```cpp
vector<vector<ll>> pascal(int n, ll mod) {
    vector<vector<ll>> C(n + 1, vector<ll>(n + 1, 0));
    for (int i = 0; i <= n; i++) {
        C[i][0] = 1;
        for (int j = 1; j <= i; j++) C[i][j] = (C[i - 1][j - 1] + C[i - 1][j]) % mod;
    }
    return C;
}
```

### 3.3 Lucas' theorem — prime p ≤ n

For prime `p`, write `n = n_0 + n_1 p + n_2 p² + …` and `k = k_0 + k_1 p + …` in base `p`. Then

```
C(n,k) ≡ Π_i C(n_i, k_i)   (mod p)
```

(with `C(n_i,k_i) = 0` when `k_i > n_i`). Proof sketch: in `F_p[x]`, `(1+x)^p ≡ 1 + x^p` (all middle
binomials are divisible by `p`), so `(1+x)^n = Π_i ((1+x)^{p^i})^{n_i} ≡ Π_i (1 + x^{p^i})^{n_i}`; the
coefficient of `x^k` on the right is exactly the product of the digit-wise binomials, since base-`p`
representation is unique.

Consequence: `C(n,k) mod p ≠ 0` iff every base-`p` digit of `k` is ≤ the corresponding digit of `n`.
For `p = 2`: `C(n,k)` is odd iff `k & n == k` (Sierpiński triangle). Number of odd entries in row `n`
is `2^{popcount(n)}`.

```cpp
// C(n,k) mod p, p prime, p small enough for factorial tables of size p (p <= ~1e7).
ll lucas(ll n, ll k, ll p, const vector<ll>& f, const vector<ll>& fi) {
    ll r = 1;
    while (n > 0 || k > 0) {
        ll a = n % p, b = k % p;
        if (b > a) return 0;
        r = r * f[a] % p * fi[b] % p * fi[a - b] % p;
        n /= p; k /= p;
    }
    return r;
}
```

Complexity O(log_p n) per query after O(p) precomputation.

### 3.4 Small k, huge n

`C(n,k) = Π_{i=0}^{k−1} (n−i) / k!` — compute the numerator as a product of `k` terms mod `p`
(reduce `n−i` mod `p` first), multiply by `inv(k!)`. O(k) per query, works for `n` up to `1e18`
as long as `k < p`. AtCoder ABC156 D "Bouquet" is exactly this.

### 3.5 Composite modulus

- Small modulus, small `n`: Pascal (3.2).
- `m = Π p_i^{e_i}` with distinct primes: compute `C(n,k) mod p_i^{e_i}` for each and combine with CRT.
  Mod a prime power, factor out all `p`'s: `n! = p^{a} · (n!)_p` where `(n!)_p` is the part coprime to
  `p` (computed by a recursion `n!_p = (n mod p^e)!_p · ((p^e−1)!_p)^{n / p^e} · ((n/p)!)_p`), and
  Legendre gives `a = Σ_{j≥1} ⌊n/p^j⌋`. This is rare; know it exists (cp-algorithms "Binomial
  coefficients", section "arbitrary modulus").
- If you only need the exact value and it fits in `long long`, multiply/divide incrementally:
  `res = res * (n − i) / (i + 1)` is always an integer at each step.

## 4. Stars and bars

Number of ways to write `n` as an ordered sum of `k` non-negative integers `x_1 + … + x_k = n`:

```
C(n + k − 1, k − 1)
```

Proof: write `n` stars and insert `k−1` bars among them, `★★|★|  |★★★` ↔ `(2,1,0,3)`. The string has
`n + k − 1` symbols, choose the positions of the bars. Bijection, done.

Variants (all reduce to the base case by substitution):

| Constraint | Substitution | Count |
|---|---|---|
| `x_i ≥ 1` | `y_i = x_i − 1` | `C(n − 1, k − 1)` |
| `x_i ≥ a_i` | `y_i = x_i − a_i` | `C(n − Σa_i + k − 1, k − 1)` |
| `x_1 + … + x_k ≤ n` | add slack `x_{k+1} ≥ 0` | `C(n + k, k)` |
| `x_i ≤ b` (upper bounds) | inclusion–exclusion over violated bounds | `Σ_j (−1)^j C(k,j) C(n − j(b+1) + k − 1, k − 1)` |
| multisets of size `n` from `k` types | same as base case | `C(n + k − 1, n)` |
| non-decreasing sequences of length `n` over `{1..k}` | multisets | `C(n + k − 1, n)` |
| strictly increasing sequences | subsets | `C(k, n)` |

CSES Distributing Apples: `C(n + m − 1, m − 1)` (apples `m`, children `n` — read the statement for which
is which). Codeforces 1288C "Two Arrays": the pair `(a non-decreasing, b non-increasing, a_i ≤ b_i)` is a
single non-decreasing sequence of length `2m` over `n` values — `C(n + 2m − 1, 2m)`.

The generating-function view (section 15): the number of solutions is the coefficient of `x^n` in
`(1 + x + x² + …)^k = 1/(1−x)^k`.

## 5. Inclusion–exclusion

For finite sets `A_1..A_n`:

```
|A_1 ∪ … ∪ A_n| = Σ_i |A_i| − Σ_{i<j} |A_i ∩ A_j| + Σ_{i<j<k} |A_i ∩ A_j ∩ A_k| − … + (−1)^{n+1} |A_1 ∩ … ∩ A_n|
```

Equivalently, the number of elements in **none** of the sets is `Σ_{S ⊆ [n]} (−1)^{|S|} |∩_{i∈S} A_i|`
with the empty intersection being the universe.

**Proof.** Fix an element `x` that lies in exactly `m ≥ 1` of the sets. Its contribution to the
right-hand side of the "none" formula is `Σ_{j=0}^{m} (−1)^j C(m,j) = (1 − 1)^m = 0`. An element in no set
contributes `1` (only `S = ∅`). Summing over elements gives exactly the count of elements in no set. ∎

The formula is useful when `|∩_{i∈S} A_i|` depends only on `|S|` (then the sum has `n+1` terms, not
`2^n`), or when `n ≤ 20` so `2^n` terms are affordable, or when intersections factor nicely
(Möbius function = inclusion–exclusion over prime divisors; chapter 04, CSES Counting Coprime Pairs).

### 5.1 Derangements

Permutations of `n` with no fixed point. `A_i` = "position `i` is fixed", `|∩_{i∈S} A_i| = (n − |S|)!`:

```
D(n) = Σ_{j=0}^{n} (−1)^j C(n,j) (n−j)! = n! Σ_{j=0}^{n} (−1)^j / j!
```

Recurrence for O(n): `D(n) = (n−1)(D(n−1) + D(n−2))`, `D(0) = 1`, `D(1) = 0`. Proof: element `n` swaps
with one of `n−1` others (`D(n−2)` ways to finish) or goes to position `j` while `j` does not go to `n`
(`D(n−1)` ways after relabeling). Also `D(n) = n·D(n−1) + (−1)^n`. `D(n)/n! → 1/e ≈ 0.3679`.
CSES Christmas Party is `D(n) mod 1e9+7`.

```
n    : 0  1  2  3  4   5    6     7
D(n) : 1  0  1  2  9  44  265  1854
```

### 5.2 Surjections and Stirling numbers

Functions from an `n`-set onto a `k`-set ("every value appears at least once"): `A_i` = "value `i` is
missing", `|∩_{i∈S} A_i| = (k − |S|)^n`:

```
Surj(n,k) = Σ_{j=0}^{k} (−1)^j C(k,j) (k−j)^n = k! · S(n,k)
```

where `S(n,k)` is the Stirling number of the second kind (section 8). O(k log n) for a single value.
CSES Counting Sequences is this pattern; Codeforces 1342E "Placing Rooks" and 1228E "Another Filling
the Grid" are inclusion–exclusion where the intersections depend only on `|S|`.

### 5.3 Contest shape: "count sequences avoiding all forbidden patterns"

With `m ≤ 20` forbidden conditions, iterate all `2^m` subsets, compute the count that satisfies at
least the chosen subset (usually a product or a simple formula), add with sign `(−1)^{|S|}`. Example:
CSES Grid Completion — permutations extending a partially filled grid: fixed rows/columns must not use
forbidden cells; iterate how many forbidden cells are "forced hit" (`j`), the count of ways to choose
them is a DP over cells, and the remaining rows are free: `Σ_j (−1)^j ways(j) (n−j)!`.

## 6. Catalan numbers

```
C_n = C(2n, n) / (n+1) = C(2n, n) − C(2n, n+1),      C_0..C_8 = 1, 1, 2, 5, 14, 42, 132, 429, 1430
C_{n+1} = Σ_{i=0}^{n} C_i C_{n−i}                      (O(n²) recurrence)
C_{n+1} = C_n · 2(2n+1)/(n+2)                          (O(n) with modular inverse)
```

### 6.1 Proof by reflection (André)

Encode a string of `n` `(` and `n` `)` as a path: `(` = step `+1`, `)` = step `−1`. Total strings:
`C(2n,n)`. A string is **bad** iff the path dips to `−1`. Take a bad path and reflect its prefix up to
the first visit of `−1` through the line `y = −1` (equivalently: flip every symbol before and including
that position). The result starts at `−2` and ends at `0`, i.e., it is a path with `n+1` down-steps and
`n−1` up-steps; every such path visits `−1` and reflecting back is the inverse. So `bad = C(2n, n+1)` and
`good = C(2n,n) − C(2n,n+1) = C(2n,n)/(n+1)`. ∎

```
height        original (bad)                 reflected prefix
  1        /\                            
  0   ____/  \____ ... first hit of -1       -1 is the mirror
 -1          \  /  ↑                        \    /\ 
 -2           \/   |                         \/\/  \ ...
```

### 6.2 Ballot / generalized formula

Paths with `a` up-steps and `b` down-steps starting at height `h ≥ 0` that never go below `0`:

```
Ballot(a, b, h) = C(a+b, b) − C(a+b, b − h − 1)         (second term is 0 if b − h − 1 < 0)
```

Same reflection: a bad path's prefix up to its first `−1` is reflected, mapping the start `h` to
`−2−h`; a path from `−2−h` to `h + a − b` with `a+b` steps has `b − h − 1` down-steps.
`h = 0, a = b = n` recovers `C_n`. This solves CSES Bracket Sequences II: given a prefix of length `k`
(verify it is itself valid) with balance `h`, the remaining `m = n − k` symbols must contain
`a = (m − h)/2` ups and `b = (m + h)/2` downs (answer `0` if `m − h` is odd or negative).

### 6.3 What Catalan counts

All of these are in bijection (proofs: map each to bracket sequences):

- balanced bracket sequences of length `2n`;
- Dyck paths of length `2n` (monotone lattice paths from `(0,0)` to `(n,n)` not crossing the diagonal);
- full binary trees with `n+1` leaves / rooted binary trees with `n` nodes (left subtree = inside the
  first matching pair, right subtree = the rest);
- triangulations of a convex `(n+2)`-gon;
- ways to parenthesize a product of `n+1` factors;
- non-crossing perfect matchings of `2n` points on a circle;
- permutations of `[n]` avoiding the pattern `123` (or `231`, etc.) — stack-sortable permutations;
- sequences `1 ≤ a_1 ≤ a_2 ≤ … ≤ a_n` with `a_i ≤ i`.

Generating function: `C(x) = 1 + x C(x)²` (a non-empty sequence is `( inner ) rest`), giving
`C(x) = (1 − √(1−4x)) / (2x)`.

Variants: `k`-ary trees / `k`-Dyck paths are counted by Fuss–Catalan `C(kn+1, n)/(kn+1)`; paths bounded
above **and** below (`−L ≤ height ≤ U`) need a DP or repeated reflections (Codeforces 1204E is the
prefix-sum version where you sum the maxima).

## 7. Burnside's lemma and Pólya counting

Setting: a finite group `G` acts on a set `X` of configurations (e.g. rotations act on colorings of a
necklace). Two configurations are "the same" if some `g ∈ G` maps one to the other. The number of
equivalence classes (orbits) is

```
|X / G| = (1/|G|) Σ_{g ∈ G} |Fix(g)|,      Fix(g) = { x ∈ X : g·x = x }
```

**Proof.** Count pairs `(g, x)` with `g·x = x` two ways. Per `g`: `Σ_g |Fix(g)|`. Per `x`: the
stabilizer `Stab(x) = {g : g·x = x}` has size `|G| / |Orb(x)|` (orbit–stabilizer). So
`Σ_x |G|/|Orb(x)| = |G| · Σ_{orbits O} Σ_{x∈O} 1/|O| = |G| · #orbits`. ∎

**Pólya's shortcut.** If `X` is the set of colorings of `m` positions with `k` colors and `g` permutes
the positions with `c(g)` cycles, then `|Fix(g)| = k^{c(g)}` (each cycle must be monochromatic).

### 7.1 Necklaces (rotations only) — CSES Counting Necklaces

`n` beads, `k` colors, `G = Z_n` (rotations). Rotation by `i` positions splits the beads into
`gcd(i,n)` cycles of length `n/gcd(i,n)`:

```
Necklaces(n,k) = (1/n) Σ_{i=0}^{n−1} k^{gcd(i,n)} = (1/n) Σ_{d | n} φ(d) k^{n/d}
```

O(n log n) with the first form, O(√n · log) with the second. Division by `n` is a modular inverse — fine
when `p > n`. Bracelets (rotations + reflections, dihedral group of order `2n`) add the reflection terms:
for odd `n`, `n` reflections each with `(n+1)/2` cycles; for even `n`, `n/2` reflections through beads
(`n/2 + 1` cycles) and `n/2` through edges (`n/2` cycles).

Trace, `n = 4, k = 2`: `gcd(0,4)=4, gcd(1,4)=1, gcd(2,4)=2, gcd(3,4)=1` → `(16 + 2 + 4 + 2)/4 = 6`.
The six necklaces: `0000 0001 0011 0101 0111 1111`. ✓

```cpp
ll necklaces(ll n, ll k) {                 // rotations only, mod MOD
    ll s = 0;
    for (ll i = 0; i < n; i++) s = (s + power(k, gcd(i, n))) % MOD;   // std::gcd, C++17
    return s * power(n, MOD - 2) % MOD;
}
```

### 7.2 Square grids under rotation — CSES Counting Grids

`n × n` cells, `k` colors, `G = {0°, 90°, 180°, 270°}`. Cycle counts of the cell permutation:

| rotation | cycles | why |
|---|---|---|
| 0° | `n²` | every cell fixed |
| 90°, 270° | `⌈n²/4⌉ = (n² + 3) / 4` | 4-cycles, plus the centre cell if `n` odd |
| 180° | `⌈n²/2⌉ = (n² + 1) / 2` | 2-cycles, plus the centre if `n` odd |

```
Grids(n,k) = ( k^{n²} + 2·k^{(n²+3)/4} + k^{(n²+1)/2} ) / 4
```

Exponents are up to `1e18` for `n ≤ 1e9`, so reduce them mod `p − 1` before `power` (Fermat: `k^{p−1} ≡ 1`
when `gcd(k,p) = 1`; `k` is a small color count so this holds, but guard `k ≡ 0`).

## 8. Stirling numbers, Bell numbers, partitions

### 8.1 Stirling numbers of the second kind `S(n,k)`

Number of ways to partition an `n`-set into `k` non-empty unlabeled blocks.

```
S(n,k) = k·S(n−1,k) + S(n−1,k−1),   S(0,0) = 1,  S(n,0) = 0 (n>0),  S(n,n) = 1
```

Proof: element `n` either forms its own block (`S(n−1,k−1)`) or joins one of the `k` existing blocks
(`k·S(n−1,k)`). O(nk) table. Explicit formula (from 5.2): `S(n,k) = (1/k!) Σ_j (−1)^j C(k,j) (k−j)^n`,
O(k log n) for one value; a whole row in O(k log k) is a single convolution (chapter 14).

```
S(n,k)    k=0  1   2   3   4  5
n=0        1
n=1        0   1
n=2        0   1   1
n=3        0   1   3   1
n=4        0   1   7   6   1
n=5        0   1  15  25  10  1
```

Uses: surjections `k!·S(n,k)`; `x^n = Σ_k S(n,k) x^{\underline{k}}` (falling factorials) — the standard
way to convert `Σ_i i^n·something` into binomials; number of equivalence relations with `k` classes.

### 8.2 Stirling numbers of the first kind `c(n,k)` (unsigned)

Number of permutations of `[n]` with exactly `k` cycles.

```
c(n,k) = (n−1)·c(n−1,k) + c(n−1,k−1),   c(0,0) = 1
```

Proof: element `n` is a fixed point (own cycle, `c(n−1,k−1)`) or is inserted after any of the `n−1`
existing elements in its cycle (`(n−1)·c(n−1,k)`). `Σ_k c(n,k) = n!`. Row `n` is the coefficient list
of `x(x+1)(x+2)…(x+n−1)` — a whole row by divide-and-conquer polynomial multiplication (ch14).

### 8.3 Bell numbers

`B(n) = Σ_k S(n,k)` = number of set partitions of `[n]`. `B(0..8) = 1, 1, 2, 5, 15, 52, 203, 877, 4140`.
Recurrence `B(n+1) = Σ_{k=0}^{n} C(n,k) B(k)` (choose the block containing element `n+1`); O(n²).
EGF `e^{e^x − 1}`.

### 8.4 Integer partitions

`p(n)` = number of multisets of positive integers summing to `n` (order irrelevant). `p(0..10) = 1, 1, 2,
3, 5, 7, 11, 15, 22, 30, 42`. Growth `~ e^{π√(2n/3)} / (4n√3)` — `p(100) = 190 569 292`.

DP as an unbounded knapsack over part sizes (outer loop = part size, so each multiset is counted once):

```cpp
vector<ll> partitions(int n) {            // p(0..n) mod MOD, O(n^2)
    vector<ll> p(n + 1, 0); p[0] = 1;
    for (int part = 1; part <= n; part++)
        for (int s = part; s <= n; s++) p[s] = (p[s] + p[s - part]) % MOD;
    return p;
}
```

Variants: `p(n, k)` partitions into exactly `k` parts = partitions with largest part `k` (conjugate the
Ferrers diagram): `p(n,k) = p(n−1,k−1) + p(n−k,k)`. Partitions into distinct parts = partitions into
odd parts (Euler; GF `Π(1+x^i) = Π 1/(1−x^{2i−1})`). Euler's pentagonal theorem gives O(n√n):
`p(n) = Σ_{k≥1} (−1)^{k−1} [ p(n − k(3k−1)/2) + p(n − k(3k+1)/2) ]`.

CSES Two Sets II is a **subset-sum count**, not a partition count: number of subsets of `{1..n}` with sum
`n(n+1)/4`, divided by `2` (each split is counted from both sides). O(n · n²) ≈ 1.25e7 for `n = 500`.

## 9. Grid paths with obstacles

Monotone paths (right/down) from `(0,0)` to `(r,c)`: `C(r+c, r)`. With obstacles:

**Small grid (`n ≤ 1000`):** DP `ways[i][j] = ways[i−1][j] + ways[i][j−1]`, `0` at obstacles. O(nm).
CSES Grid Paths I.

**Huge grid, few obstacles (`k ≤ 2000`):** sort obstacles by `(row, col)`. Let `bad[i]` = number of
paths from the start to obstacle `i` that avoid all earlier obstacles (all obstacles on a path to `i`
precede `i` in sorted order):

```
bad[i] = paths(start → o_i) − Σ_{j<i, o_j ≤ o_i componentwise} bad[j] · paths(o_j → o_i)
answer = paths(start → end) − Σ_j bad[j] · paths(o_j → end)
```

Each path that hits an obstacle is counted exactly once, at its **first** obstacle — this is
inclusion–exclusion organized as a DP. O(k²). CSES Grid Paths II, Codeforces 559C "Gerald and Giant
Chess". Treat the target as the last "obstacle" to unify the code:

```cpp
// cells sorted by (r,c); last cell is the target; Comb cb sized >= R + C
ll paths(int r1, int c1, int r2, int c2, const Comb& cb) {
    if (r2 < r1 || c2 < c1) return 0;
    return cb.C(r2 - r1 + c2 - c1, r2 - r1);
}
ll grid_paths_with_obstacles(vector<pair<int,int>> obs, int R, int C, const Comb& cb) {
    obs.push_back({R, C}); sort(obs.begin(), obs.end());
    int k = obs.size(); vector<ll> bad(k);
    for (int i = 0; i < k; i++) {
        bad[i] = paths(0, 0, obs[i].first, obs[i].second, cb);
        for (int j = 0; j < i; j++) {
            ll w = paths(obs[j].first, obs[j].second, obs[i].first, obs[i].second, cb);
            bad[i] = (bad[i] - bad[j] * w) % MOD;
        }
    }
    return (bad[k - 1] % MOD + MOD) % MOD;           // "bad" for the target = paths reaching it cleanly
}
```

## 10. Counting DPs — the shapes that recur

- **Sequences with a local constraint** (CSES Counting Towers, Array Description): state = last
  element / last column shape; transition = allowed pairs. When `n` is huge, the transition is a matrix
  (ch06).
- **Permutations with `k` inversions** (CSES Permutation Inversions): `dp[n][k] = Σ_{j=0}^{n−1} dp[n−1][k−j]`
  (insert the largest element at any position); prefix sums make it O(nk).
- **Interval / bracket-like merges** (CSES Empty String): `dp[len]` over even lengths, choose the partner
  of the first character and multiply by an interleaving binomial.
- **Placing non-attacking pieces on diagonals** (CSES Counting Bishops): process diagonals of one color
  independently, `dp[i][j] = dp[i−1][j] + dp[i−1][j−1]·(len_i − (j−1))`, convolve the two colors.
- **Digit DP** (CSES Counting Numbers): state = (position, tight, last digit, started).
- **Subset-sum counting** (Two Sets II): `dp[s] += dp[s − x]`, iterate items outermost.

Always ask: is the object built element by element (1D DP), or split at a distinguished position
(interval DP / Catalan-like convolution), or is the answer symmetric enough for a formula?

## 11. Expected value and linearity

`E[X] = Σ_x x·P(X = x)`. **Linearity:** `E[X + Y] = E[X] + E[Y]` **always** — no independence needed.
This is the single most useful fact in contest probability. Recipe:

1. Write the quantity as a sum of indicator variables `X = Σ_i 1[event_i]`.
2. `E[X] = Σ_i P(event_i)`.
3. Compute each probability independently — even if the events are wildly dependent.

Examples:

- **Expected number of inversions** (CSES Inversion Probability): `a_i` uniform in `[1, r_i]`. `E = Σ_{i<j}
  P(a_i > a_j)`. For `x = r_i, y = r_j`: if `x ≤ y`, `P = x(x−1)/2 / (xy)`; else
  `P = (xy − y(y+1)/2) / (xy)`. O(n²), doubles are fine.
- **Expected number of empty cells** (CSES Moving Robots): `E = Σ_cells P(cell empty) =
  Σ_cells Π_robots (1 − P(robot r at cell))`. Robots move independently, so the probability of the
  intersection factors; each robot's distribution after `k` steps is an `8×8 × k` DP.
- **Expected maximum** (CSES Candy Lottery, Codeforces 453A): `E[max] = Σ_{v=1}^{k} P(max ≥ v)
  = Σ_v (1 − ((v−1)/k)^n)`. General tool: for non-negative integer `X`, `E[X] = Σ_{v≥1} P(X ≥ v)`.
- **Expected number of maximal runs** (Codeforces 235B "Let's Play Osu!"): score is `Σ len²` over runs;
  write `len² = Σ` over pairs inside the run, so `E = Σ_i P(i is 'O') + 2 Σ_{i<j} P(i..j all 'O')` — the
  second sum telescopes into a linear DP.

Exact rational answers modulo `p`: probabilities `a/b` become `a·b⁻¹ mod p`. All the algebra above is
valid in `F_p` as long as no denominator is `≡ 0 mod p` (guaranteed by the statement in practice).

## 12. Probability DP

State = "what has happened so far that matters", value = probability. Transitions multiply by the
probability of the step. Standard shapes:

- **Sum of dice** (CSES Dice Probability): `dp[i][s]` = probability the first `i` dice sum to `s`;
  `dp[i][s] = (1/6) Σ_{f=1}^{6} dp[i−1][s−f]`. O(6·n·6n).
- **Random walk on a small board** (Moving Robots): `dp[step][cell]`, each step spreads mass to
  neighbours with weight `1/deg`.
- **Coins with different biases, probability of majority heads** (AtCoder EDPC I "Coins"): `dp[i][h]`.
- **Expected steps until absorption** (AtCoder EDPC J "Sushi"): `E[state] = 1 + Σ p·E[next]`; when a
  transition returns to the same state with probability `q`, solve `E = (1 + Σ_{others} p E') / (1 − q)`.
  If the state graph has genuine cycles, this becomes a linear system → Gaussian elimination (ch06).

Precision: print with `printf("%.6f")`; `double` has ~16 significant digits, enough for products of ≤ 1e6
factors in `[0,1]` as long as you do not subtract nearly equal numbers. Never compare probabilities
with `==`.

## 13. Markov chains and matrix power

If the process is a chain with `s` states and one-step transition matrix `T` (`T[i][j] = P(i → j)`),
the distribution after `k` steps is `v·T^k`. For `k ≤ 1e6` iterate; for `k ≤ 1e18`, exponentiate
`T` in O(s³ log k) (ch06). Expected values over `k` steps: augment the state with an accumulator row.
Absorbing chains and infinite horizons: solve `(I − Q) E = 1` by Gaussian elimination (ch06 §7).
Throwing Dice / Fibonacci Numbers / Graph Paths are the deterministic version of the same idea.

## 14. Fibonacci identities

`F_0 = 0, F_1 = 1, F_{n+1} = F_n + F_{n−1}`. `F_{10} = 55`, `F_{20} = 6765`, `F_{45} ≈ 1.13e9`
(last fitting `int`), `F_{92}` last fitting `long long`.

- Addition: `F_{m+n} = F_m F_{n+1} + F_{m−1} F_n`.
- Fast doubling (`O(log n)`, no matrices): `F_{2k} = F_k (2F_{k+1} − F_k)`, `F_{2k+1} = F_{k+1}² + F_k²`.
- Cassini: `F_{n−1} F_{n+1} − F_n² = (−1)^n`.
- Sums: `Σ_{i=0}^{n} F_i = F_{n+2} − 1`, `Σ F_i² = F_n F_{n+1}`, `Σ_i C(n−i, i) = F_{n+1}`.
- Divisibility: `gcd(F_m, F_n) = F_{gcd(m,n)}`; `F_m | F_n ⇔ m | n` (`m ≥ 3`).
- Zeckendorf: every positive integer is uniquely a sum of non-consecutive Fibonacci numbers (greedy).
- Pisano period: `F_n mod m` is periodic; period `≤ 6m`, `= 60` for `m = 10`, `= 1500` for `m = 1000`.
- Binet's formula needs `√5 mod p`, which exists only when `p ≡ ±1 (mod 5)`. `1e9+7 ≡ 2` and
  `998244353 ≡ 3 (mod 5)`, so neither works. Use doubling or matrices.

```cpp
pair<ll,ll> fib(ll n) {                  // returns (F_n, F_{n+1}) mod MOD, O(log n)
    if (n == 0) return {0, 1};
    auto [a, b] = fib(n >> 1);           // a = F_k, b = F_{k+1}, k = n/2
    ll c = a * ((2 * b - a + MOD) % MOD) % MOD;     // F_{2k}
    ll d = (a * a + b * b) % MOD;                   // F_{2k+1}
    if (n & 1) return {d, (c + d) % MOD};
    return {c, d};
}
```

## 15. Generating functions — the introduction

An ordinary generating function (OGF) packs a sequence into formal power series coefficients:
`A(x) = Σ_n a_n x^n`. Operations on sequences become algebra on series:

| Combinatorial operation | Series operation |
|---|---|
| choose an object of type A **and** an object of type B, total size adds | `A(x)·B(x)` (convolution `c_n = Σ a_i b_{n−i}`) |
| choose type A **or** type B | `A(x) + B(x)` |
| a sequence of any number of A-objects | `1/(1 − A(x))` |
| shift index by 1 | multiply by `x` |
| prefix sums | multiply by `1/(1−x)` |

Standard series: `1/(1−x) = Σ x^n`, `1/(1−x)^k = Σ_n C(n+k−1, k−1) x^n` (stars and bars!), `(1+x)^n = Σ
C(n,k) x^k`, `x/(1−x−x²) = Σ F_n x^n`, Catalan `C(x) = (1−√(1−4x))/(2x)`, `Π_{i≥1} 1/(1−x^i) = Σ p(n) x^n`.

How this is used in contests:

1. **Read off a formula.** Number of ways to pay `n` with coins of values `1, 2, 5` with unlimited
   supply = `[x^n] 1/((1−x)(1−x²)(1−x⁵))`. Partial fractions give a closed form; or the DP is the
   coefficient recurrence.
2. **Recognize a convolution.** If the answer is `Σ_i f(i) g(n−i)` for `n` up to `2e5` you need FFT/NTT
   (chapter 14: `O(n log n)` polynomial multiplication mod `998244353`). The `O(n²)` double loop is the
   fallback for `n ≤ 5000`.
3. **Manipulate the closed form** to get a recurrence with few terms, then matrix power (ch06) for huge
   `n`. Rational GF `P(x)/Q(x)` ⇔ linear recurrence with characteristic polynomial `Q`.
4. **Exponential generating functions** `Σ a_n x^n / n!` handle labeled structures: `exp(A(x))` counts
   sets of labeled components (Bell numbers `= n![x^n] e^{e^x − 1}`, connected graphs from all graphs).

Why NTT matters: computing the first `n` coefficients of `A(x)·B(x)`, `1/A(x)`, `log A`, `exp A` in
`O(n log n)` turns many "sum over `k` of a product" formulas into fast algorithms — full Stirling rows,
`k`-th term of a recurrence in `O(k log k log n)`, partitions in `O(n log n)`. Chapter 14 builds this.

## 16. Common modular pitfalls

1. **Negative results.** `(a − b) % MOD` can be negative in C++. Write `((a − b) % MOD + MOD) % MOD` or
   `a − b + (a < b ? MOD : 0)`. Inclusion–exclusion is the usual crime scene.
2. **Overflow before the `%`.** `a * b` with both `< 1e9+7` fits in `long long` (`< 1.0e18 < 9.2e18`), but
   `a * b * c` does not — reduce after every multiplication. `a * b` with `int` operands overflows
   immediately.
3. **Dividing.** Never `/` — multiply by the modular inverse, and the inverse exists only if the divisor
   is coprime to the modulus. Burnside's `1/|G|` and Catalan's `1/(n+1)` are fine for prime `p > n`.
4. **`inv(0)` does not exist.** `power(0, p−2) = 0` silently; a formula dividing by something that can be
   `0 mod p` is wrong, not slow.
5. **Exponents are mod `p − 1`, not mod `p`** (Fermat), and only when `gcd(base, p) = 1`. CSES
   Exponentiation II. Do not reduce the exponent when the base can be `≡ 0`.
6. **`p ≤ n`** kills factorial tables (`n! ≡ 0`). Use Lucas or the exact `p`-adic method.
7. **Composite modulus** (e.g. `1e9+6 = p−1`, or `2^k`): no Fermat inverses; use Pascal or CRT.
8. **`MOD` vs `1e9+7` as a `double`:** `const int MOD = 1e9 + 7;` is fine (exactly representable); but
   `pow(2, n) % MOD` with `std::pow` is a floating-point disaster.
9. **Mixing moduli** (`998244353` for NTT, `1e9+7` for the answer) — pick per problem, never both.
10. **Reading `n ≤ 1e18` into `int`.** Also `1 << k` with `k ≥ 31` — use `1LL << k`.
11. **Precomputing too little.** `C(2n, n)` needs tables to `2n`; `C(n + k − 1, ...)` needs `n + k`.
12. **Probabilities as fractions mod p:** the answer `P·Q⁻¹` is not a probability you can sanity-check by
    eye — verify on small inputs with a brute force that uses `double`.

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| "choose k of n", `n ≤ 1e6`, many queries | factorial tables | O(n) precompute, O(1) query |
| binomial with `n ≤ 1e18`, small `k` | falling product | O(k) |
| binomial mod prime `p ≤ n` | Lucas | O(log_p n) |
| binomial, composite small modulus, `n ≤ 5000` | Pascal | O(n²) |
| distribute `n` identical items into `k` boxes | stars and bars | O(1) |
| "each ... at least once" / "none in original place" / "avoid all" | inclusion–exclusion | O(k) or O(2^k) |
| brackets, trees, non-crossing, "never below zero" | Catalan / ballot | O(1) after tables |
| "rotations count as the same" | Burnside / Pólya | O(n log n) or O(#divisors) |
| "split into k non-empty groups" | Stirling 2nd kind | O(nk) or O(k log n) |
| "permutations with k cycles" | Stirling 1st kind | O(nk) |
| "sum n as unordered parts" | partition DP | O(n²) / O(n√n) |
| huge grid, few obstacles | lattice paths + first-obstacle DP | O(k²) |
| "expected number of ..." | indicators + linearity | O(#events) |
| "probability that after n steps ..." small state space | probability DP | O(n·states·branching) |
| same but `n ≤ 1e18` | Markov matrix power (ch06) | O(s³ log n) |
| `Σ_i f(i) g(n−i)` for all `n` up to 2e5 | convolution → NTT (ch14) | O(n log n) |

## Implementation checklist for contests

- [ ] Table size: largest argument to `C` (`2n`? `n + k`? `n + m − 1`?) — allocate `+5`.
- [ ] `k < 0 || k > n` returns `0`, not a garbage index.
- [ ] Every subtraction followed by `+ MOD) % MOD`.
- [ ] Every product reduced before the next multiplication; `long long` everywhere.
- [ ] Division → inverse; check the divisor is invertible for all inputs (`n + 1`, `|G|`, `k!`).
- [ ] Exponents from `n²`, `n·m` etc. computed in `long long` (and reduced mod `p−1` only if base ≠ 0).
- [ ] Brute force on `n ≤ 8` agrees (enumerate with `next_permutation` / bitmasks) — 5 minutes that save
      a wrong-answer spiral.
- [ ] Expected values with doubles: `printf("%.6f")` or `cout << fixed << setprecision(6)`.
- [ ] Off-by-one in Catalan/ballot: are `a`, `b`, `h` steps or symbols? Recheck with `n = 1, 2`.
- [ ] Burnside: `|G|` includes the identity; reflections exist only if the statement says so.

## Further reading

- CPH (Laaksonen, *Competitive Programmer's Handbook*): ch. 22 "Combinatorics" (binomials, Catalan,
  inclusion–exclusion, Burnside, Cayley), ch. 24 "Probability", ch. 23 "Matrices" (for ch06).
- cp-algorithms.com: "Binomial Coefficients", "Catalan Numbers", "The Inclusion-Exclusion Principle",
  "Burnside's lemma / Pólya enumeration theorem", "Fibonacci Numbers", "Lucas theorem" (inside the
  binomial page). The site is thin on probability and Stirling numbers; use CPH ch. 24 and *Concrete
  Mathematics* ch. 6 for those.
- Graham, Knuth, Patashnik, *Concrete Mathematics*, ch. 5 (binomials), ch. 6 (special numbers), ch. 7
  (generating functions) — the reference behind everything here.
- Wilf, *generatingfunctionology* (free PDF) — chapters 1–2 for the OGF/EGF mindset.
- Flajolet & Sedgewick, *Analytic Combinatorics*, part A — only if you want the theory of §15 in full.

## You can move on when...

- You can write `Comb` (factorials + inverse factorials + `C`, `P`, `catalan`) from memory in
  under 3 minutes with no bugs.
- You can prove the reflection argument for Catalan and derive the ballot formula for an arbitrary
  start height on paper.
- You can state and prove inclusion–exclusion, and write the derangement and surjection formulas
  without looking.
- You can compute necklace and grid counts with Burnside, including the cycle counts of each rotation.
- You can turn "expected number of X" into a sum of indicator probabilities within 30 seconds of
  reading the statement.
- You have solved every ★1–★3 problem in `problems.md` and at least half of the ★4s, each in one
  sitting, and stress-tested at least three of them against a brute force.
