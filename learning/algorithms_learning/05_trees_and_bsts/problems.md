# Chapter 05 — Problems

How to work these:

1. Read the matching unit section in `lesson.md` first. Do not read the hints yet.
2. Create `algorithms_learning/05_trees_and_bsts/solutions/` and put each attempt in `solutions/<slug>.c`. Use the `TreeNode` struct from the lesson, build your own test trees with `node_new`, and write your own tests in `main()` — including an empty tree, a single node, a skewed (linked-list-shaped) tree, and a tree where a node has exactly one child. Free every tree; run under `-fsanitize=address` at least once.
3. Only after a real attempt open **Hint**. Only after solving (or 30 minutes genuinely stuck) open **Key idea & complexity**, then **Approach**.
4. The Approach is prose, not code. If you read it, close it, and write the code yourself from memory.

Compile line: `cc -Wall -Wextra -std=c11 -O2 -fsanitize=address -g -o sol solutions/<slug>.c -lm`

---

## Unit 1 — Recursion on trees: what does the function return?

### 05.1.1  Maximum Depth of Binary Tree  ·  LC #104  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/maximum-depth-of-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
What does a child tell its parent about its own height?
</details>
<details><summary>Key idea & complexity</summary>
DFS, return `1 + max(left, right)`, `O(n)` time, `O(h)` stack.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The function returns the height of the subtree: an empty node returns 0, any other node returns `1 + max(left, right)`. This is the simplest form of tree recursion — the return value *is* directly the quantity asked for; no extra state is needed.

Time `O(n)`, since every node is processed once. Stack depth is `O(h)` where `h` is the tree height — `O(n)` at worst in a skewed tree, `O(log n)` on average in a balanced one.

**Pitfall:** the height of an empty tree (`root == NULL`) is 0, not -1 and not an error — check the base case separately before trusting the recursion.
</details>

### 05.1.2  Invert Binary Tree  ·  LC #226  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/invert-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
Swap the node's children and let the recursion handle the rest.
</details>
<details><summary>Key idea & complexity</summary>
DFS, swap children and recurse, `O(n)` time, `O(h)` stack.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Here the return value is the *inverted subtree itself*, not a combined number. Base case: an empty node is returned as is. Otherwise the inversion is first done recursively on the children, and then the node's own child pointers are swapped (or in the opposite order — order does not matter, since both children get inverted either way).

`O(n)` time, since every node is processed once; `O(h)` stack from the recursion.

**Pitfall:** if you swap the children *before* the recursive calls and then use the already-updated pointers, you may recurse into the wrong child twice — safest is to save the original left/right into variables before swapping, or to swap only after both recursive calls have returned.
</details>

### 05.1.3  Same Tree  ·  LC #100  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/same-tree/>
**Level:** 2
<details><summary>Hint</summary>
Compare two trees simultaneously, one node at a time, and return a boolean.
</details>
<details><summary>Key idea & complexity</summary>
Parallel DFS on two trees, `O(min(n1, n2))` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The function takes two nodes and returns a boolean "are these subtrees identical". Base cases: if both are `NULL`, true; if only one is `NULL` or the values differ, false. Otherwise the answer is equality of the left subtrees **AND** equality of the right subtrees.

This is the first example of a return value being a boolean rather than a number — still the same structure: combine the children's answers with a logical operator.

Time `O(min(n1, n2))`, because the recursion stops as soon as structure or a value differs. Remember short-circuiting `&&` so the second tree is not walked needlessly once a difference is found.
</details>

