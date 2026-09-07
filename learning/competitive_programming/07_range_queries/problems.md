# Chapter 07 — Problems

How to work this ladder:

1. Before the first problem of each section, re-type the relevant structure from `example.cpp`
   **from memory** (Fenwick, iterative segtree, lazy segtree, persistent segtree) and stress-test
   it against a brute force on random arrays. Only then open the problem.
2. Put each solution in `competitive_programming/07_range_queries/solutions/<source>_<id>.cpp`
   (e.g. `cses_1648.cpp`, `cf_380C.cpp`).
3. Time yourself: ★1–2 problems should take ≤ 15 min including typing; ★3 ≤ 30 min; ★4–5 up to
   90 min before reading the hint. Write down the time.
4. Wrong answer on a hidden test ⇒ write a brute force and a random generator, find the
   smallest failing case, fix, and only then resubmit. Never "try a tweak".
5. Upsolve everything you did not finish; write a two-line note of the key observation.

Difficulty: ★1 (warm-up) … ★5 (IOI-day hard). CF problems carry their rating estimate.

---

## A. Static arrays: prefix sums and sparse table

### 07.1  Static Range Sum Queries  ·  CSES 1646  ·  ★1
https://cses.fi/problemset/task/1646
**Technique:** 1D prefix sums.
<details><summary>Hint</summary>`p[r+1] - p[l]`; `long long`.</details>
<details><summary>Approach sketch</summary>Build the prefix array once in O(n); each query is one subtraction. Use fast IO — 2·10^5 lines of output through `endl` is already noticeable.</details>

### 07.2  Static Range Minimum Queries  ·  CSES 1647  ·  ★2
https://cses.fi/problemset/task/1647
**Technique:** Sparse table (idempotent min).
<details><summary>Hint</summary>Two overlapping power-of-two blocks cover any interval.</details>
<details><summary>Approach sketch</summary>Precompute `t[k][i] = min(a[i..i+2^k-1])` and a `floor(log2)` table; a query reads two cells. A segment tree also passes but is the wrong reflex for a static array with `q = 2·10^5`.</details>

### 07.3  Range Xor Queries  ·  CSES 1650  ·  ★1
https://cses.fi/problemset/task/1650
**Technique:** Prefix xor (xor is its own inverse).
<details><summary>Hint</summary>`x ^ x = 0`.</details>
<details><summary>Approach sketch</summary>`px[i] = a[0] ^ ... ^ a[i-1]`; the xor of `a[l..r]` is `px[r+1] ^ px[l]`, exactly like sums because every element is its own inverse.</details>

### 07.4  Forest Queries  ·  CSES 1652  ·  ★2
https://cses.fi/problemset/task/1652
**Technique:** 2D prefix sums.
<details><summary>Hint</summary>Inclusion–exclusion with four corners.</details>
<details><summary>Approach sketch</summary>`p[i+1][j+1] = tree(i,j) + p[i][j+1] + p[i+1][j] - p[i][j]`; the rectangle count is `p[y2][x2] - p[y1-1][x2] - p[y2][x1-1] + p[y1-1][x1-1]` in 1-indexed form. 1000×1000 ints is 4 MB.</details>

### 07.5  Visible Buildings Queries  ·  CSES 3304  ·  ★3
https://cses.fi/problemset/task/3304
**Technique:** Next-greater pointers + binary lifting (a sparse table of jumps).
<details><summary>Hint</summary>From building `a`, the next visible one is the next strictly taller building. Jumping along that pointer is a functional graph.</details>
<details><summary>Approach sketch</summary>Compute `nxt[i]` (next index with a taller building) with a monotonic stack. Build `jump[k][i] = 2^k-th successor`. For a query `(a, b)` count how many jumps stay `≤ b` by descending `k` from high to low — O(log n) per query. Same skeleton as LCA binary lifting (ch. 10).</details>

### 07.6  Movie Festival Queries  ·  CSES 1664  ·  ★3
https://cses.fi/problemset/task/1664
**Technique:** Greedy successor function + binary lifting.
<details><summary>Hint</summary>Watching the movie that ends earliest among those starting at or after time `t` is optimal (ch. 08 of algorithms_learning: interval scheduling).</details>
<details><summary>Approach sketch</summary>Discretise time to `[0, 10^6]`. `best[t]` = earliest end among movies starting at `≥ t` (suffix minimum). `jump[k][t]` = time after watching `2^k` movies greedily from `t`. For `(a, b)` descend `k` while `jump[k][t] ≤ b`, counting movies.</details>

