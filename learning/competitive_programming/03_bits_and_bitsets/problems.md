# Chapter 03 — Problems

How to work this ladder:

1. Re-type the chapter library (`example.cpp`) from memory first: submask loop, SOS, xor basis,
   Hamiltonian DP, bitset knapsack. Compile with `-Wall -Wextra`. Only then open the problems.
2. One file per problem: `learning/competitive_programming/03_bits_and_bitsets/solutions/<source>_<id>.cpp`
   (e.g. `solutions/cses_1654.cpp`, `solutions/cf_165E.cpp`).
3. Before submitting anything with a DP or a basis, stress-test against a brute force
   (enumerate all `2^n` subsets for `n ≤ 12`, random inputs, 1000 iterations).
4. Time yourself: ★1–2 in ≤ 15 min, ★3 in ≤ 40 min, ★4–5 up to 90 min, then read the hint,
   then the approach, then upsolve. Re-solve anything that needed the approach a week later.

Difficulty: ★1 (warm-up) … ★5 (Div1 C / IOI subtask 4). CF rating estimates in brackets.

---

## A. Bit basics and counting

### 03.1  Bit Strings  ·  CSES 1617  ·  ★1 (800)
https://cses.fi/problemset/task/1617
**Technique:** modular exponentiation / bits as choices.
<details><summary>Hint</summary>Each of the n positions is an independent 0/1 choice.</details>
<details><summary>Approach sketch</summary>Answer is 2^n mod 10^9+7. Either loop n times multiplying by 2 (n ≤ 10^6 is fine) or use binary exponentiation. The point of the exercise: never try to build 2^n in a `long long`.</details>

### 03.2  Gray Code  ·  CSES 2205  ·  ★1 (900)
https://cses.fi/problemset/task/2205
**Technique:** `i ^ (i >> 1)`.
<details><summary>Hint</summary>The i-th code word is a one-line function of i.</details>
<details><summary>Approach sketch</summary>Print `gray(i) = i ^ (i >> 1)` for i = 0..2^n−1 as an n-bit string (print bits from n−1 down to 0). Alternative recursive construction: list for n−1 with prefix 0, then the reversed list with prefix 1 — exactly what the xor formula produces.</details>

### 03.3  Two Sets  ·  CSES 1092  ·  ★1 (1000)
https://cses.fi/problemset/task/1092
**Technique:** parity via bits.
<details><summary>Hint</summary>Total sum n(n+1)/2 must be even; check with `& 1`, not `% 2`, on the product computed in `long long`.</details>
<details><summary>Approach sketch</summary>If n(n+1)/2 is odd, answer NO. Otherwise n mod 4 ∈ {0, 3}. Pair numbers from both ends (1 with n, 2 with n−1, …) alternating the target set, or use the pattern for n ≡ 0 (mod 4): in each block of four consecutive numbers put the outer two in A and the inner two in B; for n ≡ 3 start with {1,2},{3} and continue in blocks of four.</details>

### 03.4  Counting Bits  ·  CSES 1146  ·  ★2 (1400)
https://cses.fi/problemset/task/1146
**Technique:** per-bit periodicity, O(log n).
<details><summary>Hint</summary>Bit b over 0..n is periodic with period 2^(b+1): 2^b zeros then 2^b ones.</details>
<details><summary>Approach sketch</summary>For each bit b: with N = n+1 numbers, `full = N / 2^(b+1)` complete periods contribute `full · 2^b`; the tail of length `N mod 2^(b+1)` contributes `max(0, tail − 2^b)`. Sum over b < 50. Verify against a brute force for n ≤ 2000 before submitting — the off-by-one in "0..n" vs "1..n" is invisible on samples.</details>

