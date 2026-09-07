# Chapter 06 — Matrices and Linear Algebra mod p

Prerequisites: modular arithmetic and inverses (ch04), the combinatorial DPs and Markov chains of
ch05, bitsets (ch03), XOR basis first contact (ch03), graph representations
(`../../algorithms_learning/06_graphs/lesson.md`). All arithmetic is over `Z_p` with `p` prime
(`1e9+7`, `998244353`) unless the section says reals or `GF(2)`.

## What you'll be able to do after this chapter

- Turn any linear recurrence, "count walks of length k", or finite-state DP with a huge step count
  `n ≤ 1e18` into a matrix exponentiation in O(s³ log n), and know when `s` is too big for that.
- Recover an unknown linear recurrence from its first `2k` terms (Berlekamp–Massey) and evaluate its
  `n`-th term in O(k² log n) (Kitamasa) without ever building a `k × k` matrix.
- Run Gaussian elimination over reals (with partial pivoting), over `Z_p`, and over `GF(2)` with
  bitsets; get rank, determinant, inverse and a solution set from the same routine.
- Use XOR basis as "Gauss over GF(2), one vector at a time".
- Count spanning trees (Kirchhoff), solve expected-value systems of Markov chains, and compute
  shortest walks of exactly `k` edges with min-plus matrix products — and see that all of these are
  the same three lines of code over a different semiring.

## Where this shows up in contests

| Signal | Technique |
|---|---|
| `n ≤ 1e18` and a recurrence / DP with ≤ ~100 states | matrix power |
| "number of paths of length exactly k" in a graph with `n ≤ 100` | adjacency-matrix power |
| "minimum cost path with exactly k edges", `n ≤ 100`, `k ≤ 1e9` | min-plus matrix power |
| `n ≤ 1e18`, the DP's state is a bitmask over a short profile | transition matrix over profiles |
| the recurrence is given by a black-box process, order ≤ 100 | Berlekamp–Massey + Kitamasa |
| "solve the system", `n ≤ 500`, answer mod p or with `1e-6` tolerance | Gaussian elimination |
| "maximum XOR of a subset", "how many distinct subset XORs", "k-th smallest subset XOR" | XOR basis |
| "how many spanning trees" | Kirchhoff |
| "expected number of steps until …" with cycles in the state graph | linear system |
| `n ≤ 2000` vectors over `GF(2)` / boolean matrix products | bitset Gauss / bitset multiply |

Placement: CSES Mathematics (Fibonacci Numbers, Throwing Dice, Graph Paths I/II, System of Linear
Equations), CSES Bitwise Operations (XOR subset trilogy), Codeforces Div2 E / Div1 C–D (matrix power
with a twist: forbidden transitions, segments of different matrices, sum accumulators), Educational
Rounds (XOR basis is a favourite at 1900–2300), IOI/BOI rarely as the main idea but often as a
subtask ("`n ≤ 1e18`" in an otherwise DP problem).

---

## 1. Matrices mod p — representation and multiplication

Square matrices stored as `vector<vector<ll>>`; entries in `[0, p)`. Multiplication is the triple
loop in `i, k, j` order — the innermost loop then walks row `k` of `B` and row `i` of `C`
contiguously, which is 3–5× faster than the textbook `i, j, k` order for `n ≥ 64` (cache lines).

```cpp
#include <bits/stdc++.h>
using namespace std;
using ll = long long;
const ll MOD = 1'000'000'007;

struct Mat {
    int n; vector<vector<ll>> a;
    explicit Mat(int n) : n(n), a(n, vector<ll>(n, 0)) {}
    static Mat identity(int n) { Mat I(n); for (int i = 0; i < n; i++) I.a[i][i] = 1; return I; }
    Mat operator*(const Mat& o) const {
        Mat r(n);
        for (int i = 0; i < n; i++)
            for (int k = 0; k < n; k++) {
                if (a[i][k] == 0) continue;                      // sparse rows are common
                for (int j = 0; j < n; j++)
                    r.a[i][j] = (r.a[i][j] + a[i][k] * o.a[k][j]) % MOD;
            }
        return r;
    }
};

Mat mpow(Mat b, ll e) {                        // b^e, O(n^3 log e)
    Mat r = Mat::identity(b.n);
    for (; e > 0; e >>= 1, b = b * b) if (e & 1) r = r * b;
    return r;
}
vector<ll> mat_apply(const Mat& m, const vector<ll>& v) {           // m * v  (column vector)
    vector<ll> r(m.n, 0);
    for (int i = 0; i < m.n; i++) for (int j = 0; j < m.n; j++) r[i] = (r[i] + m.a[i][j] * v[j]) % MOD;
    return r;
}
```

