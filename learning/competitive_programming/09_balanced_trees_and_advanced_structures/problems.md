# Chapter 09 — Problems

How to work this ladder:

1. Re-type `Treap`, `ImplicitTreap`, `IntervalSet`, `LazyMaxPQ`, `RunningMedian`, `LiChao`, `MonotoneCHT` from memory first (see `example.cpp`). Do not open the file while solving.
2. One file per problem: `competitive_programming/09_balanced_trees_and_advanced_structures/solutions/<source>_<id>.cpp` (e.g. `cses_2072.cpp`, `cf_915E.cpp`).
3. Before submitting anything with a hand-written structure, stress-test it against a brute force (`vector` / `multiset` / `O(n²)` loop) on random small inputs. A treap bug found by the judge costs 10 minutes; found by your stress script, 1 minute.
4. Time yourself: ★1-2 → 15 min, ★3 → 30 min, ★4 → 60 min, ★5 → 90 min. When the timer runs out, read the hint, then the approach, then implement anyway. Upsolve everything you did not finish within 48 hours.
5. Problems marked *(offline alternative)* can be solved with a Fenwick tree if you read all input first — solve them **both** ways: once with the offline trick, once with the online structure.

Difficulty: ★1 (CF ~1200) … ★5 (CF ~2400+).

---

## A. `std::set` / `std::multiset` as the whole solution

### 09.1  Concert Tickets  ·  CSES 1091  ·  ★1
https://cses.fi/problemset/task/1091
**Technique:** `multiset::upper_bound`, `erase(iterator)`.
<details><summary>Hint</summary>Each customer wants the most expensive ticket with price ≤ their maximum. Which multiset call gives "largest ≤ x"?</details>
<details><summary>Approach sketch</summary>Put all prices in a `multiset`. For each customer, `it = upper_bound(x)`; if `it == begin()` print −1, otherwise `--it`, print `*it`, `erase(it)`. The erase must be by iterator — erasing by value would remove every ticket with that price. `O((n+m) log n)`.</details>

### 09.2  Towers  ·  CSES 1073  ·  ★1
https://cses.fi/problemset/task/1073
**Technique:** greedy + `multiset::upper_bound`.
<details><summary>Hint</summary>A cube goes on the tower whose top is the smallest value strictly greater than the cube. Why is this greedy optimal?</details>
<details><summary>Approach sketch</summary>Keep a multiset of tower tops. For cube `x`, find `upper_bound(x)`; if none exists start a new tower, else replace that top with `x` (erase iterator, insert `x`). Exchange argument: placing on the tightest fitting tower keeps all larger tops available for future cubes. Answer = multiset size. Equivalent to counting the minimum number of non-increasing subsequences (patience sorting).</details>

### 09.3  Traffic Lights  ·  CSES 1163  ·  ★2
https://cses.fi/problemset/task/1163
**Technique:** `set` of positions + `multiset` of gap lengths (the "set of intervals" idea in its simplest form).
<details><summary>Hint</summary>Adding a light at `p` destroys one gap and creates two. Which gap is destroyed?</details>
<details><summary>Approach sketch</summary>`set<int> pos = {0, x}`, `multiset<int> gaps = {x}`. For a new light `p`: `r = *pos.lower_bound(p)`, `l = *prev(that iterator)`; erase one copy of `r − l` from `gaps`, insert `p − l` and `r − p`, insert `p` into `pos`. Answer after each step is `*gaps.rbegin()`. Every operation is `O(log n)`.</details>

### 09.4  Room Allocation  ·  CSES 1164  ·  ★2
https://cses.fi/problemset/task/1164
**Technique:** sweep by arrival + min-heap / `set` of (departure, room).
<details><summary>Hint</summary>Sort customers by arrival. A room is reusable if its current occupant left strictly before the new arrival.</details>
<details><summary>Approach sketch</summary>Sort by arrival, keeping original indices. Maintain a min-heap of `(departure_day, room_id)` of occupied rooms. For each customer, if the heap top departs before the arrival, pop it and reuse its room; otherwise allocate a new room. Record the room per original index; the answer is the max room id. Prove: at any time the number of rooms in use equals the max overlap so far.</details>

