// Chapter 17 — C++20/23 features: ranges, span/mdspan, format, coroutines, modules, and the rest.
//
// Compile (C++20, what Apple clang 21 fully supports):
//     c++ -Wall -Wextra -std=c++20 -O2 -o ex_demo example.cpp && ./ex_demo
// Compile (C++23, enables the extra sections that libc++ 21 already ships):
//     c++ -Wall -Wextra -std=c++23 -O2 -o ex_demo example.cpp && ./ex_demo
//
// This chapter REQUIRES -std=c++20. Every C++23 feature is behind a feature-test macro from
// <version> (`__cpp_lib_*`) with a fallback, so the file compiles under -std=c++20 and under
// -std=c++23 on Apple clang 21 (libc++ 210106), and prints which path it took. Section 0 prints
// the full macro table — that table IS the "what does my compiler support" report.
//
// Sections:
//   0  feature-test macros: what this compiler + library actually provides
//   1  ranges: algorithms with projections, views, laziness, adaptor closures
//   2  C++23 views (zip, enumerate, chunk, slide, ranges::to) with graceful fallbacks
//   3  writing a custom view + range adaptor closure: my::chunk (training batches)
//   4  dangling and borrowed ranges
//   5  ranges vs raw loop: measured
//   6  std::span and std::mdspan (multidimensional views over flat storage; layout policies)
//   7  std::format / std::print, formatting a user type
//   8  coroutines: Generator<T> (co_yield), a Task with co_await/co_return, batches & particle streams
//   9  modules: what works on Apple clang 21 (comment + exact commands)
//  10  <=> and operator== rewriting
//  11  designated initializers, consteval/constinit, source_location, <bit>, <numbers>
//  12  std::expected, std::flat_map, string::contains, using enum, [[likely]], starts_with, contains
//  13  std::jthread + stop_token; <chrono> calendar (and the time-zone situation)
//  14  concepts for numerics templates

#include <version>          // feature-test macros live here (and in each header)

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <compare>
#include <concepts>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <iterator>
#include <map>
#include <numbers>
#include <numeric>
#include <random>
#include <ranges>
#include <source_location>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if __cpp_lib_print >= 202207L
#include <print>
#endif
#if __cpp_lib_mdspan >= 202207L
#include <mdspan>
#endif
#if __cpp_lib_expected >= 202202L
#include <expected>
#endif
#if __cpp_lib_flat_map >= 202207L
#include <flat_map>
#endif
#if __cpp_lib_generator >= 202207L
#include <generator>
#endif

// ---------------------------------------------------------------------------------------------
// A tiny `print` shim so the rest of the file can use std::print syntax under C++20 too.
// ---------------------------------------------------------------------------------------------
template <class... Args>
static void out(std::format_string<Args...> fmt, Args&&... args) {
#if __cpp_lib_print >= 202207L
    std::print(fmt, std::forward<Args>(args)...);
#else
    std::fputs(std::format(fmt, std::forward<Args>(args)...).c_str(), stdout);
#endif
}

template <class F>
static double time_min_ms(int reps, F&& f) {
    using clock = std::chrono::steady_clock;
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = clock::now();
        f();
        best = std::min(best, std::chrono::duration<double, std::milli>(clock::now() - t0).count());
    }
    return best;
}
template <class T> static inline void do_not_optimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }

// =============================================================================================
// §0  Feature-test macros
// =============================================================================================
struct Feat { const char* name; long value; const char* what; };
#define FEAT(m, what) Feat{#m, (m), what}
#define NOFEAT(m, what) Feat{#m, 0L, what}

