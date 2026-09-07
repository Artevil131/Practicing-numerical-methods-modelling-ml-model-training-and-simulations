# Chapter 01 — Arrays & hashing — Python

## What you'll be able to do after this chapter

- Recognise, from the wording of a problem, which of six array patterns applies: hash-set membership, frequency counting, hash-map "memory" lookup, prefix sums, sort-then-sweep, or index arithmetic on matrices — and state its time/space cost before writing a line.
- Replace an `O(n^2)` "compare every pair" loop with a single `O(n)` pass over a `set` or `dict`, and explain the invariant that makes the single pass correct.
- Use a list *as its own hash set* when values are bounded to `1..n` (sign marking, cyclic sort), dropping extra space from `O(n)` to `O(1)`.
- Precompute prefix sums with `itertools.accumulate` so any range query costs `O(1)`, and combine prefix sums with a `dict` to count or locate subarrays whose sum satisfies a condition.
- Sort with `sorted(key=...)` or `functools.cmp_to_key` as a *preprocessing* step that turns interval and scheduling problems into one greedy sweep.
- Derive matrix index formulas (transpose, rotate, spiral, flatten/reshape) without off-by-one errors, and do in-place grid updates by encoding old and new state in the same cell.

## Why this matters for ML / numerics / sims

Everything in this chapter is something NumPy does for you and hides. `np.cumsum` is a prefix sum; a range sum `P[r+1]-P[l]` is how you integrate a sampled signal over an arbitrary window in `O(1)`, and how a cumulative distribution turns into a "how much probability mass lies in `[a,b)`" query. `collections.Counter` is a frequency map — the exact structure a BPE tokenizer uses to count adjacent-pair frequencies. `dict` lookup by complement is the two-sum pattern; the same "store one half, query the other half" trick matches particle pairs in a collision detector without an `O(n^2)` sweep. Sorting-as-preprocessing is how you sweep interval overlaps in an event-driven simulation. Matrix index arithmetic — `k = i*ncols + j` — is how a flat buffer becomes a 2-D tensor in every ML runtime; transpose and rotate are the memory-layout operations behind `np.transpose` and `torch.rot90`, and Game of Life is the simplest stencil update, the same shape as a Jacobi iteration. Writing these by hand in Python, without NumPy, makes you see the memory traffic NumPy hides.

**Python vs C, once for the whole chapter:** `set` and `dict` *are* the hash table — you never write one. Ints do not overflow, so no `long long`. Python is roughly 50-100× slower than C per operation, but LeetCode limits are set so an `O(n)` or `O(n log n)` Python solution passes; an `O(n^2)` one at `n = 10^5` does not.

---

## 1. Hash-set membership & dedup

### The idea

A **hash set** gives an average `O(1)` membership test: *has this element been seen before?* Whenever a problem asks you to find a duplicate, check whether a value has already appeared, or remove repeats, ask: "do I only need to know *whether* a value was seen — not where, and not how many times?" If yes, `set` is the tool.

### The invariant

During a single left-to-right pass, at index `i` the set contains *exactly* the elements at indices `0..i-1`. Each new element either hits (react immediately) or is inserted. Insert and lookup are `O(1)` average, so the whole pass is `O(n)` time and `O(n)` space — versus the `O(n^2)` brute force.

### When to recognise it

Signal words: *duplicate*, *unique*, *does there exist*, *missing number*, *cycle in a functional graph* (follow `x -> f(x)`; the first repeated value closes the cycle).

### The Python data structures

`set` is a hash table with open addressing, built in. Keys must be hashable (ints, strs, tuples — not lists; convert a list key with `tuple(lst)`).

```python
def first_duplicate(a: list[int]) -> int | None:
    seen: set[int] = set()
    for x in a:
        if x in seen:          # O(1) average
            return x
        seen.add(x)
    return None

def longest_consecutive(a: list[int]) -> int:
    s = set(a)
    best = 0
    for x in s:
        if x - 1 in s:         # only start from run heads -> each value visited O(1) times total
            continue
        y = x
        while y + 1 in s:
            y += 1
        best = max(best, y - x + 1)
    return best
```

**Python vs C:** in C you write the table (Knuth hash, linear probing, `used[]` bytes). In Python `set(a)` builds it in one call, and `x in s` on a set is `O(1)` — but `x in a` on a *list* is `O(n)`; that is the single most common accidental `O(n^2)` in Python solutions.

### Worked mini-example: contains-duplicate on `[3, 1, 4, 1, 5]`

