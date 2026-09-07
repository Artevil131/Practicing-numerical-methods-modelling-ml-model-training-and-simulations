# Chapter 04 — Problems

How to work this ladder:

1. Re-type the chapter library (`example.cpp`) from memory first: `extgcd`, `power`, inverses,
   linear sieve + spf, `phi_mu_sieve`, Miller–Rabin, Pollard rho, BSGS. Compile with
   `-Wall -Wextra`, run the asserts. Only then open the problems.
2. One file per problem: `learning/competitive_programming/04_number_theory/solutions/<source>_<id>.cpp`
   (e.g. `solutions/cses_2417.cpp`, `solutions/cf_1034A.cpp`).
3. Stress-test against a brute force (n ≤ 200, random values, 1000 iterations) before every
   submission that involves Möbius, CRT, or any `mod` arithmetic.
4. Time yourself: ★1–2 in ≤ 15 min, ★3 in ≤ 40 min, ★4–5 up to 90 min; then hint, then approach,
   then upsolve and re-solve a week later.

Combinatorial CSES Mathematics tasks (Binomial Coefficients, Creating Strings II, Distributing
Apples, Christmas Party, Bracket Sequences, Counting Necklaces/Grids, probability and game tasks)
are in chapter 05 and chapter 16.

Difficulty: ★1 (warm-up) … ★5 (Div1 C). CF rating estimates in brackets.

---

## A. Modular arithmetic and exponentiation

### 04.1  Exponentiation  ·  CSES 1095  ·  ★1 (900)
https://cses.fi/problemset/task/1095
**Technique:** binary exponentiation.
<details><summary>Hint</summary>n = 2·10^5 queries, exponents up to 10^9: O(log b) each.</details>
<details><summary>Approach sketch</summary>`power(a, b, 10^9+7)` per query. Check `0^0 = 1` matches the statement (it does here). Fast IO matters at 2·10^5 lines.</details>

### 04.2  Exponentiation II  ·  CSES 1712  ·  ★2 (1400)
https://cses.fi/problemset/task/1712
**Technique:** Fermat's little theorem on the exponent.
<details><summary>Hint</summary>a^(b^c) mod p: the exponent b^c is astronomically large, but a^(p−1) ≡ 1.</details>
<details><summary>Approach sketch</summary>Compute e = b^c mod (p−1), then a^e mod p. Valid because p ∤ a for a < p, a ≥ 1; handle a = 0 separately (0^0 = 1, 0^positive = 0 — b^c is 0 only when b = 0 and c > 0). Two `power` calls per query.</details>

### 04.3  Subarray Divisibility  ·  CSES 1662  ·  ★2 (1400)
https://cses.fi/problemset/task/1662
**Technique:** prefix sums mod n with normalization of negatives.
<details><summary>Hint</summary>A subarray sum is divisible by n iff two prefix sums are congruent mod n.</details>
<details><summary>Approach sketch</summary>Count prefix residues `((P % n) + n) % n` (values are negative too). Answer Σ C(cnt_r, 2) over residues r, including the empty prefix. `long long` throughout — pairs count reaches 2·10^10.</details>

### 04.4  Trailing Zeros  ·  CSES 1618  ·  ★1 (1000)
https://cses.fi/problemset/task/1618
**Technique:** Legendre's formula.
<details><summary>Hint</summary>Zeros of n! = exponent of 5 in n! (there are always more 2s).</details>
<details><summary>Approach sketch</summary>Σ_{k≥1} ⌊n / 5^k⌋. Loop while 5^k ≤ n in `long long`. Same formula gives the exponent of any prime p in n!, used for binomials mod composite numbers.</details>

### 04.5  Fibonacci Numbers  ·  CSES 1722  ·  ★2 (1500)
https://cses.fi/problemset/task/1722
**Technique:** fast doubling or 2×2 matrix power.
<details><summary>Hint</summary>n ≤ 10^18: O(log n) via F(2k), F(2k+1) identities.</details>
<details><summary>Approach sketch</summary>Recursive `fib(n)` returning (F(n), F(n+1)) mod p with F(2k) = F(k)(2F(k+1) − F(k)), F(2k+1) = F(k)² + F(k+1)². Normalize `2F(k+1) − F(k)` before multiplying. Alternatively raise [[1,1],[1,0]] to the n-th power.</details>

