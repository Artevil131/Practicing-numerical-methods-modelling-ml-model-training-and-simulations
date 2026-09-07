/*
 * Chapter 05 — Trees & BSTs: pattern skeletons on a tiny neutral tree.
 *
 * Compile and run:
 *   cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * What is demonstrated (each in its own section, in the order of the lesson):
 *   0. TreeNode, node_new / tree_free (postorder free), building a BST by insertion
 *   1. Recursion shapes: height (result flows up), path-sum existence (state flows down),
 *      diameter (both: return height, record best through an out-parameter)
 *   2. Traversals: recursive pre/in/postorder into an array; iterative inorder with an
 *      explicit stack; iterative postorder as reversed N-R-L
 *   3. Level-order BFS with an array queue and the level_size snapshot
 *      (per-level sum, right side view, minimum depth)
 *   4. BST invariant: search, insert (transform shape), validate with a (lo, hi) window,
 *      k-th smallest by early-exit inorder, delete with the three cases
 *   5. LCA in a general tree (return found-or-NULL) and in a BST (navigate), max path sum
 *   6. Build from preorder+inorder with a position map; serialize/deserialize with "#"
 *
 * These are the SKELETONS from lesson.md, not solutions to the problems in problems.md.
 * Every malloc is paired with a free; run with -fsanitize=address to confirm.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* 0. The node                                                                */
/* ------------------------------------------------------------------------- */

