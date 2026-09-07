# Chapter 04 — Stack & monotonic stack

## What you'll be able to do after this chapter

- Recognise when a problem is LIFO in disguise (bracket matching, undo, collisions, nested scopes, call stacks) and state the stack invariant in one sentence before writing code.
- Collapse a stack to a single integer counter when only its *depth* matters, and know exactly when that collapse is illegal.
- Evaluate postfix and infix expressions with one or two stacks, and reuse the same "push context on `(`, merge on `)`" skeleton for strings, numbers, maps and booleans.
- Write a monotonic stack from memory: next-greater/next-smaller element, span problems, greedy "remove digits" constructions — and argue the amortised O(n) bound.
- Derive the "contribution" formula `value * left_span * right_span` and the histogram area formula `height * (right - left - 1)` from the same nearest-smaller-neighbour idea.
- Build stack-backed data structures (min-stack, queue from two stacks, lazy-increment stack) where each node carries extra state so queries answer in O(1).

## Why this matters for ML / numerics / sims

A stack is the data structure of *nesting* and *most-recent-first*. Every parser you will ever write — for a config format, an expression DSL in an autograd toy, a tensor-shape grammar, a PDB/OBJ file with nested groups — is a stack machine underneath, and this chapter's expression-evaluation unit is literally that machine. Reverse-mode autodiff records operations forward and replays them backward: a stack of tape entries. Recursive tree walks (Barnes-Hut octree, k-d tree nearest-neighbour, BVH ray traversal) run out of C stack space on deep trees; converting them to an explicit stack of pending nodes is exactly the technique in Unit 6.

The monotonic stack is the less obvious but more valuable half. "For each element, the nearest smaller/larger one to the left and right" is the primitive behind: computing the largest empty rectangle in an occupancy grid (Unit 5, maximal rectangle), the "all-nearest-smaller-values" problem in Cartesian-tree construction and range-minimum queries, skyline and visibility queries in 1-D terrain, pooling windows in a stream where you need a running max without a heap, and stock-span-style "how long has the signal been below the current level" features in time-series preprocessing. All of them are O(n) with a monotonic stack and O(n²) without one.

---

## 1. Bracket matching & the simple stack

### The idea

A **stack** fits whenever the solution must handle the *most recent still-open thing first* — LIFO order (*last in, first out*). Bracket matching is the purest example: every closing bracket belongs to the *nearest* not-yet-closed opening bracket, never an earlier one, so a stack stores the history of open brackets in exactly the order you need them.

### The invariant

At any position `i` while scanning left to right, the stack content is the **path of currently open brackets** from the start of the string to `i`. Concretely:

- On an opening bracket: push it.
- On a closing bracket: two checks must pass — (1) the stack is non-empty (otherwise this closer matches nothing), and (2) the top is the matching opener type.
- The string is valid **if and only if** neither check ever fails **and** the stack is empty at the end. "No errors during the scan" alone is not enough: `"("` produces no error but leaves an unmatched opener. "Empty at the end" alone is not enough: `")("` ends with an empty stack after the first check fails.

### Recognising it

Signal words: *brackets*, *parentheses*, *valid*, *balanced*, *matching*, or a request to add/remove the minimum number of characters so a sequence balances.

### Complexity

O(n) time, O(n) space for the stack in the worst case (`"((((("`). When the brackets are all one type and only the *count* of open ones matters, the stack collapses to an `int` depth counter: O(1) space. This collapse recurs constantly in this chapter — learn to spot it.

### C layout

You do not need a "stack library". An array plus an index *is* a stack:

```c
#include <stdbool.h>
#include <string.h>

/* Stack of chars, capacity = strlen(s) is always enough for bracket problems. */
static bool valid_brackets(const char *s) {
    size_t n = strlen(s);
    char *st = malloc(n ? n : 1);      /* stack storage; every char pushed at most once */
    size_t top = 0;                    /* number of elements; st[top-1] is the top */
    bool ok = true;
    for (size_t i = 0; i < n && ok; i++) {
        char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            st[top++] = c;                                   /* push */
        } else {
            char want = c == ')' ? '(' : c == ']' ? '[' : '{';
            if (top == 0 || st[top - 1] != want) ok = false; /* check 1 and 2 */
            else top--;                                      /* pop */
        }
    }
    ok = ok && top == 0;                                     /* check 3: nothing left open */
    free(st);
    return ok;
}
```

