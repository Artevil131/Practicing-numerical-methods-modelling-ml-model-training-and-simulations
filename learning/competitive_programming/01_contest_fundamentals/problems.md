# Chapter 01 — Problems

The ladder for this chapter is the complete CSES **Introductory Problems** section (24 tasks as of
2026 — the original 19 plus five newer ones) followed by a few Codeforces Div2 A/B classics. None
needs an algorithm beyond `../../algorithms_learning/`; every one tests a chapter-01 skill:
reading constraints, choosing types, simulating exactly, handling edge cases, printing fast.

How to work them:

1. First re-type `example.cpp`'s template, fast reader and `dbg` from memory into a fresh file;
   compile with zero warnings. That file becomes your `template.cpp`.
2. Each problem goes in `competitive_programming/01_contest_fundamentals/solutions/<source>_<id>.cpp`
   (e.g. `cses_1068.cpp`, `cf_4A.cpp`). Time yourself: target ≤ 10 minutes for ★1, ≤ 25 for ★2–3.
3. Before submitting run the checklist in `lesson.md` (constraints → types → edge cases →
   output format). Aim for first-submission AC; log every non-AC with its cause.
4. If WA on a hidden test: do not guess — write `brute.cpp` + `gen.cpp`, run `stress.sh`.
5. Upsolve anything unsolved after 45 minutes: open the hint, then the approach, then finish
   it yourself. Redo it from a blank file a week later.

Difficulty: ★1 = type it in, ★2 = one observation, ★3 = careful reasoning or a construction,
★4 = chapter-02 level (listed here because it is in the CSES Introductory set).

---

## A. Simulation, types and formulas

### 01.1  Weird Algorithm  ·  CSES 1068  ·  ★1
https://cses.fi/problemset/task/1068
**Technique:** simulation; choosing `long long`.
<details><summary>Hint</summary>The intermediate values exceed 2^31 for some n ≤ 10^6 even though n itself fits an `int`. Which type do you loop in?</details>
<details><summary>Approach sketch</summary>Print n, then repeat: if even halve, else 3n+1, printing each, until n = 1. Use `long long` throughout; the sequence for some inputs passes 10^10. Output with `'\n'`, not `endl` — the sequence can be hundreds of numbers long.</details>

### 01.2  Missing Number  ·  CSES 1083  ·  ★1
https://cses.fi/problemset/task/1083
**Technique:** arithmetic identity; overflow awareness.
<details><summary>Hint</summary>What is the sum of 1..n? What type does that need for n = 2·10^5?</details>
<details><summary>Approach sketch</summary>Answer = n(n+1)/2 − (sum of given numbers). n(n+1)/2 ≈ 2·10^10 overflows `int`, so compute in `long long` and make sure the multiplication itself is done in 64 bits (`1LL * n * (n + 1) / 2`). Alternative without overflow risk: XOR of 1..n XORed with all given numbers.</details>

### 01.3  Repetitions  ·  CSES 1069  ·  ★1
https://cses.fi/problemset/task/1069
**Technique:** single scan with a run counter.
<details><summary>Hint</summary>Every maximal run ends at some index; a single pass sees its full length exactly there.</details>
<details><summary>Approach sketch</summary>Keep `cur` = length of the run ending at i (reset to 1 when s[i] ≠ s[i−1]) and `best = max(best, cur)`. Initialise `best = 1`, not 0, since the string is non-empty. O(n).</details>

### 01.4  Increasing Array  ·  CSES 1094  ·  ★1
https://cses.fi/problemset/task/1094
**Technique:** greedy scan; `long long` answer.
<details><summary>Hint</summary>Is there ever a reason to raise an element above the previous one?</details>
<details><summary>Approach sketch</summary>Scan left to right; whenever a[i] < a[i−1] add a[i−1] − a[i] to the answer and set a[i] = a[i−1]. Raising higher than necessary only makes later elements more expensive (exchange argument, one line). The answer can reach 2·10^5 · 10^9 = 2·10^14: `long long`.</details>

