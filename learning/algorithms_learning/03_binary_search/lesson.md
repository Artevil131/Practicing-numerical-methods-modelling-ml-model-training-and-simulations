# Chapter 03 — Binary search

## What you'll be able to do after this chapter

- Write a binary search that terminates and is off-by-one free, in either of the two interval disciplines (closed `[lo, hi]` or half-open `[lo, hi)`), and never mix them inside one function.
- Reduce "find the first / last / closest / smallest that satisfies" problems to a single `lower_bound`-style skeleton with a monotone predicate `P(i)`, and read the answer off `lo` after the loop.
- Search a rotated sorted array (with and without duplicates) by deciding which half is sorted before deciding where the target lives, and explain why duplicates force `O(n)` worst case.
- Turn "minimise the maximum / maximise the minimum / smallest capacity that suffices" optimisation problems into binary search over the *value space* with a greedy `feasible(x)` check.
- Search 2D matrices three ways (flatten, staircase walk, binary search on value) and know which one matches which ordering guarantee.
- Plug a non-trivial algorithm (BFS, DFS, union-find) in as the feasibility check and state the total complexity as `T(feasible) * log(range)`.

## Why this matters for ML / numerics / sims

Binary search on the answer *is* a line search. When you bisect on a step size until a Wolfe/Armijo-style condition flips from false to true, or bisect on a Lagrange multiplier until a constraint is met, or find the largest learning rate that does not diverge, you are running `lower_bound` over a monotone predicate whose evaluation is an expensive simulation. Root finding by bisection (Chapter 12 of the C course touches float bit layouts you'll need to reason about tolerances) is the same loop with `f(mid) < 0` as the predicate. Sampling from a discrete distribution given cumulative weights — the categorical sample in every language model decoder — is `upper_bound` on a prefix-sum array. Time-series lookups ("latest value at or before timestamp t") are `lower_bound` on a sorted timestamp column. Choosing the smallest batch size / chunk size that fits a memory budget, the smallest number of shards that keeps per-shard load under a cap, or the largest safe time step in an adaptive integrator are all "minimise the maximum" instances. Finding the level-set threshold at which two regions of a grid become connected (percolation, flood fill in a height map, contact detection) is binary search on a threshold with BFS or union-find as the feasibility check.

The reason it matters in *C* specifically: `bsearch` from `<stdlib.h>` only answers "is it there"; every boundary variant, every predicate search, every value-space search you write by hand, and the two lines that decide `lo`/`hi` are where off-by-one and infinite-loop bugs live. Get the skeleton exact once and reuse it.

---

## 1. Plain sorted search

### The idea

Binary search halves the search interval each step, so it finds an element in a sorted array of length `n` in `O(log n)` time. The requirement is not "sorted array" — it is a **monotone predicate**: some condition `P(i)` that is false up to some boundary and true from there on (or the reverse). A sorted array gives you `P(i) = (a[i] >= target)` for free, but the predicate can also be a callback (`isBadVersion(i)`), a computed quantity (`i*i <= num`), or a three-way comparison (`guess(mid)`).

### The invariant — pick one form and never mix them

The whole algorithm works or breaks on how precisely the **invariant** is stated and preserved every iteration. Two disciplines:

| | Closed `[lo, hi]` | Half-open `[lo, hi)` |
|---|---|---|
| Loop condition | `while (lo <= hi)` | `while (lo < hi)` |
| `mid` | `lo + (hi - lo) / 2` | `lo + (hi - lo) / 2` |
| Exclude left incl. `mid` | `lo = mid + 1` | `lo = mid + 1` |
| Exclude right | `hi = mid - 1` | `hi = mid` (never `mid - 1`) |
| Empty interval | `lo > hi` | `lo == hi` |
| Natural use | exact-match search, three-way feedback | boundary search (first index where `P` is true) |

In the half-open form `mid` rounds down, so `mid < hi` always holds inside the loop; `hi = mid` therefore shrinks the interval strictly. In the closed form both branches step *past* `mid`, so the interval shrinks by at least one each round. Termination follows in both cases.

The most common bug is mixing them: `while (lo <= hi)` together with `hi = mid`. When `hi == lo + 1`, `mid` rounds down to `lo`, `hi = mid` sets `hi = lo`, the loop condition `lo <= hi` is still true, and `mid` is `lo` forever — an infinite loop. The second classic is `mid = (lo + hi) / 2`, which overflows a 32-bit `int` when `lo + hi > 2^31 - 1`. Always write `lo + (hi - lo) / 2`.

### C skeleton (closed form, exact match)

```c
#include <stddef.h>

/* Returns the index of target in a[0..n), or -1 if absent. a is sorted ascending. */
long bsearch_closed(const int *a, size_t n, int target) {
    long lo = 0, hi = (long)n - 1;           /* closed: both ends are candidates      */
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;      /* never (lo + hi) / 2                    */
        if (a[mid] == target) return mid;
        if (a[mid] < target)  lo = mid + 1; /* everything up to and incl. mid is < t  */
        else                  hi = mid - 1; /* everything from mid on is > t          */
    }
    return -1;                               /* lo > hi: interval is empty             */
}
```

`long` for the indices (or `ptrdiff_t`) so that `hi = -1` is representable when `n == 0` — with `size_t` the `hi = mid - 1` step underflows.

### Worked trace

`a = {1, 3, 4, 7, 9, 12, 15}`, `target = 9`, `n = 7`.

```
step  lo  hi  mid  a[mid]  action
  1    0   6   3     7     7 < 9   -> lo = 4
  2    4   6   5    12     12 > 9  -> hi = 4
  3    4   4   4     9     found, return 4
```

Now `target = 8` on the same array:

```
step  lo  hi  mid  a[mid]  action
  1    0   6   3     7     7 < 8   -> lo = 4
  2    4   6   5    12     12 > 8  -> hi = 4
  3    4   4   4     9     9 > 8   -> hi = 3
  4    lo=4 > hi=3, loop exits, return -1
```

Note that after the failed search `lo == 4` is exactly the insertion point — the first index with `a[i] >= 8`. The half-open form in section 2 makes that the *primary* output.

### Searching a computed value, not an array

`i*i <= num` is monotone in `i`; `k(k+1)/2 <= n` is monotone in `k`. Binary search on `[0, num]` or `[0, n]` for the largest `i` satisfying the predicate is the same loop with `a[mid]` replaced by a formula. The bug that actually bites here is arithmetic, not search logic: `mid * mid` with `mid` near `2^16` and `num` near `2^31` overflows `int`. Compute in `long long`, or compare `mid <= num / mid` using division instead. `k * (k + 1)` overflows even sooner.

**Python equivalent:** `bisect.bisect_left` is the half-open lower bound; there is no stdlib exact-match search because `bisect_left` plus one comparison *is* it.

### Recognising it

*Sorted*, *monotone*, *non-decreasing*, or a request for the smallest/largest value that satisfies a condition. Also any interactive problem where each query is expensive and returns "too high / too low".

### Pitfalls

- Mixing `while (lo <= hi)` with `hi = mid` → infinite loop.
- `(lo + hi) / 2` → overflow on large indices or large value ranges.
- Forgetting `+1` / `-1` in the closed form and re-examining `mid` → infinite loop between two adjacent indices.
- Doing the predicate arithmetic in 32 bits.
- Using `size_t` for `hi` in the closed form; `hi = mid - 1` when `mid == 0` wraps to `SIZE_MAX`.

---

## 2. Lower bound, upper bound and boundary discipline

### The idea

Most binary search problems do not ask for an exact hit but for a **boundary**: the smallest index whose element is `>= target` (**lower bound**) or the smallest index whose element is `> target` (**upper bound**). Both are solved by one half-open skeleton — the only difference is which way the comparison `a[mid]` vs `target` branches. Write the body in exactly this shape every time:

```c
while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (P(mid)) hi = mid;       /* mid might be the answer: keep it   */
    else        lo = mid + 1;   /* mid is definitely not: skip past   */
}
/* answer is lo (== hi) */
```

where `P` is a monotone predicate, false on the left and true on the right. `hi` starts at `n`, so "no index satisfies `P`" is reported as `lo == n` without a special case.

| Want | Predicate `P(mid)` | Result `lo` |
|---|---|---|
| lower_bound(t): first `a[i] >= t` | `a[mid] >= t` | insertion point; also first occurrence if `a[lo] == t` |
| upper_bound(t): first `a[i] > t` | `a[mid] > t` | one past the last occurrence |
| count of `t` | — | `upper_bound(t) - lower_bound(t)` |
| last `a[i] <= t` | — | `upper_bound(t) - 1` (check for `-1`/`0`) |
| last `a[i] < t` | — | `lower_bound(t) - 1` |

"Find first" and "find last" are not two algorithms; they are one skeleton with two predicates.

### C skeleton

```c
#include <stddef.h>

/* First index i in [0, n) with a[i] >= t; returns n if none. a sorted ascending. */
size_t lower_bound(const int *a, size_t n, int t) {
    size_t lo = 0, hi = n;                 /* half-open: hi is one past the last candidate */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;   /* mid < hi is guaranteed, so hi = mid shrinks */
        if (a[mid] >= t) hi = mid;         /* mid could be the boundary                    */
        else             lo = mid + 1;     /* a[mid] < t: boundary is strictly right       */
    }
    return lo;
}

/* First index i with a[i] > t; returns n if none. Same body, one character changed. */
size_t upper_bound(const int *a, size_t n, int t) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] > t) hi = mid;
        else            lo = mid + 1;
    }
    return lo;
}
```

`size_t` is fine here because the half-open form never computes `mid - 1`.

### Worked trace

`a = {2, 4, 4, 4, 7, 9}`, `n = 6`, `lower_bound(4)`:

```
step  lo  hi  mid  a[mid]  a[mid] >= 4 ?  action
  1    0   6   3     4        yes         hi = 3
  2    0   3   1     4        yes         hi = 1
  3    0   1   0     2        no          lo = 1
  4    lo == hi == 1  -> return 1          (first 4 is at index 1)
```

`upper_bound(4)` on the same array:

```
step  lo  hi  mid  a[mid]  a[mid] > 4 ?   action
  1    0   6   3     4        no          lo = 4
  2    4   6   5     9        yes         hi = 5
  3    4   5   4     7        yes         hi = 4
  4    lo == hi == 4  -> return 4          (one past the last 4)
```

Count of 4s: `4 - 1 = 3`. Last index `<= 4`: `4 - 1 = 3`. Both boundary searches together give "first and last position" in `O(log n)`.

```
index:   0  1  2  3  4  5  (6)
a:       2  4  4  4  7  9
              ^        ^
       lower_bound=1   upper_bound=4
```

### Non-trivial predicates on the same skeleton

The predicate does not have to be `a[mid] >= t`:

- **Peak finding**: `P(mid) = a[mid] > a[mid + 1]` ("are we already on the descending side"). Run on `[0, n - 1)` so that `mid + 1 <= n - 1` is always a valid read — `mid < hi = n - 1` inside the loop guarantees it. With the convention `a[-1] = a[n] = -inf`, some peak always exists and `lo` after the loop is one.
- **Window placement**: for the `k` closest elements to `x`, search the *left edge* `l` of a window `[l, l + k)` over `[0, n - k]` with `P(mid) = !(x - a[mid] > a[mid + k] - x)`; the predicate compares two distances outside the window rather than one element to a target.
- **Index parity**: in a sorted array where every element appears twice except one, before the singleton pairs start at even indices; after it, at odd indices. Force `mid` even (`mid -= mid % 2`) and use `P(mid) = (a[mid] != a[mid + 1])`; `lo = mid + 2` when the pair is intact. The interval shrinks by at least two per round.
- **Reverse lookup**: "largest stored timestamp `<= t`" is `upper_bound(t) - 1`, with `upper_bound(t) == 0` meaning "nothing that old".
- **Prefix sums for weighted sampling**: with `prefix[i] = w[0] + ... + w[i]` and `r` uniform in `[1, total]`, the chosen index is the first `i` with `prefix[i] >= r` — lower bound on a strictly increasing array (weights are positive, so no ties).

**Python equivalent:** `bisect.bisect_left` = `lower_bound`, `bisect.bisect_right` = `upper_bound`. C++'s `std::lower_bound` / `std::upper_bound` are exactly the two functions above and you'll use them in the C++ track.

### Recognising it

*First*, *last*, *closest*, *smallest that satisfies*, *insertion position*, *how many are <= x* — the problem wants a boundary, not equality.

### Pitfalls

- Writing `hi = mid - 1` in the half-open skeleton; it discards a possible answer.
- Starting with `hi = n - 1` in the half-open skeleton; then "not found" collides with "found at `n - 1`".
- Confusing `>=` (lower) with `>` (upper). The wrap-around letter problem exists purely to test this.
- Reading `a[mid + 1]` when `hi` was not reduced to `n - 1` → out-of-bounds read, which in C is silent UB, not an exception.
- Computing a threshold like `success / spell` in floating point and comparing to integers — rounding at the boundary flips the predicate. Keep it in integer arithmetic (`(success + spell - 1) / spell`, or compare the product in 64 bits).

---

## 3. Search in a rotated array

### The idea

A rotated sorted array (`{4, 5, 6, 7, 0, 1, 2}`) is no longer globally monotone, but it keeps one property that suffices: **at every binary-search split, at least one of the two halves is fully sorted**. The invariant is therefore a two-stage predicate per iteration:

1. Decide which half — `[lo, mid]` or `[mid, hi]` — is sorted by comparing `a[lo]` and `a[mid]`. If `a[lo] <= a[mid]`, the left half is sorted (no wrap inside it); otherwise the right half is.
2. Check whether the target lies inside the sorted half with an ordinary range comparison. If yes, continue there; if not, continue into the other (possibly broken) half.

The same reasoning finds the rotation point (the minimum): compare `a[mid]` to `a[hi]`. If `a[mid] > a[hi]`, the minimum is strictly to the right of `mid`; otherwise `mid` itself may be the minimum. Comparing against `a[hi]` rather than `a[lo]` is deliberate — it yields a clean two-way predicate with no third case.

### Duplicates break the reasoning

If `a[lo] == a[mid] == a[hi]`, neither half can be proven sorted from the endpoints alone. The only safe move is to shrink one step from both ends (`lo++, hi--`) and try again. For the minimum search, where only `a[mid]` and `a[hi]` are compared, the analogous case `a[mid] == a[hi]` allows `hi--` (dropping one duplicate never discards the answer). Both fixes drop the worst case from `O(log n)` to `O(n)` — unavoidable, because with duplicates the problem is genuinely ambiguous without a linear scan (`{3, 3, 1, 3}` vs `{3, 1, 3, 3}` look identical from the middle).

### C skeleton (search for target, no duplicates)

```c
/* Index of t in a rotated ascending array a[0..n) with distinct values, or -1. */
long rotated_search(const int *a, long n, int t) {
    long lo = 0, hi = n - 1;                        /* closed interval */
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        if (a[mid] == t) return mid;
        if (a[lo] <= a[mid]) {                      /* left half [lo, mid] is sorted   */
            if (a[lo] <= t && t < a[mid]) hi = mid - 1;   /* t is in it            */
            else                          lo = mid + 1;   /* t is in the other half */
        } else {                                    /* right half [mid, hi] is sorted  */
            if (a[mid] < t && t <= a[hi]) lo = mid + 1;
            else                          hi = mid - 1;
        }
    }
    return -1;
}
```

Minimum (rotation point), half-open on `[0, n - 1)`:

```c
long lo = 0, hi = n - 1;
while (lo < hi) {
    long mid = lo + (hi - lo) / 2;
    if (a[mid] > a[hi]) lo = mid + 1;   /* mid is on the high plateau: min is right of it */
    else                hi = mid;       /* mid could be the minimum                       */
}
/* a[lo] is the minimum; lo is the rotation offset */
```

### Worked trace

`a = {4, 5, 6, 7, 0, 1, 2}`, `t = 1`.

```
step  lo  hi  mid  a[lo] a[mid] a[hi]  sorted half   t in it?         action
  1    0   6   3     4     7      2     left [0,3]    4<=1<7 ? no      lo = 4
  2    4   6   5     0     1      2     found a[5]==1, return 5
```

Minimum on the same array:

```
step  lo  hi  mid  a[mid] a[hi]  action
  1    0   6   3     7      2     7 > 2  -> lo = 4
  2    4   6   5     1      2     1 <= 2 -> hi = 5
  3    4   5   4     0      1     0 <= 1 -> hi = 4
  4    lo == hi == 4 -> min is a[4] = 0, rotation offset 4
```

```
a:  4  5  6  7 | 0  1  2
    ^ sorted  ^   ^ sorted ^
              pivot at 4
```

### The mountain family

A "mountain" array (strictly increasing then strictly decreasing) is a rotated array's cousin. Peak index = the peak-finding predicate from section 2 (`a[mid] < a[mid + 1]` → still ascending → `lo = mid + 1`). To find a target in a mountain array: one search for the peak, then an ascending binary search on `[0, peak]`, then if needed a *descending* binary search (comparison flipped) on `[peak, n - 1]`. When element access is an expensive callback with a budget, cache every `get(mid)` in a local variable and never call it twice for the same index.

### Recognising it

*Rotated*, *shifted*, "sorted array rotated at an unknown pivot", "mountain array". The warm-up problem "is this array a rotation of a sorted array" is a linear pass counting descents `a[i] > a[(i + 1) % n]`: a valid rotation has at most one. That same local question — "which side is broken?" — is what the binary search asks at each split.

### Pitfalls

- Comparing `a[mid]` to `a[lo]` in the *minimum* search creates a third case when `lo == mid`; compare to `a[hi]`.
- Using `<` instead of `<=` in `a[lo] <= a[mid]` breaks the two-element case where `lo == mid`.
- Forgetting the duplicate collapse and looping forever on `{1, 1, 1, 1, 1}` — or "fixing" it with a move that can skip the answer. For target search shrink both ends; for minimum search shrink `hi` only.
- Range checks for "t in sorted half" must be half-open at the `mid` end (`t < a[mid]`), because `a[mid] == t` was already handled.

---

## 4. Binary search on the answer (minimise the maximum)

### The idea

Here the search is not over array indices but over the **possible values of the answer**. Reframe the optimisation problem ("minimise the largest chunk sum", "maximise the smallest gap", "smallest speed that finishes in time") as a decision problem: **"is value `x` achievable?"**. The feasibility predicate `feasible(x)` is monotone: in a minimisation problem, if `x` works then every larger `x` works; in a maximisation problem, if `x` works then every smaller `x` works. Binary search then finds the boundary of that predicate with the same `[lo, hi)` skeleton as the array searches.

Two parts, always:

1. **Choose the value range.** Typically `[max(values), sum(values)]` for a capacity/ceiling, `[1, max(values)]` for a speed/divisor, `[min, max]` of the data for a day/threshold. It is a *value* interval, not an index interval — think about what the smallest and largest conceivable answers are and prove both ends.
2. **Write `feasible(x)`.** Usually a **greedy simulation**: one pass over the data that fills containers/counts operations as tightly as possible at capacity `x` (e.g. "how many days are needed if daily capacity is `x`"), then compares the count to the budget. Greedy is correct here because for a fixed `x` packing as much as possible into each container never hurts.

With `feasible` in `O(n)`, the total is `O(n log(hi - lo))`.

### Direction matters

| Problem shape | Predicate monotone how | You want | Skeleton |
|---|---|---|---|
| Minimise the maximum (capacity, speed, divisor, ceiling) | false … false true … true | **smallest** true | `if (feasible(mid)) hi = mid; else lo = mid + 1;` |
| Maximise the minimum (gap, threshold, safety) | true … true false … false | **largest** true | `if (feasible(mid)) lo = mid; else hi = mid - 1;` with `mid = lo + (hi - lo + 1) / 2` |

The second row rounds `mid` *up* so that `lo = mid` still makes progress; alternatively negate the predicate and search for the smallest false, then subtract one. Pick one and be explicit. Problems where the counted quantity *increases* with `x` (total trips completed in time `t`) rather than decreasing (hours needed at speed `k`) also flip the comparison inside `feasible`; write down "as `x` grows, count goes ___" before coding.

### C skeleton

```c
#include <stddef.h>

/* Smallest capacity such that a[0..n) (in order) fits in at most days contiguous chunks
   whose sums do not exceed the capacity. a[i] > 0. */
static int fits_in_days(const int *a, size_t n, long long cap, int days) {
    int used = 1; long long cur = 0;             /* greedy: open a new chunk only when forced */
    for (size_t i = 0; i < n; i++) {
        if (cur + a[i] > cap) { used++; cur = 0; }
        cur += a[i];                             /* a[i] <= cap is guaranteed by lo = max */
    }
    return used <= days;
}

long long min_capacity(const int *a, size_t n, int days) {
    long long lo = 0, hi = 0;                    /* lo = max(a), hi = sum(a): value range */
    for (size_t i = 0; i < n; i++) { if (a[i] > lo) lo = a[i]; hi += a[i]; }
    while (lo < hi) {                            /* half-open on values, hi is feasible    */
        long long mid = lo + (hi - lo) / 2;
        if (fits_in_days(a, n, mid, days)) hi = mid;   /* smallest feasible is <= mid  */
        else                               lo = mid + 1;
    }
    return lo;
}
```

Here `hi = sum` is *known* feasible (one chunk holds everything) so the half-open form with `hi` as a candidate is correct.

### Worked trace

`a = {7, 2, 5, 10, 8}`, `days = 2`. Range `[10, 32]`.

```
step  lo   hi   mid  greedy chunks at cap=mid          used  feasible?  action
  1   10   32   21   [7,2,5] [10,8]                      2     yes      hi = 21
  2   10   21   15   [7,2,5] [10] [8]                    3     no       lo = 16
  3   16   21   18   [7,2,5] [10,8]                      2     yes      hi = 18
  4   16   18   17   [7,2,5] [10] [8]  (10+8=18>17)      3     no       lo = 18
  5   lo == hi == 18 -> answer 18
```

### The `ceil` family

Many checks reduce to a sum of ceilings: hours to eat piles at speed `k` = `sum(ceil(p / k))`; splits to cap bags at `m` = `sum(ceil(b / m) - 1)`; sum of `ceil(x / d)` under a threshold. Integer ceiling division is `(p + k - 1) / k` — not `p / k + 1`, which is wrong when `k` divides `p`. Trips completed by time `t` is a sum of *floors*, `t / time[i]`, and grows with `t`. Some checks have a closed form instead of a loop (arithmetic-series sums for a peak with slopes falling to 1 on both sides) — then `feasible` is `O(1)` and overflow in the formula is the real risk; use `long long`.

**Python equivalent:** none in the stdlib; you would write the same loop. (`bisect` on a `range` with a key is possible in 3.10+ but obscures the structure.)

### Recognising it

*Minimise the maximum*, *maximise the minimum*, *smallest speed / capacity / divisor / number of days that suffices*, *largest gap achievable*. The common misstep is trying to derive the answer directly from the data instead of searching the value space and letting `feasible` do the reasoning.

### Pitfalls

- Value range too tight (e.g. `lo = 1` for a capacity when one item alone exceeds it) or too loose (wastes iterations but is otherwise harmless — err this way when unsure).
- Impossible-instance checks belong *before* the search (e.g. `m * k > n` flowers → no answer; too many legs to round → return `-1`). A binary search over a predicate that is false everywhere returns `hi`, which silently looks like an answer.
- Getting the direction wrong: smallest-true vs largest-true. Write the truth table for `feasible` at `lo` and `hi` before coding.
- `ceil` as `p / k + 1`.
- Overflow in `sum(ceil(...))` or `k(k+1)/2` — keep the accumulator in `long long`.

---

## 5. Search in a 2D matrix

### The idea

A sorted 2D matrix extends binary search into two dimensions, but the right technique depends on **exactly what ordering is guaranteed**.

**Globally sorted** (each row begins after the previous row ends): treat the `m x n` matrix as one flat sorted array of length `m*n`. A plain 1D binary search on indices `[0, m*n - 1]` works unchanged; convert each `mid` with `row = mid / n`, `col = mid % n`. In C a matrix stored as a single contiguous block (`int *a` with `a[row * n + col]`) *is* that flat array — no conversion needed at all if you index it directly. If you have `int **rows`, do the `/` and `%`.

**Rows and columns each sorted, but not globally**: 1D binary search does not apply. Use the **staircase walk** from the top-right corner (or bottom-left): if the current value is too large, move left (the whole column below it is also too large); if too small, move down (the whole row to its left is also too small). Each step eliminates one row or one column, so the walk takes at most `m + n` steps — `O(m + n)`, not logarithmic, but the same monotonicity reasoning applied one dimension at a time. Starting from the top-left or bottom-right does not work: from those corners both moves can either increase or decrease the value.

**k-th smallest / value queries**: binary search on the **value**, not the index. Try a value `x`, count how many entries are `<= x` (usually with a staircase walk from the bottom-left: move right while `a[r][c] <= x` adding the column's contribution, else move up — `O(m + n)` per count), and find the smallest `x` whose count reaches `k`. Because the boundary of a monotone counting predicate always lands on an actual matrix value (the count only changes when `x` crosses one), the result is a real element even though the search range `[a[0][0], a[m-1][n-1]]` contains values not in the matrix. That is section 4's "search on the answer" applied in 2D.

### C skeleton (staircase walk)

```c
/* Row-major m x n matrix, each row ascending left->right, each column ascending top->down.
   Returns 1 if t is present. O(m + n). */
int staircase_find(const int *a, int m, int n, int t) {
    int r = 0, c = n - 1;                        /* top-right corner */
    while (r < m && c >= 0) {
        int v = a[r * n + c];
        if (v == t) return 1;
        if (v > t)  c--;                          /* everything below in this column is > t */
        else        r++;                          /* everything left in this row is < t     */
    }
    return 0;
}

/* Count of entries <= x in the same matrix, walking from the bottom-left. */
long count_le(const int *a, int m, int n, int x) {
    int r = m - 1, c = 0; long cnt = 0;
    while (r >= 0 && c < n) {
        if (a[r * n + c] <= x) { cnt += r + 1; c++; }   /* whole column above incl. r is <= x */
        else                    r--;
    }
    return cnt;
}
```

### Worked trace

```
      c0  c1  c2  c3
r0     1   4   7  11
r1     2   5   8  12
r2     3   6   9  16
r3    10  13  14  17
```

`staircase_find(t = 9)`:

```
step  (r,c)  v    action
  1   (0,3)  11   11 > 9  -> c = 2
  2   (0,2)   7   7 < 9   -> r = 1
  3   (1,2)   8   8 < 9   -> r = 2
  4   (2,2)   9   found
```

`count_le(x = 8)` from bottom-left:

```
step  (r,c)  v    action                cnt
  1   (3,0)  10   10 > 8  -> r = 2        0
  2   (2,0)   3   3 <= 8  -> cnt += 3, c=1   3
  3   (2,1)   6   6 <= 8  -> cnt += 3, c=2   6
  4   (2,2)   9   9 > 8   -> r = 1        6
  5   (1,2)   8   8 <= 8  -> cnt += 2, c=3   8
  6   (1,3)  12   12 > 8  -> r = 0        8
  7   (0,3)  11   11 > 8  -> r = -1, stop     8
```

Eight entries are `<= 8`, so for `k = 8` the k-th smallest is the smallest `x` with `count_le(x) >= 8`, which the outer binary search on `[1, 17]` drives to `x = 8`.

### Other 2D shapes in this unit

- **Per-row boundary**: rows of the form `1 1 1 0 0` — soldiers per row is `lower_bound` of `0` in the row, `O(log n)` each; then a selection problem (sort or heap on `(count, row)`) on the `m` results. Binary search is one component of a larger solution.
- **Count negatives in a sorted-descending matrix**: staircase from the top-right, adding the rest of the row when you hit a negative; or `upper_bound` per row.
- **2D peak**: binary search over *columns*; for `mid` column find its maximum row (`O(m)` scan), compare with the left/right neighbours on that row, move toward the larger neighbour exactly as in 1D peak finding. `O(m log n)`.
- **Largest square with sum `<= threshold`**: build a 2D prefix-sum table (`O(nm)`), then binary search the side length `k` on `[0, min(n, m)]`; `feasible(k)` scans all `k x k` top-left corners and asks whether *any* has sum `<= threshold` in `O(1)` each. Monotone because values are positive, so a bigger square is never cheaper than a smaller one contained in it.
- **k-th smallest sum choosing one per row**: value-space search where the counting function itself is a bounded row-by-row merge; the binary search is routine, the counter is the hard part.

### Recognising it

*Matrix*, *rows sorted and columns sorted*, *k-th smallest in a matrix*, *each row starts after the previous ends*. Ask first: globally sorted, or only per row/column? That single question picks the technique.

### Pitfalls

- Applying the flatten trick to a matrix that is only row/column sorted. It is wrong, not just slow.
- Starting the staircase from the wrong corner.
- Storing a matrix as `int **` with one `malloc` per row when a single `int *` block and `a[r * n + c]` is faster and simpler (see `../../c_learning/06_dynamic_memory/lesson.md`).
- Forgetting that in `count_le` the "add the whole column" step adds `r + 1` entries, not `1`.

---

## 6. Binary search plus greedy / graph feasibility checks

### The idea

This unit generalises "binary search on the answer" to cases where `feasible(x)` is no longer a linear sum but **needs its own algorithm** — a greedy construction, a BFS/DFS reachability test, or union-find connectivity. The core principle is unchanged: guess a value `x`, and check with a *completely separate* algorithm whether the constraint holds at `x`. Binary search is only responsible for locating the boundary; it does not know or care what the check does.

The recognisable structure is two-part:

1. **Argue monotonicity** of feasibility in the searched variable. "If the route succeeds with maximum step effort `x`, it also succeeds with any larger `x`" is obvious — a larger threshold admits a superset of edges, and a superset of edges cannot disconnect anything. Likewise, more water flooded = fewer usable cells = harder to cross; more time waited = fire is further along = harder to escape. Say the sentence out loud before writing code.
2. **Write the check as its own function**, which may be any algorithm. On grids it is usually BFS or DFS from a source using only edges/cells that pass the threshold, or union-find that adds edges in threshold order and asks whether two specific nodes are connected.

Total time is `T(feasible) * log(range)`. Since the check may be expensive (`O(nm)` BFS on a grid), this combination pays off only when the value range is much smaller than the state space you would otherwise have to enumerate — `log(10^6) = 20` BFS passes is cheap; searching a range of `10^18` with a slow check is not.

### Preprocessing before the search

Several problems compute a per-cell quantity *once* and then search on it: multi-source BFS from all thieves gives every cell its distance to the nearest thief (`O(nm)`), then the search threshold `s` asks "can I cross using only cells with safety `>= s`". BFS from the fire source gives every cell an ignition time, then the search on wait time `w` runs a *second* BFS for the person in which a cell is enterable only if `w + dist < ignition[cell]` (with equality allowed only at the goal). Keep the precomputed arrays and the per-check arrays separate — mixing "fire arrival" and "person arrival" in one buffer is the bug the hardest problem here is designed to produce.

### C skeleton (threshold + BFS on a grid)

```c
#include <stdlib.h>
#include <string.h>

/* Can we walk from (0,0) to (m-1,n-1) using only cells with h[r*n+c] <= t?  4-neighbour BFS
   on an explicit queue (no recursion -> no stack-depth worry on large grids). */
static int reachable_under(const int *h, int m, int n, int t) {
    if (h[0] > t) return 0;
    unsigned char *seen = calloc((size_t)m * n, 1);
    int *q = malloc(sizeof *q * (size_t)m * n);         /* ring not needed: each cell enqueued once */
    if (!seen || !q) { free(seen); free(q); return 0; }
    int head = 0, tail = 0, found = 0;
    q[tail++] = 0; seen[0] = 1;
    static const int dr[4] = {1, -1, 0, 0}, dc[4] = {0, 0, 1, -1};
    while (head < tail && !found) {
        int cur = q[head++], r = cur / n, c = cur % n;
        if (cur == m * n - 1) { found = 1; break; }
        for (int k = 0; k < 4; k++) {
            int nr = r + dr[k], nc = c + dc[k];
            if (nr < 0 || nr >= m || nc < 0 || nc >= n) continue;
            int id = nr * n + nc;
            if (seen[id] || h[id] > t) continue;
            seen[id] = 1; q[tail++] = id;
        }
    }
    free(seen); free(q);
    return found;
}
/* Outer loop: lo = 0, hi = max height; while (lo < hi) { mid; if (reachable_under(mid)) hi = mid; else lo = mid+1; } */
```

Allocating the `seen` buffer and queue once outside the search and `memset`-ing per check is the usual optimisation once correctness is in place.

### Worked trace

Height grid, find the smallest water level `t` at which `(0,0)` connects to `(2,2)` through cells with height `<= t`:

```
 0  2  6
 5  3  7
 8  4  1
```

Range `[0, 8]`.

```
step  lo  hi  mid  cells usable (<= mid)              (0,0)->(2,2)?  action
  1    0   8   4   0,2,3,4,1                           0->2->3->4->1 yes   hi = 4
  2    0   4   2   0,2,1                               stuck at (0,1)  no  lo = 3
  3    3   4   3   0,2,3,1                             (1,1) reached, (2,1)=4 blocked  no  lo = 4
  4    lo == hi == 4 -> answer 4
```

Union-find alternative: sort cells by height, union each with already-active neighbours in increasing order, stop when `find(0) == find(m*n-1)`. It answers the same question without an explicit binary search — but binary search + BFS is more direct and transfers to every other "threshold + reachability" problem, and union-find is the subject of a later chapter.

### Maximise vs minimise, again

"Last day the crossing is still possible" (more water each day) and "safest path" (largest minimum distance to a thief) are the maximisation mirror of "swim in rising water" and "minimum effort": feasibility is true on the low side and false on the high side, so you want the **largest** true. Use the upper-rounding `mid` from section 4 or search for the first false and subtract one.

### Recognising it

A threshold parameter plus any of: *path*, *reachable*, *connected*, *cross*, *escape*, *effort*, *safety*. Or a greedy construction whose cost depends monotonically on a single parameter (splitting bags to a size limit, building a peaked array under a sum budget).

### Pitfalls

- Recursion-based DFS for the check on a `10^3 x 10^3` grid can overflow the C stack (8 MB default on macOS main thread; smaller in threads). Use an explicit queue/stack.
- Re-allocating the visited array inside the check for every one of the ~20 iterations is fine for correctness but wasteful; allocate once.
- Equality at the goal: "person arrives at the same tick as the fire" is allowed at the goal cell only. Off-by-one in that comparison is the entire difficulty of the problem.
- Forgetting the "infinite wait" special case when the fire can never reach the person's route.
- Assuming the check is monotone without arguing it. If it isn't, binary search returns garbage without any warning.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Complexity |
|---|---|---|
| sorted array, find target / does it exist | closed-interval exact search (§1) | `O(log n)` |
| too high / too low feedback, expensive query | closed-interval search on a callback (§1) | `O(log n)` calls |
| integer sqrt, largest `k` with `f(k) <= n`, `f` monotone | search on a computed value in 64-bit (§1) | `O(log n)` |
| first / last / insertion point / how many `<= x` | `lower_bound` / `upper_bound` half-open (§2) | `O(log n)` |
| peak, local maximum, neighbour comparison | boundary search with `P(mid) = a[mid] > a[mid+1]` (§2) | `O(log n)` |
| k closest, window of size k | search the window's left edge (§2) | `O(log(n-k) + k)` |
| latest value at or before time t | `upper_bound(t) - 1` on a sorted timestamp list (§2) | `O(log n)` per query |
| random pick weighted by w | prefix sums + `lower_bound(r)` (§2) | `O(n)` build, `O(log n)` pick |
| median of two sorted arrays | search the partition point in the shorter array (§2) | `O(log min(m,n))` |
| rotated, shifted, pivot | decide sorted half, then range test (§3) | `O(log n)`; `O(n)` with duplicates |
| mountain array | peak search + two directional searches (§3) | `O(log n)` |
| minimise the maximum / smallest capacity, speed, divisor, days | value-space search, smallest feasible, greedy check (§4) | `O(n log range)` |
| maximise the minimum / largest gap, threshold | value-space search, largest feasible (§4, §6) | `O(n log range)` |
| matrix, each row starts after previous ends | flatten, 1D search (§5) | `O(log mn)` |
| matrix, rows and columns sorted | staircase from top-right / bottom-left (§5) | `O(m + n)` |
| k-th smallest in a sorted matrix | value search + staircase count (§5) | `O((m+n) log range)` |
| largest square under a sum threshold | 2D prefix sums + search side length (§5) | `O(nm log min(n,m))` |
| path / reachable / connected under a threshold | value search + BFS/DFS/union-find check (§6) | `O(nm log range)` |
| two BFS layers (fire + person, thief distance + path) | precompute one BFS, search, second BFS as check (§6) | `O(nm log(nm))` |

---

## Gotchas in C specifically

- **`mid = (lo + hi) / 2` overflows.** With `int` indices it needs arrays of `> 2^30` elements to bite; with value-space searches on `long long` ranges up to `10^18` it bites immediately. `lo + (hi - lo) / 2` always.
- **Signed vs unsigned for `hi`.** Closed-interval code that does `hi = mid - 1` must use a signed type (`long`, `ptrdiff_t`) — `size_t` wraps to `SIZE_MAX` and the loop condition `lo <= hi` becomes always true. Half-open code can use `size_t` safely because it never subtracts.
- **`bsearch` from `<stdlib.h>`** gives you exact-match search over any element type with a comparator (`../../c_learning/11_function_pointers_and_generics/lesson.md`), but *not* lower/upper bound and *no guarantee which* duplicate it returns. Write your own boundary searches; keep `bsearch` for one-off lookups.
- **`qsort` comparators**: `return a - b;` overflows for large-magnitude ints. Use `(a > b) - (a < b)`. Sorting is a prerequisite for most §2 and §4 problems (potions, positions, matrix rows), so this is on the critical path.
- **Integer division truncates toward zero**, so `ceil(p / k)` for positive ints is `(p + k - 1) / k`; for possibly-negative values you need a sign-aware formula. Never compute a threshold in `double` and compare to `int` at a boundary.
- **Out-of-bounds reads are silent.** `a[mid + 1]` past the end does not throw; it returns garbage or crashes later. Reduce `hi` so the read is provably in range, and test with `-fsanitize=address` (`../../c_learning/13_debugging_testing_perf/lesson.md`).
- **Matrices**: one contiguous `int *` with `a[r * n + c]` is both the natural "flattened" view for §5 and the fastest layout. `int **` with per-row `malloc` breaks the flatten trick and thrashes the cache.
- **Recursion depth**: recursive DFS as a feasibility check on a large grid can blow the stack. Use an explicit queue (BFS) or an explicit stack array.
- **Interactive problems** (`isBadVersion`, `guess`, `MountainArray.get`) become function pointers or a struct with a callback in C; cache each result in a local so you never pay for the same index twice.
- **`long long` for every product and running sum** in value-space searches: `mid * mid`, `k * (k + 1) / 2`, `spell * potion`, sums of ceilings, arithmetic-series closed forms.

---

## Common mistakes checklist

- [ ] `while (lo <= hi)` paired with `hi = mid` (infinite loop) or `while (lo < hi)` paired with `hi = mid - 1` (skips the answer).
- [ ] `(lo + hi) / 2` instead of `lo + (hi - lo) / 2`.
- [ ] Half-open search started with `hi = n - 1` instead of `hi = n`, so "not found" is indistinguishable from "found at the end".
- [ ] `>=` and `>` confused between lower and upper bound.
- [ ] Reading `a[mid + 1]` without shrinking `hi` to `n - 1`.
- [ ] Minimum-in-rotated compared against `a[lo]` instead of `a[hi]`.
- [ ] Duplicates in a rotated array with no collapse step (`lo++, hi--` for search; `hi--` for minimum).
- [ ] Value-space range that excludes the true answer (`lo = 1` when a single item exceeds it).
- [ ] Impossible instances (`m * k > n`, too many legs to round) not rejected before the search.
- [ ] Direction of monotonicity not written down: searching for the smallest true when the problem wants the largest true.
- [ ] `ceil` written as `x / k + 1`.
- [ ] 32-bit products or sums inside `feasible`.
- [ ] Threshold computed in floating point.
- [ ] Staircase walk started from top-left or bottom-right.
- [ ] Flatten trick applied to a row/column-sorted (not globally sorted) matrix.
- [ ] Feasibility assumed monotone without an argument.
- [ ] Recursive DFS as the check on a large grid.
- [ ] Interactive callback invoked twice for the same index.

---

## You can move on when...

- You can write `lower_bound` and `upper_bound` from memory, in under a minute each, with the half-open skeleton, and explain why `hi = mid` cannot loop forever.
- Given a boundary problem, you can name the predicate `P(i)` in one sentence and say which side is true before touching the keyboard.
- You can trace the rotated-array search on a 7-element input by hand, including the duplicate case, and state why duplicates cost `O(n)`.
- You can take a "minimise the maximum" problem you have not seen, name the value range with a justification for both ends, write `feasible` as a greedy pass, and pick the correct direction — and you did this for at least two of: capacity/ship, Koko, smallest divisor, split array.
- You can explain which matrix ordering allows the flatten trick, which requires the staircase, and why the staircase starts at the top-right.
- You have written one threshold + BFS check with an explicit queue in C, compiled it under `-Wall -Wextra -fsanitize=address`, and it passes on a small grid you traced by hand.
- Your `example.c`-style test programs compile warning-free with `cc -Wall -Wextra -std=c11 -O2`.
