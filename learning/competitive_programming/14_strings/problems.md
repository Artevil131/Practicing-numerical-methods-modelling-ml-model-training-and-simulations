# Chapter 14 — Problems

How to work this ladder:

1. Before each section, re-type the corresponding structure from `example.cpp` from memory (hashing,
   Z, prefix function, trie, Aho–Corasick, suffix array + Kasai, suffix automaton, Manacher,
   eertree, Duval), compile it against the asserts in your copy, diff against the file.
2. One file per problem: `competitive_programming/14_strings/solutions/<source>_<id>.cpp`
   (e.g. `cses_2105.cpp`, `cf_235C.cpp`).
3. Stress-test every structure-based solution against an O(n²) or O(n³) brute force on random
   strings over a 2-letter alphabet (small alphabets maximise repeats and expose off-by-ones) and
   on `aaaa…a` (maximum LCPs, maximum clones).
4. Time yourself: ★1–2 → 15 min, ★3 → 30 min, ★4 → 60 min, ★5 → 90 min; then hint, sketch, upsolve.
5. All 21 tasks of the CSES String Algorithms section are here; several admit two or three
   different structures — solve the hard ones twice (e.g. Distinct Substrings by SA and by SAM).

Difficulty: ★1 = warm-up, ★3 = Div2 D, ★5 = Div1 D. CF ratings are estimates.

---

## A. Hashing, Z, KMP

### 14.1  String Matching  ·  CSES 1753  ·  ★1
https://cses.fi/problemset/task/1753
**Technique:** KMP / Z / hashing — count occurrences of one pattern.
<details><summary>Hint</summary>Solve it three times: Z of `p#t`, online KMP, rolling hash windows. All O(n + m).</details>
<details><summary>Approach sketch</summary>With Z: positions `i > |p|` where `z[i] == |p|`. With KMP: count `k == m` events. With hashing: compare `get(i, i+m)` with `h(p)`. Confirm all three agree on random tests — this is your unit test for the whole section.</details>

### 14.2  Finding Borders  ·  CSES 1732  ·  ★1
https://cses.fi/problemset/task/1732
**Technique:** prefix function chain or Z.
<details><summary>Hint</summary>Borders are `pi[n−1], pi[pi[n−1]−1], …`; or `k` is a border iff `z[n−k] == k`.</details>
<details><summary>Approach sketch</summary>Output in increasing order — collect and reverse if you use the `pi` chain.</details>

### 14.3  Finding Periods  ·  CSES 1733  ·  ★1
https://cses.fi/problemset/task/1733
**Technique:** periods ↔ borders.
<details><summary>Hint</summary>`q` is a period iff `n − q` is a border or `q = n`; with Z, iff `z[q] == n − q`.</details>
<details><summary>Approach sketch</summary>Loop `q = 1..n`, test `z[q] + q == n` (define `z[n] = 0`). O(n).</details>

### 14.4  String Functions  ·  CSES 2107  ·  ★1
https://cses.fi/problemset/task/2107
**Technique:** Z-function and prefix function themselves.
<details><summary>Hint</summary>Print both arrays; check the exact definition of `z[0]` the statement uses.</details>
<details><summary>Approach sketch</summary>Direct implementation; n = 10⁶ so use fast output (`printf` or a manual buffer).</details>

### 14.5  Password  ·  Codeforces 126B  ·  ★2 (CF ~1700)
https://codeforces.com/contest/126/problem/B
**Technique:** borders that also occur in the middle.
<details><summary>Hint</summary>A border of length `k` works iff some `z[i] ≥ k` for `0 < i < n − k` (or `k` is itself a border of a longer border).</details>
<details><summary>Approach sketch</summary>Compute Z; let `M[i]` = max of `z` over positions in `(0, i]`. Try borders from longest to shortest: border `k` at position `n−k` is valid iff `M[n−k−1] ≥ k`.</details>

### 14.6  Prefixes and Suffixes  ·  Codeforces 432D  ·  ★3 (CF ~1800)
https://codeforces.com/contest/432/problem/D
**Technique:** count occurrences of every prefix via Z (or the prefix-function counting trick).
<details><summary>Hint</summary>`cnt[L]` = number of `i` with `z[i] ≥ L` — a suffix sum over a histogram of `z`.</details>
<details><summary>Approach sketch</summary>Histogram `z` values, suffix-sum it; for each border `k` (from the `pi` chain or `z`), print `k` and `cnt[k]` (+1 for the whole string if `z[0]` is not counted).</details>

