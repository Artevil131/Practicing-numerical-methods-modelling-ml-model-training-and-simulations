# Chapter 03 — Binary search — Python

## What you'll be able to do after this chapter

- Write a binary search that terminates and is off-by-one free, in either of the two interval disciplines (closed `[lo, hi]` or half-open `[lo, hi)`), and never mix them inside one function.
- Reduce "find the first / last / closest / smallest that satisfies" problems to a single `bisect_left`-style skeleton with a monotone predicate `P(i)`, and read the answer off `lo` after the loop.
- Search a rotated sorted list (with and without duplicates) by deciding which half is sorted before deciding where the target lives, and explain why duplicates force `O(n)` worst case.
- Turn "minimise the maximum / maximise the minimum / smallest capacity that suffices" problems into binary search over the *value space* with a greedy `feasible(x)` check.
- Search 2D matrices three ways (flatten, staircase walk, binary search on value) and know which one matches which ordering guarantee.
- Plug a non-trivial algorithm (BFS, DFS, union-find) in as the feasibility check and state the total complexity as `T(feasible) * log(range)`.

## Why this matters for ML / numerics / sims

Binary search on the answer *is* a line search. Bisecting on a step size until an Armijo condition flips, bisecting on a Lagrange multiplier until a constraint is met, or finding the largest learning rate that does not diverge — all are `bisect_left` over a monotone predicate whose evaluation is an expensive simulation. Root finding by bisection is the same loop with `f(mid) < 0` as the predicate. Sampling from a discrete distribution given cumulative weights — the categorical sample in every LM decoder — is `bisect_right` on a prefix-sum array (`random.choices` does exactly this). Time-series lookups ("latest value at or before `t`") are `bisect_right(ts, t) - 1`. Smallest batch size that fits a memory budget, smallest number of shards under a load cap, largest safe adaptive time step — all "minimise the maximum". The threshold at which two regions of a grid become connected (percolation) is binary search on a threshold with BFS or union-find as the check.

**Python vs C, once for the chapter:** `bisect.bisect_left` / `bisect_right` are `lower_bound` / `upper_bound`, implemented in C, and take a `key=` since 3.10. Indices and values are Python ints — no `(lo + hi) // 2` overflow, no `size_t` underflow, no `long long`. But every predicate search, every value-space search, and the two lines that decide `lo`/`hi` are still yours to write, and that is where the bugs live. Get the skeleton exact once and reuse it.

---

## 1. Plain sorted search

### The idea

Binary search halves the interval each step: `O(log n)`. The requirement is not "sorted list" — it is a **monotone predicate** `P(i)`, false up to some boundary and true from there on. A sorted list gives `P(i) = a[i] >= target` for free; the predicate can also be a callback (`isBadVersion(i)`), a computed quantity (`i*i <= num`), or three-way feedback (`guess(mid)`).

### The invariant — pick one form and never mix them

| | Closed `[lo, hi]` | Half-open `[lo, hi)` |
|---|---|---|
| Loop condition | `while lo <= hi` | `while lo < hi` |
| `mid` | `(lo + hi) // 2` | `(lo + hi) // 2` |
| Exclude left incl. `mid` | `lo = mid + 1` | `lo = mid + 1` |
| Exclude right | `hi = mid - 1` | `hi = mid` (never `mid - 1`) |
| Empty interval | `lo > hi` | `lo == hi` |
| Natural use | exact match, three-way feedback | boundary search (first index where `P` is true) |

In the half-open form `mid` rounds down, so `mid < hi` inside the loop and `hi = mid` shrinks strictly. In the closed form both branches step *past* `mid`. Termination follows.

The common bug is mixing them: `while lo <= hi` with `hi = mid`. When `hi == lo + 1`, `mid == lo`, `hi = mid` sets `hi = lo`, `lo <= hi` still holds, and `mid` is `lo` forever. **Python vs C:** `(lo + hi) // 2` cannot overflow — Python ints are arbitrary precision — so the C idiom `lo + (hi - lo) // 2` is habit, not necessity. Just make sure it is `//`, not `/`: a float `mid` raises `TypeError` on indexing.

### Python skeleton (closed form, exact match)

