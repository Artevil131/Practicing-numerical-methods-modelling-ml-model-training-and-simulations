# Chapter 02 — Two pointers & sliding window

## What you'll be able to do after this chapter

- Replace an `O(n^2)` "all pairs" scan over a **sorted** array with two opposite-end indices that meet in the middle, and prove why no valid pair is skipped.
- Detect a cycle in a linked list or in any "each node has exactly one successor" structure (a function graph) in `O(1)` space with Floyd's tortoise-and-hare, and locate where the cycle starts.
- Maintain a window statistic (sum, count, max via monotonic deque) over every length-`k` subarray in `O(n)` instead of `O(nk)`.
- Grow and shrink a variable window with a monotone predicate to find the longest / shortest valid subarray, and count subarrays with the `atMost(k) - atMost(k-1)` trick.
- Upgrade the window's state from a single number to a frequency table (`int cnt[256]` or a hash map) plus a `matched` counter, so anagram / covering / at-most-k-distinct problems stay `O(1)` per step.
- Partition an array in place into two or three regions with read/write pointers or the Dutch national flag scheme, and reuse the same partition routine inside quickselect.

## Why this matters for ML / numerics / sims

Every pattern in this chapter is a way of visiting an array *once* while keeping a small piece of state, which is the shape of almost every data-processing kernel you will write in C:

- **Fixed window = convolution / moving average / FIR filter.** A moving average over a signal is `sum += x[r] - x[l]` per step. A box filter on an image is a 2-D version. A running variance for batch-norm-style statistics is the same update on two accumulators.
- **Variable window = admission control and streaming statistics.** "Longest run where the loss stays under a threshold", "shortest span of tokens covering all required symbols", "how many contiguous frames had at most `k` outliers" are all variable-window questions over a time series.
- **Monotonic deque = max-pooling in `O(n)`.** Sliding-window maximum is exactly 1-D max-pool with stride 1, computed without a heap.
- **Opposite pointers = merging and pairing sorted data.** Matching sorted timestamps, pairing sorted particle positions by distance, computing symmetric quantities around a centre.
- **Fast/slow = cycle detection in iterated maps.** An iterated function `x <- f(x)` (a fixed-point solver, a pseudo-random generator, a Markov-chain state) either converges or cycles; Floyd finds the cycle without storing the history.
- **In-place partition = quickselect.** Selecting the median or a `k`-th percentile of a residual vector in expected `O(n)` without sorting is quickselect, which is the Dutch-flag partition plus one-sided recursion.

In Python you would call `np.convolve`, `collections.deque`, `np.partition`. In C you write the five-line loop, so you must know the invariant that makes the loop right.

---

## 1. Opposite ends

### The idea

Two indices, `left` at the start and `right` at the end of an array, move towards each other until they meet. The pattern applies when the array is **sorted** (or sorting is acceptable preprocessing) and the answer depends on the relationship between two elements — their sum, difference, or symmetry.

### Why it is correct, not just fast

Take a sorted array and a pair `(left, right)`. If `arr[left] + arr[right]` is too large, increasing `left` can never help — the only way to reduce the sum is to move `right` leftwards. Symmetrically, a sum that is too small forces `left` rightwards. Each step therefore rules out an **entire set** of pairs at once (every pair that keeps one index fixed and moves the other in the "worse" direction), not just one pair. That is why the `O(n^2)` brute force collapses to `O(n)` in one pass.

**Invariant:** at no point does a valid pair exist with both indices inside the regions already passed over. The pointers approach each other monotonically and never move back.

### Recognising it

Signal words: *sorted array*, *palindrome*, *pair whose sum / product satisfies a condition*, *two walls / containers / edges*.

Two flavours of the "which pointer moves" decision appear in the problem set:

| Decision rule | Example | What it compares |
|---|---|---|
| Move based on the **value of the objective** (sum too big / too small) | Two Sum II, 3Sum | `arr[l] + arr[r]` vs `target` |
| Move the pointer that **limits** the current answer | Container With Most Water, Trapping Rain Water | the shorter wall / the smaller running max |

### Complexity

`O(n)` time — each pointer moves at most `n` steps in total, regardless of inner `while` loops that skip characters (this is the *amortised* argument you will see again in the variable-window section). `O(1)` extra space when working in place.

### C layout

A plain `int *arr` (or `const char *s` for strings) plus two `size_t` or `int` indices. Prefer `int` indices here: `right` starts at `n - 1` and will be decremented, and `size_t` underflow at `0 - 1` wraps to `SIZE_MAX` silently (see `../../c_learning/12_numbers_bits_floats/lesson.md`). If you use `size_t`, write the loop as `while (left < right)` and never compute `right - 1` when `right == 0`.

