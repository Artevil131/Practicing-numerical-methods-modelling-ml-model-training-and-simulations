# Chapter 16 — Problems

How to work them:

1. Re-type from memory into `competitive_programming/16_game_theory_constructive_and_misc/solutions/lib.h`:
   `nimMove`, `mex`, `grundySubtraction`, `retrograde`, a Zobrist set, `countSubsetSum`, and
   the interactive skeleton with a `std::function` oracle. Diff against `example.cpp`.
2. Each problem goes in `solutions/<source>_<id>.cpp`.
3. Games: before trusting a formula, write the memoised win/lose brute force over tiny states
   and compare for every state with total size ≤ 12. Constructions: write the checker first.
   Interactive: write a local mock judge (a lambda with a hidden secret) and run the solver
   against it 1000 times with random secrets before touching the real judge.
4. Time yourself: ★1–★2 in ≤ 20 min, ★3 in ≤ 45 min, ★4–★5 upsolve after reading the hint only.
5. Upsolve everything you failed within a week, from a blank file.

Difficulty: ★1 (CF ~1200) … ★5 (CF ~2500+). Ratings for Codeforces problems are approximate.

---

## A. Impartial games — the CSES block

### 16.1  Stick Game  ·  CSES 1729  ·  ★1
https://cses.fi/problemset/task/1729
**Technique:** win/lose DP on one pile (subtraction game).
<details><summary>Hint</summary>`win[0] = false`; a position wins iff some allowed removal leads to a losing one.</details>
<details><summary>Approach sketch</summary>`win[n] = OR over s in S with s ≤ n of !win[n−s]`, O(n·k) with n ≤ 1e6, k ≤ 100 → 1e8 boolean operations, fine in 1 s. Print W/L per n. This is the Grundy computation with only the zero/non-zero information kept.</details>

### 16.2  Nim Game I  ·  CSES 1730  ·  ★1
https://cses.fi/problemset/task/1730
**Technique:** Bouton's theorem.
<details><summary>Hint</summary>One xor.</details>
<details><summary>Approach sketch</summary>First player wins iff the xor of all pile sizes is non-zero. Pile sizes go to 1e9, xor fits in `long long` trivially. Then, without looking, reproduce the proof: xor-0 positions are closed under "the opponent moves, I restore".</details>

### 16.3  Nim Game II  ·  CSES 1098  ·  ★2
https://cses.fi/problemset/task/1098
**Technique:** Grundy numbers of a bounded-removal pile.
<details><summary>Hint</summary>With removals of 1, 2 or 3 sticks, compute the Grundy numbers for the first ten pile sizes by mex and look at them.</details>
<details><summary>Approach sketch</summary>`g(n) = n mod 4` (mex of the previous three values cycles 0,1,2,3). Xor `g(aᵢ)` over the piles; non-zero ⇒ first player wins. The general lesson: a bounded subtraction game has period `max(S)+1` when `S = {1..k}`.</details>

### 16.4  Stair Game  ·  CSES 1099  ·  ★3
https://cses.fi/problemset/task/1099
**Technique:** staircase Nim.
<details><summary>Hint</summary>Which stairs can the opponent's move be "undone" from, and which cannot? Balls that reach stair 1 are gone.</details>
<details><summary>Approach sketch</summary>Only stairs at odd distance from stair 1 (stairs 2, 4, 6, …) act as Nim piles: a move from an even-distance stair can be mirrored one step further down, restoring the xor, while a move from an odd-distance stair is a genuine Nim move. Xor the counts on stairs 2, 4, 6, …; non-zero ⇒ first player wins. Verify against the brute force in `example.cpp` before believing it.</details>

### 16.5  Grundy's Game  ·  CSES 2207  ·  ★3
https://cses.fi/problemset/task/2207
**Technique:** Grundy numbers with an O(n²) transition + an experimental observation.
<details><summary>Hint</summary>`g(n) = mex{ g(a) ⊕ g(n−a) : 1 ≤ a < n−a }`. n can be large. Run the O(n²) code up to a few thousand and print the positions with `g = 0`.</details>
<details><summary>Approach sketch</summary>Compute the table exactly for `n` up to a threshold with the O(n²) recurrence. The losing positions (`g = 0`) are a short, sparse list that stops appearing — the constraints of the task are built around this fact. Precompute the table once, and for any `n` beyond the table answer "first wins". Say explicitly in a comment what you observed and up to where you verified it; that is honest and it is also how the intended solution works.</details>

### 16.6  Another Game  ·  CSES 2208  ·  ★2
https://cses.fi/problemset/task/2208
**Technique:** parity invariant.
<details><summary>Hint</summary>From an all-even position, what does every move do to the parities? From a position with at least one odd heap, what single move restores all-even?</details>
<details><summary>Approach sketch</summary>First player wins iff at least one heap is odd. Take one coin from every odd heap → all even; the opponent must break it (any move makes some heap odd). All-zero is all-even, so the last move is yours. Brute-force check in `example.cpp`.</details>

