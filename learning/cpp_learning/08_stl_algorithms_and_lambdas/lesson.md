# Chapter 08 — STL Algorithms and Lambdas

## What you'll be able to do after this chapter

- Write a lambda with the right capture list, know what type it has, and store or pass it without paying for `std::function` unless you need type erasure.
- Replace the C course's `void (*cb)(double, void *ctx)` callbacks with lambdas and explain why the C++ version is both safer and faster.
- Use `<algorithm>` and `<numeric>` for the operations you would otherwise write as loops: sort with a comparator, argmax, dot product, map, filter, erase-remove, binary search, shuffle.
- Generate reproducible random numbers correctly with `<random>` (`mt19937_64`, distributions, seeding) — and never call `rand()` again.
- Decide, per loop, whether an algorithm or a hand-written loop is the clearer and faster choice for numerical code.

## Why this matters for ML / numerics / sims

Every ML program does argmax (prediction), dot products (every layer), sorting (decision-tree splits, k-NN, percentiles), shuffling (SGD minibatches), filtering (masking), and weight initialisation with Gaussian noise. In the C course you wrote each of these as a loop with a function pointer and a `void *` for context. The STL gives you tested, `O(n log n)`-guaranteed versions that inline your comparison or transform. Lambdas are the glue: they let you hand a piece of code with local state to an algorithm in one line. They are also what `std::function<void()> backward_fn` in an autograd node will hold — micrograd's `_backward` closure, in C++.

## 1. Lambda syntax

```
[captures](parameters) -> return_type { body }
```

- `[captures]` — which outer variables the body may use, and how (section 2). Empty `[]` means none.
- `(parameters)` — like a function. May be omitted if empty *and* there is no `->` or specifier.
- `-> return_type` — optional; deduced from `return` statements when omitted (all must agree).
- `{ body }` — normal statements.

```cpp
auto square = [](double x) { return x * x; };          // return type deduced: double
auto relu   = [](double x) -> double { return x > 0 ? x : 0.0; };  // explicit; needed if returns differ in type
auto hello  = [] { std::cout << "hi\n"; };             // no params, no captures

std::cout << square(3.0) << '\n';                       // 9
relu(-2.0);                                             // 0
hello();
```

Python equivalent: `lambda x: x * x`, except C++ lambdas may have multiple statements, and capture is explicit.

## 2. Captures

A lambda body cannot see local variables of the enclosing function unless captured. Globals and `static`s are always visible.

| Capture         | Meaning                                                     |
|-----------------|-------------------------------------------------------------|
| `[]`            | nothing                                                     |
| `[x]`           | copy of `x` at the moment the lambda is *created*           |
| `[&x]`          | reference to `x` — sees later changes; dangles if `x` dies  |
| `[=]`           | everything used, by copy                                    |
| `[&]`           | everything used, by reference                               |
| `[=, &y]`       | default copy, but `y` by reference                          |
| `[&, x]`        | default reference, but `x` by copy                          |
| `[this]`        | the enclosing object's pointer (inside a member function)   |
| `[name = expr]` | init-capture: a new variable initialised from `expr`        |

```cpp
double lr = 0.1;
int calls = 0;
auto step_by_value = [lr](double w, double g) { return w - lr * g; };     // lr frozen at 0.1
auto count_calls   = [&calls](double w) { ++calls; return w; };          // mutates outer `calls`
lr = 0.5;
std::cout << step_by_value(1.0, 1.0) << '\n';   // 0.9 — captured copy is still 0.1
count_calls(0); count_calls(0);
std::cout << calls << '\n';                     // 2
```

**Init-captures** move a resource into the closure or rename it:

```cpp
auto big = std::vector<double>(1'000'000, 1.0);
auto sum_big = [v = std::move(big)] { return std::accumulate(v.begin(), v.end(), 0.0); };
// `big` is now moved-from; the lambda owns the vector.
```

**The dangling-reference trap**: a `[&]` lambda that outlives the scope of the captured variable reads freed stack memory — UB. Capture by reference only for lambdas used *within* the current scope (algorithms called right there). Capture by value (or init-capture with `std::move`) when the lambda is stored or returned.

## 3. `mutable` lambdas

By-value captures are `const` inside the body. To modify the *lambda's own copy* (state that persists across calls), add `mutable`:

```cpp
auto counter = [n = 0]() mutable { return ++n; };
counter(); counter();
std::cout << counter() << '\n';    // 3
```

