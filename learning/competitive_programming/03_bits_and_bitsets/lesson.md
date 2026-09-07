# Chapter 03 — Bits and Bitsets

Reference library: `example.cpp` (re-type it from memory; that is the exercise).
Prerequisites: `../../c_learning/12_numbers_bits_floats/lesson.md` (two's complement,
shifts), `../../algorithms_learning/07_dynamic_programming/lesson.md` (DP on subsets was
introduced there as "bitmask DP basics"; here we go to contest depth).

## What you'll be able to do after this chapter

- Read and write any bit expression (`x & -x`, `(s-1) & m`, `i ^ (i >> 1)`) without pausing, and
  know exactly which shifts are undefined behaviour.
- Enumerate subsets, submasks of a mask, supermasks, and Gray-code orders, and bound the total
  work (`3^n` for all submasks of all masks).
- Write bitmask DP for Hamiltonian paths / TSP / partition-into-groups in O(2^n · n²) or O(2^n · n),
  and know that `n ≤ 20` in a statement is an instruction, not a hint.
- Derive and implement SOS DP (sum over subsets) in O(2^n · n) and its inverse.
- Build a XOR linear basis and answer: is `x` a subset xor, what is the maximum subset xor, what is
  the k-th smallest subset xor.
- Use `std::bitset` to divide a factor of 64 out of knapsack, reachability, and dense-graph BFS,
  including the GCC `_Find_first/_Find_next` extensions.
- Multiply two 62-bit numbers modulo a third without overflow (`__int128`, `long double` trick).

## Where this shows up in contests

| Signal in the statement                                 | What it means                                   | Typical placement            |
|---------------------------------------------------------|-------------------------------------------------|------------------------------|
| `n ≤ 20` (or 15, 16, 18) and "order / subset / assign"  | 2^n states; bitmask DP or brute force subsets   | CSES DP, CF Div2 D/E         |
| `n ≤ 40` with subset sums                               | meet in the middle, 2^20 per half               | CSES Meet in the Middle      |
| "xor of a subset", "maximum xor", "k-th xor"            | linear basis over GF(2), 60 vectors             | CF Div1 B–C, CSES Bitwise    |
| values `< 2^20`, "for every mask count subsets/supersets"| SOS DP                                          | CF 1500–2100, CSES SOS Bit   |
| `n ≤ 5000` with n² pairs reachability / n³ closure      | bitset (n³/64 ≈ 2·10⁹/64 fits)                  | CSES Reachable Nodes         |
| knapsack with sum ≤ 10⁵ and only yes/no needed          | `bitset<S>` shift-or                             | CSES Money Sums, Two Sets II |
| Gray code, "adjacent differ in one bit"                 | `i ^ (i >> 1)`                                   | CSES intro                   |
| `1e18`-sized products mod `m > 2^32`                    | `__int128` / long double mulmod                 | anywhere Miller–Rabin is     |

IOI: subtask structure often gives `n ≤ 15–20` as subtask 2–3 → bitmask DP scores 30–50 % before
the intended solution. BOI/CEOI: xor basis and SOS appear roughly once every two years.

---

## 1. Two's complement, shifts, and what is undefined

A signed `int` holds bit patterns of width 32 interpreted as `x = -b₃₁·2³¹ + Σ bᵢ·2ⁱ`. Consequences:

- `-x = ~x + 1`. So `x & -x` isolates the lowest set bit (see §2), `~x = -x - 1`.
- `x >> 1` on a negative `int` is *arithmetic* on every compiler you will meet (fills with sign
  bit), but the standard only made this guaranteed in C++20. Treat right shift of negatives as
  "don't do it in contests".
- Undefined behaviour (UB), meaning the optimizer may do anything:
  - shifting by ≥ width: `1 << 32`, `1LL << 64`, `x >> 64`;
  - shifting by a negative amount;
  - left-shifting a negative value (before C++20);
  - `1 << 31` as `int` overflows into the sign bit (UB before C++20; use `1u << 31` or `1LL << 31`).
- `1 << k` has type `int`. Writing `1 << k` with `k` up to 40 because "the variable it's assigned
  to is long long" is the single most common bit bug. Rule: **always `1LL << k` or `1ULL << k`
  unless `k < 31` is provably true**.
- Comparing signed with unsigned converts the signed side to unsigned: `-1 < 1u` is **false**.
  `-Wall -Wextra` warns (`-Wsign-compare`); read those warnings.