### 05.1.4  Symmetric Tree  ·  LC #101  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/symmetric-tree/>
**Level:** 2
<details><summary>Hint</summary>
Symmetry is not the same as comparing two identical subtrees — compare the mirror image.
</details>
<details><summary>Key idea & complexity</summary>
Recurse on mirror pairs (left.left vs right.right), `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Symmetry is not `isSameTree(left, right)`, because a mirror image reverses the order: the right subtree must be the **mirror image** of the left, not a copy. A helper takes two nodes and returns whether they are mirror images of each other: values equal, and `t1.left` mirrors `t2.right` and `t1.right` mirrors `t2.left`.

The initial call is `isMirror(root.left, root.right)`. This is a good example of sometimes needing a helper with two parameters, because the original question ("is the tree symmetric") does not bend directly into a one-node recursion.

`O(n)` time, `O(h)` stack. **Pitfall:** forgetting that both nodes must be `NULL` at the same time or both present — a bare value comparison without `NULL` checks crashes.
</details>

### 05.1.5  Path Sum  ·  LC #112  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/path-sum/>
**Level:** 2
<details><summary>Hint</summary>
Subtract the node's value from the target and check at a leaf whether it hits zero.
</details>
<details><summary>Key idea & complexity</summary>
DFS carrying the remaining sum, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The return value is a boolean: "is there a root-to-leaf path whose sum is `targetSum`". Carry the remaining target as a parameter downward: at every node subtract the node's value from the target. At a leaf (both children `NULL`) check whether the remaining target is exactly zero.

At an internal node the answer is `path(left, new) || path(right, new)` — it suffices that either branch works.

`O(n)` time at worst, `O(h)` stack. **Pitfall:** an empty node (not a leaf — `NULL`) is not a valid end of a path — the check must be done *at a leaf*, not at a `NULL` node, otherwise a zero sum can match at the wrong place (e.g. a node that has only one child).
</details>

### 05.1.6  Balanced Binary Tree  ·  LC #110  ·  Easy  ·  tree, depth-first-search, binary-tree
<https://leetcode.com/problems/balanced-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
Combine the height computation and the balance check in the same recursion by encoding failure as a special value.
</details>
<details><summary>Key idea & complexity</summary>
DFS that returns the height OR `-1` as an imbalance marker, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A naive solution would compute every node's height separately and check balance in a second call, producing `O(n^2)` at worst. The efficient solution combines both into one return value: the function returns the subtree height normally, but as soon as some subtree is unbalanced (or a child already returned imbalance), it returns the special value `-1`, and this propagates straight to the root without extra work.

This is exactly the point where the choice of return value decides efficiency: one number carries two pieces of information ("height" and "is it already broken").

`O(n)` time, since every node is processed once. **Pitfall:** comparing `abs(left - right) > 1` only at the root — that is not enough, because imbalance can hide deeper in the tree even when the root itself looks balanced.
</details>

### 05.1.7  Diameter of Binary Tree  ·  LC #543  ·  Easy  ·  tree, depth-first-search, binary-tree, dp-on-trees
<https://leetcode.com/problems/diameter-of-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
The diameter is not what the function returns upward — it is a side effect that gets updated at every node.
</details>
<details><summary>Key idea & complexity</summary>
DFS, update a global maximum as a side effect, return the height, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the classic example of the asked-for answer and the recursion's return value not being the same thing. The diameter through any node is `left_height + right_height` (counted in edges), and this must be checked at **every** node, because the best route does not necessarily pass through the root.

Solution: a side variable `best` is updated at every node with the computed sum, but the function returns to its parent only the height (`1 + max(left, right)`), because the parent does not need to know the subtree's diameter — it only needs the height to combine its own diameter.

`O(n)` time, one pass. **Pitfall:** counting the diameter in nodes rather than edges, or forgetting to update `best` at leaves (where both heights are 0, but it is still a valid candidate).
</details>

### 05.1.8  Subtree of Another Tree  ·  LC #572  ·  Easy  ·  tree, depth-first-search, string-matching, binary-tree
<https://leetcode.com/problems/subtree-of-another-tree/>
**Level:** 2
<details><summary>Hint</summary>
Combine two recursions: walk the big tree and at every node check whether the subtrees match exactly.
</details>
<details><summary>Key idea & complexity</summary>
DFS that calls `isSameTree` at every node, `O(n · m)` at worst.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The solution builds directly on the `same-tree` problem: walk the big tree with DFS, and at every node check whether the subtree *starting at this node* is identical to the tree being searched for (`isSameTree`). If yes at any node, the answer is true.

This is a good example of combining two recursions: the outer recursion walks the tree ("is it here"), the inner one compares two subtrees exactly.

At worst `O(n · m)`, where `n` and `m` are the tree sizes, since a full `isSameTree` comparison may be done at every node of the bigger tree. **Pitfall:** checking only that the value matches and not the whole subtree structure — the value alone is not enough, because the same value can occur in subtrees of different sizes.
</details>

---

## Unit 2 — Traversals: preorder, inorder, postorder

### 05.2.1  Binary Tree Preorder Traversal  ·  LC #144  ·  Easy  ·  stack, tree, depth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-preorder-traversal/>
**Level:** 2
<details><summary>Hint</summary>
Process the node before moving to either child.
</details>
<details><summary>Key idea & complexity</summary>
Recursion or a stack: node, left, right, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Preorder processes the node first, then the left subtree, then the right. The recursive implementation is straightforward; the iterative version uses a stack: push the root, and on every iteration pop, append to the result, push the right child first and then the left (so that left is processed first, since the stack is LIFO).

`O(n)` time and space (result array + stack).

**Pitfall:** in the iterative version the push order of the children is easy to mix up — if you push left first, the result comes out in the wrong order, because the stack reverses the order.
</details>

### 05.2.2  Binary Tree Inorder Traversal  ·  LC #94  ·  Easy  ·  stack, tree, depth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-inorder-traversal/>
**Level:** 2
<details><summary>Hint</summary>
Go as deep left as possible before processing anything.
</details>
<details><summary>Key idea & complexity</summary>
Recursion or a stack: left, node, right, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Inorder walks the whole left subtree first, then the node, then the right subtree. Iterative version: push nodes while going left as far as possible; when there is no more left, pop, process, and move to the popped node's right child.

`O(n)` time and space.

**Why this is important:** in a BST inorder always yields the values in ascending order — this property is used directly in e.g. `validate-binary-search-tree` and `kth-smallest-element-in-a-bst`. **Pitfall:** forgetting that "process the node" happens only when no left path remains, not the moment the node is first seen on the stack.
</details>

### 05.2.3  Binary Tree Postorder Traversal  ·  LC #145  ·  Easy  ·  stack, tree, depth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-postorder-traversal/>
**Level:** 2
<details><summary>Hint</summary>
The node is processed only after both children have already been processed.
</details>
<details><summary>Key idea & complexity</summary>
Recursion, or two stacks / reversed preorder, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Postorder walks the left subtree first, then the right, and processes the node last. Iteratively the easiest way is to compute a reversed preorder (node, **right**, left) with a stack and reverse the result at the end — because the reverse of postorder is exactly "node before right before left".

`O(n)` time and space.

**Why postorder is useful:** it is the only order in which the children are always fully processed before the parent — so it fits problems where the parent's answer depends on both children's finished results (e.g. computing height or size bottom-up). **Pitfall:** trying to implement postorder iteratively directly without the reversal trick — it requires tracking which child you are returning from, which is error-prone.
</details>

### 05.2.4  N-ary Tree Preorder Traversal  ·  LC #589  ·  Easy  ·  stack, tree, depth-first-search
<https://leetcode.com/problems/n-ary-tree-preorder-traversal/>
**Level:** 2
<details><summary>Hint</summary>
There can be more than two children — push them onto the stack in reverse order.
</details>
<details><summary>Key idea & complexity</summary>
Recursion or a stack, process the node before the children in order, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same idea as preorder in a binary tree, but a node may have an arbitrary number of children in a list `children`. In the recursion, process the node and then call recursively on every child in order.

In the iterative version the children must be pushed onto the stack in **reverse order**, so that the first child is popped first — the same principle as in the binary tree, but generalised to `k` children.

`O(n)` time and space, where `n` is the number of nodes (the number of edges has no effect, since every node is processed exactly once). **Pitfall:** forgetting to reverse the children's order in the stack version, so the result comes out backwards among siblings.
</details>

### 05.2.5  Binary Tree Paths  ·  LC #257  ·  Easy  ·  string, backtracking, tree, depth-first-search
<https://leetcode.com/problems/binary-tree-paths/>
**Level:** 2
<details><summary>Hint</summary>
Build the path as a string on the way and add it to the result only at a leaf.
</details>
<details><summary>Key idea & complexity</summary>
DFS carrying the path as a string, emit at leaves, `O(n^2)` due to string copying.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Preorder-style DFS: carry the path so far as a parameter (e.g. `"1->2->3"`), and at every node append the node's value to the path. At a leaf (no children) the path is closed off and added to the result set; at an internal node recurse into the existing children.

`O(n)` nodes are visited, but copying the string at every step makes the total time `O(n^2)` at worst (skewed tree, long path copied repeatedly).

**Pitfall:** testing for a leaf with only `left == NULL`, forgetting that `right == NULL` must also hold — otherwise nodes with only one child produce bogus "paths" that do not actually end at a leaf.
</details>

### 05.2.6  Sum Root to Leaf Numbers  ·  LC #129  ·  Medium  ·  tree, depth-first-search, binary-tree
<https://leetcode.com/problems/sum-root-to-leaf-numbers/>
**Level:** 3
<details><summary>Hint</summary>
Carry the accumulated number as a parameter and add it to the sum only at a leaf.
</details>
<details><summary>Key idea & complexity</summary>
DFS carrying the accumulated number (`num · 10 + value`), sum at leaves, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same skeleton as `binary-tree-paths`, but the path is interpreted as a number rather than a string: at every node the accumulated number is updated by the formula `num = num · 10 + node.val`. At a leaf this number is added to the total.

Since the update is `O(1)` (no string copying), the whole algorithm is `O(n)` time and `O(h)` stack.

This is a good example of the same DFS skeleton working with both a string and a numeric representation — the choice affects only how the state is carried, not the traversal order itself. **Pitfall:** forgetting to handle the single-node tree (the root is also a leaf) correctly — the base case still works, because the recursion recognises the root as a leaf immediately.
</details>

### 05.2.7  Count Good Nodes in Binary Tree  ·  LC #1448  ·  Medium  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/count-good-nodes-in-binary-tree/>
**Level:** 3
<details><summary>Hint</summary>
Carry the maximum value seen since the root and compare against it at every node.
</details>
<details><summary>Key idea & complexity</summary>
DFS carrying the path maximum, count nodes whose value is `>=` the maximum, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A "good node" is one whose value is at least as large as the maximum of all its ancestors. So carry `maxSoFar` (the largest value seen since the root) as a parameter: if the current node's value is `>= maxSoFar`, it is good and is counted, and the new `maxSoFar` for the children is `max(maxSoFar, value)`.

The total is this node's goodness plus the recursive results of the left and right subtrees.

`O(n)` time, since every node is processed once with constant work. **Pitfall:** the root's `maxSoFar` must be initialised to the root's own value (or to `-infinity`, which leads to the same result) — the root is always a good node in itself.
</details>

### 05.2.8  Flatten Binary Tree to Linked List  ·  LC #114  ·  Medium  ·  linked-list, stack, tree, depth-first-search
<https://leetcode.com/problems/flatten-binary-tree-to-linked-list/>
**Level:** 3
<details><summary>Hint</summary>
Process the right subtree and the left subtree before the node itself, and link them to a global "previous" reference.
</details>
<details><summary>Key idea & complexity</summary>
Postorder-style reversed preorder (right, left, node), link via a stack/`prev`, `O(n)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The goal is to turn the tree, in place, into a right-leaning list in preorder order. The convenient way is to walk the tree in **reversed preorder** (right, left, node — i.e. postorder-like, but with the children in reversed order) and maintain a variable `prev` that points to the already-processed (already-listified) part.