```python
def search_closed(a: list[int], target: int) -> int:
    """Index of target in ascending a, or -1. Invariant: if target exists it is in [lo, hi]."""
    lo, hi = 0, len(a) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if a[mid] == target:
            return mid
        if a[mid] < target:
            lo = mid + 1                 # everything up to and incl. mid is < t
        else:
            hi = mid - 1                 # everything from mid on is > t
    return -1                            # lo > hi: empty

# stdlib equivalent: i = bisect_left(a, target); found = i < len(a) and a[i] == target
```

### Worked trace

`a = [1, 3, 4, 7, 9, 12, 15]`, `target = 9`:

```
step  lo  hi  mid  a[mid]  action
  1    0   6   3     7     7 < 9   -> lo = 4
  2    4   6   5    12     12 > 9  -> hi = 4
  3    4   4   4     9     found, return 4
```

`target = 8`:

```
step  lo  hi  mid  a[mid]  action
  1    0   6   3     7     7 < 8   -> lo = 4
  2    4   6   5    12     12 > 8  -> hi = 4
  3    4   4   4     9     9 > 8   -> hi = 3
  4    lo=4 > hi=3, return -1
```

After the failed search `lo == 4` is exactly the insertion point. The half-open form in section 2 makes that the *primary* output.

### Searching a computed value, not a list

`i*i <= num` is monotone in `i`; `k(k+1)/2 <= n` is monotone in `k`. Binary search on `[0, num]` for the largest `i` satisfying the predicate is the same loop with `a[mid]` replaced by a formula. In C the bug is `mid * mid` overflowing; in Python `mid * mid` is exact for any size. `math.isqrt(x)` is the stdlib answer for the square-root case — exact integer, unlike `int(x ** 0.5)` which fails for large `x` due to float rounding.

### Recognising it

*Sorted*, *monotone*, *non-decreasing*, or the smallest/largest value satisfying a condition. Any interactive problem where each query is expensive and returns "too high / too low".

### Pitfalls

- Mixing `while lo <= hi` with `hi = mid` → infinite loop.
- Forgetting `+1` / `-1` in the closed form → infinite loop between two adjacent indices.
- `/` instead of `//` → `TypeError: list indices must be integers`.
- `hi = -1` when `n == 0` is fine in Python (no unsigned type) — but `a[-1]` *is* legal and reads the last element; make sure the loop condition rejects it before any read.

---

## 2. Lower bound, upper bound and boundary discipline

### The idea

Most problems ask for a **boundary**: the smallest index whose element is `>= target` (**lower bound**, `bisect_left`) or `> target` (**upper bound**, `bisect_right`). One half-open skeleton solves both — only the comparison changes. Write the body in exactly this shape every time:

```python
while lo < hi:
    mid = (lo + hi) // 2
    if P(mid):
        hi = mid            # mid might be the answer: keep it
    else:
        lo = mid + 1        # mid is definitely not: skip past
# answer is lo (== hi)
```

`P` is monotone, false on the left and true on the right. `hi` starts at `n`, so "no index satisfies `P`" is `lo == n` without a special case.

| Want | Predicate `P(mid)` | Result `lo` | stdlib |
|---|---|---|---|
| lower_bound(t): first `a[i] >= t` | `a[mid] >= t` | insertion point; first occurrence if `a[lo] == t` | `bisect_left(a, t)` |
| upper_bound(t): first `a[i] > t` | `a[mid] > t` | one past the last occurrence | `bisect_right(a, t)` |
| count of `t` | — | `bisect_right - bisect_left` | |
| last `a[i] <= t` | — | `bisect_right(a, t) - 1` (check `-1`) | |
| last `a[i] < t` | — | `bisect_left(a, t) - 1` | |

"Find first" and "find last" are one skeleton with two predicates.

### Python skeleton

```python
def lower_bound(a: list[int], t: int) -> int:
    """First i with a[i] >= t, or len(a). Invariant: a[:lo] < t, a[hi:] >= t."""
    lo, hi = 0, len(a)                  # half-open: hi is one past the last candidate
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] >= t:
            hi = mid                    # mid could be the boundary
        else:
            lo = mid + 1                # a[mid] < t: boundary is strictly right
    return lo

def upper_bound(a: list[int], t: int) -> int:
    lo, hi = 0, len(a)
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] > t:                  # one character changed
            hi = mid
        else:
            lo = mid + 1
    return lo
```

**Python vs C:** in production use `bisect.bisect_left(a, t, lo, hi, key=...)` — it is C-speed and accepts sub-ranges. Write the skeleton by hand when the predicate is not "compare to an element" (peak, window edge, parity) — `bisect` on a `range` object with `key=` can express those but obscures the structure.

