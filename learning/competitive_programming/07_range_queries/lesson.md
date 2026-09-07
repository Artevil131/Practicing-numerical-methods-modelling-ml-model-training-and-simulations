# Chapter 07 — Range Queries

Prerequisites: `../../algorithms_learning/01_arrays_and_hashing/lesson.md` (prefix sums as a
pattern), `../../algorithms_learning/03_binary_search/lesson.md` (monotone predicates),
`../../cpp_learning/05_templates` (function objects, `decltype`). This chapter turns "prefix sums"
into a complete toolbox: every static and dynamic array structure that an IOI finalist is expected
to write from memory, with the proofs that make them safe to modify under contest pressure.

## What you'll be able to do after this chapter

- Answer `q` range aggregate queries on an array of `n ≤ 2·10^5` in `O((n+q) log n)` — sum, min,
  max, gcd, xor, "best subarray sum", matrix products — with point or range updates.
- Pick the cheapest structure that fits: prefix sums → sparse table → Fenwick → segment tree →
  lazy segment tree → persistent/dynamic → sqrt/Mo, and justify the choice in one sentence.
- Derive lazy-propagation push-down order from first principles for composed operations
  (range assign + range add), rather than pattern-matching a template.
- Descend a segment tree ("tree walk") to answer "first index where a prefix condition holds" in
  `O(log n)` instead of `O(log² n)`.
- Use persistence to answer "k-th smallest in `a[l..r]`", and Mo's algorithm for anything with an
  `O(1)` add/remove but no efficient combine.

## Where this shows up in contests

| Signal in the statement | Structure |
|---|---|
| `n, q ≤ 2·10^5`, "sum/min of `a[l..r]`", no updates | prefix sums / sparse table |
| same, "update `a[k] = u`" | Fenwick (sum, xor) or segment tree (min/max) |
| "add `x` to `a[l..r]`" and "value at `k`" | difference array in Fenwick |
| "add `x` to `a[l..r]`" and "sum of `a[l..r]`" | two Fenwicks or lazy segment tree |
| "set `a[l..r] = x`", "add to `a[l..r]`", both | lazy segment tree with composed lazies |
| "first position with `a[i] ≥ x`", "k-th one" | segment tree walk / Fenwick binary lifting |
| "count of values `< x` in `a[l..r]`", no updates | merge sort tree, offline BIT (ch. 08), wavelet |
| "k-th smallest in `a[l..r]`" | persistent segment tree |
| coordinates up to `10^9`, `n ≤ 2·10^5` | coordinate compression or dynamic segment tree |
| "number of distinct / mode / something with no inverse" | Mo's algorithm |
| "`a[i] = min(a[i], x)` on range" + range sum | segment tree beats (mention only) |

Placements: Codeforces Div2 C–E almost every round contains one of these; BOI/CEOI typically
hide a segment tree inside a harder reduction (sweep line, offline sorting, Euler tour of a tree —
ch. 10); IOI subtasks 3–4 of a data-structure problem are usually "the segment tree version".
Time budget: `2·10^5 · log₂(2·10^5) ≈ 3.5·10^6` node visits per operation type — a segment tree
with `n = q = 2·10^5` runs in ~0.1 s; `n = q = 10^6` fits in 1 s with an iterative tree and fast IO.

---

## 1. Static structures

### 1.1 Prefix sums (1D)

`p[i] = a[0] + … + a[i-1]`, `p[0] = 0`. Then `sum(l..r) = p[r+1] − p[l]`. Build `O(n)`, query
`O(1)`, no updates. Works for any **group** operation (has an inverse): `+`, xor, `+` mod `m`.
Not for min/max/gcd.

```cpp
struct Prefix1D {
    vector<ll> p;
    explicit Prefix1D(const vector<ll>& a) : p(a.size() + 1, 0) {
        for (size_t i = 0; i < a.size(); i++) p[i + 1] = p[i] + a[i];
    }
    ll sum(int l, int r) const { return p[r + 1] - p[l]; }   // inclusive
};
```

Trace on `a = [3, -1, 4, 1]`: `p = [0, 3, 2, 6, 7]`; `sum(1..2) = p[3] − p[1] = 6 − 3 = 3 = -1+4`.

Pitfall: `a[i]` up to `10^9`, `n = 2·10^5` ⇒ `p` up to `2·10^14`: **`long long`**. Read input with
`scanf`/fast `cin` (`ios::sync_with_stdio(false); cin.tie(nullptr);`) — `10^6` numbers through a
synced `cin` alone is ~0.3 s.

### 1.2 Prefix sums (2D)

`p[i][j] = Σ a[x][y]` for `x < i, y < j`. Build by inclusion–exclusion:

```
p[i+1][j+1] = a[i][j] + p[i][j+1] + p[i+1][j] − p[i][j]
```

Query rectangle `(r1,c1)–(r2,c2)`:

```
        c1        c2
   r1  +---------+       sum = p[r2+1][c2+1]
       |  A   B  |             − p[r1][c2+1]     (rows above)
       |  C   D  |             − p[r2+1][c1]     (cols left)
   r2  +---------+             + p[r1][c1]       (added back: top-left corner subtracted twice)
```

`O(nm)` memory; `1000 × 1000` of `long long` = 8 MB, fine. Used in CSES Forest Queries (1652),
subgrid counting, and as a subroutine in "sum over a rectangle around every cell" problems.

### 1.3 Difference array (offline range add)

To apply `k` operations "add `v` to `a[l..r]`" and only read the array afterwards:
`d[l] += v; d[r+1] −= v;` then `a = prefix(d)`. `O(n + k)`. The same trick on the *prefix-sum
level* gives "add an arithmetic progression to a range" (apply the difference twice). CSES
Range Update Queries (1651) is this made online — see Fenwick §2.3.

