# Chapter 02 — Complete Search, Backtracking, Search on the Answer, Greedy

## What you'll be able to do after this chapter

- Enumerate every subset, permutation, submask or k-combination of a small set in the idiom the
  problem wants (recursive, bitmask, `next_permutation`, Gray code), and know the exact cost.
- Write a backtracking search with O(1) feasibility checks and undo, then prune it by an order
  of magnitude with dead-end detection, symmetry and most-constrained-first ordering.
- Turn `n ≤ 40` into a meet-in-the-middle solution, and `n ≤ 20` into a bitmask enumeration
  with O(1) incremental state.
- Choose between BFS, iterative deepening and bidirectional BFS for an implicit state graph, and
  set up branch and bound for an optimisation search.
- Search over a continuous or integer domain: ternary and golden-section search on unimodal
  functions, binary search on reals with a fixed iteration count, and "binary search on the
  answer" with a written monotonicity argument.
- Prove a greedy with an exchange argument, sort by the right key, and recognise the three
  situations where greedy is wrong and DP is needed.

Prerequisites: `../../algorithms_learning/02_two_pointers_and_sliding_window`,
`03_binary_search`, `08_greedy_and_intervals`, `07_dynamic_programming` §7 (bitmask DP) for the
basic patterns, which are referenced and not re-taught; chapter 01 for the toolkit.

---

## Where this shows up in contests

| Signal in the statement                                        | Technique                          | Typical placement                                 |
|----------------------------------------------------------------|------------------------------------|---------------------------------------------------|
| `n ≤ 10`, "arrangements", "orderings"                          | permutations, O(n!)                | Datatähti qualifier, CF Div2 A–B, IOI subtask 1   |
| `n ≤ 20`, "choose a subset", "divide into two groups"          | bitmask enumeration                | CSES Apple Division, IOI subtask 1–2              |
| `n ≤ 40` (an odd number to see)                                | meet in the middle                 | CF Div2 D/Div1 B, CSES Meet in the Middle         |
| `n ≤ 25–30`, grid ≤ 8×8, "place pieces", "fill"                | backtracking with pruning          | CF Div2 C–D, CSES Chessboard and Queens, Grid Path Description |
| "minimum time / maximum size such that…", answer up to 10^18  | binary search on the answer        | CF Div2 C–D, IOI subtask 3–4, CSES Factory Machines |
| "minimise f(x)" with f convex/unimodal, real or integer x       | ternary / golden-section search    | CF Div2 D–E, geometry                             |
| shortest sequence of moves, huge implicit state space          | BFS / bidirectional BFS / IDDFS    | CF Div2 C–D, puzzle problems                      |
| "maximum number of non-overlapping…", "schedule", "deadlines"  | sort + greedy, exchange argument   | CF Div2 B–C, CSES Movie Festival, Tasks and Deadlines |
| small `n` in one subtask, `n ≤ 2·10^5` in the last              | brute force **is** the first subtask; also your stress-test oracle | every IOI task |

