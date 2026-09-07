# Chapter 02 — Problems

How to work these:

1. Read the matching unit section in `lesson.md` first.
2. Attempt the problem **in C**, in `algorithms_learning/02_two_pointers_and_sliding_window/solutions/<slug>.c` (create the `solutions/` folder yourself). Write your own tests in `main()` — the LeetCode examples plus at least one edge case (empty input, `n = 1`, all-equal values, `k >= n`). Compile with `cc -Wall -Wextra -std=c11 -O2 -fsanitize=address,undefined`.
3. Only then open the **Hint**.
4. Only then open **Key idea & complexity**.
5. Open the **Approach** after solving, or after 30 minutes stuck. It is the worked write-up, not code — you still write the code.

Numbering is `02.<unit>.<problem>`. Level is the source's 1–5 difficulty estimate, separate from LeetCode's Easy/Medium/Hard.

---

## Unit 1 — Opposite ends

### 02.1.1  Reverse String  ·  LC #344  ·  Easy  ·  two-pointers, string
<https://leetcode.com/problems/reverse-string/>
**Level:** 2
<details><summary>Hint</summary>
Swap the first and last characters, move inward, until the pointers meet.
</details>
<details><summary>Key idea & complexity</summary>
Swap from the ends towards the middle. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest possible instance of opposite pointers: `left` starts at the beginning, `right` at the end; swap `arr[left]` and `arr[right]`, then `left++`, `right--`, until `left >= right`. Every swap places two elements in their final reversed positions, so the loop does exactly `floor(n/2)` swaps. `O(n)` time, `O(1)` extra space, because everything happens in place in the original array — that is the whole value proposition of the pattern compared with building a new array backwards (`O(n)` space). Invariant: after each iteration the regions `[0, left)` and `(right, n-1]` are already in correct reversed order, and `[left, right]` is still unprocessed. Pitfall: even and odd lengths are in fact handled symmetrically (`left >= right` covers both); no separate middle-element check is needed.
</details>

### 02.1.2  Reverse Vowels of a String  ·  LC #345  ·  Easy  ·  two-pointers, string
<https://leetcode.com/problems/reverse-vowels-of-a-string/>
**Level:** 2
<details><summary>Hint</summary>
Move both pointers until each sits on a vowel, then swap only the vowels.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers, skip non-vowels. `O(n)` time, `O(1)` / `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same skeleton as `reverse-string`, but the pointers do not swap on every step — they first **search** for the nearest vowel. `left` moves right until it hits a vowel, `right` moves left until it hits a vowel, then swap and continue inward. The invariant is the same as in the basic reverse: every region already processed (swapped or skipped) is in its final form; only the vowels' mutual order is reversed, consonants stay in place. `O(n)` time, because each pointer moves at most `n` steps in total regardless of how many vowels there are. Space depends on the language: `O(1)` if the string is mutable in place (as an array), `O(n)` if a new immutable string must be built. Pitfall: both upper- and lower-case vowels (a, e, i, o, u in both cases) count as vowels — the case variation is easy to forget.
</details>

### 02.1.3  Valid Palindrome  ·  LC #125  ·  Easy  ·  two-pointers, string
<https://leetcode.com/problems/valid-palindrome/>
**Level:** 2
<details><summary>Hint</summary>
Skip non-alphanumeric characters from both ends, compare in lower case.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers + character filtering. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two pointers start at the ends and approach each other. On every step, first skip non-alphanumeric characters from both ends with inner `while` loops; when both pointers sit on a valid character, compare them case-insensitively (e.g. both normalised to lower case) — if they differ, the string is not a palindrome. `O(n)` time, because both pointers together move at most `n` steps over the whole run despite the nested loops (amortised linearity, the same idea as in `longest-consecutive-sequence`). `O(1)` space, because no separate cleaned-up string needs to be built. Pitfall: an empty string, or an input consisting only of punctuation, must be treated as a palindrome (the pointers meet without ever finding a difference).
</details>

### 02.1.4  Valid Palindrome II  ·  LC #680  ·  Easy  ·  two-pointers, string, greedy
<https://leetcode.com/problems/valid-palindrome-ii/>
**Level:** 2
<details><summary>Hint</summary>
When the first mismatch is found, try skipping either character and check the remainder.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers + one allowed deletion. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
An extension of the basic palindrome: one character may be deleted. Run the ordinary opposite-pointers check normally until you find the **first** position where `s[left] != s[right]`. At that point there are exactly two possible fixes: delete the character at the left pointer (check whether `s[left+1..right]` is a palindrome) or delete the character at the right pointer (check whether `s[left..right-1]` is a palindrome) — either one suffices, because the single deletion is allowed only once. Key insight: because this is the **first** mismatch, everything traversed so far is already symmetric, so the fix can be made locally and the whole string does not need to be retried with every possible deletion. `O(n)` time (two `O(n)` checks in the worst case), `O(1)` space. Pitfall: if both sub-checks fail, the answer is `false` — deletions may not be combined.
</details>

### 02.1.5  Squares of a Sorted Array  ·  LC #977  ·  Easy  ·  array, two-pointers, sorting
<https://leetcode.com/problems/squares-of-a-sorted-array/>
**Level:** 2
<details><summary>Hint</summary>
The largest square always comes from one of the two ends of the original sorted array.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers + build the result backwards. `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The original array is sorted but may contain negative numbers, so squaring would scramble the order — the straightforward solution would re-sort (`O(n log n)`). Opposite pointers do it in `O(n)`: because the absolute values of the negatives grow to the left and those of the positives grow to the right, the **largest** square is always found at one of the two ends of the array. Compare `arr[left]^2` and `arr[right]^2`, place the larger into the **last** free slot of the result array (so the result is built backwards), and move the corresponding pointer inward. Invariant: at every step, the largest possible square in the remaining range `[left, right]` is at one of its two edges, because each side is monotone in absolute value on its own. `O(n)` time, `O(n)` space for the result. Pitfall: the result must be built right to left (largest to smallest), not left to right.
</details>

### 02.1.6  Two Sum II - Input Array Is Sorted  ·  LC #167  ·  Medium  ·  array, two-pointers, binary-search
<https://leetcode.com/problems/two-sum-ii-input-array-is-sorted/>
**Level:** 3
<details><summary>Hint</summary>
If the sum is too large, decrease `right`; if too small, increase `left`.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers driven by the sum. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the array is already sorted, this is the canonical example of the pattern: `left` at the start, `right` at the end. If `arr[left] + arr[right] == target`, the answer is found. If the sum is too large, the only way to lower it is to move `right` leftwards (increasing `arr[left]` would only raise the sum further, and `arr[right]` is already the largest available partner candidate for the current `left`) — likewise a sum that is too small requires moving `left` rightwards. This reasoning is exactly what guarantees correctness: every move rules out a whole set of pairs at once without any valid pair going unfound. `O(n)` time, `O(1)` space — an improvement over the `hash map` version's `O(n)` space, exploiting precisely the sortedness. Pitfall: the problem usually expects 1-based indices in the answer; check the exact requirement.
</details>

### 02.1.7  Container With Most Water  ·  LC #11  ·  Medium  ·  array, two-pointers, greedy
<https://leetcode.com/problems/container-with-most-water/>
**Level:** 3
<details><summary>Hint</summary>
Always move the shorter wall — moving the taller one can never improve the result.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers, move the lower edge. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
`left` and `right` start at the ends; the volume is `min(height[left], height[right]) * (right - left)`. Greedy move: always move the **lower** wall inward. Correctness argument: the width shrinks by one regardless, so the only way to get a better result is to increase the height-limiting (lower) wall — if you moved the taller wall instead, the new volume would still be limited by the old lower wall (or by an even lower one, if the new wall is lower still), so no improvement opportunity is missed. This differs from `two-sum-ii`, where the pointer is chosen based on the sum — here the choice is based on which side *limits* the current result. `O(n)` time, because every step moves one of the pointers permanently towards the other, `O(1)` space. Pitfall: evaluate the result *before* moving on every step, not only at the end.
</details>

