# Chapter 04 — Stack & monotonic stack — Python

## What you'll be able to do after this chapter

- Recognise when a problem is LIFO in disguise (bracket matching, undo, collisions, nested scopes, call stacks) and state the stack invariant in one sentence before writing code.
- Collapse a stack to a single integer counter when only its *depth* matters, and know exactly when that collapse is illegal.
- Evaluate postfix and infix expressions with one or two stacks, and reuse the same "push context on `(`, merge on `)`" skeleton for strings, numbers, maps and booleans.
- Write a monotonic stack from memory: next-greater/next-smaller element, span problems, greedy "remove digits" constructions — and argue the amortised O(n) bound.
- Derive the "contribution" formula `value * left_span * right_span` and the histogram area formula `height * (right - left - 1)` from the same nearest-smaller-neighbour idea.
- Build stack-backed data structures (min-stack, queue from two stacks, lazy-increment stack) where each node carries extra state so queries answer in O(1).

## Why this matters for ML / numerics / sims

A stack is the data structure of *nesting* and *most-recent-first*. Every parser you will write — a config format, an expression DSL in an autograd toy, a tensor-shape grammar, an OBJ file with nested groups — is a stack machine, and this chapter's expression-evaluation unit is literally that machine. Reverse-mode autodiff records operations forward and replays them backward: a stack of tape entries. Recursive tree walks (octree, k-d tree, BVH traversal) hit Python's ~1000-frame recursion limit on deep trees; converting them to an explicit stack of pending nodes is exactly the technique in Unit 6.

The monotonic stack is the more valuable half. "For each element, the nearest smaller/larger one to the left and right" is the primitive behind: the largest empty rectangle in an occupancy grid, all-nearest-smaller-values in Cartesian-tree construction, skyline and visibility queries in 1-D terrain, running max in a stream without a heap, and stock-span-style features in time-series preprocessing. All O(n) with a monotonic stack and O(n²) without.

**Python vs C, once for the chapter:** a `list` *is* the stack — `append`, `pop`, `st[-1]`, all O(1). Ints never overflow, so no `long long` and no cast-before-multiply. Popping an empty list raises `IndexError` instead of silently corrupting memory. Speed is ~50-100× slower than C, but every pattern here is O(n) amortised, which LeetCode limits allow.

---

## 1. Bracket matching & the simple stack

### The idea

A **stack** fits whenever the solution must handle the *most recent still-open thing first*. Every closing bracket belongs to the *nearest* not-yet-closed opener, so a stack stores open brackets in exactly the order you need them.

### The invariant

At position `i` the stack is the **path of currently open brackets** from the start to `i`:

- Opening bracket: push.
- Closing bracket: (1) stack non-empty, (2) top is the matching opener type.
- Valid **iff** neither check ever fails **and** the stack is empty at the end. `"("` passes the scan but leaves an opener; `")("` ends empty but fails check 1.

### Recognising it

Signal words: *brackets*, *parentheses*, *valid*, *balanced*, *matching*, minimum adds/removes to balance.

### Complexity

O(n) time, O(n) space worst case. With one bracket type and only the *count* mattering, the stack collapses to an `int`: O(1) space.

### Python layout

```python
PAIRS = {")": "(", "]": "[", "}": "{"}

def bracket_valid(s: str) -> bool:
    st: list[str] = []
    for c in s:
        if c in "([{":
            st.append(c)
        elif not st or st[-1] != PAIRS[c]:      # checks 1 and 2
            return False
        else:
            st.pop()
    return not st                                # check 3: nothing left open
```

`not st` is the idiomatic emptiness test; `st[-1]` peeks. Never `st[len(st) - 1]`.

### Worked example: `"{[()]}("`

```
i  c   action                 stack (bottom -> top)
0  {   push                   {
1  [   push                   { [
2  (   push                   { [ (
3  )   top '(' matches, pop   { [
4  ]   top '[' matches, pop   {
5  }   top '{' matches, pop   (empty)
6  (   push                   (
end: stack non-empty -> INVALID
```

### Depth counter instead of a stack

Single bracket type: the stack is `((((` — only its height matters. `depth += 1` on `(`, `depth -= 1` on `)`, check 1 becomes `depth == 0` before decrementing.