### 03.5  Xor Pyramid Peak  ·  CSES 2419  ·  ★3 (1700)
https://cses.fi/problemset/task/2419
**Technique:** Lucas' theorem as a bit test.
<details><summary>Hint</summary>The peak is the xor of a_i taken with multiplicity C(n−1, i). Only the parity of that binomial matters.</details>
<details><summary>Approach sketch</summary>Repeatedly xoring adjacent pairs is Pascal's triangle over GF(2): the peak = ⊕ a_i over i with C(n−1, i) odd. By Lucas' theorem C(m, i) is odd iff i's set bits are a subset of m's: `(i & m) == i`. Loop i in O(n), xor the matching a_i.</details>

---

## B. Subset enumeration and meet in the middle

### 03.6  Apple Division  ·  CSES 1623  ·  ★1 (1100)
https://cses.fi/problemset/task/1623
**Technique:** brute force over 2^n masks with incremental sums.
<details><summary>Hint</summary>n ≤ 20 → 10^6 subsets. Compute each subset sum in O(1) from a smaller mask.</details>
<details><summary>Approach sketch</summary>`sum[m] = sum[m & (m−1)] + a[ctz(m)]`; answer min over m of |total − 2·sum[m]|. Use `long long` (weights up to 10^9). Recursion with include/exclude is equally fine here.</details>

### 03.7  Chessboard and Queens  ·  CSES 1624  ·  ★2 (1300)
https://cses.fi/problemset/task/1624
**Technique:** backtracking with column/diagonal masks (see chapter 02 for the search itself).
<details><summary>Hint</summary>Three masks: used columns, used `r+c` diagonals, used `r−c+7` anti-diagonals.</details>
<details><summary>Approach sketch</summary>Place row by row. Free cells of row r are `~(cols | (d1 >> r) | (d2 >> (7−r)))` intersected with the row's non-reserved cells, if you shift the diagonal masks per row; or simply keep three plain masks and test `1 << c`, `1 << (r+c)`, `1 << (r−c+7)`. 8 rows × 8 columns, 92 solutions without reserved squares.</details>

### 03.8  Meet in the Middle  ·  CSES 1628  ·  ★3 (1700)
https://cses.fi/problemset/task/1628
**Technique:** split, enumerate 2^(n/2) sums per half, sort + binary search.
<details><summary>Hint</summary>2^40 is impossible; 2 · 2^20 is not.</details>
<details><summary>Approach sketch</summary>Enumerate all subset sums of the first ⌊n/2⌋ elements and of the rest (incremental `sum[m]` formula). Sort the right list; for each left sum s add `count of x − s` in the right list via `equal_range`. Use `long long` sums; answer can exceed 2^31. Sorting 10^6 elements is well under 0.2 s.</details>

### 03.9  Hamming Distance  ·  CSES 2136  ·  ★2 (1500)
https://cses.fi/problemset/task/2136
**Technique:** popcount of xor over all pairs.
<details><summary>Hint</summary>n ≤ 2·10^4, k ≤ 30: n²/2 = 2·10^8 popcounts is fine if each pair costs one `__builtin_popcount`.</details>
<details><summary>Approach sketch</summary>Parse strings to `int`s. Double loop `min(ans, popcount(a[i] ^ a[j]))` in ~0.3 s. Alternative for tighter limits: iterate over all values with Hamming distance 1 in a hash set (n · k), then distance 2 via SOS-like tricks — unnecessary here.</details>

---

## C. Bitmask DP

### 03.10  Elevator Rides  ·  CSES 1653  ·  ★3 (1800)
https://cses.fi/problemset/task/1653
**Technique:** `dp[mask] = (rides, last weight)`, O(2^n · n).
<details><summary>Hint</summary>Two values per state, minimized lexicographically. Adding a person either fits in the last ride or opens a new one.</details>
<details><summary>Approach sketch</summary>For every mask try removing each person i from it: take `dp[mask ^ 1<<i]`, add w_i to the last ride if it fits, else increment rides and start a new ride with w_i; keep the lexicographic minimum. `dp[0] = (1, 0)`. Store as `pair<int, long long>` in an array of size 2^n, memory 16 MB.</details>