### 09.5  Movie Festival II  ·  CSES 1632  ·  ★2
https://cses.fi/problemset/task/1632
**Technique:** greedy by end time + `multiset` of member "free at" times.
<details><summary>Hint</summary>Process movies sorted by end time. Which free member should watch the current movie?</details>
<details><summary>Approach sketch</summary>`multiset<int> free` with `k` zeros. For each movie `(a, b)` in end-time order, find the member whose free time is the largest value `≤ a` (`upper_bound(a)`, step back). If one exists, replace it with `b` and count the movie; otherwise skip. The tightest-fit choice is optimal by the same exchange argument as Towers.</details>

### 09.6  Sliding Window Distinct Values  ·  CSES 3222  ·  ★2
https://cses.fi/problemset/task/3222
**Technique:** `map<value, count>` (or a compressed count array) with a sliding window.
<details><summary>Hint</summary>Distinct count changes only when a count crosses between 0 and 1.</details>
<details><summary>Approach sketch</summary>Maintain counts of values in the window; `distinct += (cnt[x]++ == 0)` on entry and `distinct -= (--cnt[x] == 0)` on exit. Compress values first if you want an array instead of a `map`. `O(n log n)`.</details>

### 09.7  Nested Ranges Check  ·  CSES 2168  ·  ★3
https://cses.fi/problemset/task/2168
**Technique:** sort by (`l` asc, `r` desc) + running max/min of `r`.
<details><summary>Hint</summary>After sorting by left endpoint ascending and right endpoint descending, "range i contains some range j" becomes a statement about the suffix minimum of `r`.</details>
<details><summary>Approach sketch</summary>With that sort order, range `i` contains some later range iff `min r over the suffix after i ≤ r_i`; range `i` is contained in some earlier range iff `max r over the prefix before i ≥ r_i`. Two linear sweeps after sorting. Ties on `l` are exactly why `r` must be sorted descending.</details>

### 09.8  Nested Ranges Count  ·  CSES 2169  ·  ★3  *(offline alternative)*
https://cses.fi/problemset/task/2169
**Technique:** same sort + order statistics over `r` (Fenwick over compressed `r`, or pbds `order_of_key`).
<details><summary>Hint</summary>In the sorted order, "number of ranges contained in i" = number of later ranges with `r ≤ r_i`.</details>
<details><summary>Approach sketch</summary>Sweep the sorted list from the right, maintaining an ordered multiset of the `r` values seen so far; `contains[i] = count(r ≤ r_i)`, then insert `r_i`. Sweep from the left for `containedBy[i] = count(r ≥ r_i)`. With pbds: `order_of_key({r_i + 1, -1})` on a set of `pair<r, idx>`. With a Fenwick: compress `r`, prefix sums. Both `O(n log n)`.</details>

---

## B. Order statistics online

### 09.9  Josephus Problem II  ·  CSES 2163  ·  ★3  *(offline alternative)*
https://cses.fi/problemset/task/2163
**Technique:** k-th element + erase in an order-statistics structure.
<details><summary>Hint</summary>The removed position is `(cur + k) mod remaining`; you need "give me the element at index i and delete it".</details>
<details><summary>Approach sketch</summary>Keep the surviving numbers in an order-statistics structure (pbds `find_by_order`, treap `kth`, or a Fenwick tree over `1..n` with binary-lifting descent to find the i-th one). Maintain the current index `cur`; each step `cur = (cur + k) % size`, output the `cur`-th element, erase it. `O(n log n)`. The Fenwick version teaches the "k-th one" descent; the treap version is the general tool.</details>

### 09.10  List Removals  ·  CSES 1749  ·  ★3  *(offline alternative)*
https://cses.fi/problemset/task/1749
**Technique:** k-th remaining element (order statistics by position).
<details><summary>Hint</summary>Exactly Josephus II without the modular step.</details>
<details><summary>Approach sketch</summary>Fenwick of ones over positions, "find k-th one" descent, set to zero after printing — or an implicit treap with `split(k-1)`, take the root of the middle, `merge`. Both `O(n log n)`. Do it with the implicit treap once to practise `kth` on positions.</details>