This does not touch any outer variable; `n` lives inside the closure object. Handy for stateful generators (`std::generate` with an incrementing value, a running average).

## 4. Generic lambdas (`auto` parameters)

`auto` in a lambda's parameter list makes `operator()` a template. One lambda then works for `int`, `double`, `Matrix`:

```cpp
auto add = [](auto a, auto b) { return a + b; };
add(1, 2);          // int
add(1.5, 2.25);     // double
add(std::string("a"), std::string("b"));   // "ab"
```

Use `const auto&` for parameters that are large: `[](const auto& m) { return m.rows(); }`.

## 5. Lambdas are objects: the closure type

A lambda expression creates an object of an unnamed **closure type** — a `struct` with the captures as members and the body as `operator()`. Chapter 06's `Sigmoid` functor *is* what `[](double x){ return 1/(1+exp(-x)); }` compiles to. Consequences:

- Every lambda expression has a **distinct type**, even two with identical text. You cannot write the type name; use `auto` or a template parameter.
- `sizeof` a lambda = sum of its captures (empty captures → 1 byte).
- A lambda with **no captures** converts implicitly to a plain function pointer — you can pass it to C APIs (`qsort`, `signal`).
- Calls through a lambda held in `auto` or a template parameter are inlined — zero overhead versus writing the loop by hand.

```cpp
auto f = [](double x) { return 2 * x; };
double (*fp)(double) = f;           // OK: captureless lambda → function pointer
auto g = [](double x) { return 2 * x; };
static_assert(!std::is_same<decltype(f), decltype(g)>::value, "each lambda is its own type");
```

## 6. Storing lambdas: `auto`, `std::function`, template parameter

| How                                   | Overhead               | Can hold different lambdas? | Use when                                   |
|---------------------------------------|------------------------|-----------------------------|--------------------------------------------|
| `auto f = [...]{...};`                | none                   | no (one fixed type)         | local variable                             |
| `template <typename F> void run(F f)` | none (inlined)         | yes, per instantiation      | algorithms, hot loops                      |
| `std::function<R(Args...)>`           | indirect call, maybe heap | yes, at runtime          | callbacks stored in structs, vectors of callbacks, virtual-like dispatch |

`std::function` is **type erasure**: it wraps any callable with the right signature behind one type. Cost: one indirect call (like a C function pointer, not inlinable) plus a heap allocation if the closure is larger than the small-buffer (typically 16-32 bytes). That cost is invisible per layer or per autograd node, and ruinous per matrix element.

```cpp
#include <functional>
std::function<double(double)> act = [](double x) { return std::tanh(x); };
act = [](double x) { return x > 0 ? x : 0.0; };       // reassign: different lambda, same std::function type
std::vector<std::function<void()>> callbacks;          // heterogeneous list of closures
if (act) act(0.5);                                     // empty std::function is falsy; calling it throws std::bad_function_call
```

In the autograd `Node`, `std::function<void()> backward_fn` captures the node's `shared_ptr` children by value and implements the local chain rule — one per node, called once per backward pass. `std::function` is exactly right there.

## 7. Lambdas as callbacks vs C's `void *ctx`

C (`../../c_learning/11_function_pointers_and_generics/lesson.md`):

```c
typedef double (*fn1)(double x, void *ctx);
double integrate(fn1 f, void *ctx, double a, double b, int n);
struct gauss_params { double mu, sigma; };
double gauss(double x, void *ctx) { struct gauss_params *p = ctx; ... }
integrate(gauss, &params, -5, 5, 1000);        // unsafe cast, indirect call per evaluation
```

C++:

```cpp
template <typename F>
double integrate(F f, double a, double b, int n) {
    double h = (b - a) / n, s = 0.5 * (f(a) + f(b));
    for (int i = 1; i < n; ++i) s += f(a + i * h);
    return s * h;
}
double mu = 0, sigma = 1;
integrate([&](double x) { return std::exp(-0.5 * ((x - mu) / sigma) * ((x - mu) / sigma)); }, -5, 5, 1000);
```

The lambda carries `mu` and `sigma` type-safely (no `void *`), the template inlines it into the loop (no indirect call), and there is nothing to keep in sync between the callback and its context struct.

## 8. Iterators: the glue

