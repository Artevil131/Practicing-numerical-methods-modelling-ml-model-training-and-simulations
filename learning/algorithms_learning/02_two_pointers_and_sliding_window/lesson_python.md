# Chapter 02 — Two pointers & sliding window — Python

## What you'll be able to do after this chapter

- Replace an `O(n^2)` "all pairs" scan over a **sorted** list with two opposite-end indices that meet in the middle, and prove why no valid pair is skipped.
- Detect a cycle in a linked list or in any "each node has exactly one successor" structure in `O(1)` space with Floyd's tortoise-and-hare, and locate where the cycle starts.
- Maintain a window statistic (sum, count, max via `collections.deque`) over every length-`k` subarray in `O(n)` instead of `O(nk)`.
- Grow and shrink a variable window with a monotone predicate to find the longest / shortest valid subarray, and count subarrays with the `atMost(k) - atMost(k-1)` trick.
- Upgrade the window's state from a number to a `Counter` plus a `matched` counter, so anagram / covering / at-most-k-distinct problems stay `O(1)` per step.
- Partition a list in place into two or three regions with read/write pointers or the Dutch national flag scheme, and reuse the same partition inside an iterative quickselect.

## Why this matters for ML / numerics / sims

Every pattern here visits an array *once* while keeping a small piece of state — the shape of almost every data-processing kernel:

- **Fixed window = convolution / moving average / FIR filter.** `sum += x[r] - x[l]` per step. A running variance for batch-norm statistics is the same update on two accumulators.
- **Variable window = admission control and streaming statistics.** "Longest run where the loss stays under a threshold", "shortest span of tokens covering all required symbols".
- **Monotonic deque = max-pooling in `O(n)`.** Sliding-window maximum is 1-D max-pool with stride 1, without a heap.
- **Opposite pointers = merging and pairing sorted data.** Matching sorted timestamps, pairing sorted particle positions.
- **Fast/slow = cycle detection in iterated maps.** `x <- f(x)` either converges or cycles; Floyd finds the cycle without storing the history.
- **In-place partition = quickselect.** Median or `k`-th percentile of a residual vector in expected `O(n)` — `np.partition`.

In NumPy you call `np.convolve`, `np.partition`. Writing the five-line loop yourself, you must know the invariant that makes it right.

**Python vs C, once for the chapter:** indices are plain ints that never underflow (`right = -1` is a valid index that silently wraps to the last element — a different trap). Sums and products never overflow. `collections.deque` replaces the hand-rolled index array. Speed is ~50-100× slower, but every pattern here is `O(n)`, which LeetCode limits allow.

---

## 1. Opposite ends

### The idea

Two indices, `left` at the start and `right` at the end, move towards each other until they meet. Applies when the list is **sorted** (or sorting is acceptable preprocessing) and the answer depends on the relationship between two elements — sum, difference, symmetry.

### Why it is correct, not just fast

If `a[left] + a[right]` is too large, increasing `left` can never help — only moving `right` leftwards reduces the sum. Each step rules out an **entire set** of pairs at once. That is why `O(n^2)` collapses to `O(n)`.

**Invariant:** no valid pair exists with both indices inside the regions already passed over. The pointers never move back.

### Recognising it

Signal words: *sorted array*, *palindrome*, *pair whose sum / product satisfies a condition*, *two walls / containers*.

| Decision rule | Example | What it compares |
|---|---|---|
| Move based on the **value of the objective** | Two Sum II, 3Sum | `a[l] + a[r]` vs `target` |
| Move the pointer that **limits** the answer | Container With Most Water, Trapping Rain Water | the shorter wall |

### Complexity

`O(n)` time — each pointer moves at most `n` steps in total, regardless of inner skip loops (amortised). `O(1)` extra space.

### Python layout

A list plus two `int` indices. No `size_t` underflow to fear, but remember `a[-1]` is legal Python — a loop that runs `right` below 0 reads the wrong end instead of crashing.

```python
def pair_sum_sorted(a: list[int], target: int) -> tuple[int, int] | None:
    left, right = 0, len(a) - 1
    while left < right:
        s = a[left] + a[right]
        if s == target:
            return left, right
        if s < target:
            left += 1            # too small: only left can fix it
        else:
            right -= 1           # too big: only right can fix it
    return None
```