### 01.5  Two Knights  ·  CSES 1072  ·  ★2
https://cses.fi/problemset/task/1072
**Technique:** counting by complement; closed formula; `long long`.
<details><summary>Hint</summary>Count all pairs of squares, then subtract attacking pairs. How many 2×3 rectangles fit in a k×k board, and how many attacking knight pairs does each contain?</details>
<details><summary>Approach sketch</summary>Total pairs C(k², 2) = k²(k²−1)/2. Two knights attack iff they are opposite corners of a 2×3 or 3×2 rectangle; each such rectangle contains exactly 2 attacking placements, and there are (k−1)(k−2) rectangles of each orientation, so subtract 4(k−1)(k−2). k up to 10^4 gives k⁴ ≈ 10^16: `long long`. Sanity-check the formula against a brute force for k ≤ 6 — a good first use of the stress-test loop.</details>

### 01.6  Number Spiral  ·  CSES 1071  ·  ★2
https://cses.fi/problemset/task/1071
**Technique:** O(1) formula per query; case analysis on parity; `long long`.
<details><summary>Hint</summary>Cell (y, x) lies on layer m = max(y, x). The largest number in layer m is m². Which direction does the layer run — depends on whether m is even or odd.</details>
<details><summary>Approach sketch</summary>Let m = max(y, x). Layer m contains the numbers (m−1)²+1 … m². If m is even, the layer's numbers increase going down the right column then… (check on the sample) — derive both parities on paper with a 4×4 spiral. Answer is m² − (offset) or (m−1)² + 1 + (offset) where offset is |y − x|-dependent. With 10^5 queries and coordinates ≤ 10^9 the answer is ≈ 10^18: `long long`, and fast output.</details>

### 01.7  Bit Strings  ·  CSES 1617  ·  ★1
https://cses.fi/problemset/task/1617
**Technique:** modular arithmetic; reduce at every step.
<details><summary>Hint</summary>Each of n positions is an independent binary choice.</details>
<details><summary>Approach sketch</summary>2^n mod (10^9+7) by an O(n) loop `x = x * 2 % M` in `long long` (or binary exponentiation in O(log n)). The product before `%` is at most 2·10^9 — fits `long long`, not `int`.</details>

