# Chapter 08 — Problems

45 problems in six units, in the order of `lesson.md`. How to work them:

1. Read the unit's section in `lesson.md` first.
2. Attempt the problem **in C** in `algorithms_learning/08_greedy_and_intervals/solutions/<slug>.c` (create the `solutions/` folder yourself). Write your own tests in `main()` — including the boundary case (touching intervals, `k` left over, a single element) — and run under `cc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined`.
3. Only if stuck, open **Hint**. Only after that, **Key idea & complexity**. Open **Approach** after you have solved it, or after 30 minutes stuck — then close the file and re-implement from memory.
4. For every greedy in units 1–5, write the exchange argument in a comment above your function before you consider the problem done. For unit 6, write the counterexample in a comment.

The **Level** is the source's 1–5 difficulty. Tags are the source's.

---

## Unit 1 — Sort-then-scan greedy

### 08.1.1  Assign Cookies  ·  LC #455  ·  Easy  ·  array, two-pointers, greedy, sorting
<https://leetcode.com/problems/assign-cookies/>
**Level:** 2
<details><summary>Hint</summary>
Sort both the children and the cookies, and give the smallest sufficient cookie to the smallest demand.
</details>
<details><summary>Key idea & complexity</summary>
Greedy two pointers: sort both and match the smallest sufficient cookie, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the children (`g`) and the cookies (`s`) ascending. Walk both with two pointers: if the current cookie is enough for the current child (`s[j] >= g[i]`), the child is satisfied and both pointers advance; otherwise only the cookie pointer advances. Exchange argument: the smallest sufficient cookie should always go to the least demanding child, because any larger cookie would be "waste" that some more demanding child might need later. Pitfall: forgetting to sort *both* lists — without that, the two-pointer walk does not work.
</details>

### 08.1.2  Lemonade Change  ·  LC #860  ·  Easy  ·  array, greedy
<https://leetcode.com/problems/lemonade-change/>
**Level:** 2
<details><summary>Hint</summary>
When giving change from a 20, use a 10-bill first if you can — save the 5-bills.
</details>
<details><summary>Key idea & complexity</summary>
Greedy one-pass simulation, keep the counts of 5- and 10-dollar bills, `O(n)` time `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Maintain only two counters, `five` and `ten` (twenties are never given back as change, so they need not be counted). When a customer pays with 5, `five++`. When paying with 10, one 5 is required: `five--, ten++`. When paying with 20, preferably give one 10 and one 5 if both exist, otherwise three 5s — this is the greedy choice, because a 10-bill is useful for no other change than 15, while a 5-bill is more flexible and worth saving. Pitfall: giving three 5s even though 10+5 was possible, which wastes the flexible 5-bills for nothing.
</details>

### 08.1.3  Maximum Units on a Truck  ·  LC #1710  ·  Easy  ·  array, greedy, sorting
<https://leetcode.com/problems/maximum-units-on-a-truck/>
**Level:** 2
<details><summary>Hint</summary>
Always take the type with the most units per box first, until the truck is full.
</details>
<details><summary>Key idea & complexity</summary>
Greedy: sort box types by units per box descending, fill the capacity, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the box types by units per box **descending**. Walk the order and load as many of each type as the remaining capacity allows before moving to the next type. Exchange argument: if you took a box with fewer units before some box with more units, swapping them never decreases the total unit sum — so the order from most valuable to least is always at least as good. Pitfall: sorting by the **number** of boxes instead of the units — the wrong key gives a wrong answer immediately.
</details>

### 08.1.4  Maximize Sum Of Array After K Negations  ·  LC #1005  ·  Easy  ·  array, greedy, sorting
<https://leetcode.com/problems/maximize-sum-of-array-after-k-negations/>
**Level:** 2
<details><summary>Hint</summary>
Sort and flip the smallest (most negative) numbers until `k` runs out — any leftover `k` is spent on the smallest absolute value.
</details>
<details><summary>Key idea & complexity</summary>
Greedy: sort ascending and flip the most negative values, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the array ascending and walk from the left: flip every negative number while `k > 0`, because flipping the smallest (i.e. most negative) number increases the sum the most. When the negatives run out or `k = 0`: if `k` remains and is odd, flip once more the number with the smallest absolute value. Exchange argument: flipping anything other than the smallest number yields a sum at least as bad. Pitfall: forgetting that the leftover `k` can still change the answer if it is odd — an even surplus has no effect at all.
</details>

### 08.1.5  Minimum Increment to Make Array Unique  ·  LC #945  ·  Medium  ·  array, greedy, sorting, counting
<https://leetcode.com/problems/minimum-increment-to-make-array-unique/>
**Level:** 3
<details><summary>Hint</summary>
When two consecutive (sorted) numbers are equal or out of order, lift the latter to the former plus one.
</details>
<details><summary>Key idea & complexity</summary>
Greedy: sort ascending and force each element to exceed the previous, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the array ascending. Walk left to right keeping `prev` = the value of the last processed (possibly already lifted) number. If the current number is `<= prev`, lift it to `prev + 1` and add the difference to the answer; otherwise it may stay as is and `prev` is updated to it. Sorting guarantees you never need to lift a number more than strictly necessary. Pitfall: processing the numbers in their original order without sorting — then minimising the increments is no longer possible, because a later smaller number could have needed less lifting had it been processed first.
</details>

### 08.1.6  Boats to Save People  ·  LC #881  ·  Medium  ·  array, two-pointers, greedy, sorting
<https://leetcode.com/problems/boats-to-save-people/>
**Level:** 3
<details><summary>Hint</summary>
Sort the weights and always try to pair the lightest remaining person with the heaviest remaining one.
</details>
<details><summary>Key idea & complexity</summary>
Greedy two pointers: sort and pair the lightest with the heaviest if they fit, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the weights ascending. Two pointers, `lo` and `hi`: if the lightest and the heaviest fit in one boat (`weight[lo] + weight[hi] <= limit`), both leave in the same boat; otherwise the heaviest leaves alone. Either way one boat is used and `hi--`, and `lo++` only if a pair was formed. Exchange argument: the heaviest person needs a boat of their own or a partner regardless — if they can be paired with someone, the lightest remaining person is the best choice, because it leaves the most room for other pairs later. Pitfall: trying to pair the heaviest with the second heaviest — wastes boat capacity that could have saved a boat by pairing lighter people.
</details>

### 08.1.7  Wiggle Subsequence  ·  LC #376  ·  Medium  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/wiggle-subsequence/>
**Level:** 3
<details><summary>Hint</summary>
Only direction changes (rising -> falling or vice versa) increase the answer — flat stretches do not.
</details>
<details><summary>Key idea & complexity</summary>
Greedy one-pass counter: count direction changes, `O(n)` time `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Keep two counters, `up` and `down`, describing the longest zigzag subsequence so far that ends in a rise or a fall respectively. When `a[i] > a[i-1]`, `up = down + 1`; when `a[i] < a[i-1]`, `down = up + 1`; on equality neither changes. The answer is `max(up, down) + 1`. This is greedy in the sense that every direction change is taken the moment it appears — inside a longer monotone stretch only the first and last points are ever useful. Pitfall: solving the problem with a more complicated `O(n)` DP (which also works) instead of the direct pair of counters.
</details>