Algorithms do not take containers; they take **iterator ranges** `[first, last)` — half-open, exactly like `for (i = 0; i < n; ++i)`. `v.begin()` points to the first element, `v.end()` one past the last. Dereference with `*it`, advance with `++it`. A raw pointer into a C array is a valid iterator: `std::sort(arr, arr + n)`.

```cpp
std::vector<double> v = {3, 1, 2};
for (auto it = v.begin(); it != v.end(); ++it) *it *= 2;      // what range-for does underneath
std::sort(v.begin(), v.end());                                 // whole container
std::sort(v.begin(), v.begin() + 2);                           // first two only
auto it = std::find(v.begin(), v.end(), 4.0);
if (it != v.end()) std::size_t idx = it - v.begin();           // iterator → index (random-access only)
```

An algorithm that "returns a position" returns `last` to mean "not found". `std::back_inserter(v)` is an output iterator that calls `push_back` for each assignment — how you write into a vector that is not pre-sized.

## 9. `<algorithm>` and `<numeric>` survey

All examples below assume `#include <algorithm>`, `#include <numeric>` and `std::vector<double> v`.

### Sorting and order statistics

```cpp
std::sort(v.begin(), v.end());                                          // ascending, O(n log n), not stable
std::sort(v.begin(), v.end(), [](double a, double b) { return a > b; }); // descending via comparator
std::sort(v.begin(), v.end(), std::greater<>{});                        // same, with <functional>
std::stable_sort(recs.begin(), recs.end(),                              // preserves order of equal keys
                 [](const Rec& a, const Rec& b) { return a.label < b.label; });
std::partial_sort(v.begin(), v.begin() + 5, v.end());                   // smallest 5, sorted, in front; rest unordered
std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());         // median at the middle, O(n) average
double median = v[v.size() / 2];
```

A comparator must be a **strict weak ordering**: `comp(a, a)` is `false`, and `comp(a,b) && comp(b,c)` implies `comp(a,c)`. Using `<=` is UB (may crash inside `sort`). Sorting indices by a key (argsort):

```cpp
std::vector<std::size_t> idx(v.size());
std::iota(idx.begin(), idx.end(), 0);                                   // 0,1,2,...
std::sort(idx.begin(), idx.end(), [&](std::size_t i, std::size_t j) { return v[i] < v[j]; });
```

Python equivalent: `sorted(key=...)`, `np.argsort`, `np.partition` (`nth_element`), `heapq.nsmallest` (`partial_sort`).

### Min / max — argmax

```cpp
auto it = std::max_element(logits.begin(), logits.end());
std::size_t predicted_class = it - logits.begin();                      // argmax
auto [lo, hi] = std::minmax_element(v.begin(), v.end());                // both in one pass (structured binding, C++17)
double m = std::max(a, b);  double c = std::clamp(x, 0.0, 1.0);          // scalars
```

### Reductions

```cpp
double sum  = std::accumulate(v.begin(), v.end(), 0.0);                 // 0.0 not 0: an int init would truncate!
double prod = std::accumulate(v.begin(), v.end(), 1.0, std::multiplies<>{});
double dot  = std::inner_product(a.begin(), a.end(), b.begin(), 0.0);   // Σ a_i b_i
double sumsq = std::inner_product(v.begin(), v.end(), v.begin(), 0.0);  // Σ v_i²
// C++17 parallel-ready versions (no ordering guarantee, so must be associative):
double sum2 = std::reduce(v.begin(), v.end(), 0.0);
double l1   = std::transform_reduce(v.begin(), v.end(), 0.0, std::plus<>{}, [](double x) { return std::fabs(x); });
```

`accumulate` sums left to right in order; `reduce` may reorder (and can be given `std::execution::par`, though libc++ on macOS does not parallelise it). For floating-point sums that must be accurate, neither does Kahan summation — write the loop (`../../c_learning/12_numbers_bits_floats/lesson.md`).

### Transforming and iterating

```cpp
std::vector<double> out(v.size());
std::transform(v.begin(), v.end(), out.begin(), [](double x) { return 1 / (1 + std::exp(-x)); });  // map
std::transform(a.begin(), a.end(), b.begin(), out.begin(), std::plus<>{});                          // zip-map
std::for_each(v.begin(), v.end(), [](double& x) { x *= 2; });          // in-place; range-for is usually clearer
```

### Counting, searching, predicates

