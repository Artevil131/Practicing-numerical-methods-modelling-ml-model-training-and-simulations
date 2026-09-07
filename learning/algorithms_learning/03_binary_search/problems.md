# Chapter 03 — Problems

How to work these:

1. Read the matching unit section in `lesson.md` first. Decide, before coding, which interval discipline you are using (closed or half-open) and what the predicate `P(mid)` is.
2. Attempt the problem in C in `algorithms_learning/03_binary_search/solutions/<slug>.c` (create the `solutions/` folder yourself). Write your own tests in `main()` — including the empty array, a one-element array, target smaller than everything, target larger than everything, and duplicates. Compile with `cc -Wall -Wextra -std=c11 -O2 -fsanitize=address,undefined`.
3. Only if stuck, open **Hint**.
4. Then **Key idea & complexity** to check your plan.
5. Open **Approach** only after solving, or after 30 minutes stuck. It is a write-up, not code — you still write the code.

Levels are 1-5 (difficulty within this course, not LeetCode's label).

---

## Unit 1 — Plain sorted search

### 03.1.1  Binary Search  ·  LC #704  ·  Easy  ·  array, binary-search
<https://leetcode.com/problems/binary-search/>
**Level:** 2
<details><summary>Hint</summary>
Keep the invariant that the answer, if it exists, is always inside [lo, hi], and shrink the interval strictly on every iteration.
</details>
<details><summary>Key idea & complexity</summary>
Standard binary search on the sorted array, O(log n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Invariant: if the answer exists, it is always between array indices `lo` and `hi` (closed interval `[lo, hi]`, condition `lo <= hi`). Each iteration computes `mid = lo + (hi - lo) / 2` and compares `nums[mid]` with the target: if equal, done; if smaller, the whole left side including `mid` is excluded, so `lo = mid + 1`; otherwise `hi = mid - 1`. In both branches the interval shrinks strictly by at least one element, so the loop is guaranteed to terminate in at most O(log n) rounds. The loop ends either with a hit or when `lo > hi`, at which point the interval is empty and the target is absent. The most common bug is forgetting the +1/-1 on the bounds and re-including the element just examined — then the algorithm can loop forever between two adjacent indices.
</details>

### 03.1.2  Search Insert Position  ·  LC #35  ·  Easy  ·  array, binary-search
<https://leetcode.com/problems/search-insert-position/>
**Level:** 2
<details><summary>Hint</summary>
Find the smallest index where nums[i] >= target — it is both the hit and the insertion point.
</details>
<details><summary>Key idea & complexity</summary>
Lower bound binary search, O(log n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Use the half-open form: `lo = 0`, `hi = n`, `while (lo < hi)`. Invariant: every index below `lo` is strictly smaller than `target`, every index from `hi` onward is `>= target` (or outside the array). When `nums[mid] < target`, `lo = mid + 1`; otherwise `hi = mid` — note that you never do `hi = mid - 1`, because `mid` may itself be the answer and must not be excluded. The loop ends when `lo == hi`, and that shared value is the smallest index with `nums[i] >= target`; if target is larger than every element the result is `n`. The same function solves both the search and the insertion point, because the insertion point is by definition the first position where target can be placed without breaking the order.
</details>

### 03.1.3  First Bad Version  ·  LC #278  ·  Easy  ·  binary-search, interactive
<https://leetcode.com/problems/first-bad-version/>
**Level:** 2
<details><summary>Hint</summary>
The predicate isBadVersion is monotone: once it becomes true it stays true — find the first true.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the boolean predicate isBadVersion, O(log n) calls.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The task is a pure boundary search in a monotone 0/1 sequence, even though the sequence is never built explicitly: the call is expensive, so it is made only O(log n) times. Use the half-open interval `[1, n]` → `[lo, hi]` such that `lo` is always known-good (or zero) and `hi` is known-bad or sitting on the boundary. `while (lo < hi)`: if `isBadVersion(mid)` is true, the first bad version is `mid` or earlier, so `hi = mid`; otherwise `lo = mid + 1`. The loop ends when `lo == hi`, which is the first bad version. Because `mid` is never wrongly excluded from the interval (`hi = mid`, not `mid - 1`), the algorithm can neither skip the answer nor get stuck, even when n = 1.
</details>

### 03.1.4  Guess Number Higher or Lower  ·  LC #374  ·  Easy  ·  binary-search, interactive
<https://leetcode.com/problems/guess-number-higher-or-lower/>
**Level:** 2
<details><summary>Hint</summary>
guess(mid) tells you directly which way to move the bound — use it for a closed-interval binary search.
</details>
<details><summary>Key idea & complexity</summary>
Interactive binary search using the three-way guess() feedback, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Here the guess() function returns three possible values, so the closed interval `[lo, hi]` with condition `lo <= hi` works directly: if `guess(mid) == 0`, done; if `guess(mid) == -1` (guess too high), `hi = mid - 1`; if `1` (too low), `lo = mid + 1`. Because both branches move the bound strictly past `mid`, the interval shrinks every iteration and an infinite loop is impossible. The problem is essentially the same as the basic binary-search problem but without direct access to an array — it teaches that binary search does not need an array, only a monotone comparison function.
</details>

### 03.1.5  Valid Perfect Square  ·  LC #367  ·  Easy  ·  math, binary-search
<https://leetcode.com/problems/valid-perfect-square/>
**Level:** 2
<details><summary>Hint</summary>
Find the largest i with i*i <= num, and check whether it hit exactly.
</details>
<details><summary>Key idea & complexity</summary>
Binary search for the integer square root, O(log n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Search for the largest integer `r` with `r*r <= num` on the closed interval `[1, num]`. The predicate `i*i <= num` is monotone: it holds for small `i` and stops holding at some point, so binary search applies. When the interval converges, `num` is a perfect square exactly when the found bound satisfies `r*r == num`. Remember to use a 64-bit type for the multiplication, because `num` can be close to 2^31 and `mid*mid` would overflow a 32-bit integer — this is the most common bug in the problem, not the search logic itself.
</details>

### 03.1.6  Sqrt(x)  ·  LC #69  ·  Easy  ·  math, binary-search, newtons-method
<https://leetcode.com/problems/sqrtx/>
**Level:** 2
<details><summary>Hint</summary>
Find the largest i with i*i <= x — the answer always rounds down.
</details>
<details><summary>Key idea & complexity</summary>
Binary search for the integer square root, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same structure as the previous problem, but now you return the rounded-down bound rather than testing for exactness: find the largest `r` with `r*r <= x` on the closed interval `[0, x]` (or a tighter upper bound, e.g. 2^16, since sqrt(x) <= 2^16 when x < 2^31). When `lo > hi`, `hi` is the answer, because it is the last value that still satisfied the predicate before the bound moved past it. The same overflow warning applies: compute `mid*mid` in a wider type, or alternatively compare `mid` against `x / mid` using division.
</details>

### 03.1.7  Arranging Coins  ·  LC #441  ·  Easy  ·  math, binary-search
<https://leetcode.com/problems/arranging-coins/>
**Level:** 2
<details><summary>Hint</summary>
The row-sum formula k(k+1)/2 is monotone in k, so find the largest k that fits.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the answer using the closed-form triangular sum, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Row `k` requires `k(k+1)/2` coins in total, and this sum grows strictly with `k`, so the predicate `k(k+1)/2 <= n` is monotone and binary search finds the largest valid `k` on the closed interval `[0, n]`. This is the first problem in the series where the binary search is not over an array directly but over the monotonicity of a **computed value** — the same idea the later "search on the answer" problems use more broadly. Use 64-bit arithmetic, because `n` can be large and `k(k+1)` would overflow 32 bits.
</details>

### 03.1.8  Find Smallest Letter Greater Than Target  ·  LC #744  ·  Easy  ·  array, binary-search
<https://leetcode.com/problems/find-smallest-letter-greater-than-target/>
**Level:** 2
<details><summary>Hint</summary>
Find the first letter that is strictly greater than target, and handle wrapping back to the start of the alphabet separately.
</details>
<details><summary>Key idea & complexity</summary>
Upper bound (strict) binary search with wraparound, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Find the smallest index with `letters[i] > target` (note strictly greater, not `>=` — this is a different boundary from search-insert-position). Half-open interval `[0, n)`: when `letters[mid] <= target`, `lo = mid + 1`, otherwise `hi = mid`. If the search ends with `lo == n`, i.e. no letter was greater, the answer wraps around to the first letter `letters[0]` — modular arithmetic `letters[lo % n]` handles this cleanly without a separate special case. This problem is a good test of whether you have the "strictly greater than" boundary under control as opposed to the "at least" boundary, since many people confuse the two.
</details>

---

## Unit 2 — Lower/upper bound and the boundary discipline

### 03.2.1  Find First and Last Position of Element in Sorted Array  ·  LC #34  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/find-first-and-last-position-of-element-in-sorted-array/>
**Level:** 3
<details><summary>Hint</summary>
First find the first occurrence with a lower-bound search, then the last by finding the first index that is greater than target.
</details>
<details><summary>Key idea & complexity</summary>
Two binary searches — lower_bound(target) and lower_bound(target+1), O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Run two separate lower-bound searches with the same skeleton: the first finds the smallest index with `nums[i] >= target` (if this does not land on target, the target is absent). The second finds the smallest index with `nums[i] >= target + 1`, i.e. strictly greater than target — subtract one from it to get the last occurrence. Both searches use exactly the same half-open skeleton, only the compared value changes; this is a good demonstration that "find first" and "find last" are not two different algorithms but the same skeleton with two different predicates.
</details>

### 03.2.2  Successful Pairs of Spells and Potions  ·  LC #2300  ·  Medium  ·  array, two-pointers, binary-search, sorting
<https://leetcode.com/problems/successful-pairs-of-spells-and-potions/>
**Level:** 3
<details><summary>Hint</summary>
Once potions is sorted, for each spell it suffices to find the smallest potion that satisfies the product condition.
</details>
<details><summary>Key idea & complexity</summary>
Sort potions, then lower_bound per spell for the minimum viable potion, O((n+m) log m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort `potions`. For each spell you need the smallest `potion[j]` with `spell[i] * potion[j] >= success`, i.e. `potion[j] >= success / spell[i]` (use integer arithmetic rounding up, or compare the product in 64-bit integers to avoid overflow). This is directly a lower-bound search in the sorted potions array, once per spell, so the total time is O(m log m + n log m). The number of successful pairs is `m - idx`, where `idx` is the found boundary. The most common bug is computing the threshold in floating point, which produces rounding errors near the boundary — keep the whole computation in integers.
</details>

### 03.2.3  Find K Closest Elements  ·  LC #658  ·  Medium  ·  array, two-pointers, binary-search, sliding-window
<https://leetcode.com/problems/find-k-closest-elements/>
**Level:** 3
<details><summary>Hint</summary>
Find the index of the window's left edge with binary search by comparing the two ends of the window against each other.
</details>
<details><summary>Key idea & complexity</summary>
Binary search for the left edge of a size-k window, O(log(n-k) + k).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Instead of searching directly for the closest element, binary search for the left edge of the window `[lo, lo+k]` on the interval `[0, n-k)`. Predicate: the window is too far right if `x - arr[mid] > arr[mid+k] - x`, in which case the right edge is closer and the window must move right (`lo = mid + 1`); otherwise the left edge is equally good or better, so `hi = mid`. This is the lower-bound skeleton where the predicate compares two distances just outside the window rather than a single value directly — good practice for the fact that the predicate does not have to be a trivial `nums[mid] >= target`, as long as it is monotone.
</details>

### 03.2.4  Random Pick with Weight  ·  LC #528  ·  Medium  ·  array, math, binary-search, prefix-sum
<https://leetcode.com/problems/random-pick-with-weight/>
**Level:** 3
<details><summary>Hint</summary>
Convert the weights into prefix sums and find the first prefix sum that exceeds the random number.
</details>
<details><summary>Key idea & complexity</summary>
Prefix sums plus upper_bound binary search, O(log n) per query after O(n) preprocessing.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Build the cumulative prefix sum of the weights. One draw `r` from `[1, total]` always corresponds to exactly one index: the index whose prefix sum is the first one that is `>= r`. This is an upper-bound-style search (the first cumulative sum that is strictly large enough), implemented with the same `[lo, hi)` skeleton as the other boundary searches. Because the prefix sums are strictly increasing (weight > 0), the predicate is monotone and ties need no special handling. Preprocessing takes O(n), each pickIndex() call O(log n).
</details>

### 03.2.5  Time Based Key-Value Store  ·  LC #981  ·  Medium  ·  hash-table, string, binary-search, design
<https://leetcode.com/problems/time-based-key-value-store/>
**Level:** 3
<details><summary>Hint</summary>
Store each key's values in time order and find the largest timestamp that is at most the queried time.
</details>
<details><summary>Key idea & complexity</summary>
Per-key list of (timestamp, value) pairs, lower_bound on timestamp, O(log n) per get.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because set calls always arrive in increasing time order within a key, each key's values can be stored in a list that is already sorted by timestamp — no separate sort is needed. get(key, timestamp) binary searches for the largest stored timestamp that is `<= timestamp`: this is a lower-bound search in the reverse direction (find the first strictly greater and take the one before it). If no stored timestamp is small enough, return the empty string. The problem combines a design task (hash map + list per key — see `../../c_learning/10_data_structures/lesson.md` for both) with a boundary search, and is a good bridge to the later stack-based design section.
</details>

### 03.2.6  Single Element in a Sorted Array  ·  LC #540  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/single-element-in-a-sorted-array/>
**Level:** 3
<details><summary>Hint</summary>
Look at even indices: before the single element, each pair starts at an even index; after it, at an odd one.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on index parity, O(log n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Every number except one appears twice, adjacently, so before the single element pairs start at even indices (`nums[2i] == nums[2i+1]`), and after the single element this breaks — from then on pairs start at odd indices. This is the invariant the binary search uses: force `mid` to be even (`mid -= mid % 2`), and if `nums[mid] == nums[mid+1]`, the single element is still ahead (`lo = mid + 2`); otherwise it is at `mid` or before it (`hi = mid`). The closed invariant keeps the single element inside `[lo, hi]` at all times, and the interval shrinks by at least two per iteration, so the loop is guaranteed to terminate.
</details>

### 03.2.7  Find Peak Element  ·  LC #162  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/find-peak-element/>
**Level:** 3
<details><summary>Hint</summary>
Compare nums[mid] and nums[mid+1]: if the sequence is rising, a peak is to the right; otherwise to the left or at mid.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the slope direction, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The predicate "we are on the descending side", i.e. `nums[mid] > nums[mid+1]`, is monotone in the sense that once it is true, the function guarantees some peak is found to the left or at that position — the boundary conditions nums[-1] = nums[n] = -inf guarantee a solution always exists. Use the half-open interval `[0, n-1)`: if `nums[mid] > nums[mid+1]`, a peak is at `mid` or before it (`hi = mid`); otherwise it is at `mid+1` or after it (`lo = mid + 1`). The critical point is that the comparison `nums[mid+1]` is always safe when `hi = n-1`, because `mid < hi` in the half-open form. This problem generalises directly to the two-dimensional version in a later unit.
</details>

### 03.2.8  Median of Two Sorted Arrays  ·  LC #4  ·  Hard  ·  array, binary-search, divide-and-conquer
<https://leetcode.com/problems/median-of-two-sorted-arrays/>
**Level:** 4
<details><summary>Hint</summary>
Binary search for the cut point in the smaller array that splits both arrays together into two equal halves.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the partition point of the smaller array, O(log(min(m,n))).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The idea is a binary search not on an element's value but on the **partition point**: choose a cut `i` in the shorter array (length `m`), which forces the cut `j = (m+n+1)/2 - i` in the other array so that the left side always contains exactly half (or one more, for an odd total) of the elements. The predicate is monotone: too large an `i` makes the largest element of the left side (`A[i-1]`) greater than the smallest of the right side (`B[j]`), so `hi = i - 1`; too small an `i` does the opposite, `lo = i + 1`. When both `A[i-1] <= B[j]` and `B[j-1] <= A[i]` hold, the partition is correct and the median is read directly from these four boundary neighbours. Edge cases (i=0, i=m, empty sides) are handled by substituting +/-infinity for the missing neighbours.
</details>

---

## Unit 3 — Search in a rotated array

### 03.3.1  Check if Array Is Sorted and Rotated  ·  LC #1752  ·  Easy  ·  array
<https://leetcode.com/problems/check-if-array-is-sorted-and-rotated/>
**Level:** 2
<details><summary>Hint</summary>
Count how many times nums[i] > nums[i+1] (wrapping around) — a valid rotation allows at most one.
</details>
<details><summary>Key idea & complexity</summary>
Single linear pass counting descents, O(n) — the warm-up before real binary search on rotation.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This problem does not yet require binary search, but it forces you to understand exactly what "rotated sorted array" means before you start searching one in logarithmic time: walk through the array, wrapping from the last element back to the first, and count how many times the next element is smaller than the previous. A valid rotation (possibly zero rotation) produces at most one such "drop"; more than one drop means the array is not a rotation of any sorted array. This linear check is exactly the same reasoning the later binary searches use locally to choose which half is sorted.
</details>

### 03.3.2  Peak Index in a Mountain Array  ·  LC #852  ·  Medium  ·  array, binary-search, ternary-search
<https://leetcode.com/problems/peak-index-in-a-mountain-array/>
**Level:** 3
<details><summary>Hint</summary>
Compare arr[mid] and arr[mid+1]: a rising side means the peak is to the right.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the slope direction, same invariant as find-peak-element, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A mountain array guarantees a unique peak, so the same slope predicate as in find-peak-element works as is: half-open interval `[0, n-1)`, and if `arr[mid] < arr[mid+1]` we are still on the rising side, so the peak is strictly after mid (`lo = mid + 1`); otherwise we are at the peak or on the descending side (`hi = mid`). Because the problem guarantees exactly one peak and no equal neighbours, the invariant stays simpler than in the general find-peak-element version. This problem is a good bridge to the previous unit: the same "compare with the neighbour and infer the direction" reasoning is exactly what the rotated array's pivot search uses next.
</details>

### 03.3.3  Find Minimum in Rotated Sorted Array  ·  LC #153  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/find-minimum-in-rotated-sorted-array/>
**Level:** 3
<details><summary>Hint</summary>
Compare nums[mid] and nums[hi]: if mid is larger, the minimum is to the right; otherwise mid may itself be the minimum.
</details>
<details><summary>Key idea & complexity</summary>
Binary search comparing nums[mid] to nums[hi], O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Use the half-open interval `[0, n-1)`. If `nums[mid] > nums[hi]`, we are on the unbroken (larger) half and the minimum is strictly after mid, so `lo = mid + 1`. Otherwise `nums[mid] <= nums[hi]`, in which case `mid` itself may be the minimum and must not be excluded: `hi = mid`. Comparing against nums[hi] rather than nums[lo] is an important choice, because it directly yields a two-way, unambiguous predicate without a third case. The loop ends when `lo == hi`, which points at the rotation point, i.e. the smallest element.
</details>

### 03.3.4  Search in Rotated Sorted Array  ·  LC #33  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/search-in-rotated-sorted-array/>
**Level:** 3
<details><summary>Hint</summary>
First decide which half, [lo, mid] or [mid, hi], is sorted, then check whether target belongs to it.
</details>
<details><summary>Key idea & complexity</summary>
Binary search deciding which half is sorted, then checking if target lies in it, O(log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
On the closed interval `[lo, hi]`: if `nums[lo] <= nums[mid]`, the left half is sorted, and target belongs to it if `nums[lo] <= target < nums[mid]` — in that case `hi = mid - 1`, otherwise `lo = mid + 1`. Symmetrically, if the right half is sorted, check whether target lies in the range `(nums[mid], nums[hi]]`. Because at least one half is always fully sorted, this two-stage decision ("which is sorted" → "does target belong to it") suffices to halve the search interval every iteration, so the total time stays O(log n) as in ordinary binary search.
</details>

### 03.3.5  Search in Rotated Sorted Array II  ·  LC #81  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/search-in-rotated-sorted-array-ii/>
**Level:** 3
<details><summary>Hint</summary>
Add a special case to the previous solution: when nums[lo]==nums[mid]==nums[hi], shrink the interval from both ends.
</details>
<details><summary>Key idea & complexity</summary>
Same rotation logic with a duplicate-collapsing fallback, O(log n) average, O(n) worst case.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same two-half reasoning as in the duplicate-free version, but when `nums[lo] == nums[mid] == nums[hi]`, these three values no longer let you infer which half is sorted — in principle either could be. The only safe move is then `lo++; hi--;` and continue, which shrinks the interval by only one step at a time. This special case can in the worst case (e.g. all elements equal except one) drop the time complexity from O(log n) to O(n) — that is unavoidable, because duplicates make the problem fundamentally ambiguous without a linear check.
</details>

### 03.3.6  Find Minimum in Rotated Sorted Array II  ·  LC #154  ·  Hard  ·  array, binary-search
<https://leetcode.com/problems/find-minimum-in-rotated-sorted-array-ii/>
**Level:** 4
<details><summary>Hint</summary>
When nums[mid] equals nums[hi], you cannot tell which side contains the minimum — shrink hi by one.
</details>
<details><summary>Key idea & complexity</summary>
Same idea as the minimum search, plus hi-- when nums[mid]==nums[hi], O(log n) average, O(n) worst case.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The basic skeleton is the same as in the duplicate-free minimum search, but a third case is added: when `nums[mid] == nums[hi]`, you cannot tell whether the minimum is on the left or the right side (e.g. [3,3,1,3] vs [3,1,3,3] look identical from the middle), so the only safe move is `hi--` — it drops just one duplicate without affecting the correct answer. This is a different fix from the search problem (there, lo++/hi-- from both ends), because here only one pair of values is compared, not three. The worst case is again O(n) when nearly all elements are equal.
</details>

### 03.3.7  Find in Mountain Array  ·  LC #1095  ·  Hard  ·  array, binary-search, interactive, ternary-search
<https://leetcode.com/problems/find-in-mountain-array/>
**Level:** 4
<details><summary>Hint</summary>
First find the peak with binary search, then search the rising and the falling side separately with ordinary binary search.
</details>
<details><summary>Key idea & complexity</summary>
Three binary searches — find the peak, then search each monotonic slope, O(log n) with a limited number of get() calls.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Build the solution from three separate binary searches with the same skeleton as find-peak-element: first find the mountain's peak by comparing `get(mid)` and `get(mid+1)`. Then run an ordinary ascending binary search on `[0, peak]` and, if needed, a descending binary search (comparison reversed) on `[peak, n-1]`. Because get() calls are expensive and limited, each of these three searches must be written so that it never calls get() again needlessly for the same index — store the values you read in variables and do not call the same mid twice. The total time stays O(log n), because each of the three phases is an independent logarithmic search.
</details>

---

## Unit 4 — Binary search on the answer (minimise the maximum)

### 03.4.1  Koko Eating Bananas  ·  LC #875  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/koko-eating-bananas/>
**Level:** 3
<details><summary>Hint</summary>
Find the smallest speed k at which all piles can be eaten within h hours — hours per pile is ceil(pile/k).
</details>
<details><summary>Key idea & complexity</summary>
Binary search on eating speed with a greedy feasibility check, O(n log(max(piles))).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the speed `k` in `[1, max(piles)]`. feasible(k) computes the sum of `ceil(pile / k)` over every pile — this is the number of hours needed at speed `k`, and it is strictly decreasing as a function of `k`, so the predicate "do h hours suffice" is monotone. Binary search finds the smallest k that passes the check with the same `[lo, hi)` skeleton as the array searches. The check is O(n) per iteration, so the total time is O(n log(max(piles))). A typical bug is computing ceil incorrectly with integer division — the correct formula is `(pile + k - 1) / k`.
</details>

### 03.4.2  Minimum Number of Days to Make m Bouquets  ·  LC #1482  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/minimum-number-of-days-to-make-m-bouquets/>
**Level:** 3
<details><summary>Hint</summary>
Ask: if every flower with bloomDay <= day counts as bloomed, can you form m bouquets from adjacent flowers?
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the day, greedy left-to-right adjacent-flower grouping, O(n log(max(bloomDay))).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the day `day` in `[min(bloomDay), max(bloomDay)]`. feasible(day) walks the flowers left to right and greedily counts runs of adjacent bloomed flowers of length `k`: whenever `k` adjacent flowers have accumulated, form a bouquet and reset the counter; an unbloomed flower breaks the run. If at least `m` bouquets are formed, day is valid. The predicate is monotone because a later day can only add bloomed flowers, never remove them. Special case: if `m * k > n`, no answer exists at all — check this before the search.
</details>

### 03.4.3  Capacity To Ship Packages Within D Days  ·  LC #1011  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/capacity-to-ship-packages-within-d-days/>
**Level:** 3
<details><summary>Hint</summary>
Find the smallest capacity at which greedy packing (fill the ship until the next package does not fit) needs at most D days.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on ship capacity with a greedy day-packing check, O(n log(sum)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the capacity `cap` in `[max(weights), sum(weights)]` — the lower bound because even a single package must be carried whole, the upper bound because a single day could always carry everything. feasible(cap) simulates greedily: load packages into the day until the next one no longer fits, start a new day, and count the number of days needed. If at most `D` days are needed, cap is acceptable. Structurally this is identical to koko-eating-bananas even though the surface description differs — in both, a greedy simulation serves as the feasibility check for the binary search.
</details>

### 03.4.4  Find the Smallest Divisor Given a Threshold  ·  LC #1283  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/find-the-smallest-divisor-given-a-threshold/>
**Level:** 3
<details><summary>Hint</summary>
The same ceil-sum idea as the banana problem, but now you seek the smallest divisor that keeps the sum under the threshold.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the divisor, sum of ceil(x/d), O(n log(max(nums))).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Structurally the same problem as koko-eating-bananas, written in different language: find the smallest divisor `d` in `[1, max(nums)]` for which the sum of ceil(nums[i] / d) is at most threshold. The sum is strictly decreasing as d grows, so the predicate is monotone. The teaching value of this problem is precisely recognising the same pattern again under different wording — it is the best test of whether the "binary search on the answer" pattern has truly been internalised or only one problem memorised.
</details>

### 03.4.5  Minimum Time to Complete Trips  ·  LC #2187  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/minimum-time-to-complete-trips/>
**Level:** 3
<details><summary>Hint</summary>
A total time t gives each bus floor(t / time_i) trips — find the smallest t at which the sum suffices.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on total time, sum of floor(time/t_i), O(n log(max(time) * totalTrips)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the total elapsed time `t`. feasible(t) computes the sum of floor(t / time[i]) over all buses — how many trips each would complete in time `t` — and compares the sum to the required totalTrips. The sum grows monotonically as t grows, so binary search works. As the upper bound, `min(time) * totalTrips` suffices, because the fastest bus alone would finish within that time. This problem differs from the previous ones in that the predicate *increases* as x grows (rather than decreasing), so the binary search looks for the smallest value that **exceeds** the threshold, not one that stays below it — the direction must be flipped carefully.
</details>

### 03.4.6  Minimum Speed to Arrive on Time  ·  LC #1870  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/minimum-speed-to-arrive-on-time/>
**Level:** 3
<details><summary>Hint</summary>
Try a speed v: every leg except the last rounds up to whole hours, the last one does not.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on train speed, sum of travel times with ceiling on intermediate legs, O(n log(10^7)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the speed `v` in [1, 10^7] (the practical upper bound). feasible(v) computes the total time: every intermediate leg is rounded up to the next whole hour (because the train departs only on the hour), but the last leg is left unrounded, because on arrival there is no next departure to wait for. The time is monotonically decreasing in v. Special case: if the number of legs is larger than the integer part of hour allows (too much rounding), no answer exists — check this before the search by returning -1.
</details>

### 03.4.7  Magnetic Force Between Two Balls  ·  LC #1552  ·  Medium  ·  array, binary-search, sorting
<https://leetcode.com/problems/magnetic-force-between-two-balls/>
**Level:** 3
<details><summary>Hint</summary>
Now you seek the largest minimum distance, not the smallest — a greedy left-to-right placement counts how many balls fit.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the minimum gap, maximising it — greedy placement, O(n log n + n log(max gap)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the "mirror image" of the same family: the previous problems minimised the largest value, here you **maximise the smallest distance**. Sort the positions, and feasible(d) places balls greedily left to right: whenever the next position is at least `d` away from the previously placed ball, place one there. If the number of greedily placed balls is at least `m`, gap `d` is achievable. Because a larger `d` can only reduce the number of balls that fit, the predicate is monotonically decreasing, and the binary search looks for the **largest** valid d — the reverse direction compared to the earlier "minimise the maximum" problems, which is this problem's most important insight.
</details>

### 03.4.8  Split Array Largest Sum  ·  LC #410  ·  Hard  ·  array, binary-search, dynamic-programming, greedy
<https://leetcode.com/problems/split-array-largest-sum/>
**Level:** 4
<details><summary>Hint</summary>
Find the smallest ceiling S at which greedy splitting (collect elements until the next would exceed S) needs at most m parts.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the maximum subarray sum, greedy chunk splitting, O(n log(sum)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the unit's namesake problem: the search range is the ceiling `S` on the largest part sum, in `[max(nums), sum(nums)]`. feasible(S) splits the array greedily: collect elements into the current part until adding the next would exceed `S`, then start a new part, and count the number of parts needed. If at most `m` parts are needed, S is achievable. This is the identical pattern to capacity-to-ship-packages-within-d-days, presented without the "shipping" framing — it is deliberately the last problem of the unit so that you notice how many seemingly different phrasings are solved by exactly the same "binary search on the answer + greedy feasibility check" skeleton.
</details>

---

## Unit 5 — Search in a 2D matrix

### 03.5.1  Count Negative Numbers in a Sorted Matrix  ·  LC #1351  ·  Easy  ·  array, binary-search, matrix
<https://leetcode.com/problems/count-negative-numbers-in-a-sorted-matrix/>
**Level:** 2
<details><summary>Hint</summary>
Start from the top-right corner: when the value is negative, the rest of the row is negative too — move down and count.
</details>
<details><summary>Key idea & complexity</summary>
Staircase walk from the top-right corner, O(m+n) (or per-row upper_bound for O(m log n)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the rows are sorted in decreasing order left to right and the columns top to bottom, start from the top-right corner. If the current value is negative, everything to its right on the same row is also negative (add them to the counter), and move down a row; if the value is non-negative, move left in the column. Each step removes either a row or a column from consideration, so the total time is O(m+n). An alternative solution runs a separate upper-bound binary search on each row (O(m log n)) — this problem is a good bridge from array searches to matrix walking.
</details>

### 03.5.2  The K Weakest Rows in a Matrix  ·  LC #1337  ·  Easy  ·  array, binary-search, sorting, heap-priority-queue
<https://leetcode.com/problems/the-k-weakest-rows-in-a-matrix/>
**Level:** 2
<details><summary>Hint</summary>
Count the soldiers in each row with binary search (find the first 0), then order the rows by (count, index).
</details>
<details><summary>Key idea & complexity</summary>
Per-row lower_bound to count soldiers, then partial sort/heap on (count, index), O(m log n + m log m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Each row has the form `1,1,...,1,0,0,...,0`, so the number of soldiers is directly a lower-bound search: the smallest index whose value is `0` (or the full row length if the row is all ones). Compute this for each row independently in O(log n). Once each row's "weakness" (soldier count, row index) is known, the `k` weakest rows are obtained either by a full sort in O(m log m) or with a min-heap if `k` is much smaller than `m` (both from `../../c_learning/10_data_structures/lesson.md`). The problem combines a per-row boundary search with a selection problem — a good reminder that binary search is often just one component of a larger solution.
</details>

### 03.5.3  Search a 2D Matrix  ·  LC #74  ·  Medium  ·  array, binary-search, matrix
<https://leetcode.com/problems/search-a-2d-matrix/>
**Level:** 3
<details><summary>Hint</summary>
Because each row continues from the previous one, the whole matrix corresponds to one long sorted array — convert the index into a (row, column) pair.
</details>
<details><summary>Key idea & complexity</summary>
Treat the matrix as one flattened sorted array, O(log(mn)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the first element of each row is larger than the last element of the previous row, the whole matrix is in order if read as one long sequence row by row. Run an ordinary closed-interval binary search on the indices `[0, m*n-1]`, and convert each mid index into a row and column with `row = mid / n, col = mid % n`. The rest of the logic is identical to the basic binary-search problem — this is deliberately the easiest problem of the unit, because its only new idea is the index conversion, not new search logic.
</details>

### 03.5.4  Search a 2D Matrix II  ·  LC #240  ·  Medium  ·  array, binary-search, divide-and-conquer, matrix
<https://leetcode.com/problems/search-a-2d-matrix-ii/>
**Level:** 3
<details><summary>Hint</summary>
Start from the top-right corner: a value too large moves you left, too small moves you down — each step rules out a row or a column.
</details>
<details><summary>Key idea & complexity</summary>
Staircase walk from the top-right (or bottom-left) corner, O(m+n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Here the matrix is not globally sorted (only the rows and columns individually), so one-dimensional binary search does not apply directly. Start from the top-right corner: if the current value is greater than the target, it and its whole column downward are too large, so move left; if smaller, it and its whole row to the left are too small, so move down. Each step rules out either one whole row or one whole column, so the path length is at most `m+n`. The choice of starting corner is critical: the top-left or bottom-right corners do not work, because from those, moving in either direction can both increase and decrease the value.
</details>

### 03.5.5  Find a Peak Element II  ·  LC #1901  ·  Medium  ·  array, binary-search, matrix
<https://leetcode.com/problems/find-a-peak-element-ii/>
**Level:** 3
<details><summary>Hint</summary>
Binary search over columns: find the row with the maximum in each column, compare with the neighbouring columns, and move toward the rising direction as in the 1D peak.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on columns, scanning the row maximum per column, O(m log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Generalise find-peak-element to two dimensions by binary searching over columns. For each mid column, find its largest value (scan the whole column, O(m)), and compare it with the values in the left and right neighbouring columns on the same row. If the right neighbour is larger, the peak is to the right (`lo = mid + 1`); if the left is larger, the peak is to the left (`hi = mid - 1`); otherwise the current row maximum is a peak. Same reasoning as the 1D version (the edges behave like -inf), but each iteration costs O(m) because of the column scan, so the total time is O(m log n).
</details>

### 03.5.6  Kth Smallest Element in a Sorted Matrix  ·  LC #378  ·  Medium  ·  array, binary-search, sorting, heap-priority-queue
<https://leetcode.com/problems/kth-smallest-element-in-a-sorted-matrix/>
**Level:** 3
<details><summary>Hint</summary>
Binary search on the value: count by walking from a corner how many elements are <= mid, and find the smallest value at which the count reaches k.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the value, counting elements <= mid via staircase walk, O(n log(max-min)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the value in `[matrix[0][0], matrix[n-1][n-1]]`. countLessEqual(x) counts how many matrix elements are at most `x` using the same staircase walk as search-a-2d-matrix-ii: start from the bottom-left corner, and move right when the value is `<= x` (adding the whole column to the counter) or up when it is larger — O(n) per value. The binary search finds the smallest x for which countLessEqual(x) >= k; because the matrix values are not necessarily consecutive integers, the found boundary is always some actual matrix value, not an arbitrary number. The total time O(n log(max-min)) beats the heap-based O(k log n) when k is close to n^2.
</details>

### 03.5.7  Maximum Side Length of a Square with Sum Less than or Equal to Threshold  ·  LC #1292  ·  Medium  ·  array, binary-search, matrix, prefix-sum
<https://leetcode.com/problems/maximum-side-length-of-a-square-with-sum-less-than-or-equal-to-threshold/>
**Level:** 3
<details><summary>Hint</summary>
Build a 2D prefix sum, and binary search the side length: is there some square of that side whose sum is at most threshold?
</details>
<details><summary>Key idea & complexity</summary>
2D prefix sums plus binary search on the side length, O(nm log(min(n,m))).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Validity of a side length is monotone: if no square of that side fits under the threshold, no longer side does either, because the sums grow with the square's size (the values are positive). Precompute a 2D prefix sum in O(nm) so that the sum of any rectangle is available in O(1). feasible(k) iterates over all possible top-left corners of `k x k` squares and checks whether even one has a sum at most threshold — O(nm) per check. Binary search on the side length in `[0, min(n,m)]` brings the total to O(nm log(min(n,m))).
</details>

### 03.5.8  Find the Kth Smallest Sum of a Matrix With Sorted Rows  ·  LC #1439  ·  Hard  ·  array, binary-search, heap-priority-queue, matrix
<https://leetcode.com/problems/find-the-kth-smallest-sum-of-a-matrix-with-sorted-rows/>
**Level:** 4
<details><summary>Hint</summary>
Binary search on the sum value: count recursively how many combinations of row choices produce a sum at most mid.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the sum value, counting achievable sums <= mid via row-merging, O(nm log(range)) roughly.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the sum of possible per-row picks, in `[sum(min per row), sum(max per row)]`. The counting function merges rows pairwise: all pair sums of two rows can be collapsed into a new "row", whose size is kept bounded by pruning the worst candidates, so that in the end a single list of possible total sums remains. The feasibility check in the binary search counts how many produced sums are at most `mid`, and finds the smallest mid for which the count is at least `k`. This is the unit's hardest problem precisely because the counting function itself (not the binary search skeleton) is intricate — the binary search is routine once the feasibility check is built correctly.
</details>

---

## Unit 6 — Binary search plus greedy feasibility checks

### 03.6.1  Minimum Limit of Balls in a Bag  ·  LC #1760  ·  Medium  ·  array, binary-search
<https://leetcode.com/problems/minimum-limit-of-balls-in-a-bag/>
**Level:** 3
<details><summary>Hint</summary>
Try a limit m: each bag needs ceil(bag/m)-1 splits — sum them and compare to maxOperations.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the limit, greedy operation count via ceil-sum, O(n log(max)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the largest allowed bag size `m` in `[1, max(nums)]`. If a bag has size `size`, splitting it so that no part exceeds m requires exactly `ceil(size/m) - 1` splits (greedy, because an even split of the parts is always optimal). feasible(m) sums these over all bags and compares to maxOperations — a smaller `m` always needs at least as many splits as a larger one, so the predicate is monotone. This is essentially a warm-up resembling the "binary search on the answer" unit before the graph-based feasibility checks.
</details>

### 03.6.2  Maximum Value at a Given Index in a Bounded Array  ·  LC #1802  ·  Medium  ·  math, binary-search, greedy
<https://leetcode.com/problems/maximum-value-at-a-given-index-in-a-bounded-array/>
**Level:** 3
<details><summary>Hint</summary>
Try a peak value v: greedily compute the smallest sum that arises when values fall evenly in both directions from index.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the peak value, greedy triangular sum on both sides, O(n log(maxSum)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the value `v` of nums[index] in `[1, maxSum]`. The cheapest way to fill the other elements for a given peak value `v` is to let them decrease evenly by one step per unit of distance until they reach 1, after which the rest are filled with ones — this sum is computed with a closed formula (arithmetic series) separately for the left and right side in O(1), not with a loop. feasible(v) compares this minimum sum to maxSum: the larger `v`, the larger the minimum sum, so the predicate is monotone. Overflow is a real risk in this problem — use 64-bit arithmetic in the sum formula.
</details>

### 03.6.3  Path With Minimum Effort  ·  LC #1631  ·  Medium  ·  array, binary-search, depth-first-search, breadth-first-search
<https://leetcode.com/problems/path-with-minimum-effort/>
**Level:** 3
<details><summary>Hint</summary>
Try a threshold e: BFS/DFS from the top-left corner using only edges whose height difference is at most e — do you reach the goal?
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the effort threshold, BFS/DFS reachability check, O(nm log(max diff)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the largest allowed per-step height difference `e` in `[0, maxDiff]`. feasible(e) is a BFS or DFS on the grid from the start cell, moving to a neighbour only if the height difference is at most `e`; does the route reach the goal. A larger `e` admits more edges, so reachability is monotone — if a route succeeds for some e, it succeeds for every larger e too. An alternative solution would use Dijkstra directly, but binary search + BFS is conceptually simpler and teaches you to recognise the same "test a threshold, check reachability" pattern that recurs in many grid problems.
</details>

### 03.6.4  Find the Safest Path in a Grid  ·  LC #2812  ·  Medium  ·  array, binary-search, breadth-first-search, union-find
<https://leetcode.com/problems/find-the-safest-path-in-a-grid/>
**Level:** 3
<details><summary>Hint</summary>
First compute each cell's distance to the nearest thief with a multi-source BFS, then binary search the threshold and check reachability.
</details>
<details><summary>Key idea & complexity</summary>
Multi-source BFS for distance-to-thief, then binary search on the safety threshold with a reachability DFS/BFS, O(nm log(nm)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two-phase solution: first compute each cell's "safety value", i.e. its grid distance to the nearest thief, with a multi-source BFS (all thieves as simultaneous start points), O(nm). Then binary search the largest threshold `s` in `[0, nm]`, where feasible(s) is a BFS/DFS that uses only cells whose safety value is at least `s`, checking whether the start reaches the goal. A larger threshold means fewer usable cells, so reachability is monotonically decreasing in s — here the binary search looks for the **largest** valid threshold, not the smallest.
</details>

### 03.6.5  Swim in Rising Water  ·  LC #778  ·  Hard  ·  array, binary-search, depth-first-search, breadth-first-search
<https://leetcode.com/problems/swim-in-rising-water/>
**Level:** 4
<details><summary>Hint</summary>
Try a water level t: use only cells whose height is at most t, and check with BFS whether you can get from the top-left to the bottom-right corner.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the water level, BFS/DFS or union-find reachability, O(n^2 log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the water level `t` in [0, n^2 - 1] (all possible height values in the grid). feasible(t) is a BFS or DFS from (0,0) using only cells whose height is at most `t`, checking whether it reaches (n-1,n-1). A larger `t` floods more cells into use and never removes any, so reachability is strictly monotone. The union-find alternative adds edges in increasing height order and stops as soon as the start and goal become connected — it solves the problem without an explicit binary search, but binary search + BFS is more direct and generalises better to other "threshold + reachability" problems.
</details>

### 03.6.6  Last Day Where You Can Still Cross  ·  LC #1970  ·  Hard  ·  array, binary-search, depth-first-search, breadth-first-search
<https://leetcode.com/problems/last-day-where-you-can-still-cross/>
**Level:** 4
<details><summary>Hint</summary>
Try a day d: mark every cell whose flooding happens on or before day d, and check with BFS/DFS whether the top and bottom edges are still connected.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the day, grid-flood-fill reachability check top-to-bottom, O(nm log(nm)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The search range is the day `d` in `[1, nm]`. feasible(d) marks as water every cell whose flooding order is at most `d`, and checks with BFS or DFS whether the set of dry cells connects the top row to the bottom row. The later the day, the more water and the less likely an intact dry path, so feasibility is monotonically decreasing — the binary search looks for the **last** day on which a path still exists. Structurally this is the exact mirror image of swim-in-rising-water: there you sought the smallest threshold that enables a route, here the largest.
</details>

### 03.6.7  Escape the Spreading Fire  ·  LC #2258  ·  Hard  ·  array, binary-search, breadth-first-search, matrix
<https://leetcode.com/problems/escape-the-spreading-fire/>
**Level:** 4
<details><summary>Hint</summary>
First compute the fire's spread time to every cell with BFS, then binary search the wait time and check with the person's BFS whether the route stays alive.
</details>
<details><summary>Key idea & complexity</summary>
Binary search on the wait time, dual BFS (fire spread + person path with fire-arrival timestamps), O(nm log(nm)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Three-phase solution: first a BFS from the fire's origin computes each cell's ignition time. Then binary search the wait time `w` in `[0, nm]` (plus a special check for infinite waiting), where feasible(w) is a second BFS from the person's start, in which a cell may be entered only if the person's arrival time (start moment + w + BFS distance) is strictly less than the cell's ignition time — except at the goal cell, where a tie is allowed. Increasing the wait time can only make things worse or leave them the same until at some point the route is blocked entirely, so feasibility is monotone. This is the unit's hardest problem because it combines two separate BFS runs inside one binary search — keep the ignition and arrival times in clearly separate variables so you do not mix them up.
</details>

---

## Progress

- [ ] 03.1.1 Binary Search (LC #704)
- [ ] 03.1.2 Search Insert Position (LC #35)
- [ ] 03.1.3 First Bad Version (LC #278)
- [ ] 03.1.4 Guess Number Higher or Lower (LC #374)
- [ ] 03.1.5 Valid Perfect Square (LC #367)
- [ ] 03.1.6 Sqrt(x) (LC #69)
- [ ] 03.1.7 Arranging Coins (LC #441)
- [ ] 03.1.8 Find Smallest Letter Greater Than Target (LC #744)
- [ ] 03.2.1 Find First and Last Position of Element in Sorted Array (LC #34)
- [ ] 03.2.2 Successful Pairs of Spells and Potions (LC #2300)
- [ ] 03.2.3 Find K Closest Elements (LC #658)
- [ ] 03.2.4 Random Pick with Weight (LC #528)
- [ ] 03.2.5 Time Based Key-Value Store (LC #981)
- [ ] 03.2.6 Single Element in a Sorted Array (LC #540)
- [ ] 03.2.7 Find Peak Element (LC #162)
- [ ] 03.2.8 Median of Two Sorted Arrays (LC #4)
- [ ] 03.3.1 Check if Array Is Sorted and Rotated (LC #1752)
- [ ] 03.3.2 Peak Index in a Mountain Array (LC #852)
- [ ] 03.3.3 Find Minimum in Rotated Sorted Array (LC #153)
- [ ] 03.3.4 Search in Rotated Sorted Array (LC #33)
- [ ] 03.3.5 Search in Rotated Sorted Array II (LC #81)
- [ ] 03.3.6 Find Minimum in Rotated Sorted Array II (LC #154)
- [ ] 03.3.7 Find in Mountain Array (LC #1095)
- [ ] 03.4.1 Koko Eating Bananas (LC #875)
- [ ] 03.4.2 Minimum Number of Days to Make m Bouquets (LC #1482)
- [ ] 03.4.3 Capacity To Ship Packages Within D Days (LC #1011)
- [ ] 03.4.4 Find the Smallest Divisor Given a Threshold (LC #1283)
- [ ] 03.4.5 Minimum Time to Complete Trips (LC #2187)
- [ ] 03.4.6 Minimum Speed to Arrive on Time (LC #1870)
- [ ] 03.4.7 Magnetic Force Between Two Balls (LC #1552)
- [ ] 03.4.8 Split Array Largest Sum (LC #410)
- [ ] 03.5.1 Count Negative Numbers in a Sorted Matrix (LC #1351)
- [ ] 03.5.2 The K Weakest Rows in a Matrix (LC #1337)
- [ ] 03.5.3 Search a 2D Matrix (LC #74)
- [ ] 03.5.4 Search a 2D Matrix II (LC #240)
- [ ] 03.5.5 Find a Peak Element II (LC #1901)
- [ ] 03.5.6 Kth Smallest Element in a Sorted Matrix (LC #378)
- [ ] 03.5.7 Maximum Side Length of a Square with Sum Less than or Equal to Threshold (LC #1292)
- [ ] 03.5.8 Find the Kth Smallest Sum of a Matrix With Sorted Rows (LC #1439)
- [ ] 03.6.1 Minimum Limit of Balls in a Bag (LC #1760)
- [ ] 03.6.2 Maximum Value at a Given Index in a Bounded Array (LC #1802)
- [ ] 03.6.3 Path With Minimum Effort (LC #1631)
- [ ] 03.6.4 Find the Safest Path in a Grid (LC #2812)
- [ ] 03.6.5 Swim in Rising Water (LC #778)
- [ ] 03.6.6 Last Day Where You Can Still Cross (LC #1970)
- [ ] 03.6.7 Escape the Spreading Fire (LC #2258)
