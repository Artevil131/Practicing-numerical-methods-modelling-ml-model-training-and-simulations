# Chapter 01 — Arrays & hashing

## What you'll be able to do after this chapter

- Recognise, from the wording of a problem, which of six array patterns applies: hash-set membership, frequency counting, hash-map "memory" lookup, prefix sums, sort-then-sweep, or index arithmetic on matrices — and state its time/space cost before writing a line.
- Replace an `O(n^2)` "compare every pair" loop with a single `O(n)` pass over a hash structure, and explain the invariant that makes the single pass correct.
- Use an array *as its own hash set* when values are bounded to `1..n` (sign marking, cyclic sort), dropping extra space from `O(n)` to `O(1)`.
- Precompute prefix sums so that any range query costs `O(1)`, and combine prefix sums with a hash map to count or locate subarrays whose sum satisfies a condition.
- Sort with `qsort` and a comparator as a *preprocessing* step that turns interval and scheduling problems into one greedy sweep.
- Derive matrix index formulas (transpose, rotate, spiral, flatten/reshape) without off-by-one errors, and do in-place grid updates by encoding old and new state in the same cell.

## Why this matters for ML / numerics / sims

Everything in this chapter is something NumPy or Python does for you and hides. `np.cumsum` is a prefix sum; a range sum `P[r+1]-P[l]` is how you integrate a sampled signal over an arbitrary window in `O(1)`, and how a cumulative distribution turns into a "how much probability mass lies in `[a,b)`" query. `collections.Counter` is a frequency map — the exact structure a BPE tokenizer uses to count adjacent-pair frequencies (Chapter 10 of the C course built it). `dict` lookup by complement is the two-sum pattern; the same "store one half, query the other half" trick is how you match particle pairs in a collision detector without an `O(n^2)` sweep. Sorting-as-preprocessing is how you sweep interval overlaps in an event-driven simulation, and how you compute an h-index-like threshold in a ranked list. Matrix index arithmetic — `k = i*ncols + j` — is literally how a `float*` buffer becomes a 2-D tensor in every C ML runtime; transpose and rotate are the memory-layout operations behind `np.transpose` and `torch.rot90`, and Game of Life is the simplest possible stencil update, the same shape as a Jacobi iteration or a cellular-automaton PDE solver. Doing these by hand in C makes you see the memory traffic that NumPy hides.

---

## 1. Hash-set membership & dedup

### The idea

A **hash set** gives an average `O(1)` membership test: *has this element been seen before?* That is the whole pattern. Whenever a problem asks you to find a duplicate, check whether some value has already appeared, or remove repeated elements, ask yourself: "do I only need to know *whether* a value was seen — not where, and not how many times?" If yes, a hash set is the right tool.

### The invariant

During a single left-to-right pass, at every index `i` the set contains *exactly* the elements at indices `0..i-1`. Each new element either hits (it is in the set — react immediately) or is inserted. Because insert and lookup are `O(1)` average, the whole algorithm is `O(n)` time and `O(n)` space — versus the `O(n^2)` brute force that compares every element with every other.

### When to recognise it

Signal words: *duplicate*, *unique*, *does there exist*, *missing number*, *cycle in a functional graph* (follow `x -> f(x)`; the first repeated value closes the cycle).

### The C data structures

Python has `set`. C has nothing — you write it (Chapter 10, `../../c_learning/10_data_structures/lesson.md`, section 6). For integer keys the simplest good choice is **open addressing with linear probing**:

```
slots:  [ empty | 17 | 42 | empty | 5 | empty | empty | 33 ]
         0        1    2    3       4   5       6       7
hash(17) = 1, hash(42) = 2, hash(5) = 4, hash(33) = 7 (or 6 and probed on)
```

Keep the table at most ~50-70% full and at least the size of the input, and every probe sequence is short on average. Use a sentinel for "empty" that cannot appear as a key, or a parallel `used[]` byte array when any `int` may be a key.

```c
typedef struct { int *keys; unsigned char *used; size_t cap; } IntSet;

static size_t hash_int(int x, size_t cap) {
    unsigned h = (unsigned)x * 2654435761u;      /* Knuth multiplicative hash */
    return h & (cap - 1);                          /* cap is a power of two */
}
/* Returns 1 if x was already present, 0 if it was inserted now. */
int set_insert(IntSet *s, int x) {
    size_t i = hash_int(x, s->cap);
    while (s->used[i]) {
        if (s->keys[i] == x) return 1;
        i = (i + 1) & (s->cap - 1);                /* linear probe, wrap around */
    }
    s->used[i] = 1; s->keys[i] = x;
    return 0;
}
```