static void section0_features() {
    out("\n=== 0. feature-test macros (__cplusplus = {}, libc++ {}) ===\n", static_cast<long>(__cplusplus),
        static_cast<long>(_LIBCPP_VERSION));
    const Feat feats[] = {
#ifdef __cpp_concepts
        FEAT(__cpp_concepts, "concepts / requires"),
#else
        NOFEAT(__cpp_concepts, "concepts / requires"),
#endif
#ifdef __cpp_lib_ranges
        FEAT(__cpp_lib_ranges, "std::ranges (C++20 views + algorithms)"),
#else
        NOFEAT(__cpp_lib_ranges, "std::ranges"),
#endif
#ifdef __cpp_lib_ranges_zip
        FEAT(__cpp_lib_ranges_zip, "views::zip (C++23)"),
#else
        NOFEAT(__cpp_lib_ranges_zip, "views::zip (C++23) — macro unset; libc++ 21 ships zip anyway"),
#endif
#ifdef __cpp_lib_ranges_enumerate
        FEAT(__cpp_lib_ranges_enumerate, "views::enumerate (C++23)"),
#else
        NOFEAT(__cpp_lib_ranges_enumerate, "views::enumerate (C++23)"),
#endif
#ifdef __cpp_lib_ranges_chunk
        FEAT(__cpp_lib_ranges_chunk, "views::chunk (C++23)"),
#else
        NOFEAT(__cpp_lib_ranges_chunk, "views::chunk (C++23)"),
#endif
#ifdef __cpp_lib_ranges_slide
        FEAT(__cpp_lib_ranges_slide, "views::slide (C++23)"),
#else
        NOFEAT(__cpp_lib_ranges_slide, "views::slide (C++23)"),
#endif
#ifdef __cpp_lib_ranges_to_container
        FEAT(__cpp_lib_ranges_to_container, "std::ranges::to (C++23)"),
#else
        NOFEAT(__cpp_lib_ranges_to_container, "std::ranges::to (C++23)"),
#endif
#ifdef __cpp_lib_ranges_fold
        FEAT(__cpp_lib_ranges_fold, "ranges::fold_left (C++23)"),
#else
        NOFEAT(__cpp_lib_ranges_fold, "ranges::fold_left (C++23)"),
#endif
#ifdef __cpp_lib_span
        FEAT(__cpp_lib_span, "std::span"),
#else
        NOFEAT(__cpp_lib_span, "std::span"),
#endif
#ifdef __cpp_lib_mdspan
        FEAT(__cpp_lib_mdspan, "std::mdspan (C++23)"),
#else
        NOFEAT(__cpp_lib_mdspan, "std::mdspan (C++23)"),
#endif
#ifdef __cpp_lib_format
        FEAT(__cpp_lib_format, "std::format"),
#else
        NOFEAT(__cpp_lib_format, "std::format"),
#endif
#ifdef __cpp_lib_print
        FEAT(__cpp_lib_print, "std::print / println (C++23)"),
#else
        NOFEAT(__cpp_lib_print, "std::print / println (C++23)"),
#endif
#ifdef __cpp_lib_format_ranges
        FEAT(__cpp_lib_format_ranges, "format(\"{}\", vector) (C++23)"),
#else
        NOFEAT(__cpp_lib_format_ranges, "format(\"{}\", vector) (C++23)"),
#endif
#ifdef __cpp_impl_coroutine
        FEAT(__cpp_impl_coroutine, "coroutines (language)"),
#else
        NOFEAT(__cpp_impl_coroutine, "coroutines (language)"),
#endif
#ifdef __cpp_lib_generator
        FEAT(__cpp_lib_generator, "std::generator (C++23)"),
#else
        NOFEAT(__cpp_lib_generator, "std::generator (C++23)"),
#endif
#ifdef __cpp_modules
        FEAT(__cpp_modules, "modules (language macro)"),
#else
        NOFEAT(__cpp_modules, "modules — macro unset; named modules need -fcxx-modules (see §9)"),
#endif
#ifdef __cpp_impl_three_way_comparison
        FEAT(__cpp_impl_three_way_comparison, "operator<=>"),
#else
        NOFEAT(__cpp_impl_three_way_comparison, "operator<=>"),
#endif
#ifdef __cpp_lib_expected
        FEAT(__cpp_lib_expected, "std::expected (C++23)"),
#else
        NOFEAT(__cpp_lib_expected, "std::expected (C++23)"),
#endif
#ifdef __cpp_lib_flat_map
        FEAT(__cpp_lib_flat_map, "std::flat_map (C++23)"),
#else
        NOFEAT(__cpp_lib_flat_map, "std::flat_map (C++23)"),
#endif
#ifdef __cpp_lib_string_contains
        FEAT(__cpp_lib_string_contains, "string::contains (C++23)"),
#else
        NOFEAT(__cpp_lib_string_contains, "string::contains (C++23)"),
#endif
#ifdef __cpp_lib_jthread
        FEAT(__cpp_lib_jthread, "std::jthread / stop_token"),
#else
        NOFEAT(__cpp_lib_jthread, "std::jthread / stop_token"),
#endif
#ifdef __cpp_lib_chrono
        FEAT(__cpp_lib_chrono, "<chrono> (201907 = calendar+tz; 201611 = no tzdb)"),
#else
        NOFEAT(__cpp_lib_chrono, "<chrono>"),
#endif
#ifdef __cpp_lib_start_lifetime_as
        FEAT(__cpp_lib_start_lifetime_as, "std::start_lifetime_as (C++23)"),
#else
        NOFEAT(__cpp_lib_start_lifetime_as, "std::start_lifetime_as (C++23)"),
#endif
#ifdef __cpp_lib_stacktrace
        FEAT(__cpp_lib_stacktrace, "std::stacktrace (C++23)"),
#else
        NOFEAT(__cpp_lib_stacktrace, "std::stacktrace (C++23)"),
#endif
    };
    for (const Feat& f : feats)
        out("  {:<38} {:>8} {}\n", f.name, f.value ? std::format("{}", f.value) : "MISSING", f.what);
}

// =============================================================================================
// §1  Ranges: algorithms with projections, views, laziness, adaptor closures
// =============================================================================================
struct Particle { double x, y, vx, vy, mass; };

static void section1_ranges() {
    out("\n=== 1. ranges ===\n");
    std::vector<Particle> ps{{0, 0, 1, 0, 2.0}, {1, 1, 0, 1, 0.5}, {2, 0, -1, 0, 1.5}, {0, 2, 0, -1, 1.0}};

    // Algorithms take a *range* and an optional *projection*: sort by a member, no lambda needed.
    std::ranges::sort(ps, {}, &Particle::mass);                   // {} = std::ranges::less
    auto heaviest = std::ranges::max_element(ps, {}, &Particle::mass);
    out("  sorted by mass: {} {} {} {}; heaviest = {}\n", ps[0].mass, ps[1].mass, ps[2].mass, ps[3].mass,
        heaviest->mass);

    // Views are lazy, non-owning, O(1) to construct. Nothing is computed until iteration.
    int transform_calls = 0;
    auto squares_of_odds = std::views::iota(1)                    // 1, 2, 3, ... (infinite)
                         | std::views::filter([](int x) { return x % 2 == 1; })
                         | std::views::transform([&](int x) { ++transform_calls; return x * x; })
                         | std::views::take(5);
    out("  iota|filter(odd)|transform(sq)|take(5): ");
    for (int v : squares_of_odds) out("{} ", v);
    out("  — transform called {} times (lazy: only what take(5) pulled)\n", transform_calls);

    // Adaptor closures compose without a range: build the pipeline once, apply it to many ranges.
    auto kinetic = std::views::transform([](const Particle& p) { return 0.5 * p.mass * (p.vx * p.vx + p.vy * p.vy); });
    auto moving = std::views::filter([](const Particle& p) { return p.vx != 0 || p.vy != 0; });
    auto ke_of_moving = moving | kinetic;                          // closure | closure = closure
    double total = 0;
    for (double k : ps | ke_of_moving) total += k;
    out("  total KE of moving particles via a stored pipeline: {}\n", total);

    // drop / reverse / take_while / split / join
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    out("  drop(2)|reverse: ");
    for (int x : v | std::views::drop(2) | std::views::reverse) out("{} ", x);
    out("\n  take_while(<5): ");
    for (int x : v | std::views::take_while([](int x) { return x < 5; })) out("{} ", x);

    std::string text = "the quick brown fox";
    out("\n  split on ' ': ");
    for (auto word : text | std::views::split(' '))               // yields subranges of the string
        out("[{}] ", std::string_view(word.data(), word.size()));  // contiguous → data()/size()

    std::vector<std::vector<int>> nested{{1, 2}, {3}, {4, 5, 6}};
    out("\n  join: ");
    for (int x : nested | std::views::join) out("{} ", x);
    out("\n");

    // Range algorithms return more information than the classic ones: sort returns the end,
    // copy returns {in, out}, minmax returns a struct with .min/.max.
    auto [mn, mx] = std::ranges::minmax(v);
    out("  ranges::minmax(v) = [{}, {}]; ranges::count_if(even) = {}\n", mn, mx,
        std::ranges::count_if(v, [](int x) { return x % 2 == 0; }));
}