### 08.1.8  Partition Labels  ·  LC #763  ·  Medium  ·  hash-table, two-pointers, string, greedy
<https://leetcode.com/problems/partition-labels/>
**Level:** 3
<details><summary>Hint</summary>
Extend the end boundary of the current part whenever you meet a letter whose last occurrence is further away.
</details>
<details><summary>Key idea & complexity</summary>
Greedy window extension by last occurrence, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Precompute for every letter its last occurrence index in the whole string. Walk the string maintaining the current part's `end` boundary: at every character `end = max(end, lastOccurrence[char])`. When the walked index reaches `end`, the current part is complete — record its length and start a new part. This is greedy because a part is closed **as soon as it can be**, never later — a smaller part is always at least as good as a larger one. Pitfall: forgetting to update `end` on every character rather than only at the start of a part.
</details>

---

## Unit 2 — Exchange-argument greedy

### 08.2.1  Two City Scheduling  ·  LC #1029  ·  Medium  ·  array, greedy, sorting, hungarian-algorithm
<https://leetcode.com/problems/two-city-scheduling/>
**Level:** 3
<details><summary>Hint</summary>
Sort people by how much cheaper city A is than city B for them.
</details>
<details><summary>Key idea & complexity</summary>
Greedy by exchange argument: sort by `costA - costB`, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the people by `costA[i] - costB[i]` ascending, send the first half to city A and the rest to city B. Exchange argument: if two people `i, j` are in the wrong order, swapping them never increases the total cost — the difference `(costA_i - costB_i) - (costA_j - costB_j)` is exactly the saving the swap yields. Half go to each city, so the first half of the sorted list is always the optimal choice for A. Pitfall: sorting by `costA` alone or `costB` alone — only the **difference** is the right key.
</details>

### 08.2.2  Monotone Increasing Digits  ·  LC #738  ·  Medium  ·  math, greedy
<https://leetcode.com/problems/monotone-increasing-digits/>
**Level:** 3
<details><summary>Hint</summary>
When a digit is smaller than its predecessor, the predecessor must be decreased — and then every digit after it can freely be maximised to 9.
</details>
<details><summary>Key idea & complexity</summary>
Greedy right to left: decrement a digit and set the rest to 9 when monotonicity breaks, `O(d)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Walk the digits from right to left. When you meet a position where `digit[i] < digit[i-1]`, do `digit[i-1] -= 1` and mark everything after this position to be set to 9 at the end — decreasing `digit[i-1]` by one frees you to choose all the rest maximal without breaking monotonicity. Exchange argument: any other way of repairing the violation leads to a smaller number, because adjusting the higher place value minimally and maximising the lower ones yields the largest possible number. Pitfall: walking left to right — then a decision made earlier can turn out wrong when a later violation appears.
</details>

### 08.2.3  Maximum Swap  ·  LC #670  ·  Medium  ·  math, greedy
<https://leetcode.com/problems/maximum-swap/>
**Level:** 3
<details><summary>Hint</summary>
For each digit: is there a larger digit to its right, and if so, which is the furthest (last) such one?
</details>
<details><summary>Key idea & complexity</summary>
Greedy: from the right, find the largest later digit that a smaller digit on the left can swap with, `O(d)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Precompute for every digit value `0..9` its **last** occurrence index in the string. Walk the digits left to right, and at every position `i` look for the largest digit `d > digit[i]` whose last occurrence is at a position `> i`; if one exists, swap them and stop. Exchange argument: you want to put the largest possible digit at the highest possible place value, and if several positions qualify for the same digit, the last occurrence is the safest choice because it does not disturb any larger digit in between. Pitfall: swapping with the first larger digit found instead of the last occurrence — sometimes yields a smaller result than the optimum.
</details>

### 08.2.4  Largest Number  ·  LC #179  ·  Medium  ·  array, string, greedy, sorting
<https://leetcode.com/problems/largest-number/>
**Level:** 3
<details><summary>Hint</summary>
Compare two numbers as strings concatenated both ways, not as numbers on their own.
</details>
<details><summary>Key idea & complexity</summary>
Greedy with a custom comparator `a+b` vs `b+a`, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Convert the numbers to strings and sort with the comparator: `a` comes before `b` if `a+b > b+a` compared as strings. Concatenate the sorted strings. Exchange argument: this order is optimal because swapping any two adjacent numbers in a way that violates it would make the concatenated number smaller — the proof follows from `a+b` and `b+a` being strings of equal length, so their numeric comparison is exactly the string comparison. Special case: if the first character of the result is `'0'`, the answer is `"0"`, not `"000...0"`. Pitfall: comparing the numbers directly or as strings without concatenation — both give the wrong order, e.g. for the pair 9 and 34.
</details>