### 04.6  Remainders Game  ·  Codeforces 687B  ·  ★2 (1600)
https://codeforces.com/problemset/problem/687/B
**Technique:** CRT uniqueness ⇔ lcm divisibility.
<details><summary>Hint</summary>Knowing x mod c_i for all i determines x mod lcm(c_i) and nothing more.</details>
<details><summary>Approach sketch</summary>x mod k is determined iff k | lcm(c_1..c_n). Compute the lcm but cap it: `lcm = lcm(lcm, c_i) mod k`-style is wrong; instead keep `g = lcm(g, c_i)` reduced by `gcd(·, k)` — i.e. maintain `g = lcm(g, gcd(c_i, k))`, which stays ≤ k. Answer Yes iff g == k.</details>

---

## B. Divisors, sieves, factorization

### 04.7  Counting Divisors  ·  CSES 1713  ·  ★2 (1300)
https://cses.fi/problemset/task/1713
**Technique:** divisor-count sieve or spf factorization.
<details><summary>Hint</summary>n ≤ 10^5 queries, values ≤ 10^6: precompute d(x) for all x.</details>
<details><summary>Approach sketch</summary>Either the harmonic double loop `for d: for m = d, 2d, …: cnt[m]++` in O(V log V), or linear sieve with spf and d(x) = ∏(e_i + 1) per query in O(log x). Both fit easily; know both.</details>

### 04.8  Common Divisors  ·  CSES 1081  ·  ★2 (1500)
https://cses.fi/problemset/task/1081
**Technique:** count multiples per candidate gcd.
<details><summary>Hint</summary>The largest d such that at least two array elements are multiples of d.</details>
<details><summary>Approach sketch</summary>Frequency array over values ≤ 10^6. For d from V down to 1, count `Σ freq[d], freq[2d], …`; first d with count ≥ 2 is the answer. Harmonic O(V log V). Note duplicates: two equal values x give gcd x.</details>

### 04.9  Sum of Divisors  ·  CSES 1082  ·  ★3 (1800)
https://cses.fi/problemset/task/1082
**Technique:** Σ_{d≤n} d·⌊n/d⌋ with floor-division blocks, O(√n).
<details><summary>Hint</summary>Σ_{k=1}^{n} σ(k) = Σ_{d=1}^{n} d · ⌊n/d⌋. ⌊n/d⌋ takes only O(√n) distinct values.</details>
<details><summary>Approach sketch</summary>Iterate blocks [l, r] with constant q = n/l, r = n/q. Block contributes q · (l + r)(r − l + 1)/2. n ≤ 10^12: the sum of d over a block is ~10^24 — reduce each factor mod p and multiply by inv(2) mod p instead of dividing. Around 2·10^6 blocks.</details>

### 04.10  Divisor Analysis  ·  CSES 2182  ·  ★3 (1900)
https://cses.fi/problemset/task/2182
**Technique:** multiplicative formulas from a given factorization with huge exponents.
<details><summary>Hint</summary>Number, sum, and product of divisors of ∏ p_i^{e_i} with e_i up to 10^9, mod 10^9+7.</details>
<details><summary>Approach sketch</summary>Count ∏(e_i+1) mod p. Sum ∏ (p_i^{e_i+1} − 1) · inv(p_i − 1) with fast power. Product = n^{d/2}: compute as ∏ p_i^{e_i · d / 2} with the exponent reduced mod (p − 1); "divide d by 2" is exact only if some e_j + 1 is even — if d is odd every e_i is even, so halve e_i instead. Keep the exponent arithmetic mod (p−1) and check it against a brute force on small inputs.</details>

### 04.11  Prime Multiples  ·  CSES 2185  ·  ★3 (1800)
https://cses.fi/problemset/task/2185
**Technique:** inclusion–exclusion over 2^k prime subsets.
<details><summary>Hint</summary>k ≤ 20 primes: for each non-empty subset the count of multiples of their product is ⌊n / prod⌋, with sign (−1)^{|S|+1}.</details>
<details><summary>Approach sketch</summary>Enumerate masks; compute the product incrementally from `mask ^ lowbit` and stop (prune) when it exceeds n — products overflow `long long` otherwise. Use `__int128` or check `prod > n / p` before multiplying. Sum ⌊n/prod⌋ with alternating signs.</details>

### 04.12  Next Prime  ·  CSES 3396  ·  ★3 (1800)
https://cses.fi/problemset/task/3396
**Technique:** deterministic Miller–Rabin.
<details><summary>Hint</summary>Values up to 10^18; the gap to the next prime is a few hundred at most.</details>
<details><summary>Approach sketch</summary>Loop candidates n+1, n+2, … and test each with the 12-base Miller–Rabin using `__int128` multiplication. Skip even candidates; optionally pre-filter with small primes. Each test costs ~30 µs; total under 50 ms for the whole input.</details>

