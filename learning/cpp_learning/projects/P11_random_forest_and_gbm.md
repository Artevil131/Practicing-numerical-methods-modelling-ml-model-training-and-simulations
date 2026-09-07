# P11 — Random Forest and Gradient Boosting

**Difficulty:** ★★★★☆   **Prereq chapters:** C++ 05, 08 (plus 01-04, 09, 11)   **Builds on:** C P10, P02

## Goal

A templated decision tree over `Tensor<T>` supporting both classification (Gini) and regression (variance) via a policy/criterion template parameter; a random forest with out-of-bag error and feature importances; then gradient boosting machines — first with squared loss (regression), then with log loss (binary classification) — with shrinkage, subsampling and early stopping. Compared against `sklearn`'s `RandomForest*`, `GradientBoosting*` and a sanity check against `xgboost`/`lightgbm` defaults.

## Why

Boosting is what wins on tabular data, and it is *gradient descent in function space*: each tree fits the negative gradient of the loss with respect to the current predictions — the same derivatives you took in C P06/P11, now used to grow trees instead of update weights. The templated criterion (C++ 05) lets one tree class serve classification, regression, and the boosting residual fits; STL algorithms (`sort`, `partition`, `nth_element`, `accumulate`, lambdas — C++ 08) replace the hand-rolled loops of C P10. Histogram-based split finding (stretch) is the trick behind LightGBM's speed and a good excuse to learn `std::vector` layout tricks.

## The math

**Tree** as in C P10, generalized. For a node with sample set $S$ and a criterion $\mathcal C$:

- Classification (Gini): $\mathcal C(S) = 1 - \sum_c p_c^2$; leaf value = class histogram.
- Regression (variance / squared error): $\mathcal C(S) = \dfrac1{|S|}\sum_{i\in S}(y_i - \bar y)^2$; leaf value $= \bar y$. With sorted sweep, maintain running $\sum y$ and $\sum y^2$ on both sides: $\text{SSE}_L = \sum y^2 - (\sum y)^2/n_L$; $O(1)$ per candidate.

Split score $= \dfrac{|S_L|}{|S|}\mathcal C(S_L) + \dfrac{|S_R|}{|S|}\mathcal C(S_R)$, minimized.

**Random forest** (C P10): bootstrap + feature subsampling + averaging. OOB error: for sample $i$, average predictions of trees whose bootstrap excluded $i$. Feature importance: mean decrease in impurity, $\text{imp}_j = \sum_{\text{nodes splitting on } j}\dfrac{|S|}{n}\left[\mathcal C(S) - \text{score}\right]$, normalized across features and averaged over trees; or permutation importance on OOB samples (more honest).

**Gradient boosting.** Model $F_M(x) = F_0 + \eta\sum_{m=1}^M h_m(x)$ with shrinkage $\eta \in (0, 1]$. At stage $m$, with loss $\ell(y, F)$:

1. Pseudo-residuals $r_i = -\dfrac{\partial\ell(y_i, F)}{\partial F}\Big|_{F = F_{m-1}(x_i)}$.
2. Fit a regression tree $h_m$ to $\{(x_i, r_i)\}$ (depth 3–6, "weak").
3. Leaf values: for squared loss the mean of $r_i$ in the leaf is already optimal; for other losses do one Newton step per leaf: $\gamma_j = \dfrac{\sum_{i\in \text{leaf}_j} r_i}{\sum_{i\in\text{leaf}_j} h_i}$ where $h_i = \partial^2\ell/\partial F^2$.
4. $F_m = F_{m-1} + \eta\,h_m$.

*Squared loss* $\ell = \tfrac12(y - F)^2$: $r_i = y_i - F(x_i)$ (plain residuals), $h_i = 1$, $F_0 = \bar y$.