### 09.11  Salary Queries  ·  CSES 1144  ·  ★3  *(offline alternative)*
https://cses.fi/problemset/task/1144
**Technique:** dynamic multiset with rank queries: pbds / treap `countLess`, or offline-compressed Fenwick, or sparse segment tree over `[1, 1e9]`.
<details><summary>Hint</summary>"How many salaries are in `[a, b]`" = `rank(b+1) − rank(a)` on the current multiset.</details>
<details><summary>Approach sketch</summary>Online: treap or pbds multiset (`pair<salary, id>`) with `countLess`. Offline: read all updates, compress the union of all salary values ever present, Fenwick point ±1 and prefix sums. Also solve it with the `SparseSeg` from `example.cpp` to feel the memory cost (`≈ 30 nodes × 4e5 operations`).</details>

### 09.12  Distinct Values Queries  ·  CSES 1734  ·  ★3
https://cses.fi/problemset/task/1734
**Technique:** offline sweep by right endpoint + Fenwick over "last occurrence" (the set-based online version uses one `set` of positions per value).
<details><summary>Hint</summary>An element at position `i` is the *last* occurrence of its value within `[l, r]` iff `next[i] > r`. Count such `i ∈ [l, r]`.</details>
<details><summary>Approach sketch</summary>Sort queries by `r`. Sweep `r` from left to right maintaining a Fenwick with a `1` at the *latest* position of each value seen (when value `x` reappears at `i`, subtract 1 at its previous position, add 1 at `i`). Query `[l, r]` = Fenwick sum over `[l, r]`. `O((n+q) log n)`. This is the template for "count distinct in range".</details>

### 09.13  Distinct Values Queries II  ·  CSES 3356  ·  ★4
https://cses.fi/problemset/task/3356
**Technique:** `set<int>` of positions per value + segment tree of `prev[i]` (min/max).
<details><summary>Hint</summary>`[l, r]` has all-distinct values iff `max_{i ∈ [l,r]} prev[i] < l`, where `prev[i]` is the previous position holding the same value. A point update changes `prev` for at most 3 positions.</details>
<details><summary>Approach sketch</summary>Keep one `set<int>` of positions per value. Changing `a[i]` from `x` to `y`: in `x`'s set the successor of `i` gets a new `prev` (its predecessor in the set), remove `i`; in `y`'s set insert `i`, set `prev[i]` = predecessor, and the successor of `i` now has `prev = i`. Each `prev` change is a point update in a max segment tree; the query is a range max compared with `l`. `O((n+q) log n)`.</details>

### 09.14  Sliding Window Median  ·  CSES 1076  ·  ★3
https://cses.fi/problemset/task/1076
**Technique:** two multisets with size invariant `|lo| = ⌈k/2⌉`.
<details><summary>Hint</summary>Median of the window = `*lo.rbegin()` if `lo` holds the smaller half and has the extra element for odd `k`.</details>
<details><summary>Approach sketch</summary>Insert into `lo` if `x ≤ *lo.rbegin()` else into `hi`; erase from the set that contains the leaving element (compare with `*lo.rbegin()`); then rebalance one element across so `|lo| = ⌈k/2⌉`. Print `*lo.rbegin()`. Every step `O(log k)`. Alternative for practice: order-statistics treap with `kth((k−1)/2)`.</details>

### 09.15  Sliding Window Cost  ·  CSES 1077  ·  ★3
https://cses.fi/problemset/task/1077
**Technique:** two multisets + running sums of each half.
<details><summary>Hint</summary>Cost to make all equal is minimised at the median (see lesson §8). Write the cost as a function of the median, `|lo|`, `|hi|`, `sumLo`, `sumHi`.</details>
<details><summary>Approach sketch</summary>Same structure as 09.14 with `sumLo`, `sumHi` maintained on every insert/erase/rebalance. `cost = m·|lo| − sumLo + sumHi − m·|hi|` with `m = *lo.rbegin()`. Use `long long`: `2e5 · 1e9` overflows `int`.</details>

