# Chapter 02 — Problems

Backbone: the complete CSES **Sorting and Searching** section (all 35 tasks), preceded by the
complete-search tasks of the Introductory section and two heavier search tasks (Meet in the
Middle, Knight's Tour). This is the Datatähti qualifier level end to end; finishing it with
first-submission ACs is the exit condition for this chapter.

How to work them:

1. Re-type `example.cpp` from memory first (subset/permutation generators, N-queens, meet in the
   middle, `first_true`, ternary search, the greedy classics). Compile warning-free before
   opening any problem.
2. Each problem goes in `competitive_programming/02_complete_search_and_backtracking/solutions/<source>_<id>.cpp`
   (e.g. `cses_1629.cpp`). Time yourself: ★1–2 ≤ 15 min, ★3 ≤ 30 min, ★4 ≤ 60 min.
3. For every greedy and every binary-search-on-the-answer problem, **write the exchange argument
   or the monotonicity sentence as a comment before coding**, then stress test against a brute
   force on n ≤ 8 (chapter 01 §11) before submitting.
4. Log every non-AC verdict with its cause. Upsolve within 48 hours: hint → approach → finish
   yourself; redo from a blank file a week later.

Difficulty: ★1 trivial once recognised, ★2 one idea, ★3 idea + careful implementation,
★4 two ideas or a subtle proof, ★5 hard for this level.

---

## A. Complete search and backtracking

### 02.1  Creating Strings  ·  CSES 1622  ·  ★1
https://cses.fi/problemset/task/1622
**Technique:** `next_permutation` over a sorted string (§2.2).
<details><summary>Hint</summary>Distinct arrangements of a multiset, in lexicographic order, each once — one standard function does exactly this.</details>
<details><summary>Approach sketch</summary>Sort the string, then `do { store } while (next_permutation(all(s)))`. Length ≤ 8 → ≤ 40320 strings. Print the count first. The only bug available is forgetting to sort.</details>

### 02.2  Apple Division  ·  CSES 1623  ·  ★2
https://cses.fi/problemset/task/1623
**Technique:** bitmask subset enumeration with O(1) incremental sums (§1.2).
<details><summary>Hint</summary>n ≤ 20. A mask fixes one group; the other is its complement.</details>
<details><summary>Approach sketch</summary>Compute `s[m] = s[m & (m−1)] + w[ctz(m)]` for all masks in O(2^n); answer = min over m of |total − 2·s[m]|. Weights up to 10^9 × 20 → `long long`. The recursive skip/take version with a running sum is equally fine.</details>

### 02.3  Gray Code  ·  CSES 2205  ·  ★2
https://cses.fi/problemset/task/2205
**Technique:** Gray code order of subsets (§1.3).
<details><summary>Hint</summary>`i ^ (i >> 1)`.</details>
<details><summary>Approach sketch</summary>For i = 0..2^n−1 print `i ^ (i >> 1)` as an n-bit binary string. Prove (or verify by brute force) that consecutive values differ in exactly one bit — the proof is in the lesson. Build the output in one string.</details>

### 02.4  Chessboard and Queens  ·  CSES 1624  ·  ★2
https://cses.fi/problemset/task/1624
**Technique:** backtracking column by column, O(1) attack checks via row and diagonal arrays (§3.1).
<details><summary>Hint</summary>Diagonals are indexed by x + y and x − y + 7. Blocked squares are just one more `continue`.</details>
<details><summary>Approach sketch</summary>Recurse over columns; for each free row not on a used diagonal and not reserved, mark, recurse, unmark. Count placements reaching column 8. Unblocked board gives 92 — use it as a test.</details>

### 02.5  Grid Path Description  ·  CSES 1625  ·  ★4
https://cses.fi/problemset/task/1625
**Technique:** backtracking with dead-end and split pruning (§3.3).
<details><summary>Hint</summary>Add pruning rules one at a time and print the number of recursive calls under `#ifdef LOCAL`: (1) target reached before 48 steps → stop; (2) cannot move straight but both left and right are free → the free region is split → stop. Fixed letters in the string already cut 3 of 4 branches.</details>
<details><summary>Approach sketch</summary>DFS on a 7×7 grid with a 1-cell visited wall around it; at step i, if the description has a letter, only that direction is tried, otherwise all four. Before branching, apply the split rule using the current direction ("forward blocked, left and right open"). The symmetry doubling from the corner-to-opposite-corner version does not apply here (the target is the lower-left corner and the string breaks symmetry). With rules 1–2 the search runs in well under a second.</details>

### 02.6  Meet in the Middle  ·  CSES 1628  ·  ★3
https://cses.fi/problemset/task/1628
**Technique:** meet in the middle over two halves of a 40-element set (§4).
<details><summary>Hint</summary>2^40 is impossible, 2^20 is a million. Every subset splits uniquely into a left part and a right part.</details>
<details><summary>Approach sketch</summary>Enumerate all subset sums of the first 20 and of the last 20 elements (O(2^20) each via the lowbit trick). Sort the right sums; for each left sum a count occurrences of x − a with `equal_range` (duplicates are common and must all be counted). Sums reach 4·10^10 and the count can reach 2^40: `long long`. ~2^20 · 20 operations for the sort.</details>

### 02.7  Knight's Tour  ·  CSES 1689  ·  ★4
https://cses.fi/problemset/task/1689
**Technique:** backtracking with value ordering (Warnsdorff's heuristic) (§3.4).
<details><summary>Hint</summary>Plain DFS over 64 squares does not finish. Order the candidate moves by the number of onward moves from the destination, fewest first.</details>
<details><summary>Approach sketch</summary>From the current square, compute for each legal knight move the degree (count of unvisited squares reachable from there); try moves in increasing degree order, recursing; undo on failure. With this ordering the first path found is almost always a full tour with no backtracking at all — the "most constrained first" principle applied to values instead of variables. Output the visit order as a grid.</details>

---

## B. Sorting, sweeps and ordered structures

### 02.8  Distinct Numbers  ·  CSES 1621  ·  ★1
https://cses.fi/problemset/task/1621
**Technique:** sort + count adjacent changes.
<details><summary>Hint</summary>After sorting, equal values are neighbours.</details>
<details><summary>Approach sketch</summary>Sort and count positions i where a[i] ≠ a[i−1], plus one. Or `set`/`unordered_set` (with a custom hash — chapter 01 §6). n = 2·10^5 → fast I/O.</details>

### 02.9  Apartments  ·  CSES 1084  ·  ★2
https://cses.fi/problemset/task/1084
**Technique:** sort both lists + two pointers; greedy matching.
<details><summary>Hint</summary>Give the smallest applicant the smallest apartment that fits them. Why is that never worse?</details>
<details><summary>Approach sketch</summary>Sort applicants and apartments. Pointer i over applicants, j over apartments: if apartment j < a[i] − k, j++; else if apartment j > a[i] + k, i++ (this applicant gets nothing); else match, i++, j++. Exchange argument: any matching can be rearranged so the smallest applicant takes the smallest apartment acceptable to them without losing a match.</details>

### 02.10  Ferris Wheel  ·  CSES 1090  ·  ★2
https://cses.fi/problemset/task/1090
**Technique:** sort + two pointers, lightest with heaviest.
<details><summary>Hint</summary>The heaviest child either rides alone or with the lightest one.</details>
<details><summary>Approach sketch</summary>Sort; `l = 0, r = n−1`; if w[l] + w[r] ≤ x pair them (l++, r−−) else r rides alone (r−−); count gondolas. Exchange: if the heaviest can share with anyone, it can share with the lightest, and pairing them leaves the most flexible remainder.</details>

### 02.11  Concert Tickets  ·  CSES 1091  ·  ★2
https://cses.fi/problemset/task/1091
**Technique:** `multiset` + `upper_bound`, greedy per customer in input order.
<details><summary>Hint</summary>Each customer takes the most expensive ticket they can afford; customers come in a fixed order, so there is no choice to optimise — only a fast lookup.</details>
<details><summary>Approach sketch</summary>Put prices in a `multiset`. For each customer, `it = upper_bound(max)`; if `it == begin()` print −1, else `--it`, print `*it`, `erase(it)` (erase the iterator, not the value — the value would erase all duplicates). O((n + m) log n).</details>

### 02.12  Restaurant Customers  ·  CSES 1619  ·  ★2
https://cses.fi/problemset/task/1619
**Technique:** event sweep.
<details><summary>Hint</summary>Turn each interval into a +1 event and a −1 event; sort events by time.</details>
<details><summary>Approach sketch</summary>Create 2n events (time, ±1), sort, scan keeping a running count and its maximum. All times are distinct in this task; in general decide the tie order from the statement (does leaving at t free a seat for someone arriving at t?).</details>

### 02.13  Stick Lengths  ·  CSES 1074  ·  ★2
https://cses.fi/problemset/task/1074
**Technique:** median minimises the sum of absolute deviations.
<details><summary>Hint</summary>Moving the target past one stick changes the cost by (#sticks on one side − #on the other). Where is that zero?</details>
<details><summary>Approach sketch</summary>Sort, take the middle element (either of the two middles for even n), sum |p_i − median|. Proof: the cost function is convex piecewise linear with slope (left count − right count), which changes sign at the median. Ternary search over the target also works (§7) — try it as an exercise. Sum up to 2·10^5 · 10^9: `long long`.</details>

### 02.14  Collecting Numbers  ·  CSES 2216  ·  ★2
https://cses.fi/problemset/task/2216
**Technique:** position array; count descents between consecutive values.
<details><summary>Hint</summary>A new round starts at value v exactly when v appears to the left of v − 1.</details>
<details><summary>Approach sketch</summary>pos[v] = index of value v. Answer = 1 + number of v in 2..n with pos[v] < pos[v−1]. O(n).</details>

### 02.15  Collecting Numbers II  ·  CSES 2217  ·  ★3
https://cses.fi/problemset/task/2217
**Technique:** maintain the descent count under swaps; local recomputation.
<details><summary>Hint</summary>A swap of positions changes only the pairs (v−1, v) and (v, v+1) for the two swapped values — at most four pairs.</details>
<details><summary>Approach sketch</summary>Keep `rounds` as in 02.14. For a swap of the values x and y: collect the affected value pairs {(x−1,x), (x,x+1), (y−1,y), (y,y+1)} as a set (dedupe — the pairs can coincide when |x − y| = 1), subtract their current contributions, perform the swap in both the array and `pos`, add the new contributions. O(1) per swap. Stress test against the O(n) recomputation.</details>

### 02.16  Nested Ranges Check  ·  CSES 2168  ·  ★3
https://cses.fi/problemset/task/2168
**Technique:** sort by (x ascending, y descending) + prefix/suffix extremes.
<details><summary>Hint</summary>After sorting by start ascending with ties by end descending, "range i contains some later range" ⇔ some later range has end ≤ end_i. "Range i is contained in some earlier range" ⇔ some earlier range has end ≥ end_i.</details>
<details><summary>Approach sketch</summary>Sort as in the hint (the tie rule makes equal-start ranges behave: the longer one comes first and contains the shorter). Scan from the right keeping the minimum end seen: i contains something iff minEnd_after ≤ y_i. Scan from the left keeping the maximum end: i is contained iff maxEnd_before ≥ y_i. Restore original order for output. O(n log n).</details>

### 02.17  Nested Ranges Count  ·  CSES 2169  ·  ★4
https://cses.fi/problemset/task/2169
**Technique:** same sweep, but counting → order statistics (Fenwick tree over compressed ends).
<details><summary>Hint</summary>Replace "is there a later range with end ≤ y_i" by "how many" — count elements ≤ y_i among ends inserted so far.</details>
<details><summary>Approach sketch</summary>Same sort. Compress the end coordinates. Sweep right-to-left: contains-count = number of already inserted ends ≤ y_i (Fenwick prefix query), then insert y_i. Sweep left-to-right: contained-count = number of inserted ends ≥ y_i. Fenwick/BIT is chapter 07 (`../07_range_queries`); a pbds `ordered_set` with `order_of_key` also works (chapter 01 §9). Do this problem now with whichever you can write; return after chapter 07.</details>

### 02.18  Traffic Lights  ·  CSES 1163  ·  ★3
https://cses.fi/problemset/task/1163
**Technique:** `set` of positions + `multiset` of gap lengths.
<details><summary>Hint</summary>Inserting a light at p splits exactly one gap into two.</details>
<details><summary>Approach sketch</summary>Positions set starts as {0, x}; gaps multiset {x}. For each new light p: find neighbours l = prev(lower_bound(p)), r = lower_bound(p); erase one copy of r − l from gaps, insert p − l and r − p; answer is `*gaps.rbegin()`. O(n log n). Erase by iterator (`gaps.find(r − l)`), not by value.</details>

### 02.19  Josephus Problem I  ·  CSES 2162  ·  ★2
https://cses.fi/problemset/task/2162
**Technique:** simulation with a queue.
<details><summary>Hint</summary>Skip one, remove one — a queue does both in O(1).</details>
<details><summary>Approach sketch</summary>Queue 1..n; repeat: pop front and push it back (skip), then pop front and print (remove). O(n) total. There is also an O(1)-per-answer formula, but the simulation is the point here.</details>

### 02.20  Josephus Problem II  ·  CSES 2163  ·  ★4
https://cses.fi/problemset/task/2163
**Technique:** order statistics: k-th remaining element (pbds `ordered_set` or Fenwick + binary search).
<details><summary>Hint</summary>Keep the current index i in the circle of remaining people; the next removed is at index (i + k) mod remaining. You need "the j-th smallest remaining element" fast.</details>
<details><summary>Approach sketch</summary>Maintain `idx = (idx + k) % size`, then remove the element with rank idx (0-indexed) among the remaining ones — `find_by_order(idx)` in a pbds tree (GCC only; chapter 01 §9), or a Fenwick tree of 1s with "find the smallest position whose prefix sum equals idx+1" (chapter 07). O(n log n). A plain `vector` erase is O(n²) = 4·10^10 — TLE.</details>

### 02.21  Distinct Values Subsequences  ·  CSES 3421  ·  ★2
https://cses.fi/problemset/task/3421
**Technique:** counting with a frequency map; product rule modulo 10^9+7.
<details><summary>Hint</summary>A subsequence with all distinct values chooses, for each value, either none or one of its occurrences.</details>
<details><summary>Approach sketch</summary>Sort (or hash-count) to get the multiplicity c_v of each value. Answer = Π(c_v + 1) − 1 modulo 10^9+7 (subtract the empty subsequence; add M before the final `%`). O(n log n).</details>

---

## C. Two pointers, sliding windows, prefix sums (pointers to algorithms_learning 02–04)

### 02.22  Sum of Two Values  ·  CSES 1640  ·  ★2
https://cses.fi/problemset/task/1640
**Technique:** sort with indices + two pointers (or a hash map).
<details><summary>Hint</summary>Sort pairs (value, original index). Opposite-end pointers move inward.</details>
<details><summary>Approach sketch</summary>Two pointers on the sorted pairs: sum < x → l++, sum > x → r−−, else print the two original indices (1-based!). If the pointers cross, `IMPOSSIBLE`. Alternative: scan with a map value → index, checking x − a[i] before inserting a[i] (handles the "two different positions" requirement automatically).</details>

### 02.23  Sum of Three Values  ·  CSES 1641  ·  ★3
https://cses.fi/problemset/task/1641
**Technique:** sort; fix one element, two pointers on the rest. O(n²), n ≤ 5000.
<details><summary>Hint</summary>n ≤ 5000 says O(n²). Fix the smallest index i; find a pair in (i, n) with the two-pointer scan.</details>
<details><summary>Approach sketch</summary>Sort (value, index). For each i, run two pointers l = i+1, r = n−1 for sum x − a[i]. 2.5·10^7 steps. Print original indices in any order.</details>

### 02.24  Sum of Four Values  ·  CSES 1642  ·  ★4
https://cses.fi/problemset/task/1642
**Technique:** meet in the middle over index pairs: pair sums in a hash map, disjointness by construction.
<details><summary>Hint</summary>n ≤ 1000 → O(n²) pairs. Store pair sums; but the four indices must be distinct — control which pairs are in the map when you query.</details>
<details><summary>Approach sketch</summary>Iterate j from 0..n−1; before processing j, insert all pairs (i, j−1) for i < j−1 into a map sum → (i, j−1). Then for each k > j, look up x − a[j] − a[k]: any stored pair has both indices < j < k, so all four are distinct. O(n²) map operations; use a custom hash or sort-based lookup to dodge anti-hash worst cases. Alternatively sort and do O(n²) two-pointer for each pair (i, j) — O(n³) at n = 1000 is too slow, so the map is necessary.</details>

### 02.25  Maximum Subarray Sum  ·  CSES 1643  ·  ★2
https://cses.fi/problemset/task/1643
**Technique:** Kadane / prefix-min. (`../../algorithms_learning/07` §1.)
<details><summary>Hint</summary>Best subarray ending at i = a[i] + max(0, best ending at i−1).</details>
<details><summary>Approach sketch</summary>One pass, `cur = max(a[i], cur + a[i])`, track the max. Initialise `best` with a[0], not 0 (all-negative arrays). Sums up to 2·10^14: `long long`. This is the fast side of the chapter-01 stress-test example.</details>

### 02.26  Maximum Subarray Sum II  ·  CSES 1644  ·  ★4
https://cses.fi/problemset/task/1644
**Technique:** prefix sums + sliding-window minimum (monotonic deque or `multiset`).
<details><summary>Hint</summary>sum(l..r) = P[r] − P[l−1]. For fixed r, l−1 ranges over [r−b, r−a]; you need the minimum P in a window that slides right by one each step.</details>
<details><summary>Approach sketch</summary>For r from a to n: add P[r−a] to the window, remove P[r−b−1] when it leaves; answer = max over r of P[r] − minWindow. A monotonic deque gives O(n); a `multiset` gives O(n log n), both fine for 2·10^5. `algorithms_learning/04` has the deque.</details>

### 02.27  Nearest Smaller Values  ·  CSES 1645  ·  ★2
https://cses.fi/problemset/task/1645
**Technique:** monotonic stack (`../../algorithms_learning/04_stack_and_monotonic_stack`).
<details><summary>Hint</summary>Pop while the top is ≥ a[i]; the remaining top is the answer; push i.</details>
<details><summary>Approach sketch</summary>Stack of indices with increasing values. Each index is pushed and popped at most once: O(n). Output 0 when the stack is empty. Positions are 1-based in the output.</details>

### 02.28  Playlist  ·  CSES 1141  ·  ★2
https://cses.fi/problemset/task/1141
**Technique:** variable-size sliding window with last-seen map (`algorithms_learning/02` §4).
<details><summary>Hint</summary>When a[r] was last seen at position p ≥ l, jump l to p + 1.</details>
<details><summary>Approach sketch</summary>Map value → last index. For each r: `l = max(l, last[a[r]] + 1)`, update last, answer = max(r − l + 1). Values up to 10^9 → `unordered_map` with custom hash, or compress by sorting first. O(n) / O(n log n).</details>

### 02.29  Distinct Values Subarrays  ·  CSES 3420  ·  ★2
https://cses.fi/problemset/task/3420
**Technique:** sliding window; count = Σ window lengths.
<details><summary>Hint</summary>The number of all-distinct subarrays ending at r equals the length of the longest all-distinct window ending at r.</details>
<details><summary>Approach sketch</summary>Same window as Playlist; add (r − l + 1) to the answer at each r. `long long` (up to n(n+1)/2 ≈ 2·10^10).</details>

### 02.30  Distinct Values Subarrays II  ·  CSES 2428  ·  ★3
https://cses.fi/problemset/task/2428
**Technique:** sliding window with a frequency map and a distinct counter ("at most k").
<details><summary>Hint</summary>Maintain the window [l, r] with ≤ k distinct values; every l' in [l, r] gives a valid subarray ending at r.</details>
<details><summary>Approach sketch</summary>Add a[r] to the counts (distinct++ if it was 0); while distinct > k remove a[l] (distinct−− if it hits 0), l++. Add r − l + 1. O(n) with compressed values or a custom-hash map. `long long` answer.</details>

### 02.31  Subarray Sums I  ·  CSES 1660  ·  ★2
https://cses.fi/problemset/task/1660
**Technique:** two pointers on positive values.
<details><summary>Hint</summary>All values are positive, so the window sum is monotone in both ends.</details>
<details><summary>Approach sketch</summary>Expand r, shrink l while sum > x, count when sum == x. Positivity is what makes this valid; with zeros or negatives use 02.32.</details>

### 02.32  Subarray Sums II  ·  CSES 1661  ·  ★3
https://cses.fi/problemset/task/1661
**Technique:** prefix sums + hash map of counts.
<details><summary>Hint</summary>sum(l..r) = x ⇔ P[l−1] = P[r] − x. Count earlier prefixes equal to that.</details>
<details><summary>Approach sketch</summary>Map prefix value → count, seeded with P[0] = 0 → 1. For each r add `cnt[P[r] − x]` to the answer then increment `cnt[P[r]]`. Negatives are fine. `long long` prefixes and answer; custom hash (2·10^5 keys, anti-hash tests exist on CSES too) or sort-and-count.</details>

### 02.33  Subarray Divisibility  ·  CSES 1662  ·  ★3
https://cses.fi/problemset/task/1662
**Technique:** prefix sums modulo n; count equal residues.
<details><summary>Hint</summary>sum(l..r) ≡ 0 (mod n) ⇔ P[l−1] ≡ P[r] (mod n). Residues live in [0, n): an array suffices.</details>
<details><summary>Approach sketch</summary>Count residues of prefix sums with `((P % n) + n) % n` (negatives!), starting with residue 0 counted once. Answer = Σ C(c_r, 2) over residues. O(n), `long long`.</details>

---

## D. Greedy with exchange arguments

### 02.34  Movie Festival  ·  CSES 1629  ·  ★2
https://cses.fi/problemset/task/1629
**Technique:** interval scheduling — sort by end (§10.1).
<details><summary>Hint</summary>Earliest finishing movie first. Write the one-sentence exchange argument in a comment.</details>
<details><summary>Approach sketch</summary>Sort by end; take a movie if its start ≥ last taken end (a movie may start exactly when the previous ends). Storing pairs as (end, start) lets the default `sort` order them. O(n log n).</details>

### 02.35  Missing Coin Sum  ·  CSES 2183  ·  ★2
https://cses.fi/problemset/task/2183
**Technique:** greedy "reach" invariant (§10.4).
<details><summary>Hint</summary>If every sum 0..R is makeable and the next smallest coin is c ≤ R + 1, then every sum 0..R + c is makeable.</details>
<details><summary>Approach sketch</summary>Sort; `reach = 0`; for each coin c: if c > reach + 1, answer is reach + 1; else reach += c. If the loop ends, answer reach + 1. O(n log n), `long long` (sum up to 2·10^14).</details>

### 02.36  Tasks and Deadlines  ·  CSES 1630  ·  ★2
https://cses.fi/problemset/task/1630
**Technique:** adjacent-swap argument → sort by duration (§10.2).
<details><summary>Hint</summary>Σ(deadline − finish) = Σdeadline − Σfinish. Only the second sum depends on the order.</details>
<details><summary>Approach sketch</summary>Sort by duration ascending, accumulate finishing times, sum d_i − f_i. Deadlines do not influence the order. Answer may be negative and up to ±10^14 in magnitude: `long long`. Stress test against all permutations for n ≤ 7 — it takes two minutes and builds the habit.</details>

### 02.37  Reading Books  ·  CSES 1631  ·  ★3
https://cses.fi/problemset/task/1631
**Technique:** two lower bounds, one of which is always achievable.
<details><summary>Hint</summary>Both readers must read every book: total ≥ Σt. The longest book must be read by both, one after the other: total ≥ 2·max. Show one of these bounds is always attainable.</details>
<details><summary>Approach sketch</summary>Answer = max(Σt, 2·t_max). If 2·t_max ≥ Σt, reader A reads the longest book first, then the rest; B reads the rest first, then the longest — no overlap and total 2·t_max. Otherwise, A reads in order 1..n and B reads the same cyclic order shifted by one book; with t_max ≤ Σ(others) they never read the same book at the same time and both finish at Σt. `long long`.</details>

### 02.38  Room Allocation  ·  CSES 1164  ·  ★3
https://cses.fi/problemset/task/1164
**Technique:** sweep by arrival + min-heap of (departure, room).
<details><summary>Hint</summary>Process customers by arrival. Reuse the room whose current guest leaves earliest — if that guest has already left.</details>
<details><summary>Approach sketch</summary>Sort by arrival (keep original indices). Min-heap keyed by departure. For each customer: if the heap top departs strictly before this arrival, pop it and reuse its room number; otherwise allocate a new room. Push (departure, room). The number of rooms equals the maximum overlap, so this is optimal; the greedy proof is that reusing any free room is never worse than opening a new one. O(n log n); output rooms in input order.</details>

### 02.39  Movie Festival II  ·  CSES 1632  ·  ★3
https://cses.fi/problemset/task/1632
**Technique:** sort by end + `multiset` of members' current end times; assign to the member who finished latest but still in time.
<details><summary>Hint</summary>Among members free before this movie starts, take the one whose last movie ended latest — it leaves the members with earlier ends available for movies that start earlier.</details>
<details><summary>Approach sketch</summary>Sort movies by end. Multiset holds each member's current end time (k copies of 0 initially). For a movie (s, e): `it = upper_bound(s)`; if `it == begin()` skip the movie; else `--it`, erase it, insert e, count++. Exchange argument: assigning to the latest-ending eligible member dominates any other assignment because the set of remaining free times is pointwise earlier. O(n log k).</details>

### 02.40  Towers  ·  CSES 1073  ·  ★3
https://cses.fi/problemset/task/1073
**Technique:** greedy with `multiset::upper_bound` — the patience-sorting view of LIS.
<details><summary>Hint</summary>Place each cube on the tower whose top is the smallest value strictly greater than the cube. If none, start a new tower.</details>
<details><summary>Approach sketch</summary>Multiset of tower tops. For cube c: `it = upper_bound(c)`; if it exists, erase it and insert c; else insert c (new tower). Exchange: choosing the smallest valid top keeps every larger top available for future cubes, so no future option is lost. The answer equals the length of the longest non-decreasing subsequence (patience sorting) — a fact you will reuse in the DP chapters. O(n log n).</details>

---

## E. Binary search on the answer

### 02.41  Factory Machines  ·  CSES 1620  ·  ★3
https://cses.fi/problemset/task/1620
**Technique:** binary search on time; monotone feasibility predicate (§8).
<details><summary>Hint</summary>"Can t products be made in T seconds?" = Σ⌊T / k_i⌋ ≥ t. More time never hurts.</details>
<details><summary>Approach sketch</summary>`first_true(1, min(k)·t, ok)`, hi ≤ 10^18 so `long long` and `mid = lo + (hi−lo)/2`. In `ok`, break as soon as the running count reaches t — with 2·10^5 machines and T up to 10^18 the sum otherwise overflows. 60 iterations × O(n).</details>

### 02.42  Array Division  ·  CSES 1085  ·  ★3
https://cses.fi/problemset/task/1085
**Technique:** binary search on the maximum part sum; greedy predicate (§8, worked example).
<details><summary>Hint</summary>"Can we split into ≤ k parts with each sum ≤ S?" is monotone in S, and a greedy left-to-right cut answers it in O(n).</details>
<details><summary>Approach sketch</summary>lo = max(a), hi = Σa. Predicate: scan, start a new part whenever adding the element would exceed S; feasible iff parts ≤ k. Cutting as late as possible is optimal for the predicate by an exchange argument. O(n log Σa), `long long`. Brute-force the cut positions for n ≤ 10 to stress test.</details>

---

Practice more: Codeforces problemset, tag `brute force`, rating 1000–1500 (enumeration with a
clever bound is the whole problem).
Practice more: Codeforces problemset, tag `binary search`, rating 1300–1800 — for each one, write
the monotonicity sentence before coding.
Practice more: Codeforces problemset, tag `greedy` + `sortings`, rating 1200–1700 — stress test
every greedy against a brute force before submitting; count how many wrong greedies you catch.
Practice more: Codeforces problemset, tag `meet-in-the-middle`, rating 1600–2100.
Practice more: Codeforces problemset, tag `ternary search`, rating 1700–2100.
Practice more: AtCoder ABC problems D–E tagged with bit-DP / all-subsets enumeration (the
`2^N` constraint is stated openly in ABC statements).

---

## Progress

- [ ] 02.1 Creating Strings (CSES 1622)
- [ ] 02.2 Apple Division (CSES 1623)
- [ ] 02.3 Gray Code (CSES 2205)
- [ ] 02.4 Chessboard and Queens (CSES 1624)
- [ ] 02.5 Grid Path Description (CSES 1625)
- [ ] 02.6 Meet in the Middle (CSES 1628)
- [ ] 02.7 Knight's Tour (CSES 1689)
- [ ] 02.8 Distinct Numbers (CSES 1621)
- [ ] 02.9 Apartments (CSES 1084)
- [ ] 02.10 Ferris Wheel (CSES 1090)
- [ ] 02.11 Concert Tickets (CSES 1091)
- [ ] 02.12 Restaurant Customers (CSES 1619)
- [ ] 02.13 Stick Lengths (CSES 1074)
- [ ] 02.14 Collecting Numbers (CSES 2216)
- [ ] 02.15 Collecting Numbers II (CSES 2217)
- [ ] 02.16 Nested Ranges Check (CSES 2168)
- [ ] 02.17 Nested Ranges Count (CSES 2169)
- [ ] 02.18 Traffic Lights (CSES 1163)
- [ ] 02.19 Josephus Problem I (CSES 2162)
- [ ] 02.20 Josephus Problem II (CSES 2163)
- [ ] 02.21 Distinct Values Subsequences (CSES 3421)
- [ ] 02.22 Sum of Two Values (CSES 1640)
- [ ] 02.23 Sum of Three Values (CSES 1641)
- [ ] 02.24 Sum of Four Values (CSES 1642)
- [ ] 02.25 Maximum Subarray Sum (CSES 1643)
- [ ] 02.26 Maximum Subarray Sum II (CSES 1644)
- [ ] 02.27 Nearest Smaller Values (CSES 1645)
- [ ] 02.28 Playlist (CSES 1141)
- [ ] 02.29 Distinct Values Subarrays (CSES 3420)
- [ ] 02.30 Distinct Values Subarrays II (CSES 2428)
- [ ] 02.31 Subarray Sums I (CSES 1660)
- [ ] 02.32 Subarray Sums II (CSES 1661)
- [ ] 02.33 Subarray Divisibility (CSES 1662)
- [ ] 02.34 Movie Festival (CSES 1629)
- [ ] 02.35 Missing Coin Sum (CSES 2183)
- [ ] 02.36 Tasks and Deadlines (CSES 1630)
- [ ] 02.37 Reading Books (CSES 1631)
- [ ] 02.38 Room Allocation (CSES 1164)
- [ ] 02.39 Movie Festival II (CSES 1632)
- [ ] 02.40 Towers (CSES 1073)
- [ ] 02.41 Factory Machines (CSES 1620)
- [ ] 02.42 Array Division (CSES 1085)
- [ ] Practice-more sets: 10 brute force, 10 binary search, 10 greedy (stress-tested), 3 meet-in-the-middle
