# Chapter 09 — Balanced Trees and Advanced Structures

## What you'll be able to do after this chapter

- Recognise when a problem needs an *ordered dynamic set with order statistics* ("k-th smallest right now", "how many are smaller", "delete the k-th") and answer it in `O(log n)` with a treap, pbds `tree`, or an offline Fenwick trick — and know which of the three to reach for.
- Write an **implicit treap** from memory in under 5 minutes: insert at position, erase a range, reverse a range with lazy flip, range sum/min, and cut-and-paste — the "rope" that solves every "operate on a dynamic sequence" task.
- Use `std::set` / `std::multiset` as a precision tool: `lower_bound`, erasing while iterating, the *set of disjoint intervals* pattern, and the classic multiset traps (`erase(value)` kills all copies, `count` is linear in multiplicity, no `operator[]`).
- Replace "delete arbitrary element from a heap" with **lazy deletion** and solve running/sliding median with two heaps or two multisets.
- Maintain the upper envelope of lines with a **Li Chao tree** or a **monotone convex hull trick** deque and recognise the DP transitions `dp[i] = min_j (a_j * x_i + b_j)` that call for them.
- Explain what a sparse segment tree, a wavelet tree, a splay tree, a link-cut tree and a skip list do, and when (rarely) they are the right tool at IOI level.

Prerequisites: `../../algorithms_learning/05_trees_and_bsts/lesson.md` (BST invariant, rotations idea), `../../algorithms_learning/04_stack_and_monotonic_stack/lesson.md` (monotone deque), and the segment tree / Fenwick chapter of this course (range queries). Not re-taught here.

## Where this shows up in contests

| Signal in the statement | Structure | Typical placement |
|---|---|---|
| "k-th smallest among current elements", "number of elements less than x", insertions and deletions interleaved with queries | order-statistics tree (treap / pbds / offline BIT) | CSES Range Queries, CF Div2 D-E, BOI/IOI subtasks |
| "remove the k-th element of the remaining list", Josephus-style | order statistics | CSES Sorting & Searching |
| "cut a substring and paste it elsewhere", "reverse a range", "insert a block at position p", any *positional* operation on a sequence with `n, q ≤ 2e5` | implicit treap (rope) | CSES Advanced Techniques, CF Div1 C-D, IOI 2011 "Elephants"-type ideas |
| "paint / assign a value to a range, query sum of values; intervals stay few", random data | set of intervals (Chtholly / ODT) | CF Div1 C-E |
| "sliding window median / cost", "median of the stream" | two heaps or two multisets | CSES Sliding Window, CF Div2 C-D |
| "delete arbitrary element from a priority queue" | lazy deletion | everywhere (Dijkstra variants, schedulers) |
| DP `dp[i] = min_j (dp[j] + f(i) * g(j))`, "cost of a segment is linear in its length times something" | CHT / Li Chao | CF Div1 C-D, IOI subtasks 4-5 (e.g. IOI 2016 "Aliens" uses Aliens trick + CHT) |
| coordinates up to `1e9`, point updates, range queries, online | sparse segment tree (or coordinate compression if offline) | CSES Salary Queries, CF Div2 E |
| "k-th smallest in a range", "count values in `[a,b]` in a range", static array | wavelet tree / merge sort tree / offline BIT | CSES Range Interval Queries, CF Div1 D |
| link/cut edges of a forest, path aggregates online | link-cut tree | CF Div1 E+; practically never at IOI |

The *shape* to recognise: a static array problem becomes a dynamic one (insertions, deletions, or re-orderings between queries). Segment trees handle *values changing at fixed positions*; balanced trees handle *positions themselves changing*.

---

## 1. Why balanced BSTs in contests

A sorted `vector` gives `O(log n)` search but `O(n)` insert. A `std::set` gives `O(log n)` insert/erase/search but **no k-th element and no rank** (iterators are bidirectional, `std::distance` is `O(n)`). A segment tree over values gives k-th and rank in `O(log C)` — *if the value universe is known in advance* (offline). The gaps a balanced BST fills:

1. **Order statistics online**: `kth(k)`, `rank(x)` with arbitrary insert/erase, values unknown in advance, in `O(log n)`.
2. **Dynamic sequences**: the key is the *position*, which shifts when you insert/erase — no array structure can do this in `O(log n)` per operation.
3. **Split / merge**: cut an ordered set into two by key or by position and glue two back together. Segment trees cannot merge two arbitrary trees in `O(log n)`.

Every balanced BST (AVL, red-black, splay, treap, AA, scapegoat, B-tree...) does the same operations. In contests the choice is dictated by *implementation cost under time pressure*: the treap wins because split/merge are 10 lines each and everything else is built from them. Red-black trees are what `std::set` uses; you never write one yourself.

Cost model for `n = q = 2e5`: a treap does ~`2 ln n ≈ 24` expected node visits per split/merge, each with a cache miss (pointer chasing). Expect ~`1e7` node visits total → 0.1-0.3 s. Fine for 1 s limits; a node-pool `vector` (as in `example.cpp`) is 2-3× faster than `new`.

---

## 2. Treap with explicit keys

### 2.1 Idea and invariants

Each node has a **key** (BST order) and a random **priority** (max-heap order: parent's priority ≥ children's). Claim: given the keys and priorities, the tree is unique — it is the Cartesian tree of the sequence (sorted by key) with priorities as heights. Since priorities are random, the tree's shape is that of a BST built by inserting the keys in random order, which has expected depth `O(log n)`.

