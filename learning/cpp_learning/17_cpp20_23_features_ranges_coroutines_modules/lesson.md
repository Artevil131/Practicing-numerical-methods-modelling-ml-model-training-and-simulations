# Chapter 17 — C++20/23 features: ranges, coroutines, modules, and what to actually use

This chapter requires `-std=c++20`; several sections need `-std=c++23` and say so. Compile with
`c++ -Wall -Wextra -std=c++20 -O2` (or `-std=c++23`). Assumes Chapters 05 (templates), 08 (algorithms, lambdas), 12 (performance), 13 (idioms, `variant`, `optional`) and 16 (lifetime, allocators, `span<std::byte>`).

Everything stated here about compiler support was **verified on Apple clang 21.0.0 (clang-2100.1.1.101), libc++ 210106, macOS arm64** with small test programs; §19 has the table.

## What you'll be able to do after this chapter

- Write numerics pipelines with `std::ranges` (algorithms with projections, lazy views, adaptor closures), know what they cost against a raw loop, avoid dangling views, and write a custom view with its own `|` adaptor.
- Use `std::span` for kernel arguments and `std::mdspan` (C++23) with `layout_right`/`layout_left`/`layout_stride` as the view type a `Matrix`/`Tensor` needs — with a fallback where `mdspan` is missing.
- Replace `printf`/`<<` with `std::format`/`std::print`, including a `formatter` specialization for your own types.
- Explain the coroutine machinery (`promise_type`, `coroutine_handle`, awaiters, `co_yield`/`co_await`/`co_return`), implement `Generator<T>`, and use it to stream training batches or particles lazily.
- Build a named module on Apple clang 21 with the flag it actually needs, and give a realistic assessment of modules today.
- Use `<=>` and `operator==` rewriting, designated initializers, `consteval`/`constinit`, `std::source_location`, `<bit>`, `<numbers>`, `std::expected`, `std::flat_map`, `std::jthread`, `<chrono>` calendars; write code that degrades gracefully with `__cpp_lib_*` feature-test macros; and decide from a migration table what to adopt now.

## Why this matters for ML / numerics / sims