If the input length is bounded (LeetCode usually says `1 <= n <= 10^4`), a fixed `char st[10001]` on the C stack is fine and avoids `malloc`. See `../../c_learning/10_data_structures/lesson.md` for the growable version.

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

Without the final emptiness check this string would wrongly pass.

### Depth counter instead of a stack

When there is a single bracket type, the stack content is `((((` — pure repetition. Only `top` matters. So `int depth` replaces the whole array: `(` does `depth++`, `)` does `depth--`, and "check 1" becomes `depth == 0` before decrementing. Two further generalisations appear in the problems:

- **Counting fixes** (minimum insertions): a closer hitting `depth == 0` is one unmatched closer — count it instead of failing; at the end, leftover `depth` is the number of unmatched openers.
- **Two-sided feasibility** (locked/unlocked positions): run the counter left-to-right treating free characters as openers, then right-to-left treating them as closers; both sweeps must stay non-negative.

**Python equivalent:** `stack = []; stack.append(c); stack.pop(); stack[-1]`. Nothing more — Python's `list` is already the array-backed stack.

### Pitfalls

- Forgetting the end-of-string emptiness check.
- Popping from an empty stack (segfault in C, `IndexError` in Python). Always check `top == 0` first.
- Storing characters when you later need positions: if the output is "which indices to delete", push **indices**, not characters (Unit 1 problem 6 and Unit 1 problem 9).
- In "longest valid" style problems the bottom of the stack is a **boundary marker**, not an open bracket waiting for a partner. Seed it with `-1` and treat an empty stack after a pop as "this closer is the new boundary".

---

## 2. Stack as an undo / state machine

### The idea

A stack is not just for brackets. It is the general tool whenever a new event can **cancel, replace, or collide with** previously recorded state, and the effect always reaches back only to the *nearest* earlier event. Path normalisation is the canonical example: `".."` cancels the nearest still-existing directory, not an arbitrary earlier one.

### The invariant

Scan the input once, left to right, keeping a stack of "state that is still in force so far". Each new element does one of three things:

- **(a) push** — it is a new independent piece of state;
- **(b) cancel** the top — and possibly keep cancelling, if removing the top exposes a new collision with the element below;
- **(c) get skipped** — because the top "wins" against it.

The difference from bracket matching is that the cancel rule can be an arbitrary predicate — compare sizes, compare directory names, check letter case — rather than a fixed type correspondence.

### Recognising it

Signal words: *undo*, *backspace*, *remove adjacent duplicates*, *simulate*, *go back*, *collision*, *destroyed*, *cancel*, or a description of a call stack / directory navigation.

### Complexity

O(n) amortised even with the inner "keep cancelling" loop: every element is pushed at most once and popped at most once, so total pops <= total pushes <= n. Space O(n).

### C layout

The pattern in C is the same array-plus-index. The three cases fall out as an `if / else if / else` around one `while`:

```c
/* Skeleton: resolve collisions where a new item x may destroy items on top. */
static size_t resolve(const int *in, size_t n, int *out /* capacity n */) {
    size_t top = 0;
    for (size_t i = 0; i < n; i++) {
        int x = in[i];
        bool alive = true;
        while (alive && top > 0 && collides(out[top - 1], x)) {
            int cmp = compare(out[top - 1], x);
            if (cmp < 0)       top--;                    /* (b) top loses, keep going   */
            else if (cmp == 0) { top--; alive = false; } /* both destroyed              */
            else               alive = false;            /* (c) top wins, x is skipped  */
        }
        if (alive) out[top++] = x;                       /* (a) push                    */
    }
    return top;   /* out[0..top) is the surviving state, bottom first */
}
```

Reading the answer "bottom first" is important: the surviving state is `out[0..top)` in index order, which is the natural left-to-right result. You never need to reverse it.

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

Note the chaining: removing `bb` exposes `a a`, which a single non-stack pass would miss.

### Depth counter, again

When the question is only "how deep are we" (directory depth after a log of `cd` commands), the stack of names collapses to an `int` that never goes below zero. When the question is "what is the final path", you need the actual names — the counter is illegal. Ask yourself *what does the output need?* before choosing.

**Python equivalent:** the same `list`; `os.path.normpath` is Unit 2 problem 4 in the standard library.