### 09.16  Programmers and Artists  ·  CSES 2426  ·  ★4
https://cses.fi/problemset/task/2426
**Technique:** sort by `x − y`, prefix/suffix greedy with a min-heap ("top-k sum" maintained by a heap).
<details><summary>Hint</summary>After sorting by `x − y` descending, an optimal solution takes programmers from a prefix and artists from a suffix. Why?</details>
<details><summary>Approach sketch</summary>Exchange argument: if a programmer `i` and artist `j` with `x_i − y_i < x_j − y_j` are both chosen, swapping roles does not decrease the total. So for every split point `p`, choose the `a` largest `x` among the first `p` people and the `b` largest `y` among the rest. Maintain "sum of the `a` largest of a growing prefix" with a min-heap of size `a` (pop the smallest when it exceeds `a`), and symmetrically for the suffix; combine over all `p`. `O(n log n)`.</details>

---

## C. Implicit treap — the rope

### 09.17  Cut and Paste  ·  CSES 2072  ·  ★3
https://cses.fi/problemset/task/2072
**Technique:** implicit treap, `cutPaste`.
<details><summary>Hint</summary>"Remove substring `[a, b]` and append it to the end" is three splits and three merges.</details>
<details><summary>Approach sketch</summary>Build the implicit treap from the string (values = characters). For each operation: `split(root, a−1) → A, BC`; `split(BC, b−a+1) → B, C`; `root = merge(merge(A, C), B)`. Print the in-order traversal at the end (iterative or recursive — depth is `O(log n)`). Stress-test against `string::erase` + `append`.</details>

### 09.18  Substring Reversals  ·  CSES 2073  ·  ★3
https://cses.fi/problemset/task/2073
**Technique:** implicit treap with lazy `rev`.
<details><summary>Hint</summary>Isolate `[a, b]`, toggle the flag on its root, merge back. Where must `push` be called?</details>
<details><summary>Approach sketch</summary>Exactly `ImplicitTreap::reverse`. The classic bug: `merge` without `push` on both arguments. Verify with random reversals against `std::reverse` on a `string` before submitting.</details>

### 09.19  Reversals and Sums  ·  CSES 2074  ·  ★4
https://cses.fi/problemset/task/2074
**Technique:** implicit treap with lazy reversal + `sum` aggregate.
<details><summary>Hint</summary>Sum is symmetric under reversal, so the aggregate stays valid without pushing — only child order is stale.</details>
<details><summary>Approach sketch</summary>`reverse(l, r)` and `query(l, r).first` from the lesson. Values up to `1e9` × `2e5` elements → `long long` sums. Make sure the null node has `sum = 0`.</details>

### 09.20  Reversal Sorting  ·  CSES 2075  ·  ★5
https://cses.fi/problemset/task/2075
**Technique:** implicit treap with lazy reversal, subtree-min aggregate, and *position lookup* of a node (parent pointers or "descend by min").
<details><summary>Hint</summary>Sort a permutation with at most `n` reversals: at step `i`, reverse `[i, pos(i)]` where `pos(i)` is the current position of value `i`. You need "current position of the minimum in the suffix" quickly.</details>
<details><summary>Approach sketch</summary>Split off the unsorted suffix `[i, n]`; find the position of its minimum by descending from the root while comparing the child's `mn` aggregates (push lazy flags on the way down) and accumulating `sz(left)+1`; that gives `p`. Output `(i, p)`, reverse `[i, p]` in the treap, continue. `O(n log n)` total, `n` operations printed.</details>

---

## D. Lines: CHT and Li Chao