Widths: `int` 32, `long long` 64, `__int128` 128 (GCC/Clang extension, 64-bit targets only —
works on CSES, Codeforces (except old MSVC), AtCoder, oj.uz).

| Op         | Meaning                       | Notes                                              |
|------------|-------------------------------|----------------------------------------------------|
| `a & b`    | AND                           | intersection of sets                               |
| `a \| b`   | OR                            | union                                              |
| `a ^ b`    | XOR                           | symmetric difference; `a ^ a = 0`, `a ^ 0 = a`     |
| `~a`       | NOT                           | complement within the type's width                 |
| `a << k`   | `a · 2^k`                     | UB if it overflows a signed type                   |
| `a >> k`   | `⌊a / 2^k⌋` for a ≥ 0         | for negative a: arithmetic shift, avoid            |
| `a & ~b`   | set difference A \ B          |                                                    |

Precedence trap: `&`, `^`, `|` bind **weaker** than `==` and `<`. `x & 1 == 0` parses as
`x & (1 == 0)`. Always parenthesize: `(x & 1) == 0`, or use `x >> k & 1` which is fine because
`>>` binds tighter than `&`.

## 2. Single-bit operations

```cpp
inline bool test_bit  (ull x, int k) { return (x >> k) & 1ULL; }
inline ull  set_bit   (ull x, int k) { return x |  (1ULL << k); }
inline ull  clear_bit (ull x, int k) { return x & ~(1ULL << k); }
inline ull  toggle_bit(ull x, int k) { return x ^  (1ULL << k); }
inline ll   lowbit     (ll x) { return x & -x; }
inline ll   drop_lowbit(ll x) { return x & (x - 1); }
inline bool is_pow2    (ll x) { return x > 0 && (x & (x - 1)) == 0; }
```

**Why `x & -x` is the lowest set bit.** Write `x = y 1 0^k` (some prefix `y`, then the lowest
1, then `k` zeros). `~x = ȳ 0 1^k`; adding 1 carries through the trailing ones: `-x = ȳ 1 0^k`.
The prefix bits are complementary, the lowest-1 position is 1 in both, everything below is 0 in
both. AND leaves exactly `0…0 1 0^k`.

**Why `x & (x-1)` clears it.** `x - 1 = y 0 1^k`: the lowest 1 borrows, turning into 0, and the
trailing zeros become ones; the prefix is untouched. AND with `x` keeps `y`, zeroes the rest.

```
x     = 0 0 1 0 1 0 0 0   (40)
-x    = 1 1 0 1 1 0 0 0   (-40)
x&-x  = 0 0 0 0 1 0 0 0   (8)     lowest set bit
x-1   = 0 0 1 0 0 1 1 1   (39)
x&x-1 = 0 0 1 0 0 0 0 0   (32)    lowest set bit removed
```

**Builtins** (GCC/Clang; `<bit>` in C++20 gives `std::popcount`, `std::countr_zero` etc.):

| Builtin                    | Returns                                  | On 0                    |
|----------------------------|------------------------------------------|-------------------------|
| `__builtin_popcountll(x)`  | number of set bits                       | 0                       |
| `__builtin_ctzll(x)`       | index of lowest set bit (trailing zeros) | **UB** — wrap it        |
| `__builtin_clzll(x)`       | leading zeros; `63 - clz` = ⌊log₂ x⌋      | **UB** — wrap it        |
| `__builtin_parityll(x)`    | popcount mod 2                           | 0                       |

Use the `ll` suffix for 64-bit arguments. `__builtin_popcount((long long)x)` silently truncates to
32 bits. This bug does not show on samples.

```cpp
inline int ctz(ull x)    { return x ? __builtin_ctzll(x) : 64; }
inline int floor_log2(ull x) { assert(x); return 63 - __builtin_clzll(x); }
inline int bit_length(ull x) { return x ? 64 - __builtin_clzll(x) : 0; }
inline ull next_pow2(ull x)  { return x <= 1 ? 1 : 1ULL << bit_length(x - 1); }
```

**Iterating set bits** in O(popcount):

```cpp
for (ull m = mask; m; m &= m - 1) { int b = __builtin_ctzll(m); /* use b */ }
```

**Counting Bits (CSES 1146) pattern.** Total set bits in `1..n` for `n ≤ 10^15` in O(log n): bit
`b` over `0..n` is periodic with period `2^{b+1}` — `2^b` zeros then `2^b` ones. With `N = n + 1`
numbers, `full = N / 2^{b+1}` full periods contribute `full · 2^b`, and the partial tail of length
`N mod 2^{b+1}` contributes `max(0, tail − 2^b)`.