### 16.7  Removal Game  ·  CSES 1097  ·  ★2
https://cses.fi/problemset/task/1097
**Technique:** minimax as interval DP (scored game).
<details><summary>Hint</summary>Let `d(l, r)` be the best (my score − your score) the mover can force on `a[l..r]`.</details>
<details><summary>Approach sketch</summary>`d(l, r) = max(a[l] − d(l+1, r), a[r] − d(l, r−1))`, base `d(l, l) = a[l]`. O(n²) for n = 5000: a full 5000×5000 `long long` table is 200 MB, so iterate by interval length and keep only the previous diagonal (or use a 5000×5000 `int` table if the values fit — check the bound). First player's score = `(total + d(0, n−1)) / 2`.</details>

## B. Constructions — CSES Introductory and Construction Problems

### 16.8  Permutations  ·  CSES 1070  ·  ★1
https://cses.fi/problemset/task/1070
**Technique:** construction with a parity split.
<details><summary>Hint</summary>Brute-force n ≤ 6 and print *all* valid permutations; look at where the evens are.</details>
<details><summary>Approach sketch</summary>All evens ascending, then all odds ascending. Differences inside a block are 2; at the seam the largest even meets 1, difference `n−1` or `n−2` ≥ 2 once n ≥ 4. Exceptions n = 2, 3 (print NO SOLUTION); n = 1 prints 1.</details>

### 16.9  Number Spiral  ·  CSES 1071  ·  ★1
https://cses.fi/problemset/task/1071
**Technique:** closed formula by layers.
<details><summary>Hint</summary>Cell `(y, x)` lies on layer `m = max(x, y)`, which contains `(m−1)²+1 … m²`. In which direction does layer m run?</details>
<details><summary>Approach sketch</summary>Even `m`: row `m` increases left-to-right from `(m−1)²+1`, then column `m` decreases upward from `m²`. Odd `m`: the mirror image. Two cases, O(1) per query, values to 1e18 → `long long`. Test against a generated 10×10 spiral.</details>

### 16.10  Two Knights  ·  CSES 1072  ·  ★2
https://cses.fi/problemset/task/1072
**Technique:** counting by complement + local structure (formula fitting).
<details><summary>Hint</summary>Count all pairs, subtract attacking pairs. Every attacking pair lives in exactly one 2×3 or 3×2 rectangle, and each such rectangle contains exactly two attacking pairs.</details>
<details><summary>Approach sketch</summary>`C(k², 2) − 4(k−1)(k−2)`. Brute-force k ≤ 6 to confirm, and note the method: when the answer is obviously a polynomial in k, brute force a few values and fit.</details>

### 16.11  Gray Code  ·  CSES 2205  ·  ★1
https://cses.fi/problemset/task/2205
**Technique:** `i ^ (i >> 1)`, or reflect-and-prefix recursion.
<details><summary>Hint</summary>The list for n bits is the list for n−1 bits with a leading 0, followed by the same list reversed with a leading 1.</details>
<details><summary>Approach sketch</summary>Print `i ^ (i >> 1)` for `i = 0 .. 2ⁿ−1` as n-bit binary strings. Proof of the one-bit property in the lesson. n ≤ 16 → 65536 lines; use `'\n'` and a fast output routine.</details>

### 16.12  Two Sets  ·  CSES 1092  ·  ★2
https://cses.fi/problemset/task/1092
**Technique:** feasibility by an invariant + greedy construction.
<details><summary>Hint</summary>The total `n(n+1)/2` must be even. If it is, pair `n` with `1`, `n−1` with `2`, … when n mod 4 == 0; what about n mod 4 == 3?</details>
<details><summary>Approach sketch</summary>Impossible iff `n(n+1)/2` is odd (n mod 4 ∈ {1, 2}). Otherwise greedy from the largest: add `n, n−1, …` to set A while its sum stays ≤ target, then put the exact remainder (which is guaranteed to be an unused number ≤ current) into A and the rest into B. Prove the remainder is always available — or use the 4-block pattern {1,4},{2,3} repeated.</details>

### 16.13  Palindrome Reorder  ·  CSES 1755  ·  ★1
https://cses.fi/problemset/task/1755
**Technique:** counting invariant.
<details><summary>Hint</summary>At most one character may have an odd count.</details>
<details><summary>Approach sketch</summary>Count letters; if more than one odd count, NO SOLUTION. Otherwise print half of each count, the odd letter's full count in the middle, and the mirrored half. O(n).</details>

### 16.14  Raab Game I  ·  CSES 3399  ·  ★3
https://cses.fi/problemset/task/3399
**Technique:** construction of two permutations with prescribed win counts.
<details><summary>Hint</summary>Player A wins round i iff `a_i > b_i`. With `x` wins for A and `y` for B, `x + y ≤ n` and the remaining rounds must be exact ties, which forces equal numbers there. Try shifting a block cyclically.</details>
<details><summary>Approach sketch</summary>Use the identity permutation for A on the tie positions, and on a block of `x + y` positions let B be a cyclic shift of A's values: a shift by `y` in a block of length `x + y` makes exactly `x` positions where A is larger and `y` where B is. Feasible iff `x + y ≤ n` and `(x, y) ≠ (0, k)` / `(k, 0)` for `0 < k` (one side cannot win without the other winning somewhere, since both permutations have the same sum). Write the checker, then hunt the edge cases.</details>