### 09.21  Monster Game I  ·  CSES 2084  ·  ★4
https://cses.fi/problemset/task/2084
**Technique:** DP with convex hull trick; slopes monotone → deque `O(n)`, or Li Chao `O(n log C)`.
<details><summary>Hint</summary>`dp[i] = min_{j<i} ( dp[j] + s_i · f_j )` after you write the cost of killing monsters `j+1..i` with the sword obtained from monster `j`. What are the lines and the query point?</details>
<details><summary>Approach sketch</summary>Killing monster `i` with the current sword factor `f_j` costs `s_i · f_j`, and after killing `i` you may switch to `f_i`. So `dp[i] = min_j (dp[j] + s_i · f_j)`: lines with slope `f_j`, intercept `dp[j]`, queried at `x = s_i`. In game I the `f` are given decreasing (and `s` increasing), so the monotone deque applies. Products reach `1e9 · 1e9` — `long long` values, `__int128` in the intersection test.</details>

### 09.22  Monster Game II  ·  CSES 2085  ·  ★4
https://cses.fi/problemset/task/2085
**Technique:** same DP, no monotonicity → Li Chao tree over `s` (compressed) or a dynamic hull.
<details><summary>Hint</summary>Which assumption of the deque version is broken here, and which structure does not need it?</details>
<details><summary>Approach sketch</summary>Same transition as 09.21, but `f` and `s` are arbitrary. Insert each line `(f_j, dp[j])` into a Li Chao tree over the compressed set of `s` values (min version) and query at `s_i`. `O(n log n)`.</details>

### 09.23  Lines and Queries I  ·  CSES 3429  ·  ★3
https://cses.fi/problemset/task/3429
**Technique:** static upper envelope: sort lines by slope, build the hull once, binary search per query — or Li Chao with all lines inserted first.
<details><summary>Hint</summary>All lines are known before the queries. Build the hull once; each query is a binary search on the breakpoints.</details>
<details><summary>Approach sketch</summary>Sort by slope, drop useless lines with the `bad` test, then for each `x` binary search the hull for the optimal segment (or sort the queries and use the deque pointer). Equivalent and lazier: insert all lines into a Li Chao over the query range and answer each in `O(log C)`.</details>

### 09.24  Lines and Queries II  ·  CSES 3430  ·  ★4
https://cses.fi/problemset/task/3430
**Technique:** Li Chao tree — lines arrive interleaved with queries.
<details><summary>Hint</summary>Online insertion in any slope order with max queries at arbitrary `x` is the Li Chao signature.</details>
<details><summary>Approach sketch</summary>Read all query `x` values first only if you need compression; with `|x| ≤ 1e6`-scale coordinates use the array Li Chao directly. Insert on "add line", query on "ask". `O((n+q) log C)`. Watch `a · x + b` overflow bounds from the statement.</details>

---

## E. Range order statistics and wavelet-type queries

### 09.25  Range Interval Queries  ·  CSES 3163  ·  ★4
https://cses.fi/problemset/task/3163
**Technique:** offline: sort queries by value bound, Fenwick over positions (`count(l, r, ≤ b) − count(l, r, ≤ a−1)`); online: wavelet tree or merge-sort tree.
<details><summary>Hint</summary>"Count positions in `[l, r]` with value in `[a, b]`" is 2D dominance counting: split each query into two "value ≤ v" queries and sweep by `v`.</details>
<details><summary>Approach sketch</summary>Sort array elements by value and the split queries by threshold. Sweep thresholds upward, adding position `i` to a Fenwick when `a_i ≤` threshold; answer each split query as a Fenwick range sum on `[l, r]`. `O((n+q) log n)`. Then read about the wavelet tree and see why it answers the same in `O(log σ)` online.</details>

### 09.26  Hotel Queries  ·  CSES 1143  ·  ★2
https://cses.fi/problemset/task/1143
**Technique:** segment tree descent (the "first position with value ≥ x" primitive that treaps and Li Chao also rely on).
<details><summary>Hint</summary>Walk from the root, always preferring the left child if its max is ≥ demand.</details>
<details><summary>Approach sketch</summary>Max segment tree over room capacities; for each group descend to the leftmost leaf with `max ≥ x`, output its index, subtract `x` (point update). `O((n+m) log n)`. Included here as the descent pattern used in `Treap::kth` and Fenwick "k-th one".</details>

---

## F. Set of intervals (Chtholly) and beyond — Codeforces

