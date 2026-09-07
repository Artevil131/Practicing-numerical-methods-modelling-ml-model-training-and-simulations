# Chapter 06 — Problems

How to work this ladder:

1. Re-type `example.cpp` from memory first: `Mat` + `mpow`, the companion-matrix builder, `MinPlus`,
   `gauss` (reals), `det_mod`, `gauss_gf2`, `XorBasis`. Compile, all asserts green.
2. Solutions go in `competitive_programming/06_matrices_and_linear_algebra_mod_p/solutions/<source>_<id>.cpp`.
3. For every matrix-power problem write the O(n) DP in the same file and assert equality for
   `n ≤ 50` on random inputs before submitting — the companion-matrix off-by-one is invisible otherwise.
4. For Gauss problems, generate a random solvable system (pick `x`, compute `b = Ax`), solve, compare.
5. Budgets: ★1–★2 ≤ 20 min, ★3 ≤ 40 min, ★4 ≤ 75 min. Upsolve within a week.

---

## A. Matrix power for recurrences

### 06.1  Fibonacci Numbers  ·  CSES 1722  ·  ★2
https://cses.fi/problemset/task/1722
**Technique:** 2×2 matrix power.
<details><summary>Hint</summary>`[[1,1],[1,0]]^n = [[F_{n+1},F_n],[F_n,F_{n−1}]]`.</details>
<details><summary>Approach sketch</summary>Compute `T^n` and read `T^n[0][1]`. Check `n = 0` (answer 0) and `n = 1` (1). Compare with the fast-doubling solution from ch05 — both O(log n).</details>

### 06.2  Throwing Dice  ·  CSES 1096  ·  ★3
https://cses.fi/problemset/task/1096
**Technique:** companion matrix of a 6-term recurrence.
<details><summary>Hint</summary>`f(n) = Σ_{i=1}^{6} f(n−i)`, `f(0) = 1`, `f(<0) = 0`. State vector `(f(n), …, f(n−5))`, start `(1,0,0,0,0,0)`.</details>
<details><summary>Approach sketch</summary>`T` has ones on the first row and the sub-diagonal. `f(n) = (T^n v_0)[0]`. Verify `f(1..6) = 1, 2, 4, 8, 16, 32` and `f(7) = 63` by the naive DP inside your program.</details>

### 06.3  Counting Towers with `n ≤ 1e18` (self-set variant of CSES 2413)  ·  ★3
https://cses.fi/problemset/task/2413
**Technique:** 2-state DP → 2×2 matrix.
<details><summary>Hint</summary>Solve the original (`n ≤ 1e6`) with the linear DP first; then pretend `n ≤ 1e18` and write the same transition as a matrix.</details>
<details><summary>Approach sketch</summary>`(wide, narrow)_{n} = [[2,1],[1,4]] · (wide, narrow)_{n−1}` with `(1,1)` at height 1. Submit the matrix version to the real problem too — it must pass. The point of the exercise is the "DP → matrix" translation.</details>

### 06.4  Magic Gems  ·  Codeforces 1117D  ·  ★3 (2100)
https://codeforces.com/problemset/problem/1117/D
**Technique:** `k`-term recurrence `f(n) = f(n−1) + f(n−m)` with `n ≤ 1e18`, `m ≤ 100`.
<details><summary>Hint</summary>The last unit of space is either a plain gem (`f(n−1)`) or the end of a split gem (`f(n−m)`).</details>
<details><summary>Approach sketch</summary>Companion matrix of size `m`; `m³ log n ≈ 1e6 · 60 · 2` — fine. Initial vector `f(0..m−1) = 1`.</details>

### 06.5  Plant  ·  Codeforces 185A  ·  ★2 (1600)
https://codeforces.com/problemset/problem/185/A
**Technique:** two-state recurrence with `n ≤ 1e18`.
<details><summary>Hint</summary>Track (upward triangles, downward triangles) after each step; each step is a fixed 2×2 linear map.</details>
<details><summary>Approach sketch</summary>`up' = 3·up + down`, `down' = up + 3·down`, start `(1, 0)`. Matrix power (or the closed form `(4^n + 2^n)/2` — derive it from the eigenvalues 4 and 2 as an exercise).</details>