### Pitfalls

- Stopping after one cancellation. The `while` is not optional.
- Forgetting the "both destroyed" case in symmetric collisions (equal sizes).
- Off-by-one in time-accounting stacks (Unit 2 problem 7): an `end` event at timestamp `t` means the function ran *through* `t`, so the delta is `t - prev + 1` and the next function resumes at `t + 1`.
- Splitting strings in C: `strtok` modifies its input and is not re-entrant — copy the string first, or scan with `strchr`/manual pointer walking.

---

## 3. Expression evaluation

### The idea

Expression evaluation needs a stack for two *separate* reasons — keep them apart in your head:

1. **Order of operators and intermediate results.** In postfix notation (RPN), operands are pushed and each operator consumes the top two and pushes the result. No brackets, no precedence rules: the notation already encodes them.
2. **Nesting and precedence** in infix expressions like `"(1+(4*5+2)*3)"`: one stack for the current partial result and pending operator, and a second (explicit or implicit) for unwinding nesting at `)`.

### The skeleton for infix

Keep a running `result` and the previous operator/sign. On `(`, push the current `result` and sign onto the stack and start a fresh local computation. On `)`, pop the outer sign and outer result and merge: `result = outer_result + outer_sign * result`. Multiplication/division without brackets is handled by remembering only the *previous number* (not the whole stack) and replacing it when a `*` or `/` arrives.

The same "push context on `(`, merge on `)`" skeleton reappears with different payloads:

| Problem | What `[`/`(` pushes | What `]`/`)` does |
|---|---|---|
| Basic calculator | `(result, sign)` | `result = outer + sign * inner` |
| Decode string | `(repeat_count, prefix_string)` | `cur = prefix + cur * k` |
| Score of parentheses | `0` (new level) | `outer += max(2*inner, 1)` |
| Number of atoms | new empty count-map | multiply inner map by `k`, merge into outer |
| Boolean expression | operator token | pop operands until operator, apply, push result |

Recognise the shape once and you have five problems.

### Recognising it

Signal words: *evaluate*, *calculator*, *decode*, *parse*, *postfix/RPN*, *nested*.

### Complexity

O(n) time and O(n) space for a single pass; decode-string is O(n * maxK) because of output size.

### C layout

An operand stack of `long` (RPN inputs can overflow `int` in intermediate results):

```c
/* RPN evaluator skeleton. tokens[i] are NUL-terminated strings. Returns 0 on success. */
static int eval_rpn(const char **tokens, size_t n, long *out) {
    long *st = malloc((n ? n : 1) * sizeof *st);
    size_t top = 0;
    int err = 0;
    for (size_t i = 0; i < n && !err; i++) {
        const char *t = tokens[i];
        bool is_op = strlen(t) == 1 && strchr("+-*/", t[0]);
        if (!is_op) {
            st[top++] = strtol(t, NULL, 10);          /* operand: push */
        } else if (top < 2) {
            err = 1;                                  /* malformed */
        } else {
            long b = st[--top], a = st[--top];        /* a is the LEFT operand */
            switch (t[0]) {
            case '+': st[top++] = a + b; break;
            case '-': st[top++] = a - b; break;
            case '*': st[top++] = a * b; break;
            case '/': if (b == 0) err = 1; else st[top++] = a / b; break;
            }
        }
    }
    if (!err && top == 1) *out = st[0]; else err = 1;
    free(st);
    return err;
}
```

Note the operand order: the second pop is the **left** operand. `"3 4 -"` is `3 - 4`, not `4 - 3`. Also note `strchr("+-*/", t[0])` alone would treat a negative number like `"-3"` as an operator — the `strlen(t) == 1` guard prevents that.

### Worked example: RPN `2 1 + 3 *`

```
token  stack (bottom -> top)
2      2
1      2 1
+      3           (pop 1, pop 2, push 2+1)
3      3 3
*      9           (pop 3, pop 3, push 3*3)
result 9
```

Worked example: infix `1 - (2 + 3)` with the (result, sign) stack

```
char  result  sign  stack           note
1     1       +1    []              result += sign*1
-     1       -1    []
(     0       +1    [(1, -1)]       push outer context, reset
2     2       +1    [(1, -1)]
+     2       +1    [(1, -1)]
3     5       +1    [(1, -1)]
)     -4            []              pop (1,-1): result = 1 + (-1)*5
```