### 09.27  Physical Education Lessons  ·  Codeforces 915E  ·  ★4 (CF 2300)
https://codeforces.com/contest/915/problem/E
**Technique:** set of disjoint intervals with assign; maintain the total number of working days.
<details><summary>Hint</summary>Each operation assigns "working" or "non-working" to a range of days up to `1e9`; only the assignments' endpoints matter.</details>
<details><summary>Approach sketch</summary>`IntervalSet` over `[1, n]` with values `0/1`; on assign, subtract the lengths of the erased intervals weighted by their value and add the new interval's contribution. `O(q log q)` amortised by the argument in lesson §5.2. Alternative: sparse segment tree with lazy assign.</details>

### 09.28  Willem, Chtholly and Seniorious  ·  Codeforces 896C  ·  ★5 (CF 3000)
https://codeforces.com/contest/896/problem/C
**Technique:** the original "Chtholly tree": set of intervals with assign, add, k-th smallest, sum of powers — on **random** operations.
<details><summary>Hint</summary>Why does the expected number of intervals stay `O(log n)` when assigns are random?</details>
<details><summary>Approach sketch</summary>Implement `split` once; every operation iterates the intervals intersecting `[l, r]`: add → add to each value; assign → erase and insert one; k-th → collect `(value, length)` pairs, sort, walk; power sum → sum `len · value^x mod y`. Correct only because the data is generated randomly; know that this is the exception, not a general technique.</details>

### 09.29  T-Shirts  ·  Codeforces 702F  ·  ★5 (CF 2600)
https://codeforces.com/contest/702/problem/F
**Technique:** treap with lazy "subtract c" and merging with `O(log²)` amortisation.
<details><summary>Hint</summary>Process shirts by quality; each shirt is bought by every customer whose budget ≥ its cost. After buying, budgets in `[c, 2c)` fall below `c` and cross over the others — reinsert them one by one.</details>
<details><summary>Approach sketch</summary>Keep budgets in a treap keyed by remaining money with lazy tags "subtract `c`" and "+1 shirt". For a shirt of cost `c`: split into `< c`, `[c, 2c)`, `≥ 2c`; subtract `c` and +1 on both upper parts; merge the `≥ 2c` part back (its order relative to `< c` is preserved); reinsert the `[c, 2c)` elements one by one. Each reinsertion at least halves a budget, so each element is reinserted `O(log C)` times: `O(n log C log n)` total.</details>

### 09.30  Escape Through Leaf  ·  Codeforces 932F  ·  ★5 (CF 2700)
https://codeforces.com/contest/932/problem/F
**Technique:** Li Chao tree + merge small into large on a tree (bridge to Chapter 10).
<details><summary>Hint</summary>`dp[v] = min over leaves u in subtree(v) of (a_v · b_u + dp[u])`: lines `(b_u, dp[u])` queried at `a_v`, one line set per subtree.</details>
<details><summary>Approach sketch</summary>Each vertex owns a dynamic Li Chao (pointer-based over `[-1e5, 1e5]`) containing the lines of its subtree's leaves; merge children's trees into the largest one (re-inserting each line of the smaller trees) — every line moves `O(log n)` times, each insertion `O(log C)`. Query at `a_v` for `dp[v]`, then insert `(b_v, dp[v])`. `O(n log n log C)`.</details>

### 09.31  Kalila and Dimna in the Logging Industry  ·  Codeforces 319C  ·  ★4 (CF 2200)
https://codeforces.com/contest/319/problem/C
**Technique:** convex hull trick, monotone deque.
<details><summary>Hint</summary>`dp[i] = min_j (dp[j] + b_j · a_i)` with `a` increasing and `b` decreasing — the textbook monotone case. Which tree must be cut last, and why is the answer `dp[n]`?</details>
<details><summary>Approach sketch</summary>Cutting tree `n` costs `b_n = 0` afterwards, so once you have cut tree `n` everything else is free; the DP finds the cheapest way to fully cut tree `n` using intermediate trees as charge levels. Lines have slope `b_j` (decreasing), queries at `a_i` (increasing) → deque CHT, `O(n)`. Products need `long long`; the intersection test `__int128` or `long double`.</details>

