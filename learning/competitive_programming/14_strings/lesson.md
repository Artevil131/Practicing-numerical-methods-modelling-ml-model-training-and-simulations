# Chapter 14 — String algorithms

Prerequisites: `../../algorithms_learning/01_arrays_and_hashing/lesson.md` (hash maps, prefix
sums), `../../algorithms_learning/03_binary_search/lesson.md` (`lower_bound` discipline), chapter 13
of this course (DP over automata, §18). The `std::string` API itself is
`../../cpp_learning/04_std_vector_string_and_containers/lesson.md`. Reference library:
`example.cpp` — every structure here is implemented there and cross-checked against brute force.

## What you'll be able to do after this chapter

- Compare any two substrings of a 10⁶-character string in O(1) with a rolling hash you can defend
  against anti-hash tests, and compute LCPs, count distinct substrings, and find repeated
  substrings with hashing + binary search.
- Implement the Z-function and the prefix function from memory in under three minutes each, with
  their proofs of linearity, and use them for pattern matching, borders and periods.
- Build a trie and an Aho–Corasick automaton with suffix links and full transitions; count
  occurrences of 10⁵ patterns in a 10⁶ text; run a DP over the automaton.
- Build a suffix array in O(n log n) with counting sort and the LCP array with Kasai; use them
  for distinct substrings, longest repeated substring, longest common substring and pattern search.
- Build a suffix automaton in O(n) and read off distinct substrings, occurrence counts, longest
  common substring, and the k-th smallest substring.
- Find all palindromic substrings in O(n) with Manacher; know what the eertree adds.
- Compute the Lyndon factorization and the minimal rotation in O(n) with Duval.
- Pick the right structure from the statement in under a minute (comparison table at the end).

## Where this shows up in contests

| Signal | Structure | Placement |
|---|---|---|
| "how many times does p occur in s" (one pattern) | KMP / Z / hashing | Div2 B–C |
| many patterns, one text; "which patterns occur"; "count occurrences of each" | Aho–Corasick | Div2 E, Div1 C |
| "number of distinct substrings", "k-th substring", "longest repeated substring" | suffix array / automaton | Div2 E, Div1 B–C, BOI |
| "longest common substring of two/many strings" | suffix automaton (or SA of the concatenation) | Div1 C |
| "is s[a..b] equal to s[c..d]" queries, "compare substrings", "LCP of two suffixes" | hashing (+ binary search) or SA + LCP RMQ | Div2 D–E |
| palindromes: longest, count, per-position | Manacher / eertree | Div2 D–E |
| "lexicographically smallest rotation", Lyndon words | Duval | Div2 D |
| "count strings of length n that avoid/contain …" | DP over KMP / Aho–Corasick automaton | Div2 E, Div1 B |
| periods, borders, "smallest k such that s is k-periodic" | prefix function | Div2 C–D |

IOI itself uses strings sparingly (hashing and tries appear; suffix structures are rare), but
BOI/CEOI/JOI and Codeforces use all of it. Strings are also where "knows the library" pays off
most directly: a correct suffix automaton typed from memory in 4 minutes is worth a whole problem.

---

## 0. Notation and conventions

Strings are 0-indexed. `s[l, r)` is the half-open substring of length `r − l`. `n = |s|`. Σ is
the alphabet size (26 for lowercase). A **border** of `s` is a proper prefix that is also a suffix.
A **period** `q` means `s[i] = s[i+q]` for all valid `i`; `q` is a period ⟺ `n − q` is a border
(or `q = n`). A **substring** is contiguous; a **subsequence** is not (Distinct Subsequences,
CSES 1149, is a DP, not a string-structure problem).

---

## 1. Polynomial hashing

`h(s) = Σ s[i]·B^(n−1−i) mod p` — read the string as a number in base `B`. With prefix hashes
`pre[i] = h(s[0, i))` we get `h(s[l, r)) = pre[r] − pre[l]·B^(r−l) mod p` in O(1). Two substrings
are (almost surely) equal iff their hashes are equal.

**Which modulus.** `p = 2⁶¹ − 1` (Mersenne prime): the multiplication `a·b mod p` needs no
division — write `c = a·b` as a 128-bit value, then `(c & p) + (c >> 61)` is congruent to `c` and
one more fold brings it below `p`. Collision probability for two fixed distinct strings of length
≤ n is ≤ n/p ≈ 10⁶/2⁶¹ ≈ 4·10⁻¹³ per comparison; a 10⁶-comparison program has failure
probability ~10⁻⁷. Alternatives: two 32-bit moduli (10⁹+7 and 998244353 — a "double hash") with
the same collision bound but two multiplications and a `pair`. Never use `unsigned long long`
natural overflow (mod 2⁶⁴): the Thue–Morse string breaks *every* base ("anti-hash test", known
since Codeforces 2012).

**Which base.** Random, chosen at runtime, from `[256, p−2]`. A fixed base can be attacked by a
prepared test (birthday attack on a public base: ~2³⁰·⁵ strings for a 61-bit hash is still
infeasible, but for 10⁹+7 it is trivial); a random base means no test prepared in advance works.
The base must exceed the maximum character value so that distinct single characters hash
differently.