`example.c` in this folder has a complete, tested version with `set_contains`.

### Worked mini-example: contains-duplicate on `[3, 1, 4, 1, 5]`

```
i=0  x=3   set={}          not present -> insert   set={3}
i=1  x=1   set={3}         not present -> insert   set={3,1}
i=2  x=4   set={3,1}       not present -> insert   set={3,1,4}
i=3  x=1   set={3,1,4}     PRESENT     -> return true
```

Three inserts, one hit, done after four elements. Brute force would have done ten comparisons.

### The array as its own hash set

Special case that appears again and again: when values are bounded to a range like `1..n`, the input array itself can act as the set. Two techniques:

1. **Sign marking.** Value `v` "belongs" to index `|v|-1`. Seeing `v`, negate `a[|v|-1]`. If it is already negative, `|v|` has been seen. Always read `|a[i]|`, never `a[i]`, because earlier marks may have flipped its sign.
2. **Cyclic sort / put-in-place.** Swap `v` into index `v-1` until the value at `i` is either correct or out of range. Each swap puts at least one more value in its final place, so the total number of swaps is `O(n)`. Afterwards, the first `i` with `a[i] != i+1` reveals the missing value `i+1`.

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

This drops the extra space to `O(1)` and is the key to `first-missing-positive`.

### Pitfalls

- "Duplicate" means *any* two equal values, not adjacent ones — sorting-then-comparing-neighbours is the alternative (`O(n log n)`, `O(1)` extra), not a mistake, but do it consciously.
- Some problems in this group do not need a set at all: XOR of all elements cancels pairs (`x ^ x = 0`), and the sum formula `n(n+1)/2` finds a single missing value. Know both the hash solution and the arithmetic one — the arithmetic one is `O(1)` space.
- Sum-based tricks overflow `int` on large inputs; XOR does not.

**Python equivalent:** `seen = set(); for x in a: if x in seen: ...; seen.add(x)`.

---

## 2. Frequency counting

### The idea

When a problem talks about *how many times*, *most frequent*, *least frequent*, or comparing the *composition* of two collections, the structure is a **hash map from value to count** (key → counter). One pass accumulates the count of each element (or character); a second pass — or a direct scan of the map — answers the question from the counters.

### The invariant

The map always describes exactly what has been seen so far — every update is `+1` on an existing or new key. Update and lookup are `O(1)` average, so the total is typically `O(n)`, or `O(n log n)` if you must sort or extract the top `k` at the end.

### Multiset comparison

Special case: comparing two **multisets**, e.g. anagrams. Build *one* map, add `+1` per character from the first string and `-1` per character from the second, and check that every counter is zero at the end. When the alphabet is fixed and small (26 lowercase letters, 128 ASCII, 256 bytes), replace the map with a fixed-size array `int cnt[26]` indexed by `c - 'a'` — the same speed advantage as "array as hash set" in section 1.

### When to recognise it

Signal words: *frequency*, *most common*, *top-k*, *anagram*, *same composition*, *majority*.

### The C data structures

- Small fixed alphabet: `int cnt[256] = {0}; cnt[(unsigned char)c]++;` — cast to `unsigned char` first; a plain `char` may be negative and index out of bounds.
- Arbitrary integer keys: an open-addressing map of `(int key, int count)` — see `example.c`.
- Bucket sort by frequency: counts live in `[1, n]`, so an array of `n+1` buckets (each a small dynamic list) indexed by count replaces an `O(n log n)` sort with an `O(n)` scan from bucket `n` down.

```c
/* Anagram check with a fixed alphabet. Returns 1 if s and t are anagrams. */
int is_anagram(const char *s, const char *t) {
    int cnt[26] = {0};
    size_t i;
    for (i = 0; s[i] && t[i]; i++) { cnt[s[i]-'a']++; cnt[t[i]-'a']--; }
    if (s[i] || t[i]) return 0;              /* different lengths */
    for (int c = 0; c < 26; c++) if (cnt[c]) return 0;
    return 1;
}
```

### Worked mini-example: top-2 frequent on `[1, 1, 1, 2, 2, 3]`

```
count:   1 -> 3,  2 -> 2,  3 -> 1                (one pass)
buckets: [0]=[] [1]=[3] [2]=[2] [3]=[1] [4]=[] [5]=[] [6]=[]   (n+1 = 7 buckets)
scan from bucket 6 down: bucket 3 gives 1, bucket 2 gives 2 -> k=2 collected, stop
```