```cpp
ll count_ones_upto(ll n) {
    ll res = 0;
    for (int b = 0; b < 62 && (1LL << b) <= n; b++) {
        ll half = 1LL << b, period = half << 1;
        ll full = (n + 1) / period, rem = (n + 1) % period;
        res += full * half + max(0LL, rem - half);
    }
    return res;
}
```

Trace `n = 5` (numbers 0..5): b=0: period 2, full=3 → 3, rem 0. b=1: period 4, full=1 → 2,
rem 2 → +0. b=2: period 8, full 0, rem 6 → +2. Total 7 = popcounts 1+1+2+1+2. ✓

## 3. Subsets, submasks, supermasks, Gray code

Subsets of `{0..n-1}` ↔ integers `0..2^n − 1`; bit `i` set ⇔ element `i` in the set.
Union `a|b`, intersection `a&b`, difference `a&~b`, complement `((1<<n)-1) ^ a`, `|A| =
popcount(a)`, `A ⊆ B` ⇔ `(a & b) == a` ⇔ `(a | b) == b`.

**All submasks of `m`, decreasing:**

```cpp
for (int s = m; ; s = (s - 1) & m) { /* use s */ if (s == 0) break; }
```

Why `(s-1) & m` is the next smaller submask: `s - 1` clears the lowest set bit of `s` and sets
all bits below it; masking with `m` removes the ones that are not in `m`. Every bit pattern
between `s-1 & m` and `s` (exclusive) has a set bit outside `m` or is > `s-1`. The loop must use
`do…while` shape because `s = 0` is a submask and `(0-1) & m = m` would restart the loop.

**Total cost over all masks:** Σ_m 2^{popcount(m)} = Σ_k C(n,k) 2^k = (1+2)^n = **3^n**. Each
element is in {not in m, in m but not in s, in s}. So "for every mask, for every submask" is
3^n ≈ 3.5·10⁹ at `n = 20` (too slow), 1.4·10⁷ at `n = 15` (fine). This bounds partition DPs like
`dp[mask] = min over submask s of dp[mask ^ s] + cost(s)`.

**All supermasks of `m` within `n` bits, increasing:**

```cpp
for (int s = m; s < (1 << n); s = (s + 1) | m) { /* use s */ }
```

**Gray code:** `gray(i) = i ^ (i >> 1)`. Consecutive codes differ in exactly one bit because
`gray(i) ^ gray(i+1) = (i ^ (i+1)) ^ ((i ^ (i+1)) >> 1)`, and `i ^ (i+1) = 2^{k+1} − 1` where `k`
is the number of trailing ones of `i`, so the xor is `(2^{k+1}−1) ^ (2^k − 1) = 2^k` — a single
bit. Inverse: `i = g ^ (g>>1) ^ (g>>2) ^ …` (prefix xor of the bits).

```
i   : 000 001 010 011 100 101 110 111
gray: 000 001 011 010 110 111 101 100
```

Uses: CSES Gray Code (2205) directly; iterating subsets so that consecutive subsets differ by one
element (incremental sum/xor maintenance: O(1) per subset instead of O(n)).

**Incremental subset sums** without Gray code: `sum[m] = sum[m & (m-1)] + a[ctz(m)]`, O(2^n)
total — used in Two Sets II style brute forces and in meet in the middle.

## 4. Bitmask DP

State = (subset of processed elements, small extra info). `2^n · n` states with `n ≤ 20` is
2·10⁷ — fine. `2^n · n²` transitions at `n = 20` is 4·10⁸ simple ops — fine in 1 s with tight
inner loops, borderline with `vector<vector<>>` indirection; use a flat array.

### 4.1 Hamiltonian paths (CSES 1690 Hamiltonian Flights)

`dp[mask][v]` = number of paths starting at `s`, visiting exactly the vertices in `mask`, ending
at `v`. Transition: `dp[mask | 1<<w][w] += dp[mask][v] · adj[v][w]`. Invariant: masks are
processed in increasing numeric order and every transition goes to a strictly larger mask, so
`dp[mask]` is final when read.