### 1.4 Sparse table — O(1) idempotent range queries

For an **idempotent** associative op (`min`, `max`, `gcd`, `and`, `or` — `op(x, x) = x`), store
`t[k][i] = op(a[i .. i + 2^k − 1])`. A query `[l, r]` with `len = r − l + 1`, `k = ⌊log₂ len⌋`
is covered by two blocks of length `2^k` that **overlap**; overlap is harmless exactly because of
idempotence.

```
a:      5  2  7  1  8  3  6  4          query [1, 6]  (len 6, k = 2)
t[0]:   5  2  7  1  8  3  6  4          block A = t[2][1] = min(2,7,1,8) = 1
t[1]:   2  2  1  1  3  3  4  .          block B = t[2][3] = min(1,8,3,6) = 1
t[2]:   1  1  1  1  3  .  .  .          answer = min(1, 1) = 1
t[3]:   1  .  .  .  .  .  .  .
```

```cpp
struct SparseTable {
    int n, K; vector<vector<ll>> t; vector<int> lg;
    explicit SparseTable(const vector<ll>& a) : n(a.size()), lg(n + 1, 0) {
        for (int i = 2; i <= n; i++) lg[i] = lg[i / 2] + 1;      // floor(log2 i)
        K = lg[n] + 1; t.assign(K, vector<ll>(n)); t[0] = a;
        for (int k = 1; k < K; k++)
            for (int i = 0; i + (1 << k) <= n; i++)
                t[k][i] = min(t[k - 1][i], t[k - 1][i + (1 << (k - 1))]);
    }
    ll query(int l, int r) const {
        int k = lg[r - l + 1];
        return min(t[k][l], t[k][r - (1 << k) + 1]);
    }
};
```

Build `O(n log n)` time and memory (`2·10^5 · 18 · 8 B ≈ 29 MB` for `long long` — check the
memory limit; use `int` if values fit). Query `O(1)`. **No updates.**

Why the log table instead of `__lg(len)` or `31 − __builtin_clz(len)`: both are fine on GCC, the
table is just portable and branch-free; `std::log2` in floating point is *not* safe (rounding).

**Non-idempotent ops (sum) in O(1)**: the *disjoint sparse table* stores, for each level `k` and
each block of size `2^k`, prefix/suffix aggregates from the block's midpoint; a query `[l, r]`
with `l ≠ r` finds the level where `l` and `r` fall on different sides of a midpoint
(`k = lg[l ^ r]`) and combines one suffix with one prefix. Same `O(n log n)` build, `O(1)` query,
works for any associative op. Rarely needed — a Fenwick is usually fast enough — but it is the
tool when `q ≈ 10^7` sum queries are given as a generated stream.

---

## 2. Fenwick tree (binary indexed tree)

### 2.1 Idea and the lowbit proof