```c
/* Returns 1 and writes indices if some a[i] + a[j] == target (a sorted ascending). */
static int pair_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) {
    int left = 0, right = n - 1;
    while (left < right) {
        long s = (long)a[left] + a[right];          /* long: no int overflow on the sum */
        if (s == target) { *i_out = left; *j_out = right; return 1; }
        if (s < target) left++;                      /* too small: only left can fix it */
        else            right--;                     /* too big: only right can fix it */
    }
    return 0;
}
```

### Worked example

`a = [1, 3, 4, 6, 8, 11]`, `target = 10`.

```
l=0 r=5  1+11 = 12 > 10  -> r--        [1 3 4 6 8 11]
                                          ^         ^
l=0 r=4  1+8  =  9 < 10  -> l++        [1 3 4 6 8 11]
                                          ^       ^
l=1 r=4  3+8  = 11 > 10  -> r--        [1 3 4 6 8 11]
                                            ^     ^
l=1 r=3  3+6  =  9 < 10  -> l++
l=2 r=3  4+6  = 10       -> found (2, 3)
```

Five comparisons instead of fifteen pairs. After step 1, every pair containing index 5 was ruled out at once: `11` plus anything at or above `1` already exceeds 10.

### Pitfalls

- Reversing in place: the loop condition `left < right` handles both even and odd lengths; no separate middle check is needed.
- Palindrome checks with filtering: the inner `while` loops that skip non-alphanumerics must also test `left < right`, or they run off the ends on all-punctuation input.
- Building a result "backwards" (Squares of a Sorted Array) needs a third index that starts at `n - 1` and decrements.
- 3Sum: after sorting with `qsort`, skip duplicates on **all three** indices, not just the fixed one.

**Python equivalent:** there is none built in — you'd write the same `while l < r` loop, or cheat with `set`/`dict` and pay `O(n)` memory.

---

## 2. Fast & slow pointers (cycle detection)

### The idea

Floyd's cycle detection ("tortoise and hare") moves two pointers in the **same** direction at different speeds: `slow` one step at a time, `fast` two. It applies to **linked lists** and to **function graphs** — structures where every node has exactly one successor, like the digit-square-sum sequence in Happy Number or the index chain `i -> nums[i]` in Find the Duplicate Number.

### Why it works

If there is a cycle, `fast` enters it first and starts going round while `slow` is still on its way in. Inside the cycle `fast` gains exactly one node per step on `slow`. Because the gap shrinks by exactly one and the cycle length is finite, `fast` **catches** `slow` within one full lap — it cannot jump over it unnoticed. If there is no cycle, `fast` reaches the end (`NULL`) before any meeting happens.

### Finding the cycle start (phase two)

Let `a` = distance from the head to the cycle entry, `b` = distance from the entry to the meeting point, `c` = remaining cycle length from the meeting point back to the entry (so cycle length `L = b + c`). When they meet, `slow` has walked `a + b` and `fast` has walked `2(a + b)`. The difference `a + b` is a whole number of laps, `a + b = kL`, so `a = kL - b = (k-1)L + c`. Walking `a` steps from the head and `a` steps from the meeting point both land on the entry — so reset one pointer to the head, advance both one step at a time, and they meet exactly at the cycle start.

### Recognising it

Signal words: *linked list*, *cycle*, *middle of the list*, *apply a function repeatedly until a value repeats*, *array where values are valid indices*.

### Complexity

`O(n)` time (at most about `2n` pointer moves in phase one, `n` in phase two), `O(1)` space. The alternative — a hash set of visited nodes — is also `O(n)` time but `O(n)` space and needs the hash table from `../../c_learning/10_data_structures/lesson.md`.

### C layout

For a linked list: `typedef struct Node { int val; struct Node *next; } Node;` and two `Node *`. For a function graph on an array, the "pointers" are plain `int` indices or values and "advance" is `x = f(x)`.

```c
typedef struct Node { int val; struct Node *next; } Node;

/* Returns the first node of the cycle, or NULL if the list is acyclic. */
static Node *cycle_start(Node *head) {
    Node *slow = head, *fast = head;
    while (fast && fast->next) {                 /* guard BOTH hops before taking them */
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {                      /* phase 1: met inside the cycle */
            Node *p = head;                      /* phase 2: reset one to head      */
            while (p != slow) { p = p->next; slow = slow->next; }
            return p;                            /* a == c, so they meet at the entry */
        }
    }
    return NULL;
}
```