Palindrome check: `s == s[::-1]` is the idiom, but it copies (`O(n)` memory). The two-pointer loop is what you write when filtering (skip non-alphanumerics via `str.isalnum`).

### Worked example

`a = [1, 3, 4, 6, 8, 11]`, `target = 10`.

```
l=0 r=5  1+11 = 12 > 10  -> r--
l=0 r=4  1+8  =  9 < 10  -> l++
l=1 r=4  3+8  = 11 > 10  -> r--
l=1 r=3  3+6  =  9 < 10  -> l++
l=2 r=3  4+6  = 10       -> found (2, 3)
```

Five comparisons instead of fifteen pairs.

### Pitfalls

- Reversing in place: `while left < right` handles both even and odd lengths. (`a.reverse()` exists; the loop is for the pattern.)
- Palindrome with filtering: inner skip loops must also test `left < right`.
- Squares of a Sorted Array: a third index writes the output backwards from `n - 1`.
- 3Sum: after `a.sort()`, skip duplicates on **all three** indices.

---

## 2. Fast & slow pointers (cycle detection)

### The idea

Floyd's cycle detection moves two pointers in the **same** direction at different speeds: `slow` one step, `fast` two. It applies to **linked lists** and to **function graphs** — structures where every node has exactly one successor (Happy Number's digit-square-sum, `i -> nums[i]` in Find the Duplicate Number).

### Why it works

If there is a cycle, `fast` enters it first and gains exactly one node per step on `slow` inside the cycle. The gap shrinks by one each step, so `fast` **catches** `slow` within one lap. If there is no cycle, `fast` reaches `None`.

### Finding the cycle start (phase two)

Let `a` = head-to-entry distance, `b` = entry-to-meeting distance, `L` = cycle length. At the meeting `slow` walked `a + b`, `fast` walked `2(a + b)`, so `a + b = kL` and `a = (k-1)L + c` where `c = L - b`. Walking `a` steps from the head and from the meeting point both land on the entry — reset one pointer to head, advance both one step at a time.

### Recognising it

Signal words: *linked list*, *cycle*, *middle of the list*, *apply a function repeatedly until a value repeats*, *array where values are valid indices*.

### Complexity

`O(n)` time, `O(1)` space. The Python-natural alternative — `seen = set()` — is also `O(n)` time but `O(n)` space; use Floyd when the problem says "constant space".

### Python layout

A tiny class for the list node (the C used a struct). For a function graph, the "pointers" are ints and "advance" is `x = f(x)`.

```python
class Node:
    __slots__ = ("val", "next")
    def __init__(self, val: int, next: "Node | None" = None) -> None:
        self.val, self.next = val, next

def cycle_start(head: Node | None) -> Node | None:
    slow = fast = head
    while fast and fast.next:                    # guard BOTH hops
        slow, fast = slow.next, fast.next.next
        if slow is fast:                         # identity, not equality
            p = head
            while p is not slow:
                p, slow = p.next, slow.next
            return p
    return None
```

**Python vs C:** compare nodes with `is`, never `==` — two distinct nodes with equal `val` are different nodes. `slow, fast = slow.next, fast.next.next` evaluates the right side first, so the double hop is safe.

### Worked example (function graph)

`nums = [3, 1, 3, 4, 2]`. Successor of `i` is `nums[i]`.

```
0 -> 3 -> 4 -> 2 -> 3 -> 4 -> 2 ...        entry is node 3 (a = 1, L = 3)

phase 1:  step  slow  fast
           0     0     0
           1     3     4
           2     4     3
           3     2     2       met at 2
phase 2:  p=0 slow=2 -> p=3 slow=3  -> entry 3
```

3 is the duplicated value.

### Pitfalls

- `while fast and fast.next` before the double step; a two-node acyclic list otherwise raises `AttributeError` on `None.next`.
- Middle of the list: `while fast and fast.next` gives the *second* middle on even lengths; `while fast.next and fast.next.next` gives the first.
- The phase-one meeting point is **not** the cycle start.
- In an array-as-function-graph do not modify or sort the array.
- Circular Array Loop: check direction-consistency and cycle length > 1 on every step.

---

## 3. Fixed-size window

### The idea

For a quantity over **every subarray of exactly length `k`**, brute force is `O(nk)`. Sliding a fixed window is `O(n)`: one element leaves on the left, one enters on the right, so the state updates in `O(1)`.

### Invariant

