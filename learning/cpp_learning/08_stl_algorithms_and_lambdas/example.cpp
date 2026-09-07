// Chapter 08 — STL Algorithms and Lambdas.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo
//
// What this program demonstrates:
//   1. Lambda syntax and every capture form ([x], [&x], [=], [&], init-capture, mutable, generic).
//   2. Lambdas are objects: distinct types, sizeof, conversion of captureless lambdas to function
//      pointers, and the three ways to hold one (auto / template parameter / std::function).
//   3. Lambdas replacing C's `f(x, void *ctx)` callbacks: a generic trapezoid integrator.
//   4. <algorithm> / <numeric> survey on real ML-ish data: sort with comparator, stable_sort,
//      partial_sort, nth_element (median), argmax, argsort, accumulate, inner_product (dot),
//      transform (softmax), count_if, find_if, all_of/any_of, copy_if + back_inserter,
//      erase-remove idiom, unique, reverse, iota, lower_bound/upper_bound/binary_search,
//      reduce / transform_reduce, function objects from <functional>.
//   5. <random> done right: one mt19937_64, distributions, seeding, shuffle for minibatches,
//      Gaussian weight init — and the "new engine per call" bug shown explicitly.
//   6. A place where the hand-written loop is the right tool (fused axpy over three arrays).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Print helper: `os << vec` for any vector of printable T.
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    os << '[';
    for (std::size_t i = 0; i < v.size(); ++i) os << (i ? ", " : "") << v[i];
    return os << ']';
}

// ---------------------------------------------------------------------------------------------
// 3. Generic integrator: F is deduced; the lambda is inlined into the loop. No void* context.
// ---------------------------------------------------------------------------------------------
template <typename F>
double trapezoid(F f, double a, double b, int n) {
    const double h = (b - a) / n;
    double s = 0.5 * (f(a) + f(b));
    for (int i = 1; i < n; ++i) s += f(a + i * h);
    return s * h;
}

// Three ways to accept a callable. Same body, different costs.
template <typename F> double apply_twice_tmpl(F f, double x) { return f(f(x)); }
double apply_twice_fn(const std::function<double(double)>& f, double x) { return f(f(x)); }
double apply_twice_c(double (*f)(double), double x) { return f(f(x)); }

// The mutable-capture accumulator: an object with state, returned from a function.
// Captures by VALUE (init-capture), so nothing dangles after make_running_mean returns.
auto make_running_mean() {
    return [n = 0, mean = 0.0](double x) mutable {
        ++n;
        mean += (x - mean) / n;            // Welford's online mean
        return mean;
    };
}