### Worked trace

`a = [2, 4, 4, 4, 7, 9]`, `lower_bound(4)`:

```
step  lo  hi  mid  a[mid]  a[mid] >= 4 ?  action
  1    0   6   3     4        yes         hi = 3
  2    0   3   1     4        yes         hi = 1
  3    0   1   0     2        no          lo = 1
  4    lo == hi == 1  -> return 1
```

`upper_bound(4)`:

```
step  lo  hi  mid  a[mid]  a[mid] > 4 ?   action
  1    0   6   3     4        no          lo = 4
  2    4   6   5     9        yes         hi = 5
  3    4   5   4     7        yes         hi = 4
  4    lo == hi == 4  -> return 4
```

Count of 4s: `4 - 1 = 3`. Last index `<= 4`: `3`.

```
index:   0  1  2  3  4  5  (6)
a:       2  4  4  4  7  9
              ^        ^
       lower_bound=1   upper_bound=4
```

### Non-trivial predicates on the same skeleton

- **Peak finding**: `P(mid) = a[mid] > a[mid + 1]` on `[0, n - 1)`, so `mid + 1 <= n - 1` is always a valid read.
- **Window placement**: `k` closest to `x` — search the left edge `l` of `[l, l + k)` over `[0, n - k]` with `P(mid) = not (x - a[mid] > a[mid + k] - x)`.
- **Index parity**: sorted list, every element twice except one. Force `mid` even (`mid -= mid % 2`), `P(mid) = a[mid] != a[mid + 1]`, `lo = mid + 2` when the pair is intact.
- **Reverse lookup**: latest timestamp `<= t` is `bisect_right(ts, t) - 1`; `0` means "nothing that old".
- **Weighted sampling**: `prefix = list(accumulate(w))`, `r = randint(1, total)`, chosen index = `bisect_left(prefix, r)`. This is exactly what `random.choices(population, weights)` does.

### Recognising it

*First*, *last*, *closest*, *smallest that satisfies*, *insertion position*, *how many are <= x*.

### Pitfalls