The window state always equals the state of `[right - k + 1, right]`. Compute the first window directly, then every shift subtracts the leaving element and adds the entering one.

### Why this is correct

The size is fixed, so "remove left, add right" is always the same operation. Sliding Window Maximum needs a **monotonic deque** because a counter cannot recover the max when the leaving element *was* the max.

### Recognising it

Signal words: *exactly `k` long*, *every window of size `k`*, *moving average*, *choose `k` consecutive*, and "take `k` from the two ends" = "leave a block of `n - k` in the middle".

### Complexity

`O(n)` time, `O(1)` space for sum/count; `O(k)` for the deque.

### Python layout

One int accumulator (no overflow). For counts over a fixed alphabet, `[0]*26` or a `Counter`. For the maximum, `collections.deque` of **indices**: `append`/`pop` at the right and `popleft` at the left are all `O(1)` — a plain list's `pop(0)` is `O(n)` and turns the algorithm into `O(nk)`.

```python
def window_sums(a: list[int], k: int) -> list[int]:
    s = sum(a[:k])                               # first window directly
    out = [s]
    for right in range(k, len(a)):
        s += a[right] - a[right - k]             # add entering, remove leaving
        out.append(s)
    return out
```

Monotonic deque for window max (indices, values decreasing from head to tail):

```python
from collections import deque

def window_max(a: list[int], k: int) -> list[int]:
    dq: deque[int] = deque()
    out: list[int] = []
    for r, x in enumerate(a):
        while dq and a[dq[-1]] <= x:             # pop smaller-or-equal from back
            dq.pop()
        dq.append(r)
        if dq[0] <= r - k:                       # front fell out of the window
            dq.popleft()
        if r >= k - 1:
            out.append(a[dq[0]])
    return out
```

### Worked example

`a = [2, 1, 5, 1, 3, 2]`, `k = 3`, running sums:

```
[2 1 5] . . .   sum = 8
. [1 5 1] . .   sum = 8 + 1 - 2 = 7
. . [5 1 3] .   sum = 7 + 3 - 1 = 9   <- max
. . . [1 3 2]   sum = 9 + 2 - 5 = 6
```

Monotonic deque (`k = 3`), `dq` as indices with values in brackets:

```
r=0 a=2   dq=[0(2)]
r=1 a=1   dq=[0(2) 1(1)]
r=2 a=5   pop 1(1), pop 0(2)     dq=[2(5)]              window [0,2] max 5
r=3 a=1   dq=[2(5) 3(1)]                                window [1,3] max 5
r=4 a=3   pop 3(1)               dq=[2(5) 4(3)]         window [2,4] max 5
r=5 a=2   dq=[2(5) 4(3) 5(2)]    front 2 <= 5-3 -> drop  dq=[4(3) 5(2)]  window [3,5] max 3
```

### Pitfalls

- Divide for an average **once at the end**; compare `s >= k * threshold` rather than `s / k >= threshold` (float).
- Contains Duplicate II: remove the leaving element *before* the membership test (or consistently after); if values repeat inside the window use a dict to the last index.
- Cards / bookstore: handle `k >= n` separately.
- The deque stores **indices**, never values.
- `sum(a[:k])` slices — fine once; never inside the loop.

---

## 4. Variable window (longest / shortest)

### The idea

`right` grows the window while the condition holds; when it breaks, `left` shrinks until it holds again. For the **longest** or **shortest** subarray satisfying a condition on a sum, product, or count.

### Why it is correct, not just fast

**Monotonicity**: if `[left, right]` satisfies the condition, every smaller window inside it does too (or the mirror for failing). Once `left` moved right for the current `right`, it never has to move back.

Monotonicity is a property of the *predicate* — check it first. "Sum ≥ target" with all-positive values is monotone. "Sum == target" with negatives is not (that is prefix-sum + dict, Chapter 01). "Exactly `k` odd" is not — but "at most `k`" is.

### Invariant and amortised time

`left` and `right` together move at most `2n` steps, so the total is `O(n)` even when the inner loop runs many times for one `right`.

### Two rhythms

| Goal | When condition breaks | Shrink how far | Record answer when |
|---|---|---|---|
| **Longest** valid window | shrink `left` | until **just** valid again | after every `right` step |
| **Shortest** valid window | condition **becomes true** | as far as possible while valid | at every valid shrink step |

### Counting subarrays: `atMost(k) - atMost(k-1)`