### 14.7  MUH and Cube Walls  ·  Codeforces 471D  ·  ★3 (CF ~1800)
https://codeforces.com/contest/471/problem/D
**Technique:** KMP on difference arrays.
<details><summary>Hint</summary>Shape equality ⇔ equality of consecutive differences.</details>
<details><summary>Approach sketch</summary>Replace both arrays by `a[i+1] − a[i]`; count occurrences with KMP over integers (the alphabet is large, so use the prefix-function version, not an automaton table). Handle `m = 1` (answer `n`).</details>

### 14.8  Compress Words  ·  Codeforces 1200E  ·  ★3 (CF ~2000)
https://codeforces.com/contest/1200/problem/E
**Technique:** longest overlap of a suffix of the result with a prefix of the next word — Z or hashing.
<details><summary>Hint</summary>Only the last `|w|` characters of the result matter; run Z on `w + '#' + tail`.</details>
<details><summary>Approach sketch</summary>For each word, take `tail = last min(|w|, |res|) chars`, Z of `w#tail`, the overlap is the largest `z[i]` with `i + z[i] == |w#tail|`. Total O(Σ|wᵢ|). Append with `+=`, never `res = res + …`.</details>

### 14.9  Watto and Mechanism  ·  Codeforces 514C  ·  ★3 (CF ~2000)
https://codeforces.com/contest/514/problem/C
**Technique:** hashing with a single-character substitution.
<details><summary>Hint</summary>Changing position `i` from `a` to `b` changes the hash by `(b − a)·B^(n−1−i)`.</details>
<details><summary>Approach sketch</summary>Store hashes of all dictionary words (grouped by length or in one set). For each query and each position and each of the two other letters, compute the modified hash in O(1) and probe. O(total length × 2). Use 2⁶¹−1 (a 10⁹+7 single hash is attackable here and known to fail).</details>

### 14.10  Palindrome Queries  ·  CSES 2420  ·  ★4
https://cses.fi/problemset/task/2420
**Technique:** hashing with point updates (Fenwick / segment tree over hash contributions).
<details><summary>Hint</summary>Maintain `Σ s[i]·B^i` and `Σ s[i]·B^(n−1−i)` in two Fenwick trees; a substring is a palindrome iff its forward hash equals its reversed hash after aligning powers.</details>
<details><summary>Approach sketch</summary>Forward hash of `[l, r]` = `(F(r) − F(l−1))·B^(−l)` — avoid inverses by comparing `F·B^(n−1−r)` with `R·B^l` instead (cross-multiply the alignment). Point update = add the difference at one index in both trees. O(log n) per operation.</details>

### 14.11  Palindrome Degree  ·  Codeforces 7D  ·  ★3 (CF ~2100)
https://codeforces.com/contest/7/problem/D
**Technique:** hashing (forward and backward) + a 1D DP over prefixes.
<details><summary>Hint</summary>`deg[i] = deg[i/2 − 1] + 1` if prefix `i` is a palindrome, else 0.</details>
<details><summary>Approach sketch</summary>Build the forward hash and the hash of the reversed string incrementally as prefixes grow; palindrome test in O(1). Sum the degrees. n = 5·10⁶ — no `vector<pair>`, tight `ull` arithmetic.</details>

---

## B. Trie and Aho–Corasick

### 14.12  Word Combinations  ·  CSES 1731  ·  ★2
https://cses.fi/problemset/task/1731
**Technique:** trie walk + DP.
<details><summary>Hint</summary>`dp[j+1] += dp[i]·cnt[node]` while walking `s[i..]` down the trie.</details>
<details><summary>Approach sketch</summary>Lesson §4. Total dictionary length ≤ 10⁶ → `array<int,26>` nodes are ~100 MB; if memory is tight, store children as `int[26]` only via a flat `vector<int>` of size nodes·26 (same thing, less overhead) or note that the walk depth is bounded by the longest word.</details>