*Log loss* (binary, $y \in \{0, 1\}$, $F$ = logit, $p = \sigma(F)$): $\ell = -[y\log p + (1-y)\log(1-p)]$; $r_i = y_i - p_i$ (C P06's gradient); $h_i = p_i(1 - p_i)$; $F_0 = \log\dfrac{\bar y}{1 - \bar y}$. Leaf value $\gamma_j = \dfrac{\sum r_i}{\sum p_i(1-p_i)}$ — this is exactly what sklearn's `GradientBoostingClassifier` does.

Stochastic GBM: fit each tree on a random `subsample` fraction (0.5–0.8) of rows. Early stopping: track validation loss, stop after `patience` stages without improvement. Regularization knobs: `max_depth`, `min_samples_leaf`, `eta`, `subsample`, `n_estimators`. Multiclass log loss (stretch): $K$ trees per stage, softmax over $F_k$, $r_{ik} = y_{ik} - p_{ik}$.

**Bias–variance intuition to verify empirically.** Forest: deep trees (low bias), averaging cuts variance; more trees never hurt. Boosting: shallow trees (high bias), sequential fitting cuts bias; too many stages overfit unless $\eta$ is small — the $\eta$ vs $M$ trade-off ($\eta = 0.1, M = 500$ ≈ $\eta = 0.05, M = 1000$).

## Spec

**Interface** (`trees.hpp`, `forest.hpp`, `gbm.hpp`):

```cpp
struct GiniCriterion   { using Target = std::size_t; /* impurity(counts...), leaf value = argmax + histogram */ };
struct VarianceCriterion { using Target = double;    /* impurity from sum, sumsq, n; leaf value = mean */ };

template <typename T, typename Criterion>
class DecisionTree {
public:
    struct Params { int max_depth = -1; std::size_t min_samples_leaf = 1; std::size_t max_features = 0; unsigned seed = 0; double min_impurity_decrease = 0.0; };
    explicit DecisionTree(Params p = {});
    void fit(const Tensor<T>& X, const std::vector<typename Criterion::Target>& y,
             const std::vector<std::size_t>* row_subset = nullptr, const std::vector<double>* sample_weight = nullptr);
    std::vector<double> predict(const Tensor<T>& X) const;                 // class label or regression value
    std::vector<std::vector<double>> predict_proba(const Tensor<T>& X) const;   // classification only
    std::vector<std::size_t> apply(const Tensor<T>& X) const;             // leaf index per row (needed by GBM Newton step)
    void set_leaf_values(const std::vector<double>& values);               // overwrite leaf outputs (GBM)
    std::size_t n_leaves() const;  int depth() const;
    std::vector<double> feature_importances(std::size_t n_features) const;   // impurity-based, unnormalized
    void print(std::ostream&, const std::vector<std::string>* feature_names = nullptr) const;
private:
    struct Node { int feature = -1; T threshold; int left = -1, right = -1; double value; std::vector<double> dist; double impurity; std::size_t n; };
    std::vector<Node> nodes_;     // flat storage, children by index — cache-friendly and trivially serializable
};

template <typename T, typename Criterion>
class RandomForest {
public:
    struct Params { std::size_t n_trees = 100; typename DecisionTree<T, Criterion>::Params tree; bool bootstrap = true; unsigned seed = 0; int n_threads = 1; };
    void fit(const Tensor<T>& X, const std::vector<typename Criterion::Target>& y);
    std::vector<double> predict(const Tensor<T>& X) const;
    double oob_score() const;                                              // accuracy or R^2
    std::vector<double> feature_importances() const;                       // normalized, averaged
    std::vector<double> permutation_importance(const Tensor<T>& X, const std::vector<typename Criterion::Target>& y, unsigned seed) const;
};

struct SquaredLoss { static double loss(double y, double F); static double grad(double y, double F); static double hess(double y, double F); static double init(const std::vector<double>& y); static double transform(double F); };
struct LogLoss     { /* same interface; transform = sigmoid; init = logit(mean) */ };

template <typename T, typename Loss>
class GradientBoosting {
public:
    struct Params { std::size_t n_estimators = 200; double learning_rate = 0.1; int max_depth = 3; std::size_t min_samples_leaf = 5; double subsample = 1.0; unsigned seed = 0;
                    std::size_t early_stopping_patience = 0; };
    void fit(const Tensor<T>& X, const std::vector<double>& y, const Tensor<T>* X_val = nullptr, const std::vector<double>* y_val = nullptr);
    std::vector<double> predict_raw(const Tensor<T>& X, std::size_t n_stages = 0) const;   // F(x); 0 = all stages
    std::vector<double> predict(const Tensor<T>& X) const;                                  // Loss::transform(F)
    const std::vector<double>& train_curve() const;  const std::vector<double>& val_curve() const;
    std::size_t best_iteration() const;
};
```

**CLI**

```
./trees rf  train.csv [test.csv] --task clf|reg --trees 200 --max_features sqrt|0.3|3 --oob --importance
./trees gbm train.csv [test.csv] --task clf|reg --stages 500 --lr 0.1 --depth 3 --subsample 0.8 --early_stop 20 --curve curve.csv
./trees bench --n 100000 --d 20                    # fit time single vs multi-threaded forest; GBM stages/sec
```

CSV: header, features, last column target (integer for `clf`, float for `reg`). Datasets: `make_classification(20000, 20, n_informative=8)`, `make_friedman1(20000)` (regression, known true function), California housing (`fetch_california_housing`), and `load_breast_cancer`.

## Milestones

1. **M1 — templated tree, both criteria.** Iris depth-3 tree identical to C P10 and sklearn; regression tree on Friedman-1 with depth 6 gets test $R^2 \approx 0.85$; `apply` returns consistent leaf ids; `print` works for both.
2. **M2 — random forest + OOB + importances.** `make_classification`: test accuracy within 1% of sklearn's RF with the same params; OOB within 1% of test; impurity importances rank the 8 informative features first. `n_threads = 8` via `std::thread`/`std::async` over trees gives ≥ 5x.
3. **M3 — GBM squared loss.** Friedman-1: test MSE within 5% of `GradientBoostingRegressor(n_estimators=500, learning_rate=0.1, max_depth=3)`; `train_curve` decreases monotonically; val curve bottoms then rises for $\eta = 0.5$ (overfitting visible).
4. **M4 — GBM log loss with Newton leaves.** Breast cancer: test accuracy ≥ 96%, log loss within 0.01 of sklearn's; the `predict_raw` after 1 stage equals $F_0 + \eta\gamma$ exactly as sklearn computes it (verify on 5 rows).
5. **M5 — regularization study.** Grid over $\eta \in \{0.01, 0.05, 0.1, 0.3\}$, `subsample` $\in \{1, 0.5\}$, depth $\in \{2, 4, 6\}$ with early stopping; table to CSV; best config within noise of sklearn's best.
6. **M6 — California housing + speed.** Test $R^2 \approx 0.83$–$0.84$ with 1000 stages, $\eta = 0.05$, depth 6 (sklearn HistGradientBoosting gets ~0.84–0.85). Stages/sec reported; presort features once per tree (or globally) and reuse — ≥ 3x faster than re-sorting per node.

## Verification

```python
import numpy as np, pandas as pd
from sklearn.datasets import make_friedman1, load_breast_cancer
from sklearn.ensemble import GradientBoostingRegressor, GradientBoostingClassifier, RandomForestClassifier
from sklearn.metrics import mean_squared_error, log_loss
tr, te = pd.read_csv("f1_train.csv"), pd.read_csv("f1_test.csv")
g = GradientBoostingRegressor(n_estimators=500, learning_rate=0.1, max_depth=3, min_samples_leaf=5, random_state=0)
g.fit(tr.iloc[:, :-1], tr.y); print(mean_squared_error(te.y, g.predict(te.iloc[:, :-1])))   # your MSE within 5%
bc = load_breast_cancer(); X, y = bc.data, bc.target
c = GradientBoostingClassifier(n_estimators=1, learning_rate=0.1, max_depth=3, random_state=0).fit(X, y)
print(c.init_.predict_proba(X[:1]), c.decision_function(X[:5]))   # F0 = logit(mean y); stage-1 raw scores to compare with predict_raw(...,1)
rf = RandomForestClassifier(200, max_features="sqrt", oob_score=True, random_state=0).fit(Xtr, ytr)
print(rf.oob_score_, rf.score(Xte, yte), np.argsort(rf.feature_importances_)[::-1][:8])
```

## Stretch goals

- Histogram-based split finding: bin each feature into 256 quantile bins once; node statistics per bin; 10x faster on 100k+ rows (LightGBM's core idea).
- Multiclass GBM (softmax, $K$ trees per stage) on MNIST-PCA-50 — ~97%.
- Second-order (XGBoost-style) leaf values with L2 regularization $\lambda$: $\gamma_j = -\dfrac{\sum g_i}{\sum h_i + \lambda}$ and the corresponding gain formula for splits.
- SHAP-like per-feature contributions via the TreeSHAP path algorithm for a single tree, checked against the `shap` package.

## Hints

- Flat node storage (`std::vector<Node>`, children as indices) beats `unique_ptr` trees here: trivially copyable, serializable, cache-friendly; the tree is built breadth-first or depth-first into the vector.
- Sorting per (node, feature) is the cost center. Presort each feature's row indices once (`std::vector<std::vector<size_t>>` of argsorted rows) and, at each node, filter the presorted list by a membership bitmap — `std::copy_if` with a lambda. Or use the histogram approach in the stretch.
- The criterion as a template parameter with a small static interface (`init`, `add(y)`, `remove(y)`, `impurity()`, `leaf_value()`) lets `best_split` be written once. The running-sum trick means `add`/`remove` are $O(1)$ for both criteria.
- GBM leaf Newton step: after fitting the tree on residuals, call `apply` to get leaf ids, accumulate $\sum r$ and $\sum h$ per leaf with `std::vector<double>` indexed by leaf id, then `set_leaf_values`.
- Multithreaded forest: `std::vector<std::future<Tree>>` with `std::async(std::launch::async, ...)`, each with its own RNG seeded from `seed + tree_index`; results are deterministic regardless of thread count.
- Early stopping needs the validation raw scores maintained incrementally (`F_val += eta * h_m(X_val)`) — never recompute from scratch each stage.
- Match sklearn's conventions when comparing: `min_samples_leaf`, `max_features="sqrt"` for classification and `1.0` for regression, and its use of *midpoints* between sorted distinct values as thresholds.

## Where to put it

`cpp/ml/trees/` — `include/trees.hpp`, `include/forest.hpp`, `include/gbm.hpp`, `apps/trees.cpp`, `tests/test_trees.cpp`, `tests/gen_data.py`, `tests/compare_sklearn.py`, `CMakeLists.txt`.
