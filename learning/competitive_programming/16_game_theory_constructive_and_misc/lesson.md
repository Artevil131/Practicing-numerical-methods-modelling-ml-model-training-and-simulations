# Chapter 16 — Game Theory, Constructive Problems, Randomization and Contest Misc

Companion library: `example.cpp` (`c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo`).
Everything claimed in Sections 1–3 is cross-checked there against brute-force minimax.

## What you'll be able to do after this chapter

- Decide any impartial game in the syllabus: Nim (with Bouton's proof), misère Nim, staircase
  Nim, subtraction games, games on graphs (retrograde analysis with draws), and *any* sum of
  games via Sprague–Grundy — compute Grundy numbers with `mex`, xor them, and find the winning
  move.
- Attack a constructive problem systematically: small cases by brute force, spot the
  invariant or parity obstruction, build with a greedy that you can prove, and verify the
  construction with a checker before submitting.
- Write interactive solutions that never deadlock (flush!), keep the query count within the
  bound, and survive adaptive judges.
- Use randomization where it is the *right* tool: random pivots, random hashing of sets and
  multisets (Zobrist / sum-of-random-weights), Freivalds-style probabilistic checks, random
  restarts — and quantify the failure probability.
- Play the IOI format: read the subtasks first, allocate the 5 hours across 3 problems, bank
  partial scores with brute force, and know what communication / output-only / interactive
  tasks look like.

## Where this shows up in contests

| Shape of statement | Placement |
|---|---|
| "Two players alternate… who wins with optimal play?" with piles/sticks/coins | CF Div2 B–D (1200–1900); CSES Mathematics block |
| Same, but the move set is weird (split a pile, take from a subset, move on a board) | CF Div2 E / Div1 C (2000–2300): Sprague–Grundy |
| "Who wins" on a graph, possibly with cycles ("the game may last forever") | CF Div2 D–E: retrograde analysis with draws |
| "Construct any array / permutation / grid such that…" or "print −1 if impossible" | Every CF round has one: Div2 B–D (1200–2000) |
| Interactive: "you may ask at most k questions" | CF Div2 D–E (1600–2200); IOI has 1–2 interactive tasks per year |
| Hashing / randomness as a tool ("are the multisets equal after each update") | CF Div1 C–E (2200–2700) |
| Subtasks with tiny constraints, "output-only", "communication" | IOI, BOI, CEOI — the format itself |

Prerequisites: `../../algorithms_learning/07_dynamic_programming/lesson.md` (memoised
win/lose DP), `06_graphs` (topological order, BFS), bit operations, `mt19937_64`.

---

## 1. Impartial games and Nim

**Definitions.** An *impartial* game: two players alternate, both have the same moves from any
position, no chance, finite (no infinite play), *normal play*: the player who cannot move
loses. Every position is either **N** (Next player wins) or **P** (Previous player wins):

- a position with no moves is P;
- a position is N iff it has a move to a P position; P iff all moves go to N positions.

This is just the win/lose DP from the algorithms course, and for small state spaces it is the
whole solution: `win(s) = any(!win(t) for t in moves(s))` with memoisation. Everything below is
about state spaces too large to enumerate.

### 1.1 Nim and Bouton's theorem

Piles `a₁..aₙ`; a move removes ≥ 1 stones from one pile. **The position is P iff
`a₁ ⊕ a₂ ⊕ … ⊕ aₙ = 0`.**

*Proof.* (a) The terminal position (all zero) has xor 0. (b) From xor 0, any move changes
exactly one pile, hence changes at least one bit of the xor: the result has xor ≠ 0. (c) From
xor `s ≠ 0`: let `b` be the highest set bit of `s`. Some pile `aᵢ` has bit `b` set (otherwise
the xor's bit `b` would be 0). Then `aᵢ ⊕ s < aᵢ` (bit `b` is cleared, higher bits unchanged),
so reducing pile `i` to `aᵢ ⊕ s` is a legal move and the new xor is `s ⊕ aᵢ ⊕ (aᵢ ⊕ s) = 0`.
(b) and (c) together say: the xor-0 positions are exactly the P positions, by induction on the
total number of stones. ∎

```cpp
bool nimFirstWins(const vector<ll>& a) { ll x = 0; for (ll v : a) x ^= v; return x != 0; }
pair<int, ll> nimMove(const vector<ll>& a) {              // {pile, new size}; {-1,-1} if losing
    ll s = 0; for (ll v : a) s ^= v;
    if (!s) return {-1, -1};
    for (int i = 0; i < (int)a.size(); i++) if ((a[i] ^ s) < a[i]) return {i, a[i] ^ s};
    return {-1, -1};
}
```

Worked trace, piles `3, 4, 5`: `3 ⊕ 4 ⊕ 5 = 011 ⊕ 100 ⊕ 101 = 010 = 2`. Highest bit of 2 is
bit 1; piles with bit 1 set: `3 (011)`. Move: `3 → 3 ⊕ 2 = 1`. New piles `1, 4, 5`:
`001 ⊕ 100 ⊕ 101 = 000`. ✓ (CSES "Nim Game I".)

### 1.2 Misère Nim

Last move *loses*. Rule: **play normal Nim until the position would consist only of piles of
size ≤ 1; then the first player wins iff the number of 1-piles is even.** Proof sketch: while
some pile is ≥ 2, the player who is winning under normal play can always keep the xor 0 *and*
avoid entering an all-ones position with the wrong parity (when reducing the last big pile,
choose to leave it at 0 or 1 — exactly one of the choices gives an odd number of ones to the
opponent). With all piles ≤ 1 the game is forced: parity decides.

### 1.3 Staircase Nim (CSES "Stair Game")

Balls on stairs `1..n`; a move moves ≥ 1 balls from stair `k ≥ 2` down to stair `k−1`; balls
on stair 1 never move again. **Winner: xor of the ball counts on stairs at odd distance from
the sink** (stairs `2, 4, 6, …` when the sink is stair 1).

*Why.* Treat the odd-distance stairs as Nim piles. Moving balls *from* an odd stair to the
even stair below is a Nim move (that pile shrinks). Moving balls from an even stair to an odd
stair is "adding to a pile" — but the opponent simply moves the same balls one further down
(back to an even stair), restoring the xor; that reply is always available and never hurts,
so such moves are irrelevant. Ordinary Nim strategy on the odd stairs therefore wins exactly
when the odd-stair xor is non-zero. The test in `example.cpp` confirms this against full
minimax on tiny staircases.

### 1.4 Subtraction games and "Stick Game"

One pile, allowed removals `S = {s₁, …, sₖ}`. `win[n] = any(!win[n − s])`, O(n·k). CSES
"Stick Game" is exactly this (n ≤ 1e6, k ≤ 100). Grundy values of a subtraction game with a
*finite* `S` are eventually periodic — useful when `n` is huge: compute a prefix, detect the
period by matching `k·max(S)` consecutive values, and index by `n mod period`.

### 1.5 "Another Game" (take one from any subset of heaps)

Move: choose a non-empty subset of non-empty heaps, remove one coin from each. **First player
wins iff some heap is odd.** All-even: every move makes at least one heap odd. Some-odd: take
one from *every* odd heap → all even. Terminal all-zero is all-even. ∎ (This kind of parity
invariant is the template for many "weird move set" games: look for a class of positions closed
under the opponent's moves and reachable by yours.)

---

## 2. Sprague–Grundy

**Grundy number** of a position: `g(s) = mex{ g(t) : t reachable from s }`, where `mex` is the
smallest non-negative integer not in the set. Terminal positions have `g = 0`.

**Facts.** (1) `g(s) = 0 ⇔ s is P`. (2) From `s` with `g(s) = k` you can reach every value
`0..k−1` (definition of mex) and never `k` itself. (3) A position of a *single pile of Nim*
with `k` stones has Grundy number `k`. (4) **Sprague–Grundy theorem:** the Grundy number of a
*sum* of games (positions `s₁, …, sₙ`, a move plays in exactly one component) is
`g(s₁) ⊕ … ⊕ g(sₙ)`.

*Proof of (4).* Each component behaves like a Nim pile of size `g(sᵢ)`: from it you can move to
any smaller "size" (fact 2) — this is what Bouton's argument needs — and possibly to *larger*
sizes, but a move to a larger Grundy value can be answered by moving that same component back
to the original value (fact 2 again), so it never helps the mover. The xor-zero positions are
therefore exactly the P positions, by the same induction as Bouton. ∎

Consequence: to solve any impartial game that splits into independent parts, compute `g` for
a single part and xor. The mex computation costs O(#moves) per state:

```cpp
int mex(vector<int> v) { sort(v.begin(), v.end()); int m = 0; for (int x : v) { if (x == m) m++; else if (x > m) break; } return m; }

vector<int> grundySubtraction(int N, const vector<int>& S) {   // one pile, remove any s in S
    vector<int> g(N + 1, 0);
    for (int n = 1; n <= N; n++) { vector<int> o; for (int s : S) if (s <= n) o.push_back(g[n - s]); g[n] = mex(o); }
    return g;
}
```

Trace for `S = {1, 3, 4}`:

```
n :  0 1 2 3 4 5 6 7 8 9 ...
g :  0 1 0 1 2 3 2 0 1 0 ...      period 7: 0 1 0 1 2 3 2
     n=4: options g[3],g[1],g[0] = {1,1,0} -> mex 2
     n=5: options g[4],g[2],g[1] = {2,0,1} -> mex 3
     n=7: options g[6],g[4],g[3] = {2,2,1} -> mex 0   (P position)
```

Two piles `5` and `6`: `g = 3 ⊕ 2 = 1 ≠ 0` → N. Winning move: make the xor 0 → change pile
`5` (g=3) to a position with `g = 2`: `5 − 1 = 4` (g[4]=2). ✓

**"Nim Game II"** (remove 1–3 from one pile): `g(n) = n mod 4`, xor over piles.
**"Grundy's Game"**: split a pile into two *unequal* non-empty piles; a pile of size ≤ 2 is
dead. `g(n) = mex{ g(a) ⊕ g(n−a) : 1 ≤ a < n−a }`, O(n²). The values are famously irregular
(no proven period); the P-positions below a few thousand are a short list that you can *find
by running the O(n²) code*, and the CSES constraints are designed around that observation
— compute the table for small `n`, observe that no new P-position appears past a point, and
answer accordingly. (Do this yourself; the point of the task is the experiment.)

**Games on grids / boards.** Grundy numbers extend to any finite impartial game; when the
position is a board with independent regions (a broken chocolate bar, a row of coins with
gaps), split into regions and xor. When it does *not* split, fall back to the win/lose DP over
the whole state (Section 3).

---

## 3. Game DP on graphs: retrograde analysis with draws

Position = node, moves = out-edges, a node with no out-edges loses for the mover. On a DAG
this is the memoised DP. **With cycles, there are draws**, and recursion no longer works (it
loops). Retrograde analysis: propagate *backwards* from the terminal losers.

```cpp
enum Result { DRAW = 0, WIN = 1, LOSE = 2 };
vector<int> retrograde(const vector<vector<int>>& adj) {
    int n = adj.size(); vector<vector<int>> radj(n); vector<int> deg(n), res(n, DRAW), q;
    for (int u = 0; u < n; u++) { deg[u] = adj[u].size(); for (int v : adj[u]) radj[v].push_back(u); }
    for (int u = 0; u < n; u++) if (deg[u] == 0) { res[u] = LOSE; q.push_back(u); }
    for (size_t i = 0; i < q.size(); i++) {
        int v = q[i];
        for (int u : radj[v]) {
            if (res[u] != DRAW) continue;
            if (res[v] == LOSE) { res[u] = WIN; q.push_back(u); }        // one losing successor suffices
            else if (--deg[u] == 0) { res[u] = LOSE; q.push_back(u); }   // all successors are winning
        }
    }
    return res;
}
```

**Invariant.** A node is labelled WIN as soon as *one* successor is known LOSE; labelled LOSE
when the count of *unresolved* successors hits zero and none of them was LOSE (all WIN). Nodes
never labelled are DRAW: every option leads to a WIN-for-opponent or to another unresolved
node, and the opponent can keep it that way forever. O(V + E).

```
  0 -> 1 -> 3 (terminal)      3: LOSE (no moves)          4: WIN  (can move to 3)
  0 -> 2 -> 3                 1,2: WIN (move to LOSE)     5: LOSE (only move is to 4 = WIN)
  4 -> 3, 4 <-> 5             0: LOSE (all moves to WIN)  7 <-> 8 with no exits: both DRAW
```

Extensions: **"who wins with a token on each of several nodes"** — if the components are
independent, Grundy numbers on the DAG (`g(u) = mex g(v)`), xor. **Partisan games** (players
have different moves, e.g. chess-like) — same retrograde idea with two node types (whose turn),
plus the value when both aim at different goals. **Distance-to-win / shortest forced mate** —
record the BFS layer at which each node got labelled.

### 3.1 Minimax and alpha-beta (scored games)

When the game has a *score* instead of win/lose (CSES "Removal Game": take from either end,
maximise your total), the DP is `best(l, r) = max(a[l] − best(l+1, r), a[r] − best(l, r−1))`
in "difference" form, O(n²). For explicit game trees with heuristic evaluation (AI-style),
alpha-beta pruning keeps a window `[α, β]` and stops exploring a node once its value cannot
influence the parent; with good move ordering it visits ~`b^(d/2)` instead of `b^d` nodes. In
contests it appears in IOI-style "play against the judge" tasks; the skeleton is in
`example.cpp` (`alphabeta`) and it is asserted to return exactly the minimax value.

---

## 4. Constructive problems — a method, not a trick

"Print any array/permutation/grid/tree satisfying X, or −1." No algorithm to apply; you must
*invent* an object and *prove* it works. The method:

1. **Brute force small cases** (n ≤ 8) in 5 minutes. Look at *all* valid answers, not one:
   the pattern is usually visible in the lexicographically smallest one or in the counts.
2. **Look for an obstruction** when the brute force says "impossible" for some n: parity, a
   sum invariant, pigeonhole, a divisibility. If you can't state the obstruction, you probably
   have the impossible cases wrong.
3. **Extremal principle**: start with the largest / smallest element; put it where it has the
   fewest conflicts. Many constructions are "sort, then interleave" or "pair the ends".
4. **Build greedily and prove the invariant** that the partial object can always be extended.
5. **Verify with a checker** — a 20-line function that tests the output for the stated
   property, run on all n ≤ 12 before submitting. Constructive WAs are almost always in the
   special cases n = 1, 2, 3.

Four CSES Introductory tasks are constructions or closed formulas; each is verified against
brute force in `example.cpp`:

- **Gray Code** (`2205`): `g(i) = i ⊕ (i >> 1)`. Adjacent codes differ in one bit because
  `g(i) ⊕ g(i+1) = (i ⊕ (i+1)) ⊕ ((i ⊕ (i+1)) >> 1)`; `i ⊕ (i+1)` is a block of ones
  `2^{k+1} − 1`, and a block of ones xor its shift is the single bit `2^k`. The recursive
  view: list for `n−1`, then the same list reversed with a leading 1.
- **Permutations** (`1070`): adjacent elements must not differ by 1. Evens ascending then
  odds ascending: within each half differences are 2, at the seam `n − 1` (the largest even
  and 1): impossible only for n = 2, 3 (n = 1 is trivially fine). The brute force tells you
  the exceptions; the invariant ("all evens before all odds") tells you why the rest works.
- **Two Knights** (`1072`): count placements of two knights on a `k×k` board that do not
  attack. Total pairs `C(k², 2)`; attacking pairs: every `2×3` and `3×2` sub-rectangle
  contains exactly 2 attacking pairs and there are `(k−1)(k−2)` of each orientation, so
  subtract `4(k−1)(k−2)`. Formula found by computing the answer for k ≤ 6 by brute force and
  fitting — a legitimate contest technique when the answer is clearly polynomial.
- **Number Spiral** (`1071`): layer `m = max(x, y)` holds `(m−1)²+1 … m²`; even layers run
  along row `m` left-to-right then up column `m`, odd layers the other way. Two cases, O(1).

Common construction patterns worth memorising: **permutation with a given number of
inversions** (reverse a prefix, then fix the remainder), **permutation with LIS = a and LDS =
b** (blocks of decreasing runs; impossible iff `a·b < n` or `a + b > n + 1`), **complete
tournament schedule** (round-robin by rotating a circle), **tiling** (dominos/trominoes by
induction on 2×3 blocks), **grid with distinct row/column sums** (arithmetic progressions),
**graph with given degree sequence** (Havel–Hakimi), **bracket sequences / balanced strings**
(prefix-balance invariant).

---

## 5. Interactive problems

The judge is a program on the other end of a pipe. Rules that are never optional:

1. **Flush after every query**: `cout << "? " << x << endl;` (`endl` flushes) or
   `cout << ... << '\n' << flush;`. Without a flush the judge never sees the query and you get
   *Idleness limit exceeded*. Never use `ios::sync_with_stdio(false)` together with
   `printf`/`scanf` mixing; pick one I/O family.
2. **Read the response before the next query**, and stop immediately on a `−1` / error token.
3. **Respect the query budget exactly**: binary search on `[1, 1e9]` needs 30 queries, not
   31 — check your loop's worst case with a counter (the skeleton in `example.cpp` asserts
   `≤ 21` for a range of 1e6).
4. **Assume an adaptive judge** unless told otherwise: the secret may change consistently
   with all answers so far. A strategy that only works "with high probability against a fixed
   secret" fails; a strategy whose query set does not depend on luck does not care.
5. **Write a local judge** (a function `ask(x)` with a hidden secret) and test end-to-end —
   the interactive skeleton in `example.cpp` shows the pattern: pass the oracle as a
   `std::function`, so the same solver runs against your mock and against `cin/cout`.

Typical shapes: **binary search on a hidden number** (CSES "Hidden Integer"); **find a
permutation with comparison / "is x before y" queries** (sorting with n log n comparisons, or
finding max/min with n−1); **k-th element with rank queries**; **parity / xor queries to recover
bits**; **"guess the string, judge says number of matching positions"**. Information-theoretic
lower bound: if there are `M` possible secrets and each answer has `r` outcomes, you need at
least `log_r M` queries — compute this first to know whether your plan can fit the budget.

---

## 6. Randomized algorithms in contests

Randomness buys simplicity or beats adversarial tests; you must be able to say what the
failure probability is.

- **Seed** from the clock: `mt19937_64 rng(chrono::steady_clock::now().time_since_epoch().count());`.
  A fixed seed is an anti-hash test waiting to happen; `rand()` is 15-bit on some judges.
- **Random pivot** (quicksort/quickselect): expected O(n) / O(n log n) *for every input*; no
  adversarial case exists because the input does not know your pivot. `std::sort` is already
  introsort; `nth_element` is fine; the pitfall is writing your own quicksort with a fixed pivot.
- **Randomized hashing of sets — Zobrist.** Assign each value a random 64-bit key; the hash
  of a set is the xor of its members' keys. Insert/erase is O(1) (`h ^= key[v]`), set equality
  is hash equality with false-positive probability `2⁻⁶⁴` per comparison. **Multisets**: use
  *sum* of keys (mod 2⁶⁴) so duplicates count; "is subarray `[l, r]` a permutation of
  `[l', r']`" becomes prefix-sum equality. "Is the multiset of colours in this subtree equal to
  that one" — xor/sum over the Euler tour.
- **Probabilistic verification — Freivalds.** To check `A·B = C` for `n×n` matrices in O(n²):
  pick a random vector `r`, compare `A(Br)` with `Cr`. A wrong `C` passes with probability
  ≤ 1/2 per round over a field of size ≥ 2 (Schwartz–Zippel: a non-zero polynomial of degree
  1 has few roots); 30 rounds → `2⁻³⁰`. Same principle: **polynomial identity testing** by
  evaluating at random points modulo a large prime (string hashing *is* this), and checking
  "are these two multisets equal" via `Π (x − aᵢ)` at a random `x`.
- **Random restarts / random order**: when a greedy depends on the processing order and you
  cannot prove which order is right, shuffle and repeat while time remains; keep the best.
  Also: "pick a random element; with probability ≥ 1/2 it belongs to the majority/answer set"
  → repeat 30 times (CF "Ghd", "Kazaee").
- **Expected-time accounting**: report the *expected* running time and make sure the *worst*
  case still fits with a `while (clock() < limit)` guard when restarts are involved.

---

## 7. Hashing tricks beyond strings

| Need | Trick |
|---|---|
| Equality of two sets under insert/erase | Zobrist xor of random 64-bit keys |
| Equality of multisets, subarray-is-permutation | sum of random keys (mod 2⁶⁴) with prefix sums |
| Does the tree/subtree structure repeat (isomorphism) | AHU canonical form, or hash = f(sorted child hashes) with random per-depth salts |
| Rolling structure over a sliding window | maintain xor/sum; the window is a set/multiset difference |
| Detect a repeated state in a simulation (cycle) | hash the state, `unordered_set` — Floyd/Brent if memory matters |
| Avoid `unordered_map` blow-up on anti-hash tests | custom hash with splitmix64 and a random seed, or `gp_hash_table`/sorted vector |

Collision probabilities are per comparison; with `q` comparisons the total is about
`q / 2⁶⁴` — fine for anything a contest can throw at you as long as the keys are
truly 64-bit random, not `rand() % 1e9`.

---

## 8. Meet in the middle (recap)

Split the choices into two halves of `n/2`, enumerate each half (`2^{n/2}` items), sort one
side, and combine with binary search / two pointers: `O(2^{n/2} · n)`. `n = 40 → 2²⁰ ≈ 1e6`
per side. Canonical: subset sum count (`example.cpp` `countSubsetSum`, CSES "Meet in the
Middle"), 4-SUM in O(n² log n), "minimum |sum − target|", bidirectional BFS for shortest
transformation sequences (`b^{d/2}` each side). Memory is the constraint: `2²⁰` `long long`s is
8 MB — fine; `2²⁴` is 128 MB — not.

---

## 9. Heuristics, pruning and partial scores (IOI-style)

An IOI task has 3–7 subtasks worth a total of 100; the full solution is often 2–3 ideas
stacked. Points come from banking the small ones:

1. **Read every subtask's constraints before designing anything.** The constraints *are* the
   hints: `n ≤ 20` → bitmask; `n ≤ 2000` → O(n²); "all values distinct" → a special structure;
   "the graph is a line" → the full problem with an array.
2. **Write the brute force first** (it also becomes your stress-tester), submit it, bank the
   subtask. Then improve. A 30-point brute force submitted in 20 minutes beats a 100-point idea
   that is 80% implemented at the end.
3. **Pruning in exhaustive search** (`../02_complete_search_and_backtracking`): feasibility
   bounds ("even if every remaining step is perfect we cannot beat the best"), symmetry
   (fix the first choice), order the branching to fail early, memoise states. CSES "Grid Path
   Description" (`1625`) is the model: from 1e12 paths to under a second with three prunings.
4. **Output-only tasks**: the inputs are given; you may run anything for as long as you like,
   including randomised hill-climbing / simulated annealing with restarts, and hand-tune per
   input. Score is usually relative to the best known solution — every improvement is points.
5. **Heuristics for large subtasks you cannot solve exactly**: greedy + local improvement,
   randomised order + take-the-best, beam search. Always check them against the brute force
   on the small subtasks so you at least know where they break.

---

## 10. Offline vs online

**Offline**: all queries are known in advance — sort them (by right endpoint, by time, by
value), answer in the convenient order, reorder the answers at the end (Mo's algorithm,
"process queries by increasing k", sweep line, divide and conquer over time, offline dynamic
connectivity). **Online**: the next query depends on the previous answer (interactive, or
"the answer is xored with the last answer" — a statement trick that forbids offline
processing). When a statement bothers to say "queries must be answered online", the offline
approach would have been too easy — expect a persistent structure, a `set`-based dynamic
hull, or a sqrt decomposition.

---

## 11. Invariants and monovariants — the proof toolbox

You are stuck proving that a process terminates, that a greedy is optimal, or that a
configuration is unreachable. Try, in this order:

| Tool | Statement pattern | Example |
|---|---|---|
| **Parity / colouring** | "can we reach…" → no | domino tiling of a board with two opposite corners removed; knight returns to a same-colour square only after an even number of moves |
| **Sum / weighted sum invariant** | a quantity never changes under any move | `Σ aᵢ mod k`, xor of piles (Nim!), number of inversions mod 2 under swaps |
| **Monovariant** | a quantity strictly decreases/increases, bounded → termination | potential `Σ aᵢ²` in "replace two numbers by their average"; number of unsorted pairs in bubble sort |
| **Extremal principle** | look at the max/min/first/last element | the largest element of a permutation must be at an end in "adjacent elements differ by ≤ 1" |
| **Pigeonhole** | `n+1` objects in `n` boxes | among `n+1` numbers in `[1, 2n]` two are coprime |
| **Exchange argument** | greedy optimality | swap two adjacent choices, show the objective does not get worse (see `../../algorithms_learning/08_greedy_and_intervals`) |
| **Induction on a well-chosen parameter** | constructions | Gray code, tromino tilings, tournament schedules |
| **Symmetry / strategy stealing** | who wins without knowing how | in Chomp / Hex the first player wins; mirror strategies in symmetric positions |

Write the invariant down in one sentence *before* coding the greedy. If you cannot, expect a
WA on test 7.

---

## 12. Common contest math

- **Floor/ceil**: `⌈a/b⌉ = ⌊(a + b − 1)/b⌋` for positive `a, b`; for negatives use the
  `floorDiv` in `example.cpp` (C++ `/` truncates toward zero). `⌊⌊a/b⌋/c⌋ = ⌊a/(bc)⌋`.
  Number of multiples of `k` in `[l, r]` = `⌊r/k⌋ − ⌊(l−1)/k⌋`.
- **Sums**: `Σi = n(n+1)/2`, `Σi² = n(n+1)(2n+1)/6`, `Σi³ = (n(n+1)/2)²`, geometric
  `Σ_{i<n} rⁱ = (rⁿ − 1)/(r − 1)`. Compute `n(n+1)/2` as `n/2·(n+1)` or `(n+1)/2·n` depending
  on parity to dodge overflow when `n ~ 4e9`.
- **Harmonic sums**: `Σ_{i=1}^{n} n/i ≈ n ln n`. Hence the "for each `i`, for each multiple of
  `i` up to `n`" double loop is O(n log n), and so is "for each `d`, iterate blocks of size `d`".
  Sieve of Eratosthenes is O(n log log n) — the same argument summed over primes only.
- **Number of distinct values of `⌊n/i⌋`** is O(√n) → iterate `i` in blocks where the
  quotient is constant (`j = n / (n / i)` is the last index of the block).
- **Euler's formula** for connected planar graphs: `V − E + F = 2`; hence a simple planar graph
  has `E ≤ 3V − 6` (so sparse), and a grid of `n` axis-parallel segments creates
  `V − E + F = 2` faces relations that let you count regions from intersections.
- **Counting lattice points under a line**, **gcd/lcm identities**, **Legendre's formula**
  (`v_p(n!) = Σ ⌊n/pᵏ⌋`) — see the number theory chapter.
- **Expected value linearity** — for "expected number of X" sum indicator probabilities; no
  independence needed.
- **Stars and bars**: `k` identical items into `n` boxes: `C(n+k−1, k)`.

---

## 13. IOI-specific

**Format.** Two competition days, 5 hours each, 3 tasks per day, each worth 100 points split
into subtasks (usually 3–6). Partial credit is per subtask — all tests in a subtask must pass.
Full feedback on submissions (you see the subtask scores) with a submission limit (50–100 per
task historically). Languages: C++ (everyone), occasionally others. Scores of ~400–600 / 600
medal; the cut for gold is usually a full solve of 4–5 problems plus partials.

**Task types.**
- *Batch* (standard): read input, print output. Most tasks.
- *Interactive* (function-based): you implement functions the grader calls, and you call
  grader functions to query. Examples you should study: IOI 2014 "Game" (answer edge queries
  while keeping the graph's connectivity undetermined as long as possible), IOI 2015 "Scales"
  (weighing queries), IOI 2018 "Combo" (guess a string with prefix-count feedback), IOI 2016
  "Messy" (restricted insert/query interface).
- *Communication*: two separate programs (or two calls of your code on different inputs) must
  agree on an encoding: IOI 2011 "Parrots" (encode a message into a multiset of numbers),
  IOI 2020 "Stations" (label a tree so that routing works from labels alone).
- *Constructive with a checker*: IOI 2020 "Connecting Supertrees" (build a graph with given
  path counts), IOI 2019 "Vision" (build a boolean circuit).
- *Output-only / open-ended* (rarer now): IOI 2013 "Art Class" (classify images; heuristic,
  scored by accuracy) is the classic example.

**Reading an IOI statement.** (1) Skim the story, then read the *Implementation details*
section carefully — the function signatures and the exact meaning of each parameter are the
task. (2) Read *all* subtasks; note which ones are "free" (tiny n), which require a special
structure, and which is the intended full solution. (3) Read the sample grader / sample
interaction and make sure you can run it locally before writing any solution code. (4) Note
the limits on the *number of calls* in interactive tasks — they are part of the scoring.

**Time allocation for 3 problems in 5 hours** (a plan that works for a typical finalist):

```
0:00–0:20   read all three statements fully; write down the subtask table for each; rank by expected points/hour
0:20–1:00   bank the easy subtasks on all three (brute force, small-n special cases) — 60–100 points on the board
1:00–3:00   the problem you rated best: build up subtask by subtask, submit after each
3:00–4:15   second problem: same
4:15–4:45   third problem: whatever partial is reachable; or return to the best remaining subtask
4:45–5:00   no new ideas — re-read outputs, check for TLE on the largest subtask, re-submit best versions
```

Never spend more than 90 minutes without a submission. If an idea has been "almost working"
for 45 minutes, write the brute force for the next subtask instead and come back. Track your
score on paper.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| piles, remove any amount from one pile, last move wins | Nim xor (Bouton) | O(n) |
| same, last move loses | misère rule (all ≤ 1 ⇒ parity) | O(n) |
| moves from stair k to k−1 / coins move left along a row | staircase Nim on odd-distance positions | O(n) |
| one pile, fixed set of removals, n ≤ 1e6 | win/lose DP; Grundy periodicity for huge n | O(n·|S|) |
| weird move set but independent components | Grundy numbers via mex, xor | O(states · moves) |
| game on a graph, "may last forever" | retrograde analysis, in-degree counting, DRAW | O(V + E) |
| take-from-either-end with scores | interval DP in difference form | O(n²) |
| "construct any … or −1" | small-case brute force → invariant → greedy + checker | — |
| "you may ask at most k queries" | interactive: flush, budget check, information bound | log-many queries |
| "are the multisets / sets equal after each update" | random-key xor/sum hashing | O(1) per update |
| adversarial input to pivot/hash | randomize pivot / seed from clock | expected O(n log n) |
| verify a product / identity cheaply | Freivalds / random evaluation (Schwartz–Zippel) | O(n²) per round |
| n ≤ 40 subsets, n ≤ 20 on each side | meet in the middle | O(2^{n/2} n) |
| subtasks with n ≤ 20 / n ≤ 2000 / special structure | brute force first, bank points, then generalise | — |
| "queries must be answered online" | persistent / dynamic structures, no sorting of queries | — |

## Implementation checklist for contests

- [ ] Game: the *terminal* position's label is right (normal play: no move ⇒ LOSE; misère: opposite).
- [ ] Grundy: `mex` over *all* reachable values, including moves that reach a larger Grundy value; xor combined only over *independent* components.
- [ ] Retrograde: out-degree counter decremented only when the successor is WIN; DRAW is the default for untouched nodes.
- [ ] Constructive: checker function written and run for all n ≤ 12; n = 1, 2, 3 handled; the impossible cases proved, not guessed.
- [ ] Interactive: `endl`/`flush` after every query; exit on error token; query counter asserted against the budget; tested against a local mock judge.
- [ ] Randomized: `mt19937_64` seeded from the clock; failure probability written down; worst-case time still fits.
- [ ] Hashing: 64-bit random keys, not `rand()`; multisets use sum, sets use xor.
- [ ] Meet in the middle: memory of `2^{n/2}` elements computed; the split is balanced.
- [ ] IOI: sample grader compiles and runs locally; the *number of grader calls* is within the limit for the largest subtask; each subtask submitted separately.
- [ ] Contest math: `n(n+1)/2` overflow checked; floor division of negatives handled; `long long` everywhere a product appears.

## Further reading

- CPH ch. 25 "Game theory" (Nim, Sprague–Grundy, Grundy's game), ch. 5 "Complete search"
  (meet in the middle, pruning), ch. 19 "Paths and circuits" for the Euler-formula remark.
- cp-algorithms.com: "Sprague-Grundy theorem. Nim", "Games on arbitrary graphs",
  "Schwartz–Zippel lemma" (under Probability), "Zobrist hashing" (in "String Hashing"
  neighbours), "Meet in the middle".
- Bouton, C. L. (1901), "Nim, a game with a complete mathematical theory", *Annals of
  Mathematics* — the original 3-page proof.
- Sprague (1935) / Grundy (1939) — the theorem; Berlekamp, Conway, Guy, *Winning Ways for
  Your Mathematical Plays* — the full theory including partisan games.
- Codeforces blog "Interactive problems: guide for participants" (the official one) for the
  I/O protocol details.
- IOI syllabus (ioinformatics.org) — the official list of what may appear; and the IOI
  archive of past tasks with graders (ioi.te.lv / oj.uz) for the task-type examples above.

## You can move on when...

- You can prove Bouton's theorem and the Sprague–Grundy theorem on a whiteboard in five
  minutes each, and explain why staircase Nim only counts odd stairs.
- You can write `retrograde` from memory and it agrees with the DAG recursion on random graphs.
- You have solved all six CSES game tasks plus "Removal Game", and the Introductory
  constructions (Permutations, Number Spiral, Two Knights, Gray Code, Two Sets) each in under
  15 minutes with a checker.
- You have solved at least three CSES interactive tasks with a local mock judge and one CF
  interactive problem live in a contest without an idleness verdict.
- You can state the failure probability of a Zobrist-hash equality test and of 20 rounds of
  Freivalds, and you know when a fixed random seed is unsafe.
- You have run a full 5-hour mock with three IOI tasks, following the time plan above, and
  reviewed where the points were lost.