At every node set `node.right = prev`, `node.left = NULL`, and `prev = node`. Because right is visited first, `prev` is always the correctly linked continuation.

`O(n)` time, `O(1)` extra space (only the recursion stack, which can be avoided with an iterative version). **Pitfall:** trying to do this in normal preorder order — then the `node.left` reference is lost before it has been used to walk the left subtree.
</details>

---

## Unit 3 — Level by level: BFS on trees

### 05.3.1  Average of Levels in Binary Tree  ·  LC #637  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/average-of-levels-in-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
Process a whole level at a time and compute its average before the next level.
</details>
<details><summary>Key idea & complexity</summary>
BFS level by level, sum/size per level, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The basic formula for BFS on a tree: `levelSize = queue.length` is saved before the inner loop, and the loop processes exactly as many nodes as were on the current level, pushing their children into the queue for the next round. Each level's sum is divided by its size.

`O(n)` time and space, since the queue can hold `O(n)` nodes at worst (e.g. the bottom level of a perfect tree).

**Pitfall:** using the queue's current length directly as the loop condition (`while (i < queue.length)`) instead of saving it beforehand — since children are added to the queue mid-loop, the length changes and the level boundary disappears.
</details>

### 05.3.2  Minimum Depth of Binary Tree  ·  LC #111  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/minimum-depth-of-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
BFS finds the nearest leaf first — stop as soon as one is found.
</details>
<details><summary>Key idea & complexity</summary>
BFS, return the first level on which a leaf is found, `O(n)` at worst but often faster.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Here BFS is genuinely the better choice over DFS: DFS would compute the length of every root-to-leaf path and take the minimum, walking the whole tree regardless. BFS instead visits nodes in order of distance from the root, so **the first leaf encountered is automatically the nearest leaf** — the algorithm can return the answer immediately and need not walk the whole tree.

At worst (e.g. a fully symmetric tree with no early leaf) the time is still `O(n)`, but in practice BFS often stops much earlier than DFS would.

**Pitfall:** defining "leaf" wrongly — a node with only one child is *not* a leaf even if the other side is `NULL`; the minimum depth must not end early because one child is missing.
</details>

### 05.3.3  Cousins in Binary Tree  ·  LC #993  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/cousins-in-binary-tree/>
**Level:** 2
<details><summary>Hint</summary>
Two nodes are cousins if they are on the same level but have different parents.
</details>
<details><summary>Key idea & complexity</summary>
BFS level by level, track the parent of every node, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
BFS fits this naturally, because both the level number and the parent are needed for every node. Walk level by level; on every level check whether both target nodes are found on this level, and if so compare their parents — if the parents differ, the nodes are cousins.

`O(n)` time, since at worst the whole tree is walked before both nodes are found.

**Pitfall:** comparing only the depths and forgetting the parent check (so siblings are wrongly counted as cousins), or conversely comparing only the parents and forgetting the depth — both conditions ("same level" AND "different parent") are mandatory.
</details>

### 05.3.4  Binary Tree Level Order Traversal  ·  LC #102  ·  Medium  ·  tree, breadth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-level-order-traversal/>
**Level:** 3
<details><summary>Hint</summary>
Same level-counter idea as in the averages problem, but collect the values into a list instead of a sum.
</details>
<details><summary>Key idea & complexity</summary>
BFS, collect every level as its own list, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The basic BFS skeleton: the queue starts with the root, and on every round `levelSize = queue.length` decides how many nodes belong to this level. Their values are collected into their own list, and their existing children are pushed into the queue for the next round. Finally the level's list is appended to the result.

`O(n)` time and space — every node is processed and enqueued exactly once.

This is the base structure for every other per-level problem: once this is mastered, averages, zigzag, the right edge and the rest are just small variations of the same skeleton. **Pitfall:** forgetting to check that a child exists before pushing it into the queue, so `NULL` values end up in the queue.
</details>