### 04.13  Enlarge GCD  ·  Codeforces 1034A  ·  ★3 (1900)
https://codeforces.com/problemset/problem/1034/A
**Technique:** divide out the gcd, count elements per prime with spf.
<details><summary>Hint</summary>After dividing by g the gcd is 1; the new gcd must be some prime p dividing as many elements as possible.</details>
<details><summary>Approach sketch</summary>Divide all a_i by g. Sieve spf up to 1.5·10^7 (values bound). For each element, add 1 to `cnt[p]` for each distinct prime p of a_i (spf factorization). Answer n − max cnt[p]; −1 if all elements became 1. Memory: spf as `int` over 1.5·10^7 = 60 MB — fits, or use a `bitset` sieve and trial division by primes.</details>

### 04.14  Short Task  ·  Codeforces 1512G  ·  ★3 (1800)
https://codeforces.com/problemset/problem/1512/G
**Technique:** σ(n) for all n ≤ 10^7 via sieve; inverse lookup.
<details><summary>Hint</summary>Precompute σ for all n up to 10^7 and record the smallest n for each σ value ≤ 10^7.</details>
<details><summary>Approach sketch</summary>Harmonic double loop is 1.6·10^8 additions — fine in ~0.5 s; or linear sieve with the "power of spf" trick for σ in O(n). Then `ans[σ(n)] = min(ans[σ(n)], n)`. Queries in O(1).</details>

### 04.15  Multiplication Table  ·  CSES 2422  ·  ★3 (1900)
https://cses.fi/problemset/task/2422
**Technique:** binary search on the answer + Σ⌊x/i⌋ counting.
<details><summary>Hint</summary>Median of the n×n table: count entries ≤ x in O(n) with Σ_i min(n, ⌊x/i⌋).</details>
<details><summary>Approach sketch</summary>Binary search the smallest x with count(x) ≥ (n²+1)/2. Count is Σ_{i=1}^{n} min(n, x / i). n ≤ 10^6, ~40 iterations → 4·10^7 divisions. The floor-block trick makes count O(√x) if needed.</details>

---

## C. Möbius, totient, gcd counting

### 04.16  Counting Coprime Pairs  ·  CSES 2417  ·  ★3 (1900)
https://cses.fi/problemset/task/2417
**Technique:** Möbius inversion over multiples.
<details><summary>Hint</summary>Pairs with gcd exactly 1 = Σ_d μ(d) · C(c_d, 2), c_d = #elements divisible by d.</details>
<details><summary>Approach sketch</summary>Sieve μ to 10^6. Frequency array; c_d by harmonic sum over multiples. Accumulate μ(d) · c_d(c_d−1)/2 in `long long` (intermediate terms are large and signed). O(V log V).</details>

### 04.17  GCD Subsets  ·  CSES 3161  ·  ★4 (2000)
https://cses.fi/problemset/task/3161
**Technique:** count subsets with gcd exactly g for all g, top-down subtraction.
<details><summary>Hint</summary>Subsets whose gcd is a multiple of g: 2^{c_g} − 1. Exact gcd g: subtract the counts of exact 2g, 3g, ….</details>
<details><summary>Approach sketch</summary>h[g] = 2^{c_g} − 1 mod p. For g from V down to 1: f[g] = h[g] − Σ_{k≥2} f[kg]. Harmonic O(V log V). Equivalent to Σ_k μ(k) h[kg]. Precompute powers of two up to n.</details>

### 04.18  Counting LCM Arrays  ·  CSES 3169  ·  ★4 (2200)
https://cses.fi/problemset/task/3169
**Technique:** multiplicativity over prime powers of the target lcm.
<details><summary>Hint</summary>The number of length-n arrays with lcm exactly m factors over the primes of m; for p^e the exponents are in [0, e] with at least one equal to e.</details>
<details><summary>Approach sketch</summary>Factor m (trial division to 10^6 or Pollard). For each p^e: ways = (e+1)^n − e^n (all exponent choices minus those that never reach e). Multiply over primes, mod p. Check the exact statement (whether elements are bounded by m or arbitrary divisors of m) against the sample before trusting the formula.</details>