**Correctness of `mpow`:** invariant `result · b^{e_remaining} = b^{e_original}` — each step either
folds the lowest bit into `result` or squares `b` and halves `e`. Matrix multiplication is
associative, which is all that binary exponentiation needs (it does not need commutativity — never
reorder `r * b` vs `b * r` casually, though here both are powers of the same `b` so it happens to be
fine).

**Constants.** One product is `n³` multiply-adds with a `%` each. `%` by a runtime constant is the
bottleneck. Two speedups when it matters:

- Accumulate in `unsigned long long` and reduce every 16 terms: each product is `< (1e9+7)² ≈ 1.0e18`
  and `2^64 ≈ 1.8e19`, so 16 products fit. Roughly 3× faster.
- For `p < 2^31` and `n ≤ 500`, `__int128` accumulation with a single `%` per output cell is simplest.

```cpp
// faster inner loop: reduce every 16 products (MOD < 2^30 assumed)
for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) {
    unsigned long long s = 0;
    for (int k = 0; k < n; k++) {
        s += (unsigned long long)a[i][k] * o.a[k][j];
        if ((k & 15) == 15) s %= MOD;
    }
    r.a[i][j] = s % MOD;
}
```

| `n` (matrix size) | one product | `log(1e18) ≈ 60` squarings + ≤ 60 multiplies | verdict |
|---|---|---|---|
| 2–10 | negligible | negligible | anything goes; `1e5` queries each with their own power is fine |
| 60 | 2.2e5 | 2.6e7 | fine, ~30 ms |
| 100 | 1e6 | 1.2e8 | fine, ~0.2 s |
| 200 | 8e6 | ~1e9 | 1–2 s: use the 16-term trick, or Kitamasa if it is a recurrence |
| 500 | 1.25e8 | 1.5e10 | no; find another idea (or the exponent is small) |

Memory is never the issue; time is.

## 2. Linear recurrences as matrix powers

A recurrence `a_n = c_1 a_{n−1} + c_2 a_{n−2} + … + c_k a_{n−k}` is a linear map on the state vector
`v_n = (a_n, a_{n−1}, …, a_{n−k+1})ᵀ`:

```
v_{n+1} = T · v_n,     T = | c_1  c_2  …  c_{k−1}  c_k |      (companion matrix)
                           |  1    0   …    0       0  |
                           |  0    1   …    0       0  |
                           |  …                        |
                           |  0    0   …    1       0  |
```

so `v_n = T^{n} v_0` and `a_n` is the first coordinate. Cost O(k³ log n).

### 2.1 Fibonacci — the trace

`T = [[1,1],[1,0]]`, `v_0 = (F_1, F_0)ᵀ = (1, 0)ᵀ`, `v_n = (F_{n+1}, F_n)ᵀ`. Then `T^n = [[F_{n+1}, F_n],[F_n, F_{n−1}]]`.

```
n = 13 = 1101₂         e   bit   r (after)                          b (after squaring)
start                  13        I                                   T   = [[1,1],[1,0]]
step 1                 13   1    T           = [[1,1],[1,0]]         T²  = [[2,1],[1,1]]
step 2                  6   0    T                                   T⁴  = [[5,3],[3,2]]
step 3                  3   1    T·T⁴ = T⁵   = [[8,5],[5,3]]         T⁸  = [[34,21],[21,13]]
step 4                  1   1    T⁵·T⁸= T¹³  = [[377,233],[233,144]] (done)
```

`F_13 = 233`, `F_14 = 377`. ✓ Four squarings for a 4-bit exponent; `1e18` takes 60.

### 2.2 Building the matrix from any linear DP

Procedure: (1) list the quantities you need at step `n+1`; (2) express each as a linear combination
of quantities at step `n`; (3) if something is missing (a constant, a running sum, an earlier term),
add it to the state until the list closes.

| Recurrence | State | Matrix rows |
|---|---|---|
| `a_n = a_{n−1} + a_{n−2} + 1` | `(a_n, a_{n−1}, 1)` | `[1 1 1; 1 0 0; 0 0 1]` |
| `a_n = 2a_{n−1} + n` | `(a_n, n, 1)` | `[2 1 1; 0 1 1; 0 0 1]` |
| `S_n = Σ_{i≤n} F_i` | `(F_{n+1}, F_n, S_n)` | `[1 1 0; 1 0 0; 1 0 1]` |
| Throwing Dice: `f_n = Σ_{i=1}^{6} f_{n−i}` | `(f_n, …, f_{n−5})`, `v_0 = (1,0,0,0,0,0)` | companion `k=6` |
| Counting Towers (ch05): 2 row shapes | `(wide_n, narrow_n)` | `[2 1; 1 4]` |