### 02.1.8  3Sum  ·  LC #15  ·  Medium  ·  array, two-pointers, sorting
<https://leetcode.com/problems/3sum/>
**Level:** 3
<details><summary>Hint</summary>
Sort the array, fix one number at a time, solve the rest as a two-pointer problem.
</details>
<details><summary>Key idea & complexity</summary>
Sort + fix one number + opposite pointers on the rest. `O(n^2)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the array first (`O(n log n)`) — this enables both duplicate skipping and opposite pointers. Fix the first number `nums[i]` with an outer loop and solve the rest with two pointers (`left = i+1`, `right = n-1`) exactly as in `two-sum-ii`: look for a pair whose sum is `-nums[i]`. When a pair is found, move both pointers inward and **skip duplicates** for the fixed number and for both pointers, so the same triple is not added several times. The outer loop runs `O(n)` times, the inner two-pointer solution is `O(n)` per round, total `O(n^2)` — a significant improvement over the brute-force `O(n^3)`. `O(1)` extra space (the result not counted). Pitfall: duplicate skipping must be done separately for all three indices, otherwise repeated triples appear in the result.
</details>

### 02.1.9  Trapping Rain Water  ·  LC #42  ·  Hard  ·  array, two-pointers, dynamic-programming, stack
<https://leetcode.com/problems/trapping-rain-water/>
**Level:** 4
<details><summary>Hint</summary>
Water at position `i` is bounded by the smaller of the maximum heights from either direction.
</details>
<details><summary>Key idea & complexity</summary>
Opposite pointers + running maxima. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The water at index `i` is `min(leftMax[i], rightMax[i]) - height[i]`. The naive solution precomputes two `O(n)` arrays; opposite pointers do it in `O(1)` space: `left`, `right` at the ends, plus running `leftMax`, `rightMax`. Move the pointer whose side has the **smaller** running maximum — if `leftMax < rightMax`, we know for certain that the water at `left` is bounded by `leftMax` (because somewhere to the right there is a wall of height at least `rightMax > leftMax`, so the right side's maximum cannot be the bottleneck), so the amount `leftMax - height[left]` can be computed as final immediately. This solves the same kind of problem as `container-with-most-water` but answers a different question — there a single pair was sought, here the local water at every point is summed. `O(n)` time, `O(1)` space. Pitfall: the criterion for which pointer to move is the comparison of the running maxima, not a direct comparison of `height[left]` vs `height[right]`.
</details>

---

## Unit 2 — Fast & slow pointers (cycle detection)

### 02.2.1  Middle of the Linked List  ·  LC #876  ·  Easy  ·  linked-list, two-pointers
<https://leetcode.com/problems/middle-of-the-linked-list/>
**Level:** 2
<details><summary>Hint</summary>
When the fast pointer reaches the end, the slow pointer is exactly in the middle.
</details>
<details><summary>Key idea & complexity</summary>
Fast/slow pointer. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest application, with no cycle: `slow` and `fast` start at the head, `slow` moves one node at a time, `fast` two. When `fast` (or `fast.next`) reaches the end of the list, `slow` is exactly at the middle — because `fast` has travelled twice as far as `slow`, and the whole list's length divided by two is precisely `slow`'s position. `O(n)` time in one pass, `O(1)` space — compared with the two-pass solution (first count the length, then walk halfway), which is also `O(n)` but needs two separate traversals. Pitfall: for an even-length list the "middle" is, depending on the problem's definition, one of the two central nodes — check whether `fast` stops on the condition `fast != null` or `fast.next != null`, because they give different results for even lengths.
</details>

### 02.2.2  Linked List Cycle  ·  LC #141  ·  Easy  ·  hash-table, linked-list, two-pointers, floyds-cycle-finding-algorithm
<https://leetcode.com/problems/linked-list-cycle/>
**Level:** 2
<details><summary>Hint</summary>
If the fast and slow pointers ever meet, the list has a cycle.
</details>
<details><summary>Key idea & complexity</summary>
Floyd's cycle detection. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The classic Floyd algorithm: `slow` moves one node, `fast` two nodes at a time. If the list has no cycle, `fast` reaches the `null` end and the algorithm returns `false`. If a cycle exists, `fast` enters it and starts going round; because `fast` closes in on `slow` by one step per iteration inside the cycle (the speed difference is 1 node/step), they inevitably meet within at most the cycle's length in steps — the meeting cannot be missed, because the distance shrinks by exactly one and never jumps straight past. `O(n)` time, `O(1)` space — a significant improvement over the `hash set` solution (store seen nodes, `O(n)` space), which also works but is not space-optimal. Pitfall: check `fast != null && fast.next != null` before every double step, otherwise a null-pointer error on an empty or acyclic list.
</details>

### 02.2.3  Happy Number  ·  LC #202  ·  Easy  ·  hash-table, math, two-pointers, floyds-cycle-finding-algorithm
<https://leetcode.com/problems/happy-number/>
**Level:** 2
<details><summary>Hint</summary>
Interpret "next number = sum of squared digits" as a function and look for a cycle with the same technique.
</details>
<details><summary>Key idea & complexity</summary>
Floyd's cycle detection on a numeric function. `O(log n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is not a linked list, but the structure is exactly the same: every number points unambiguously to a "next" number (the sum of the squares of its digits), so the whole process is a function graph in which every node has exactly one successor. Such a graph either ends at the fixed point 1 (a happy number) or keeps circling a cycle that does not contain 1 (not happy). Apply Floyd's fast/slow technique directly: `slow` computes one step at a time, `fast` two by applying the function twice; if they meet at the value 1, the number is happy; if they meet at any other value, a cycle without 1 has been found. The alternative is a `hash set` of seen values (`O(1)` space is lost to an `O(log n)`-sized set), but fast/slow achieves the same in `O(1)` space. Pitfall: do not forget that "cycle" here means any repeating value, not only the particular known cycle {4, 16, 37, 58, 89, 145, 42, 20}.
</details>

### 02.2.4  Palindrome Linked List  ·  LC #234  ·  Easy  ·  linked-list, two-pointers, stack, recursion
<https://leetcode.com/problems/palindrome-linked-list/>
**Level:** 2
<details><summary>Hint</summary>
Find the middle with fast/slow, reverse the tail half, compare with the head half.
</details>
<details><summary>Key idea & complexity</summary>
Find the middle + reverse the second half + compare. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Combines two patterns from this section: first find the middle exactly as in `middle-of-the-linked-list` with fast/slow pointers. Then reverse the **second half** of the list in place (relinking, no new memory). Finally compare the original first half and the reversed second half in parallel, node by node — if all values match, the list is a palindrome. This avoids the `O(n)` extra array that would be needed if the values were first copied into a list or a stack for comparison. `O(n)` time (middle search + reversal + comparison, all linear), `O(1)` space. Pitfall: a tidy solution reverses the tail back to its original form at the end, if the original list must not be left modified — depending on the problem statement this may be mandatory.
</details>

### 02.2.5  Find the Duplicate Number  ·  LC #287  ·  Medium  ·  array, two-pointers, binary-search, bit-manipulation
<https://leetcode.com/problems/find-the-duplicate-number/>
**Level:** 3
<details><summary>Hint</summary>
Interpret the values as index pointers: `nums[i]` is the "next" node — find the start of the cycle.
</details>
<details><summary>Key idea & complexity</summary>
Floyd's cycle detection on the index chain. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
An array of `n+1` values in the range `[1, n]` forces at least one duplicate (pigeonhole principle). Interpret the array as a function graph: index `i` points to node `nums[i]`. Because the duplicated value has at least two indices pointing to it, the graph necessarily contains a cycle, and the duplicated value is exactly the cycle's entry point. Apply Floyd's algorithm in two phases: first `slow = nums[slow]`, `fast = nums[nums[fast]]` until they meet inside the cycle; then reset one pointer to the start (index 0) and move both at the same speed — they meet exactly at the cycle entry, which is the duplicate. This is the same two-phase mathematics as in `linked-list-cycle-ii`. `O(n)` time, `O(1)` space — satisfying the problem's requirement not to modify the array and not to use extra memory, unlike the `hash set` solution. Pitfall: the array may not be modified or sorted, otherwise the index chain breaks.
</details>

### 02.2.6  Linked List Cycle II  ·  LC #142  ·  Medium  ·  hash-table, linked-list, two-pointers, floyds-cycle-finding-algorithm
<https://leetcode.com/problems/linked-list-cycle-ii/>
**Level:** 3
<details><summary>Hint</summary>
When the pointers meet, reset one to the head and move both at the same speed.
</details>
<details><summary>Key idea & complexity</summary>
Floyd's cycle detection + second phase for the cycle start. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A continuation of `linked-list-cycle`: instead of mere cycle existence, the exact node where the cycle begins is wanted. The first phase is ordinary Floyd: `slow` and `fast` meet somewhere inside the cycle, if one exists. The second phase uses distance arithmetic: let `a` = distance from the head to the cycle start, `b` = distance from the cycle start to the meeting point, `c` = remaining cycle length from the meeting point back to the start. One can show that `a = c` (because `fast` has travelled exactly the cycle length more than `slow`, which reduces to this equation). Therefore: reset one pointer to the head of the list, leave the other at the meeting point, move both one node at a time — they meet exactly at the cycle start, `a = c` steps later. `O(n)` time, `O(1)` space. Pitfall: the phase-one meeting point is not itself the cycle start — the second phase is mandatory; the commonest mistake is to return the first meeting point directly.
</details>

### 02.2.7  Reorder List  ·  LC #143  ·  Medium  ·  linked-list, two-pointers, stack, recursion
<https://leetcode.com/problems/reorder-list/>
**Level:** 3
<details><summary>Hint</summary>
Split the list in half with fast/slow, reverse the tail, merge alternately.
</details>
<details><summary>Key idea & complexity</summary>
Middle + reverse the tail + interleave the lists. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Three phases chained: (1) find the middle with fast/slow pointers as in `middle-of-the-linked-list`; (2) reverse the second half in place as in `palindrome-linked-list`; (3) interleave the two lists (the original first half and the reversed second half) by taking one node from each list in turn until one runs out. This produces the "L0 → Ln → L1 → Ln-1 → …" order without an extra array or stack for storing values, because the reversal makes the last nodes easily reachable from the front. `O(n)` time (all three phases linear), `O(1)` space. This problem is a good demonstration that fast/slow pointers and link reversal combine naturally into more complex list manipulations. Pitfall: the interleaving's termination condition must be checked carefully — with even and odd node counts, one of the lists runs out first at a different point.
</details>

### 02.2.8  Circular Array Loop  ·  LC #457  ·  Medium  ·  array, hash-table, two-pointers, floyds-cycle-finding-algorithm
<https://leetcode.com/problems/circular-array-loop/>
**Level:** 3
<details><summary>Hint</summary>
Every index points to one successor; find a cycle whose steps all go in the same direction.
</details>
<details><summary>Key idea & complexity</summary>
Fast/slow pointer on a function graph + direction check. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Every index `i` of the array points to the "next" index `(i + nums[i]) mod n` — again a function graph on which fast/slow pointers work, but with extra conditions: a cycle counts only if it is **uniformly directed** (all values in the cycle are either all positive or all negative) and **longer than one** (a single node pointing to itself does not count). Try every index as a possible cycle start; if fast/slow find a meeting, verify afterwards the uniform direction and the length by walking the cycle again. If the cycle does not qualify, mark all its nodes as zero so they are not retried later — this keeps the total time linear, because every node is visited at most a constant number of times. `O(n)` time, `O(1)` space. Pitfall: the direction check (positive vs. negative) must be done for every single step, not only for the cycle's final outcome.
</details>

---

## Unit 3 — Fixed-size window

### 02.3.1  Maximum Average Subarray I  ·  LC #643  ·  Easy  ·  array, sliding-window
<https://leetcode.com/problems/maximum-average-subarray-i/>
**Level:** 2
<details><summary>Hint</summary>
Compute the first window's sum, then update by removing the left and adding the right element.
</details>
<details><summary>Key idea & complexity</summary>
Fixed window, maintain a running sum. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Compute the sum of the first `k`-sized window directly. After that, on every shift to the right: `sum += nums[right] - nums[left]`, where `left` is the index just leaving the window and `right` the one just entering — a constant-time update instead of a full recomputation. Maintain the maximum sum seen after each update, and finally return `maxSum / k`. `O(n)` time (one pass, constant work per step), `O(1)` space — versus the brute-force `O(nk)`, where every window's sum would be computed from scratch. This is the base case of the whole fixed-window pattern, on which every other problem in this section builds. Pitfall: the division for the average is done only at the end on the integer maximum sum, not separately for every window — floating-point errors would accumulate needlessly.
</details>

### 02.3.2  Contains Duplicate II  ·  LC #219  ·  Easy  ·  array, hash-table, sliding-window
<https://leetcode.com/problems/contains-duplicate-ii/>
**Level:** 2
<details><summary>Hint</summary>
Keep only the last `k` elements in a hash set; if a new value is already there, the answer is yes.
</details>
<details><summary>Key idea & complexity</summary>
Fixed-distance hash set. `O(n)` time, `O(k)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the fixed-window idea applied to a membership test instead of a sum: we want to know whether the same value occurs twice within at most `k` indices of each other. Keep a `hash set` (or a `hash map` to the most recent index) containing exactly the last `k` values seen — the window size is a fixed `k`. On every step, first check whether the current value is in the set (a hit means a duplicate within `k`), then add the value to the set, and if the set's size exceeds `k`, remove the oldest value (the one just leaving the window). `O(n)` time, `O(k)` space (bounded by the window size, not the whole array as in the basic `contains-duplicate`). Pitfall: the expired value must be removed from the set consistently *before or after* the check — the easiest way is to use an ordered structure (e.g. a queue) to track removal order, if the same value can occur several times inside the window.
</details>

### 02.3.3  K Radius Subarray Averages  ·  LC #2090  ·  Medium  ·  array, sliding-window
<https://leetcode.com/problems/k-radius-subarray-averages/>
**Level:** 3
<details><summary>Hint</summary>
Every centre needs `k` elements on both sides — the window size is always `2k+1`.
</details>
<details><summary>Key idea & complexity</summary>
Fixed `2k+1` window + running sum. `O(n)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The `k`-radius window centred on each index `i` covers exactly `[i-k, i+k]`, i.e. a fixed-size interval of length `2k+1` — if `i-k < 0` or `i+k >= n`, the average cannot be computed (mark it `-1`). Compute the first valid window's sum directly, then slide it with the same "remove left, add right" principle as in `maximum-average-subarray-i`, except that the window size is now `2k+1` rather than `k`. The average is computed as integer division (usually rounding down), so `sum / (2k+1)`. `O(n)` time, `O(1)` extra space. This problem is good practice for the fact that "fixed size" does not just mean "start at the beginning" — the window's centre moves, but its size stays constant throughout. Pitfall: sum overflow is a real risk with large values and large `k`; use a wider integer type where needed.
</details>

### 02.3.4  Number of Sub-arrays of Size K and Average Greater than or Equal to Threshold  ·  LC #1343  ·  Medium  ·  array, sliding-window
<https://leetcode.com/problems/number-of-sub-arrays-of-size-k-and-average-greater-than-or-equal-to-threshold/>
**Level:** 3
<details><summary>Hint</summary>
Same running sum as maximum-average-subarray-i; count how many exceed the threshold.
</details>
<details><summary>Key idea & complexity</summary>
Fixed window + threshold comparison. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A straightforward application of fixed-window sliding: keep a running sum for the `k`-sized window with the same "remove left, add right" update. Instead of looking for a maximum, compare every window's sum against the threshold value `k * threshold` (multiplying up front avoids a floating-point division in every window) and increment a counter whenever the condition holds. This shows that the fixed-window pattern is not limited to finding a maximum or a sum — any predicate on the window sum that is computable in `O(1)` works just as well. `O(n)` time, `O(1)` space. Pitfall: compare the integer sum with `k*threshold` and do not divide the sum by `k` on every step — integer division rounds down and can give wrong results in edge cases.
</details>

### 02.3.5  Maximum Number of Vowels in a Substring of Given Length  ·  LC #1456  ·  Medium  ·  string, sliding-window
<https://leetcode.com/problems/maximum-number-of-vowels-in-a-substring-of-given-length/>
**Level:** 3
<details><summary>Hint</summary>
Same skeleton as maximum-average-subarray-i, but the state is a vowel count instead of a sum.
</details>
<details><summary>Key idea & complexity</summary>
Fixed window + vowel counter. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Exactly the same fixed-window skeleton as `maximum-average-subarray-i`, but the maintained state is a **counter** rather than an integer sum: how many vowels the current `k`-sized window contains. The first window is counted directly, and on every shift the counter is incremented by one if the entering character is a vowel and decremented by one if the leaving character was a vowel — the same "remove left, add right" invariant, but with a boolean-valued add/remove instead of a sum. Maintain the maximum count seen. `O(n)` time, `O(1)` space. This problem shows that the fixed window's state can be any quantity updatable in constant time — a sum, a counter, or even a simple frequency map, as long as both removal and insertion are `O(1)`. Pitfall: both upper- and lower-case vowels (a, e, i, o, u) count, depending on the case of the problem's input.
</details>

### 02.3.6  Maximum Points You Can Obtain from Cards  ·  LC #1423  ·  Medium  ·  array, sliding-window, prefix-sum
<https://leetcode.com/problems/maximum-points-you-can-obtain-from-cards/>
**Level:** 3
<details><summary>Hint</summary>
Taking `k` cards from the ends leaves exactly `n-k` cards in the middle — minimise their sum.
</details>
<details><summary>Key idea & complexity</summary>
Inverted fixed window: minimise the `(n-k)`-window left in the middle. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A clever reformulation: the `k` cards are always taken from one end or the other (a combination of both), which means that **exactly `n-k` consecutive cards remain in the middle**, untaken. Since the total sum is fixed, maximising the points is exactly equivalent to **minimising** the sum of the `(n-k)`-sized window left in the middle — and that is directly a fixed-window sliding problem: slide an `(n-k)`-sized window over the whole array and find the smallest sum with the same "remove left, add right" update. The answer is `totalSum - smallest (n-k)-window sum`. `O(n)` time, `O(1)` space — much simpler than directly trying all `k+1` ways to split the takes between the left and right ends (which is also possible in `O(k)` but needs more bookkeeping). Pitfall: if `k >= n`, the whole array is taken and no middle window remains — this special case is worth checking separately.
</details>

### 02.3.7  Grumpy Bookstore Owner  ·  LC #1052  ·  Medium  ·  array, sliding-window
<https://leetcode.com/problems/grumpy-bookstore-owner/>
**Level:** 3
<details><summary>Hint</summary>
Count the always-satisfied customers separately, then find the best position for the `k`-long "calm window".
</details>
<details><summary>Key idea & complexity</summary>
Fixed window over the recoverable customers. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Split the problem in two: customers who are satisfied regardless (`grumpy[i] == 0`) are summed directly, once. The remaining customers (lost if the owner is grumpy) can be rescued by using the "calming technique" for `k` consecutive minutes — this is exactly a fixed-window problem: slide a `k`-sized window and, for each window, count how many otherwise-lost customers (`grumpy[i] == 1`) it would rescue, maintaining the sum with the "remove left, add right" principle. The answer is `always-satisfied + best window's rescued count`. `O(n)` time, `O(1)` space. This is a good example of the fixed-window pattern hiding behind more elaborate wording — recognise that "the best choice of `k` consecutive minutes" is always fixed-window sliding. Pitfall: do not count a grumpy minute's customers twice (both in the base sum and in the window gain).
</details>

### 02.3.8  Sliding Window Maximum  ·  LC #239  ·  Hard  ·  array, queue, sliding-window, heap-priority-queue
<https://leetcode.com/problems/sliding-window-maximum/>
**Level:** 4
<details><summary>Hint</summary>
Keep a deque in decreasing value order; remove both expired and smaller indices.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic deque of indices. `O(n)` time, `O(k)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A simple counter cannot maintain the maximum, because the element leaving the window may be precisely the maximum — a structure that finds the new maximum quickly is needed. A **monotonic deque** stores indices in decreasing value order: before pushing a new index at the back, pop every index at the back whose value is less than or equal to the new value — they can never be the answer again, because the new element is both fresher and at least as large. The deque's **front** is always the index of the current window's maximum; if the front falls outside the window (`front <= right - k`), pop it from the front. Invariant: the deque is always in decreasing order and contains only indices inside the window, so the front is always the correct answer. Every index is pushed and popped at most once over the whole run, so the total time is `O(n)` amortised, even though the inner `while` pop looks like `O(nk)` at first. `O(k)` space for the deque. Pitfall: the deque stores indices, not values, so that falling out of the window can be checked.
</details>

---

## Unit 4 — Variable window (longest / shortest)

### 02.4.1  Max Consecutive Ones  ·  LC #485  ·  Easy  ·  array
<https://leetcode.com/problems/max-consecutive-ones/>
**Level:** 2
<details><summary>Hint</summary>
Always grow the window; when you hit a zero, restart the window right after it.
</details>
<details><summary>Key idea & complexity</summary>
Variable window with a zero counter. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest variable-window case, because the condition allows zero "violations" (none at all): when a zero is encountered, the window can no longer contain it, so `left` jumps straight to the index after the zero instead of running the general length-recomputation. In practice this is simpler than the general variable window: keep a running length of the current consecutive run, increment it on every one, reset it on every zero, and maintain the maximum seen. `O(n)` time in one pass, `O(1)` space. This works as a good bridge towards the more general variable-window model (`max-consecutive-ones-iii`), where more than zero violations are allowed and moving `left` needs proper shrink logic instead of a plain reset. Pitfall: remember to update the maximum on *every* step, not only when a zero is met — otherwise the final run may go uncounted.
</details>

### 02.4.2  Longest Substring Without Repeating Characters  ·  LC #3  ·  Medium  ·  hash-table, string, sliding-window
<https://leetcode.com/problems/longest-substring-without-repeating-characters/>
**Level:** 3
<details><summary>Hint</summary>
Grow the right edge; when a character repeats, shrink the left edge until the repeat is gone.
</details>
<details><summary>Key idea & complexity</summary>
Variable window + hash set/map of characters. `O(n)` time, `O(min(n, sigma))` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the canonical example of the pattern, even though it needs a `hash set` (or map) as a helper structure. Grow `right` and add the character to the set as long as it is not already in the window. When a character already in the set is encountered, shrink `left` (removing characters from the set) until the duplicate has left the window — only then add the new character. Maintain the maximum window length `right - left + 1` after every expansion. Monotonicity holds: once `left` has been moved to a certain position for a certain `right`, the window `[left, right]` is already the minimum needed to be duplicate-free, so `left` never moves back for later values of `right`. `O(n)` time, `O(min(n, sigma))` space (`sigma` = alphabet size). Pitfall: the simplest correct version shrinks one character at a time in a `while` loop, rather than jumping straight to the duplicate's index — the latter needs a map from character to its most recent index, not just a set.
</details>

### 02.4.3  Minimum Size Subarray Sum  ·  LC #209  ·  Medium  ·  array, binary-search, sliding-window, prefix-sum
<https://leetcode.com/problems/minimum-size-subarray-sum/>
**Level:** 3
<details><summary>Hint</summary>
Grow the right edge until the sum is `>= target`, then shrink the left as far as possible.
</details>
<details><summary>Key idea & complexity</summary>
Variable window on a sum. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
We seek the **shortest** subarray whose sum is at least `target`. Grow `right` and add the value to a running sum; as soon as the sum reaches or exceeds `target`, this is the moment to shrink: move `left` rightwards and subtract from the sum as far as the condition still holds, updating the minimum length at every valid shrink. Because all numbers are positive, the sum is monotonically increasing in `right` and monotonically decreasing as `left` grows — this monotonicity guarantees that the greedy shrink "as far as possible" never skips a better solution. `O(n)` time (both pointers move at most `2n` steps in total), `O(1)` space — an improvement over the `O(n log n)` binary-search + prefix-sum solution. Pitfall: if no subarray satisfies the condition, return `0`, not the never-updated "infinity" value.
</details>

### 02.4.4  Max Consecutive Ones III  ·  LC #1004  ·  Medium  ·  array, binary-search, sliding-window, prefix-sum
<https://leetcode.com/problems/max-consecutive-ones-iii/>
**Level:** 3
<details><summary>Hint</summary>
Always grow the window; when the zero count exceeds `k`, shrink the left until it is `<= k` again.
</details>
<details><summary>Key idea & complexity</summary>
Variable window with a zero counter. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A generalisation of `max-consecutive-ones`: at most `k` zeros may be flipped to ones inside the window. Always grow `right`, and increment a `zeros` counter if the entering element is a zero. When `zeros > k`, the window is broken — shrink `left` (decrementing `zeros` if the leaving element was a zero) until `zeros <= k` again. Maintain the maximum window length after every step. Here the window never needs to *shrink* back below the previous best, because it never shrinks more than necessary — at worst it stays the same size, never smaller than the previous maximum. `O(n)` time, `O(1)` space — an improvement over the `O(nk)` brute force that would test every window separately. Pitfall: this differs from `minimum-size-subarray-sum`, where you shrink as far as possible — here you shrink only until the condition is **just** satisfied, no further.
</details>

### 02.4.5  Subarray Product Less Than K  ·  LC #713  ·  Medium  ·  array, binary-search, sliding-window, prefix-sum
<https://leetcode.com/problems/subarray-product-less-than-k/>
**Level:** 3
<details><summary>Hint</summary>
Shrink until the product is `< k`; at every `right`, add `(right-left+1)` new valid subarrays.
</details>
<details><summary>Key idea & complexity</summary>
Variable window on a product + formula for the sub-windows inside the window. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Grow `right` and multiply into a running product; when the product reaches or exceeds `k`, shrink `left` (dividing the product by the leaving value) until the product is below `k` again. The counting trick: for every valid `right`, the window `[left, right]` is the **longest** valid window ending at that `right`, and because all numbers are positive (the product stays monotone), every **sub-window** of it that ends at `right` (i.e. `[left, right], [left+1, right], ..., [right, right]`) is valid too — a total of `right - left + 1` new subarrays are added to the answer on this step. This counting formula is the core of the whole problem, not just the window length. `O(n)` time, `O(1)` space. Pitfall: if `k <= 1`, no product of positive integers can ever be below `k`, so the answer is `0` directly — check this edge case before the loop, because the multiply/divide logic does not handle it correctly automatically in all implementations.
</details>

### 02.4.6  Count Number of Nice Subarrays  ·  LC #1248  ·  Medium  ·  array, hash-table, math, sliding-window
<https://leetcode.com/problems/count-number-of-nice-subarrays/>
**Level:** 3
<details><summary>Hint</summary>
Compute a function `atMost(k)` = subarrays with at most `k` odd numbers, and subtract `atMost(k-1)`.
</details>
<details><summary>Key idea & complexity</summary>
Variable window, "exactly `k`" = "at most `k`" − "at most `k-1`". `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
"Exactly `k` odd numbers" is not directly a monotone condition (growing the window may or may not push the odd count over `k`), but **"at most `k` odd numbers"** is monotone and is solved directly with a variable window as in `subarray-product-less-than-k`: shrink until the odd count is at most `k`, and add `right - left + 1` on every step. The classic trick: `exactly(k) = atMost(k) - atMost(k-1)`, because "at most `k-1`" is a subset of "at most `k`" and the difference leaves exactly those subarrays with precisely `k` odd numbers. Implement the `atMost` function once and call it twice. `O(n)` time (two linear passes), `O(1)` space. Pitfall: `atMost(0)` is a valid call (zero odd numbers allowed) — do not forget to special-case `k = 0` if `atMost(k-1)` would crash on a negative parameter.
</details>

### 02.4.7  Binary Subarrays With Sum  ·  LC #930  ·  Medium  ·  array, hash-table, sliding-window, prefix-sum
<https://leetcode.com/problems/binary-subarrays-with-sum/>
**Level:** 3
<details><summary>Hint</summary>
Same formula as count-number-of-nice-subarrays, but with the sum of ones as the quantity.
</details>
<details><summary>Key idea & complexity</summary>
The same "exactly = atMost(k) − atMost(k-1)" trick. `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Structurally identical to `count-number-of-nice-subarrays`, only the interpretation changes: instead of odd numbers, count the sum of ones, and split "exactly goal" again with the `atMost(goal) - atMost(goal-1)` formula, where `atMost(g)` counts with a variable window all subarrays whose sum of ones is at most `g`. Inside `atMost` the window grows on the right, shrinks on the left when the sum exceeds `g`, and at every valid `right` adds `right - left + 1` new subarrays — the same counting principle as `subarray-product-less-than-k`. `O(n)` time, `O(1)` space. This problem is deliberately almost identical to the previous one: noticing the same `atMost` formula recurring is a good sign that you recognise the pattern despite its different disguises. Pitfall: the edge case `goal = 0` requires handling `atMost(-1) = 0` explicitly (the function should return 0 for a negative target).
</details>

### 02.4.8  Frequency of the Most Frequent Element  ·  LC #1838  ·  Medium  ·  array, binary-search, greedy, sliding-window
<https://leetcode.com/problems/frequency-of-the-most-frequent-element/>
**Level:** 3
<details><summary>Hint</summary>
Sort first; the cost to raise a window to its max value is `max*len - windowSum`.
</details>
<details><summary>Key idea & complexity</summary>
Sort + variable window with a raise cost. `O(n log n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the array first (`O(n log n)`) — after sorting, "raising" any window `[left, right]` so that all elements equal the largest value `nums[right]` costs exactly `nums[right] * (right-left+1) - windowSum` operations, because the smaller values are always below the largest. Variable window: grow `right` and add to the running sum; if the raise cost exceeds `k`, shrink `left` (subtracting from the sum) until the cost fits the budget. Maintain the maximum window length. Why sorting is mandatory: without it, "raise everything to the same value" would not be a well-defined notion without separately choosing the target value — sorting makes the target automatically the window's largest element. `O(n log n)` time (sorting dominates the sliding window's `O(n)`), `O(1)` extra space. Pitfall: sum overflow with large values and a large window — use a 64-bit integer where needed.
</details>

---

## Unit 5 — Window with a counter / frequency map

### 02.5.1  Permutation in String  ·  LC #567  ·  Medium  ·  hash-table, two-pointers, string, sliding-window
<https://leetcode.com/problems/permutation-in-string/>
**Level:** 3
<details><summary>Hint</summary>
The window size is `len(s1)`; compare the window's letter histogram with `s1`'s histogram.
</details>
<details><summary>Key idea & complexity</summary>
Fixed-size window + frequency-map comparison. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A permutation means the same multiset of letters in a different order, so the question reduces to: is there a `len(s1)`-sized window in `s2` whose letter histogram matches `s1`'s histogram? This is in fact a combination of the **fixed-size** window (Unit 3) and a frequency map: slide a `|s1|`-sized window over `s2`, maintaining the window's letter counts with the "remove left, add right" principle, and compare the counts against `s1`'s counts at every step. Efficiency trick to avoid comparing the whole histogram every step: keep a single counter `matches` (how many letters match exactly), and update it in `O(1)` on every individual count change instead of comparing the whole 26-array. `O(n)` time (the fixed 26-letter alphabet makes the comparison `O(1)`), `O(1)` space. Pitfall: the naive full-histogram comparison at every step works but is `O(26n)` — acceptable, but not optimal.
</details>

### 02.5.2  Find All Anagrams in a String  ·  LC #438  ·  Medium  ·  hash-table, string, sliding-window
<https://leetcode.com/problems/find-all-anagrams-in-a-string/>
**Level:** 3
<details><summary>Hint</summary>
Same as permutation-in-string, but collect all match indices instead of one.
</details>
<details><summary>Key idea & complexity</summary>
Fixed-size window + frequency-map comparison. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Structurally the same solution as `permutation-in-string`, but instead of returning `true`/`false` at the first hit, collect **all** window start indices where the histogram matches `p`'s histogram. The same `matches` counter technique keeps every step `O(1)`: when the count for some letter becomes exactly right or wrong, update `matches` accordingly, and when `matches == 26` (or the alphabet size), the whole window is an anagram. `O(n)` time with a fixed-size window slid over `s`, `O(1)` space (26-sized arrays). This pair (`permutation-in-string` and this one) is good for showing that the same technical solution often answers several different questions ("does one exist" vs. "list them all") — only the way results are collected changes. Pitfall: store `left`, not `right`, in the result as the window's start position.
</details>

### 02.5.3  Maximum Erasure Value  ·  LC #1695  ·  Medium  ·  array, hash-table, sliding-window
<https://leetcode.com/problems/maximum-erasure-value/>
**Level:** 3
<details><summary>Hint</summary>
Shrink the left edge until the duplicate is gone, maintaining the window sum at the same time.
</details>
<details><summary>Key idea & complexity</summary>
Variable window with a hash set (distinct-values window) + sum. `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
We want the longest subarray in which all elements are distinct (like `longest-substring-without-repeating-characters`, but with numbers), and as the answer its best sum instead of a count. Use a `hash set` to track the window's contents and a running sum as the value: grow `right` and add the value both to the set and to the sum, until a duplicate is met. When a duplicate is met, shrink `left`, removing elements from the set and the sum, until the duplicate has left the window — the same shrink logic as in the basic pattern, but now maintaining both a set (for the validity check) and a sum (for the answer) side by side. Maintain the maximum sum seen. `O(n)` time (amortised, same argument as in Unit 4), `O(n)` space for the set. This is a good example of the window's "validity state" (the set) and "answer state" (the sum) being different structures that update in sync.
</details>

### 02.5.4  Fruit Into Baskets  ·  LC #904  ·  Medium  ·  array, hash-table, sliding-window
<https://leetcode.com/problems/fruit-into-baskets/>
**Level:** 3
<details><summary>Hint</summary>
Keep a frequency map of fruit types in the window; shrink when there are more than two types.
</details>
<details><summary>Key idea & complexity</summary>
Variable window with at most two distinct keys in the map. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A disguised version of "longest subarray with at most `k` distinct values", here with `k = 2` (two baskets). Maintain a `hash map` from fruit type to its count in the window. Grow `right` and update the map; when the number of keys (distinct types) exceeds 2, shrink `left` — on every shrink decrement the leaving type's count, and if the count drops to zero, remove the key from the map entirely (otherwise the key count goes wrong). Maintain the maximum window length. `O(n)` time, `O(1)` space (the map holds at most 3 keys at a time). This problem is a good test of whether you recognise the "distinct-values window" pattern behind its verbal disguise (fruit trees, baskets) — the core is the same as Unit 5's general model, only `k` is fixed at two. Pitfall: forgetting to remove a key whose count has dropped to zero, which makes the key count give the wrong result.
</details>

### 02.5.5  Longest Repeating Character Replacement  ·  LC #424  ·  Medium  ·  hash-table, string, sliding-window
<https://leetcode.com/problems/longest-repeating-character-replacement/>
**Level:** 3
<details><summary>Hint</summary>
The window is valid when `(windowLength - mostFrequentCount) <= k`.
</details>
<details><summary>Key idea & complexity</summary>
Variable window + most-frequent-letter counter. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A window can be made uniform with at most `k` replacements exactly when `windowLength - maxFreq <= k`, where `maxFreq` is the count of the window's most frequent letter — all the other (non-most-frequent) letters must be replaced, and there may be at most `k` of them. Maintain a 26-sized frequency map and a running `maxFreq` value (which never needs to be decreased on shrinking — see below). Always grow `right` and update the map and `maxFreq`; when the condition breaks, move `left` by one without recomputing `maxFreq` exactly. This is a deliberate correctness trick: because the goal is the **maximum length**, a wrong (too large) `maxFreq` can at most keep the window size the same, never produce a wrong, too-large answer — the window just "slides" without growing until a genuinely better `maxFreq` is found. `O(n)` time, `O(1)` space. Pitfall: do not try to decrease `maxFreq` on shrinking; that breaks the argument above and complicates the code needlessly.
</details>

### 02.5.6  Minimum Window Substring  ·  LC #76  ·  Hard  ·  hash-table, string, sliding-window
<https://leetcode.com/problems/minimum-window-substring/>
**Level:** 4
<details><summary>Hint</summary>
Grow until all of `t`'s characters are present in sufficient quantity, then shrink greedily.
</details>
<details><summary>Key idea & complexity</summary>
Variable window + frequency map + `matched` counter. `O(n+m)` time, `O(k)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The section's hardest classic: find the **shortest** substring of `s` containing all characters of `t` (multiplicities included). Maintain a map of the character counts `t` requires and of the window's current counts, plus a counter `matched` (how many distinct characters have already reached their required count in the window). Grow `right` until `matched` reaches the number of distinct characters in `t` — the window is now valid. Then shrink `left` greedily as far as the window stays valid, updating the minimum length at every valid shrink, before resuming the growth of `right`. The same monotone two-phase rhythm as `minimum-size-subarray-sum`, but the validity condition is a whole frequency distribution instead of a single number. `O(n+m)` time (`n = |s|`, `m = |t|` to build the map), `O(k)` space (`k` = number of distinct characters in `t`). Pitfall: the `matched` counter counts **distinct characters** whose requirement is met, not the total number of characters — the two notions are easy to confuse.
</details>

### 02.5.7  Subarrays with K Different Integers  ·  LC #992  ·  Hard  ·  array, hash-table, sliding-window, counting
<https://leetcode.com/problems/subarrays-with-k-different-integers/>
**Level:** 4
<details><summary>Hint</summary>
`atMost(k)` counts subarrays with at most `k` distinct values, like fruit-into-baskets generalised.
</details>
<details><summary>Key idea & complexity</summary>
The same "exactly = atMost(k) − atMost(k-1)" trick + frequency map. `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Combines this section's frequency-map idea with Unit 4's "exactly = atMost(k) − atMost(k-1)" counting formula into one hard-level problem. `atMost(k)` is directly `fruit-into-baskets` generalised to arbitrary `k`: slide a variable window, maintain a `hash map` from value to its count in the window, shrink when the number of distinct keys exceeds `k`, and add `right - left + 1` new subarrays to the answer at every valid `right`. The answer is `atMost(k) - atMost(k-1)`, because "exactly `k` distinct integers" is not itself a monotone condition, but "at most `k`" is. This problem is a good litmus test for the whole section: do you recognise both the frequency-map window and the `atMost` subtraction formula as a combination, rather than trying to solve the "exactly `k`" condition directly in one sweep (which is not possible due to the lack of monotonicity). `O(n)` time (two linear `atMost` calls), `O(k)` space.
</details>

---

## Unit 6 — Partitioning in place

### 02.6.1  Sort Array By Parity  ·  LC #905  ·  Easy  ·  array, two-pointers, sorting
<https://leetcode.com/problems/sort-array-by-parity/>
**Level:** 2
<details><summary>Hint</summary>
Opposite pointers: swap an odd from the left with an even from the right.
</details>
<details><summary>Key idea & complexity</summary>
Two pointers, swap odds and evens. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest two-region partition with opposite pointers: `left` at the start, `right` at the end. If `nums[left]` is even, it is already on the correct side; move `left`. If `nums[right]` is odd, it is already on the correct side; move `right`. When `nums[left]` is odd and `nums[right]` is even, swap them — both land on the correct side with one swap. Continue until `left >= right`. Invariant: the region `[0, left)` contains only evens, the region `(right, n-1]` only odds — the same opposite-pointers approach as in Unit 1, but now used for partitioning instead of finding a sum. `O(n)` time, `O(1)` extra space. Pitfall: the order inside each group does not have to be preserved (the problem does not require it) — if it did, a `writePointer`-style stable partition would be needed.
</details>

### 02.6.2  Sort Array By Parity II  ·  LC #922  ·  Easy  ·  array, two-pointers, sorting
<https://leetcode.com/problems/sort-array-by-parity-ii/>
**Level:** 2
<details><summary>Hint</summary>
Keep one pointer at the next wrong even position and one at the next wrong odd position.
</details>
<details><summary>Key idea & complexity</summary>
Two separate pointers for even and odd indices. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A more demanding version: in the output, even *indices* must hold even *values* and odd indices odd values — two constraints at once. Keep two pointers: `even` visits only even indices, `odd` only odd indices. When `nums[even]` is odd (in the wrong place), use `odd` to find the next even value sitting at an odd index (which is also in the wrong place) and swap them — both are fixed with one swap, because the swap moves each value to an index of the right parity. `O(n)` time (each pointer moves at most `n/2` steps in total on its own side), `O(1)` space. This is a good extension of the basic partition to two *parallel* partitioning criteria (value parity and index parity) instead of one. Pitfall: the `odd` pointer must not be reset every round — it continues from where it last stopped, otherwise the time grows to `O(n^2)`.
</details>

### 02.6.3  Move Zeroes  ·  LC #283  ·  Easy  ·  array, two-pointers
<https://leetcode.com/problems/move-zeroes/>
**Level:** 2
<details><summary>Hint</summary>
Write all non-zero elements to the front in order, fill the rest with zeros.
</details>
<details><summary>Key idea & complexity</summary>
Write pointer + read pointer. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The classic `writePointer`/`readPointer` partition where order must be preserved (unlike `sort-array-by-parity`). `write` points to the next free slot for non-zero values; `read` walks the whole array. When `nums[read]` is non-zero, write it to position `write` (swap or copy) and advance `write`. Because `read >= write` always (the read pointer is at least as far along), a write never overwrites data not yet read. At the end of the pass, fill the region `[write, n)` with zeros. Invariant: `[0, write)` contains the non-zero values seen so far in their original relative order — exactly this stability distinguishes this from `sort-array-by-parity`. `O(n)` time, `O(1)` space. Pitfall: use a swap instead of a direct overwrite if the array is traversed in a single pass without a separate zero-fill at the end.
</details>

### 02.6.4  Remove Duplicates from Sorted Array II  ·  LC #80  ·  Medium  ·  array, two-pointers
<https://leetcode.com/problems/remove-duplicates-from-sorted-array-ii/>
**Level:** 3
<details><summary>Hint</summary>
Write an element only if it differs from the value at `write-2` — allows at most two copies.
</details>
<details><summary>Key idea & complexity</summary>
Write pointer + comparison two slots back. `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A generalisation of the basic "remove all duplicates": here each value is allowed at most twice. The `write` pointer points to the next write position; because the array is already sorted, it suffices to compare the element being read with the value **two slots back** in the written result (`nums[write-2]`): if they differ, the new value is safe to write (it is either the first or the second occurrence), because a third identical value cannot be followed by two different ones in between. If they match, the value being read would be a third consecutive copy and is skipped without writing. `O(n)` time in one pass, `O(1)` space. This generalises directly to any allowed multiplicity `m` by comparing against `nums[write-m]` — worth noticing as a structural pattern rather than just this special case's solution. Pitfall: the comparison works only because the input is **sorted** — the same trick does not work for an unsorted array.
</details>

### 02.6.5  Sort Colors  ·  LC #75  ·  Medium  ·  array, two-pointers, sorting, quicksort
<https://leetcode.com/problems/sort-colors/>
**Level:** 3
<details><summary>Hint</summary>
Three pointers low/mid/high; swap 0s to `low`, 2s to `high`, leave 1s in place.
</details>
<details><summary>Key idea & complexity</summary>
Dutch national flag partition, three pointers. `O(n)` time, `O(1)` space, one pass.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The classic three-class partition (Dutch national flag problem): `low` points to the next slot for zeros, `high` to the next slot for twos, `mid` is the index currently under inspection. When `nums[mid] == 0`, swap with `low` and advance **both** `low` and `mid` — safe, because the value swapped in from `low` is always already known to be a one (every position `mid` has passed in `[low, mid)` is a one by the invariant). When `nums[mid] == 2`, swap with `high` and move **only** `high` backwards, not `mid` — because the value swapped in from `high` is unknown and must be re-inspected. When `nums[mid] == 1`, only `mid` advances. This asymmetry between the `low` and `high` swaps is the problem's subtlest point and the commonest source of bugs. `O(n)` time in one pass (not two separate counting passes), `O(1)` space. Pitfall: advancing `mid` after a `high` swap gives a wrong answer, because the new `nums[mid]` value is then never inspected.
</details>

### 02.6.6  Partition Array According to Given Pivot  ·  LC #2161  ·  Medium  ·  array, two-pointers, simulation
<https://leetcode.com/problems/partition-array-according-to-given-pivot/>
**Level:** 3
<details><summary>Hint</summary>
Write the smaller elements from the front, the larger from the back, the pivot values in between.
</details>
<details><summary>Key idea & complexity</summary>
Three regions relative to the pivot, write pointers. `O(n)` time, `O(1)` extra space (into a new array).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A generalisation of `sort-colors` to an arbitrary pivot value instead of two fixed values (0, 2), with the additional requirement that the original **relative order** is preserved within each of the three groups. Use two write pointers into a new output array: one fills from the front with elements smaller than the pivot (in a single left-to-right pass, preserving order automatically); pivot-valued elements are written into the middle at a position computed in the same pass (after the count of smaller elements); larger elements are written from the end with a separate pointer while traversing right to left (or collected into a separate list and appended at the end). Because order must be preserved, an in-place three-pointer `sort-colors`-style swap is not sufficient as such — it would scramble the relative order. `O(n)` time, `O(n)` space if a new array is allowed (many implementations require this precisely to preserve order). Pitfall: the pivot value itself belongs to the middle group, not to either extreme.
</details>

### 02.6.7  Wiggle Sort II  ·  LC #324  ·  Medium  ·  array, divide-and-conquer, greedy, sorting
<https://leetcode.com/problems/wiggle-sort-ii/>
**Level:** 3
<details><summary>Hint</summary>
Sort, split at the median, put the smaller half at odd indices and the larger half at even indices.
</details>
<details><summary>Key idea & complexity</summary>
Sort + partition around the median, alternating. `O(n log n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
We want an order where `nums[0] < nums[1] > nums[2] < nums[3] ...` strictly (no ties between neighbours). Simple alternation on the sorted array fails with duplicates, because equal values can end up adjacent. Solution: sort the array, split it in two at the median (`smallerHalf`, `largerHalf`), and place the **smaller half in reverse order at the odd indices** and the **larger half in reverse order at the even indices**. The reverse order within each half ensures that potential duplicates (which cluster around the median, at the halves' boundaries) are pushed as far apart in index as possible instead of ending up adjacent. `O(n log n)` time (sorting dominates), `O(n)` space for storing the halves and the result (`O(n)`-time median selection without full sorting is possible but considerably more complex). Pitfall: the reverse order within the halves (not forward order) is the solution's critical detail for separating duplicates.
</details>

### 02.6.8  Kth Largest Element in an Array  ·  LC #215  ·  Medium  ·  array, divide-and-conquer, sorting, heap-priority-queue
<https://leetcode.com/problems/kth-largest-element-in-an-array/>
**Level:** 3
<details><summary>Hint</summary>
Partition around a pivot as in quicksort, but recurse only into the side where `k` lies.
</details>
<details><summary>Key idea & complexity</summary>
Quickselect: partition without full sorting. `O(n)` average time, `O(n^2)` worst case.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
`Quickselect` applies `quicksort`'s partition routine but recurses into only **one** side at a time, not both. Choose a pivot, partition the array in place (smaller to the left, larger to the right, like the two-region version of `sort-colors`), and check the pivot's final index `p`: if `p` is exactly the sought `k`-th index, the answer is found; if `k < p`, recurse only into the left partition; if `k > p`, only into the right. Because only one side is recursed into, the expected total work is `O(n) + O(n/2) + O(n/4) + ... = O(n)` with random pivot selection — a significant improvement over full sorting at `O(n log n)`, because the problem does not require sorting the whole array, only the correct position of one element. The worst case is `O(n^2)` with a bad pivot choice (e.g. always the smallest/largest on already-sorted input), which random pivot selection makes unlikely in practice. `O(1)` extra space (in-place partition). Pitfall: "`k`-th largest" usually corresponds to index `n-k` in ascending order — check the direction carefully.
</details>

---

## Progress

- [ ] 02.1.1 Reverse String (LC #344)
- [ ] 02.1.2 Reverse Vowels of a String (LC #345)
- [ ] 02.1.3 Valid Palindrome (LC #125)
- [ ] 02.1.4 Valid Palindrome II (LC #680)
- [ ] 02.1.5 Squares of a Sorted Array (LC #977)
- [ ] 02.1.6 Two Sum II - Input Array Is Sorted (LC #167)
- [ ] 02.1.7 Container With Most Water (LC #11)
- [ ] 02.1.8 3Sum (LC #15)
- [ ] 02.1.9 Trapping Rain Water (LC #42)
- [ ] 02.2.1 Middle of the Linked List (LC #876)
- [ ] 02.2.2 Linked List Cycle (LC #141)
- [ ] 02.2.3 Happy Number (LC #202)
- [ ] 02.2.4 Palindrome Linked List (LC #234)
- [ ] 02.2.5 Find the Duplicate Number (LC #287)
- [ ] 02.2.6 Linked List Cycle II (LC #142)
- [ ] 02.2.7 Reorder List (LC #143)
- [ ] 02.2.8 Circular Array Loop (LC #457)
- [ ] 02.3.1 Maximum Average Subarray I (LC #643)
- [ ] 02.3.2 Contains Duplicate II (LC #219)
- [ ] 02.3.3 K Radius Subarray Averages (LC #2090)
- [ ] 02.3.4 Number of Sub-arrays of Size K and Average Greater than or Equal to Threshold (LC #1343)
- [ ] 02.3.5 Maximum Number of Vowels in a Substring of Given Length (LC #1456)
- [ ] 02.3.6 Maximum Points You Can Obtain from Cards (LC #1423)
- [ ] 02.3.7 Grumpy Bookstore Owner (LC #1052)
- [ ] 02.3.8 Sliding Window Maximum (LC #239)
- [ ] 02.4.1 Max Consecutive Ones (LC #485)
- [ ] 02.4.2 Longest Substring Without Repeating Characters (LC #3)
- [ ] 02.4.3 Minimum Size Subarray Sum (LC #209)
- [ ] 02.4.4 Max Consecutive Ones III (LC #1004)
- [ ] 02.4.5 Subarray Product Less Than K (LC #713)
- [ ] 02.4.6 Count Number of Nice Subarrays (LC #1248)
- [ ] 02.4.7 Binary Subarrays With Sum (LC #930)
- [ ] 02.4.8 Frequency of the Most Frequent Element (LC #1838)
- [ ] 02.5.1 Permutation in String (LC #567)
- [ ] 02.5.2 Find All Anagrams in a String (LC #438)
- [ ] 02.5.3 Maximum Erasure Value (LC #1695)
- [ ] 02.5.4 Fruit Into Baskets (LC #904)
- [ ] 02.5.5 Longest Repeating Character Replacement (LC #424)
- [ ] 02.5.6 Minimum Window Substring (LC #76)
- [ ] 02.5.7 Subarrays with K Different Integers (LC #992)
- [ ] 02.6.1 Sort Array By Parity (LC #905)
- [ ] 02.6.2 Sort Array By Parity II (LC #922)
- [ ] 02.6.3 Move Zeroes (LC #283)
- [ ] 02.6.4 Remove Duplicates from Sorted Array II (LC #80)
- [ ] 02.6.5 Sort Colors (LC #75)
- [ ] 02.6.6 Partition Array According to Given Pivot (LC #2161)
- [ ] 02.6.7 Wiggle Sort II (LC #324)
- [ ] 02.6.8 Kth Largest Element in an Array (LC #215)