**Proof sketch (expected depth).** Sort the keys; consider nodes `u` and `v` with `d = |rank(u) − rank(v)|`. `v` is an ancestor of `u` iff `v` has the maximum priority among the `d+1` keys between them inclusive; by symmetry that has probability `1/(d+1)`. So `E[depth(u)] = Σ_{v≠u} 1/(d+1) ≤ 2 H_n ≈ 2 ln n`. Every operation below walks one or two root-to-node paths, hence expected `O(log n)`. With `mt19937` priorities there is no adversarial input.

### 2.2 The two primitives

```
split(t, k)  →  (L, R)     L = all nodes with key < k,  R = all nodes with key ≥ k
merge(L, R)  →  t          requires max key of L  ≤  min key of R
```

Both recurse down a single path:

- `split`: at node `v`, if `key(v) < k` then `v` and its whole left subtree belong to `L`; recurse into `v.right` to split off the part of it that is `≥ k`, and hang the `< k` part back as `v.right`. Symmetric otherwise.
- `merge`: the root of the result is whichever root has the larger priority (heap invariant); recurse on the facing side.

```cpp
struct Treap {
    struct Node { ll key; unsigned pri; int l, r, sz; };
    vector<Node> t; mt19937 rng{12345}; int root = 0;      // t[0] is the null node
    Treap() { t.push_back({0, 0, 0, 0, 0}); }
    int newNode(ll k) { t.push_back({k, (unsigned)rng(), 0, 0, 1}); return (int)t.size() - 1; }
    int sz(int v) const { return t[v].sz; }
    void upd(int v) { if (v) t[v].sz = 1 + sz(t[v].l) + sz(t[v].r); }
    void split(int v, ll k, int& l, int& r) {
        if (!v) { l = r = 0; return; }
        if (t[v].key < k) { split(t[v].r, k, t[v].r, r); l = v; }
        else              { split(t[v].l, k, l, t[v].l); r = v; }
        upd(v);
    }
    int merge(int a, int b) {
        if (!a || !b) return a ? a : b;
        if (t[a].pri > t[b].pri) { t[a].r = merge(t[a].r, b); upd(a); return a; }
        else                     { t[b].l = merge(a, t[b].l); upd(b); return b; }
    }
    void insert(ll k) { int l, r; split(root, k, l, r); root = merge(merge(l, newNode(k)), r); }
    bool erase(ll k) {                                   // one copy
        int l, m, r; split(root, k, l, m); split(m, k + 1, m, r);
        if (!m) { root = merge(l, r); return false; }
        m = merge(t[m].l, t[m].r);                       // drop m's root = one copy of k
        root = merge(merge(l, m), r); return true;
    }
    ll kth(int k) const {                                // 0-indexed
        int v = root;
        for (;;) { int ls = sz(t[v].l);
            if (k < ls) v = t[v].l; else if (k == ls) return t[v].key; else { k -= ls + 1; v = t[v].r; } }
    }
    int countLess(ll k) const {                          // = rank of k
        int v = root, res = 0;
        while (v) { if (t[v].key < k) { res += sz(t[v].l) + 1; v = t[v].r; } else v = t[v].l; }
        return res;
    }
};
```

The `sz` field is the whole point: `kth` and `countLess` are the same descent as BST search, using subtree sizes to decide the direction.

### 2.3 Worked trace

Insert keys 5, 2, 8, 6 with priorities 90, 40, 70, 95 (drawn at random). After all inserts the treap is *forced* to be:

```
              6(95)
             /     \
          5(90)    8(70)
          /
       2(40)
```

`kth(2)` (third smallest): at 6, `sz(left)=2`, `k=2 == 2` → answer 6. `countLess(7)`: at 6, `6<7` → `res = 2+1 = 3`, go right to 8, `8 ≥ 7` → go left → null → answer 3 (keys 2, 5, 6).

`erase(5)`: `split(root, 5)` → `L = {2}`, `M∪R = {5,6,8}`; `split(·, 6)` → `M = {5}`, `R = {6,8}`; `M` becomes `merge(null, null) = null`; `root = merge(merge({2}, null), {6,8})`.

### 2.4 Variants

- **Duplicates**: the code above is a multiset. For a set, check `countLess(k+1) - countLess(k) == 0` before inserting.
- **Augmentation**: any associative aggregate (sum, min, max, xor, hash) lives next to `sz` and is recomputed in `upd`. This is the same discipline as a segment tree node.
- **Union of two treaps with interleaved keys** (`unite(a, b)`): split `b` by `key(a)`, recurse both sides — `O(m log(n/m))`, the basis of "merge small into large" on trees.
- **Persistent treap**: copy the node on every write in split/merge (`O(log n)` new nodes per operation); priorities must then be decided by a coin weighted by subtree sizes in `merge` (`rand() % (sz(a)+sz(b)) < sz(a)`) to keep the expectation argument valid when subtrees are shared.

### 2.5 Pitfalls

- `split` for "keys `≤ k`" is `split(v, k+1, ...)` — do not write two versions.
- Recursion depth is the tree depth, `O(log n)` expected — safe. But **never** use recursion for `toVector`-style traversals on `n = 1e6` without thinking; the depth is still only `O(log n)`, so it is fine — the danger is only for degenerate structures like a linked-list BST.
- Node pool: references into `t` (`int& l`) are invalidated by `push_back` inside the same expression. Allocate nodes *before* taking references (`insert` above calls `newNode` after `split` returns — `merge` does not allocate). The same bug is shown and fixed in `SparseSeg::child` in `example.cpp`.
- Use `unsigned` priorities from `mt19937`; `rand()` on some judges returns 15-bit values → many ties → depth degrades.

---

## 3. Implicit treap — the rope

### 3.1 Idea

Drop the key. A node's *position* in the sequence is implicit: `pos(v) = sz(v.left) + (positions contributed by ancestors where v is in the right subtree)`. In-order traversal *is* the sequence. `split(t, k)` now means "first `k` elements to the left, rest to the right" and descends by comparing `k` with `sz(left)`:

```
split(v, k):
    if sz(v.left) >= k:  the split point is inside the left subtree
        split(v.left, k, L, v.left);  R = v
    else:                              v and its left subtree go left
        split(v.right, k - sz(v.left) - 1, v.right, R);  L = v
```

`merge` is unchanged (there are no keys to compare — merge concatenates). Every sequence operation is now 3 splits + 3 merges:

| Operation | Recipe |
|---|---|
| `insert(pos, x)` | `split(root, pos) → (A, B)`; `root = merge(merge(A, new(x)), B)` |
| `erase [l, r)` | `split → A, [B, C]`; `split(B∪C, r−l) → B, C`; `root = merge(A, C)` |
| `reverse [l, r)` | isolate `B`; `B.rev ^= 1`; merge back |
| `query [l, r)` | isolate `B`; read `B.sum / B.mn`; merge back |
| cut `[l, r)` and paste at `pos` | isolate `B`; `rest = merge(A, C)`; `split(rest, pos) → X, Y`; `root = merge(merge(X, B), Y)` |

### 3.2 Lazy reversal

`rev` on node `v` means "the in-order of `v`'s subtree must be mirrored, but I have not yet done it". Applying it = swap `v.l` and `v.r`, toggle `rev` on both children. This is the same lazy discipline as a segment tree: **push before descending into children** (in `split`, `merge`, `get`, `toVector`), and note that `sz`, `sum`, `min` are symmetric so aggregates stay valid without pushing — only *child order* is stale.

```cpp
struct ImplicitTreap {
    struct Node { ll val, sum, mn; unsigned pri; int l, r, sz; bool rev; };
    vector<Node> t; mt19937 rng{777}; int root = 0;
    ImplicitTreap() { t.push_back({0, 0, LLONG_MAX, 0, 0, 0, 0, false}); }
    int newNode(ll v) { t.push_back({v, v, v, (unsigned)rng(), 0, 0, 1, false}); return (int)t.size() - 1; }
    int sz(int v) const { return t[v].sz; }
    void push(int v) {
        if (v && t[v].rev) { swap(t[v].l, t[v].r);
            if (t[v].l) t[t[v].l].rev ^= 1; if (t[v].r) t[t[v].r].rev ^= 1; t[v].rev = false; }
    }
    void upd(int v) {
        if (!v) return; const Node& L = t[t[v].l]; const Node& R = t[t[v].r];
        t[v].sz = 1 + L.sz + R.sz; t[v].sum = t[v].val + L.sum + R.sum; t[v].mn = min({t[v].val, L.mn, R.mn});
    }
    void split(int v, int k, int& l, int& r) {          // first k elements -> l
        if (!v) { l = r = 0; return; }
        push(v);
        if (sz(t[v].l) < k) { split(t[v].r, k - sz(t[v].l) - 1, t[v].r, r); l = v; }
        else                { split(t[v].l, k, l, t[v].l); r = v; }
        upd(v);
    }
    int merge(int a, int b) {
        if (!a || !b) return a ? a : b;
        push(a); push(b);
        if (t[a].pri > t[b].pri) { t[a].r = merge(t[a].r, b); upd(a); return a; }
        else                     { t[b].l = merge(a, t[b].l); upd(b); return b; }
    }
    void insert(int pos, ll v) { int l, r; split(root, pos, l, r); root = merge(merge(l, newNode(v)), r); }
    void erase(int l, int r) { int a, b, c; split(root, l, a, b); split(b, r - l, b, c); root = merge(a, c); }
    void reverse(int l, int r) { int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
                                 if (b) t[b].rev ^= 1; root = merge(merge(a, b), c); }
    pair<ll, ll> query(int l, int r) { int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
                                       pair<ll, ll> res = {t[b].sum, t[b].mn}; root = merge(merge(a, b), c); return res; }
    void cutPaste(int l, int r, int pos) {              // move [l,r) to start at index pos of the remainder
        int a, b, c; split(root, l, a, b); split(b, r - l, b, c);
        int rest = merge(a, c), x, y; split(rest, pos, x, y); root = merge(merge(x, b), y); }
};
```

Building from an array: `n` inserts at the end is `O(n log n)` and fine. `O(n)` build: generate priorities, build a Cartesian tree with a stack (or just `merge` in order — still `O(n log n)` expected but tiny constant).

### 3.3 Worked trace: reverse

Sequence `[a b c d e]`, `reverse(1, 4)` (so `b c d` → `d c b`). Suppose the isolated middle treap is

```
      c            after rev^=1 on the root and one push:      c
     / \                                                      / \
    b   d           swap children, mark both children        d   b
                    (leaves: marks are no-ops)
```

In-order is now `d c b`. Merged back: `[a d c b e]`. The marks on leaves get consumed lazily the next time anything descends into them.

### 3.4 Range-assign, range-add, and "position of value x"

- **Range add / assign**: another lazy tag, pushed the same way; `sum += add * sz`, `mn += add`. Composition rule: assign overrides pending add.
- **Where is value `x` now?** Store, per value, its node index; walk *up* parent pointers summing `sz(left)+1` whenever you come from a right child. This needs a `par` field maintained in `upd` (`t[t[v].l].par = t[t[v].r].par = v`) and pushing of `rev` tags along the root path *first* (collect the path, push top-down, then compute). This is how "Reversal Sorting" (CSES 2075) finds the minimum's current index: keep it as `mn` with its node id.

### 3.5 Pitfalls