Use 1-indexed positions internally. `lowbit(i) = i & −i` is the value of the lowest set bit
(two's complement: `−i = ~i + 1` flips all bits above the lowest set bit and keeps it).
Cell `t[i]` stores the sum of `a[i − lowbit(i) + 1 .. i]`, a block of length `lowbit(i)` ending
at `i`.

```
i:     1    2    3    4    5    6    7    8
t[i]:  a1   a1+a2  a3  a1..a4  a5  a5+a6  a7  a1..a8

              8 ────────────────────────────────
              4 ──────────          
              2 ────    6 ────
              1    3    5    7        (block lengths = lowbit)
```

**Prefix query** `prefix(i)`: add `t[i]`, then `i −= lowbit(i)`. Each step removes the lowest set
bit, so the visited blocks are `[i − lowbit(i) + 1, i]`, `[i' − lowbit(i') + 1, i']`, … which tile
`[1, i]` exactly (each block ends where the previous one starts). At most `popcount(i) ≤ log₂ n`
steps.

**Point update** `add(i, v)`: every block containing `i` must change. Blocks are of the form
`[j − lowbit(j) + 1, j]` with `j ≥ i`. Claim: the smallest such `j > i` is `i + lowbit(i)`.
Proof sketch: adding `lowbit(i)` carries into the next higher zero bit, producing a `j` whose
lowbit is larger than `lowbit(i)`, hence whose block `[j − lowbit(j) + 1, j]` starts at or before
`i − lowbit(i) + 1 ≤ i`. Any `j'` strictly between `i` and `i + lowbit(i)` has
`lowbit(j') < lowbit(i)` (its low bits are those of `j' − i < lowbit(i)`), so its block starts
after `i`. Repeating `i += lowbit(i)` visits exactly the ancestors — `O(log n)` steps.

```cpp
struct Fenwick {                       // 0-indexed API, 1-indexed storage
    int n; vector<ll> t;
    explicit Fenwick(int n) : n(n), t(n + 1, 0) {}
    void add(int i, ll v) { for (i++; i <= n; i += i & -i) t[i] += v; }
    ll prefix(int i) const {           // sum a[0..i], prefix(-1) == 0
        ll s = 0; for (i++; i > 0; i -= i & -i) s += t[i]; return s;
    }
    ll sum(int l, int r) const { return prefix(r) - prefix(l - 1); }
};
```

Trace: `n = 8`, `add(4, 5)` (0-indexed 4 → internal 5): `t[5] += 5`, `5 + 1 = 6`: `t[6] += 5`,
`6 + 2 = 8`: `t[8] += 5`, stop. `prefix(5)` (internal 6): `t[6] + t[4]` — blocks `[5,6]` and
`[1,4]`.

Constants: a Fenwick is ~3× faster than a recursive segment tree and a few lines long; when the
op is a group op (sum, xor, count), prefer it. `O(n)` build from an array is possible
(`t[i] += a[i]; j = i + lowbit(i); if j ≤ n: t[j] += t[i]`), but `n` calls to `add` is usually fine.

**Point assign** `a[k] = u` = `add(k, u − a[k])` — keep a copy of `a`.

### 2.2 Order statistics: k-th element by binary lifting

Store counts (`a[i] ≥ 0`). "Find the smallest `idx` with `prefix(idx) ≥ k`" — the k-th smallest
present value, or the k-th remaining person in CSES List Removals (1749). Descend from the top
power of two:

```cpp
int kth(ll k) const {                 // 1 <= k <= total; returns 0-indexed position
    int pos = 0, LOG = 1; while ((1 << LOG) <= n) LOG++;
    for (int pw = 1 << LOG; pw; pw >>= 1)
        if (pos + pw <= n && t[pos + pw] < k) { pos += pw; k -= t[pos]; }
    return pos;                       // pos = last 1-indexed prefix with sum < k
}
```

Correctness: after processing power `pw`, `pos` is a multiple of `pw` and `t[pos + pw]` is exactly
the sum of `a[pos+1 .. pos+pw]` (because `lowbit(pos + pw) = pw` when `pos` is a multiple of
`2·pw` — which holds by induction). So the loop is a binary search on the prefix sums that
reads one cell per level: `O(log n)` instead of `O(log² n)` for a binary search on `prefix()`.

### 2.3 Range add + point query

Keep the *difference array* in a Fenwick: `range_add(l, r, v) = add(l, v), add(r+1, −v)`;
`point(i) = prefix(i)`. CSES Range Update Queries (1651).

### 2.4 Range add + range sum (two Fenwicks)

After `range_add(l, r, v)`, the contribution to `prefix(i)` is `0` for `i < l`, `v·(i − l + 1)` on
`[l, r]`, and `v·(r − l + 1)` after. Write it as `(i + 1)·B1.prefix(i) − B2.prefix(i)` with

```
B1: +v at l,       −v at r+1
B2: +v·l at l,     −v·(r+1) at r+1
```

Check for `l ≤ i ≤ r`: `(i+1)·v − v·l = v(i − l + 1)`. For `i > r`: `0 − (v·l − v(r+1)) = v(r−l+1)`.
Two Fenwicks, `O(log n)` per op, `long long` everywhere (`v·(r+1)` up to `10^9 · 2·10^5`).

### 2.5 2D Fenwick

`t[i][j]`, both loops with lowbit; `O(log n · log m)` per op, `O(nm)` memory. Point update +
rectangle sum (CSES Forest Queries II, 1739). For `n = m = 1000`, `q = 2·10^5`: `2·10^5 · 100`
operations — trivial. For point coordinates up to `10^9` with offline queries, compress or use the
offline sweep + 1D BIT (ch. 08 §7).

### 2.6 Coordinate compression (offline)

When indices are values in `[1, 10^9]` but there are only `n + q` distinct ones: collect all
values that will ever be touched, `sort` + `unique`, replace each by `lower_bound` index. CSES
Salary Queries (1144): compress all initial salaries and all update values (they are known in
advance since the input is fully read first), then a Fenwick of counts answers "how many salaries
in `[a, b]`" as `prefix(ub(b)) − prefix(lb(a))`. If the input must be processed truly online, use
a dynamic segment tree (§4.6) or an order-statistics tree (`__gnu_pbds`).

---

## 3. Segment tree

### 3.1 Structure and complexity

A binary tree over index ranges; node `v` covers `[lo, hi]`, children cover `[lo, mid]` and
`[mid+1, hi]`. Leaves are single elements; an internal node stores `combine(left, right)` for any
**associative** op (no inverse needed, no idempotence needed). Height `⌈log₂ n⌉`.

Query `[l, r]` decomposes into `≤ 2⌈log₂ n⌉` canonical nodes: at each level at most two nodes are
"partially covered" (the ones containing `l` and `r`), everything between them is fully covered
and returned without descending. Point update touches one root-to-leaf path. Memory `2n` for the
iterative version, `4n` for the recursive one with `v → 2v, 2v+1` indexing.

### 3.2 Iterative bottom-up version (any n, any associative op)

Leaves live at `t[n .. 2n−1]`, node `i` has children `2i, 2i+1`, parent `i >> 1`. For a query on
`[l, r)`, walk both borders up; whenever `l` is a right child (`l & 1`) its node is fully inside
the query — consume it and move right; symmetrically for `r`.

```cpp
template <class T, class F>
struct SegTree {
    int n; vector<T> t; T id; F f;
    SegTree(int n, T id, F f) : n(n), t(2 * n, id), id(id), f(f) {}
    void build(const vector<T>& a) {
        for (int i = 0; i < n; i++) t[n + i] = a[i];
        for (int i = n - 1; i >= 1; i--) t[i] = f(t[2 * i], t[2 * i + 1]);
    }
    void set(int p, T v) {
        for (t[p += n] = v; p > 1; p >>= 1) t[p >> 1] = f(t[p & ~1], t[p | 1]);
    }
    T query(int l, int r) const {                       // half-open [l, r)
        T resl = id, resr = id;
        for (l += n, r += n; l < r; l >>= 1, r >>= 1) {
            if (l & 1) resl = f(resl, t[l++]);
            if (r & 1) resr = f(t[--r], resr);
        }
        return f(resl, resr);
    }
};
// usage: auto mn = [](ll x, ll y){ return min(x, y); };
//        SegTree<ll, decltype(mn)> st(n, LLONG_MAX, mn);
```

Trace, `n = 5`, `a = [5, 3, 8, 6, 2]`, min:

```
index:   1     2     3     4     5     6     7     8     9
         2     3     2     8     5     3     8     6     2
                          ^leaves start at t[5]
t[4] = min(t[8], t[9]) = 2 ; t[3] = min(t[6], t[7]) = 3 ; t[2] = min(t[4], t[5]) = 2 ; t[1] = 2
query [1, 4) -> l = 6, r = 9 : l odd -> resl = t[6] = 3, l = 7 ; r odd -> resr = t[8] = 6, r = 8
                l = 3, r = 4 : l odd -> resl = min(3, t[3]) = 3, l = 4 ;  loop ends (l == r)
answer min(3, 6) = 3   (= min(3, 8, 6))
```

For non-power-of-two `n` the "tree" is a forest of perfect trees glued at odd indices; the two
accumulators keep left-to-right order, so **non-commutative** combines (matrix product, the
`BestNode` below) are still correct. `set` uses `t[p & ~1], t[p | 1]` (left child first) for the
same reason.

### 3.3 Nodes with structure: arbitrary associative combine

Anything associative: `sum`, `min`, `max`, `gcd`, `xor`, `and/or`, `2×2` matrices (linear
recurrences on ranges), affine maps `x ↦ ax + b` (composition), `(min, count of min)`,
`(max, second max)`. The classic non-trivial node is **maximum subarray sum** (CSES Subarray
Sum Queries, 1190):

```cpp
struct BestNode { ll sum, pref, suf, best; };          // best >= 0 (empty subarray allowed)
BestNode leaf(ll v) { return {v, max(0LL, v), max(0LL, v), max(0LL, v)}; }
BestNode combine(const BestNode& a, const BestNode& b) {
    return {a.sum + b.sum,
            max(a.pref, a.sum + b.pref),                // prefix: all of a, or a + prefix of b
            max(b.suf,  b.sum + a.suf),                 // suffix: all of b, or suffix of a + b
            max({a.best, b.best, a.suf + b.pref})};     // crossing the midpoint
}
```

Identity `{0, 0, 0, 0}`. If the empty subarray is *not* allowed, use `−∞` for `pref/suf/best` of
the identity and `leaf = {v, v, v, v}`. Recipe for designing a node: ask "what do I need from a
child to compute the parent's answer?" and store exactly that; verify associativity by checking
that combining three nodes in either order yields the same fields.

### 3.4 Recursive version and lazy propagation

The recursive form is the one to extend with range updates. Node `v` covers `[lo, hi]`, root
`v = 1`, children `2v`, `2v+1`, array size `4n`.

**Lazy propagation invariant**: the value stored at `v` is *correct for `v`'s whole range*, but
pending updates for the subtree may be parked in `lazy[v]` and not yet applied to the children.
Two rules make this consistent:

1. `apply(v, op)` updates `val[v]` **and** composes `op` into `lazy[v]` (so that the children
   will receive it later).
2. `push(v)` — before *any* descent into `v`'s children (update or query), apply `lazy[v]` to both
   children, then clear `lazy[v]`.

**Why push before descending, not after**: the recursion will read `val[2v]`, `val[2v+1]` to
recompute `val[v]` on the way up (pull). If a child still had a pending lazy that we then compose
*on top of* the new partial update, the composition order would be reversed (older op applied
after newer). Pushing first guarantees that each node's lazy is always *newer* than everything
already inside its children — a stack discipline: the root has the newest pending ops, leaves
the oldest.

**Composing two operation types** — range assign `x` and range add `d`. Represent the pending
operation as "(optionally) assign `x`, then add `d`":

| pending | new op | result |
|---|---|---|
| `(—, d)` | add `e` | `(—, d + e)` |
| `(x, d)` | add `e` | `(x, d + e)` |
| anything | assign `y` | `(y, 0)` — an assign wipes all history |

The effect on a node of length `len`: assign ⇒ `sum = x·len`; add ⇒ `sum += d·len`.

```cpp
struct LazySeg {
    int n; vector<ll> sum, add, asg; vector<char> has;   // has[v]: pending assign?
    explicit LazySeg(int n) : n(n), sum(4*n), add(4*n), asg(4*n), has(4*n) {}
    void apply_assign(int v, int len, ll x) { has[v] = 1; asg[v] = x; add[v] = 0; sum[v] = x * len; }
    void apply_add(int v, int len, ll d)    { add[v] += d; sum[v] += d * len; }
    void push(int v, int lo, int hi) {
        if (!has[v] && add[v] == 0) return;
        int mid = (lo + hi) / 2;
        for (int c : {2 * v, 2 * v + 1}) {
            int len = (c == 2 * v) ? mid - lo + 1 : hi - mid;
            if (has[v]) apply_assign(c, len, asg[v]);      // assign FIRST
            if (add[v]) apply_add(c, len, add[v]);         // then add
        }
        has[v] = 0; add[v] = 0;
    }
    void update(int v, int lo, int hi, int l, int r, ll x, bool assign) {
        if (r < lo || hi < l) return;
        if (l <= lo && hi <= r) { assign ? apply_assign(v, hi-lo+1, x) : apply_add(v, hi-lo+1, x); return; }
        push(v, lo, hi);
        int mid = (lo + hi) / 2;
        update(2*v, lo, mid, l, r, x, assign); update(2*v+1, mid+1, hi, l, r, x, assign);
        sum[v] = sum[2*v] + sum[2*v+1];
    }
    ll query(int v, int lo, int hi, int l, int r) {
        if (r < lo || hi < l) return 0;
        if (l <= lo && hi <= r) return sum[v];
        push(v, lo, hi);
        int mid = (lo + hi) / 2;
        return query(2*v, lo, mid, l, r) + query(2*v+1, mid+1, hi, l, r);
    }
};
```

Trace, `n = 4`, `a = [1, 1, 1, 1]`, then `add(0..3, +2)`, `assign(1..2, 5)`, `query(0..3)`:

```
after add(0..3, +2):   node1 [0,3] sum=12 lazy=(-,+2)        children untouched (sum 2, 2)
assign(1..2, 5):       node1 not fully covered -> push: node2 [0,1] sum=6 lazy=(-,+2)
                                                        node3 [2,3] sum=6 lazy=(-,+2)
                       node2 partially -> push: leaves 4,5 sum=3 each; node2 lazy cleared
                          leaf5 [1,1] fully covered: assign -> sum=5, lazy=(5,0)
                          node2 pull: sum = 3 + 5 = 8
                       node3 partially -> push: leaves 6,7 sum=3 each
                          leaf6 [2,2] assign -> sum=5 ; node3 pull: 5 + 3 = 8
                       node1 pull: 16
query(0..3) = 16 = 3 + 5 + 5 + 3   ok
```

Complexity `O(log n)` per operation (same canonical decomposition; `push` is `O(1)`). Memory `4n`
per array. `n = q = 2·10^5`: well under 0.2 s.

Other classic lazy pairs: range add + range min/max (`apply`: `mn += d`); range multiply + range
add mod `p` (`(a, b)`: `x ↦ ax + b`, composition `(a₂, b₂)∘(a₁, b₁) = (a₂a₁, a₂b₁ + b₂)`); range
flip (xor 1) + count ones (`cnt = len − cnt`, lazy = parity); range add arithmetic progression
(store `(first term, difference)` — CSES Polynomial Queries, 1736: lazy `(a, d)` adds
`a, a+d, a+2d, …` across the node; sum increases by `a·len + d·len(len−1)/2`, and the right child
gets first term `a + d·len_left`).

### 3.5 Segment tree walk ("descend", "binary search on the tree")

Many queries of the form "leftmost index satisfying a monotone predicate on a prefix aggregate"
can be answered in a single `O(log n)` descent instead of `O(log² n)` binary search over queries.

**First index with prefix sum ≥ k** (a ≥ 0):

```cpp
int walk(int v, int lo, int hi, ll k) {          // k <= sum[1]
    if (lo == hi) return lo;
    push(v, lo, hi);
    int mid = (lo + hi) / 2;
    if (sum[2*v] >= k) return walk(2*v, lo, mid, k);
    return walk(2*v+1, mid+1, hi, k - sum[2*v]);
}
```

**Leftmost `i ≥ l` with `a[i] ≥ x`** in a max tree (CSES Hotel Queries, 1143 — with `l = 0`):

```cpp
int first(int v, int lo, int hi, int l, ll x) {   // -1 if none
    if (hi < l || mx[v] < x) return -1;
    if (lo == hi) return lo;
    int mid = (lo + hi) / 2;
    int res = first(2*v, lo, mid, l, x);
    return res != -1 ? res : first(2*v+1, mid+1, hi, l, x);
}
```

Complexity: nodes fully inside `[l, n)` are pruned by `mx[v] < x` in `O(1)`; among the
`O(log n)` boundary nodes, once we enter a node with `mx ≥ x` fully inside the range the descent
never fails (the max is somewhere below), so the total is `O(log n)`. The same shape solves "first
zero", "first position where prefix max ≥ x", and "largest `r` such that `sum(l..r) ≤ s`".

### 3.6 Merge sort tree

Node stores the **sorted vector** of its segment; built by `std::merge` bottom-up in
`O(n log n)`, memory `O(n log n)` (`2·10^5 · 18 · 4 B ≈ 14 MB` with `int`). Query "count of
`a[i] < x` in `[l, r)`" = sum of `lower_bound` over the `O(log n)` canonical nodes: `O(log² n)`.
No updates. Also answers "k-th smallest in `[l, r]`" by binary searching on `x`: `O(log³ n)` —
the persistent tree (§4.4) does it in `O(log n)`. The offline alternative for static arrays is a
sweep with a BIT (ch. 08 §7), which is faster and lighter.

```cpp
struct MergeSortTree {
    int n; vector<vector<ll>> t;
    explicit MergeSortTree(const vector<ll>& a) : n(a.size()), t(2 * n) {
        for (int i = 0; i < n; i++) t[n + i] = {a[i]};
        for (int i = n - 1; i >= 1; i--) {
            t[i].resize(t[2*i].size() + t[2*i+1].size());
            merge(t[2*i].begin(), t[2*i].end(), t[2*i+1].begin(), t[2*i+1].end(), t[i].begin());
        }
    }
    int count_less(int l, int r, ll x) const {
        int res = 0;
        for (l += n, r += n; l < r; l >>= 1, r >>= 1) {
            if (l & 1) res += lower_bound(t[l].begin(), t[l].end(), x) - t[l].begin(), l++;
            if (r & 1) --r, res += lower_bound(t[r].begin(), t[r].end(), x) - t[r].begin();
        }
        return res;
    }
};
```

**Fractional cascading** (pointers from each node's vector into the children's) reduces query to
`O(log n)`; rarely worth implementing in a contest.

---

## 4. Beyond the array: persistence, dynamic trees, and friends

### 4.1 Segment tree over the value domain

Index the tree by **value** rather than by position: leaf `x` counts how many elements equal `x`.
Then `prefix count(x)` = "how many elements ≤ x", k-th smallest = walk. This is the "order
statistics" view of a Fenwick/segment tree, and the base of §4.4.

### 4.2 Node pooling

For persistent and dynamic trees, nodes are allocated from a `vector<Node>` and referenced by
integer index (`0` = null). Reasons: cache-friendly, no `new`/`delete`, trivially copyable
(`t.push_back(t[prev])` clones a node). Pitfall: `t[cur].l = insert(...)` — evaluate the call
into a local first; `insert` may `push_back` and reallocate, invalidating the `t[cur]` reference
(C++17 sequences the right operand before the left for `=`, but the reference is still taken to
a possibly reallocated buffer — write it in two statements).

### 4.3 Persistent segment tree — the idea

A **version** is a root pointer. An update creates a new root and copies only the `O(log n)`
nodes along the path; all other nodes are shared with the previous version. Memory
`O((n + q) log n)` — `2·10^5 · 18 · 12 B ≈ 43 MB` for `(l, r, cnt)` `int` nodes; tighten if the
limit is 64 MB. Every past version stays queryable in `O(log n)`.

```
version 0:      version 1 (after inserting into leaf 2):
     r0              r1
    /  \            /  \
   A    B          A'   B          A' is a copy of A with one child replaced; B is shared
  / \             / \
 L0 L1           L0  L1'
```

### 4.4 K-th smallest in a subarray

Compress values to `[0, m)`. Version `i` = tree over values containing `a[0..i−1]`. The
elements of `a[l..r]` are exactly `version(r+1) − version(l)` — and because the two trees have
identical shape, we can walk them **in parallel**, using `cnt(B.left) − cnt(A.left)` as "how many
elements of `a[l..r]` are in the left half":

```cpp
struct PersistentSeg {
    struct Node { int l, r, cnt; };
    vector<Node> t; int m;
    explicit PersistentSeg(int m) : m(m) { t.push_back({0, 0, 0}); }       // node 0: empty
    int insert(int prev, int lo, int hi, int pos) {
        int cur = t.size(); t.push_back(t[prev]); t[cur].cnt++;
        if (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            if (pos < mid) { int c = insert(t[cur].l, lo, mid, pos); t[cur].l = c; }
            else           { int c = insert(t[cur].r, mid, hi, pos); t[cur].r = c; }
        }
        return cur;
    }
    int kth(int ra, int rb, int k) const {                                  // 1-indexed k
        int lo = 0, hi = m;
        while (hi - lo > 1) {
            int mid = (lo + hi) / 2, left = t[t[rb].l].cnt - t[t[ra].l].cnt;
            if (k <= left) { ra = t[ra].l; rb = t[rb].l; hi = mid; }
            else { k -= left; ra = t[ra].r; rb = t[rb].r; lo = mid; }
        }
        return lo;                                                          // compressed value
    }
};
// build: root[0] = 0; root[i+1] = insert(root[i], 0, m, pos(a[i]))
// query k-th smallest of a[l..r]: vals[kth(root[l], root[r+1], k)]
```

Node `0` points to itself with `cnt = 0`, so "walking into a null child" is harmless. The same
structure gives "count of values in `[x, y]` inside `a[l..r]`" (two-tree range count), and CSES
Range Queries and Copies (1737) is persistence with **point updates** (each update creates a new
version; a "copy" is just a new root equal to an existing one).

### 4.5 Persistence vs. offline

If all queries are known in advance, the same problems are usually solvable offline with a
Fenwick: sort by `r`, insert elements, answer at the right moment (ch. 08 §7). Persistence is
the tool when queries are **online** (e.g. depend on previous answers, or forced-online by
xor-encoding), or when versions branch (CSES 1737).

### 4.6 Dynamic (sparse) segment tree

When coordinates go up to `10^9` (or `10^18`) and compression is impossible (online), create nodes
lazily: each update allocates at most one node per level, `O(log N)` nodes; `2·10^5` updates ×
30 levels × 16 B ≈ 100 MB — check the memory limit; shrink with `int` sums or by storing
children in two `vector<int>` and values in one `vector<ll>`.

```cpp
struct DynSeg {
    struct Node { int l = 0, r = 0; ll sum = 0; };
    vector<Node> t; ll N;
    explicit DynSeg(ll N) : N(N) { t.emplace_back(); t.emplace_back(); }   // 0 null, 1 root
    void add(int v, ll lo, ll hi, ll pos, ll val) {
        t[v].sum += val;
        if (hi - lo == 1) return;
        ll mid = lo + (hi - lo) / 2;
        if (pos < mid) { if (!t[v].l) { int c = t.size(); t.emplace_back(); t[v].l = c; } add(t[v].l, lo, mid, pos, val); }
        else           { if (!t[v].r) { int c = t.size(); t.emplace_back(); t[v].r = c; } add(t[v].r, mid, hi, pos, val); }
    }
    ll sum(int v, ll lo, ll hi, ll l, ll r) const {                        // [l, r)
        if (!v || r <= lo || hi <= l) return 0;
        if (l <= lo && hi <= r) return t[v].sum;
        ll mid = lo + (hi - lo) / 2;
        return sum(t[v].l, lo, mid, l, r) + sum(t[v].r, mid, hi, l, r);
    }
};
```

Alternatives: coordinate compression when offline; `std::map`/`pbds` `tree` for pure order
statistics; a treap (ch. on balanced BSTs) when you also need split/merge.

### 4.7 Segment tree beats (mention)

For "`a[i] = min(a[i], x)` on a range" together with range sum, store per node `(max, second
max, count of max)`; an update is applied lazily when `second max < x ≤ max` (only the maxima
change), and recursed otherwise. Amortised `O(log² n)` per op (Ji Ruyi, 2016). Also handles range
`max=`, range add, and "historic max". Learn it only after everything else in this chapter is
automatic.

---

## 5. Sqrt decomposition and Mo's algorithm

### 5.1 Blocks with lazy tags

Split the array into blocks of size `B ≈ √n`. Per block store an aggregate (`sum`) and a lazy
tag (`add`). A range operation touches `O(n/B)` whole blocks (`O(1)` each) and `O(B)` boundary
elements: `O(√n)` per operation with `B = √n`.

```cpp
struct SqrtDecomp {
    int n, B; vector<ll> a, bsum, blazy;
    explicit SqrtDecomp(const vector<ll>& v) : n(v.size()), B(max(1, (int)sqrt(n))), a(v) {
        int nb = (n + B - 1) / B; bsum.assign(nb, 0); blazy.assign(nb, 0);
        for (int i = 0; i < n; i++) bsum[i / B] += a[i];
    }
    void range_add(int l, int r, ll d) {
        for (int i = l; i <= r;) {
            if (i % B == 0 && i + B - 1 <= r) { blazy[i / B] += d; bsum[i / B] += d * B; i += B; }
            else { a[i] += d; bsum[i / B] += d; i++; }
        }
    }
    ll sum(int l, int r) const {
        ll s = 0;
        for (int i = l; i <= r;) {
            if (i % B == 0 && i + B - 1 <= r) { s += bsum[i / B]; i += B; }
            else { s += a[i] + blazy[i / B]; i++; }
        }
        return s;
    }
};
```

Why bother when a segment tree is `O(log n)`: (1) blocks support operations that don't compose
(e.g. "sort each block, binary search for count `< x`", "mode inside a block"), (2) you can
rebuild a block in `O(B)` or `O(B log B)` after any weird update, (3) `O((n + q)√n) ≈ 10^8` for
`n = q = 2·10^5` fits in 1–2 s with tight loops. The full pattern catalogue is in ch. 08 §8.

### 5.2 Mo's algorithm — offline queries with add/remove

Requirements: queries `[l, r]` are known in advance; you can maintain the answer for a window
under `add(i)` / `remove(i)` in `O(1)` (or `O(log n)`), but **cannot** combine two disjoint
windows (otherwise use a segment tree). Examples: number of distinct values (CSES 1734, also
solvable offline with a BIT), number of pairs with equal value, the mode, "number of `x` such
that `x` occurs exactly `x` times".

Sort queries by `(⌊l / B⌋, r)`, then move the two pointers of the current window from query to
query.

**Cost proof** with `B` blocks of size `B`: within one block of `l`, `r` moves monotonically ⇒
`O(n)` per block ⇒ `O(n · n/B)` total for `r`; `l` moves `O(B)` per query ⇒ `O(qB)`. Total
`O(n²/B + qB)`, minimised at `B = n/√q` giving `O(n√q)`. With `n = q`: `O(n√n) ≈ 9·10^7` for
`n = 2·10^5`. The **odd–even trick** — sort `r` descending in odd blocks — removes the `r` pointer's
reset between consecutive blocks and roughly halves the runtime. **Hilbert-curve order** (sort by
the Hilbert index of `(l, r)` on a `2^k × 2^k` grid) gives the same `O(n√q)` bound with a smaller
constant and no block-size tuning; it is a ~15-line function worth pasting from a library when
Mo's is the bottleneck.

```cpp
vector<int> mo_distinct(const vector<int>& a, const vector<pair<int,int>>& qs) {
    int n = a.size(), q = qs.size(), B = max(1, (int)(n / sqrt((double)q + 1)));
    vector<int> ord(q); iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int x, int y) {
        int bx = qs[x].first / B, by = qs[y].first / B;
        if (bx != by) return bx < by;
        return (bx & 1) ? qs[x].second > qs[y].second : qs[x].second < qs[y].second;
    });
    vector<int> cnt(*max_element(a.begin(), a.end()) + 1), ans(q);
    int L = 0, R = -1, distinct = 0;
    auto add = [&](int i) { if (cnt[a[i]]++ == 0) distinct++; };
    auto rem = [&](int i) { if (--cnt[a[i]] == 0) distinct--; };
    for (int id : ord) {
        auto [l, r] = qs[id];
        while (L > l) add(--L);  while (R < r) add(++R);
        while (L < l) rem(L++);  while (R > r) rem(R--);
        ans[id] = distinct;
    }
    return ans;
}
```

Pointer order matters: **expand first, then shrink** — otherwise the window can become invalid
(`L > R + 1`) and `remove` may be called on an element never added. Mo's with point updates
(add a time dimension, blocks of size `n^{2/3}`, `O(n^{5/3})`) and Mo's on trees (Euler tour,
windows of tour positions) are the two standard extensions; ch. 08 sketches the former.