---

## B. Fenwick tree

### 07.7  Dynamic Range Sum Queries  ·  CSES 1648  ·  ★1
https://cses.fi/problemset/task/1648
**Technique:** Fenwick, point assign + range sum.
<details><summary>Hint</summary>Assign = add the difference; keep a copy of the array.</details>
<details><summary>Approach sketch</summary>`add(k, u - a[k]); a[k] = u`. Range sum is `prefix(r) - prefix(l-1)`. This is the file you should be able to type in 90 seconds.</details>

### 07.8  Range Update Queries  ·  CSES 1651  ·  ★2
https://cses.fi/problemset/task/1651
**Technique:** Fenwick over the difference array.
<details><summary>Hint</summary>Point value = prefix sum of differences.</details>
<details><summary>Approach sketch</summary>Initialise with `d[i] = a[i] - a[i-1]`. Range add `(l, r, x)`: `add(l, x), add(r+1, -x)`. Point query `k`: `prefix(k)`. Size the tree `n + 1` so `r + 1 = n` is a valid index.</details>

### 07.9  List Removals  ·  CSES 1749  ·  ★2
https://cses.fi/problemset/task/1749
**Technique:** Fenwick of presence bits + k-th element by binary lifting.
<details><summary>Hint</summary>"Remove the k-th remaining element" = find the smallest index with prefix count ≥ k.</details>
<details><summary>Approach sketch</summary>Start with all ones. `kth(k)` descends the implicit tree in O(log n); then `add(pos, -1)`. An `O(log² n)` binary search over `prefix()` also passes but the lifting version is the one to learn.</details>

### 07.10  Salary Queries  ·  CSES 1144  ·  ★3
https://cses.fi/problemset/task/1144
**Technique:** Offline coordinate compression + Fenwick of counts.
<details><summary>Hint</summary>Every salary that will ever exist appears in the input (initial or in a `!` update).</details>
<details><summary>Approach sketch</summary>Read all input; collect initial salaries and all update values; sort + unique. Maintain counts in a Fenwick indexed by compressed value. `? a b` = `prefix(upper_bound(b) - 1) - prefix(lower_bound(a) - 1)`. Online alternative: dynamic segment tree over `[1, 10^9]`.</details>

### 07.11  Forest Queries II  ·  CSES 1739  ·  ★3
https://cses.fi/problemset/task/1739
**Technique:** 2D Fenwick.
<details><summary>Hint</summary>Toggle = add `±1` depending on the current cell state.</details>
<details><summary>Approach sketch</summary>Keep the grid; on toggle, `add(y, x, tree ? -1 : +1)`. Rectangle sum via four prefix rectangles. `2·10^5 · log² 1000 ≈ 2·10^7` cell touches.</details>

### 07.12  Nested Ranges Count  ·  CSES 2169  ·  ★3
https://cses.fi/problemset/task/2169
**Technique:** Sort by one coordinate, Fenwick on the other (2D dominance counting).
<details><summary>Hint</summary>Sort by left endpoint ascending, right endpoint descending for ties; then "contains" becomes a prefix count on right endpoints.</details>
<details><summary>Approach sketch</summary>After sorting, range `i` contains range `j` iff `j` comes later and has `r_j ≤ r_i`. Sweep from the end with a Fenwick over compressed right endpoints: answer is `prefix(r_i)` before inserting `r_i`. Sweep from the front for "is contained by". Equal endpoints are why the tie-break order matters.</details>

### 07.13  Sliding Window Inversions  ·  CSES 3223  ·  ★3
https://cses.fi/problemset/task/3223
**Technique:** Fenwick maintained over a sliding window.
<details><summary>Hint</summary>Adding an element at the right adds "number of window elements greater than it"; removing at the left subtracts "number of window elements smaller than it".</details>
<details><summary>Approach sketch</summary>Compress values. Keep a Fenwick of counts of the current window. Slide: remove `a[i]` (inversions −= count of values `< a[i]` in window after removing it), add `a[i+k]` (inversions += count of values `> a[i+k]`). O(n log n).</details>

---

## C. Segment tree — point updates, structured nodes, walks