### 08.2.5  Queue Reconstruction by Height  ·  LC #406  ·  Medium  ·  array, binary-indexed-tree, segment-tree, sorting
<https://leetcode.com/problems/queue-reconstruction-by-height/>
**Level:** 3
<details><summary>Hint</summary>
Handle the tallest first — their `k` value no longer changes when shorter people are inserted later.
</details>
<details><summary>Key idea & complexity</summary>
Greedy: sort height descending (ties by `k` ascending) and insert at index `k`, `O(n^2)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the people by height **descending**, and within equal height by `k` ascending. Insert them into an empty list in this order, each directly at index `k`. Exchange argument: once a taller-or-equal person has been inserted, a shorter person inserted later never affects the `k` values of those already placed, so every insertion at index `k` is definitively correct the moment it is made. Pitfall: sorting by `k` first, or by height ascending — neither guarantees that earlier placements stay correct after later insertions.
</details>

### 08.2.6  Remove Duplicate Letters  ·  LC #316  ·  Medium  ·  string, stack, greedy, monotonic-stack
<https://leetcode.com/problems/remove-duplicate-letters/>
**Level:** 3
<details><summary>Hint</summary>
Pop a letter from the stack if it is larger than the next letter to process AND it still occurs later.
</details>
<details><summary>Key idea & complexity</summary>
Greedy stack + last-occurrence table, keep the lexicographically smallest duplicate-free sequence, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Precompute every letter's last occurrence index and keep a stack for the result sequence plus an `inStack` set. Walk the string: if the letter is already in the stack, skip it. Otherwise, while the stack top is larger than the current letter **and** the stack top still occurs later, pop it. Push the current letter. Exchange argument: removing a larger letter that will appear again later never makes the result worse — it can always be re-added at a later, better position. Pitfall: forgetting to check whether the letter being popped still occurs later — otherwise the letter may be lost entirely.
</details>

### 08.2.7  Candy  ·  LC #135  ·  Hard  ·  array, greedy
<https://leetcode.com/problems/candy/>
**Level:** 4
<details><summary>Hint</summary>
One pass is not enough — the neighbour condition must hold in both directions simultaneously.
</details>
<details><summary>Key idea & complexity</summary>
Greedy two passes (left to right, right to left), take the max of both, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two passes: first left to right, for every child with `rating[i] > rating[i-1]`, `candies[i] = candies[i-1] + 1` (otherwise the initial value 1). Then right to left, for every child with `rating[i] > rating[i+1]`, `candies[i] = max(candies[i], candies[i+1] + 1)`. Neither pass alone suffices, because the neighbour condition concerns both directions at once — the `max` combines both requirements without breaking either. The answer is the sum of all `candies`. Pitfall: doing only one pass, or forgetting the `max` in the second pass, so the first pass's result is accidentally overwritten with something too small.
</details>

---

## Unit 3 — Interval scheduling & merging

### 08.3.1  Merge Intervals  ·  LC #56  ·  Medium  ·  array, sorting, quicksort
<https://leetcode.com/problems/merge-intervals/>
**Level:** 3
<details><summary>Hint</summary>
As you walk the sorted intervals, the next one either continues the current one or starts a new one.
</details>
<details><summary>Key idea & complexity</summary>
Greedy: sort by start and extend overlapping ones, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the intervals by start ascending. Walk them keeping `current` = the most recent open merged interval. If the next interval starts `<= current.end`, they overlap: extend `current.end = max(current.end, next.end)`. Otherwise `current` is finished — append it to the result and start a new `current`. Sorting guarantees that once you move on from an interval, no later interval can overlap it anymore. Pitfall: comparing `next.start` against the wrong value (`current.start` instead of `current.end`) — overlap depends specifically on the end, not the start.
</details>

### 08.3.2  Insert Interval  ·  LC #57  ·  Medium  ·  array
<https://leetcode.com/problems/insert-interval/>
**Level:** 3
<details><summary>Hint</summary>
The list is already sorted — insert the new interval in the right place in one pass, without re-sorting.
</details>
<details><summary>Key idea & complexity</summary>
Greedy three-phase pass (before, overlapping, after), `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the input is already sorted and non-overlapping, no separate sort is needed. Three phases in one pass: (1) append directly all intervals that end before the new interval starts; (2) merge all intervals that overlap the new one by widening the new interval's bounds (`newStart = min(...)`, `newEnd = max(...)`), then append the widened new interval; (3) append the rest, which start after the new interval's end. The greedy widening in phase 2 works because in a sorted list all the overlapping intervals are consecutive. Pitfall: reusing the full sort from merge-intervals — throws away the fact that the input is already sorted.
</details>

### 08.3.3  Non-overlapping Intervals  ·  LC #435  ·  Medium  ·  array, dynamic-programming, greedy, sorting
<https://leetcode.com/problems/non-overlapping-intervals/>
**Level:** 3
<details><summary>Hint</summary>
This is activity selection asked backwards: how many are left unselected when you maximise the number selected.
</details>
<details><summary>Key idea & complexity</summary>
Greedy by end time (activity selection), count removals on overlap, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same skeleton as classic activity selection: sort by end time, keep `lastEnd` = the end of the last kept interval. Walk: if the next interval's start is `>= lastEnd`, keep it and update `lastEnd`; otherwise it overlaps and is counted as removed. The answer is the number removed. Exchange argument as in activity selection: the earliest-ending interval always leaves the most room for later choices. Pitfall: sorting by start instead of end — gives the wrong answer on the same counterexample as in activity selection.
</details>

### 08.3.4  Minimum Number of Arrows to Burst Balloons  ·  LC #452  ·  Medium  ·  array, greedy, sorting
<https://leetcode.com/problems/minimum-number-of-arrows-to-burst-balloons/>
**Level:** 3
<details><summary>Hint</summary>
Same end-time order as activity selection, but count the number of groups needed.
</details>
<details><summary>Key idea & complexity</summary>
Greedy by end time, count a new arrow whenever an interval no longer overlaps, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the balloons by end (`end`) ascending. Shoot the first arrow at the first balloon's end, `arrowPos = balloons[0].end`, `arrows = 1`. Walk the rest: if a balloon's start is `> arrowPos`, it is no longer hit by the current arrow — shoot a new arrow at its end and increment the counter. Same exchange argument as non-overlapping-intervals: placing the arrow at the earliest-ending balloon's end covers as many later overlapping balloons as possible. Pitfall: closed/open boundaries handled wrongly — if `end` values are equal the balloons share the arrow (`>`, not `>=`, in the new-arrow check).
</details>