### 06.6  Tetrahedron  ·  Codeforces 166E  ·  ★2 (1500)
https://codeforces.com/problemset/problem/166/E
**Technique:** walks of length `n` on `K_4` returning to the start.
<details><summary>Hint</summary>`n ≤ 1e7` so a two-variable DP suffices — but write the adjacency-matrix power as well and compare.</details>
<details><summary>Approach sketch</summary>Let `d_n` = walks of length `n` ending at D, `o_n` = walks ending at one specific other vertex. Then `d_{n+1} = 3·o_n`, `o_{n+1} = d_n + 2·o_n`, start `(d_0, o_0) = (1, 0)`. Equivalently `(A^n)[D][D]` with `A = J − I` on 4 vertices.</details>

### 06.7  Xor-sequences  ·  Codeforces 691E  ·  ★3 (1800)
https://codeforces.com/problemset/problem/691/E
**Technique:** count walks of length `k−1` in a graph on `n ≤ 100` values.
<details><summary>Hint</summary>Edge `a_i → a_j` iff `popcount(a_i xor a_j)` is a multiple of 3. Sequences of length `k` are walks with `k−1` edges.</details>
<details><summary>Approach sketch</summary>`Σ_{i,j} (A^{k−1})[i][j]`, `k ≤ 1e18`. `100³ · 60 · 2 ≈ 1.2e8` — fine, but the 16-term accumulation trick from the lesson is worth having ready.</details>

### 06.8  Decoding Genome  ·  Codeforces 222E  ·  ★3 (1900)
https://codeforces.com/problemset/problem/222/E
**Technique:** automaton with forbidden adjacent pairs as an adjacency matrix.
<details><summary>Hint</summary>Letters are nodes, allowed transitions are edges; a genome of length `n` is a walk with `n−1` edges.</details>
<details><summary>Approach sketch</summary>`m ≤ 52` nodes, `n ≤ 1e15`: `Σ (A^{n−1})[i][j]` over all pairs. Handle `n = 1` separately.</details>