No sort; frequencies are bounded by `n`, so direct indexing replaces comparison sorting — the same argument as counting sort.

### Boyer-Moore voting (majority element)

The bridge from counting to invariant-based thinking. Keep a candidate and a counter; when the counter is 0, adopt the current element as candidate; increment on match, decrement otherwise. Because the majority element occurs more than `n/2` times, it can never be fully "cancelled out" by the others — the final candidate is the majority. `O(n)` time, `O(1)` space, no map at all.

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

- Membership is not enough when items are *consumed* (ransom note: each magazine letter is used once) — you must count.
- Several keys can share one frequency: a bucket must be a list, not a single value.
- A `26`-array assumes lowercase ASCII; Unicode or mixed case needs a general map. Case is significant unless the problem says otherwise.
- Two-level thinking: the *values* of a frequency map can themselves become keys of a second set ("are all occurrence counts distinct?").

**Python equivalent:** `collections.Counter(a).most_common(k)`; `Counter(s) == Counter(t)` for anagrams.

---

## 3. Hash-map index lookup

### The idea

The third hashing pattern neither counts nor tests membership: it uses the **hash map as memory** — "have I seen value `x`, and if so, *where* or *what did it correspond to*?" The classic is `two-sum`: instead of trying every pair (`O(n^2)`), store each visited number's **index** in the map, and at every step ask "is the complement `target - x` already in the map?" One pass finds the answer.

### The invariant

The map always contains exactly the values (with their latest associated datum — index, matching character, count from another source) seen *before* the current element. Lookup and update are `O(1)` average, so total time drops from `O(n^2)` to `O(n)` — that is the whole value proposition.

**Order matters:** query first, then insert. Otherwise an element can pair with itself.

### Bijection between two sequences

Generalisation: a **one-to-one mapping** between two sequences (`isomorphic-strings`, `word-pattern`). One map `s -> t` is not enough — two different `s` characters could map to the same `t` character. Keep two maps, `mapST` and `mapTS`; at each position, if either mapping exists it must agree with the current pair; if neither exists, add both at once. The state of the two maps is always a consistent *partial bijection* on what has been seen so far. Counter-example that breaks the one-map version: `s = "badc", t = "baba"`.

### When to recognise it

Signal words: *pair summing to X*, *does this structure match that one*, *first non-repeating*, *transform to X and check if already seen*. Difference from section 2: here the map's **value** carries meaning (an index, a partner character, a count of pairs from another source), not just presence or count.

### The C data structures

An open-addressing map `int key -> int value` (index). For character bijections, two arrays `int st[256], ts[256]` initialised to `-1` are the entire "map". For word ↔ character, the key is a string: hash the bytes (FNV-1a or djb2), store the `char *` in the slot, and compare with `strcmp` on probe collisions — Chapter 10's string-keyed table.

```c
/* Two-sum skeleton. Returns 1 and fills *i,*j on success. Uses an int->int map. */
int two_sum(const int *a, int n, int target, int *i, int *j) {
    IntMap m; map_init(&m, 2 * n);
    for (int k = 0; k < n; k++) {
        int idx;
        if (map_get(&m, target - a[k], &idx)) {    /* query BEFORE insert */
            *i = idx; *j = k; map_free(&m); return 1;
        }
        map_put(&m, a[k], k);                      /* value -> its index */
    }
    map_free(&m);
    return 0;
}
```

### Worked mini-example: two-sum, `a = [2, 7, 11, 15, 3]`, `target = 10`

```
k=0  a=2   need 8   map={}              miss -> put 2->0
k=1  a=7   need 3   map={2:0}           miss -> put 7->1
k=2  a=11  need -1  map={2:0,7:1}       miss -> put 11->2
k=3  a=15  need -5  map={2:0,7:1,11:2}  miss -> put 15->3
k=4  a=3   need 7   map={...,15:3}      HIT idx=1 -> answer (1, 4)
```

The found pair is always in index order (first index smaller) because the map only holds earlier elements.

### Scaling the idea: 4Sum II

Four arrays, count quadruples summing to 0. Brute force `O(n^4)`. Instead put all `n^2` sums `A[i]+B[j]` into a map `sum -> count`, then for every `C[k]+D[l]` add `map[-(C[k]+D[l])]` to the answer. Same "store one half, query the complement of the other half", but the unit is a pair instead of a single number: `O(n^2)` time and space. Because many pairs can produce the same sum, the map must hold counts, not presence.