### 05.3.5  Binary Tree Level Order Traversal II  ·  LC #107  ·  Medium  ·  tree, breadth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-level-order-traversal-ii/>
**Level:** 3
<details><summary>Hint</summary>
Collect the levels normally top-down and reverse the whole list at the very end.
</details>
<details><summary>Key idea & complexity</summary>
Same BFS as basic level order, reverse the result list at the end, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Algorithmically identical to `binary-tree-level-order-traversal` — the only difference is that the result is wanted bottom-up. The simplest solution is to collect the levels in normal BFS order (starting from the root) and reverse the whole list at the very end, rather than trying to insert results at the front of the list mid-run (which would be `O(n)` per insertion in an array).

`O(n)` time for the BFS plus `O(k)` for the reversal, where `k` is the number of levels — still `O(n)` total.

**Pitfall:** inserting every level at the front of the list as it is computed — in an array-backed list this is an `O(n)` operation per level and `O(n^2)` total at worst.
</details>

### 05.3.6  Binary Tree Zigzag Level Order Traversal  ·  LC #103  ·  Medium  ·  tree, breadth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-zigzag-level-order-traversal/>
**Level:** 3
<details><summary>Hint</summary>
Collect every level normally, but reverse the list only when appending it to the result, on every other level.
</details>
<details><summary>Key idea & complexity</summary>
BFS level by level, reverse every other level, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same BFS skeleton as basic level order: collect the node values of one level at a time into a list. The only addition is a counter (or `level % 2`) saying whether the current level is even or odd — on odd levels (or every other level by convention) the collected list is reversed before being appended to the final result.

The traversal order of the nodes in the queue itself stays the same throughout (left to right) — only the **presentation order of the result** alternates per level, not the direction of the BFS itself.

`O(n)` time and space. **Pitfall:** trying to change the queue's processing order itself (e.g. pushing children in reversed order every other time) instead of just reversing the finished level's list — this scrambles the children's queue order for the next level.
</details>

### 05.3.7  Binary Tree Right Side View  ·  LC #199  ·  Medium  ·  tree, depth-first-search, breadth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-right-side-view/>
**Level:** 3
<details><summary>Hint</summary>
Seen from the right, the last node of every level is visible, in left-to-right processing order.
</details>
<details><summary>Key idea & complexity</summary>
BFS level by level, take the last node of every level, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
BFS naturally visits every level's nodes left to right (as long as children are always pushed left-first). Then it suffices to record every level's **last** processed node before moving to the next level — that is exactly the node visible when looking at the tree from the right.

An alternative DFS solution walks the tree right-first and stores the first node encountered at every depth — both work, but BFS is conceptually more direct, because "last on the level" is directly readable from the queue structure.

`O(n)` time and space. **Pitfall:** taking the level's *first* node instead of the last — then the result would be the left edge, not the right.
</details>

### 05.3.8  Populating Next Right Pointers in Each Node  ·  LC #116  ·  Medium  ·  linked-list, tree, depth-first-search, breadth-first-search
<https://leetcode.com/problems/populating-next-right-pointers-in-each-node/>
**Level:** 3
<details><summary>Hint</summary>
Link every node in the queue to the next node on the same level before moving to the next level.
</details>
<details><summary>Key idea & complexity</summary>
BFS level by level, link consecutive nodes in the queue, `O(n)` time, `O(1)` extra space in a perfect tree.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Basic approach: BFS level by level, and on every level link consecutive nodes to each other via the `next` field as they are dequeued — the last node's `next` stays `NULL`.

Since the tree is perfect (every internal node has exactly two children), a more efficient solution uses the `next` links already built on the previous level to move without a separate queue: once level `k` is linked, it can be used to walk all of level `k+1`'s nodes and link them, dropping the extra space from `O(n)` to `O(1)`.

`O(n)` time either way. **Pitfall:** trying the same `O(1)`-space trick on a non-perfect tree without special handling — missing children break the linking assumption, and a separate "next existing node" search is needed.
</details>

---

## Unit 4 — The BST invariant and how to exploit it

### 05.4.1  Search in a Binary Search Tree  ·  LC #700  ·  Easy  ·  tree, binary-search-tree, binary-tree
<https://leetcode.com/problems/search-in-a-binary-search-tree/>
**Level:** 2
<details><summary>Hint</summary>
Compare the sought value to the node and continue in the correct direction only — not both.
</details>
<details><summary>Key idea & complexity</summary>
A single descent left/right based on the BST invariant, `O(h)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the most direct application of the BST invariant: at every node compare the sought value to the node's value. If equal, found; if smaller, continue left (since everything larger is on the right, it need not be examined); if larger, continue right. In a general tree you would have to examine both children.

`O(h)` time, where `h` is the tree height — `O(log n)` in a balanced tree, `O(n)` at worst in a skewed one.

Can be implemented equally well iteratively with a `while` loop or recursively — neither needs to combine return values from several children, because only one direction is ever taken.
</details>

### 05.4.2  Range Sum of BST  ·  LC #938  ·  Easy  ·  tree, depth-first-search, binary-search-tree, binary-tree
<https://leetcode.com/problems/range-sum-of-bst/>
**Level:** 2
<details><summary>Hint</summary>
Don't walk a branch at all if the whole subtree is certainly outside the given range.
</details>
<details><summary>Key idea & complexity</summary>
DFS that prunes branches using the BST invariant, `O(h + k)` where `k` is the number of nodes in the range.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A naive solution would walk the whole tree and sum the values in range, `O(n)`. The BST invariant enables pruning: if `node.val < low`, the whole left subtree is certainly too small and need not be walked at all — recurse right only. Symmetrically, if `node.val > high`, recurse left only.

Only when the value is in range are both directions walked and the node's value added to the sum.

In practice this pruning makes the algorithm considerably faster than a full traversal, especially when the range is narrow relative to the tree. **Pitfall:** forgetting the pruning and walking the whole tree regardless — it works correctly, but wastes exactly the advantage the BST structure offers.
</details>

### 05.4.3  Minimum Absolute Difference in BST  ·  LC #530  ·  Easy  ·  tree, depth-first-search, breadth-first-search, binary-search-tree
<https://leetcode.com/problems/minimum-absolute-difference-in-bst/>
**Level:** 2
<details><summary>Hint</summary>
The smallest difference between two BST nodes is always found between consecutive values in inorder order.
</details>
<details><summary>Key idea & complexity</summary>
Inorder traversal, compare consecutive values, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Since inorder traversal yields the BST's values in ascending order, the smallest possible difference between any two nodes is **always** found between two consecutive inorder values — there is no need to compare all pairs.

Implementation: do an inorder traversal and maintain the previously seen value (`prev`); at every node compute `node.val - prev` and update the minimum, then `prev = node.val`.

`O(n)` time with a single traversal, versus the naive `O(n^2)` comparison of all pairs.

**Pitfall:** comparing all pairs, or forgetting that "smallest difference in the whole tree" does not require the nodes to be in a parent-child relationship — it suffices to be consecutive *in value order*, which is exactly what inorder reveals directly.
</details>

### 05.4.4  Two Sum IV - Input is a BST  ·  LC #653  ·  Easy  ·  hash-table, two-pointers, tree, depth-first-search
<https://leetcode.com/problems/two-sum-iv-input-is-a-bst/>
**Level:** 2
<details><summary>Hint</summary>
Collect a sorted list with an inorder traversal and then use two pointers, as in the ordinary Two Sum on a sorted array.
</details>
<details><summary>Key idea & complexity</summary>
Inorder + two pointers, or DFS + hash set, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two natural solutions. (1) An inorder traversal yields an ascending sorted list, turning the problem directly into "two sum on a sorted array": two pointers from the ends of the list moving inward. (2) DFS in any order, maintaining a hash set of values seen — at every node check whether `target - node.val` is in the set.

Both are `O(n)` time; the first explicitly uses the BST invariant (ordering), the second does not use the BST property at all and would work on any tree.

**Pitfall:** forgetting that the same node must not match with itself (the `target = 2 · node.val` case) — when using the hash set, the node is added to the set only after the check, so this is handled automatically.
</details>

### 05.4.5  Insert into a Binary Search Tree  ·  LC #701  ·  Medium  ·  tree, binary-search-tree, binary-tree
<https://leetcode.com/problems/insert-into-a-binary-search-tree/>
**Level:** 3
<details><summary>Hint</summary>
Find the place where the value would belong, as in search, but stop at the first empty slot and attach a new node there.
</details>
<details><summary>Key idea & complexity</summary>
Navigate by the BST invariant to an empty slot, attach the new node, `O(h)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same navigation as in search: compare the value to insert against every node and go left or right according to the BST invariant. The difference is the stopping condition — instead of looking for a matching value, continue until a `NULL` slot is hit, and attach a new leaf node there.

