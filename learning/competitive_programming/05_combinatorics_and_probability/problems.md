# Chapter 05 — Problems

How to work this ladder:

1. Re-type `example.cpp` from memory first (at least `power`, `Comb`, ballot, Burnside for necklaces,
   the partition DP). Compile, run, all asserts green. Only then open the problems.
2. Each problem goes in `competitive_programming/05_combinatorics_and_probability/solutions/<source>_<id>.cpp`
   (e.g. `cses_1079.cpp`, `cf_1288C.cpp`, `atcoder_abc156_d.cpp`).
3. For every counting problem write a 10-line brute force for `n ≤ 8` (enumerate with
   `next_permutation`, bitmasks, or nested loops) and stress-test with random inputs before
   submitting. Counting bugs are silent; the judge only tells you "wrong answer".
4. Time yourself: ★1–★2 should take ≤ 15 min, ★3 ≤ 30 min, ★4 ≤ 60 min. If you exceed 2× the budget,
   read the hint; if still stuck, read the sketch, close it, and implement from your own understanding.
5. Upsolve everything you failed within a week.

Difficulty: ★1 (CSES easy) … ★5 (Div1 D / IOI subtask 4). Codeforces problems also carry their rating.

---

## A. Counting principles and binomials

### 05.1  Two Knights  ·  CSES 1072  ·  ★1
https://cses.fi/problemset/task/1072
**Technique:** complement counting.
<details><summary>Hint</summary>Count all placements of two knights, then subtract attacking pairs. How many 2×3 rectangles fit in a k×k board?</details>
<details><summary>Approach sketch</summary>Total pairs: `C(k², 2)`. Two knights attack each other iff they sit on opposite corners of a 2×3 or 3×2 rectangle; each such rectangle contributes exactly 2 attacking pairs and there are `(k−1)(k−2)` rectangles of each orientation, hence `4(k−1)(k−2)` attacking pairs. Use `long long` (k up to 10000 gives ~5e15).</details>

### 05.2  Bit Strings  ·  CSES 1617  ·  ★1
https://cses.fi/problemset/task/1617
**Technique:** rule of product, modular exponentiation.
<details><summary>Hint</summary>Each position is an independent binary choice.</details>
<details><summary>Approach sketch</summary>`2^n mod 1e9+7` by binary exponentiation. This is your `power` function — get it right once.</details>

### 05.3  Creating Strings  ·  CSES 1622  ·  ★1
https://cses.fi/problemset/task/1622
**Technique:** enumerating permutations of a multiset.
<details><summary>Hint</summary>`std::next_permutation` on a sorted string enumerates each distinct arrangement exactly once.</details>
<details><summary>Approach sketch</summary>Sort, then loop `do { print } while (next_permutation(...))`. The count printed first is the multinomial `n!/(Πc_i!)`, which `next_permutation` handles for you here; in 05.5 you must compute it. Length ≤ 8 so at most 40320 lines.</details>

### 05.4  Binomial Coefficients  ·  CSES 1079  ·  ★2
https://cses.fi/problemset/task/1079
**Technique:** factorial and inverse-factorial tables.
<details><summary>Hint</summary>`n ≤ 1e6`, `q ≤ 1e5`: precompute once, answer each query in O(1).</details>
<details><summary>Approach sketch</summary>Build `fact[0..1e6]`, one exponentiation for `inv_fact[1e6]`, walk down. `C(a,b) = fact[a]·inv_fact[b]·inv_fact[a−b]`. Use fast I/O (`scanf`/`printf` or untied `cin`) — 1e5 lines.</details>

### 05.5  Creating Strings II  ·  CSES 1715  ·  ★2
https://cses.fi/problemset/task/1715
**Technique:** multinomial coefficient.
<details><summary>Hint</summary>Arrangements of a multiset with letter counts `c_a … c_z`.</details>
<details><summary>Approach sketch</summary>`n! · Π inv_fact[c_i]` mod p. Count letters with an array of 26, tables to 1e6.</details>