```cpp
struct Hash61 {
    static constexpr ull P = (1ULL << 61) - 1;
    static ull mulmod(ull a, ull b) {
        __uint128_t c = (__uint128_t)a * b;
        ull r = (ull)(c & P) + (ull)(c >> 61);
        r = (r & P) + (r >> 61);
        return r >= P ? r - P : r;
    }
    static ull base() { static ull B = uniform_int_distribution<ull>(256, P - 2)(rng); return B; }
    vector<ull> pre, pw;
    Hash61(const string& s) : pre(s.size() + 1, 0), pw(s.size() + 1, 1) {
        ull B = base();
        for (size_t i = 0; i < s.size(); i++) {
            pre[i + 1] = (mulmod(pre[i], B) + (unsigned char)s[i]) % P;
            pw[i + 1] = mulmod(pw[i], B);
        }
    }
    ull get(int l, int r) const { ull v = pre[r] + P - mulmod(pre[l], pw[r - l]); return v >= P ? v - P : v; }
};
```

**Applications.**

- *Compare substrings / LCP of two suffixes*: `lcp(i, j)` = largest `k` with `get(i, i+k) == get(j,
  j+k)`; monotone in `k`, so binary search: O(log n). Lexicographic comparison of two substrings =
  LCP then one character. Sorting all suffixes with this comparator gives a suffix array in
  O(n log² n) — simple, and often enough.
- *Count distinct substrings of each length*: hash every substring of that length into a set,
  O(n) per length, O(n²) total — fine for n ≤ 5000; for larger n use a suffix array/automaton.
- *Longest repeated substring*: binary search the length `L`; for a given `L`, insert all
  `n−L+1` window hashes into a set and check for a duplicate. O(n log n) with `unordered_set`
  (reserve it) or sort+adjacent. Same shape: longest common substring of two strings
  (put hashes of `s`-windows into a set, probe with `t`-windows).
- *Palindrome checks in O(1)*: hash `s` and reversed `s`; `s[l, r)` is a palindrome iff its hash
  equals the hash of the corresponding window of the reverse. Palindrome Queries (CSES 2420) adds
  point updates → keep the two hashes in Fenwick trees (each position contributes
  `s[i]·B^i`, point update = add a difference).
- *Hashing of sequences other than characters*: arrays of integers, tree serialisations
  (tree isomorphism), paths — same formula.

Pitfalls: `s[i]` must be mapped to a nonzero value (`'a' → 1`, not `0`; otherwise `"a"` and
`"aa"` collide when the leading character is 0 — in the code above characters are already ≥ 97,
fine); combining hashes of two strings `h(a·b) = h(a)·B^|b| + h(b)`; storing hashes in
`unordered_set` needs a custom hash for pairs (or use 2⁶¹−1 to keep a single `ull`).

---

## 2. Z-function

`z[i]` = length of the longest common prefix of `s` and `s[i..)` (`z[0] = n` by convention).

**Algorithm.** Maintain the rightmost segment `[l, r)` with `s[l, r) = s[0, r−l)` found so far
(the "Z-box"). For `i < r`: `s[i, r)` equals `s[i−l, r−l)`, so `z[i] ≥ min(z[i−l], r−i)`. Start
from that and extend by comparing characters. Update the box if `i + z[i] > r`.

**Proof of O(n).** Each successful comparison inside the while-loop increases `i + z[i]`, and
after that `r` is set to `i + z[i]` — so every successful comparison moves `r` strictly right;
`r ≤ n`. Each `i` has at most one failed comparison. Total ≤ 2n. The lower bound `min(z[i−l],
r−i)` is exact when `z[i−l] < r−i` (a mismatch inside the box is mirrored), and when it is not,
we cannot know more than `r−i` without comparing — hence the `min`. ∎

```cpp
vector<int> z_function(const string& s) {
    int n = s.size(); vector<int> z(n, 0);
    for (int i = 1, l = 0, r = 0; i < n; i++) {
        if (i < r) z[i] = min(r - i, z[i - l]);
        while (i + z[i] < n && s[z[i]] == s[i + z[i]]) z[i]++;
        if (i + z[i] > r) { l = i; r = i + z[i]; }
    }
    if (n) z[0] = n;
    return z;
}
```

Trace `s = aabxaab`:

```
i:   0 1 2 3 4 5 6
s:   a a b x a a b
z:   7 1 0 0 3 1 0
i=1: box empty, compare a=a, a!=b -> z=1, box [1,2)
i=4: i>=r, compare aab vs aab then end -> z=3, box [4,7)
i=5: inside box, z[5-4]=z[1]=1, r-i=2 -> start 1; s[1]=a vs s[6]=b mismatch -> z=1
```

**Applications.** *Pattern matching*: Z of `p + '#' + t` (separator not in the alphabet):
positions with `z = |p|` are occurrences. *Periods* (Finding Periods, CSES 1733): `q` is a
period iff `z[q] = n − q` (or `q = n`). *Borders* (Finding Borders, CSES 1732): `k` is a border
iff `z[n−k] = k`. *Compressing a string*: smallest period that divides `n`. *Number of
occurrences of each prefix*: bucket-count `z` values and take suffix sums (String Functions,
CSES 2107 asks for Z and prefix function themselves).

---

## 3. Prefix function and KMP

`pi[i]` = length of the longest proper border of `s[0..i]`.

**Algorithm.** Knowing `pi[i−1] = k`: if `s[i] == s[k]`, `pi[i] = k+1`. Otherwise the next
candidate is the longest border of `s[0..k)` — that is `pi[k−1]` — because any border of
`s[0..i]` shorter than `k+1` is (border of a border) + `s[i]`. Follow the chain until a match or
`k = 0`.

**Proof of O(n).** `pi[i] ≤ pi[i−1] + 1`, so Σ increases ≤ n; every iteration of the while-loop
decreases `k` by ≥ 1 and `k` never goes negative → total decreases ≤ total increases ≤ n. ∎