### 03.11  Hamiltonian Flights  ·  CSES 1690  ·  ★3 (1900)
https://cses.fi/problemset/task/1690
**Technique:** `dp[mask][v]` counting paths, O(2^n · m) with adjacency lists.
<details><summary>Hint</summary>City n may only be entered as the very last step; parallel flights count separately.</details>
<details><summary>Approach sketch</summary>dp[1][0] = 1. For each mask in increasing order and each v in mask with dp > 0, push along every edge v→w with w ∉ mask, skipping w = n−1 unless `mask | 1<<w` is full. Iterate the adjacency list of v (m ≤ 2·10^4) instead of an n×n matrix. Store `int` mod 10^9+7: 2^20 × 20 × 4 B = 84 MB.</details>

### 03.12  Counting Tilings  ·  CSES 2181  ·  ★4 (2100)
https://cses.fi/problemset/task/2181
**Technique:** broken-profile DP; the profile is a bitmask of the column boundary.
<details><summary>Hint</summary>n ≤ 10 rows, m ≤ 1000 columns. Profile = which cells of the current column are already covered by horizontal tiles poking in from the previous column.</details>
<details><summary>Approach sketch</summary>Process column by column. State: mask of cells in the next column already filled. To transition, enumerate ways to fill the current column's free cells with vertical dominoes (pairs of adjacent free rows) and horizontal dominoes (which set the bit in the next column's mask) — a recursive fill over rows generates all valid next masks. O(m · 2^n · (something small)); with n = 10 the transition table has ~ 2^10 × (fills) entries and can be precomputed once.</details>

### 03.13  Kefa and Dishes  ·  Codeforces 580D  ·  ★3 (1800)
https://codeforces.com/problemset/problem/580/D
**Technique:** `dp[mask][last]` with pair bonuses.
<details><summary>Hint</summary>n ≤ 18 dishes, choose exactly m in some order maximizing satisfaction + adjacency bonuses.</details>
<details><summary>Approach sketch</summary>dp[mask][last] = best value having eaten `mask` with `last` most recent. Transition adds dish j ∉ mask with `a_j + bonus[last][j]`. Answer max over masks with popcount m. 2^18 · 18 · 18 ≈ 8.5·10^7.</details>

### 03.14  Matching  ·  AtCoder Educational DP Contest O  ·  ★3 (1700)
https://atcoder.jp/contests/dp/tasks/dp_o
**Technique:** `dp[mask]` where the number of assigned men is popcount(mask).
<details><summary>Hint</summary>The i-th man is matched when |mask| = i, so the second dimension is implicit.</details>
<details><summary>Approach sketch</summary>dp[mask] = number of ways to match men 0..popcount(mask)−1 to the women in mask. Transition: for man i = popcount(mask), add each compatible woman j ∉ mask into `dp[mask | 1<<j]`. O(2^n · n), n = 21.</details>

### 03.15  Grouping  ·  AtCoder Educational DP Contest U  ·  ★4 (2000)
https://atcoder.jp/contests/dp/tasks/dp_u
**Technique:** submask DP, O(3^n).
<details><summary>Hint</summary>Precompute the score of every group in O(2^n · n) (add one element at a time), then dp over submasks.</details>
<details><summary>Approach sketch</summary>`score[mask]` = sum of pairwise values inside mask, built incrementally by adding the lowest bit. `dp[mask] = max over submask s containing the lowest bit of mask of score[s] + dp[mask ^ s]` — fixing the lowest bit in s halves the work and avoids double counting. n = 16 → 3^16 / 2 ≈ 2·10^7.</details>

---

## D. SOS DP