### Worked example (function graph)

`nums = [3, 1, 3, 4, 2]` (five values in `[1,4]`, so one is duplicated). Successor of `i` is `nums[i]`.

```
0 -> 3 -> 4 -> 2 -> 3 -> 4 -> 2 ...        entry of the cycle is node 3 (a = 1, L = 3)

phase 1 (start both at 0; f(x) = nums[x]):
  step  slow            fast
   0     0               0
   1     f(0)=3          f(f(0))=f(3)=4
   2     f(3)=4          f(f(4))=f(2)=3
   3     f(4)=2          f(f(3))=f(4)=2      slow == fast == 2 -> met

phase 2 (p from the head, slow stays at the meeting point, both one step at a time):
   p=0  slow=2
   p=f(0)=3  slow=f(2)=3                     equal -> cycle entry is 3
```

3 is the duplicated value. Here `a = 1` and `c = 1` (from meeting point 2 back to entry 3), confirming `a = c`. Tracing by hand is error-prone — that is exactly why `example.c` prints each step.

### Pitfalls

- Always check `fast != NULL && fast->next != NULL` before the double step; a two-node acyclic list otherwise dereferences `NULL`.
- Middle of the list: `while (fast && fast->next)` gives the *second* middle on even lengths; `while (fast->next && fast->next->next)` gives the first. Pick deliberately.
- The phase-one meeting point is **not** the cycle start. Returning it is the single most common bug in Linked List Cycle II.
- In an array-as-function-graph you must not modify or sort the array — that breaks the chain.
- Circular Array Loop adds two validity conditions (all moves in one direction, cycle length > 1) that must be checked on every step, not only at the end.

**Python equivalent:** none built in; people usually use a `set()` of seen values and accept `O(n)` memory.

---

## 3. Fixed-size window

### The idea

When a problem asks for some quantity (sum, mean, count) over **every subarray of exactly length `k`**, brute force recomputes each window in `O(k)`, for `O(nk)` total. Sliding a fixed window does it in `O(n)`: moving the window one step right removes exactly one element on the left and adds exactly one on the right, so the window's state updates in constant time instead of being rebuilt.

### Invariant

The window state (sum, counter, frequency table) always equals the state of the current window `[right - k + 1, right]`. Proof by induction: compute the first window directly, then every shift preserves the invariant by subtracting the left edge's contribution and adding the right edge's.

### Why this is correct

Because the size is fixed, "remove left, add right" is always the same operation regardless of where the window is — there is no decision about when to shrink or grow as in the variable window. The harder variant (Sliding Window Maximum) needs a **monotonic deque** to keep the window max in `O(1)` amortised per step, because a simple counter cannot recover the maximum when the element that leaves the window *was* the maximum.

### Recognising it

Signal words: *exactly `k` long*, *every window of size `k`*, *moving average*, *choose `k` consecutive minutes/cards*, and the disguised version "take `k` from the two ends" = "leave a contiguous block of `n - k` in the middle".

### Complexity

`O(n)` time, `O(1)` space for sum/count states; `O(k)` for the deque in the maximum problem.

### C layout

For sums: one accumulator of a type wide enough for `k * max|value|` — use `long long` unless you have checked the bound. For counts over a fixed alphabet: `int cnt[26]` or `int cnt[256]` indexed by `(unsigned char)c`. For the maximum: a deque of **indices** in a plain array `int dq[n]` with `head`/`tail` indices — since each index is pushed at most once, an `n`-sized array never wraps and you do not need a ring buffer.

```c
/* Prints the sum of every window of length k. n >= k >= 1. */
static void window_sums(const int *a, int n, int k) {
    long long sum = 0;
    for (int i = 0; i < k; i++) sum += a[i];             /* first window directly */
    printf("[0,%d] sum=%lld\n", k - 1, sum);
    for (int right = k; right < n; right++) {
        sum += a[right] - a[right - k];                  /* add entering, remove leaving */
        printf("[%d,%d] sum=%lld\n", right - k + 1, right, sum);
    }
}
```

Monotonic deque skeleton for window max (indices, values decreasing from head to tail):

```c
/* dq holds indices; a[dq[head]] is the max of the current window. */
int head = 0, tail = 0;                                  /* dq[head..tail) */
for (int r = 0; r < n; r++) {
    while (tail > head && a[dq[tail - 1]] <= a[r]) tail--;   /* pop smaller-or-equal from back */
    dq[tail++] = r;
    if (dq[head] <= r - k) head++;                            /* front fell out of the window */
    if (r >= k - 1) out[r - k + 1] = a[dq[head]];
}
```