### Pitfalls

- Duplicated values keep only the latest index in the map — harmless for two-sum because query-before-insert still finds the first valid pair.
- "First unique character" needs **two passes**: while counting, you cannot know whether the current character reappears later. Return the first index whose *final* count is 1, not the first character you *see*.
- Do not forget the second direction of a bijection.

**Python equivalent:** `seen: dict[int,int]`; `if target - x in seen: return seen[target-x], i`.

---

## 4. Prefix sums

### The idea

The **prefix sum** `P[i] = sum(a[0..i-1])`, with `P[0] = 0`, precomputes the cumulative sum up to every index. Built once in `O(n)`, it answers the sum of any subarray `[l, r]` (inclusive) in `O(1)`: `P[r+1] - P[l]`. Instead of recomputing an `O(n)` sum for every query, `q` queries cost `O(n + q)` rather than `O(nq)`. Recognise it whenever a problem has *many subarray-sum queries on an immutable array*, or asks for a subarray whose sum satisfies a condition.

### The invariant

`P[i]` is always the sum of the whole prefix `a[0..i-1]`, independent of which range is queried later — build and query are completely separate phases. Note the **half-open convention**: `P[i]` is the sum *before* index `i`, not including it. This is the single most common off-by-one in the pattern.

```
a  = [ 3, -1,  4,  1, -5,  9 ]
P  = [ 0,  3,  2,  6,  7,  2, 11 ]      (n+1 entries)
sum(a[1..3]) = P[4] - P[1] = 7 - 3 = 4     (-1 + 4 + 1)
sum(a[0..5]) = P[6] - P[0] = 11
```

### The strongest combination: prefix sum + hash map

To find or count subarrays whose sum is exactly `k`: accumulate the running prefix, and at each index ask the map whether `prefix - k` has been seen before — if it has, the subarray between that earlier position and here sums to `k`. This is `two-sum` applied to cumulative sums, and it solves "subarray sum = k" in `O(n)` with no double loop.

What you store in the map depends on the question:

| Question | Map stores | Initialisation |
|---|---|---|
| *how many* subarrays sum to `k` | prefix value → **count** | `{0: 1}` (empty prefix) |
| *longest* subarray with sum 0 / balanced | prefix value → **first index** | `{0: -1}` |
| sum divisible by `k` (exists, length >= 2) | prefix mod `k` → **first index** | `{0: -1}` |
| *how many* subarrays divisible by `k` | prefix mod `k` → **count** | `{0: 1}` |

The initialisation encodes "the empty prefix has sum 0 at position -1" and is mandatory — without it, subarrays starting at index 0 are missed, and negative numbers make it worse.

```c
/* Count subarrays with sum == k. Uses a long long -> int map (prefix may exceed int). */
long long count_subarrays_sum_k(const int *a, int n, long long k) {
    LLMap m; llmap_init(&m, 2 * n + 2);
    llmap_put(&m, 0, 1);                           /* empty prefix seen once */
    long long prefix = 0, ans = 0;
    for (int i = 0; i < n; i++) {
        prefix += a[i];
        int c;
        if (llmap_get(&m, prefix - k, &c)) ans += c;  /* query first ... */
        llmap_add(&m, prefix, 1);                     /* ... then record */
    }
    llmap_free(&m);
    return ans;
}
```

### Worked mini-example: count subarrays with sum 2 in `[1, 1, 1, -1, 2]`

```
map={0:1}  prefix=0 ans=0
i=0 a=1   prefix=1   need -1  miss    map={0:1,1:1}
i=1 a=1   prefix=2   need 0   HIT +1  map={0:1,1:1,2:1}         ans=1  ([1,1])
i=2 a=1   prefix=3   need 1   HIT +1  map={0:1,1:1,2:1,3:1}     ans=2  ([1,1] at 1..2)
i=3 a=-1  prefix=2   need 0   HIT +1  map={0:1,1:1,2:2,3:1}     ans=3  ([1,1,1,-1])
i=4 a=2   prefix=4   need 2   HIT +2  map={...,4:1}             ans=5  ([-1,2] and [2])
```

Five subarrays, one pass. Note the last step adds 2 because prefix value 2 had been seen twice.

### Modulo variant