```cpp
ll hamiltonian_paths(int n, const vector<vector<int>>& adj, int s, int t, ll mod) {
    vector<vector<ll>> dp(1 << n, vector<ll>(n, 0));
    dp[1 << s][s] = 1;
    for (int mask = 0; mask < (1 << n); mask++) {
        if (!(mask >> s & 1)) continue;
        for (int v = 0; v < n; v++) {
            if (!(mask >> v & 1) || dp[mask][v] == 0) continue;
            if (v == t && mask != (1 << n) - 1) continue;    // t must be the last vertex
            ll cur = dp[mask][v];
            for (int w = 0; w < n; w++)
                if (!(mask >> w & 1) && adj[v][w])
                    dp[mask | 1 << w][w] = (dp[mask | 1 << w][w] + cur * adj[v][w]) % mod;
        }
    }
    return dp[(1 << n) - 1][t];
}
```

`adj[v][w]` is the *multiplicity* of edge `v→w` (Hamiltonian Flights has parallel edges — count
them, do not dedupe). Memory: `2^20 · 20 · 8 B = 168 MB` as `ll` — too much for a 512 MB limit
only if you also store the graph badly, but do reduce: for counting mod 10⁹+7 store `int`
(84 MB), or iterate over adjacency lists instead of the `n²` matrix. The `dp[mask][v]==0` skip is
what makes sparse instances fast.

Trace, `n = 3`, edges 0→1, 0→2, 1→2, 2→1:

```
mask=001: v=0 → dp[011][1]=1, dp[101][2]=1
mask=011: v=1 → dp[111][2]=1
mask=101: v=2 → dp[111][1]=1   (but t=2, mask≠full → the v=2 row is skipped; dp[111][1] stays 0)
mask=111: answer dp[111][2] = 1   (0→1→2)
```

### 4.2 TSP, O(2^n · n²)

`dp[mask][v]` = minimum cost of a path from 0 through `mask` ending at `v`. Answer
`min_v dp[full][v] + w[v][0]`. Same loop shape with `min`. `n = 20`: 2^20·400 = 4·10⁸ — needs a
flat array and `-O2`; `n ≤ 18` is comfortable.

### 4.3 Partition into groups (CSES 1653 Elevator Rides)

`dp[mask] = (rides, weight of last ride)` minimized lexicographically. Add person `i` to the
solution for `mask ^ (1<<i)`: if it fits in the current last ride, extend it, else open a new
ride. Correctness: any optimal packing of `mask` can be reordered so that `i` is the last person
of the last ride, and greedy-filling within a fixed order is optimal for that order.
O(2^n · n).

```cpp
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
```

### 4.4 Other bitmask DP shapes

| Shape                                                  | State                          | Cost           |
|--------------------------------------------------------|--------------------------------|----------------|
| Assign n workers to n jobs, min cost                   | `dp[mask]`, worker = popcount  | O(2^n · n)     |
| Count permutations avoiding adjacent-pair constraints  | `dp[mask][last]`               | O(2^n · n²)    |
| Broken-profile / row DP on grids (Counting Tilings)    | `dp[col][profile]`             | O(n · m · 2^n) |
| Partition into k valid groups (`3^n` submask DP)       | `dp[mask]` over submasks       | O(3^n)         |
| Steiner tree on `k ≤ 10` terminals                     | `dp[mask][v]`                  | O(3^k n + 2^k m log n) |

Reduce memory with `dp[mask]` only when the last element is implied (popcount order) — Elevator
Rides is the example.

## 5. SOS DP — sum over subsets

Problem: given `a[0..2^n)`, compute `f[mask] = Σ_{sub ⊆ mask} a[sub]` for every mask. Naive
submask enumeration is 3^n. SOS does it in O(2^n · n):

```cpp
vector<ll> sos_subsets(vector<ll> f, int n) {        // f initially = a
    for (int i = 0; i < n; i++)
        for (int mask = 0; mask < (1 << n); mask++)
            if (mask >> i & 1) f[mask] += f[mask ^ (1 << i)];
    return f;
}
```

**Proof (invariant).** Define `f_i[mask] = Σ a[sub]` over `sub` that (a) equal `mask` on bits
`≥ i` and (b) are a subset of `mask` on bits `< i`. Then `f_0 = a` and `f_n = f`. Step `i` turns
`f_i` into `f_{i+1}`: for masks with bit `i` clear, condition (b) on bit `i` forces `sub_i = 0 =
mask_i`, so nothing changes; for masks with bit `i` set, `sub_i` may be 1 (already counted in
`f_i[mask]`) or 0 (counted in `f_i[mask ^ (1<<i)]`, whose bits `≥ i` agree with `mask` except
bit `i`, which is what we want). Both terms use `f_i` values: the write to `f[mask]` reads
`f[mask ^ (1<<i)]` which has bit `i` clear and is *not* modified during round `i`. ∎