### 14.13  Finding Patterns  ·  CSES 2102  ·  ★3
https://cses.fi/problemset/task/2102
**Technique:** Aho–Corasick, presence of each pattern.
<details><summary>Hint</summary>Visit counts propagated up suffix links; pattern present iff its terminal node has count > 0.</details>
<details><summary>Approach sketch</summary>Lesson §5 `count_all`. Duplicate patterns map to the same node — answer per input index via the node id.</details>

### 14.14  Counting Patterns  ·  CSES 2103  ·  ★3
https://cses.fi/problemset/task/2103
**Technique:** Aho–Corasick counts (or suffix automaton with `cnt`).
<details><summary>Hint</summary>Same as 14.13, print the counts.</details>
<details><summary>Approach sketch</summary>Solve it twice: AC (patterns first, one text pass) and SAM of the text (walk each pattern, read `cnt`). Same asymptotics; compare running times.</details>

### 14.15  Pattern Positions  ·  CSES 2104  ·  ★3
https://cses.fi/problemset/task/2104
**Technique:** Aho–Corasick with `min` propagation (first occurrence).
<details><summary>Hint</summary>Record the smallest text position visiting each node; propagate `min` up the links in reverse BFS order; first occurrence of `p` = `minpos[node] − |p| + 1`.</details>
<details><summary>Approach sketch</summary>Identical structure to counting, with `min` instead of `+`. Print −1 for never-visited nodes (use a large sentinel).</details>

### 14.16  Required Substring  ·  CSES 1112  ·  ★4
https://cses.fi/problemset/task/1112
**Technique:** DP over the KMP automaton (chapter 13 §18).
<details><summary>Hint</summary>Count strings avoiding `p`, subtract from 26ⁿ.</details>
<details><summary>Approach sketch</summary>`aut[k][c]` from the prefix function; `dp[i][k]` for `k < m`; O(n·m·26) = 2.6·10⁷. Generalise mentally to several forbidden strings — that is the AC version (`count_avoiding` in `example.cpp`).</details>

### 14.17  Frequency of String  ·  Codeforces 963D  ·  ★5 (CF ~2500)
https://codeforces.com/contest/963/problem/D
**Technique:** Aho–Corasick over all queries + the "distinct patterns are few" bound.
<details><summary>Hint</summary>Deduplicate the queries. Patterns of length `L` occur at most `n − L + 1` times each, and there are at most `n/L` distinct patterns of length `L` with total length ≤ n — summing over lengths gives O(n √n) total occurrences, so collecting every occurrence of every distinct pattern is affordable.</details>
<details><summary>Approach sketch</summary>Build AC on the distinct query strings, collect occurrence positions per pattern (total O(n√n) by the length argument), and for each query take the minimum window covering `k` consecutive occurrences (`pos[i+k−1] − pos[i] + L`).</details>

---

## C. Suffix array and suffix automaton

### 14.18  Distinct Substrings  ·  CSES 2105  ·  ★3
https://cses.fi/problemset/task/2105
**Technique:** suffix array + LCP, or suffix automaton.
<details><summary>Hint</summary>`n(n+1)/2 − Σ lcp`, or `Σ len[v] − len[link[v]]`.</details>
<details><summary>Approach sketch</summary>Do both; they must agree exactly. Answer up to ~5·10⁹ → `long long`.</details>

### 14.19  Repeating Substring  ·  CSES 2106  ·  ★3
https://cses.fi/problemset/task/2106
**Technique:** maximum of the LCP array (or hashing + binary search on the length).
<details><summary>Hint</summary>The longest substring occurring twice is the LCP of two SA-adjacent suffixes.</details>
<details><summary>Approach sketch</summary>`argmax lcp` gives the suffix; print its first `lcp` characters, or −1 if all zero. Hashing alternative: binary search `L`, check duplicate window hashes with a sorted vector.</details>

### 14.20  Substring Order I  ·  CSES 2108  ·  ★4
https://cses.fi/problemset/task/2108
**Technique:** k-th distinct substring — SAM path counting or SA walk.
<details><summary>Hint</summary>SAM: `paths[v]` = 1 + Σ children (over decreasing `len`); descend choosing the smallest letter whose subtree holds ≥ k. SA: suffix `sa[i]` contributes `n − sa[i] − lcp[i−1]` new substrings.</details>
<details><summary>Approach sketch</summary>With SA, find the first `i` where the cumulative count reaches `k`; the answer is the prefix of suffix `sa[i]` of length `lcp[i−1] + (k − prefix count)`. `k ≤ 10¹⁸` fits `long long`.</details>

