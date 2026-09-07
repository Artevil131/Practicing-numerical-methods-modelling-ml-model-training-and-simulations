# Chapter 08 — Exercises

Write each exercise as `ex08_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex08_K ex08_K.cpp`. Zero warnings is part of the task.

---

**08.1 — Capture semantics, predicted then observed**
Write `main` with `double a = 1, b = 2;` and four lambdas: `f1 = [a, b]`, `f2 = [&a, &b]`, `f3 = [=]`, `f4 = [&]`, each returning `a + b`. Then set `a = 10; b = 20;` and call all four. Before running, write your predicted outputs in a comment; after running, confirm. Add a fifth lambda `[n = 0]() mutable { return ++n; }`, call it three times, and print the results. Print `sizeof` of each of the five lambdas and explain each number in a comment.

Example: `f1() = 3`, `f2() = 30`.

<details><summary>Hint</summary>
By-value captures freeze at creation; by-reference see the update. `sizeof` a by-value lambda is the sum of its captured members; a by-reference capture stores one pointer per variable.
</details>

---

**08.2 — Three ways to hold a callable**
Write a `template <typename F> double apply_n(F f, double x, int n)` that applies `f` `n` times. Then write the same as `double apply_n_fn(const std::function<double(double)>& f, double x, int n)` and as a C-style `double apply_n_c(double (*f)(double), double x, int n)`. Call each with `[](double v) { return 0.5 * v; }` and `x = 1024, n = 10`. Then try to pass a lambda that *captures* a local scale factor to `apply_n_c` and record the compiler error in a comment (why does the captureless one convert and this one not?). Time the three versions for `n = 100'000'000` with `<chrono>` and report the ratio.

Example: all three print `1`.

<details><summary>Hint</summary>
Only captureless lambdas convert to a function pointer. The template version inlines; `std::function` and the function pointer make an indirect call per iteration (the compiler may still optimise some of this — report what you measure).
</details>

---

**08.3 — Argsort, argmax, median with algorithms**
Given `std::vector<double> v = {3.2, -1.0, 7.5, 0.0, 7.5, 2.2}`, print (a) the index of the max (`max_element`), (b) the argsort (`iota` + `sort` with a comparator capturing `v` by reference), (c) the median via `nth_element` on a copy (for even sizes, average the two middle values — you need two `nth_element` calls or one plus `max_element` on the lower half), (d) the top-2 values via `partial_sort`, (e) the count of values `> 1.0` via `count_if`. Every operation must be an algorithm call, not a hand loop.

Example: argmax `2`; argsort `1 3 5 0 2 4` (or `1 3 5 0 4 2` — equal keys; say which and why `stable_sort` would fix it).

<details><summary>Hint</summary>
`nth_element(v.begin(), v.begin()+k, v.end())` puts the k-th smallest at position k and partitions around it; the max of `[begin, begin+k)` is then the (k-1)-th smallest.
</details>

---

**08.4 — Erase-remove and friends**
Start with `std::vector<double> v = {1, NAN, 2, 2, INFINITY, 3, NAN, 3, 3}`. (a) Remove all non-finite values with `remove_if` + `erase` (use `std::isfinite`). (b) Deduplicate with `sort` + `unique` + `erase`. (c) Deliberately call `remove_if` *without* `erase` on a fresh copy and print `size()` and contents — describe in a comment what you see at the tail and why the values there are unspecified. (d) Use `copy_if` with `std::back_inserter` to build a new vector of the even integers from `iota`-generated `1..20`.

Example: after (a) and (b): `1 2 3`.

<details><summary>Hint</summary>
`remove_if` shifts kept elements forward and returns the new logical end; elements past it are valid objects with unspecified values (for `double`, whatever was moved from). `unique` only collapses adjacent runs.
</details>

---

**08.5 — `<random>` done right, and done wrong**
Write `double gauss_sample(std::mt19937_64& rng)` that returns `N(0,1)` using `std::normal_distribution`. Draw 100 000 samples with ONE engine seeded with `42`, and print the sample mean and standard deviation (use `accumulate` and `inner_product`). Then write the buggy version `double gauss_sample_bad()` that constructs `std::mt19937_64 rng(42)` *inside* the function, draw 10 samples, and print them to show they are identical. Finally show that two runs with `std::random_device{}()` as seed differ (print the first three samples of each).

Example: mean ≈ `0.00x`, std ≈ `1.00x`.

<details><summary>Hint</summary>
`std = sqrt(inner_product(v,v,0.0)/n - mean*mean)` is fine here. Pass the engine by non-const reference — distributions must advance its state.
</details>

---