### 03.16  SOS Bit Problem  ·  CSES 1654  ·  ★3 (1900)
https://cses.fi/problemset/task/1654
**Technique:** subset-sum and superset-sum SOS.
<details><summary>Hint</summary>Three counts per x: y ⊆ x (subset sum), y ⊇ x (superset sum), y & x ≠ 0 (complement of y ⊆ ~x).</details>
<details><summary>Approach sketch</summary>Let cnt[v] be the frequency of value v (values < 2^20). `sub = SOS_subsets(cnt)`, `sup = SOS_supersets(cnt)`. For x: answers are sub[x], sup[x], and `n − sub[full ^ x]` (y & x = 0 ⇔ y ⊆ complement of x). O(20 · 2^20).</details>

### 03.17  And Subset Count  ·  CSES 3141  ·  ★4 (2100)
https://cses.fi/problemset/task/3141
**Technique:** superset SOS then inverse (Möbius on the lattice).
<details><summary>Hint</summary>Subsets whose AND is a superset of m are easy to count: any non-empty subset of the elements that are supersets of m.</details>
<details><summary>Approach sketch</summary>c[m] = number of elements y with y ⊇ m (superset SOS of the frequency array). g[m] = 2^{c[m]} − 1 counts non-empty subsets with AND ⊇ m. The exact-AND count is the inverse superset transform of g: run the superset loop with subtraction (mod p). O(k · 2^k).</details>

### 03.18  Xor Pyramid Diagonal  ·  CSES 3194  ·  ★4 (2100)
https://cses.fi/problemset/task/3194
**Technique:** SOS with xor as the operation.
<details><summary>Hint</summary>The left edge value at height h is ⊕ a_j over j with C(h, j) odd, i.e. over j ⊆ h in bits (Lucas).</details>
<details><summary>Approach sketch</summary>Pad n to a power of two. f[h] = xor of a_j over all submasks j of h is exactly the subset-SOS transform with xor instead of +. O(n log n). Output the diagonal for h = 0..n−1.</details>

### 03.19  Xor Pyramid Row  ·  CSES 3195  ·  ★4 (2100)
https://cses.fi/problemset/task/3195
**Technique:** decompose the number of xor steps into powers of two.
<details><summary>Hint</summary>After m steps position i holds ⊕ a_{i+t} over t ⊆ m. Applying "step 2^b" for each set bit b of m composes correctly.</details>
<details><summary>Approach sketch</summary>Doing 2^b elementary steps equals `a[i] ^= a[i + 2^b]` for all i (Pascal mod 2 on rows that are powers of two has only two ones). Iterate over the set bits of m = (number of steps) and apply each shift-xor in O(n). Total O(n log n), in place.</details>

### 03.20  Compatible Numbers  ·  Codeforces 165E  ·  ★3 (1900)
https://codeforces.com/problemset/problem/165/E
**Technique:** superset SOS storing an index instead of a sum.
<details><summary>Hint</summary>a_i & a_j = 0 ⇔ a_j ⊆ ~a_i. You need any element that is a subset of a given mask.</details>
<details><summary>Approach sketch</summary>any[mask] = index of some element equal to mask (or −1). Subset-SOS with "take any non-empty child": `if (any[mask] < 0) any[mask] = any[mask ^ 1<<i]`. Then answer for a_i is any[full ^ a_i]. 22 bits, O(22 · 2^22) ≈ 9·10^7.</details>

### 03.21  Jzzhu and Numbers  ·  Codeforces 449D  ·  ★4 (2100)
https://codeforces.com/problemset/problem/449/D
**Technique:** superset SOS + inclusion–exclusion (or inverse transform).
<details><summary>Hint</summary>Count subsets with AND exactly 0 = Σ over masks m of (−1)^{|m|} · (2^{c[m]} − 1) where c[m] = #elements ⊇ m.</details>
<details><summary>Approach sketch</summary>Superset SOS on the frequency array gives c[m]. Inclusion–exclusion over "AND has bits m set" with sign (−1)^{popcount(m)}. Equivalent to the And Subset Count solution evaluated at m = 0. Mod 10^9+7, 2^20 states.</details>