### 05.6  Distributing Apples  ·  CSES 1716  ·  ★2
https://cses.fi/problemset/task/1716
**Technique:** stars and bars.
<details><summary>Hint</summary>Non-negative integer solutions of `x_1 + … + x_n = m`.</details>
<details><summary>Approach sketch</summary>`C(m + n − 1, n − 1)`. Tables to `2e6` since `n, m ≤ 1e6`.</details>

### 05.7  Bouquet  ·  AtCoder ABC156 D  ·  ★2
https://atcoder.jp/contests/abc156/tasks/abc156_d
**Technique:** binomial with huge `n`, small `k`; complement.
<details><summary>Hint</summary>`n ≤ 1e9` but `a, b ≤ 2e5`: you cannot build factorials to `n`.</details>
<details><summary>Approach sketch</summary>Answer `2^n − 1 − C(n,a) − C(n,b)`. Compute `C(n,k)` as `Π_{i<k}(n−i) · inv(k!)` in O(k). Mind negative intermediate results.</details>

## B. Inclusion–exclusion

### 05.8  Christmas Party  ·  CSES 1717  ·  ★2
https://cses.fi/problemset/task/1717
**Technique:** derangements.
<details><summary>Hint</summary>Recurrence `D(n) = (n−1)(D(n−1) + D(n−2))` is O(n); or the alternating-sum formula with factorial tables.</details>
<details><summary>Approach sketch</summary>Nobody gives a gift to themselves = permutation with no fixed point. Either formula from the lesson, `n ≤ 1e6`, one pass.</details>

### 05.9  Counting Sequences  ·  CSES 2228  ·  ★3
https://cses.fi/problemset/task/2228
**Technique:** inclusion–exclusion over missing values (surjections).
<details><summary>Hint</summary>"Every value from 1 to k appears at least once" — let `A_i` = value `i` never appears. `|∩_{i∈S} A_i| = (k − |S|)^n`.</details>
<details><summary>Approach sketch</summary>`Σ_{j=0}^{k} (−1)^j C(k,j) (k−j)^n`. Verify with brute force on `n, k ≤ 4` before trusting the sign pattern. If the statement has an additional condition, it will be a small modification of the same alternating sum — read it twice.</details>

### 05.10  Grid Completion  ·  CSES 2429  ·  ★4
https://cses.fi/problemset/task/2429
**Technique:** inclusion–exclusion with a DP counting "forced" cells.
<details><summary>Hint</summary>A completed grid is a permutation (one A per row and column, rest B). Pre-filled A's fix rows/columns; pre-filled B's are forbidden cells for the permutation.</details>
<details><summary>Approach sketch</summary>Remove rows and columns already containing an A. Among the remaining `m×m` free sub-grid there are some forbidden cells (pre-placed B). Count permutations avoiding all forbidden cells by inclusion–exclusion over `j` forbidden cells that are hit: `Σ_j (−1)^j ways(j) (m−j)!`, where `ways(j)` = number of ways to pick `j` forbidden cells in distinct rows and distinct columns. Since each row has few forbidden cells after grouping, `ways(j)` is a DP over rows (choose a cell in this row or not) restricted to distinct columns — organize the forbidden cells so the DP is `O(m · #cells)`.</details>

### 05.11  Counting Coprime Pairs  ·  CSES 2417  ·  ★4
https://cses.fi/problemset/task/2417
**Technique:** inclusion–exclusion over prime divisors (Möbius function).
<details><summary>Hint</summary>Count pairs with gcd divisible by `d` for each `d`, weight by `μ(d)`.</details>
<details><summary>Approach sketch</summary>Let `cnt[d]` = number of array elements divisible by `d` (sieve-style over multiples of `d`, `O(M log M)`). Pairs with gcd divisible by `d`: `C(cnt[d], 2)`. Coprime pairs `= Σ_d μ(d) C(cnt[d], 2)` — inclusion–exclusion where the sign is `μ`. The number-theory chapter has the sieve; this problem is the bridge.</details>