The recursive formulation is natural: the function returns the subtree (possibly unchanged, or a new node if we were at a `NULL` slot), and the parent sets this as its own child — the classic "return the modified subtree" pattern.

`O(h)` time. **Pitfall:** forgetting that the new value never replaces an existing node and never requires rebalancing the tree — the problem's BST is not self-balancing, so the worst case is `O(n)` in a skewed tree.
</details>

### 05.4.6  Validate Binary Search Tree  ·  LC #98  ·  Medium  ·  tree, depth-first-search, binary-search-tree, binary-tree
<https://leetcode.com/problems/validate-binary-search-tree/>
**Level:** 3
<details><summary>Hint</summary>
Comparing against the immediate child alone is not enough — the whole subtree must be on the correct side, not just one level deep.
</details>
<details><summary>Key idea & complexity</summary>
DFS carrying the allowed `(min, max)` window, or inorder monotonicity, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The classic pitfall is checking only `node.left.val < node.val < node.right.val`, which wrongly accepts trees where a deeper node violates the constraint even though the immediate child looks right. The correct invariant carries **the whole allowed window** `(min, max)` down the recursion: going left, the upper bound tightens to the current value; going right, the lower bound.

An alternative solution does an inorder traversal and checks that the values are strictly increasing (comparing against the previously seen value) — this directly uses the fact that inorder on a BST is always sorted.

`O(n)` time either way. **Pitfall number two:** using `Integer.MIN/MAX_VALUE` as the bounds when the values themselves may be these — safer is to use `null`/`±infinity` notation for unbounded intervals (in C: a `NULL` pointer meaning "no bound", or `long` bounds).
</details>

### 05.4.7  Kth Smallest Element in a BST  ·  LC #230  ·  Medium  ·  tree, depth-first-search, binary-search-tree, binary-tree
<https://leetcode.com/problems/kth-smallest-element-in-a-bst/>
**Level:** 3
<details><summary>Hint</summary>
Inorder yields the values in ascending order, so the k-th processed node is directly the answer.
</details>
<details><summary>Key idea & complexity</summary>
Inorder traversal, stop at the k-th processed node, `O(h + k)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Since inorder traversal walks the BST's values in ascending order, the k-th smallest value is exactly the k-th node encountered in inorder order — there is no need to collect the whole list in memory before finding the answer.

The most efficient implementation is an iterative inorder with a stack that stops as soon as the counter reaches `k`: `O(h + k)` time, since first `h` steps are taken left onto the stack and then `k` nodes are processed.

If `k`-queries are made repeatedly on the same tree, consider storing a "subtree size" in every node, so that a single query speeds up to `O(h)` without a traversal. **Pitfall:** collecting the whole inorder list in memory and indexing into it — it works, but wastes `O(n)` space when `O(h)` would suffice.
</details>

### 05.4.8  Delete Node in a BST  ·  LC #450  ·  Medium  ·  tree, binary-search-tree, binary-tree
<https://leetcode.com/problems/delete-node-in-a-bst/>
**Level:** 3
<details><summary>Hint</summary>
Three cases: leaf, one child, two children — only the two-child case requires finding the inorder successor.
</details>
<details><summary>Key idea & complexity</summary>
Navigate to the node, replace it with its inorder successor (or predecessor), `O(h)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First navigate to the node to delete using the BST invariant, as in search. Deletion splits into three cases: (1) a leaf — remove directly, return `NULL`; (2) one child — replace the node with that child; (3) two children — here an invariant-preserving replacement is needed: the **inorder successor** (the smallest value in the right subtree, i.e. as far left as possible from the right subtree) or correspondingly the inorder predecessor. The node's value is replaced with the successor's value, and the successor is deleted recursively from the right subtree.

This works because the inorder successor is larger than the whole left subtree and smaller than the rest of the right subtree — exactly the position the BST invariant defines.

`O(h)` time. **Pitfall:** replacing only the value and forgetting to remove the original successor node — then the same value remains in the tree twice.
</details>

---

## Unit 5 — Lowest common ancestor and path problems