- Forgetting `push` in `merge` — the most common bug: `merge` reads `t[a].r` after a lazy `rev` swapped it.
- `reverse` on an empty range: guard `if (b)` before toggling `t[0].rev`.
- `LLONG_MAX` as identity for `mn` in the null node; `0` for `sum`; `0` for `sz`. Set them once in the constructor, never touch node 0 otherwise (`upd(0)` must return immediately).
- Complexity is expected, not worst case — an anti-random-seed test cannot exist because priorities are internal, but do seed `mt19937` from `chrono` if you fear a fixed-seed hack on Codeforces (irrelevant for IOI-style judges).

---

## 4. pbds `tree`: order statistics for free (GCC only)

```cpp
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
using namespace __gnu_pbds;
template <class T>
using ordered_set = tree<T, null_type, less<T>, rb_tree_tag, tree_order_statistics_node_update>;

ordered_set<int> s;
s.insert(5); s.insert(2); s.insert(8);
*s.find_by_order(1);      // 5     (0-indexed k-th)
s.order_of_key(6);        // 2     (# elements < 6)
s.erase(5);
```

A red-black tree with subtree sizes: `O(log n)` for everything, about 2× slower than a hand-rolled treap in practice but zero implementation risk.

**Multiset**: `less<T>` refuses duplicates. Options: (a) `ordered_set<pair<T,int>>` with a unique counter as the second component — then `order_of_key({x, -1})` counts `< x` and `order_of_key({x, INT_MAX})` counts `≤ x`; (b) `less_equal<T>` — works for insert/order_of_key/find_by_order, **but `find(x)` and `erase(x)` break** (they use the comparator to test equality); erase via `s.erase(s.find_by_order(s.order_of_key(x)))` where `order_of_key` with `less_equal` returns the count `≤ x`... which is why (a) is what you should use.

**Caveats**: libstdc++ only. Apple clang (libc++), MSVC, and some judges' clang setups do not have `<ext/pb_ds>`. `example.cpp` guards it with `#if __has_include(<ext/pb_ds/assoc_container.hpp>)`; you can also test `#ifdef __GLIBCXX__`. IOI, CSES, Codeforces, AtCoder all use GCC → available. The same header also gives `gp_hash_table`, a faster `unordered_map` (use a custom hash: splitmix64 of `x ^ FIXED_RANDOM`).

---

## 5. `std::set` as a precision tool

### 5.1 Iterator arithmetic that is actually O(log n)

| Need | Code |
|---|---|
| smallest element `≥ x` | `auto it = s.lower_bound(x); if (it != s.end()) ...` |
| largest element `< x` | `auto it = s.lower_bound(x); if (it != s.begin()) --it;` |
| largest element `≤ x` | `auto it = s.upper_bound(x); if (it != s.begin()) --it;` |
| min / max | `*s.begin()`, `*s.rbegin()` (or `*prev(s.end())`) |
| neighbours of a just-inserted element | `auto [it, ok] = s.insert(x); prev(it) / next(it)` with boundary checks |
| erase while iterating | `it = s.erase(it);` (C++11 returns the next iterator) |
| erase a range of the set | `s.erase(s.lower_bound(a), s.upper_bound(b));` — `O(k + log n)` |

**Never** call `s.lower_bound(x)` as `std::lower_bound(s.begin(), s.end(), x)` — the free function on bidirectional iterators is `O(n)`.

Erase-while-iterating pattern (the classic wrong version increments a dead iterator):

```cpp
for (auto it = s.begin(); it != s.end(); )
    if (bad(*it)) it = s.erase(it); else ++it;
```

### 5.2 Set of disjoint intervals (Chtholly / "ODT")

Maintain a partition of `[0, n)` into maximal intervals with a value: `map<int, pair<int, ll>> m; // l -> (r, val)`. The one primitive is `split(x)`: make `x` an interval boundary and return the iterator to the interval starting at `x`. Then any range operation on `[l, r)` is

```cpp
auto itr = split(r), itl = split(l);   // r FIRST (keeps itr valid in libraries where split could erase)
for (auto it = itl; it != itr; ++it) { /* operate on [it->first, it->second.first) */ }
```

and *assign* collapses the range into one interval: `m.erase(itl, itr); m[l] = {r, v};`.

```cpp
struct IntervalSet {
    map<int, pair<int, ll>> m;                            // disjoint, covering [0,n)
    IntervalSet(int n, ll v) { m[0] = {n, v}; }
    auto split(int x) {
        auto it = prev(m.upper_bound(x));                  // the interval containing x
        if (it->first == x) return it;
        auto [r, v] = it->second; it->second.first = x;
        return m.emplace(x, make_pair(r, v)).first;
    }
    void assign(int l, int r, ll v) { auto itr = split(r), itl = split(l); m.erase(itl, itr); m[l] = {r, v}; }
    ll sum(int l, int r) { auto itr = split(r), itl = split(l); ll s = 0;
        for (auto it = itl; it != itr; ++it) s += (ll)(it->second.first - it->first) * it->second.second; return s; }
};
```

**Complexity.** Each `assign` creates at most 3 intervals (two from splits, one new) and erases everything in between. Every interval erased was created by some earlier operation, so the total number of erasures is `O(n + q)` and the amortised cost is `O((n + q) log n)` *for assign*. Non-assign range operations (add, sum) cost `O(#intervals in range)`, which is only bounded if assigns are frequent — on **random** operations the expected number of intervals is `O(log n)` per query (the Chtholly argument), on **adversarial** data it degenerates to `O(n)`. Use it when (a) the problem is "assign + something" and assigns dominate, (b) the data is stated to be random, or (c) you need a "set of occupied segments" (CSES Traffic Lights, Room Allocation variants) where every operation is a split or merge of neighbours — then it is worst-case `O(log n)`.

Trace: `[0,10)=0`, `assign(3,7,5)`: `split(7)` → `{0:(3..7?)}`... concretely `m = {0:(10,0)}` → after `split(7)`: `{0:(7,0), 7:(10,0)}` → after `split(3)`: `{0:(3,0), 3:(7,0), 7:(10,0)}` → erase `[3..7)` entry and insert `3:(7,5)`: `{0:(3,0), 3:(7,5), 7:(10,0)}`.