```cpp
auto n_pos = std::count_if(v.begin(), v.end(), [](double x) { return x > 0; });
auto first_neg = std::find_if(v.begin(), v.end(), [](double x) { return x < 0; });
bool all_finite = std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); });
bool any_nan    = std::any_of(v.begin(), v.end(), [](double x) { return std::isnan(x); });
bool none_neg   = std::none_of(v.begin(), v.end(), [](double x) { return x < 0; });
```

### Copying, filtering, removing

```cpp
std::vector<double> positives;
std::copy_if(v.begin(), v.end(), std::back_inserter(positives), [](double x) { return x > 0; });

// erase-remove idiom: remove_if only MOVES kept elements to the front and returns the new end;
// erase then shrinks the vector. Forgetting erase leaves garbage at the tail.
v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return std::isnan(x); }), v.end());

std::sort(v.begin(), v.end());
v.erase(std::unique(v.begin(), v.end()), v.end());                      // dedupe (adjacent only → sort first)
std::reverse(v.begin(), v.end());
std::fill(v.begin(), v.end(), 0.0);
std::iota(v.begin(), v.end(), 1.0);                                     // 1,2,3,...
```

Python equivalent: list comprehension with `if` (`copy_if`), `v[~np.isnan(v)]` (erase-remove), `np.unique` (sort + unique), `np.arange` (`iota`).

### Binary search on sorted ranges

```cpp
std::sort(v.begin(), v.end());
bool present = std::binary_search(v.begin(), v.end(), 2.5);
auto lb = std::lower_bound(v.begin(), v.end(), 2.5);   // first element >= 2.5  (np.searchsorted side='left')
auto ub = std::upper_bound(v.begin(), v.end(), 2.5);   // first element >  2.5  (side='right')
std::size_t count_equal = ub - lb;
```

`lower_bound` is how you find the bin of a sample in a CDF (inverse-transform sampling), the interval for interpolation, or the insertion point that keeps a vector sorted.

### Shuffle

```cpp
std::mt19937_64 rng(42);
std::shuffle(indices.begin(), indices.end(), rng);     // Fisher-Yates; rng passed BY REFERENCE (it must advance)
```

Never use `std::random_shuffle` (removed in C++17) or a hand-rolled `rand() % n` swap (biased).

## 10. Function objects from `<functional>`

Pre-lambda C++ shipped functors for the operators: `std::plus<>`, `std::minus<>`, `std::multiplies<>`, `std::negate<>`, `std::greater<>`, `std::less<>`, `std::equal_to<>`. The empty `<>` (C++14) deduces the operand types. They read well as arguments: `std::accumulate(b, e, 1.0, std::multiplies<>{})`. For anything else write a lambda.

`std::bind(f, _1, 5)` creates a callable with some arguments fixed. It predates lambdas and is harder to read; `[](auto x) { return f(x, 5); }` says the same. Recognise `std::bind` and `std::placeholders::_1` in older code; do not write them.

## 11. `<random>` properly

C's `rand()` is low quality, has a tiny range, and `rand() % n` is biased. `<random>` separates the **engine** (bits) from the **distribution** (shape):

```cpp
#include <random>
std::mt19937_64 rng(12345);                       // Mersenne Twister, 64-bit output. Seed → reproducible.
std::uniform_real_distribution<double> U(0.0, 1.0);        // [0, 1)
std::normal_distribution<double> N(0.0, 1.0);              // mean 0, stddev 1
std::uniform_int_distribution<int> die(1, 6);              // inclusive both ends
std::bernoulli_distribution coin(0.3);                      // true with p = 0.3

double u = U(rng);   double z = N(rng);   int d = die(rng);   bool heads = coin(rng);
```

Rules:
1. **One engine per program (or per thread)**, created once, passed by reference. Creating a fresh engine per call is the classic bug — same seed, same "random" number every time.
2. **Distributions are cheap objects**; make them where you need them.
3. **Seed** from a constant for reproducible experiments (you need this to debug training), from `std::random_device{}()` for a different run each time. `std::random_device` is slow — use it only to seed.
4. `mt19937_64`'s state is 2.5 KB; do not copy it around by value. `std::minstd_rand` is tiny and fast but low quality; fine for shuffling, not for Monte Carlo.

Weight initialisation (He/Kaiming for ReLU):

```cpp
std::normal_distribution<double> init(0.0, std::sqrt(2.0 / fan_in));
for (double& w : W.data()) w = init(rng);
```