### 05.5.1  Lowest Common Ancestor of a Binary Search Tree  ·  LC #235  ·  Medium  ·  tree, depth-first-search, binary-search-tree, binary-tree
<https://leetcode.com/problems/lowest-common-ancestor-of-a-binary-search-tree/>
**Level:** 3
<details><summary>Hint</summary>
Use the BST ordering: if both values are smaller, go left; if both are larger, go right; otherwise you have found the LCA.
</details>
<details><summary>Key idea & complexity</summary>
Navigate by the BST invariant until the values diverge, `O(h)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The BST invariant makes this much more direct than in a general tree: if both `p`'s and `q`'s values are smaller than the current node's value, the LCA is certainly in the left subtree (neither can be on the right nor at this node); symmetrically right if both are larger. As soon as the values "split" (one `<=` the node, the other `>=` the node), the current node is the LCA — no need to search deeper.

Can be implemented iteratively with no recursion or stack at all.

`O(h)` time and `O(1)` space iteratively, considerably better than the general tree's `O(n)`. **Pitfall:** using the general-tree LCA algorithm and forgetting the BST shortcut — it works, but throws away the invariant's advantage entirely.
</details>

### 05.5.2  Lowest Common Ancestor of a Binary Tree  ·  LC #236  ·  Medium  ·  tree, depth-first-search, binary-tree, binary-lifting
<https://leetcode.com/problems/lowest-common-ancestor-of-a-binary-tree/>
**Level:** 3
<details><summary>Hint</summary>
The function returns: if `p` or `q` (or both) is found in this subtree, return that find upward; if both are found in different children, the current node is the LCA.
</details>
<details><summary>Key idea & complexity</summary>
DFS that returns the found node or `NULL`, combined at the parent, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Without the BST invariant there is no direct navigation — both nodes must be searched for in the whole tree. The recursion's return value is: "if this subtree contains `p` or `q` (or is one of them itself), return that node; otherwise `NULL`".

When both recursive calls (left and right) return non-`NULL`, that means `p` and `q` were found in different branches — then the **current node** is the LCA, and it is returned upward. If only one side found something, that is passed upward as is (the LCA is deeper in that branch, or is one of the sought nodes itself).

`O(n)` time, since at worst the whole tree is walked once. **Pitfall:** assuming `p` and `q` are always in different branches — if one is an ancestor of the other, the LCA is that ancestor itself, which the algorithm handles correctly precisely because a node can "be its own find".
</details>

### 05.5.3  Path Sum II  ·  LC #113  ·  Medium  ·  backtracking, tree, depth-first-search, binary-tree
<https://leetcode.com/problems/path-sum-ii/>
**Level:** 3
<details><summary>Hint</summary>
Same as path-sum, but keep the whole path in a list and copy it into the result whenever the target matches at a leaf.
</details>
<details><summary>Key idea & complexity</summary>
DFS with backtracking, carry the path and the remaining sum, record at leaves, `O(n)` (plus copying the paths into the result).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
An extension of `path-sum`: now all matching paths must be returned, not just whether one exists. Carry both the remaining target sum and the current path (a list of node values) as parameters downward; the node's value is appended to the list before the recursion and removed (backtrack) after it, so that the list is correct when sibling branches are processed too.

At a leaf, if the remaining sum is zero, the current path is **copied** into the result — a direct reference to the same list would break with the next backtrack.

Time `O(n)` for processing the nodes plus `O(L)` per found path for copying it, where `L` is the path length. **Pitfall:** forgetting the backtrack removal (`path.pop()`, or `len--` in C) after the recursive call, so the path grows wrongly across sibling branches.
</details>

### 05.5.4  Path Sum III  ·  LC #437  ·  Medium  ·  tree, depth-first-search, binary-tree
<https://leetcode.com/problems/path-sum-iii/>
**Level:** 3
<details><summary>Hint</summary>
The path neither starts at the root nor ends at a leaf — convert it to a prefix sum as in the array subarray-sum problem.
</details>
<details><summary>Key idea & complexity</summary>
DFS + prefix-sum hash table (cumulative sum from the root), `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Here a path can start and end at any node, not just root to leaf — so a direct `path-sum`-style solution is not enough as is; a naive version would try every node as a starting point, `O(n^2)`.

The `O(n)` solution borrows the array trick "subarray sum equals k": carry along the DFS a cumulative sum from the root to the current node, and in a hash table keep a count of how many times each cumulative sum has been seen on the path from the root to here. If `cumSum - target` is in the table, every such occurrence corresponds to one valid path.

**Critical detail:** the hash table update must be **undone** (decrement by one) when returning from the recursion — otherwise sibling branches see each other's sums, producing false hits. This backtracking step is the most common mistake in this problem.
</details>

### 05.5.5  Distribute Coins in Binary Tree  ·  LC #979  ·  Medium  ·  tree, depth-first-search, binary-tree, dp-on-trees
<https://leetcode.com/problems/distribute-coins-in-binary-tree/>
**Level:** 3
<details><summary>Hint</summary>
Return from every subtree its coin excess (coins minus number of nodes) — the number of moves is the sum of the absolute values of these.
</details>
<details><summary>Key idea & complexity</summary>
DFS returns the subtree's coin excess, sum the absolute values of the flows, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Postorder recursion: every subtree returns its **excess** — how many coins it has over (positive) or under (negative) relative to its own node count, once all internal moves have been made. The subtree's excess is `coins(node) - 1 + left_excess + right_excess`.

At every node the number of moves increases by `abs(left_excess) + abs(right_excess)`, because every coin that exceeds or falls short of a child's need must cross the edge exactly once (in either direction).

This is a good example of the return value (excess) not being the same as the asked-for answer (total number of moves) — the answer accumulates in a global variable as a side effect, the same pattern as in `diameter-of-binary-tree`. `O(n)` time.
</details>

### 05.5.6  All Nodes Distance K in Binary Tree  ·  LC #863  ·  Medium  ·  hash-table, tree, depth-first-search, breadth-first-search
<https://leetcode.com/problems/all-nodes-distance-k-in-binary-tree/>
**Level:** 3
<details><summary>Hint</summary>
A tree offers no direct backward movement — add parent references first, then do an ordinary BFS in three directions (left, right, parent).
</details>
<details><summary>Key idea & complexity</summary>
Build parent references with DFS, then BFS from the target node for `k` steps, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The problem combines this chapter's two ideas: tree recursion and BFS. The tree itself only allows moving downward, but distance `k` may require moving upward from the target node too — so the first phase is a DFS that builds a `parent` reference for every node.

After that, run an ordinary BFS from the target node, treating the tree as a general graph in which every node has three neighbours (left child, right child, parent), and stop at level `k`. Marking visited nodes is mandatory so that the BFS does not go back through the parent to the node it just came from.

`O(n)` time for both phases. **Pitfall:** forgetting to mark the start node as visited before the BFS, so the first step can return to itself and loop forever.
</details>