- `hi = mid - 1` in the half-open skeleton discards a possible answer.
- Starting with `hi = n - 1` in the half-open skeleton: "not found" collides with "found at `n - 1`".
- `>=` vs `>` confusion — the wrap-around letter problem exists purely to test this.
- Reading `a[mid + 1]` when `hi` was not reduced to `n - 1`: `IndexError` (better than C's silent UB, still a bug).
- Thresholds like `success / spell` in floating point — rounding at the boundary flips the predicate. Use `-(-success // spell)` (ceiling division on ints) or compare the product.
- **`bisect` needs sorted input.** It does not check; unsorted input gives a meaningless index, no error.

---

## 3. Search in a rotated array

### The idea

A rotated sorted list (`[4, 5, 6, 7, 0, 1, 2]`) is not globally monotone, but **at every split at least one half is fully sorted**. Two-stage predicate per iteration:

1. Which half is sorted? If `a[lo] <= a[mid]`, the left half `[lo, mid]`; otherwise the right.
2. Is the target inside the sorted half (ordinary range check)? If yes, go there; else go to the other half.

To find the rotation point (minimum): compare `a[mid]` to `a[hi]`. If `a[mid] > a[hi]`, the minimum is strictly right of `mid`; else `mid` may be it. Comparing to `a[hi]` (not `a[lo]`) yields a clean two-way predicate.

### Duplicates break the reasoning

If `a[lo] == a[mid] == a[hi]`, neither half can be proven sorted. Shrink one step from both ends (`lo += 1; hi -= 1`). For the minimum search, `a[mid] == a[hi]` allows `hi -= 1`. Both drop the worst case to `O(n)` — unavoidable: `[3, 3, 1, 3]` vs `[3, 1, 3, 3]` look identical from the middle.

### Python skeleton (search for target, no duplicates)

```python
def rotated_search(a: list[int], t: int) -> int:
    lo, hi = 0, len(a) - 1                       # closed interval
    while lo <= hi:
        mid = (lo + hi) // 2
        if a[mid] == t:
            return mid
        if a[lo] <= a[mid]:                      # left half [lo, mid] is sorted
            if a[lo] <= t < a[mid]:              # chained comparison: t is in it
                hi = mid - 1
            else:
                lo = mid + 1
        else:                                    # right half [mid, hi] is sorted
            if a[mid] < t <= a[hi]:
                lo = mid + 1
            else:
                hi = mid - 1
    return -1

def rotated_min_index(a: list[int]) -> int:
    lo, hi = 0, len(a) - 1                       # half-open on [0, n-1)
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] > a[hi]:
            lo = mid + 1                         # mid is on the high plateau
        else:
            hi = mid                             # mid could be the minimum
    return lo                                    # a[lo] is the minimum; lo is the offset
```

Python's chained comparisons (`a[lo] <= t < a[mid]`) read exactly like the range test; use them.

### Worked trace

`a = [4, 5, 6, 7, 0, 1, 2]`, `t = 1`:

```
step  lo  hi  mid  a[lo] a[mid] a[hi]  sorted half   t in it?         action
  1    0   6   3     4     7      2     left [0,3]    4<=1<7 ? no      lo = 4
  2    4   6   5     0     1      2     found a[5]==1, return 5
```

Minimum:

```
step  lo  hi  mid  a[mid] a[hi]  action
  1    0   6   3     7      2     7 > 2  -> lo = 4
  2    4   6   5     1      2     1 <= 2 -> hi = 5
  3    4   5   4     0      1     0 <= 1 -> hi = 4
  4    lo == hi == 4 -> min is a[4] = 0, rotation offset 4
```

### The mountain family

A mountain array (strictly up then strictly down) is a rotated array's cousin. Peak = the section-2 predicate. Target in a mountain: peak search, ascending search on `[0, peak]`, then descending search (comparison flipped) on `[peak, n - 1]`. When element access is an expensive callback with a budget, cache every `get(mid)` in a local and never call it twice for the same index — `functools.lru_cache` on a wrapper does this for free.

### Recognising it

*Rotated*, *shifted*, *pivot*, *mountain array*. "Is this a rotation of a sorted array" is a linear pass counting descents `a[i] > a[(i + 1) % n]`: a valid rotation has at most one.

### Pitfalls

- Comparing `a[mid]` to `a[lo]` in the *minimum* search creates a third case when `lo == mid`; compare to `a[hi]`.
- `<` instead of `<=` in `a[lo] <= a[mid]` breaks the two-element case.
- Forgetting the duplicate collapse on `[1, 1, 1, 1, 1]` → infinite loop.
- Range checks must be half-open at the `mid` end because `a[mid] == t` was already handled.

---

## 4. Binary search on the answer (minimise the maximum)

### The idea

Search over the **possible values of the answer**. Reframe "minimise the largest chunk sum" as "is value `x` achievable?" — `feasible(x)` is monotone: for minimisation, if `x` works every larger `x` works; for maximisation, every smaller. Binary search finds the boundary with the same `[lo, hi)` skeleton.

1. **Choose the value range.** `[max(a), sum(a)]` for a capacity; `[1, max(a)]` for a speed/divisor; `[min, max]` of the data for a threshold. Prove both ends.
2. **Write `feasible(x)`.** Usually a **greedy simulation**: one pass that packs as tightly as possible at capacity `x`, then compares the count to the budget.

With `feasible` in `O(n)`, total is `O(n log(hi - lo))`.

### Direction matters

| Problem shape | Predicate | You want | Skeleton |
|---|---|---|---|
| Minimise the maximum | false…false true…true | **smallest** true | `if feasible(mid): hi = mid` else `lo = mid + 1` |
| Maximise the minimum | true…true false…false | **largest** true | `mid = (lo + hi + 1) // 2`; `if feasible(mid): lo = mid` else `hi = mid - 1` |

The second row rounds `mid` *up* so `lo = mid` makes progress. Write down "as `x` grows, count goes ___" before coding.

### Python skeleton

```python
def fits_in_parts(a: list[int], cap: int, parts: int) -> bool:
    """Greedy: open a new chunk only when forced. a[i] <= cap guaranteed by lo = max(a)."""
    used, cur = 1, 0
    for x in a:
        if cur + x > cap:
            used += 1
            cur = 0
        cur += x
    return used <= parts

def min_max_chunk_sum(a: list[int], parts: int) -> int:
    lo, hi = max(a), sum(a)                      # value range; hi is known feasible
    while lo < hi:
        mid = (lo + hi) // 2
        if fits_in_parts(a, mid, parts):
            hi = mid                             # smallest feasible is <= mid
        else:
            lo = mid + 1
    return lo
```

**Python vs C:** on 3.10+ the whole search is `bisect_left(range(lo, hi + 1), True, key=lambda cap: fits_in_parts(a, cap, parts)) + lo` — `bisect` accepts any sequence, `range` is a lazy sequence, and `False < True`. It is correct and fast, but the explicit loop shows the structure; use whichever your reader prefers.

### Worked trace

`a = [7, 2, 5, 10, 8]`, `parts = 2`. Range `[10, 32]`.

```
step  lo   hi   mid  greedy chunks at cap=mid          used  feasible?  action
  1   10   32   21   [7,2,5] [10,8]                      2     yes      hi = 21
  2   10   21   15   [7,2,5] [10] [8]                    3     no       lo = 16
  3   16   21   18   [7,2,5] [10,8]                      2     yes      hi = 18
  4   16   18   17   [7,2,5] [10] [8]  (10+8=18>17)      3     no       lo = 18
  5   lo == hi == 18 -> answer 18
```

### The `ceil` family

Hours to eat piles at speed `k` = `sum(ceil(p / k))`. Integer ceiling in Python: `-(-p // k)` or `(p + k - 1) // k` — never `math.ceil(p / k)`, which goes through a float and is wrong for large ints. Trips completed by time `t` = `sum(t // time_i)` and grows with `t`. Closed-form checks (arithmetic-series sums) are `O(1)` and, in Python, overflow-free.

### Recognising it

*Minimise the maximum*, *maximise the minimum*, *smallest speed / capacity / divisor / days that suffices*, *largest gap achievable*.

### Pitfalls

- Value range too tight (`lo = 1` for a capacity when one item alone exceeds it).
- Impossible instances belong *before* the search (`m * k > n` flowers → `-1`). A search over an all-false predicate returns `hi`, which silently looks like an answer.
- Direction wrong: smallest-true vs largest-true.
- `math.ceil(p / k)` on large ints; `p // k + 1` when `k` divides `p`.

---

## 5. Search in a 2D matrix

### The idea

The right technique depends on **exactly what ordering is guaranteed**.

**Globally sorted** (each row begins after the previous ends): treat the `m x n` matrix as one flat sorted list of length `m*n`. 1D search on `[0, m*n - 1]` with `r, c = divmod(mid, n)`.

**Rows and columns each sorted, not globally**: **staircase walk** from the top-right. Too large → move left (whole column below is larger); too small → move down. Each step kills one row or column: `O(m + n)`. Top-left or bottom-right corners do not work — both moves change the value in the same direction.

**k-th smallest / value queries**: binary search on the **value** with `count_le(x)` as the predicate, computed by a staircase from the bottom-left in `O(m + n)`. The boundary of a monotone counting predicate lands on an actual matrix value, even though the search range `[a[0][0], a[-1][-1]]` contains values not in the matrix. Section 4 in 2D.

### Python skeleton (staircase walk)

```python
def staircase_find(a: list[list[int]], t: int) -> bool:
    m, n = len(a), len(a[0])
    r, c = 0, n - 1                              # top-right corner
    while r < m and c >= 0:
        v = a[r][c]
        if v == t:
            return True
        if v > t:
            c -= 1                               # everything below in this column is > t
        else:
            r += 1                               # everything left in this row is < t
    return False

def count_le(a: list[list[int]], x: int) -> int:
    m, n = len(a), len(a[0])
    r, c, cnt = m - 1, 0, 0                      # bottom-left
    while r >= 0 and c < n:
        if a[r][c] <= x:
            cnt += r + 1                         # whole column above incl. r is <= x
            c += 1
        else:
            r -= 1
    return cnt

def kth_smallest(a: list[list[int]], k: int) -> int:
    lo, hi = a[0][0], a[-1][-1]
    while lo < hi:
        mid = (lo + hi) // 2
        if count_le(a, mid) >= k:
            hi = mid
        else:
            lo = mid + 1
    return lo
```

**Python vs C:** `c >= 0` must be explicit — `a[r][-1]` would silently read the last column and the walk would never terminate correctly. The flatten trick on a list-of-lists needs `divmod`; a NumPy array does it with `a.ravel()` (a view, no copy, when contiguous).

### Worked trace

```
      c0  c1  c2  c3
r0     1   4   7  11
r1     2   5   8  12
r2     3   6   9  16
r3    10  13  14  17
```

`staircase_find(9)`:

```
step  (r,c)  v    action
  1   (0,3)  11   11 > 9  -> c = 2
  2   (0,2)   7   7 < 9   -> r = 1
  3   (1,2)   8   8 < 9   -> r = 2
  4   (2,2)   9   found
```

`count_le(8)` from bottom-left:

```
step  (r,c)  v    action                     cnt
  1   (3,0)  10   10 > 8  -> r = 2            0
  2   (2,0)   3   3 <= 8  -> cnt += 3, c=1    3
  3   (2,1)   6   6 <= 8  -> cnt += 3, c=2    6
  4   (2,2)   9   9 > 8   -> r = 1            6
  5   (1,2)   8   8 <= 8  -> cnt += 2, c=3    8
  6   (1,3)  12   12 > 8  -> r = 0            8
  7   (0,3)  11   11 > 8  -> r = -1, stop     8
```

Eight entries `<= 8`, so for `k = 8` the outer search on `[1, 17]` lands on `x = 8`.

### Other 2D shapes in this unit

- **Per-row boundary**: rows like `1 1 1 0 0` — soldiers per row is `bisect_left(row, 0)` if you view the row as descending, or `n - bisect_left(row[::-1]... )`; simpler: `row.index(0)` is `O(n)` and often fine, but the log version is `bisect_left(range(n), True, key=lambda j: row[j] == 0)`.
- **Count negatives in a descending-sorted matrix**: staircase from top-right, or `bisect` per row on the negated row.
- **2D peak**: binary search over *columns*; for `mid` column take `max(range(m), key=lambda r: a[r][mid])`, compare with neighbours, move toward the larger. `O(m log n)`.
- **Largest square with sum `<= threshold`**: 2D prefix sums, then search the side length `k` on `[0, min(m, n)]`.
- **k-th smallest sum choosing one per row**: value-space search where the counter is a bounded row-by-row merge.

### Recognising it

*Matrix*, *rows sorted and columns sorted*, *k-th smallest in a matrix*, *each row starts after the previous ends*. Ask first: globally sorted, or only per row/column?

### Pitfalls

- Flatten trick on a row/column-sorted (not globally sorted) matrix — wrong, not just slow.
- Staircase from the wrong corner.
- Letting `c` go to `-1` — Python wraps instead of stopping.
- `count_le`: the "add the whole column" step adds `r + 1`, not `1`.

---

## 6. Binary search plus greedy / graph feasibility checks

### The idea

`feasible(x)` may **need its own algorithm** — a greedy construction, BFS/DFS reachability, or union-find connectivity. Guess `x`, check with a separate algorithm. Binary search only locates the boundary.

1. **Argue monotonicity.** "If the route succeeds with maximum step effort `x`, it succeeds with any larger `x`" — a larger threshold admits a superset of edges. Say it out loud before coding.
2. **Write the check as its own function.** On grids: BFS with `collections.deque` from a source using only cells that pass the threshold.

Total `T(feasible) * log(range)`. `log(10^6) = 20` BFS passes is cheap.

### Preprocessing before the search

Multi-source BFS from all thieves gives every cell its distance to the nearest thief once; the search on `s` then asks "can I cross using only cells with safety `>= s`". Fire-escape: BFS from the fire gives ignition times, then each check runs a *second* BFS for the person with `w + dist < ignition[cell]` (equality allowed only at the goal). Keep the precomputed grid and the per-check grid separate.

### Python skeleton (threshold + BFS on a grid)

```python
from collections import deque

def reachable_under(h: list[list[int]], t: int) -> bool:
    """(0,0) -> (m-1,n-1) using only cells with h <= t. Iterative BFS: no recursion limit."""
    m, n = len(h), len(h[0])
    if h[0][0] > t:
        return False
    seen = [[False] * n for _ in range(m)]       # NOT [[False]*n]*m
    seen[0][0] = True
    q: deque[tuple[int, int]] = deque([(0, 0)])
    while q:
        r, c = q.popleft()
        if (r, c) == (m - 1, n - 1):
            return True
        for nr, nc in ((r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)):
            if 0 <= nr < m and 0 <= nc < n and not seen[nr][nc] and h[nr][nc] <= t:
                seen[nr][nc] = True
                q.append((nr, nc))
    return False

def min_level(h: list[list[int]]) -> int:
    lo, hi = 0, max(map(max, h))
    while lo < hi:
        mid = (lo + hi) // 2
        if reachable_under(h, mid):
            hi = mid
        else:
            lo = mid + 1
    return lo
```

**Python vs C:** recursive DFS as the check hits the ~1000 recursion limit on a `1000 x 1000` grid — `sys.setrecursionlimit` postpones the problem and risks a segfault; use the explicit `deque`. Allocating `seen` per check is fine (~20 checks); a flat `bytearray(m * n)` is the faster allocation if it matters.

### Worked trace

Smallest water level `t` at which `(0,0)` connects to `(2,2)` through cells `<= t`:

```
 0  2  6
 5  3  7
 8  4  1
```

Range `[0, 8]`.

```
step  lo  hi  mid  cells usable (<= mid)              (0,0)->(2,2)?  action
  1    0   8   4   0,2,3,4,1                           yes            hi = 4
  2    0   4   2   0,2,1                               no             lo = 3
  3    3   4   3   0,2,3,1                             (2,1)=4 blocked, no   lo = 4
  4    lo == hi == 4 -> answer 4
```

Union-find alternative: sort cells by height, union each with already-active neighbours, stop when `find(0) == find(m*n-1)` — no explicit binary search, covered in Chapter 06.

### Maximise vs minimise, again

"Last day the crossing is still possible" and "safest path" are the maximisation mirror: true on the low side, false on the high side; use the upper-rounding `mid` or search for the first false and subtract one.

### Recognising it

A threshold parameter plus *path*, *reachable*, *connected*, *cross*, *escape*, *effort*, *safety*. Or a greedy construction whose cost depends monotonically on one parameter.

### Pitfalls

- Recursive DFS as the check → `RecursionError` on large grids.
- Equality at the goal: "person arrives at the same tick as the fire" allowed at the goal cell only.
- The "infinite wait" special case when the fire can never reach the route.
- Assuming monotonicity without arguing it — the search returns garbage silently.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Complexity |
|---|---|---|
| sorted list, find target / does it exist | closed exact search, or `bisect_left` + one compare (§1) | `O(log n)` |
| too high / too low feedback, expensive query | closed search on a callback (§1) | `O(log n)` calls |
| integer sqrt, largest `k` with `f(k) <= n` | search on a computed value; `math.isqrt` (§1) | `O(log n)` |
| first / last / insertion point / how many `<= x` | `bisect_left` / `bisect_right` (§2) | `O(log n)` |
| peak, local maximum | boundary search, `P(mid) = a[mid] > a[mid+1]` (§2) | `O(log n)` |
| k closest, window of size k | search the window's left edge (§2) | `O(log(n-k) + k)` |
| latest value at or before time t | `bisect_right(ts, t) - 1` (§2) | `O(log n)` per query |
| random pick weighted by w | `accumulate` + `bisect_left`; `random.choices` (§2) | `O(n)` build, `O(log n)` pick |
| median of two sorted arrays | search the partition point in the shorter (§2) | `O(log min(m,n))` |
| rotated, shifted, pivot | decide sorted half, then range test (§3) | `O(log n)`; `O(n)` with duplicates |
| mountain array | peak search + two directional searches (§3) | `O(log n)` |
| minimise the maximum / smallest capacity, speed, days | value-space search, smallest feasible, greedy check (§4) | `O(n log range)` |
| maximise the minimum / largest gap, threshold | value-space search, largest feasible (§4, §6) | `O(n log range)` |
| matrix, each row starts after previous ends | flatten with `divmod`, 1D search (§5) | `O(log mn)` |
| matrix, rows and columns sorted | staircase from top-right / bottom-left (§5) | `O(m + n)` |
| k-th smallest in a sorted matrix | value search + staircase count (§5) | `O((m+n) log range)` |
| largest square under a sum threshold | 2D prefix sums + search side length (§5) | `O(nm log min(n,m))` |
| path / reachable / connected under a threshold | value search + `deque` BFS / union-find check (§6) | `O(nm log range)` |
| two BFS layers (fire + person) | precompute one BFS, search, second BFS as check (§6) | `O(nm log(nm))` |

---

## Gotchas in Python specifically

- **`bisect` needs sorted input** and never checks. `bisect_left(a, x)` on an unsorted list returns *some* index with no error. Sort first, or verify with `all(a[i] <= a[i+1] for i in range(len(a)-1))` in a test.
- **`bisect` key= is 3.10+** and applies to the list elements only, not to `x`. To search a `range` with a predicate: `bisect_left(range(lo, hi), True, key=pred)` — `False < True` makes booleans a monotone key.
- **`//` vs `/`.** `mid = (lo + hi) / 2` is a float; `a[mid]` raises `TypeError`. Floor division rounds toward `-inf`: `(-1 + 0) // 2 == -1`, which matters only if your interval goes negative.
- **Negative indices wrap.** `a[-1]` is the last element. A staircase walk that lets `c` reach `-1`, or a closed search that reads `a[hi]` when `hi == -1`, gives wrong answers silently. Every read must be guarded by the loop condition.
- **Recursion limit ~1000.** DFS as a feasibility check on a `10^3 x 10^3` grid must be iterative (`deque` BFS or an explicit list stack). `sys.setrecursionlimit(10**6)` can segfault the interpreter.
- **`math.ceil(a / b)` goes through a float** — wrong for ints beyond 2^53. Use `-(-a // b)` or `(a + b - 1) // b`.
- **`int(x ** 0.5)` is wrong for large `x`**; use `math.isqrt`.
- **No overflow, ever.** `(lo + hi) // 2`, `mid * mid`, `k * (k + 1) // 2` are exact. The C defensive idioms are not needed, but big-int arithmetic gets slower past 64 bits.
- **`[[False] * n] * m`** aliases rows — the `seen` grid must be `[[False] * n for _ in range(m)]`.
- **`list.pop(0)` is `O(n)`** — the BFS queue must be a `collections.deque`.
- **Float thresholds.** `success / spell >= k` at the boundary can round the wrong way; compare `success >= k * spell` on ints.
- **`is` vs `==`.** Compare ints with `==`; `mid is lo` is meaningless.
- **Mutable default args.** `def bfs(grid, seen=[])` shares `seen` across the ~20 checks. Use `None`.
- **Speed.** A Python `feasible` at `n = 10^5` runs ~30 times for `log(10^9)`: `3 * 10^6` loop iterations, roughly a second. Fine. `sum(-(-p // k) for p in piles)` is faster than an explicit loop.

---

## Common mistakes checklist

- [ ] `while lo <= hi` paired with `hi = mid` (infinite loop) or `while lo < hi` paired with `hi = mid - 1` (skips the answer).
- [ ] `/` instead of `//` for `mid`.
- [ ] Half-open search started with `hi = n - 1` instead of `hi = n`.
- [ ] `>=` and `>` confused between `bisect_left` and `bisect_right`.
- [ ] Reading `a[mid + 1]` without shrinking `hi` to `n - 1`.
- [ ] Minimum-in-rotated compared against `a[lo]` instead of `a[hi]`.
- [ ] Duplicates in a rotated array with no collapse step.
- [ ] Value-space range that excludes the true answer.
- [ ] Impossible instances not rejected before the search.
- [ ] Direction of monotonicity not written down.
- [ ] `math.ceil(p / k)` or `p // k + 1` for integer ceiling.
- [ ] Staircase from top-left or bottom-right; `c` allowed to reach `-1`.
- [ ] Flatten trick on a row/column-sorted matrix.
- [ ] Feasibility assumed monotone without an argument.
- [ ] Recursive DFS as the check on a large grid.
- [ ] `bisect` on unsorted input.
- [ ] Interactive callback invoked twice for the same index (wrap it in `lru_cache`).

---

## You can move on when...

- You can write `lower_bound` and `upper_bound` from memory in the half-open skeleton, explain why `hi = mid` cannot loop forever, and name their `bisect` equivalents.
- Given a boundary problem, you can name the predicate `P(i)` in one sentence and say which side is true before touching the keyboard.
- You can trace the rotated-array search on a 7-element input by hand, including the duplicate case, and state why duplicates cost `O(n)`.
- You can take a "minimise the maximum" problem you have not seen, name the value range with a justification for both ends, write `feasible` as a greedy pass, and pick the correct direction — for at least two of: ship capacity, Koko, smallest divisor, split array.
- You can explain which matrix ordering allows the flatten trick, which requires the staircase, and why the staircase starts at the top-right.
- You have written one threshold + `deque` BFS check and it passes on a small grid you traced by hand.
- `example.py` runs clean with `python3 example.py`.