```
i=0  x=3   seen={}          not present -> add   seen={3}
i=1  x=1   seen={3}         not present -> add   seen={3,1}
i=2  x=4   seen={3,1}       not present -> add   seen={3,1,4}
i=3  x=1   seen={3,1,4}     PRESENT     -> return True
```

Three inserts, one hit. Brute force would have done ten comparisons. One-liner when only the boolean is needed: `len(set(a)) < len(a)`.

### The array as its own hash set

When values are bounded to `1..n`, the list itself can act as the set:

1. **Sign marking.** Value `v` "belongs" to index `abs(v)-1`. Seeing `v`, negate `a[abs(v)-1]`. If it is already negative, `abs(v)` was seen. Always read `abs(a[i])`, never `a[i]`.
2. **Cyclic sort.** Swap `v` into index `v-1` until the value at `i` is correct or out of range. Each swap places at least one value permanently, so total swaps are `O(n)`. Afterwards the first `i` with `a[i] != i+1` reveals the missing `i+1`.

```python
def first_missing_positive(a: list[int]) -> int:
    n = len(a)
    for i in range(n):
        while 1 <= a[i] <= n and a[a[i] - 1] != a[i]:
            j = a[i] - 1
            a[i], a[j] = a[j], a[i]         # tuple swap: no temp variable
    for i in range(n):
        if a[i] != i + 1:
            return i + 1
    return n + 1
```

Trace of sign marking on `[4, 3, 2, 7, 8, 2, 3, 1]` (find all duplicates):

```
x=4  -> mark idx 3:  [4, 3, 2,-7, 8, 2, 3, 1]
x=3  -> mark idx 2:  [4, 3,-2,-7, 8, 2, 3, 1]
x=2  -> mark idx 1:  [4,-3,-2,-7, 8, 2, 3, 1]
x=7  -> mark idx 6:  [4,-3,-2,-7, 8, 2,-3, 1]
x=8  -> mark idx 7:  [4,-3,-2,-7, 8, 2,-3,-1]
x=2  -> idx 1 already negative -> 2 is a duplicate
x=3  -> idx 2 already negative -> 3 is a duplicate
x=1  -> mark idx 0: [-4,-3,-2,-7, 8, 2,-3,-1]
```

Extra space `O(1)` — the whole point when the problem forbids a set.

### Pitfalls

- "Duplicate" means *any* two equal values, not adjacent ones — `sorted(a)` then compare neighbours is the `O(n log n)`, `O(1)`-extra alternative.
- XOR of all elements cancels pairs (`x ^ x == 0`); `n*(n+1)//2 - sum(a)` finds a single missing value. Both are `O(1)` space. In Python the sum cannot overflow, so the sum trick is always safe.
- The tuple swap `a[i], a[j] = a[j], a[i]` evaluates the right side fully before assigning — correct even when `i == j`.

---

## 2. Frequency counting

### The idea

When a problem talks about *how many times*, *most frequent*, or comparing the *composition* of two collections, the structure is a **map from value to count**. One pass accumulates; a second pass over the map answers the question.

### The invariant

The map always describes exactly what has been seen so far — every update is `+1` on an existing or new key. `O(n)` total, or `O(n log n)` if you sort at the end.

### Multiset comparison

Anagrams: `Counter(s) == Counter(t)`. Manually: one `dict`, `+1` per character of `s`, `-1` per character of `t`, all zero at the end. For a fixed 26-letter alphabet a list `cnt = [0]*26` indexed by `ord(c) - 97` is faster than a dict.

### When to recognise it

Signal words: *frequency*, *most common*, *top-k*, *anagram*, *same composition*, *majority*.

### The Python data structures

- `collections.Counter` — a `dict` subclass; missing keys read as `0`, `.most_common(k)` sorts by count, `+`/`-`/`&` combine multisets.
- `collections.defaultdict(int)` when you want plain `d[k] += 1` without `Counter` semantics.
- Bucket sort by frequency: counts live in `[1, n]`, so `buckets: list[list[int]]` of length `n+1` indexed by count replaces an `O(n log n)` sort with an `O(n)` scan from `n` down.

```python
from collections import Counter

def is_anagram(s: str, t: str) -> bool:
    return len(s) == len(t) and Counter(s) == Counter(t)

def top_k_frequent(a: list[int], k: int) -> list[int]:
    freq = Counter(a)
    buckets: list[list[int]] = [[] for _ in range(len(a) + 1)]   # NOT [[]] * (n+1): shared list
    for v, f in freq.items():
        buckets[f].append(v)
    out: list[int] = []
    for f in range(len(a), 0, -1):
        for v in buckets[f]:
            out.append(v)
            if len(out) == k:
                return out
    return out
```