`v_0` convention: put the base terms with `a_{negative} = 0` and `a_0 = 1` when the DP says "one way to
have made zero steps" — then `a_n = (T^n v_0)[0]` with no off-by-one. Verify `n = 0, 1, 2` against the
naive DP before submitting; the companion-matrix off-by-one is the most common bug in this chapter.

```cpp
// a_n for a_n = sum_j c[j] * a_{n-1-j}, given a_0..a_{k-1}; O(k^3 log n)
ll kth_term_matrix(const vector<ll>& c, const vector<ll>& init, ll n) {
    int k = c.size();
    if (n < k) return init[n] % MOD;
    Mat T(k);
    for (int j = 0; j < k; j++) T.a[0][j] = c[j] % MOD;
    for (int i = 1; i < k; i++) T.a[i][i - 1] = 1;
    vector<ll> v(k);                                  // v = (a_{k-1}, a_{k-2}, ..., a_0)
    for (int i = 0; i < k; i++) v[i] = init[k - 1 - i] % MOD;
    v = mat_apply(mpow(T, n - (k - 1)), v);
    return v[0];
}
```

### 2.3 Periodic or piecewise transitions

If the transition changes in a pattern of period `L` (weekday/weekend rules), multiply the `L`
matrices into one period matrix `P = T_L ⋯ T_1`, raise to `n / L`, then apply the remaining `n mod L`
individually. If the transition changes at `m` known breakpoints (Codeforces 954F "Runner's Problem":
blocked cells on a 3-row strip given as segments), sort the breakpoints, and for each segment raise
the segment's matrix to the segment's length — O(m · k³ log n).

## 3. Counting walks — the adjacency matrix

For a directed graph with adjacency matrix `A` (`A[i][j]` = number of edges `i → j`),
`(A^k)[i][j]` = number of walks of length exactly `k` from `i` to `j`. Proof by induction: a walk of
length `k` is a walk of length `k−1` to some `m` followed by an edge `m → j`, and
`(A^{k−1} A)[i][j] = Σ_m A^{k−1}[i][m] A[m][j]`. Exactly the matrix product. CSES Graph Paths I:
`n ≤ 100`, `k ≤ 1e9` → O(n³ log k) ≈ 3e7. ✓

Variants: walks of length **at most** `k` — add a self-loop at the target (or use the state
augmentation `(A, I; 0, I)`); walks that avoid a vertex — delete it; walks in a graph that changes
every step — §2.3; count walks with a weight `Π w_e` — put the weights in `A` (they multiply along a
walk and add over walks: that is the `(+,×)` semiring at work).

**DP automata as graphs.** "Count strings of length `n` over an alphabet with forbidden adjacent
pairs" (Codeforces 222E "Decoding Genome"): nodes are letters, `A[x][y] = 1` if `xy` is allowed,
answer `Σ_{i,j} (A^{n−1})[i][j]`. "Count strings avoiding a pattern": nodes are KMP automaton states
(ch14). Any DP whose state is small and whose transition does not depend on the step index is a
matrix.

## 4. Semirings — the same code, a different `(⊕, ⊗)`

Matrix multiplication only uses two operations: `⊗` for combining a step and `⊕` for merging
alternatives, with `⊕` associative and commutative, `⊗` associative, and `⊗` distributing over `⊕`.
Any such structure (a **semiring**) supports "exponentiation by squaring" because associativity of
the induced matrix product is all that the proof needs.

| Semiring `(⊕, ⊗, 0, 1)` | `(A^k)[i][j]` means | Contest use |
|---|---|---|
| `(+, ×, 0, 1)` mod p | number of walks of length `k` | counting (Graph Paths I) |
| `(min, +, ∞, 0)` | cheapest walk with exactly `k` edges | Graph Paths II, "k stops" |
| `(max, +, −∞, 0)` | most valuable walk with `k` edges | longest paths, DP with `k` phases |
| `(OR, AND, 0, 1)` | is there a walk of length `k` | reachability; bitset rows, O(n³/64) |
| `(max, min, −∞, ∞)` | widest bottleneck walk with `k` edges | capacity routing |
| `(XOR, AND)` = `GF(2)` | parity of the number of walks | Gauss over `GF(2)` |