### 05.12  Another Filling the Grid  ·  Codeforces 1228E  ·  ★4 (2300)
https://codeforces.com/problemset/problem/1228/E
**Technique:** inclusion–exclusion over rows and columns simultaneously.
<details><summary>Hint</summary>"Every row and every column has minimum exactly 1." Let bad sets be rows/columns with no 1.</details>
<details><summary>Approach sketch</summary>Fix `i` rows and `j` columns that contain no `1`: those `i·n + j·n − i·j` cells take values in `[2,k]`, the rest in `[1,k]`. Sum `(−1)^{i+j} C(n,i) C(n,j) (k−1)^{in+jn−ij} k^{(n−i)(n−j)}` over `i, j ≤ n ≤ 250`: `O(n² log)`.</details>

### 05.13  Placing Rooks  ·  Codeforces 1342E  ·  ★4 (2300)
https://codeforces.com/problemset/problem/1342/E
**Technique:** inclusion–exclusion / surjections.
<details><summary>Hint</summary>Exactly `k` attacking pairs with `n` rooks means all rooks share columns in a specific way — think about how many distinct rows/columns are used.</details>
<details><summary>Approach sketch</summary>With `n` rooks and no pair sharing both a row and a column being possible in a "nice" placement, exactly `k` attacking pairs forces the rooks to occupy all `n` columns and exactly `n−k` rows (or vice versa). Count surjections from `n` columns onto `n−k` rows with inclusion–exclusion, times `C(n, n−k)` for the choice of rows, times 2 for the symmetric case (unless `k = 0`). `k ≥ n` gives `0`.</details>

## C. Catalan and ballot numbers

### 05.14  Bracket Sequences I  ·  CSES 2064  ·  ★2
https://cses.fi/problemset/task/2064
**Technique:** Catalan number.
<details><summary>Hint</summary>Odd `n` → 0. Otherwise `C_{n/2}`.</details>
<details><summary>Approach sketch</summary>`C(n, n/2) · inv(n/2 + 1)` with tables to `1e6`.</details>

### 05.15  Bracket Sequences II  ·  CSES 2187  ·  ★3
https://cses.fi/problemset/task/2187
**Technique:** ballot formula with a given start height.
<details><summary>Hint</summary>Walk the prefix: if its balance ever drops below zero, answer `0`. Otherwise the remaining `m = n − k` symbols form a path from height `h` to `0` that never dips below `0`.</details>
<details><summary>Approach sketch</summary>Need `a` ups and `b` downs with `a + b = m`, `h + a − b = 0`; if `m − h` is odd or negative, `0`. Otherwise `C(m, b) − C(m, b − h − 1)` (second term zero when `b − h − 1 < 0`). Check `k = 0` reproduces Catalan.</details>

### 05.16  Empty String  ·  CSES 1080  ·  ★3
https://cses.fi/problemset/task/1080
**Technique:** interval counting DP with interleaving binomials.
<details><summary>Hint</summary>The first character must be removed together with some equal character at position `j`; what is removed strictly between them is independent of what is removed outside.</details>
<details><summary>Approach sketch</summary>`dp[l][r]` = ways to empty `s[l..r]`. For each `j` with `s[j] = s[l]` and `j − l` odd: the inside `s[l+1..j−1]` and the outside `s[j+1..r]` are emptied independently, and their removal steps interleave in `C((r−l+1)/2 − 1, (j−l−1)/2)` ways (choose which of the remaining steps belong to the inside). `O(n³)` with `n ≤ 500`.</details>

