# P07 — k-Means and k-NN

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C 10 (plus 01-09)   **Builds on:** P04, P02

## Goal

Two gradient-free algorithms on MNIST pixels: k-means (Lloyd's algorithm) whose 10 cluster centers you write out as PGM images and inspect, and a k-nearest-neighbours classifier that reaches ~97% test accuracy using a binary heap to keep the top-k neighbours. Both are dominated by one loop — squared distances between 784-D vectors — which you will make fast.

## Why

Distance loops and argmin over $n \times k$ are the workhorse of the Barnes–Hut neighbour search (P15) and the charge-deposition loops of the C++ PIC sim. The heap is the data structure from C 10 you will need for the priority queue in the P03 stretch and the event queue in later sims. k-means initialization (k-means++) and the "assign / update" iteration are the pattern of expectation-maximization, and the cluster-mean images are the first time you *see* structure a model found. k-NN gives you a strong non-parametric baseline to beat with P11/P12.

## The math

Squared Euclidean distance between $x, c \in \mathbb{R}^{784}$:

$$d^2(x, c) = \lVert x - c\rVert^2 = \lVert x\rVert^2 - 2\,x^Tc + \lVert c\rVert^2$$

The expanded form lets you compute all $n \times k$ distances as one matmul $-2XC^T$ plus two precomputed norm vectors — many times faster than $n \cdot k$ separate loops of 784 subtractions. (Same trick sklearn uses.)

**k-means objective**: $J = \sum_{i} \lVert x_i - c_{a(i)}\rVert^2$ where $a(i) = \arg\min_j d^2(x_i, c_j)$.

Lloyd's algorithm: repeat (1) assign each point to its nearest center, (2) set each center to the mean of its assigned points, until assignments stop changing (or $J$ decreases by less than a tolerance). $J$ is non-increasing every iteration — assert it.

**k-means++ initialization**: pick the first center uniformly; each next center is sampled with probability proportional to $D(x)^2$, the squared distance to the nearest already-chosen center.

**k-NN**: for a query $q$, find the $k$ training points with smallest $d^2(q, x_i)$, predict the majority label (ties: smallest label, or weight by $1/d$). A max-heap of size $k$ keyed on distance: push while size $< k$; otherwise if the new distance is smaller than the heap max, replace the max and sift down. $O(n \log k)$ per query instead of $O(n \log n)$ for a full sort.

## Spec

**CLI**

```
./kmeans <images> <labels> <k> <max_iter> <seed> outdir/     # centers as outdir/center_j.pgm, prints J per iter
./knn <train_images> <train_labels> <test_images> <test_labels> <k> [n_test]   # accuracy, confusion matrix
```

**Signatures** (`kmeans.c`, `knn.c`, `heap.c`/`heap.h`, `dist.c`/`dist.h`):

```c
/* distances */
double  sqdist(const double *a, const double *b, size_t d);
Matrix *sqdist_all(const Matrix *X, const Matrix *C);      /* n x k via the expanded form */

/* k-means */
void    kmeans_init_pp(const Matrix *X, Matrix *C, unsigned seed);         /* C: k x d */
double  kmeans_assign(const Matrix *X, const Matrix *C, int *assign);      /* returns J */
void    kmeans_update(const Matrix *X, const int *assign, Matrix *C);      /* means; empty cluster: keep old */
int     kmeans_fit(const Matrix *X, Matrix *C, int *assign, int max_iter, double tol, FILE *log);
double  cluster_purity(const int *assign, const int *labels, size_t n, int k);

/* bounded max-heap of (distance, label) */
typedef struct { double dist; int label; } Neighbor;
typedef struct { Neighbor *a; size_t size, cap; } MaxHeap;
void    heap_init(MaxHeap *h, size_t cap);
void    heap_push_bounded(MaxHeap *h, Neighbor nb);   /* keeps the cap smallest */
void    heap_free(MaxHeap *h);

int     knn_predict(const Matrix *Xtr, const int *ytr, const double *q, int k, MaxHeap *scratch);
```

**Output**

```
$ ./kmeans train-images train-labels 10 50 0 out/
iter 0  J 2.41e6  changed 60000
iter 1  J 1.98e6  changed 12034
...
iter 23 J 1.91e6  changed 0
purity 0.59
$ ./knn train-images train-labels t10k-images t10k-labels 3 10000
accuracy 0.9705   (3 neighbors, 10000 queries, 41.2 s)
confusion matrix:
  974    1    1    0    0    1    2    1    0    0
  ...
```

## Milestones

1. **M1 — `sqdist` and `sqdist_all` agree.** On random 100x784 vs 10x784, the matmul form matches the loop form to 1e-9. Time both on 60000x10: the matmul form should be > 5x faster.
2. **M2 — k-means with random init on 10 clusters.** $J$ strictly non-increasing (assert it). Terminates when `changed == 0`. Centers written as PGMs look like blurry digits (some merged, e.g. 4/9, some duplicated).
3. **M3 — k-means++.** Fewer iterations to converge and lower final $J$ on average over 5 seeds than random init. Print purity (~0.55-0.65 for $k=10$; ~0.75 for $k=30$).
4. **M4 — heap.** Push 1000 random distances with cap 5; the heap contains exactly the 5 smallest (check against `qsort`). Sift-down and sift-up tested individually.
5. **M5 — k-NN on 1000 test queries.** $k = 3$: ~97% accuracy. Matches sklearn's `KNeighborsClassifier(3)` predictions on the same 1000 queries at least 99.5% of the time (ties can differ).
6. **M6 — full 10000 queries with the matmul distance trick in blocks** of 1000 queries; confusion matrix printed; total time under a minute.

## Verification

```python
import numpy as np
from sklearn.cluster import KMeans
from sklearn.neighbors import KNeighborsClassifier
# X, y, Xt, yt from the P04 reader; X in [0,1]
km = KMeans(10, init="k-means++", n_init=1, random_state=0).fit(X)
print(km.inertia_)                # your final J should be in the same range (±5%; different seeds)
knn = KNeighborsClassifier(3).fit(X, y)
pred = knn.predict(Xt[:1000])
print((pred == yt[:1000]).mean())  # ~0.97; compare row by row with your predictions file
# heap check: your top-5 for query 0 should equal
d = ((X - Xt[0])**2).sum(1); print(np.sort(d)[:5], y[np.argsort(d)[:5]])
```

## Stretch goals

- Elbow plot: final $J$ vs $k$ for $k \in \{2, 5, 10, 20, 30, 50\}$ to CSV.
- Use the 10 cluster centers as a classifier (label each cluster by majority vote) — accuracy ~60%; with $k=100$ clusters ~90%.
- k-NN with a k-d tree — then discover why it barely helps in 784 dimensions (curse of dimensionality); measure.
- PCA to 50 dimensions first (power iteration for top eigenvectors of the covariance — a preview of C++ P06), then k-NN: nearly the same accuracy, 15x faster.

## Hints

- Keep the data as one `Matrix`; pass row pointers `X->data + i*X->cols` to `sqdist` rather than copying rows.
- `sqdist_all`: precompute `xn[i] = ||x_i||^2` and `cn[j] = ||c_j||^2`, compute `G = X C^T` with `mat_matmul`, then `D[i][j] = xn[i] - 2 G[i][j] + cn[j]`. Clamp tiny negatives to 0.
- `kmeans_update`: zero the centers, accumulate sums and counts in one pass over the points, divide. An empty cluster has count 0 — leave its old center (or re-seed it to a random point).
- For k-means++ sampling: compute `D2[i]`, its cumulative sum, draw `u * total`, and binary-search the cumulative array.
- Heap indices: children of `i` are `2i+1`, `2i+2`; parent is `(i-1)/2`. Write `sift_down` and `sift_up` as separate static functions and test them before the bounded push.
- Majority vote from a heap: count labels in an `int[10]`, pick argmax. Don't sort the heap.

## Where to put it

`neural_network_c/kmeans_knn/` — `kmeans.c`, `knn.c`, `dist.c/.h`, `heap.c/.h`, `Makefile` (links `../matrix/matrix.o`, `../mnist/mnist.o`, `../mnist/image_io.o`).