### 08.3.5  Interval List Intersections  ·  LC #986  ·  Medium  ·  array, two-pointers, sweep-line
<https://leetcode.com/problems/interval-list-intersections/>
**Level:** 3
<details><summary>Hint</summary>
Both lists are already sorted — the intersection is found directly with two pointers, no sorting.
</details>
<details><summary>Key idea & complexity</summary>
Greedy two pointers over two already-sorted lists, `O(n + m)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two pointers `i` and `j`, one per list. The intersection of the two current intervals is `[max(start_i, start_j), min(end_i, end_j)]`, appended to the result if start `<=` end. Then advance the pointer whose interval **ends earlier** — it can produce no further intersections with any later interval of either list. Exchange argument: the earlier-ending interval is "used up" as soon as its intersection has been handled. Pitfall: advancing both pointers at once, or the wrong one — loses possible intersections, because the longer-lasting interval may still intersect the next, shorter one.
</details>

### 08.3.6  Remove Covered Intervals  ·  LC #1288  ·  Medium  ·  array, sorting
<https://leetcode.com/problems/remove-covered-intervals/>
**Level:** 3
<details><summary>Hint</summary>
When the start is equal, handle first the one whose end is furthest — it can never become covered.
</details>
<details><summary>Key idea & complexity</summary>
Greedy: sort start ascending (end descending on ties), count the uncovered, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the intervals by start ascending; on ties by end **descending**, so that the widest interval comes first. Walk them keeping `maxEnd` = the largest end seen so far. If the current interval's end is `<= maxEnd`, it is entirely covered by some earlier one — skip it; otherwise it is a new uncovered interval, increment the counter and update `maxEnd`. Pitfall: forgetting the tie order — if two intervals start at the same point, the shorter one would be handled first and would wrongly look uncovered before the wider one.
</details>

### 08.3.7  My Calendar I  ·  LC #729  ·  Medium  ·  array, binary-search, design, segment-tree
<https://leetcode.com/problems/my-calendar-i/>
**Level:** 3
<details><summary>Hint</summary>
Two intervals do NOT overlap exactly when one ends before or at the start of the other.
</details>
<details><summary>Key idea & complexity</summary>
Linear (or tree-based) overlap check for every new booking, `O(n)` per booking naively.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
On every `book(start, end)` call, check against all previously accepted bookings whether the new one overlaps any of them: two intervals do **not** overlap exactly when `newEnd <= existing.start || newStart >= existing.end`. If none overlaps, the booking is accepted and added permanently to the list. The naive implementation is `O(n)` per call (`O(n^2)` total); a balanced search tree or ordered structure improves this to `O(log n)` per call. Pitfall: writing the overlap condition with the wrong inequality signs (`<` vs `<=`), which either rejects legal adjacent bookings or wrongly accepts overlapping ones.
</details>

---

## Unit 4 — Greedy with a heap

### 08.4.1  Task Scheduler  ·  LC #621  ·  Medium  ·  array, hash-table, greedy, sorting
<https://leetcode.com/problems/task-scheduler/>
**Level:** 3
<details><summary>Hint</summary>
Use a max-heap of the remaining counts per task type and always run the largest remaining one that the cooldown allows.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + heap (or a direct formula), always run the task type with the most remaining, `O(n log 26)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Maintain a max-heap of the remaining counts per task type. At each time unit take the heap maximum, execute it, decrement its count and put it into a cooldown queue for `n` steps before it returns to the heap. If the heap is empty but the cooldown queue is not, an idle time unit is spent. The greedy works because running the most-remaining task first minimises the risk that the cooldown later forces an idle unit. An alternative `O(26)` formula computes directly from the largest count `maxCount` and the number of types having it. Pitfall: forgetting that idle time is counted only when the heap is empty but the whole schedule is not yet complete.
</details>

### 08.4.2  Reorganize String  ·  LC #767  ·  Medium  ·  hash-table, string, greedy, sorting
<https://leetcode.com/problems/reorganize-string/>
**Level:** 3
<details><summary>Hint</summary>
Always take the two most frequent letters from the heap and place them alternately — the same letter never ends up adjacent.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + max-heap on letter frequencies, alternate the most common ones, `O(n log 26)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Maintain a max-heap of letter–frequency pairs. Each round pop the most frequent letter, append it to the result, decrement its count and set it aside for a moment, then pop the second most frequent and do the same, then push both back if their counts are still `> 0`. Pre-check: if any letter occurs more than `ceil(n/2)` times, the arrangement is impossible — return the empty string. Exchange argument: always placing the most frequent letter first minimises the risk that it is left alone at the end with no place to go. Pitfall: forgetting the impossibility pre-check.
</details>

### 08.4.3  Furthest Building You Can Reach  ·  LC #1642  ·  Medium  ·  array, greedy, heap-priority-queue
<https://leetcode.com/problems/furthest-building-you-can-reach/>
**Level:** 3
<details><summary>Hint</summary>
Always use ladders on the largest height differences so far — keep a min-heap of the jumps you have spent ladders on.
</details>
<details><summary>Key idea & complexity</summary>
Greedy min-heap: ladders for the largest height differences, bricks for the rest, `O(n log k)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Walk the buildings and at every positive height difference first assume you use a ladder; push the difference into a min-heap. When the heap size exceeds the number of available ladders, pop the **smallest** difference from the heap and pay it with bricks instead, because ladders should be saved for the largest differences. If the bricks run out, stop at the previous building. Exchange argument: ladders should always go to the largest height differences — the min-heap automatically keeps "the one to convert to bricks" as the smallest difference previously reserved for a ladder, in case a better target appears later. Pitfall: spending bricks greedily up front instead of deciding retroactively with the heap.
</details>