If `[left, right]` is the longest valid window ending at `right`, all `right - left + 1` subarrays ending at `right` are valid — add that count each step. `exactly(k) = atMost(k) - atMost(k-1)`. Make `atMost(-1)` return 0.

### Complexity

`O(n)` time, `O(1)` space.

### Python layout

`left = 0`, `for right in range(n)`, inner `while not valid: ...; left += 1`. Answer initialised to `0` (longest) or `float('inf')` / `n + 1` (shortest), mapped to the problem's sentinel at the end.

```python
def longest_with_k_zeros(a: list[int], k: int) -> int:
    left = zeros = best = 0
    for right, x in enumerate(a):
        zeros += x == 0                          # bool is an int: True == 1
        while zeros > k:                         # broken: shrink until just valid
            zeros -= a[left] == 0
            left += 1
        best = max(best, right - left + 1)
    return best

def shortest_sum_at_least(a: list[int], target: int) -> int:
    left = s = 0
    best = len(a) + 1
    for right, x in enumerate(a):
        s += x
        while s >= target:                       # valid: shrink as far as possible
            best = min(best, right - left + 1)
            s -= a[left]
            left += 1
    return 0 if best == len(a) + 1 else best
```

### Worked example

`a = [1, 1, 0, 1, 0, 1, 1]`, `k = 1`:

```
r=0 a=1 zeros=0  [1]                    len 1
r=1 a=1 zeros=0  [1 1]                  len 2
r=2 a=0 zeros=1  [1 1 0]                len 3
r=3 a=1 zeros=1  [1 1 0 1]              len 4   <- best
r=4 a=0 zeros=2  > k: drop a[0], a[1], a[2] -> zeros=1, left=3
                 [1 0]                  len 2
r=5 a=1 zeros=1  [1 0 1]                len 3
r=6 a=1 zeros=1  [1 0 1 1]              len 4
answer 4
```

`left` moved 0 -> 3 in one `right` step, but never moves back.

### Pitfalls

- Longest: shrink until *just* valid. Shortest: shrink *as far as possible*.
- Update the answer at **every** step, not only when the condition breaks.
- Shortest-window problems return `0` when nothing qualifies, not the sentinel.
- Product windows: if `k <= 1` return 0 before the loop. (No overflow in Python, but the product can get huge — the shrink keeps it `< k * max_value`.)
- Longest Substring Without Repeating: jumping `left` straight past the duplicate needs a last-index `dict`, not a `set`.

---

## 5. Window with a counter / frequency map

### The idea

The validity condition is a **whole frequency distribution** — "contains every char of `t` at least the required number of times", "is an anagram of `p`", "has at most `k` distinct values". Maintain a `Counter` (or `[0]*26`) for the window plus one int (`matched` or `distinct`) so the validity test stays `O(1)`.

### Invariant

The map describes exactly the current window; the helper counter says how many keys currently meet their target. Both update in `O(1)` per move.

### Why correctness carries over

Same monotonicity as Section 4; only the *state* differs.

### The `matched` bookkeeping

For target `need[c]` and window `have[c]`:

- Add `c`: `have[c] += 1`; if `have[c] == need[c]`: `matched += 1`.
- Remove `c`: if `have[c] == need[c]`: `matched -= 1`; then `have[c] -= 1`.
- Valid when `matched == len(need)`.

For "at most `k` distinct": `distinct` = number of keys with `have[c] > 0`; increment on `0 -> 1`, decrement on `1 -> 0`. Forgetting to drop a key at zero is the classic Fruit Into Baskets bug. With a `Counter`, `len(have)` is *wrong* after decrements unless you `del have[c]` when it hits 0 — a zero-count key still counts toward `len`.

### Recognising it

Signal words: *anagram / permutation inside a string*, *cover all characters of X*, *at most / exactly `k` distinct*, *replace at most `k` characters*, *two baskets*.

### Complexity

`O(n + m)` time, `O(sigma)` or `O(k)` space.

### Python layout

`collections.Counter` or `defaultdict(int)`; for lowercase strings `[0]*26` with `ord(c) - 97` is faster. No `unsigned char` cast — Python strs are Unicode code points and dicts take any hashable key.

