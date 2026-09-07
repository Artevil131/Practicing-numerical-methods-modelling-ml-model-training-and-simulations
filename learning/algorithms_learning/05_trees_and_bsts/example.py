"""Chapter 05 — Trees & BSTs. Python port of example.c: same skeletons, same neutral tree.
Run: python3 example.py
"""

from collections import deque, defaultdict


class TreeNode:
    def __init__(self, val=0, left=None, right=None):
        self.val = val
        self.left = left
        self.right = right


def build_tree(values):
    """Builds a BST by repeated insertion, left to right."""
    root = None
    for v in values:
        root = bst_insert(root, v)
    return root

# ------------------------------------------------------------------------
# 1. Recursion shapes
# ------------------------------------------------------------------------

def tree_height(t):
    if t is None:
        return 0
    return 1 + max(tree_height(t.left), tree_height(t.right))


def has_path_sum(t, target):
    if t is None:
        return False
    if t.left is None and t.right is None:
        return target == t.val
    remaining = target - t.val
    return has_path_sum(t.left, remaining) or has_path_sum(t.right, remaining)


def diameter(root):
    best = 0

    def dfs(t):
        nonlocal best
        if t is None:
            return 0
        hl = dfs(t.left)
        hr = dfs(t.right)
        best = max(best, hl + hr)
        return 1 + max(hl, hr)

    dfs(root)
    return best

# ------------------------------------------------------------------------
# 2. Traversals
# ------------------------------------------------------------------------

def preorder(t, out):
    if t is None:
        return
    out.append(t.val)
    preorder(t.left, out)
    preorder(t.right, out)


def inorder(t, out):
    if t is None:
        return
    inorder(t.left, out)
    out.append(t.val)
    inorder(t.right, out)


def postorder(t, out):
    if t is None:
        return
    postorder(t.left, out)
    postorder(t.right, out)
    out.append(t.val)


def inorder_iter(root):
    out = []
    st = []
    cur = root
    while cur or st:
        while cur:
            st.append(cur)
            cur = cur.left
        cur = st.pop()
        out.append(cur.val)
        cur = cur.right
    return out


def postorder_iter(root):
    """Reversed N-R-L."""
    if root is None:
        return []
    out = []
    st = [root]
    while st:
        t = st.pop()
        out.append(t.val)
        if t.left:
            st.append(t.left)
        if t.right:
            st.append(t.right)
    return out[::-1]

# ------------------------------------------------------------------------
# 3. Level-order BFS
# ------------------------------------------------------------------------

def level_order(root):
    if root is None:
        return []
    result = []
    q = deque([root])
    while q:
        level_size = len(q)
        level = []
        for _ in range(level_size):
            t = q.popleft()
            level.append(t.val)
            if t.left:
                q.append(t.left)
            if t.right:
                q.append(t.right)
        result.append(level)
    return result


def right_side_view(root):
    if root is None:
        return []
    view = []
    q = deque([root])
    while q:
        level_size = len(q)
        last = None
        for _ in range(level_size):
            t = q.popleft()
            last = t.val
            if t.left:
                q.append(t.left)
            if t.right:
                q.append(t.right)
        view.append(last)
    return view


def min_depth(root):
    if root is None:
        return 0
    q = deque([root])
    depth = 1
    while q:
        level_size = len(q)
        for _ in range(level_size):
            t = q.popleft()
            if t.left is None and t.right is None:
                return depth
            if t.left:
                q.append(t.left)
            if t.right:
                q.append(t.right)
        depth += 1
    return depth

# ------------------------------------------------------------------------
# 4. BST invariant
# ------------------------------------------------------------------------

def bst_search(t, key):
    while t and t.val != key:
        t = t.left if key < t.val else t.right
    return t


def bst_insert(t, key):
    if t is None:
        return TreeNode(key)
    if key < t.val:
        t.left = bst_insert(t.left, key)
    elif key > t.val:
        t.right = bst_insert(t.right, key)
    return t


def bst_valid(t, lo=float("-inf"), hi=float("inf")):
    if t is None:
        return True
    if not (lo < t.val < hi):
        return False
    return bst_valid(t.left, lo, t.val) and bst_valid(t.right, t.val, hi)


def kth_smallest(root, k):
    st = []
    cur = root
    while True:
        while cur:
            st.append(cur)
            cur = cur.left
        cur = st.pop()
        k -= 1
        if k == 0:
            return cur.val
        cur = cur.right


def bst_delete(t, key):
    if t is None:
        return None
    if key < t.val:
        t.left = bst_delete(t.left, key)
    elif key > t.val:
        t.right = bst_delete(t.right, key)
    else:
        if t.left is None:
            return t.right
        if t.right is None:
            return t.left
        succ = t.right
        while succ.left:
            succ = succ.left
        t.val = succ.val
        t.right = bst_delete(t.right, succ.val)
    return t

# ------------------------------------------------------------------------
# 5. LCA and path problems
# ------------------------------------------------------------------------

def lca(t, p, q):
    if t is None or t is p or t is q:
        return t
    left = lca(t.left, p, q)
    right = lca(t.right, p, q)
    if left and right:
        return t
    return left if left else right


def bst_lca(t, p, q):
    while (p < t.val and q < t.val) or (p > t.val and q > t.val):
        t = t.left if p < t.val else t.right
    return t