- **Counting fixes**: a closer at `depth == 0` is one unmatched closer — count it; leftover `depth` at the end is the number of unmatched openers.
- **Two-sided feasibility** (locked positions): sweep left-to-right treating free chars as openers, then right-to-left treating them as closers; both must stay non-negative.

### Pitfalls

- Forgetting the end-of-string emptiness check.
- `st.pop()` on an empty list raises `IndexError` — guard with `if not st`.
- If the output is "which indices to delete", push **indices**, not characters.
- In "longest valid substring" the bottom of the stack is a **boundary marker**: seed with `-1`, and after a pop that empties the stack, push the current index as the new boundary.

---

## 2. Stack as an undo / state machine

### The idea

A new event can **cancel, replace, or collide with** previously recorded state, and the effect reaches back only to the *nearest* earlier event. Path normalisation: `".."` cancels the nearest still-existing directory.

### The invariant

Scan once, keeping a stack of "state still in force". Each new element:

- **(a) push** — new independent state;
- **(b) cancel** the top — possibly repeatedly, if removing it exposes a new collision;
- **(c) get skipped** — the top wins.

The cancel rule is an arbitrary predicate, not a fixed type correspondence.

### Recognising it

Signal words: *undo*, *backspace*, *remove adjacent duplicates*, *simulate*, *go back*, *collision*, *destroyed*, *cancel*, call stack / directory navigation.

### Complexity

O(n) amortised: each element pushed at most once, popped at most once. Space O(n).

### Python layout

```python
def cancel_adjacent(s: str) -> str:
    """Removes adjacent equal pairs repeatedly: "abbaccd" -> "d"."""
    st: list[str] = []
    for c in s:
        if st and st[-1] == c:
            st.pop()                             # (b) cancel; the loop continues with the next c
        else:
            st.append(c)                         # (a) push
    return "".join(st)                           # bottom first = left-to-right result

def asteroid_collision(a: list[int]) -> list[int]:
    st: list[int] = []
    for x in a:
        alive = True
        while alive and st and st[-1] > 0 > x:  # collides: top moves right, x moves left
            if st[-1] < -x:
                st.pop()                         # (b) top loses, keep going
            elif st[-1] == -x:
                st.pop(); alive = False          # both destroyed
            else:
                alive = False                    # (c) top wins
        if alive:
            st.append(x)
    return st
```

**Python vs C:** the result is `"".join(st)` — never `result += c` in a loop (quadratic in the worst case, though CPython often optimises it). `os.path.normpath` is the path problem in the stdlib.

### Worked example: adjacent-duplicate cancellation on `a b b a c c d`

```
x   stack before   rule                       stack after
a   (empty)        push                       a
b   a              push                       a b
b   a b            top==x -> cancel           a
a   a              top==x -> cancel           (empty)
c   (empty)        push                       c
c   c              top==x -> cancel           (empty)
d   (empty)        push                       d
result: "d"
```

Removing `bb` exposes `a a`, which a single non-stack pass would miss.

### Depth counter, again

"How deep are we" → an `int` clamped at 0. "What is the final path" → the actual names; the counter is illegal. Ask *what does the output need?*

### Pitfalls

- Stopping after one cancellation. The `while` is not optional.
- Forgetting "both destroyed" in symmetric collisions.
- Exclusive-time stacks: an `end` at `t` means the function ran *through* `t`; delta is `t - prev + 1`, next resumes at `t + 1`.
- `path.split("/")` yields empty strings for `//` and leading `/` — filter them: `[p for p in path.split("/") if p and p != "."]`.

---

## 3. Expression evaluation

### The idea

Two *separate* reasons for a stack:

1. **Order of operators and intermediate results.** RPN: push operands; each operator consumes the top two and pushes the result.
2. **Nesting and precedence** in infix: a stack of saved contexts unwound at `)`.

### The skeleton for infix

Running `result` and current `sign`. On `(`, push `(result, sign)` and reset. On `)`, pop and merge: `result = outer_result + outer_sign * result`. `*`/`/` without brackets: remember only the *previous number* and replace it.