### 16.15  Mex Grid Construction  ·  CSES 3419  ·  ★2
https://cses.fi/problemset/task/3419
**Technique:** direct simulation of a definition (mex of row/column prefixes).
<details><summary>Hint</summary>Each cell is the mex of the values to its left in the row and above in the column. n ≤ 100: simulate directly, then look for the pattern.</details>
<details><summary>Approach sketch</summary>Simulating with a boolean `seen` array per cell is O(n³) = 1e6, trivially fine. The resulting grid is `(i xor j)` — the Nim-sum table — which is exactly the Sprague–Grundy value of a two-pile subtraction-free game. Recognise it; you will meet the xor table again.</details>

### 16.16  Grid Coloring I  ·  CSES 3311  ·  ★2
https://cses.fi/problemset/task/3311
**Technique:** local repair with a fixed palette.
<details><summary>Hint</summary>You must change every cell to a different colour such that no two adjacent cells match; with 4 colours (A–D) a checkerboard-style pattern leaves each cell a choice of 2.</details>
<details><summary>Approach sketch</summary>Assign each cell a pair of candidate colours by its parity class `(i + j) mod 2` — class 0 picks from {A, B}, class 1 from {C, D}. Adjacent cells are in different classes, so any choice is conflict-free; pick the candidate that differs from the original. Proof that a valid choice always exists: two candidates, one forbidden.</details>

### 16.17  Inverse Inversions  ·  CSES 2214  ·  ★2
https://cses.fi/problemset/task/2214
**Technique:** construction of a permutation with exactly k inversions.
<details><summary>Hint</summary>Placing the largest remaining element first contributes `(remaining − 1)` inversions. Greedy from the left.</details>
<details><summary>Approach sketch</summary>Walk positions left to right; if `k ≥ remaining − 1` put the largest remaining number (adds `remaining − 1`), else put the smallest remaining number that adds exactly `k` inversions (the `(k+1)`-th smallest), then the rest ascending. O(n log n) with an order-statistics structure, or O(n) by observing the result is "a prefix of reversed values then a rotated tail".</details>

### 16.18  Monotone Subsequences  ·  CSES 2215  ·  ★3
https://cses.fi/problemset/task/2215
**Technique:** Erdős–Szekeres bound + block construction.
<details><summary>Hint</summary>Split `1..n` into `k` consecutive blocks and reverse each block: LIS = number of blocks, LDS = largest block. What must hold between n, LIS and LDS?</details>
<details><summary>Approach sketch</summary>Feasible iff `a·b ≥ n` and `a + b ≤ n + 1` (Erdős–Szekeres for the first, the second because an LIS and an LDS share at most one element). Build `a` decreasing blocks whose sizes are ≤ b and sum to n, with at least one block of size exactly b and exactly a blocks. Checker: O(n log n) LIS/LDS.</details>

### 16.19  Third Permutation  ·  CSES 3422  ·  ★3
https://cses.fi/problemset/task/3422
**Technique:** derangement-style construction against two permutations.
<details><summary>Hint</summary>Find a permutation `c` that differs from both `a` and `b` at every position. Think of each position as forbidding two values; a cyclic shift of one of them often works — when does it fail?</details>
<details><summary>Approach sketch</summary>Positions forbid ≤ 2 values each, so for n ≥ 3 a solution exists except small cases; construct greedily with a repair step: assign values in some order, and when the only remaining value is forbidden, swap with an earlier position where the swap keeps both valid (always possible for n ≥ 3 by counting). Brute-force n ≤ 5 to list the impossible inputs and to test the repair.</details>

### 16.20  Permutation Prime Sums  ·  CSES 3423  ·  ★3
https://cses.fi/problemset/task/3423
**Technique:** construction with a number-theoretic constraint (adjacent sums prime).
<details><summary>Hint</summary>A prime sum of two numbers ≥ 2 must be odd, so parities must alternate. Pair each even with an odd so the sum is prime, then chain the pairs.</details>
<details><summary>Approach sketch</summary>Parity alternation forces the even/odd counts to differ by at most one. Then build a path in the bipartite graph "even–odd, edge if sum is prime": for these sizes a greedy with backtracking, or matching-based chaining, finds one quickly because prime pairs are dense (Bertrand-flavoured). Sieve primes up to 2n. Verify with a checker.</details>

### 16.21  Chess Tournament  ·  CSES 1697  ·  ★3
https://cses.fi/problemset/task/1697
**Technique:** graph with a given degree sequence (Havel–Hakimi).
<details><summary>Hint</summary>Sort by remaining degree, connect the largest to the next `d` largest, repeat; use a priority queue.</details>
<details><summary>Approach sketch</summary>Havel–Hakimi: repeatedly take the player with the most remaining games and match them against the `d` players with the next-highest remaining demands. If at any point fewer than `d` others remain with positive demand → IMPOSSIBLE. Correctness: the exchange argument shows any realisation can be transformed into one where the max-degree vertex is adjacent to the next-highest ones. O(n² log n) worst case with careful re-insertion, fine for the constraints.</details>