### 5.3 Other set idioms

- **Coordinate "next free slot"**: keep a `set` of free indices; `*s.lower_bound(x)` gives the next free `≥ x`; erase on use. Amortised alternative when only "next free to the right" is needed: DSU with `find(x+1)`.
- **Set of events keyed by time with `pair<time, id>`**: gives stable ordering and `O(log n)` deletion by id if you store the pair.
- **`set<pair<val, idx>>`** as an *indexed* multiset: deletable by `{val, idx}`, no duplicate ambiguity.
- **Merge small into large** on sets: `if (a.size() < b.size()) swap(a, b); for (x : b) a.insert(x);` — each element moves `O(log n)` times → `O(n log² n)` total. This solves "Distinct Colors" (Chapter 10) and many "merge components with statistics" tasks.

---

## 6. `std::multiset` pitfalls

| Trap | Fix |
|---|---|
| `ms.erase(x)` erases **every** copy of `x` | `ms.erase(ms.find(x))` erases one |
| `ms.count(x)` is `O(log n + multiplicity)` | usually fine; for "is present" use `find`, for multiplicities keep a `map<T,int>` |
| `ms.find(x)` when `x` is absent returns `end()` → `erase(end())` is UB | always check `if (it != ms.end())` |
| iterating and erasing by value inside the loop invalidates the current iterator | `it = ms.erase(it)` |
| "top element" changed after erase of an *equal* value elsewhere → still fine, iterators to other elements stay valid (node-based) | but never cache `ms.begin()` across an erase of that element |
| `lower_bound` on `multiset<pair<>>` with a partial key | use `{key, INT_MIN}` / `{key, INT_MAX}` sentinels |

Multiset-based sliding window (median / cost): keep `lo` (multiset of the smaller half) and `hi`; on insert/erase rebalance so `|lo| = ⌈k/2⌉`. Erase from the correct half by comparing against `*lo.rbegin()`. For *cost* (sum of `|x - median|`) also maintain `sumLo`, `sumHi`: cost = `median·|lo| − sumLo + sumHi − median·|hi|`. This is exactly CSES 1076/1077.

---

## 7. `priority_queue` with lazy deletion

A binary heap cannot delete an arbitrary element, but almost always you only need to delete elements that will *eventually reach the top*. Keep a second heap (or `map<T,int>` of pending deletions); when the top of the main heap equals the top of the deletion heap, pop both.

```cpp
struct LazyMaxPQ {
    priority_queue<ll> q, del;
    void push(ll x) { q.push(x); }
    void erase(ll x) { del.push(x); }                    // x must be present
    void clean() { while (!del.empty() && !q.empty() && q.top() == del.top()) { q.pop(); del.pop(); } }
    ll top() { clean(); return q.top(); }
    void pop() { clean(); q.pop(); }
    bool empty() { clean(); return q.empty(); }
};
```

Each element is pushed/popped at most once from each heap → `O(log n)` amortised. Two heaps on `2e5` operations run in ~30 ms versus ~120 ms for a `multiset`, and `priority_queue` has no allocation per element. The same trick appears in Dijkstra ("skip stale entries": `if (d > dist[v]) continue;`) — lazy deletion by *staleness* rather than by an explicit deletion heap.

Variant for "delete by id": store `(value, id)` in the heap and an `alive[id]` flag; pop while `!alive[top.id]`.

---

## 8. Two heaps for the median

Lower half in a max-heap `lo`, upper half in a min-heap `hi`, `|lo| ∈ {|hi|, |hi|+1}`. Insert into the side determined by comparing with `lo.top()`, then rebalance by moving one element. The lower median is always `lo.top()`; `O(log n)` per insert, `O(1)` per median. With **deletions** (sliding window) you cannot use heaps directly → either lazy deletion in both heaps (track which half each element is in via a hash of counts) or, simpler, the two-multiset version from §6. The heap version is right for the *streaming* median; the multiset version for *sliding windows*.

Why the median minimises `Σ|x_i − m|`: moving `m` by `ε` to the right changes the sum by `ε·(#points left of m − #points right of m)`, which is `≤ 0` only while at least half the points are to the right. Hence any point between the two middle order statistics is optimal — CSES Sliding Window Cost, and the "make all elements equal with ±1 moves" family.

---

## 9. Li Chao tree

**Problem.** Insert lines `y = a·x + b` online; query `max_i (a_i·x + b_i)` at integer `x ∈ [lo, hi]`. Slopes in any order, queries in any order.

**Idea.** A segment tree over the *x-domain*. Each node stores one line — the one that is best at the node's midpoint among all lines that were pushed to it. Insert a new line `nw` at a node holding `cur`: compare at the midpoint; keep the winner at the node; the loser can be better than the winner only on **one** side (two lines cross at most once), determined by comparing at the left endpoint; recurse into that side only. Query walks root-to-leaf taking the max of the stored lines. Both `O(log(hi − lo))`.

