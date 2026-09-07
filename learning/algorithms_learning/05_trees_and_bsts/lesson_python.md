# Chapter 05 — Trees & BSTs — Python

## What you'll be able to do after this chapter

- Design a tree recursion by answering one question first — *what does the parent need from its child?* — and recognise the three return-value shapes (state flowing down, result flowing up, both via a `nonlocal`/side variable).
- Write preorder / inorder / postorder traversals both recursively and with an explicit stack, and pick the order from where on the root-to-leaf route you need the node's value.
- Run BFS on a tree with `collections.deque` and the `level_size` snapshot, and solve every "per level" question (averages, right side view, zigzag, nearest leaf) from the same skeleton.
- State the BST invariant precisely (whole subtree, not just children), exploit it for `O(h)` navigation and for "inorder = sorted", and validate it with a `(lo, hi)` window.
- Solve lowest-common-ancestor and path-sum problems, including the prefix-sum-dict trick for paths that start and end anywhere.
- Rebuild a tree from traversal pairs and serialise/deserialise one with explicit null markers.

## Why this matters for ML / numerics / sims

Trees are the data structure you build when the problem has hierarchy or a divide-and-conquer split. In numerics and simulation they show up constantly: a **k-d tree** or **octree** partitions space so k-NN, range queries and Barnes-Hut gravity run in `O(log n)` per query instead of `O(n)` — every operation on it is a "which subtree do I recurse into?" decision exactly like a BST search. A **Huffman tree** or a **decision tree / random forest** is a binary tree where inference is a root-to-leaf walk carrying state downward (Unit 1's "state flows down" shape). An **expression tree** in an autograd engine is evaluated postorder (children before parent — Unit 2), and backprop is the same tree walked in the opposite direction. A **BVH** (bounding volume hierarchy) in a collision/ray sim is rebuilt from a sorted array the same way Unit 6 rebuilds a balanced BST from a sorted array. **Level-order BFS** is how you compute the depth of each node in a hierarchical clustering dendrogram or lay out a tree for plotting.

**Python vs C, once for the chapter:** a node is a small class with `.val`/`.left`/`.right` — `None` is `NULL`, and there is no `tree_free`: the garbage collector reclaims a subtree the moment nothing references it, so "did I free every node" simply does not apply. There is no fixed-size `TreeNode *stack[64]` — a `list` grows on demand — and no `deque`-free BFS queue hack: `collections.deque` gives O(1) popleft. Recursion depth is the one place Python is *worse* than C: the default limit is ~1000 frames, so a skewed 10^4-node tree needs either `sys.setrecursionlimit` or (better, and what this chapter teaches) the iterative form.

---

## 0. The node, and the two shapes of a tree function

Every problem in this chapter uses this node (LeetCode's `TreeNode`):

```python
class TreeNode:
    def __init__(self, val: int = 0, left: "TreeNode | None" = None, right: "TreeNode | None" = None):
        self.val = val
        self.left = left
        self.right = right
```

No `node_new`/`tree_free` pair: `TreeNode(5)` allocates, and a subtree is reclaimed automatically once nothing points to it — `t.left = None` is enough to drop it. A tree of `n` nodes is still `n` separate objects on the heap (Python objects, not 24-byte C structs — each `TreeNode` is well over 50 bytes with its dict/slots overhead), scattered and cache-hostile exactly like the C version. Trees earn their pointers when the *shape* carries information; arrays (heaps, Chapter 10) win when it does not.

Two conventions you will use over and over:

| Shape | Signature | Meaning | Examples |
|---|---|---|---|
| Query | `def f(t: TreeNode | None, ...) -> X` | compute something about the subtree, don't change it | height, path sum, validate |
| Transform | `def f(t: TreeNode | None, ...) -> TreeNode | None` | return the (possibly new) root of the modified subtree; the caller assigns it: `t.left = f(t.left)` | invert, insert, delete, build |

The transform shape matters just as much in Python as in C, even though Python *can* mutate `t.left` directly: `bst_insert(root, 5)` on an empty tree cannot rebind the caller's `root` variable from inside the function (Python passes the reference by value, same as a C pointer) — you must return the new root and have the caller reassign it, `root = bst_insert(root, 5)`.

---

## 1. Recursion on trees: what does the function return?

The basic form of tree recursion is always the same: the function receives a node, calls itself on the children, and combines the children's answers into its own answer. All the difficulty lies in **what the function returns** — that decision solves the whole problem.

### Three typical return values

1. **State flowing down** (parameter). The information is about the path from the root: "is this branch within the allowed `(lo, hi)` range?", "what is the largest value seen so far on the way here?", "what is the remaining target sum?". The parent computes a new value and *passes it as an argument*.
2. **Combined result flowing up** (return value). Height, size, sum, "is identical to that other tree". The children's answers are computed *first*, combined only on the way back — a postorder computation whether you call it that or not.
3. **Both at once via a side variable.** The answer the problem asks for is *not* the same thing the function must return to its parent. Diameter: every node updates a shared `best` as a side effect, but still returns the *height*, because height is what the parent needs to combine its own candidate. In Python, "side variable" means `nonlocal` inside a nested helper, or a one-element list/`itertools.count`-style mutable cell if you want to avoid a closure.

The **typical mistake** is trying to return two things as one number — e.g. encoding "unbalanced" as `-1`. It works, but only if you recognise that this is exactly the trick you need. Always ask first: *what information does the parent node need from its child in order to make its own decision?*

### Python layout

```python
def height_and_diameter(root: TreeNode | None) -> int:
    best = 0
    def dfs(t: TreeNode | None) -> int:
        nonlocal best
        if t is None:
            return 0                                # base case: empty subtree has height 0
        hl = dfs(t.left)                            # children first ...
        hr = dfs(t.right)
        best = max(best, hl + hr)                   # side effect: the actual answer
        return 1 + max(hl, hr)                       # return value: height for the parent
    dfs(root)
    return best
```

`nonlocal best` is the `int *best` out-parameter: without it, `best = ...` inside `dfs` would create a new local shadowing the outer one, and the diameter would silently stay `0`.

### Worked mini-example

Tree (7 nodes):

```
          1
        /   \
       2     3
      / \     \
     4   5     6
    /
   7
```

Postorder evaluation of `dfs`:

| node | hl | hr | `hl+hr` | best after | returns |
|---|---|---|---|---|---|
| 7 | 0 | 0 | 0 | 0 | 1 |
| 4 | 1 | 0 | 1 | 1 | 2 |
| 5 | 0 | 0 | 0 | 1 | 1 |
| 2 | 2 | 1 | 3 | 3 | 3 |
| 6 | 0 | 0 | 0 | 3 | 1 |
| 3 | 0 | 1 | 1 | 3 | 2 |
| 1 | 3 | 2 | **5** | **5** | 4 |

Diameter 5 edges (7-4-2-1-3-6). The root's *return value* (height 4) is not the answer — `best` is.

### Complexity and pitfalls

- Every node visited once with constant work: `O(n)` time. Call-stack depth `O(h)`, worst case `O(n)` on a degenerate (linked-list-shaped) tree, `O(log n)` balanced — and `O(n)` recursion in Python risks `RecursionError` well before it risks a segfault.
- "Leaf" means `t.left is None and t.right is None`. A node with one child is *not* a leaf; checking only one side is the most common path-sum bug.
- Comparing `abs(hl - hr) > 1` only at the root is not a balance check — combine it into one pass (sentinel `-1`) or you get `O(n^2)` from recomputing heights.
- Short-circuit `and`/`or` in boolean recursions (`same_tree`) so you stop at the first mismatch.
- Nested recursions (`is_subtree`: walk the big tree, at each node run `is_same`) multiply cost: `O(n*m)`.

---

## 2. Traversals: preorder, inorder, postorder

The three basic traversals differ only in **when** the node itself is processed relative to its children: preorder processes the node before its children (N L R), inorder between them (L N R), postorder after them (L R N). In a BST, inorder yields the values in ascending order — **"inorder BST = sorted"**.

| Order | Visit sequence | Node is processed... | Use it when |
|---|---|---|---|
| preorder | N L R | on the way down | copying/serialising; carrying a root-to-leaf path (string, number, running max) |
| inorder | L N R | between the subtrees | anything wanting BST values sorted: k-th smallest, min gap, validate |
| postorder | L R N | on the way back up | the parent's answer depends on both children's finished results (height, size, flatten) |

### Recursive skeleton (all three are the same lines with one moved)

```python
def inorder(t: TreeNode | None, out: list[int]) -> None:
    if t is None:
        return
    inorder(t.left, out)
    out.append(t.val)            # move this line up for preorder, down for postorder
    inorder(t.right, out)
```

`yield` is the more Pythonic version and avoids building the whole list up front when a caller only wants the first few values:

```python
def inorder_gen(t: TreeNode | None):
    if t is not None:
        yield from inorder_gen(t.left)
        yield t.val
        yield from inorder_gen(t.right)
```

### Iterative versions: the explicit stack shows what recursion hides

```python
def inorder_iter(root: TreeNode | None) -> list[int]:
    out: list[int] = []
    st: list[TreeNode] = []           # a plain list IS the stack: append/pop are O(1)
    cur = root
    while cur or st:
        while cur:                     # go as far left as possible
            st.append(cur)
            cur = cur.left
        cur = st.pop()                 # leftmost unprocessed node
        out.append(cur.val)            # process on POP, not on push
        cur = cur.right                # then its right subtree
    return out
```

Iterative preorder: push root; loop `pop, process, push right, push left` — pushing left *last* makes it pop *first*. Iterative postorder: run preorder with children swapped (N R L), collect, then reverse — reversed N-R-L is L-R-N, i.e. postorder. `out[::-1]` does the reversal in one expression.

### Worked mini-example

```
        4
      /   \
     2     6
    / \   /
   1   3 5
```

| order | output |
|---|---|
| preorder | 4 2 1 3 6 5 |
| inorder | 1 2 3 4 5 6  (sorted — this is a BST) |
| postorder | 1 3 2 5 6 4 |
| N R L (preorder, children swapped) | 4 6 5 2 3 1 |
| N R L reversed | 1 3 2 5 6 4 = postorder |

Iterative inorder stack trace, first few steps:

```
cur=4: push 4, cur=2: push 2, cur=1: push 1, cur=None      stack: [4 2 1]
pop 1, out=[1], cur=1.right=None                            stack: [4 2]
pop 2, out=[1 2], cur=2.right=3: push 3, cur=None           stack: [4 3]
pop 3, out=[1 2 3], cur=None                                stack: [4]
pop 4, out=[1 2 3 4], cur=6: push 6, cur=5: push 5 ...      stack: [6 5]
```

### Carrying state down a traversal

```python
def sum_numbers(t: TreeNode | None, acc: int = 0) -> int:
    """Preorder with state flowing down: sum of root-to-leaf numbers."""
    if t is None:
        return 0
    acc = acc * 10 + t.val
    if t.left is None and t.right is None:      # leaf: this path is complete
        return acc
    return sum_numbers(t.left, acc) + sum_numbers(t.right, acc)
```

Carrying an `int` accumulator is O(1) per step and immutable-safe: each recursive call gets its own local `acc`, so there is nothing to backtrack — Python's argument passing does the copy for you. A *string* path (`path + str(t.val)`) would also work and cost the same asymptotically here because strings are immutable and each concatenation makes a new one, but a mutable `list` path needs an explicit `path.append(...)` / `path.pop()` backtrack, same as the C `char` buffer's `len--`.

### Pitfalls

- Iterative preorder: pushing left before right reverses the output.
- Iterative inorder: "process" happens on **pop**, not on push.
- N-ary trees: push children in *reverse* order so the first child pops first.
- Flatten-to-linked-list: do N-R-L (reversed preorder) while tracking `prev`; a normal preorder overwrites `.left` before you have used it to descend.
- A `list` used as a stack (`append`/`pop`) is fine; never `list.pop(0)` or `list.insert(0, x)` — those are O(n) and belong to a queue, not a stack.

---

## 3. Level by level: BFS on trees

BFS on a tree visits nodes level by level using a queue — a tree never needs a "visited" mark because there is exactly one path from the root to each node. On each round you process **the whole current queue size** (`level_size = len(q)`) before moving to the next level; that snapshot is what separates "BFS over the whole tree" from "one level at a time".

You recognise a BFS problem when it asks something **per level**: the level's average, the level's rightmost visible node, a zigzag ordering, or the shortest root-to-leaf distance. DFS would find *some* leaf; BFS finds the *nearest* leaf first, because it visits nodes in distance order — same principle as shortest path in an unweighted graph (Chapter 06).

### The queue in Python

```python
from collections import deque

def bfs_levels(root: TreeNode | None) -> None:
    if root is None:
        return
    q: deque[TreeNode] = deque([root])
    depth = 0
    while q:
        level_size = len(q)               # snapshot BEFORE the inner loop
        total = 0
        for _ in range(level_size):
            t = q.popleft()
            total += t.val                  # per-level work: sum / collect / last-node
            if t.left:
                q.append(t.left)
            if t.right:
                q.append(t.right)
        print(f"depth {depth}: {level_size} nodes, avg {total / level_size:.2f}")
        depth += 1
```

`collections.deque` gives O(1) `popleft`; a plain `list` with `pop(0)` would be O(n) per dequeue, turning the whole BFS into O(n^2) — the one place a `list` is the *wrong* choice of queue.

### Worked mini-example

```
        3
      /   \
     9    20
         /  \
        15   7
```

| round | queue before | level_size | processed | enqueued | per-level result |
|---|---|---|---|---|---|
| 0 | [3] | 1 | 3 | 9, 20 | avg 3.0; last = 3 |
| 1 | [9 20] | 2 | 9, 20 | 15, 7 | avg 14.5; last = 20 |
| 2 | [15 7] | 2 | 15, 7 | — | avg 11.0; last = 7 |

Right side view = the "last" column: `3 20 7`. Minimum depth: the first leaf encountered is `9` at round 1 → depth 2, stop immediately. Zigzag: reverse the collected list on odd rounds (`[3] [20 9] [15 7]`) — the *queue order never changes*, only how you emit the level.

### Variants from the same skeleton

| Problem | Per-level work |
|---|---|
| Level order (list of lists) | collect the level's values into a fresh list |
| Level order bottom-up | collect normally, `result.reverse()` at the end — never `insert(0, level)` |
| Average of levels | `total / level_size` |
| Right side view | remember the last node processed in the level |
| Zigzag | `level.reverse()` when `depth` is odd |
| Minimum depth | return `depth + 1` on the first node with no children |
| Cousins | keep a parallel `parent` dict alongside the queue; same level and different parents → cousins |
| Populate next-right pointers | link `q[i]` to `q[i+1]` while iterating the level (needs random access — use a `list` snapshot of the level instead of draining a `deque`) |

### Pitfalls

- Using the live `len(q)` as the loop bound instead of a snapshot: children get appended mid-loop, the level boundary disappears.
- Enqueuing `None` children — check `if t.left:` before appending.
- A node with one child is not a leaf; minimum depth must not stop there.
- Taking the *first* node of a level gives the left view, not the right.
- `list.pop(0)` as a queue: correct but O(n) — always `deque`.

---

## 4. The BST invariant and how to exploit it

A binary search tree's invariant: for every node, the **entire left subtree** contains only smaller values and the **entire right subtree** only larger values — not just the immediate children, the whole subtree. This one property gives a binary-search decision at every node ("go left or go right"), bringing search, insert and delete to `O(h)` — `O(log n)` balanced.

### Two ways to exploit the invariant

1. **Navigation.** Every node tells you exactly which direction the sought value lies in, so you never examine both children. Search, insert, delete, LCA in a BST, range pruning: all `O(h)`.
2. **Inorder order.** Inorder visits left-node-right and the invariant guarantees left < node < right, so inorder yields values in ascending order — **always**. K-th smallest, minimum gap, two-sum, validate-by-monotonicity all become sorted-array problems.

### Python layout

```python
def bst_search(t: TreeNode | None, key: int) -> TreeNode | None:
    while t and t.val != key:                 # iterative, O(h), no stack
        t = t.left if key < t.val else t.right
    return t                                   # None if absent

def bst_insert(t: TreeNode | None, key: int) -> TreeNode | None:
    """Transform shape: caller must do `root = bst_insert(root, key)`."""
    if t is None:
        return TreeNode(key)                  # reached the empty slot: attach here
    if key < t.val:
        t.left = bst_insert(t.left, key)
    elif key > t.val:
        t.right = bst_insert(t.right, key)
    return t                                    # duplicates: ignored

def bst_valid(t: TreeNode | None, lo: float = float("-inf"), hi: float = float("inf")) -> bool:
    """Carry the allowed OPEN interval down. float inf sentinels never collide with int values."""
    if t is None:
        return True
    if not (lo < t.val < hi):
        return False
    return bst_valid(t.left, lo, t.val) and bst_valid(t.right, t.val, hi)
```

**Validation** is the subtle spot: checking only `left.val < val < right.val` is not enough, because the constraint applies to the whole subtree. So validation carries the allowed `(lo, hi)` window downward — going left tightens the upper bound to the current value, going right tightens the lower bound.

### Delete: the three cases

Navigate to the node as in search, then:

1. **Leaf** — return `None`.
2. **One child** — return that child (the parent splices it in; Python's GC reclaims the removed node once nothing references it).
3. **Two children** — find the **inorder successor** (leftmost node of the right subtree), copy its value into this node, then recursively delete the successor's value from the right subtree.

```python
def bst_delete(t: TreeNode | None, key: int) -> TreeNode | None:
    if t is None:
        return None
    if key < t.val:
        t.left = bst_delete(t.left, key)
    elif key > t.val:
        t.right = bst_delete(t.right, key)
    else:
        if t.left is None:
            return t.right                      # 0 or 1 child (right)
        if t.right is None:
            return t.left                       # 1 child (left)
        succ = t.right
        while succ.left:                        # leftmost of right subtree
            succ = succ.left
        t.val = succ.val                        # copy value up
        t.right = bst_delete(t.right, succ.val)  # remove the original successor
    return t
```

Copying the value but forgetting to remove the original successor leaves the value in the tree twice.

### Worked mini-example: range pruning

BST, query range sum for `[7, 15]`:

```
          10
        /    \
       5      15
      / \       \
     3   7       18
```

| node | relation to [7,15] | action | running sum |
|---|---|---|---|
| 10 | inside | add; recurse both | 10 |
| 5 | `5 < 7` | whole left subtree (3) too small → skip; recurse right only | 10 |
| 7 | inside | add; both children None | 17 |
| 15 | inside | add; recurse both | 32 |
| 18 | `18 > 15` | whole right subtree too large → skip; recurse left only (None) | 32 |

Node `3` was never visited. Cost `O(h + k)` where `k` is the number of in-range nodes, not `O(n)`.

### Pitfalls

- `float("-inf")`/`float("inf")` sentinels are the Python fix for the C `INT_MIN`/`INT_MAX` collision bug — a node value can never equal infinity.
- Inorder-based problems (min gap, k-th smallest) do not need the whole sorted list in memory — carry `prev` through the traversal via `nonlocal`: `O(h)` space instead of `O(n)`.
- K-th smallest with an early-exit iterative inorder is `O(h + k)`.
- Two Sum on a BST via a `set`: insert into the set *after* checking, so `target == 2*val` cannot match a node with itself.
- This BST is not self-balancing; a sorted insertion sequence gives `h = n` — recursive insert can raise `RecursionError` on large inputs, so use the iterative form there.
- `bisect` on a sorted `list` is the array analogue of BST navigation; `sortedcontainers.SortedList` (third-party, not stdlib) is what a balanced BST gives you.

---

## 5. Lowest common ancestor and path problems

### LCA in a general binary tree

Node `x` is the LCA of `p` and `q` exactly when `p` and `q` are found in **different** subtrees among `x`'s children (or `x` itself is one of them). The function returns "did I find `p` or `q` in this subtree — and if both were found in different branches, this node is the LCA" — Unit 1's Shape 2 with a node reference as the return type.

```python
def lca(t: TreeNode | None, p: TreeNode, q: TreeNode) -> TreeNode | None:
    if t is None or t is p or t is q:
        return t                                # found one (or nothing)
    left = lca(t.left, p, q)
    right = lca(t.right, p, q)
    if left and right:
        return t                                 # one on each side: t is the LCA
    return left if left else right               # pass the single find upward
```

Use `is` here, not `==`: nodes are compared by identity, not by `.val` (duplicate values would break `==`). If `p` is an ancestor of `q`, the recursion stops at `p` and `p` is correctly the LCA — the algorithm handles it *because* a node can be "its own find". `O(n)` time.

### LCA in a BST

```python
def bst_lca(t: TreeNode, p: int, q: int) -> TreeNode:
    while (p < t.val and q < t.val) or (p > t.val and q > t.val):
        t = t.left if p < t.val else t.right
    return t                                     # t is the LCA
```

`O(h)` instead of `O(n)` — the invariant tells you which direction each value lies in, so the LCA is the first node where `p` and `q` "split".

### Path problems: where can the path start and end?

| Path type | Technique | Complexity |
|---|---|---|
| root -> leaf, existence | DFS carrying `remaining = target - val`; check `remaining == 0` at a *leaf* | `O(n)` |
| root -> leaf, list all | same, plus `path.append(val)` / `path.pop()` backtracking; **copy** (`path[:]` or `list(path)`) into the result at a leaf | `O(n)` + `O(L)` per hit |
| any node down any descendant, count | prefix-sum `dict` along the current root path: count of `cum - target` seen so far; **undo** the dict entry on return | `O(n)` |
| any node to any node (may bend), max sum | Shape 3: return the best *single-direction* extension `val + max(0, l, r)`; update global best with `val + max(0,l) + max(0,r)` | `O(n)` |

The prefix-sum trick is the array "subarray sum equals k" trick lifted onto a tree: the root-to-current path *is* an array, and the recursion stack is the iteration over it. `collections.defaultdict(int)` is the count table; "undo" means decrementing the count after both recursive calls return, so sibling branches never see each other's sums.

```python
def path_sum_count(root: TreeNode | None, target: int) -> int:
    from collections import defaultdict
    counts = defaultdict(int)
    counts[0] = 1                                 # empty prefix, seeded once
    total = 0
    def dfs(t: TreeNode | None, cum: int) -> None:
        nonlocal total
        if t is None:
            return
        cum += t.val
        total += counts[cum - target]              # how many prefixes make a subpath == target
        counts[cum] += 1
        dfs(t.left, cum)
        dfs(t.right, cum)
        counts[cum] -= 1                            # undo: this path is done, siblings must not see it
    dfs(root, 0)
    return total
```

### Worked mini-example: maximum path sum

```
       -10
       /  \
      9    20
          /  \
         15   7
```

| node | l (single-dir best) | r | return `val + max(0,l,r)` | candidate `val + max(0,l) + max(0,r)` | best |
|---|---|---|---|---|---|
| 9 | 0 | 0 | 9 | 9 | 9 |
| 15 | 0 | 0 | 15 | 15 | 15 |
| 7 | 0 | 0 | 7 | 7 | 15 |
| 20 | 15 | 7 | 35 | **42** | 42 |
| -10 | 9 | 35 | 25 | 34 | 42 |

Answer 42 (15-20-7). The root returns 25 — not the answer — because a path through the root's *parent* could only use one of its branches. Negative branches are cut with `max(0, ...)`.

### Distance-k: turn the tree into a graph

For "all nodes at distance k from a target" you need to move up too: first a DFS that records each node's parent (`dict[TreeNode, TreeNode]`), then an ordinary BFS from the target treating every node as having three neighbours (left, right, parent), stopping at depth `k`. A visited `set` **is** mandatory here — the parent edge creates a cycle — and the start node must be marked visited before the BFS begins.

### Pitfalls

- Path-sum leaf test at `t is None` instead of at a leaf: a node with one child can spuriously "complete" a path.
- Forgetting to backtrack (`path.pop()`, or `counts[cum] -= 1`) after the recursive calls.
- Storing a *reference* to the path list in the result instead of a copy (`path[:]`) — the list changes on the next backtrack.
- Maximum path sum: initialise `best` to `float("-inf")`, not `0` — all-negative trees exist.
- LCA in a BST: the split condition needs strict `<`/`>` on *both* sides so equality falls through and stops there — one of `p`, `q` may equal the node.
- `defaultdict` seeded with `{0: 1}` for the prefix-sum trick — forgetting the empty-prefix seed undercounts paths starting at the root.

---

## 6. Building a tree from traversals, and serialisation

Reconstructing a tree from traversals rests on what each traversal reveals: **preorder** reveals the root as its first element; **inorder** reveals which elements are to the root's *left* and which to its *right* (the root splits the inorder list in two). Combine them: take the first preorder element as the root, find it in inorder (the split point), recurse on the left and right portions of both. The same works for postorder + inorder, but the root is read from the **last** postorder element.

**Why preorder alone is not enough** without inorder: it tells you the root but not how many elements belong to the left versus right subtree — that comes precisely from inorder. (Exception: a BST — the ordering itself gives the split without a separate inorder.)

### Python layout: preorder + inorder, index ranges, O(1) lookup

```python
def build_pre_in(pre: list[int], in_: list[int]) -> TreeNode | None:
    pos = {v: i for i, v in enumerate(in_)}       # value -> index in inorder, O(n) once

    def build(p_lo: int, i_lo: int, i_hi: int) -> TreeNode | None:
        if i_lo >= i_hi:                           # half-open range: empty when lo == hi
            return None
        root_val = pre[p_lo]
        k = pos[root_val]                          # split point in inorder
        left_size = k - i_lo
        t = TreeNode(root_val)
        t.left  = build(p_lo + 1,             i_lo,  k)
        t.right = build(p_lo + 1 + left_size, k + 1, i_hi)
        return t

    return build(0, 0, len(in_))
```

Never slice sub-lists (`pre[1:]`) — that copies `O(n)` per call, `O(n^2)` total; pass index bounds into the original lists instead. Never call `in_.index(v)` per node — precompute `pos` first. For inorder + postorder: the root is `post[p_hi - 1]`, and walking postorder backwards you must build the **right** subtree first — the mirror of the preorder case.

### Worked mini-example

`pre = [3, 9, 20, 15, 7]`, `in_ = [9, 3, 15, 20, 7]`.

```
root = pre[0] = 3; pos[3] = 1 in inorder -> left has 1 element (9), right has 3 (15 20 7)
  left:  pre[1..2) = [9],       in[0,1) = [9]          -> leaf 9
  right: pre[2..5) = [20,15,7], in[2,5) = [15,20,7]
         root = 20; pos[20] = 3 -> left size 1
           left:  pre[3] = 15, in[2,3) -> leaf 15
           right: pre[4] = 7,  in[4,5) -> leaf 7
```

Result: the BFS example tree from Unit 3.

### Balanced BST from a sorted array

A sorted list *is* a BST's inorder. Pick the middle element as the root (`mid = (lo + hi) // 2` — no search needed, the split point is the index), recurse on both halves. `O(n)` total, height `O(log n)`. This is the same recursion a BVH or k-d tree build uses on a coordinate-sorted array of primitives.

```python
def sorted_to_bst(a: list[int]) -> TreeNode | None:
    def build(lo: int, hi: int) -> TreeNode | None:
        if lo >= hi:
            return None
        mid = (lo + hi) // 2
        t = TreeNode(a[mid])
        t.left  = build(lo, mid)
        t.right = build(mid + 1, hi)
        return t
    return build(0, len(a))
```

### BST from preorder alone

Carry a shared cursor into the preorder list and an upper bound `hi`: while `pre[idx] < hi`, the next value belongs to the current subtree — values less than the node go left (bound tightens to `t.val`), otherwise the recursion returns and the value goes right. `O(n)` — each element consumed exactly once. The index **must** be shared state; a plain `int` argument would be re-copied per call and branches would re-consume the same elements. Python's fix: a one-element list `idx = [0]`, or a class attribute, or reading from an iterator.

```python
def bst_from_preorder(pre: list[int]) -> TreeNode | None:
    idx = [0]                                      # shared cursor: NOT a plain int argument
    def build(hi: float) -> TreeNode | None:
        if idx[0] == len(pre) or pre[idx[0]] >= hi:
            return None
        val = pre[idx[0]]; idx[0] += 1
        t = TreeNode(val)
        t.left  = build(val)
        t.right = build(hi)
        return t
    return build(float("inf"))
```

### Serialisation: preorder with explicit nulls

```python
def serialize(t: TreeNode | None) -> str:
    out: list[str] = []
    def dfs(t: TreeNode | None) -> None:
        if t is None:
            out.append("#")
            return
        out.append(str(t.val))
        dfs(t.left)
        dfs(t.right)
    dfs(t)
    return " ".join(out)                           # "".join in a loop would be quadratic; join once

def deserialize(data: str) -> TreeNode | None:
    tokens = iter(data.split())                     # shared cursor: an iterator, consumed once
    def build() -> TreeNode | None:
        tok = next(tokens)
        if tok == "#":
            return None
        t = TreeNode(int(tok))
        t.left  = build()                           # same iterator: left subtree consumes its tokens ...
        t.right = build()                           # ... so right starts exactly where left stopped
        return t
    return build()
```

An `iter()` over the split token list is the Python analogue of the C `const char **cursor` — each `next(tokens)` call advances shared state, so left and right subtrees consume disjoint, correctly-ordered slices of the stream without any index bookkeeping. The tree `3 (9) (20 (15) (7))` serialises to `"3 9 # # 20 15 # # 7 # #"`.

### Related: duplicate subtrees and maximum binary tree

- **Find duplicate subtrees**: two subtrees are identical iff their postorder serialisation (values + structure, e.g. `f"{left_id},{right_id},{val}"`) is identical. Postorder fits because children are serialised before the parent, so the subtree's full string is ready when the parent is processed. Count serialisations in a `dict`; when a count hits exactly `2`, record that root.
- **Maximum binary tree**: largest element is the root, split the array there, recurse — `O(n)` average, `O(n^2)` on sorted input. The `O(n)` version uses a **monotonic stack** (Chapter 04): keep it decreasing; when a new value exceeds the top, popped nodes become the new node's left child (chained); the new node becomes the right child of the remaining top.

### Pitfalls

- Slicing sub-lists per call (`pre[1:]`, `in_[:k]`) — works, wastes `O(n)` per level.
- `in_.index(v)` per call instead of a precomputed `pos` dict.
- Mixing up which traversal is read backwards: postorder-based builds construct the right subtree first.
- Deserialising with a re-created iterator per call instead of one shared iterator — the left subtree's consumption would be lost.
- BST-from-preorder with a plain `int` cursor argument instead of a shared mutable cell — the classic bug here.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Complexity |
|---|---|---|
| "depth", "height", "balanced", "diameter", "count nodes", "same/symmetric" | postorder recursion; decide the return value first | `O(n)` time, `O(h)` stack |
| "path from root to leaf", "sum of numbers formed by paths", "good nodes" | preorder DFS carrying state down | `O(n)` |
| "in preorder/inorder/postorder", "flatten", "kth smallest in BST", "sorted" | traversal; iterative with explicit `list`-stack if early exit or `O(h)` space matters | `O(n)`; `O(h + k)` early exit |
| "level", "average per level", "right side view", "zigzag", "minimum depth", "cousins" | BFS with `deque` + `level_size` snapshot | `O(n)` time, `O(w)` queue |
| "BST" + "search / insert / delete / range / LCA" | navigate by comparison, never both children; Transform returns the subtree | `O(h)` |
| "BST" + "min difference / kth / two sum / validate" | inorder = sorted; carry `prev` via `nonlocal` | `O(n)` time, `O(h)` space |
| "validate BST" | `(lo, hi)` window down, or inorder monotonic | `O(n)` |
| "lowest common ancestor" (general tree) | return found-node-or-`None`, both sides non-`None` -> this node | `O(n)` |
| "path may start and end anywhere", "count paths with sum" | prefix-sum `dict` along root path + undo on return | `O(n)` |
| "maximum path sum", "distribute coins" | Shape 3: return one-direction value, update global with both | `O(n)` |
| "distance k", "nodes at distance", "burn/infect tree" | parent `dict` + BFS with visited `set` | `O(n)` |
| "construct from preorder and inorder / inorder and postorder" | root from pre[0]/post[last]; `pos` dict split; index ranges | `O(n)` |
| "sorted array to BST", "balanced" | middle element as root, recurse halves | `O(n)`, height `O(log n)` |
| "serialize", "duplicate subtrees", "encode" | preorder with `#` nulls (shared iterator); postorder strings/IDs + `dict` count | `O(n)` / `O(n^2)` strings |
| "maximum binary tree", "next greater" shape | monotonic stack | `O(n)` |

---

## Gotchas in Python specifically

- **Recursion limit ~1000.** LeetCode trees go to 10^4-10^5 nodes and can be skewed; a plain recursive height/insert/validate raises `RecursionError` on a degenerate tree well before C's 8 MB stack would segfault. Use the iterative stack/queue forms in this chapter for adversarial inputs, or `sys.setrecursionlimit(20000)` as a last resort (still bounded by the real C stack underneath CPython, and it will not save you at 10^6 nodes).
- **`nonlocal` is the out-parameter.** `int *best` becomes `nonlocal best` inside a nested `def`; forgetting it makes `best = ...` create a shadowing local, and the "answer" silently stays at its initial value.
- **Transform functions must be assigned.** `bst_insert(root, 5)` does nothing to the caller's `root` when `root is None` — Python passes references by value, exactly like a C pointer. Always `root = bst_insert(root, 5)`, `t.left = f(t.left)`.
- **No manual free, but watch aliasing, not leaks.** The GC reclaims a removed subtree once nothing references it — the C "did every `malloc` get a `free`" checklist item does not apply. What replaces it: never assign the *same* node object into two places in the result tree unless you mean to alias it (e.g. building a graph, not a tree).
- **`is` vs `==` for node identity.** LCA and "same node" checks must use `t is p`, not `t == p` — `==` falls back to identity for a plain class anyway *unless* you defined `__eq__`, but relying on that is fragile; be explicit.
- **`float("-inf")`/`float("inf")` sentinels**, not `-sys.maxsize`, for BST validation bounds — they can never collide with a real node value the way `INT_MIN`/`INT_MAX` can in C.
- **List slicing copies.** `pre[1:]`, `in_[:k]` look free but are `O(n)` each — pass `(lo, hi)` index pairs into the original list instead, exactly like the C version's index ranges.
- **`list.pop(0)` / `insert(0, x)` are O(n).** Never use a `list` as a BFS queue; `collections.deque` gives O(1) `popleft`/`append`.
- **Shared mutable state for a cursor.** A plain `int` argument (or even module-level `global`) copied into each recursive call breaks BST-from-preorder and the naive deserialiser the same way a copied-not-pointer index breaks the C version. Use a one-element list, an `iter()` you call `next()` on, or `nonlocal`.
- **String building.** `"".join(parts)` once at the end, never `s += tok` in a loop — same rule as Chapter 04.
- **Comparators for `sorted`/`.sort()`.** Never subtract for tie-breaking on large values (`sorted(vals, key=lambda x: x - pivot)` can misbehave with floats/overflow-adjacent logic ported from C); use `key=` directly on the natural value.
- **Returning "arrays".** LeetCode Python signatures just return a `list`; there is no `returnSize` out-parameter and no caller-frees-it convention — one less bookkeeping axis than C, but still return a fresh `list`, not a generator, when the harness expects `List[int]`.

---

## Common mistakes checklist

- [ ] Decided what the function returns *before* writing it — and whether the answer is a `nonlocal` side variable.
- [ ] Base case `t is None` handled first; empty tree height is `0`.
- [ ] "Leaf" checked as `not t.left and not t.right`, never one side.
- [ ] Balance/diameter/max-path computed in a single postorder pass, not by recomputing heights (`O(n^2)`).
- [ ] Iterative preorder pushes right *then* left; iterative inorder processes on *pop*.
- [ ] BFS snapshots `level_size = len(q)` before the inner loop; never enqueues `None`; uses `deque`, not `list.pop(0)`.
- [ ] BST validation carries the whole `(lo, hi)` window, not just parent-child comparison; `float("inf")` sentinels used.
- [ ] Inorder-based BST problems carry `prev`/counter via `nonlocal` instead of materialising the sorted list.
- [ ] Delete case 3 copies the successor value *and* removes the successor node.
- [ ] Backtracking undone after recursion (`path.pop()`, dict count decrement).
- [ ] Prefix-sum dict in Path Sum III seeded with `{0: 1}`.
- [ ] Tree construction: precomputed `pos` dict, index ranges (no slicing), right-first for postorder.
- [ ] Deserialiser and preorder-BST builder use a *shared* iterator/cursor cell, not a plain copied argument.
- [ ] Node identity checks use `is`, not `==`.
- [ ] Recursion depth considered for skewed inputs; iterative fallback available.

---

## You can move on when...

- You can write height, invert, same-tree, symmetric and diameter from memory in under ten minutes total, and explain which return-value shape each uses.
- You can produce all three traversals iteratively, including postorder via reversed N-R-L, and trace the stack on a 6-node tree by hand.
- Your level-order BFS is a reusable skeleton: you can switch it to right-side-view, zigzag or minimum-depth by changing only the per-level body.
- You can state the BST invariant in one sentence that includes the words "entire subtree", and name the two exploitation modes (navigate / inorder = sorted) with two problems for each.
- You can write `bst_delete` with all three cases and verify with an inorder dump.
- You can explain why `maximum_path_sum` returns one thing and records another, and why `max(0, ...)` is there.
- You can rebuild a tree from preorder + inorder with index ranges and a `pos` dict, serialise it with `#` markers, deserialise it with a shared iterator, and get back an identical tree.
- You have solved the level-2 and level-3 problems of every unit in `problems.md` in Python, with your own `assert` tests.