### 16.22  Distinct Sums Grid  ·  CSES 3424  ·  ★2
https://cses.fi/problemset/task/3424
**Technique:** arithmetic-progression construction.
<details><summary>Hint</summary>All row sums and column sums distinct. Fill with `grid[i][j] = i·n + j`? Check whether a row sum can equal a column sum, then adjust one cell.</details>
<details><summary>Approach sketch</summary>Row sums of `i·n + j` are spaced by `n²`, column sums by `n`; they can collide, so use `grid[i][j] = i·n + j` for the first n−1 rows and a shifted last row (or add a large multiple to the diagonal) so that rows sums fall in one range and column sums in a disjoint one. Prove disjointness by comparing the minimum of one family with the maximum of the other; checker confirms for all n ≤ 50.</details>

### 16.23  Filling Trominos  ·  CSES 2423  ·  ★3
https://cses.fi/problemset/task/2423
**Technique:** tiling by induction on 2×3 blocks with a small-case table.
<details><summary>Hint</summary>Every L-tromino tiling of a rectangle decomposes into 2×3 and 3×2 blocks except for a few small hand-made cases. Which rectangles are tileable at all? Area divisible by 3 is necessary; find the exceptions.</details>
<details><summary>Approach sketch</summary>Tile `2×3k` and `3k×2` with two trominoes per 2×3 block; larger sizes reduce to those plus special cases (e.g. `5×9`, `6×n`). Precompute by brute force which `(h, w)` up to ~10 are tileable and store the explicit tilings; for large sizes cut off 2 rows or 3 columns and recurse. Print letters so adjacent trominoes differ — colour greedily afterwards.</details>

### 16.24  Grid Path Construction  ·  CSES 2418  ·  ★4
https://cses.fi/problemset/task/2418
**Technique:** Hamiltonian path construction on a grid with fixed endpoints; parity obstruction.
<details><summary>Hint</summary>Colour the grid like a chessboard. A Hamiltonian path alternates colours, so for an even number of cells the endpoints must have different colours, for odd the endpoints must both be the majority colour. Then build by snakes.</details>
<details><summary>Approach sketch</summary>Check the colouring condition (and the 1×n / 2×n special cases). Then construct recursively: peel off a full row or column that does not contain the endpoints and traverse it as a snake, reducing to a smaller grid; when both endpoints are in a 2-row strip, hand-construct. Brute force all inputs for grids up to 4×4 to validate both the impossibility rule and the construction.</details>

### 16.25  Beautiful Permutation II  ·  CSES 3175  ·  ★3
https://cses.fi/problemset/task/3175
**Technique:** the "Permutations" construction plus a lexicographic/count twist — read the statement.
<details><summary>Hint</summary>Same adjacency constraint (no adjacent difference of 1) with an extra requirement. Start from the evens-then-odds construction and see what freedom remains.</details>
<details><summary>Approach sketch</summary>Enumerate all valid permutations for n ≤ 7 by brute force and look at what the extra requirement selects; then extend the two-block construction (evens / odds) with the freedom to permute inside blocks and to swap blocks, and prove the small exceptions. Write the checker first.</details>

### 16.26  Grid Path Description  ·  CSES 1625  ·  ★4
https://cses.fi/problemset/task/1625
**Technique:** exhaustive search with pruning (partial-score mindset).
<details><summary>Hint</summary>Three prunings: (1) if you reach the target early, abort; (2) if the cell ahead is blocked and both sides are free, the grid splits — abort; (3) symmetry of the first move.</details>
<details><summary>Approach sketch</summary>DFS over the 7×7 grid following the 48-character pattern with `?` as free choices. Without pruning ~1e12 paths; with the "dead-end split" pruning (moving into a wall/visited cell while left and right are both open cuts the unvisited region in two) the count drops to a few million. This is the model for IOI subtask pruning: measure the effect of each pruning separately.</details>

## C. Interactive — CSES Interactive Problems

### 16.27  Hidden Integer  ·  CSES 3112  ·  ★1
https://cses.fi/problemset/task/3112
**Technique:** interactive binary search; flushing.
<details><summary>Hint</summary>Count the queries your loop needs for the worst case *before* running against the judge.</details>
<details><summary>Approach sketch</summary>Binary search on the answer with `? x` queries; `endl` after every query; read the reply; print `! x` at the end. Test against a local lambda judge with random secrets. Query bound: `ceil(log2(range)) + 1`.</details>