### Worked mini-example: top-2 frequent on `[1, 1, 1, 2, 2, 3]`

```
count:   {1: 3, 2: 2, 3: 1}                                      (one pass)
buckets: [0]=[] [1]=[3] [2]=[2] [3]=[1] [4]=[] [5]=[] [6]=[]      (n+1 = 7 buckets)
scan from bucket 6 down: bucket 3 gives 1, bucket 2 gives 2 -> k=2 collected, stop
```

`Counter(a).most_common(2)` gives `[(1, 3), (2, 2)]` in `O(n log n)`; the buckets version is `O(n)` — same argument as counting sort.

### Boyer-Moore voting (majority element)

Keep a candidate and a counter; when the counter is 0 adopt the current element; increment on match, decrement otherwise. A value occurring more than `n/2` times can never be fully cancelled. `O(n)` time, `O(1)` space.

```python
def majority_element(a: list[int]) -> int:
    cand, cnt = a[0], 0
    for x in a:
        if cnt == 0:
            cand = x
        cnt += 1 if x == cand else -1
    return cand
```

```
[2, 2, 1, 1, 1, 2, 2]
x=2 cnt=0 -> cand=2,cnt=1
x=2 match -> cnt=2
x=1 miss  -> cnt=1
x=1 miss  -> cnt=0
x=1 cnt=0 -> cand=1,cnt=1
x=2 miss  -> cnt=0
x=2 cnt=0 -> cand=2,cnt=1     -> answer 2
```

### Pitfalls

- Membership is not enough when items are *consumed* (ransom note) — count. `Counter(note) <= Counter(magazine)` (multiset subset) is the idiom on 3.10+; otherwise `not (Counter(note) - Counter(magazine))`.
- Several keys can share one frequency: a bucket must be a list.
- `[[]] * n` creates `n` references to *one* list — a shallow-copy trap. Use a comprehension.
- Two-level thinking: the values of a frequency map can be keys of a second set: `len(set(freq.values())) == len(freq)`.

---

## 3. Hash-map index lookup

### The idea

The third pattern uses the **dict as memory** — "have I seen value `x`, and if so, *where* or *what did it correspond to*?" Two-sum: store each visited number's **index**, and at every step ask whether the complement `target - x` is already in the dict. One pass.

### The invariant

The dict contains exactly the values (with their latest associated datum) seen *before* the current element. `O(n)` total instead of `O(n^2)`.

**Order matters:** query first, then insert. Otherwise an element pairs with itself.

### Bijection between two sequences

A **one-to-one mapping** between two sequences (`isomorphic-strings`, `word-pattern`) needs two dicts, `st` and `ts`; at each position, if either mapping exists it must agree; if neither exists, add both. Counter-example that breaks the one-dict version: `s = "badc", t = "baba"`.

### When to recognise it

Signal words: *pair summing to X*, *does this structure match that one*, *first non-repeating*, *transform to X and check if seen*. Here the dict's **value** carries meaning, not just presence or count.

### The Python data structures

`dict[int, int]` for value → index. For character bijections two `dict[str, str]`. Word ↔ character keys are just strs — no hashing or `strcmp` to write.

```python
def two_sum(a: list[int], target: int) -> tuple[int, int] | None:
    seen: dict[int, int] = {}
    for k, x in enumerate(a):
        if target - x in seen:          # query BEFORE insert
            return seen[target - x], k
        seen[x] = k
    return None

def is_bijection(s: str, t: str) -> bool:
    if len(s) != len(t):
        return False
    st: dict[str, str] = {}
    ts: dict[str, str] = {}
    for c, d in zip(s, t):
        if st.get(c, d) != d or ts.get(d, c) != c:
            return False
        st[c] = d
        ts[d] = c
    return True
```

`st.get(c, d) != d` reads: "if `c` is mapped, it must map to `d`; if unmapped, treat as fine."

### Worked mini-example: two-sum, `a = [2, 7, 11, 15, 3]`, `target = 10`

```
k=0  a=2   need 8   seen={}              miss -> put 2->0
k=1  a=7   need 3   seen={2:0}           miss -> put 7->1
k=2  a=11  need -1  seen={2:0,7:1}       miss -> put 11->2
k=3  a=15  need -5  seen={2:0,7:1,11:2}  miss -> put 15->3
k=4  a=3   need 7   seen={...,15:3}      HIT idx=1 -> answer (1, 4)
```