### Pitfalls

- Operand order on `-` and `/`.
- C integer division truncates toward zero, which is what these problems specify — but `%` on negatives also follows the dividend's sign; do not use `%` to "round".
- Reading multi-digit numbers: accumulate `num = num * 10 + (c - '0')` and only *commit* the number when you see an operator, a bracket, or the end of the string. Forgetting the end-of-string commit is the number one bug.
- Precedence without brackets (`2 + 3 * 4`): push `+`/`-` terms, but apply `*`/`/` immediately to the top of the stack. Sum the stack at the end.
- Whitespace: skip it, but do not let it break a number in half.
- Nested string building in C needs growable buffers — use the `Vec` pattern from `../../c_learning/06_dynamic_memory/lesson.md`.

---

## 4. Monotonic stack — next greater element

### The idea

A **monotonic stack** keeps its content sorted (increasing or decreasing from bottom to top) while scanning the input once, by popping elements from the top that **can no longer be the answer for any future query**. In "next greater element" problems the stack is **decreasing**: when a new element is larger than the top, the top has just found its answer (the new element is its next greater), so it is popped and its answer recorded — repeat while the top is smaller than the new element, then push the new element itself.

### The invariant

After processing index `i`, the stack holds exactly those indices `j <= i` whose next-greater element has **not yet been seen**, and their values are strictly decreasing from bottom to top. Why strictly decreasing? Because if `a[j] <= a[i]` for some `j` still on the stack, `i` *is* the next greater of `j` and `j` would have been popped.

### Complexity

Amortised O(n) even though the inner loop looks quadratic: each element is pushed exactly once and popped at most once over the whole run, so total pops cannot exceed total pushes. Space O(n).

### Two directions, four variants

| Want | Stack keeps | Pop while |
|---|---|---|
| next **greater** (strict) | decreasing | `a[top] < a[i]` |
| next **greater or equal** | strictly decreasing | `a[top] <= a[i]` |
| next **smaller** (strict) | increasing | `a[top] > a[i]` |
| next **smaller or equal** | strictly increasing | `a[top] >= a[i]` |

Scan right-to-left instead and "next" becomes "previous". Scan the array twice with `i % n` and you have the circular variant.

### Greedy construction

The same skeleton generalises to **greedy construction**: when building the smallest/largest possible result sequence by deleting elements (digits, letters), the stack stays monotonic by popping the top whenever the new element would be a better choice in its place — the same "pop until the condition holds" loop as next-greater, with the pop condition and stack direction varying per problem. Extra constraints get bolted onto the pop condition: a deletion budget `k`, a "still enough elements left to reach length `k`" check, or "this letter appears again later so it is safe to drop".

### Recognising it

Signal words: *next greater/smaller*, *days until a warmer temperature*, *span*, *previous less*, *remove k digits*, *lexicographically smallest subsequence*, *most competitive*.

### C layout

Push **indices**, not values. You almost always need both the value (for comparison) and the position (for the distance or to write the answer):

```c
/* For each i, ans[i] = index of the next strictly greater element, or -1. */
static void next_greater(const int *a, size_t n, int *ans) {
    size_t *st = malloc((n ? n : 1) * sizeof *st);   /* stack of indices */
    size_t top = 0;
    for (size_t i = 0; i < n; i++) {
        while (top > 0 && a[st[top - 1]] < a[i]) {    /* top just found its answer */
            ans[st[top - 1]] = (int)i;
            top--;
        }
        st[top++] = i;                                /* i now waits for its own answer */
    }
    while (top > 0) ans[st[--top]] = -1;              /* leftovers never found one */
    free(st);
}
```

### Worked example: `a = [2, 1, 2, 4, 3]`

```
i  a[i]  pops (index:value -> answer)   stack (indices, bottom->top)   values on stack
0  2     -                              [0]                            [2]
1  1     -                              [0 1]                          [2 1]
2  2     1:1 -> 2                       [0 2]                          [2 2]   (2 not > 2, no pop)
3  4     2:2 -> 3,  0:2 -> 3            [3]                            [4]
4  3     -                              [3 4]                          [4 3]
end: 3 -> -1, 4 -> -1
ans = [3, 2, 3, -1, -1]   (as indices)
```