### 16.28  Hidden Permutation  ·  CSES 3139  ·  ★2
https://cses.fi/problemset/task/3139
**Technique:** sorting with comparison queries under a query budget.
<details><summary>Hint</summary>If a query compares two elements, then any O(n log n)-comparison sort works: merge sort with a custom comparator that queries the judge — check the exact budget in the statement.</details>
<details><summary>Approach sketch</summary>Implement merge sort (or `std::stable_sort` with a comparator that performs the query and caches the answer). Count comparisons for the worst case n to make sure `n log n` fits; if the budget is tighter, use insertion via binary search into a sorted list (`n log n` too, but fewer in practice) or the specific structure the statement offers.</details>

### 16.29  K-th Highest Score  ·  CSES 3305  ·  ★3
https://cses.fi/problemset/task/3305
**Technique:** selection with limited comparison/rank queries.
<details><summary>Hint</summary>To find the k-th largest you do not need a full sort: quickselect-style partitioning needs O(n) expected queries per level; or a tournament tree if the budget is close to `n + k log n`.</details>
<details><summary>Approach sketch</summary>Read the exact query type. If queries are comparisons: randomised quickselect with the judge as comparator (expected ~3.4n comparisons). If queries return the k-th value of a subset or a rank, exploit that directly with binary search over candidates. Simulate the worst-case query count locally before submitting.</details>

### 16.30  Permuted Binary Strings  ·  CSES 3228  ·  ★3
https://cses.fi/problemset/task/3228
**Technique:** identify a hidden permutation through query strings.
<details><summary>Hint</summary>Each query string you send comes back permuted. Choose strings so every position gets a unique binary "name" across the queries: `log2 n` queries, bit `j` of position `i` in query `j`.</details>
<details><summary>Approach sketch</summary>Send `ceil(log2 n)` strings where string `j` has a 1 at positions whose index has bit `j` set. From the permuted answers, each output position's bits across the queries spell the index it came from. Information-theoretically optimal up to a constant.</details>

### 16.31  Colored Chairs  ·  CSES 3273  ·  ★3
https://cses.fi/problemset/task/3273
**Technique:** binary search on a circle with a discrete intermediate-value argument.
<details><summary>Hint</summary>Chairs around a circle with two colours in equal numbers; you want two adjacent chairs of different colours, or an opposite pair, with few colour queries. If chair `i` and its opposite differ, then somewhere between `i` and `i + n/2` the relation flips — binary search.</details>
<details><summary>Approach sketch</summary>Query chair 0 and its opposite. Define `f(i) = [colour(i) == colour(i + n/2)]`; if `f(0)` is false, binary search on `[0, n/2]` for the flip point using `f` (each evaluation = 2 queries), which yields adjacent chairs of different colour or the required opposite pair. O(log n) queries; test with a mock judge on random balanced colourings.</details>

### 16.32  Inversion Sorting  ·  CSES 3140  ·  ★4
https://cses.fi/problemset/task/3140
**Technique:** sort a hidden array by reversal operations with feedback (adaptive, budgeted).
<details><summary>Hint</summary>Each operation both changes the array and gives information (e.g. the new inversion count). Design operations whose effect on the feedback identifies elements one by one.</details>
<details><summary>Approach sketch</summary>Reversing a segment changes the inversion count by a known function of the relative order inside it; reversing a length-2 segment tells you whether that pair was inverted. Use that as a comparison oracle to run insertion-by-binary-search or a selection procedure, keeping operations within the stated budget. Full details depend on the exact feedback — read the statement, then derive the oracle.</details>

## D. Advanced techniques (CSES)

### 16.33  Meet in the Middle  ·  CSES 1628  ·  ★2
https://cses.fi/problemset/task/1628
**Technique:** meet in the middle on subset sums.
<details><summary>Hint</summary>n ≤ 40: 2⁴⁰ is out, 2 × 2²⁰ is in.</details>
<details><summary>Approach sketch</summary>Enumerate the subset sums of each half (2²⁰ each), sort one side, and for each sum `s` of the other count `target − s` with `equal_range`. O(2^{n/2} · n). Memory: 8 MB per side.</details>

### 16.34  Hamming Distance  ·  CSES 2136  ·  ★2
https://cses.fi/problemset/task/2136
**Technique:** bitset / popcount brute force with tight constants.
<details><summary>Hint</summary>n ≤ 2e4 strings of length ≤ 30: all pairs is 2e8 popcounts of a 30-bit xor — fits with `__builtin_popcount`.</details>
<details><summary>Approach sketch</summary>Pack each string into an `int`, double loop with `__builtin_popcount(a ^ b)`, keep the minimum, early exit at 1. ~0.3 s. This is the "heuristic/constant-factor" lesson: an O(n²) with a 1-cycle body beats a clever O(n·2^k) alternative.</details>

## E. Codeforces

### 16.35  Lost Numbers  ·  Codeforces 1167B  ·  ★1 (≈1300, interactive)
https://codeforces.com/problemset/problem/1167/B
**Technique:** interactive with a tiny query budget; use the answer structure.
<details><summary>Hint</summary>Six known values, products are distinct enough to identify pairs in 4 queries.</details>
<details><summary>Approach sketch</summary>Query products `(1,2), (2,3), (4,5), (5,6)`; brute-force all `6!` permutations of the fixed set and keep the one consistent with the four answers. Flush after each query.</details>