### Worked example

`a = [2, 1, 5, 1, 3, 2]`, `k = 3`, running sums:

```
window [2 1 5] . . .   sum = 8
       . [1 5 1] . .   sum = 8 + 1 - 2 = 7
       . . [5 1 3] .   sum = 7 + 3 - 1 = 9   <- max
       . . . [1 3 2]   sum = 9 + 2 - 5 = 6
```

Same array with the monotonic deque (`k = 3`), showing `dq` as indices with values in brackets:

```
r=0 a=2   dq=[0(2)]
r=1 a=1   dq=[0(2) 1(1)]
r=2 a=5   pop 1(1), pop 0(2)     dq=[2(5)]              window [0,2] max 5
r=3 a=1   dq=[2(5) 3(1)]                                window [1,3] max 5
r=4 a=3   pop 3(1)               dq=[2(5) 4(3)]         window [2,4] max 5
r=5 a=2   dq=[2(5) 4(3) 5(2)]    front 2 <= 5-3 -> drop  dq=[4(3) 5(2)]  window [3,5] max 3
```

### Pitfalls

- Do the division for an average **once at the end** on the integer max sum, not per window — avoids accumulated floating-point error. Compare `sum >= k * threshold` instead of `sum / k >= threshold`.
- Sum overflow: `k * 10^5 * 10^4` does not fit in `int`. Use `long long`.
- Contains Duplicate II: remove the element that leaves the window *before* (or consistently *after*) the membership test; if values can repeat inside the window, a set is not enough — use a map to the last index.
- Cards / bookstore: check `k >= n` separately (no middle window remains), and do not count the same customers in both the base sum and the window gain.
- The deque stores **indices**, never values — you need the index to know when the front has expired.

**Python equivalent:** `collections.deque` for the monotonic queue; `np.convolve(a, np.ones(k), 'valid')` for the sums.

---

## 4. Variable window (longest / shortest)

### The idea

A variable window keeps two indices `left` and `right` that bound the current window, but unlike the fixed window the size **lives**: `right` grows the window as long as the condition holds, and when it breaks, `left` shrinks the window until the condition holds again. It applies when you want the **longest** or **shortest** subarray satisfying a condition on a sum, a product, or a count of distinct / special elements.

### Why it is correct, not just fast

The key is **monotonicity**: if the window `[left, right]` satisfies the condition, every smaller window inside it (same `right`, larger `left`) satisfies it too (or the mirror statement for failing). Hence once it was worth moving `left` rightwards for the current `right`, it never has to move back left for any later `right` — both pointers move in one direction for the whole run.

Monotonicity is a property of the *predicate*, and you must check it before using this pattern. "Sum ≥ target" with all-positive numbers is monotone. "Sum == target" with negative numbers is not (that is a prefix-sum + hash-map problem from Chapter 01). "Exactly `k` odd numbers" is not monotone either — but "at most `k`" is, which gives the counting trick below.

### Invariant and amortised time

`left` and `right` together move at most `2n` steps regardless of how the inner loop looks. So the total is `O(n)` even though for a single `right` the `left` loop may run many times — the same amortised argument as Longest Consecutive Sequence in Chapter 01.

### Two rhythms

| Goal | When condition breaks | Shrink how far | Record answer when |
|---|---|---|---|
| **Longest** valid window (Max Consecutive Ones III, Longest Substring Without Repeating) | shrink `left` | only until the window is **just** valid again | after every `right` step |
| **Shortest** valid window (Minimum Size Subarray Sum, Minimum Window Substring) | not "breaks" — condition **becomes true** | as far as possible while still valid | at every valid shrink step |

### Counting subarrays: `atMost(k) - atMost(k-1)`

If for each `right` the window `[left, right]` is the **longest** valid window ending at `right`, then by monotonicity all `right - left + 1` subarrays `[left..right], [left+1..right], ..., [right..right]` are valid too — add that count each step. "Exactly `k`" is not monotone, but `exactly(k) = atMost(k) - atMost(k-1)`, because "at most `k-1`" is a subset of "at most `k`". Write `atMost` once, call it twice, and make `atMost(-1)` return 0.

### Complexity

`O(n)` time, `O(1)` space for numeric states (plus `O(n log n)` if a sort is needed first, as in Frequency of the Most Frequent Element).

### C layout

`int left = 0`, a `for (int right = 0; right < n; right++)` outer loop, an inner `while (!valid) { ...remove a[left]...; left++; }`, and the state as a `long long` sum / product or an `int` counter. Keep the answer in a variable initialised to `0` (longest) or `INT_MAX` / `n + 1` (shortest), and map "never updated" to the problem's required sentinel at the end.