---

## 6. Which structure to pick

| Need | Static | Point update | Range update |
|---|---|---|---|
| sum / xor (group op) | prefix sums `O(1)` | Fenwick | two Fenwicks / lazy segtree |
| min / max / gcd (idempotent) | sparse table `O(1)` | segment tree | lazy segtree (add+min, assign+min) |
| non-commutative / structured node | segment tree (or disjoint sparse table) | segment tree | lazy segtree |
| "first index with …" | prefix + binary search | tree walk | tree walk with push |
| count `< x` in range | merge sort tree / offline BIT / wavelet | BIT of segtrees (heavy) | — |
| k-th smallest in range | persistent segtree / offline | — | — |
| distinct / mode / no combine | Mo's | Mo's with updates | — |
| coordinates `10^9`, online | dynamic segtree | dynamic segtree | dynamic lazy segtree |
| `chmin`/`chmax` + sum | — | — | segment tree beats |

Rule of thumb from the Finnish IOI track: *does the array change?* (no ⇒ prefix / sparse table);
*does the op have an inverse?* (yes ⇒ Fenwick, shortest code); otherwise segment tree.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| "sum of `a[l..r]`", static | prefix sums | `O(n + q)` |
| "min of `a[l..r]`", static, `q` huge | sparse table | `O(n log n + q)` |
| "update `a[k]`", sum/xor | Fenwick | `O((n+q) log n)` |
| "update `a[k]`", min/max/gcd/custom | segment tree | `O((n+q) log n)` |
| "add to range", "value at `k`" | Fenwick on differences | `O((n+q) log n)` |
| "add to range", "sum of range" | two Fenwicks / lazy segtree | `O((n+q) log n)` |
| "set range", "add to range", sum | lazy segtree with composed tags | `O((n+q) log n)` |
| "k-th remaining", "first hotel with ≥ x rooms" | BIT lifting / tree walk | `O(log n)` per query |
| "max subarray sum in `[l..r]`" with updates | segment tree with 4-field node | `O((n+q) log n)` |
| "count `< x` in `[l..r]`", static | merge sort tree / offline BIT | `O(q log² n)` / `O((n+q) log n)` |
| "k-th smallest in `[l..r]`", online | persistent segtree | `O((n+q) log n)` |
| "copies of the array", "version `k`" | persistent segtree | `O((n+q) log n)` |
| indices to `10^9`, online | dynamic segtree | `O(q log C)` |
| indices to `10^9`, offline | coordinate compression + anything | — |
| "distinct values in `[l..r]`", offline | Mo's / sweep + BIT | `O(n√q)` / `O((n+q) log n)` |
| 2D rectangle sums, static | 2D prefix | `O(nm + q)` |
| 2D point update, rectangle sum | 2D Fenwick | `O(q log n log m)` |
| polynomial / arithmetic-progression range add | lazy `(a, d)` tag | `O((n+q) log n)` |