Total pushes: 5. Total pops: 3. That is the amortised argument in numbers.

**Python equivalent:** `stack = []` of indices; there is no library shortcut, which is why it is worth learning.

### Pitfalls

- `<` versus `<=` in the pop condition decides how duplicates behave. Read the problem: "warmer" is strict, "discount if price <= current" is non-strict.
- Writing values on the stack and then needing distances. Push indices.
- Forgetting the leftover pass: elements still on the stack need `-1` / "unchanged" / `0` answers explicitly.
- In greedy-removal problems, when the scan ends with budget left over (input was already monotonic), delete from the **end** of the stack. Then strip leading zeros; an empty result means `"0"`.
- Online versions (stock span) store `(value, span)` pairs so popped spans accumulate — a popped element's span is inherited, not discarded.

---

## 5. Monotonic stack — histogram and rectangle problems

### The idea

When a problem asks for each element's **contribution** to a sum, or for the largest rectangle among bars, the monotonic stack solves two things at once in a single pass: for every element, its **nearest smaller neighbour on the left and on the right**. When bar `i` is popped from an increasing stack because the incoming element is smaller, we know exactly: bar `i`'s "region of influence" reaches left to the new top (the element remaining under it) and right to the element that just arrived — that interval is the widest region in which `i` is the minimum, i.e. the width bar `i` could span as a rectangle.

### The core formula

Keep an **increasing** stack of indices. When a new value `a[i]` is smaller than the top, pop the top `h`; its width is

```
width = i - (new top index) - 1        if the stack is non-empty after the pop
width = i                              if the stack is now empty
```

Everything else is what you multiply that width by:

| Problem | Contribution of popped `h` |
|---|---|
| Largest rectangle in histogram | `a[h] * width` — take the max |
| Sum of subarray minimums | `a[h] * left[h] * right[h]` where `left = h - L`, `right = R - h` — take the sum |
| Sum of subarray ranges | (sum of maximums) - (sum of minimums), two passes |
| Trapping rain water (decreasing stack, mirror image) | `(min(a[L], a[i]) - a[h]) * (i - L - 1)` — take the sum |

### The sentinel trick

After the scan, the stack still holds an increasing run of bars that were never popped. Rather than writing a second draining loop, append a virtual bar of height `0` (or `-infinity`) at index `n`: it is smaller than everything, so it pops the entire stack with the same formula. In C you can do this without copying by treating `i == n` as height `0` inside the loop.

### Recognising it

Signal words: *largest rectangle*, *histogram*, *maximal rectangle of 1s*, *sum over all subarrays of min/max*, *trapped water*, *contribution of each element*.

### Complexity

O(n) time and space. The 2-D version (maximal rectangle in a binary matrix) is O(rows * cols): build a per-row histogram of consecutive 1s above each cell, run the 1-D routine per row.

### C layout

```c
/* Largest rectangle under a histogram. Sentinel handled by letting i reach n. */
static long largest_rect(const int *h, size_t n) {
    size_t *st = malloc((n + 1) * sizeof *st);   /* increasing stack of indices */
    size_t top = 0;
    long best = 0;
    for (size_t i = 0; i <= n; i++) {
        int cur = (i == n) ? 0 : h[i];            /* virtual 0-height bar at i == n */
        while (top > 0 && h[st[top - 1]] > cur) { /* strict >: equal heights stay */
            size_t k = st[--top];
            size_t width = (top == 0) ? i : i - st[top - 1] - 1;
            long area = (long)h[k] * (long)width;
            if (area > best) best = area;
        }
        if (i < n) st[top++] = i;
    }
    free(st);
    return best;
}
```

For the contribution-sum problems store the left span on push (`left[i] = i - (top ? st[top-1] : -1)`) and the right span on pop (`right[k] = i - k`), then accumulate `a[k] * left[k] * right[k]`. Use `long long` and reduce modulo `1e9+7` as you go.

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

ASCII view of the winning rectangle:

```
        6
      5 #
      # #
      # #     3
  2   # # 2   #
  # 1 # # # # #
```

### Duplicates and double counting

For *sums* over subarrays, equal values must be counted exactly once. Use a **strict** comparison on one side and **non-strict** on the other: e.g. "nearest strictly smaller on the left, nearest smaller-or-equal on the right". Then among a run of equal minima, each subarray is attributed to exactly one of them. For *max* problems (largest rectangle) it does not matter which of two equal bars gets the credit, but the strictness still affects which pop computes the full width — with `>` in the pop condition (as above) the leftmost equal bar computes the full-width rectangle when the sentinel arrives.