```cpp
struct LiChao {
    struct Line { ll a, b; ll operator()(ll x) const { return a * x + b; } };
    int lo, hi; vector<Line> line; vector<char> has;
    LiChao(int lo_, int hi_) : lo(lo_), hi(hi_) {
        int sz = 1; while (sz < hi - lo + 1) sz <<= 1; line.assign(2 * sz, {0, 0}); has.assign(2 * sz, 0); }
    void insert(Line nw) { insert(1, lo, hi, nw); }
    void insert(int v, int l, int r, Line nw) {
        if (!has[v]) { line[v] = nw; has[v] = 1; return; }
        int m = (l + r) >> 1;
        bool leftBetter = nw(l) > line[v](l), midBetter = nw(m) > line[v](m);
        if (midBetter) swap(line[v], nw);                // node keeps the winner at m
        if (l == r) return;
        if (leftBetter != midBetter) insert(2 * v, l, m, nw);   // loser wins somewhere on the left half
        else insert(2 * v + 1, m + 1, r, nw);
    }
    ll query(ll x) const {
        int v = 1, l = lo, r = hi; ll res = LLONG_MIN;
        for (;;) { if (has[v]) res = max(res, line[v](x));
            if (l == r) return res; int m = (l + r) >> 1;
            if (x <= m) { v = 2 * v; r = m; } else { v = 2 * v + 1; l = m + 1; } }
    }
};
```

**Correctness invariant.** For every `x` in a node's range, the best line among those *that passed through this node* is either the node's line or lives in the child whose range contains `x`. When a loser is pushed left because `leftBetter != midBetter`, it is worse than the winner on the whole right half (it loses at `m` and, having crossed once before `m`, stays worse to the right).

**Variants.**
- *Min* instead of max: flip comparisons or insert `(-a, -b)` and negate.
- Real-valued or huge `x`: compress the query coordinates first (offline) — the tree is over *query* positions, and comparisons use the true `x` values `xs[m]`. Or make the tree dynamic (pointers / node pool) over `[-1e9, 1e9]`: 31 levels, one new node per level in the worst case.
- **Segments** (a line valid only on `[l, r]`): decompose `[l, r]` into `O(log)` tree nodes and insert the line at each with the same push-down — `O(log² C)` per insert.
- Li Chao **on a segment tree** ("Kinetic"/"line container per node"): to support *deleting* lines or "max over lines added in a time window", store a Li Chao in each segment-tree node over time — `O(log² )` per query. Rarely needed at IOI.
- Overflow: `a·x + b` with `|a|, |x| ≤ 1e9` overflows `ll` → compress or use `__int128` in comparisons.

Trace: domain `[0, 7]`, insert `L1: y = x`, then `L2: y = −x + 6`. Root (mid 3): `L1(3)=3`, `L2(3)=3` → tie, `midBetter=false`; `L2(0)=6 > L1(0)=0` → `leftBetter=true ≠ midBetter` → push `L2` left. Left child `[0,3]` empty → store `L2`. Query `x=1`: root gives `1`, left child gives `5` → 5. Query `x=6`: root `6`, right child empty → 6. Correct (`max(6, 0)`).

---

## 10. Convex hull trick as a structure

Same problem — set of lines, extreme value at `x` — but with monotonicity assumptions that buy `O(1)` amortised.

### 10.1 Monotone deque version

Assumptions (min version): lines are added in **strictly decreasing slope** order; queries come in **non-decreasing `x`**. Then the lower envelope, read left to right, uses the lines in insertion order, and the optimal line for increasing `x` moves monotonically along the deque.

Adding line `l3` after `l1, l2` (back of deque): `l2` is useless iff `x(l1, l3) ≤ x(l1, l2)` — the intersection of the new line with `l1` is left of where `l2` took over from `l1`. Cross-multiply to avoid division:

```
x(l1,l2) = (b2 − b1) / (a1 − a2)      (a1 > a2 so the denominator is positive)
l2 useless  ⇔  (b3 − b1)·(a1 − a2)  ≤  (b2 − b1)·(a1 − a3)
```

Use `__int128` for the products when `|a|, |b|` reach `1e9`/`1e18`.

```cpp
struct MonotoneCHT {                                     // min; slopes strictly decreasing; x non-decreasing
    vector<pair<ll, ll>> h; size_t ptr = 0;
    static bool bad(pair<ll, ll> l1, pair<ll, ll> l2, pair<ll, ll> l3) {
        return (__int128)(l3.second - l1.second) * (l1.first - l2.first) <=
               (__int128)(l2.second - l1.second) * (l1.first - l3.first); }
    void add(ll a, ll b) {
        while (h.size() >= 2 && bad(h[h.size() - 2], h.back(), {a, b})) h.pop_back();
        h.push_back({a, b}); if (ptr >= h.size()) ptr = h.size() - 1; }
    ll query(ll x) {
        while (ptr + 1 < h.size() && h[ptr + 1].first * x + h[ptr + 1].second <= h[ptr].first * x + h[ptr].second) ++ptr;
        return h[ptr].first * x + h[ptr].second; }
};
```

Each line is pushed and popped at most once; `ptr` only moves forward → `O(n + q)` total.

Where the DP comes from: `dp[i] = min_{j<i} ( dp[j] + (x_i − x_j)·c_i )`-type transitions rewrite as `dp[i] = c_i·x_i + min_j ( −x_j·c_i + dp[j] )`: a line with slope `−x_j` and intercept `dp[j]`, queried at `c_i`. Slopes are monotone if `x_j` is monotone in `j` (typical: prefix sums); queries are monotone if `c_i` is sorted. If only slopes are monotone but queries are not: binary search on the deque for the query (`O(log n)`). If neither: Li Chao, or the `std::set`-based dynamic hull.

### 10.2 Dynamic hull with `std::set` (any slope order, any query order)

Keep the lines sorted by slope in a `set`; each line also stores the `x` from which it becomes optimal (`lo`). Insertion: find the neighbours, remove lines dominated on both sides (same `bad` test), recompute `lo` for the new line and its right neighbour. Query: find the last line with `lo ≤ x` — needs a second ordering, done in one `set` via a comparator that compares by slope for insertions and by `lo` for queries (the "Line Container" from KACTL). It is `O(log n)` per operation but fiddly to write under pressure — **prefer Li Chao** unless the query coordinates are unbounded and cannot be compressed (they almost always can).