### 09.32  The Fair Nut and Rectangles  ·  Codeforces 1083E  ·  ★4 (CF 2400)
https://codeforces.com/contest/1083/problem/E
**Technique:** CHT after sorting rectangles by `x`.
<details><summary>Hint</summary>Sorted by `x` (hence by `y` decreasing, since no rectangle is nested), the union area of a chosen subset telescopes: `x_i · y_i − x_{prev} · y_i`.</details>
<details><summary>Approach sketch</summary>`dp[i] = x_i·y_i − a_i + max_j (dp[j] − x_j · y_i)`: lines slope `−x_j` (decreasing), queried at `y_i` (decreasing) — monotone deque for max. `O(n log n)` from the sort. Values up to `1e18` — careful with intersection arithmetic.</details>

### 09.33  ORDERSET — Order statistic set  ·  SPOJ  ·  ★3
https://www.spoj.com/problems/ORDERSET/
**Technique:** the raw order-statistics set: insert, delete, k-th, count-less.
<details><summary>Hint</summary>This is `Treap`/`ordered_set` with no twist; use it to benchmark your treap against pbds.</details>
<details><summary>Approach sketch</summary>Map `I x` → insert if absent, `D x` → erase if present, `K k` → `kth(k−1)` or "invalid", `C x` → `countLess(x)`. `O(q log q)`. Submit once with pbds, once with the treap, compare running times.</details>

### 09.34  Practice more
- Codeforces problemset, tag `data structures`, rating 2100-2400, filter mentally for "treap"/"implicit treap" (statements with cut/paste, reverse, insert-at-position).
- Codeforces problemset, tag `data structures` + `dp`, rating 2000-2400, for convex hull trick and Li Chao.
- CSES Range Queries section: redo *Salary Queries*, *List Removals*, *Distinct Values Queries* with the *other* method (online structure vs offline Fenwick).

---

## Progress

- [ ] 09.1 Concert Tickets (CSES 1091)
- [ ] 09.2 Towers (CSES 1073)
- [ ] 09.3 Traffic Lights (CSES 1163)
- [ ] 09.4 Room Allocation (CSES 1164)
- [ ] 09.5 Movie Festival II (CSES 1632)
- [ ] 09.6 Sliding Window Distinct Values (CSES 3222)
- [ ] 09.7 Nested Ranges Check (CSES 2168)
- [ ] 09.8 Nested Ranges Count (CSES 2169)
- [ ] 09.9 Josephus Problem II (CSES 2163)
- [ ] 09.10 List Removals (CSES 1749)
- [ ] 09.11 Salary Queries (CSES 1144)
- [ ] 09.12 Distinct Values Queries (CSES 1734)
- [ ] 09.13 Distinct Values Queries II (CSES 3356)
- [ ] 09.14 Sliding Window Median (CSES 1076)
- [ ] 09.15 Sliding Window Cost (CSES 1077)
- [ ] 09.16 Programmers and Artists (CSES 2426)
- [ ] 09.17 Cut and Paste (CSES 2072)
- [ ] 09.18 Substring Reversals (CSES 2073)
- [ ] 09.19 Reversals and Sums (CSES 2074)
- [ ] 09.20 Reversal Sorting (CSES 2075)
- [ ] 09.21 Monster Game I (CSES 2084)
- [ ] 09.22 Monster Game II (CSES 2085)
- [ ] 09.23 Lines and Queries I (CSES 3429)
- [ ] 09.24 Lines and Queries II (CSES 3430)
- [ ] 09.25 Range Interval Queries (CSES 3163)
- [ ] 09.26 Hotel Queries (CSES 1143)
- [ ] 09.27 Physical Education Lessons (CF 915E)
- [ ] 09.28 Willem, Chtholly and Seniorious (CF 896C)
- [ ] 09.29 T-Shirts (CF 702F)
- [ ] 09.30 Escape Through Leaf (CF 932F)
- [ ] 09.31 Kalila and Dimna in the Logging Industry (CF 319C)
- [ ] 09.32 The Fair Nut and Rectangles (CF 1083E)
- [ ] 09.33 ORDERSET (SPOJ)