```c
/* Length of the longest subarray with at most k zeros (a[i] in {0,1}). */
static int longest_with_k_zeros(const int *a, int n, int k) {
    int left = 0, zeros = 0, best = 0;
    for (int right = 0; right < n; right++) {
        if (a[right] == 0) zeros++;                  /* grow: include a[right]            */
        while (zeros > k) {                          /* broken: shrink until just valid   */
            if (a[left] == 0) zeros--;
            left++;
        }
        if (right - left + 1 > best) best = right - left + 1;
    }
    return best;
}
```

### Worked example

`a = [1, 1, 0, 1, 0, 1, 1]`, `k = 1`:

```
r=0 a=1 zeros=0  [1]                    len 1
r=1 a=1 zeros=0  [1 1]                  len 2
r=2 a=0 zeros=1  [1 1 0]                len 3
r=3 a=1 zeros=1  [1 1 0 1]              len 4   <- best
r=4 a=0 zeros=2  > k: drop a[0]=1, a[1]=1, a[2]=0 -> zeros=1, left=3
                 [1 0]                  len 2
r=5 a=1 zeros=1  [1 0 1]                len 3
r=6 a=1 zeros=1  [1 0 1 1]              len 4
answer 4
```

Note that `left` moved from 0 to 3 in one `right` step — three inner iterations — but it will never move back, so the total inner-loop work over the whole run is bounded by `n`.

### Pitfalls

- Longest-window problems shrink until the condition is **just** valid; shortest-window problems shrink **as far as possible**. Mixing the two gives wrong answers.
- Update the answer at **every** step, not only when the condition breaks — otherwise the final run is never counted (Max Consecutive Ones).
- Shortest-window problems must return `0` (or whatever the statement says) when no window qualifies, not the sentinel.
- Product windows: if `k <= 1` no product of positive integers is `< k` — return 0 before the loop, and use `long long` for the product.
- Longest Substring Without Repeating Characters: the simplest correct version shrinks one character at a time with a `while`; jumping `left` straight to the duplicate's index needs a last-index map, not just a set.
- `atMost(k-1)` with `k = 0` must return 0 rather than crash on a negative parameter.

**Python equivalent:** none; this is the same `while` loop. `set()` / `dict()` become `int cnt[256]` in C when the alphabet is bytes.

---

## 5. Window with a counter / frequency map

### The idea

This is the most powerful form of the variable window: the validity condition is not a single number (sum, count) but a **whole frequency distribution** — "does the window contain every character of the target string at least the required number of times", "is the window an anagram of a word", "does the window have at most `k` distinct values". Maintain a map from element to its count in the window (a `hash map`, or an array over a fixed alphabet), plus usually one integer (`matched`, "how many requirements are currently satisfied") so the validity test stays `O(1)` instead of rescanning the map on every step.

### Invariant

The map always describes exactly the current window's contents, and the helper counter (`matched` or similar) always says how many individual keys currently meet their target. Both update in `O(1)` on every `left` or `right` move.

### Why correctness carries over

The same monotonicity principle as Section 4 holds: once the window satisfies (or breaks) the condition for the current `right`, shrinking continues only until the condition flips, and `left` never moves back. The only difference from Section 4 is *what* state is maintained: one number versus a whole distribution.

### The `matched` bookkeeping

For a target histogram `need[c]` and window histogram `have[c]`:

- When adding `c` to the window: `have[c]++`; if `have[c] == need[c]` then `matched++` (this key just became satisfied).
- When removing `c`: if `have[c] == need[c]` then `matched--` (about to become unsatisfied); then `have[c]--`.
- Window is valid when `matched == number of distinct keys in need`.

For "at most `k` distinct" the counter is simply the number of keys with `have[c] > 0`: increment it when a count goes `0 -> 1`, decrement when it goes `1 -> 0`. Forgetting to remove a key whose count hit zero is the classic Fruit Into Baskets bug.

### Recognising it

Signal words: *anagram / permutation inside a string*, *cover all characters of X*, *at most / exactly `k` distinct values*, *replace at most `k` characters*, *two baskets*.

### Complexity

`O(n + m)` time (`m` to build the target histogram), `O(sigma)` space for a fixed alphabet (constant) or `O(k)` for a hash map of `k` distinct keys.

### C layout

For byte strings: `int need[256] = {0}, have[256] = {0};` indexed by `(unsigned char)s[i]` — casting matters because a plain `char` may be negative and index before the array (`../../c_learning/04_arrays_and_strings/lesson.md`). For integer keys with a large range: the open-addressing `int -> int` hash map from Chapter 01 / `../../c_learning/10_data_structures/lesson.md`, with a `remove` or a "count decremented to zero" convention.