### 03.22  Bits And Pieces  ·  Codeforces 1208F  ·  ★5 (2600)
https://codeforces.com/problemset/problem/1208/F
**Technique:** superset SOS keeping the two largest indices.
<details><summary>Hint</summary>Maximize a_i | (a_j & a_k) with i < j < k. Fix i, build the answer greedily bit by bit from the top; you need "are there two indices > i whose values both contain mask m".</details>
<details><summary>Approach sketch</summary>For each mask keep the two largest positions of elements that are supersets of it (superset SOS merging pairs). Process i from left to right; for the current i greedily add high bits b not in a_i to the target mask while the stored second-largest index for the mask is still > i. O(2^21 · 21).</details>

---

## E. XOR and the linear basis

### 03.23  Maximum Xor Subarray  ·  CSES 1655  ·  ★3 (1800)
https://cses.fi/problemset/task/1655
**Technique:** prefix xors + binary trie.
<details><summary>Hint</summary>Subarray xor = P[r] ^ P[l−1]. Maximizing q ^ (something in a set) is a trie walk taking the opposite bit when possible.</details>
<details><summary>Approach sketch</summary>Insert prefix xors P[0..i−1] into a trie of 30 levels; for P[i] walk the trie preferring the branch that sets the current bit. O(n · 30). Note the linear basis does not directly answer this: you need max over pairs from a set, not max over the span.</details>

### 03.24  Maximum Xor Subset  ·  CSES 3191  ·  ★3 (1800)
https://cses.fi/problemset/task/3191
**Technique:** xor basis, greedy max.
<details><summary>Hint</summary>Build the echelon basis; greedily xor pivots from the top if that increases the value.</details>
<details><summary>Approach sketch</summary>Insert all values (O(n · 30)); `max_xor(0)`. The maximum of the span is attained by taking each pivot iff it sets a currently-zero bit, from the highest pivot down.</details>

### 03.25  Number of Subset Xors  ·  CSES 3211  ·  ★2 (1600)
https://cses.fi/problemset/task/3211
**Technique:** rank of the basis.
<details><summary>Hint</summary>The set of subset xors is a vector space over GF(2).</details>
<details><summary>Approach sketch</summary>Number of distinct xors = 2^rank (0 included, from the empty subset). Insert all values and print `1 << sz` (or `2^sz` mod p if required).</details>

### 03.26  K Subset Xors  ·  CSES 3192  ·  ★4 (2000)
https://cses.fi/problemset/task/3192
**Technique:** reduced echelon basis, k-th element by binary digits of k.
<details><summary>Hint</summary>After full reduction the pivots have disjoint leading bits; span elements ordered by value correspond to k written in binary over the pivots.</details>
<details><summary>Approach sketch</summary>Build the basis, run `reduce()` so each pivot bit appears in exactly one basis vector, collect pivots ascending. The k-th smallest (0-indexed) is the xor of pivots at the set bits of k. Check the problem's indexing (whether 0 counts as the first) against the sample.</details>

### 03.27  Square Subsets  ·  CSES 3193  ·  ★4 (2100)
https://cses.fi/problemset/task/3193
**Technique:** xor basis over exponent-parity vectors.
<details><summary>Hint</summary>A product is a perfect square iff every prime exponent is even. Values ≤ 70 → 19 primes → each number is a 19-bit parity vector.</details>
<details><summary>Approach sketch</summary>Map each number to the xor-vector of its prime exponents mod 2. Non-empty subsets with xor 0 number 2^{n − rank} − 1 (each of the 2^rank span elements has 2^{n−rank} preimages among the 2^n subsets). Compute rank with a 19-bit basis, answer with fast power mod p. Same problem: Codeforces 895C.</details>