### 07.14  Dynamic Range Minimum Queries  ·  CSES 1649  ·  ★2
https://cses.fi/problemset/task/1649
**Technique:** Iterative segment tree, min.
<details><summary>Hint</summary>Fenwick cannot do min with arbitrary updates (no inverse).</details>
<details><summary>Approach sketch</summary>`SegTree<ll>` with `min` and identity `LLONG_MAX`. Point set + range query. Use this problem to verify your iterative template works for `n` not a power of two.</details>

### 07.15  Hotel Queries  ·  CSES 1143  ·  ★3
https://cses.fi/problemset/task/1143
**Technique:** Max segment tree + tree walk ("first index with value ≥ x").
<details><summary>Hint</summary>Do not binary search over range-max queries; descend once.</details>
<details><summary>Approach sketch</summary>If `mx[root] < x` answer 0. Otherwise descend: go left if `mx[left] ≥ x`, else right. Subtract the group size at the leaf and update upwards. O(log n) per group.</details>

### 07.16  Subarray Sum Queries  ·  CSES 1190  ·  ★3
https://cses.fi/problemset/task/1190
**Technique:** Segment tree with a 4-field node `(sum, pref, suf, best)`.
<details><summary>Hint</summary>What do you need from the two halves to compute the best subarray crossing the middle?</details>
<details><summary>Approach sketch</summary>Node combine: `best = max(L.best, R.best, L.suf + R.pref)`, `pref = max(L.pref, L.sum + R.pref)`, `suf = max(R.suf, R.sum + L.suf)`. Empty subarray allowed ⇒ all fields ≥ 0. Point update, answer is `root.best`.</details>

### 07.17  Subarray Sum Queries II  ·  CSES 3226  ·  ★3
https://cses.fi/problemset/task/3226
**Technique:** Same node as 07.16, but the query is over a range — the combine must be applied in left-to-right order.
<details><summary>Hint</summary>Non-commutative combine ⇒ separate left and right accumulators in the iterative query, or the recursive version.</details>
<details><summary>Approach sketch</summary>Reuse the `BestNode`. The range version tests that your query folds nodes in order; if you used a single accumulator, `pref/suf` come out wrong on a non-power-of-two `n`.</details>

### 07.18  Prefix Sum Queries  ·  CSES 2166  ·  ★3
https://cses.fi/problemset/task/2166
**Technique:** Segment tree node `(sum, max prefix)`.
<details><summary>Hint</summary>Max prefix sum of `a[l..r]` (empty allowed) = `max(L.maxpref, L.sum + R.maxpref)`.</details>
<details><summary>Approach sketch</summary>Two-field node, point update, range query — a strict subset of 07.16. Answer `max(0, node.maxpref)` if the empty prefix counts (read the statement).</details>

### 07.19  Pizzeria Queries  ·  CSES 2206  ·  ★3
https://cses.fi/problemset/task/2206
**Technique:** Two min segment trees to remove the absolute value.
<details><summary>Hint</summary>`p[i] + |i - k|` is `p[i] - i + k` for `i ≤ k` and `p[i] + i - k` for `i ≥ k`.</details>
<details><summary>Approach sketch</summary>Maintain trees over `p[i] - i` and `p[i] + i`. Query = `min(minL(0..k) + k, minR(k..n-1) - k)`. Point update touches both trees.</details>

### 07.20  Distinct Values Queries II  ·  CSES 3356  ·  ★4
https://cses.fi/problemset/task/3356
**Technique:** `prev[i]` (previous occurrence of the same value) + min segment tree; `std::set` per value to maintain `prev` under updates.
<details><summary>Hint</summary>All values in `[a, b]` are distinct iff `min(prev[a..b]) < a`.</details>
<details><summary>Approach sketch</summary>Keep `set<int>` of positions for each value. A point update changes `prev` for at most three positions (the updated one, its old successor, its new successor). Update those leaves in a min tree; a query is one range-min comparison against `a`.</details>

### 07.21  Bit Inversions  ·  CSES 1188  ·  ★3
https://cses.fi/problemset/task/1188
**Technique:** Segment tree node = `(len, prefix run, suffix run, best run, first char, last char)`.
<details><summary>Hint</summary>The longest constant run crossing the midpoint exists only if the boundary characters match.</details>
<details><summary>Approach sketch</summary>Point flip = set the leaf. Combine: `best = max(L.best, R.best, L.suf + R.pref if L.last == R.first)`; `pref = L.pref + (L.pref == L.len && L.last == R.first ? R.pref : 0)`, symmetric for `suf`. Answer `root.best` after each flip.</details>