### 04.19  Coprime Subsequences  ·  Codeforces 803F  ·  ★3 (1900)
https://codeforces.com/problemset/problem/803/F
**Technique:** Möbius over multiples with 2^{c_d} − 1.
<details><summary>Hint</summary>Non-empty subsequences with gcd 1 = Σ_d μ(d)(2^{c_d} − 1).</details>
<details><summary>Approach sketch</summary>Same skeleton as Counting Coprime Pairs with the pair count replaced by the subset count. Precompute powers of two; mod 10^9+7, keep intermediate signs correct by adding p when subtracting.</details>

### 04.20  Orac and LCM  ·  Codeforces 1349A  ·  ★3 (1700)
https://codeforces.com/problemset/problem/1349/A
**Technique:** gcd of pairwise lcms via per-prime second-smallest exponent.
<details><summary>Hint</summary>For each prime p, the exponent in the answer is the second smallest exponent of p among all a_i.</details>
<details><summary>Approach sketch</summary>Sieve spf to 2·10^5, factor each element, collect exponents per prime (elements not containing p have exponent 0 — so if fewer than n−1 elements contain p the answer's exponent is 0). Multiply p^{second min}. O(n log V).</details>

### 04.21  Row GCD  ·  Codeforces 1458A  ·  ★2 (1600)
https://codeforces.com/problemset/problem/1458/A
**Technique:** gcd(a+b_j, …) via differences.
<details><summary>Hint</summary>gcd(a_1 + x, a_2 + x, …) = gcd(a_1 + x, a_2 − a_1, a_3 − a_1, …).</details>
<details><summary>Approach sketch</summary>Precompute G = gcd of |a_i − a_1| over i ≥ 2. Each query b_j answers gcd(a_1 + b_j, G). Values up to 10^18: `long long` and `std::gcd`; handle G = 0 (n = 1).</details>

### 04.22  Make It One  ·  Codeforces 1043F  ·  ★4 (2500)
https://codeforces.com/problemset/problem/1043/F
**Technique:** Möbius-counted subsets of fixed size.
<details><summary>Hint</summary>The answer is at most 7 (product of the first 7 primes exceeds 3·10^5). For each size k count k-subsets with gcd 1 via Σ_d μ(d) C(c_d, k).</details>
<details><summary>Approach sketch</summary>c_d as usual. For k = 1..7 compute Σ_d μ(d) C(c_d, k) mod p; the first k with a non-zero count is the answer (checking non-zero mod p is a heuristic but accepted). Binomials via factorial tables (chapter 05).</details>

### 04.23  Divan and Kostomuksha (easy)  ·  Codeforces 1614D1  ·  ★4 (2300)
https://codeforces.com/problemset/problem/1614/D1
**Technique:** DP over divisors with c_d counts.
<details><summary>Hint</summary>Order elements so that the prefix gcds are large as long as possible: dp[g] = best sum with the "current" gcd equal to g, transitions to g/p.</details>
<details><summary>Approach sketch</summary>c_d = #elements divisible by d (harmonic). dp[d] = max over primes p of dp[d·p] + (c_d − c_{d·p}) · d, processed from large d down; answer dp[1]. Iterate only prime multiples (spf sieve) to keep O(V log log V).</details>

---

## D. Extended Euclid, CRT, discrete log

### 04.24  Power Products  ·  Codeforces 1225D  ·  ★3 (1900)
https://codeforces.com/problemset/problem/1225/D
**Technique:** normalize exponents mod k, match complements.
<details><summary>Hint</summary>a_i · a_j is a k-th power iff the exponent vectors of a_i and a_j sum to 0 mod k.</details>
<details><summary>Approach sketch</summary>Factor each a_i with spf (values ≤ 10^5), reduce each exponent mod k, drop zero exponents; the "signature" is the list of (p, e mod k). Its complement is (p, k − e). Count pairs with a hash map keyed by the signature (as a vector or as the reconstructed integer, capped when it exceeds 10^5 since then no complement can exist).</details>

### 04.25  Lunar New Year and a Recursive Sequence  ·  Codeforces 1106F  ·  ★5 (2400)
https://codeforces.com/problemset/problem/1106/F
**Technique:** matrix power on exponents + BSGS + linear congruence.
<details><summary>Hint</summary>Write f_i = 3^{e_i} (3 is a primitive root of 998244353). Exponents follow a linear recurrence mod p−1; f_n = 3^{c · e_k} where c comes from the matrix power.</details>
<details><summary>Approach sketch</summary>Matrix exponentiation mod (p−1) gives the coefficient c of e_k in e_n. BSGS finds e_n from f_n. Then solve c · e_k ≡ e_n (mod p−1) with extgcd (may have no solution → −1). Output 3^{e_k}.</details>

### 04.26  Josephus Problem I  ·  CSES 2162  ·  ★2 (1300)
https://cses.fi/problemset/task/2162
**Technique:** k = 2 removal order by simulation / structure.
<details><summary>Hint</summary>Every second person is removed; a queue or a vector rebuilt each round works in O(n).</details>
<details><summary>Approach sketch</summary>Simulate with a `queue`: pop front and push back (skipped), pop front and output (removed), until empty. O(n) total since each round halves the circle.</details>

### 04.27  Josephus Problem II  ·  CSES 2163  ·  ★3 (1700)
https://cses.fi/problemset/task/2163
**Technique:** order statistics: find and delete the k-th remaining element.
<details><summary>Hint</summary>Position of the next removed person = (current + k) mod (remaining). You need "k-th alive index" and deletion in O(log n).</details>
<details><summary>Approach sketch</summary>Fenwick tree with prefix counts and binary-lifting `find_kth`, or `pb_ds` order-statistics tree (GCC only). Each step: idx = (idx + k) mod remaining, output the idx-th alive, delete it. O(n log n).</details>

### 04.28  Josephus Queries  ·  CSES 2164  ·  ★4 (2100)
https://cses.fi/problemset/task/2164
**Technique:** O(log n) recursion on the removal index.
<details><summary>Hint</summary>With n people the first ⌊n/2⌋ removals are 2, 4, 6, …. The rest is the same problem on the survivors, whose positions map back linearly with a parity twist.</details>
<details><summary>Approach sketch</summary>Define who(n, j): if j ≤ n/2 return 2j. Else let j' = j − n/2 and n' = ⌈n/2⌉ (survivors); p = who(n', j'). Map back: if n is even survivors are odd positions, return 2p − 1; if n is odd the circle restarts after position n (removed... check: survivors are 1, 3, …, n with the round continuing from 3), so return 2p + 1 with wrap (p = n' maps to 1). Verify on n ≤ 20 against a simulation.</details>

---

Practice more: Codeforces problemset, tag `number theory`, rating 1400–1900 (sieves, gcd, mod
arithmetic); tag `number theory` + `combinatorics`, rating 1900–2400 (Möbius, multiplicative
functions); tag `math` with `chinese remainder theorem` in the statement, rating 1800–2200. AtCoder
ABC problems E/F tagged number theory are a good source of clean floor-sum and gcd exercises.

## Progress

- [ ] 04.1  Exponentiation (CSES 1095)
- [ ] 04.2  Exponentiation II (CSES 1712)
- [ ] 04.3  Subarray Divisibility (CSES 1662)
- [ ] 04.4  Trailing Zeros (CSES 1618)
- [ ] 04.5  Fibonacci Numbers (CSES 1722)
- [ ] 04.6  Remainders Game (CF 687B)
- [ ] 04.7  Counting Divisors (CSES 1713)
- [ ] 04.8  Common Divisors (CSES 1081)
- [ ] 04.9  Sum of Divisors (CSES 1082)
- [ ] 04.10 Divisor Analysis (CSES 2182)
- [ ] 04.11 Prime Multiples (CSES 2185)
- [ ] 04.12 Next Prime (CSES 3396)
- [ ] 04.13 Enlarge GCD (CF 1034A)
- [ ] 04.14 Short Task (CF 1512G)
- [ ] 04.15 Multiplication Table (CSES 2422)
- [ ] 04.16 Counting Coprime Pairs (CSES 2417)
- [ ] 04.17 GCD Subsets (CSES 3161)
- [ ] 04.18 Counting LCM Arrays (CSES 3169)
- [ ] 04.19 Coprime Subsequences (CF 803F)
- [ ] 04.20 Orac and LCM (CF 1349A)
- [ ] 04.21 Row GCD (CF 1458A)
- [ ] 04.22 Make It One (CF 1043F)
- [ ] 04.23 Divan and Kostomuksha (easy) (CF 1614D1)
- [ ] 04.24 Power Products (CF 1225D)
- [ ] 04.25 Lunar New Year and a Recursive Sequence (CF 1106F)
- [ ] 04.26 Josephus Problem I (CSES 2162)
- [ ] 04.27 Josephus Problem II (CSES 2163)
- [ ] 04.28 Josephus Queries (CSES 2164)