```cpp
const ll INF = LLONG_MAX / 4;
struct MinPlus {                                   // (min,+) matrices for "exactly k edges" shortest walks
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
```

The identity is `0` on the diagonal and `∞` elsewhere (a walk of length 0 costs 0 and goes nowhere).
`INF = LLONG_MAX/4` leaves room for one addition without overflow; still guard with the `!= INF`
tests. Multiple edges `i → j`: keep the cheapest. CSES Graph Paths II is `(min,+)`, `n ≤ 100`,
`k ≤ 1e9`.

Min-plus has no fast (`n^{2.37}`) algorithm — it is the reason all-pairs shortest paths is
Θ(n³)-ish in practice; that is fine here because `n ≤ 100`.

## 5. DP over profiles as a matrix (broken-profile revisited)

Counting Tilings (CSES 2181) with `n` columns and `m ≤ 10` rows is a broken-profile DP with `2^m`
states per column. If `n` were `1e18` instead of `1000`, the column-to-column transition (all
`2^m × 2^m` pairs of profiles that are compatible) is a fixed matrix `T`, and the answer is
`(T^n)[0][0]`. Feasible iff `2^m ≤ ~120`, i.e. `m ≤ 6–7`. The same holds for any "profile" DP: row
colourings with adjacency constraints, tilings with several piece shapes, "no two adjacent chosen
cells", etc. The recipe:

1. Enumerate states of one column/row (`s` of them).
2. Build `T[x][y]` = number of ways to go from profile `x` to profile `y` in one step (usually 0/1, but
   can be a count when a step involves internal choices).
3. `answer = Σ_y (T^n)[start][y]` over accepting profiles `y`.

If `s` is ~1000 (`m = 10`), `s³ log n ≈ 6e10` — too slow; then you need Berlekamp–Massey (§6) on the
first `2s` terms of the answer sequence, because that sequence satisfies a linear recurrence of order
≤ `s` (Cayley–Hamilton: `T` is annihilated by its characteristic polynomial of degree `s`, so is every
sequence `uᵀ T^n v`). Compute `2s + 10` terms by the O(s · n) DP, run BM in O(s²), Kitamasa in
O(s² log n). Total ~1e6·log — the standard trick for "profile DP with huge n".

## 6. Kitamasa and Berlekamp–Massey — recurrences without matrices

### 6.1 Kitamasa: `a_n` in O(k² log n)

Let `a_n = Σ_{j=1}^{k} c_j a_{n−j}`, characteristic polynomial `Q(x) = x^k − c_1 x^{k−1} − … − c_k`.
Then `a_n = Σ_i r_i a_i` where `x^n ≡ Σ_{i<k} r_i x^i (mod Q)`. Compute `x^n mod Q` by binary
exponentiation of polynomials modulo `Q` — each step is a product of two degree-`< k` polynomials
(O(k²), or O(k log k) with NTT, ch14) followed by reduction mod `Q` (O(k²)). Result O(k² log n) — for
`k = 1000, n = 1e18` about `6e7 · 2 = 1.2e8`: fine, where the matrix (`1e9 · 60`) is hopeless.

```cpp
// a_n where a_i = sum_{j<k} tr[j] * a_{i-1-j}; S = (a_0 .. a_{k-1}). O(k^2 log n). (KACTL-style)
ll linear_rec(const vector<ll>& S, const vector<ll>& tr, ll n) {
    int k = tr.size();
    auto combine = [&](vector<ll> a, vector<ll> b) {          // (a * b) mod characteristic poly
        vector<ll> res(2 * k + 1, 0);
        for (int i = 0; i <= k; i++) for (int j = 0; j <= k; j++)
            res[i + j] = (res[i + j] + a[i] * b[j]) % MOD;
        for (int i = 2 * k; i > k; i--)                          // reduce x^i using x^k = sum tr[j] x^{k-1-j}
            for (int j = 0; j < k; j++)
                res[i - 1 - j] = (res[i - 1 - j] + res[i] * tr[j]) % MOD;
        res.resize(k + 1);
        return res;
    };
    vector<ll> pol(k + 1, 0), e(pol);
    pol[0] = 1; e[1] = 1;                                       // pol = 1, e = x
    for (++n; n; n /= 2) {                                      // compute x^{n+1} mod Q, shifted convention
        if (n % 2) pol = combine(pol, e);
        e = combine(e, e);
    }
    ll res = 0;
    for (int i = 0; i < k; i++) res = (res + pol[i + 1] * S[i]) % MOD;
    return res;
}
```