### 03.28  All Subarray Xors  ·  CSES 3233  ·  ★5 (2400)
https://cses.fi/problemset/task/3233
**Technique:** prefix xor frequencies + xor convolution (Walsh–Hadamard transform).
<details><summary>Hint</summary>For each value x, the number of pairs (l, r) with P[l] ^ P[r] = x is the xor auto-convolution of the frequency array of prefix xors.</details>
<details><summary>Approach sketch</summary>Let f[v] = number of prefix xors equal to v (including P[0] = 0). Pairs (i < j) with P[i] ^ P[j] = x: compute g = f ⊛_xor f via the Walsh–Hadamard transform (transform, square pointwise, inverse transform), then subtract the i = j pairs (g[0] −= n+1) and halve. O(k · 2^k) with k = 20.</details>

### 03.29  (Zero XOR Subset)-less  ·  Codeforces 1101G  ·  ★4 (2000)
https://codeforces.com/problemset/problem/1101/G
**Technique:** rank of prefix xors.
<details><summary>Hint</summary>Segments' xors must have no zero-xor subset ⇔ they are linearly independent; the maximum count is the rank of the prefix-xor vectors, unless the total xor is 0.</details>
<details><summary>Approach sketch</summary>If the total xor is 0, answer −1 (the whole set of segments always xors to 0). Otherwise insert all prefix xors P[1..n] into a basis and output its rank: segment xors are differences of prefix xors, and independence of the chosen prefix set is what's needed.</details>

### 03.30  Ivan and Burgers  ·  Codeforces 1100F  ·  ★5 (2500)
https://codeforces.com/problemset/problem/1100/F
**Technique:** prefix basis with timestamps (keep the most recent vector per pivot).
<details><summary>Hint</summary>Query "max xor of a subset of a[l..r]". Build the basis of a[1..r] but when two vectors compete for a pivot keep the one with the larger index and push the older one down.</details>
<details><summary>Approach sketch</summary>Process r from 1 to n; maintain a basis where each pivot stores (vector, index). On insert, at each pivot, if the incoming index is newer swap it with the stored one and continue reducing the older vector. For a query (l, r) at time r use only pivots with index ≥ l in the greedy max. O((n + q) · 20). Offline sort queries by r.</details>

---

## F. Bitsets

### 03.31  Money Sums  ·  CSES 1745  ·  ★2 (1400)
https://cses.fi/problemset/task/1745
**Technique:** bitset knapsack.
<details><summary>Hint</summary>Only feasibility is asked. `bs |= bs << x`.</details>
<details><summary>Approach sketch</summary>`bitset<100001> bs; bs[0] = 1; for x: bs |= bs << x;` then list set indices ≥ 1. O(n · S / 64) ≈ 1.6·10^5 word ops. The plain O(nS) bool DP also passes; the bitset version is the one to remember.</details>

### 03.32  Corner Subgrid Check  ·  CSES 3360  ·  ★3 (1900)
https://cses.fi/problemset/task/3360
**Technique:** row bitsets + popcount of AND.
<details><summary>Hint</summary>Two rows share ≥ 2 black columns ⇔ a rectangle with four black corners exists.</details>
<details><summary>Approach sketch</summary>Store each row as `bitset<3000>`. For each pair of rows compute `(r_i & r_j).count() >= 2`. n²/2 · n/64 ≈ 2·10^8 word ops for n = 3000 — passes in ~0.5 s with −O2.</details>

### 03.33  Corner Subgrid Count  ·  CSES 2137  ·  ★3 (1900)
https://cses.fi/problemset/task/2137
**Technique:** same as above, summing C(count, 2).
<details><summary>Hint</summary>Each pair of rows with c common black columns contributes c(c−1)/2 rectangles.</details>
<details><summary>Approach sketch</summary>Identical loop; accumulate `c*(c-1)/2` in `long long`. Iterate j > i only.</details>