### 16.36  Palindrome Game (easy / hard)  ·  Codeforces 1527B1 / 1527B2  ·  ★2 / ★4 (≈1200 / 1900)
https://codeforces.com/problemset/problem/1527/B1
**Technique:** game analysis by cases and parity; mirror strategy.
<details><summary>Hint</summary>In the palindrome version Bob mirrors Alice's move; count the zeros. In the hard version the string is not a palindrome — who benefits from reversing first?</details>
<details><summary>Approach sketch</summary>Easy: with an even number of zeros Bob wins by mirroring (he pays the same as Alice and reverses at the right moment to shift one payment onto her); with exactly one zero Bob wins trivially; with an odd count ≥ 3 Alice flips the middle first and then becomes the mirroring player, winning by 1. Hard: a non-palindrome lets Alice reverse for free repeatedly; Alice wins unless the string is one flip away from a palindrome with the right parity (draw case). Derive all cases with a brute force over strings of length ≤ 8.</details>

### 16.37  Stoned Game  ·  Codeforces 1396B  ·  ★2 (≈1600)
https://codeforces.com/problemset/problem/1396/B
**Technique:** game with a "cannot take from the same pile twice in a row" rule; extremal argument.
<details><summary>Hint</summary>If one pile holds more than half of all stones the first player wins by always taking from it; otherwise the game is decided by the parity of the total.</details>
<details><summary>Approach sketch</summary>Case 1: `max > sum − max` ⇒ T wins (keeps hitting the big pile). Case 2: otherwise, both players can always take from the current maximum among the allowed piles, and no pile ever becomes dominant; the game lasts exactly `sum` moves, so parity of the total decides. Verify by brute force for total ≤ 12.</details>

### 16.38  Deleting Divisors  ·  Codeforces 1537D  ·  ★3 (≈1700)
https://codeforces.com/problemset/problem/1537/D
**Technique:** win/lose by number-theoretic structure (odd / even / power of two).
<details><summary>Hint</summary>Compute win/lose for n ≤ 64 by brute force over divisors; then group by parity and by powers of two.</details>
<details><summary>Approach sketch</summary>Odd n loses (every move produces an even non-power-of-two, which wins). Even n that is not a power of two wins (subtract an odd divisor to hand back an odd number). Powers of two alternate: `2^k` wins iff k is even... derive the exact alternation from the table. Print per test in O(log n).</details>

### 16.39  Permutation Game  ·  Codeforces 1033C  ·  ★3 (≈1700)
https://codeforces.com/problemset/problem/1033/C
**Technique:** retrograde analysis on a DAG defined by a permutation.
<details><summary>Hint</summary>From position i you may jump to any j with `a[j] > a[i]` and `|i − j|` a multiple of `a[i]`. Process positions in decreasing order of `a[i]`.</details>
<details><summary>Approach sketch</summary>Sort positions by value descending; each is WIN iff some reachable position (stepping by `a[i]` in both directions, harmonic-sum total O(n log n)) is LOSE. The largest value has no moves → LOSE. Output the string of A/B.</details>

### 16.40  A Game With Numbers  ·  Codeforces 919F  ·  ★5 (≈2400)
https://codeforces.com/problemset/problem/919/F
**Technique:** retrograde analysis with draws on a state graph of multisets mod 5.
<details><summary>Hint</summary>A hand is a multiset of 8 cards with values 0–4: `C(12, 4) = 495` hands, `495²` ordered states. Build the game graph and run the in-degree-counting retrograde analysis; cycles give "Deal".</details>
<details><summary>Approach sketch</summary>Enumerate hands, index states `(mover's hand, other's hand)`, edges by the card-addition rule; terminal states where the mover's hand is all zeros are LOSE (opponent just emptied — read the winning condition carefully). Run `retrograde` once; answer each query in O(1). Memory ~245k states with edge lists — fine.</details>

### 16.41  Special Permutation  ·  Codeforces 1352G  ·  ★2 (≈1600)
https://codeforces.com/problemset/problem/1352/G
**Technique:** constructive permutation with |adjacent difference| ∈ [2, 4].
<details><summary>Hint</summary>Odds descending, then a hop through 4 and 2, then evens ascending. Which small n fail?</details>
<details><summary>Approach sketch</summary>For n ≥ 4: `[odds descending], 4, 2, [evens ≥ 6 ascending]`; the seams are `1→4` (3) and `4→2` (2) and `2→6` (4). n < 4 impossible. Checker on n ≤ 10.</details>

### 16.42  Grid-00100  ·  Codeforces 1371D  ·  ★2 (≈1300)
https://codeforces.com/problemset/problem/1371/D
**Technique:** constructive grid minimising row/column imbalance; diagonal filling.
<details><summary>Hint</summary>Place the k ones along wrapped diagonals `(i, (i + d) mod n)`.</details>
<details><summary>Approach sketch</summary>Fill diagonal by diagonal: all row and column counts stay within 1 of each other, so `f = 0` if `n | k`, else `2`. Proof: after `d` full diagonals every row/column has exactly `d`; a partial diagonal adds 1 to distinct rows and columns.</details>