The `++n` / `pol[i+1]` shift is the KACTL indexing convention — it is tested against a naive DP in
`example.cpp`; keep the tested version verbatim.

### 6.2 Berlekamp–Massey: find the recurrence

Given a sequence `s_0, s_1, …, s_{m−1}` (over a field — `Z_p` works, integers/reals do not because of
division), BM returns the shortest linear recurrence `c_1..c_L` with `s_i = Σ_j c_j s_{i−j}` for all
`L ≤ i < m`. If the true recurrence has order `k`, `m ≥ 2k` terms are enough and the result is unique.
O(m²).

```cpp
vector<ll> berlekamp_massey(const vector<ll>& s) {      // returns c with s[i] = sum_j c[j] s[i-1-j]
    int n = s.size(), L = 0, m = 0;
    vector<ll> C(n, 0), B(n, 0), T;
    C[0] = B[0] = 1;
    ll b = 1;
    for (int i = 0; i < n; i++) { ++m;
        ll d = s[i] % MOD;                                // discrepancy
        for (int j = 1; j <= L; j++) d = (d + C[j] * s[i - j]) % MOD;
        if (d == 0) continue;
        T = C;
        ll coef = d * power(b, MOD - 2) % MOD;
        for (int j = m; j < n; j++) C[j] = ((C[j] - coef * B[j - m]) % MOD + MOD) % MOD;
        if (2 * L > i) continue;
        L = i + 1 - L; B = T; b = d; m = 0;
    }
    C.resize(L + 1); C.erase(C.begin());
    for (ll& x : C) x = (MOD - x) % MOD;
    return C;
}
```

Use: compute `2s + 5` terms of your DP naively, BM → coefficients, Kitamasa → `a_n`. When the
statement's recurrence is unknown but you can simulate it (matrix of unknown structure, complicated
counting with a small hidden state), this pair replaces hours of algebra. Sanity check: after BM,
re-verify the recurrence reproduces terms `L..m−1` — if `m` was too small you get garbage silently.

## 7. Gaussian elimination

### 7.1 Over the reals — partial pivoting

Solve `A x = b`, `A` is `n × m`. Forward elimination with the row of largest `|pivot|` swapped up
(partial pivoting keeps the growth of rounding errors bounded), then back-substitution. O(n · m · min)
≈ O(n³); `n = 500` is `1.25e8/3` — fine.

```cpp
const double EPS = 1e-9;
// Solves a (n x m) * x = b. Returns 0: none, 1: unique, 2: infinitely many. x has size m.
int gauss(vector<vector<double>> a, vector<double> b, vector<double>& x) {
    int n = a.size(), m = a[0].size();
    vector<int> where(m, -1);                                    // where[col] = pivot row for col
    for (int col = 0, row = 0; col < m && row < n; col++) {
        int sel = row;
        for (int i = row; i < n; i++) if (fabs(a[i][col]) > fabs(a[sel][col])) sel = i;
        if (fabs(a[sel][col]) < EPS) continue;                    // free column
        swap(a[sel], a[row]); swap(b[sel], b[row]);
        where[col] = row;
        for (int i = 0; i < n; i++) if (i != row) {                // eliminate col from all other rows
            double f = a[i][col] / a[row][col];
            if (fabs(f) < EPS) continue;
            for (int j = col; j < m; j++) a[i][j] -= f * a[row][j];
            b[i] -= f * b[row];
        }
        row++;
    }
    x.assign(m, 0.0);
    for (int j = 0; j < m; j++) if (where[j] != -1) x[j] = b[where[j]] / a[where[j]][j];
    for (int i = 0; i < n; i++) {                                 // consistency: 0 = b_i rows
        double s = 0;
        for (int j = 0; j < m; j++) s += a[i][j] * x[j];
        if (fabs(s - b[i]) > EPS) return 0;
    }
    for (int j = 0; j < m; j++) if (where[j] == -1) return 2;
    return 1;
}
```

This is reduced row echelon form (Gauss–Jordan: eliminate above **and** below the pivot). It costs
about 1.5× forward-only elimination but gives the solution and the free variables directly.

**Determinant:** run forward elimination (rows below only), track `sign` flips on swaps,
`det = sign · Π pivots`; a zero pivot column ⇒ `det = 0`. **Inverse:** augment `[A | I]`, Gauss–Jordan,
normalize pivot rows to 1; the right half becomes `A⁻¹` (or `A` is singular if some column has no
pivot). **Rank:** number of pivots found.