### 10.3 Divide & conquer alternatives

If the transition is `dp[i] = min_j (dp[j] + cost(j, i))` with `cost` satisfying the quadrangle inequality, you often do not need CHT at all: monotone optimum → divide & conquer optimisation (`O(n log n)` per layer) or Knuth. CSES "Subarray Squares"/"Houses and Schools"/"Knuth Division" are that family; "Monster Game I/II" are pure CHT.

---

## 11. Sparse segment tree (recap)

A segment tree over `[0, C)` with `C = 1e9` whose nodes are created on demand. Point update touches `O(log C) ≈ 30` nodes → `q = 2e5` updates use `≤ 6e6` nodes × 16 bytes ≈ 100 MB — **too much for a 64 MB limit**; either compress coordinates offline (sort all query values, `O(log q)` depth) or use a Fenwick over compressed values. Use the sparse tree when the problem is truly online (interactive, or values depend on previous answers) or when the leaves' identities are unknown until runtime (e.g. "Salary Queries" can be done both ways). The implementation in `example.cpp` (`SparseSeg`) uses a node pool; note the `child()` reallocation pitfall.

A second use: **segment tree over values + merge small to large** (segment tree merging), where each vertex of a tree owns a sparse segment tree of its subtree's values and children are merged bottom-up in `O(n log C)` total — the standard solution to "k-th smallest in subtree" and "Distinct Colors"-style problems beyond IOI level.

---

## 12. Structures you should be able to name

**Wavelet tree.** Static array, values in `[0, σ)`. A binary tree over the value range; each node stores, for its subsequence, a bit per element ("goes left or right") and the prefix counts of those bits. Answers *k-th smallest in `[l, r)`*, *count of values `< x` in `[l, r)`*, *count of `x` in `[l, r)`* in `O(log σ)` each, `O(n log σ)` bits of memory, build `O(n log σ)`. Equivalent power to a persistent segment tree over positions (often easier to reason about) and to a merge-sort tree with fractional cascading. Needed for CSES Range Interval Queries if you want online; offline BIT sweep is the contest-standard alternative.

**Splay tree.** Self-adjusting BST; every access rotates the node to the root. Amortised `O(log n)` per operation with *no* stored balance information, and its `splay(x)` primitive makes *split* and *merge* trivial (splay the boundary node, cut a child pointer). Constant factor lower than a treap for sequential/local access patterns; code length ~1.5× a treap; used as the inner tree of link-cut trees.

**Link-cut tree.** Maintains a **forest under edge insertions and deletions** with path aggregates (`link(u,v)`, `cut(u,v)`, `path_query(u,v)`, `find_root`, `make_root`) in amortised `O(log n)`. It decomposes the tree into preferred paths, each a splay tree keyed by depth. Solves "dynamic connectivity in a forest", "dynamic MST", "path max with edge changes and re-linking". Statement signal: *edges are added and removed and the graph is always a forest*. IOI has essentially never required it (offline alternatives — divide-and-conquer on time with rollback DSU — cover the dynamic connectivity cases, e.g. CSES Dynamic Connectivity). Learn it after everything else in this course.

**Skip list.** Sorted linked lists in `O(log n)` expected levels; each element is promoted to the next level with probability 1/2. Same asymptotics as a treap, more memory, more code, no split/merge advantage. Historically relevant (concurrent implementations); in contests: never.

**Scapegoat / AVL / red-black.** Deterministic balance; you will not write them — `std::set` *is* a red-black tree. Know that `std::set` guarantees worst-case `O(log n)`, so a hostile test cannot break it, while treap and splay are expected/amortised.

---

## 13. Comparison table

| Structure | Operations | Time per op | Worst/expected/amortised | Lines to write | Use when |
|---|---|---|---|---|---|
| `std::set` / `map` | insert, erase, lower_bound, neighbours | `O(log n)` | worst-case | 0 | no order statistics needed |
| pbds `tree` | set + `order_of_key`, `find_by_order` | `O(log n)` | worst-case | 3 (typedef) | GCC available, order statistics |
| Treap (keys) | set + k-th, rank, split/merge by key, augmentation | `O(log n)` | expected | ~50 | order statistics + merging, persistence, hashing |
| Implicit treap | insert/erase at position, reverse, cut&paste, range aggregates | `O(log n)` | expected | ~70 | *positions* change |
| Splay | as treap, cheaper locality, no priorities | `O(log n)` | amortised | ~80 | link-cut internals |
| Link-cut tree | link, cut, path aggregate, reroot | `O(log n)` | amortised | ~150 | dynamic forest, online |
| Set of intervals | assign range, iterate range | `O(log n)` per interval touched | amortised (assign) | ~25 | assign-dominated, "occupied segments" |
| `priority_queue` + lazy delete | push, pop-max, erase(x) | `O(log n)` | amortised | 10 | Dijkstra-like, schedulers |
| Two heaps | streaming median | `O(log n)` | worst-case | 12 | median of a stream |
| Li Chao | insert line, extremum at x | `O(log C)` | worst-case | 30 | any slope/query order |
| Monotone CHT | insert line, extremum at x | `O(1)` amortised | amortised | 20 | monotone slopes and queries |
| Sparse segtree | point update, range query over `[0, 1e9)` | `O(log C)` | worst-case | 35 | online, huge coordinates |
| Wavelet tree | static k-th / count in range | `O(log σ)` | worst-case | 60 | static range order statistics online |
| Fenwick over compressed values | offline k-th / rank | `O(log n)` | worst-case | 15 | **default** when queries are known in advance |

