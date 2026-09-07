# Chapter 05 — Trees & BSTs

## What you'll be able to do after this chapter

- Design a tree recursion by answering one question first — *what does the parent need from its child?* — and recognise the three return-value shapes (state flowing down, result flowing up, both via a side variable).
- Write preorder / inorder / postorder traversals both recursively and with an explicit stack, and pick the order from where on the root-to-leaf route you need the node's value.
- Run BFS on a tree with the `levelSize` counter and solve every "per level" question (averages, right side view, zigzag, nearest leaf) from the same skeleton.
- State the BST invariant precisely (whole subtree, not just children), exploit it for `O(h)` navigation and for "inorder = sorted", and validate it with a `(min, max)` window.
- Solve lowest-common-ancestor and path-sum problems, including the prefix-sum-hash-table trick for paths that start and end anywhere.
- Rebuild a tree from traversal pairs and serialise/deserialise one with explicit null markers — all with `malloc`ed nodes that you also free.

## Why this matters for ML / numerics / sims

Trees are the data structure you build when the problem has hierarchy or a divide-and-conquer split. In numerics and simulation they show up constantly: a **k-d tree** or **octree** partitions space so k-NN, range queries and Barnes-Hut gravity run in `O(log n)` per query instead of `O(n)` — and every operation on it is a "which subtree do I recurse into?" decision exactly like a BST search. A **Huffman tree** or a **decision tree / random forest** is a binary tree where inference is a root-to-leaf walk carrying state downward (Unit 1's "state flows down" shape). An **expression tree** in an autograd engine is evaluated postorder (children before parent — Unit 2), and backprop is the same tree walked in the opposite direction. A **BVH** (bounding volume hierarchy) in a collision/ray sim is rebuilt from a sorted array the same way Unit 6 rebuilds a balanced BST from a sorted array. **Level-order BFS** is how you compute the depth of each node in a hierarchical clustering dendrogram or lay out a tree for plotting. And the discipline of this chapter — "what must this function return so its caller can make its decision?" — is the discipline of writing any recursive kernel correctly.

The C side: a node is a `struct` with two pointers (Chapter 07), allocated with `malloc` (Chapter 06), and the "tree" is a pointer to the root. You already built a BST in `../../c_learning/10_data_structures/lesson.md`; this chapter turns that into a problem-solving toolkit. There is no `None`, no garbage collector, no `deque` — you write the queue, you free the nodes, you watch the recursion depth.

---

## 0. The node, and the two shapes of a tree function

Every problem in this chapter uses this node (LeetCode's `struct TreeNode`):

```c
struct TreeNode {
    int val;
    struct TreeNode *left;
    struct TreeNode *right;
};
typedef struct TreeNode TreeNode;

static TreeNode *node_new(int v) {
    TreeNode *n = malloc(sizeof *n);
    if (!n) { perror("malloc"); exit(1); }
    n->val = v; n->left = n->right = NULL;
    return n;
}
static void tree_free(TreeNode *t) {          /* postorder: free children, then self */
    if (!t) return;
    tree_free(t->left); tree_free(t->right); free(t);
}
```

Memory layout: nodes are scattered on the heap, each 16 bytes of payload+padding plus two 8-byte pointers (24 bytes total on a 64-bit machine, typically). A tree of `n` nodes is `n` separate allocations. This is the opposite of a dynamic array — cache-hostile, allocation-heavy — and it is why array-backed heaps (Chapter 10) beat pointer trees for priority queues. Trees earn their pointers when the *shape* carries information.

Two conventions you will use over and over:

| Shape | Signature | Meaning | Examples |
|---|---|---|---|
| Query | `int f(TreeNode *t, ...)` | compute something about the subtree, don't change it | height, path sum, validate |
| Transform | `TreeNode *f(TreeNode *t, ...)` | return the (possibly new) root of the modified subtree; the caller assigns it: `t->left = f(t->left)` | invert, insert, delete, build |

The transform shape is the C way to "modify a pointer the caller owns" without passing `TreeNode **`. Insert into an empty subtree returns a fresh node; the parent stores it. Delete returns the replacement (maybe `NULL`); the parent stores it. Learn the idiom once and half the chapter is written.

**Python equivalent:** `None` is `NULL`; a `TreeNode` class with `.left/.right` is the struct. The difference is ownership: in Python the tree is garbage-collected; here `tree_free` is your job and a `Transform`-shaped delete must `free` the removed node exactly once.

---

## 1. Recursion on trees: what does the function return?

The basic form of tree recursion is always the same: the function receives a node, calls itself on the children, and combines the children's answers into its own answer. All the difficulty lies in **what the function returns** — that decision solves the whole problem.

### Three typical return values

1. **State flowing down** (parameter). The information is about the path from the root: "is this branch within the allowed `(min, max)` range?", "what is the largest value seen so far on the way here?", "what is the remaining target sum?". The parent computes a new value and *passes it as an argument*; the child never needs to hand anything back except maybe a boolean.
2. **Combined result flowing up** (return value). Height, size, sum, "is identical to that other tree". The children's answers are computed *first*, and combined only on the way back — this is a postorder computation whether you call it that or not.
3. **Both at once via a side variable.** The answer the problem asks for is *not* the same thing the function must return to its parent. Diameter of a tree: every node updates a global (or by-pointer) `best` as a side effect, but still returns the *height*, because height is what the parent needs to combine its own candidate.

The **typical mistake** is trying to return two things as one number — e.g. encoding "unbalanced" as a negative height. It works (`balanced-binary-tree`), but only if you recognise that this is exactly the trick you need. Always ask first: *what information does the parent node need from its child in order to make its own decision?* The answer to that question is the function's return value.

### Skeleton

```c
/* Shape 3: returns height (what the parent needs), updates *best (what the problem asks). */
static int height_and_diameter(const TreeNode *t, int *best) {
    if (!t) return 0;                                  /* base case: empty subtree has height 0 */
    int hl = height_and_diameter(t->left,  best);      /* children first ... */
    int hr = height_and_diameter(t->right, best);
    int through_here = hl + hr;                        /* edges on the longest path through t */
    if (through_here > *best) *best = through_here;    /* side effect: the actual answer */
    return 1 + (hl > hr ? hl : hr);                    /* return value: height for the parent */
}
/* Caller:  int best = 0; height_and_diameter(root, &best); */
```

In C the "global" is an out-parameter (`int *best`) or a `static` file-scope variable; prefer the out-parameter — it is reentrant and testable.

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

Postorder evaluation of `height_and_diameter`:

| node | hl | hr | through_here | best after | returns |
|---|---|---|---|---|---|
| 7 | 0 | 0 | 0 | 0 | 1 |
| 4 | 1 | 0 | 1 | 1 | 2 |
| 5 | 0 | 0 | 0 | 1 | 1 |
| 2 | 2 | 1 | 3 | 3 | 3 |
| 6 | 0 | 0 | 0 | 3 | 1 |
| 3 | 0 | 1 | 1 | 3 | 2 |
| 1 | 3 | 2 | **5** | **5** | 4 |

Diameter 5 edges (7-4-2-1-3-6). Note that the return value at the root (height 4) is not the answer.

### Complexity and pitfalls

- Every node is visited once with constant work: `O(n)` time. Stack depth is `O(h)`, worst case `O(n)` on a degenerate (linked-list-shaped) tree, `O(log n)` when balanced.
- Base case first: the height of an empty tree is `0`, not `-1` and not a crash. Check `root == NULL` before trusting recursion.
- "Leaf" means `left == NULL && right == NULL`. A node with one child is *not* a leaf; checking only one side is the most common path-sum bug.
- Comparing `abs(hl - hr) > 1` only at the root is not a balance check — imbalance can hide deeper. Either combine (return `-1` as a sentinel) or you get `O(n^2)` from recomputing heights.
- Short-circuit `&&`/`||` in boolean recursions (`same-tree`) so you stop at the first mismatch.
- When two recursions nest (`subtree-of-another-tree`: walk the big tree, at each node run `is_same`), the cost multiplies: `O(n·m)`.

**Python equivalent:** `nonlocal best` inside a nested helper is the `int *best` out-parameter.

---

## 2. Traversals: preorder, inorder, postorder

The three basic traversals differ only in **when** the node itself is processed relative to its children: preorder processes the node before its children (node, left, right), inorder between them (left, node, right), postorder after them (left, right, node). In a BST, inorder yields the values in ascending order — so useful it deserves its own mnemonic: **"inorder BST = sorted"**.

| Order | Visit sequence | Node is processed... | Use it when |
|---|---|---|---|
| preorder | N L R | on the way down | copying/serialising a tree; carrying a root-to-leaf path (string, number, running max) |
| inorder | L N R | between the subtrees | anything that wants BST values sorted: k-th smallest, min gap, validate |
| postorder | L R N | on the way back up | the parent's answer depends on both children's finished results (height, size, flatten, free) |

### Recursive skeleton (all three are the same six lines with one line moved)

```c
static void inorder(const TreeNode *t, int *out, int *n) {
    if (!t) return;
    inorder(t->left, out, n);
    out[(*n)++] = t->val;            /* move this line up for preorder, down for postorder */
    inorder(t->right, out, n);
}
```

### Iterative versions: the explicit stack shows what recursion hides

The recursive form is straightforward, but the iterative version reveals what recursion does behind the scenes: an explicit stack simulates the call stack. Preorder is the easiest to iterate (push right, then left, process in pop order); postorder is the hardest (usually easiest as *reversed* preorder-with-children-swapped).

```c
/* Iterative inorder. stack[] holds pointers; sp is the stack top. */
static void inorder_iter(TreeNode *root, int *out, int *n) {
    TreeNode *stack[64]; int sp = 0;              /* size >= tree height; use malloc for unknown h */
    TreeNode *cur = root;
    while (cur || sp > 0) {
        while (cur) { stack[sp++] = cur; cur = cur->left; }   /* go as far left as possible */
        cur = stack[--sp];                                      /* leftmost unprocessed node */
        out[(*n)++] = cur->val;                                 /* process */
        cur = cur->right;                                       /* then its right subtree */
    }
}
```

Iterative preorder: push root; loop `pop, process, push right, push left`. The LIFO stack reverses the push order, so pushing left *last* makes it come out *first*.

Iterative postorder: run preorder with the children swapped (N R L), collecting into an array, then reverse the array. Reversed N-R-L is L-R-N, which is postorder. A direct iterative postorder needs to know which child you are returning from — error-prone; use the reversal trick.

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
| N R L (preorder with children swapped) | 4 6 5 2 3 1 |
| N R L reversed | 1 3 2 5 6 4 = postorder |

Re-derive the N-R-L row yourself before trusting it: reading a trace too fast is exactly how the push order gets swapped in code.

Iterative inorder stack trace, first few steps:

```
cur=4: push 4, cur=2: push 2, cur=1: push 1, cur=NULL     stack: [4 2 1]
pop 1, out=[1], cur=1->right=NULL                          stack: [4 2]
pop 2, out=[1 2], cur=2->right=3: push 3, cur=NULL         stack: [4 3]
pop 3, out=[1 2 3], cur=NULL                               stack: [4]
pop 4, out=[1 2 3 4], cur=6: push 6, cur=5: push 5 ...     stack: [6 5]
```

### Carrying state down a traversal

Many problems don't ask for a traversal by name but use the same movement pattern to carry information from root to leaf (a path as a string or number) or to collect results on the way back. You recognise it when the problem talks about "a path from root to leaf" or asks for output in a specific order — then pick the traversal type by *where on the route you need the node's value*.

```c
/* Preorder with state flowing down: sum of root-to-leaf numbers. */
static long sum_numbers(const TreeNode *t, long acc) {
    if (!t) return 0;
    acc = acc * 10 + t->val;                        /* O(1) update, unlike string paths */
    if (!t->left && !t->right) return acc;          /* leaf: this path is complete */
    return sum_numbers(t->left, acc) + sum_numbers(t->right, acc);
}
```

Carrying a *string* path instead costs a copy per step: `O(n)` nodes but `O(n^2)` worst case (skewed tree, long path copied repeatedly). In C the string version means a `char` buffer of size proportional to height, `snprintf` into it at offset `len`, and truncating back (`buf[len] = '\0'`) after the recursive calls — that truncation *is* backtracking.

### Pitfalls

- In iterative preorder, pushing left before right reverses the output — the stack flips the order.
- In iterative inorder, "process node" happens when it is *popped*, not when it is first pushed.
- N-ary trees: children are an array; push them onto the stack in *reverse* order so the first child pops first.
- Flatten-to-linked-list: do N-R-L (reversed preorder) while maintaining `prev`; a normal preorder would overwrite `left` before you had used it to descend.
- Stack size: `TreeNode *stack[64]` is fine for balanced trees up to 2^64 nodes but *not* for a 10^4-node skewed tree — `malloc` a stack of `n` pointers when the input can be degenerate.

**Python equivalent:** `yield from inorder(t.left); yield t.val; yield from inorder(t.right)`. There is no generator in C — write into an output array with a running count, or pass a callback (Chapter 11).

---

## 3. Level by level: BFS on trees

BFS on a tree visits nodes level by level using a queue — exactly the same technique as on graphs, but a tree never needs a "visited" mark because there is exactly one path from the root to each node. Implementation: the queue starts with the root, and on each round you process **the whole current queue size** (`levelSize = queue length`) before moving to the next level — it is precisely this counter that separates "BFS over the whole tree" from "one level at a time".

You recognise a BFS problem when it asks something **per level**: the level's average, the level's last (rightmost visible) node, a zigzag ordering, or the shortest root-to-leaf distance. The last one matters: DFS would find *some* leaf, but BFS finds the *nearest* leaf first, because it visits nodes in distance order. Same principle as shortest path in an unweighted graph (Chapter on graphs).

### The queue in C

There is no `deque`. For a tree of `n` nodes a queue never holds more than `n` pointers, so a plain array with two indices is the simplest correct queue: `q[head..tail)`, no wrap-around needed.

```c
/* Level-order BFS. Processes each level as a unit. */
static void bfs_levels(TreeNode *root, size_t n_nodes) {
    if (!root) return;
    TreeNode **q = malloc(n_nodes * sizeof *q);     /* capacity n is enough: each node enqueued once */
    size_t head = 0, tail = 0;
    q[tail++] = root;
    int depth = 0;
    while (head < tail) {
        size_t level_size = tail - head;            /* snapshot BEFORE the inner loop */
        long sum = 0;
        for (size_t i = 0; i < level_size; i++) {
            TreeNode *t = q[head++];
            sum += t->val;                          /* per-level work: sum / collect / last-node */
            if (t->left)  q[tail++] = t->left;      /* enqueue only existing children */
            if (t->right) q[tail++] = t->right;
        }
        printf("depth %d: %zu nodes, avg %.2f\n", depth++, level_size, (double)sum / level_size);
    }
    free(q);
}
```

If you don't know `n` up front, use a growable array (Chapter 10's `Vec`) for the queue, or a ring buffer — but for LeetCode-style constraints "count nodes first, then allocate" is the least error-prone.

### Worked mini-example

```
        3
      /   \
     9    20
         /  \
        15   7
```

| round | head..tail before | level_size | processed | enqueued | per-level result |
|---|---|---|---|---|---|
| 0 | [3] | 1 | 3 | 9, 20 | avg 3.0; last = 3 |
| 1 | [9 20] | 2 | 9, 20 | 15, 7 | avg 14.5; last = 20 |
| 2 | [15 7] | 2 | 15, 7 | — | avg 11.0; last = 7 |

Right side view = the "last" column: `3 20 7`. Minimum depth: the first leaf encountered is `9` at round 1 → depth 2, stop immediately. Zigzag: reverse the collected list on odd rounds (`[3] [20 9] [15 7]`) — the *queue order never changes*, only how you emit the level.

### Variants from the same skeleton

| Problem | Per-level work |
|---|---|
| Level order (list of lists) | copy the level's values into a fresh array |
| Level order bottom-up | collect normally, reverse the list of levels at the end (`O(k)`), never insert at the front (`O(n)` each) |
| Average of levels | `sum / level_size` |
| Right side view | remember the last node processed in the level |
| Zigzag | reverse the level's array when `depth` is odd |
| Minimum depth | return `depth + 1` on the first node with no children |
| Cousins | for each node also record its parent (a parallel `parent[]` array or a small struct in the queue); same level and different parents → cousins |
| Populate next-right pointers | link `q[head]` to `q[head+1]` while `i + 1 < level_size` |

The recursive alternative (pass `depth`, append to `result[depth]`) works but needs a dynamic array of dynamic arrays and gives no early exit; the queue version is more natural when you specifically want level order.

### Pitfalls

- Using the live queue length as the loop bound (`while (i < tail)`) instead of a snapshot: children get appended mid-loop, the level boundary disappears.
- Enqueuing `NULL` children. Check before pushing — or every dequeue needs a `NULL` test and `level_size` counts phantom nodes.
- A node with one child is not a leaf; minimum depth must not stop there.
- Taking the *first* node of a level gives the left view, not the right.
- Trying to zigzag by pushing children in alternating order breaks the next level's queue order. Reverse the emitted level only.
- The `O(1)`-extra-space next-right trick (walk level `k` via its `next` links to link level `k+1`) relies on a *perfect* tree; with missing children you need a "next existing node" search.

**Python equivalent:** `collections.deque`; `for _ in range(len(q)):` is the `level_size` snapshot.

---

## 4. The BST invariant and how to exploit it

A binary search tree's invariant: for every node, the **entire left subtree** contains only smaller values and the **entire right subtree** only larger values — not just the immediate children, the whole subtree. This one property is the source of all BST efficiency: at every node it gives a binary-search decision ("go left or go right"), which brings search, insert and delete down to `O(h)` — `O(log n)` in a balanced tree.

### Two ways to exploit the invariant

1. **Navigation.** Because every node tells you exactly which direction the sought value lies in, you never examine both children — unlike a general tree. Search, insert, delete, LCA in a BST, range pruning: all `O(h)`.
2. **Inorder order.** Because inorder visits left-node-right and the invariant guarantees left < node < right, inorder yields the values in ascending order — **always**. This turns many apparent tree problems into sorted-array problems: k-th smallest, minimum gap between any two values, two-sum, validate-by-monotonicity.

### Skeletons

```c
/* Navigation: iterative search, O(h), no stack. */
static TreeNode *bst_search(TreeNode *t, int key) {
    while (t && t->val != key) t = (key < t->val) ? t->left : t->right;
    return t;                                  /* NULL if absent */
}

/* Transform shape: insert, returns the subtree root. Parent stores the result. */
static TreeNode *bst_insert(TreeNode *t, int key) {
    if (!t) return node_new(key);              /* reached the empty slot: attach here */
    if (key < t->val)      t->left  = bst_insert(t->left,  key);
    else if (key > t->val) t->right = bst_insert(t->right, key);
    return t;                                  /* duplicates: ignored */
}

/* Validation: carry the allowed open interval down. Use pointers (or long) so that
   INT_MIN / INT_MAX can appear as node values without breaking the bounds. */
static bool bst_valid(const TreeNode *t, const int *lo, const int *hi) {
    if (!t) return true;
    if ((lo && t->val <= *lo) || (hi && t->val >= *hi)) return false;
    return bst_valid(t->left, lo, &t->val) && bst_valid(t->right, &t->val, hi);
}
/* Caller: bst_valid(root, NULL, NULL) */
```

**Validation** is the subtle spot: checking only `left->val < val < right->val` is not enough, because the constraint applies to the whole subtree. So validation either carries the allowed `(min, max)` window downward — going left tightens the upper bound to the current value, going right tightens the lower bound — or does an inorder traversal and checks strict monotonicity against the previous value.

### Delete: the three cases

Navigate to the node as in search, then:

1. **Leaf** — free it, return `NULL`.
2. **One child** — free the node, return that child (the parent splices it in).
3. **Two children** — find the **inorder successor** (the leftmost node of the right subtree), copy its value into this node, then recursively delete the successor's value from the right subtree. The successor is greater than the whole left subtree and smaller than the rest of the right subtree — exactly the position the invariant demands. (The inorder predecessor — rightmost of the left subtree — works symmetrically.)

Copying the value but forgetting to remove the original successor leaves the value in the tree twice. In C, case 3 done as "copy value, recurse" frees exactly one node, which is what you want; do not `free(t)` in case 3.

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
| 5 | `5 < 7` | whole left subtree (3) is too small → skip it; recurse right only | 10 |
| 7 | inside | add; both children NULL | 17 |
| 15 | inside | add; recurse both | 32 |
| 18 | `18 > 15` | whole right subtree too large → skip; recurse left only (NULL) | 32 |

Node `3` was never visited. Cost `O(h + k)` where `k` is the number of in-range nodes, not `O(n)`.

### Pitfalls

- Validating with `INT_MIN`/`INT_MAX` as sentinels fails when node values can *be* `INT_MIN`/`INT_MAX`. Use `NULL` pointers (above) or `long` bounds.
- Inorder-based problems (min gap, k-th smallest) do not need the whole sorted array in memory — carry `prev` or a counter through the traversal: `O(h)` space instead of `O(n)`.
- k-th smallest with an early-exit iterative inorder is `O(h + k)`: `h` steps left to the minimum, then `k` pops.
- Two Sum on a BST via a hash set must not match a node with itself (`target == 2*val`): insert into the set *after* checking.
- The BST in these problems is not self-balancing; a sorted insertion sequence gives `h = n`, so recursive insert can blow the stack on large inputs — use the iterative form there.
- Forgetting to prune in range-sum is not a correctness bug, it is a "you threw away the only advantage the BST gave you" bug.

**Python equivalent:** `bisect` on a sorted list is the array analogue of BST navigation; `sortedcontainers.SortedList` is what a balanced BST gives you.

---

## 5. Lowest common ancestor and path problems

### LCA in a general binary tree

The LCA recursion rests on one observation: node `x` is the LCA of `p` and `q` exactly when `p` and `q` are found in **different** subtrees among `x`'s children (or `x` itself is one of them). In a general binary tree the function returns "did I find `p` or `q` in this subtree — and if both were found in different branches, this node is the LCA". The return value thus carries both the search result and the answer in the same variable — Unit 1's Shape 2 with a pointer as the return type.

```c
/* Returns: NULL if neither p nor q is in subtree t; otherwise p, q, or their LCA. */
static TreeNode *lca(TreeNode *t, const TreeNode *p, const TreeNode *q) {
    if (!t || t == p || t == q) return t;            /* found one (or nothing) */
    TreeNode *l = lca(t->left,  p, q);
    TreeNode *r = lca(t->right, p, q);
    if (l && r) return t;                            /* one on each side: t is the LCA */
    return l ? l : r;                                /* pass the single find upward */
}
```

If `p` is an ancestor of `q`, the recursion stops at `p` (`t == p` returns immediately) and `p` is correctly the LCA — the algorithm handles it *because* a node can be "its own find". `O(n)` time.

### LCA in a BST

The same problem is solved without walking the whole tree: the invariant tells you which direction each value lies in, so the LCA is the first node where `p` and `q` "split" (one is smaller, the other greater than or equal to the node) — `O(h)` instead of `O(n)`, iterative, no stack:

```c
while ((p < t->val && q < t->val) || (p > t->val && q > t->val))
    t = (p < t->val) ? t->left : t->right;
/* t is the LCA */
```

Using the general algorithm on a BST works but wastes the invariant.

### Path problems: where can the path start and end?

The key question in the path-sum family: is the path confined to root-to-leaf, or can it start and end at any node?

| Path type | Technique | Complexity |
|---|---|---|
| root → leaf, existence | DFS carrying `remaining = target - val`; check `remaining == 0` at a *leaf* | `O(n)` |
| root → leaf, list all | same, plus a path buffer with backtracking (`path[len++] = val; ... len--`); *copy* the buffer into the result at a leaf | `O(n)` + `O(L)` per hit |
| any node ↓ any descendant, count | prefix-sum hash map along the current root path: count of `cum - target` seen so far; **undo** the map entry on return | `O(n)` |
| any node → any node (may bend at a node), max sum | Shape 3: return the best *single-direction* extension `val + max(0, l, r)`; update global best with `val + max(0,l) + max(0,r)` | `O(n)` |

The prefix-sum trick is the array "subarray sum equals k" trick lifted onto a tree: the root-to-current path *is* an array, and the recursion stack is the iteration over it. In C the hash map is your own open-addressing `long → int` count table (Chapter 10), and "undo" means decrementing the count after both recursive calls return. Sibling branches must not see each other's sums.

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

Answer 42 (15-20-7). The root returns 25 — not the answer — because a path through the root's *parent* could only use one of its branches. Negative branches are cut with `max(0, ·)`: a bad branch is better left out entirely.

### Distribute coins / "excess flows up"

Another Shape 3 problem: each subtree returns its coin *excess* (`coins - 1 + excess_left + excess_right`, positive = surplus, negative = deficit); the global move count increases by `|excess_left| + |excess_right|` at each node, because every coin crossing an edge crosses it exactly once. Same skeleton as diameter.

### Distance-k: turn the tree into a graph

A tree only lets you move downward. For "all nodes at distance k from a target" you need to move up too: first a DFS that records each node's parent (a hash map `TreeNode* → TreeNode*`, or an `int` index map), then an ordinary BFS from the target treating every node as having three neighbours (left, right, parent), stopping at depth `k`. Here a visited set **is** mandatory — the parent edge creates a cycle — and the start node must be marked visited before the BFS begins.

### Pitfalls

- Path-sum leaf test at `NULL` instead of at a leaf: a node with one child can spuriously "complete" a path.
- Forgetting to backtrack (`len--`, or decrement the hash count) after the recursive calls — the classic bug in Path Sum II and III.
- Storing a *pointer* to the path buffer in the result instead of a copy — the buffer changes on the next backtrack.
- Maximum path sum: initialise `best` to `INT_MIN` (or the root's value), not `0` — all-negative trees exist.
- LCA in a BST: the split condition uses `<=`/`>=` — one of `p`, `q` may equal the node.

---

## 6. Building a tree from traversals, and serialisation

Reconstructing a tree from traversals rests on what each traversal reveals: **preorder** reveals the root as its first element; **inorder** reveals which elements are to the root's *left* and which to its *right* (the root splits the inorder list in two). Combine them: take the first preorder element as the root, find it in inorder (the split point), and recurse on the left and right portions of both arrays simultaneously. The same works for postorder + inorder, but the root is read from the **last** postorder element.

**Why preorder alone (or postorder alone) is not enough** without inorder: they tell you the root but not how many elements belong to the left versus the right subtree — that information comes precisely from inorder. (Exception: if the tree is a BST, the ordering itself gives the split without a separate inorder — `construct-binary-search-tree-from-preorder-traversal`.)

### Skeleton: preorder + inorder, index ranges, O(1) lookup

```c
/* Builds the subtree whose preorder is pre[p_lo..] and whose inorder is in[i_lo, i_hi).
   pos[] maps value -> index in inorder (precomputed; a hash table if values are not small ints).
   Returns the root; the recursion consumes exactly (i_hi - i_lo) preorder elements. */
static TreeNode *build_pre_in(const int *pre, int p_lo, const int *in, int i_lo, int i_hi, const int *pos) {
    if (i_lo >= i_hi) return NULL;                 /* half-open range: empty when lo == hi */
    int root_val = pre[p_lo];
    int k = pos[root_val];                          /* split point in inorder */
    int left_size = k - i_lo;
    TreeNode *t = node_new(root_val);
    t->left  = build_pre_in(pre, p_lo + 1,             in, i_lo,  k,    pos);
    t->right = build_pre_in(pre, p_lo + 1 + left_size, in, k + 1, i_hi, pos);
    return t;
}
```

Never copy sub-arrays — pass index bounds into the original arrays. Never call a linear `index_of` per node — precompute `pos` (`O(n^2)` → `O(n)`). For inorder + postorder: the root is `post[p_hi - 1]`, and walking postorder backwards you must build the **right** subtree first — the mirror of the preorder case.

### Worked mini-example

`pre = [3, 9, 20, 15, 7]`, `in = [9, 3, 15, 20, 7]`.

```
root = pre[0] = 3; pos[3] = 1 in inorder → left has 1 element (9), right has 3 (15 20 7)
  left:  pre[1..2) = [9],       in[0,1) = [9]          → leaf 9
  right: pre[2..5) = [20,15,7], in[2,5) = [15,20,7]
         root = 20; pos[20] = 3 → left size 1
           left:  pre[3] = 15, in[2,3) → leaf 15
           right: pre[4] = 7,  in[4,5) → leaf 7
```

Result: the BFS example tree from Unit 3.

### Balanced BST from a sorted array

A sorted array *is* a BST's inorder. Pick the middle element as the root (the split point is known from the index — no search), recurse on both halves. Choosing the middle guarantees both halves are about equal, so the height is `O(log n)`. `O(n)` total. For even-length ranges either middle is valid; the problem accepts several shapes. This is the same recursion a BVH or k-d tree build uses on a coordinate-sorted array of primitives.

### BST from preorder alone

The BST exception: build with a bound. Carry a shared index into the preorder array and an upper bound `hi`: while `pre[idx] < hi`, the next value belongs to the current subtree — values less than the node go left (bound tightens to `node->val`), otherwise the recursion returns and the value goes right. Each element is consumed exactly once, `O(n)`. The index **must** be shared state (`int *idx`), not a copied value — otherwise branches re-consume the same elements.

### Serialisation: preorder with explicit nulls

Serialisation is the same idea reversed: encode the tree as a string in a way that preserves enough structure to decode — most commonly preorder + explicit `null` markers, so that one traversal is enough for unambiguous decoding without a second list.

```c
/* Serialise: preorder, "#" for NULL. Decoding reads the same stream in the same order. */
static void serialize(const TreeNode *t, char *buf, size_t cap, size_t *len) {
    if (!t) { *len += (size_t)snprintf(buf + *len, cap - *len, "# "); return; }
    *len += (size_t)snprintf(buf + *len, cap - *len, "%d ", t->val);
    serialize(t->left,  buf, cap, len);
    serialize(t->right, buf, cap, len);
}
/* Deserialise: *cursor walks the string; shared across the whole recursion. */
static TreeNode *deserialize(const char **cursor) {
    while (**cursor == ' ') (*cursor)++;
    if (**cursor == '#') { (*cursor)++; return NULL; }
    char *end; long v = strtol(*cursor, &end, 10); *cursor = end;
    TreeNode *t = node_new((int)v);
    t->left  = deserialize(cursor);          /* same cursor: left subtree consumes its tokens ... */
    t->right = deserialize(cursor);          /* ... so right starts exactly where left stopped */
    return t;
}
```

The tree `3 (9) (20 (15) (7))` serialises to `3 9 # # 20 15 # # 7 # # `. Every node contributes exactly one token and every missing child exactly one `#`, so the decoder always knows whether the next token is a child or a gap: `2n + 1` tokens, `O(n)` both ways.

### Related: duplicate subtrees and maximum binary tree

- **Find duplicate subtrees**: two subtrees are identical iff their postorder serialisation (values + structure, e.g. `"left,right,val"`) is identical. Postorder fits because children are serialised before the parent, so the subtree's full string is available when the parent is processed. Count serialisations in a hash table; when a count hits exactly 2, record that root. Strings cost `O(n)` per node → `O(n^2)`; assigning integer IDs to `(left_id, right_id, val)` triples brings it to `O(n)`.
- **Maximum binary tree**: largest element is the root, split the array there, recurse — `O(n)` average, `O(n^2)` on sorted input. The `O(n)` version uses a **monotonic stack**: keep the stack decreasing; when a new value exceeds the top, popped nodes become the new node's left child (chained); the new node becomes the right child of the remaining top. This is a bridge to the monotonic-stack chapter.

### Pitfalls

- Copying sub-arrays per call (works, wastes `O(n)` per level).
- Linear `index_of` per call instead of a precomputed position map.
- Mixing up which traversal is read backwards: postorder-based builds construct the right subtree first.
- Deserialising with a copied index instead of a shared cursor — the left subtree's consumption is lost and the right subtree starts at the wrong token.
- Serialisation buffer size: each `int` is up to 11 chars plus a separator, plus `n+1` markers — size the buffer as `n * 12 + (n + 1) * 2 + 1`, or grow it.

---

## Pattern recognition cheatsheet

| Signal words in the problem | Pattern | Complexity |
|---|---|---|
| "depth", "height", "balanced", "diameter", "count nodes", "same/symmetric" | postorder recursion; decide the return value first | `O(n)` time, `O(h)` stack |
| "path from root to leaf", "sum of numbers formed by paths", "good nodes (≥ all ancestors)" | preorder DFS carrying state down (remaining sum, accumulated number, running max) | `O(n)` |
| "in preorder/inorder/postorder", "flatten", "kth smallest in BST", "sorted" | traversal; iterative with explicit stack if early exit or `O(h)` space matters | `O(n)`; `O(h + k)` with early exit |
| "level", "average per level", "right side view", "zigzag", "minimum depth", "cousins", "next right pointer" | BFS with `level_size` snapshot | `O(n)` time, `O(w)` queue (max width) |
| "BST" + "search / insert / delete / range / LCA" | navigate by comparison, never both children; Transform shape returns the subtree | `O(h)` |
| "BST" + "min difference / kth / two sum / validate" | inorder = sorted; carry `prev` or a counter | `O(n)` time, `O(h)` space |
| "validate BST" | `(lo, hi)` window down, or inorder monotonic | `O(n)` |
| "lowest common ancestor" (general tree) | return found-node-or-NULL, both sides non-NULL → this node | `O(n)` |
| "path may start and end anywhere", "count paths with sum" | prefix-sum hash map along root path + undo on return | `O(n)` |
| "maximum path sum", "distribute coins" | Shape 3: return one-direction value, update global with both directions | `O(n)` |
| "distance k", "nodes at distance", "burn/infect tree" | parent map + BFS with visited set | `O(n)` |
| "construct from preorder and inorder / inorder and postorder" | root from pre[0] / post[last]; split inorder via precomputed position map; index ranges | `O(n)` |
| "sorted array to BST", "balanced" | middle element as root, recurse halves | `O(n)`, height `O(log n)` |
| "serialize", "duplicate subtrees", "encode" | preorder with `#` nulls (shared cursor); postorder strings/IDs + hash count | `O(n)` / `O(n^2)` strings |
| "maximum binary tree", "next greater" shape | monotonic stack | `O(n)` |

---

## Gotchas in C specifically

- **Recursion depth is real.** LeetCode trees go to 10^4–10^5 nodes and can be skewed; the default main-thread stack is 8 MB on macOS/Linux, roughly 10^5 frames of a small function. `O(h)` recursion is fine for balanced trees; for adversarial inputs use the iterative stack/queue versions or `malloc` an explicit stack sized `n`. See `../../c_learning/13_debugging_testing_perf/lesson.md` for reading a stack-overflow segfault.
- **No `deque`, no `set`, no `dict`.** The BFS queue is `TreeNode **q` with `head/tail` indices (capacity `n`, no wrap needed). The hash maps in Path Sum III, distance-k and tree construction are your own tables from `../../c_learning/10_data_structures/lesson.md`; when values are small non-negative ints (`0..n`), a plain `int pos[n]` array is a faster "hash map".
- **`NULL` is the empty tree.** Every function's first line is `if (!t) return <base>;`. Dereferencing `t->left->val` without checking `t->left` is the segfault you will write most often.
- **Transform functions must be assigned.** `bst_insert(root, 5);` silently does nothing when `root == NULL`. Always `root = bst_insert(root, 5);`, `t->left = f(t->left)`.
- **Free what you remove.** Delete in a BST frees exactly one node; flatten/invert free nothing; every test in `main` ends with `tree_free(root)`. Run under `-fsanitize=address` (Chapter 13) — leaks and use-after-free in tree code are silent otherwise.
- **Out-parameters replace `nonlocal`.** `int *best`, `int *count`, `const char **cursor`, `int *idx`. Passing `int idx` by value in the preorder-BST build or the deserialiser is *the* bug in those problems.
- **Sentinel bounds overflow.** Validate-BST with `int lo = INT_MIN` fails when a node holds `INT_MIN`. Use `long` bounds or `const int *` with `NULL` = unbounded.
- **Sums overflow `int`.** Path sums and root-to-leaf numbers: use `long`/`long long` for accumulators.
- **Returning arrays.** LeetCode C signatures return `int *` plus `int *returnSize` (and `int **returnColumnSizes` for lists of lists) — you `malloc` the result and the harness frees it. Count first (or grow a `Vec`), then fill.
- **Comparisons for `qsort`.** If you sort values extracted from a tree, the comparator is `(a > b) - (a < b)`, never `a - b` (overflow).
- **`snprintf` into a path buffer** returns the number of characters it *would* write; check against remaining capacity before advancing `len`.

---

## Common mistakes checklist

- [ ] Decided what the function returns *before* writing it — and whether the answer is a side variable.
- [ ] Base case `t == NULL` handled first; empty tree height is `0`.
- [ ] "Leaf" checked as `!left && !right`, never one side.
- [ ] Balance/diameter/max-path computed in a single postorder pass, not by recomputing heights (`O(n^2)`).
- [ ] Iterative preorder pushes right *then* left; iterative inorder processes on *pop*.
- [ ] BFS snapshots `level_size` before the inner loop; never enqueues `NULL`.
- [ ] BST validation carries the whole `(lo, hi)` window, not just parent-child comparison; sentinels don't collide with legal values.
- [ ] Inorder-based BST problems carry `prev`/counter instead of materialising the sorted array.
- [ ] Delete case 3 copies the successor value *and* removes the successor node.
- [ ] Backtracking undone after recursion (path length, hash count).
- [ ] Prefix-sum hash map in Path Sum III seeded with `{0: 1}`.
- [ ] Tree construction: precomputed position map, index ranges, right-first for postorder.
- [ ] Deserialiser and preorder-BST builder use a *shared* cursor/index (pointer), not a copied one.
- [ ] Every `malloc`ed node is freed exactly once; tests run clean under ASan.
- [ ] Recursion depth considered for skewed inputs; iterative fallback available.

---

## You can move on when...

- You can write height, invert, same-tree, symmetric and diameter from memory in under ten minutes total, and explain which return-value shape each uses.
- You can produce all three traversals iteratively, including postorder via reversed N-R-L, and trace the stack on a 6-node tree by hand.
- Your level-order BFS is a reusable skeleton: you can switch it to right-side-view, zigzag or minimum-depth by changing only the per-level body.
- You can state the BST invariant in one sentence that includes the words "entire subtree", and you can name the two exploitation modes (navigate / inorder = sorted) and give two problems for each.
- You can write `bst_delete` with all three cases, free exactly one node, and verify with an inorder dump plus ASan.
- You can explain why `maximum-path-sum` returns one thing and records another, and why `max(0, ·)` is there.
- You can rebuild a tree from preorder + inorder with index ranges and a position map, serialise it with `#` markers, deserialise it with a shared cursor, and get back an identical tree (checked with `same_tree`).
- `example.c` compiles with zero warnings and you have modified it — added a traversal variant, a BST delete case test, or a zigzag — without breaking the others.