This is an n-dimensional prefix sum over `{0,1}^n`, one dimension at a time. Same skeleton:

| Variant                            | Inner line                                           |
|------------------------------------|------------------------------------------------------|
| sum over supersets                 | `if (!(mask>>i&1)) f[mask] += f[mask \| 1<<i]`       |
| inverse (recover `a` from `f`)     | same loops with `-=` (Möbius inversion on the lattice)|
| max over subsets                   | `max` instead of `+`                                  |
| count of `x` in array with `x ⊆ mask` | `a[x]++` then SOS                                 |
| count pairs with `a_i & a_j == 0`  | superset-sum of complement, or subset-sum of `~a_i`   |
| Subset convolution `h[m]=Σ_{s⊆m} f[s]g[m\s]` | rank by popcount + SOS per rank, O(2^n n²)  |

Applications: CSES 1654 SOS Bit Problem (for each `x` count `y` with `x|y=x`, `x&y=x`, `x&y≠0`)
and 3141 And Subset Count; CF "Compatible Numbers" (find `y` with `x & y = 0` — superset SOS on
`~x` storing an index). Memory: `2^20` ints = 4 MB; `2^20` `ll` = 8 MB.

Trace `n = 2`, `a = [a0, a1, a2, a3]` (index = mask):

```
after i=0: f = [a0, a0+a1, a2, a2+a3]
after i=1: f = [a0, a0+a1, a0+a2, a0+a1+a2+a3]
```

## 6. XOR and the linear basis over GF(2)

XOR facts used constantly: `a ^ a = 0`, `a ^ 0 = a`, commutative and associative; prefix xors
`P[i] = a_0 ^ … ^ a_{i-1}` give `xor(l, r) = P[r+1] ^ P[l]` (Range Xor Queries 1650, Maximum Xor
Subarray 1655 → basis or trie). `a + b = (a ^ b) + 2 (a & b)`. Parity of a set = xor of 1s.
`x ^ y < x + y` when they share bits. Bit `k` of Σ over subsets: independent per bit.

**Vectors over GF(2).** A 60-bit integer is a vector in `GF(2)^60`; xor is vector addition. The
set of all subset xors of `a_1..a_n` is the *span*, a subspace of size `2^rank`. A basis in
echelon form: `b[i]` is either 0 or a vector whose highest set bit is `i`.

```cpp
struct XorBasis {
    static const int B = 60;
    ll b[B]; int sz;
    XorBasis() : sz(0) { memset(b, 0, sizeof b); }
    bool insert(ll x) {                        // false if x already in span
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
    ll max_xor(ll x = 0) const {               // max over span elements s of x ^ s
        for (int i = B - 1; i >= 0; i--) if ((x ^ b[i]) > x) x ^= b[i];
        return x;
    }
    void reduce() {                            // reduced echelon: each pivot bit in one vector
        for (int i = 0; i < B; i++) if (b[i])
            for (int j = i + 1; j < B; j++) if (b[j] && (b[j] >> i & 1)) b[j] ^= b[i];
    }
    ll kth(ll k) const {                       // k in [0, 2^sz), after reduce(); k=0 gives 0
        ll res = 0; int j = 0;
        for (int i = 0; i < B; i++) if (b[i]) { if (k >> j & 1) res ^= b[i]; j++; }
        return res;
    }
};
```

**Insert correctness.** Reducing `x` by `b[i]` at its highest bit `i` either zeroes the bit
(strictly lowering the highest set bit) or places `x` as the new pivot. `x` ends as 0 exactly when
it was in the span of the existing pivots (an echelon basis: leading bits distinct ⇒ linearly
independent, and reduction is exact Gaussian elimination). O(B) per insert, O(nB) total, B = 60
or 30. The span has `2^sz` elements, all distinct; 0 is in it (empty subset), and if `n > sz`
some *non-empty* subset xors to 0 as well.

**max_xor greedy.** Going from the highest pivot down: if xoring `b[i]` sets bit `i` of the
current value (it can only affect bits ≤ i), do it — bit `i` outweighs everything below. Since
`b[i]`'s highest bit is `i`, `(x ^ b[i]) > x` ⇔ bit `i` of `x` is 0. Answers "max xor of a subset"
(3191 Maximum Xor Subset) and "max `q ^ subset`".