If `P[i] mod k == P[j] mod k` with `i < j`, then `P[j] - P[i] ≡ 0 (mod k)`: the subarray `a[i..j-1]` sums to a multiple of `k`. In C, `%` on a negative operand yields a negative result (`-1 % 5 == -1`), so normalise: `r = ((prefix % k) + k) % k`. Forgetting this produces wrong map hits.

### Prefix products

Same idea multiplicatively: `result[i] = prod(a[0..i-1]) * prod(a[i+1..n-1])`. First pass left to right fills the prefix product into `result`; second pass right to left multiplies in a running suffix product held in one variable. Works with zeros (division does not), `O(n)` time, `O(1)` extra space.

### When to recognise it

Signal words: *subarray sum*, *many queries on an immutable array*, *divisible by*, *balance between 0s and 1s* (map 0 → -1 and the balanced subarray becomes sum 0), *running total*, *highest altitude / cumulative gain*, *pivot index* (left sum == total − left − a[i]).

Often you do not even need to materialise `P`: if the query is "max over the whole walk" or "the pivot in one pass", a running scalar suffices.

### Pitfalls

- `P` has `n+1` entries; allocate `n+1`, not `n`.
- `P[r+1] - P[l]` for inclusive `[l, r]`; `P[r] - P[l]` for half-open `[l, r)`.
- Prefix sums of `int` data overflow `int` quickly — accumulate in `long long`.
- Left sum vs. current element: in pivot-index, compare *before* adding `a[i]` to the left sum.
- Overwriting the input with its running sum is fine only if nothing needs the original later.

**Python equivalent:** `itertools.accumulate(a, initial=0)`; `np.cumsum`.

---

## 5. Sorting as preprocessing

### The idea

Many problems that do not look like "sorting problems" become much easier once the array (or the intervals, events, objects) is **sorted first** by some key. Sorting costs `O(n log n)`, but it exposes structure — adjacency, coverage, monotonicity — so that the rest of the solution is a simple one-pass greedy sweep in `O(n)`. Total time is still `O(n log n)`, but the *reasoning* becomes radically simpler.

### Two signals

**Overlapping intervals.** Once intervals are sorted by start (or by end), overlap can only occur between *neighbours* in sorted order, not between arbitrary pairs — `O(n^2)` pair checks become an `O(n)` sweep.

**"Largest/smallest first" greedy that is only correct on sorted input.** When the task is to find an optimal order or selection (h-index, task scheduling, selecting non-overlapping events), sorting reveals the order in which a greedy choice is provably right. The exchange argument — "swapping a greedy choice for any alternative cannot improve the result" — works precisely because sorting has already lined up the competing alternatives.

### The invariant of the interval sweep

Maintain a "current merged interval" `[cs, ce]`. Because every later interval starts at or after the current one, the next interval `[s, e]` either overlaps (`s <= ce`: extend `ce = max(ce, e)`) or does not (`s > ce`: emit `[cs, ce]`, start a new one). The `max` is essential — a fully nested interval has `e < ce`.

### The C data structures

`qsort` from `<stdlib.h>` with a comparator (Chapter 11, `../../c_learning/11_function_pointers_and_generics/lesson.md`). Sort an array of structs by a field; the comparator receives `const void *` and must return negative / zero / positive.

```c
typedef struct { int start, end; } Interval;

static int cmp_by_start(const void *pa, const void *pb) {
    const Interval *a = pa, *b = pb;
    return (a->start > b->start) - (a->start < b->start);   /* no overflow, unlike a - b */
}

/* Merge overlapping intervals in place; returns the new count. */
int merge_intervals(Interval *v, int n) {
    if (n == 0) return 0;
    qsort(v, (size_t)n, sizeof v[0], cmp_by_start);
    int w = 0;                                   /* write index of last merged interval */
    for (int i = 1; i < n; i++) {
        if (v[i].start <= v[w].end) {
            if (v[i].end > v[w].end) v[w].end = v[i].end;
        } else {
            v[++w] = v[i];
        }
    }
    return w + 1;
}
```

Never write `return a->start - b->start;` — it overflows for large magnitudes and gives the wrong sign. For descending order swap the operands. For string concatenation comparisons (largest-number), build `a+b` and `b+a` into two small buffers and `strcmp`.

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
| largest concatenated number | custom `a+b > b+a` | concatenate |
| custom order given by another array | frequency map + that array's order | emit counts, then sort leftovers |

Sorting by *start* for the non-overlapping selection gives a wrong answer — the earliest-ending interval is the one that leaves the most room for later choices.

### Pitfalls