```cpp
vector<int> prefix_function(const string& s) {
    int n = s.size(); vector<int> pi(n, 0);
    for (int i = 1; i < n; i++) {
        int k = pi[i - 1];
        while (k > 0 && s[i] != s[k]) k = pi[k - 1];
        if (s[i] == s[k]) k++;
        pi[i] = k;
    }
    return pi;
}
```

Trace `s = abacaba`: `pi = 0 0 1 0 1 2 3`. At `i = 6` (`a`), `k = pi[5] = 2`, `s[6] = a ≠ s[2] = a`?
equal → `pi[6] = 3`. At `i = 3` (`c`), `k = 1`, `c ≠ s[1] = b` → `k = pi[0] = 0`, `c ≠ s[0]` → 0.

**KMP matching** (Pattern Positions, CSES 2104; String Matching, CSES 1753). Run the same
recurrence over `p + '#' + t`, or online: keep `k` = current matched length; on `t[i]`, drop `k`
through `pi` until `p[k] == t[i]` (treat `k == m` as a mismatch first), increment, and report an
occurrence when `k == m`. Overlapping occurrences are found (`aaa` in `aaaaa` → 3).

```cpp
vector<int> kmp_find(const string& t, const string& p) {
    vector<int> pi = prefix_function(p), res; int m = p.size();
    for (int i = 0, k = 0; i < (int)t.size(); i++) {
        while (k > 0 && (k == m || t[i] != p[k])) k = pi[k - 1];
        if (t[i] == p[k]) k++;
        if (k == m) res.push_back(i - m + 1);
    }
    return res;
}
```

**Borders and periods.** All borders of `s`: `pi[n−1], pi[pi[n−1]−1], …` (each is a border of the
previous). Smallest period = `n − pi[n−1]`. All periods = `n − border` for each border, plus `n`.
`s` is a power `w^k` (k ≥ 2) iff the smallest period `q` divides `n` and `q < n`.

**Automaton view.** `aut[k][c]` = matched length after reading `c` in state `k`:
`aut[k][c] = k+1` if `p[k] == c`, else `aut[pi[k−1]][c]` (and `0` for `k = 0`). Build in
O(mΣ) by increasing `k` (the fallback state is smaller). This turns matching into one table lookup
per character, and — the important use — makes "count strings that contain/avoid `p`" a DP over
`m+1` states (chapter 13 §18; Required Substring, CSES 1112).

**Counting occurrences of each prefix in s** (a classic): `cnt[pi[i]]++` for each `i`, then for
`k` from `n` down: `cnt[pi[k−1]] += cnt[k]`, then `cnt[k]++` for the prefix itself.

Pitfalls: `while (k > 0 && ...)` — the `k > 0` guard first; using `p[k]` when `k == m` (out of
range) in the online matcher — hence the `k == m` check; `'#'` separator must be outside the
alphabet; the prefix function of `p + '#' + t` needs O(|p| + |t|) memory, the online version O(|p|).

---

## 4. Trie

A rooted tree of characters; each node is a prefix of some inserted word. Array-based, not
`map`-based, when Σ is small: `nxt[node][c]`, `−1` for no child. Memory: (total length) × Σ ×
4 B — for 10⁶ characters and Σ = 26, 104 MB: too much. Fixes: `Σ = 26` with `int` → use
`unordered_map`/sorted `vector` children only when Σ is large, or a binary trie (Σ = 2) for
XOR problems (bitwise chapter).

```cpp
struct Trie {
    vector<array<int,26>> nxt; vector<int> cnt;
    Trie() { new_node(); }
    int new_node() { array<int,26> a; a.fill(-1); nxt.push_back(a); cnt.push_back(0); return nxt.size() - 1; }
    void insert(const string& w) {
        int v = 0;
        for (char ch : w) {
            int c = ch - 'a';
            if (nxt[v][c] == -1) { int u = new_node(); nxt[v][c] = u; }   // temporary on purpose
            v = nxt[v][c];
        }
        cnt[v]++;
    }
};
```

The temporary `u` matters: `nxt[v][c] = new_node();` evaluates `nxt[v][c]` (a reference into the
vector) and `new_node()` (which may reallocate the vector) in an order that only C++17 pins down
(right operand first). Writing the temporary works under every standard and every compiler.

**Word Combinations** (CSES 1731): number of ways to write `s` as a concatenation of dictionary
words. `dp[i]` = ways for `s[0, i)`; from `i`, walk down the trie along `s[i..]`, and at each node
that ends `c` words add `dp[i]·c` to `dp[j+1]`. O(n · maxlen) ≤ 5000·5000 — fine; the trie
makes the dictionary lookup implicit. Compare with hashing (hash each dictionary word, probe
`hash(s[i, j))` for every `j` — same complexity, more constant).

Other uses: longest common prefix queries on a set, autocomplete counts (`cnt` of words in a
subtree = number of inserted words with that prefix), XOR maximisation with binary tries.

---

## 5. Aho–Corasick

A trie of the patterns plus **suffix links**: `link[v]` = the deepest trie node that is a proper
suffix of `v`'s string. With links, one pass over the text finds all occurrences of all patterns —
KMP for many patterns. Build with BFS (by depth): for edge `v →c u`, `link[u] = go(link[v], c)`,
where `go` follows links until a `c`-edge exists (`go(root, c) = root` if none). Filling
`go[v][c]` for *all* `c` gives the full automaton: O(nodes·Σ) memory, O(1) per text character.
Without the full table, the walk `v = link[v]` until an edge exists is amortised O(1) per
character (same argument as KMP) but needs `map` children or a sorted lookup.