// =============================================================================================
// §2  C++23 views with fallbacks: zip / enumerate / chunk / slide / ranges::to
// =============================================================================================
// A fallback for std::ranges::to: collect any range into a vector.
template <std::ranges::input_range R>
static auto to_vector(R&& r) {
#if __cpp_lib_ranges_to_container >= 202202L
    return std::forward<R>(r) | std::ranges::to<std::vector>();
#else
    std::vector<std::ranges::range_value_t<R>> v;
    if constexpr (std::ranges::sized_range<R>) v.reserve(std::ranges::size(r));
    for (auto&& x : r) v.push_back(std::forward<decltype(x)>(x));
    return v;
#endif
}

static void section2_cpp23_views() {
    out("\n=== 2. C++23 views with fallbacks ===\n");
    std::vector<double> w{0.5, 1.5, 2.5};
    std::vector<double> g{0.1, 0.2, 0.3};

    // zip: iterate two ranges in lockstep (SGD update: w -= lr * g). libc++ 21 ships zip_view
    // under -std=c++23 but does NOT define __cpp_lib_ranges_zip (the macro covers the whole
    // paper incl. adjacent/zip_transform). Real code checks the macro, then a vendor fallback.
#if __cpp_lib_ranges_zip >= 202110L || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000 && __cplusplus > 202002L)
    for (auto [wi, gi] : std::views::zip(w, g)) wi -= 0.1 * gi;
    out("  zip (std::views::zip): ");
#else
    for (std::size_t i = 0; i < w.size(); ++i) w[i] -= 0.1 * g[i];   // fallback: index loop
    out("  zip (fallback index loop): ");
#endif
    out("w = {} {} {}\n", w[0], w[1], w[2]);

    // enumerate: (index, value) pairs. Fallback: zip-like with iota, or an index loop.
#if __cpp_lib_ranges_enumerate >= 202302L
    out("  enumerate (std): ");
    for (auto [i, x] : std::views::enumerate(w)) out("{}:{} ", i, x);
#else
    out("  enumerate (fallback iota|transform): ");
    auto enumerate = [](auto& r) {
        return std::views::iota(std::size_t{0}, std::ranges::size(r))
             | std::views::transform([&r](std::size_t i) { return std::pair<std::size_t, decltype(r[i])>{i, r[i]}; });
    };
    for (auto [i, x] : enumerate(w)) out("{}:{} ", i, x);
#endif
    out("\n");

    // chunk: batches. slide: sliding windows (moving average, finite differences).
    std::vector<int> data = to_vector(std::views::iota(0, 10));
#if __cpp_lib_ranges_chunk >= 202202L
    out("  chunk(4) (std): ");
    for (auto batch : data | std::views::chunk(4)) out("[{} elems, first {}] ", std::ranges::size(batch), batch[0]);
#else
    out("  chunk(4) (fallback, see §3 my::chunk): ");
    for (std::size_t i = 0; i < data.size(); i += 4) {
        auto batch = std::span(data).subspan(i, std::min<std::size_t>(4, data.size() - i));
        out("[{} elems, first {}] ", batch.size(), batch[0]);
    }
#endif
    out("\n");
#if __cpp_lib_ranges_slide >= 202202L
    out("  slide(3) moving sums (std): ");
    for (auto win : data | std::views::slide(3)) out("{} ", std::accumulate(win.begin(), win.end(), 0));
#else
    out("  slide(3) moving sums (fallback): ");
    for (std::size_t i = 0; i + 3 <= data.size(); ++i) out("{} ", data[i] + data[i + 1] + data[i + 2]);
#endif
    out("\n");

    // ranges::to — materialize a pipeline into a container.
    auto evens_sq = to_vector(data | std::views::filter([](int x) { return x % 2 == 0; })
                                   | std::views::transform([](int x) { return x * x; }));
#if __cpp_lib_ranges_to_container >= 202202L
    out("  ranges::to<vector> (std): ");
#else
    out("  ranges::to<vector> (fallback to_vector): ");
#endif
    for (int x : evens_sq) out("{} ", x);
    out("\n");
}

// =============================================================================================
// §3  A custom view + range adaptor closure: my::chunk (what views::chunk does)
// =============================================================================================
namespace my {

template <std::ranges::view V>
    requires std::ranges::random_access_range<V> && std::ranges::sized_range<V>
class chunk_view : public std::ranges::view_interface<chunk_view<V>> {   // view_interface: empty(), size(), front()...
    V base_;                                                       // no `{}`: ref_view has no default ctor
    std::ranges::range_difference_t<V> n_ = 1;

public:
    using Base = V;
    class iterator {
        std::ranges::iterator_t<V> cur_{};
        std::ranges::iterator_t<V> end_{};
        std::ranges::range_difference_t<V> n_ = 1;
    public:
        using value_type = std::ranges::subrange<std::ranges::iterator_t<V>>;
        using difference_type = std::ranges::range_difference_t<V>;
        using iterator_concept = std::forward_iterator_tag;       // forward is enough for range-for

        iterator() = default;
        iterator(std::ranges::iterator_t<V> c, std::ranges::iterator_t<V> e, difference_type n)
            : cur_(c), end_(e), n_(n) {}
        value_type operator*() const {                            // a subrange: [cur, cur+n) clipped to end
            auto last = (end_ - cur_ < n_) ? end_ : cur_ + n_;
            return value_type{cur_, last};
        }
        iterator& operator++() { cur_ = (end_ - cur_ < n_) ? end_ : cur_ + n_; return *this; }
        iterator operator++(int) { auto t = *this; ++*this; return t; }
        bool operator==(const iterator& o) const { return cur_ == o.cur_; }
    };