**08.6 — Generic `integrate` with lambdas**
Write `template <typename F> double simpson(F f, double a, double b, int n)` (n even) implementing composite Simpson's rule. Integrate: (a) `[](double x){ return x*x; }` on `[0,1]` (exact `1/3`); (b) a Gaussian density with `mu`, `sigma` captured by reference on `[-5σ, 5σ]` (≈ `1`); (c) a `std::function<double(double)>` chosen at runtime from a `std::vector` of lambdas by a command-line index. Print the absolute error for (a) and (b) with `n = 10, 100, 1000` and confirm the `O(h^4)` convergence (error drops ~10 000x per 10x in n, until roundoff).

<details><summary>Hint</summary>
Simpson: `h/3 * (f0 + 4 f1 + 2 f2 + 4 f3 + ... + fn)`. The template inlines the lambda; the `std::function` version in (c) is the same code with one type-erased call per evaluation.
</details>

---

**08.7 — Sorting records with comparators**
Define `struct Sample { double x; int label; std::string name; }`. Fill a vector with 8 samples (some sharing labels). (a) `sort` by `x` descending. (b) `stable_sort` by `label` — then print and check that within each label the `x`-descending order from (a) survived. (c) Sort by `(label, name)` lexicographically using `std::tie`. (d) Write a comparator using `<=` instead of `<`, feed a vector of 50 *equal* values to `std::sort`, and observe what happens (it may crash, loop, or appear to work — UB). Explain in a comment why `<=` is not a strict weak ordering.

<details><summary>Hint</summary>
`return std::tie(a.label, a.name) < std::tie(b.label, b.name);` compares lexicographically. For (d) build with `-fsanitize=address` to catch the out-of-bounds read that libc++'s `sort` performs when the comparator lies.
</details>

---

**08.8 — Softmax and cross-entropy with algorithms (ML)**
Write `std::vector<double> softmax(const std::vector<double>& logits)` using: `max_element` (stability shift), `transform` (exp), `accumulate` (sum), `transform` (divide). Write `double cross_entropy(const std::vector<double>& probs, int target)`. Then `int predict(const std::vector<double>& logits)` via `max_element`. Test on `logits = {2.0, 1.0, 0.1}`: softmax `≈ {0.659, 0.242, 0.099}`, `predict = 0`, `cross_entropy(softmax(logits), 0) ≈ 0.417`. Confirm the probabilities sum to `1` within `1e-12` and that shifting all logits by `+1000` gives the same result (no overflow).

<details><summary>Hint</summary>
Subtract the max before `exp`. Use `std::transform` with an output iterator into a pre-sized vector; capture the max and the sum by value in the lambdas.
</details>

---

**08.9 — Minibatch SGD data pipeline (ML)**
Generate `n = 1000` points `(x, y)` with `y = 3x - 2 + noise`, `x ~ U(-1,1)`, `noise ~ N(0, 0.1)` using one `mt19937_64`. Write an index vector with `iota`, and for each of 20 epochs: `shuffle` the indices, iterate over minibatches of 32 (use iterators `first, last` into the index vector — `std::min` for the last partial batch), compute the gradient of MSE w.r.t. `(w, b)` with `accumulate` over the batch (accumulate a `std::pair<double,double>` with a lambda as the binary op), and update with `lr = 0.1`. Print `(w, b)` after each epoch; they should converge to ≈ `(3, -2)`. Then use `transform_reduce` to compute the final MSE over all points.

<details><summary>Hint</summary>
`std::accumulate(first, last, std::pair<double,double>{0,0}, [&](auto acc, std::size_t i) { double e = w*x[i]+b - y[i]; acc.first += e*x[i]; acc.second += e; return acc; })`. Divide by the batch size before the update.
</details>

---

**08.10 — Histogram, CDF and inverse-transform sampling (numerics / sims)**
Build a discrete probability table for a PIC/Monte-Carlo style sampler: (a) `std::vector<double> weights = {0.1, 0.5, 0.2, 0.15, 0.05}`; compute the CDF with `std::partial_sum`, and normalise so the last entry is exactly `1.0`. (b) Write `std::size_t sample(const std::vector<double>& cdf, std::mt19937_64& rng)` using `uniform_real_distribution` + `lower_bound` (this is inverse-transform sampling). (c) Draw 1 000 000 samples, histogram them with a `std::vector<std::size_t> counts(5)` (a plain loop is correct here — say why an algorithm is not the natural choice), and print observed frequencies next to the weights. (d) Use `std::max_element` on `|freq - weight|` to report the largest deviation; it should be < `0.002`. (e) Bonus: `std::transform_reduce` to compute the entropy `-Σ w log w` of the table.

Example row: `bin 1  weight 0.500  observed 0.4997`.

<details><summary>Hint</summary>
`lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin()` gives the first bin whose cumulative weight exceeds `u`. The histogram loop needs an index derived from data (`counts[sample(...)]++`) — a scatter, which no single algorithm expresses cleanly.
</details>