### 08.4.4  Single-Threaded CPU  ·  LC #1834  ·  Medium  ·  array, sorting, heap-priority-queue
<https://leetcode.com/problems/single-threaded-cpu/>
**Level:** 3
<details><summary>Hint</summary>
Sort the tasks by arrival time, and keep a heap of all already-arrived unprocessed tasks ordered by duration from shortest to longest.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + min-heap: sort by arrival and pick the shortest available, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the tasks by arrival time. Maintain a min-heap of `(duration, original index)` for all tasks that have arrived but not yet been processed. Simulate time: if the heap is empty, jump directly to the next arriving task's time; otherwise pop the shortest task, run it, and push all tasks that arrived in the meantime into the heap before the next pick. Exchange argument: running the shortest remaining task first minimises the average waiting time of the tasks. Pitfall: not pushing all tasks into the heap before picking — new, possibly shorter tasks that arrived in the meantime must be in the heap before the next choice.
</details>

### 08.4.5  Maximum Number of Events That Can Be Attended  ·  LC #1353  ·  Medium  ·  array, greedy, sorting, heap-priority-queue
<https://leetcode.com/problems/maximum-number-of-events-that-can-be-attended/>
**Level:** 3
<details><summary>Hint</summary>
Walk time one day at a time; push the events starting that day into the heap, and always attend the one whose deadline is nearest.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + min-heap by deadline, walk day by day and attend the event with the nearest deadline, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the events by start day. Simulate time one day at a time: every day push into the heap all events starting that day (min-heap by deadline), pop all already-expired events, and if the heap is not empty attend the event with the **nearest** deadline and remove it from the heap. Exchange argument: the event ending earliest should be attended first, because it has the fewest remaining opportunities — a later-ending event can still wait. Pitfall: walking day by day all the way to the maximum day even when no event is relevant that day — the more efficient version jumps straight to the next relevant day.
</details>

### 08.4.6  Minimum Number of Refueling Stops  ·  LC #871  ·  Hard  ·  array, dynamic-programming, greedy, heap-priority-queue
<https://leetcode.com/problems/minimum-number-of-refueling-stops/>
**Level:** 4
<details><summary>Hint</summary>
Drive as far as you can, and if you get stuck, refuel (retroactively, from the heap) at the largest skipped station you could have used.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + max-heap of the fuel at skipped stations, refuel lazily when needed, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Walk the stations in order of distance. Push every passed station into a max-heap by its fuel amount **before** checking whether the current fuel suffices to reach the next station. When the current range is not enough, "refuel retroactively": pop the largest skipped station from the heap and add its fuel to the range, repeating until the range suffices or the heap is empty. Every such pop counts as one stop. Exchange argument: the lazy decision is replaced by the optimal retroactive choice, which is provably the same as the optimal look-ahead strategy. Pitfall: refuelling greedily at every station right away instead of deferring the decision and making it from the heap afterwards — produces too many stops.
</details>

### 08.4.7  IPO  ·  LC #502  ·  Hard  ·  array, greedy, sorting, heap-priority-queue
<https://leetcode.com/problems/ipo/>
**Level:** 4
<details><summary>Hint</summary>
Sort projects by capital requirement, open the available ones into a heap by profit, and always pick the largest profit.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + two heaps (capital requirement min-heap, profit max-heap), `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the projects by capital requirement into a min-heap. Maintain a second, max-heap of the profits of the "available" projects. Repeat `k` times: move from the min-heap to the max-heap every project whose capital requirement is `<=` the current capital; pick the largest profit from the max-heap and add it to the capital. Exchange argument: always choosing the largest available profit is optimal, because the order of choices among projects does not affect which projects become available — only the accumulated capital does, and it grows by always maximising the largest possible increment. Pitfall: forgetting to update the set of available projects after every pick.
</details>

### 08.4.8  Minimum Cost to Hire K Workers  ·  LC #857  ·  Hard  ·  array, greedy, sorting, heap-priority-queue
<https://leetcode.com/problems/minimum-cost-to-hire-k-workers/>
**Level:** 4
<details><summary>Hint</summary>
Sort workers by wage/quality ratio; for a fixed ratio, the `k` workers with the smallest quality sum are always the optimum.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + max-heap by wage ratio in a sliding window, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the workers by wage/quality ratio (`wage/quality`) ascending. Walk the order and at every position assume that this ratio is the maximum ratio of the whole group. Maintain a max-heap of the `quality` values of the chosen workers: push the current one, and if the size exceeds `k`, pop the largest quality. When the size is exactly `k`, compute the cost `qualitySum * currentRatio` and update the best. Exchange argument: for a fixed ratio, minimising the quality sum is straightforward, and by walking the ratios in ascending order every possible "maximum ratio in the group" case is checked exactly once. Pitfall: forgetting that the group's cost depends on the **largest** ratio in the group, not the average.
</details>

---

## Unit 5 — Reach/jump greedy

### 08.5.1  Jump Game  ·  LC #55  ·  Medium  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/jump-game/>
**Level:** 3
<details><summary>Hint</summary>
Maintain the farthest reachable index so far and check that no index falls behind it.
</details>
<details><summary>Key idea & complexity</summary>
Greedy one-pass reach maintenance, `O(n)` time `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Maintain `farthest` = the largest index reachable using elements `0..i`. Walk the array left to right: if some index `i > farthest`, it was never reached — return false immediately. Otherwise `farthest = max(farthest, i + nums[i])`. If the loop runs to the end without interruption, the last index is reachable. This is greedy because only the largest possible reach at any moment matters. Pitfall: modelling the problem as full DP — works, but `O(n^2)` in the worst case; the single-variable greedy is both simpler and faster.
</details>