## Implementation checklist for contests

- [ ] `long long` for every sum; check `v · len` products in lazy tags (`10^9 · 2·10^5 = 2·10^14`).
- [ ] Fenwick: internal index is `i + 1`; never enter the loop with `0`; array size `n + 1`
      (`n + 2` if you call `add(r + 1)` for range updates).
- [ ] Segment tree recursive: array size `4n`; iterative: `2n`; identity element is the *true*
      identity (`LLONG_MAX` for min, `0` for sum/gcd, `{0,0,0,0}` for best-subarray with empty).
- [ ] Half-open vs. inclusive: pick one per structure and write it in the comment above the
      struct. Iterative query here is `[l, r)`; recursive is `[l, r]`.
- [ ] Lazy: `push` **before** recursing on children; recompute the parent **after**; when
      composing tags, assign kills add.
- [ ] Tree walk assumes the predicate is monotone and the total satisfies it — check
      `k ≤ sum[1]` (or return `n`) before descending.
- [ ] Sparse table: query needs `l ≤ r`; memory `n log n` — use `int` if possible.
- [ ] Persistent/dynamic: never hold a reference/pointer into the node vector across a
      `push_back`; reserve if you know the count (`n · 20`).
- [ ] Mo's: expand before shrink; `cnt` array sized to `max value + 1` (compress values first).
- [ ] Fast IO: `ios::sync_with_stdio(false); cin.tie(nullptr);` and `'\n'`, never `endl`, in a
      loop of `2·10^5` outputs.