```python
from collections import defaultdict

def longest_k_distinct(s: str, k: int) -> int:
    have: dict[str, int] = defaultdict(int)
    distinct = left = best = 0
    for right, c in enumerate(s):
        if have[c] == 0:
            distinct += 1                        # 0 -> 1: new key
        have[c] += 1
        while distinct > k:
            d = s[left]; left += 1
            have[d] -= 1
            if have[d] == 0:
                distinct -= 1                    # 1 -> 0: key gone
        best = max(best, right - left + 1)
    return best
```

### Worked example

`s = "eceba"`, `k = 2`:

```
r=0 'e'  have{e:1}           distinct=1  [e]       len 1
r=1 'c'  have{e:1,c:1}       distinct=2  [ec]      len 2
r=2 'e'  have{e:2,c:1}       distinct=2  [ece]     len 3  <- best
r=3 'b'  have{e:2,c:1,b:1}   distinct=3  > k
         drop 'e' -> e:1                 distinct=3
         drop 'c' -> c:0                 distinct=2  left=2   [eb]  len 2
r=4 'a'  have{e:1,b:1,a:1}   distinct=3  > k
         drop 'e' -> e:0                 distinct=2  left=3   [ba]  len 2
answer 3
```

### Pitfalls

- `matched` counts **distinct keys** satisfied, not characters.
- Anagram problems are a **fixed** window plus a histogram (Sections 3 + 5). Record `left` as the match position.
- `Counter(window) == Counter(p)` at each step is `O(26 n)` — correct, not optimal.
- Longest Repeating Character Replacement: do **not** recompute `maxFreq` downward when shrinking.
- Maximum Erasure Value: a set for validity and a sum for the answer — update both on every add and remove.

---

## 6. Partitioning in place

### The idea

Split a list into regions in one pass with no extra memory — the `partition` step of quicksort. The two-region version reads with one pointer and writes with a slower one: accepted elements are written at `write` and `write` advances.

### Invariant (two regions)

`[0, write)` contains exactly the accepted elements so far in original order. `read >= write` always, so a write never destroys unread data.

### Three regions: the Dutch national flag

`low` (next slot for class 0), `mid` (under inspection), `high` (next slot for class 2). Regions: `[0, low)` = 0, `[low, mid)` = 1, `[mid, high]` = unknown, `(high, n-1]` = 2.

- `a[mid] == 1`: `mid += 1`.
- `a[mid] == 0`: swap `a[low], a[mid]`; `low += 1` **and** `mid += 1` (the element from `low` was a known 1).
- `a[mid] == 2`: swap `a[mid], a[high]`; `high -= 1` **only** — the element from `high` is unknown.

That asymmetry is the whole difficulty.

### Stable versus unstable

| Keep relative order? | Technique | Example |
|---|---|---|
| No | opposite pointers swapping across the boundary | Sort Array By Parity, Sort Colors |
| Yes | write/read pointer, or list comprehensions into a fresh list | Move Zeroes, Partition by Pivot |

**Python vs C:** the stable three-way partition in Python is `[x for x in a if x < p] + [x for x in a if x == p] + [x for x in a if x > p]` — `O(n)` extra memory but obviously correct. Write the in-place version only when the problem demands it.

### Quickselect

Partition around a pivot; if it lands at the target rank, done; else recurse into one side only. Expected `O(n)` with random pivots; worst `O(n^2)`. "`k`-th largest" is rank `n - k` ascending. Write it as a `while` loop — recursion depth would be `O(n)` worst case against Python's ~1000 limit. `heapq.nlargest(k, a)[-1]` is the `O(n log k)` alternative; `sorted(a)[k]` the `O(n log n)` one.

### Recognising it

Signal words: *in place without extra memory*, *move all X to the front*, *three classes*, *k-th largest without full sorting*, *remove duplicates keeping at most m copies*.

### Complexity

`O(n)` time, `O(1)` extra for in-place variants.

### Python layout

Tuple swaps `a[i], a[j] = a[j], a[i]`; `random.randrange` for the pivot.