### 01.8  Trailing Zeros  ·  CSES 1618  ·  ★2
https://cses.fi/problemset/task/1618
**Technique:** number theory (Legendre's formula); `long long` loop variable.
<details><summary>Hint</summary>Each trailing zero is a factor 10 = 2·5. Which prime is scarcer in n!?</details>
<details><summary>Approach sketch</summary>Answer = ⌊n/5⌋ + ⌊n/25⌋ + ⌊n/125⌋ + …; there are always more 2s than 5s. Loop `for (ll p = 5; p <= n; p *= 5)` — with `int p` the last multiplication overflows (5^14 > 2^31).</details>

### 01.9  Coin Piles  ·  CSES 1754  ·  ★2
https://cses.fi/problemset/task/1754
**Technique:** necessary-and-sufficient condition; multi-test I/O.
<details><summary>Hint</summary>Each move removes 3 coins in total. After x moves of type (2 from a, 1 from b) and y moves of type (1 from a, 2 from b), what are a and b?</details>
<details><summary>Approach sketch</summary>Need 2x + y = a and x + 2y = b with x, y ≥ 0 integers. Adding: 3(x + y) = a + b, so (a + b) mod 3 = 0. Solving gives x = (2a − b)/3, y = (2b − a)/3, both non-negative iff 2·min(a, b) ≥ max(a, b). Both conditions together are sufficient. t up to 10^5 tests: fast I/O, `'\n'`.</details>

### 01.10  Palindrome Reorder  ·  CSES 1755  ·  ★2
https://cses.fi/problemset/task/1755
**Technique:** counting characters; construction.
<details><summary>Hint</summary>How many letters may have an odd count in a palindrome?</details>
<details><summary>Approach sketch</summary>Count letters. If more than one has an odd count, `NO SOLUTION`. Otherwise output, for each letter, count/2 copies; then the odd letter's full count (if any) in the middle; then the first half reversed. Build the string in a `string` and print once — n up to 10^6.</details>

### 01.11  Digit Queries  ·  CSES 2431  ·  ★3
https://cses.fi/problemset/task/2431
**Technique:** block arithmetic on the infinite string 1234567891011…; `long long`.
<details><summary>Hint</summary>All d-digit numbers together contribute 9·10^{d−1}·d characters. Skip whole blocks first.</details>
<details><summary>Approach sketch</summary>Given k (≤ 10^18), subtract block lengths 9·1·1, 90·2, 900·3, … until k falls inside the block of d-digit numbers. Then the number is 10^{d−1} + (k−1)/d and the digit is at position (k−1) mod d within it (convert with `to_string`). Everything in `long long`; the block length 9·10^{d−1}·d for d = 18 is ≈ 1.6·10^19 — careful: stop the loop when the remaining k is ≤ the block length, so you never compute the next oversized block.</details>

---

## B. Constructions

### 01.12  Permutations  ·  CSES 1070  ·  ★2
https://cses.fi/problemset/task/1070
**Technique:** constructive; small-case special handling.
<details><summary>Hint</summary>Numbers of the same parity differ by at least 2. Print one parity class, then the other — in which order, and which n have no solution?</details>
<details><summary>Approach sketch</summary>Output all even numbers 2, 4, …, then all odd 1, 3, …; the only adjacent pair across the boundary is (largest even, 1), which differs by ≥ 3 when n ≥ 4. n = 1 → `1`; n = 2, 3 → `NO SOLUTION`. Verify the boundary pair by hand rather than trusting the description.</details>

### 01.13  Two Sets  ·  CSES 1092  ·  ★2
https://cses.fi/problemset/task/1092
**Technique:** parity/feasibility argument + greedy construction.
<details><summary>Hint</summary>The total 1+…+n must be even. Which n satisfy that? Then pair 1 with n, 2 with n−1, … — alternate the pairs between the sets.</details>
<details><summary>Approach sketch</summary>Sum n(n+1)/2 is even iff n mod 4 ∈ {0, 3}. If n mod 4 = 0, the pairs (1, n), (2, n−1), … all sum to n+1; give them alternately to the two sets. If n mod 4 = 3, put 1, 2 in set A and 3 in set B (both sum 3), then handle 4..n as the previous case. Alternative: greedy from n downward, adding to set A while its sum stays ≤ target — also works. Print sizes then elements; 2·10^5 numbers → fast output.</details>

### 01.14  Gray Code  ·  CSES 2205  ·  ★2
https://cses.fi/problemset/task/2205
**Technique:** bit construction; recursion vs closed form.
<details><summary>Hint</summary>Reflected Gray code: the list for n bits is the list for n−1 bits prefixed with 0, followed by the same list reversed prefixed with 1. There is also a one-line formula.</details>
<details><summary>Approach sketch</summary>The i-th code is `i ^ (i >> 1)`; print it as an n-bit string (`bitset<16>(g).to_string().substr(16 − n)` or a manual loop). 2^16 lines: build output in one string. Chapter 02 revisits this as "generating all subsets in an order where neighbours differ by one element".</details>

### 01.15  Tower of Hanoi  ·  CSES 2165  ·  ★2
https://cses.fi/problemset/task/2165
**Technique:** recursion; move count 2^n − 1.
<details><summary>Hint</summary>To move n disks from A to C via B: move n−1 to B, move the largest to C, move n−1 from B to C.</details>
<details><summary>Approach sketch</summary>Print 2^n − 1 first, then recurse as in the hint (`solve(n, from, to, via)`). n ≤ 16 gives 65535 lines — use `'\n'`. Depth of recursion is only n.</details>

### 01.16  Raab Game I  ·  CSES 3399  ·  ★3
https://cses.fi/problemset/task/3399
**Technique:** constructive; case analysis on (n, a, b).
<details><summary>Hint</summary>a rounds are won by the first player, b by the second, the rest n − a − b are ties. If a = 0 then b must be 0 (why? think about the sum of all cards). Then build: ties are easy (same card), a win is (big, small), a loss is (small, big).</details>
<details><summary>Approach sketch</summary>Infeasible if a + b > n, or exactly one of a, b is zero (the sums of both hands are equal, so the first player cannot win some rounds and lose none). Otherwise, take the first player's cards in order 1..n. Use the a+b non-tie rounds as a block: give the second player the first player's cards cyclically shifted within that block by a positions in one direction, so that exactly a rounds have p1 > p2 and b have p1 < p2; the remaining n − a − b rounds are ties (same card in both hands). Verify with a brute force over small n that your construction has exactly a wins and b losses — this is a stress-test problem par excellence.</details>

### 01.17  Mex Grid Construction  ·  CSES 3419  ·  ★3
https://cses.fi/problemset/task/3419
**Technique:** pattern discovery via brute force, then O(n²) formula.
<details><summary>Hint</summary>Compute the grid naively for n = 8 and stare at it. It is the table of a familiar bitwise operation.</details>
<details><summary>Approach sketch</summary>Fill the grid directly by definition for small n (each cell = smallest non-negative integer not appearing to its left in its row or above it in its column). The result is the nim-addition table: cell (i, j) (0-indexed) equals i XOR j. Print `i ^ j` for the whole n×n grid (n ≤ 100). This is a lesson in "run the brute force, recognise the pattern".</details>

### 01.18  Grid Coloring I  ·  CSES 3311  ·  ★3
https://cses.fi/problemset/task/3311
**Technique:** constructive; two disjoint alphabets on a checkerboard.
<details><summary>Hint</summary>Cells with (i + j) even are never adjacent to each other. Give them letters from one pool and the odd cells letters from another pool.</details>
<details><summary>Approach sketch</summary>For even (i + j): output `A`, unless the original letter is `A`, then `C`. For odd (i + j): `B`, unless original is `B`, then `D`. Adjacent cells always have opposite parity so they get letters from disjoint pools and differ; every cell differs from its original. O(nm).</details>

### 01.19  String Reorder  ·  CSES 1743  ·  ★3
https://cses.fi/problemset/task/1743
**Technique:** greedy with a feasibility check per step.
<details><summary>Hint</summary>Lexicographically smallest string with no two equal neighbours: at each position, take the smallest letter that (a) differs from the previous one and (b) does not make the rest impossible. When is the rest impossible?</details>
<details><summary>Approach sketch</summary>A multiset of m remaining letters can be arranged with no equal neighbours iff the maximum count ≤ ⌈m/2⌉. If some letter has count exactly ⌈m/2⌉ (with m odd) or (m/2 with the previous letter equal to it… work out the exact case), it must be placed now — otherwise choose the smallest letter ≠ previous whose removal keeps the condition. 26 candidates per position, n ≤ 10^6: O(26n). Check impossibility (max count > ⌈n/2⌉) up front.</details>

---

## C. Complete search — preview of chapter 02 (all in the Introductory set)

### 01.20  Creating Strings  ·  CSES 1622  ·  ★2
https://cses.fi/problemset/task/1622
**Technique:** `next_permutation` over a sorted string.
<details><summary>Hint</summary>Starting from the sorted string, `next_permutation` produces every distinct arrangement exactly once, in lexicographic order.</details>
<details><summary>Approach sketch</summary>`sort(s)`, then `do { out.push_back(s); } while (next_permutation(all(s)));`. Print the count then the strings. Length ≤ 8 → at most 40320 strings. Starting unsorted is the classic bug: the loop then misses permutations.</details>

### 01.21  Apple Division  ·  CSES 1623  ·  ★2
https://cses.fi/problemset/task/1623
**Technique:** bitmask subset enumeration; `long long`.
<details><summary>Hint</summary>n ≤ 20 says enumerate all 2^n splits. A single mask fixes both groups.</details>
<details><summary>Approach sketch</summary>For each mask 0..2^n−1 compute s = sum of selected weights; the difference is |total − 2s|. O(2^n · n) ≈ 2·10^7. Weights up to 10^9 and 20 of them: `long long`. Chapter 02 shows the O(2^n) version via `s[mask] = s[mask & (mask−1)] + w[ctz(mask)]`.</details>

### 01.22  Chessboard and Queens  ·  CSES 1624  ·  ★3
https://cses.fi/problemset/task/1624
**Technique:** backtracking column by column with row/diagonal occupancy arrays.
<details><summary>Hint</summary>Exactly one queen per column. Diagonals are indexed by x + y and x − y + 7.</details>
<details><summary>Approach sketch</summary>Recurse over columns; in each try every free, unreserved row; mark `row[x]`, `d1[x+y]`, `d2[x−y+7]`, recurse, unmark. Count complete placements. Without blocked squares the answer is 92 — a built-in test.</details>

### 01.23  Grid Path Description  ·  CSES 1625  ·  ★4
https://cses.fi/problemset/task/1625
**Technique:** backtracking with strong pruning (the CPH showcase).
<details><summary>Hint</summary>Naive DFS over 48 steps on a 7×7 grid is hopeless. Prune: (1) if you reach the target cell early, stop; (2) if you cannot continue straight but can turn both left and right, the grid splits into two unreachable parts — prune; (3) exploit the symmetry: paths starting with D mirror those starting with R.</details>
<details><summary>Approach sketch</summary>DFS from (0,0) matching the pattern string; wildcards branch 4 ways. Add the pruning rules in order and measure the node count after each — CPH reports the reduction from ~1.5·10^8 recursive calls down to ~10^5 with all rules. Chapter 02 §3 works through this problem in detail; it belongs here so you experience *why* pruning matters before the theory.</details>

### 01.24  Knight Moves Grid  ·  CSES 3217  ·  ★2
https://cses.fi/problemset/task/3217
**Technique:** BFS on a grid (pointer to `../../algorithms_learning/06_graphs`).
<details><summary>Hint</summary>Unweighted shortest paths from the corner to every cell: one BFS.</details>
<details><summary>Approach sketch</summary>Standard BFS with the 8 knight offsets on an n×n grid (n ≤ 1000, 10^6 cells). Every cell is reachable for n ≥ 4; print the distance grid with a single output buffer — 10^6 numbers is exactly where `endl` or unsynced `cout` costs you the time limit.</details>

---

## D. Codeforces Div2 A/B calibration

Speed drills. Each should take under 10 minutes including reading; the point is the discipline
(constraints → type → edge case → `'\n'`), not the algorithm.

### 01.25  Watermelon  ·  Codeforces 4A  ·  CF 800
https://codeforces.com/problemset/problem/4/A
**Technique:** reading the statement precisely.
<details><summary>Hint</summary>Two even positive parts. Is w = 2 possible?</details>
<details><summary>Approach sketch</summary>Answer YES iff w is even and w > 2. The single edge case (w = 2) is the whole problem — the most-failed problem on the site by count.</details>

### 01.26  Way Too Long Words  ·  Codeforces 71A  ·  CF 800
https://codeforces.com/problemset/problem/71/A
**Technique:** string formatting.
<details><summary>Hint</summary>Strictly more than 10 characters gets abbreviated.</details>
<details><summary>Approach sketch</summary>If `s.size() > 10` print `s[0] + to_string(s.size() − 2) + s.back()`, else print s. Multi-test: loop, `'\n'`.</details>

### 01.27  Theatre Square  ·  Codeforces 1A  ·  CF 1000
https://codeforces.com/problemset/problem/1/A
**Technique:** ceiling division; overflow.
<details><summary>Hint</summary>⌈n/a⌉ · ⌈m/a⌉ with n, m, a ≤ 10^9. What is the maximum answer?</details>
<details><summary>Approach sketch</summary>`(n + a − 1) / a * ((m + a − 1) / a)` in `long long`; the answer reaches 10^18. Do not use `ceil()` on doubles.</details>

### 01.28  Next Round  ·  Codeforces 158A  ·  CF 800
https://codeforces.com/problemset/problem/158/A
**Technique:** careful condition.
<details><summary>Hint</summary>Advance iff score ≥ the k-th place score **and** score > 0.</details>
<details><summary>Approach sketch</summary>Count i with a[i] ≥ a[k−1] and a[i] > 0. Both conditions matter; the "positive" one is the trap.</details>

### 01.29  Domino piling  ·  Codeforces 50A  ·  CF 800
https://codeforces.com/problemset/problem/50/A
**Technique:** one-line formula with a proof.
<details><summary>Hint</summary>Each domino covers 2 cells; can you always cover ⌊mn/2⌋ cells?</details>
<details><summary>Approach sketch</summary>Answer ⌊m·n/2⌋. Proof: tile row by row; if a row has odd length, pair its last cell vertically with the next row's — one cell is left over at most. Upper bound is trivial.</details>

### 01.30  Bit++  ·  Codeforces 282A  ·  CF 800
https://codeforces.com/problemset/problem/282/A
**Technique:** parsing.
<details><summary>Hint</summary>Each statement contains either `++` or `--` — only one character decides.</details>
<details><summary>Approach sketch</summary>Read each token; if `s[1] == '+'` add 1 else subtract 1. The operator can appear before or after `X`; checking the middle character handles both.</details>

### 01.31  Team  ·  Codeforces 231A  ·  CF 800
https://codeforces.com/problemset/problem/231/A
**Technique:** counting with a threshold.
<details><summary>Hint</summary>Sum of three 0/1 values ≥ 2.</details>
<details><summary>Approach sketch</summary>Count lines whose three numbers sum to at least 2. Pure I/O drill: n ≤ 1000, but do it with fast I/O out of habit.</details>

Practice more: Codeforces problemset, tags `implementation`, `math`, rating 800–1200 — aim for
30 problems in ≤ 10 minutes each before moving on.
Practice more: AtCoder Beginner Contest problems A–C (any recent ABC) — 100-minute virtual
participations; target A–C in 25 minutes total.
Practice more: Codeforces Div3/Div4 rounds, virtual participation — one per week, upsolve to the
first unsolved problem within 48 hours.

---

## Progress

- [ ] 01.1 Weird Algorithm (CSES 1068)
- [ ] 01.2 Missing Number (CSES 1083)
- [ ] 01.3 Repetitions (CSES 1069)
- [ ] 01.4 Increasing Array (CSES 1094)
- [ ] 01.5 Two Knights (CSES 1072)
- [ ] 01.6 Number Spiral (CSES 1071)
- [ ] 01.7 Bit Strings (CSES 1617)
- [ ] 01.8 Trailing Zeros (CSES 1618)
- [ ] 01.9 Coin Piles (CSES 1754)
- [ ] 01.10 Palindrome Reorder (CSES 1755)
- [ ] 01.11 Digit Queries (CSES 2431)
- [ ] 01.12 Permutations (CSES 1070)
- [ ] 01.13 Two Sets (CSES 1092)
- [ ] 01.14 Gray Code (CSES 2205)
- [ ] 01.15 Tower of Hanoi (CSES 2165)
- [ ] 01.16 Raab Game I (CSES 3399)
- [ ] 01.17 Mex Grid Construction (CSES 3419)
- [ ] 01.18 Grid Coloring I (CSES 3311)
- [ ] 01.19 String Reorder (CSES 1743)
- [ ] 01.20 Creating Strings (CSES 1622)
- [ ] 01.21 Apple Division (CSES 1623)
- [ ] 01.22 Chessboard and Queens (CSES 1624)
- [ ] 01.23 Grid Path Description (CSES 1625)
- [ ] 01.24 Knight Moves Grid (CSES 3217)
- [ ] 01.25 Watermelon (CF 4A)
- [ ] 01.26 Way Too Long Words (CF 71A)
- [ ] 01.27 Theatre Square (CF 1A)
- [ ] 01.28 Next Round (CF 158A)
- [ ] 01.29 Domino piling (CF 50A)
- [ ] 01.30 Bit++ (CF 282A)
- [ ] 01.31 Team (CF 231A)
- [ ] 30 CF problems rated 800–1200 in ≤ 10 min each
- [ ] One virtual Div3/Div4 round + upsolve, problem log started