The pair is always in index order because the dict only holds earlier elements.

### Scaling the idea: 4Sum II

Put all `n^2` sums `A[i]+B[j]` into `Counter`, then for each `C[k]+D[l]` add `cnt[-(C[k]+D[l])]`. `O(n^2)` time and space; the map must hold counts, not presence. `Counter(x + y for x in A for y in B)` builds it in one expression.

### Pitfalls

- Duplicated values keep only the latest index — harmless for two-sum because query-before-insert finds the first valid pair.
- "First unique character" needs **two passes**: `Counter(s)` first, then `next(i for i, c in enumerate(s) if cnt[c] == 1)`.
- Do not forget the second direction of a bijection. `len(set(s)) == len(set(t)) == len(set(zip(s, t)))` is a compact full check.

---

## 4. Prefix sums

### The idea

The **prefix sum** `P[i] = sum(a[:i])`, with `P[0] = 0`, is built once in `O(n)` and answers the sum of any inclusive subarray `[l, r]` in `O(1)`: `P[r+1] - P[l]`. `q` queries cost `O(n + q)` rather than `O(nq)`.

### The invariant

`P[i]` is the sum of the prefix `a[0..i-1]` — the **half-open convention**: `P[i]` is the sum *before* index `i`. This is the most common off-by-one in the pattern.

```python
from itertools import accumulate
P = list(accumulate(a, initial=0))        # n+1 entries, P[0] == 0
```

```
a  = [ 3, -1,  4,  1, -5,  9 ]
P  = [ 0,  3,  2,  6,  7,  2, 11 ]
sum(a[1..3]) = P[4] - P[1] = 7 - 3 = 4
sum(a[0..5]) = P[6] - P[0] = 11
```

**Python vs C:** `sum(a[l:r+1])` is `O(r-l)` *and* the slice copies — fine for one query, wrong inside a loop. Ints never overflow, so there is no `long long` to remember.

### The strongest combination: prefix sum + dict

To count subarrays summing to exactly `k`: accumulate the running prefix, and at each index ask the dict whether `prefix - k` has been seen. This is two-sum on cumulative sums.

| Question | Dict stores | Initialisation |
|---|---|---|
| *how many* subarrays sum to `k` | prefix value → **count** | `{0: 1}` |
| *longest* subarray with sum 0 / balanced | prefix value → **first index** | `{0: -1}` |
| sum divisible by `k` (exists, length >= 2) | prefix mod `k` → **first index** | `{0: -1}` |
| *how many* subarrays divisible by `k` | prefix mod `k` → **count** | `{0: 1}` |

The initialisation encodes "the empty prefix has sum 0 at position -1" and is mandatory.

```python
from collections import defaultdict

def count_subarrays_with_sum(a: list[int], k: int) -> int:
    seen: dict[int, int] = defaultdict(int)
    seen[0] = 1                                  # empty prefix seen once
    prefix = ans = 0
    for x in a:
        prefix += x
        ans += seen[prefix - k]                  # query first ... (defaultdict: missing -> 0)
        seen[prefix] += 1                        # ... then record
    return ans

def longest_zero_sum_subarray(a: list[int]) -> int:
    first: dict[int, int] = {0: -1}
    prefix = best = 0
    for i, x in enumerate(a):
        prefix += x
        if prefix in first:
            best = max(best, i - first[prefix])   # do NOT overwrite: keep the earliest index
        else:
            first[prefix] = i
    return best
```

Careful: `seen[prefix - k]` on a `defaultdict` *inserts* a zero entry on a miss. Harmless for counts (it stays 0), but for the "first index" variant use a plain dict with `in`.

### Worked mini-example: count subarrays with sum 2 in `[1, 1, 1, -1, 2]`

```
seen={0:1}  prefix=0 ans=0
i=0 a=1   prefix=1   need -1  miss    seen={0:1,1:1}
i=1 a=1   prefix=2   need 0   HIT +1  seen={0:1,1:1,2:1}         ans=1  ([1,1])
i=2 a=1   prefix=3   need 1   HIT +1  seen={0:1,1:1,2:1,3:1}     ans=2  ([1,1] at 1..2)
i=3 a=-1  prefix=2   need 0   HIT +1  seen={0:1,1:1,2:2,3:1}     ans=3  ([1,1,1,-1])
i=4 a=2   prefix=4   need 2   HIT +2  seen={...,4:1}             ans=5  ([-1,2] and [2])
```