### 08.5.2  Jump Game II  ·  LC #45  ·  Medium  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/jump-game-ii/>
**Level:** 3
<details><summary>Hint</summary>
When you reach the boundary of the current layer, the next layer is already known from the `farthest` value of the pass so far.
</details>
<details><summary>Key idea & complexity</summary>
Greedy BFS by layers, `currentEnd` and `farthest`, `O(n)` time `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two variables: `currentEnd` = the furthest boundary of the current "jump layer", `farthest` = the furthest reach possible from the elements seen so far. Walk from index 0: `farthest = max(farthest, i + nums[i])`; when `i` reaches `currentEnd`, increment the jump count and set `currentEnd = farthest`. This corresponds to BFS on an unweighted graph where each "layer" is one jump — the greedy works because it always pays to go as far as possible within the current jump. Pitfall: incrementing the jump counter too early or too late — the counter grows exactly when `i` reaches `currentEnd`, not on every `farthest` update.
</details>

### 08.5.3  Gas Station  ·  LC #134  ·  Medium  ·  array, greedy
<https://leetcode.com/problems/gas-station/>
**Level:** 3
<details><summary>Hint</summary>
If the total sum is negative there is no solution — otherwise the starting point is found in one pass.
</details>
<details><summary>Key idea & complexity</summary>
Greedy one-pass net sum; the start is the point after which the sum last turned negative, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First compute the total sum of `gas - cost` over all stations; if it is negative, the loop cannot be completed from any point — return `-1`. Otherwise walk the stations once, maintaining a running `tank` sum and a `start` candidate: when `tank` goes negative at station `i`, no route started anywhere in `start..i` could have succeeded, so set `start = i + 1` and reset `tank`. Because the total is non-negative, the last remaining `start` is guaranteed to be a solution. Pitfall: trying every possible starting point separately in `O(n^2)` — works but is needlessly slow compared to the one-pass greedy.
</details>

### 08.5.4  Broken Calculator  ·  LC #991  ·  Medium  ·  math, greedy
<https://leetcode.com/problems/broken-calculator/>
**Level:** 3
<details><summary>Hint</summary>
Simulate backwards from `target` to `start` — multiplication turns into division, subtraction into addition.
</details>
<details><summary>Key idea & complexity</summary>
Greedy reverse simulation from target to start, `O(log(target))` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The forward simulation is hard to make greedy, because "double" is not always clearly the best choice. The reverse view works better: simulate from `target` to `start` with the operations inverted — if `target` is even, halve it; if odd, add one. Once `target < start`, the remaining steps are exactly `start - target`. Greedy reasoning: halving is always better than repeated adding/subtracting of ones for large numbers, because it reduces the distance exponentially instead of linearly — an odd number must always be made even before halving, so `+1` is the only useful move in that situation. Pitfall: trying to be greedy in the forward direction — the decision "double or subtract" is not locally clear there.
</details>

### 08.5.5  Reach a Number  ·  LC #754  ·  Medium  ·  math, binary-search
<https://leetcode.com/problems/reach-a-number/>
**Level:** 3
<details><summary>Hint</summary>
Grow the step count `n` until `1+2+...+n >= |target|`, then fix the parity difference by flipping one step.
</details>
<details><summary>Key idea & complexity</summary>
Greedy + parity fix: grow the step count until the sum exceeds the target with the same parity, `O(sqrt(target))` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Take `target = |target|` (symmetric about the origin). Grow the step count `n`, keeping the running sum `1 + 2 + ... + n`, until the sum is `>=` target. If the sum has exactly the same parity as the target, `n` is the answer directly — half of the difference can always be realised by flipping one suitable step negative. If the parity does not match, grow `n` by one or two more until it does. Greedy because the smallest `n` for which the sum is `>=` target with the right parity is always achievable without needing a larger `n`. Pitfall: forgetting the parity check and returning the wrong (too small) `n` as soon as the sum first exceeds the target.
</details>

### 08.5.6  Video Stitching  ·  LC #1024  ·  Medium  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/video-stitching/>
**Level:** 3
<details><summary>Hint</summary>
This is jump-game with intervals: cover the current time span by choosing the clip that reaches furthest.
</details>
<details><summary>Key idea & complexity</summary>
Greedy coverage (like jump game), extend the reach one segment at a time, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Maintain `covered` = the furthest time covered by the clips chosen so far, and `lastEnd` = the end of the previously chosen clip. Walk the clips sorted: when the walked time reaches `lastEnd`, choose the clip with the largest `covered` value among those seen so far, increment the clip count and set `lastEnd = covered`. If `covered` never exceeds the current `lastEnd` before the target is reached, covering is impossible. Structurally identical to jump-game-ii, except the "layers" are now the clips' time spans rather than indices. Pitfall: choosing the first valid clip greedily instead of waiting and choosing the one that reaches furthest.
</details>

### 08.5.7  Minimum Number of Taps to Open to Water a Garden  ·  LC #1326  ·  Hard  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/minimum-number-of-taps-to-open-to-water-a-garden/>
**Level:** 4
<details><summary>Hint</summary>
Convert every tap `[i, i+range]` into an interval and solve like jump-game-ii.
</details>
<details><summary>Key idea & complexity</summary>
Greedy jump-game-ii transformation: convert each tap into an interval and use the same layer greedy, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Convert every tap `i` into the interval `[max(0, i - range_i), i + range_i]`. Build an array `farthest[x]` = the furthest right edge of any tap that can start watering at position `x` or earlier. Then run exactly the same greedy layer algorithm as in jump-game-ii: `currentEnd`, `farthest`, and the tap counter grows whenever `currentEnd` is reached. If `farthest` never advances past the current `currentEnd` before the target is reached, watering is impossible. This is a direct generalisation of jump-game-ii. Pitfall: forgetting to handle taps whose left edge is negative (clamp to zero) — without the clamp the `farthest` array is indexed wrongly.
</details>

---

## Unit 6 — Where greedy fails, DP is needed

### 08.6.1  House Robber  ·  LC #198  ·  Medium  ·  array, dynamic-programming
<https://leetcode.com/problems/house-robber/>
**Level:** 3
Naive greedy: sort the houses by value descending and greedily pick the largest house that is not adjacent to one already picked. Why does this fail?
<details><summary>Hint</summary>
Try the houses `[11,4,4,10,13,8]` and compare the greedy choice with the true optimum.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (value order + independent picking) fails — you need 1-D DP, `dp[i]` = best loot from houses up to `i`, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: houses `[11,4,4,10,13,8]`. The greedy (pick from largest to smallest, skip if a neighbour is already picked) chooses in order `13, 11` — not adjacent, both allowed; then `4` (index 2, not adjacent to either pick). Greedy sum: `13 + 11 + 4 = 28`. The true optimum is `11 + 10 + 8 = 29` (indices 0, 3, 5). The greedy commits to the largest value (`13`) immediately, which destroys two good neighbours (`10` and `8`) around it at once — it never weighs "what do I lose among this choice's neighbours" globally. DP keeps both alternatives (`dp[i-1]`, `dp[i-2] + nums[i]`) open at every position: `dp[i] = max(dp[i-1], dp[i-2] + nums[i])`.
</details>

### 08.6.2  Coin Change  ·  LC #322  ·  Medium  ·  array, dynamic-programming, breadth-first-search, knapsack-problem
<https://leetcode.com/problems/coin-change/>
**Level:** 3
The classic greedy "always take the largest coin that fits" works with euro coins but not in general. Why?
<details><summary>Hint</summary>
Try coins `{1,3,4}` for amount `6` and compare the greedy with the optimum.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (largest coin first) fails on non-canonical coin sets — you need unbounded-knapsack DP, `dp[x]` = fewest coins, `O(n * amount)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: coins `{1,3,4}`, amount `6`. The greedy takes the largest that fits: `4`, leaving `2`; then `1`, leaving `1`; then `1`. Total `4 + 1 + 1`, three coins. The optimum is `3 + 3 = 6`, i.e. two coins. Taking the largest coin locks the remaining amount (`2`) into something that can no longer be filled efficiently — the greedy never undoes its choice to see whether a different first choice would have led to a better outcome. DP fixes this by trying every possible first coin: `dp[x] = 1 + min_c dp[x - c]`. With euro coins `{1,2,5,10,...}` the greedy happens to work, because the set is specifically constructed so that this commitment problem never arises — but that cannot be assumed in general without proof.
</details>

