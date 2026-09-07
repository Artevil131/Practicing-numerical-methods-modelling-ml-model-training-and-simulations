# Chapter 08 — Greedy & intervals — Python

## What you'll be able to do after this chapter

- Recognise the six greedy shapes in this chapter from the wording of a problem: sort-then-scan, exchange-argument comparator, interval merge/select, heap-driven "best available", reach/coverage layers, and the case where greedy is a trap and you need DP.
- Write the **exchange argument** for a greedy rule in three sentences, and derive the `sorted(key=...)` (or `functools.cmp_to_key`) comparator directly from the inequality it produces.
- Implement the interval toolkit in Python — merge, activity selection, group counting, two-sorted-list intersection, covered-interval removal — with tuples/lists sorted by `sort(key=...)` and one running variable.
- Drive a greedy with a min-/max-heap (`heapq`) of `(key, payload)` when the candidate set changes between rounds, and know why a single pre-sort is not enough there.
- Maintain a single `farthest` variable for reachability and the `current_end`/`farthest` pair for minimum-steps ("BFS without a queue").
- Break a wrong greedy with a 5-element counterexample, and cross-check any greedy against brute force before trusting it.

## Why this matters for ML / numerics / sims

Greedy algorithms are the engine of most scheduling and resource-allocation code that surrounds a numerical pipeline. A data loader that packs variable-length sequences into fixed-size batches is "sort by length, then fill the budget" — the sort-then-scan greedy. Interval merging is how you coalesce overlapping time ranges of sensor readings, overlapping bounding boxes on a line, or overlapping memory ranges in a custom allocator. Activity selection by earliest finish time is the correctness core of any single-resource scheduler — a GPU stream, a shared bus in a hardware sim. The heap-driven greedy is literally the event loop of a discrete-event simulation: sort events by arrival, keep a priority queue of what is ready, always take the best one. Reach/coverage greedy is BFS on an implicit graph: how far can the particle get with the moves seen so far, how many refuelling stops, how many taps to water a garden — the same shape as "how many kernel launches to cover a range". And the last unit is the most important for a numerics person: greedy is a *local* rule, DP is a *global* search with memory — Viterbi decoding, CTC alignment, edit distance and beam search all exist because the greedy "take the best token now" fails. Knowing *when* a local choice is provably safe (the exchange argument) is the difference between a fast algorithm and a fast wrong answer.

**Python vs C, once for the chapter:** `list.sort(key=...)` (or `sorted(...)`) replaces `qsort` and its comparator entirely for single-key sorts; `functools.cmp_to_key` is the escape hatch for the rare case where the ordering genuinely needs a pairwise comparison (concatenation order) rather than a key function. `heapq` replaces the hand-rolled array-backed binary heap — tuples compare lexicographically, so `(key, payload)` "just works" as a heap item with no sift-loop tie-break code to write. Python ints never overflow, so the entire "cross-multiply instead of divide", "`long long` for `i + a[i]`", and "`-LLONG_MIN` is UB" story disappears. `sort` is stable in Python (Timsort), unlike C's `qsort` — "sort by X then by Y" can be two separate stable sorts in reverse priority order, or one key tuple `(x, y)`, whichever reads better.

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

### Python data structures

- A plain `list[int]`, sorted in place with `.sort()`. If you need to sort *pairs* (e.g. `(units, count)`), sort a list of tuples — Python compares tuples lexicographically, so `list.sort()` alone often replaces a hand-written comparator.
- One or two scalars for state. No allocation beyond the list itself.
- For the "last occurrence" variant on lowercase strings: `last = {}` (or `[-1] * 26`), one pass `last[c] = i`.

```python
def greedy_fill(cost: list[int], budget: int) -> int:
    """How many items fit in `budget` if we always take the cheapest next?
    Sort ascending, walk once, stop at the first that does not fit. O(n log n)."""
    cost = sorted(cost)
    taken = 0
    for c in cost:
        if c > budget:
            break                      # irrevocable: never revisit
        budget -= c
        taken += 1
    return taken
```