### 14.21  Substring Order II  ·  CSES 2109  ·  ★5
https://cses.fi/problemset/task/2109
**Technique:** k-th substring with multiplicity — SAM with `cnt` weights.
<details><summary>Hint</summary>Same descent as 14.20, but the weight of a state is `cnt[v]` (occurrences) and the subtree total is `paths[v] = cnt[v] + Σ paths[child]`.</details>
<details><summary>Approach sketch</summary>Propagate `cnt` up the link tree first, then `paths` over children in decreasing `len`. Descend: at state `v`, for letters in order, if `k ≤ paths[u]` go to `u` and subtract `cnt[u]` (the string ending exactly here is counted `cnt[u]` times); stop when `k ≤ 0`. Totals can reach n²/2 ≈ 5·10⁹ → `long long`.</details>

### 14.22  Substring Distribution  ·  CSES 2110  ·  ★4
https://cses.fi/problemset/task/2110
**Technique:** distinct substrings of each length — SA + difference array, or SAM lengths.
<details><summary>Hint</summary>Suffix `sa[i]` introduces exactly one new distinct substring for each length in `(lcp[i−1], n − sa[i]]`.</details>
<details><summary>Approach sketch</summary>Difference array `d[lcp+1] += 1`, `d[n−sa[i]+1] −= 1`; prefix sum gives the answer per length. With a SAM, each state contributes 1 to lengths `(len[link], len]` — same difference array.</details>

### 14.23  Inverse Suffix Array  ·  CSES 3225  ·  ★4
https://cses.fi/problemset/task/3225
**Technique:** greedy reconstruction from `sa` using `rank`.
<details><summary>Hint</summary>Adjacent suffixes in SA order start with the same letter iff the rest of the smaller one still ranks before the rest of the larger one: `rank[sa[i−1]+1] < rank[sa[i]+1]`.</details>
<details><summary>Approach sketch</summary>Assign `'a'` to `sa[0]`; walk `i = 1..n−1`, keep the letter if the condition holds, else increment; if the letter would exceed `'z'`, print −1. Treat the empty suffix (`sa[i] + 1 == n`) as rank −1. Verify by rebuilding the SA of the output.</details>

### 14.24  Cyclical Quest  ·  Codeforces 235C  ·  ★4 (CF ~2700)
https://codeforces.com/contest/235/problem/C
**Technique:** suffix automaton with occurrence counts, walking a cyclic string.
<details><summary>Hint</summary>Walk `x + x` (minus the last character) through SAM(s) with the "shrink to `len[link]` on mismatch" rule, keeping the matched length ≤ |x|; each time the length reaches |x| you are at a state — count it once per distinct state.</details>
<details><summary>Approach sketch</summary>Per query O(|x|) plus resetting the visited marks; sum `cnt[state]` over distinct states hit. Total O(|s| + Σ|xᵢ|).</details>

### 14.25  Three strings  ·  Codeforces 452E  ·  ★5 (CF ~2800)
https://codeforces.com/contest/452/problem/E
**Technique:** generalised suffix automaton (or SA of the concatenation with LCP intervals).
<details><summary>Hint</summary>For each SAM state, know how many end positions come from each of the three strings; a state with all three positive contributes `cnt₁·cnt₂·cnt₃` to every length in `(len[link], len]`.</details>
<details><summary>Approach sketch</summary>Build one automaton over `s₁#s₂$s₃` (or a generalised SAM); propagate three counters up the links; difference array over lengths, mod 10⁹+7.</details>

### 14.26  Match & Catch  ·  Codeforces 427D  ·  ★4 (CF ~2000)
https://codeforces.com/contest/427/problem/D
**Technique:** shortest substring occurring exactly once in each of two strings.
<details><summary>Hint</summary>SAM of `s#t`: a state that has exactly one end position in `s` and exactly one in `t` yields a candidate of length `len[link] + 1`.</details>
<details><summary>Approach sketch</summary>Two counters per state as in 14.25; the minimum `len[link]+1` over states with both counters equal to 1. SA alternative: adjacent-in-SA suffixes from different strings with LCP-interval bookkeeping.</details>