The "modern" features fall into three buckets for a numerics programmer. **Adopt now**: `span`/`mdspan` (the view type your matrix library has been reinventing), `format` (correct, type-safe output), concepts (readable template errors), `<bit>`/`<numbers>`/`<=>`/designated initializers (small wins every day), ranges *algorithms* (projections, safety). **Adopt with a benchmark**: range *views* in hot loops — the example measures a `filter | transform` pipeline at **2.4× slower** than the raw loop at `-O2` on this machine (1.5× at `-O3`), while a branchless `transform_reduce` matches the loop at `-O2` and halves it at `-O3` — so views belong in data-preparation code, not in the innermost kernel. **Skip for now**: coroutines (great for lazy data loaders, a per-element resume is a function call you can't inline) and modules (work on Apple clang 21 only with an Apple-specific flag, no `import std;`, no tooling).

The chapter also teaches how to *check* rather than assume: `<version>` macros, tiny probe programs, and `#if` fallbacks are how a codebase survives a compiler upgrade — or a colleague on Linux with GCC 14.

---

## 1. Range algorithms and projections

`std::ranges::sort(v)` takes the whole range; classic `std::sort(v.begin(), v.end())` still exists. The ranges versions add three things: they take ranges, they accept a **projection** (a callable applied to each element before comparing/predicating), and they return richer results.

```cpp
struct Particle { double x, y, vx, vy, mass; };
std::vector<Particle> ps = ...;
std::ranges::sort(ps, {}, &Particle::mass);           // sort by a member: {} = std::ranges::less
auto heaviest = std::ranges::max_element(ps, {}, &Particle::mass);
auto [mn, mx] = std::ranges::minmax(v);               // struct with .min/.max
auto n_even = std::ranges::count_if(v, [](int x) { return x % 2 == 0; });
```

A projection replaces the "write a lambda that compares `a.mass < b.mass`" boilerplate and is exactly NumPy's `key=`/`argsort` of one column. Projections work with pointers-to-members (`&Particle::mass`) because the library calls them via `std::invoke`.

Return types differ from the classic algorithms: `ranges::copy` returns `{in, out}` (an `in_out_result`), `ranges::sort` returns the end iterator, `ranges::find` returns an iterator or `std::ranges::dangling` (§4). Everything lives in `<algorithm>`, `<numeric>` (C++23 `ranges::fold_left` — **missing in libc++ 21**, use `std::accumulate` on `begin()/end()`), and `<ranges>`.

Python equivalent: `sorted(ps, key=lambda p: p.mass)`; `max(ps, key=...)`.

---

## 2. Views: laziness, pipelines, adaptor closures

A **view** is a lightweight (O(1) copy), non-owning range that computes elements on demand. `std::views::filter(pred)`, `transform(f)`, `take(n)`, `drop(n)`, `take_while`, `drop_while`, `reverse`, `iota(a[, b])`, `split(delim)`, `join`, `keys`/`values`/`elements<N>`, `all`, `counted`, `common`.

```cpp
int calls = 0;
auto r = std::views::iota(1)                                 // infinite: 1, 2, 3, ...
       | std::views::filter([](int x) { return x % 2 == 1; })
       | std::views::transform([&](int x) { ++calls; return x * x; })
       | std::views::take(5);
for (int v : r) std::print("{} ", v);                        // 1 9 25 49 81
// calls == 5: transform ran exactly 5 times — laziness.
```

`r` here is a *type* about 300 characters long that holds three lambdas and two ints; building it does no work. Iterating pulls one element at a time through the chain. That is what makes `iota(1)` (infinite) usable.

**Range adaptor closures**: `std::views::filter(pred)` *without a range* is a closure object; `r | closure` applies it, and `closure1 | closure2` composes into a new closure. So a pipeline can be built once, stored, and applied to many ranges:

```cpp
auto kinetic = std::views::transform([](const Particle& p) { return 0.5 * p.mass * (p.vx*p.vx + p.vy*p.vy); });
auto moving  = std::views::filter([](const Particle& p) { return p.vx != 0 || p.vy != 0; });
auto ke_of_moving = moving | kinetic;                         // closure
for (double k : ps | ke_of_moving) total += k;
```

`split` on a `std::string` yields **subranges of the string** (contiguous, so `word.data()`/`word.size()` give a `string_view` with no copy); `join` flattens a range of ranges (`vector<vector<int>>` → ints). Together they are a zero-allocation tokenizer for BPE preprocessing.

Views are **not** containers: no `size()` unless the underlying range is sized and the view preserves it (`filter` does not), no random access through `filter`, and `begin()` may be O(n) for `filter` (it must find the first match; views cache it, which is why a `filter_view` must be non-`const` to iterate).

Python equivalent: generator expressions / `itertools` — `(x*x for x in count(1) if x % 2)` with `islice(..., 5)`.

---

## 3. C++23 views: `zip`, `enumerate`, `chunk`, `slide`, `ranges::to` — and what libc++ 21 has

| View / utility | C++ | Purpose in numerics | libc++ 21, `-std=c++23` |
|---|---|---|---|
| `views::zip(a, b, ...)` | 23 | SGD update `w -= lr * g` over two vectors; SoA particle loops | **Ships** (`zip_view` works) but `__cpp_lib_ranges_zip` is **not defined** (paper also covers `adjacent`, `zip_transform`, which are missing) |
| `views::enumerate` | 23 | `(i, x)` pairs | **Missing** |
| `views::chunk(n)` | 23 | Mini-batches | **Missing** |
| `views::slide(n)` | 23 | Moving averages, finite differences | **Missing** |
| `views::stride(n)` | 23 | A column of a row-major matrix | **Missing** |
| `views::adjacent<N>` / `pairwise` | 23 | Neighbour pairs | **Missing** |
| `views::cartesian_product` | 23 | Grid index pairs `(i, j)` | **Missing** |
| `views::chunk_by(pred)` | 23 | Group runs | present |
| `views::join_with(sep)` | 23 | Join tokens with `' '` | present |
| `views::as_rvalue`, `views::repeat` | 23 | — | present |
| `ranges::to<vector>()` | 23 | Materialize a pipeline | present (`__cpp_lib_ranges_to_container` 202202) |
| `ranges::fold_left` | 23 | `reduce` with an init and an op | **Missing** |
| `ranges::contains`, `starts_with` | 23 | — | present |

The fallbacks the example uses when the macro is absent — and these are what you write today:

```cpp
// zip: index loop (or a hand-written zip over iota)
for (std::size_t i = 0; i < w.size(); ++i) w[i] -= lr * g[i];
// enumerate: iota | transform
auto enumerate = [](auto& r) {
    return std::views::iota(std::size_t{0}, std::ranges::size(r))
         | std::views::transform([&r](std::size_t i) { return std::pair<std::size_t, decltype(r[i])>{i, r[i]}; });
};
// chunk: std::span::subspan in a loop, or the custom view in §4
// ranges::to: a small to_vector() that reserve()s if sized_range, then push_back
```

`ranges::to` fallback, generic:

```cpp
template <std::ranges::input_range R>
auto to_vector(R&& r) {
#if __cpp_lib_ranges_to_container >= 202202L
    return std::forward<R>(r) | std::ranges::to<std::vector>();
#else
    std::vector<std::ranges::range_value_t<R>> v;
    if constexpr (std::ranges::sized_range<R>) v.reserve(std::ranges::size(r));
    for (auto&& x : r) v.push_back(std::forward<decltype(x)>(x));
    return v;
#endif
}
```

---

## 4. Writing a custom view and adaptor closure

A view is a class that (1) models `std::ranges::view` (movable, default-constructible *if the base is*, derived from `std::ranges::view_interface<Self>`), (2) has `begin()`/`end()` returning an iterator/sentinel pair, and (3) wraps its base as a view (`std::views::all_t<R>` — a `ref_view` for lvalue containers, `owning_view` for rvalues). `view_interface` supplies `empty()`, `front()`, `operator bool`, `size()` when possible, `operator[]` for random access.

The example implements `my::chunk_view` (what `views::chunk` does — mini-batches):

```cpp
template <std::ranges::view V>
    requires std::ranges::random_access_range<V> && std::ranges::sized_range<V>
class chunk_view : public std::ranges::view_interface<chunk_view<V>> {
    V base_;                       // NO `{}` initializer: ref_view has no default constructor
    std::ranges::range_difference_t<V> n_ = 1;
public:
    class iterator {               // forward iterator yielding subrange(cur, min(cur+n, end))
        std::ranges::iterator_t<V> cur_{}, end_{}; std::ranges::range_difference_t<V> n_ = 1;
    public:
        using value_type = std::ranges::subrange<std::ranges::iterator_t<V>>;
        using difference_type = std::ranges::range_difference_t<V>;
        using iterator_concept = std::forward_iterator_tag;
        iterator() = default;
        value_type operator*() const { auto last = (end_ - cur_ < n_) ? end_ : cur_ + n_; return {cur_, last}; }
        iterator& operator++() { cur_ = (end_ - cur_ < n_) ? end_ : cur_ + n_; return *this; }
        iterator operator++(int) { auto t = *this; ++*this; return t; }
        bool operator==(const iterator& o) const { return cur_ == o.cur_; }
        // ...
    };
    chunk_view() requires std::default_initializable<V> = default;
    chunk_view(V base, std::ranges::range_difference_t<V> n) : base_(std::move(base)), n_(n) {}
    iterator begin() { return {std::ranges::begin(base_), std::ranges::end(base_), n_}; }
    iterator end()   { return {std::ranges::end(base_),   std::ranges::end(base_), n_}; }
};
template <class R> chunk_view(R&&, std::ranges::range_difference_t<R>) -> chunk_view<std::views::all_t<R>>;

struct chunk_fn {                   // the adaptor closure
    std::ptrdiff_t n;
    template <std::ranges::viewable_range R> auto operator()(R&& r) const { return chunk_view(std::views::all(std::forward<R>(r)), n); }
    template <std::ranges::viewable_range R> friend auto operator|(R&& r, chunk_fn f) { return f(std::forward<R>(r)); }
};
inline chunk_fn chunk(std::ptrdiff_t n) { return {n}; }
```

Now `dataset | my::chunk(32) | std::views::transform(mean)` works and is lazy. The parts that bite: the iterator must satisfy `std::forward_iterator` (default-constructible, `==`, both `++`, `value_type`, `difference_type`, `iterator_concept`) — check with `static_assert(std::ranges::forward_range<my::chunk_view<...>>)`; the deduction guide is what makes `chunk_view(vec, 4)` wrap the vector in a `ref_view`; and in C++23 you would derive `chunk_fn` from `std::ranges::range_adaptor_closure` instead of writing `operator|`.

---

## 5. Dangling and borrowed ranges

```cpp
auto it = std::ranges::max_element(make_vector());     // rvalue vector → temporary dies
static_assert(std::same_as<decltype(it), std::ranges::dangling>);   // NOT an iterator
```

Classic `std::max_element(make_vector().begin(), ...)` would return a pointer into freed memory. The ranges algorithms detect an rvalue non-borrowed range and return `std::ranges::dangling`, an empty struct: any use is a compile error. A **borrowed range** ([range.range]) is one whose iterators stay valid after the range object dies — `std::span`, `std::string_view`, `subrange`, `ref_view`, `iota_view`, and anything that specializes `std::ranges::enable_borrowed_range<T> = true`. Passing an rvalue `span` is fine.

`make_vector() | views::filter(...)` is fine too: since P2415 (a C++20 defect report libc++ applies), `views::all` wraps an rvalue container in `owning_view`, which moves it in. What is still UB: a view over a *reference* to something that dies (`std::string_view sv = make_string();` and anything built on it).

---

## 6. Ranges vs raw loops — measured

Sum of squares of the even numbers in 4 M `int`s into a `std::uint64_t`, min of 5, Apple M-series:

| Formulation | `-O2` | vs raw | `-O3` vs raw |
|---|---|---|---|
| `for (int x : xs) if (x % 2 == 0) s += x*x;` | 0.59 ms | 1.00× | 1.00× |
| `for (auto v : xs \| filter(even) \| transform(sq)) s += v;` | 1.41 ms | 2.40× | 1.49× |
| `std::accumulate(view.begin(), view.end(), 0)` over the same view | 1.41 ms | 2.40× | 1.26× |
| `std::transform_reduce(xs, 0, plus, x%2==0 ? x*x : 0)` (branchless) | 0.59 ms | 1.00× | 0.52× |

Reading: the plain loop's `if` is a predictable pattern that Clang turns into a select and vectorizes; the `filter_view` iterator's `++` is a loop that re-tests the predicate and compares against the end each step, Clang does not vectorize through it, and a compare-and-branch per element is what you pay. The branchless `transform_reduce` (or the equivalent loop) vectorizes with NEON and is never slower. `-O3` narrows the gap but does not close it (the example prints the numbers for whatever flags you pass — rerun it). Two caveats that matter for honest benchmarking: an earlier version of this section accumulated into `long long` and UBSan flagged the signed overflow (Σx² ≈ 10¹⁹ > 2⁶³) — with the signed type the raw loop was also unvectorized and the view's penalty looked like 1.9×; small changes in the accumulator type change the optimizer's decisions, so always read the ratio *and* the absolute time. Conclusions: (1) views in *hot* kernels: measure, expect 1.5–2.5× slower for `filter`, ~1× for `transform`/`take`/`iota` alone; (2) views for data preparation, tokenization, batch assembly, logging: free; (3) prefer a standard algorithm to a view when one fits — `transform_reduce`, `inner_product`, `ranges::for_each`.

---

## 7. `std::span`

`std::span<T>` (C++20, `<span>`) is `(T*, size)` with bounds `[]` (unchecked unless libc++ hardening is on), `first(n)`, `last(n)`, `subspan(off, n)`, `size_bytes()`, and iterators. It is the parameter type for every kernel:

```cpp
double dot(std::span<const double> a, std::span<const double> b);
dot(vec, other_vec);                   // vector → span implicitly
dot(std::span(arr).first(3), std::span(vec).first(3));   // array, sub-view
```

`std::span<double, 3>` has a **static extent**: `sizeof == sizeof(double*)` (8 bytes vs 16), and the size is a compile-time constant the optimizer can unroll. `std::as_bytes(span)` / `as_writable_bytes` convert to `span<const std::byte>` for serialization (Chapter 16 §11). `span` is a borrowed range and a contiguous range, so it composes with ranges. It never owns; a `span` into a `vector` is invalidated by `push_back` exactly like an iterator.

Python equivalent: a NumPy view `a[2:4]` — same "no copy, shares storage" semantics, minus the strides.

---

## 8. `std::mdspan` (C++23) — the view a `Matrix`/`Tensor` needs

`std::mdspan<T, Extents, LayoutPolicy, AccessorPolicy>` (`<mdspan>`, **present in libc++ 21 under `-std=c++23`**, absent under `-std=c++20`) is a non-owning multidimensional view over flat storage:

```cpp
std::vector<double> a(6);
std::mdspan<double, std::dextents<std::size_t, 2>> m(a.data(), 2, 3);                      // row-major (layout_right)
std::mdspan<double, std::dextents<std::size_t, 2>, std::layout_left> ml(a.data(), 2, 3);  // column-major (Fortran/BLAS)
m[1, 2] = 5;                               // C++23 multidimensional operator[]
m.extent(0), m.extent(1)                   // 2, 3
m.stride(0), m.stride(1)                   // 3, 1     (ml: 1, 2)
std::mdspan<double, std::extents<std::size_t, 3, 2>> fixed(a.data());   // static extents: sizeof == 8
std::layout_stride::mapping<std::dextents<std::size_t, 2>> tmap({3, 2}, std::array<std::size_t, 2>{1, 3});
std::mdspan<double, std::dextents<std::size_t, 2>, std::layout_stride> mt(a.data(), tmap);  // the transpose, zero copy
```

The **layout policy** is a mapping `(i, j, ...) → offset`. `layout_right` is C/NumPy order, `layout_left` is Fortran/BLAS/`cblas_dgemm(CblasColMajor)` order, `layout_stride` is arbitrary strides — NumPy's `strides=` and PyTorch's `TensorImpl` strides (Chapter 16 §16). `extents` mixes static (`3`) and dynamic (`std::dynamic_extent`) sizes; every static extent is a size the compiler knows.

What `mdspan` gives your library: one vocabulary for "view of a matrix" that kernels can accept regardless of who owns the memory, and correct-by-construction index math. What it does *not* give: `submdspan` (C++26), bounds checking, arithmetic, or ownership. Under `-std=c++20` the example falls back to a 40-line `MdSpan2<T, Layout>` with `LayoutRight`/`LayoutLeft` policy structs — enough to code against today and swap for `std::mdspan` behind `#if __cpp_lib_mdspan`.

---

## 9. `std::format` and `std::print`

`std::format` (C++20, `<format>`, **present**) is Python's `str.format`/f-strings, type-safe and checked at compile time. `std::print`/`std::println` (C++23, `<print>`, **present under `-std=c++23`**) write it to `stdout` without the `std::cout` machinery.

```cpp
std::format("|{:>10.3f}|{:<8}|{:^9}|{:#x}|{:08.2e}|{:+d}|", pi, "left", "mid", 255, 12345.678, 42)
// |     3.142|left    |   mid   |0xff|1.23e+04|+42|
std::format("{:{}}", "ab", 6)      // width from an argument
std::format("{:*^12}", "mid")      // fill and align
std::format("{:d}", 1.5)           // COMPILE ERROR: spec/type mismatch caught statically
std::vformat(runtime_fmt, std::make_format_args(kg))   // runtime format strings; args must be lvalues (P2905)
```

Spec grammar: `[[fill]align][sign][#][0][width][.precision][type]` with `<`/`>`/`^` alignment, `+`/`-`/space sign, `#` alternate form, `0` zero-pad, types `d x b o c s f e g a` and `?` (C++23 debug-quoting). C++23 adds range formatting (`{}` of a `vector` → `[1, 2, 3]`, of a `map` → `{"lr": 0.01}`, `{:n}` without brackets; **present under `-std=c++23`**).

**Formatting your own types**: specialize `std::formatter<T>`. Inheriting from `std::formatter<double>` reuses its `parse` so users can write `{:8.3f}` and have the spec apply to each component:

```cpp
template <> struct std::formatter<Vec3> : std::formatter<double> {
    auto format(const Vec3& v, std::format_context& ctx) const {
        auto o = std::format_to(ctx.out(), "(");
        o = std::formatter<double>::format(v.x, ctx); o = std::format_to(o, ", ");
        o = std::formatter<double>::format(v.y, ctx); o = std::format_to(o, ", ");
        o = std::formatter<double>::format(v.z, ctx);
        return std::format_to(o, ")");
    }
};
std::format("{:.2f}", Vec3{1, 2.5, -3.25})   // (1.00, 2.50, -3.25)
```

For a `Matrix<T>`: iterate rows, `format_to` each element with the inherited spec, `'\n'` between rows. For a runtime `print` shim under C++20: `std::fputs(std::format(...).c_str(), stdout)`. Performance: `format` is 1.5–3× faster than `ostringstream` and comparable to `snprintf`; it is not the bottleneck in anything numeric.

---

## 10. Coroutines: the machinery

A coroutine is a function containing `co_await`, `co_yield` or `co_return`. The compiler rewrites it into a **frame** (heap-allocated by default: locals + parameters + a resume point), and talks to your code through a **promise type** obtained from the return type:

```
caller                                   coroutine frame
------                                   ---------------
auto g = gen();  ──► allocate frame, construct promise_type,
                     call promise.get_return_object() → g
                     promise.initial_suspend()  → suspend_always ⇒ return to caller now
g.begin() → h.resume() ───────────────► run until `co_yield v`
                                         = co_await promise.yield_value(v)  → suspend_always ⇒ back to caller
*it → h.promise().value_
++it → h.resume() ────────────────────► continue after the co_yield ...
                                         falls off the end → promise.return_void(), final_suspend()
h.done() == true
~Generator → h.destroy() → frame freed
```

| Customization point | On | Purpose |
|---|---|---|
| `promise_type` (nested type or `std::coroutine_traits`) | return type | Everything below |
| `get_return_object()` | promise | Build the object the caller receives |
| `initial_suspend()` / `final_suspend()` | promise | `suspend_always` = lazy / keep frame; `suspend_never` = eager / auto-destroy |
| `yield_value(v)` | promise | What `co_yield v` does; returns an awaiter |
| `return_void()` / `return_value(v)` | promise | What `co_return [v]` does |
| `unhandled_exception()` | promise | Called in the catch-all around the body; store `std::current_exception()` |
| `await_transform(x)` | promise (optional) | Rewrite every `co_await x` |
| `await_ready()` / `await_suspend(handle)` / `await_resume()` | **awaiter** | The three hooks of `co_await` |

`co_await expr`: get an awaiter (`expr` itself, or via `operator co_await`/`await_transform`); if `await_ready()` is false, call `await_suspend(handle)` — return `void`/`true` to suspend, `false` to continue immediately, or another handle to symmetric-transfer to it; when resumed, `await_resume()`'s value is the value of the `co_await` expression. `std::suspend_always` and `std::suspend_never` are the trivial awaiters.

---

## 11. `Generator<T>` and lazy batches

The example's `Generator<T>` (about 50 lines) is the `std::generator` shape:

```cpp
template <class T> class Generator {
public:
    struct promise_type {
        T value_{}; std::exception_ptr exc_;
        Generator get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T v) noexcept { value_ = std::move(v); return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { exc_ = std::current_exception(); }
    };
    // move-only; ~Generator() { if (h_) h_.destroy(); }
    // iterator: operator++ → h_.resume() then rethrow if exc_; operator* → promise().value_; == default_sentinel_t → h_.done()
};

Generator<std::span<const float>> batches(std::span<const float> data, std::size_t bs) {
    for (std::size_t i = 0; i < data.size(); i += bs) co_yield data.subspan(i, std::min(bs, data.size() - i));
}
Generator<Particle> particle_stream(unsigned seed) { std::mt19937 rng(seed); for (;;) co_yield Particle{...}; }
```

`for (auto b : batches(X, 32))` pulls one batch at a time; `particle_stream` is infinite and the consumer `break`s — destroying the `Generator` destroys the frame, including the `mt19937` inside it. An exception thrown in the body is caught by the compiler-generated handler, stored, and rethrown in the consumer's `++` — the example shows it. `#include <iterator>` for `std::default_sentinel_t`.

The `Task<T>` in the example shows `co_return` and a custom awaiter (`Checkpoint`) whose `await_suspend` returns `false` — the three hooks run, nothing actually suspends — a debugging/instrumentation pattern.

**Cost**: each `resume` is an indirect call with a frame switch (~2–5 ns), and the body can't be inlined into the consumer. A per-element generator in an inner loop is 5–20× slower than a loop; a per-*batch* generator is free. `std::generator<T>` (C++23, `<generator>`) is **missing in libc++ 21**; when it arrives it adds allocator support and recursive `co_yield std::ranges::elements_of(inner)`.

Python equivalent: exactly `def batches(...): yield ...` — the semantics are identical; C++ just makes you write the promise type.

---

## 12. Modules — what works on Apple clang 21

Tested with `math.cppm` / `use.cpp`:

```cpp
// math.cppm
export module math;
export int sq(int x) { return x * x; }
// use.cpp
import math;
int main() { return sq(7) - 49; }
```

| Attempt | Result |
|---|---|
| `c++ -std=c++20 --precompile math.cppm` | **error: expected template** at `export module` — Apple clang does not enable named modules by default |
| `c++ -std=c++20 -fcxx-modules --precompile -o math.pcm math.cppm` then `c++ -std=c++20 -fcxx-modules -fmodule-file=math=math.pcm -o use use.cpp math.pcm` | **works**, prints 49 |
| `import std;` (`-std=c++23`) | error: no `std.cppm` shipped in the Xcode toolchain |
| `__cpp_modules` | never defined, even when modules work |
| `-fmodules` (Clang *header* modules, implicit) | works; this is what Xcode uses to speed up `#include <vector>` |
| `import <vector>;` header units with `-fmodules -fcxx-modules` | compiles |

Realistic assessment (2026): named modules **work** on Apple clang 21 with `-fcxx-modules`, upstream Clang ≥ 16 and GCC ≥ 14 without extra flags, and MSVC. `import std;` needs a toolchain that ships `std.cppm` (upstream libc++ does; Apple's does not) or building it yourself. Build systems: CMake ≥ 3.28 with Ninja supports scanning (`CXX_SCAN_FOR_MODULES`); Make requires you to order the `.pcm` steps by hand. Tooling (clangd, clang-tidy, code coverage) lags. Verdict for a numerics library: **not yet** — headers with `#pragma once` and a precompiled header (or `-fmodules`) give most of the compile-time win with zero risk. Revisit when your CMake, compiler, and IDE all agree.

---

## 13. Three-way comparison and `operator==` rewriting

```cpp
struct Version { int major, minor, patch; auto operator<=>(const Version&) const = default; };
// defaulted <=> also defaults ==; return type deduced as std::strong_ordering (lexicographic members)
a < b, a <= b, a > b, a >= b   // rewritten as (a <=> b) < 0 etc.
a != b                         // rewritten as !(a == b)
```

The comparison categories: `std::strong_ordering` (equal ⇒ substitutable; ints), `std::weak_ordering` (equivalent but distinguishable; case-insensitive strings), `std::partial_ordering` (some pairs unordered; `double` because of NaN). `double <=> double` is `partial_ordering`; `(x <=> nan) == std::partial_ordering::unordered`. Helpers: `std::is_lt/is_eq/is_gt`, `std::strong_order`, `std::weak_order(a, b)` (a total order on doubles that places NaN).

**Rewriting rules**: for `a @ b` the compiler also tries `b @' a` with the operator reversed and the synthesized forms — so a single heterogeneous `Length::operator<=>(const Meters&)` makes both `L > M` and `M < L` work; a single `operator==` gives `!=` too. Two functions replace the C++17 six. Do not default `<=>` when member order isn't the right order (a `Fraction` needs cross-multiplication); write `==` separately when equality is looser than ordering (tolerance) — the example's `Approx` does exactly that. `std::ranges::sort` and `std::map` use `<` and so pick up `<=>` automatically.

---

## 14. Small C++20 features you should be using

**Designated initializers**: `TrainConfig{.lr = 0.01, .epochs = 3}` — named fields, must be in declaration order, rest default-initialized; only for aggregates. Replaces builder patterns and "constructor with eight doubles".

**`consteval`**: a function that *must* be evaluated at compile time (`consteval std::uint64_t pow2(int k)`); calling it with a runtime argument is an error. **`constinit`**: a variable with static storage whose initializer *must* be a constant expression — kills the static-initialization-order fiasco for lookup tables. `constexpr` functions may now contain `std::vector`/`std::string` (C++20, `__cpp_lib_constexpr_dynamic_alloc`) as long as they don't escape.

**`std::source_location::current()`** as a default argument captures the *caller's* file, line and function — a `check(cond, msg)` helper or a logger without macros.

**`<bit>`**: `std::popcount`, `bit_width` (⌈log₂⌉+1), `countl_zero`/`countr_zero`, `bit_ceil`/`bit_floor`, `has_single_bit`, `rotl`/`rotr`, `std::bit_cast`, `std::endian::native`; C++23 `std::byteswap` (present). Replace `__builtin_popcount` in competitive code and bitset tricks with these.

**`<numbers>`**: `std::numbers::pi`, `e`, `sqrt2`, `ln2`, `inv_sqrtpi`, `phi`, and `pi_v<float>` templates. No more `M_PI` (POSIX, not standard) or `4*atan(1)`.

**`using enum Activation;`** inside a `switch` drops the qualification. **`[[likely]]`/`[[unlikely]]`** on a statement after `if`/`else` hint block layout; use only where a profile shows a hot, skewed branch. **`std::string::starts_with`/`ends_with`**, **`map::contains`**, **`std::erase_if(container, pred)`**, **`std::ssize(c)`**, **`std::lerp`**, **`std::midpoint`** (overflow-safe binary search midpoint).

**`std::jthread`**: a `std::thread` that `request_stop()`s and `join()`s in its destructor; the callable may take a `std::stop_token` as first parameter and poll `stop_requested()`. No more forgotten `join()` → `std::terminate`. **`std::barrier`**, **`std::latch`**, **`std::counting_semaphore`**, **`std::atomic_ref`**, `std::atomic<T>::wait/notify` are all present.

---

## 15. C++23 library pieces: `expected`, `flat_map`, `string::contains`

All three **present in libc++ 21 under `-std=c++23`**, absent under `-std=c++20`:

```cpp
std::expected<double, std::string> safe_log(double x) {
    if (x <= 0) return std::unexpected(std::format("log domain error: x = {}", x));
    return std::log(x);
}
r.has_value(), *r, r.value(), r.error(), r.value_or(0.0)
safe_log(10).transform([](double v) { return 2 * v; }).and_then(safe_log).or_else(...)   // monadic chain
```

`std::expected<T, E>` is the error-handling type Chapter 10 anticipated: a `variant<T, E>` with a good API, no exceptions, no `optional` + out-parameter. `std::flat_map<K, V>` stores two sorted `vector`s (keys, values): O(log n) lookup, cache-friendly, no per-node allocation, O(n) insert — ideal for a vocabulary built once and queried millions of times; `.keys()` is the contiguous key array. `std::string::contains` completes `starts_with`/`ends_with`.

Also present in C++23 mode: `std::to_underlying`, `std::unreachable()`, `std::is_scoped_enum`, multidimensional `operator[]`, `if consteval`, deducing `this` (`auto&& self`), `std::byteswap`, `size_t` literals (`10uz`), `std::is_implicit_lifetime`. **Missing**: `std::stacktrace`, `std::move_only_function`, `std::spanstream`, `std::start_lifetime_as`, `std::generator`, and the ranges pieces of §3.

---

## 16. `<chrono>` calendar — and the time-zone situation

`std::chrono` calendar types (C++20) work: `year_month_day`, `sys_days`, `weekday`, `2026y / September / 5`, `ymd + months{1}`, `hh_mm_ss`, `floor<days>()`, and `std::format("{:%Y-%m-%d}", ymd)` with `strftime`-style specs; durations format as `5430s`.

**Time zones do not**: `zoned_time`, `current_zone()`, `locate_zone`, `get_tzdb()`, `utc_clock`, and `std::chrono::parse` are absent — libc++ reports `__cpp_lib_chrono == 201611` (the tz feature is 201907), because Apple's libc++ ships no tzdb. Options: do all arithmetic in UTC with `system_clock` + calendar types, or use Howard Hinnant's `date`/`tz` library (`brew install howard-hinnant-date`), which is what the standard was based on. Linux with GCC 14 / libstdc++ has the full tz support.

For simulations, none of this matters — use `steady_clock` for timing (Chapter 12) and `system_clock` for log timestamps.

---

## 17. Concepts for numerics templates

Chapter 13 introduced `template <std::floating_point T>`. Two more shapes you'll use constantly:

```cpp
template <class T> concept Scalar = std::floating_point<T> || std::integral<T>;
template <class R> concept ScalarRange = std::ranges::contiguous_range<R> && Scalar<std::ranges::range_value_t<R>>;
template <ScalarRange R> auto sum_sq(const R& r);            // vector<double>, array<int>, span<float>... not vector<string>

template <class M> concept MatrixLike = requires(const M& m, std::size_t i, std::size_t j) {
    { m.rows() } -> std::convertible_to<std::size_t>;
    { m.cols() } -> std::convertible_to<std::size_t>;
    { m(i, j) }  -> Scalar;
};
template <MatrixLike M> auto trace(const M& m);
```

A constraint failure is one line — `constraints not satisfied ... because 'Scalar<std::string>' evaluated to false` — instead of a 200-line instantiation trace, and overloads can be selected by concept (`contiguous_range` → SIMD path, otherwise generic). Standard concepts to know: `std::same_as`, `convertible_to`, `integral`, `floating_point`, `invocable`, `predicate`, `std::ranges::range/sized_range/random_access_range/contiguous_range/view/borrowed_range`, `std::sortable`.

---

## 18. Feature-test macros and graceful degradation

`<version>` (C++20) defines `__cpp_lib_<feature>` = `YYYYMM` for every library feature the implementation has; language features define `__cpp_<feature>` (`__cpp_concepts`, `__cpp_impl_coroutine`, `__cpp_designated_initializers`). Each header also defines the macros for its own features. The pattern:

```cpp
#include <version>
#if __cpp_lib_mdspan >= 202207L
#  include <mdspan>
   using Mat2 = std::mdspan<double, std::dextents<std::size_t, 2>>;
#else
   using Mat2 = MdSpan2<double>;        // your fallback
#endif
```

Three rules. (1) Test the **library** macro for library features, never `__cplusplus` — `__cplusplus == 202302L` on Apple clang 21 while half of C++23's library is missing. (2) Compare with `>=` against the value cppreference lists for the paper you need (`__cpp_lib_ranges >= 202110L` means "the P2415 owning_view fix is in"). (3) Macros can lag: libc++ 21 ships `views::zip` without `__cpp_lib_ranges_zip` because the macro covers the whole paper (`adjacent`, `zip_transform` are missing). For such cases add a vendor check as a *second* branch, as the example does:

```cpp
#if __cpp_lib_ranges_zip >= 202110L || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000 && __cplusplus > 202002L)
```

Write the fallback first, then the `#if` — the fallback is what most of your users run.

---

## 19. What Apple clang 21 + libc++ 21 actually supports (verified 2026-09)

Compiled tiny probes with `c++ -std=c++20` and `-std=c++23`; `example.cpp` §0 prints the same table for whatever compiler you have.

| Feature | `-std=c++20` | `-std=c++23` |
|---|---|---|
| Concepts, `<=>`, designated init, `consteval`/`constinit`, `using enum`, `[[likely]]` | yes | yes |
| Coroutines (language + `<coroutine>`) | yes | yes |
| `std::ranges` (C++20 algorithms and views) | yes (202110) | yes |
| `std::span`, `std::format`, `std::source_location`, `<bit>`, `<numbers>`, `std::jthread` | yes | yes |
| `views::zip` | no | **yes, but macro unset** |
| `views::enumerate`, `chunk`, `slide`, `stride`, `adjacent`/`pairwise`, `cartesian_product` | no | **no** |
| `views::join_with`, `chunk_by`, `as_rvalue`, `repeat`; `ranges::to`; `ranges::contains` | no | yes |
| `ranges::fold_left` | no | **no** |
| `std::mdspan` (`layout_right/left/stride`, static extents) | no | yes |
| `std::print`/`println`, range formatting | no | yes |
| `std::expected`, `std::flat_map`/`flat_set`, `string::contains`, `std::byteswap`, `to_underlying`, `unreachable` | no | yes |
| Multidimensional `operator[]`, `if consteval`, deducing `this`, `uz` literals | no | yes |
| `std::generator` | no | **no** |
| `std::stacktrace`, `std::move_only_function`, `std::spanstream` | no | **no** |
| `std::start_lifetime_as` | no | **no** (`is_implicit_lifetime` yes) |
| `<chrono>` calendar types + formatting | yes | yes |
| `<chrono>` time zones (`zoned_time`, `current_zone`, tzdb), `utc_clock`, `chrono::parse` | **no** | **no** (`__cpp_lib_chrono` = 201611) |
| Named modules (`export module` / `import`) | **only with `-fcxx-modules`** | same |
| `import std;` | no | no |
| `__cpp_modules` macro | never defined | never defined |
| `std::hardware_destructive_interference_size` | yes (256 on M-series) | yes |

Linux comparison: GCC 14 / libstdc++ has `enumerate`, `chunk`, `slide`, `stride`, `fold_left`, `std::generator`, `std::stacktrace` (with `-lstdc++exp`), `move_only_function`, time zones, and modules without extra flags, but no `std::flat_map` until GCC 15. This is why every one of those needs a fallback if your code must build on both.

---

## 20. Migration guide for numerics code

| Feature | Verdict | Where | Why / caveat |
|---|---|---|---|
| `std::span<const T>` kernel params | **Adopt now** | every kernel signature | Replaces `(T*, n)`; zero cost; call sites unchanged |
| `std::mdspan` (with `MdSpan2` fallback) | **Adopt now** behind `#if` | `Matrix::view()`, BLAS interop | The right abstraction; fallback is 40 lines |
| `std::format`/`print` | **Adopt now** | all output, logging, `formatter<Matrix>` | Type-safe; compile-time checked; C++20 needs the `fputs` shim for `print` |
| Concepts | **Adopt now** | every template (`floating_point`, `MatrixLike`, `ScalarRange`) | Readable errors; overload by concept |
| Range **algorithms** + projections | **Adopt now** | sorting particles, argmax, counts | Same speed as classic; safer |
| Range **views** | Adopt in data prep, **measure** in kernels | tokenizer, batch assembly, logging | `filter` ~2.4× slower than a loop at `-O2` (1.5× at `-O3`); `transform_reduce` never slower |
| `<=>` | Adopt | `Shape`, `Version`, indices | Two operators instead of six |
| Designated initializers | Adopt | configs (`TrainConfig{.lr=...}`) | Kills eight-double constructors |
| `consteval`/`constinit` | Adopt | lookup tables, `pow2`, constants | Compile-time guaranteed |
| `<bit>`, `<numbers>` | Adopt | everywhere | Replaces builtins and `M_PI` |
| `std::jthread`, `std::barrier` | Adopt | thread pools, per-step sync | Fewer bugs than `std::thread` |
| `std::expected` | Adopt when on C++23 | parsers, loaders, API boundaries | Fallback: `std::optional` + error code (Chapter 10) |
| `std::flat_map` | Adopt when on C++23 | vocabularies, sparse indices | Fallback: sorted `vector` + `lower_bound` |
| Coroutines (`Generator<T>`) | **Selective** | data loaders, per-batch streams, event sources | Never per element in a hot loop; `std::generator` missing |
| Modules | **Skip for now** | — | Works only with `-fcxx-modules` on Apple; no `import std;`; tooling immature |
| Time zones | Skip | — | Not in Apple's libc++ |
| `[[likely]]` | Only with a profile | rare error branches | Measurable rarely; wrong hints hurt |

Compile flags to standardize on for this course from here: `-std=c++20 -Wall -Wextra -Wconversion`; try `-std=c++23` per file when you want `mdspan`/`print`/`expected`, guarded by macros.

---

## Gotchas and undefined behavior

- **Dangling view**: `auto v = get_string() | views::split(' ');` where `get_string()` returns a `std::string` *reference member* or `string_view` into a temporary → UB. `owning_view` only saves you for rvalue *containers*.
- **`std::ranges::dangling`** returned from an algorithm on an rvalue range: compile error when used. If you meant a borrowed range, pass a `span`.
- **`filter_view::begin()` caches** the first match; iterating a `const filter_view` is a compile error, and mutating elements so the predicate changes while iterating is UB-ish (the cache lies).
- **`views::transform` with a stateful lambda by reference** (`[&calls]`): fine, but copying the view copies the lambda — a by-value counter would diverge.
- **`std::span` outlives its container** after `push_back`/`resize`: dangling, like an iterator. ASan catches it as heap-use-after-free.
- **`mdspan` with wrong extents**: no bounds checks; `m[3, 0]` on a 2×3 is out-of-bounds UB. Wrap with `assert` in debug.
- **`std::format` runtime string**: `std::format(user_fmt, x)` is a compile error (must be a constant); `std::vformat` + `std::make_format_args(lvalue)` — rvalues are rejected since P2905.
- **Coroutine lambda capturing by reference** (`[&] -> Generator<int> { co_yield x; }`) where the lambda object dies before the coroutine finishes: UB (captures live in the lambda, not the frame). Pass parameters instead.
- **Coroutine parameters taken by reference**: the reference is stored in the frame; if the referent dies before the generator finishes — dangling. `Generator<T> f(const std::vector<T>& v)` called with a temporary is the classic case. Take by value or `span` (and make sure the span's target outlives the generator).
- **Destroying a running coroutine** (`h.destroy()` from inside itself) or resuming a `done()` coroutine: UB.
- **Signed overflow in a benchmark accumulator**: Σx² over 4 M ints exceeds `INT64_MAX`; UBSan flagged it in an earlier version of the example. Use `std::uint64_t` (wraparound defined) or reduce `n`.
- **Defaulted `<=>` for a type whose member order is not the semantic order** (`Fraction`, `{den, num}`): compiles, sorts wrong.
- **`operator==` defaulted from a user-written `<=>`**: NOT automatic — a user-declared `<=>` does not generate `==`; write it or `= default` it explicitly.
- **`consteval` function called from a non-constant context**: compile error, by design.
- **`constinit` on a non-constant initializer**: compile error, by design — do not "fix" by removing it.
- **`[[likely]]` on the wrong branch**: measurable slowdowns; only under a profile.
- **`std::jthread` capturing locals by reference and outliving the scope**: same as `std::thread`; keep the `jthread` inside the scope that owns the data.
- **Testing `__cplusplus` instead of `__cpp_lib_*`**: `-std=c++23` on Apple clang 21 gives you `202302L` and no `std::generator`.
- **`-fcxx-modules` without `--precompile` ordering**: the `.pcm` must exist before any TU that imports it compiles; Make/CMake must know that.

---

## Common mistakes checklist

- [ ] Kernel signatures take `std::span<const T>` (or `mdspan`), not `const std::vector<T>&`.
- [ ] Every C++23 library use is behind `#if __cpp_lib_xxx >= YYYYMM` with a working fallback, and the file was compiled under `-std=c++20` too.
- [ ] Range views are used for data preparation; hot kernels are loops or standard algorithms (`transform_reduce`), with a measurement attached when a view is used there.
- [ ] No view over a temporary string/vector *reference*; algorithms on rvalues use `span`.
- [ ] `std::formatter<T>` specializations inherit from a numeric formatter when the spec should forward.
- [ ] `Generator<T>` is move-only, destroys its handle, rethrows stored exceptions, and takes parameters by value/`span`.
- [ ] No coroutine per element in an inner loop.
- [ ] `<=>` is defaulted only when member order equals semantic order; `==` is written when equality differs from ordering.
- [ ] `consteval` for compile-time-only functions, `constinit` for static tables; `<numbers>` instead of `M_PI`; `<bit>` instead of builtins.
- [ ] Threads are `std::jthread`; step synchronization uses `std::barrier`.
- [ ] Modules not used in the library; headers + `#pragma once` (+ `-fmodules` if compile time hurts).
- [ ] Time zones not assumed on macOS.
- [ ] Feature support decided by `<version>` macros plus a probe program, never by `__cplusplus` or by memory.

---

## You can move on when...

- You can write `ps | views::filter(alive) | views::transform(kinetic)` and say what happens at construction, at `begin()`, and per `++`, and why `filter` costs ~2.4× a raw loop at `-O2`.
- You can write a `chunk_view` with its `|` adaptor and list the requirements its iterator must satisfy for `std::ranges::forward_range` to hold.
- You can explain `std::ranges::dangling` and name four borrowed range types.
- You can view a 2×3 `std::vector<double>` as row-major, column-major and transposed `mdspan`s without copying, and state which is BLAS order.
- You can write `std::formatter<Matrix<T>>` that forwards `{:8.3f}` to each element.
- You can draw the coroutine control flow for `for (auto b : batches(X, 32))` — who calls `resume`, where `co_yield` returns to, when the frame is freed — and implement `Generator<T>` from memory.
- You can build a two-file module on this machine and name the flag Apple clang 21 requires, and say why you won't use modules in your library yet.
- You can list, without looking, five C++23 library features libc++ 21 lacks and the fallback for each.
- You can turn a Chapter 05 `Matrix<T>` API into the §20 migration table's "adopt now" column without changing any call site or losing 2% performance.