```python
import random

def sort_colors(a: list[int]) -> None:
    low, mid, high = 0, 0, len(a) - 1
    while mid <= high:
        if a[mid] == 0:
            a[low], a[mid] = a[mid], a[low]; low += 1; mid += 1
        elif a[mid] == 2:
            a[mid], a[high] = a[high], a[mid]; high -= 1        # mid stays
        else:
            mid += 1

def quickselect(a: list[int], k: int) -> int:
    """k-th smallest, 0-based. Iterative: no recursion depth issue. Mutates a."""
    lo, hi = 0, len(a) - 1
    while True:
        p = random.randint(lo, hi)
        a[p], a[hi] = a[hi], a[p]
        store = lo
        for i in range(lo, hi):                 # Lomuto: everything < pivot goes left
            if a[i] < a[hi]:
                a[store], a[i] = a[i], a[store]; store += 1
        a[store], a[hi] = a[hi], a[store]
        if store == k:
            return a[store]
        if k < store:
            hi = store - 1
        else:
            lo = store + 1
```

### Worked example

`a = [2, 0, 1, 2, 0]`:

```
                 a            low mid high   action
start        [2 0 1 2 0]       0   0   4
a[mid]=2     [0 0 1 2 2]       0   0   3     swap mid<->high, high--   (mid re-inspects a[0]=0)
a[mid]=0     [0 0 1 2 2]       1   1   3     swap low<->mid (self), low++ mid++
a[mid]=0     [0 0 1 2 2]       2   2   3     swap, low++ mid++
a[mid]=1     [0 0 1 2 2]       2   3   3     mid++
a[mid]=2     [0 0 1 2 2]       2   3   2     swap mid<->high (self), high--
mid > high -> done:  [0 0 | 1 | 2 2]
```

Move Zeroes on `[0, 1, 0, 3, 12]`:

```
read=0 a=0   skip                 write=0
read=1 a=1   a[0]=1, write=1                [1 1 0 3 12]
read=2 a=0   skip
read=3 a=3   a[1]=3, write=2                [1 3 0 3 12]
read=4 a=12  a[2]=12, write=3               [1 3 12 3 12]
fill [3,5) with 0:                          [1 3 12 0 0]      a[write:] = [0] * (n - write)
```

### Pitfalls

- After a `high` swap, do **not** advance `mid`.
- Sort Array By Parity II: the `odd` pointer continues from where it stopped.
- Move Zeroes single-pass without the fill must *swap*, not copy.
- Partition by Pivot: the pivot value belongs to the middle group and order is preserved — not a Sort Colors swap.
- Wiggle Sort II: after sorting and splitting at the median, place each half in **reverse** order.

---

## Pattern recognition cheatsheet

| Signal in the problem statement | Pattern | Section | Time / space |
|---|---|---|---|
| sorted array, pair with sum / difference | opposite ends | 1 | `O(n)` / `O(1)` |
| palindrome, reverse, symmetric | opposite ends (or `s[::-1]`, `O(n)` mem) | 1 | `O(n)` / `O(1)` |
| two walls / heights, water, area | opposite ends, move the limiting side | 1 | `O(n)` / `O(1)` |
| triplets summing to X | sort + fix one + opposite ends | 1 | `O(n^2)` / `O(1)` |
| linked list: cycle, middle, palindrome, reorder | fast / slow | 2 | `O(n)` / `O(1)` |
| repeated `x = f(x)`, values are valid indices | fast / slow on a function graph | 2 | `O(n)` / `O(1)` |
| every subarray of size exactly `k`, moving average | fixed window | 3 | `O(n)` / `O(1)` |
| max / min of each window of size `k` | fixed window + `deque` | 3 | `O(n)` / `O(k)` |
| take `k` from the ends | fixed window of size `n - k` in the middle | 3 | `O(n)` / `O(1)` |
| longest subarray with sum / product / count ≤ bound (positive) | variable window, shrink until just valid | 4 | `O(n)` / `O(1)` |
| shortest subarray with sum ≥ target (positive) | variable window, shrink as far as possible | 4 | `O(n)` / `O(1)` |
| count subarrays with exactly `k` of something | `atMost(k) - atMost(k-1)` | 4 | `O(n)` / `O(1)` |
| anagram / permutation of `p` inside `s` | fixed window + histogram + `matched` | 3+5 | `O(n)` / `O(sigma)` |
| shortest window covering all chars of `t` | variable window + `Counter` + `matched` | 5 | `O(n+m)` / `O(sigma)` |
| at most `k` distinct values, two baskets | variable window + count of live keys | 5 | `O(n)` / `O(k)` |
| move all X to the front, keep order | write / read pointer | 6 | `O(n)` / `O(1)` |
| three classes in place | Dutch national flag | 6 | `O(n)` / `O(1)` |
| `k`-th largest / smallest without sorting | iterative quickselect (or `heapq.nlargest`) | 6 | `O(n)` expected / `O(1)` |
| subarray sum == target with **negative** numbers | *not* this chapter — prefix sums + dict | 01 | `O(n)` / `O(n)` |