### 05.17  Natasha, Sasha and the Prefix Sums  ·  Codeforces 1204E  ·  ★4 (2000)
https://codeforces.com/problemset/problem/1204/E
**Technique:** ballot numbers / reflection, summed over the maximum prefix sum.
<details><summary>Hint</summary>`Σ_{arrays} max_prefix = Σ_{v ≥ 1} #(arrays whose max prefix ≥ v)`.</details>
<details><summary>Approach sketch</summary>Number of `±1` arrays with `n` ones and `m` minus-ones whose path reaches height `v` is, by reflection, `C(n+m, m + v)` when `v > n − m` and all `C(n+m, n)` otherwise (the path ends at `n − m ≥ v`). Sum over `v = 1..n`. Pascal or factorial tables, `n, m ≤ 2000`.</details>

## D. Burnside

### 05.18  Counting Necklaces  ·  CSES 2209  ·  ★3
https://cses.fi/problemset/task/2209
**Technique:** Burnside's lemma, cyclic group.
<details><summary>Hint</summary>Rotation by `i` fixes `k^{gcd(i,n)}` colorings.</details>
<details><summary>Approach sketch</summary>`(1/n) Σ_{i=0}^{n−1} m^{gcd(i,n)}` mod p; `1/n` is a modular inverse. `n ≤ 1e6`, so the `O(n log)` sum is fine.</details>

### 05.19  Counting Grids  ·  CSES 2210  ·  ★3
https://cses.fi/problemset/task/2210
**Technique:** Burnside's lemma, rotation group of order 4.
<details><summary>Hint</summary>Count cycles of the cell permutation for 90°, 180°, 270° — mind the centre cell when `n` is odd.</details>
<details><summary>Approach sketch</summary>`(2^{n²} + 2·2^{(n²+3)/4} + 2^{(n²+1)/2}) / 4`. `n ≤ 1e9`: compute `n²` in `long long` and reduce the exponent mod `p−1` (base 2 is coprime to p). Multiply by `inv(4)`.</details>

### 05.20  Cube  ·  AtCoder ABC198 F  ·  ★5
https://atcoder.jp/contests/abc198/tasks/abc198_f
**Technique:** Burnside over the 24 rotations of a cube + stars and bars / linear recurrence.
<details><summary>Hint</summary>Each rotation splits the 6 faces into cycles; a fixed labeling has equal values on each cycle, so you count solutions of a small "sum of weighted variables = S" equation.</details>
<details><summary>Approach sketch</summary>Classify the 24 rotations by cycle type on faces (identity: 6 cycles of size 1; face rotations by 90°: 1,1,4; by 180°: 1,1,2,2; edge rotations: 2,2,2; vertex rotations: 3,3). For each type count positive solutions of `Σ (cycle length) · x_c = S` — with `S ≤ 1e18` this is a linear recurrence / matrix power (ch06) or a closed form with binomials after substitution. Sum weighted by the number of rotations of each type, divide by 24.</details>

## E. Stirling, Bell, partitions and counting DPs

### 05.21  Two Sets II  ·  CSES 1093  ·  ★2
https://cses.fi/problemset/task/1093
**Technique:** subset-sum counting DP.
<details><summary>Hint</summary>Total `n(n+1)/2` must be even; count subsets summing to half; each split counted twice.</details>
<details><summary>Approach sketch</summary>`dp[s] += dp[s − i]` for `i = 1..n`, `s` descending. Answer `dp[S/2] · inv(2)` (or only let element `n` be in the first set to avoid the division). `500 · 62500` updates.</details>

### 05.22  Counting Towers  ·  CSES 2413  ·  ★3
https://cses.fi/problemset/task/2413
**Technique:** counting DP with a 2-state profile.
<details><summary>Hint</summary>Row by row; the state is whether the current row is one 2-wide block or two 1-wide blocks.</details>
<details><summary>Approach sketch</summary>`wide[i] = 2·wide[i−1] + narrow[i−1]`, `narrow[i] = wide[i−1] + 4·narrow[i−1]` (count how each row shape can extend or start new blocks). Precompute to `1e6`, answer `t` queries in O(1). Note: this is a 2×2 linear recurrence — ch06 does it for `n = 1e18`.</details>