Python equivalent: `rng = np.random.default_rng(12345); rng.normal(0, s, size)` — same engine/distribution split, same rule of one generator.

## 12. C++20 ranges (mention)

C++20 lets you write `v | std::views::filter(pred) | std::views::transform(f)` lazily, and `std::ranges::sort(v)` without `begin/end`. Not available under `-std=c++17`. Know that it exists so pipelines in modern code look familiar; everything here works identically with iterator pairs.

## 13. When to write the loop yourself

Algorithms win when the operation has a name (`sort`, `nth_element`, `lower_bound`, `shuffle`): you get a correct, tested `O(...)` and the reader knows instantly what happens. Loops win in numerical kernels:

- **Fused operations**: `y = a*x + b` over three arrays is one loop; with algorithms it is a `transform` with a zip you have to fake.
- **Memory access order** in 2D/3D stencils (FDTD, fluids): you want `for i, for j` with the inner loop contiguous, and to hoist `i*cols`. No algorithm expresses that.
- **Accuracy**: Kahan/pairwise summation, compensated dot products.
- **Early exit with index**, multiple outputs (min *and* argmin *and* second-min in one pass).

Rule of thumb: named operation over a 1D range → algorithm. Multi-array numeric kernel → loop. When in doubt, write the loop and keep it simple; the compiler vectorises simple loops.

## Gotchas and undefined behavior

- **`[&]` capture in a stored lambda** → dangling reference when the enclosing function returns. UB. Store by value.
- **Modifying a container while iterating** (e.g. `push_back` inside range-for) invalidates iterators — UB. Collect then insert, or iterate by index with the bound re-read.
- **`std::accumulate(..., 0)`** with an `int` initial value sums doubles as `int`. Write `0.0`.
- **Comparator with `<=`** violates strict weak ordering → UB in `sort` (real crashes on libc++). Use `<`.
- **`remove_if` without `erase`** — the vector's size is unchanged and the tail holds moved-from garbage.
- **`unique` on unsorted data** removes only adjacent duplicates.
- **`lower_bound` on unsorted data** — UB (returns nonsense; no diagnostic).
- **`std::shuffle(..., rng)`** must receive the engine by reference; algorithms take it by `URBG&&`, so passing a named engine is fine, but `std::shuffle(b, e, std::mt19937_64(42))` in a loop re-seeds each time and shuffles identically.
- **New engine per call** (`std::mt19937_64 rng(seed)` inside a function called in a loop) — identical outputs every call.
- **`std::function` call on empty** throws `std::bad_function_call`.
- **Generic lambda with `auto` parameters by value** copies the `Matrix` on each call — use `const auto&`.
- **Returning a reference from a lambda** with deduced return type: `[](std::vector<double>& v) -> double& { return v[0]; }` needs the explicit `-> double&`; without it, `auto` deduction strips the reference and you return a copy.

## Common mistakes checklist

- [ ] Stored/returned lambdas capture by value (or init-capture with `std::move`), never `[&]`.
- [ ] `std::function` only where type erasure is needed (stored callbacks); templates or `auto` in hot code.
- [ ] Comparators use `<` (strict), never `<=`.
- [ ] Every `remove_if`/`unique` is followed by `erase(..., end())`.
- [ ] `accumulate` initial value has the right type (`0.0`).
- [ ] `sort` before `unique`, `binary_search`, `lower_bound`.
- [ ] One `std::mt19937_64` per program, seeded once, passed by reference; distributions created freely.
- [ ] `auto&` / `const auto&` in range-for and generic lambdas when elements are large.
- [ ] Argmax is `max_element(...) - begin()`, not a hand loop with `<=` off-by-one.
- [ ] Multi-array numeric kernels are plain loops with contiguous inner index.

## You can move on when...

- You can write, from memory, a lambda that captures one variable by value, one by reference, and one by move, and explain when each would dangle.
- You can state what `sizeof` a captureless lambda is, why two identical lambdas have different types, and which of `auto`, template parameter and `std::function` involves an indirect call.
- You can implement argmax, dot product, argsort, median, filter-and-collect and erase-NaNs with algorithms in one line each.
- You can explain the erase-remove idiom and what goes wrong without the `erase`.
- You can set up reproducible Gaussian weight initialisation with `<random>` and name the classic bug (engine created per call).
- You can give two examples where a hand-written loop beats an algorithm in numerical code and say why.