### 05.5.7  Binary Tree Maximum Path Sum  ·  LC #124  ·  Hard  ·  dynamic-programming, tree, depth-first-search, binary-tree
<https://leetcode.com/problems/binary-tree-maximum-path-sum/>
**Level:** 4
<details><summary>Hint</summary>
Same pattern as the tree diameter: the return value (best in one direction) and the asked-for answer (best through any node) are not the same thing.
</details>
<details><summary>Key idea & complexity</summary>
DFS, return the best single-direction extension, update a global best as the two-direction sum, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This ties together the chapter's central lesson: the function's **return value** is not what the problem asks for. The return value is "the best path sum starting at this node and going downward in one direction" — `value + max(0, left, right)`, where negative branches are cut off by the `max(0, ...)`, because a bad branch is better left out of the path entirely.

The global best sum, in contrast, is updated at every node separately as a side effect, taking **both** directions into account at once: `value + max(0, left) + max(0, right)`, because the best path through any single node may use both the left and the right branch — but the return value to the parent may use only one, since a path cannot fork in two inside the tree.

`O(n)` time. This is the hardest problem of the whole unit precisely because the distinction between the return value and the answer is most subtle here.
</details>

---

## Unit 6 — Building a tree from traversals, serialisation

### 05.6.1  Convert Sorted Array to Binary Search Tree  ·  LC #108  ·  Easy  ·  array, divide-and-conquer, tree, binary-search-tree
<https://leetcode.com/problems/convert-sorted-array-to-binary-search-tree/>
**Level:** 2
<details><summary>Hint</summary>
Always pick the middle element as the root and recurse on both halves — this guarantees balance automatically.
</details>
<details><summary>Key idea & complexity</summary>
Recursion: middle element as root, split in half, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Construction succeeds directly without a separate inorder lookup, because the input is already a sorted array — it *is* already the BST's inorder representation. Pick the middle element as the root (the split point is thus known directly from the index, no search needed), and recurse on the left half and the right half by the same principle.

Choosing the middle guarantees that both halves are always about equal in size, so the resulting tree is height-balanced — `O(log n)`.

`O(n)` time in total, since every element is processed once. **Pitfall:** the middle of an even-length sub-range is not unique (lower or upper middle) — either is a valid answer; the problem accepts several different BST shapes.
</details>

### 05.6.2  Construct Binary Search Tree from Preorder Traversal  ·  LC #1008  ·  Medium  ·  array, stack, tree, binary-search-tree
<https://leetcode.com/problems/construct-binary-search-tree-from-preorder-traversal/>
**Level:** 3
<details><summary>Hint</summary>
In a BST, preorder alone suffices: use an upper bound to tell when the next value no longer belongs to the current subtree.
</details>
<details><summary>Key idea & complexity</summary>
Recursion with a bound derived from the BST invariant, or a stack, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the exception to this unit's general rule "preorder alone is not enough" — that rule holds for a general binary tree, but in a BST the invariant itself gives the split: when building a node with bounds `(low, high)`, the next preorder value belongs to the left subtree if it is smaller than the current node, otherwise it no longer belongs to this subtree and the recursion returns.

The implementation carries a global index into the preorder array and the bounds `(low, high)` within which the next value must lie in order to be attached to the current subtree.

`O(n)` time, since every element is processed and consumed from the preorder array exactly once (even though more recursive calls are made). **Pitfall:** forgetting that the index must be shared state (a pointer or global), not a value — otherwise the recursion branches re-consume the same elements.
</details>

### 05.6.3  Construct Binary Tree from Preorder and Inorder Traversal  ·  LC #105  ·  Medium  ·  array, hash-table, divide-and-conquer, tree
<https://leetcode.com/problems/construct-binary-tree-from-preorder-and-inorder-traversal/>
**Level:** 3
<details><summary>Hint</summary>
The first preorder element is always the root — find it in inorder to learn how many elements belong to the left subtree.
</details>
<details><summary>Key idea & complexity</summary>
Recursion: first of preorder is the root, split inorder at the root, `O(n)` with a hash table.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The first preorder element is always the root of the whole (sub)tree. Finding this value in the inorder list gives the split point: everything before it in inorder belongs to the left subtree, everything after it to the right — and these sizes tell how many of the following preorder elements belong to each side.

A naive implementation would look up the root with `indexOf` in inorder on every recursive call, `O(n)` per call and `O(n^2)` total. The efficient solution precomputes a hash table value → index before the recursion, so the lookup is `O(1)` and the whole algorithm `O(n)`.

**Pitfall:** copying sub-arrays into new lists on every recursive call — this works but wastes time and space; better to carry only index bounds into the original arrays.
</details>

### 05.6.4  Construct Binary Tree from Inorder and Postorder Traversal  ·  LC #106  ·  Medium  ·  array, hash-table, divide-and-conquer, tree
<https://leetcode.com/problems/construct-binary-tree-from-inorder-and-postorder-traversal/>
**Level:** 3
<details><summary>Hint</summary>
The last postorder element is the root (not the first, as in preorder) — build the right subtree first, because postorder lists the right branch just before the root.
</details>
<details><summary>Key idea & complexity</summary>
Recursion: last of postorder is the root, split inorder at the root, `O(n)` with a hash table.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The mirror image of the previous problem: in postorder the root is the **last** element (because postorder walks left-right-node). The split from inorder works as before: the root's index in inorder tells how many elements belong to each side.

Important detail: since postorder has the form `[left subtree][right subtree][root]`, and the efficient implementation processes the postorder array **backwards** (from the root toward the start), the right subtree must be built before the left — the opposite order from the preorder version.

The same `O(1)` lookup via a hash table makes the total time `O(n)` instead of searching for the root with `indexOf` every time (`O(n^2)`). **Pitfall:** mixing up which traversal is read backwards — the left/right build order flips wrongly if this is not accounted for.
</details>