| Problem | What `(` pushes | What `)` does |
|---|---|---|
| Basic calculator | `(result, sign)` | `result = outer + sign * inner` |
| Decode string | `(repeat_count, prefix)` | `cur = prefix + cur * k` |
| Score of parentheses | `0` (new level) | `outer += max(2 * inner, 1)` |
| Number of atoms | new empty `Counter` | multiply inner by `k`, merge into outer |
| Boolean expression | operator token | pop operands until operator, apply, push |

### Recognising it

Signal words: *evaluate*, *calculator*, *decode*, *parse*, *postfix/RPN*, *nested*.

### Complexity

O(n) time and space; decode-string is O(output size).

### Python layout

```python
def eval_rpn(tokens: list[str]) -> int:
    st: list[int] = []
    for t in tokens:
        if t in ("+", "-", "*", "/"):            # "-3" is not in this tuple -> operand
            b, a = st.pop(), st.pop()            # a is the LEFT operand
            if t == "+":   st.append(a + b)
            elif t == "-": st.append(a - b)
            elif t == "*": st.append(a * b)
            else:          st.append(int(a / b)) # truncate toward zero, NOT a // b
        else:
            st.append(int(t))
    assert len(st) == 1, "malformed"
    return st[0]

def eval_infix_pm(s: str) -> int:
    """Digits, + - ( ) and spaces."""
    result, sign, num = 0, 1, 0
    st: list[tuple[int, int]] = []
    for c in s + "+":                            # trailing '+' commits the last number
        if c.isdigit():
            num = num * 10 + int(c)
        elif c in "+-":
            result += sign * num; num = 0
            sign = 1 if c == "+" else -1
        elif c == "(":
            st.append((result, sign)); result, sign = 0, 1
        elif c == ")":
            result += sign * num; num = 0
            outer, outer_sign = st.pop()
            result = outer + outer_sign * result
    return result
```

**Python vs C:** `a // b` floors toward `-inf` (`-7 // 2 == -4`); LeetCode's RPN specifies truncation toward zero, so use `int(a / b)` (safe for values below 2^53) or `abs(a) // abs(b) * (1 if (a < 0) == (b < 0) else -1)` for arbitrary ints. Appending a sentinel `"+"` to the input replaces the "commit at end of string" special case.

### Worked example: RPN `2 1 + 3 *`

```
token  stack (bottom -> top)
2      2
1      2 1
+      3           (pop 1, pop 2, push 2+1)
3      3 3
*      9
result 9
```

Infix `1 - (2 + 3)`:

```
char  result  sign  stack           note
1     1       +1    []
-     1       -1    []
(     0       +1    [(1, -1)]       push outer context, reset
2     2       +1    [(1, -1)]
+     2       +1    [(1, -1)]
3     5       +1    [(1, -1)]
)     -4            []              pop (1,-1): result = 1 + (-1)*5
```

### Pitfalls

- Operand order on `-` and `/`.
- `//` vs truncation toward zero.
- Multi-digit numbers: accumulate, commit on operator / bracket / end. The sentinel trick handles the end.
- Precedence without brackets: push `+`/`-` terms, apply `*`/`/` to `st[-1]` immediately, `sum(st)` at the end.
- Whitespace: skip it, but do not let it split a number.
- Decode-string: `prefix + cur * k` — Python's string repetition does the work.

---

## 4. Monotonic stack — next greater element

### The idea

A **monotonic stack** stays sorted while scanning once, by popping elements that **can no longer be the answer for any future query**. For next-greater the stack is **decreasing**: a new element larger than the top *is* the top's answer — pop and record, repeat, then push the new element.

### The invariant

After index `i`, the stack holds exactly those `j <= i` whose next-greater has **not yet been seen**, values strictly decreasing bottom to top.

### Complexity

Amortised O(n): each index pushed once, popped at most once. Space O(n).

### Two directions, four variants

| Want | Stack keeps | Pop while |
|---|---|---|
| next **greater** (strict) | decreasing | `a[st[-1]] < a[i]` |
| next **greater or equal** | strictly decreasing | `a[st[-1]] <= a[i]` |
| next **smaller** (strict) | increasing | `a[st[-1]] > a[i]` |
| next **smaller or equal** | strictly increasing | `a[st[-1]] >= a[i]` |