Precision: `EPS = 1e-9` is for well-scaled inputs (`|a_ij| ≤ 1e3`); use relative tests
`fabs(a) < EPS · max(1, row_norm)` if entries span many magnitudes; `long double` buys ~3 digits.
Never compare doubles with `==`.

### 7.2 Over `Z_p`

Same algorithm with `/` replaced by multiplication with the modular inverse and no pivoting concerns
(any non-zero pivot is exact). Determinant mod `p`: forward elimination, product of pivots, sign flips.
CSES System of Linear Equations is this — mind: "no solution" iff a zero row of `A` has non-zero `b`;
"infinitely many" iff some column has no pivot; output any particular solution by setting free
variables to `0`.

```cpp
// A (n x n) mod MOD: returns determinant; elimination in place (rows below the pivot only)
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
            for (int j = col; j < n; j++) a[i][j] = ((a[i][j] - f * a[col][j]) % MOD + MOD) % MOD;
        }
    }
    return det;
}
```

Composite modulus (no inverses): use the Euclid-style elimination — repeatedly subtract `⌊a/b⌋`
multiples between two rows until one entry is zero, like `gcd` on the leading entries; O(n³ log) and
correct because you only add integer multiples of rows (determinant unchanged, sign flips on swaps).

### 7.3 Over `GF(2)` — bitsets

Rows are bit vectors; addition is XOR; `AND` is multiplication. Store each row as `bitset<M>` and the
whole row operation is one XOR — O(n · m · n / 64). `n = m = 2000`: `1.25e8` word operations,
trivially fast; `n = 5000` still fits in a second.

```cpp
const int M = 2048;                                              // number of columns (variables)
// Reduces rows in place; returns rank. Row i is (coefficients | rhs) with rhs at bit M-1 if wanted.
int gauss_gf2(vector<bitset<M>>& rows, int cols) {
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
```

Applications: parity puzzles ("press a switch, neighbours toggle" — Lights Out), "which subset XORs
to a given value", number of solutions = `2^{#free variables}` (or 0 if inconsistent), rank of a set
of bit vectors. If a problem uses `bitset::_Find_first` / `_Find_next` (libstdc++-only) to skip to the
next set bit, remember it does not exist on libc++ (macOS) — the loop above is portable.

### 7.4 XOR basis — Gauss over `GF(2)` one vector at a time

Vectors are integers (`≤ 60` bits). Keep `basis[b]` = a basis vector whose highest set bit is `b`,
or 0. Inserting `x`: for `b` from high to low, if bit `b` of `x` is set: if `basis[b]` is empty store
`x` there and stop, else `x ^= basis[b]`. If `x` becomes 0 it was dependent. O(60) per insert.
The set of achievable subset XORs is exactly the span; its size is `2^rank`.

```cpp
struct XorBasis {
    static const int B = 60;
    ll basis[B] = {}; int rank = 0;
    bool insert(ll x) {
        for (int b = B - 1; b >= 0; b--) {
            if (!(x >> b & 1)) continue;
            if (!basis[b]) { basis[b] = x; rank++; return true; }
            x ^= basis[b];
        }
        return false;                                             // x in span already
    }
    bool can_make(ll x) const {
        for (int b = B - 1; b >= 0; b--) if (x >> b & 1) { if (!basis[b]) return false; x ^= basis[b]; }
        return true;
    }
    ll max_xor(ll start = 0) const {                             // max over span of (start ^ v)
        ll r = start;
        for (int b = B - 1; b >= 0; b--) if (basis[b] && !(r >> b & 1)) r ^= basis[b];
        return r;
    }
    // k-th smallest (1-indexed) distinct subset xor, k in [1, 2^rank]. Fully reduce first so that
    // every pivot bit occurs in exactly one basis vector; then value order = mask order.
    ll kth(ll k) const {
        vector<ll> red;                                           // reduced basis, increasing pivot bit
        for (int b = 0; b < B; b++) if (basis[b]) {
            ll v = basis[b];
            for (ll u : red) if (v >> (63 - __builtin_clzll(u)) & 1) v ^= u;   // clear lower pivot bits
            red.push_back(v);                                     // v has no pivot bit of a lower vector
        }
        ll r = 0; k--;                                            // 0-indexed position
        for (int i = 0; i < (int)red.size(); i++) if (k >> i & 1) r ^= red[i];
        return r;
    }
};
```