The last row is the honest default: a huge share of "order statistics" problems in contests are solved by reading all queries first, compressing values, and using a Fenwick tree (k-th via binary lifting on the Fenwick). Reach for a treap when you cannot read ahead or when positions shift.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| "k-th smallest / how many smaller", online, insert+erase | pbds `tree` or Treap (or offline Fenwick) | `O((n+q) log n)` |
| "remove the k-th remaining element" | order statistics (Fenwick k-th if offline) | `O(n log n)` |
| "cut `[a,b]` and paste after `c`", "reverse `[a,b]`", "insert at position" | implicit treap | `O((n+q) log n)` |
| "sum/min of `[a,b]` interleaved with reversals / cut-paste" | implicit treap with aggregates + lazy | `O((n+q) log n)` |
| "assign value to a range" repeated, random data, plus weird range ops | set of intervals | `O((n+q) log n)` amortised |
| "positions of traffic lights / gaps between points, dynamic" | `set` + `multiset` of gap lengths | `O(q log n)` |
| "cheapest ticket ≤ budget, each sold once" | `multiset::upper_bound` + erase(it) | `O(n log n)` |
| "median / sum of distances to median of a sliding window" | two multisets with size balance | `O(n log n)` |
| "median of a stream" | two heaps | `O(n log n)` |
| "delete arbitrary element from PQ" | lazy deletion | `O(log n)` amortised |
| `dp[i] = min_j (dp[j] + a_j·x_i)` with monotone `a_j`, `x_i` | monotone CHT deque | `O(n)` |
| same, no monotonicity | Li Chao over `x` (compress if needed) | `O(n log C)` |
| "max of lines at x" queries online | Li Chao | `O(log C)` |
| coordinates `≤ 1e9`, point updates, online | sparse segtree (or compress offline) | `O(log C)` |
| "count values in `[a,b]` inside positions `[l,r]`", static | offline BIT sweep / wavelet / merge-sort tree | `O((n+q) log n)` |
| edges added and removed, forest, path queries online | link-cut tree (or offline D&C + rollback DSU) | `O(log n)` amortised |

## Implementation checklist for contests

- [ ] Treap: `t[0]` is the null node with `sz=0, sum=0, mn=+INF`; `upd(0)` returns immediately; never allocate nodes while holding an `int&` into the pool.
- [ ] Implicit treap: `push(v)` at the top of `split`, `merge`, `get`, and any traversal; `reverse` guards the empty middle; `split` uses `sz(left) < k` (first `k` go left).
- [ ] Ranges: decide `[l, r)` vs `[l, r]` once; CSES uses 1-indexed inclusive — convert at input (`l--`).
- [ ] `multiset::erase(value)` deletes all copies — use `erase(find(x))`; check `find != end()`.
- [ ] `std::set`: member `lower_bound`, not `std::lower_bound`; `it = s.erase(it)` in loops; check `begin()` before `--it`.
- [ ] Interval set: `split(r)` before `split(l)`; the map always covers the full range (start with one interval).
- [ ] Lazy PQ: `clean()` before *every* `top/pop/empty`; only erase elements that are actually present.
- [ ] Two heaps: rebalance after each insert; median = `lo.top()`; for even counts decide lower vs upper median as the statement says.
- [ ] CHT: slopes *strictly* monotone (handle equal slopes: keep the better intercept); `__int128` in `bad`; `ptr` clamped after pops; queries monotone or use binary search.
- [ ] Li Chao: domain bounds inclusive; `has[]` flag for empty nodes; `LLONG_MIN` identity; watch `a·x` overflow.
- [ ] pbds: `#include <ext/pb_ds/...>` compiles only on GCC — have the treap ready as a fallback; multiset via `pair<T, id>`.
- [ ] `ios::sync_with_stdio(false); cin.tie(nullptr);` and `'\n'`, never `endl`, when `q = 2e5` lines of output.
- [ ] Stress test against a `vector`/`multiset` brute force with random operations before submitting (this is what `example.cpp` does).

## Further reading

- CPH (Laaksonen, *Competitive Programmer's Handbook*): Ch. 4 (sets, maps, priority queues, policy-based structures), Ch. 8.3 (sliding window median), Ch. 9 (range queries), Ch. 10.2 (convex hull trick mention).
- cp-algorithms.com: "Treap (Cartesian tree)", "Implicit treap" section, "Sqrt / Segment tree — dynamic segment tree", "Convex hull trick and Li Chao tree", "Splay tree" (linked references), "Randomized Heap".
- KACTL (KTH ACM Contest Template Library): `LineContainer` (dynamic hull with `std::multiset`), `Treap`, `LinkCutTree`.
- Papers: Seidel & Aragon, *Randomized Search Trees* (1996) — the treap; Sleator & Tarjan, *Self-Adjusting Binary Search Trees* (1985) — splay trees; Sleator & Tarjan, *A Data Structure for Dynamic Trees* (1983) — link-cut; Pugh, *Skip Lists* (1990); Grossi, Gupta, Vitter, *High-Order Entropy-Compressed Text Indexes* (2003) — wavelet trees.
- Codeforces blog: "Chtholly Tree / ODT" write-ups for the interval-set amortisation; "Li Chao tree" tutorial by *zscoder*-style blogs (search the phrase).

## You can move on when...

- You can type `Treap` and `ImplicitTreap` from memory, compile with `-Wall -Wextra`, and pass random stress against `multiset` / `vector` in one sitting.
- You solve CSES *Cut and Paste*, *Substring Reversals*, *Reversals and Sums* each in under 25 minutes including debugging.
- Given a DP transition, you can say in 30 seconds whether it is CHT-shaped, whether the monotone deque applies, and what the slope/intercept/query are.
- You know without looking which `multiset`/`set` calls are traps, and you never write `std::lower_bound(s.begin(), s.end(), x)`.
- You can state what a link-cut tree and a wavelet tree do and give one reason each why an IOI problem would *not* need them.
