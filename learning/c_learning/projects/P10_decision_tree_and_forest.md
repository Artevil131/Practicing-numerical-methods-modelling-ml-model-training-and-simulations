# P10 — Decision Tree and Random Forest

**Difficulty:** ★★★☆☆   **Prereq chapters:** C 07, 10 (plus 01-06)   **Builds on:** P05 (CSV loader)

## Goal

A CART classifier: recursively split a dataset on the (feature, threshold) pair that minimizes weighted Gini impurity, stop at a max depth or minimum leaf size, predict by walking the tree. Then a random forest: $B$ trees each trained on a bootstrap sample with a random feature subset per split, predicting by majority vote. Tested on Iris (4 features, 3 classes, 150 rows) and a harder synthetic dataset, compared against sklearn.

## Why

Trees are the recursive data structure of C 10 doing real work: a node is a `struct` with two child pointers, built by a recursive function, freed by a recursive function. The same "build a tree by splitting a set" pattern is the Barnes–Hut quadtree of P15/C++ P07. Bootstrap sampling and the "average many noisy models" idea return in the C++ P11 gradient-boosting project, which starts from this tree. And trees are the model that wins most tabular ML competitions — worth knowing from the inside.

## The math

Classes $c \in \{0, \dots, C-1\}$. For a set $S$ with class proportions $p_c$:

$$\text{Gini}(S) = 1 - \sum_{c} p_c^2 \qquad (\text{0 = pure, } 1 - 1/C \text{ = uniform})$$

A split on feature $j$ at threshold $\tau$ partitions $S$ into $S_L = \{x : x_j \le \tau\}$ and $S_R$. Its cost is the weighted impurity

$$G(j, \tau) = \frac{|S_L|}{|S|}\,\text{Gini}(S_L) + \frac{|S_R|}{|S|}\,\text{Gini}(S_R)$$

Choose $(j^*, \tau^*) = \arg\min G$. Candidate thresholds: midpoints between consecutive *distinct sorted* values of feature $j$ within $S$. Sorting $S$ by feature $j$ once and sweeping the threshold left to right lets you update the left/right class counts incrementally, so the best split of one feature costs $O(n \log n)$ instead of $O(n^2)$.

Stopping: depth $=$ `max_depth`, or $|S| <$ `min_samples_split`, or $S$ pure, or no split improves impurity. Leaf prediction $=$ majority class (store the full class histogram so you can also output probabilities).

**Random forest.** For $b = 1..B$: draw $n$ indices with replacement (bootstrap); grow a tree where at each node only $m$ randomly chosen features are considered ($m = \lceil\sqrt{d}\rceil$ by default); no depth limit (or a large one). Predict by majority vote over trees (or average the class histograms). Out-of-bag (OOB) error: each sample is left out of ~$1/e \approx 37\%$ of the bootstraps; predict it using only those trees to get a free validation estimate.

Entropy as an alternative criterion: $H(S) = -\sum_c p_c \log_2 p_c$; information gain is the corresponding weighted difference.

## Spec

**CLI**

```
./tree train.csv [test.csv] --max_depth 5 --min_leaf 1 [--criterion gini|entropy] [--print]
./forest train.csv [test.csv] --trees 100 --max_features 2 --seed 0 [--oob]
```

CSV format: header line, numeric features, **last column is the integer class label**. If `test.csv` is omitted, do an 80/20 split with the given seed and report accuracy on the 20%.

Iris: `python3 -c "from sklearn.datasets import load_iris; import pandas as pd; d=load_iris(); df=pd.DataFrame(d.data, columns=['f0','f1','f2','f3']); df['y']=d.target; df.to_csv('iris.csv', index=False)"`.

**Signatures** (`tree.h`, `tree.c`, `forest.c`):

```c
typedef struct Node {
    int    feature;        /* -1 for leaf */
    double threshold;
    struct Node *left, *right;
    int   *class_counts;   /* length n_classes (leaf: histogram; internal: may be NULL) */
    int    prediction;     /* majority class */
} Node;

typedef struct {
    int max_depth, min_samples_leaf, n_classes, max_features /* 0 = all */;
    unsigned seed;
    int criterion;         /* 0 gini, 1 entropy */
} TreeParams;

double gini(const int *counts, int n_classes, int total);
double entropy(const int *counts, int n_classes, int total);

/* find the best split over the rows given by idx[0..n) considering the listed features */
int    best_split(const Matrix *X, const int *y, const size_t *idx, size_t n,
                  const int *features, int n_features, int n_classes, int criterion,
                  int *out_feature, double *out_threshold, double *out_score);

Node  *tree_build(const Matrix *X, const int *y, size_t *idx, size_t n, int depth, const TreeParams *p);
int    tree_predict(const Node *t, const double *x);
void   tree_free(Node *t);
void   tree_print(const Node *t, int depth, FILE *out);      /* indented text dump */
int    tree_depth(const Node *t);
int    tree_n_leaves(const Node *t);

typedef struct { Node **trees; int n_trees; int n_classes; } Forest;
Forest *forest_train(const Matrix *X, const int *y, int n_trees, const TreeParams *p,
                     int **out_oob_mask /* n_trees x n bytes, optional */);
int     forest_predict(const Forest *f, const double *x);
double  forest_oob_error(const Forest *f, const Matrix *X, const int *y, const int *oob_mask);
void    forest_free(Forest *f);
```

