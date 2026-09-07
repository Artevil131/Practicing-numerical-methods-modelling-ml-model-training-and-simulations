# Chapter 08 — Greedy & intervals

## What you'll be able to do after this chapter

- Recognise the six greedy shapes in this chapter from the wording of a problem: sort-then-scan, exchange-argument comparator, interval merge/select, heap-driven "best available", reach/coverage layers, and the case where greedy is a trap and you need DP.
- Write the **exchange argument** for a greedy rule in three sentences, and derive the `qsort` comparator directly from the inequality it produces.
- Implement the interval toolkit in C — merge, activity selection, group counting, two-sorted-list intersection, covered-interval removal — with structs sorted by `qsort` and one running variable.
- Drive a greedy with a min-/max-heap of `{key, payload}` when the candidate set changes between rounds, and know why a single pre-sort is not enough there.
- Maintain a single `farthest` variable for reachability and the `currentEnd`/`farthest` pair for minimum-steps ("BFS without a queue").
- Break a wrong greedy with a 5-element counterexample, and cross-check any greedy against brute force before trusting it.

## Why this matters for ML / numerics / sims

Greedy algorithms are the engine of most scheduling and resource-allocation code that surrounds a numerical pipeline. A data loader that packs variable-length sequences into fixed-size batches is "sort by length, then fill the budget" — the sort-then-scan greedy. Interval merging is how you coalesce overlapping time ranges of sensor readings, overlapping bounding boxes on a line, or overlapping memory ranges in a custom allocator. Activity selection by earliest finish time is the correctness core of any single-resource scheduler — a GPU stream, a shared bus in a hardware sim. The heap-driven greedy is literally the event loop of a discrete-event simulation: sort events by arrival, keep a priority queue of what is ready, always take the best one. Reach/coverage greedy is BFS on an implicit graph: how far can the particle get with the moves seen so far, how many refuelling stops, how many taps to water a garden — the same shape as "how many kernel launches to cover a range". And the last unit is the most important for a numerics person: greedy is a *local* rule, DP is a *global* search with memory — Viterbi decoding, CTC alignment, edit distance and beam search all exist because the greedy "take the best token now" fails. Knowing *when* a local choice is provably safe (the exchange argument) is the difference between a fast algorithm and a fast wrong answer.

Everything here is `O(n log n)` from a `qsort` plus one `O(n)` pass, or `O(n log n)` from `n` heap operations. The C you need is Chapter 10's `qsort` comparator and binary heap (`../../c_learning/10_data_structures/lesson.md`), structs (`../../c_learning/07_structs_unions_enums/lesson.md`), and careful index arithmetic (`../../c_learning/12_numbers_bits_floats/lesson.md`).

---

## 1. Sort-then-scan greedy

### The idea

The simplest greedy is mechanical: **sort the input by some key, then walk it once, making an irrevocable local choice at every step.** The rule is usually so obvious it needs no separate proof — but you should still be able to justify it.

**Why sorting helps.** Once the input is in order, the future consequences of a greedy choice become predictable: you no longer need to compare against all remaining options, you only need to look at the *next* element in the sequence. Sorting converts a 2-D question ("which of the remaining items?") into a 1-D one ("does the next item fit?").

**The template.** Sort by key `k` (size, value, deviation, ...). Keep one or two running variables (e.g. "budget left", "last boundary used"). For each element do the same check-and-update.

**Invariant.** After processing the first `i` sorted elements, the running variables describe an optimal (or feasible-and-optimal-so-far) partial answer for exactly those `i` elements, and nothing that comes later can improve a decision already made — because everything later is at least as large (or small) as what you have seen.

**Complexity.** Almost always `O(n log n)`, dominated by the sort; the scan itself is `O(n)`. If the sort is unnecessary (input already ordered, or the "key" is just a running simulation as in a change-making counter), the whole thing drops to `O(n)` with `O(1)` extra space.

### When to recognise it

- "Maximise the number of X you can satisfy / fit / take" with a single capacity or budget.
- Two sorted sequences to be matched smallest-to-smallest (two pointers after two sorts).
- "Make every element strictly larger than the previous with minimum total increments" — sort first, then force `prev+1`.
- A running simulation where the greedy rule is "spend the least flexible resource first" (give the 10-bill before three 5-bills).
- A one-pass counter that only cares about *direction changes*, not magnitudes (zigzag/wiggle counting).
- "Partition into as many pieces as possible so that each symbol lives in one piece" — precompute last occurrence, extend `end`, close the piece the moment `i == end`.

The key question in these problems is never "does greedy work" but **"which key do we sort by, and in which direction?"** The wrong key (ascending vs descending, value vs ratio, count vs units) is the most common error, and it produces a plausible-looking wrong answer rather than an obvious crash.

### C data structures and memory layout

- A plain `int *` or `long long *` array, sorted in place with `qsort`. If you need to sort *pairs* (e.g. `{units, count}`), define a `struct` and sort the array of structs; sorting a parallel index array (argsort) is the alternative when the payload is large.
- One or two scalars for state. No allocation beyond the array itself.
- For the "last occurrence" variant on lowercase strings: `int last[26]`, fill with `-1`, one pass `last[s[i]-'a'] = i`.