### 06.9  Runner's Problem  ·  Codeforces 954F  ·  ★4 (2300)
https://codeforces.com/problemset/problem/954/F
**Technique:** piecewise-constant transition matrices over sorted segments.
<details><summary>Hint</summary>Three rows; a blocked cell zeroes one row of the 3×3 transition. Blocked intervals are given as at most `1e4` segments over `n ≤ 1e18` columns.</details>
<details><summary>Approach sketch</summary>Sweep the column axis by segment boundaries. Between consecutive boundaries the set of blocked rows is constant, so raise the corresponding (up to 8 distinct) 3×3 matrices to the segment length. Total O(#segments · 27 · 60).</details>

## B. Semirings: counting, min-plus, boolean

### 06.10  Graph Paths I  ·  CSES 1723  ·  ★3
https://cses.fi/problemset/task/1723
**Technique:** adjacency-matrix power, `(+,×)` semiring.
<details><summary>Hint</summary>`(A^k)[1][n]` with `n ≤ 100`, `k ≤ 1e9`.</details>
<details><summary>Approach sketch</summary>Count parallel edges in `A` (they are allowed). O(n³ log k).</details>

### 06.11  Graph Paths II  ·  CSES 1724  ·  ★4
https://cses.fi/problemset/task/1724
**Technique:** min-plus matrix power.
<details><summary>Hint</summary>Replace `(+,×)` by `(min,+)`; identity is `0` on the diagonal and `INF` elsewhere.</details>
<details><summary>Approach sketch</summary>Keep the cheapest parallel edge. If the result is `INF`, print `-1`. Guard additions with `INF` to avoid overflow.</details>

### 06.12  Fixed Length Walk Queries  ·  CSES 3357  ·  ★4
https://cses.fi/problemset/task/3357
**Technique:** when **not** to use matrices: parity + BFS.
<details><summary>Hint</summary>`n` is far too large for `n³`. In an undirected graph, if a walk of length `L` from `a` to `b` exists, so does one of length `L + 2` (bounce along an edge). So only the shortest even and shortest odd walk lengths matter.</details>
<details><summary>Approach sketch</summary>BFS on states `(vertex, parity)` from each query source — or, since queries are many, note the answer depends on the component and on the shortest even/odd distances; precompute per component with multi-source reasoning (bipartite components have only one parity per pair). Recognize this pattern to avoid a hopeless matrix approach.</details>

### 06.13  Reachable Nodes  ·  CSES 2138  ·  ★3
https://cses.fi/problemset/task/2138
**Technique:** boolean semiring with bitsets (transitive closure in a DAG).
<details><summary>Hint</summary>Process nodes in reverse topological order; `reach[v] = OR of reach[u]` over edges `v → u`, plus `v` itself. Each OR is a `bitset<50001>` operation.</details>
<details><summary>Approach sketch</summary>`n, m ≤ 5e4`: `m · n / 64 ≈ 4e7` word ops. This is one row of a boolean matrix product per edge. Answer `reach[v].count()`.</details>

### 06.14  Counting Tilings  ·  CSES 2181  ·  ★4
https://cses.fi/problemset/task/2181
**Technique:** broken-profile DP; then the transition-matrix view.
<details><summary>Hint</summary>Solve with the standard profile DP over `2^m` masks per column (m ≤ 10, n ≤ 1000). Then, for `m ≤ 6`, build the `2^m × 2^m` column-to-column matrix and check `(T^n)[0][0]` matches — this is the "n ≤ 1e18" upgrade path.</details>
<details><summary>Approach sketch</summary>Profile = which cells of the next column are already filled by horizontal dominoes. Column transition: fill the free cells of the current column with vertical dominoes (pairs of consecutive free cells) or start horizontal ones. Enumerate compatible pairs of masks by recursion over rows. The matrix `T` is exactly the count of ways for each `(mask, mask')`.</details>

## C. Recurrence recovery

### 06.15  Berlekamp–Massey drill (self-set)  ·  ★3
**Technique:** BM + Kitamasa on your own sequences.
<details><summary>Hint</summary>Generate 40 terms of `a_n = 3a_{n−1} − 2a_{n−3} + a_{n−4}` mod p from random initial values, feed to BM, confirm it returns `(3, 0, −2, 1)`, then compute `a_{1e18}` with Kitamasa and with the 4×4 matrix and assert equality.</details>
<details><summary>Approach sketch</summary>Then do the same for the Counting Tilings sequence with `m = 8` (`256` states): compute `600` terms by the profile DP, BM gives a recurrence of order ≤ 256, Kitamasa evaluates any `n`. This is the full "profile DP with huge n" pipeline.</details>

## D. Gaussian elimination and XOR basis

### 06.16  System of Linear Equations  ·  CSES 3154  ·  ★3
https://cses.fi/problemset/task/3154
**Technique:** Gauss–Jordan over `Z_p`.
<details><summary>Hint</summary>Reduced row echelon form; classify none / any solution; free variables set to 0.</details>
<details><summary>Approach sketch</summary>Eliminate with modular inverses. After elimination, a row that is all zeros in `A` but non-zero in `b` means no solution. Otherwise output pivot variables from their rows and `0` for free variables. Read the exact output format (the judge may accept any valid solution).</details>

### 06.17  Maximum Xor Subset  ·  CSES 3191  ·  ★2
https://cses.fi/problemset/task/3191
**Technique:** XOR basis, greedy maximum.
<details><summary>Hint</summary>Insert all values; walk from the highest bit down, XOR in the basis vector when it sets a new bit.</details>
<details><summary>Approach sketch</summary>`insert` then `max_xor()`. O(n · 30).</details>

### 06.18  Number of Subset Xors  ·  CSES 3211  ·  ★2
https://cses.fi/problemset/task/3211
**Technique:** span size = `2^rank`.
<details><summary>Hint</summary>Distinct achievable XOR values form the linear span of the input over `GF(2)`.</details>
<details><summary>Approach sketch</summary>Build the basis, output `2^rank` (mod p if required by the statement; if the answer must be exact and rank ≤ 30, a `long long` suffices).</details>

### 06.19  K Subset Xors  ·  CSES 3192  ·  ★4
https://cses.fi/problemset/task/3192
**Technique:** fully reduced XOR basis, k-th value with multiplicities.
<details><summary>Hint</summary>Every achievable value is produced by exactly `2^{n−rank}` subsets. Reduce the basis so each pivot bit appears in one vector only; then the `j`-th smallest distinct value is the XOR of the basis vectors selected by the bits of `j`.</details>
<details><summary>Approach sketch</summary>Given `k`, compute `j = (k−1) / 2^{n−rank}` (careful when `n − rank ≥ 60`: then `j = 0`), answer `kth(j+1)`. Sort the outputs if the task asks for the sequence.</details>

### 06.20  Square Subsets  ·  CSES 3193  ·  ★4
https://cses.fi/problemset/task/3193
**Technique:** XOR basis over prime-exponent parity vectors.
<details><summary>Hint</summary>Values ≤ 70 → 19 primes. A product is a perfect square iff every prime's total exponent is even.</details>
<details><summary>Approach sketch</summary>Map each number to a 19-bit parity vector, insert into the basis, answer `2^{n − rank} − 1` mod p (non-empty subsets whose vector-sum is 0). Same as Codeforces 895C.</details>

### 06.21  (Zero XOR Subset)-less  ·  Codeforces 1101G  ·  ★3 (2300)
https://codeforces.com/problemset/problem/1101/G
**Technique:** rank of prefix XORs.
<details><summary>Hint</summary>Segments correspond to differences of prefix XORs; a "bad" subset of segments exists iff the chosen prefix values are linearly dependent.</details>
<details><summary>Approach sketch</summary>Answer is the rank of `{p_1, …, p_n}` (prefix XORs), or `−1` if `p_n = 0` (the whole array XORs to 0, so no valid split exists).</details>

### 06.22  Ivan and Burgers  ·  Codeforces 1100F  ·  ★4 (2500)
https://codeforces.com/problemset/problem/1100/F
**Technique:** XOR basis with "latest index wins" for range queries offline.
<details><summary>Hint</summary>Maintain, for each bit, the basis vector with the largest index among those that could occupy it. Then a query `[l, r]` uses only basis vectors with index ≥ `l` from the prefix basis at `r`.</details>
<details><summary>Approach sketch</summary>Sweep `r` left to right; when inserting `a_r` with index `r`, at each bit prefer keeping the vector with the larger index and push the smaller one down. Answer queries sorted by `r` with the greedy max restricted to index ≥ `l`. O((n + q) · 20).</details>

### 06.23  Shortest Path Problem?  ·  Codeforces 845G  ·  ★4 (2300)
https://codeforces.com/problemset/problem/845/G
**Technique:** XOR basis of cycle space + any path.
<details><summary>Hint</summary>XOR of any path from 1 to n equals (XOR of a fixed spanning-tree path) XOR (some element of the span of cycle XORs).</details>
<details><summary>Approach sketch</summary>DFS; for each non-tree edge `(u,v,w)` insert `d[u] xor d[v] xor w` into the basis. Answer = minimize `d[n]` over the span: walk bits from high to low, XOR in a basis vector when it clears a set bit.</details>

## E. Determinants, Kirchhoff, Markov systems

### 06.24  Kirchhoff drill (self-set)  ·  ★3
**Technique:** matrix-tree theorem via `det_mod`.
<details><summary>Hint</summary>Verify `K_n` gives `n^{n−2}` for `n = 3..8`, `C_n` gives `n`, and a random multigraph on 6 vertices agrees with a brute force over all edge subsets of size `n−1`.</details>
<details><summary>Approach sketch</summary>Build `L`, drop row/column 0, determinant mod p. For the brute force use DSU to test "acyclic and spanning". Then do the directed version (arborescences) with `D_out` and check on a small DAG by hand.</details>

### 06.25  Highways  ·  SPOJ HIGH  ·  ★3
https://www.spoj.com/problems/HIGH/
**Technique:** number of spanning trees, exact answer.
<details><summary>Hint</summary>`n ≤ 12`: the count fits in `long long`; Gauss over doubles (or `long double`) and round, or fraction-free elimination.</details>
<details><summary>Approach sketch</summary>Laplacian cofactor with floating-point Gauss; the answer is at most `12^{10} ≈ 6e10`, so rounding a `double` determinant is safe. Watch multi-edges (statement: none) and disconnected graphs (determinant 0).</details>

### 06.26  Broken robot  ·  Codeforces 24D  ·  ★4 (2200)
https://codeforces.com/problemset/problem/24/D
**Technique:** expected steps, row-by-row tridiagonal linear systems.
<details><summary>Hint</summary>The robot moves down, left, right or stays with equal probability among the legal moves. Rows below are already solved; each row is a system where `E[i][j]` depends on `E[i][j±1]` and `E[i+1][j]`.</details>
<details><summary>Approach sketch</summary>For each row from bottom to top solve the tridiagonal system with the Thomas algorithm (forward sweep, back substitution) in O(m). Handle `m = 1` (only down/stay) separately. Print with `1e-4` precision.</details>

### 06.27  Expected-value systems drill (self-set)  ·  ★3
**Technique:** `(I − Q) E = 1` with Gauss over reals.
<details><summary>Hint</summary>Random walk on `{0..n}` with reflection at 0 and absorption at `n`: `E_0 = n²`. Set up the system and check numerically for `n ≤ 50`; then a random graph walk: compare Gauss with a Monte-Carlo estimate.</details>
<details><summary>Approach sketch</summary>Unknowns `E_0..E_{n−1}`; `E_0 = 1 + E_1`, `E_i = 1 + (E_{i−1} + E_{i+1})/2`, `E_n = 0`. Solve, assert `|E_0 − n²| < 1e-6`. Same code solves any absorbing chain with ≤ 500 states.</details>

---

## Practice more

- Codeforces problemset, tag `matrices`, rating 1600–2300 (recurrence → matrix, walks of length k,
  segment-wise matrices).
- Codeforces problemset, search "linear basis" / "xor basis" in editorials; tag `math` + `bitmasks`,
  rating 1900–2400.
- Codeforces problemset, tag `math` with "Gaussian elimination" in the editorial, rating 2000–2500
  (expected values with cycles; parity puzzles over `GF(2)`).
- Matrix-tree theorem: search Codeforces / AtCoder editorials for "Kirchhoff" or "matrix tree" —
  rare (rating 2400+), but the drill 06.24 covers the technique.
- Berlekamp–Massey: Codeforces blogs on "Berlekamp–Massey" list practice problems; AtCoder ABC F/G
  problems with `n ≤ 1e18` and a small hidden state are the natural targets.

## Progress

- [ ] 06.1 Fibonacci Numbers (CSES 1722)
- [ ] 06.2 Throwing Dice (CSES 1096)
- [ ] 06.3 Counting Towers, matrix version (CSES 2413)
- [ ] 06.4 Magic Gems (CF 1117D)
- [ ] 06.5 Plant (CF 185A)
- [ ] 06.6 Tetrahedron (CF 166E)
- [ ] 06.7 Xor-sequences (CF 691E)
- [ ] 06.8 Decoding Genome (CF 222E)
- [ ] 06.9 Runner's Problem (CF 954F)
- [ ] 06.10 Graph Paths I (CSES 1723)
- [ ] 06.11 Graph Paths II (CSES 1724)
- [ ] 06.12 Fixed Length Walk Queries (CSES 3357)
- [ ] 06.13 Reachable Nodes (CSES 2138)
- [ ] 06.14 Counting Tilings (CSES 2181)
- [ ] 06.15 Berlekamp–Massey drill
- [ ] 06.16 System of Linear Equations (CSES 3154)
- [ ] 06.17 Maximum Xor Subset (CSES 3191)
- [ ] 06.18 Number of Subset Xors (CSES 3211)
- [ ] 06.19 K Subset Xors (CSES 3192)
- [ ] 06.20 Square Subsets (CSES 3193)
- [ ] 06.21 (Zero XOR Subset)-less (CF 1101G)
- [ ] 06.22 Ivan and Burgers (CF 1100F)
- [ ] 06.23 Shortest Path Problem? (CF 845G)
- [ ] 06.24 Kirchhoff drill
- [ ] 06.25 Highways (SPOJ HIGH)
- [ ] 06.26 Broken robot (CF 24D)
- [ ] 06.27 Expected-value systems drill