**Output.** `term[v]` marks a pattern ending exactly at `v`. A text position ending at node `v`
matches every pattern on the link chain of `v` — potentially many. Two standard tricks:

- *Exit links* (`exitl[v]` = nearest node on the chain that is terminal): enumerate matches per
  position in O(#matches); total O(|t| + matches).
- *Counting only*: `cnt[v]++` for each text position, then push counts up the links in **reverse
  BFS order** (`cnt[link[v]] += cnt[v]`): `cnt[term node of p]` is the number of occurrences of
  `p`. O(|t| + nodes). This is Counting Patterns (CSES 2103) and Finding Patterns (CSES 2102 — any
  count > 0); Pattern Positions (CSES 2104, first occurrence) records the minimum position
  instead of a count and propagates with `min`.

```cpp
struct AhoCorasick {
    vector<array<int,26>> go; vector<int> link, term, order;
    int insert(const string& w, int id) { /* trie insert; term[v] = id; return v */ }
    void build() {
        queue<int> q;
        for (int c = 0; c < 26; c++) { if (go[0][c] == -1) go[0][c] = 0; else { link[go[0][c]] = 0; q.push(go[0][c]); } }
        while (!q.empty()) {
            int v = q.front(); q.pop(); order.push_back(v);
            for (int c = 0; c < 26; c++) {
                int u = go[v][c];
                if (u == -1) go[v][c] = go[link[v]][c];      // missing edge: same as from the suffix
                else { link[u] = go[link[v]][c]; q.push(u); }
            }
        }
    }
};
```

Correctness of `link[u] = go[link[v]][c]`: the longest proper suffix of `vc` that is a trie node is
(some suffix of `v` that is a node) + `c`, and the longest such is obtained from the longest
suffix of `v`, i.e. `link[v]`, falling back further only if it has no `c`-edge — which is exactly
what the completed `go` of the shallower node `link[v]` computes (BFS order guarantees it is
complete).

**DP over the automaton** (count strings of length `n` avoiding all patterns): a node is
*forbidden* if it or any link ancestor is terminal (propagate `bad[v] = term[v] || bad[link[v]]`
in BFS order). `dp[i][v]` = number of length-`i` strings ending in state `v`, transitions over
Σ, skipping forbidden nodes. O(n · nodes · Σ).

Memory: nodes ≤ total pattern length + 1; `go` as `array<int,26>` → 104 B per node; 10⁶ nodes =
104 MB — for such inputs store `go` as `int[26]` only where needed or reduce Σ. Pitfalls: the
root's missing edges must point to the root; propagate counts in *reverse* BFS order (deep first);
duplicate patterns share a node — keep a list of ids or count multiplicity.

---

## 6. Suffix array and LCP array

`sa[i]` = start index of the `i`-th smallest suffix. `lcp[i]` = LCP of suffixes `sa[i]`, `sa[i+1]`.

**O(n log² n) — sort with doubling.** Round `k` sorts suffixes by their first `2^k` characters
using ranks from round `k−1`: the key of suffix `i` is the pair `(rank[i], rank[i + 2^(k−1)])`
(second component −1 past the end). `std::sort` with that comparator, then recompute ranks. log n
rounds × O(n log n). 20 lines, n = 2·10⁵ in ~150 ms. Acceptable for most problems.

**O(n log n) — counting sort on cyclic shifts.** Append a sentinel smaller than every character;
then the sorted *cyclic shifts* of `s + '$'` are exactly the sorted suffixes (the sentinel stops
comparisons). In round `k`, the shift starting at `i` has key `(c[i], c[i + 2^k mod n])`. The
list `p` sorted by the second component is just `p` from the previous round with each entry shifted
by `−2^k` (a shift sorted by its first half is the "second half" of the shift `2^k` earlier).
Then a stable counting sort by the first component. Equivalence classes `c` are recomputed by
comparing adjacent pairs. Stop when all classes are distinct.

```cpp
vector<int> suffix_array(const string& str) {
    string s = str + '\x01'; int n = s.size(), classes = 256;
    vector<int> p(n), c(n), cnt(max(classes, n), 0), pn(n), cn(n);
    for (char ch : s) cnt[(unsigned char)ch]++;
    for (int i = 1; i < classes; i++) cnt[i] += cnt[i-1];
    for (int i = n-1; i >= 0; i--) p[--cnt[(unsigned char)s[i]]] = i;
    c[p[0]] = 0; classes = 1;
    for (int i = 1; i < n; i++) { if (s[p[i]] != s[p[i-1]]) classes++; c[p[i]] = classes - 1; }
    for (int k = 1; k < n; k <<= 1) {
        for (int i = 0; i < n; i++) { pn[i] = p[i] - k; if (pn[i] < 0) pn[i] += n; }   // sorted by 2nd half
        fill(cnt.begin(), cnt.begin() + classes, 0);
        for (int i = 0; i < n; i++) cnt[c[pn[i]]]++;
        for (int i = 1; i < classes; i++) cnt[i] += cnt[i-1];
        for (int i = n-1; i >= 0; i--) p[--cnt[c[pn[i]]]] = pn[i];                    // stable by 1st half
        cn[p[0]] = 0; classes = 1;
        for (int i = 1; i < n; i++) {
            pair<int,int> cur{c[p[i]], c[(p[i]+k) % n]}, prv{c[p[i-1]], c[(p[i-1]+k) % n]};
            if (cur != prv) classes++;
            cn[p[i]] = classes - 1;
        }
        c.swap(cn);
        if (classes == n) break;
    }
    p.erase(p.begin());   // drop the sentinel's suffix
    return p;
}
```

Trace `s = banana`, with sentinel `$`:

```
suffixes sorted:      sa    lcp
$                     6      -    (dropped)
a                     5      1
ana                   3      3
anana                 1      0
banana                0      0
na                    4      2
nana                  2
distinct substrings = 6*7/2 - (1+3+0+0+2) = 21 - 6 = 15
```

**Kasai's LCP in O(n).** Process suffixes in text order `i = 0, 1, …`. Suppose suffix `i` has
LCP `h ≥ 1` with its SA-neighbour `j`. Dropping the first character of both gives suffixes `i+1`
and `j+1` with LCP `h−1`, and `j+1` still ranks on the same side of `i+1`; the SA-neighbour of
`i+1` lies between them, so its LCP with `i+1` is ≥ `h−1`. Hence start the comparison for `i+1`
at `h−1`. The counter `k` decreases by ≤ 1 per step and increases ≤ n in total → O(n). ∎
(Code in `example.cpp`: `lcp[rank[i]]` is the LCP between `sa[rank[i]]` and `sa[rank[i]+1]`.)

**Applications.**

| Task | Method | Complexity |
|---|---|---|
| Distinct Substrings (CSES 2105) | `n(n+1)/2 − Σ lcp` | O(n) after SA |
| Repeating Substring (CSES 2106) — longest substring occurring ≥ 2 times | `max lcp`, its position `sa[argmax]` | O(n) |
| count occurrences of `p` (Counting Patterns, per query) | binary search the SA range of suffixes starting with `p` | O(|p| log n) |
| Substring Order I (CSES 2108) — k-th distinct substring | walk `sa`: suffix `sa[i]` introduces `n − sa[i] − lcp[i−1]` new substrings (its prefixes longer than `lcp[i−1]`), in lexicographic order; stop when the running total reaches k | O(n) |
| Substring Order II (CSES 2109) — k-th substring with multiplicity | same walk, but a prefix of length `L` of suffix `sa[i]` is counted once per suffix sharing it: the multiplicity is the size of the LCP-interval around `i` with `lcp ≥ L` (stack over the LCP array, or a SAM with `cnt`) | O(n) |
| Substring Distribution (CSES 2110) — number of distinct substrings of each length | suffix `sa[i]` adds one distinct substring of each length in `(lcp[i−1], n − sa[i]]` → difference array | O(n) |
| Inverse Suffix Array (CSES 3225) — reconstruct a string from `sa` | assign letters greedily: suffix `sa[i]` gets the same letter as `sa[i−1]` iff the suffix following `sa[i−1]` in text ranks before the one following `sa[i]` (check with `rank`) | O(n) |
| LCP of any two suffixes | RMQ (sparse table) over `lcp` between their ranks | O(1) per query |
| longest common substring of `s`, `t` | SA of `s + '#' + t`, max `lcp` between adjacent suffixes from different strings | O(n log n) |
| longest common substring of k strings | SA of the concatenation + sliding window over ranks covering all k sources, min-LCP in window | O(n log n) |

Pitfalls: the sentinel must be smaller than every character (`\x01` if the input has no control
characters); `cnt` must be sized `max(256, n)`; with `int` indices `(p[i]+k) % n` is fine; forgetting
`p.erase(p.begin())` shifts every answer.

---

## 7. Suffix automaton

The minimal DFA accepting all suffixes of `s`. Equivalent view: states are classes of substrings
with the same **endpos** set (positions where they end); within a class the strings are suffixes
of one another with consecutive lengths `(len[link[v]], len[v]]`. `link[v]` is the state of the
longest suffix of `v`'s strings with a *different* (strictly larger) endpos set. States ≤ 2n−1,
transitions ≤ 3n−4.

**Construction (online, add one character).** Create `cur` with `len = len[last] + 1`. Walk
`p = last, link[last], …` adding transitions `p →c cur` while `p` has no `c`-edge. If we reach
the root without an edge, `link[cur] = root`. Otherwise `q = go[p][c]`: if `len[q] == len[p] + 1`,
`link[cur] = q` (q's strings all end at the new position too); else *clone* `q` into a state with
`len = len[p] + 1`, same transitions and link, redirect the `c`-edges from `p`'s chain that pointed
to `q` onto the clone, and set `link[q] = link[cur] = clone`. `last = cur`.

**Amortisation sketch.** The first loop adds transitions; each iteration steps to a strictly
shorter link, and the total number of transitions ever added is ≤ 3n. The second loop (redirecting
edges to the clone) can be charged to the decrease of a potential: the depth of `last` in the link
tree grows by at most 2 per character (cur plus a clone) and each redirected edge corresponds to a
step that decreases the link-depth of a state; total O(n). Full proof in cp-algorithms ("Suffix
Automaton", section on linear complexity).

```cpp
struct SuffixAutomaton {
    struct State { int len, link; array<int,26> nxt; ll cnt; };
    vector<State> st; int last = 0;
    SuffixAutomaton() { State r{0, -1, {}, 0}; r.nxt.fill(-1); st.push_back(r); }
    void extend(int c) {
        int cur = st.size();
        State ns{st[last].len + 1, -1, {}, 1}; ns.nxt.fill(-1); st.push_back(ns);
        int p = last;
        while (p != -1 && st[p].nxt[c] == -1) { st[p].nxt[c] = cur; p = st[p].link; }
        if (p == -1) st[cur].link = 0;
        else {
            int q = st[p].nxt[c];
            if (st[p].len + 1 == st[q].len) st[cur].link = q;
            else {
                int clone = st.size();
                State cl = st[q]; cl.len = st[p].len + 1; cl.cnt = 0; st.push_back(cl);
                while (p != -1 && st[p].nxt[c] == q) { st[p].nxt[c] = clone; p = st[p].link; }
                st[q].link = st[cur].link = clone;
            }
        }
        last = cur;
    }
};
```

Note `State cl = st[q]; st.push_back(cl);` — never `st.push_back(st[q])`: `push_back` may
reallocate and the reference `st[q]` dangles (a classic UB that works on small tests).

Trace of the link tree and lengths for `s = aba`:

```
after 'a':  0 -a-> 1(len1, link 0)
after 'b':  1 -b-> 2(len2), 0 -b-> 2, link[2] = 0
after 'a':  cur=3(len3); p=2 has no 'a': 2 -a-> 3; p=0 has 'a' -> q=1, len[0]+1 == len[1] -> link[3]=1
states: 0(root)  1{a}  2{ab, b}  3{aba, ba}
distinct substrings = sum over v of len[v]-len[link[v]] = (1-0)+(2-0)+(3-1) = 5 = |{a,b,ab,ba,aba}|
link tree: 0 -> 1 -> 3,  0 -> 2      (the suffix tree of the reversed string "aba")
```

Always sanity-check the formula on a 3-letter string like this before trusting it on 10⁶.

**Applications.**

| Task | Method |
|---|---|
| distinct substrings | `Σ_v (len[v] − len[link[v]])`, or count paths from the root |
| occurrences of `p` | walk `p`; `cnt[state]` where `cnt` = 1 for non-clone states, propagated to links in decreasing `len` (String Matching, Counting Patterns — one text, many patterns, online) |
| first occurrence / all occurrences | `firstpos` stored at creation (clone inherits), all via the inverse link tree |
| longest common substring of `s`, `t` | walk `t` through SAM(s) with current length `l`: on a missing edge follow links (`l = len[link]`), on an edge `l++`; `max l` |
| k-th smallest distinct substring (Substring Order I) | `paths[v]` = 1 + Σ paths of children (in decreasing `len`); descend choosing the first letter whose subtree count ≥ k |
| k-th with multiplicity (Substring Order II) | same, weights `cnt[v]` instead of 1 |
| smallest cyclic shift | SAM of `s+s`, walk `n` steps taking the smallest edge each time |
| number of distinct substrings of each length | `len` histogram of `(len[link[v]], len[v]]` intervals via difference array |

Suffix automaton vs suffix array: SAM is online, O(n) build, natural for "walk a query string";
SA gives sorted order and LCP arrays, natural for "compare/rank suffixes" and lexicographic
enumeration. Memory: SAM with `array<int,26>` = 2n states × 112 B ≈ 22 MB for n = 10⁵, 220 MB for
10⁶ — switch to `map` children or Σ-specific arrays for 10⁶.

---

## 8. Suffix tree (mention)

The compressed trie of all suffixes: O(n) nodes, edges labelled by substrings (stored as index
pairs). Everything a suffix array + LCP gives, as a tree. Two ways to get one: Ukkonen's online
O(n) construction (~80 lines, easy to get wrong), or build it from SA + LCP in O(n) by a stack
walk (each LCP value is an internal node depth). In practice: use SA+LCP or the suffix automaton
(its link tree is the suffix tree of the *reversed* string — a fact that lets you answer suffix-tree
questions with a SAM).

---

## 9. Manacher

`d1[i]` = number of odd-length palindromes centred at `i` (their radii `1..d1[i]`), `d2[i]` =
number of even-length palindromes centred between `i−1` and `i`. Longest palindrome = max of
`2·d1[i]−1` and `2·d2[i]`; total palindromic substrings = Σ d1 + Σ d2.

**Algorithm.** Keep the rightmost palindrome `[l, r]` found so far. For `i ≤ r`, the mirror
position `l + r − i` has a known radius; the palindrome at `i` has radius ≥ `min(d[mirror], r − i +
1)` (it is a mirror image inside `[l, r]`, cut at the border). Extend by comparison. The same
"every successful comparison moves `r`" argument as Z gives O(n) — Manacher *is* the Z-function
idea applied to palindromic centres.

```cpp
vector<int> manacher_odd(const string& s) {
    int n = s.size(); vector<int> d1(n);
    for (int i = 0, l = 0, r = -1; i < n; i++) {
        int k = i > r ? 1 : min(d1[l + r - i], r - i + 1);
        while (0 <= i - k && i + k < n && s[i - k] == s[i + k]) k++;
        d1[i] = k--;
        if (i + k > r) { l = i - k; r = i + k; }
    }
    return d1;
}
vector<int> manacher_even(const string& s) {
    int n = s.size(); vector<int> d2(n);
    for (int i = 0, l = 0, r = -1; i < n; i++) {
        int k = i > r ? 0 : min(d2[l + r - i + 1], r - i + 1);
        while (0 <= i - k - 1 && i + k < n && s[i - k - 1] == s[i + k]) k++;
        d2[i] = k--;
        if (i + k > r) { l = i - k - 1; r = i + k; }
    }
    return d2;
}
```

Trace `s = abaab`: `d1 = 1 2 1 1 1`, `d2 = 0 0 0 2 0` (the even palindrome `baab` centred between
indices 2 and 3 has `d2[3] = 2`: `aa` and `baab`). Longest = 4.

The single-array variant inserts `#` between characters (`#a#b#a#a#b#`) and runs the odd
version once; the two-array version avoids the 2× memory and the index gymnastics.
**Longest Palindrome** (CSES 1111) and **All Palindromes** (CSES 3138 — for each centre, the
longest palindrome) are direct reads of `d1`/`d2`. "Is `s[l, r)` a palindrome?" per query: `d1`/`d2`
at the centre ≥ half-length — O(1) per query without hashing.

---

## 10. Palindromic tree / eertree (mention)

One node per distinct palindromic substring (there are ≤ n of them — each position adds at most
one new palindrome, the longest palindromic suffix ending there). Node `v` has `len[v]`, edges
`v →c cvc`, and a suffix link to the longest proper palindromic suffix. Adding a character:
follow links from the longest palindromic suffix of the previous prefix until `s[i − len − 1] ==
s[i]`. Amortised O(n). It gives, in one pass: the number of distinct palindromes, the number of
occurrences of each (propagate counts along links), the longest palindromic suffix at every
prefix, and online palindromic queries — things Manacher does not give directly (Manacher gives
per-centre radii, not the *set* of distinct palindromes). Implementation in `example.cpp` §9
(~40 lines). Use it when the question is about *distinct* palindromes or occurrence counts.

---

## 11. Lyndon factorization and minimal rotation (Duval)

A **Lyndon word** is strictly smaller than all its proper suffixes (equivalently, than all its
rotations). Every string has a unique factorization `s = w₁w₂…wₖ` into Lyndon words with
`w₁ ≥ w₂ ≥ … ≥ wₖ`. Duval computes it in O(n): scan with `i` (start of the current factor), `j`
(scanning position) and `k` (position inside the repeating candidate); while `s[k] ≤ s[j]`,
extend (`s[k] < s[j]` resets `k = i`, equality advances `k`); on `s[k] > s[j]`, the prefix `s[i, j)`
is a Lyndon word repeated `⌊(j−i)/(j−k)⌋` times plus a remainder — output the repetitions,
restart from the remainder.

```cpp
vector<string> duval(const string& s) {
    int n = s.size(), i = 0; vector<string> fac;
    while (i < n) {
        int j = i + 1, k = i;
        while (j < n && s[k] <= s[j]) { if (s[k] < s[j]) k = i; else k++; j++; }
        while (i <= k) { fac.push_back(s.substr(i, j - k)); i += j - k; }
    }
    return fac;
}
```

**Minimal Rotation** (CSES 1110): run Duval on `s + s`; the answer is the start of the Lyndon
factor that begins in `[0, n)` and extends furthest (a Lyndon word is its own smallest rotation,
and the smallest rotation of `s` is a Lyndon factor start of `ss`). Alternatives: the two-pointer
Booth/"minimum expression" algorithm (also O(n)), or a suffix array of `ss` (O(n log n)). Duval is
15 lines and hard to get wrong.

Trace `s = bcab` → `t = bcabbcab`: factors `bc`, `abbc`, `ab`; the factor `abbc` starts at 2 →
rotation `abbc`. ✓

---

## 12. String DP integration

The structures above become DP transition tables:

- **KMP automaton** — count/optimise strings with respect to one pattern (Required Substring,
  ch. 13 §18).
- **Aho–Corasick** — the same with many patterns; also "minimum cost to build a string from
  dictionary words" (`dp[i]` from the exit-link chain at position `i`: every pattern ending at
  `i` gives a transition from `dp[i − |p|]`). Word Combinations via AC instead of a trie walk.
- **Suffix automaton** — `dp` over states in `len` order: number of paths (k-th substring),
  longest path (longest substring with a property), `cnt` propagation.
- **Suffix array** — DP over adjacent ranks (Substring Distribution, Inverse Suffix Array).
- **Hashing** — `dp[i]` with transitions "if `s[i−L, i)` equals a dictionary word" checked in O(1).
- **Z / prefix function** — periodicity structure feeding a DP (String Transform, CSES 1113, is a
  Burrows–Wheeler inversion, a different beast: sort the characters, follow the permutation).

---

## 13. `std::string` performance pitfalls

| Pattern | Cost | Fix |
|---|---|---|
| `s = s + c` in a loop | O(n) per step → O(n²) | `s += c` or `s.push_back(c)` (amortised O(1)) |
| `s.substr(l, len)` inside an inner loop | allocates a new string each call | compare with `s.compare(l, len, t)` or with hashes/indices |
| passing `string` by value | copy | `const string&` |
| `string::find` in a loop for all occurrences | worst case O(n·m) (libstdc++ uses a naive search) | KMP / Z; in practice `find` is fast on random text |
| `cin >> s` for 10⁶ chars without `sync_with_stdio(false)` | ~3× slower | `ios::sync_with_stdio(false); cin.tie(nullptr);` |
| `getline` when the input has `\r\n` | stray `\r` | strip it |
| `unordered_set<string>` | hashes the whole string each op | store 64-bit hashes instead |
| `map<char,int>` children in a trie/SAM | ~50× slower than arrays for Σ = 26 | `array<int,26>` |
| comparing `string`s in a `sort` of suffixes | O(n) per comparison → O(n² log n) | hashing+LCP comparator or the SA algorithm |

`std::string` has SSO (short strings, ≤ 15 chars in libstdc++/libc++) — no heap allocation — so
`substr` of short pieces is cheap-ish; anything longer allocates.

---

## Comparison table

| Structure | Build | Memory | Best at | Weak at |
|---|---|---|---|---|
| Hashing | O(n) | O(n) | substring equality, LCP with binary search, ad-hoc problems, non-character sequences | probabilistic; lexicographic enumeration |
| Z-function | O(n) | O(n) | one-pattern matching, periods, borders | anything beyond prefix structure |
| Prefix function / KMP | O(n) | O(m) online | one pattern streaming, borders, automaton DP | many patterns |
| Trie | O(total) | O(total·Σ) | dictionary lookup, prefix counts, XOR | memory for large Σ |
| Aho–Corasick | O(total·Σ) | O(total·Σ) | many patterns in one text, automaton DP | changing pattern sets |
| Suffix array + LCP | O(n log n) | O(n) ints | sorted suffixes, k-th substring, LCS of many strings, distinct-by-length | online updates |
| Suffix automaton | O(n) | O(n·Σ) | online build, occurrence counting, LCS of two, walking queries | large Σ, huge n memory |
| Suffix tree | O(n) | O(n) | theory; same as SA+LCP | implementation length |
| Manacher | O(n) | O(n) | all palindromic radii | distinct palindromes |
| Eertree | O(n) | O(n·Σ) | distinct palindromes, counts | none for its niche |
| Duval | O(n) | O(1) extra | Lyndon factors, minimal rotation | — |

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| one pattern, positions/count | KMP or Z (or hashing) | O(n + m) |
| "borders" / "periods" of s | prefix function or Z | O(n) |
| is `s[a..b] == s[c..d]`, many queries | hashing | O(1) per query |
| LCP of two suffixes, many queries | hashing + binary search, or SA + LCP RMQ | O(log n) / O(1) |
| longest repeated substring | SA max lcp; or hashing + binary search | O(n log n) |
| number of distinct substrings | SA (`n(n+1)/2 − Σlcp`) or SAM | O(n log n) / O(n) |
| k-th substring (distinct / with multiplicity) | SAM path counts or SA walk | O(n) |
| distinct substrings of each length | SA difference array or SAM lengths | O(n) |
| longest common substring of two strings | SAM walk / SA of concatenation | O(n) / O(n log n) |
| many patterns, count each in text | Aho–Corasick + count propagation | O(text + total·Σ) |
| count strings avoiding/containing patterns | DP over KMP / AC automaton | O(n · states · Σ) |
| longest palindrome / palindromic radii | Manacher | O(n) |
| distinct palindromes / occurrences of each | eertree | O(n) |
| minimal rotation / Lyndon words | Duval | O(n) |
| reconstruct string from its suffix array | greedy with `rank` comparisons | O(n) |
| "split into dictionary words" count/min | trie walk DP or AC DP or hashing DP | O(n · maxlen) |

## Implementation checklist for contests

- [ ] Hashing: modulus 2⁶¹−1 (or double mod), random base ≥ 256, `__uint128_t` multiply,
      substring formula `pre[r] − pre[l]·pw[r−l]` with a `+P` before subtracting.
- [ ] KMP online matcher: `k == m` treated as mismatch before indexing `p[k]`.
- [ ] Z / prefix-function separator `#` not in the alphabet; array sized `|p| + 1 + |t|`.
- [ ] Trie/AC: `array<int,26>` children, `−1` sentinel, node 0 = root, root's missing edges → root.
- [ ] AC counts: propagate in **reverse BFS order**; DP forbidden flags in BFS order.
- [ ] SA: sentinel `\x01`, `cnt` sized `max(256, n)`, erase the sentinel entry, `classes == n` break.
- [ ] Kasai: `k = max(k−1, 0)` carry, skip the last-ranked suffix.
- [ ] SAM: copy the state before `push_back` when cloning; `cnt = 0` on clones; propagate `cnt`
      in decreasing `len`; states array sized `2n`.
- [ ] Manacher: two arrays `d1`, `d2`; even indices are "between `i−1` and `i`".
- [ ] `ios::sync_with_stdio(false); cin.tie(nullptr);` and `'\n'` not `endl` when printing 10⁵ lines.
- [ ] Tested on: empty/one-char string, all-same string (`aaaa…` — worst case for naive methods,
      max clones for SAM, max LCPs for SA), the two-letter Thue–Morse-like string, random.

## Further reading

- CPH ch. 26 (String algorithms: trie, hashing, Z-algorithm), ch. 26.3 for the anti-hash discussion.
- cp-algorithms.com: "String Hashing", "Z-function", "Prefix function. Knuth–Morris–Pratt",
  "Aho-Corasick algorithm", "Suffix Array", "Suffix Automaton", "Manacher's Algorithm", "Lyndon
  factorization", "Finding repetitions" (Main–Lorentz — a nice Z application).
- Gusfield, *Algorithms on Strings, Trees, and Sequences* — chapters on suffix trees and the
  linear-time constructions, if you want the proofs in full.
- Codeforces blog: "Anti-hash test" (the Thue–Morse attack on mod 2⁶⁴ hashing).
- Codeforces blog: "Palindromic tree" (the eertree paper's authors' write-up).
- Kärkkäinen–Sanders, "Simple linear work suffix array construction" (DC3/skew) — O(n) SA, for
  completeness; never needed in contests.

## You can move on when...

- You can type hashing, Z, prefix function, KMP automaton, trie, Aho–Corasick, suffix array +
  Kasai, suffix automaton, Manacher and Duval from memory, each compiling and passing the
  `example.cpp` asserts on the first or second try.
- You can explain the amortised argument of Z, KMP, Kasai and SAM in two sentences each.
- Given the statement of any CSES String Algorithms task you name the structure in 30 seconds.
- All 21 CSES String Algorithms tasks in `problems.md` are solved; Substring Order II and Inverse
  Suffix Array without hints.