```c
#include <stdlib.h>

static int cmp_int_asc(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);              /* never `return x - y`: overflows */
}

/* Greedy fill: how many items fit in `budget` if we always take the cheapest next?
   Sort ascending, walk once, stop at the first that does not fit. O(n log n). */
static int greedy_fill(int *cost, int n, long long budget) {
    qsort(cost, (size_t)n, sizeof cost[0], cmp_int_asc);
    int taken = 0;
    for (int i = 0; i < n && cost[i] <= budget; i++) {
        budget -= cost[i];                 /* irrevocable: never revisit i */
        taken++;
    }
    return taken;
}
```

The two-pointer variant (match each demand to the smallest sufficient supply) has the same shape: sort both arrays, `i` over demands, `j` over supplies, advance `j` always and `i` only on a successful match.

### Worked mini-example

`cost = [7, 2, 9, 4, 3]`, `budget = 12`.

```
sorted:  [2, 3, 4, 7, 9]
i=0  cost 2 <= 12  take   budget=10  taken=1
i=1  cost 3 <= 10  take   budget=7   taken=2
i=2  cost 4 <= 7   take   budget=3   taken=3
i=3  cost 7 >  3   stop                -> answer 3
```

Exchange argument in one line: if an optimal answer ever skips a cheaper item to take a more expensive one, swapping them keeps the budget feasible and does not reduce the count — so taking cheapest-first is never worse.

### Pitfalls

- Forgetting to sort *both* arrays in a two-pointer matching. Without both sorted the pointer walk is meaningless.
- Sorting by the wrong key: by *count* of boxes instead of *units per box*; by `costA` instead of `costA - costB`.
- Leftover budget/`k` that still matters: after flipping all negatives, an odd remaining `k` flips the smallest-magnitude element; an even remainder is a no-op.
- In "make unique with minimum increments", processing in the *original* order instead of sorted order — then a later smaller value could have needed less lifting had it gone first.
- In "partition labels", updating `end` only at the start of a piece instead of at *every* character.

**Python equivalent:** `sorted(xs)` then a `for` loop with a running variable; `collections.Counter` for the frequency table you would write as `int cnt[26]` in C.

---

## 2. Exchange-argument greedy

### The idea

When the right greedy rule is *not* obvious — what is the correct sort key, what is the right comparison function — you need the **exchange argument**, the standard proof that a greedy is optimal.

**Structure of the proof.** Let `O` be some optimal solution. If `O` violates the greedy rule somewhere, show that two *adjacent* choices in `O` can be **swapped** so that (a) the solution stays valid and (b) its value does not get worse. Repeating the swap turns `O`, one step at a time, into the solution the greedy rule produces, without the value ever decreasing — so the greedy solution is optimal too.

**In practice** this shows up as *deriving the comparator*: swapping two adjacent elements `a, b` is beneficial (or neutral) exactly when some inequality `f(a,b) <= f(b,a)` holds — and that inequality *is* the sort key. Once you have written the inequality, the comparator is a transcription.

**Concatenation order** (forming the largest number from pieces) is the classic case: compare two elements **joined both ways** (`a+b` vs `b+a`), not their values in isolation — only the joined comparison reveals the right order for this problem. Since `a+b` and `b+a` have the same length as strings, numeric comparison of the concatenations *is* plain `strcmp`.

### When to recognise it

- "Order the items to maximise/minimise a total" where the natural key (value, size) gives wrong answers on small cases.
- A cost that depends on a *difference* or *ratio* between two attributes per item (`costA - costB`, `wage/quality`, `p/w`).
- Digit manipulation where you must decide *which* position to alter: fix the highest place value minimally, then maximise everything after it (monotone digits, maximum swap).
- "Reconstruct an order where each element states how many larger/earlier elements precede it" — insert tallest first, since later shorter insertions never disturb earlier `k` counts.
- Lexicographically smallest result with a stack: pop the top while it is larger than the current symbol **and** will appear again later.
- Constraints that go *both* directions along an array (each child vs left neighbour *and* right neighbour): two passes, combine with `max`.

### Deriving a comparator: weighted completion time

Jobs `(p_i, w_i)` (processing time, weight) run one after another; minimise `sum_i w_i * C_i` where `C_i` is finish time. Swap adjacent `a` then `b` versus `b` then `a`. Only their two terms change:

```
a first:  w_a * p_a + w_b * (p_a + p_b)
b first:  w_b * p_b + w_a * (p_a + p_b)
a-first is no worse  <=>  w_b * p_a <= w_a * p_b  <=>  p_a / w_a <= p_b / w_b
```