**Python equivalent:** none in the standard library; `itertools.accumulate` gives you the prefix sums for the prefix-of-prefix variants.

### Pitfalls

- `int` overflow: `height * width` with heights and widths up to 10^4–10^5 exceeds 2^31. Multiply in `long long`.
- Off-by-one in `width = i - L - 1`: the bars at `L` and `i` are the *walls*, not part of the rectangle.
- Forgetting the sentinel and returning `best` with bars still on the stack.
- Mixing up which stack direction: rectangles/minimums use an *increasing* stack (pop on smaller incoming); trapping water uses a *decreasing* stack (pop on taller incoming). Both compute left/right neighbours — of opposite kinds.
- Modular arithmetic: reduce after every multiplication; `(a * b) % MOD` overflows if `a, b` are both near `MOD` unless `a, b` are `long long` and already reduced.

---

## 6. Min-stack and stack-backed design problems

### The idea

In design problems the stack is a building block inside a larger structure, and the trick is almost always the same: **each stack node carries extra information about the current state**, not just the original value, so a query ("what is the minimum right now?") answers in O(1) without scanning the stack. `min-stack` is the base example: alongside every pushed value store "the smallest value in the stack up to here"; popping cannot break the minimum because it automatically reverts to the previous node's snapshot.

### Two stacks make a queue

Two stacks (an `in` end and an `out` end) simulate a FIFO queue: when `out` is empty, pour the whole of `in` into it in one go — the order flips exactly right, and each element moves between stacks at most once in its lifetime, which keeps the amortised cost at O(1) per operation even though a single pour is O(n).

### Recognising it

The problem asks you to *implement* or *design* a class supporting several operations efficiently (`push/pop/top/getMin`, `visit/back/forward`, `next/hasNext`, `push/popAtStack`). Signal words: *design*, *implement*, *support the following operations*, *in O(1)*.

### Complexity

O(1) per operation, sometimes amortised. Space O(n) plus whatever per-node extra state you carry.

### C layout

Structs with parallel arrays or an array of structs. Here is a min-stack with an array of `(value, min_so_far)` pairs, growable with `realloc`:

```c
typedef struct { int value, min_so_far; } MinNode;
typedef struct { MinNode *data; size_t len, cap; } MinStack;

static void ms_init(MinStack *s) { s->data = NULL; s->len = s->cap = 0; }
static void ms_free(MinStack *s) { free(s->data); ms_init(s); }

/* Returns 0 on success, -1 on allocation failure. */
static int ms_push(MinStack *s, int v) {
    if (s->len == s->cap) {
        size_t ncap = s->cap ? 2 * s->cap : 8;
        MinNode *t = realloc(s->data, ncap * sizeof *t);
        if (!t) return -1;
        s->data = t; s->cap = ncap;
    }
    int m = s->len ? s->data[s->len - 1].min_so_far : v;
    s->data[s->len].value = v;
    s->data[s->len].min_so_far = v < m ? v : m;      /* snapshot the minimum */
    s->len++;
    return 0;
}
static void ms_pop(MinStack *s)        { if (s->len) s->len--; }
static int  ms_top(const MinStack *s)  { return s->data[s->len - 1].value; }
static int  ms_min(const MinStack *s)  { return s->data[s->len - 1].min_so_far; }
```

`ms_top` and `ms_min` assume a non-empty stack: in C there is no exception to throw, so document the precondition and check `len` at the call site (or return a status code and write through a pointer, as in Chapter 10).

### Worked example: min-stack ops `push 5, push 3, push 7, push 2, pop, getMin, pop, getMin`

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

"Increment the bottom `k` elements by `val`" is O(k) naively. Instead keep a parallel `inc[]` array and add `val` only at index `min(k, len) - 1`. On pop from index `i`, return `data[i] + inc[i]`, then push the pending increment one level down (`inc[i-1] += inc[i]`) and clear `inc[i]`. Every operation becomes O(1). This is the same *lazy* idea as a lazy segment tree, in its simplest possible setting.

### Explicit stack instead of recursion