### 05.6.5  Maximum Binary Tree  ·  LC #654  ·  Medium  ·  array, divide-and-conquer, stack, tree
<https://leetcode.com/problems/maximum-binary-tree/>
**Level:** 3
<details><summary>Hint</summary>
The largest element is always the root — split the rest into left and right parts by its position and recurse.
</details>
<details><summary>Key idea & complexity</summary>
Recursion: largest element as root, split the array there, `O(n)` on average, `O(n^2)` at worst; monotonic stack `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Straightforward recursion: find the array's largest element, make it the root, and recurse on the left sub-array (before the largest) and the right (after it). Naively the search for the largest is `O(n)` per call, giving `O(n^2)` at worst (an already sorted array, where every recursion level drops only one element).

The more efficient `O(n)` solution builds the tree with a **monotonic stack** one element at a time: keep the stack in decreasing order, and when a new element is larger than the stack top, the popped element becomes the new node's left child (and the chain continues); otherwise the new element becomes the right child of the stack top.

This is a good bridge to the next topic: the monotonic stack is a general technique that recurs in array problems outside trees too.
</details>

### 05.6.6  Find Duplicate Subtrees  ·  LC #652  ·  Medium  ·  hash-table, tree, depth-first-search, binary-tree
<https://leetcode.com/problems/find-duplicate-subtrees/>
**Level:** 3
<details><summary>Hint</summary>
Serialise every subtree in postorder order into a string and count how many times each serialisation occurs.
</details>
<details><summary>Key idea & complexity</summary>
Postorder serialisation + hash table, return subtrees whose serialisation is seen `>= 2` times, `O(n^2)` due to string copying (or `O(n)` with ID encoding).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two subtrees are identical exactly when their postorder serialisation (values + structure, e.g. `"left,right,value"` as a string) is the same. Postorder fits this naturally, because the children are serialised before the parent — so the subtree's whole serialisation is directly available as soon as the parent is processed.

A hash table counts how many times each serialisation has been seen; when the counter hits exactly two, that subtree's root is added to the result (this avoids the same subtree appearing several times in the result if there are more than two copies).

Building and comparing the strings costs `O(n)` per node at worst, so the total time is `O(n^2)`; the more efficient version replaces the strings with integer identifiers (triple left-ID, right-ID, value → new ID), dropping the total to `O(n)`.
</details>

### 05.6.7  Serialize and Deserialize Binary Tree  ·  LC #297  ·  Hard  ·  string, tree, depth-first-search, breadth-first-search
<https://leetcode.com/problems/serialize-and-deserialize-binary-tree/>
**Level:** 4
<details><summary>Hint</summary>
Mark every missing child explicitly (e.g. `"#"`), so that preorder alone suffices for unambiguous decoding without a separate inorder.
</details>
<details><summary>Key idea & complexity</summary>
Preorder + explicit null markers for encoding, same order for decoding, `O(n)`.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Without inorder, reconstructing a tree from preorder alone is normally impossible — but if every `NULL` child is marked **explicitly** with its own symbol (e.g. `"#"`), the preorder order alone determines the tree completely and unambiguously, because the existence (or absence) of both children of every node is directly readable from the stream.

Serialisation is a direct preorder DFS that emits `"#"` in place of every `NULL`. Decoding is the mirror image: read tokens from the stream in order, and if the next one is `"#"`, return `NULL`; otherwise create a node and recurse into its left and right child using the same stream.

`O(n)` time for both encoding and decoding. **Pitfall:** forgetting that decoding consumes the stream from **shared** state (a pointer or a queue, not a copied index) — otherwise the left subtree's decoding does not advance to the right subtree's starting point.
</details>

---

## Progress

- [ ] 05.1.1 Maximum Depth of Binary Tree (LC #104)
- [ ] 05.1.2 Invert Binary Tree (LC #226)
- [ ] 05.1.3 Same Tree (LC #100)
- [ ] 05.1.4 Symmetric Tree (LC #101)
- [ ] 05.1.5 Path Sum (LC #112)
- [ ] 05.1.6 Balanced Binary Tree (LC #110)
- [ ] 05.1.7 Diameter of Binary Tree (LC #543)
- [ ] 05.1.8 Subtree of Another Tree (LC #572)
- [ ] 05.2.1 Binary Tree Preorder Traversal (LC #144)
- [ ] 05.2.2 Binary Tree Inorder Traversal (LC #94)
- [ ] 05.2.3 Binary Tree Postorder Traversal (LC #145)
- [ ] 05.2.4 N-ary Tree Preorder Traversal (LC #589)
- [ ] 05.2.5 Binary Tree Paths (LC #257)
- [ ] 05.2.6 Sum Root to Leaf Numbers (LC #129)
- [ ] 05.2.7 Count Good Nodes in Binary Tree (LC #1448)
- [ ] 05.2.8 Flatten Binary Tree to Linked List (LC #114)
- [ ] 05.3.1 Average of Levels in Binary Tree (LC #637)
- [ ] 05.3.2 Minimum Depth of Binary Tree (LC #111)
- [ ] 05.3.3 Cousins in Binary Tree (LC #993)
- [ ] 05.3.4 Binary Tree Level Order Traversal (LC #102)
- [ ] 05.3.5 Binary Tree Level Order Traversal II (LC #107)
- [ ] 05.3.6 Binary Tree Zigzag Level Order Traversal (LC #103)
- [ ] 05.3.7 Binary Tree Right Side View (LC #199)
- [ ] 05.3.8 Populating Next Right Pointers in Each Node (LC #116)
- [ ] 05.4.1 Search in a Binary Search Tree (LC #700)
- [ ] 05.4.2 Range Sum of BST (LC #938)
- [ ] 05.4.3 Minimum Absolute Difference in BST (LC #530)
- [ ] 05.4.4 Two Sum IV - Input is a BST (LC #653)
- [ ] 05.4.5 Insert into a Binary Search Tree (LC #701)
- [ ] 05.4.6 Validate Binary Search Tree (LC #98)
- [ ] 05.4.7 Kth Smallest Element in a BST (LC #230)
- [ ] 05.4.8 Delete Node in a BST (LC #450)
- [ ] 05.5.1 Lowest Common Ancestor of a Binary Search Tree (LC #235)
- [ ] 05.5.2 Lowest Common Ancestor of a Binary Tree (LC #236)
- [ ] 05.5.3 Path Sum II (LC #113)
- [ ] 05.5.4 Path Sum III (LC #437)
- [ ] 05.5.5 Distribute Coins in Binary Tree (LC #979)
- [ ] 05.5.6 All Nodes Distance K in Binary Tree (LC #863)
- [ ] 05.5.7 Binary Tree Maximum Path Sum (LC #124)
- [ ] 05.6.1 Convert Sorted Array to Binary Search Tree (LC #108)
- [ ] 05.6.2 Construct Binary Search Tree from Preorder Traversal (LC #1008)
- [ ] 05.6.3 Construct Binary Tree from Preorder and Inorder Traversal (LC #105)
- [ ] 05.6.4 Construct Binary Tree from Inorder and Postorder Traversal (LC #106)
- [ ] 05.6.5 Maximum Binary Tree (LC #654)
- [ ] 05.6.6 Find Duplicate Subtrees (LC #652)
- [ ] 05.6.7 Serialize and Deserialize Binary Tree (LC #297)