### 08.6.3  Perfect Squares  ·  LC #279  ·  Medium  ·  math, dynamic-programming, breadth-first-search, knapsack-problem
<https://leetcode.com/problems/perfect-squares/>
**Level:** 3
The greedy "always take the largest square that fits" resembles the coin problem. Is it optimal?
<details><summary>Hint</summary>
Try `n = 12`: compare the greedy's `9+1+1+1` with the better alternative.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (largest square first) fails — you need unbounded-knapsack DP, `dp[n]` = fewest squares, `O(n sqrt n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: `n = 12`. The greedy takes the largest square that fits, `9`, leaving `3 = 1 + 1 + 1`, total `9 + 1 + 1 + 1`, i.e. four squares. The optimum is `4 + 4 + 4 = 12`, three squares. Same phenomenon as the coin problem: taking the largest piece locks the remaining amount into an unfavourable split. DP systematically tries every first square `j^2 <= n` and picks the best: `dp[n] = 1 + min_j dp[n - j^2]`. Worth noting: Lagrange's four-square theorem guarantees the answer is always `<= 4`, but it does not say **which** squares to choose — that still has to be computed with DP.
</details>

### 08.6.4  Partition Equal Subset Sum  ·  LC #416  ·  Medium  ·  array, dynamic-programming, knapsack-problem, 0-1-knapsack
<https://leetcode.com/problems/partition-equal-subset-sum/>
**Level:** 3
The greedy "sort descending and stuff the largest into one pile until the target is met" feels natural. Does it work?
<details><summary>Hint</summary>
Try the set `{15,15,14,11,8,5}` (target 34) and compare the greedy packing with the correct split.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (largest-first packing) fails — you need 0/1-knapsack DP, `dp[s]` = is sum `s` reachable, `O(n * sum)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: set `{15,15,14,11,8,5}`, total `68`, target per side `34`. The greedy (largest to smallest, add if it fits) takes `15 + 15 = 30`, leaving `4` — none of the remaining (`14, 11, 8, 5`) fits exactly into what is left; the greedy gets stuck. Yet a correct split exists: `{15,14,5} = 34` and `{15,11,8} = 34`. The greedy is a **fractional knapsack** strategy, which is optimal only when items could be cut — in the 0/1 knapsack, whole items can lock into combinations that prevent the exact sum. DP checks all subsets at the bit level: `dp[s] |= dp[s - num]`, with `s` from large to small.
</details>

### 08.6.5  Longest Increasing Subsequence  ·  LC #300  ·  Medium  ·  array, binary-search, dynamic-programming, longest-increasing-subsequence
<https://leetcode.com/problems/longest-increasing-subsequence/>
**Level:** 3
The greedy "walk the sequence and include an element whenever it is larger than the last one chosen" seems natural. Is it optimal?
<details><summary>Hint</summary>
Try the sequence `[5,4,3,8,6,7]` and compare the length of the greedy's choice with the true LIS.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (local extend-if-larger) fails — you need `O(n^2)` DP or `O(n log n)` patience sorting, `dp[i]` = LIS length ending at `i`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: `[5,4,3,8,6,7]`. The greedy (include whenever larger than the last chosen, starting from the first element) chooses `5`, skips `4` and `3`, chooses `8`, skips `6` and `7` — result `[5,8]`, length `2`. The true longest increasing subsequence is `[3,6,7]` or `[4,6,7]`, length `3`. The greedy commits to the first suitable starting value (`5`) instead of considering whether a smaller start (`3` or `4`) would have gone further later. DP (or the `tails` array in patience sorting) keeps open all the "current best end value per length" alternatives instead of committing to one greedily at the start.
</details>

### 08.6.6  Word Break  ·  LC #139  ·  Medium  ·  array, hash-table, string, dynamic-programming
<https://leetcode.com/problems/word-break/>
**Level:** 3
The greedy "always take the longest dictionary word that matches the prefix" (maximal munch) feels like natural tokenisation. Does it work?
<details><summary>Hint</summary>
Try the dictionary `{aa, a, ab}` and the string `"aab"` — compare the greedy with the correct answer.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (longest match first) fails — you need 1-D DP, `dp[i]` = can the prefix of length `i` be split into words, `O(n^2)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: dictionary `{aa, a, ab}`, string `"aab"`. The greedy (take the longest matching prefix) chooses `"aa"` (length 2) from position 0, leaving `"b"` — not in the dictionary, so the greedy fails and wrongly returns false. Yet a correct split exists: `"a" + "ab"`, both in the dictionary. Choosing the longest match locks the rest in a way that cannot be repaired — the same commitment problem as in the coin and square examples, now in the world of strings. DP tries every possible last word: `dp[i] = OR` over all `j < i` with `dp[j]` true and `s[j..i)` in the dictionary, base case `dp[0] = true`.
</details>