### 07.22  Sliding Window Mex  ·  CSES 3219  ·  ★4
https://cses.fi/problemset/task/3219
**Technique:** Segment tree over values storing last occurrence + tree walk.
<details><summary>Hint</summary>Value `v` is missing from window `[i, i+k-1]` iff `last[v] < i`. The mex is the smallest such `v`.</details>
<details><summary>Approach sketch</summary>Values ≥ `n` never matter (mex ≤ `n`). Tree over `v ∈ [0, n]` storing `last[v]` with a min node. Walk: go left if `min(left) < i`, else right — O(log n) per window; updates are point sets as the window slides.</details>

---

## D. Lazy propagation

### 07.23  Range Updates and Sums  ·  CSES 1735  ·  ★4
https://cses.fi/problemset/task/1735
**Technique:** Lazy segment tree, range add + range assign + range sum (composed tags).
<details><summary>Hint</summary>Assign kills pending add; add on top of assign stays as `(assign, add)`.</details>
<details><summary>Approach sketch</summary>Exactly `LazySeg` from `example.cpp`. Push before descending; recompute after. Test with `n = 5` and mixed operations against a brute force before submitting — this is the problem where an untested push order costs an hour.</details>

### 07.24  Polynomial Queries  ·  CSES 1736  ·  ★4
https://cses.fi/problemset/task/1736
**Technique:** Lazy tag = arithmetic progression `(first term a, difference d)`.
<details><summary>Hint</summary>Adding `1, 2, 3, ...` to `[l, r]` is adding the AP with `a = 1, d = 1`; on a node of length `len` the sum grows by `a·len + d·len(len-1)/2`.</details>
<details><summary>Approach sketch</summary>Tags compose by adding component-wise. When pushing to the right child, its first term is `a + d · len_left`. Range sum otherwise standard. `long long` — the added sums reach `10^{10}` per query.</details>

### 07.25  Increasing Array Queries  ·  CSES 2416  ·  ★5
https://cses.fi/problemset/task/2416
**Technique:** Offline sweep from the right + monotonic stack + range assign / range sum segment tree.
<details><summary>Hint</summary>Cost of `[a, b]` = `Σ (prefixmax_a(i) - a[i])`. Fix `a` and sweep it leftwards: the "prefix max" function is a staircase that changes by merging steps.</details>
<details><summary>Approach sketch</summary>Process `a` from `n-1` down to `0`, maintaining a stack of steps (value, range) of the prefix-max function starting at `a`. When `a[a]` becomes the new start, it swallows all steps with smaller value — assign `a[a]` on their merged range in a lazy tree of "prefix max values". Answer for `(a, b)` = `sum_tree(a..b) - (p[b+1] - p[a])`. Amortised O((n+q) log n).</details>

---

## E. Persistence, merge sort tree, offline vs. Mo's

### 07.26  Distinct Values Queries  ·  CSES 1734  ·  ★3
https://cses.fi/problemset/task/1734
**Technique:** Three valid solutions — offline sweep + BIT (ch. 08), Mo's algorithm, or persistent segment tree over `prev[i]`. Do it with Mo's here and with the BIT sweep in ch. 08.
<details><summary>Hint</summary>Mo's: maintain `cnt[value]`; `distinct` changes when a count crosses 0.</details>
<details><summary>Approach sketch</summary>Compress values. Sort queries by `(l / B, r)` with the odd-even trick, `B = n / sqrt(q)`. Expand before shrink. Around 10^8 simple operations — fits, but the O((n+q) log n) offline solution is the "real" one for this problem.</details>

### 07.27  Range Interval Queries  ·  CSES 3163  ·  ★4
https://cses.fi/problemset/task/3163
**Technique:** Count of values in `[c, d]` within `a[a..b]`: merge sort tree (O(log² n)), persistent segment tree (O(log n)), or offline BIT.
<details><summary>Hint</summary>`count(a..b, ≤ d) - count(a..b, ≤ c-1)`.</details>
<details><summary>Approach sketch</summary>Persistent version: version `i` contains `a[0..i-1]` over the value domain; the answer is `count_leq(root[b+1], d) - count_leq(root[a], d)` minus the same for `c-1`. Merge sort tree passes too and is easier to write; do both.</details>