**k-th smallest.** After `reduce()`, pivot vectors `v_0 < v_1 < … < v_{sz-1}` (by leading bit)
have pairwise disjoint pivot bits, so xoring a subset of them produces a number whose bit at
pivot `j` equals "was `v_j` chosen"; comparing two span elements is comparing their choice
vectors as binary numbers, highest pivot first. Hence the k-th smallest is `xor of v_j for bits
j of k`. (3192 K Subset Xors; 3211 Number of Subset Xors = `2^sz`.)

Trace: insert 6 (110), 5 (101), 3 (011) with B = 3.

```
insert 6: b[2]=110                       sz=1
insert 5: bit2 set, x^=b[2] → 011; bit1 set, b[1] empty → b[1]=011   sz=2
insert 3: bit1 set, x^=b[1] → 000 → already in span (3 = 6^5)      sz=2
reduce:  b[2]=110 has bit1 → b[2]^=b[1] → 101.  pivots: b[1]=011, b[2]=101
span sorted: 0, 3, 5, 6  →  kth(0)=0, kth(1)=011, kth(2)=101, kth(3)=110 ✓
max_xor(): 0 → ^101=5 → ^011? (5^3=6 > 5) → 6 ✓
```

Extensions: basis with timestamps for "xor basis on a prefix / range" (keep the most recent
vector at each pivot — CF 1100F "Ivan and Burgers"); basis merging for tree paths; xor
subarray max via prefix xors + basis or a binary trie (trie for "max `q ^ a_i`" with insert/erase).

## 7. `std::bitset` — the 64× speedup

`bitset<N>` (N a compile-time constant) stores N bits in `⌈N/64⌉` words; `&, |, ^, ~, <<, >>`,
`count()`, `any()`, `none()`, `test(i)`, `set/reset/flip`, `operator[]` are all O(N/64) or O(1).
For run-time sizes either over-allocate (`bitset<5001>`) or write a small word-array struct
(`Bits` in `example.cpp`). Never `vector<bool>` for this — no word-level operators.

GCC libstdc++ extensions (available on CSES/CF/oj.uz/AtCoder; **absent on Apple clang/libc++**):

```cpp
for (int i = bs._Find_first(); i < N; i = bs._Find_next(i)) { /* i is a set bit */ }
```

`_Find_first()` returns `N` when no bit is set; `_Find_next(i)` returns the first set index `> i`
or `N`. Both O(N/64). Wrap them behind `find_first/find_next` helpers with an `#ifdef __GLIBCXX__`
fallback so the same file compiles locally (see `example.cpp`).

### 7.1 Subset-sum reachability (Money Sums 1745, Two Sets II style checks)

```cpp
bitset<100001> bs; bs[0] = 1;
for (int c : coins) bs |= bs << c;          // O(n * S / 64)
// bs[s] == 1  ⇔  some subset sums to s
```

`n = 100, S = 10^5`: 100 · 1563 words ≈ 1.6·10⁵ word ops vs 10⁷ for bool DP. Only feasibility,
not counts — for counting mod p you need the ordinary DP.

### 7.2 Transitive closure / reachability (Reachable Nodes 2138, Reachability Queries 2143)

Rows `reach[i]`. For a DAG process vertices in reverse topological order:
`reach[v] = {v} ∪ ⋃_{v→u} reach[u]`, cost O((n + m) · n / 64): with `n = 5·10⁴, m = 5·10⁴`
that is 10⁵ · 782 words ≈ 8·10⁷ — fast; but `5·10⁴` rows × 782 words × 8 B = 312 MB, too much,
so process sources in **blocks of 64**: a `ll` per vertex holds "which of these 64 sources reach
me", one DP pass per block, total O(n/64 · (n+m)). For general digraphs, condense SCCs first
(Reachability Queries). The Floyd–Warshall-shaped closure for dense small graphs:

```cpp
for (int k = 0; k < n; k++)
    for (int i = 0; i < n; i++)
        if (reach[i][k]) reach[i] |= reach[k];      // O(n^3 / 64); n = 2000 → 1.25e8 word ops
```

### 7.3 Dense-graph BFS / complement-graph BFS

Keep an `unvisited` bitset. From `v`, the new frontier is `adj[v] & unvisited` (or
`~adj[v] & unvisited` on the complement graph); iterate it with `_Find_next`, clearing bits. Each
vertex is removed from `unvisited` once, each `&` costs `n/64`: O(n²/64) total for the whole BFS
even on the complement of a sparse graph, where the naive approach is O(n²).

### 7.4 Other bitset wins