So sort ascending by `p/w` (Smith's rule). The comparator compares cross-products to avoid floating point:

```c
typedef struct { long long p, w; } Job;

/* a before b  <=>  p_a * w_b <= p_b * w_a.  Cross-multiply: no division, no doubles. */
static int cmp_job(const void *x, const void *y) {
    const Job *a = x, *b = y;
    long long lhs = a->p * b->w, rhs = b->p * a->w;    /* watch overflow: |p|,|w| < 2^31 */
    return (lhs > rhs) - (lhs < rhs);
}

/* Concatenation comparator: a before b  <=>  strcmp(a+b, b+a) > 0. Both are <= 2*L chars. */
static int cmp_concat_desc(const void *x, const void *y) {
    const char *a = *(const char *const *)x, *b = *(const char *const *)y;
    char ab[64], ba[64];
    snprintf(ab, sizeof ab, "%s%s", a, b);
    snprintf(ba, sizeof ba, "%s%s", b, a);
    return strcmp(ba, ab);                               /* reversed: larger concatenation first */
}
```

### Worked mini-example

Jobs `(p,w)`: `A=(3,1) B=(1,2) C=(2,2) D=(4,4)`. Ratios `p/w`: `A=3.0 B=0.5 C=1.0 D=1.0`.

```
sorted by p/w:  B(0.5)  C(1.0)  D(1.0)  A(3.0)      (C/D tie: either order, same cost)
finish times:   C_B=1   C_C=3   C_D=7   C_A=10
weighted sum:   2*1 + 2*3 + 4*7 + 1*10 = 46
naive "shortest first" (B C A D):  1*2 + 3*2 + 6*1 + 10*4 = 54   <- worse
```

Check the swap: `C` then `D` gives `2*2 + 4*(2+4) = 28`; `D` then `C` gives `4*4 + 2*(4+2) = 28`. Equal, as the tie predicts.

### Pitfalls

- Sorting by one attribute alone when the cost depends on the *difference* or *ratio*.
- Comparing numbers as numbers (or as bare strings) when the objective is the concatenation.
- Right-to-left problems done left-to-right: a decision made early can be invalidated by a violation you have not seen yet (monotone digits).
- Taking the *first* occurrence of a larger digit instead of the *last* — the last is the one that cannot break an intermediate larger digit.
- Forgetting the "appears again later" check when popping a stack — the letter is lost forever.
- One pass instead of two for a two-sided neighbour constraint, or overwriting instead of `max` in the second pass.
- A comparator that is not a strict weak ordering (e.g. mixed `<` with `<=` on ties) — `qsort` behaviour becomes undefined.

**Python equivalent:** `sorted(xs, key=cmp_to_key(f))` from `functools`; C has only the comparator, no `key=` shortcut, so you always write the three-way function.

---

## 3. Interval scheduling & merging

### The idea

In interval problems the first step is almost always **sorting** — by start or by end — after which one pass suffices.

**Merging.** Sort by start. Keep the "current merged interval"; if the next interval starts before or exactly when the current one ends, extend the current (`end = max(end, next.end)`); otherwise close the current and start a new one.

**Selection (scheduling).** Sort by **end** instead: always pick the next interval that starts after the previously picked one finished. This maximises the number picked, because the interval that ends earliest leaves the most room for the rest.

**Groups and arrows** (burst balloons with arrows) use the same end-time order but count *how many groups are needed* rather than how many intervals fit.

**Intersections between two separate lists** use two pointers with no sort, because both lists are already sorted.

**Common principle.** Once all intervals are sorted by the right key, every future decision depends on **one** running variable (the end of the last chosen/merged interval), not on the whole history. That single variable is the whole state.

### Recognising it and choosing the key

| Question | Sort key | Running variable | Step |
|---|---|---|---|
| Merge overlapping | start asc | `cur.end` | overlap if `next.start <= cur.end` -> extend, else emit |
| Max non-overlapping (or min removals) | end asc | `lastEnd` | keep if `start >= lastEnd` |
| Min points/arrows to hit all | end asc | `arrowPos` | new arrow if `start > arrowPos` |
| Remove covered | start asc, end **desc** on tie | `maxEnd` | covered if `end <= maxEnd` |
| Insert into sorted disjoint list | (already sorted) | `newStart,newEnd` | three phases: before / overlap / after |
| Intersect two sorted lists | (already sorted) | `i, j` | emit `[max(s), min(e)]` if nonempty; advance the one ending first |
| Online booking, no double-book | (list of accepted) | — | non-overlap iff `newEnd <= s || newStart >= e` |

### C data structures and memory layout

```c
typedef struct { int s, e; } Interval;                 /* 8 bytes, contiguous array, cache friendly */

static int cmp_by_start(const void *a, const void *b) {
    const Interval *x = a, *y = b;
    if (x->s != y->s) return (x->s > y->s) - (x->s < y->s);
    return (y->e > x->e) - (y->e < x->e);              /* tie: longer first (end descending) */
}
static int cmp_by_end(const void *a, const void *b) {
    const Interval *x = a, *y = b;
    return (x->e > y->e) - (x->e < y->e);
}

/* Merge in place: returns the new count; out[0..m) holds the merged intervals. */
static int merge_intervals(Interval *v, int n) {
    if (n == 0) return 0;
    qsort(v, (size_t)n, sizeof v[0], cmp_by_start);
    int m = 0;                                          /* v[0..m] is the merged prefix */
    for (int i = 1; i < n; i++) {
        if (v[i].s <= v[m].e) { if (v[i].e > v[m].e) v[m].e = v[i].e; }
        else                   v[++m] = v[i];
    }
    return m + 1;
}

/* Activity selection: max number of pairwise non-overlapping intervals. */
static int max_non_overlapping(Interval *v, int n) {
    qsort(v, (size_t)n, sizeof v[0], cmp_by_end);
    int kept = 0; long long last_end = LLONG_MIN;
    for (int i = 0; i < n; i++)
        if (v[i].s >= last_end) { kept++; last_end = v[i].e; }
    return kept;                                        /* removals = n - kept */
}
```

Merging in place works because the write index `m` never overtakes the read index `i`. The result for LeetCode-style `int**` outputs is then a matter of copying `m+1` structs into whatever shape the harness wants.

### Worked mini-example: merge

`[1,3] [8,10] [2,6] [15,18] [9,12]`

```
sorted by start:  [1,3] [2,6] [8,10] [9,12] [15,18]
m=0 cur=[1,3]
i=1 [2,6]:   2 <= 3   overlap  -> cur.e = max(3,6)  = 6     cur=[1,6]
i=2 [8,10]:  8 >  6   emit     -> m=1 cur=[8,10]
i=3 [9,12]:  9 <= 10  overlap  -> cur.e = max(10,12)= 12    cur=[8,12]
i=4 [15,18]: 15 > 12  emit     -> m=2 cur=[15,18]
result: [1,6] [8,12] [15,18]     (3 intervals)
```

### Worked mini-example: selection by end

Same five intervals, sorted by end: `[1,3] [2,6] [8,10] [9,12] [15,18]`.

```
lastEnd=-inf  [1,3]:   1 >= -inf keep   lastEnd=3    kept=1
              [2,6]:   2 <  3    drop
              [8,10]:  8 >= 3    keep   lastEnd=10   kept=2
              [9,12]:  9 <  10   drop
              [15,18]: 15 >= 10  keep   lastEnd=18   kept=3
```

Sorting by *start* instead fails on `[1,100] [2,3] [4,5]`: start-order keeps `[1,100]` and drops both small ones (1 kept), end-order keeps `[2,3] [4,5]` (2 kept).

### Pitfalls

- Comparing `next.start` against `cur.start` instead of `cur.end`. Overlap is about the *end*.
- Sorting by start when the task is selection — the counterexample above.
- Boundary semantics: touching intervals `[1,3] [3,5]` — do they overlap? For merging on LeetCode, yes (`<=`); for "arrows", equal ends share one arrow, so a new arrow is needed only when `start > arrowPos` (strict). For calendar booking with half-open `[s,e)`, non-overlap is `newEnd <= s || newStart >= e`. Decide the semantics *before* writing the inequality.
- Forgetting the tie-break in "remove covered": with equal starts, the longer one must come first or the shorter looks uncovered.
- Re-sorting an input that is already sorted (insert interval) — wastes the structure you were given.
- In two-list intersection, advancing both pointers, or the wrong one. Advance the interval that **ends first**: it can produce no further intersections; the longer one may still intersect the next interval on the other side.

**Python equivalent:** `intervals.sort(key=lambda x: x[1])` — in C the same `key` becomes a comparator on the struct field.

---

## 4. Greedy with a heap

### The idea

When the greedy choice is "take the best of the remaining options" and the option set **changes dynamically** between rounds, a single pre-sort is not enough — you need a **heap** (priority queue) that keeps "the best remaining" available in `O(log n)` after every update.

**The template.** Sort the events (e.g. by arrival time) and walk them forward in time. At every moment the heap contains all options *currently available*, and the greedy choice is always the heap top. Two phases per step: **admit** everything that has become available (push), then **choose** (pop).

**Where the heap is essential.**
- Task scheduling with a cooldown: the heap holds the remaining count of each task type; always run the most frequent one that is allowed.
- Attending events with deadlines: the heap holds all currently open events keyed by deadline; attend the one expiring soonest.
- Hiring `k` workers with a wage/quality ratio: the heap holds the `k` smallest qualities in a sliding window over ratio order.
- Refuelling: a max-heap of *skipped* stations, popped lazily only when you get stuck — "retroactive" greedy.
- IPO: two heaps — a min-heap on capital requirement to admit projects, a max-heap on profit to choose.

**Why plain sorting is not enough.** One greedy pass works only when "the best option" stays the same throughout or changes predictably. When the best option depends on what has *already been chosen* (your capital grew, some tasks cooled down, new tasks arrived), the heap maintains this shifting "bestness" efficiently without re-sorting the whole remaining set at every step.

**Complexity.** `O(n log n)` for the sort plus `n` pushes and pops at `O(log n)` each. When the key space is tiny (26 letters), the heap is `O(log 26)` and effectively constant — a sorted array of 26 counters would do.

### C data structures and memory layout

Chapter 10's binary heap, but of a struct so the payload rides along with the key:

```c
typedef struct { long long key; int payload; } HItem;    /* 16 bytes with padding */
typedef struct { HItem *a; int len, cap; } MinHeap;      /* array-backed, a[0] is the min */

static void heap_swap(HItem *x, HItem *y) { HItem t = *x; *x = *y; *y = t; }

static void heap_push(MinHeap *h, HItem it) {            /* caller guarantees len < cap */
    int i = h->len++;
    h->a[i] = it;
    while (i > 0 && h->a[(i - 1) / 2].key > h->a[i].key) {
        heap_swap(&h->a[i], &h->a[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}
static HItem heap_pop(MinHeap *h) {                      /* caller guarantees len > 0 */
    HItem top = h->a[0];
    h->a[0] = h->a[--h->len];
    for (int i = 0;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->len && h->a[l].key < h->a[m].key) m = l;
        if (r < h->len && h->a[r].key < h->a[m].key) m = r;
        if (m == i) break;
        heap_swap(&h->a[i], &h->a[m]); i = m;
    }
    return top;
}
```

A **max**-heap is the same code with the comparisons flipped — or push `-key` into the min-heap (careful with `LLONG_MIN`). For ties that must be broken by a second field (duration, then original index) store both in the struct and compare lexicographically inside the sift loops instead of on `.key` alone. Allocate `cap = n` up front with `malloc`; the heap never holds more than `n` items in these problems.

### Worked mini-example: shortest available task first

Tasks `(arrival, duration)`: `T0=(0,5) T1=(1,2) T2=(2,1) T3=(8,3) T4=(9,1)`. One CPU. Admit everything that has arrived, then pop the shortest.

```
t=0   admit T0            heap {T0:5}          pop T0  run 0..5
t=5   admit T1,T2 (arrived by 5)   heap {T2:1, T1:2}   pop T2  run 5..6
t=6   nothing new         heap {T1:2}          pop T1  run 6..8
t=8   admit T3            heap {T3:3}          pop T3  run 8..11
t=11  admit T4            heap {T4:1}          pop T4  run 11..12
order: T0 T2 T1 T3 T4
```

At `t=5` a plain pre-sort by duration would have picked `T2` fine — but at `t=0` it would have wanted `T2` or `T4` first, which have not arrived. The heap only ever contains what is *admissible now*. Note the idle-time rule: if the heap is empty but tasks remain, jump `t` straight to the next arrival.

### Pitfalls

- Not admitting *all* newly arrived items before the next pop — a shorter task that arrived during the last run must be in the heap before you choose.
- Counting idle time wrongly: an idle unit is spent only when the heap is empty **and** the schedule is not finished (cooldown queue non-empty).
- Missing the impossibility pre-check (a letter more frequent than `ceil(n/2)` cannot be reorganised).
- Using bricks (the cheap resource) greedily up front instead of deferring the decision to the heap — the min-heap of ladder-jumps lets you *retroactively* swap the smallest one to bricks when a bigger gap appears.
- Refuelling at every station instead of lazily popping the largest skipped one only when stuck.
- IPO: forgetting to re-admit newly affordable projects after each pick — capital grew.
- Hiring: computing cost from the *average* ratio instead of the group's **maximum** ratio.

**Python equivalent:** `heapq.heappush(h, (key, payload))` / `heappop`. Python compares tuples lexicographically for free; in C you write the tie-break into the sift comparisons.

---

## 5. Reach/jump greedy

### The idea

In reachability problems the greedy rule is typically: **maintain the farthest reach possible so far, and check at every step whether it is still enough.**

**Maintaining the reach.** `farthest` = the largest index reachable using only the elements already processed. At each step `farthest = max(farthest, i + jump[i])`. If `farthest` ever falls behind the current index, the target is unreachable.

**Minimum number of steps** adds layer processing: keep the boundary of the current "layer" `currentEnd`; when `i` reaches it, the `farthest` seen so far defines the next layer — structurally this is **BFS without an explicit queue**. The step counter increases exactly when `i == currentEnd`, not on every `farthest` update.

**The circular version** maintains a running net sum: if the total over the whole loop is negative, there is no solution; otherwise the starting point is found in one pass, because the very point after which the running sum first turns negative reveals that no earlier start up to that point could have worked.

**Common trait.** In all of these, one running "best seen so far" variable replaces a full search, because later information never makes a previously rejected option viable again.

### When to recognise it

- "Can you reach the last index / how few jumps" from an array of jump lengths.
- "Minimum clips/taps/segments to cover `[0, T]`" — each item is an interval, coverage extends the reach; identical to min-jumps after converting each tap `i` with range `r` into `[max(0, i-r), i+r]` and building `farthest[x]` = max right edge of any interval starting at or before `x`.
- Circular route with gains and costs — total sum test, then one pass with a `start` candidate.
- Reversed simulation: when the forward greedy is unclear ("double or subtract?"), reverse it from the target (`even -> halve`, `odd -> +1`); the operation with exponential effect becomes obviously preferable.
- Step-sum with parity: grow `n` until `1+...+n >= |target|` *and* the parity matches; the difference can always be realised by flipping one step.

### C data structures and memory layout

No structure beyond the input array and two or three `long long` scalars. For coverage problems build one extra `int farthest_at[T+1]` array (`O(T)` memory) so the scan is over positions, not intervals. For the circular sum use `long long tank, total` — sums of `n` ints can overflow `int`.

```c
/* Min jumps to reach index n-1 (each a[i] >= 0 is the max jump from i). Returns -1 if unreachable. */
static int min_jumps(const int *a, int n) {
    int jumps = 0;
    long long current_end = 0, farthest = 0;             /* long long: i + a[i] may exceed INT_MAX */
    for (int i = 0; i < n - 1; i++) {                    /* n-1: we never need to jump FROM the last index */
        if (i + (long long)a[i] > farthest) farthest = i + (long long)a[i];
        if (i == current_end) {                          /* end of the current layer: must jump */
            if (farthest <= i) return -1;                /* no progress possible: stuck */
            jumps++;
            current_end = farthest;
            if (current_end >= n - 1) break;
        }
    }
    return jumps;
}
```

### Worked mini-example

`a = [1, 3, 0, 0, 4, 1, 0]`, `n = 7`, target index 6.

```
i   a[i]  i+a[i]  farthest  i==currentEnd?             jumps  currentEnd
0   1     1       1         yes (0)  -> jump           1      1
1   3     4       4         yes (1)  -> jump           2      4
2   0     2       4         no
3   0     3       4         no
4   4     8       8         yes (4)  -> jump           3      8 >= 6  stop
answer: 3 jumps   (0 -> 1 -> 4 -> 6)
```

Layers: `{0}`, `{1}`, `{2,3,4}`, `{5,6,...}` — the same layers BFS would produce; `jumps` is the depth. Reachability alone is the same loop without `currentEnd`: fail as soon as `i > farthest`.

Circular net sum on deltas `d = gas - cost = [-2, 3, -1, -3, 4]`: total `= 1 >= 0`, so a start exists.

```
i   d    tank   action
0   -2   -2     <0 -> start=1, tank=0
1    3    3
2   -1    2
3   -3   -1     <0 -> start=4, tank=0
4    4    4
start = 4   (check: 4, 2, 5, 4, 1 -- never negative)
```

### Pitfalls

- Incrementing the jump counter on every `farthest` update instead of exactly when `i == currentEnd`.
- Running the min-jumps loop to `i == n-1` and counting a phantom jump from the last index.
- Picking the *first* clip that reaches past `lastEnd` instead of waiting to pick the one that reaches farthest.
- Negative left edges in the tap conversion — clamp to 0 or you index `farthest_at[-3]`.
- Trying every start in the circular problem: `O(n^2)` works but the one-pass argument makes it `O(n)`.
- Greedy in the *forward* direction for the broken calculator — the local choice is not clear there; reverse it.
- Forgetting the parity check in reach-a-number and returning the first `n` whose sum passes the target.
- Full DP for jump game: correct but `O(n^2)` worst case, when one variable does it in `O(n)`.

**Python equivalent:** none special — but note Python's `int` never overflows; `i + a[i]` in C can, hence `long long`.

---

## 6. Where greedy fails, DP is needed

### The idea

Every greedy so far worked because the local choice never excluded a better future — the exchange argument could be proven. In this unit a local choice **can** exclude the better solution, and so you need DP, which keeps all alternatives open and never commits prematurely.

**The recognition test.** Ask: *"If I greedily take the option that looks best right now, can a later choice ever make this one bad?"* If yes, greedy does not work.

**Coins with a non-canonical set** is the classic: taking the largest coin fixes the remaining amount into something that can no longer be filled efficiently — coins `{1,3,4}`, amount `6`: greedy gives `4+1+1` (three coins), optimum is `3+3` (two).

**House-robber-type problems:** "always take the larger/more valuable option" fails because a locally worse choice can open access to *two* later large values the greedy would have blocked.

**Games where you pick from the ends:** "always take the larger end" ignores what the opponent does next.

DP fixes this by trying (or memoising the result of) **all** first choices, not just the one that looks best now — exhaustive search with memory, instead of premature commitment.

### The catalogue of traps in this unit

| Problem shape | Tempting greedy | Why it fails | DP that works |
|---|---|---|---|
| Non-adjacent max sum | take largest, skip neighbours | one big value kills two good neighbours | `dp[i] = max(dp[i-1], dp[i-2] + a[i])` |
| Min coins to make `x` | largest coin first | leftover amount becomes inefficient | `dp[x] = 1 + min_c dp[x-c]` (unbounded knapsack) |
| Min perfect squares summing to `n` | largest square first | same as coins | `dp[n] = 1 + min_j dp[n - j*j]` |
| Split into two equal-sum halves | largest first, fill a bin | 0/1 items lock; fractional greedy assumption | `dp[s] |= dp[s - a]`, `s` descending (0/1 knapsack) |
| Longest increasing subsequence | extend whenever larger | commits to a bad starting value | `dp[i] = 1 + max dp[j]` for `a[j] < a[i]`, or patience `tails[]` |
| Segment string into dictionary words | longest match first (maximal munch) | long prefix leaves unbreakable suffix | `dp[i] = OR_j (dp[j] && s[j..i) in dict)` |
| Two-player pick from ends | take larger end | ignores opponent's reply | `dp[i][j] = max(a[i] - dp[i+1][j], a[j] - dp[i][j-1])` |
| Burst balloons for `left*cur*right` | largest immediate score | bursting changes future neighbours | interval DP on the balloon burst **last** in `(i,j)` |

### How to test a greedy before trusting it

You cannot run an exchange argument in your head for every problem. The engineering shortcut:

1. Write the greedy.
2. Write the dumbest exhaustive search you can (`2^n` subsets, all permutations, full recursion) for `n <= 8`.
3. Loop over a few thousand random tiny instances; print the first mismatch.

If a mismatch appears you have your counterexample and a proof that greedy is wrong; if none appears after many trials you still owe a proof, but you can proceed. `example.c` includes this harness for 0/1 knapsack.

```c
/* Skeleton: 1-D DP where each state depends on a bounded set of earlier states. */
static long long best_non_adjacent(const int *a, int n) {
    long long take_prev2 = 0, take_prev1 = 0;            /* dp[i-2], dp[i-1] rolled into two scalars */
    for (int i = 0; i < n; i++) {
        long long cur = take_prev1;                      /* skip i */
        if (take_prev2 + a[i] > cur) cur = take_prev2 + a[i];   /* or take i, then i-1 is forbidden */
        take_prev2 = take_prev1;
        take_prev1 = cur;
    }
    return take_prev1;
}
```

### Worked mini-example

Coins `{1,3,4}`, amount `6`.

```
greedy:  6 -> take 4 -> 2 -> take 1 -> 1 -> take 1 -> 0     3 coins
DP:      dp[0]=0 dp[1]=1 dp[2]=2 dp[3]=1 dp[4]=1 dp[5]=2 dp[6]=min(dp[5],dp[3],dp[2])+1 = 2
         dp[6] = 2  via 3+3
```

Euro coins `{1,2,5,10,20,50}` happen to make greedy optimal because the set was *designed* so that this commitment problem never arises — you cannot assume that without proof.

### Pitfalls

- Assuming a greedy is right because it passes the sample inputs. The traps above all pass the samples.
- Writing the 0/1 knapsack inner loop over `s` **ascending** — that turns it into the unbounded knapsack (an item can be reused).
- Thinking "the answer is bounded by 4" (Lagrange, perfect squares) tells you *which* squares — it does not.
- Minimax with the wrong sign convention: `dp[i][j]` is the *score difference* for the player to move; each option subtracts the opponent's best from the remaining range.
- Burst balloons: reasoning about which balloon to burst *first* (neighbours change) instead of *last* (neighbours are the fixed borders `i,j`).

**Python equivalent:** `functools.lru_cache` on a recursive function is the memoised form of every table above. In C you allocate the table (`calloc`) and fill it bottom-up; there is no decorator.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Sort key / structure | Complexity |
|---|---|---|---|
| "maximise number satisfied", one budget/capacity | sort-then-scan | ascending by cost/need | `O(n log n)` |
| two lists matched smallest-to-smallest | sort both + two pointers | ascending both | `O(n log n)` |
| "make change", "cooldown", one pass simulation | greedy counter simulation | none | `O(n)` |
| "reorder to maximise total", cost depends on pair order | exchange argument -> comparator | `f(a,b) <= f(b,a)` | `O(n log n)` |
| "form the largest/smallest number by ordering pieces" | concatenation comparator | `strcmp(b+a, a+b)` | `O(n L log n)` |
| "highest place value first", digits | right-to-left digit greedy | last-occurrence table `int last[10]` | `O(d)` |
| "lexicographically smallest, keep one of each" | monotonic stack + last occurrence | `int last[26]`, `char stack[]` | `O(n)` |
| constraint vs both neighbours | two passes + `max` | none | `O(n)` |
| "merge overlapping" | interval merge | start asc | `O(n log n)` |
| "max non-overlapping" / "min removals" | activity selection | end asc | `O(n log n)` |
| "min arrows/points to hit all" | group counting | end asc, strict `>` | `O(n log n)` |
| "remove covered" | covered scan | start asc, end desc on tie | `O(n log n)` |
| two already-sorted lists, "intersection" | two pointers, advance earliest end | none | `O(n + m)` |
| "book if no overlap", online | linear/tree overlap test | list or BST | `O(n)` / `O(log n)` per op |
| "best available at each time", options arrive/expire | sort by arrival + heap | min-heap on the choice key | `O(n log n)` |
| "cooldown", "alternate most frequent" | max-heap of counts | 26 counters or heap | `O(n log k)` |
| "defer the choice until stuck" | lazy greedy with heap of skipped | max-heap | `O(n log n)` |
| `k`-window over ratio order | sort by ratio + max-heap of size `k` | heap | `O(n log n)` |
| "can you reach", "min jumps" | `farthest` / `currentEnd` layers | none | `O(n)` |
| "min intervals to cover `[0,T]`" | reach layers over positions | `farthest_at[T+1]` | `O(n + T)` |
| circular route, gains and costs | total test + one pass `start` | none | `O(n)` |
| "double or subtract", forward unclear | reverse simulation | none | `O(log target)` |
| coins/squares/words/subsets, greedy "obviously" works | **stop** — test against brute force | DP table | `O(n * amount)` etc. |
| two players alternate, pick from ends | interval DP minimax | `dp[i][j]` | `O(n^2)` |

---

## Gotchas in C specifically

- **`qsort` comparator overflow.** `return a - b` on `int` overflows for large magnitudes; on `long long` cross-products it is worse. Always `(x > y) - (x < y)`.
- **Cross-multiplication overflow.** `p_a * w_b` with values up to `10^9` needs `long long` (`10^18 < 9.2 * 10^18`, fine); with values up to `10^10` it does not fit. Know your bounds or fall back to `long double` with an epsilon.
- **`qsort` is not stable.** "Sort by height descending, then by `k` ascending" must be encoded *inside one comparator*. Two consecutive `qsort` calls do not compose.
- **Concatenation comparator buffers.** `snprintf` into a fixed stack buffer sized for `2 * max_len + 1`; or precompute each string's length and compare with `memcmp` in a loop without building the concatenation. Never `strcat` into the source strings.
- **Sorting an array of `char *` vs an array of `char[]`.** For `char *strs[]` the comparator receives `const char *const *` and you dereference once. Getting this cast wrong compiles and crashes.
- **Half-open vs closed intervals.** `[s, e)` non-overlap is `newEnd <= s || newStart >= e`; closed `[s, e]` overlap is `next.s <= cur.e`; arrows need strict `>`. Write the boundary case `[1,3] [3,5]` as your first test.
- **Right-to-left loops with `size_t`.** `for (size_t i = n - 1; i >= 0; i--)` never terminates. Use `for (size_t i = n; i-- > 0;)` or `int`/`ptrdiff_t` indices. Candy, monotone digits and the "insert interval" third phase all walk backwards or hold a second index.
- **`i + a[i]` overflow** in reach problems when `a[i]` can be `INT_MAX`. Compute in `long long`.
- **Sums of `n` ints.** Weighted sums, `gas - cost` totals, candy totals: `long long`.
- **Heap of structs.** Copy the struct through `HItem t = *x` — do not swap `.key` alone and forget the payload. Check `len < cap` on push; `malloc(n * sizeof(HItem))` up front.
- **Max-heap via negation.** `-LLONG_MIN` is undefined behaviour. Flip the comparisons instead.
- **Digit strings.** `snprintf(buf, sizeof buf, "%d", num)` to get digits; `buf[i] - '0'` to get values; `strtol` back. Do not do decimal arithmetic on `char`.
- **`int last[26]` indexing.** `s[i] - 'a'` only for guaranteed lowercase; anything else needs `(unsigned char)s[i]` and a 256-entry table.
- **`abs(INT_MIN)`** is undefined. In reach-a-number take `target = llabs((long long)target)`.
- **Hash table for word-break dictionary.** There is no `set` in C: use Chapter 10's open-addressing string table, or sort the dictionary and `bsearch` each substring (copy it into a stack buffer with a terminating `'\0'` first).
- **DP tables.** `calloc((size_t)amount + 1, sizeof(int))`; initialise "infinity" as `INT_MAX / 2` so that `dp[x-c] + 1` cannot overflow; `free` at the end. For 2-D interval DP, one flat `n*n` block with `dp[i * n + j]` indexing.
- **Recursion depth.** The problems in this chapter are iterative; if you memoise the games recursively, depth is `O(n)` — fine for `n <= 10^4`, not for `10^6`.

---

## Common mistakes checklist

- [ ] The sort key and direction are justified by a one-line exchange argument, not by "it looked right".
- [ ] Both arrays sorted before a two-pointer match.
- [ ] Comparator returns `(x > y) - (x < y)`; ties handled inside the same comparator.
- [ ] Interval overlap compares against the running **end**, with the boundary semantics (`<=` vs `<`) decided from the problem statement and tested on touching intervals.
- [ ] Selection problems sort by **end**, merge problems by **start**, covered-removal by start asc / end desc.
- [ ] Two-list intersection advances the pointer whose interval **ends first**.
- [ ] Heap greedy admits *all* newly available items before every pop, and jumps time forward when the heap is empty.
- [ ] Lazy/retroactive heap greedy defers the choice (refuel, ladders) until it is forced.
- [ ] `farthest`/`currentEnd`: counter increments only when `i == currentEnd`; loop stops before the last index; `long long` for `i + a[i]`.
- [ ] Left edges clamped to 0 before indexing a position array.
- [ ] Circular problems: total-sum feasibility test first, then one pass.
- [ ] Any "obviously greedy" coin/subset/word/game problem is cross-checked against brute force on tiny inputs before trusting it.
- [ ] 0/1 knapsack inner loop runs `s` **descending**.
- [ ] No `size_t` reverse loops with `>= 0`.
- [ ] Every `malloc`/`calloc` for heaps and DP tables is `free`d; run under `-fsanitize=address`.

---

## You can move on when...

- You can state the exchange argument for activity selection (earliest end first) and for Smith's rule, and turn each into a `qsort` comparator without floating point.
- You can write interval merge, activity selection and arrows-counting from memory, with the correct `<=`/`>` at the boundaries, and explain the `[1,100] [2,3] [4,5]` counterexample to start-sorting.
- You can implement the two-list intersection pointer walk and say why only the earlier-ending pointer advances.
- You can write the `{key, payload}` min-heap and use it for "admit arrived, pop shortest" event simulation, including the idle-time jump.
- You can write `min_jumps` with `currentEnd`/`farthest` and explain why it is BFS, and convert taps/clips into the same loop.
- You can produce, from memory, a counterexample where largest-coin-first fails and where "take the biggest house" fails, and write the DP recurrence that fixes each.
- You have a brute-force cross-check harness you reuse whenever you are tempted to trust a greedy.

Next: `problems.md` in this folder — 45 problems in six units, each with a hint, the key idea, and an approach behind collapsible sections. Work them in C in `solutions/`.