### Modulo variant

If `P[i] % k == P[j] % k` with `i < j`, the subarray `a[i..j-1]` sums to a multiple of `k`. **Python vs C:** Python's `%` with a positive divisor always returns a result in `[0, k)` (`-7 % 5 == 3`), so the C normalisation `((x % k) + k) % k` is unnecessary — but `math.fmod` and C-style truncation are *not* what `%` does; don't mix them.

### Prefix products

`result[i] = prod(a[:i]) * prod(a[i+1:])`. Left pass fills prefix products into `result`; right pass multiplies in a running suffix product. Handles zeros, `O(n)` time, `O(1)` extra.

### When to recognise it

Signal words: *subarray sum*, *many queries on an immutable array*, *divisible by*, *balance between 0s and 1s* (map 0 → -1), *running total*, *highest altitude*, *pivot index* (left sum == total − left − a[i]).

Often a running scalar suffices — you need not materialise `P`.

### Pitfalls

- `P` has `n+1` entries — `accumulate(..., initial=0)` gives exactly that (Python 3.8+).
- `P[r+1] - P[l]` for inclusive `[l, r]`; `P[r] - P[l]` for half-open `[l, r)`.
- In pivot-index, compare *before* adding `a[i]` to the left sum.
- Overwriting the input with its running sum is fine only if nothing needs the original later.

---

## 5. Sorting as preprocessing

### The idea

Many problems that do not look like sorting problems become easy once the data is **sorted first** by some key. Sorting costs `O(n log n)`, but exposes adjacency, coverage, monotonicity — so the rest is a one-pass greedy sweep.

### Two signals

**Overlapping intervals.** Sorted by start (or end), overlap can only occur between *neighbours* — `O(n^2)` pair checks become an `O(n)` sweep.

**"Largest/smallest first" greedy that is only correct on sorted input.** Sorting lines up the competing alternatives so the exchange argument works.

### The invariant of the interval sweep

Maintain a current merged interval `[cs, ce]`. The next `[s, e]` either overlaps (`s <= ce`: `ce = max(ce, e)`) or does not (emit, start new). The `max` is essential — a nested interval has `e < ce`.

### The Python data structures