| Task                                                  | Trick                                                       |
|-------------------------------------------------------|-------------------------------------------------------------|
| Count triangles / common neighbours in dense graph    | `(adj[u] & adj[v]).count()`, O(n³/64)                       |
| Hamming Distance (2136): min over pairs, n=2·10⁴, k=30 | `popcount(a^b)` on ints; bitset only if k > 64            |
| String matching with wildcards, all positions         | per-letter position bitsets, shift-and: O(nm/64)            |
| Bipartite matching (Kuhn) on dense graphs             | `unvisited & adj[v]` to find next candidate: O(n³/64)       |
| Corner Subgrid Check (3360) `n=3000` rows             | row bitsets, `(row_i & row_j).count() ≥ 2`, O(n³/64)        |
| Gaussian elimination mod 2 (3154 System of Linear Equations over GF(2) variant, Xor problems) | rows as bitsets: O(n³/64) |

Memory: `bitset<5000>` = 632 B; 5000 of them = 3.1 MB. `bitset<100000>` × 2000 = 25 MB.

## 8. Meet in the middle (CSES 1628)

`n ≤ 40`, count subsets with sum `x`: enumerate the `2^20` subset sums of each half (incremental
`sum[m] = sum[m & (m-1)] + a[ctz(m)]`), sort one side, for each left sum binary-search
`x − s` on the right (or two pointers after sorting both). O(2^{n/2} · n). Memory: two arrays
of 2^20 `ll` = 16 MB. Sorting 10⁶ elements ≈ 0.1 s. The idea generalizes: split "choose 4 numbers
that sum to x" into pairs, split a 2^n search into 2^{n/2} · 2^{n/2} with hashing on the boundary.

## 9. Overflow-safe multiplication modulo m

`a * b % m` overflows once `m > 2^31.5 ≈ 3·10⁹`. Options:

```cpp
ull mulmod128(ull a, ull b, ull m) { return (ull)((unsigned __int128)a * b % m); }   // best

ll mulmod_ld(ll a, ll b, ll m) {             // when __int128 is unavailable; a,b < m < 2^63
    ll q = (ll)((long double)a * b / m);     // q is within ±1-2 of the true quotient
    ll r = (ll)((ull)a * (ull)b - (ull)q * (ull)m);   // exact mod 2^64; true value in (-2m, 2m)
    r %= m; if (r < 0) r += m;
    return r;
}
```

The long-double trick relies on an 80-bit `long double` (64-bit mantissa) — true on x86 Linux
judges. **On arm64 (Apple Silicon Macs, Raspberry Pi judges) `long double` is 53-bit and the trick
is only correct for `m < 2^52`** (`numeric_limits<long double>::digits` tells you which). Test
locally against `__int128`, as `example.cpp` does. `__int128` `%` is ~30–60 ns; if that is the
bottleneck (Pollard rho, Miller–Rabin over millions of numbers) use Montgomery multiplication.

Also: `(a + b) % m` overflows for `a, b < m < 2^63`? No — `a + b < 2^64` as unsigned; as signed
`ll` it overflows when `m > 2^62`. Use `a += b; if (a >= m) a -= m;` on unsigned.

## 10. Contest bit tricks