Why `max_xor` is greedy-correct: with a basis in "highest-bit" form, taking `basis[b]` whenever bit `b`
of the current value is 0 can only make the final value larger — bit `b` is decided by exactly one
basis vector and higher bits are unaffected. Why `kth` works: after full reduction (each pivot bit
appears in exactly one basis vector), the subset XORs are in bijection with bitmasks over the basis,
and the order of the XOR value equals the order of the mask. `k-th` with duplicates counted (CSES K
Subset Xors counts subsets, not distinct values) needs `2^{n−rank}` copies of each value.

CSES trio: Maximum Xor Subset (`max_xor`), Number of Subset Xors (`2^rank`), K Subset Xors
(`kth` with multiplicity `2^{n−rank}`). Square Subsets (CSES 3193 / Codeforces 895C): a product is a
square iff every prime exponent is even → each number is a vector over the 19 primes ≤ 70;
non-empty subsets with zero XOR = `2^{n − rank} − 1`.

## 8. Applications of linear systems

### 8.1 Kirchhoff's matrix-tree theorem

For an undirected multigraph without self-loops, let `L = D − A` (degree matrix minus adjacency
matrix; parallel edges count with multiplicity). The number of spanning trees equals **any** cofactor
of `L`: delete one row and the same column, take the determinant.

Proof sketch: `L = B Bᵀ` for the signed incidence matrix `B` (`n × m`, arbitrary orientation). By
Cauchy–Binet, `det(L with row/col r removed) = Σ_{S ⊆ E, |S| = n−1} det(B_S)²` where `B_S` is `B`
restricted to the edges `S` (and row `r` removed). `det(B_S) = ±1` if `S` is a spanning tree and `0`
otherwise (a cycle in `S` gives linearly dependent columns; a spanning tree's incidence matrix is
totally unimodular). ∎

```
K_4:  L = [ 3 -1 -1 -1]    delete row/col 0:  det [ 3 -1 -1]  = 3(9-1) +1(-3-1) -1(1+3) = 24 - 4 - 4 = 16 = 4^{2}  (Cayley)
          [-1  3 -1 -1]                           [-1  3 -1]
          [-1 -1  3 -1]                           [-1 -1  3]
          [-1 -1 -1  3]
```

Compute with `det_mod` (answer mod p) or with doubles (`n ≤ 50`, exact up to ~1e15; round). Cayley's
formula `n^{n−2}` for `K_n` is the special case. Directed version: number of spanning arborescences
rooted at `r` (all edges pointing toward `r`) = det of `L_out = D_out − A` with row/col `r` removed;
use `D_in` for arborescences pointing away from `r`. Weighted version: put edge weights in `A` and
`D`; the cofactor is the sum over spanning trees of the product of weights (generating function of
trees).

### 8.2 Expected values in Markov chains with cycles

Absorbing chain with transient states `1..t` and transition probabilities `P`. Expected steps to
absorption `E_i = 1 + Σ_j P_ij E_j` (`E = 0` at absorbing states) — a linear system `(I − Q) E = 1`.
Gauss with `t ≤ 500`. If the chain is a "line" or "band" (each state talks to `O(1)` neighbours or the
system is tridiagonal), Gauss is O(t · bandwidth²) — Codeforces 24D "Broken robot" solves each row of
a grid as a tridiagonal system, O(n · m).

Other quantities by the same recipe: absorption probabilities into a specific state (`h_i = Σ_j P_ij
h_j`, `h = 1` at the target, `0` at other absorbing states); expected total reward; stationary
distribution (`π P = π`, `Σ π = 1`). When a state has a self-loop with probability `q`, the equation
`E_i = 1 + q E_i + …` is solved by dividing by `1 − q` — no Gauss if the graph is otherwise a DAG
(ch05 §12).

### 8.3 Interpolation, rank, "is this set of vectors dependent"