Scan right-to-left for "previous". Scan `2n` with `i % n` for circular.

### Greedy construction

Same skeleton builds the smallest/largest result by deletion (remove k digits, smallest subsequence): pop the top whenever the new element would be better in its place. Constraints bolt onto the pop condition: a budget `k`, "enough elements left to reach length `k`", "this letter appears again later".

### Recognising it

Signal words: *next greater/smaller*, *days until warmer*, *span*, *previous less*, *remove k digits*, *lexicographically smallest subsequence*, *most competitive*.

### Python layout

Push **indices**; you need both the value and the position.

```python
def next_greater(a: list[int]) -> list[int]:
    """ans[i] = index of the next strictly greater element, or -1."""
    ans = [-1] * len(a)                          # leftovers default to -1: no drain loop
    st: list[int] = []
    for i, x in enumerate(a):
        while st and a[st[-1]] < x:              # top just found its answer
            ans[st.pop()] = i
        st.append(i)
    return ans

def remove_k_digits(num: str, k: int) -> str:
    st: list[str] = []
    for d in num:
        while k and st and st[-1] > d:
            st.pop(); k -= 1
        st.append(d)
    st = st[:len(st) - k] if k else st           # leftover budget: delete from the end
    return "".join(st).lstrip("0") or "0"
```

**Python vs C:** pre-filling `ans` with `-1` replaces the leftover drain loop. `"".join(st).lstrip("0") or "0"` handles both the leading zeros and the empty result in one expression.

### Worked example: `a = [2, 1, 2, 4, 3]`

```
i  a[i]  pops (index:value -> answer)   stack (indices)   values on stack
0  2     -                              [0]               [2]
1  1     -                              [0 1]             [2 1]
2  2     1:1 -> 2                       [0 2]             [2 2]   (2 not > 2, no pop)
3  4     2:2 -> 3,  0:2 -> 3            [3]               [4]
4  3     -                              [3 4]             [4 3]
end: 3 -> -1, 4 -> -1
ans = [3, 2, 3, -1, -1]
```

5 pushes, 3 pops — the amortised argument in numbers.

### Pitfalls

- `<` vs `<=` decides duplicate behaviour. "Warmer" is strict; "discount if price <= current" is non-strict.
- Push indices, not values, when distances matter.
- Leftover elements: pre-fill the answer or drain explicitly.
- Greedy removal: leftover budget deletes from the **end**; strip leading zeros; empty means `"0"`.
- Online stock span: store `(value, span)` pairs; a popped element's span is inherited.

---

## 5. Monotonic stack — histogram and rectangle problems

### The idea

For each element's **contribution** to a sum, or the largest rectangle among bars, one pass finds every element's **nearest smaller neighbour on both sides**. When bar `i` is popped from an increasing stack, its region of influence reaches left to the new top and right to the incoming element.

### The core formula

Increasing stack of indices. Pop `h` when `a[i] < a[st[-1]]`:

```
width = i - st[-1] - 1        if the stack is non-empty after the pop
width = i                     if the stack is now empty
```

| Problem | Contribution of popped `h` |
|---|---|
| Largest rectangle in histogram | `a[h] * width` — max |
| Sum of subarray minimums | `a[h] * (h - L) * (R - h)` — sum |
| Sum of subarray ranges | (sum of maxes) - (sum of mins), two passes |
| Trapping rain water (decreasing stack) | `(min(a[L], a[i]) - a[h]) * (i - L - 1)` — sum |

### The sentinel trick

Append a virtual bar of height `0` at index `n`: it pops everything with the same formula. In Python, `for i, cur in enumerate(h + [0])` or `itertools.chain(h, [0])`.

### Recognising it

Signal words: *largest rectangle*, *histogram*, *maximal rectangle of 1s*, *sum over all subarrays of min/max*, *trapped water*, *contribution*.

### Complexity

O(n) time and space. Maximal rectangle in a binary matrix: O(rows * cols) — per-row histogram, 1-D routine per row.

### Python layout