def max_path_sum(root):
    best = [float("-inf")]

    def dfs(t):
        if t is None:
            return 0
        left = max(0, dfs(t.left))
        right = max(0, dfs(t.right))
        best[0] = max(best[0], t.val + left + right)
        return t.val + max(left, right)

    dfs(root)
    return best[0]


def path_sum_count(root, target):
    counts = defaultdict(int)
    counts[0] = 1
    total = 0

    def dfs(t, cum):
        nonlocal total
        if t is None:
            return
        cum += t.val
        total += counts[cum - target]
        counts[cum] += 1
        dfs(t.left, cum)
        dfs(t.right, cum)
        counts[cum] -= 1

    dfs(root, 0)
    return total

# ------------------------------------------------------------------------
# 6. Construction and serialisation
# ------------------------------------------------------------------------

def build_pre_in(pre, in_):
    pos = {v: i for i, v in enumerate(in_)}

    def build(p_lo, i_lo, i_hi):
        if i_lo >= i_hi:
            return None
        root_val = pre[p_lo]
        k = pos[root_val]
        left_size = k - i_lo
        t = TreeNode(root_val)
        t.left = build(p_lo + 1, i_lo, k)
        t.right = build(p_lo + 1 + left_size, k + 1, i_hi)
        return t

    return build(0, 0, len(in_))


def sorted_to_bst(a):
    def build(lo, hi):
        if lo >= hi:
            return None
        mid = (lo + hi) // 2
        t = TreeNode(a[mid])
        t.left = build(lo, mid)
        t.right = build(mid + 1, hi)
        return t

    return build(0, len(a))


def serialize(t):
    out = []

    def dfs(t):
        if t is None:
            out.append("#")
            return
        out.append(str(t.val))
        dfs(t.left)
        dfs(t.right)

    dfs(t)
    return " ".join(out)


def deserialize(data):
    tokens = iter(data.split())

    def build():
        tok = next(tokens)
        if tok == "#":
            return None
        t = TreeNode(int(tok))
        t.left = build()
        t.right = build()
        return t

    return build()


def same_tree(a, b):
    if a is None and b is None:
        return True
    if a is None or b is None:
        return False
    return a.val == b.val and same_tree(a.left, b.left) and same_tree(a.right, b.right)

# ------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------

def main():
    # 7-node BST: insert order gives the Unit 1 example shape
    #           5
    #         /   \
    #        3     8
    #       / \     \
    #      2   4     9
    #     /
    #    1
    root = build_tree([5, 3, 8, 2, 4, 9, 1])

    assert tree_height(root) == 4
    assert has_path_sum(root, 5 + 3 + 2 + 1)
    assert not has_path_sum(root, 100)
    assert diameter(root) == 5                    # 1-2-3-5-8-9, 5 edges

    pre, ino, post = [], [], []
    preorder(root, pre); inorder(root, ino); postorder(root, post)
    assert ino == [1, 2, 3, 4, 5, 8, 9]            # inorder BST = sorted
    assert inorder_iter(root) == ino
    assert postorder_iter(root) == post

    levels = level_order(root)
    assert levels == [[5], [3, 8], [2, 4, 9], [1]]
    assert right_side_view(root) == [5, 8, 9, 1]
    assert min_depth(root) == 3                    # 5 -> 8 -> 9

    assert bst_search(root, 4).val == 4
    assert bst_search(root, 100) is None
    assert bst_valid(root)
    bad = TreeNode(5, TreeNode(3), TreeNode(8, TreeNode(4), TreeNode(9)))
    assert not bst_valid(bad)                      # 4 is in bad's right subtree, must be > 5

    assert kth_smallest(root, 1) == 1
    assert kth_smallest(root, 7) == 9

    small = build_tree([2, 1, 3])
    small = bst_delete(small, 2)                   # two-child delete at the root
    small_in = []
    inorder(small, small_in)
    assert small_in == [1, 3]

    n3, n9 = bst_search(root, 3), bst_search(root, 9)
    n1 = bst_search(root, 1)
    assert lca(root, n1, n3.right) is n3            # general-tree LCA: 1 and 4 split at node 3
    assert lca(root, n1, n9).val == 5
    assert bst_lca(root, 1, 4) is n3                 # BST LCA, O(h)
    assert bst_lca(root, 1, 9).val == 5

    mps_tree = TreeNode(-10, TreeNode(9), TreeNode(20, TreeNode(15), TreeNode(7)))
    assert max_path_sum(mps_tree) == 42

    psum_tree = TreeNode(10, TreeNode(5, TreeNode(3), TreeNode(-3)),
                          TreeNode(-3, right=TreeNode(11)))
    assert path_sum_count(psum_tree, 8) >= 1        # e.g. 10 -> 5 -> -3, or 5 -> 3

    built = build_pre_in([3, 9, 20, 15, 7], [9, 3, 15, 20, 7])
    assert same_tree(built, TreeNode(3, TreeNode(9), TreeNode(20, TreeNode(15), TreeNode(7))))

    bal = sorted_to_bst([1, 2, 3, 4, 5, 6, 7])
    bal_in = []
    inorder(bal, bal_in)
    assert bal_in == [1, 2, 3, 4, 5, 6, 7]
    assert tree_height(bal) == 3                    # balanced: ceil(log2(7+1)) == 3

    s = serialize(built)
    assert s == "3 9 # # 20 15 # # 7 # #"
    back = deserialize(s)
    assert same_tree(built, back)

    print("chapter 05 trees & BSTs: all asserts passed")


if __name__ == "__main__":
    main()