The two-pointer variant (match each demand to the smallest sufficient supply) has the same shape: sort both lists, `i` over demands, `j` over supplies, advance `j` always and `i` only on a successful match.

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

- Forgetting to sort *both* lists in a two-pointer matching. Without both sorted the pointer walk is meaningless.
- Sorting by the wrong key: by *count* of boxes instead of *units per box*; by `cost_a` instead of `cost_a - cost_b`.
- Leftover budget/`k` that still matters: after flipping all negatives, an odd remaining `k` flips the smallest-magnitude element; an even remainder is a no-op.
- In "make unique with minimum increments", processing in the *original* order instead of sorted order — then a later smaller value could have needed less lifting had it gone first.
- In "partition labels", updating `end` only at the start of a piece instead of at *every* character.

---

## 2. Exchange-argument greedy

### The idea

When the right greedy rule is *not* obvious — what is the correct sort key, what is the right comparison function — you need the **exchange argument**, the standard proof that a greedy is optimal.

**Structure of the proof.** Let `O` be some optimal solution. If `O` violates the greedy rule somewhere, show that two *adjacent* choices in `O` can be **swapped** so that (a) the solution stays valid and (b) its value does not get worse. Repeating the swap turns `O`, one step at a time, into the solution the greedy rule produces, without the value ever decreasing — so the greedy solution is optimal too.

**In practice** this shows up as *deriving the comparator*: swapping two adjacent elements `a, b` is beneficial (or neutral) exactly when some inequality `f(a,b) <= f(b,a)` holds — and that inequality *is* the sort key. Once you have written the inequality, the comparator is a transcription.

**Concatenation order** (forming the largest number from pieces) is the classic case: compare two elements **joined both ways** (`a+b` vs `b+a`), not their values in isolation — only the joined comparison reveals the right order for this problem. Since `a+b` and `b+a` have the same length as strings, this comparison is a plain string comparison, and Python gives you `functools.cmp_to_key` to turn it directly into a sort key.

### When to recognise it

- "Order the items to maximise/minimise a total" where the natural key (value, size) gives wrong answers on small cases.
- A cost that depends on a *difference* or *ratio* between two attributes per item (`cost_a - cost_b`, `wage/quality`, `p/w`).
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