```python
def largest_rect(h: list[int]) -> int:
    st: list[int] = []                           # increasing stack of indices
    best = 0
    for i, cur in enumerate(h + [0]):            # virtual 0-height bar at i == n
        while st and h[st[-1]] > cur:            # strict >: equal heights stay
            k = st.pop()
            width = i if not st else i - st[-1] - 1
            best = max(best, h[k] * width)
        st.append(i)
    return best

def sum_subarray_mins(a: list[int], mod: int = 10**9 + 7) -> int:
    """Strict on the left, non-strict on the right: each subarray attributed once."""
    n = len(a)
    left = [0] * n                               # left[i] = i - (nearest strictly smaller on the left)
    right = [0] * n                              # right[i] = (nearest smaller-or-equal on the right) - i
    st: list[int] = []
    for i, x in enumerate(a):
        while st and a[st[-1]] >= x:             # non-strict pop => right neighbour is <=
            k = st.pop()
            right[k] = i - k
        left[i] = i - (st[-1] if st else -1)
        st.append(i)
    while st:
        k = st.pop()
        right[k] = n - k
    return sum(x * l * r for x, l, r in zip(a, left, right)) % mod
```

**Python vs C:** no overflow, so one `% mod` at the end is correct; reducing as you go only matters for speed on huge inputs (big-int arithmetic slows past 64 bits).

### Worked example: `h = [2, 1, 5, 6, 2, 3]`

```
i  h[i]  pops: k (height) width area          stack after (indices)  heights on stack
0  2     -                                    [0]                    [2]
1  1     k=0 (2)  width=1  area=2             [1]                    [1]
2  5     -                                    [1 2]                  [1 5]
3  6     -                                    [1 2 3]                [1 5 6]
4  2     k=3 (6)  width=4-2-1=1  area=6       [1 2]
         k=2 (5)  width=4-1-1=2  area=10      [1 4]                  [1 2]
5  3     -                                    [1 4 5]                [1 2 3]
6  0*    k=5 (3)  width=6-4-1=1  area=3
         k=4 (2)  width=6-1-1=4  area=8
         k=1 (1)  width=6 (empty) area=6      []
best = 10   (bars 5 and 6, width 2)
```

```
        6
      5 #
      # #
      # #     3
  2   # # 2   #
  # 1 # # # # #
```

### Duplicates and double counting

For *sums*, equal values must be counted once: **strict** on one side, **non-strict** on the other. For *max* problems it does not matter which equal bar gets credit, but strictness decides which pop sees the full width — with `>` in the pop condition, the leftmost equal bar computes the full-width rectangle when the sentinel arrives.

### Pitfalls

- `width = i - L - 1`: bars at `L` and `i` are the *walls*.
- Forgetting the sentinel and returning `best` with bars still on the stack.
- Rectangles/minimums: *increasing* stack (pop on smaller incoming). Trapping water: *decreasing* stack (pop on taller incoming).
- `h + [0]` copies the list — fine once; never inside a loop.

---

## 6. Min-stack and stack-backed design problems

### The idea

**Each stack node carries extra state** so a query answers in O(1). Min-stack: store `(value, min_so_far)` per node; popping reverts to the previous snapshot automatically.

### Two stacks make a queue

`inbox` and `outbox`. When `outbox` is empty, pour all of `inbox` into it — the order flips exactly right; each element moves at most once, so amortised O(1).

### Recognising it

*Design*, *implement*, *support the following operations*, *in O(1)*.

### Complexity

O(1) per operation, sometimes amortised. Space O(n) plus per-node state.

### Python layout

The C used structs, so these are classes.

```python
class MinStack:
    def __init__(self) -> None:
        self._st: list[tuple[int, int]] = []     # (value, min_so_far)

    def push(self, v: int) -> None:
        m = min(v, self._st[-1][1]) if self._st else v
        self._st.append((v, m))                  # snapshot the minimum

    def pop(self) -> None:
        self._st.pop()

    def top(self) -> int:
        return self._st[-1][0]

    def get_min(self) -> int:
        return self._st[-1][1]

class TwoStackQueue:
    def __init__(self) -> None:
        self._in: list[int] = []
        self._out: list[int] = []

    def push(self, x: int) -> None:
        self._in.append(x)

    def _pour(self) -> None:
        if not self._out:                        # pour ONLY when out is empty
            while self._in:
                self._out.append(self._in.pop())

    def pop(self) -> int:
        self._pour()
        return self._out.pop()

    def peek(self) -> int:
        self._pour()
        return self._out[-1]

    def empty(self) -> bool:
        return not self._in and not self._out
```