- Overlap condition is `<=`, not `<`: `[1,2]` and `[2,3]` touch and merge (or the balloon starting exactly at the arrow still bursts) — check the problem's convention.
- When the input is *already* sorted and disjoint, do not sort again — a single `O(n)` three-phase pass is the point.
- When the key range is bounded (`citations in [0,n]`, frequencies in `[1,n]`), counting sort turns `O(n log n)` into `O(n)`.
- Task scheduler: the formula `(f_max - 1) * (n + 1) + (#tasks sharing f_max)` is a lower bound; the answer is `max(formula, total_tasks)`.
- Largest number: `[0, 0]` yields `"00"` — normalise to `"0"`.

**Python equivalent:** `sorted(intervals, key=lambda x: x[0])`; `functools.cmp_to_key` for pairwise comparators.

---

## 6. Matrix & array manipulation

### The idea

This group is not one algorithmic pattern but the craft of **index arithmetic**: in matrix problems you usually know the algorithm at once (traverse, rotate, copy), and the difficulty is deriving the right row and column indices without off-by-one errors. Three ideas recur.

### Index transforms

- **Transpose:** `(i, j) -> (j, i)`. Output dimensions are swapped for a non-square matrix — allocate `n x m` for an `m x n` input. In place on a square matrix, swap only the upper triangle `i < j`; swapping every pair would swap each twice and restore the original.
- **Rotate 90° clockwise:** transpose, then reverse each row. Coordinates: transpose sends `(i,j) -> (j,i)`, reversing the row sends that to `(j, n-1-i)` — exactly the clockwise rotation formula. Both steps in place, `O(n^2)` time, `O(1)` extra.
- **Flatten / reshape:** `k = i * ncols + j` flattens; `i = k / new_cols, j = k % new_cols` reshapes. Check `r*c == m*n` *before* any index computation. This is the row-major layout of every C 2-D buffer.
- **Diagonals:** main diagonal `i == j`; anti-diagonal `i + j == n-1`; each anti-diagonal of a rectangular matrix has constant `i + j`. For a diagonal-sum on odd `n`, the centre cell `i == n-1-i` belongs to both — count it once.
- **Toeplitz check:** a local condition suffices globally: `m[i][j] == m[i-1][j-1]` for all `i>0, j>0`, because each diagonal is a chain of adjacent links.
- **Spiral walk:** four bounds `top, bottom, left, right`, shrunk one at a time; after each direction re-check `top <= bottom` and `left <= right`, or a one-row / one-column remainder gets visited twice.

```c
/* Row-major storage: element (i,j) of an m x n matrix lives at a[i*n + j]. */
static void transpose(const int *a, int m, int n, int *out /* n x m */) {
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            out[j * m + i] = a[i * n + j];
}
static void rotate_cw_inplace(int *a, int n) {
    for (int i = 0; i < n; i++)                /* transpose: upper triangle only */
        for (int j = i + 1; j < n; j++) {
            int t = a[i*n + j]; a[i*n + j] = a[j*n + i]; a[j*n + i] = t;
        }
    for (int i = 0; i < n; i++)                /* reverse each row */
        for (int l = 0, r = n - 1; l < r; l++, r--) {
            int t = a[i*n + l]; a[i*n + l] = a[i*n + r]; a[i*n + r] = t;
        }
}
```

### Worked mini-example: rotate a 3x3

```
        1 2 3        transpose     1 4 7      reverse rows     7 4 1
        4 5 6         ------->     2 5 8       ---------->     8 5 2
        7 8 9                      3 6 9                       9 6 3
```

Check with the formula: `(0,0)=1 -> (0, n-1-0) = (0,2)`. The top-right of the result is 1. Correct.

Spiral on the same 3x3 (`top=0 bottom=2 left=0 right=2`):

```
row top  L->R : 1 2 3          top=1
col right T->B: 6 9            right=1
row bottom R->L (top<=bottom): 8 7      bottom=1
col left B->T (left<=right):   4        left=1
row top  L->R : 5              top=2   -> top > bottom, stop
output: 1 2 3 6 9 8 7 4 5
```

### In-place editing with marker state

When the matrix may not be copied, its **first row and first column** can serve as memory for which rows/columns to zero — the same "the structure is its own memory" idea as the sign-marking trick of section 1. Because row 0 and column 0 might themselves contain a zero, save their own status in two booleans *before* the marking pass, and handle them last.

### Simulation with two states in one cell