typedef struct TreeNode {
    int val;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

static TreeNode *node_new(int v) {
    TreeNode *n = malloc(sizeof *n);
    if (!n) { perror("malloc"); exit(1); }
    n->val = v;
    n->left = n->right = NULL;
    return n;
}

/* Postorder: children must be freed before the parent, otherwise the child
   pointers are gone. This is the one traversal order that works for free(). */
static void tree_free(TreeNode *t) {
    if (!t) return;
    tree_free(t->left);
    tree_free(t->right);
    free(t);
}

static size_t tree_size(const TreeNode *t) {
    return t ? 1 + tree_size(t->left) + tree_size(t->right) : 0;
}

/* Prints a sideways picture: right subtree above, left below, indented by depth. */
static void tree_print(const TreeNode *t, int depth) {
    if (!t) return;
    tree_print(t->right, depth + 1);
    printf("%*s%d\n", depth * 4, "", t->val);
    tree_print(t->left, depth + 1);
}

static void print_arr(const char *label, const int *a, size_t n) {
    printf("%-28s", label);
    for (size_t i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n");
}

/* ------------------------------------------------------------------------- */
/* 1. Recursion shapes: what does the function return?                       */
/* ------------------------------------------------------------------------- */

/* Shape 2 — result flows UP. Empty tree has height 0 (edges+1 = nodes on the
   longest root-to-leaf path). */
static int height(const TreeNode *t) {
    if (!t) return 0;
    int hl = height(t->left), hr = height(t->right);
    return 1 + (hl > hr ? hl : hr);
}

/* Shape 1 — state flows DOWN as a parameter. Returns a boolean. The check is at
   a LEAF (both children NULL), never at a NULL node. */
static bool has_root_to_leaf_sum(const TreeNode *t, long remaining) {
    if (!t) return false;
    remaining -= t->val;
    if (!t->left && !t->right) return remaining == 0;
    return has_root_to_leaf_sum(t->left, remaining) ||
           has_root_to_leaf_sum(t->right, remaining);
}

/* Shape 3 — BOTH. Returns height (what the parent needs); records the diameter
   in *best (what the problem asks). Diameter counted in edges. */
static int height_and_diameter(const TreeNode *t, int *best) {
    if (!t) return 0;
    int hl = height_and_diameter(t->left, best);
    int hr = height_and_diameter(t->right, best);
    if (hl + hr > *best) *best = hl + hr;
    return 1 + (hl > hr ? hl : hr);
}

/* Sentinel trick: height, or -1 as soon as any subtree is unbalanced. One pass. */
static int height_or_minus1(const TreeNode *t) {
    if (!t) return 0;
    int hl = height_or_minus1(t->left);
    if (hl < 0) return -1;
    int hr = height_or_minus1(t->right);
    if (hr < 0) return -1;
    if (abs(hl - hr) > 1) return -1;
    return 1 + (hl > hr ? hl : hr);
}

static bool same_tree(const TreeNode *a, const TreeNode *b) {
    if (!a && !b) return true;
    if (!a || !b || a->val != b->val) return false;
    return same_tree(a->left, b->left) && same_tree(a->right, b->right);   /* short-circuit */
}

/* ------------------------------------------------------------------------- */
/* 2. Traversals                                                              */
/* ------------------------------------------------------------------------- */

/* All three recursive orders are the same function with one line moved. The
   output array is filled through a running count *n (C's version of `yield`). */
static void preorder(const TreeNode *t, int *out, size_t *n) {
    if (!t) return;
    out[(*n)++] = t->val;
    preorder(t->left, out, n);
    preorder(t->right, out, n);
}
static void inorder(const TreeNode *t, int *out, size_t *n) {
    if (!t) return;
    inorder(t->left, out, n);
    out[(*n)++] = t->val;
    inorder(t->right, out, n);
}
static void postorder(const TreeNode *t, int *out, size_t *n) {
    if (!t) return;
    postorder(t->left, out, n);
    postorder(t->right, out, n);
    out[(*n)++] = t->val;
}

/* Iterative inorder. The explicit stack is what the call stack was doing.
   Stack capacity = number of nodes is always enough (>= height). */
static void inorder_iter(TreeNode *root, size_t cap, int *out, size_t *n) {
    TreeNode **stack = malloc(cap * sizeof *stack);
    if (!stack) { perror("malloc"); exit(1); }
    size_t sp = 0;
    TreeNode *cur = root;
    while (cur || sp > 0) {
        while (cur) { stack[sp++] = cur; cur = cur->left; }   /* dive left, pushing */
        cur = stack[--sp];                                      /* leftmost unprocessed */
        out[(*n)++] = cur->val;                                 /* process on POP */
        cur = cur->right;                                       /* then its right subtree */
    }
    free(stack);
}

/* Iterative postorder = reversed "N R L". N-R-L is preorder with the children
   swapped: pop, emit, push LEFT then RIGHT (so right pops first). Reverse at end. */
static void postorder_iter(TreeNode *root, size_t cap, int *out, size_t *n) {
    if (!root) return;
    TreeNode **stack = malloc(cap * sizeof *stack);
    if (!stack) { perror("malloc"); exit(1); }
    size_t sp = 0, start = *n;
    stack[sp++] = root;
    while (sp > 0) {
        TreeNode *t = stack[--sp];
        out[(*n)++] = t->val;
        if (t->left)  stack[sp++] = t->left;
        if (t->right) stack[sp++] = t->right;
    }
    free(stack);
    for (size_t i = start, j = *n - 1; i < j; i++, j--) {      /* reverse in place */
        int tmp = out[i]; out[i] = out[j]; out[j] = tmp;
    }
}

/* ------------------------------------------------------------------------- */
/* 3. Level-order BFS with an array queue                                     */
/* ------------------------------------------------------------------------- */

/* A tree of n nodes enqueues each node once, so a flat array of n pointers with
   head/tail indices is a complete queue — no wrap-around, no deque. */
static void bfs_levels(TreeNode *root, size_t n_nodes) {
    if (!root) { printf("(empty tree)\n"); return; }
    TreeNode **q = malloc(n_nodes * sizeof *q);
    if (!q) { perror("malloc"); exit(1); }
    size_t head = 0, tail = 0;
    q[tail++] = root;
    int depth = 0, min_depth = -1;
    while (head < tail) {
        size_t level_size = tail - head;                /* SNAPSHOT before the loop */
        long sum = 0;
        int last = 0;
        for (size_t i = 0; i < level_size; i++) {
            TreeNode *t = q[head++];
            sum += t->val;
            last = t->val;                              /* last processed = right side view */
            if (min_depth < 0 && !t->left && !t->right) min_depth = depth + 1;
            if (t->left)  q[tail++] = t->left;          /* never enqueue NULL */
            if (t->right) q[tail++] = t->right;
        }
        printf("  depth %d: %zu node(s), sum %ld, avg %.2f, rightmost %d\n",
               depth, level_size, sum, (double)sum / (double)level_size, last);
        depth++;
    }
    printf("  minimum depth (first leaf seen by BFS): %d\n", min_depth);
    free(q);
}

/* ------------------------------------------------------------------------- */
/* 4. BST invariant                                                           */
/* ------------------------------------------------------------------------- */

/* Navigation: never both children. Iterative, O(h), no stack. */
static TreeNode *bst_search(TreeNode *t, int key) {
    while (t && t->val != key) t = (key < t->val) ? t->left : t->right;
    return t;
}

/* Transform shape: returns the subtree root; the caller assigns it.
   root = bst_insert(root, k);  t->left = bst_insert(t->left, k); */
static TreeNode *bst_insert(TreeNode *t, int key) {
    if (!t) return node_new(key);
    if (key < t->val)      t->left  = bst_insert(t->left, key);
    else if (key > t->val) t->right = bst_insert(t->right, key);
    return t;                                            /* duplicate: ignored */
}

/* Validation carries the whole open interval (lo, hi) downward. NULL means
   "unbounded" so INT_MIN / INT_MAX can legally appear as values. */
static bool bst_valid(const TreeNode *t, const int *lo, const int *hi) {
    if (!t) return true;
    if ((lo && t->val <= *lo) || (hi && t->val >= *hi)) return false;
    return bst_valid(t->left, lo, &t->val) && bst_valid(t->right, &t->val, hi);
}

/* "inorder BST = sorted" with early exit: k-th smallest in O(h + k), O(h) space.
   Returns false if the tree has fewer than k nodes. */
static bool bst_kth_smallest(TreeNode *root, size_t cap, int k, int *out) {
    TreeNode **stack = malloc(cap * sizeof *stack);
    if (!stack) { perror("malloc"); exit(1); }
    size_t sp = 0;
    TreeNode *cur = root;
    bool found = false;
    while (cur || sp > 0) {
        while (cur) { stack[sp++] = cur; cur = cur->left; }
        cur = stack[--sp];
        if (--k == 0) { *out = cur->val; found = true; break; }
        cur = cur->right;
    }
    free(stack);
    return found;
}

/* Delete: navigate, then three cases. Frees exactly one node in every case.
   Case 3 copies the inorder successor's value and deletes THAT value from the
   right subtree (which lands in case 1 or 2 down there). */
static TreeNode *bst_delete(TreeNode *t, int key) {
    if (!t) return NULL;
    if (key < t->val)      { t->left  = bst_delete(t->left, key);  return t; }
    if (key > t->val)      { t->right = bst_delete(t->right, key); return t; }
    /* key == t->val */
    if (!t->left && !t->right) { free(t); return NULL; }              /* case 1: leaf */
    if (!t->left || !t->right) {                                      /* case 2: one child */
        TreeNode *child = t->left ? t->left : t->right;
        free(t);
        return child;
    }
    TreeNode *succ = t->right;                                        /* case 3: two children */
    while (succ->left) succ = succ->left;                             /* min of right subtree */
    t->val = succ->val;
    t->right = bst_delete(t->right, succ->val);
    return t;
}

/* ------------------------------------------------------------------------- */
/* 5. LCA and path problems                                                   */
/* ------------------------------------------------------------------------- */

/* General tree: return value carries the search result AND the answer.
   NULL = neither found here; p or q = one found; other node = the LCA. */
static TreeNode *lca_general(TreeNode *t, const TreeNode *p, const TreeNode *q) {
    if (!t || t == p || t == q) return t;
    TreeNode *l = lca_general(t->left, p, q);
    TreeNode *r = lca_general(t->right, p, q);
    if (l && r) return t;
    return l ? l : r;
}

/* BST: navigate until the two keys split. O(h), O(1). */
static TreeNode *lca_bst(TreeNode *t, int a, int b) {
    while (t && ((a < t->val && b < t->val) || (a > t->val && b > t->val)))
        t = (a < t->val) ? t->left : t->right;
    return t;
}

/* Shape 3 again: return the best single-direction extension from t; record the
   best path through t (both directions) in *best. max(0, .) drops bad branches. */
static long max_path_gain(const TreeNode *t, long *best) {
    if (!t) return 0;
    long l = max_path_gain(t->left, best);
    long r = max_path_gain(t->right, best);
    if (l < 0) l = 0;
    if (r < 0) r = 0;
    if (t->val + l + r > *best) *best = t->val + l + r;
    return t->val + (l > r ? l : r);
}

/* ------------------------------------------------------------------------- */
/* 6. Construction and serialisation                                          */
/* ------------------------------------------------------------------------- */

/* Preorder + inorder, index ranges only (no sub-array copies), O(1) split lookup
   through pos[] (value -> inorder index; a hash table when values are not small ints).
   Half-open inorder range [i_lo, i_hi). Consumes exactly (i_hi - i_lo) preorder items. */
static TreeNode *build_pre_in(const int *pre, int p_lo,
                              int i_lo, int i_hi, const int *pos) {
    if (i_lo >= i_hi) return NULL;
    int root_val = pre[p_lo];
    int k = pos[root_val];
    int left_size = k - i_lo;
    TreeNode *t = node_new(root_val);
    t->left  = build_pre_in(pre, p_lo + 1,             i_lo,  k,    pos);
    t->right = build_pre_in(pre, p_lo + 1 + left_size, k + 1, i_hi, pos);
    return t;
}

/* Sorted array -> height-balanced BST: middle element is the root. */
static TreeNode *build_balanced(const int *a, int lo, int hi) {     /* [lo, hi) */
    if (lo >= hi) return NULL;
    int mid = lo + (hi - lo) / 2;                                    /* no (lo+hi)/2 overflow */
    TreeNode *t = node_new(a[mid]);
    t->left  = build_balanced(a, lo, mid);
    t->right = build_balanced(a, mid + 1, hi);
    return t;
}

/* Serialise: preorder, "#" for NULL, space separated. Buffer must be large enough:
   n * 12 (int + space) + (n + 1) * 2 (markers) + 1. */
static void serialize(const TreeNode *t, char *buf, size_t cap, size_t *len) {
    if (*len >= cap) return;
    if (!t) { *len += (size_t)snprintf(buf + *len, cap - *len, "# "); return; }
    *len += (size_t)snprintf(buf + *len, cap - *len, "%d ", t->val);
    serialize(t->left, buf, cap, len);
    serialize(t->right, buf, cap, len);
}

/* Deserialise from the SAME stream position for every recursive call: *cursor is
   shared, so the left subtree's consumption moves the right subtree's start. */
static TreeNode *deserialize(const char **cursor) {
    while (**cursor == ' ') (*cursor)++;
    if (**cursor == '\0') return NULL;
    if (**cursor == '#') { (*cursor)++; return NULL; }
    char *end;
    long v = strtol(*cursor, &end, 10);
    *cursor = end;
    TreeNode *t = node_new((int)v);
    t->left  = deserialize(cursor);
    t->right = deserialize(cursor);
    return t;
}

/* ------------------------------------------------------------------------- */
/* main: a neutral 7-node BST                                                 */
/* ------------------------------------------------------------------------- */

int main(void) {
    /* Insertion order chosen to give a bushy tree (NOT height-balanced: node 10
       has left height 0 and right height 2 -- the sentinel check below reports this):
                 8
               /   \
              3     10
             / \      \
            1   6      14
               / \    /
              4   7  13                                                   */
    const int keys[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    const size_t nkeys = sizeof keys / sizeof keys[0];
    TreeNode *root = NULL;
    for (size_t i = 0; i < nkeys; i++) root = bst_insert(root, keys[i]);   /* assign the result! */
    size_t n = tree_size(root);

    printf("=== 0. Tree (sideways: right subtree above, left below) ===\n");
    tree_print(root, 0);
    printf("size %zu\n\n", n);

    printf("=== 1. Recursion shapes ===\n");
    printf("height (result flows up):            %d\n", height(root));
    printf("root-to-leaf sum 17 exists (state down)? %s   (8+3+6 = 17, but 6 is not a leaf)\n",
           has_root_to_leaf_sum(root, 17) ? "yes" : "no");
    printf("root-to-leaf sum 21 exists?              %s   (8+3+6+4)\n",
           has_root_to_leaf_sum(root, 21) ? "yes" : "no");
    int best = 0;
    int h = height_and_diameter(root, &best);
    printf("diameter (side effect) = %d edges, while the function returned height %d\n", best, h);
    printf("balanced (sentinel -1 trick)?         %s\n", height_or_minus1(root) >= 0 ? "yes" : "no");
    printf("same_tree(root, root)?                %s\n\n", same_tree(root, root) ? "yes" : "no");

    printf("=== 2. Traversals ===\n");
    int *buf = malloc(n * sizeof *buf);
    if (!buf) { perror("malloc"); return 1; }
    size_t cnt;
    cnt = 0; preorder(root, buf, &cnt);             print_arr("preorder  (N L R):", buf, cnt);
    cnt = 0; inorder(root, buf, &cnt);              print_arr("inorder   (L N R) = sorted:", buf, cnt);
    cnt = 0; postorder(root, buf, &cnt);            print_arr("postorder (L R N):", buf, cnt);
    cnt = 0; inorder_iter(root, n, buf, &cnt);      print_arr("inorder, explicit stack:", buf, cnt);
    cnt = 0; postorder_iter(root, n, buf, &cnt);    print_arr("postorder, reversed N-R-L:", buf, cnt);
    printf("\n");

    printf("=== 3. Level-order BFS (level_size snapshot) ===\n");
    bfs_levels(root, n);
    printf("\n");

    printf("=== 4. BST invariant ===\n");
    printf("search 7:  %s\n", bst_search(root, 7) ? "found" : "absent");
    printf("search 5:  %s\n", bst_search(root, 5) ? "found" : "absent");
    printf("valid BST? %s\n", bst_valid(root, NULL, NULL) ? "yes" : "no");
    int kth;
    if (bst_kth_smallest(root, n, 3, &kth)) printf("3rd smallest (early-exit inorder): %d\n", kth);
    /* Break the invariant deliberately, validate, restore. */
    TreeNode *six = bst_search(root, 6);
    six->val = 9;                                       /* 9 in the left subtree of 8: invalid */
    printf("after setting node 6 -> 9 (child check 3<9 passes, subtree check fails): valid? %s\n",
           bst_valid(root, NULL, NULL) ? "yes" : "no");
    six->val = 6;
    /* Delete: all three cases. */
    root = bst_delete(root, 13);                        /* case 1: leaf */
    root = bst_delete(root, 10);                        /* case 2: one child (14) */
    root = bst_delete(root, 3);                         /* case 3: two children -> successor 4 */
    cnt = 0; inorder(root, buf, &cnt);
    print_arr("inorder after del 13,10,3:", buf, cnt);
    printf("still valid? %s, size %zu\n", bst_valid(root, NULL, NULL) ? "yes" : "no", tree_size(root));
    tree_print(root, 0);
    printf("\n");

    printf("=== 5. LCA and path sums ===\n");
    TreeNode *p = bst_search(root, 1), *q = bst_search(root, 7);
    TreeNode *l1 = lca_general(root, p, q);
    TreeNode *l2 = lca_bst(root, 1, 7);
    printf("LCA(1, 7): general-tree recursion -> %d, BST navigation -> %d\n", l1->val, l2->val);
    TreeNode *l3 = lca_general(root, bst_search(root, 4), bst_search(root, 6));
    printf("LCA(4, 6) where 4 is an ancestor of 6 -> %d\n", l3->val);
    long best_path = -1000000000L;
    long gain = max_path_gain(root, &best_path);
    printf("max path sum through any node = %ld (function returned one-direction gain %ld at root)\n",
           best_path, gain);
    printf("\n");

    printf("=== 6. Build from traversals, serialise, round-trip ===\n");
    const int pre[] = {3, 9, 20, 15, 7};
    const int in[]  = {9, 3, 15, 20, 7};
    const int m = 5;
    int pos[32] = {0};                                  /* value -> inorder index; values < 32 */
    for (int i = 0; i < m; i++) pos[in[i]] = i;
    TreeNode *built = build_pre_in(pre, 0, 0, m, pos);
    tree_print(built, 0);
    int chk[8]; size_t c = 0;
    preorder(built, chk, &c);  print_arr("rebuilt preorder:", chk, c);
    c = 0; inorder(built, chk, &c); print_arr("rebuilt inorder:", chk, c);

    char ser[256]; size_t len = 0;
    serialize(built, ser, sizeof ser, &len);
    printf("serialised: \"%s\"\n", ser);
    const char *cursor = ser;
    TreeNode *copy = deserialize(&cursor);
    printf("deserialised == original? %s\n", same_tree(built, copy) ? "yes" : "no");

    const int sorted[] = {1, 2, 3, 4, 5, 6, 7};
    TreeNode *bal = build_balanced(sorted, 0, 7);
    printf("balanced BST from sorted array of 7: height %d, valid %s\n",
           height(bal), bst_valid(bal, NULL, NULL) ? "yes" : "no");

    /* Every tree freed exactly once. */
    tree_free(root);
    tree_free(built);
    tree_free(copy);
    tree_free(bal);
    free(buf);
    return 0;
}