| Need                                         | Expression                                          |
|----------------------------------------------|-----------------------------------------------------|
| parity of x                                  | `x & 1`, `__builtin_parityll(x)`                    |
| x is a power of two                          | `x > 0 && (x & (x-1)) == 0`                         |
| round up to multiple of 2^k                  | `(x + (1<<k) - 1) & ~((1<<k) - 1)`                  |
| ⌊log₂ x⌋                                     | `63 - __builtin_clzll(x)` (x > 0)                   |
| swap without temp (don't)                    | `a ^= b; b ^= a; a ^= b;` — breaks when `&a == &b`  |
| min(a,b) branchless (usually slower than cmov)| `b ^ ((a ^ b) & -(a < b))`                          |
| all ones of width k                          | `(1ULL << k) - 1` — k = 64 is UB; special-case      |
| highest set bit value                        | `1ULL << (63 - clz(x))`                             |
| next permutation of bits with same popcount  | Gosper's hack: `t = x \| (x-1); y = (t+1) \| (((~t & -~t) - 1) >> (ctz(x) + 1))` |
| xor of 0..n                                  | by `n % 4`: `n, 1, n+1, 0`                          |
| a % 2^k for negative a (two's complement)    | `a & ((1<<k)-1)` gives non-negative result          |

Remember `#define int long long` exists and some people use it; it doubles memory and makes
`1 << k` safe but `int main` needs `signed main`. Prefer explicit `ll`.

## Recognition cheatsheet

| Statement signal                                        | Technique                          | Complexity          |
|---------------------------------------------------------|------------------------------------|---------------------|
| n ≤ 20, choose order / assign / visit all               | bitmask DP `dp[mask][last]`        | O(2^n n²)           |
| n ≤ 20, partition into groups with capacity             | `dp[mask]` (rides, last weight)    | O(2^n n)            |
| n ≤ 15–16, partition into arbitrary groups              | submask DP                         | O(3^n)              |
| n ≤ 40, count subsets with sum                          | meet in the middle                 | O(2^{n/2} n)        |
| values < 2^20, "count y ⊆ x / y ⊇ x / y & x = 0"        | SOS DP                             | O(2^k k)            |
| "xor of some subset", max/min/k-th/count                | linear basis                       | O(n · 60)           |
| "xor of subarray" max                                   | prefix xor + trie / basis          | O(n · 30)           |
| n ≤ 5000 reachability / n³ pairwise ops                 | bitset rows                        | O(n³/64)            |
| n ≤ 100, sum ≤ 10⁵, feasibility only                    | bitset knapsack                    | O(nS/64)            |
| dense or complement graph BFS, n ≤ 10⁵ with m huge      | bitset `unvisited`                 | O(n²/64)            |
| Σ popcount(1..n), n ≤ 10^15                             | per-bit periodicity                | O(log n)            |
| mod m with m up to 10^18                                | `__int128` mulmod                  | O(1)                |

## Implementation checklist for contests

- Every shift of a 1 that may reach bit 31+: `1LL <<` / `1ULL <<`. Grep your file for `1 <<`.
- `__builtin_*ll` for `long long` arguments; `ctz/clz` never called on 0.
- Parentheses around `&`, `|`, `^` next to comparisons.
- Submask loop terminates on `s == 0` *after* processing it; superset loop bound `< (1<<n)`.
- Bitmask DP: array size `1 << n` (not `n`), `dp` initialized to identity (`0` for counts, `INF`
  for min), memory computed: `2^n · n · sizeof` — switch to `int` or drop the second dimension if
  > 256 MB.
- Hamiltonian-path style: the target vertex is only allowed as the last step.
- SOS: outer loop over bits, inner over masks, read `mask ^ (1<<i)` — never the other nesting
  with in-place updates.
- Xor basis: `B = 60` for `a_i ≤ 10^18`, `B = 30` for `≤ 10^9`; k-th smallest needs `reduce()`
  and the convention whether the empty subset (value 0) counts.
- `bitset<N>`: `N` ≥ max size + 1; `_Find_next` only on GCC; use `count()` not a manual loop.
- Mulmod: `__int128` on the judge; if using long double, know the platform's mantissa width.
- Fast IO (`ios::sync_with_stdio(false); cin.tie(nullptr);`), `'\n'` not `endl`, in every file.

## Further reading

- CPH (Laaksonen, *Competitive Programmer's Handbook*): Ch. 10 Bit manipulation (10.1–10.5), Ch. 5.4
  meet in the middle, Ch. 19.4 (bitset applications in graph problems).
- cp-algorithms.com: "Bit manipulation", "Submask enumeration", "Gray code", "Sum over subsets
  DP" (under Combinatorics/others), "Linear algebra: Gauss over GF(2)", "Bitmask DP (Hamiltonian
  path)".
- Codeforces blog: "SOS Dynamic Programming" (usaxena95), "Linear basis" tutorials (xor basis).
- Knuth, *TAOCP* Vol. 4A, §7.1.3 "Bitwise tricks and techniques" — the source of Gosper's hack and
  the 3^n argument.
- Hacker's Delight (Warren) Ch. 2–5 for the arithmetic of bit tricks.

## You can move on when...

- You can write the submask loop, SOS loop, xor basis `insert/max_xor/kth`, and Hamiltonian
  path DP from memory with no compile errors on the first try.
- You can say without hesitation why `for mask, for submask` is 3^n and why SOS is `2^n n`.
- You've solved Bit Strings, Gray Code, Counting Bits, Hamiltonian Flights, Elevator Rides,
  SOS Bit Problem, Maximum Xor Subset, K Subset Xors, Meet in the Middle, Reachable Nodes, and at
  least one bitset-closure and one submask-DP problem from `problems.md`.
- Given `n ≤ 20` in a statement you immediately estimate `2^n·n` vs `2^n·n²` vs `3^n` memory and
  time before writing anything.