**Python vs C:** no `init`/`free` pairs; `__init__` and garbage collection handle lifetime. Empty-stack access raises `IndexError` — the problem statements guarantee valid calls, so no status codes. `collections.deque` is the real queue; the two-stack version is for the pattern.

### Worked example: `push 5, push 3, push 7, push 2, pop, getMin, pop, getMin`

```
op        stack of (value, min)              getMin
push 5    (5,5)
push 3    (5,5) (3,3)
push 7    (5,5) (3,3) (7,3)
push 2    (5,5) (3,3) (7,3) (2,2)
pop       (5,5) (3,3) (7,3)
getMin                                       3     <- reverted automatically
pop       (5,5) (3,3)
getMin                                       3
```

### Lazy propagation on a stack

"Increment the bottom `k` elements by `val`": keep a parallel `inc` list and add `val` at index `min(k, len) - 1` only. On pop from index `i`, return `data[i] + inc[i]`, push the pending increment down (`inc[i-1] += inc[i]`), clear `inc[i]`. O(1) each — the simplest lazy segment tree.

### Explicit stack instead of recursion

A nested-list iterator over `[1, [4, [6]]]`: push the top-level elements reversed; `has_next` pops lists and pushes their children (reversed) until the top is a scalar. Each element unpacked once. This is how you convert a recursive tree traversal to iterative when Python's recursion limit is a concern (Chapter 05).

### Pitfalls

- Storing the minimum in a single attribute: pop cannot recover the previous minimum. Per node.
- Pouring while `out` is non-empty destroys FIFO order.
- Browser-history `visit` discards the *whole* forward stack: `self._fwd.clear()`.
- Dinner plates: a `heapq` of "stacks with free room".

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Stack holds | Complexity |
|---|---|---|---|
| valid / balanced brackets, matching pairs | simple stack, 3 checks | opener chars | O(n) / O(n) |
| one bracket type; "minimum adds"; depth | depth counter | nothing (an `int`) | O(n) / O(1) |
| longest valid substring | stack of indices + boundary `-1` at bottom | indices | O(n) / O(n) |
| undo, backspace, adjacent duplicates, collision, simulate | state-machine stack with `while` cancel loop | surviving items | O(n) amortised |
| final path / directory depth | stack of names / depth counter | names or `int` | O(n) |
| call stack, exclusive time | stack of ids + `prev_time` | function ids | O(n) |
| RPN / postfix | operand stack | numbers | O(n) |
| infix with `( )`, decode `k[...]`, nested formula | push `(context)` on `(`, merge on `)` | tuples / `Counter`s | O(n) |
| next greater / smaller, warmer day, discount | monotonic stack (dec. for greater, inc. for smaller) | indices | O(n) amortised |
| circular array next greater | monotonic stack, scan `2n` with `i % n` | indices | O(n) |
| stock span, online stream | monotonic stack of `(value, span)` | tuples | amortised O(1)/call |
| remove k digits, smallest subsequence, competitive | increasing stack + budget condition | chars / values | O(n) |
| remove duplicate letters | monotonic stack + in-stack `set` + last-occurrence dict | chars | O(n) |
| 132 pattern | right-to-left decreasing stack + best popped value | values | O(n) |
| largest rectangle, histogram | increasing stack + sentinel, `h * (i - L - 1)` | indices | O(n) |
| maximal rectangle in 0/1 matrix | per-row histogram → largest rectangle | indices per row | O(rows * cols) |
| sum of subarray minimums / maximums / ranges | contribution `a[i] * left * right` | indices | O(n) |
| trapping rain water | decreasing stack, `(min(walls) - bottom) * width` | indices | O(n) |
| design: getMin in O(1) | node carries `min_so_far` | `(value, min)` | O(1) each |
| design: queue from stacks | two lists, pour when `out` empty | values | amortised O(1) |
| design: increment bottom k | lazy `inc` list | values + pending inc | O(1) each |
| design: flatten nested iterator | explicit stack replacing recursion | list cursors | amortised O(1) |
| design: max-frequency pop | `dict` freq → stack of values, `max_freq` | per-frequency stacks | O(1) each |