### 08.6.7  Predict the Winner  ·  LC #486  ·  Medium  ·  array, math, dynamic-programming, recursion
<https://leetcode.com/problems/predict-the-winner/>
**Level:** 3
The greedy "always take the larger end pile" seems sensible in a two-player game. Is it a winning strategy?
<details><summary>Hint</summary>
Try the piles `[1,5,2,4,6]` and ask whether the greedy "larger end first" ever looks two moves ahead.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (always take the larger end) does not guarantee optimality — you need interval DP with minimax, `dp[i][j]` = best achievable score difference, `O(n^2)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The greedy "always take the larger end" focuses only on **your own** immediate gain and does not compute what situation the opponent is left in afterwards. For example with piles `[1,5,2,4,6]` the greedy first takes `6` (ends `1` vs `6`), but this choice ignores that it may leave the opponent the chance to grab both the `5` and later another large value in succession — in a game where both play optimally, local greed never looks two moves ahead. The correct strategy is computed backwards with minimax: `dp[i][j] = max(nums[i] - dp[i+1][j], nums[j] - dp[i][j-1])`, where every option also accounts for the opponent's best reply. Games always require this two-moves-ahead view, which plain greed does not provide.
</details>

### 08.6.8  Burst Balloons  ·  LC #312  ·  Hard  ·  array, dynamic-programming
<https://leetcode.com/problems/burst-balloons/>
**Level:** 4
The greedy "always burst the balloon that gives the largest immediate score" feels natural. Does it work?
<details><summary>Hint</summary>
Try the balloons `[3,1,5,8]` and think about how bursting one balloon changes the neighbours of the remaining ones.
</details>
<details><summary>Key idea & complexity</summary>
Greedy (largest immediate score first) fails — you need interval DP, `dp[i][j]` = best score for bursting the open range `(i,j)` with some balloon burst last, `O(n^3)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Counterexample: balloons `[3,1,5,8]` (with border padding `[1,3,1,5,8,1]`). The greedy would first burst the balloon giving the largest **immediate** score right now — for example the balloon with value `5` (neighbours `1` and `8`, score `1*5*8 = 40`) before the balloon `1` (neighbours `3` and `5`, score `3*1*5 = 15`). But by bursting `5` first, the neighbours of balloon `1` become unfavourable later in the bursting order. The true optimum (167 points) is obtained by bursting `1` first, which keeps the large neighbours (`3` and `8`) adjacent for as long as possible. The greedy ignores that the bursting order changes **future** adjacencies — interval DP solves this by thinking about which balloon is burst **last** in each range, so that its neighbours are always the known border points `i, j`.
</details>

---

## Progress

- [ ] 08.1.1 Assign Cookies (LC #455)
- [ ] 08.1.2 Lemonade Change (LC #860)
- [ ] 08.1.3 Maximum Units on a Truck (LC #1710)
- [ ] 08.1.4 Maximize Sum Of Array After K Negations (LC #1005)
- [ ] 08.1.5 Minimum Increment to Make Array Unique (LC #945)
- [ ] 08.1.6 Boats to Save People (LC #881)
- [ ] 08.1.7 Wiggle Subsequence (LC #376)
- [ ] 08.1.8 Partition Labels (LC #763)
- [ ] 08.2.1 Two City Scheduling (LC #1029)
- [ ] 08.2.2 Monotone Increasing Digits (LC #738)
- [ ] 08.2.3 Maximum Swap (LC #670)
- [ ] 08.2.4 Largest Number (LC #179)
- [ ] 08.2.5 Queue Reconstruction by Height (LC #406)
- [ ] 08.2.6 Remove Duplicate Letters (LC #316)
- [ ] 08.2.7 Candy (LC #135)
- [ ] 08.3.1 Merge Intervals (LC #56)
- [ ] 08.3.2 Insert Interval (LC #57)
- [ ] 08.3.3 Non-overlapping Intervals (LC #435)
- [ ] 08.3.4 Minimum Number of Arrows to Burst Balloons (LC #452)
- [ ] 08.3.5 Interval List Intersections (LC #986)
- [ ] 08.3.6 Remove Covered Intervals (LC #1288)
- [ ] 08.3.7 My Calendar I (LC #729)
- [ ] 08.4.1 Task Scheduler (LC #621)
- [ ] 08.4.2 Reorganize String (LC #767)
- [ ] 08.4.3 Furthest Building You Can Reach (LC #1642)
- [ ] 08.4.4 Single-Threaded CPU (LC #1834)
- [ ] 08.4.5 Maximum Number of Events That Can Be Attended (LC #1353)
- [ ] 08.4.6 Minimum Number of Refueling Stops (LC #871)
- [ ] 08.4.7 IPO (LC #502)
- [ ] 08.4.8 Minimum Cost to Hire K Workers (LC #857)
- [ ] 08.5.1 Jump Game (LC #55)
- [ ] 08.5.2 Jump Game II (LC #45)
- [ ] 08.5.3 Gas Station (LC #134)
- [ ] 08.5.4 Broken Calculator (LC #991)
- [ ] 08.5.5 Reach a Number (LC #754)
- [ ] 08.5.6 Video Stitching (LC #1024)
- [ ] 08.5.7 Minimum Number of Taps to Open to Water a Garden (LC #1326)
- [ ] 08.6.1 House Robber (LC #198)
- [ ] 08.6.2 Coin Change (LC #322)
- [ ] 08.6.3 Perfect Squares (LC #279)
- [ ] 08.6.4 Partition Equal Subset Sum (LC #416)
- [ ] 08.6.5 Longest Increasing Subsequence (LC #300)
- [ ] 08.6.6 Word Break (LC #139)
- [ ] 08.6.7 Predict the Winner (LC #486)
- [ ] 08.6.8 Burst Balloons (LC #312)