- [ ] Recursion depth is only `log n` here — fine. (Deep recursion issues appear in ch. 10 trees.)
- [ ] Stress-test against an `O(n)` brute force on random small arrays **before** submitting
      anything with a lazy tag or a persistent node.

## Further reading

- CPH (Laaksonen, *Competitive Programmer's Handbook*): ch. 9 "Range queries" (static array
  queries, binary indexed tree, segment tree), ch. 27 "Square root algorithms" (Mo's), ch. 28
  "Segment trees revisited" (lazy propagation, dynamic and persistent segment trees, 2D trees).
- cp-algorithms.com: "Sparse Table", "Fenwick Tree", "Segment Tree" (incl. "finding the k-th
  zero", "searching for the first element greater than", "range updates (lazy propagation)",
  "persistent segment tree", "dynamic segment tree"), "Sqrt Decomposition" (incl. Mo's algorithm),
  "Disjoint Sparse Table" (as a blog reference).
- Codeforces blog "Efficient and easy segment trees" (Al.Cash) — the iterative tree, including the
  non-commutative and lazy variants.
- Segment tree beats: Codeforces blog "A simple introduction to 'Segment tree beats'" (jiry_2).
- Fenwick, original: P. Fenwick, "A New Data Structure for Cumulative Frequency Tables" (1994).

## You can move on when...

- You can write Fenwick (with `kth`), iterative segment tree, and lazy segment tree
  (assign + add + sum) from memory in under 12 minutes total, and each passes a random
  brute-force stress test on the first compile.
- You can explain, without notes, why `i += i & -i` visits exactly the Fenwick cells that contain
  `i`, and why lazy tags must be pushed before descending.
- You have solved every CSES Range Queries task in `problems.md` (including 1735, 1736, 1737) and
  at least three of the Codeforces / olympiad problems there.
- Given a new statement, you can name the structure and its complexity in under a minute using
  the decision table in §6.