### 14.27  Good Substrings  ·  Codeforces 271D  ·  ★3 (CF ~1900)
https://codeforces.com/contest/271/problem/D
**Technique:** count distinct substrings satisfying a constraint — hashing set or SAM/trie.
<details><summary>Hint</summary>n ≤ 1500: all O(n²) substrings fit; count "bad" letters with prefix sums, insert hashes of good substrings into a set (or a trie of all suffixes).</details>
<details><summary>Approach sketch</summary>For each start, extend while `bad ≤ k`, insert `get(l, r)` into an `unordered_set<ull>` (reserve 10⁶). 2⁶¹−1 avoids the collision risk that makes this problem famous for hacking single 32-bit hashes.</details>

### 14.28  String  ·  Codeforces 128B  ·  ★4 (CF ~2300)
https://codeforces.com/contest/128/problem/B
**Technique:** k-th substring with multiplicity, k ≤ 10⁵ — priority queue of suffix prefixes or SAM.
<details><summary>Hint</summary>Because `k ≤ 10⁵`, a heap over "(current prefix, suffix start)" popped `k` times works; or use 14.21's method directly.</details>
<details><summary>Approach sketch</summary>Heap: start with all single characters keyed by (char, position); pop the minimum, push its extension by one character. Compare strings by `(hash LCP, char)` or just store the substring since total popped length is bounded by `k`·depth. The SAM solution is O(n + k) after `cnt` propagation.</details>

---

## D. Palindromes and rotations

### 14.29  Longest Palindrome  ·  CSES 1111  ·  ★3
https://cses.fi/problemset/task/1111
**Technique:** Manacher.
<details><summary>Hint</summary>Max over `2·d1[i]−1` and `2·d2[i]`; remember the centre to print the string.</details>
<details><summary>Approach sketch</summary>Lesson §9. Cross-check with hashing (forward/backward hashes + binary search per centre, O(n log n)) — a second solution you should know.</details>

### 14.30  All Palindromes  ·  CSES 3138  ·  ★3
https://cses.fi/problemset/task/3138
**Technique:** Manacher, per-position maximum radius.
<details><summary>Hint</summary>The statement asks for the longest palindrome at each centre — `d1` and `d2` are exactly that.</details>
<details><summary>Approach sketch</summary>Print `2·d1[i]−1` for odd centres and `2·d2[i]` for even ones in the order the statement specifies. n = 10⁶ → fast output.</details>

### 14.31  Palisection  ·  Codeforces 17E  ·  ★5 (CF ~2900)
https://codeforces.com/contest/17/problem/E
**Technique:** Manacher + counting palindromes ending before / starting after each position.
<details><summary>Hint</summary>Count intersecting pairs = total pairs − non-intersecting pairs; non-intersecting = Σ over `i` of (palindromes ending at ≤ i) × (palindromes starting at i+1).</details>
<details><summary>Approach sketch</summary>From `d1`, `d2` build difference arrays of "number of palindromes starting at `i`" and "ending at `i`" (each centre contributes a range of starts/ends) — O(n) — then prefix sums, mod 51123987. n = 2·10⁶: arrays of `int`, no `vector<vector>`.</details>

### 14.32  Minimal Rotation  ·  CSES 1110  ·  ★3
https://cses.fi/problemset/task/1110
**Technique:** Duval on `s + s` (or Booth, or SA of `s + s`).
<details><summary>Hint</summary>The smallest rotation starts at the start of the Lyndon factor of `ss` that begins in `[0, n)` and reaches furthest.</details>
<details><summary>Approach sketch</summary>Lesson §11 `min_rotation`. Test on `aaaa`, `abab`, `ba`, and a random string against the O(n²) minimum over all rotations.</details>

### 14.33  Distinct Subsequences  ·  CSES 1149  ·  ★3
https://cses.fi/problemset/task/1149
**Technique:** counting DP (not a string structure): distinct subsequences modulo a prime.
<details><summary>Hint</summary>`dp[i] = 2·dp[i−1] − dp[last[c]−1]` where `last[c]` is the previous occurrence of `s[i]`.</details>
<details><summary>Approach sketch</summary>Doubling counts every subsequence twice when a repeated letter appears; subtracting the count just before its previous occurrence removes the duplicates. Exclude the empty subsequence if the statement does. Watch negative values mod p.</details>