```c
/* Length of the longest substring with at most k distinct bytes. */
static int longest_k_distinct(const char *s, int n, int k) {
    int have[256] = {0}, distinct = 0, left = 0, best = 0;
    for (int right = 0; right < n; right++) {
        unsigned char c = (unsigned char)s[right];
        if (have[c]++ == 0) distinct++;                    /* 0 -> 1: new key */
        while (distinct > k) {
            unsigned char d = (unsigned char)s[left++];
            if (--have[d] == 0) distinct--;                /* 1 -> 0: key gone */
        }
        if (right - left + 1 > best) best = right - left + 1;
    }
    return best;
}
```

### Worked example

`s = "eceba"`, `k = 2`:

```
r=0 'e'  have{e:1}           distinct=1  [e]       len 1
r=1 'c'  have{e:1,c:1}       distinct=2  [ec]      len 2
r=2 'e'  have{e:2,c:1}       distinct=2  [ece]     len 3  <- best
r=3 'b'  have{e:2,c:1,b:1}   distinct=3  > k
         drop 'e' -> e:1 (still >0)      distinct=3
         drop 'c' -> c:0                 distinct=2  left=2
                                         [eb]      len 2
r=4 'a'  have{e:1,b:1,a:1}   distinct=3  > k
         drop 'e' -> e:0                 distinct=2  left=3
                                         [ba]      len 2
answer 3
```

### Pitfalls

- `matched` counts **distinct keys** whose requirement is met, not total characters — the two are easy to confuse in Minimum Window Substring.
- Anagram problems are a **fixed** window (size `|p|`) plus a histogram — recognise the combination of Sections 3 and 5. Record `left`, not `right`, as the match position.
- Comparing the whole 26-entry histogram at each step is `O(26 n)` — correct but not optimal; the `matches` counter makes it `O(n)`.
- Longest Repeating Character Replacement: do **not** recompute `maxFreq` downward when shrinking. A stale, too-large `maxFreq` can only keep the window size the same, never produce a too-large answer, because the target is a maximum length.
- Maximum Erasure Value keeps two synchronised states — a set for validity and a sum for the answer. Update both on every add and every remove.

**Python equivalent:** `collections.Counter` for the histogram; in C it is `int have[256]` or your own hash map.

---

## 6. Partitioning in place

### The idea

In-place partitioning uses two (or three) pointers to split an array into several regions in one pass with no extra memory — the same technique as the `partition` step of quicksort. The basic version (`write` / `read`, two regions) reads the array with one pointer and writes the result with a second, slower pointer: when the read element belongs to the "accepted" region, it is written at the write pointer's position and the write pointer advances.

### Invariant (two regions)

The region left of the write pointer, `[0, write)`, always contains exactly the accepted elements so far, in their original relative order. The read pointer can be far ahead without breaking the invariant, because rejected elements are simply skipped. Since `read >= write` always, a write never destroys unread data.

### Three regions: the Dutch national flag

Sort Colors keeps three pointers: `low` (next slot for the smallest class), `mid` (element under inspection), `high` (next slot for the largest class). Regions: `[0, low)` = class 0, `[low, mid)` = class 1, `[mid, high]` = unknown, `(high, n-1]` = class 2.

- `a[mid] == 1`: `mid++` only.
- `a[mid] == 0`: swap `a[low]` and `a[mid]`, then `low++` **and** `mid++`. Safe because whatever came from `low` is already known to be a 1 (every position in `[low, mid)` is a 1 by the invariant).
- `a[mid] == 2`: swap `a[mid]` and `a[high]`, then `high--` **only**. The element that arrived from `high` is unknown and must be re-inspected next iteration.

That asymmetry is the whole difficulty of the problem and the commonest bug.

### Stable versus unstable

| Need to keep relative order inside each group? | Technique | Example |
|---|---|---|
| No | opposite pointers swapping across the boundary | Sort Array By Parity, Sort Colors |
| Yes | write/read pointer (stable), or write into a fresh array from both ends | Move Zeroes, Partition Array According to Given Pivot |

### Quickselect

Partition around a pivot, look where the pivot landed at index `p`. If `p` is the target rank, done; if the target is smaller, recurse **only** into the left part; else only into the right. Expected work `n + n/2 + n/4 + ... = O(n)` with random pivots; worst case `O(n^2)` with a bad pivot on already-sorted input, which random pivot selection makes unlikely. `O(1)` extra space. "`k`-th largest" is index `n - k` in ascending order — check the direction.