### 07.28  Missing Coin Sum Queries  ·  CSES 2184  ·  ★4
https://cses.fi/problemset/task/2184
**Technique:** "Smallest unreachable sum" doubling + range "sum of values ≤ x" via persistent segment tree (or merge sort tree with prefix sums).
<details><summary>Hint</summary>If all coins ≤ x in the range sum to `s ≥ x`, every sum up to `s` is reachable; set `x = s + 1` and repeat. `x` at least doubles each step.</details>
<details><summary>Approach sketch</summary>Persistent tree over compressed values storing counts and sums. Each query runs O(log(sum)) iterations, each a two-root descent "sum of values ≤ x in `a[l..r]`" — O(log² n) per query overall.</details>

### 07.29  Range Queries and Copies  ·  CSES 1737  ·  ★4
https://cses.fi/problemset/task/1737
**Technique:** Persistent segment tree with point updates and version copying.
<details><summary>Hint</summary>A "copy" costs O(1): push another root pointer.</details>
<details><summary>Approach sketch</summary>Keep `vector<int> roots`. Update on version `k` = path copy from `roots[k]`, replacing `roots[k]` with the new root. Sum query is a normal descent from `roots[k]`. Memory ≈ (n + q) · 18 nodes.</details>

### 07.30  Sereja and Brackets  ·  Codeforces 380C  ·  ~2000
https://codeforces.com/problemset/problem/380/C
**Technique:** Segment tree node `(matched pairs, unmatched '(', unmatched ')')`.
<details><summary>Hint</summary>Crossing matches = `min(L.open, R.close)`.</details>
<details><summary>Approach sketch</summary>Combine: `m = min(L.open, R.close)`; `pairs = L.pairs + R.pairs + m`; `open = L.open + R.open - m`; `close = L.close + R.close - m`. Static array, so a merge is the whole solution; answer `2 · pairs`.</details>

### 07.31  Xenia and Bit Operations  ·  Codeforces 339D  ·  ~1700
https://codeforces.com/problemset/problem/339/D
**Technique:** Segment tree whose combine depends on the level (alternating or/xor).
<details><summary>Hint</summary>Level parity is fixed by depth; `n = 2^k` so the iterative tree is perfect.</details>
<details><summary>Approach sketch</summary>Use the iterative tree; when recomputing `t[p >> 1]`, the operation is `or` on odd levels from the bottom and `xor` on even ones. Point updates only.</details>

### 07.32  Circular RMQ  ·  Codeforces 52C  ·  ~2200
https://codeforces.com/problemset/problem/52/C
**Technique:** Lazy range add + range min on a circular array.
<details><summary>Hint</summary>A wrapping range `[l, r]` with `l > r` is two ranges.</details>
<details><summary>Approach sketch</summary>Standard lazy tree (add tag, min value); split every circular range at the array end. Parsing: a line with two numbers is a query, three is an update.</details>

### 07.33  Powerful array  ·  Codeforces 86D  ·  ~2200
https://codeforces.com/problemset/problem/86/D
**Technique:** Mo's algorithm.
<details><summary>Hint</summary>Adding an occurrence of `v` changes `Σ cnt[v]² · v` by `(2·cnt[v] + 1) · v`.</details>
<details><summary>Approach sketch</summary>O(1) add/remove, no combine possible ⇒ Mo's. Values up to 10^6 so `cnt` can be a plain array. Use the odd-even trick or Hilbert order; this problem is known to need a tight constant.</details>

### 07.34  One Occurrence  ·  Codeforces 1000F  ·  ~2500
https://codeforces.com/problemset/problem/1000/F
**Technique:** Offline sweep over `r` with a segment tree over positions storing `prev[i]` (or persistent).
<details><summary>Hint</summary>`a[i]` occurs exactly once in `[l, r]` iff `i ≤ r`, `prev[i] < l`, and `next[i] > r`.</details>
<details><summary>Approach sketch</summary>Sweep `r` from left to right; keep only the last occurrence of each value "active" (deactivate the previous one). Store `prev[i]` at active position `i`, `+∞` elsewhere; query = position of the minimum over `[l, r]`, valid iff that minimum `< l`. Needs a `(min, argmin)` node.</details>

