# Chapter 07 — Problems

How to work these:

1. Read the matching unit section in `lesson.md` first. Write the **state / transition / base case / order / answer** for the problem in five lines on paper before touching the keyboard.
2. Attempt the problem in C in `algorithms_learning/07_dynamic_programming/solutions/<slug>.c` — a folder you create. Write your own tests in `main()`: the LeetCode examples, `n = 1`, an empty or all-negative input, and one input where a greedy approach is wrong. Print the `dp` table for a tiny input and check it by hand. Compile with `cc -Wall -Wextra -std=c11 -O2`.
3. Only then open **Hint**. Only after solving (or after 30 minutes stuck) open **Approach**.
4. Tick the box in **Progress** when your C solution passes your tests. Come back later and redo the Level 4 ones from memory.

The Level number (1–5) is the source's difficulty rating within this course, independent of LeetCode's Easy/Medium/Hard label.

---

## Unit 1 — 1D DP: state, transition, base case

### 07.1.1  Climbing Stairs  ·  LC #70  ·  Easy  ·  math, dynamic-programming, memoization
<https://leetcode.com/problems/climbing-stairs/>
**Level:** 2
<details><summary>Hint</summary>
Step i can only be reached from steps i-1 and i-2.
</details>
<details><summary>Key idea & complexity</summary>
1D DP, dp[i] = number of ways to reach step i, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = number of ways to reach step i when each move climbs 1 or 2 steps. Transition: `dp[i] = dp[i-1] + dp[i-2]`, because the last move was either one or two steps. Base case: `dp[0] = 1` (the empty path), `dp[1] = 1`. This is the Fibonacci sequence shifted by one. Because the transition uses only the last two values, no array is needed — two variables suffice and space drops to O(1). The most common mistake is mixing up `dp[0]` and `dp[1]` as base cases, or forgetting that step counts n = 0 and n = 1 are both trivially one-way cases.
</details>

### 07.1.2  Min Cost Climbing Stairs  ·  LC #746  ·  Easy  ·  array, dynamic-programming
<https://leetcode.com/problems/min-cost-climbing-stairs/>
**Level:** 2
<details><summary>Hint</summary>
You may start from step 0 or step 1 for free.
</details>
<details><summary>Key idea & complexity</summary>
1D DP, dp[i] = minimum cost to reach step i, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = minimum total cost to reach step i (standing on a step costs `cost[i]`, but jumping off it is free). Transition: `dp[i] = min(dp[i-1] + cost[i-1], dp[i-2] + cost[i-2])`, because the previous step had to be paid for before jumping off it. Base case: `dp[0] = dp[1] = 0`, because starting from either is allowed for free. The answer is `dp[n]`, where n is the "top" of the staircase, one step past the last one. Pitfall: the cost is paid at the **source** step, not the destination — many people index this backwards.
</details>

### 07.1.3  N-th Tribonacci Number  ·  LC #1137  ·  Easy  ·  math, dynamic-programming, memoization
<https://leetcode.com/problems/n-th-tribonacci-number/>
**Level:** 2
<details><summary>Hint</summary>
Same idea as Fibonacci, but three predecessors instead of two.
</details>
<details><summary>Key idea & complexity</summary>
1D DP, dp[i] = i-th Tribonacci number, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = the i-th number of the Tribonacci sequence. Transition: `dp[i] = dp[i-1] + dp[i-2] + dp[i-3]`. Base case: `dp[0] = 0, dp[1] = 1, dp[2] = 1`. Because the transition looks at only the last three values, keeping three variables instead of an array suffices — O(1) space. The problem is a good exercise in seeing that the "window size" of the transition (here 3) directly determines how many previous states must be kept in memory; a larger window does not mean a more complex DP, just more variables.
</details>

### 07.1.4  House Robber  ·  LC #198  ·  Medium  ·  array, dynamic-programming
<https://leetcode.com/problems/house-robber/>
**Level:** 3
<details><summary>Hint</summary>
At every house there are two options: rob it or skip it.
</details>
<details><summary>Key idea & complexity</summary>
1D DP, dp[i] = best loot from houses up to i, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = the largest sum obtainable by robbing among houses 0..i with no two adjacent houses. Transition: `dp[i] = max(dp[i-1], dp[i-2] + nums[i])` — either skip house i (value `dp[i-1]`) or rob it, in which case house i-1 cannot have been robbed (value `dp[i-2] + nums[i]`). Base case: `dp[-1] = 0`, `dp[0] = nums[0]` (or two helper variables starting at zero before the loop). Space reduces to two variables. Classic mistake: greedily picking the larger of two neighbouring houses without DP — this fails because a locally better choice can exclude a better pair later on.
</details>