### 16.43  Ehab and Path-etic MEXs  ·  Codeforces 1325C  ·  ★2 (≈1500)
https://codeforces.com/problemset/problem/1325/C
**Technique:** constructive labelling with an extremal observation.
<details><summary>Hint</summary>If there is a vertex of degree ≥ 3, put 0, 1, 2 on three of its edges; no path contains all three.</details>
<details><summary>Approach sketch</summary>Tree is a path ⇒ any labelling gives the same result. Otherwise pick a vertex with degree ≥ 3, assign 0, 1, 2 to three incident edges, the remaining labels arbitrarily. Then the max MEX over paths is 2.</details>

### 16.44  Ehab and the Expected XOR Problem  ·  Codeforces 1174D  ·  ★3 (≈1900)
https://codeforces.com/problemset/problem/1174/D
**Technique:** constructive via prefix xors; no subarray xor equals 0 or x.
<details><summary>Hint</summary>Subarray xors are differences of prefix xors. Choose distinct prefix values such that no two differ (xor) by `x`: pick one from each pair `{v, v ^ x}`.</details>
<details><summary>Approach sketch</summary>Iterate `v` from 1 to `2ⁿ − 1`; take `v` as a prefix xor if neither `v` nor `v ^ x` was taken. The array is consecutive xors of the chosen prefixes (starting from 0). Length `2^{n−1} − 1` if `x < 2ⁿ`, else `2ⁿ − 1`.</details>

### 16.45  Guess the K-th Zero (easy)  ·  Codeforces 1520F1  ·  ★2 (≈1600, interactive)
https://codeforces.com/problemset/problem/1520/F1
**Technique:** interactive binary search on prefix sums.
<details><summary>Hint</summary>A query returns the number of ones in `[l, r]`; zeros in a prefix = length − ones.</details>
<details><summary>Approach sketch</summary>Binary search the smallest prefix with at least k zeros: ~20 queries for n = 2e5. Flush. The hard version (F2, ≈2600) reuses answers across t rounds with a segment tree of known counts — good upsolve.</details>

### 16.46  XOR Guessing  ·  Codeforces 1207E  ·  ★3 (≈1900, interactive)
https://codeforces.com/problemset/problem/1207/E
**Technique:** two queries with disjoint bit ranges; information design.
<details><summary>Hint</summary>Query 1: 100 numbers with the low 7 bits zero → the answer reveals the low 7 bits of x. Query 2: 100 numbers with the high 7 bits zero → the high bits.</details>
<details><summary>Approach sketch</summary>Query `{1·128, 2·128, …, 100·128}`: the reply's low 7 bits are x's low 7 bits (the chosen number contributes zero there). Then `{1, …, 100}` for the high 7 bits. Combine; two queries total. Adaptive judge is irrelevant since the information is exact.</details>

### 16.47  Game with modulo  ·  Codeforces 1103B  ·  ★4 (≈2000, interactive)
https://codeforces.com/problemset/problem/1103/B
**Technique:** interactive exponential + binary search with comparison queries.
<details><summary>Hint</summary>Query `(x, 2x)` tells you whether `a ≤ x` (via `x mod a` vs `2x mod a`); double `x` to bracket `a`, then binary search.</details>
<details><summary>Approach sketch</summary>Start `x = 1`, doubling until the reply flips (≈30 queries), then binary search inside `(x, 2x]` (≈30 more) — within the 60-query limit. Handle `a = 1` and the special `x = 0` query separately. Test with a mock judge across all `a ≤ 1e4`.</details>

### 16.48  Interactive LowerBound  ·  Codeforces 843B  ·  ★4 (≈2200, randomized interactive)
https://codeforces.com/problemset/problem/843/B
**Technique:** random sampling in a hidden linked list + linear walk.
<details><summary>Hint</summary>You may inspect ~2000 nodes of a sorted linked list of n ≤ 5e4. Sample ~1000 random nodes, take the largest value below x, then walk forward.</details>
<details><summary>Approach sketch</summary>After sampling 1000 random positions, the expected gap between consecutive sampled values is n/1000 = 50 nodes; walk forward ≤ ~1000 steps from the best sample. Failure probability (gap > 1000) is about `(1 − 1000/n)^{1000} ≈ e^{−20}`. Randomized-algorithm accounting in practice.</details>

### 16.49  Maximum Subsequence  ·  Codeforces 888E  ·  ★3 (≈1800)
https://codeforces.com/problemset/problem/888/E
**Technique:** meet in the middle with modular combination.
<details><summary>Hint</summary>n ≤ 35; enumerate both halves' subset sums mod m, sort one, and for each `s` find the largest `t < m − s`.</details>
<details><summary>Approach sketch</summary>Two sorted lists of `2^{17}` residues; for each `s` in the first, binary search the largest `t ≤ m − 1 − s` in the second; also consider the largest element overall (`s + t` wrapping). O(2^{n/2} log).</details>