    chunk_view() requires std::default_initializable<V> = default;   // std views do the same
    chunk_view(V base, std::ranges::range_difference_t<V> n) : base_(std::move(base)), n_(n) {}
    iterator begin() { return iterator{std::ranges::begin(base_), std::ranges::end(base_), n_}; }
    iterator end() { return iterator{std::ranges::end(base_), std::ranges::end(base_), n_}; }
    std::size_t size() const {
        auto s = static_cast<std::ranges::range_difference_t<V>>(std::ranges::size(base_));
        return static_cast<std::size_t>((s + n_ - 1) / n_);
    }
};
template <class R>
chunk_view(R&&, std::ranges::range_difference_t<R>) -> chunk_view<std::views::all_t<R>>;

// The range adaptor closure object: makes both `chunk(n)(r)` and `r | chunk(n)` work. C++23 has
// std::ranges::range_adaptor_closure to derive from; in C++20 you write the operator| yourself.
struct chunk_fn {
    std::ptrdiff_t n;
    template <std::ranges::viewable_range R>
    auto operator()(R&& r) const { return chunk_view(std::views::all(std::forward<R>(r)), n); }
    template <std::ranges::viewable_range R>
    friend auto operator|(R&& r, chunk_fn f) { return f(std::forward<R>(r)); }
};
inline chunk_fn chunk(std::ptrdiff_t n) { return chunk_fn{n}; }

}  // namespace my

static_assert(std::ranges::forward_range<my::chunk_view<std::views::all_t<std::vector<int>&>>>);
static_assert(std::ranges::view<my::chunk_view<std::views::all_t<std::vector<int>&>>>);

static void section3_custom_view() {
    out("\n=== 3. custom view: my::chunk ===\n");
    std::vector<float> dataset(10);
    std::iota(dataset.begin(), dataset.end(), 0.f);
    auto batches = dataset | my::chunk(4);                        // lazy; nothing copied
    out("  {} batches of <=4 over {} samples: ", batches.size(), dataset.size());
    for (auto batch : batches) {
        float mean = std::accumulate(batch.begin(), batch.end(), 0.f) / static_cast<float>(std::ranges::size(batch));
        out("[n={} mean={}] ", std::ranges::size(batch), mean);
    }
    out("\n");
    // Composes with standard adaptors because it is a real view:
    auto batch_means = dataset | my::chunk(4)
                     | std::views::transform([](auto b) { return std::accumulate(b.begin(), b.end(), 0.f); });
    out("  my::chunk(4) | transform(sum): ");
    for (float s : batch_means) out("{} ", s);
    out("\n");
}

// =============================================================================================
// §4  Dangling and borrowed ranges
// =============================================================================================
static std::vector<int> make_temp() { return {3, 1, 4, 1, 5}; }

static void section4_dangling() {
    out("\n=== 4. dangling and borrowed ranges ===\n");
    // Passing an rvalue vector to a range algorithm would return an iterator into a destroyed
    // temporary. The ranges library returns std::ranges::dangling instead — a compile error if used.
    auto it = std::ranges::max_element(make_temp());
    static_assert(std::same_as<decltype(it), std::ranges::dangling>);
    out("  ranges::max_element(rvalue vector) has type std::ranges::dangling (unusable — good)\n");

    // A *borrowed* range is one whose iterators outlive the range object: span, string_view,
    // subrange, and any type that opts in with enable_borrowed_range. Those are fine as rvalues.
    std::vector<int> owner = make_temp();
    auto it2 = std::ranges::max_element(std::span(owner));          // span is borrowed → real iterator
    static_assert(std::ranges::borrowed_range<std::span<int>>);
    static_assert(!std::ranges::borrowed_range<std::vector<int>>);
    out("  ranges::max_element(rvalue span) = {}  (span is a borrowed_range)\n", *it2);

    // Views over a temporary: `auto v = make_temp() | views::filter(...)` is OK ONLY because
    // `owning_view` (C++20 DR P2415) moves the vector into the view. Views over a *dangling
    // reference* are still UB: `std::string_view sv = make_string();` style.
    auto owning = make_temp() | std::views::filter([](int x) { return x > 2; });
    out("  filter over an rvalue vector (owning_view): ");
    for (int x : owning) out("{} ", x);
    out("\n");
}

// =============================================================================================
// §5  Ranges vs raw loop — measured
// =============================================================================================
static void section5_perf() {
    out("\n=== 5. ranges vs raw loop (numbers depend on -O2/-O3; lesson.md §6 shows both) ===\n");
    const int n = 4'000'000;
    std::vector<int> xs(n);
    std::iota(xs.begin(), xs.end(), 0);
    // Sum of x^2 for x < 4e6 is ~1e19: past INT64_MAX. Unsigned wraps (defined); signed would be UB
    // — UBSan caught exactly that in an earlier version of this file.
    using U = std::uint64_t;
    U sink = 0;

    double t_raw = time_min_ms(5, [&] {
        U s = 0;
        for (int x : xs) if (x % 2 == 0) s += static_cast<U>(x) * static_cast<U>(x);
        sink += s;
    });
    auto even = [](int x) { return x % 2 == 0; };
    auto sq = [](int x) { return static_cast<U>(x) * static_cast<U>(x); };
    double t_view = time_min_ms(5, [&] {
        U s = 0;
        for (U v : xs | std::views::filter(even) | std::views::transform(sq)) s += v;
        sink += s;
    });
    double t_acc = time_min_ms(5, [&] {
        auto view = xs | std::views::filter(even) | std::views::transform(sq);
        sink += std::accumulate(view.begin(), view.end(), U{0});
    });
    double t_alg = time_min_ms(5, [&] {                              // no filter: transform_reduce vectorizes
        sink += std::transform_reduce(xs.begin(), xs.end(), U{0}, std::plus<>{},
                                      [](int x) { return x % 2 == 0 ? static_cast<U>(x) * static_cast<U>(x) : U{0}; });
    });
    do_not_optimize(sink);
    out("  sum of squares of evens, n = {}:\n", n);
    out("    raw loop                      {:8.3f} ms\n", t_raw);
    out("    filter|transform range-for    {:8.3f} ms  ({:.2f}x raw)\n", t_view, t_view / t_raw);
    out("    accumulate over the view      {:8.3f} ms  ({:.2f}x raw)\n", t_acc, t_acc / t_raw);
    out("    transform_reduce, branchless  {:8.3f} ms  ({:.2f}x raw)\n", t_alg, t_alg / t_raw);
}