So sort ascending by `p/w` (Smith's rule). Python needs no cross-multiplication trick to avoid floating point — ints never overflow — but comparing the ratio directly as `p / w` still risks float rounding on adversarial inputs, so a `key=` function returning a `fractions.Fraction(p, w)` (or the cross-product comparator below) is the robust choice:

```python
from functools import cmp_to_key

def cmp_job(a: tuple[int, int], b: tuple[int, int]) -> int:
    """a before b <=> p_a * w_b <= p_b * w_a. Cross-multiply: exact with Python's unbounded ints."""
    pa, wa = a
    pb, wb = b
    lhs, rhs = pa * wb, pb * wa
    return (lhs > rhs) - (lhs < rhs)

jobs_sorted = sorted(jobs, key=cmp_to_key(cmp_job))

def cmp_concat_desc(a: str, b: str) -> int:
    """a before b <=> a+b is lexicographically GREATER than b+a (largest number first)."""
    ab, ba = a + b, b + a
    return (ba > ab) - (ba < ab)          # reversed: larger concatenation sorts first

pieces_sorted = sorted(pieces, key=cmp_to_key(cmp_concat_desc))
```

`cmp_to_key` is the one place C's comparator style survives unchanged in Python — everywhere else, a plain `key=` function (which computes one sort key per element, not a pairwise comparison) is both simpler and asymptotically faster, since Python calls the key function once per element rather than `O(n log n)` times.

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
- A comparator passed to `cmp_to_key` that is not a strict weak ordering (e.g. inconsistent on ties) — sort behaviour becomes unreliable, same as an undefined `qsort` comparator in C.
- Reaching for `cmp_to_key` when a plain `key=` function would do — it is slower and usually a sign the comparator can be simplified to a key.

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
| Merge overlapping | start asc | `cur_end` | overlap if `next.start <= cur.end` -> extend, else emit |
| Max non-overlapping (or min removals) | end asc | `last_end` | keep if `start >= last_end` |
| Min points/arrows to hit all | end asc | `arrow_pos` | new arrow if `start > arrow_pos` |
| Remove covered | start asc, end **desc** on tie | `max_end` | covered if `end <= max_end` |
| Insert into sorted disjoint list | (already sorted) | `new_start,new_end` | three phases: before / overlap / after |
| Intersect two sorted lists | (already sorted) | `i, j` | emit `[max(s), min(e)]` if nonempty; advance the one ending first |
| Online booking, no double-book | (list of accepted) | -- | non-overlap iff `new_end <= s or new_start >= e` |

### Python data structures

```python
def merge_intervals(intervals: list[list[int]]) -> list[list[int]]:
    """Merge overlapping intervals. Returns a new merged list (does not mutate in place,
    unlike the C version, since Python lists don't benefit from the write-index trick here)."""
    if not intervals:
        return []
    intervals = sorted(intervals, key=lambda iv: iv[0])       # sort by start
    merged = [intervals[0][:]]
    for s, e in intervals[1:]:
        if s <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], e)
        else:
            merged.append([s, e])
    return merged


def max_non_overlapping(intervals: list[list[int]]) -> int:
    """Activity selection: max number of pairwise non-overlapping intervals."""
    intervals = sorted(intervals, key=lambda iv: iv[1])       # sort by end
    kept = 0
    last_end = float("-inf")
    for s, e in intervals:
        if s >= last_end:
            kept += 1
            last_end = e
    return kept                                                # removals = n - kept
```

Merging into a fresh `list` (rather than in place) is the idiomatic Python approach — there is no allocation cost to worry about the way there is with `malloc`, and it avoids the "does the write index ever overtake the read index" reasoning the C version needs. The result for LeetCode-style outputs is then just `merged` itself, no copying step required.

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
- Boundary semantics: touching intervals `[1,3] [3,5]` — do they overlap? For merging on LeetCode, yes (`<=`); for "arrows", equal ends share one arrow, so a new arrow is needed only when `start > arrow_pos` (strict). For calendar booking with half-open `[s,e)`, non-overlap is `new_end <= s or new_start >= e`. Decide the semantics *before* writing the inequality.
- Forgetting the tie-break in "remove covered": with equal starts, the longer one must come first or the shorter looks uncovered — sort key `(start, -end)` handles both in one tuple.
- Re-sorting an input that is already sorted (insert interval) — wastes the structure you were given.
- In two-list intersection, advancing both pointers, or the wrong one. Advance the interval that **ends first**: it can produce no further intersections; the longer one may still intersect the next interval on the other side.
- Mutating a list you are iterating with `for iv in intervals:` while also appending to `intervals` — iterate a snapshot or build a new list, as the merge skeleton above does.

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

**Complexity.** `O(n log n)` for the sort plus `n` pushes and pops at `O(log n)` each. When the key space is tiny (26 letters), the heap is `O(log 26)` and effectively constant — a sorted `list`/`Counter` of 26 counters would do just as well.

### Python data structures

`heapq` is a **min-heap only**, operating on a plain `list`. Tuples compare lexicographically for free, so `(key, payload)` is the whole "struct" — no manual sift-loop tie-breaking to write.

```python
import heapq

def shortest_available_first(tasks: list[tuple[int, int]]) -> list[int]:
    """tasks = [(arrival, duration), ...]. One CPU. Admit everything that has
    arrived, then run the shortest. Returns the order tasks are run in (by index)."""
    tasks_by_arrival = sorted(range(len(tasks)), key=lambda i: tasks[i][0])
    heap: list[tuple[int, int]] = []                 # (duration, index)
    order: list[int] = []
    t = 0
    ptr = 0
    n = len(tasks)
    while len(order) < n:
        if not heap and (ptr == n or tasks[tasks_by_arrival[ptr]][0] > t):
            t = tasks[tasks_by_arrival[ptr]][0]        # idle: jump to the next arrival
        while ptr < n and tasks[tasks_by_arrival[ptr]][0] <= t:
            idx = tasks_by_arrival[ptr]
            heapq.heappush(heap, (tasks[idx][1], idx))  # admit
            ptr += 1
        dur, idx = heapq.heappop(heap)                  # choose
        order.append(idx)
        t += dur
    return order
```

A **max**-heap is either negated keys (`heapq.heappush(h, (-key, payload))`, safe here since Python ints have no `LLONG_MIN`-style minimum to worry about) or the third-party `heapq`-adjacent trick of wrapping items in a class with a reversed `__lt__`. For ties broken by a second field, just extend the tuple: `(key, tiebreak, payload)` — Python compares element by element automatically.

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
- Missing the impossibility pre-check (a letter more frequent than `ceil(n/2)` cannot be reorganised) — `collections.Counter.most_common()` is the natural tool to find the offender.
- Using bricks (the cheap resource) greedily up front instead of deferring the decision to the heap — the min-heap of ladder-jumps lets you *retroactively* swap the smallest one to bricks when a bigger gap appears.
- Refuelling at every station instead of lazily popping the largest skipped one only when stuck.
- IPO: forgetting to re-admit newly affordable projects after each pick — capital grew.
- Hiring: computing cost from the *average* ratio instead of the group's **maximum** ratio.
- Forgetting `heapq` is min-only and pushing raw (non-negated) keys when you need a max-heap.

---

## 5. Reach/jump greedy

### The idea

In reachability problems the greedy rule is typically: **maintain the farthest reach possible so far, and check at every step whether it is still enough.**

**Maintaining the reach.** `farthest` = the largest index reachable using only the elements already processed. At each step `farthest = max(farthest, i + jump[i])`. If `farthest` ever falls behind the current index, the target is unreachable.

**Minimum number of steps** adds layer processing: keep the boundary of the current "layer" `current_end`; when `i` reaches it, the `farthest` seen so far defines the next layer — structurally this is **BFS without an explicit queue**. The step counter increases exactly when `i == current_end`, not on every `farthest` update.

**The circular version** maintains a running net sum: if the total over the whole loop is negative, there is no solution; otherwise the starting point is found in one pass, because the very point after which the running sum first turns negative reveals that no earlier start up to that point could have worked.

**Common trait.** In all of these, one running "best seen so far" variable replaces a full search, because later information never makes a previously rejected option viable again.

### When to recognise it

- "Can you reach the last index / how few jumps" from an array of jump lengths.
- "Minimum clips/taps/segments to cover `[0, T]`" — each item is an interval, coverage extends the reach; identical to min-jumps after converting each tap `i` with range `r` into `[max(0, i-r), i+r]` and building `farthest_at[x]` = max right edge of any interval starting at or before `x`.
- Circular route with gains and costs — total sum test, then one pass with a `start` candidate.
- Reversed simulation: when the forward greedy is unclear ("double or subtract?"), reverse it from the target (`even -> halve`, `odd -> +1`); the operation with exponential effect becomes obviously preferable.
- Step-sum with parity: grow `n` until `1+...+n >= abs(target)` *and* the parity matches; the difference can always be realised by flipping one step.

### Python data structures

No structure beyond the input list and two or three scalars — and no `long long` concerns, since Python ints never overflow regardless of how large `i + a[i]` gets.

```python
def min_jumps(a: list[int]) -> int:
    """Min jumps to reach the last index (each a[i] >= 0 is the max jump from i).
    Returns -1 if unreachable."""
    n = len(a)
    jumps = 0
    current_end = farthest = 0
    for i in range(n - 1):                      # never need to jump FROM the last index
        farthest = max(farthest, i + a[i])
        if i == current_end:                     # end of the current layer: must jump
            if farthest <= i:
                return -1                         # no progress possible: stuck
            jumps += 1
            current_end = farthest
            if current_end >= n - 1:
                break
    return jumps
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

Layers: `{0}`, `{1}`, `{2,3,4}`, `{5,6,...}` — the same layers BFS would produce; `jumps` is the depth. Reachability alone is the same loop without `current_end`: fail as soon as `i > farthest`.

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

- Incrementing the jump counter on every `farthest` update instead of exactly when `i == current_end`.
- Running the min-jumps loop to `i == n-1` and counting a phantom jump from the last index.
- Picking the *first* clip that reaches past `last_end` instead of waiting to pick the one that reaches farthest.
- Negative left edges in the tap conversion — clamp to 0 (`max(0, i - r)`) or you index `farthest_at[-3]`, which in Python silently wraps to the *end* of the list instead of raising — a much quieter bug than C's crash.
- Trying every start in the circular problem: `O(n^2)` works but the one-pass argument makes it `O(n)`.
- Greedy in the *forward* direction for the broken calculator — the local choice is not clear there; reverse it.
- Forgetting the parity check in reach-a-number and returning the first `n` whose sum passes the target.
- Full DP for jump game: correct but `O(n^2)` worst case, when one variable does it in `O(n)`.

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
| Split into two equal-sum halves | largest first, fill a bin | 0/1 items lock; fractional greedy assumption | `dp[s] = dp[s] or dp[s - a]`, `s` descending (0/1 knapsack) |
| Longest increasing subsequence | extend whenever larger | commits to a bad starting value | `dp[i] = 1 + max dp[j]` for `a[j] < a[i]`, or `bisect`-based `tails[]` |
| Segment string into dictionary words | longest match first (maximal munch) | long prefix leaves unbreakable suffix | `dp[i] = any(dp[j] and s[j:i] in words for j in ...)` |
| Two-player pick from ends | take larger end | ignores opponent's reply | `dp[i][j] = max(a[i] - dp[i+1][j], a[j] - dp[i][j-1])` |
| Burst balloons for `left*cur*right` | largest immediate score | bursting changes future neighbours | interval DP on the balloon burst **last** in `(i,j)` |

### How to test a greedy before trusting it

You cannot run an exchange argument in your head for every problem. The engineering shortcut:

1. Write the greedy.
2. Write the dumbest exhaustive search you can (`2^n` subsets via `itertools`, all permutations, full recursion) for `n <= 8`.
3. Loop over a few thousand random tiny instances (`random.randint`); print the first mismatch.

If a mismatch appears you have your counterexample and a proof that greedy is wrong; if none appears after many trials you still owe a proof, but you can proceed.

```python
def best_non_adjacent(a: list[int]) -> int:
    """dp[i-2], dp[i-1] rolled into two scalars."""
    take_prev2 = take_prev1 = 0
    for x in a:
        cur = max(take_prev1, take_prev2 + x)      # skip i, or take i (then i-1 forbidden)
        take_prev2, take_prev1 = take_prev1, cur
    return take_prev1
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
- Using `@functools.lru_cache` on a recursive brute-force cross-checker and forgetting to `cache_clear()` between independently-generated random test instances — stale results from a previous instance leak in.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Sort key / structure | Complexity |
|---|---|---|---|
| "maximise number satisfied", one budget/capacity | sort-then-scan | ascending by cost/need | `O(n log n)` |
| two lists matched smallest-to-smallest | sort both + two pointers | ascending both | `O(n log n)` |
| "make change", "cooldown", one pass simulation | greedy counter simulation | none | `O(n)` |
| "reorder to maximise total", cost depends on pair order | exchange argument -> comparator | `f(a,b) <= f(b,a)` | `O(n log n)` |
| "form the largest/smallest number by ordering pieces" | concatenation comparator (`cmp_to_key`) | `b+a` vs `a+b` | `O(n L log n)` |
| "highest place value first", digits | right-to-left digit greedy | last-occurrence table / dict | `O(d)` |
| "lexicographically smallest, keep one of each" | monotonic stack + last occurrence | `dict`, `list` as stack | `O(n)` |
| constraint vs both neighbours | two passes + `max` | none | `O(n)` |
| "merge overlapping" | interval merge | start asc | `O(n log n)` |
| "max non-overlapping" / "min removals" | activity selection | end asc | `O(n log n)` |
| "min arrows/points to hit all" | group counting | end asc, strict `>` | `O(n log n)` |
| "remove covered" | covered scan | `(start asc, end desc)` key | `O(n log n)` |
| two already-sorted lists, "intersection" | two pointers, advance earliest end | none | `O(n + m)` |
| "book if no overlap", online | linear/tree overlap test | list or sorted structure | `O(n)` / `O(log n)` per op |
| "best available at each time", options arrive/expire | sort by arrival + heap | `heapq` min-heap on the choice key | `O(n log n)` |
| "cooldown", "alternate most frequent" | max-heap of counts | `Counter` + `heapq` (negated) | `O(n log k)` |
| "defer the choice until stuck" | lazy greedy with heap of skipped | max-heap (negated) | `O(n log n)` |
| `k`-window over ratio order | sort by ratio + max-heap of size `k` | heap | `O(n log n)` |
| "can you reach", "min jumps" | `farthest` / `current_end` layers | none | `O(n)` |
| "min intervals to cover `[0,T]`" | reach layers over positions | `farthest_at[T+1]` | `O(n + T)` |
| circular route, gains and costs | total test + one pass `start` | none | `O(n)` |
| "double or subtract", forward unclear | reverse simulation | none | `O(log target)` |
| coins/squares/words/subsets, greedy "obviously" works | **stop** — test against brute force | DP table | `O(n * amount)` etc. |
| two players alternate, pick from ends | interval DP minimax | `dp[i][j]` | `O(n^2)` |

---

## Gotchas in Python specifically

- **`sort`/`sorted` is stable, `.sort(key=...)` almost always beats `cmp_to_key`.** A plain key function is called once per element (`O(n)` calls total) versus a comparator called `O(n log n)` times — prefer `key=` whenever the ordering can be expressed as "compute one value per element and compare those", and reserve `functools.cmp_to_key` for genuinely pairwise rules (string concatenation order).
- **`heapq` is a min-heap only.** Max-heap needs negated keys pushed in: `heapq.heappush(h, (-key, payload))`, and remember to negate back on pop. Unlike C, there is no `-LLONG_MIN` overflow trap — Python ints have no minimum — but a mixed sign convention (forgetting to negate consistently on both push and the comparison) is still the classic bug.
- **`list.pop(0)` and `list.insert(0, x)` are O(n).** Never use a plain `list` as a FIFO queue for the "layers" BFS-without-a-queue reach greedy if you ever do keep an explicit frontier — though the pattern in this chapter usually avoids needing one at all, relying only on scalar `farthest`/`current_end`.
- **Recursion limit ~1000.** A recursive brute-force cross-checker (unit 6) on `n > ~900` raises `RecursionError` where the intended `n <= 8` sandbox is safe; keep the brute-force harness explicitly bounded to small `n` and never accidentally call it on the real input size.
- **Mutable default arguments.** `def dp(i, memo={})` in a hand-written memoiser keeps state across calls and across independently-generated test instances in the brute-force harness — default to `None` and create fresh, or use `@lru_cache` which is scoped correctly but must be `.cache_clear()`-ed between unrelated instances if the function closes over instance-specific data.
- **`//` vs `/`.** Reach-a-number / circular-route math that ports a C integer-division formula must use `//` (floor toward `-inf` in Python, not truncation toward zero as in C) — `-7 // 2 == -4` in Python, `-3` if C had truncated; check which one the derivation actually needs and convert explicitly (`int(a / b)` or a sign-aware formula) if truncation was intended.
- **No overflow, but no cross-multiplication requirement either.** The C chapter cross-multiplies `p_a * w_b` vs `p_b * w_a` specifically to avoid floating point *and* overflow; in Python the overflow half of that reasoning is moot (ints are unbounded), but cross-multiplication (or `fractions.Fraction`) is still the right call whenever a plain `p / w` float comparison risks rounding on adversarial equal-ratio inputs.
- **Tuple comparison for heap tie-breaks.** `heapq.heappush(h, (key, tiebreak, payload))` just works because Python compares tuples element-by-element — no manual "compare the second field if the first ties" code, unlike the C sift-loop.
- **`abs()` never overflows.** C's `abs(INT_MIN)` undefined-behaviour trap does not exist; `abs(target)` in reach-a-number is always safe.
- **Dictionary/set for "last occurrence" and dictionary lookups**, not a fixed 26/256-entry array — `last = {}` handles arbitrary alphabets (Unicode included) without the `s[i] - 'a'` indexing assumption C relies on, and a `dict` naturally reports "not seen" via `.get(c, -1)` instead of a pre-filled sentinel array.
- **Word-break dictionaries**: a Python `set[str]` gives O(1) average membership tests directly — no hash table to write, no `bsearch` fallback, unlike C where this needs Chapter 10's open-addressing table or a sorted array.

---

## Common mistakes checklist

- [ ] The sort key and direction are justified by a one-line exchange argument, not by "it looked right".
- [ ] Both lists sorted before a two-pointer match.
- [ ] `key=` used wherever possible; `cmp_to_key` reserved for genuinely pairwise comparisons (concatenation order).
- [ ] Interval overlap compares against the running **end**, with the boundary semantics (`<=` vs `<`) decided from the problem statement and tested on touching intervals.
- [ ] Selection problems sort by **end**, merge problems by **start**, covered-removal by `(start asc, end desc)`.
- [ ] Two-list intersection advances the pointer whose interval **ends first**.
- [ ] Heap greedy admits *all* newly available items before every pop, and jumps time forward when the heap is empty.
- [ ] Lazy/retroactive heap greedy defers the choice (refuel, ladders) until it is forced.
- [ ] `farthest`/`current_end`: counter increments only when `i == current_end`; loop stops before the last index.
- [ ] Left edges clamped to 0 before indexing a position list (Python will not raise on a negative index — it silently wraps).
- [ ] Circular problems: total-sum feasibility test first, then one pass.
- [ ] Any "obviously greedy" coin/subset/word/game problem is cross-checked against brute force on tiny inputs before trusting it.
- [ ] 0/1 knapsack inner loop runs `s` **descending**.
- [ ] Max-heap uses negated keys consistently on both push and pop.
- [ ] Recursion depth considered for any recursive brute-force cross-checker; bounded explicitly to small `n`.

---

## You can move on when...

- You can state the exchange argument for activity selection (earliest end first) and for Smith's rule, and turn each into a `sorted(key=...)` or `cmp_to_key` comparator.
- You can write interval merge, activity selection and arrows-counting from memory, with the correct `<=`/`>` at the boundaries, and explain the `[1,100] [2,3] [4,5]` counterexample to start-sorting.
- You can implement the two-list intersection pointer walk and say why only the earlier-ending pointer advances.
- You can write a `(key, payload)` heap with `heapq` and use it for "admit arrived, pop shortest" event simulation, including the idle-time jump.
- You can write `min_jumps` with `current_end`/`farthest` and explain why it is BFS, and convert taps/clips into the same loop.
- You can produce, from memory, a counterexample where largest-coin-first fails and where "take the biggest house" fails, and write the DP recurrence that fixes each.
- You have a brute-force cross-check harness you reuse whenever you are tempted to trust a greedy.