### Recognising it

Signal words: *in place without extra memory*, *move all X to the front / back*, *three classes*, *k-th largest without full sorting*, *remove duplicates keeping at most m copies*.

### Complexity

`O(n)` time, `O(1)` extra space for all in-place variants; `O(n)` extra if the problem demands stability across three groups and you use a fresh output array.

### C layout

`int *a`, `int` indices, and a `swap` helper. For quickselect, `rand()` from `<stdlib.h>` (seeded once with `srand`) for the pivot index, or the median-of-three rule. For the "compare with `a[write - m]`" trick (Remove Duplicates II) the input must be sorted.

```c
static void swap_int(int *x, int *y) { int t = *x; *x = *y; *y = t; }

/* Dutch national flag: a[i] in {0,1,2}. Sorts in one pass. */
static void sort_colors(int *a, int n) {
    int low = 0, mid = 0, high = n - 1;
    while (mid <= high) {
        if (a[mid] == 0)      { swap_int(&a[low], &a[mid]); low++; mid++; }
        else if (a[mid] == 2) { swap_int(&a[mid], &a[high]); high--; }   /* mid stays */
        else                  { mid++; }
    }
}
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

Move Zeroes on `[0, 1, 0, 3, 12]` with write/read:

```
read=0 a=0   skip                 write=0   [0 1 0 3 12]
read=1 a=1   a[0]=1, write=1                [1 1 0 3 12]
read=2 a=0   skip
read=3 a=3   a[1]=3, write=2                [1 3 0 3 12]
read=4 a=12  a[2]=12, write=3               [1 3 12 3 12]
fill [3,5) with 0:                          [1 3 12 0 0]
```

### Pitfalls

- After a `high` swap, do **not** advance `mid`.
- Sort Array By Parity II: the `odd` pointer continues from where it stopped — resetting it each round makes the algorithm `O(n^2)`.
- Move Zeroes: if you do a single pass without the final zero-fill, you must *swap* rather than copy.
- Partition Array According to Given Pivot: the pivot value itself belongs to the middle group, and order must be preserved, so a Sort Colors swap scheme is wrong there.
- Wiggle Sort II: after sorting and splitting at the median, place each half in **reverse** order (small half to odd indices, large half to even); forward order puts duplicates adjacent.

**Python equivalent:** `np.partition(a, k)` is quickselect; `sorted(a, key=...)` is the stable partition when you don't care about memory.

---

## Pattern recognition cheatsheet

| Signal in the problem statement | Pattern | Section | Time / space |
|---|---|---|---|
| sorted array, pair with sum / difference | opposite ends | 1 | `O(n)` / `O(1)` |
| palindrome, reverse, symmetric | opposite ends | 1 | `O(n)` / `O(1)` |
| two walls / heights, water, area | opposite ends, move the limiting side | 1 | `O(n)` / `O(1)` |
| triplets summing to X | sort + fix one + opposite ends | 1 | `O(n^2)` / `O(1)` |
| linked list: cycle, middle, palindrome, reorder | fast / slow | 2 | `O(n)` / `O(1)` |
| repeated `x = f(x)`, values are valid indices | fast / slow on a function graph | 2 | `O(n)` / `O(1)` |
| every subarray of size exactly `k`, moving average | fixed window | 3 | `O(n)` / `O(1)` |
| max / min of each window of size `k` | fixed window + monotonic deque | 3 | `O(n)` / `O(k)` |
| take `k` from the ends | fixed window of size `n - k` in the middle | 3 | `O(n)` / `O(1)` |
| longest subarray with sum / product / count ≤ bound (positive values) | variable window, shrink until just valid | 4 | `O(n)` / `O(1)` |
| shortest subarray with sum ≥ target (positive values) | variable window, shrink as far as possible | 4 | `O(n)` / `O(1)` |
| count subarrays with exactly `k` of something | `atMost(k) - atMost(k-1)` | 4 | `O(n)` / `O(1)` |
| anagram / permutation of `p` inside `s` | fixed window + histogram + `matches` | 3+5 | `O(n)` / `O(sigma)` |
| shortest window covering all chars of `t` | variable window + histogram + `matched` | 5 | `O(n+m)` / `O(sigma)` |
| at most `k` distinct values, two baskets | variable window + count of live keys | 5 | `O(n)` / `O(k)` |
| move all X to the front, keep order | write / read pointer | 6 | `O(n)` / `O(1)` |
| three classes in place | Dutch national flag | 6 | `O(n)` / `O(1)` |
| `k`-th largest / smallest without sorting | quickselect | 6 | `O(n)` expected / `O(1)` |
| subarray sum == target with **negative** numbers | *not* this chapter — prefix sums + hash map | 01 | `O(n)` / `O(n)` |

---

## Gotchas in C specifically

- **Unsigned underflow.** `size_t right = n - 1; while (right >= 0)` is an infinite loop. Use `int` (or `ptrdiff_t`) for indices that are decremented, or structure the loop so the decrement can never happen at zero.
- **Sum / product overflow.** `int` holds about `2.1e9`. Window sums over `10^5` elements of `10^4` overflow; products overflow after a handful of multiplications. Use `long long` and, for products, shrink before the multiplication can overflow (the window invariant guarantees the product stays `< k` after shrinking, so `product * a[right]` is bounded by `k * max_value`).
- **`char` may be signed.** `int cnt[256]; cnt[s[i]]++` indexes negative memory for bytes ≥ 128. Always `cnt[(unsigned char)s[i]]`.
- **Strings end at `'\0'`.** `strlen` is `O(n)` — call it once before the loop, not in the loop condition (`../../c_learning/04_arrays_and_strings/lesson.md`).
- **No built-in hash set.** For distinct-value windows over small integers use a counting array; for arbitrary ints write or reuse the open-addressing map from Chapter 01 (`../../c_learning/10_data_structures/lesson.md`), and give it a `decrement`/`remove` operation so keys can leave the window.
- **No deque.** A monotonic deque over indices never needs to wrap: allocate `int dq[n]` (or `malloc` for large `n`) and use `head`/`tail` indices.
- **`qsort` comparator.** `return *(const int*)a - *(const int*)b;` overflows for large-magnitude inputs; write `(x > y) - (x < y)` instead (`../../c_learning/11_function_pointers_and_generics/lesson.md`).
- **Half-open ranges.** Keep windows as `[left, right]` inclusive with length `right - left + 1`, or as `[left, right)` with length `right - left` — pick one and never mix them in the same function.
- **NULL checks on lists.** Every `->next->next` must be preceded by a check of both hops.
- **Recursion depth in quickselect.** Recursion is one-sided so depth is `O(log n)` expected, but `O(n)` worst case — write it as a `while` loop instead; there is nothing to do after the recursive call.
- **`rand()` seeding.** Seed once with `srand` in `main`, never inside the partition routine.

---

## Common mistakes checklist

- [ ] Loop condition `left < right` (not `<=`) for opposite pointers that swap; `left <= right` only when a single middle element must be inspected.
- [ ] Inner skip loops (non-alphanumerics, non-vowels) re-check `left < right`.
- [ ] Moved the pointer that *limits* the answer (water problems), not the one with the smaller raw height.
- [ ] Guarded `fast && fast->next` before the double hop.
- [ ] Ran phase two — did not return the phase-one meeting point as the cycle start.
- [ ] First window computed directly; the shift loop starts at `right = k`, not `0`.
- [ ] Deque stores indices; expired front removed before reading the max.
- [ ] Longest-window: shrink until *just* valid. Shortest-window: shrink *as far as possible*. Answer updated at the right moment for each.
- [ ] Checked the predicate is monotone before using a variable window (all-positive values, "at most" not "exactly").
- [ ] `atMost(-1)` returns 0.
- [ ] Key removed from the distinct-count when its count hits zero.
- [ ] `matched` counts satisfied *keys*, not characters.
- [ ] `mid` not advanced after a swap with `high`.
- [ ] Stable partition used where order must be preserved.
- [ ] `long long` for sums and products; `(unsigned char)` for histogram indices; `int` for decremented indices.

---

## You can move on when...

- You can write the opposite-pointers pair-sum loop from memory and explain in two sentences why moving `right` when the sum is too big cannot skip a valid pair.
- You can draw the `a = c` argument for Floyd's phase two on a whiteboard and implement `cycle_start` without looking.
- You can state the fixed-window invariant, write the sum version in under a minute, and explain why the maximum version needs a monotonic deque.
- Given a new subarray problem you can say within a minute whether the predicate is monotone, whether it is a longest- or shortest-window problem, and whether the answer is a length or a count (`atMost` trick).
- You can implement Minimum Window Substring with `need[256] / have[256] / matched` and trace it on a 6-character input.
- You can write Sort Colors and explain the `high`-swap asymmetry, and turn the same partition into a working quickselect.
- `example.c` compiles with `-Wall -Wextra` clean and you have modified at least one demo input and predicted the output before running it.

Next: `../03_*/lesson.md` (binary search).