### 07.1.5  Maximum Subarray  ·  LC #53  ·  Medium  ·  array, divide-and-conquer, dynamic-programming
<https://leetcode.com/problems/maximum-subarray/>
**Level:** 3
<details><summary>Hint</summary>
When the running sum goes negative, it pays to reset it.
</details>
<details><summary>Key idea & complexity</summary>
1D DP (Kadane), dp[i] = largest sum of a subarray ending at i, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = the largest sum of a contiguous subarray that **ends exactly at index** i. Transition: `dp[i] = max(a[i], dp[i-1] + a[i])` — either start a new subarray at i, or extend the previous one. Base case: `dp[0] = a[0]`. The final answer is `max(dp)`, not `dp[n-1]`, because the best subarray can end anywhere. Space reduces to a single running variable (Kadane's algorithm). Pitfall: forgetting to keep a separate `best` variable and accidentally returning only the last `dp[i]`, which is wrong whenever the best subarray does not extend to the end.
</details>

### 07.1.6  Decode Ways  ·  LC #91  ·  Medium  ·  string, dynamic-programming
<https://leetcode.com/problems/decode-ways/>
**Level:** 3
<details><summary>Hint</summary>
The last group is either a single digit or two digits in the range 10–26.
</details>
<details><summary>Key idea & complexity</summary>
1D DP, dp[i] = number of ways to decode the prefix of length i, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = number of ways to decode the first i characters of the string as a digit-to-letter code (A=1..Z=26). Transition: `dp[i] += dp[i-1]` if the single digit `s[i-1]` is 1–9; `dp[i] += dp[i-2]` if the two-digit `s[i-2..i-1]` is 10–26. Base case: `dp[0] = 1` (the empty prefix decodes in exactly one way). Zero is the trap in this problem: a lone '0' encodes no letter, so it is valid only as part of the two-digit group 10 or 20 — if `s[i-1] = '0'` and the preceding pair is not 10 or 20, the whole prefix is undecodable and `dp[i] = 0` propagates forward.
</details>

### 07.1.7  Delete and Earn  ·  LC #740  ·  Medium  ·  array, hash-table, dynamic-programming
<https://leetcode.com/problems/delete-and-earn/>
**Level:** 3
<details><summary>Hint</summary>
First transform the problem into an array points[v] = v times its count, then solve it like House Robber.
</details>
<details><summary>Key idea & complexity</summary>
House Robber transform: build a points table by value, then 1D DP, O(n + k) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The transform is the key: build `points[v] = v * (occurrences of value v)` for every value v. Because choosing the number v deletes all v-1 and v+1, the problem is exactly House Robber on the `points` array between adjacent values. State: `dp[v]` = best point total using values 0..v. Transition: `dp[v] = max(dp[v-1], dp[v-2] + points[v])`. Base case: `dp[0] = points[0]`, `dp[-1] = 0`. Time O(n + k), where k is the size of the value range. Pitfall: if you try to solve it directly on the indices of the sequence rather than on the array regrouped by value, the transition's "adjacency" (values v-1, v+1) gets confused with list adjacency.
</details>

### 07.1.8  Longest Increasing Subsequence  ·  LC #300  ·  Medium  ·  array, binary-search, dynamic-programming, longest-increasing-subsequence
<https://leetcode.com/problems/longest-increasing-subsequence/>
**Level:** 3
<details><summary>Hint</summary>
Start from the O(n²) version where dp[i] looks at all j < i, then think about why the smallest possible tail value is enough.
</details>
<details><summary>Key idea & complexity</summary>
O(n²) DP, dp[i] = length of the longest increasing subsequence ending at i; improvable to O(n log n) with patience sorting.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i]` = length of the longest increasing subsequence that **ends at index** i. Transition: `dp[i] = 1 + max(dp[j])` over all j < i with `a[j] < a[i]`; if no such j exists, `dp[i] = 1`. Base case: `dp[i] = 1` for every i. The answer is `max(dp)`. This is O(n²). A better solution maintains an array `tails`, where `tails[k]` is the smallest possible tail value of an increasing subsequence of length k+1; each element is placed at the correct position by binary search, giving O(n log n) time. Pitfall: the `tails` array is not an actual subsequence but an optimistic helper structure — its length, however, is the correct answer.
</details>

---

## Unit 2 — Decision DP: take or skip

### 07.2.1  Best Time to Buy and Sell Stock  ·  LC #121  ·  Easy  ·  array, dynamic-programming
<https://leetcode.com/problems/best-time-to-buy-and-sell-stock/>
**Level:** 2
<details><summary>Hint</summary>
Keep track of the smallest price seen so far.
</details>
<details><summary>Key idea & complexity</summary>
1D DP / running minimum, dp[i] = best profit selling on day i, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `minPrice` = the smallest price over indices 0..i-1, and `best` = the largest profit so far. Transition: for every i, `best = max(best, price[i] - minPrice)`, then `minPrice = min(minPrice, price[i])`. This is a special case of the general buy-sell DP in which only one trade is allowed: the "holding" state is always "bought at the cheapest price seen". Base case: `minPrice = price[0]`, `best = 0`. Pitfall: update order — if `minPrice` is updated before computing `best` in the same iteration, you would accidentally compute a profit from selling on the same day you bought.
</details>

### 07.2.2  House Robber II  ·  LC #213  ·  Medium  ·  array, dynamic-programming
<https://leetcode.com/problems/house-robber-ii/>
**Level:** 3
<details><summary>Hint</summary>
Houses 0 and n-1 are neighbours — split the problem into two linear cases.
</details>
<details><summary>Key idea & complexity</summary>
Two House Robber runs (without the first / without the last house), O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The circular constraint (the first and last house cannot both be included) is decomposed into two separate linear House Robber problems: one over houses 0..n-2 (the last is always left out) and another over houses 1..n-1 (the first is always left out). The answer is the larger of the two. Each run uses the same state/transition/base case as the basic problem. Special case n = 1: the answer is the value of the only house. Pitfall: many try to solve the circular case in a single DP run with an extra state "was house 0 chosen" — that works but is needlessly complex compared to two straightforward runs.
</details>

### 07.2.3  Best Time to Buy and Sell Stock II  ·  LC #122  ·  Medium  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/best-time-to-buy-and-sell-stock-ii/>
**Level:** 3
<details><summary>Hint</summary>
With unlimited trades it suffices to collect every positive price increase.
</details>
<details><summary>Key idea & complexity</summary>
Greedy / DP with a state machine (hold, sold): sum all positive daily changes, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][hold]` = best profit up to day i, where `hold` says whether we own the share. Transition: `dp[i][1] = max(dp[i-1][1], dp[i-1][0] - price[i])`, `dp[i][0] = max(dp[i-1][0], dp[i-1][1] + price[i])`. Because trades are unlimited, this simplifies to a greedy: sum `max(0, price[i] - price[i-1])` over every day. Base case: `dp[0][0] = 0, dp[0][1] = -price[0]`. Pitfall: this is the only stock variant where greedy and DP give the same result — with a cooldown or a transaction fee the greedy no longer suffices, because consecutive trades are no longer independent of each other.
</details>

### 07.2.4  Best Time to Buy and Sell Stock with Cooldown  ·  LC #309  ·  Medium  ·  array, dynamic-programming
<https://leetcode.com/problems/best-time-to-buy-and-sell-stock-with-cooldown/>
**Level:** 3
<details><summary>Hint</summary>
After a sale you need an intermediate state from which you cannot buy on the very next day.
</details>
<details><summary>Key idea & complexity</summary>
3-state DP (hold, sold, rest), O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The state machine has three states: `hold` (owns the share), `sold` (sold today), `rest` (owns nothing, did not sell yesterday — may buy). Transitions: `hold[i] = max(hold[i-1], rest[i-1] - price[i])`, `sold[i] = hold[i-1] + price[i]`, `rest[i] = max(rest[i-1], sold[i-1])`. The cooldown is modelled by the fact that from `sold[i-1]` you cannot go straight to buying — you must pass through the `rest` state. Base case: `hold[0] = -price[0]`, `sold[0] = -∞`, `rest[0] = 0`. Answer: `max(sold[n-1], rest[n-1])`. Pitfall: if the cooldown state is not separated from `rest`, the cooldown constraint vanishes unnoticed.
</details>

### 07.2.5  Best Time to Buy and Sell Stock with Transaction Fee  ·  LC #714  ·  Medium  ·  array, dynamic-programming, greedy
<https://leetcode.com/problems/best-time-to-buy-and-sell-stock-with-transaction-fee/>
**Level:** 3
<details><summary>Hint</summary>
Subtract the fee consistently in only one of the two transitions, not both.
</details>
<details><summary>Key idea & complexity</summary>
2-state DP (hold, cash) subtracting the fee on buy or on sell, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same two-state machine as in the unlimited version, but `-fee` is added to one transition: `cash[i] = max(cash[i-1], hold[i-1] + price[i] - fee)`, `hold[i] = max(hold[i-1], cash[i-1] - price[i])`. The fee can equally be subtracted at buy time — as long as it is subtracted **exactly once per trade**. Base case: `cash[0] = 0`, `hold[0] = -price[0]`. Answer: `cash[n-1]`. Pitfall: the fee is accidentally subtracted on both buy and sell, so every trade pays double and the profit is systematically underestimated.
</details>