### 05.23  Permutation Inversions  ·  CSES 2229  ·  ★3
https://cses.fi/problemset/task/2229
**Technique:** insertion DP with prefix sums.
<details><summary>Hint</summary>Insert the largest element into a permutation of `n−1`: inserting at position `j` from the right adds exactly `j` inversions.</details>
<details><summary>Approach sketch</summary>`dp[n][k] = Σ_{j=0}^{min(k, n−1)} dp[n−1][k−j]` — a sliding window over the previous row, O(1) per cell with prefix sums. `n, k ≤ 500`.</details>

### 05.24  Counting Bishops  ·  CSES 2176  ·  ★4
https://cses.fi/problemset/task/2176
**Technique:** DP over diagonals, then convolve two independent colours.
<details><summary>Hint</summary>Bishops on black squares never interact with bishops on white squares. Within one colour, sort the diagonals by length and place bishops one diagonal at a time.</details>
<details><summary>Approach sketch</summary>For one colour, the diagonals in one direction have lengths `L_1 ≤ L_2 ≤ …`; a bishop placed on diagonal `i` blocks one anti-diagonal for all later diagonals: `dp[i][j] = dp[i−1][j] + dp[i−1][j−1] · (L_i − (j−1))`. Do both colours, then `ans = Σ_j black[j] · white[k−j]`. `n ≤ 500`, `k ≤ n`.</details>

### 05.25  Grid Paths I  ·  CSES 1638  ·  ★1
https://cses.fi/problemset/task/1638
**Technique:** grid DP (`../../algorithms_learning/07_dynamic_programming/lesson.md` §4).
<details><summary>Hint</summary>`ways[i][j] = ways[i−1][j] + ways[i][j−1]`, zero on traps.</details>
<details><summary>Approach sketch</summary>Standard. `n ≤ 1000`, O(n²). Warm-up for 05.26.</details>

### 05.26  Grid Paths II  ·  CSES 1078  ·  ★4
https://cses.fi/problemset/task/1078
**Technique:** lattice-path binomials + first-obstacle inclusion–exclusion DP.
<details><summary>Hint</summary>`n ≤ 1e6`, `≤ 1000` traps: count paths that reach each trap first, in sorted order.</details>
<details><summary>Approach sketch</summary>Sort traps by `(row, col)`. `bad[i] = C(r_i + c_i, r_i) − Σ_{j<i} bad[j] · C(r_i − r_j + c_i − c_j, r_i − r_j)` (skipping `j` not dominated by `i`). Answer `C(2n−2, n−1) − Σ_j bad[j] · paths(j → end)`. Tables to `2e6`. Mind `+MOD` after every subtraction.</details>

### 05.27  Gerald and Giant Chess  ·  Codeforces 559C  ·  ★4 (2200)
https://codeforces.com/problemset/problem/559/C
**Technique:** same as 05.26 with different framing.
<details><summary>Hint</summary>`h, w ≤ 1e5`, `n ≤ 2000` black cells. Identical DP.</details>
<details><summary>Approach sketch</summary>Exactly 05.26 — solve it right after, from memory, in one go. Then compare the two codes: they should be the same function.</details>

### 05.28  Two Arrays  ·  Codeforces 1288C  ·  ★3 (1600)
https://codeforces.com/problemset/problem/1288/C
**Technique:** bijection to a single monotone sequence + stars and bars.
<details><summary>Hint</summary>`a` non-decreasing, `b` non-increasing, `a_m ≤ b_m`: read `a_1..a_m, b_m..b_1` as one sequence.</details>
<details><summary>Approach sketch</summary>The concatenation is a non-decreasing sequence of length `2m` over `{1..n}`; multisets of size `2m` from `n` types: `C(n + 2m − 1, 2m)`. `m ≤ 10` so even a `O(n·m)` DP works, but the formula is one line.</details>