### 03.34  Reachable Nodes  ·  CSES 2138  ·  ★4 (2000)
https://cses.fi/problemset/task/2138
**Technique:** DAG reachability with bitsets, processing sources in blocks of 64.
<details><summary>Hint</summary>n = 5·10^4 rows of 5·10^4 bits is 312 MB. Do 64 sources at a time with one `unsigned long long` per vertex.</details>
<details><summary>Approach sketch</summary>Topologically sort. For each block of 64 source vertices: set bit b in `mask[v_b]`, then propagate along edges in topological order (`mask[u] |= mask[v]` for v→u). After the pass, popcount of mask[u] over all u tells, for each source in the block, how many nodes it reaches (sum bit-by-bit: `reach[v_b] += mask[u] >> b & 1`, or transpose by iterating set bits). Total O(n/64 · (n + m)) ≈ 8·10^7 word ops.</details>

### 03.35  Reachability Queries  ·  CSES 2143  ·  ★4 (2100)
https://cses.fi/problemset/task/2143
**Technique:** SCC condensation + blockwise bitset reachability.
<details><summary>Hint</summary>General digraph: condense strongly connected components first (chapter 11), then it's Reachable Nodes on a DAG plus per-query lookup.</details>
<details><summary>Approach sketch</summary>Build the condensation DAG. For each block of 64 components as sources run the propagation pass; answer every query (a, b) whose comp(a) is in the current block by testing the bit of comp(a) in mask[comp(b)]. Group queries by the block of their source. O(n/64 · (n + m) + q).</details>

---

Practice more: Codeforces problemset, tag `bitmasks`, rating 1600–2200 (bitmask DP and SOS);
tag `bitmasks` + `math`, rating 1900–2400 (linear basis). AtCoder Educational DP Contest problems
O and U above are the reference pair for `dp[mask]` and `3^n` submask DP.

## Progress

- [ ] 03.1  Bit Strings (CSES 1617)
- [ ] 03.2  Gray Code (CSES 2205)
- [ ] 03.3  Two Sets (CSES 1092)
- [ ] 03.4  Counting Bits (CSES 1146)
- [ ] 03.5  Xor Pyramid Peak (CSES 2419)
- [ ] 03.6  Apple Division (CSES 1623)
- [ ] 03.7  Chessboard and Queens (CSES 1624)
- [ ] 03.8  Meet in the Middle (CSES 1628)
- [ ] 03.9  Hamming Distance (CSES 2136)
- [ ] 03.10 Elevator Rides (CSES 1653)
- [ ] 03.11 Hamiltonian Flights (CSES 1690)
- [ ] 03.12 Counting Tilings (CSES 2181)
- [ ] 03.13 Kefa and Dishes (CF 580D)
- [ ] 03.14 Matching (AtCoder dp_o)
- [ ] 03.15 Grouping (AtCoder dp_u)
- [ ] 03.16 SOS Bit Problem (CSES 1654)
- [ ] 03.17 And Subset Count (CSES 3141)
- [ ] 03.18 Xor Pyramid Diagonal (CSES 3194)
- [ ] 03.19 Xor Pyramid Row (CSES 3195)
- [ ] 03.20 Compatible Numbers (CF 165E)
- [ ] 03.21 Jzzhu and Numbers (CF 449D)
- [ ] 03.22 Bits And Pieces (CF 1208F)
- [ ] 03.23 Maximum Xor Subarray (CSES 1655)
- [ ] 03.24 Maximum Xor Subset (CSES 3191)
- [ ] 03.25 Number of Subset Xors (CSES 3211)
- [ ] 03.26 K Subset Xors (CSES 3192)
- [ ] 03.27 Square Subsets (CSES 3193)
- [ ] 03.28 All Subarray Xors (CSES 3233)
- [ ] 03.29 (Zero XOR Subset)-less (CF 1101G)
- [ ] 03.30 Ivan and Burgers (CF 1100F)
- [ ] 03.31 Money Sums (CSES 1745)
- [ ] 03.32 Corner Subgrid Check (CSES 3360)
- [ ] 03.33 Corner Subgrid Count (CSES 2137)
- [ ] 03.34 Reachable Nodes (CSES 2138)
- [ ] 03.35 Reachability Queries (CSES 2143)