Solving `Σ_j a_j x_i^j = y_i` for the coefficients of a degree-`(n−1)` polynomial through `n` points
is a Vandermonde system — Gauss works for `n ≤ 500`; Lagrange (ch04/ch14) is O(n²) with no system.
Rank over `Z_p` of `n` vectors answers "how many of these constraints are independent" (e.g. how many
switch presses are truly distinct in a toggling puzzle).

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| linear recurrence, `k ≤ 100`, `n ≤ 1e18` | companion matrix power | O(k³ log n) |
| linear recurrence, `k ≤ 5000`, `n ≤ 1e18` | Kitamasa (+NTT if `k ≥ 1e4`) | O(k² log n) |
| hidden recurrence, order ≤ `k`, simulate `2k` terms | Berlekamp–Massey → Kitamasa | O(k²) + O(k² log n) |
| walks of length exactly `k`, `n ≤ 100` | adjacency power | O(n³ log k) |
| cheapest walk with exactly `k` edges | min-plus power | O(n³ log k) |
| reachability with many queries, `n ≤ 5000` | boolean bitset matrices / bitset BFS | O(n³/64) |
| profile DP with `n ≤ 1e18`, `2^m ≤ 100` | transition matrix power | O(8^m log n) |
| profile DP with `n ≤ 1e18`, `2^m ≤ 1000` | DP for `2·2^m` terms + BM + Kitamasa | O(4^m · m + 4^m log n) |
| solve `Ax = b`, `n ≤ 500`, reals | Gauss with partial pivoting | O(n³) |
| solve `Ax = b` mod p / determinant mod p | Gauss with inverses | O(n³) |
| `n ≤ 2000` parity equations | bitset Gauss | O(n³/64) |
| max / count / k-th subset XOR | XOR basis | O(n · 60) |
| number of spanning trees | Kirchhoff cofactor determinant | O(n³) |
| expected steps with cyclic state graph, `≤ 500` states | `(I − Q)E = 1`, Gauss | O(s³) |

## Implementation checklist for contests

- [ ] Companion matrix orientation: does `T · v_n` really give `v_{n+1}`? Check `n = 0, 1, 2` against
      the naive DP in the same program before printing anything.
- [ ] Exponent `0` returns the identity; `n < k` returns the base term directly.
- [ ] `%` after every multiply-add; `long long` everywhere; `INF` for min-plus is `LLONG_MAX/4` and is
      tested for before adding.
- [ ] Min-plus identity is `0`/`INF`, not `1`/`0`.
- [ ] Matrix size × log: compute `s³ · 2 · log₂ n` and compare with `1e8` before committing to the
      approach. `s > 200` → Kitamasa or a different idea.
- [ ] BM: feed at least `2k + 5` terms; verify the recovered recurrence on the last few terms.
- [ ] Gauss reals: partial pivoting on `|a|`, `EPS` relative to the scale of the data; report "none /
      unique / infinite" correctly (zero row with non-zero rhs vs free column).
- [ ] Gauss mod p: every pivot is inverted once; subtracting rows needs `+ MOD) % MOD`.
- [ ] `GF(2)`: bitset size is a compile-time constant ≥ number of variables (+1 for the RHS column).
- [ ] XOR basis: `60` bits for values `< 2^60`; `kth` needs the fully reduced basis and `k ≤ 2^rank`.
- [ ] Kirchhoff: no self-loops in `L`; parallel edges counted; delete **the same** row and column.
- [ ] Doubles for spanning-tree counts only when the count `< 2^53`; otherwise mod p.

## Further reading

- CPH ch. 23 "Matrices" (matrix power, linear recurrences, graph paths, Kirchhoff), ch. 24 for the
  Markov-chain setting.
- cp-algorithms.com: "Gauss method for solving system of linear equations", "Calculating the
  determinant of a matrix by Gauss", "Finding the rank of a matrix", "Kirchhoff's theorem", "Linear
  recurrences" (not a page there — see below).
- Codeforces blog "Berlekamp-Massey algorithm" (search the title; several good expositions with the
  KACTL implementation) and KACTL `LinearRecurrence.h`, `BerlekampMassey.h` — the code in this chapter
  follows KACTL's conventions.
- Codeforces blog "Linear basis" / "XOR basis" tutorials (Educational Round editorials for 1101G,
  1100F, 845G are short and complete).
- Cormen et al., *Introduction to Algorithms*, ch. 28 (LU decomposition; why partial pivoting).
- Aigner, *A Course in Enumeration*, ch. on the matrix-tree theorem for the full Cauchy–Binet proof.

## You can move on when...

- You can write `Mat`, `mpow`, the companion-matrix builder and the min-plus variant from memory,
  and compute `F_{1e18} mod 1e9+7` and Throwing Dice with them in under 10 minutes.
- You can build the transition matrix for a 2-row or 3-row profile DP and verify it against the
  linear-time DP.
- You can run BM on a sequence you generated and explain why `2k` terms suffice (Cayley–Hamilton).
- You can write Gauss over reals and over `Z_p` from memory, including the none/unique/infinite
  classification, and derive rank, determinant and inverse from the same elimination.
- You can solve the CSES XOR trio and Square Subsets without looking anything up.
- You can compute the number of spanning trees of `K_4` by hand (`16`) and of a small graph with your
  code, and explain Cauchy–Binet in two sentences.
- Every ★1–★3 problem in `problems.md` is solved and at least half of the ★4s.