### 05.29  Colorful Blocks  ·  AtCoder ABC167 E  ·  ★3
https://atcoder.jp/contests/abc167/tasks/abc167_e
**Technique:** choose which adjacent pairs are equal, product rule.
<details><summary>Hint</summary>Fix `i` (≤ `k`) adjacent pairs with equal colours: the sequence collapses into `n − i` blocks.</details>
<details><summary>Approach sketch</summary>`Σ_{i=0}^{k} C(n−1, i) · m · (m−1)^{n−1−i}`. Tables to `2e5`.</details>

### 05.30  Strivore  ·  AtCoder ABC171 F  ·  ★4
https://atcoder.jp/contests/abc171/tasks/abc171_f
**Technique:** counting supersequences by fixing the leftmost embedding.
<details><summary>Hint</summary>Count strings of length `|S| + k` containing `S` as a subsequence — make the count unique by always matching greedily from the left.</details>
<details><summary>Approach sketch</summary>Let the greedy embedding put the last character of `S` at position `i` (`|S| ≤ i ≤ |S| + k`). Before it, the `i − |S|` non-matched positions each have `25` choices (they must not equal the next character to be matched); after it, `26` each. Sum `C(i−1, |S|−1) · 25^{i−|S|} · 26^{|S|+k−i}`.</details>

## F. Probability and expected value

### 05.31  Dice Probability  ·  CSES 1725  ·  ★2
https://cses.fi/problemset/task/1725
**Technique:** probability DP over sums.
<details><summary>Hint</summary>`dp[i][s]` = probability that `i` dice sum to `s`.</details>
<details><summary>Approach sketch</summary>`dp[i][s] = Σ_{f=1}^{6} dp[i−1][s−f] / 6`; answer `Σ_{s=a}^{b} dp[n][s]`. `n ≤ 100`, sums ≤ 600. Print with 6 decimals.</details>

### 05.32  Candy Lottery  ·  CSES 1727  ·  ★2
https://cses.fi/problemset/task/1727
**Technique:** expected maximum via `E[X] = Σ_v P(X ≥ v)`.
<details><summary>Hint</summary>`P(max ≤ v) = (v/k)^n`.</details>
<details><summary>Approach sketch</summary>`E = Σ_{v=1}^{k} v · ((v/k)^n − ((v−1)/k)^n)`. Doubles, print with 2 decimals (`printf("%.2f")` — the judge expects standard rounding).</details>

### 05.33  Inversion Probability  ·  CSES 1728  ·  ★3
https://cses.fi/problemset/task/1728
**Technique:** linearity of expectation with pair indicators.
<details><summary>Hint</summary>`E[#inversions] = Σ_{i<j} P(a_i > a_j)`; each probability depends only on `(r_i, r_j)`.</details>
<details><summary>Approach sketch</summary>With `a_i` uniform in `[1,x]`, `a_j` uniform in `[1,y]`: if `x ≤ y`, `P = x(x−1)/(2xy)`; else `P = (xy − y(y+1)/2)/(xy)`. `n ≤ 100`, O(n²) pairs.</details>

### 05.34  Moving Robots  ·  CSES 1726  ·  ★3
https://cses.fi/problemset/task/1726
**Technique:** independence across robots + linearity over cells.
<details><summary>Hint</summary>Expected empty squares `= Σ_cells Π_robots (1 − P(robot ends on cell))`.</details>
<details><summary>Approach sketch</summary>For each of the 64 robots run a `k`-step distribution DP on the 8×8 board (uniform over the 2–4 neighbours). Robots move independently, so `P(cell empty) = Π_r (1 − p_r(cell))`. Total work `64 · k · 64 · 4`.</details>