When the whole grid must update *simultaneously* from current values (Game of Life), direct overwriting corrupts the neighbour counts of later cells. Without an extra matrix, encode **both old and new state in the same cell** with two bits: `cell = old + 2*new` (values 0-3). Neighbour counting reads `cell & 1` (the old state) regardless of whether the neighbour is already encoded; a second pass shifts every cell right by one (`cell >>= 1`). `O(mn)` time, `O(1)` extra space. This is the smallest stencil computation you will ever write; a Jacobi step or a heat-equation update has the same shape, just with `double` and a second buffer instead of bit packing.

### When to recognise it

Signal words: *matrix*, *grid*, *rotate*, *transpose*, *diagonal*, *in place, without extra memory*, *spiral*, *reshape*.

### The C data structures

A flat `int *a` of `m*n` elements with `a[i*n + j]` is the right representation — it is one allocation, cache-friendly, and identical to how BLAS and every tensor library store row-major data (`../../c_learning/04_arrays_and_strings/lesson.md`, `../../c_learning/06_dynamic_memory/lesson.md`). LeetCode's `int **matrix` interface is an array of row pointers; convert or index as `matrix[i][j]`, but do not confuse the two layouts.

### Pitfalls

- Always draw a small `3x3` (and a `2x3`!) on paper before writing an index formula — it finds off-by-ones faster than reading code.
- Boundary handling: neighbour loops must clamp `0 <= i+di < m`, `0 <= j+dj < n`. Test the top, bottom, left and right edges separately (diagonal traverse turns at each edge differently).
- Non-square matrices swap dimensions under transpose; square-only tricks (in-place transpose, rotate) must not be applied to them.
- Many problems here chain several ideas: spiral = four-bound simulation; rotation = two transforms; set-zeroes borrows the section-1 marking principle.

**Python equivalent:** `a.T`, `np.rot90(a, -1)`, `a.reshape(r, c)`, `a.ravel()`.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Time / extra space |
|---|---|---|
| duplicate, unique, does there exist, seen before | hash set, single pass | `O(n)` / `O(n)` |
| values bounded to `1..n`, "without extra space" | array as its own set: sign marking or cyclic sort | `O(n)` / `O(1)` |
| exactly one element odd-count, rest pairs; one missing from `0..n` | XOR / sum formula | `O(n)` / `O(1)` |
| how many times, most frequent, top-k, anagram, majority | frequency map (or `int[26]`); bucket by count | `O(n)` / `O(n)` or `O(1)` |
| majority element only | Boyer-Moore voting | `O(n)` / `O(1)` |
| pair summing to X, complement, first non-repeating | hash map value → index, query before insert | `O(n)` / `O(n)` |
| does A's structure mirror B's (1-1 mapping) | two maps, both directions | `O(n)` / `O(alphabet)` |
| quadruples / pairs of pairs summing to X | map of pair sums → count | `O(n^2)` / `O(n^2)` |
| many range-sum queries, immutable array | prefix array `P[r+1]-P[l]` | `O(n)` build / `O(1)` query |
| subarray with sum = k (count) | prefix + map value → count, init `{0:1}` | `O(n)` / `O(n)` |
| longest balanced / zero-sum subarray | prefix + map value → first index, init `{0:-1}` | `O(n)` / `O(n)` |
| subarray sum divisible by k | prefix mod k + map (normalise negatives) | `O(n)` / `O(k)` |
| product of all except self, no division | prefix and suffix products | `O(n)` / `O(1)` |
| intervals, overlap, merge | sort by start, sweep neighbours | `O(n log n)` / `O(n)` |
| max non-overlapping / min removals / min arrows | sort by **end**, greedy | `O(n log n)` / `O(1)` |
| k-th largest, threshold on sorted (h-index) | sort desc (or counting sort if bounded) | `O(n log n)` or `O(n)` |
| custom order, concatenation order | `qsort` with pairwise comparator | `O(n log n)` |
| matrix rotate / transpose / spiral / reshape | index formulas, four bounds, `k = i*n + j` | `O(mn)` / `O(1)` |
| grid updated simultaneously, in place | two-bit state per cell | `O(mn)` / `O(1)` |
| zero rows/columns in place | first row/col as markers + two flags | `O(mn)` / `O(1)` |

---

## Gotchas in C specifically