// =============================================================================================
// §6  std::span and std::mdspan
// =============================================================================================
static double dot(std::span<const double> a, std::span<const double> b) {   // kernel signature
    double s = 0;
    for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

#if !(__cpp_lib_mdspan >= 202207L)
// Fallback: a minimal 2-D mdspan showing what a *layout policy* is — a mapping (i, j) -> offset.
struct LayoutRight { static std::size_t idx(std::size_t i, std::size_t j, std::size_t rows, std::size_t cols) { (void)rows; return i * cols + j; } };
struct LayoutLeft  { static std::size_t idx(std::size_t i, std::size_t j, std::size_t rows, std::size_t cols) { (void)cols; return i + j * rows; } };
template <class T, class Layout = LayoutRight>
struct MdSpan2 {
    T* p; std::size_t rows, cols;
    T& operator()(std::size_t i, std::size_t j) const { return p[Layout::idx(i, j, rows, cols)]; }
    std::size_t extent(int d) const { return d == 0 ? rows : cols; }
};
#endif

static void section6_span_mdspan() {
    out("\n=== 6. span and mdspan ===\n");
    std::vector<double> a{1, 2, 3, 4, 5, 6}, b(6, 1.0);
    std::array<double, 6> arr{6, 5, 4, 3, 2, 1};
    out("  dot(vector, vector) = {}, dot(array, first 3 of vector) = {}\n", dot(a, b),
        dot(std::span(arr).first(3), std::span(a).first(3)));
    std::span<double, 3> fixed(a.data(), 3);                      // static extent: size known at compile time
    static_assert(sizeof(fixed) == sizeof(double*), "fixed-extent span is one pointer");
    out("  span<double,3> is {} bytes; span<double> is {} bytes; subspan(2,2) = [{}, {}]\n", sizeof fixed,
        sizeof(std::span<double>), std::span(a).subspan(2, 2)[0], std::span(a).subspan(2, 2)[1]);

    // Multidimensional view over the same flat storage: 2x3 row-major and 2x3 column-major.
#if __cpp_lib_mdspan >= 202207L
    std::mdspan<double, std::dextents<std::size_t, 2>> m(a.data(), 2, 3);                       // layout_right
    std::mdspan<double, std::dextents<std::size_t, 2>, std::layout_left> ml(a.data(), 2, 3);   // Fortran/BLAS order
    out("  std::mdspan (C++23): extents {}x{}; row-major m[1,2]={} col-major ml[1,2]={}; strides right=({},{}) left=({},{})\n",
        m.extent(0), m.extent(1), m[1, 2], ml[1, 2], m.stride(0), m.stride(1), ml.stride(0), ml.stride(1));
    // Static extents: a 3x3 rotation matrix with no runtime size stored at all.
    std::mdspan<double, std::extents<std::size_t, 3, 2>> fixed3(a.data());
    out("  mdspan with static extents<3,2>: sizeof = {} (just the pointer); fixed3[2,1] = {}\n", sizeof fixed3, fixed3[2, 1]);
    // layout_stride: arbitrary strides — a transpose without moving data.
    std::layout_stride::mapping<std::dextents<std::size_t, 2>> tmap(std::dextents<std::size_t, 2>(3, 2), std::array<std::size_t, 2>{1, 3});
    std::mdspan<double, std::dextents<std::size_t, 2>, std::layout_stride> mt(a.data(), tmap);
    out("  transpose via layout_stride: mt[2,1] = {} (== m[1,2] = {})\n", mt[2, 1], m[1, 2]);
#else
    MdSpan2<double> m{a.data(), 2, 3};
    MdSpan2<double, LayoutLeft> ml{a.data(), 2, 3};
    out("  std::mdspan MISSING under this -std; fallback MdSpan2: row-major m(1,2)={} col-major ml(1,2)={}\n",
        m(1, 2), ml(1, 2));
    out("  (compile with -std=c++23 to run the real std::mdspan path: layout_right/left/stride, static extents)\n");
#endif
}

// =============================================================================================
// §7  std::format / std::print, user-type formatter
// =============================================================================================
struct Vec3 { double x, y, z; };

// A formatter that accepts the same spec as double ("{:8.3f}") and applies it to each component.
template <>
struct std::formatter<Vec3> : std::formatter<double> {
    auto format(const Vec3& v, std::format_context& ctx) const {
        auto o = std::format_to(ctx.out(), "(");
        o = std::formatter<double>::format(v.x, ctx); o = std::format_to(o, ", ");
        o = std::formatter<double>::format(v.y, ctx); o = std::format_to(o, ", ");
        o = std::formatter<double>::format(v.z, ctx);
        return std::format_to(o, ")");
    }
};

static void section7_format() {
    out("\n=== 7. std::format / std::print ===\n");
    out("  |{:>10.3f}|{:<8}|{:^9}|{:#x}|{:08.2e}|{:+d}|{:e}|\n", std::numbers::pi, "left", "mid", 255, 12345.678, 42, 0.1);
    out("  width from argument: |{:{}}|  fill: |{:*^12}|  bool: {}  char: {:c}\n", "ab", 6, "mid", true, 65);
    out("  user type with forwarded spec: {:.2f}  default: {}\n", Vec3{1, 2.5, -3.25}, Vec3{0, 0, 1});
    std::string runtime_fmt = "{:.1f} kg";                        // not a constant expression →
    double kg = 2.5;                                               // make_format_args takes lvalues (P2905, applied as a DR)
    out("  runtime format string via vformat: {}\n", std::vformat(runtime_fmt, std::make_format_args(kg)));
    // std::format("{:d}", 1.5) would be a COMPILE error: format strings are checked at compile time.
#if __cpp_lib_format_ranges >= 202207L
    std::vector<int> v{1, 2, 3};
    std::map<std::string, double> m{{"lr", 0.01}, {"momentum", 0.9}};
    out("  format ranges (C++23): {} {} {:n}\n", v, m, v);
#else
    out("  format ranges: MISSING under this -std (needs -std=c++23; libc++ 21 has it)\n");
#endif
#if __cpp_lib_print >= 202207L
    std::println("  std::println is live (C++23): {} + {} = {}", 1, 2, 3);
#else
    out("  std::print MISSING under this -std; using fputs(std::format(...)) shim\n");
#endif
}

// =============================================================================================
// §8  Coroutines
// =============================================================================================
// Generator<T>: a lazy sequence. `co_yield v` suspends the coroutine and hands v to the consumer.
// The three pieces: (1) promise_type — what the compiler talks to; (2) coroutine_handle — resume
// and destroy the frame; (3) an iterator so range-for works.
template <class T>
class Generator {
public:
    struct promise_type {
        T value_{};
        std::exception_ptr exc_;
        Generator get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }   // lazy: don't run until first ++
        std::suspend_always final_suspend() noexcept { return {}; }     // keep the frame so done() is observable
        std::suspend_always yield_value(T v) noexcept { value_ = std::move(v); return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { exc_ = std::current_exception(); }
        // No await_transform: co_await inside a generator is a compile error here, on purpose.
    };
    using handle_type = std::coroutine_handle<promise_type>;

    explicit Generator(handle_type h) : h_(h) {}
    Generator(Generator&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    Generator& operator=(Generator&& o) noexcept {
        if (this != &o) { if (h_) h_.destroy(); h_ = std::exchange(o.h_, {}); }
        return *this;
    }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    ~Generator() { if (h_) h_.destroy(); }                        // frees the coroutine frame (heap)

    class iterator {
        handle_type h_{};
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        iterator() = default;
        explicit iterator(handle_type h) : h_(h) {}
        bool operator==(std::default_sentinel_t) const { return !h_ || h_.done(); }
        iterator& operator++() { h_.resume(); rethrow(); return *this; }
        void operator++(int) { ++*this; }
        const T& operator*() const { return h_.promise().value_; }
        void rethrow() { if (h_.promise().exc_) std::rethrow_exception(h_.promise().exc_); }
    };
    iterator begin() { iterator it{h_}; if (h_) { h_.resume(); it.rethrow(); } return it; }
    std::default_sentinel_t end() { return {}; }

private:
    handle_type h_;
};

// Lazily yield mini-batches over a dataset: no copies, the consumer pulls one batch at a time.
static Generator<std::span<const float>> batches(std::span<const float> data, std::size_t batch_size) {
    for (std::size_t i = 0; i < data.size(); i += batch_size)
        co_yield data.subspan(i, std::min(batch_size, data.size() - i));
}

// An infinite stream of particles: the consumer decides how many to take.
static Generator<Particle> particle_stream(unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> pos(0.0, 1.0), vel(0.0, 0.1);
    std::uniform_real_distribution<double> mass(0.5, 2.0);
    for (;;) co_yield Particle{pos(rng), pos(rng), vel(rng), vel(rng), mass(rng)};
}

static Generator<int> throws_after(int n) {
    for (int i = 0; i < n; ++i) co_yield i;
    throw std::runtime_error("generator exhausted with an error");
}

// A Task<T>: co_return a value; the caller resumes it with get(). Shows co_await with a custom
// awaiter — the three hooks await_ready / await_suspend / await_resume.
template <class T>
class Task {
public:
    struct promise_type {
        T result_{};
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(T v) { result_ = std::move(v); }         // co_return v
        void unhandled_exception() { std::terminate(); }
    };
    using handle_type = std::coroutine_handle<promise_type>;
    explicit Task(handle_type h) : h_(h) {}
    Task(Task&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    Task(const Task&) = delete;
    ~Task() { if (h_) h_.destroy(); }
    T get() { while (!h_.done()) h_.resume(); return h_.promise().result_; }   // drive to completion
private:
    handle_type h_;
};

struct Checkpoint {                                               // an awaiter
    const char* label;
    static inline int suspensions = 0;
    bool await_ready() const noexcept { return false; }           // false → call await_suspend
    bool await_suspend(std::coroutine_handle<>) noexcept {         // return false → don't actually suspend
        ++suspensions; out("    [checkpoint '{}' reached, suspensions so far = {}]\n", label, suspensions);
        return false;                                              // (true would hand control back to get())
    }
    int await_resume() const noexcept { return suspensions; }      // value of the co_await expression
};

static Task<double> integrate_with_checkpoints(int n) {
    double h = 1.0 / n, s = 0;
    for (int i = 0; i < n; ++i) { double x = (i + 0.5) * h; s += 4.0 / (1 + x * x) * h; }
    int k = co_await Checkpoint{"after midpoint sum"};
    (void)k;
    co_return s;                                                   // ≈ π
}

static void section8_coroutines() {
    out("\n=== 8. coroutines ===\n");
    std::vector<float> dataset(10);
    std::iota(dataset.begin(), dataset.end(), 1.f);
    int nb = 0;
    for (std::span<const float> b : batches(dataset, 4)) {
        out("  batch {}: n={} sum={}\n", nb++, b.size(), std::accumulate(b.begin(), b.end(), 0.f));
    }

    int taken = 0; double total_mass = 0;
    for (const Particle& p : particle_stream(42)) {              // infinite; we stop
        total_mass += p.mass;
        if (++taken == 1000) break;                                // destroying the Generator frees the frame
    }
    out("  took {} particles from an infinite stream, mean mass = {:.3f}\n", taken, total_mass / taken);

    try {
        int s = 0;
        for (int v : throws_after(3)) s += v;
        out("  (unreachable) {}\n", s);
    } catch (const std::exception& e) {
        out("  exception thrown inside a coroutine propagates to the consumer: '{}'\n", e.what());
    }

    out("  Task with co_await/co_return:\n");
    double pi_est = integrate_with_checkpoints(100000).get();
    out("    integral = {:.6f} (pi = {:.6f})\n", pi_est, std::numbers::pi);

#if __cpp_lib_generator >= 202207L
    out("  std::generator (C++23) is available\n");
#else
    out("  std::generator (C++23): MISSING in libc++ 21 — the hand-written Generator<T> above is the substitute\n");
#endif
}

// =============================================================================================
// §9  Modules — see lesson.md §12. Verified on Apple clang 21 (clang-2100.1.1.101):
//
//   // math.cppm                       // use.cpp
//   export module math;                import math;
//   export int sq(int x) { return x*x; }   int main() { return sq(7) - 49; }
//
//   c++ -std=c++20 -fcxx-modules --precompile -o math.pcm math.cppm          # -> BMI
//   c++ -std=c++20 -fcxx-modules -fmodule-file=math=math.pcm -o use use.cpp math.pcm
//
//   Without -fcxx-modules, Apple clang rejects `export module` ("expected template"). `import std;`
//   does not work (no std.cppm shipped in the Xcode toolchain). `__cpp_modules` is never defined.
//   Clang *header* modules (-fmodules, implicit, for #include) work out of the box and are what
//   Xcode uses for system headers.
// =============================================================================================
static void section9_modules() {
    out("\n=== 9. modules ===\n");
    out("  Named modules on Apple clang 21: WORK with -fcxx-modules (two-step: --precompile, then link the .pcm).\n");
    out("  `import std;` : NOT available. __cpp_modules: {}. Header modules (-fmodules): available.\n",
#ifdef __cpp_modules
        static_cast<long>(__cpp_modules));
#else
        "undefined");
#endif
}

// =============================================================================================
// §10  <=> and operator== rewriting
// =============================================================================================
struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;             // also defaults operator== → strong_ordering
};
struct Approx {                                                    // doubles: partial ordering (NaN)
    double v;
    std::partial_ordering operator<=>(const Approx& o) const { return v <=> o.v; }
    bool operator==(const Approx& o) const { return std::abs(v - o.v) < 1e-9; }
};
struct Meters { double m; };
struct Length {
    double m;
    // Heterogeneous: only ONE direction written; `Meters{1} < Length{2}` is found by *reversing*.
    std::weak_ordering operator<=>(const Meters& o) const { return std::weak_order(m, o.m); }
    bool operator==(const Meters& o) const { return m == o.m; }
};

static void section10_spaceship() {
    out("\n=== 10. <=> and == rewriting ===\n");
    Version a{1, 2, 3}, b{1, 3, 0};
    out("  Version 1.2.3 < 1.3.0: {}; == : {}; != (rewritten from ==): {}\n", a < b, a == b, a != b);
    auto c = a <=> b;
    static_assert(std::same_as<decltype(c), std::strong_ordering>);
    out("  (a <=> b) is strong_ordering: less? {}  std::is_lt: {}\n", c == std::strong_ordering::less, std::is_lt(c));

    Approx x{1.0}, y{1.0 + 1e-12}, nan{std::numeric_limits<double>::quiet_NaN()};
    out("  Approx: x == y (tolerance): {}; x < y (exact): {}; x <=> NaN unordered: {}\n", x == y, x < y,
        (x <=> nan) == std::partial_ordering::unordered);

    Length L{2.0}; Meters M{1.0};
    out("  heterogeneous: Length{{2}} > Meters{{1}}: {}; Meters{{1}} < Length{{2}} (reversed candidate): {}; Meters{{1}} != Length{{2}}: {}\n",
        L > M, M < L, M != L);
    // Sorting by <=> works through std::ranges::less automatically:
    std::vector<Version> vs{{2, 0, 0}, {1, 9, 9}, {1, 10, 0}};
    std::ranges::sort(vs);
    out("  sorted Versions: {}.{}.{} {}.{}.{} {}.{}.{}\n", vs[0].major, vs[0].minor, vs[0].patch, vs[1].major,
        vs[1].minor, vs[1].patch, vs[2].major, vs[2].minor, vs[2].patch);
}

// =============================================================================================
// §11  Designated initializers, consteval/constinit, source_location, <bit>, <numbers>
// =============================================================================================
struct TrainConfig { double lr = 1e-3; int epochs = 10; int batch = 32; bool shuffle = true; };

consteval std::uint64_t pow2(int k) { return std::uint64_t{1} << k; }  // MUST run at compile time
constinit std::uint64_t g_table_size = pow2(12);                       // static init, guaranteed constant
constexpr std::array<double, 4> make_weights() {                        // constexpr with std::array
    std::array<double, 4> w{};
    for (int i = 0; i < 4; ++i) w[static_cast<std::size_t>(i)] = 1.0 / (i + 1);
    return w;
}
constexpr auto kWeights = make_weights();

static void check(bool ok, std::string_view what, std::source_location loc = std::source_location::current()) {
    out("  [{}] {}  ({}:{} in {})\n", ok ? "ok" : "FAIL", what, loc.file_name(), loc.line(), loc.function_name());
}

static void section11_misc20() {
    out("\n=== 11. designated init, consteval/constinit, source_location, <bit>, <numbers> ===\n");
    TrainConfig cfg{.lr = 0.01, .epochs = 3};                       // named, in order, rest defaulted
    out("  TrainConfig{{.lr=0.01, .epochs=3}}: lr={} epochs={} batch={} shuffle={}\n", cfg.lr, cfg.epochs,
        cfg.batch, cfg.shuffle);
    static_assert(pow2(10) == 1024);
    out("  consteval pow2(12) → constinit g_table_size = {}; constexpr kWeights[3] = {}\n", g_table_size, kWeights[3]);
    check(kWeights[0] == 1.0, "kWeights computed at compile time");

    unsigned x = 0b1011'0000u;
    out("  <bit>: popcount({:#b})={} bit_width={} countr_zero={} bit_ceil(100)={} has_single_bit(64)={} rotl(1,3)={}\n",
        x, std::popcount(x), std::bit_width(x), std::countr_zero(x), std::bit_ceil(100u),
        std::has_single_bit(64u), std::rotl(1u, 3));
    out("  bit_cast<uint32_t>(1.0f) = {:#010x}; endian: {}\n", std::bit_cast<std::uint32_t>(1.0f),
        std::endian::native == std::endian::little ? "little" : "big");
    out("  <numbers>: pi={} e={} sqrt2={} inv_sqrt_pi={} pi_v<float>={}\n", std::numbers::pi, std::numbers::e,
        std::numbers::sqrt2, std::numbers::inv_sqrtpi, std::numbers::pi_v<float>);
}

// =============================================================================================
// §12  expected, flat_map, string::contains, using enum, [[likely]], starts_with, contains
// =============================================================================================
enum class Activation { ReLU, Tanh, GELU };

static const char* act_name(Activation a) {
    using enum Activation;                                         // C++20: bring enumerators into scope
    switch (a) {
        case ReLU: return "relu";
        case Tanh: return "tanh";
        case GELU: return "gelu";
    }
    return "?";
}

#if __cpp_lib_expected >= 202202L
static std::expected<double, std::string> safe_log(double x) {
    if (x <= 0) return std::unexpected(std::format("log domain error: x = {}", x));
    return std::log(x);
}
#endif

static double clamp_grad(double g) {
    if (std::abs(g) > 1e3) [[unlikely]] return g > 0 ? 1e3 : -1e3;   // rare path: hint the layout
    return g;
}

static void section12_misc23() {
    out("\n=== 12. expected / flat_map / contains / using enum / likely ===\n");
#if __cpp_lib_expected >= 202202L
    auto r1 = safe_log(std::numbers::e), r2 = safe_log(-1.0);
    out("  std::expected (C++23): log(e) = {}; log(-1) → error: '{}'; value_or(0) = {}\n", r1.value(), r2.error(),
        r2.value_or(0.0));
    auto chained = safe_log(10.0).transform([](double v) { return v * 2; }).and_then([](double v) { return safe_log(v); });
    out("  monadic: log(10)*2 then log → {:.4f}\n", chained.value());
#else
    out("  std::expected: MISSING under this -std (C++23; libc++ 21 has it with -std=c++23; see ../10_error_handling)\n");
#endif
#if __cpp_lib_flat_map >= 202207L
    std::flat_map<std::string, int> vocab{{"the", 3}, {"a", 1}, {"fox", 2}};   // sorted vectors, cache-friendly
    out("  std::flat_map (C++23): {} keys, contains(\"fox\") = {}, keys are contiguous: first key '{}'\n",
        vocab.size(), vocab.contains("fox"), vocab.keys()[0]);
#else
    out("  std::flat_map: MISSING under this -std (C++23; libc++ 21 has it with -std=c++23). Use sorted vector + lower_bound.\n");
#endif
    std::string name = "layer3.weight";
    out("  starts_with(\"layer\") = {}, ends_with(\".bias\") = {}", name.starts_with("layer"), name.ends_with(".bias"));
#if __cpp_lib_string_contains >= 202011L
    out(", contains(\"weight\") = {} (C++23)\n", name.contains("weight"));
#else
    out(", contains: MISSING → find != npos: {}\n", name.find("weight") != std::string::npos);
#endif
    std::map<std::string, double> params{{"lr", 0.1}};
    out("  map::contains (C++20): {} {}\n", params.contains("lr"), params.contains("momentum"));
    out("  using enum: {} {} {}; clamp_grad(1e9) = {} ([[unlikely]] path)\n", act_name(Activation::ReLU),
        act_name(Activation::Tanh), act_name(Activation::GELU), clamp_grad(1e9));
}

// =============================================================================================
// §13  std::jthread + stop_token; <chrono> calendar and time zones
// =============================================================================================
static void section13_jthread_chrono() {
    out("\n=== 13. jthread, chrono calendar ===\n");
    std::atomic<long> iterations{0};
    {
        std::jthread worker([&](std::stop_token st) {             // receives a stop_token automatically
            while (!st.stop_requested()) { ++iterations; std::this_thread::yield(); }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }                                                              // ~jthread: request_stop() then join()
    out("  jthread ran {} iterations, stopped and joined by its destructor (no detach/join boilerplate)\n",
        iterations.load());

    using namespace std::chrono;
    year_month_day ymd = 2026y / September / 5;                    // calendar types (C++20)
    sys_days d = ymd;                                              // days since 1970-01-01
    weekday wd{d};
    auto next_month = ymd + months{1};
    out("  {:%Y-%m-%d} is a {}; day-of-year {}; +1 month = {:%Y-%m-%d}; days since epoch = {}\n", ymd, wd,
        (d - sys_days{ymd.year() / January / 1}).count() + 1, next_month, d.time_since_epoch().count());
    auto dur = 90min + 30s;
    out("  durations format: {} = {} = {:%H:%M:%S}\n", dur, duration_cast<seconds>(dur), hh_mm_ss{dur});
#if __cpp_lib_chrono >= 201907L
    out("  time zones: current_zone()->name() = {}\n", current_zone()->name());
#else
    out("  time zones (zoned_time, current_zone, tzdb): MISSING — __cpp_lib_chrono = {} (< 201907). libc++ on macOS\n"
        "  ships no tzdb; use system_clock + calendar arithmetic in UTC, or Howard Hinnant's date/tz library.\n",
        static_cast<long>(__cpp_lib_chrono));
#endif
}

// =============================================================================================
// §14  Concepts for numerics templates
// =============================================================================================
template <class T>
concept Scalar = std::floating_point<T> || std::integral<T>;

template <class R>
concept ScalarRange = std::ranges::contiguous_range<R> && Scalar<std::ranges::range_value_t<R>>;

template <ScalarRange R>
auto sum_sq(const R& r) {
    using T = std::ranges::range_value_t<R>;
    T s{};
    for (T x : r) s += x * x;                                      // contiguous → vectorizes
    return s;
}

template <class M>
concept MatrixLike = requires(const M& m, std::size_t i, std::size_t j) {
    { m.rows() } -> std::convertible_to<std::size_t>;
    { m.cols() } -> std::convertible_to<std::size_t>;
    { m(i, j) } -> Scalar;
};
struct DenseF { std::vector<float> d; std::size_t r, c; std::size_t rows() const { return r; } std::size_t cols() const { return c; } float operator()(std::size_t i, std::size_t j) const { return d[i * c + j]; } };
template <MatrixLike M> auto trace(const M& m) { decltype(m(0, 0)) t{}; for (std::size_t i = 0; i < std::min(m.rows(), m.cols()); ++i) t += m(i, i); return t; }

static void section14_concepts() {
    out("\n=== 14. concepts ===\n");
    std::vector<double> v{1, 2, 3};
    std::array<int, 3> a{1, 2, 3};
    out("  sum_sq(vector<double>) = {}, sum_sq(array<int>) = {}\n", sum_sq(v), sum_sq(a));
    static_assert(ScalarRange<std::vector<float>> && !ScalarRange<std::vector<std::string>>);
    static_assert(MatrixLike<DenseF> && !MatrixLike<std::vector<float>>);
    DenseF I{{1, 0, 0, 1}, 2, 2};
    out("  trace(MatrixLike) = {}; sum_sq(vector<string>) would fail with 'constraints not satisfied', not 200 lines of template errors\n",
        trace(I));
}

// =============================================================================================
int main() {
    out("Chapter 17 — C++20/23 features (this binary: -std={})\n", __cplusplus >= 202302L ? "c++23" : "c++20");
    section0_features();
    section1_ranges();
    section2_cpp23_views();
    section3_custom_view();
    section4_dangling();
    section5_perf();
    section6_span_mdspan();
    section7_format();
    section8_coroutines();
    section9_modules();
    section10_spaceship();
    section11_misc20();
    section12_misc23();
    section13_jthread_chrono();
    section14_concepts();
    return 0;
}