### 05.35  Little Pony and Expected Maximum  ·  Codeforces 453A  ·  ★2 (1600)
https://codeforces.com/problemset/problem/453/A
**Technique:** expected maximum of i.i.d. dice.
<details><summary>Hint</summary>Same as 05.32 with `m`-sided dice and `n` throws — beware `m^n` overflow: never compute powers of integers here, use `pow((double)v/m, n)`.</details>
<details><summary>Approach sketch</summary>`Σ_{v=1}^{m} v·((v/m)^n − ((v−1)/m)^n)`; `m ≤ 1e5`, `n ≤ 1e5`. Print with `1e-4` tolerance.</details>

### 05.36  Let's Play Osu!  ·  Codeforces 235B  ·  ★3 (2000)
https://codeforces.com/problemset/problem/235/B
**Technique:** linearity over pairs inside runs; telescoping DP.
<details><summary>Hint</summary>`len² = len + 2·C(len,2)`; the number of pairs `(i<j)` inside one run is the number of pairs `(i,j)` with all of `i..j` equal to O.</details>
<details><summary>Approach sketch</summary>`E[score] = Σ_i p_i + 2 Σ_{i<j} Π_{t=i}^{j} p_t`. Maintain `run[j] = Σ_{i<j} Π_{t=i}^{j} p_t = p_j (run[j−1] + p_{j−1})`-style accumulator — one pass, O(n).</details>

### 05.37  Coins  ·  AtCoder EDPC I  ·  ★2
https://atcoder.jp/contests/dp/tasks/dp_i
**Technique:** probability DP with heterogeneous coins.
<details><summary>Hint</summary>`dp[i][h]` = probability of exactly `h` heads among the first `i` coins.</details>
<details><summary>Approach sketch</summary>`dp[i][h] = dp[i−1][h−1]·p_i + dp[i−1][h]·(1−p_i)`; answer `Σ_{h > n/2} dp[n][h]`. `n ≤ 2999`, O(n²).</details>

### 05.38  Sushi  ·  AtCoder EDPC J  ·  ★3
https://atcoder.jp/contests/dp/tasks/dp_j
**Technique:** expected steps DP with a self-loop.
<details><summary>Hint</summary>State = (number of plates with 1, 2, 3 sushi). The self-loop (picking an empty plate) is solved algebraically, not iterated.</details>
<details><summary>Approach sketch</summary>`E[a,b,c] = (n + a·E[a−1,b,c] + b·E[a+1,b−1,c] + c·E[a,b+1,c−1]) / (a+b+c)` after moving the `(n−a−b−c)/n · E[a,b,c]` term to the left. Memoize; `~ n³/6` states for `n ≤ 300`.</details>

## G. Recurrences — preview of chapter 06

### 05.39  Fibonacci Numbers  ·  CSES 1722  ·  ★2
https://cses.fi/problemset/task/1722
**Technique:** fast doubling or 2×2 matrix power.
<details><summary>Hint</summary>`n ≤ 1e18`: O(log n) with `F_{2k} = F_k(2F_{k+1} − F_k)`, `F_{2k+1} = F_k² + F_{k+1}²`.</details>
<details><summary>Approach sketch</summary>Recursive doubling returning the pair `(F_n, F_{n+1})`; watch negative values in `2F_{k+1} − F_k`. Chapter 06 redoes this with matrices; solve it both ways.</details>

### 05.40  Throwing Dice  ·  CSES 1096  ·  ★3
https://cses.fi/problemset/task/1096
**Technique:** 6-term linear recurrence, `n ≤ 1e18` → matrix power (ch06).
<details><summary>Hint</summary>`f(n) = Σ_{i=1}^{6} f(n−i)`; the 6×6 companion matrix raised to the `n`th power.</details>
<details><summary>Approach sketch</summary>Come back after ch06 §3; write the general "k-step recurrence → companion matrix" helper and reuse it here.</details>