### 07.2.6  Best Time to Buy and Sell Stock III  ·  LC #123  ·  Hard  ·  array, dynamic-programming
<https://leetcode.com/problems/best-time-to-buy-and-sell-stock-iii/>
**Level:** 4
<details><summary>Hint</summary>
Same state machine as II, but extend the state to record how many trades have been made.
</details>
<details><summary>Key idea & complexity</summary>
DP with extra state = number of trades made (max 2), O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][k][hold]` = best profit up to day i having made at most k trades (k ∈ {0, 1, 2}), with `hold` saying whether we own the share. Transitions are the same as in the basic case, but every "buy" transition consumes one trade from the budget: `dp[i][k][1] = max(dp[i-1][k][1], dp[i-1][k-1][0] - price[i])`. Base case: `dp[0][k][0] = 0`, `dp[0][k][1] = -price[0]`. Answer: `dp[n-1][2][0]`. In practice four variables suffice (`buy1, sell1, buy2, sell2`). Pitfall: the `buy2` transition must use the `sell1` value from the same day *before* it is updated — the update order inside the loop is significant.
</details>

### 07.2.7  Best Time to Buy and Sell Stock IV  ·  LC #188  ·  Hard  ·  array, dynamic-programming
<https://leetcode.com/problems/best-time-to-buy-and-sell-stock-iv/>
**Level:** 4
<details><summary>Hint</summary>
When k >= n/2, unlimited trading suffices — otherwise extend the state of III.
</details>
<details><summary>Key idea & complexity</summary>
IV generalises III to arbitrary k, dp[i][k][hold], O(nk) time, O(k) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A generalisation of the previous problem: the state `dp[i][k][hold]` with the same formula, but k is looped over 1..K. Time O(nK); space reducible to O(K) with two rows `buy[k], sell[k]`. Important optimisation: if K ≥ n/2, trades can never be made more than ⌊n/2⌋ times (every trade needs at least two days), so the problem reduces to the unlimited version. Pitfall: if this special case is not handled and K is given as a huge number, the table `dp[i][k]` does not fit in memory and the time is not enough either.
</details>

---

## Unit 3 — Knapsack & subset sums

### 07.3.1  Coin Change  ·  LC #322  ·  Medium  ·  array, dynamic-programming, breadth-first-search, knapsack-problem
<https://leetcode.com/problems/coin-change/>
<details><summary>Hint</summary>
dp[x] = 1 + min over all coins c of dp[x-c].
</details>
**Level:** 3
<details><summary>Key idea & complexity</summary>
Unbounded knapsack, dp[x] = fewest coins summing to x, O(n · amount) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[x]` = the smallest number of coins that sum to exactly x (∞ if impossible). Transition: `dp[x] = 1 + min_c dp[x-c]` over all c ≤ x. Base case: `dp[0] = 0`. Because the same coin may appear many times, this is the unbounded knapsack. The answer is `dp[amount]`, or -1 if it is still ∞. Pitfall: the initialisation to ∞ must happen before the computation, and the final result must be checked separately for unreachability — the greedy (take the largest coin) fails on non-canonical coin sets.
</details>