---

## Gotchas in Python specifically

- **`list` is the stack.** `append` / `pop` / `st[-1]` are O(1). `list.pop(0)` and `list.insert(0, x)` are O(n) — never use a list as a queue; that is `collections.deque`.
- **`IndexError` on empty pop/peek.** Better than C's undefined behaviour, still a bug. Guard with `if st:` / `while st and ...`.
- **`//` floors, `/` gives a float.** RPN division that must truncate toward zero is `int(a / b)` (or the sign-aware `abs` formula for ints beyond 2^53). `-7 // 2 == -4`, `int(-7 / 2) == -3`.
- **No overflow.** `h[k] * width`, `x * left * right`, RPN intermediates are all exact. Reduce `% mod` once at the end unless the numbers get huge enough to slow big-int arithmetic.
- **String building.** `"".join(list_of_chars)` at the end, never `s += c` in a loop for large outputs. Strings are immutable; the stack must be a `list[str]`.
- **`in` on a list is O(n).** "Is this letter already on the stack" (remove duplicate letters) needs a parallel `set`, not `c in st`.
- **Recursion limit ~1000.** A recursive-descent parser on `10^4` nested brackets raises `RecursionError`. Use the explicit stack — this whole chapter — rather than `sys.setrecursionlimit`.
- **Shallow copies.** `h + [0]` and `st[:]` are new lists; `st2 = st` aliases. `[[]] * n` for per-frequency stacks shares one list — use a `defaultdict(list)`.
- **Mutable default arguments.** `def solve(tokens, st=[])` keeps state across calls. Use `None`.
- **`is` vs `==`.** Compare token strings and ints with `==`. `t is "+"` may work by interning accident and then break.
- **Late-binding closures.** `key=lambda: st[-1]` captures the list, not the value — fine here, but a `lambda` built in a loop over `i` sees the final `i`.
- **Float keys.** Not relevant to stacks, but do not put computed floats in the `Counter` for number-of-atoms.
- **`heapq` is min-heap only** — negate for the dinner-plates max index. **`bisect` needs sorted input.**
- **Speed.** `while st and a[st[-1]] < x` is the hot loop; keep it tight — bind `a[st[-1]]` once if you use it twice, avoid function calls inside. Roughly 50-100× slower than C; O(n) with `n = 10^5` is ~0.1 s.

---

## Common mistakes checklist

- [ ] Stack **empty at the end** (bracket problems)?
- [ ] `if st` / `while st and ...` before every peek/pop?
- [ ] Pushing **indices** when you need distances, widths, or positions?
- [ ] Cancel/pop loop is a `while`, not an `if`?
- [ ] Strict `<` or non-strict `<=` — how does the problem treat equal values?
- [ ] Leftover stack elements handled (pre-filled `-1`, sentinel, or drain loop)?
- [ ] In `(`/`)` merges, is the stack storing the **outer** context, and is local state reset after the push?
- [ ] Last multi-digit number committed at end of string (sentinel `"+"`)?
- [ ] Second popped operand is the **left** one for `-` and `/`? Division truncates toward zero, not `//`?
- [ ] Widths: `i - L - 1`, not `i - L`. Empty-stack case uses `i`?
- [ ] Greedy removal: leftover budget deleted from the end, `.lstrip("0") or "0"`?
- [ ] Min-stack: extra state stored **per node**, not as one attribute?
- [ ] Two-stack queue: pour only when `out` is empty?
- [ ] Output string built with `"".join`, not `+=`?

---

## You can move on when...

- You can write `next_greater` and `largest_rect` from memory, with the sentinel, in under ten minutes each.
- You can explain in one sentence why the monotonic-stack inner loop is O(n) total, and tell whether a problem wants an increasing or decreasing stack without trying both.
- Given a bracket-flavoured problem, you can say immediately whether a depth counter suffices or a real stack (of chars? of indices?) is required.
- You can write the `(result, sign)` infix evaluator and describe how to change it into decode-string and score-of-parentheses without touching the control flow.
- You have solved at least the level-2 and level-3 problems of every unit in `problems.md` in Python, with your own `assert` tests, and at least one level-4 problem from Unit 5.