A nested-list iterator (`hasNext`/`next` over `[1,[4,[6]]]`) flattens lazily: push the top-level elements in reverse so the first is on top; `hasNext` keeps popping lists and pushing their children (reversed) until the top is a scalar or the stack is empty. Each element is unpacked exactly once, so total work is linear even though it is spread across many calls. This is precisely how you convert a recursive tree traversal into an iterative one in C when recursion depth is a concern.

**Python equivalent:** `collections.deque` for the queue; the min-stack has no stdlib equivalent — you write it the same way.

### Pitfalls

- Storing the minimum in a single global variable. Pop then cannot recover the previous minimum. It must live in each node.
- Pouring `in` into `out` while `out` is non-empty. That interleaves two batches and destroys FIFO order. Pour **only when `out` is empty**.
- A browser-history `visit` must discard the *whole* forward stack, not just its top.
- Designs with many small stacks (dinner plates) need an auxiliary structure — a min-heap of "stacks with free room" — see `../../c_learning/10_data_structures/lesson.md` for the heap.
- In C every design struct needs an explicit `init` and `free`; there is no destructor. Put ownership in the struct's comment.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Stack holds | Complexity |
|---|---|---|---|
| valid / balanced brackets, matching pairs | simple stack, 3 checks | opener chars | O(n) / O(n) |
| only one bracket type; "minimum adds"; depth | depth counter | nothing (an `int`) | O(n) / O(1) |
| longest valid substring | stack of indices + boundary marker at bottom | indices | O(n) / O(n) |
| undo, backspace, adjacent duplicates, collision, simulate | state-machine stack with `while` cancel loop | surviving items | O(n) amortised |
| final path / directory depth | stack of names / depth counter | names or `int` | O(n) |
| call stack, exclusive time | stack of ids + `prev_time` | function ids | O(n) |
| RPN / postfix | operand stack | numbers | O(n) |
| infix with `( )`, decode `k[...]`, nested formula | push `(context)` on `(`, merge on `)` | (result, sign) / (k, prefix) / maps | O(n) |
| next greater / smaller, warmer day, discount | monotonic stack (dec. for greater, inc. for smaller) | indices | O(n) amortised |
| circular array next greater | monotonic stack, scan `2n` with `i % n` | indices | O(n) |
| stock span, online stream | monotonic stack of (value, span) | pairs | amortised O(1)/call |
| remove k digits, smallest subsequence, competitive | monotonic increasing stack + budget condition | digits / values | O(n) |
| remove duplicate letters | monotonic stack + in-stack set + last-occurrence array | chars | O(n) |
| 132 pattern | right-to-left decreasing stack + best popped value | values | O(n) |
| largest rectangle, histogram | increasing stack + sentinel, `h * (i - L - 1)` | indices | O(n) |
| maximal rectangle in 0/1 matrix | per-row histogram → largest rectangle | indices per row | O(rows * cols) |
| sum of subarray minimums / maximums / ranges | contribution `a[i] * left * right` | indices | O(n) |
| trapping rain water | decreasing stack, `(min(walls) - bottom) * width` | indices | O(n) |
| sum of all subarray sums where `i` is min | monotonic stack + prefix-of-prefix sums, mod | indices | O(n) |
| design: getMin in O(1) | node carries `min_so_far` | (value, min) | O(1) each |
| design: queue from stacks | two stacks, pour when `out` empty | values | amortised O(1) |
| design: increment bottom k | lazy `inc[]` array | values + pending inc | O(1) each |
| design: flatten nested iterator | explicit stack replacing recursion | list cursors | amortised O(1) |
| design: max-frequency pop | map freq → stack of values, `max_freq` | per-frequency stacks | O(1) each |

---

## Gotchas in C specifically