// The bug from the lesson: a fresh engine per call yields the same number every call.
double gauss_sample_BAD() {
    std::mt19937_64 rng(42);               // WRONG: reseeded every call
    std::normal_distribution<double> N(0.0, 1.0);
    return N(rng);
}
double gauss_sample(std::mt19937_64& rng) {   // RIGHT: engine passed by reference, advances
    std::normal_distribution<double> N(0.0, 1.0);
    return N(rng);
}

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // =========================================================================================
    // 1. Captures
    // =========================================================================================
    std::cout << "=== 1. Captures ===\n";
    {
        double lr = 0.1;
        int calls = 0;
        auto step_by_value = [lr](double w, double g) { return w - lr * g; };   // lr frozen at 0.1
        auto step_by_ref   = [&lr](double w, double g) { return w - lr * g; };  // sees later changes
        auto counted       = [&calls](double w) { ++calls; return w; };
        lr = 0.5;
        std::cout << "  by value (lr frozen 0.1): " << step_by_value(1.0, 1.0)
                  << "   by ref (lr now 0.5): " << step_by_ref(1.0, 1.0) << '\n';
        counted(0); counted(0); counted(0);
        std::cout << "  calls counted through [&calls]: " << calls << '\n';

        auto rm = make_running_mean();                   // stateful closure, safe to return
        rm(2.0); rm(4.0);
        std::cout << "  running mean after 2,4,9: " << rm(9.0) << '\n';

        std::vector<double> big(1000, 1.0);
        auto owns_big = [v = std::move(big)] { return v.size(); };   // init-capture by move
        std::cout << "  init-capture moved vector: closure sees " << owns_big()
                  << " elements; original now has " << big.size() << '\n';

        auto add = [](auto a, auto b) { return a + b; };            // generic lambda
        std::cout << "  generic add: " << add(1, 2) << ' ' << add(1.5, 2.25) << ' '
                  << add(std::string("con"), std::string("cat")) << '\n';
    }

    // =========================================================================================
    // 2. Lambdas are objects
    // =========================================================================================
    std::cout << "\n=== 2. Closure types ===\n";
    {
        auto f = [](double x) { return 2 * x; };
        auto g = [](double x) { return 2 * x; };         // identical text, DIFFERENT type
        static_assert(!std::is_same<decltype(f), decltype(g)>::value, "each lambda is unique");
        double a = 1, b = 2, c = 3;
        auto by_val = [a, b, c](double x) { return x + a + b + c; };
        auto by_ref = [&a, &b, &c](double x) { return x + a + b + c; };
        std::cout << "  sizeof captureless=" << sizeof(f) << "  three doubles by value=" << sizeof(by_val)
                  << "  three by reference=" << sizeof(by_ref) << "  std::function=" << sizeof(std::function<double(double)>) << '\n';

        double (*fp)(double) = f;                        // captureless -> function pointer
        std::cout << "  as C function pointer: " << fp(21.0) << '\n';

        std::cout << "  template: " << apply_twice_tmpl(f, 1.0) << "  std::function: " << apply_twice_fn(f, 1.0)
                  << "  fn-pointer: " << apply_twice_c(f, 1.0) << '\n';
        // apply_twice_c(by_val, 1.0);   // ERROR: a capturing lambda has no function-pointer conversion

        std::function<double(double)> act;               // empty
        std::cout << "  empty std::function is " << (act ? "callable" : "not callable") << "; ";
        act = [](double x) { return std::tanh(x); };
        act = [](double x) { return x > 0 ? x : 0.0; };  // reassign a different lambda: fine
        std::cout << "after assignment act(-1)=" << act(-1.0) << ", act(2)=" << act(2.0) << '\n';
    }

    // =========================================================================================
    // 3. Lambda callbacks vs void* ctx
    // =========================================================================================
    std::cout << "\n=== 3. Lambda as callback (no void* ctx) ===\n";
    {
        double mu = 0.0, sigma = 1.0;
        auto gauss = [&](double x) {                     // by-ref is safe: used right here
            double z = (x - mu) / sigma;
            return std::exp(-0.5 * z * z) / (sigma * std::sqrt(2 * M_PI));
        };
        std::cout << "  integral of N(0,1) over [-5,5]  = " << trapezoid(gauss, -5, 5, 2000) << '\n';
        sigma = 2.0;                                     // lambda sees the change
        std::cout << "  integral of N(0,2) over [-10,10]= " << trapezoid(gauss, -10, 10, 2000) << '\n';
        std::cout << "  integral of x^2 over [0,1]      = " << trapezoid([](double x) { return x * x; }, 0, 1, 1000) << '\n';
    }

    // =========================================================================================
    // 4. Algorithms
    // =========================================================================================
    std::cout << "\n=== 4. <algorithm> / <numeric> ===\n";
    std::vector<double> logits = {2.0, -1.0, 0.1, 3.5, 0.1, -0.7};
    {
        // --- argmax: THE prediction step
        auto it = std::max_element(logits.begin(), logits.end());
        std::cout << "  logits " << logits << "  argmax=" << (it - logits.begin()) << " value=" << *it << '\n';
        auto [lo, hi] = std::minmax_element(logits.begin(), logits.end());
        std::cout << "  min=" << *lo << " max=" << *hi << '\n';

        // --- softmax with transform + accumulate
        const double mx = *it;
        std::vector<double> probs(logits.size());
        std::transform(logits.begin(), logits.end(), probs.begin(), [mx](double x) { return std::exp(x - mx); });
        const double Z = std::accumulate(probs.begin(), probs.end(), 0.0);        // 0.0, NOT 0
        std::transform(probs.begin(), probs.end(), probs.begin(), [Z](double p) { return p / Z; });
        std::cout << "  softmax " << probs << "  sum=" << std::accumulate(probs.begin(), probs.end(), 0.0) << '\n';

        // --- reductions: dot product, sum of squares, L1 via transform_reduce
        std::vector<double> w = {0.5, -0.5, 1.0, 0.0, 2.0, 1.0};
        double dot = std::inner_product(logits.begin(), logits.end(), w.begin(), 0.0);
        double l2sq = std::inner_product(w.begin(), w.end(), w.begin(), 0.0);
        double l1 = std::transform_reduce(w.begin(), w.end(), 0.0, std::plus<>{}, [](double x) { return std::fabs(x); });
        double sum2 = std::reduce(w.begin(), w.end(), 0.0);
        std::cout << "  dot(logits,w)=" << dot << "  |w|^2=" << l2sq << "  |w|_1=" << l1 << "  reduce(w)=" << sum2 << '\n';

        // --- argsort: iota + sort with comparator capturing the data by reference
        std::vector<std::size_t> idx(logits.size());
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](std::size_t i, std::size_t j) { return logits[i] > logits[j]; });
        std::cout << "  argsort descending: " << idx << '\n';

        // --- sorting variants on a copy
        std::vector<double> s = logits;
        std::sort(s.begin(), s.end());                               // ascending
        std::cout << "  sorted: " << s;
        std::sort(s.begin(), s.end(), std::greater<>{});             // <functional> functor
        std::cout << "  desc: " << s << '\n';
        std::vector<double> p = logits;
        std::partial_sort(p.begin(), p.begin() + 2, p.end(), std::greater<>{});   // top-2 in front
        std::cout << "  top-2 via partial_sort: " << p[0] << ' ' << p[1] << '\n';
        std::vector<double> m = logits;
        std::nth_element(m.begin(), m.begin() + m.size() / 2, m.end());
        std::cout << "  nth_element upper median: " << m[m.size() / 2] << '\n';

        // --- stable_sort keeps the order of equal keys
        struct Rec { std::string name; int label; };
        std::vector<Rec> recs = {{"d", 1}, {"a", 0}, {"c", 1}, {"b", 0}};
        std::stable_sort(recs.begin(), recs.end(), [](const Rec& x, const Rec& y) { return x.label < y.label; });
        std::cout << "  stable_sort by label: ";
        for (const auto& r : recs) std::cout << r.name << r.label << ' ';
        std::cout << "(a,b keep input order; so do d,c)\n";

        // --- predicates
        auto n_pos = std::count_if(logits.begin(), logits.end(), [](double x) { return x > 0; });
        auto first_neg = std::find_if(logits.begin(), logits.end(), [](double x) { return x < 0; });
        bool all_finite = std::all_of(logits.begin(), logits.end(), [](double x) { return std::isfinite(x); });
        bool any_big = std::any_of(logits.begin(), logits.end(), [](double x) { return x > 3; });
        bool none_nan = std::none_of(logits.begin(), logits.end(), [](double x) { return std::isnan(x); });
        std::cout << "  count>0: " << n_pos << "  first<0 at index " << (first_neg - logits.begin())
                  << "  all_finite=" << all_finite << " any>3=" << any_big << " none_nan=" << none_nan << '\n';

        // --- copy_if + back_inserter (filter into a new vector)
        std::vector<double> positives;
        std::copy_if(logits.begin(), logits.end(), std::back_inserter(positives), [](double x) { return x > 0; });
        std::cout << "  positives: " << positives << '\n';

        // --- erase-remove idiom, then sort+unique+erase, then reverse
        std::vector<double> dirty = {1.0, NAN, 2.0, 2.0, INFINITY, 3.0, NAN, 3.0};
        dirty.erase(std::remove_if(dirty.begin(), dirty.end(), [](double x) { return !std::isfinite(x); }), dirty.end());
        std::cout << "  after erase-remove non-finite: " << dirty;
        std::sort(dirty.begin(), dirty.end());
        dirty.erase(std::unique(dirty.begin(), dirty.end()), dirty.end());
        std::cout << "  dedup: " << dirty;
        std::reverse(dirty.begin(), dirty.end());
        std::cout << "  reversed: " << dirty << '\n';

        // --- binary search family on the sorted copy `s` (currently descending: re-sort ascending)
        std::sort(s.begin(), s.end());
        auto lb = std::lower_bound(s.begin(), s.end(), 0.1);
        auto ub = std::upper_bound(s.begin(), s.end(), 0.1);
        std::cout << "  sorted " << s << "  lower_bound(0.1)=" << (lb - s.begin()) << " upper_bound(0.1)=" << (ub - s.begin())
                  << " (count of 0.1 = " << (ub - lb) << ")  binary_search(2.0)=" << std::binary_search(s.begin(), s.end(), 2.0) << '\n';

        // --- inverse-transform sampling uses lower_bound on a CDF
        std::vector<double> weights = {0.1, 0.5, 0.2, 0.2}, cdf(weights.size());
        std::partial_sum(weights.begin(), weights.end(), cdf.begin());
        std::cout << "  cdf via partial_sum: " << cdf << "  u=0.65 falls in bin "
                  << (std::lower_bound(cdf.begin(), cdf.end(), 0.65) - cdf.begin()) << '\n';
    }

    // =========================================================================================
    // 5. <random>
    // =========================================================================================
    std::cout << "\n=== 5. <random> ===\n";
    {
        std::cout << "  BAD (new engine per call): ";
        for (int i = 0; i < 4; ++i) std::cout << gauss_sample_BAD() << ' ';
        std::cout << "  <- identical every call\n";

        std::mt19937_64 rng(42);                           // ONE engine, seeded once
        std::cout << "  GOOD (shared engine):      ";
        for (int i = 0; i < 4; ++i) std::cout << gauss_sample(rng) << ' ';
        std::cout << '\n';

        // Gaussian weight init (He), then check the sample statistics with algorithms.
        const std::size_t fan_in = 256, n = 100000;
        std::normal_distribution<double> init(0.0, std::sqrt(2.0 / fan_in));
        std::vector<double> W(n);
        std::generate(W.begin(), W.end(), [&] { return init(rng); });
        double mean = std::accumulate(W.begin(), W.end(), 0.0) / n;
        double var = std::inner_product(W.begin(), W.end(), W.begin(), 0.0) / n - mean * mean;
        std::cout << "  He init: target std=" << std::sqrt(2.0 / fan_in) << "  sample mean=" << mean
                  << " sample std=" << std::sqrt(var) << '\n';

        // Minibatch shuffle: iota + shuffle, then iterate in chunks.
        std::vector<std::size_t> order(10);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);     // engine by reference
        std::cout << "  shuffled indices: " << order << "  batches of 4: ";
        for (std::size_t s = 0; s < order.size(); s += 4) {
            auto first = order.begin() + s;
            auto last = order.begin() + std::min(s + 4, order.size());
            std::cout << '[' << (last - first) << "] ";
        }
        std::cout << '\n';

        std::uniform_int_distribution<int> die(1, 6);
        std::bernoulli_distribution coin(0.3);
        std::uniform_real_distribution<double> U(0.0, 1.0);
        std::cout << "  die=" << die(rng) << " coin=" << coin(rng) << " U=" << U(rng)
                  << "   sizeof(mt19937_64)=" << sizeof(rng) << " bytes (pass by reference!)\n";
    }

    // =========================================================================================
    // 6. When a loop is better: fused y = a*x + b*z over three arrays
    // =========================================================================================
    std::cout << "\n=== 6. The loop that should stay a loop ===\n";
    {
        std::vector<double> x(5), z(5), y(5);
        std::iota(x.begin(), x.end(), 1.0);
        std::iota(z.begin(), z.end(), 10.0);
        const double a = 2.0, b = 0.5;
        for (std::size_t i = 0; i < x.size(); ++i) y[i] = a * x[i] + b * z[i];   // one pass, clear, vectorises
        std::cout << "  y = 2x + 0.5z = " << y << "  (a transform would need a fake zip of two inputs)\n";
    }
    return 0;
}