### 16.50  Kazaee  ·  Codeforces 1746F  ·  ★5 (≈2700)
https://codeforces.com/problemset/problem/1746/F
**Technique:** randomized multiset hashing (sum of random weights) + Fenwick tree.
<details><summary>Hint</summary>"Every value in the range occurs a multiple of k times" — assign each value a random 0/1 weight (repeat ~30 times); a valid range has weighted sum divisible by k with certainty, an invalid one fails a round with probability ≥ 1/2.</details>
<details><summary>Approach sketch</summary>For each of R ≈ 30 rounds, assign random bits to values, maintain a Fenwick tree of the bits over positions (point updates on modification), and test `sum(l, r) mod k == 0`. Answer YES iff all rounds pass. Error ≤ `2^{−R}` per query. Same trick powers "Ghd" (CF 364D): sample random elements, the answer divides one of them with probability ≥ 1/2.</details>

### Practice more

- Codeforces problemset, tag `games`, rating 1700–2200 — after the CSES block, ten of these;
  brute-force every one of them for tiny sizes before trusting a pattern.
- Codeforces problemset, tag `constructive algorithms`, rating 1500–2100 — the single most
  frequent tag on Codeforces; do two per week, always with a checker.
- Codeforces problemset, tag `interactive`, rating 1600–2100 — with a local mock judge each time.
- Codeforces problemset, tags `probabilities` + `hashing`, rating 2200–2600 for randomized
  techniques.
- IOI past tasks for the format (with graders on oj.uz): IOI 2018 "Combo" (interactive), IOI
  2014 "Game" (interactive), IOI 2011 "Parrots" (communication), IOI 2020 "Stations"
  (communication), IOI 2020 "Connecting Supertrees" (construction with checker).

---

## Progress

- [ ] 16.1 Stick Game (CSES 1729)
- [ ] 16.2 Nim Game I (CSES 1730)
- [ ] 16.3 Nim Game II (CSES 1098)
- [ ] 16.4 Stair Game (CSES 1099)
- [ ] 16.5 Grundy's Game (CSES 2207)
- [ ] 16.6 Another Game (CSES 2208)
- [ ] 16.7 Removal Game (CSES 1097)
- [ ] 16.8 Permutations (CSES 1070)
- [ ] 16.9 Number Spiral (CSES 1071)
- [ ] 16.10 Two Knights (CSES 1072)
- [ ] 16.11 Gray Code (CSES 2205)
- [ ] 16.12 Two Sets (CSES 1092)
- [ ] 16.13 Palindrome Reorder (CSES 1755)
- [ ] 16.14 Raab Game I (CSES 3399)
- [ ] 16.15 Mex Grid Construction (CSES 3419)
- [ ] 16.16 Grid Coloring I (CSES 3311)
- [ ] 16.17 Inverse Inversions (CSES 2214)
- [ ] 16.18 Monotone Subsequences (CSES 2215)
- [ ] 16.19 Third Permutation (CSES 3422)
- [ ] 16.20 Permutation Prime Sums (CSES 3423)
- [ ] 16.21 Chess Tournament (CSES 1697)
- [ ] 16.22 Distinct Sums Grid (CSES 3424)
- [ ] 16.23 Filling Trominos (CSES 2423)
- [ ] 16.24 Grid Path Construction (CSES 2418)
- [ ] 16.25 Beautiful Permutation II (CSES 3175)
- [ ] 16.26 Grid Path Description (CSES 1625)
- [ ] 16.27 Hidden Integer (CSES 3112)
- [ ] 16.28 Hidden Permutation (CSES 3139)
- [ ] 16.29 K-th Highest Score (CSES 3305)
- [ ] 16.30 Permuted Binary Strings (CSES 3228)
- [ ] 16.31 Colored Chairs (CSES 3273)
- [ ] 16.32 Inversion Sorting (CSES 3140)
- [ ] 16.33 Meet in the Middle (CSES 1628)
- [ ] 16.34 Hamming Distance (CSES 2136)
- [ ] 16.35 Lost Numbers (CF 1167B)
- [ ] 16.36 Palindrome Game (CF 1527B1/B2)
- [ ] 16.37 Stoned Game (CF 1396B)
- [ ] 16.38 Deleting Divisors (CF 1537D)
- [ ] 16.39 Permutation Game (CF 1033C)
- [ ] 16.40 A Game With Numbers (CF 919F)
- [ ] 16.41 Special Permutation (CF 1352G)
- [ ] 16.42 Grid-00100 (CF 1371D)
- [ ] 16.43 Ehab and Path-etic MEXs (CF 1325C)
- [ ] 16.44 Ehab and the Expected XOR Problem (CF 1174D)
- [ ] 16.45 Guess the K-th Zero (CF 1520F1)
- [ ] 16.46 XOR Guessing (CF 1207E)
- [ ] 16.47 Game with modulo (CF 1103B)
- [ ] 16.48 Interactive LowerBound (CF 843B)
- [ ] 16.49 Maximum Subsequence (CF 888E)
- [ ] 16.50 Kazaee (CF 1746F)