### 05.41  Graph Paths I / II  ·  CSES 1723 / 1724  ·  ★3 / ★4
https://cses.fi/problemset/task/1723 · https://cses.fi/problemset/task/1724
**Technique:** counting walks = adjacency matrix power; shortest walk of exact length = min-plus power (ch06).
<details><summary>Hint</summary>`A^k[i][j]` counts walks of length `k`; with `(min, +)` instead of `(+, ×)` it gives the cheapest such walk.</details>
<details><summary>Approach sketch</summary>Chapter 06 §4–§5. Listed here so the ladder is complete.</details>

---

## Practice more

- Codeforces problemset, tag `combinatorics`, rating 1400–1900 (binomials, stars and bars, simple
  inclusion–exclusion); then 2000–2400 (inclusion–exclusion with DP, ballot numbers).
- Codeforces problemset, tag `probabilities`, rating 1600–2200 (linearity, expected-value DP).
- Codeforces problemset, tags `combinatorics` + `math`, search the statements for "rotation" — Burnside
  appears rarely but predictably at 2200+.
- AtCoder Beginner Contest, problems D–F with "mod 998244353" in the statement — nearly every round.
- AtCoder Educational DP Contest (contest `dp`): problems I, J, and the counting problems M, T.

## Progress

- [ ] 05.1 Two Knights (CSES 1072)
- [ ] 05.2 Bit Strings (CSES 1617)
- [ ] 05.3 Creating Strings (CSES 1622)
- [ ] 05.4 Binomial Coefficients (CSES 1079)
- [ ] 05.5 Creating Strings II (CSES 1715)
- [ ] 05.6 Distributing Apples (CSES 1716)
- [ ] 05.7 Bouquet (ABC156 D)
- [ ] 05.8 Christmas Party (CSES 1717)
- [ ] 05.9 Counting Sequences (CSES 2228)
- [ ] 05.10 Grid Completion (CSES 2429)
- [ ] 05.11 Counting Coprime Pairs (CSES 2417)
- [ ] 05.12 Another Filling the Grid (CF 1228E)
- [ ] 05.13 Placing Rooks (CF 1342E)
- [ ] 05.14 Bracket Sequences I (CSES 2064)
- [ ] 05.15 Bracket Sequences II (CSES 2187)
- [ ] 05.16 Empty String (CSES 1080)
- [ ] 05.17 Natasha, Sasha and the Prefix Sums (CF 1204E)
- [ ] 05.18 Counting Necklaces (CSES 2209)
- [ ] 05.19 Counting Grids (CSES 2210)
- [ ] 05.20 Cube (ABC198 F)
- [ ] 05.21 Two Sets II (CSES 1093)
- [ ] 05.22 Counting Towers (CSES 2413)
- [ ] 05.23 Permutation Inversions (CSES 2229)
- [ ] 05.24 Counting Bishops (CSES 2176)
- [ ] 05.25 Grid Paths I (CSES 1638)
- [ ] 05.26 Grid Paths II (CSES 1078)
- [ ] 05.27 Gerald and Giant Chess (CF 559C)
- [ ] 05.28 Two Arrays (CF 1288C)
- [ ] 05.29 Colorful Blocks (ABC167 E)
- [ ] 05.30 Strivore (ABC171 F)
- [ ] 05.31 Dice Probability (CSES 1725)
- [ ] 05.32 Candy Lottery (CSES 1727)
- [ ] 05.33 Inversion Probability (CSES 1728)
- [ ] 05.34 Moving Robots (CSES 1726)
- [ ] 05.35 Little Pony and Expected Maximum (CF 453A)
- [ ] 05.36 Let's Play Osu! (CF 235B)
- [ ] 05.37 Coins (EDPC I)
- [ ] 05.38 Sushi (EDPC J)
- [ ] 05.39 Fibonacci Numbers (CSES 1722)
- [ ] 05.40 Throwing Dice (CSES 1096)
- [ ] 05.41 Graph Paths I / II (CSES 1723 / 1724)