### 07.3.2  Perfect Squares  ·  LC #279  ·  Medium  ·  math, dynamic-programming, breadth-first-search, knapsack-problem
<https://leetcode.com/problems/perfect-squares/>
**Level:** 3
<details><summary>Hint</summary>
The coins are now the perfect squares 1, 4, 9, 16, ...
</details>
<details><summary>Key idea & complexity</summary>
Unbounded knapsack with square numbers, dp[n] = fewest squares, O(n √n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same skeleton as Coin Change, but the "coins" are all perfect squares 1, 4, 9, ... ≤ n. State: `dp[i]` = the fewest square numbers whose sum is i. Transition: `dp[i] = 1 + min_{j² ≤ i} dp[i - j²]`. Base case: `dp[0] = 0`. There are O(√n) squares per i, so the total time is O(n √n). Pitfall: the greedy (always take the largest square that fits) fails — e.g. 12 = 4 + 4 + 4 (3 squares) beats the greedy's 9 + 1 + 1 + 1 (4 squares).
</details>

### 07.3.3  Combination Sum IV  ·  LC #377  ·  Medium  ·  array, dynamic-programming
<https://leetcode.com/problems/combination-sum-iv/>
**Level:** 3
<details><summary>Hint</summary>
Despite the name, this counts ordered sequences, not combinations.
</details>
<details><summary>Key idea & complexity</summary>
Unbounded knapsack, counting ways as permutations, dp[t] = number of ways to reach sum t with order taken into account, O(t · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[t]` = number of ordered sequences (elements may repeat) whose sum is t. Transition: `dp[t] = Σ_num dp[t - num]` over all `num` ≤ t — the sum is the **outer** loop and the numbers the inner one, so order is counted (permutations). Base case: `dp[0] = 1`. This is the important counterpart to Coin Change II, where the loop order is reversed and combinations are counted. Pitfall: the problem's name is misleading — (1, 2) and (2, 1) are counted as two different ways, and that only works with the sum-outer loop order.
</details>

### 07.3.4  Partition Equal Subset Sum  ·  LC #416  ·  Medium  ·  array, dynamic-programming, knapsack-problem, 0-1-knapsack
<https://leetcode.com/problems/partition-equal-subset-sum/>
**Level:** 3
<details><summary>Hint</summary>
The question is: can some subset sum to exactly sum/2?
</details>
<details><summary>Key idea & complexity</summary>
0/1 knapsack, dp[s] = is sum s reachable, O(n · sum) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First observe: if the total sum is odd, the answer is immediately no. Otherwise, state: `dp[s]` = does some subset exist with sum s. Transition: for every number `num`, `dp[s] |= dp[s - num]`, and because every number may contribute only **once**, `s` is traversed **from large to small**. Base case: `dp[0] = true`. Answer: `dp[sum/2]`. Pitfall: if `s` is traversed from small to large instead, the same number can influence several `dp[s]` values within one item's pass and the problem accidentally becomes an unbounded knapsack — the result can still look correct on small inputs.
</details>

### 07.3.5  Last Stone Weight II  ·  LC #1049  ·  Medium  ·  array, dynamic-programming, knapsack-problem, 0-1-knapsack
<https://leetcode.com/problems/last-stone-weight-ii/>
**Level:** 3
<details><summary>Hint</summary>
Split the stones into two piles so that the difference between the piles is as small as possible — that is the same as subset sum.
</details>
<details><summary>Key idea & complexity</summary>
0/1 knapsack as subset sum, minimise |sum − 2 · closest reachable sum|, O(n · sum) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The problem is equivalent to subset sum: split the stones into two groups A and B (A + B = sum) so that |A − B| is as small as possible. State: `dp[s]` = is a subset sum s reachable, s ∈ [0, sum/2]. Transition and iteration order are the same as in Partition Equal Subset Sum: `s` from large to small. Once the table is filled, find the largest reachable s ≤ sum/2; the answer is `sum - 2*s`. Base case: `dp[0] = true`. Pitfall: the problem is disguised as a "stone smashing" simulation, but a greedy simulation does not give the optimum — the correct solution is subset sum.
</details>

### 07.3.6  Ones and Zeroes  ·  LC #474  ·  Medium  ·  array, string, dynamic-programming, knapsack-problem
<https://leetcode.com/problems/ones-and-zeroes/>
**Level:** 3
<details><summary>Hint</summary>
Two capacities instead of one — a budget of zeros and a budget of ones.
</details>
<details><summary>Key idea & complexity</summary>
2D 0/1 knapsack (budget of zeros and ones), dp[i][j] = most strings with i zeros and j ones, O(L · m · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the largest set of strings that fits using at most i zeros and j ones. Every string is an "item" whose "weight" is two-dimensional: (number of zeros, number of ones). Transition: for every string with weight (z, o), `dp[i][j] = max(dp[i][j], dp[i-z][j-o] + 1)`, and both capacities are traversed from large to small (0/1 knapsack in two dimensions). Base case: `dp[0][0] = 0` everywhere. Time O(L · m · n). Pitfall: if either capacity is traversed in the wrong order, a single string can be counted more than once within one item pass.
</details>

### 07.3.7  Target Sum  ·  LC #494  ·  Medium  ·  array, dynamic-programming, backtracking, knapsack-problem
<https://leetcode.com/problems/target-sum/>
**Level:** 3
<details><summary>Hint</summary>
Split the numbers into a plus group and a minus group: P − N = target, P + N = sum, so P = (sum + target)/2.
</details>
<details><summary>Key idea & complexity</summary>
0/1 knapsack transformed into subset sum (+/− signs), dp[s] = number of ways, O(n · sum) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
An algebraic transform turns this into subset sum: if P is the sum of the plus-signed numbers and N the sum of the minus-signed ones, then P − N = target and P + N = sum, so P = (sum + target)/2. State: `dp[s]` = number of ways to choose a subset with sum s. Transition: `dp[s] += dp[s - num]` per number, `s` from large to small. Base case: `dp[0] = 1`. Answer: `dp[P]`. Pitfall: if (sum + target) is odd or |target| > sum, the answer is immediately 0 — this boundary condition is easily forgotten before running the DP.
</details>

### 07.3.8  Coin Change II  ·  LC #518  ·  Medium  ·  array, dynamic-programming, knapsack-problem, complete-knapsack
<https://leetcode.com/problems/coin-change-ii/>
**Level:** 3
<details><summary>Hint</summary>
Compare with Combination Sum IV — here the loop order is reversed.
</details>
<details><summary>Key idea & complexity</summary>
Unbounded knapsack, counting ways as combinations, dp[x] = number of ways to reach sum x regardless of order, O(n · amount) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[x]` = number of ways to form sum x with the given coins when **order does not matter**. Transition: the coin is the **outer** loop, the sum `x` the inner one, traversed from small to large: `dp[x] += dp[x - coin]`. Base case: `dp[0] = 1`. The loop order is the whole core of the problem: if the sum were outer and the coin inner, the same set of coins in different orders would be counted many times. Pitfall: this is the direct mirror image of Combination Sum IV — solve both side by side and compare why the loop order changes the meaning of the answer.
</details>

---

## Unit 4 — Grid DP

### 07.4.1  Unique Paths  ·  LC #62  ·  Medium  ·  math, dynamic-programming, combinatorics
<https://leetcode.com/problems/unique-paths/>
**Level:** 3
<details><summary>Hint</summary>
A cell can only be entered from above or from the left.
</details>
<details><summary>Key idea & complexity</summary>
2D DP, dp[y][x] = number of paths to cell (y, x), O(mn) time, reducible to O(n) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[y][x]` = number of distinct paths from the start (0, 0) to cell (y, x) when moving only right or down. Transition: `dp[y][x] = dp[y-1][x] + dp[y][x-1]`. Base case: the first row and column are all 1. Because row y depends only on row y-1, the space reduces to a single row-length array — O(n) space in O(mn) time. A closed-form solution C(m+n−2, m−1) exists, but the DP form generalises directly to obstacles, whereas the formula does not generalise as easily.
</details>

### 07.4.2  Unique Paths II  ·  LC #63  ·  Medium  ·  array, dynamic-programming, matrix
<https://leetcode.com/problems/unique-paths-ii/>
**Level:** 3
<details><summary>Hint</summary>
An obstacle makes dp[y][x] = 0 regardless of the neighbours.
</details>
<details><summary>Key idea & complexity</summary>
Same 2D DP as Unique Paths, with dp = 0 in obstacle cells, O(mn) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same skeleton as Unique Paths, but if cell (y, x) contains an obstacle, `dp[y][x] = 0` regardless of what the neighbours hold. The transition is otherwise the same: `dp[y][x] = dp[y-1][x] + dp[y][x-1]` in obstacle-free cells. Base case: the first row/column is 1 up to the obstacle and 0 after it (the obstacle cuts off the entire rest of the row/column). Pitfall: the base values of the first row and column can no longer be assumed to be all ones as in the obstacle-free version — an obstacle in the first row makes every cell after it unreachable.
</details>

### 07.4.3  Minimum Path Sum  ·  LC #64  ·  Medium  ·  array, dynamic-programming, matrix
<https://leetcode.com/problems/minimum-path-sum/>
**Level:** 3
<details><summary>Hint</summary>
Same movement restriction as Unique Paths, but costs are summed and minimised.
</details>
<details><summary>Key idea & complexity</summary>
2D DP, dp[y][x] = minimum cost of a path to cell (y, x), O(mn) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[y][x]` = the minimum cost sum of a path from the start to cell (y, x). Transition: `dp[y][x] = grid[y][x] + min(dp[y-1][x], dp[y][x-1])`. Base case: `dp[0][0] = grid[0][0]`; the first row and column are cumulative sums. The answer is `dp[m-1][n-1]`. Structurally identical to Unique Paths — only the combining operation changes from sum to minimum plus the cost of the added cell. Pitfall: forgetting to initialise the first row/column as cumulative sums, so that the `min` operation tries to compare against a nonexistent neighbour.
</details>

### 07.4.4  Triangle  ·  LC #120  ·  Medium  ·  array, dynamic-programming
<https://leetcode.com/problems/triangle/>
**Level:** 3
<details><summary>Hint</summary>
Start from the bottom row and combine upwards — then the edge cases disappear.
</details>
<details><summary>Key idea & complexity</summary>
2D DP bottom-up, dp[j] = minimum path sum from the row downwards, O(n²) time, O(n) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State (bottom-up): `dp[j]` = the minimum sum of a path from node j on row i down to the bottom of the triangle. Transition: `dp[j] = triangle[i][j] + min(dp[j], dp[j+1])`, computed in place in the same array one row at a time from the bottom upwards. Base case: the bottom row is copied as-is into `dp`. The answer at the end is `dp[0]`. The direction is deliberately bottom-up: top-down, every cell except the edges would need two possible predecessors, and the edge nodes only one — special cases. Bottom-up, every node always has exactly two successors.
</details>

### 07.4.5  Maximal Square  ·  LC #221  ·  Medium  ·  array, dynamic-programming, matrix
<https://leetcode.com/problems/maximal-square/>
**Level:** 3
<details><summary>Hint</summary>
The side of a square is always limited by the smallest of three neighbours: above, left, above-left.
</details>
<details><summary>Key idea & complexity</summary>
2D DP, dp[y][x] = side of the largest square whose bottom-right corner is (y, x), O(mn) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[y][x]` = the side length of the largest all-ones square whose **bottom-right corner** is cell (y, x). Transition: `dp[y][x] = 1 + min(dp[y-1][x], dp[y][x-1], dp[y-1][x-1])` — the square can grow only as far as all three neighbours allow. Base case: the first row and column are `dp[y][x] = grid[y][x]`. The answer is `max(dp)²`. Pitfall: confusing "largest square" with "largest rectangle" — the latter needs a different, more complex (stack-based) algorithm.
</details>

### 07.4.6  Out of Boundary Paths  ·  LC #576  ·  Medium  ·  dynamic-programming
<https://leetcode.com/problems/out-of-boundary-paths/>
**Level:** 3
<details><summary>Hint</summary>
The third dimension is the number of moves remaining.
</details>
<details><summary>Key idea & complexity</summary>
3D DP, dp[k][y][x] = number of paths leaving the grid in k moves from cell (y, x), O(k · m · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[k][y][x]` = number of ways to leave the grid in exactly k moves starting from cell (y, x). Transition: `dp[k][y][x] = Σ` of the neighbours' `dp[k-1][...]` over the four directions, and if a neighbour lies outside the grid it counts as one exit. Base case: `dp[0][start_y][start_x] = 1`, 0 elsewhere. The answer is the accumulation of all exits in ≤ maxMove moves. The third dimension k is necessary because the same cell can be reached with many different move counts. Remember the modulo 10⁹ + 7 in the summation.
</details>

### 07.4.7  Dungeon Game  ·  LC #174  ·  Hard  ·  array, dynamic-programming, matrix
<https://leetcode.com/problems/dungeon-game/>
**Level:** 4
<details><summary>Hint</summary>
Compute backwards from the goal to the start — then the minimum remaining health is well defined.
</details>
<details><summary>Key idea & complexity</summary>
2D DP backwards (bottom-right → top-left), dp[y][x] = minimum required health, O(mn) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[y][x]` = the smallest number of health points needed at cell (y, x), before that cell's effect, so that the character survives to the goal. Because this depends on the future cells, the computation proceeds backwards: from the bottom-right corner to the top-left. Transition: `need = min(dp[y+1][x], dp[y][x+1]) - dungeon[y][x]`, and `dp[y][x] = max(1, need)` (health must never drop below 1). Base case: the fictitious cells beyond the goal are 1. Pitfall: forward computation does not work, because the required health depends on the entire rest of the path — the direction must be reversed.
</details>

### 07.4.8  Cherry Pickup  ·  LC #741  ·  Hard  ·  array, dynamic-programming, matrix
<https://leetcode.com/problems/cherry-pickup/>
**Level:** 4
<details><summary>Hint</summary>
Model the two consecutive trips as one trip of two simultaneous walkers with the same number of steps.
</details>
<details><summary>Key idea & complexity</summary>
3D DP for two simultaneous walkers, dp[step][x1][x2] = best cherry total, O(n³) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two separate trips (out and back) correspond to two walkers walking **simultaneously** from start to goal — because both have the same step count t = y + x, the state reduces to `dp[t][x1][x2]`. The transition iterates over all 2 × 2 = 4 direction combinations for the two walkers and takes the best; if both are in the same cell, the cherry is counted only once. Base case: `dp[0][0][0] = grid[0][0]`. An obstacle cell makes the state unreachable (−∞). Pitfall: if the trips are modelled sequentially rather than simultaneously, the state grows too large — simultaneity is the whole insight of the problem.
</details>

---

## Unit 5 — String DP

### 07.5.1  Longest Palindromic Substring  ·  LC #5  ·  Medium  ·  two-pointers, string, dynamic-programming, manacher
<https://leetcode.com/problems/longest-palindromic-substring/>
**Level:** 3
<details><summary>Hint</summary>
Fill the table in order of increasing length, not row by row.
</details>
<details><summary>Key idea & complexity</summary>
2D interval DP, dp[i][j] = is s[i..j] a palindrome, O(n²) time, O(n²) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = is the substring `s[i..j]` a palindrome. Transition: `dp[i][j] = (s[i] == s[j]) && (j - i < 2 || dp[i+1][j-1])`. Base case: single-character intervals are always true, two-character intervals are true if the characters match. The fill order is decisive: increasing length, because `dp[i][j]` needs the inner interval already computed — the ordinary row-by-row order would go in the wrong direction. The alternative O(n²)-time O(1)-space solution (expand around the centre) is faster in practice, but the DP table generalises directly to the Palindromic Substrings problem.
</details>

### 07.5.2  Palindromic Substrings  ·  LC #647  ·  Medium  ·  two-pointers, string, dynamic-programming
<https://leetcode.com/problems/palindromic-substrings/>
**Level:** 3
<details><summary>Hint</summary>
Same table as in the previous problem — this time count the true values instead of finding the longest.
</details>
<details><summary>Key idea & complexity</summary>
Same 2D interval DP as Longest Palindromic Substring; count the true values, O(n²) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Identical state and transition to Longest Palindromic Substring: `dp[i][j] = (s[i] == s[j]) && (j - i < 2 || dp[i+1][j-1])`, filled in order of increasing interval length. The difference is the goal: instead of looking for the longest true interval, count the number of **all** true intervals. Base case as before. Time O(n²), space O(n²). This pair is a good example of one DP table answering many questions — only the final aggregation changes.
</details>

### 07.5.3  Longest Common Subsequence  ·  LC #1143  ·  Medium  ·  string, dynamic-programming, longest-common-subsequence
<https://leetcode.com/problems/longest-common-subsequence/>
**Level:** 3
<details><summary>Hint</summary>
If the last characters match, they always belong to some optimal LCS.
</details>
<details><summary>Key idea & complexity</summary>
2D DP over two sequences, dp[i][j] = LCS length of the prefixes, O(nm) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the length of the longest common subsequence of `A[0..i)` and `B[0..j)`. Transition: if `A[i-1] == B[j-1]`, `dp[i][j] = dp[i-1][j-1] + 1`; otherwise `dp[i][j] = max(dp[i-1][j], dp[i][j-1])`. Base case: `dp[0][j] = dp[i][0] = 0`. Answer `dp[n][m]`. Pitfall: LCS is not the same as the longest common **substring** (contiguous) — a subsequence may skip over characters, which is the entire reason for the `max(dp[i-1][j], dp[i][j-1])` branch.
</details>

### 07.5.4  Edit Distance  ·  LC #72  ·  Medium  ·  string, dynamic-programming
<https://leetcode.com/problems/edit-distance/>
**Level:** 3
<details><summary>Hint</summary>
The three operations insert, delete, replace correspond to the three neighbours in the dp table.
</details>
<details><summary>Key idea & complexity</summary>
2D DP, dp[i][j] = edit distance between the prefixes A[..i], B[..j], O(nm) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the minimum number of operations to turn `A[0..i)` into `B[0..j)`. Transition: if `A[i-1] == B[j-1]`, `dp[i][j] = dp[i-1][j-1]`; otherwise `dp[i][j] = 1 + min(dp[i-1][j-1], dp[i-1][j], dp[i][j-1])`, corresponding to replace, delete and insert. Base case: `dp[0][j] = j`, `dp[i][0] = i`. Pitfall: forgetting that `dp[i-1][j]` and `dp[i][j-1]` correspond to different operations (delete vs insert) depending on which string is being transformed into which — keep the direction fixed throughout the solution.
</details>

### 07.5.5  Interleaving String  ·  LC #97  ·  Medium  ·  string, dynamic-programming
<https://leetcode.com/problems/interleaving-string/>
**Level:** 3
<details><summary>Hint</summary>
The third string C always has length i + j, so its index can be derived.
</details>
<details><summary>Key idea & complexity</summary>
2D DP, dp[i][j] = can A[..i] + B[..j] interleave to form C[..i+j], O(nm) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = can `C[0..i+j)` be formed by interleaving `A[0..i)` and `B[0..j)`. Transition: `dp[i][j] = (dp[i-1][j] && A[i-1] == C[i+j-1]) || (dp[i][j-1] && B[j-1] == C[i+j-1])` — the last character of C came from either A or B. Base case: `dp[0][0] = true`; `dp[i][0]` and `dp[0][j]` follow straightforwardly from prefix matching. Quick boundary check before the DP: if `len(A) + len(B) != len(C)`, the answer is immediately false. Pitfall: the index `i+j-1` into C is not updated correctly when i or j is 0.
</details>

### 07.5.6  Palindrome Partitioning II  ·  LC #132  ·  Hard  ·  string, dynamic-programming
<https://leetcode.com/problems/palindrome-partitioning-ii/>
**Level:** 4
<details><summary>Hint</summary>
First precompute which intervals are palindromes, then minimise the cuts using that table.
</details>
<details><summary>Key idea & complexity</summary>
1D DP on top of a palindrome table, dp[i] = fewest cuts for the prefix of length i, O(n²) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A two-phase solution: first fill the 2D table `isPal[i][j]` (the same as in Longest Palindromic Substring), then a 1D DP: `dp[i]` = the fewest cuts so that `s[0..i)` splits into palindromes only. Transition: `dp[i] = min over j < i with isPal[j][i-1] of dp[j] + 1`. Base case: `dp[0] = -1` (a counting trick so that the first whole-prefix palindrome gives `dp[i] = 0`). Time O(n²) for both phases. Pitfall: if the `isPal` table is not precomputed and palindromicity is re-checked every time, the total time grows to O(n³).
</details>

### 07.5.7  Distinct Subsequences  ·  LC #115  ·  Hard  ·  string, dynamic-programming
<https://leetcode.com/problems/distinct-subsequences/>
**Level:** 4
<details><summary>Hint</summary>
At every matching character pair there are two options: use it, or skip it in S.
</details>
<details><summary>Key idea & complexity</summary>
2D DP, dp[i][j] = number of ways to form T[..j] as a subsequence of S[..i], O(nm) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the number of ways `T[0..j)` occurs as a subsequence of `S[0..i)`. Transition: `dp[i][j] = dp[i-1][j]` (skip `S[i-1]`) `+ dp[i-1][j-1]` if `S[i-1] == T[j-1]`. Base case: `dp[i][0] = 1` for all i, `dp[0][j] = 0` for j > 0. Answer `dp[n][m]`. Pitfall: this is computed as a **number of ways**, not as a truth value like Interleaving String — plus, not OR, and at a match there are always two distinct ways which are both counted.
</details>

### 07.5.8  Shortest Common Supersequence  ·  LC #1092  ·  Hard  ·  string, dynamic-programming, longest-common-subsequence
<https://leetcode.com/problems/shortest-common-supersequence/>
**Level:** 4
<details><summary>Hint</summary>
Shortest common supersequence = both strings joined with the LCS shared only once.
</details>
<details><summary>Key idea & complexity</summary>
LCS-based construction: compute the LCS table, then build the string backwards, O(nm) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The solution rests on LCS: first compute `dp[i][j]` = LCS length. The length of the answer string is `len(A) + len(B) - LCS`. The string itself is built by walking the `dp` table backwards from (n, m) to (0, 0): if `A[i-1] == B[j-1]`, add this character once and move to (i-1, j-1); otherwise add the character whose row has the larger `dp` value and move to the corresponding neighbour. Whatever remains of either string is appended in full at the end. Pitfall: the constructed string is in reverse order and must be reversed at the end.
</details>

---

## Unit 6 — Interval DP

### 07.6.1  Predict the Winner  ·  LC #486  ·  Medium  ·  array, math, dynamic-programming, recursion
<https://leetcode.com/problems/predict-the-winner/>
**Level:** 3
<details><summary>Hint</summary>
Do not track each player's score separately — compute the difference directly.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP as a score difference, dp[i][j] = best achievable difference on [i, j], O(n²) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the largest achievable score difference (player to move minus opponent) when piles i..j remain. Transition: `dp[i][j] = max(nums[i] - dp[i+1][j], nums[j] - dp[i][j-1])` — the player to move takes either end, and the minus sign flips the opponent's reply to negative from the current player's point of view. Base case: `dp[i][i] = nums[i]`. Win if `dp[0][n-1] >= 0`. Pitfall: the greedy "always take the larger end" feels right but is not — the player must also account for what the opponent does next, not only the immediate gain.
</details>

### 07.6.2  Stone Game  ·  LC #877  ·  Medium  ·  array, math, dynamic-programming, minimax-algorithm
<https://leetcode.com/problems/stone-game/>
**Level:** 3
<details><summary>Hint</summary>
Same DP as Predict the Winner — the answer is provably always true for an even number of piles.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP (same as Predict the Winner, always true), dp[i][j] = best difference, O(n²) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Exactly the same state, transition and base case as Predict the Winner: `dp[i][j] = max(nums[i] - dp[i+1][j], nums[j] - dp[i][j-1])`. In this version the number of piles is always even and the total number of stones odd, which mathematically guarantees the first player's win — but the DP solution does not rely on that observation; it works in general. Pitfall: if you rely only on the mathematical observation instead of implementing the DP, the solution does not generalise to the variants (Stone Game II, III) where the answer is not always fixed.
</details>

### 07.6.3  Guess Number Higher or Lower II  ·  LC #375  ·  Medium  ·  math, dynamic-programming, minimax-algorithm, game-theory
<https://leetcode.com/problems/guess-number-higher-or-lower-ii/>
**Level:** 3
<details><summary>Hint</summary>
Optimise the worst-case cost, not the average — the adversary always picks the worst branch.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP minimax, dp[i][j] = smallest guaranteed cost to guess a number in [i, j], O(n³) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the smallest **guaranteed** amount of money sufficient to guess the correct number in [i, j] even in the worst case. Transition: `dp[i][j] = min_k (k + max(dp[i][k-1], dp[k+1][j]))` — choose a guess k, pay its price, and prepare for the worse of the two continuation intervals. Base case: `dp[i][j] = 0` when i ≥ j. The third loop over k makes the total time O(n³). Pitfall: it is easy to forget the `max` operation and accidentally compute an average — the problem requires minimax thinking, not an expected value.
</details>

### 07.6.4  Minimum Score Triangulation of Polygon  ·  LC #1039  ·  Medium  ·  array, dynamic-programming, triangulation, polygons
<https://leetcode.com/problems/minimum-score-triangulation-of-polygon/>
**Level:** 3
<details><summary>Hint</summary>
Every side i–j belongs to exactly one triangle with some third vertex k.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP, dp[i][j] = minimum score to triangulate the vertex range [i..j], O(n³) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the minimum total score to triangulate the part of the polygon whose vertices are i, i+1, ..., j. Transition: `dp[i][j] = min_k (dp[i][k] + dp[k][j] + values[i] * values[k] * values[j])` — vertex k is the third point of the triangle on side (i, j), which splits the region into two smaller regions to triangulate. Base case: `dp[i][i+1] = 0` (two adjacent vertices do not form a triangle). Pitfall: many start the transition with the wrong indices — it must be i < k < j, otherwise the same side is counted twice.
</details>

### 07.6.5  Burst Balloons  ·  LC #312  ·  Hard  ·  array, dynamic-programming
<https://leetcode.com/problems/burst-balloons/>
**Level:** 4
<details><summary>Hint</summary>
Think of k as the last balloon burst on the interval (i, j), not the first.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP, dp[i][j] = best score for bursting all balloons in the open interval (i, j), O(n³) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The core of the trick: instead of thinking about which balloon is burst **first**, think about which is burst **last** on the open interval (i, j). Boundary padding `nums[-1] = nums[n] = 1` is added so that the edges become ordinary cases. State: `dp[i][j]` = the best score for bursting all balloons in the open interval (i, j). Transition: `dp[i][j] = max_k (dp[i][k] + nums[i] * nums[k] * nums[j] + dp[k][j])` — when k is burst last, its neighbours at that moment are exactly i and j. Base case: `dp[i][i+1] = 0`. Pitfall: the "first balloon burst" viewpoint makes the transition depend on unknown neighbours.
</details>

### 07.6.6  Strange Printer  ·  LC #664  ·  Hard  ·  string, dynamic-programming
<https://leetcode.com/problems/strange-printer/>
**Level:** 4
<details><summary>Hint</summary>
If s[i] == s[k] for some k in the interval, those two print strokes can often be merged into one.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP, dp[i][j] = fewest print strokes for the substring s[i..j], O(n³) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j]` = the fewest print strokes to produce `s[i..j]`. Basic transition: `dp[i][j] = 1 + dp[i+1][j]`. But if some `k ∈ (i, j]` satisfies `s[k] == s[i]`, the stroke that prints `s[i]` can be "stretched" to cover position k as well: `dp[i][j] = min(dp[i][j], dp[i][k-1] + dp[k+1][j])`. Base case: `dp[i][i] = 1`. Pitfall: checking the transition too rarely — every k with `s[k] == s[i]` must be tried, not just the first match, because the best combination is not always the nearest one.
</details>

### 07.6.7  Minimum Cost to Merge Stones  ·  LC #1000  ·  Hard  ·  array, dynamic-programming, prefix-sum
<https://leetcode.com/problems/minimum-cost-to-merge-stones/>
**Level:** 4
<details><summary>Hint</summary>
The constraint "exactly k consecutive piles at a time" forces the number of piles into the state.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP with an extra k-dimension (number of piles), dp[i][j][m], O(n³/k) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j][m]` = the minimum cost to merge the interval [i, j] into exactly m piles. A solution is impossible unless (n − 1) mod (k − 1) = 0. Transition: `dp[i][j][m] = min_k (dp[i][k][1] + dp[k+1][j][m-1])` when m > 1; when m = 1, `dp[i][j][1] = dp[i][j][k] + sum(i, j)`. Base case: `dp[i][i][1] = 0`. This is clearly the hardest problem in the unit: the extra state m is essential, because [i, j] alone does not say how many piles the interval currently contains. Pitfall: forgetting the impossibility check k − 1 | n − 1 up front.
</details>

### 07.6.8  Remove Boxes  ·  LC #546  ·  Hard  ·  array, dynamic-programming, memoization
<https://leetcode.com/problems/remove-boxes/>
**Level:** 4
<details><summary>Hint</summary>
Add to the state how many boxes of the same colour are glued next to the left edge i, before the interval.
</details>
<details><summary>Key idea & complexity</summary>
Interval DP with an extra k-dimension (number of same-coloured boxes carried along), dp[i][j][k], O(n⁴) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][j][k]` = the best score for removing the interval [i, j] when, in addition, there are k boxes of the same colour as `boxes[i]` already "attached" to the left side, to be removed together later. Two transitions: (1) remove `boxes[i]` now together with its k attachments: `(k+1)² + dp[i+1][j][0]`; or (2) find `m ∈ (i, j]` with `boxes[m] == boxes[i]` and move the whole bundle of k+1 to wait there: `dp[i+1][m-1][0] + dp[m][j][k+1]`. Base case: `dp[i][i-1][k] = 0`. The hardest problem in the whole DP topic: the third dimension models memory of same-coloured boxes skipped earlier.
</details>

---

## Unit 7 — Bitmask DP: DP over subsets

### 07.7.1  Beautiful Arrangement  ·  LC #526  ·  Medium  ·  array, dynamic-programming, backtracking, bit-manipulation
<https://leetcode.com/problems/beautiful-arrangement/>
**Level:** 3
<details><summary>Hint</summary>
The position is always popcount(mask) + 1 — it does not need its own dimension.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP, dp[mask] = number of ways to fill the first popcount(mask) positions, O(2ⁿ · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[mask]` = the number of ways to place the numbers represented by `mask` into positions 1..popcount(mask) according to the beautiful-arrangement condition. The observation that saves one dimension: the position currently being filled is always `popcount(mask) + 1`. Transition: for every absent number i, if i mod pos = 0 or pos mod i = 0, `dp[mask | (1 << i)] += dp[mask]`. Base case: `dp[0] = 1`. Pitfall: forgetting that the position is derived from `mask` via popcount — if a separate table dimension is reserved for it, memory grows needlessly by a factor of n.
</details>

### 07.7.2  Matchsticks to Square  ·  LC #473  ·  Medium  ·  array, dynamic-programming, backtracking, bit-manipulation
<https://leetcode.com/problems/matchsticks-to-square/>
**Level:** 3
<details><summary>Hint</summary>
Four equal sides correspond to the same subset-sum constraint applied four times.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP, dp[mask] = remaining length in the current side after the used matchsticks, O(2ⁿ · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First check that the total length divides evenly by four. State: `dp[mask]` = the remaining room in the current side when the matchsticks in `mask` have already been used. Transition: for every unused matchstick i, if it fits into the remaining room, `dp[mask | (1 << i)] = (dp[mask] - matchsticks[i]) mod sideLen` (zero means the side just filled up and a new one begins). Base case: `dp[0] = 0`. Answer: is `dp[full_mask] == 0` reachable. Pitfall: merely "does a subset with sum sideLen exist" is not enough, because all four sides must be filled simultaneously from the same set of matchsticks.
</details>

### 07.7.3  Partition to K Equal Sum Subsets  ·  LC #698  ·  Medium  ·  array, dynamic-programming, backtracking, bit-manipulation
<https://leetcode.com/problems/partition-to-k-equal-sum-subsets/>
**Level:** 3
<details><summary>Hint</summary>
Same skeleton as Matchsticks to Square, but k groups instead of 4 sides.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP, dp[mask] = can the subset mask be split into full sum/k-sized groups, O(2ⁿ · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Structurally the same problem as Matchsticks to Square generalised to k groups: state `dp[mask]` = the remaining room in the current group when `mask` has been used. Transition is the same: try every unused element, check whether it fits, update `mask`. Base case: `dp[0] = 0`. Boundary check up front: if `sum % k != 0` or any single number exceeds `target`, the answer is immediately false. Pitfall: naive backtracking without memoisation repeats the same `mask` state many times from different call paths — the bitmask `dp` table is what makes the solution feasible.
</details>

### 07.7.4  Can I Win  ·  LC #464  ·  Medium  ·  math, dynamic-programming, bit-manipulation, memoization
<https://leetcode.com/problems/can-i-win/>
**Level:** 3
<details><summary>Hint</summary>
The state is fully determined by which numbers have been used — the running sum itself need not be stored.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP with memoised recursion, dp[mask] = can the player to move win with the remaining numbers, O(2ⁿ · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[mask]` = can the player to move force a win when the used numbers are the bits of `mask` (the remaining target is always derivable from the sum of the used numbers, so it need not be stored separately). Transition: try every unused number i; if it reaches the target directly, win; otherwise if the opponent **cannot** win from state `dp[mask | (1 << i)]`, the current player also wins. Memoise the result for every `mask`. Pitfall: without memoisation the recursion re-explores the same `mask` state once per arrival order — exponential blow-up, even though there are only 2ⁿ distinct states.
</details>

### 07.7.5  Maximum Compatibility Score Sum  ·  LC #1947  ·  Medium  ·  array, dynamic-programming, backtracking, bit-manipulation
<https://leetcode.com/problems/maximum-compatibility-score-sum/>
**Level:** 3
<details><summary>Hint</summary>
The state records which mentors are already used — the student index follows directly from popcount(mask).
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP for building a permutation, dp[mask] = best match when the mentors in mask are assigned, O(2ⁿ · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[mask]` = the best compatibility score sum when `mask` records which mentors have already been assigned to some student. As in Beautiful Arrangement, the student being handled is always `popcount(mask)`. Transition: for every unused mentor i, `dp[mask | (1 << i)] = max(dp[mask | (1 << i)], dp[mask] + score(student, i))`, where `score` is a precomputed table of compatibility scores. Base case: `dp[0] = 0`. This is in effect a perfect matching between two sets solved by brute-force bitmask, because the small n makes O(2ⁿ n) a fast alternative to the Hungarian algorithm.
</details>

### 07.7.6  Shortest Path Visiting All Nodes  ·  LC #847  ·  Hard  ·  dynamic-programming, bit-manipulation, breadth-first-search, graph
<https://leetcode.com/problems/shortest-path-visiting-all-nodes/>
**Level:** 4
<details><summary>Hint</summary>
This is the shortest-path version of the travelling salesman problem — the extra state is the current node, because the next step depends on the position.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP with BFS, dp[mask][node] = shortest path visiting the set mask and ending at node, O(2ⁿ · n²) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[mask][node]` = the length of the shortest path that visits exactly the node set `mask` and ends at `node`. Because the graph is unweighted, BFS layer by layer over the whole `(mask, node)` state space finds the shortest path naturally. Base case: every `dp[1 << i][i] = 0` is a start node (starting from any node is allowed). Transition in BFS: from every state move to a neighbour node', new state `(mask | (1 << node'), node')`. Answer: the fewest steps at which `mask` = all bits set. Pitfall: starting the BFS only from node 0 instead of adding every node to the queue in the initial state.
</details>

### 07.7.7  Smallest Sufficient Team  ·  LC #1125  ·  Hard  ·  array, dynamic-programming, bit-manipulation, bitmask
<https://leetcode.com/problems/smallest-sufficient-team/>
**Level:** 4
<details><summary>Hint</summary>
The bitmask is now over the skills, not the people — people are iterated with an ordinary loop.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP over the skill requirements, dp[skillMask] = smallest set of people covering skillMask, O(2ˢ · n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The inverted bitmask idea: the mask represents the **required skills**, not the chosen people. State: `dp[skillMask]` = the smallest set of people that together cover all the skills in `skillMask`. Transition: for every person p with skill mask `personSkills[p]`, `dp[skillMask | personSkills[p]] = min(current, dp[skillMask] + {p})` compared by size. Base case: `dp[0] = ∅`. Because there are few skills but possibly many more people, a bitmask over the skills — not the people — keeps the state size manageable. Pitfall: masking the wrong set — here the small number of skills is exactly the hint for which set to mask.
</details>

### 07.7.8  Minimum Cost to Connect Two Groups of Points  ·  LC #1595  ·  Hard  ·  array, dynamic-programming, bit-manipulation, matrix
<https://leetcode.com/problems/minimum-cost-to-connect-two-groups-of-points/>
**Level:** 4
<details><summary>Hint</summary>
The bitmask covers the second (smaller) group — the other group is iterated with an ordinary index.
</details>
<details><summary>Key idea & complexity</summary>
Bitmask DP over the second group + per-row minimum, dp[i][mask] = minimum cost to connect group 1 up to i while covering mask of group 2, O(n · 2ᵐ · m) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
State: `dp[i][mask]` = the minimum cost to connect points 0..i of group 1 to points of group 2 so that at least the bits of `mask` in group 2 are already covered. Transition: on row i, try every way to connect point i to one or more points of group 2, `dp[i+1][mask | added] = min(..., dp[i][mask] + cost)`. Once all points of group 1 have been processed, every still-uncovered point of group 2 is connected separately to its cheapest group-1 point. Base case: `dp[0][0] = 0`. Pitfall: forgetting that every group-1 point must connect to **at least one** group-2 point — the coverage condition of both groups must be checked.
</details>

---

## Progress

- [ ] 07.1.1 Climbing Stairs (LC #70)
- [ ] 07.1.2 Min Cost Climbing Stairs (LC #746)
- [ ] 07.1.3 N-th Tribonacci Number (LC #1137)
- [ ] 07.1.4 House Robber (LC #198)
- [ ] 07.1.5 Maximum Subarray (LC #53)
- [ ] 07.1.6 Decode Ways (LC #91)
- [ ] 07.1.7 Delete and Earn (LC #740)
- [ ] 07.1.8 Longest Increasing Subsequence (LC #300)
- [ ] 07.2.1 Best Time to Buy and Sell Stock (LC #121)
- [ ] 07.2.2 House Robber II (LC #213)
- [ ] 07.2.3 Best Time to Buy and Sell Stock II (LC #122)
- [ ] 07.2.4 Best Time to Buy and Sell Stock with Cooldown (LC #309)
- [ ] 07.2.5 Best Time to Buy and Sell Stock with Transaction Fee (LC #714)
- [ ] 07.2.6 Best Time to Buy and Sell Stock III (LC #123)
- [ ] 07.2.7 Best Time to Buy and Sell Stock IV (LC #188)
- [ ] 07.3.1 Coin Change (LC #322)
- [ ] 07.3.2 Perfect Squares (LC #279)
- [ ] 07.3.3 Combination Sum IV (LC #377)
- [ ] 07.3.4 Partition Equal Subset Sum (LC #416)
- [ ] 07.3.5 Last Stone Weight II (LC #1049)
- [ ] 07.3.6 Ones and Zeroes (LC #474)
- [ ] 07.3.7 Target Sum (LC #494)
- [ ] 07.3.8 Coin Change II (LC #518)
- [ ] 07.4.1 Unique Paths (LC #62)
- [ ] 07.4.2 Unique Paths II (LC #63)
- [ ] 07.4.3 Minimum Path Sum (LC #64)
- [ ] 07.4.4 Triangle (LC #120)
- [ ] 07.4.5 Maximal Square (LC #221)
- [ ] 07.4.6 Out of Boundary Paths (LC #576)
- [ ] 07.4.7 Dungeon Game (LC #174)
- [ ] 07.4.8 Cherry Pickup (LC #741)
- [ ] 07.5.1 Longest Palindromic Substring (LC #5)
- [ ] 07.5.2 Palindromic Substrings (LC #647)
- [ ] 07.5.3 Longest Common Subsequence (LC #1143)
- [ ] 07.5.4 Edit Distance (LC #72)
- [ ] 07.5.5 Interleaving String (LC #97)
- [ ] 07.5.6 Palindrome Partitioning II (LC #132)
- [ ] 07.5.7 Distinct Subsequences (LC #115)
- [ ] 07.5.8 Shortest Common Supersequence (LC #1092)
- [ ] 07.6.1 Predict the Winner (LC #486)
- [ ] 07.6.2 Stone Game (LC #877)
- [ ] 07.6.3 Guess Number Higher or Lower II (LC #375)
- [ ] 07.6.4 Minimum Score Triangulation of Polygon (LC #1039)
- [ ] 07.6.5 Burst Balloons (LC #312)
- [ ] 07.6.6 Strange Printer (LC #664)
- [ ] 07.6.7 Minimum Cost to Merge Stones (LC #1000)
- [ ] 07.6.8 Remove Boxes (LC #546)
- [ ] 07.7.1 Beautiful Arrangement (LC #526)
- [ ] 07.7.2 Matchsticks to Square (LC #473)
- [ ] 07.7.3 Partition to K Equal Sum Subsets (LC #698)
- [ ] 07.7.4 Can I Win (LC #464)
- [ ] 07.7.5 Maximum Compatibility Score Sum (LC #1947)
- [ ] 07.7.6 Shortest Path Visiting All Nodes (LC #847)
- [ ] 07.7.7 Smallest Sufficient Team (LC #1125)
- [ ] 07.7.8 Minimum Cost to Connect Two Groups of Points (LC #1595)