---

## Gotchas in Python specifically

- **Negative indices wrap.** `a[right]` with `right == -1` silently reads the last element. A loop that lets an index go below 0 gives wrong answers, not an `IndexError`. Guard with `while left < right`, `while lo <= hi`.
- **`list.pop(0)` is `O(n)`.** The monotonic queue *must* be a `collections.deque`; `popleft()` is `O(1)`. `deque` also supports `dq[0]` and `dq[-1]` in `O(1)`, but `dq[i]` for interior `i` is `O(n)`.
- **`in` on a list is `O(n)`.** The "seen" structure in Longest Substring Without Repeating is a `set` or `dict`, never a list.
- **`is` vs `==`.** Linked-list node comparison is `slow is fast`. `==` on nodes without `__eq__` also compares identity, but be explicit — and `x is 0` is wrong for ints.
- **Slicing copies.** `sum(a[l:r+1])` inside a loop is `O(n)` each — the whole point of the window is to avoid it. `s[::-1]` allocates a new string.
- **Recursion limit ~1000.** Quickselect and any divide-and-conquer on `n = 10^5` must be iterative or use `sys.setrecursionlimit(10**6)` (which still risks a C-stack crash for very deep recursion).
- **`Counter` zero-count keys still count in `len`.** After `have[c] -= 1` reaches 0, `del have[c]` if you rely on `len(have)` for the distinct count — or keep a separate `distinct` int.
- **Mutable default args.** `def solve(a, seen=set())` shares `seen` across calls. Use `None`.
- **Late-binding closures.** `[lambda: x for x in a]` all see the last `x`. Rarely bites here, but `key=lambda i: a[i]` inside a loop over `a` reassignment does.
- **`//` vs `/`.** Middle index is `(lo + hi) // 2`; `/` gives a float and `a[2.0]` raises `TypeError`.
- **Float averages.** Compare `s * 1 >= k * threshold` on ints, not `s / k >= threshold`.
- **`heapq` is min-heap only**; negate for max. **`bisect` needs sorted input.** (Chapters 3 and 8.)
- **Speed.** Roughly 50-100× slower than C; `sum`, `max`, `Counter`, `deque` ops run in C. `for right, x in enumerate(a)` beats `for right in range(n): x = a[right]`.

---

## Common mistakes checklist

- [ ] Loop condition `left < right` (not `<=`) for opposite pointers that swap.
- [ ] Inner skip loops re-check `left < right`.
- [ ] Moved the pointer that *limits* the answer (water problems).
- [ ] Guarded `fast and fast.next` before the double hop.
- [ ] Ran phase two — did not return the phase-one meeting point as the cycle start.
- [ ] First window computed directly; the shift loop starts at `right = k`.
- [ ] Deque stores indices; expired front removed before reading the max; it is a `deque`, not a list.
- [ ] Longest-window: shrink until *just* valid. Shortest-window: shrink *as far as possible*.
- [ ] Predicate is monotone (all-positive values, "at most" not "exactly").
- [ ] `atMost(-1)` returns 0.
- [ ] Key removed from the distinct count when it hits zero (`del` or a separate counter).
- [ ] `matched` counts satisfied *keys*, not characters.
- [ ] `mid` not advanced after a swap with `high`.
- [ ] Stable partition used where order must be preserved.
- [ ] Quickselect is iterative.

---

## You can move on when...

- You can write the opposite-pointers pair-sum loop from memory and explain why moving `right` when the sum is too big cannot skip a valid pair.
- You can draw the `a = c` argument for Floyd's phase two and implement `cycle_start` without looking.
- You can state the fixed-window invariant, write the sum version in under a minute, and explain why the maximum version needs a `deque`.
- Given a new subarray problem you can say whether the predicate is monotone, longest vs shortest, length vs count (`atMost` trick).
- You can implement Minimum Window Substring with `need / have / matched` and trace it on a 6-character input.
- You can write Sort Colors and explain the `high`-swap asymmetry, and turn the same partition into an iterative quickselect.
- `example.py` runs clean and you have modified at least one input and predicted the output before running it.

Next: `../03_*/lesson_python.md` (binary search).