- **No stack type in the standard library.** An array plus a `size_t top` is the stack. `st[top++] = x` pushes, `x = st[--top]` pops, `st[top - 1]` peeks. Never peek or pop when `top == 0` — it is undefined behaviour, and on most inputs it will *appear* to work.
- **Capacity.** In every pattern in this chapter each input element is pushed at most once (twice for the circular scan), so capacity `n` (or `2n`, or `n + 1` for a sentinel) is always enough. Allocate it once; do not `realloc` in the loop unless the input length is unknown (streams, design problems).
- **`size_t` vs `int` for indices.** `size_t` is unsigned, so `i - st[top-1] - 1` is fine when `i > st[top-1]` (always true here) but `top - 1` when `top == 0` wraps to `SIZE_MAX`. Guard every `st[top - 1]` with `top > 0`. When you need `-1` as an answer, the answer array must be `int`/`long`, not `size_t`.
- **Overflow in products.** `height * width`, `value * left * right`, and RPN intermediates can exceed `INT_MAX`. Cast to `long long` *before* multiplying: `(long long)h[k] * width`, not `(long long)(h[k] * width)`.
- **Modular arithmetic.** Reduce after every `*` and `+`; keep operands in `long long`; `MOD` as `const long long MOD = 1000000007LL`.
- **Character stacks are `char` arrays, not strings.** They have no terminating `'\0'` unless you add it. If you need to return the stack as a string, write `st[top] = '\0'` after allocating `n + 1` bytes.
- **Splitting a path.** `strtok` mutates and is stateful; prefer scanning with two pointers between `'/'` characters, or `strtok_r`/`strsep` where available. Component strings you keep on the stack must be copied (`strndup` on POSIX, or `malloc` + `memcpy`) or stored as `(start, length)` pairs pointing into the original buffer.
- **Digit stacks.** `c - '0'` converts a digit char; `'0' + d` converts back. A stack of digits for "remove k digits" is a `char` array; strip leading zeros by advancing a pointer, and remember that an empty result is `"0"`, not `""`.
- **Recursion depth.** LeetCode inputs of 10^4–10^5 nested brackets will overflow the default 8 MB C stack in a recursive descent parser. Use an explicit stack (this whole chapter) or increase the stack size — the explicit stack is the portable choice.
- **Sorting output** (number of atoms wants alphabetical order): `qsort` with a comparator `int cmp(const void *a, const void *b)` that `strcmp`s the keys. See `../../c_learning/11_function_pointers_and_generics/lesson.md`.
- **Hash tables** (number of atoms, next greater element I, max frequency stack) you write yourself; for small alphabets (`'a'..'z'`, 26 digits, 128 ASCII) a plain array indexed by the character is a perfectly good hash table and is what you should use.
- **Boolean return.** Include `<stdbool.h>` and return `bool`, not `int`, from predicates — it documents intent and `-Wall` will not complain either way.

---

## Common mistakes checklist

- [ ] Did you check the stack is **empty at the end** (bracket problems)?
- [ ] Did you check `top > 0` before every peek/pop?
- [ ] Are you pushing **indices** when you need distances, widths, or positions to delete?
- [ ] Is the cancel/pop loop a `while`, not an `if`?
- [ ] Strict `<` or non-strict `<=` — did you read how the problem treats equal values?
- [ ] Did you handle **leftover** stack elements (answer `-1`, sentinel, or drain loop)?
- [ ] In `(`/`)` merges, is the stack storing the **outer** context (result *before* the bracket), and do you reset local state after the push?
- [ ] Did you commit the last multi-digit number at end of string?
- [ ] Is the second popped operand the **left** one for `-` and `/`?
- [ ] Widths: `i - L - 1`, not `i - L`. Does the empty-stack case use `i` (or `i + 1` for the contribution count)?
- [ ] Are products computed in `long long` and reduced modulo if required?
- [ ] For greedy removal: budget exhausted correctly, trailing deletions from the end, leading zeros stripped, `"0"` for empty?
- [ ] For min-stack designs: is the extra state stored **per node**, not globally?
- [ ] For two-stack queues: do you pour only when `out` is empty?
- [ ] Did you `free` every buffer you `malloc`ed, including on early-return error paths?

---

## You can move on when...

- You can write `next_greater` and `largest_rect` from memory in C, with the sentinel, in under ten minutes each, and they compile with `-Wall -Wextra` clean.
- You can explain in one sentence why the monotonic-stack inner loop is O(n) total, and you can tell whether a given problem wants an increasing or decreasing stack without trying both.
- Given a bracket-flavoured problem, you can say immediately whether a depth counter suffices or a real stack (of chars? of indices?) is required.
- You can write the `(result, sign)` infix evaluator and then describe how to change it into decode-string and score-of-parentheses without touching the control flow.
- You have solved at least the level-2 and level-3 problems of every unit in `problems.md` in C, with your own tests in `main()`, and at least one level-4 problem from Unit 5.