- **You write the hash table.** There is no `set` or `dict`. Keep a small, tested `IntSet` / `IntMap` (open addressing, power-of-two capacity, `used[]` bytes or a sentinel key) in your solutions folder and paste it in. Size the table at `>= 2 * n` slots so the load factor stays below 0.5 and probes stay short. Never use `%` with a negative key for the bucket index — cast to `unsigned` first.
- **`char` may be signed.** `cnt[c]` with `char c = 0xE9` indexes negative. Always `cnt[(unsigned char)c]`.
- **Integer overflow.** Sums of `int` arrays (prefix sums, `n(n+1)/2`, pair sums in 4Sum II) exceed `INT_MAX` fast. Accumulate in `long long`. In comparators, `a - b` overflows; use `(a > b) - (a < b)`.
- **Negative modulo.** `-7 % 5 == -2` in C (truncation toward zero). Normalise with `((x % k) + k) % k` before using a remainder as a map key or bucket index.
- **Half-open ranges.** `P` has `n+1` entries; `sum[l..r] = P[r+1] - P[l]`. Allocate `n+1`. Off-by-one here is the most common bug in this whole chapter.
- **`qsort` comparator signature** is `int (*)(const void *, const void *)`. Cast inside, return sign only, and make the comparison a strict weak ordering (largest-number's `a+b` vs `b+a` is one, but you must trust the proof).
- **Row-major flat arrays vs `int **`.** LeetCode gives `int **matrix` plus `int matrixSize, int *matrixColSize`. Your own code should prefer one flat buffer `a[i*n + j]`. Do not mix the two.
- **Returning arrays.** LeetCode-in-C returns `malloc`ed arrays and writes the length through `int *returnSize`; for 2-D results it also wants `int **returnColumnSizes`. Allocate with `malloc`, never return a pointer to a local array (Chapter 06, `../../c_learning/06_dynamic_memory/lesson.md`).
- **`abs` for sign marking** is in `<stdlib.h>`; `INT_MIN` has no positive counterpart — bounded-range problems guarantee it will not appear, but your own tests should stay in range.
- **Strings as keys** need copying (`strdup` is POSIX, not C11 — write a `malloc + memcpy`) and `strcmp` on collision, not `==`.
- **Bit tricks.** `x ^ x == 0` works on any width; `cell >>= 1` on a non-negative `int` is fine, but never right-shift a negative value and expect a defined result.

---

## Common mistakes checklist

- [ ] Inserting into the hash map *before* querying the complement (two-sum pairs an element with itself).
- [ ] Reading `a[i]` instead of `abs(a[i])` after sign marking has begun.
- [ ] Placing values outside `[1, n]` during cyclic sort (index out of bounds).
- [ ] Forgetting that a duplicate is any two equal values, not adjacent ones.
- [ ] Using a set where a counter is required (ransom note, 4Sum II, relative sort with repeats).
- [ ] Only one direction of a bijection (isomorphic strings, word pattern).
- [ ] Missing the `{0: 1}` / `{0: -1}` initialisation in prefix-sum + map.
- [ ] Using `P[r] - P[l]` for an inclusive range.
- [ ] Updating the "first index" for a remainder when the problem needs the *earliest* occurrence (continuous-subarray-sum, contiguous-array).
- [ ] Sorting intervals by start when the greedy needs end (non-overlapping intervals, arrows).
- [ ] `<` instead of `<=` on the touching-interval boundary.
- [ ] Assigning `cur.end = next.end` instead of `max(cur.end, next.end)` when merging.
- [ ] Transposing the full square instead of the upper triangle (undoes itself).
- [ ] Reading a neighbour's already-updated state in a simultaneous grid update.
- [ ] Spiral: not re-checking the bounds before the third and fourth sides.
- [ ] Prefix sums or pair sums in `int` instead of `long long`.
- [ ] Zeroing row 0 / column 0 markers before saving their own original status.

---

## You can move on when...

- Given a problem statement, you can name the pattern and its complexity within a minute, before coding.
- Your `IntSet` / `IntMap` compiles warning-free, passes your own tests with collisions and negative keys, and you can write it from memory.
- You can explain *why* the longest-consecutive-sequence inner `while` loop is `O(n)` in total, and why the two-sum map must be queried before it is updated.
- You can write the prefix-sum + hash-map skeleton (with correct initialisation) for both "count" and "longest" variants without looking.
- You can sort an array of structs with `qsort` and a safe comparator, and state which key (start vs. end) each interval problem needs and why.
- You can rotate a square matrix in place and walk a rectangular one in a spiral, both correct on `1xN`, `Nx1`, `2x3` and `3x3` inputs.
- You have solved at least the level-2 problems in every unit of `problems.md` and at least three level-3 ones, in C, with your own `main()` tests.