`list.sort(key=...)` (in place) or `sorted(iterable, key=...)` (new list). Both are Timsort: `O(n log n)`, **stable**, and adaptive (nearly-sorted input runs in `O(n)`). Sort by a field with `key=lambda iv: iv[0]` or `operator.itemgetter(0)`; multiple keys with a tuple `key=lambda iv: (iv[1], iv[0])`; descending with `reverse=True`. Pairwise comparators (largest-number's `a+b` vs `b+a`) go through `functools.cmp_to_key`.

```python
def merge_intervals(v: list[list[int]]) -> list[list[int]]:
    v.sort(key=lambda iv: iv[0])
    out: list[list[int]] = []
    for s, e in v:
        if out and s <= out[-1][1]:
            out[-1][1] = max(out[-1][1], e)
        else:
            out.append([s, e])
    return out
```

**Python vs C:** no comparator overflow (`a - b` is safe on Python ints), no `const void *` casts. But `sorted(...)` allocates a new list; use `.sort()` when the input may be mutated. Sorting tuples/lists without `key` compares lexicographically — `[1,3] < [2,6]` works, but write the `key` anyway for clarity.

### Worked mini-example: merge `[[8,10],[1,3],[2,6],[15,18],[6,7]]`

```
sorted by start: [1,3] [2,6] [6,7] [8,10] [15,18]
cur=[1,3]
[2,6]:  2 <= 3  -> extend, cur=[1,6]
[6,7]:  6 <= 6  -> extend (touching counts), cur=[1,7]
[8,10]: 8 >  7  -> emit [1,7], cur=[8,10]
[15,18]: 15 > 10 -> emit [8,10], cur=[15,18]
end -> emit [15,18]
result: [1,7] [8,10] [15,18]
```

### Choosing the sort key

| Task | Sort by | Sweep |
|---|---|---|
| merge overlapping intervals | start | extend or emit |
| insert one interval into sorted disjoint set | already sorted — no sort | copy-before, merge-overlapping, copy-after |
| max number of non-overlapping (min removals) | **end** ascending | keep if `start >= last_end` |
| min arrows / points to cover all intervals | **end** ascending | new arrow when `start > arrow_pos` |
| h-index | citations descending | largest `i` with `c[i] >= i+1` |
| largest concatenated number | `cmp_to_key(lambda a, b: -1 if a+b > b+a else 1)` on strs | `"".join` |
| custom order given by another array | `Counter` + that array's order | emit counts, then sort leftovers |

Sorting by *start* for the non-overlapping selection is wrong — the earliest-*ending* interval leaves the most room.

### Pitfalls

- Overlap condition is `<=`, not `<`: `[1,2]` and `[2,3]` touch and merge — check the problem's convention.
- Already sorted and disjoint input: do not sort again; the `O(n)` three-phase pass is the point.
- Bounded key range (`citations in [0,n]`): counting sort turns `O(n log n)` into `O(n)`.
- Task scheduler: answer is `max((f_max - 1) * (n + 1) + (#tasks at f_max), total_tasks)`.
- Largest number: `["0", "0"]` yields `"00"` — normalise with `str(int(result))`.

---

## 6. Matrix & array manipulation

### The idea

Not one algorithmic pattern but the craft of **index arithmetic**: the algorithm is obvious (traverse, rotate, copy); the difficulty is the indices.

### Index transforms

- **Transpose:** `(i, j) -> (j, i)`. Non-square: output is `n x m`. In place on a square matrix, swap only the upper triangle `i < j`. Idiom: `list(map(list, zip(*a)))` transposes any list-of-lists in one line.
- **Rotate 90° clockwise:** transpose, then reverse each row: `(i,j) -> (j, n-1-i)`. One-liner: `[list(r[::-1]) for r in zip(*a)]` (allocates); in place needs the two loops.
- **Flatten / reshape:** `k = i * ncols + j` flattens; `i, j = divmod(k, new_cols)` reshapes. Check `r*c == m*n` first.
- **Diagonals:** main `i == j`; anti `i + j == n-1`; for odd `n` the centre belongs to both — count once.
- **Toeplitz check:** `m[i][j] == m[i-1][j-1]` for all `i>0, j>0`.
- **Spiral walk:** four bounds `top, bottom, left, right`; after each side re-check `top <= bottom` and `left <= right`, or a one-row / one-column remainder is visited twice.

```python
def transpose(a: list[list[int]]) -> list[list[int]]:
    return [list(col) for col in zip(*a)]            # m x n -> n x m

def rotate_cw_inplace(a: list[list[int]]) -> None:
    n = len(a)
    for i in range(n):                               # transpose: upper triangle only
        for j in range(i + 1, n):
            a[i][j], a[j][i] = a[j][i], a[i][j]
    for row in a:                                    # reverse each row in place
        row.reverse()

def spiral_order(a: list[list[int]]) -> list[int]:
    out: list[int] = []
    top, bottom, left, right = 0, len(a) - 1, 0, len(a[0]) - 1
    while top <= bottom and left <= right:
        out.extend(a[top][j] for j in range(left, right + 1)); top += 1
        out.extend(a[i][right] for i in range(top, bottom + 1)); right -= 1
        if top <= bottom:
            out.extend(a[bottom][j] for j in range(right, left - 1, -1)); bottom -= 1
        if left <= right:
            out.extend(a[i][left] for i in range(bottom, top - 1, -1)); left += 1
    return out
```

**Python vs C:** a Python matrix is a list of row lists, not a flat buffer, so `a[i][j]` is two pointer hops — but the flat `a[i*n + j]` layout is still what NumPy uses underneath, and the formulas above are the same. Never build a grid with `[[0]*n]*m` — every row is the *same* list (shallow copy); use `[[0]*n for _ in range(m)]`.

### Worked mini-example: rotate a 3x3

```
        1 2 3        transpose     1 4 7      reverse rows     7 4 1
        4 5 6         ------->     2 5 8       ---------->     8 5 2
        7 8 9                      3 6 9                       9 6 3
```

Check: `(0,0)=1 -> (0, n-1-0) = (0,2)`. Top-right of the result is 1. Correct.

Spiral on the same 3x3:

```
row top  L->R : 1 2 3          top=1
col right T->B: 6 9            right=1
row bottom R->L (top<=bottom): 8 7      bottom=1
col left B->T (left<=right):   4        left=1
row top  L->R : 5              top=2   -> top > bottom, stop
output: 1 2 3 6 9 8 7 4 5
```

### In-place editing with marker state

When the matrix may not be copied, its **first row and first column** serve as memory for which rows/columns to zero — the sign-marking idea again. Save row 0 / col 0's own status in two booleans *before* marking, handle them last.

### Simulation with two states in one cell

Game of Life must update *simultaneously*. Encode both states in one cell: `cell = old + 2*new` (0-3). Neighbour counting reads `cell & 1`; a second pass does `cell >>= 1`. `O(mn)` time, `O(1)` extra. A Jacobi step or heat-equation update has the same shape with floats and a second buffer.

```python
def game_of_life_step(g: list[list[int]]) -> None:
    m, n = len(g), len(g[0])
    for i in range(m):
        for j in range(n):
            live = sum(g[i + di][j + dj] & 1
                       for di in (-1, 0, 1) for dj in (-1, 0, 1)
                       if (di or dj) and 0 <= i + di < m and 0 <= j + dj < n)
            if (g[i][j] & 1) and live in (2, 3) or not (g[i][j] & 1) and live == 3:
                g[i][j] |= 2                     # bit 1 = new state
    for row in g:
        for j in range(n):
            row[j] >>= 1
```

### When to recognise it

Signal words: *matrix*, *grid*, *rotate*, *transpose*, *diagonal*, *in place*, *spiral*, *reshape*.

### The Python data structures

`list[list[int]]` is the LeetCode interface. For heavy numeric work NumPy's flat row-major buffer (`a.ravel()[i*n + j]`) is the real layout. `zip(*a)` is the idiomatic transpose; `divmod` the idiomatic reshape.

### Pitfalls

- Draw a `3x3` *and* a `2x3` on paper before writing an index formula.
- Neighbour loops must clamp `0 <= i+di < m`, `0 <= j+dj < n`; Python's negative indices wrap silently (`a[-1]` is the last row) — a bug that C would at least crash on.
- Square-only tricks (in-place transpose, rotate) must not be applied to rectangular matrices.
- `a[::-1]`, `a[:]`, `list(a)` all copy: `O(k)` time and memory each time.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Time / extra space |
|---|---|---|
| duplicate, unique, does there exist, seen before | `set`, single pass | `O(n)` / `O(n)` |
| values bounded to `1..n`, "without extra space" | list as its own set: sign marking or cyclic sort | `O(n)` / `O(1)` |
| one element odd-count, rest pairs; one missing from `0..n` | XOR / sum formula | `O(n)` / `O(1)` |
| how many times, most frequent, top-k, anagram, majority | `Counter` (or `[0]*26`); bucket by count | `O(n)` / `O(n)` or `O(1)` |
| majority element only | Boyer-Moore voting | `O(n)` / `O(1)` |
| pair summing to X, complement, first non-repeating | `dict` value → index, query before insert | `O(n)` / `O(n)` |
| does A's structure mirror B's (1-1 mapping) | two dicts, both directions | `O(n)` / `O(alphabet)` |
| quadruples / pairs of pairs summing to X | `Counter` of pair sums | `O(n^2)` / `O(n^2)` |
| many range-sum queries, immutable array | `accumulate(initial=0)`, `P[r+1]-P[l]` | `O(n)` build / `O(1)` query |
| subarray with sum = k (count) | prefix + `defaultdict(int)`, init `{0:1}` | `O(n)` / `O(n)` |
| longest balanced / zero-sum subarray | prefix + dict value → first index, init `{0:-1}` | `O(n)` / `O(n)` |
| subarray sum divisible by k | prefix `% k` + dict (Python `%` already non-negative) | `O(n)` / `O(k)` |
| product of all except self, no division | prefix and suffix products | `O(n)` / `O(1)` |
| intervals, overlap, merge | `sort(key=start)`, sweep neighbours | `O(n log n)` / `O(n)` |
| max non-overlapping / min removals / min arrows | `sort(key=end)`, greedy | `O(n log n)` / `O(1)` |
| k-th largest, threshold on sorted (h-index) | `sorted(reverse=True)` or counting sort | `O(n log n)` or `O(n)` |
| custom order, concatenation order | `cmp_to_key` | `O(n log n)` |
| matrix rotate / transpose / spiral / reshape | `zip(*a)`, four bounds, `divmod` | `O(mn)` / `O(1)` |
| grid updated simultaneously, in place | two-bit state per cell | `O(mn)` / `O(1)` |
| zero rows/columns in place | first row/col as markers + two flags | `O(mn)` / `O(1)` |

---

## Gotchas in Python specifically

- **`in` on a list is `O(n)`.** `if x in some_list` inside a loop is the hidden `O(n^2)`. Convert to `set` once. Same for `list.index` and `list.count`.
- **`list.pop(0)` and `list.insert(0, x)` are `O(n)`.** Use `collections.deque` for queue behaviour (`popleft`, `appendleft` are `O(1)`).
- **Shallow copies.** `[[0]*n]*m` and `[[]]*n` share one inner list; `b = a` aliases; `a[:]`, `list(a)`, `a.copy()` are one level deep. Use comprehensions for grids and `copy.deepcopy` only when necessary.
- **Mutable default arguments.** `def f(seen=set())` keeps one set across all calls. Use `None` and create inside.
- **`is` vs `==`.** `is` compares identity; use it only for `None`. Small ints and interned strings make `is` *look* correct until it isn't.
- **Late-binding closures.** `[lambda: i for i in range(3)]` all return 2. Bind with a default `lambda i=i: i` if you must.
- **Float keys.** `0.1 + 0.2 != 0.3`; never use computed floats as dict/set keys — use `round`, `fractions.Fraction`, or integers scaled.
- **Slicing copies.** `a[l:r]` is `O(r-l)` time and memory. Iterating with indices or `itertools.islice` avoids it.
- **`//` vs `/`.** `/` always yields a float. Index arithmetic needs `//` (floor division — rounds toward `-inf`, so `-7 // 2 == -4`), or `divmod`.
- **`%` is floored, not truncated.** `-7 % 5 == 3`. Good for modulo bucketing, but the sign differs from C — remember when porting.
- **Recursion limit ~1000.** Not an issue in this chapter, but any recursive helper on `n = 10^4` needs `sys.setrecursionlimit` or an iterative rewrite.
- **`heapq` is a min-heap only** — negate for max (Chapter 8). **`bisect` needs sorted input** (Chapter 3).
- **Hashability.** Lists and dicts are not hashable; use `tuple(row)` or `frozenset(s)` as keys. A tuple is hashable only if all its elements are.
- **Speed.** Roughly 50-100× slower than C. `sum(...)`, `Counter`, `sorted`, `zip`, `accumulate` run in C and are the fastest tools you have; a hand-written Python loop is the slowest. Prefer builtins over explicit loops when the semantics match.

---

## Common mistakes checklist

- [ ] Inserting into the dict *before* querying the complement (two-sum pairs an element with itself).
- [ ] Reading `a[i]` instead of `abs(a[i])` after sign marking has begun.
- [ ] Placing values outside `[1, n]` during cyclic sort (Python's negative indices wrap instead of crashing).
- [ ] Forgetting that a duplicate is any two equal values, not adjacent ones.
- [ ] Using a set where a `Counter` is required (ransom note, 4Sum II, relative sort with repeats).
- [ ] Only one direction of a bijection.
- [ ] Missing the `{0: 1}` / `{0: -1}` initialisation in prefix-sum + dict.
- [ ] Using `P[r] - P[l]` for an inclusive range.
- [ ] Overwriting the "first index" for a remainder when the problem needs the *earliest* occurrence.
- [ ] Sorting intervals by start when the greedy needs end.
- [ ] `<` instead of `<=` on the touching-interval boundary.
- [ ] Assigning `cur[1] = e` instead of `max(cur[1], e)` when merging.
- [ ] Transposing the full square instead of the upper triangle (undoes itself).
- [ ] Reading a neighbour's already-updated state in a simultaneous grid update.
- [ ] Spiral: not re-checking the bounds before the third and fourth sides.
- [ ] `[[0]*n]*m` for a grid (aliased rows).
- [ ] `x in list` inside a loop.

---

## You can move on when...

- Given a problem statement, you can name the pattern and its complexity within a minute, before coding.
- You know which of `set`, `dict`, `Counter`, `defaultdict` fits a problem and why, and can state the cost of `in` on each container type.
- You can explain *why* the longest-consecutive-sequence inner `while` loop is `O(n)` in total, and why the two-sum dict must be queried before it is updated.
- You can write the prefix-sum + dict skeleton (with correct initialisation) for both "count" and "longest" variants without looking.
- You can sort with `key=` and `cmp_to_key`, and state which key (start vs. end) each interval problem needs and why.
- You can rotate a square matrix in place and walk a rectangular one in a spiral, both correct on `1xN`, `Nx1`, `2x3` and `3x3` inputs.
- You have solved at least the level-2 problems in every unit of `problems.md` and at least three level-3 ones, in Python, with your own `assert` tests.