### 07.35  K-th Number  ·  SPOJ MKTHNUM  ·  ★4
https://www.spoj.com/problems/MKTHNUM/
**Technique:** Persistent segment tree, k-th smallest in a subarray.
<details><summary>Hint</summary>Exactly §4.4 of the lesson.</details>
<details><summary>Approach sketch</summary>Compress, build versions, two-root descent. Also solvable with a merge sort tree + binary search in O(log³ n) — try that first if you want to feel the difference.</details>

### 07.36  Wall  ·  IOI 2014 (oj.uz)  ·  ★4
https://oj.uz/problem/view/IOI14_wall
**Technique:** Lazy segment tree with composed `chmax` / `chmin` tags (clamp).
<details><summary>Hint</summary>A pending pair `(lo, hi)` meaning "clamp into `[lo, hi]`" composes with a new clamp into another clamp.</details>
<details><summary>Approach sketch</summary>Each node stores the clamp `(lo, hi)`; adding `chmax(x)` sets `lo = max(lo, x), hi = max(hi, x)`, `chmin(x)` symmetric. Leaves start at 0. After all operations, push everything down and read the leaves. O((n + k) log n).</details>

---

Practice more: Codeforces problemset, tag `data structures`, rating 1600–2100 (segment trees,
Fenwick, offline sweeps). Then tag `data structures` + `divide and conquer` at 2100–2400 for
persistent trees and Mo's.
Practice more: SPOJ GSS1 / GSS3 (max subarray, static / with updates), SPOJ DQUERY (distinct
values), SPOJ COT (k-th on tree paths — after ch. 10).
Practice more: AtCoder Library Practice Contest, problem J "Segment Tree" (tree walk with
`max_right`), and the AtCoder Beginner Contest "Ex/G" segment-tree problems.

## Progress

- [ ] 07.1 Static Range Sum Queries (CSES 1646)
- [ ] 07.2 Static Range Minimum Queries (CSES 1647)
- [ ] 07.3 Range Xor Queries (CSES 1650)
- [ ] 07.4 Forest Queries (CSES 1652)
- [ ] 07.5 Visible Buildings Queries (CSES 3304)
- [ ] 07.6 Movie Festival Queries (CSES 1664)
- [ ] 07.7 Dynamic Range Sum Queries (CSES 1648)
- [ ] 07.8 Range Update Queries (CSES 1651)
- [ ] 07.9 List Removals (CSES 1749)
- [ ] 07.10 Salary Queries (CSES 1144)
- [ ] 07.11 Forest Queries II (CSES 1739)
- [ ] 07.12 Nested Ranges Count (CSES 2169)
- [ ] 07.13 Sliding Window Inversions (CSES 3223)
- [ ] 07.14 Dynamic Range Minimum Queries (CSES 1649)
- [ ] 07.15 Hotel Queries (CSES 1143)
- [ ] 07.16 Subarray Sum Queries (CSES 1190)
- [ ] 07.17 Subarray Sum Queries II (CSES 3226)
- [ ] 07.18 Prefix Sum Queries (CSES 2166)
- [ ] 07.19 Pizzeria Queries (CSES 2206)
- [ ] 07.20 Distinct Values Queries II (CSES 3356)
- [ ] 07.21 Bit Inversions (CSES 1188)
- [ ] 07.22 Sliding Window Mex (CSES 3219)
- [ ] 07.23 Range Updates and Sums (CSES 1735)
- [ ] 07.24 Polynomial Queries (CSES 1736)
- [ ] 07.25 Increasing Array Queries (CSES 2416)
- [ ] 07.26 Distinct Values Queries (CSES 1734)
- [ ] 07.27 Range Interval Queries (CSES 3163)
- [ ] 07.28 Missing Coin Sum Queries (CSES 2184)
- [ ] 07.29 Range Queries and Copies (CSES 1737)
- [ ] 07.30 Sereja and Brackets (CF 380C)
- [ ] 07.31 Xenia and Bit Operations (CF 339D)
- [ ] 07.32 Circular RMQ (CF 52C)
- [ ] 07.33 Powerful array (CF 86D)
- [ ] 07.34 One Occurrence (CF 1000F)
- [ ] 07.35 K-th Number (SPOJ MKTHNUM)
- [ ] 07.36 Wall (IOI 2014)