**Output**

```
$ ./tree iris.csv --max_depth 3 --print
|-- f2 <= 2.4500  [50 50 50]
|   |-- leaf -> 0  [50 0 0]
|   |-- f3 <= 1.7500  [0 50 50]
|   |   |-- f2 <= 4.9500  ...
train acc 0.9733  test acc 0.9667  depth 3  leaves 5
```

## Milestones

1. **M1 — Gini and the best split on one feature.** On Iris feature 2 (petal length) with all 150 rows, the best threshold is 2.45 with left node pure class 0. You'll know it works when it matches `DecisionTreeClassifier(max_depth=1)`'s root.
2. **M2 — recursive build + predict + print.** Depth-3 tree on Iris matches sklearn's tree structure exactly (same features and thresholds at each node, since Gini + midpoints is what sklearn does; ties may differ). Training accuracy ~97%.
3. **M3 — unlimited depth.** 100% training accuracy (memorized), lower test accuracy; `tree_free` is leak-free under ASan; `tree_depth` and `tree_n_leaves` reported.
4. **M4 — a harder dataset.** Generate 2000 rows, 10 features, 3 classes with `sklearn.datasets.make_classification(n_informative=5, random_state=0)`. Single tree test accuracy ~80%; plot test accuracy vs `max_depth` to CSV.
5. **M5 — random forest.** 100 trees, `max_features = 3`: test accuracy jumps to ~90%; accuracy vs number of trees curve flattens after ~50. Bootstrap with `rand()` seeded per tree.
6. **M6 — OOB error** within ~2% of the held-out test error; sklearn comparison (Verification).

## Verification

```python
import pandas as pd, numpy as np
from sklearn.tree import DecisionTreeClassifier, export_text
from sklearn.ensemble import RandomForestClassifier
d = pd.read_csv("iris.csv"); X, y = d.iloc[:, :-1].values, d.y.values
t = DecisionTreeClassifier(max_depth=3, random_state=0).fit(X, y)
print(export_text(t, feature_names=list(d.columns[:-1])))     # compare with your --print
print(t.score(X, y))                                          # your train acc
h = pd.read_csv("hard_train.csv"); Xt = pd.read_csv("hard_test.csv")
rf = RandomForestClassifier(100, max_features=3, oob_score=True, random_state=0)
rf.fit(h.iloc[:, :-1], h.y)
print(rf.oob_score_, rf.score(Xt.iloc[:, :-1], Xt.y))         # your OOB acc / test acc within ~2%
```

## Stretch goals

- Regression trees (predict the mean; split on variance reduction) — required for C++ P11 gradient boosting.
- Feature importance: total impurity decrease per feature, normalized; compare with `rf.feature_importances_`.
- Cost-complexity pruning with a validation set.
- Save/load a tree as a text file (pre-order traversal) and reload it into an identical structure.

## Hints

- Work with an index array `idx` into the rows of `X`, never copies of the data. Partitioning a node's rows into left/right is an in-place partition of `idx` (like quicksort's), so children get contiguous sub-ranges.
- For `best_split`, sort a per-feature copy of the indices by that feature's value (`qsort` with a comparator that needs the feature column — use a `static` global or sort an array of `(value, label)` structs). Then sweep: move one sample from right counts to left counts, and evaluate the split only where the value changes.
- Store `class_counts` on every node during build; it makes `tree_print` informative and the forest's soft voting trivial.
- `min_samples_leaf`: skip candidate thresholds that leave fewer than that many rows on either side.
- Random feature subset per node: Fisher–Yates shuffle a `0..d-1` array and take the first `m`.
- Recursion depth is at most `max_depth` (or $n$ for unlimited) — no stack problem for these sizes, but write `tree_free` as post-order recursion and check with ASan.

## Where to put it

`neural_network_c/trees/` — `tree.h`, `tree.c`, `forest.c`, `main_tree.c`, `main_forest.c`, `Makefile` (uses `../matrix` and `../linreg/csv.c`), `gen_hard.py`.