The IOI habit this chapter builds: **the brute force is never wasted**. It is subtask 1, it is
the checker for your real solution, and looking at its output on small cases is how patterns
(chapter 01's Mex Grid) and greedy rules are discovered.

---

## 1. Generating subsets

### 1.1 Recursive (skip / take)

```cpp
void subsets_rec(int k, int n, vector<int>& cur, vector<vector<int>>& out) {
    if (k == n) { out.push_back(cur); return; }
    subsets_rec(k + 1, n, cur, out);        // k not chosen
    cur.push_back(k);
    subsets_rec(k + 1, n, cur, out);        // k chosen
    cur.pop_back();
}
```

The recursion tree is a complete binary tree of depth n: 2^n leaves, 2^{n+1} − 1 calls. Use this
form when the "take" step needs state that is expensive to compute from scratch (running sums,
a partially filled board) — you update on the way down and undo on the way up.

Trace for n = 3 (the order in which leaves are produced):

```
                        {}                         k=0
              /                    \
           {}                       {0}            k=1
         /     \                  /      \
       {}      {1}             {0}       {0,1}     k=2
      /  \    /   \           /   \      /    \
    {}  {2} {1} {1,2}      {0} {0,2} {0,1} {0,1,2}   k=3: leaves
```

### 1.2 Bitmask

Subset ↔ integer in [0, 2^n): bit i set ⇔ element i chosen. This is the form for **bitmask DP**
(`../../algorithms_learning/07_dynamic_programming` §7) and for anything that needs subsets as
array indices.

```cpp
for (int m = 0; m < (1 << n); m++)
    for (int i = 0; i < n; i++)
        if (m >> i & 1) { /* element i is in subset m */ }
```

Cost O(2^n · n) to expand every subset. The inner loop disappears when the per-subset quantity
can be built from a **smaller subset in O(1)**:

```cpp
// sum[m] = sum over subset m, for all m, in O(2^n) total
vector<ll> s(1 << n);
for (int m = 1; m < (1 << n); m++)
    s[m] = s[m & (m - 1)] + w[__builtin_ctz(m)];   // m & (m-1) drops the lowest set bit
```

`m & (m − 1)` is m without its lowest set bit, `__builtin_ctz(m)` is the index of that bit —
so `s[m]` is "the same subset minus one element, plus that element's weight". The same trick
computes popcount tables, XOR of subsets, products, or any associative aggregate.

Operations you must know cold:

| Expression              | Meaning                              |
|-------------------------|--------------------------------------|
| `m >> i & 1`            | is i in m                            |
| `m | 1 << i`            | add i                                |
| `m & ~(1 << i)`         | remove i                             |
| `m ^ 1 << i`            | toggle i                             |
| `m & (m - 1)`           | drop lowest set bit                  |
| `m & -m`                | lowest set bit as a value            |
| `(1 << n) - 1`          | the full set                         |
| `__builtin_popcount(m)` | size of m                            |
| `1LL << i`              | when i ≥ 31 — `1 << 40` is UB        |

### 1.3 Gray code order

Iterating m = 0, 1, 2, … changes many bits at once between consecutive subsets. The reflected
Gray code `g(i) = i ^ (i >> 1)` visits every subset exactly once and consecutive subsets differ
in **exactly one bit**: the bit that flips between g(i) and g(i+1) is `__builtin_ctz(i + 1)`.
So a state that is expensive to rebuild but cheap to update by one element (a hash of the set, a
product, a matrix rank) is maintained in O(1) per subset, giving O(2^n) instead of O(2^n · n).

Proof that neighbours differ in one bit: g(i) ^ g(i+1) = (i ^ (i+1)) ^ ((i ^ (i+1)) >> 1). Adding
1 flips a block of trailing bits: i ^ (i+1) = 2^{k+1} − 1 for k = ctz(i+1). Then
(2^{k+1} − 1) ^ (2^k − 1) = 2^k. ∎

```
i : 0  1  2  3  4  5  6  7
g : 000 001 011 010 110 111 101 100
```

CSES "Gray Code" (2205) prints exactly this list; the recursive definition (list for n−1 with a
0 prefix, then reversed with a 1 prefix) is the same sequence.

### 1.4 Submasks of a mask, and k-subsets

```cpp
for (int s = m; ; s = (s - 1) & m) { /* s ⊆ m */ if (s == 0) break; }
```

`(s − 1) & m` is the next smaller submask: subtracting 1 borrows through the low zero bits of s
and `& m` clears the bits that were never in m. Summed over all m of n bits this is 3^n (each
element is in neither, in m only, or in both) — the cost of "for every subset, for every
sub-subset" DP such as CSES-style set partition problems; 3^15 ≈ 1.4·10^7 fine, 3^20 ≈ 3.5·10^9
not.

All subsets of size k in increasing numeric order (Gosper's hack): with `c = s & -s`,
`r = s + c`, next = `(((r ^ s) >> 2) / c) | r`. Useful for C(n,k) enumeration when 2^n is too
much but C(n,k) is not (e.g. C(30,5) = 142506).

---

## 2. Generating permutations

### 2.1 Recursive with `used[]`

```cpp
void perms_rec(int n, vector<int>& cur, vector<bool>& used, vector<vector<int>>& out) {
    if ((int)cur.size() == n) { out.push_back(cur); return; }
    for (int i = 0; i < n; i++) {
        if (used[i]) continue;
        used[i] = true; cur.push_back(i);
        perms_rec(n, cur, used, out);
        cur.pop_back(); used[i] = false;
    }
}
```

n! leaves, each reached through n levels: O(n! · n). n = 10 → 3.6·10^7 — fine; n = 12 → 5.7·10^9
— not. This is the general **"assign positions one by one"** backtracking skeleton; every
constraint that can be tested on a prefix goes right after `if (used[i]) continue;` and prunes.

### 2.2 `std::next_permutation`

```cpp
sort(all(v));
do { /* use v */ } while (next_permutation(all(v)));
```

Two guarantees you will rely on: (1) starting from the **sorted** sequence it produces every
permutation exactly once in lexicographic order and returns `false` after the last; (2) with
**duplicate elements** it produces every *distinct* arrangement exactly once — "Creating
Strings" (1622) is `sort` + this loop, no dedup needed. Amortised O(1) per step (worst step is
O(n)). Two classic bugs: not sorting first (you miss permutations before the start), and calling it
on a `vector<bool>`-like proxy sequence where swaps misbehave — use `vector<char>`.

How it works: find the largest i with v[i] < v[i+1] (the "pivot"); if none, the sequence is the
last permutation. Otherwise find the largest j with v[j] > v[i], swap, reverse the suffix after i.
Knowing this lets you implement `prev_permutation` or a k-th permutation by hand when needed.

### 2.3 Which to use

| Need                                                   | Use                                          |
|--------------------------------------------------------|----------------------------------------------|
| every permutation, no pruning, n ≤ 10                  | `next_permutation`                           |
| distinct arrangements of a multiset                    | `next_permutation`                           |
| constraints on prefixes (prune early)                  | recursive with `used[]`                      |
| permutations as DP state                               | bitmask DP over visited set, O(2^n · n²)     |
| random permutation                                     | `shuffle(all(v), rng)` — never `random_shuffle` |

---

## 3. Backtracking with pruning

Backtracking = DFS over partial solutions where a partial solution is extended one decision at a
time, and the branch is abandoned as soon as it **cannot** lead to a full solution. The
correctness invariant: every partial solution on the stack is consistent with all constraints
that involve only the decided part. Complexity is exponential in the worst case; the *entire*
craft is in (a) O(1) feasibility tests, (b) undoing state, (c) pruning, (d) ordering.

### 3.1 N-queens: O(1) checks via indexed diagonals

One queen per column, so decide column by column, choosing the row. Attack tests are three
array lookups: `row[x]`, `d1[x + y]` (↙↗ diagonals have constant x + y), `d2[x − y + n − 1]`
(↖↘ diagonals have constant x − y, shifted to be non-negative).

```cpp
int n; ll cnt = 0;
vector<char> row, d1, d2;           // sizes n, 2n, 2n
void go(int y) {
    if (y == n) { cnt++; return; }
    for (int x = 0; x < n; x++) {
        if (row[x] || d1[x + y] || d2[x - y + n - 1]) continue;
        row[x] = d1[x + y] = d2[x - y + n - 1] = 1;
        go(y + 1);
        row[x] = d1[x + y] = d2[x - y + n - 1] = 0;   // undo — forgetting this is THE bug
    }
}
```

n = 8 gives 92 solutions after ~2000 recursive calls (out of 8^8 ≈ 1.7·10^7 naive); n = 12 →
14200 solutions, still instant; n = 15 takes seconds. CSES "Chessboard and Queens" (1624) adds
blocked squares — one extra `continue`.

```
y:   0 1 2 3          Trace n=4, first solution found:
   +-+-+-+-+          go(0): x=0 ok → go(1): x=0,1 fail (row, d1) x=2 ok → go(2): x=0 fail(d2)
 0 |.|.|Q|.|                 x=1 fail(d1) x=2 fail(row) x=3 fail(d1)  → back, undo x=2
 1 |Q|.|.|.|                 go(1): x=3 ok → go(2): x=1 ok → go(3): x=0..3 all fail → back …
 2 |.|.|.|Q|          … eventually go(0): x=1 → go(1): x=3 → go(2): x=0 → go(3): x=2 ✓ cnt=1
 3 |.|Q|.|.|          Total: 2 solutions for n=4.
   +-+-+-+-+
```

### 3.2 Sudoku: candidate bitmasks and most-constrained-first

State: for each row, column and 3×3 box a 9-bit mask of used digits. Candidates of an empty
cell = `~(rowm | colm | boxm) & 0x3FE`. Instead of filling cells in reading order, pick the empty
cell with the **fewest candidates** (minimum remaining values). A cell with 0 candidates kills the
branch immediately; a cell with 1 candidate is a forced move and costs no branching. This
ordering alone takes hard sudokus from millions of nodes to hundreds. `example.cpp` solves the
classic "53..7…" puzzle in under a millisecond.

The general principle — **branch on the most constrained variable, try the most promising
values first** — is the difference between backtracking that finishes and backtracking that does
not, in almost every board-filling / assignment problem.

### 3.3 Grid paths: dead-end and split pruning, symmetry

CPH's showcase (chapter 5): count Hamiltonian paths in a 7×7 grid from one corner to the
opposite corner. 4^48 naive; the actual count is 88418. Pruning rules, each a two-line check
before recursing, with CPH's measured call counts:

| Rule                                                                            | Recursive calls | Time  |
|---------------------------------------------------------------------------------|-----------------|-------|
| none                                                                            | 7.6·10^10       | 483 s |
| 1. symmetry: first step down ≡ first step right (mirror in the diagonal); count one, double | 3.8·10^10 | 244 s |
| 2. reaching the target before all cells are visited → stop                      | 2.0·10^10       | 119 s |
| 3. cannot go straight, but both left and right are free → the path plus the wall splits the free region; stop | 2.2·10^8 | 1.8 s |
| 4. rule 3 also when the blocker is a visited cell, not just the wall             | 6.9·10^7        | 0.6 s |

Why rule 3/4 is valid: the path from the start (on the border) to the current cell, together
with the blocker ahead (border, or an earlier part of the same path), forms a closed barrier.
The cells to the left and right of the current cell lie on opposite sides of it, so no
Hamiltonian continuation can visit both. This is the pattern of a strong prune: a **cheap local
test that certifies global infeasibility**.

Implementation habits that make this fast: a 1-cell wall of "visited" around the board removes
all bounds checks; direction arrays `dx[4], dy[4]` with `(dir + 1) & 3` for "left" and
`(dir + 3) & 3` for "right"; state as a flat `vector<char>`. CSES "Grid Path Description" (1625)
gives the path as a string with `?` wildcards from the top-left to the bottom-**left** corner —
rules 2–4 apply verbatim; rule 1 does not (the corners are not symmetric); a fixed letter in the
string prunes 3 of 4 branches for free.

### 3.4 Pruning heuristics, in order of power

1. **Feasibility of the partial state**, tested in O(1) with auxiliary arrays (rows, diagonals,
   remaining capacity, remaining sum).
2. **Bounding**: an optimistic estimate of the best completion; if it cannot beat the best
   solution found, stop (§6).
3. **Dead-end / connectivity detection**: the split rule above; "the remaining items cannot fill
   the remaining space"; "fewer moves remain than the Manhattan distance to the target".
4. **Symmetry breaking**: fix the first choice up to symmetry and multiply; canonical orderings
   (e.g. in "partition into k equal groups", put the first unused item in the first non-full group
   only — this removes the k! relabelings).
5. **Ordering**: most constrained variable first; most promising value first (so the first
   solution found is good, which makes bounding bite earlier).
6. **Memoisation of states** when the same partial state recurs (then you are doing DP; see
   chapter 07 DP advanced — bitmask DP is exactly "memoised backtracking over subsets").

Always **measure node counts** (a global counter printed under `#ifdef LOCAL`) after each rule.
A rule that halves the nodes is worth 5 lines; one that removes 5% is not.

---

## 4. Meet in the middle

When 2^n is too large but 2^{n/2} is fine (n ≈ 32–44), and the objective splits additively over
two halves: enumerate all 2^{n/2} partial results of each half, then **combine with one lookup
per left element** (sort + binary search, or a hash map).

Canonical problem (CSES 1628 "Meet in the Middle"): count subsets with sum exactly x, n ≤ 40.

```cpp
ll count_subsets_with_sum(const vector<ll>& w, ll x) {
    int n = w.size(), h = n / 2;
    vector<ll> A(w.begin(), w.begin() + h), B(w.begin() + h, w.end());
    vector<ll> sa = subset_sums(A), sb = subset_sums(B);     // §1.2, O(2^{n/2}) each
    sort(all(sb));
    ll ans = 0;
    for (ll a : sa) {
        auto [lo, hi] = equal_range(all(sb), x - a);
        ans += hi - lo;
    }
    return ans;
}
```

Correctness: every subset S of the whole set decomposes uniquely as S∩A ∪ S∩B, and
sum(S) = x ⇔ sum(S∩B) = x − sum(S∩A); the loop counts, for each left part, exactly the right
parts that complete it. `equal_range` (not `binary_search`) because several right subsets can
have the same sum. Complexity O(2^{n/2} · n) from the sort: n = 40 → 2^20 · 20 ≈ 2·10^7.
Memory: two arrays of 2^20 `long long` = 16 MB. The answer can be up to 2^40: `long long`.

```
w = [3, 5 | 2, 7], x = 10
sa = sums of {3,5} = [0, 3, 5, 8]
sb = sums of {2,7} = [0, 2, 7, 9] (sorted)
a=0 → need 10: none    a=3 → need 7: one ({7})    a=5 → need 5: none    a=8 → need 2: one ({2})
answer 2  ({3,7}, {8→3,5}+{2} = {3,5,2})
```

Variants: minimise |sum − x| (for each a, `lower_bound` x − a and check the neighbour); two
arrays and "a_i + b_j = x" counting; CSES "Sum of Four Values" (1642) — all pair sums of one
array, O(n²) pairs in a hash map, then pairs with disjoint indices (2 + 2 split is meet in the
middle over positions); 4-SUM-style problems generally; "Hamming Distance"-type problems where
the split is over bits. Bidirectional BFS (§5) is meet in the middle over paths.

---

## 5. Iterative deepening and bidirectional BFS

State-space search over an implicit graph (positions of a puzzle, numbers reachable by
operations). BFS gives shortest paths in O(states) time and memory
(`../../algorithms_learning/06_graphs`). Two situations where plain BFS is the wrong tool:

**Memory: the state space is far larger than the reachable depth allows to store.** Iterative
deepening DFS (IDDFS) runs a depth-limited DFS with limits 0, 1, 2, …, d. Memory O(d); time
O(b^d) where b is the branching factor — the same order as BFS, because the last iteration
dominates the geometric sum (Σ b^k = O(b^d)). Finds the shortest path like BFS, uses no queue and
no visited set. Pair it with a heuristic lower bound (IDA*: prune when depth + h(state) > limit)
for 15-puzzle-class problems.

```cpp
bool dls(State v, State t, int depth) {
    if (v == t) return true;
    if (depth == 0) return false;
    for (State u : moves(v)) if (dls(u, t, depth - 1)) return true;
    return false;
}
int iddfs(State s, State t) { for (int d = 0; ; d++) if (dls(s, t, d)) return d; }
```

**Time: b^d is too big but b^{d/2} is fine.** Bidirectional BFS grows one frontier from the
source using forward moves and one from the target using **reverse** moves, always expanding the
smaller frontier; the first state seen from both sides gives distance d_s + d_t. Explores
O(b^{d/2}) states from each side. It needs (a) an explicitly known target state and (b) the
inverse move set — for permutation puzzles moves are self-inverse; for "multiply by 2" the
inverse is "halve if even".

```cpp
int bidir(State s, State t) {
    if (s == t) return 0;
    map<State,int> ds{{s,0}}, dt{{t,0}};
    vector<State> fs{s}, ft{t};
    while (!fs.empty() && !ft.empty()) {
        bool fromS = fs.size() <= ft.size();
        auto& fr = fromS ? fs : ft;  auto& mine = fromS ? ds : dt;  auto& other = fromS ? dt : ds;
        vector<State> nxt;
        for (State v : fr)
            for (State u : (fromS ? moves(v) : rmoves(v))) {
                if (mine.count(u)) continue;
                mine[u] = mine[v] + 1;
                if (other.count(u)) return mine[u] + other[u];
                nxt.push_back(u);
            }
        fr.swap(nxt);
    }
    return -1;
}
```

Correctness: frontiers are expanded one full layer at a time from the smaller side, so when a
state u is first found in both maps, mine[u] + other[u] is the length of a shortest path through
u, and any shorter path would have produced a meeting at an earlier layer. Pitfall: check for the
meeting **when generating** u (as above) — checking only when popping can overshoot by one layer
and return a non-minimal distance if you stop too early.

| Method             | Time      | Memory   | Needs                                   |
|--------------------|-----------|----------|-----------------------------------------|
| BFS                | O(b^d)    | O(b^d)   | nothing                                 |
| IDDFS              | O(b^d)    | O(d)     | small d; cycles are harmless but wasteful|
| bidirectional BFS  | O(b^{d/2})| O(b^{d/2})| known target, inverse moves            |
| A* / IDA*          | depends   | —        | admissible heuristic                    |

---

## 6. Branch and bound

Backtracking for **optimisation**: keep the best complete solution found; at every node compute
an **optimistic bound** on the best completion of this partial solution; if bound ≤ best, prune.
Exact (never prunes a branch that could win) as long as the bound is truly optimistic.
Exponential worst case, but with good ordering the first solution found is near-optimal and the
bound cuts most of the tree.

0/1 knapsack as the model: sort items by value density; bound = current value + fractional
knapsack of the remaining items into the remaining capacity (an LP relaxation — always ≥ the
true integer optimum of the remainder). Try "take" before "skip" so the greedy-like solution is
found first.

```cpp
void go(int k, ll w, ll v) {
    best = max(best, v);
    if (k == n) return;
    if (bound(k, w, v) <= best) return;                  // prune
    if (w + wt[k] <= cap) go(k + 1, w + wt[k], v + val[k]);
    go(k + 1, w, v);
}
```

Contest use: when n ≈ 30–50 and a clean DP is blocked by large values or an extra constraint,
branch and bound on a well-ordered search often passes when it "should not". Always compare with
a DP on small cases (as `example.cpp` does) — an over-optimistic bound that is *not* an upper
bound silently gives wrong answers. Related: **DFS with memoised upper bounds**, and the
"random restarts + greedy" heuristics used for output-only IOI tasks.

---

## 7. Ternary search and golden-section search

For a **unimodal** function (strictly increasing then strictly decreasing — or the reverse for a
minimum) on an interval, compare two interior points; the maximum cannot lie in the third that is
beyond the smaller value.

```cpp
// integer domain: returns argmax on [lo, hi]
while (hi - lo > 2) {
    ll m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
    if (f(m1) < f(m2)) lo = m1 + 1; else hi = m2;
}
// finish by checking lo..hi directly (≤ 3 values)
```

```
lo        m1        m2        hi
|---------|---------|---------|
f(m1) < f(m2) → max is in (m1, hi]      f(m1) ≥ f(m2) → max is in [lo, m2]
```

Each step keeps 2/3 of the interval: ~log_{1.5}(n) ≈ 1.7 log₂ n steps, 2 evaluations each. On
integers, a **plateau** breaks strict unimodality: if f(m1) == f(m2) with the plateau *not* at the
maximum the search may go the wrong way. Safe when equal values occur only at the maximum
(typical for convex-like integer functions). If f is convex/concave on integers you can instead
binary search on the sign of f(x+1) − f(x), which handles plateaus at the maximum cleanly.

Real domain: iterate a **fixed number of times** (100 iterations shrink by (2/3)^100 ≈ 2.5·10^-18,
already below `double` resolution; 200 if you use `long double`) — never `while (hi − lo > eps)`,
which can spin forever when eps is below the representable gap. Precision caveat: near a smooth
maximum f is flat, so comparisons are decided by rounding once |m1 − m2| ≲ √ε ≈ 10^-8; the
**argmax** is only accurate to ~10^-7 even though the **maximum value** is accurate to ~10^-15.
When the position is the answer, use exact arithmetic or a derivative-sign binary search.

**Golden-section search** places the two points at the golden ratio so that one of them is
reused in the next iteration: 1 evaluation per step, interval ×0.618 per step versus ×0.667 per
2 evaluations for ternary. Use it when f is expensive (each evaluation is itself an O(n) scan or a
simulation). Implementation in `example.cpp`.

Typical contest shapes: minimise the maximum distance from a point on a line to given points
(convex in the parameter); "choose a threshold t, cost(t) is convex"; nested — ternary search
over one variable with an inner binary search or greedy computing f.

---

## 8. Binary search on the answer — at contest depth

`../../algorithms_learning/03_binary_search` §4 covers the mechanics. What changes at contest
level is that you must **prove monotonicity** and pick the **predicate direction and bounds**
without thinking — the predicate is the whole problem.

**Statement shape.** "Find the minimum X such that [something is achievable]" or "the maximum X
such that [something is still possible]". Replace "find the optimal X" with "given X, can we?" —
if the answer to *can we?* is monotone in X (achievable for X ⇒ achievable for X+1, or the
reverse), the optimum is the boundary and O(log range) predicate evaluations find it.

**The proof obligation.** Write one sentence: "if T seconds suffice to make t products, then
T+1 seconds also suffice (each machine makes at least as many)". If you cannot write that
sentence, binary search is wrong for this problem. Typical monotonicity arguments: more
time/capacity/budget can only help (superset of options); a stricter threshold admits a subset of
configurations; a larger allowed maximum admits a superset of partitions.

**Canonical skeleton — smallest x with ok(x) true:**

```cpp
ll first_true(ll lo, ll hi, auto ok) {        // ok is false…false true…true on [lo, hi]
    while (lo < hi) {
        ll mid = lo + (hi - lo) / 2;          // (lo+hi)/2 overflows at 1e18
        if (ok(mid)) hi = mid; else lo = mid + 1;
    }
    return lo;                                 // == hi; hi must be a value where ok is true (or a sentinel)
}
```

Invariant: ok(x) is false for all x < lo, and ok(hi) is true (or hi is the sentinel "no
answer"). Each step preserves it and shrinks hi − lo, so it terminates at the boundary. For
"largest x with ok(x) true" either flip the predicate (`!ok`) and subtract one, or write the
mirror loop with `mid = lo + (hi − lo + 1) / 2`; `lo = mid` / `hi = mid − 1`. Do not keep two
templates in your head — keep one and transform the predicate.

**Bounds.** lo = the smallest value that could possibly work (1, or max element); hi = a value
that certainly works — compute it, do not guess (Factory Machines: `hi = min(k) · t` ≤ 10^18,
so `ll`, and the predicate's running sum must **early-exit at ≥ t** or cap, otherwise
Σ T/k_i over 2·10^5 machines can overflow). Range 10^18 → 60 iterations × O(n) predicate:
n = 2·10^5 → 1.2·10^7, fine.

**Predicate cost decides everything.** Greedy predicates are O(n) (Array Division: scan and cut
when the running sum would exceed S — greedy is optimal because cutting as late as possible
never hurts, an exchange argument); sometimes the predicate is itself a BFS/Dijkstra
(`algorithms_learning/03` §6), a DP, or a matching — then the total is O(log range × that).

**Real-valued answers**: same loop with a fixed 100 iterations (§7), answer `hi`. Output with the
required precision; when the answer is rational and the checker is exact, binary search on
integers over a scaled domain instead.

**Parallel binary search / integer-rational tricks** come later; recognise them by "q queries
each asking for a threshold" (chapter 09).

Worked example — Array Division (1085): split a into k contiguous parts minimising the largest
part sum. ok(S) = "can we split into ≤ k parts each ≤ S". Monotone: a split that works for S
works for S+1. Predicate: greedy scan, new part when cur + x > S; count ≤ k. lo = max(a) (any
part must hold its largest element), hi = Σa (one part). k = 2, a = [2, 4, 7, 3, 5]:

```
S=21 ok(1 part)   S=13 ok([2,4,7],[3,5])   S=9 no ([2,4],[7],[3,5] = 3 parts)   S=11 no   S=12 no
S=13 → answer 13  (parts sums 13 and 8)
```

---

## 9. Two pointers and sliding window — pointers only

Already covered in `../../algorithms_learning/02_two_pointers_and_sliding_window`. Contest-level
additions are only these reminders:

- Two pointers on a sorted array solves "pair with sum x" (1640), "three values" (1641, O(n²)
  with the third pointer), and is the O(n) alternative to a hash map with no anti-hash risk.
- Sliding window with a counter map solves "shortest subarray with k distinct" (1141 Playlist
  is the longest-without-repeat version), "count subarrays with ≤ k distinct" (2428).
- Prefix sums + hash map counts subarrays with sum x (1661) or divisible by n (1662): count
  equal prefix residues, `(x % n + n) % n`.
- Sliding window minimum/maximum via a monotonic deque and "nearest smaller value" via a
  monotonic stack (1645) are in `algorithms_learning/04`. CSES 1644 (Maximum Subarray Sum II)
  is "for each r, min prefix in a window of length [a, b]" — the deque, or a `multiset`.

The CSES tasks of these shapes are in `problems.md` so the whole Sorting and Searching section
is covered here; the techniques are not repeated.

---

## 10. Greedy, revisited: the exchange argument formalised

A greedy algorithm makes an irrevocable locally best choice at each step. It is correct exactly
when you can prove it; the standard proof is the **exchange argument**:

1. Let O be any optimal solution and G the greedy solution. Suppose they differ.
2. Find the first decision where they differ; O made choice o, greedy made g.
3. Show that replacing o by g in O (possibly adjusting the rest) yields a solution O' that is
   still feasible and **no worse** than O.
4. Repeat: O → O' → O'' … agrees with G on more and more decisions and never gets worse, so G
   is at least as good as O, hence optimal. ∎

The *adjacent swap* form is often easier for ordering problems: show that any two adjacent
elements in the wrong order can be swapped without loss; then the sorted order is optimal because
every other order can be bubble-sorted into it without loss.

### 10.1 Interval scheduling (Movie Festival, 1629)

Sort by **end time**; take an interval whenever it starts at or after the last taken end.
Exchange: greedy's first interval g ends earliest of all intervals; if O's first interval is o ≠ g,
end(g) ≤ end(o), so every later interval of O still fits after g; swap o → g, size unchanged.
Induct on the remaining intervals after end(g). O(n log n). Counter-examples for the tempting
alternatives: sort by start (0,10),(1,2),(3,4) picks 1 not 2; sort by length (0,4),(3,5),(4,8)
picks 1 not 2.

### 10.2 Ordering by a key: adjacent swap

Tasks and Deadlines (1630): reward Σ(d_i − f_i) where f_i is the finishing time. Σd_i is
constant, so minimise Σf_i. Two adjacent tasks a then b with durations x > y starting at time s
contribute (s+x) + (s+x+y); swapped, (s+y) + (s+y+x). Difference x − y > 0 — swapping helps. So
in an optimal order no longer task precedes a shorter one: **sort by duration ascending**; the
deadlines are irrelevant to the order.

General recipe for "in which order": write the cost of the pair (a, b) vs (b, a) with everything
else fixed; the inequality that says "a before b is better" is your comparator — check that it is
**transitive** (a valid strict weak ordering), otherwise `sort` is UB and the greedy is wrong.
Classic: minimise Σ w_i · C_i (weighted completion) → sort by p_i / w_i (Smith's rule); concatenate
numbers for the largest result → `a + b > b + a` as strings (transitive because it is equivalent to
comparing a/(10^|a| − 1)); "swap two adjacent to reduce cost" also drives Huffman and many CF C's.

### 10.3 Sorting by custom keys in C++17

```cpp
sort(all(v), [](const auto& a, const auto& b) {
    if (a.end != b.end) return a.end < b.end;       // primary key
    return a.start > b.start;                       // secondary, descending
});
sort(all(v), [](auto& a, auto& b) { return tie(a.x, a.y) < tie(b.x, b.y); });  // lexicographic
sort(all(idx), [&](int i, int j) { return a[i] < a[j]; });   // sort indices, keep a intact
stable_sort(...)                                            // preserve input order among equals
```

Comparators must return false for equal elements. `<=` is not a strict weak ordering: `sort` may
read out of bounds and crash. Comparing doubles with a tolerance inside a comparator is also not
transitive — compare exact keys, or integers.

### 10.4 Coin problems

Greedy (largest coin that fits) is optimal for **canonical** coin systems (euro, US, any system
where each coin is a multiple of the previous, and others one can test) and fails in general:
{1, 3, 4}, x = 6 → greedy 4+1+1 (3 coins), optimum 3+3 (2). The failure is precisely a failed
exchange: taking 4 commits to a remainder 2 that cannot be covered well. Fallback is the DP
`f(x) = 1 + min_c f(x − c)` (`algorithms_learning/07`). A system is canonical iff greedy is
optimal for all x < (largest coin + second largest) — a checkable brute force (Kozen–Zaks); in
a contest, if the coins are arbitrary, do DP.

Missing Coin Sum (2183): sort; maintain `reach` = all sums 0..reach are makeable; if the next
coin c > reach + 1 then reach + 1 is the answer, else reach += c. Proof: sums 0..reach are
makeable by induction, and with c ≤ reach+1 every sum up to reach + c is makeable (s ≤ reach
already, or s − c ∈ [0, reach]).

### 10.5 Huffman intuition

Merging the two smallest weights repeatedly builds a prefix code of minimum total cost
Σ w_i · depth_i. Exchange argument: in any optimal tree the two smallest weights can be made
siblings at the deepest level (swapping a deeper heavier leaf with a shallower lighter one does
not increase cost), so the greedy first step is safe; the merged node behaves as a single symbol
of weight w₁ + w₂ and induction finishes. Total cost = sum of all merged weights.
`priority_queue<ll, vector<ll>, greater<ll>>`, O(n log n). The same "merge two smallest" greedy
answers "minimum total cost to combine piles / ropes" and appears as CF Div2 C. When merging cost
is not symmetric or merges are constrained (adjacent only) it is an interval DP instead.

### 10.6 When to suspect greedy fails

- The local choice **commits** you to a remainder with worse structure (coins {1,3,4}).
- Two competing objectives (weight and value) with no single ordering that dominates (0/1
  knapsack: density greedy fails; fractional knapsack: density greedy works).
- The exchange step changes something *else* in the solution that you cannot control (feasibility
  of a later item depends on more than one earlier choice).
- You cannot write the adjacent-swap inequality, or it is not transitive.
- Small brute force disagrees with greedy on random tests — this is why you **always** stress
  test a greedy against a brute force on n ≤ 8 before submitting. It takes five minutes and
  catches 90% of wrong greedies.

When greedy fails and n is small: backtracking with pruning (§3) or DP over the decision
structure (chapters 07/13).

---

## Recognition cheatsheet

| Statement signal                                             | Technique                                 | Complexity                    |
|--------------------------------------------------------------|-------------------------------------------|-------------------------------|
| n ≤ 10, orderings                                            | `next_permutation` / recursive perms      | O(n! · n)                     |
| n ≤ 20–25, subsets / two groups / choose items               | bitmask enumeration, O(1) incremental via lowbit or Gray code | O(2^n) – O(2^n · n) |
| n ≤ 40, subset sum / count                                   | meet in the middle                        | O(2^{n/2} · n)                |
| for each subset, over its subsets                            | submask enumeration                       | O(3^n), n ≤ 15                |
| board ≤ 8×8 / ≤ 30 cells to fill, pieces to place            | backtracking: O(1) checks, undo, prune, MRV ordering | exponential, pruned |
| count Hamiltonian paths / tilings on a small grid             | backtracking with split/dead-end pruning + symmetry | exponential, pruned |
| min/max X such that feasible; X up to 10^18                  | binary search on the answer + monotone predicate | O(log range · cost(ok)) |
| minimise a convex/unimodal cost over a parameter             | ternary / golden-section search           | O(log range · cost(f))        |
| shortest move sequence, known target, inverse moves          | bidirectional BFS                         | O(b^{d/2})                    |
| shortest move sequence, states too many to store             | IDDFS / IDA*                              | O(b^d), memory O(d)           |
| maximise value under a budget, n ≈ 30–50, values huge        | branch and bound                          | exponential, pruned           |
| max non-overlapping intervals                                | sort by end, greedy                       | O(n log n)                    |
| order tasks to minimise sum of completion / lateness         | adjacent-swap → sort by key               | O(n log n)                    |
| fewest coins, canonical system                               | greedy largest-first; else DP             | O(x) / O(x · coins)           |
| merge piles at cost = sum                                    | Huffman (min-heap)                        | O(n log n)                    |
| pairs/triples with given sum in an array                     | sort + two pointers / hash map            | O(n log n) / O(n²)            |
| subarrays with sum x / divisible by n / ≤ k distinct         | prefix sums + map / sliding window        | O(n) – O(n log n)             |

---

## Implementation checklist for contests

1. Enumeration size computed: 2^n, n!, 3^n, 2^{n/2} · n — and compared with 10^8.
2. Bitmask code uses `1LL << i` when i can reach 31+; `__builtin_ctz(0)` never called.
3. `next_permutation` starts from a sorted sequence; loop is `do … while`.
4. Backtracking: every state change is undone on the way back; feasibility arrays sized for
   the full index range (diagonals 2n − 1); node counter printed under `LOCAL` while tuning.
5. Meet in the middle: `equal_range` for counting (duplicates!), `long long` sums and answer,
   memory 2^{n/2} · 8 bytes checked.
6. Binary search on the answer: monotonicity sentence written; `lo`/`hi` are feasible bounds
   (hi certainly true or sentinel); `mid = lo + (hi − lo) / 2`; predicate early-exits to avoid
   overflow; the returned value is re-verified with `ok()` if "no answer" is possible.
7. Real-valued search: fixed iteration count (100), answer printed with `fixed << setprecision`.
8. Ternary search: unimodality argued; plateau only at the optimum; integer version finishes with
   a direct scan of the last ≤ 3 candidates.
9. Greedy: exchange argument written in one sentence; comparator is a strict weak ordering (`<`
   not `<=`, no epsilon); stress-tested against brute force on n ≤ 8, 500+ random cases.
10. Bidirectional BFS: meeting detected at generation time; reverse moves correct (write them as
    "u such that v ∈ moves(u)" and test on random states).

---

## Further reading

- Laaksonen, CPH: ch. 5 (Complete search: subsets, permutations, backtracking, pruning the
  grid-path search, meet in the middle), ch. 6 (Greedy: coins, scheduling, tasks and deadlines,
  minimising sums, data compression/Huffman), ch. 3.3 (binary search, finding the smallest
  solution, maximum value of a unimodal function).
- cp-algorithms.com: "Submask Enumeration", "Binary Search" (incl. search on the answer and
  parallel binary search notes), "Ternary Search", "Meet in the Middle" is covered under
  "Bitmask / Combinatorics" and in the knapsack article; "Huffman coding" in the string
  compression notes.
- Halim & Halim, *Competitive Programming 4*, book 1 ch. 3 (Complete Search, Divide & Conquer,
  Greedy) — the pruning-order discussion and "Iterative Complete Search" taxonomy.
- Knuth, *The Art of Computer Programming* Vol. 4A §7.2.1.1–7.2.1.2 (generating tuples,
  permutations; Gray codes) — for the mechanics behind `next_permutation` and Gosper's hack.
- Kozen & Zaks, "Optimal bounds for the change-making problem" (1994) — when greedy coin change
  is optimal.
- Codeforces EDU section: "Binary Search" course (the predicate framing, real-valued search).

---

## You can move on when...

- You can write, from memory and warning-free, the 2^n subset enumeration with O(1) incremental
  sums, `next_permutation` loop, N-queens with diagonal arrays, meet-in-the-middle counting,
  `first_true` binary search, ternary search (integer and real), and the interval-scheduling greedy
  — each in under three minutes.
- For Grid Path Description you have measured the recursive-call count after each pruning rule
  and can explain why the split rule is sound.
- You have solved every ★1–★3 problem in `problems.md`, at least 60% of the ★4s, and all five
  Introductory complete-search tasks with first-submission AC.
- For three greedy problems you have written the exchange argument before coding and confirmed
  it with a stress test; for one problem you found a counter-example to a plausible greedy.
- You can state, unprompted, the monotonicity sentence for Factory Machines, Array Division and
  Concert-Tickets-style feasibility, and know which direction the predicate runs.