### 14.34  String Transform  ·  CSES 1113  ·  ★4
https://cses.fi/problemset/task/1113
**Technique:** inverse Burrows–Wheeler transform (a sorting + permutation-following exercise).
<details><summary>Hint</summary>The BWT is the last column of the sorted rotations; the first column is the sorted characters. The `i`-th occurrence of a letter in the last column is the `i`-th occurrence in the first column.</details>
<details><summary>Approach sketch</summary>Stable-sort the character indices to build `next[i]`; start from the row whose last character is the sentinel `#`, follow `next` n times collecting first-column characters. O(n log n) or O(n) with counting sort.</details>

### 14.35  Martian Strings  ·  Codeforces 149E  ·  ★4 (CF ~2300)
https://codeforces.com/contest/149/problem/E
**Technique:** Z / KMP forward and backward.
<details><summary>Hint</summary>For a pattern `p`, find for each split point the earliest end of a prefix of `p` in `s` and the latest start of the matching suffix of `p`.</details>
<details><summary>Approach sketch</summary>Run Z of `p#s` to get, for each prefix length `L`, the earliest position where a prefix of length ≥ L ends (`min` over positions, then prefix-min); run Z of `rev(p)#rev(s)` for suffixes; a pattern is beautiful iff some `L` in `[1, |p|−1]` has earliest prefix end < latest suffix start. O(|s|·m) for m patterns with Σ|p| ≤ 10⁵.</details>

---

## Practice more

- Codeforces problemset, tag `strings`, rating 1800–2400 — two per week, upsolve each.
- Codeforces problemset, tag `string suffix structures`, rating 2100–2500 (SAM/SA drills).
- Codeforces problemset, tag `hashing`, rating 1800–2200.
- AtCoder Beginner Contest E/F tasks tagged strings — many are hashing/Z one-liners at
  Div2 C–D level, good for speed.
- CSES Advanced Techniques: 2073 Substring Reversals and 2074 Reversals and Sums are treap
  problems on strings (chapter on balanced trees), not string algorithms — skip them here.

## Progress

- [ ] 14.1 String Matching (CSES 1753)
- [ ] 14.2 Finding Borders (CSES 1732)
- [ ] 14.3 Finding Periods (CSES 1733)
- [ ] 14.4 String Functions (CSES 2107)
- [ ] 14.5 Password (CF 126B)
- [ ] 14.6 Prefixes and Suffixes (CF 432D)
- [ ] 14.7 MUH and Cube Walls (CF 471D)
- [ ] 14.8 Compress Words (CF 1200E)
- [ ] 14.9 Watto and Mechanism (CF 514C)
- [ ] 14.10 Palindrome Queries (CSES 2420)
- [ ] 14.11 Palindrome Degree (CF 7D)
- [ ] 14.12 Word Combinations (CSES 1731)
- [ ] 14.13 Finding Patterns (CSES 2102)
- [ ] 14.14 Counting Patterns (CSES 2103)
- [ ] 14.15 Pattern Positions (CSES 2104)
- [ ] 14.16 Required Substring (CSES 1112)
- [ ] 14.17 Frequency of String (CF 963D)
- [ ] 14.18 Distinct Substrings (CSES 2105)
- [ ] 14.19 Repeating Substring (CSES 2106)
- [ ] 14.20 Substring Order I (CSES 2108)
- [ ] 14.21 Substring Order II (CSES 2109)
- [ ] 14.22 Substring Distribution (CSES 2110)
- [ ] 14.23 Inverse Suffix Array (CSES 3225)
- [ ] 14.24 Cyclical Quest (CF 235C)
- [ ] 14.25 Three strings (CF 452E)
- [ ] 14.26 Match & Catch (CF 427D)
- [ ] 14.27 Good Substrings (CF 271D)
- [ ] 14.28 String (CF 128B)
- [ ] 14.29 Longest Palindrome (CSES 1111)
- [ ] 14.30 All Palindromes (CSES 3138)
- [ ] 14.31 Palisection (CF 17E)
- [ ] 14.32 Minimal Rotation (CSES 1110)
- [ ] 14.33 Distinct Subsequences (CSES 1149)
- [ ] 14.34 String Transform (CSES 1113)
- [ ] 14.35 Martian Strings (CF 149E)
