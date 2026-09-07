// Chapter 14 — Advanced templates and metaprogramming.
//
// REQUIRES C++20 (concepts, requires-expressions, class-type NTTPs, consteval/constinit,
// std::type_identity, constexpr std::array algorithms). Apple clang 21 supports all of it.
//
// Compile:  c++ -Wall -Wextra -std=c++20 -O2 -o ex_demo example.cpp
// Run:      ./ex_demo
// Watch the compiler work:
//           c++ -std=c++20 -O2 -ftime-trace -c example.cpp -o /dev/null  (open example.json in
//           chrome://tracing or https://ui.perfetto.dev — see lesson §22)
// See what a SFINAE error looks like vs a concepts error: uncomment the two lines marked
// "ERROR DEMO" at the end of main and compile again.
//
// Sections (each is a numbered function called from main):
//   1  template argument deduction, forwarding references, reference collapsing, std::forward
//   2  parameter packs: recursion vs fold, sizeof..., index_sequence over a tuple
//   3  type traits: <type_traits> survey, our own traits, the void_t detection idiom
//   4  SFINAE (enable_if, expression SFINAE) — then the same thing as a concept
//   5  tag dispatch vs if constexpr vs concepts overloading; subsumption
//   6  CRTP: Comparable<T> mixin, static polymorphism
//   7  expression templates: Vec and Matrix where a*b + c allocates nothing (asserted vs naive)
//   8  constexpr programming: sin table, popcount table, consteval, constinit
//   9  integral_constant, class-type NTTPs (Shape), template template params, policy-based Matrix
//  10  decltype(auto), auto return deduction, std::invoke, std::apply, tuple metaprogramming
//  11  compile-time strings (fixed_string NTTP) and std::array tricks
//
// Everything asserted; the program prints one line per check and exits 0.

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// A tiny type-name printer for demos. __PRETTY_FUNCTION__ is a clang/gcc extension; we only use it
// to *display* deduced types, never for logic.
template <class T>
std::string type_name() {
    std::string_view p = __PRETTY_FUNCTION__;          // "std::string type_name() [T = int &]"
    auto b = p.find("T = ") + 4;
    auto e = p.rfind(']');
    return std::string(p.substr(b, e - b));
}

// =====================================================================================
// 1. Deduction, forwarding references, reference collapsing, std::forward
// =====================================================================================
namespace s1 {

// P/A deduction: the parameter type P = T&& with argument A. If A is an lvalue of type U,
// T deduces to U& and T&& collapses to U&. If A is an rvalue, T = U and T&& = U&&.
template <class T>
std::string classify(T&& x) {
    (void)x;
    // Reference collapsing rules ([dcl.ref]/6):  & &  -> &,  & && -> &,  && & -> &,  && && -> &&
    if constexpr (std::is_lvalue_reference_v<T>) return "lvalue  (T = " + type_name<T>() + ")";
    else                                          return "rvalue  (T = " + type_name<T>() + ")";
}

struct Tracker {                                         // reports whether it was copied or moved
    std::string log;
    Tracker() = default;
    Tracker(const Tracker& o) : log(o.log + "copy;") {}
    Tracker(Tracker&& o) noexcept : log(std::move(o.log) + "move;") {}
};

// The canonical make_unique: forward each argument with its original value category.
template <class T, class... Args>
std::unique_ptr<T> make(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

struct Holder {
    Tracker t;
    explicit Holder(Tracker x) : t(std::move(x)) {}     // take by value, move into place
};

// Deduction does NOT happen through: implicit conversions, non-deduced contexts, braced-init.
template <class T> T twice(T x) { return x + x; }
template <class T> void takes_vec(const std::vector<T>&) {}

void run() {
    std::puts("--- 1. deduction and forwarding");
    int i = 3;
    const int ci = 4;
    std::cout << "classify(i)     -> " << classify(i) << '\n';        // T = int&
    std::cout << "classify(ci)    -> " << classify(ci) << '\n';       // T = const int&
    std::cout << "classify(7)     -> " << classify(7) << '\n';        // T = int
    std::cout << "classify(move)  -> " << classify(std::move(i)) << '\n';

    Tracker src;
    auto h1 = make<Holder>(src);              // Args = Tracker&  -> forward gives lvalue -> copy, then move
    auto h2 = make<Holder>(std::move(src));   // Args = Tracker   -> forward gives rvalue -> move, then move
    std::printf("make<Holder>(lvalue): %s   make<Holder>(rvalue): %s\n", h1->t.log.c_str(), h2->t.log.c_str());
    assert(h1->t.log == "copy;move;" && h2->t.log == "move;move;");

    // twice(1, 2.0) would fail: T deduced as int from one arg and double from the other.
    // takes_vec({1,2,3}) fails: a braced-init-list is a non-deduced context. Both are compile errors.
    assert(twice(2.5) == 5.0);
    takes_vec(std::vector<int>{1, 2, 3});
    static_assert(std::is_same_v<decltype(twice(1)), int>);
}
}  // namespace s1

// =====================================================================================
// 2. Parameter packs: recursion vs fold, sizeof..., index_sequence
// =====================================================================================
namespace s2 {

// C++11 style: recursion needs a base case and instantiates N functions.
template <class T> T sum_rec(T x) { return x; }
template <class T, class... R> T sum_rec(T x, R... r) { return x + sum_rec(r...); }

// C++17 style: a fold expression. One instantiation, one expression.
template <class... T> auto sum_fold(T... x) { return (x + ... + 0); }   // binary right fold with init
template <class... T> bool all_positive(T... x) { return ((x > 0) && ...); }
template <class... T> constexpr std::size_t count(T...) { return sizeof...(T); }   // sizeof... counts pack elements

// Fold over the comma operator = "for each argument".
template <class... T> void print_all(T const&... x) { ((std::cout << x << ' '), ...); std::cout << '\n'; }

// Iterating a tuple: expand an index_sequence<0,1,...,N-1> into std::get<I>(t)...
template <class Tuple, class F, std::size_t... I>
void for_each_impl(Tuple&& t, F&& f, std::index_sequence<I...>) {
    (f(std::get<I>(std::forward<Tuple>(t))), ...);
}
template <class Tuple, class F>
void tuple_for_each(Tuple&& t, F&& f) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_cvref_t<Tuple>>;
    for_each_impl(std::forward<Tuple>(t), std::forward<F>(f), std::make_index_sequence<N>{});
}

void run() {
    std::puts("--- 2. parameter packs");
    assert(sum_rec(1, 2, 3, 4) == 10);
    assert(sum_fold(1.5, 2.5, 3.0) == 7.0);
    assert(all_positive(1, 2, 3) && !all_positive(1, -2, 3));
    static_assert(count(1, 'a', 2.0) == 3 && count() == 0);
    print_all("layer", 3, 2.5, 'x');
    auto t = std::make_tuple(1, std::string("two"), 3.0);
    std::cout << "tuple_for_each: ";
    tuple_for_each(t, [](const auto& e) { std::cout << e << " | "; });
    std::cout << '\n';
}
}  // namespace s2

// =====================================================================================
// 3. Type traits: survey, our own, void_t detection
// =====================================================================================
namespace s3 {

// Our own trait: is T one of our matrix types? Primary template = false, specializations = true.
template <class T> struct Mat { T* p = nullptr; std::size_t rows = 0, cols = 0; };
template <class T> struct is_matrix : std::false_type {};
template <class T> struct is_matrix<Mat<T>> : std::true_type {};
template <class T> inline constexpr bool is_matrix_v = is_matrix<T>::value;

// A trait that transforms a type: element type of "anything with value_type or a Mat".
template <class T, class = void> struct element_type { using type = T; };
template <class T> struct element_type<T, std::void_t<typename T::value_type>> { using type = typename T::value_type; };
template <class T> struct element_type<Mat<T>, void> { using type = T; };
template <class T> using element_type_t = typename element_type<T>::type;

// The void_t detection idiom (C++17): "does the expression t.size() compile?"
template <class T, class = void> struct has_size : std::false_type {};
template <class T> struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};

// Same detection with a C++20 concept: one line, and it reads as English.
template <class T> concept HasSize = requires(T t) { { t.size() } -> std::convertible_to<std::size_t>; };

void run() {
    std::puts("--- 3. type traits");
    // Survey of <type_traits> you will actually use:
    static_assert(std::is_same_v<std::remove_cvref_t<const int&>, int>);        // strip const/ref
    static_assert(std::is_same_v<std::decay_t<int[3]>, int*>);                  // array->pointer, fn->ptr
    static_assert(std::is_floating_point_v<double> && std::is_integral_v<char>);
    static_assert(std::is_arithmetic_v<float> && !std::is_arithmetic_v<std::string>);
    static_assert(std::is_trivially_copyable_v<double[4]>);                     // memcpy is legal
    static_assert(std::is_nothrow_move_constructible_v<std::vector<int>>);      // vector reallocation moves
    static_assert(std::is_same_v<std::common_type_t<int, double>, double>);     // result of int + double
    static_assert(std::is_same_v<std::conditional_t<(sizeof(void*) == 8), std::int64_t, std::int32_t>, std::int64_t>);
    static_assert(std::is_invocable_r_v<double, double (*)(double), float>);
    static_assert(std::is_base_of_v<std::true_type, has_size<std::vector<int>>>);

    static_assert(is_matrix_v<Mat<double>> && !is_matrix_v<std::vector<double>>);
    static_assert(std::is_same_v<element_type_t<std::vector<float>>, float>);
    static_assert(std::is_same_v<element_type_t<Mat<double>>, double>);
    static_assert(std::is_same_v<element_type_t<int>, int>);
    static_assert(has_size<std::string>::value && !has_size<int>::value);
    static_assert(HasSize<std::vector<int>> && !HasSize<double>);
    std::puts("all static_asserts passed (traits evaluated at compile time)");
}
}  // namespace s3

// =====================================================================================
// 4. SFINAE and its concepts rewrite
// =====================================================================================
namespace s4 {

// (a) enable_if in the return type: pick an overload by a type property.
template <class T>
std::enable_if_t<std::is_integral_v<T>, T> half_sfinae(T x) { return x / 2; }
template <class T>
std::enable_if_t<std::is_floating_point_v<T>, T> half_sfinae(T x) { return x * T(0.5); }

// (b) expression SFINAE: viable only if x.norm() is a valid expression.
template <class T>
auto magnitude_sfinae(const T& x) -> decltype(x.norm()) { return x.norm(); }
template <class T>
auto magnitude_sfinae(const T& x) -> std::enable_if_t<std::is_arithmetic_v<T>, T> { return x < 0 ? -x : x; }

// (c) The concepts rewrite. Same overload set, readable, and the error message names the concept.
template <std::integral T>       T half(T x) { return x / 2; }
template <std::floating_point T> T half(T x) { return x * T(0.5); }

template <class T> concept HasNorm = requires(const T& x) { { x.norm() } -> std::floating_point; };
template <HasNorm T>              auto magnitude(const T& x) { return x.norm(); }
template <class T> requires std::is_arithmetic_v<T>
T magnitude(T x) { return x < 0 ? -x : x; }

struct V2 { double x, y; double norm() const { return std::sqrt(x * x + y * y); } };

void run() {
    std::puts("--- 4. SFINAE vs concepts");
    assert(half_sfinae(7) == 3 && half_sfinae(7.0) == 3.5);
    assert(half(7) == 3 && half(7.0) == 3.5);
    assert(magnitude_sfinae(V2{3, 4}) == 5.0 && magnitude_sfinae(-2) == 2);
    assert(magnitude(V2{3, 4}) == 5.0 && magnitude(-2.5) == 2.5);
    std::puts("both overload sets pick the same functions; only the error messages differ");
}
}  // namespace s4

// =====================================================================================
// 5. Tag dispatch vs if constexpr vs constrained overloads; subsumption
// =====================================================================================
namespace s5 {

// Tag dispatch (pre-C++17): the iterator-category trick from the standard library.
template <class It>
void advance_impl(It& it, int n, std::random_access_iterator_tag) { it += n; }
template <class It>
void advance_impl(It& it, int n, std::input_iterator_tag) { while (n-- > 0) ++it; }
template <class It>
void my_advance(It& it, int n) {
    advance_impl(it, n, typename std::iterator_traits<It>::iterator_category{});
}

// if constexpr (C++17): one function, branches discarded at compile time. Best when the two
// bodies are small and share most of their code.
template <class It>
void my_advance2(It& it, int n) {
    if constexpr (std::random_access_iterator<It>) it += n;
    else while (n-- > 0) ++it;
}

// Constrained overloads (C++20) with SUBSUMPTION: random_access_iterator subsumes forward_iterator,
// so the more constrained overload wins for vector iterators — no ambiguity.
template <std::forward_iterator It>       int kind(It) { return 1; }
template <std::random_access_iterator It> int kind(It) { return 2; }

// Subsumption works only through named concepts, not through equivalent bare expressions.
template <class T> concept Number   = std::is_arithmetic_v<T>;
template <class T> concept Floating = Number<T> && std::is_floating_point_v<T>;   // Floating subsumes Number
template <Number T>   const char* describe(T) { return "number"; }
template <Floating T> const char* describe(T) { return "floating number"; }       // preferred for double

void run() {
    std::puts("--- 5. dispatch strategies");
    std::vector<int> v{0, 1, 2, 3, 4, 5};
    auto a = v.begin(); my_advance(a, 3);
    auto b = v.begin(); my_advance2(b, 3);
    assert(*a == 3 && *b == 3);
    assert(kind(v.begin()) == 2);
    assert(std::string(describe(3)) == "number" && std::string(describe(3.0)) == "floating number");
    std::puts("tag dispatch, if constexpr and constrained overloads agree; subsumption picked the tighter overload");
}
}  // namespace s5

// =====================================================================================
// 6. CRTP: Comparable<T> mixin, static polymorphism
// =====================================================================================
namespace s6 {

// Mixin: derive from Comparable<You>, define only operator< and operator==, get the other four.
template <class D>
struct Comparable {
    friend bool operator!=(const D& a, const D& b) { return !(a == b); }
    friend bool operator> (const D& a, const D& b) { return b < a; }
    friend bool operator<=(const D& a, const D& b) { return !(b < a); }
    friend bool operator>=(const D& a, const D& b) { return !(a < b); }
};
struct Version : Comparable<Version> {
    int major, minor;
    Version(int ma, int mi) : major(ma), minor(mi) {}     // a base class makes brace-init of members awkward; give a ctor
    friend bool operator< (const Version& a, const Version& b) { return std::tie(a.major, a.minor) < std::tie(b.major, b.minor); }
    friend bool operator==(const Version& a, const Version& b) { return a.major == b.major && a.minor == b.minor; }
};

// Static polymorphism: the base calls derived().step() with no vtable and full inlining.
template <class D>
struct Integrator {
    void run(int steps) { for (int i = 0; i < steps; ++i) derived().step(); }
    D&       derived()       { return static_cast<D&>(*this); }
    const D& derived() const { return static_cast<const D&>(*this); }
};
struct Euler : Integrator<Euler> {
    double x = 0, v = 1, dt = 0.1;
    void step() { x += v * dt; v -= x * dt; }               // symplectic Euler on a harmonic oscillator
};

// Chaining mixins: each CRTP base adds a capability.
template <class D> struct Printable { void print() const { std::cout << static_cast<const D&>(*this).name() << '\n'; } };
template <class D> struct Counted   { static inline int live = 0; Counted() { ++live; } ~Counted() { --live; } };
struct Particle : Printable<Particle>, Counted<Particle> { const char* name() const { return "Particle"; } };

void run() {
    std::puts("--- 6. CRTP");
    Version a{1, 2}, b{1, 10};
    assert(a < b && a != b && b > a && a <= b && b >= a);
    Euler e; e.run(10);
    assert(e.x > 0.8 && e.x < 1.0);
    { Particle p, q; assert(Counted<Particle>::live == 2); p.print(); }
    assert(Counted<Particle>::live == 0);
    std::puts("Comparable<Version> generated 4 operators; Integrator<Euler> ran with no virtual calls");
}
}  // namespace s6

// =====================================================================================
// 7. Expression templates: Vec and Matrix, a*b + c with zero temporaries
// =====================================================================================
namespace et {

// Every expression node derives from Expr<Derived> (CRTP). A node knows its size and how to
// compute element i. Nothing is evaluated until assignment to a concrete Vec.
template <class D>
struct Expr {
    const D& self() const { return static_cast<const D&>(*this); }
    double operator[](std::size_t i) const { return self()[i]; }
    std::size_t size() const { return self().size(); }
};

struct Vec : Expr<Vec> {
    std::vector<double> d;
    static inline int evaluations = 0;                     // counts how many element-wise loops ran

    Vec() = default;
    explicit Vec(std::size_t n, double v = 0.0) : d(n, v) {}
    Vec(std::initializer_list<double> il) : d(il) {}

    // THE ONLY loop: assignment from any expression. This is where the whole tree collapses
    // into one pass — one load per operand per element, one store, no heap traffic.
    template <class E>
    Vec(const Expr<E>& e) : d(e.size()) { assign(e); }
    template <class E>
    Vec& operator=(const Expr<E>& e) { d.resize(e.size()); assign(e); return *this; }

    template <class E>
    void assign(const Expr<E>& e) {
        ++evaluations;
        for (std::size_t i = 0; i < d.size(); ++i) d[i] = e[i];   // e[i] inlines to a*b+c per element
    }
    double  operator[](std::size_t i) const { return d[i]; }
    double& operator[](std::size_t i)       { return d[i]; }
    std::size_t size() const { return d.size(); }
};

// Binary node. Operands are stored BY REFERENCE for Vec (they outlive the expression) and BY
// VALUE for nested expression nodes (they are temporaries and would dangle otherwise).
template <class T> using operand_t = std::conditional_t<std::is_same_v<T, Vec>, const Vec&, T>;

template <class L, class R, class Op>
struct BinOp : Expr<BinOp<L, R, Op>> {
    operand_t<L> l; operand_t<R> r;
    BinOp(const L& l_, const R& r_) : l(l_), r(r_) { assert(l.size() == r.size()); }
    double operator[](std::size_t i) const { return Op::apply(l[i], r[i]); }
    std::size_t size() const { return l.size(); }
};
struct Add { static double apply(double a, double b) { return a + b; } };
struct Sub { static double apply(double a, double b) { return a - b; } };
struct Mul { static double apply(double a, double b) { return a * b; } };

// Scalar * expression node.
template <class E>
struct Scale : Expr<Scale<E>> {
    double s; operand_t<E> e;
    Scale(double s_, const E& e_) : s(s_), e(e_) {}
    double operator[](std::size_t i) const { return s * e[i]; }
    std::size_t size() const { return e.size(); }
};

// Operators are constrained so they only match our expression types — this is what keeps
// `1.0 + 2.0` or `std::vector + std::vector` from ever seeing these overloads.
template <class E> concept IsExpr = std::derived_from<E, Expr<E>>;

template <IsExpr L, IsExpr R> auto operator+(const L& l, const R& r) { return BinOp<L, R, Add>(l, r); }
template <IsExpr L, IsExpr R> auto operator-(const L& l, const R& r) { return BinOp<L, R, Sub>(l, r); }
template <IsExpr L, IsExpr R> auto operator*(const L& l, const R& r) { return BinOp<L, R, Mul>(l, r); }   // element-wise
template <IsExpr E> auto operator*(double s, const E& e) { return Scale<E>(s, e); }
template <IsExpr E> auto operator*(const E& e, double s) { return Scale<E>(s, e); }

// Reductions can consume an expression without materializing it either.
template <IsExpr E> double sum(const E& e) { double s = 0; for (std::size_t i = 0; i < e.size(); ++i) s += e[i]; return s; }
template <IsExpr A, IsExpr B> double dot(const A& a, const B& b) { return sum(a * b); }

// A dense row-major Matrix with the same machinery for element-wise ops. Matrix PRODUCT is the
// famous exception: A*B needs every element of both operands many times, so lazy per-element
// evaluation would recompute rows/columns O(n) times. Eigen's Product<> node therefore evaluates
// eagerly into a temporary (or directly into the destination when it can prove no aliasing).
template <class D>
struct MExpr {
    const D& self() const { return static_cast<const D&>(*this); }
    double operator()(std::size_t i, std::size_t j) const { return self()(i, j); }
    std::size_t rows() const { return self().rows(); }
    std::size_t cols() const { return self().cols(); }
};
struct Matrix : MExpr<Matrix> {
    std::size_t r = 0, c = 0; std::vector<double> d;
    Matrix() = default;
    Matrix(std::size_t r_, std::size_t c_, double v = 0.0) : r(r_), c(c_), d(r_ * c_, v) {}
    template <class E> Matrix(const MExpr<E>& e) : r(e.rows()), c(e.cols()), d(r * c) { assign(e); }
    template <class E> Matrix& operator=(const MExpr<E>& e) { r = e.rows(); c = e.cols(); d.resize(r * c); assign(e); return *this; }
    template <class E> void assign(const MExpr<E>& e) {
        for (std::size_t i = 0; i < r; ++i) for (std::size_t j = 0; j < c; ++j) d[i * c + j] = e(i, j);
    }
    double  operator()(std::size_t i, std::size_t j) const { return d[i * c + j]; }
    double& operator()(std::size_t i, std::size_t j)       { return d[i * c + j]; }
    std::size_t rows() const { return r; }
    std::size_t cols() const { return c; }
};
template <class T> using moperand_t = std::conditional_t<std::is_same_v<T, Matrix>, const Matrix&, T>;
template <class L, class R, class Op>
struct MBinOp : MExpr<MBinOp<L, R, Op>> {
    moperand_t<L> l; moperand_t<R> r;
    MBinOp(const L& l_, const R& r_) : l(l_), r(r_) { assert(l.rows() == r.rows() && l.cols() == r.cols()); }
    double operator()(std::size_t i, std::size_t j) const { return Op::apply(l(i, j), r(i, j)); }
    std::size_t rows() const { return l.rows(); }
    std::size_t cols() const { return l.cols(); }
};
template <class E>
struct MScale : MExpr<MScale<E>> {
    double s; moperand_t<E> e;
    MScale(double s_, const E& e_) : s(s_), e(e_) {}
    double operator()(std::size_t i, std::size_t j) const { return s * e(i, j); }
    std::size_t rows() const { return e.rows(); }
    std::size_t cols() const { return e.cols(); }
};
// Eager product node: computes into an owned Matrix at construction (what Eigen does for A*B).
struct MProd : MExpr<MProd> {
    Matrix m;
    template <class A, class B>
    MProd(const MExpr<A>& a, const MExpr<B>& b) : m(a.rows(), b.cols()) {
        assert(a.cols() == b.rows());
        for (std::size_t i = 0; i < a.rows(); ++i)
            for (std::size_t k = 0; k < a.cols(); ++k) {
                const double aik = a(i, k);
                for (std::size_t j = 0; j < b.cols(); ++j) m(i, j) += aik * b(k, j);
            }
    }
    double operator()(std::size_t i, std::size_t j) const { return m(i, j); }
    std::size_t rows() const { return m.rows(); }
    std::size_t cols() const { return m.cols(); }
};
template <class E> concept IsMExpr = std::derived_from<E, MExpr<E>>;
template <IsMExpr L, IsMExpr R> auto operator+(const L& l, const R& r) { return MBinOp<L, R, Add>(l, r); }
template <IsMExpr L, IsMExpr R> auto operator-(const L& l, const R& r) { return MBinOp<L, R, Sub>(l, r); }
template <IsMExpr L, IsMExpr R> auto operator*(const L& l, const R& r) { return MProd(l, r); }   // matrix product
template <IsMExpr E> auto operator*(double s, const E& e) { return MScale<E>(s, e); }

}  // namespace et

namespace s7 {
// Naive reference implementations to assert against.
std::vector<double> naive_axpby(const std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c) {
    std::vector<double> ab(a.size()); for (std::size_t i = 0; i < a.size(); ++i) ab[i] = a[i] * b[i];      // temp 1
    std::vector<double> out(a.size()); for (std::size_t i = 0; i < a.size(); ++i) out[i] = ab[i] + c[i];    // temp 2
    return out;
}

void run() {
    using namespace et;
    std::puts("--- 7. expression templates");
    const std::size_t n = 1000;
    Vec a(n), b(n), c(n);
    for (std::size_t i = 0; i < n; ++i) { a[i] = 0.5 * i; b[i] = 1.0 / (i + 1); c[i] = std::sin(0.01 * i); }

    Vec::evaluations = 0;
    Vec r = a * b + c;                        // ONE loop, no temporaries; type of RHS is BinOp<BinOp<Vec,Vec,Mul>,Vec,Add>
    Vec r2 = 2.0 * (a - b) + 0.5 * c;         // still one loop
    assert(Vec::evaluations == 2);

    auto ref = naive_axpby(a.d, b.d, c.d);
    for (std::size_t i = 0; i < n; ++i) {
        assert(r[i] == ref[i]);                                                  // bit-identical: same ops, same order
        assert(std::abs(r2[i] - (2.0 * (a[i] - b[i]) + 0.5 * c[i])) < 1e-12);
    }
    double d1 = dot(a, b), d2 = 0; for (std::size_t i = 0; i < n; ++i) d2 += a[i] * b[i];
    assert(std::abs(d1 - d2) < 1e-9);
    std::cout << "type of a*b+c: " << type_name<decltype(a * b + c)>() << '\n';
    std::printf("Vec: two expressions -> %d loops total (naive needs 4 loops + 2 temporaries); results identical\n", Vec::evaluations);

    // Matrix: element-wise lazy, product eager.
    Matrix A(4, 3), B(3, 5), C(4, 5);
    for (std::size_t i = 0; i < 4; ++i) for (std::size_t j = 0; j < 3; ++j) A(i, j) = double(i + j);
    for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 5; ++j) B(i, j) = double(i * j) - 1.0;
    for (std::size_t i = 0; i < 4; ++i) for (std::size_t j = 0; j < 5; ++j) C(i, j) = 0.25 * double(i) - double(j);
    Matrix R = A * B + 2.0 * C;               // MProd computed once, then one fused pass for + and *2
    for (std::size_t i = 0; i < 4; ++i) for (std::size_t j = 0; j < 5; ++j) {
        double s = 0; for (std::size_t k = 0; k < 3; ++k) s += A(i, k) * B(k, j);
        assert(std::abs(R(i, j) - (s + 2.0 * C(i, j))) < 1e-12);
    }
    std::puts("Matrix: A*B + 2*C matches naive triple loop");
}
}  // namespace s7

// =====================================================================================
// 8. constexpr programming
// =====================================================================================
namespace s8 {

// constexpr sin via Taylor series with range reduction. Runs at compile time when given a
// constant; also callable at runtime. (Not <cmath>: std::sin is constexpr only since C++26.)
constexpr double pi = 3.14159265358979323846;
constexpr double csin(double x) {
    // reduce to [-pi, pi]
    while (x >  pi) x -= 2 * pi;
    while (x < -pi) x += 2 * pi;
    double term = x, sum = x;
    for (int k = 1; k < 12; ++k) {
        term *= -x * x / ((2 * k) * (2 * k + 1));
        sum += term;
    }
    return sum;
}

// A lookup table filled by a constexpr loop. std::array is a literal type; this lives in .rodata.
template <std::size_t N>
constexpr std::array<double, N> make_sin_table() {
    std::array<double, N> t{};
    for (std::size_t i = 0; i < N; ++i) t[i] = csin(2 * pi * double(i) / double(N));
    return t;
}
constexpr auto SIN256 = make_sin_table<256>();

// Bit table: popcount of every byte. std::popcount exists in <bit>, this shows the technique.
constexpr std::array<std::uint8_t, 256> make_popcount_table() {
    std::array<std::uint8_t, 256> t{};
    for (unsigned i = 0; i < 256; ++i) { unsigned v = i, c = 0; while (v) { c += v & 1u; v >>= 1; } t[i] = std::uint8_t(c); }
    return t;
}
constexpr auto POPCNT8 = make_popcount_table();
constexpr int popcount32(std::uint32_t v) {
    return POPCNT8[v & 0xFF] + POPCNT8[(v >> 8) & 0xFF] + POPCNT8[(v >> 16) & 0xFF] + POPCNT8[v >> 24];
}

// std::vector in constexpr (C++20): allowed as long as it is destroyed before the evaluation ends.
constexpr double mean_of_squares(int n) {
    std::vector<double> v; for (int i = 1; i <= n; ++i) v.push_back(double(i) * i);
    return std::accumulate(v.begin(), v.end(), 0.0) / n;
}

// consteval: MUST run at compile time. Calling it with a runtime value is a compile error.
consteval std::uint64_t pow2(int k) { return std::uint64_t{1} << k; }

// constinit: guarantees static initialization (no "static initialization order fiasco"), but the
// variable stays mutable at runtime.
constinit int g_steps = 1 << 10;

void run() {
    std::puts("--- 8. constexpr");
    static_assert(SIN256.size() == 256);
    static_assert(SIN256[64] > 0.999999 && SIN256[64] < 1.000001);   // sin(pi/2)
    static_assert(SIN256[0] == 0.0);
    static_assert(popcount32(0xF0F0F0F0u) == 16);
    static_assert(mean_of_squares(3) == (1 + 4 + 9) / 3.0);
    static_assert(pow2(10) == 1024);
    for (int i = 0; i < 256; ++i) assert(std::abs(SIN256[i] - std::sin(2 * pi * i / 256.0)) < 1e-12);
    g_steps *= 2;
    std::printf("sin table max error vs std::sin < 1e-12; popcount table ok; g_steps=%d\n", g_steps);
}
}  // namespace s8

// =====================================================================================
// 9. integral_constant, class-type NTTPs, template template params, policy-based design
// =====================================================================================
namespace s9 {

// integral_constant: a value with a type. std::true_type is integral_constant<bool,true>.
using Two = std::integral_constant<int, 2>;
static_assert(Two::value == 2 && Two{} == 2);                // implicit conversion to int

// Class-type non-type template parameter (C++20): needs a structural type (public members,
// no user-provided operator==... actually == is defaulted implicitly for NTTP equivalence).
struct Shape { std::size_t rows, cols; };
template <Shape S>
struct StaticMatrix {
    std::array<double, S.rows * S.cols> d{};
    static constexpr std::size_t rows() { return S.rows; }
    static constexpr std::size_t cols() { return S.cols; }
    double& operator()(std::size_t i, std::size_t j) { return d[i * S.cols + j]; }
};
template <Shape A, Shape B>
    requires (A.cols == B.rows)
StaticMatrix<Shape{A.rows, B.cols}> matmul(const StaticMatrix<A>& a, const StaticMatrix<B>& b) {
    StaticMatrix<Shape{A.rows, B.cols}> c;
    for (std::size_t i = 0; i < A.rows; ++i)
        for (std::size_t k = 0; k < A.cols; ++k)
            for (std::size_t j = 0; j < B.cols; ++j) c(i, j) += a.d[i * A.cols + k] * b.d[k * B.cols + j];
    return c;
}

// Template template parameter: accept "a container template", not a container type.
template <class T, template <class...> class Container = std::vector>
struct Buffer { Container<T> items; void add(T x) { items.push_back(std::move(x)); } };

// Policy-based design: Matrix<T, Storage> where the storage policy decides heap vs stack.
struct HeapStorage {
    template <class T> struct type {
        std::vector<T> d;
        type(std::size_t n) : d(n) {}
        T* data() { return d.data(); } const T* data() const { return d.data(); }
        static constexpr const char* name() { return "heap"; }
    };
};
template <std::size_t N>
struct StackStorage {
    template <class T> struct type {
        std::array<T, N> d{};
        type(std::size_t n) { assert(n <= N); (void)n; }
        T* data() { return d.data(); } const T* data() const { return d.data(); }
        static constexpr const char* name() { return "stack"; }
    };
};
template <class T, class StoragePolicy = HeapStorage>
class PMatrix {
    std::size_t r_, c_;
    typename StoragePolicy::template type<T> s_;
public:
    PMatrix(std::size_t r, std::size_t c) : r_(r), c_(c), s_(r * c) {}
    T& operator()(std::size_t i, std::size_t j) { return s_.data()[i * c_ + j]; }
    const T& operator()(std::size_t i, std::size_t j) const { return s_.data()[i * c_ + j]; }
    static constexpr const char* storage() { return StoragePolicy::template type<T>::name(); }
    std::size_t rows() const { return r_; } std::size_t cols() const { return c_; }
};

void run() {
    std::puts("--- 9. NTTPs, template template params, policies");
    StaticMatrix<Shape{2, 3}> a; StaticMatrix<Shape{3, 2}> b;
    for (std::size_t i = 0; i < 2; ++i) for (std::size_t j = 0; j < 3; ++j) { a(i, j) = 1.0; b(j, i) = 2.0; }
    auto c = matmul(a, b);                                     // type: StaticMatrix<Shape{2,2}>, checked at compile time
    static_assert(decltype(c)::rows() == 2 && decltype(c)::cols() == 2);
    assert(c(0, 0) == 6.0);
    // matmul(a, a) -> compile error: constraint A.cols == B.rows not satisfied.

    Buffer<int> heap_buf; heap_buf.add(1);
    Buffer<int, std::vector> same; same.add(2);
    assert(heap_buf.items.size() == 1 && same.items.size() == 1);

    PMatrix<double> hm(3, 3);
    PMatrix<double, StackStorage<16>> sm(3, 3);
    hm(1, 1) = 5; sm(1, 1) = 6;
    static_assert(sizeof(sm) >= 16 * sizeof(double));         // the data is inline
    std::printf("PMatrix<double>: %s storage, PMatrix<double, StackStorage<16>>: %s storage (%zu bytes)\n",
                hm.storage(), sm.storage(), sizeof(sm));
    assert(hm(1, 1) == 5 && sm(1, 1) == 6);
}
}  // namespace s9

// =====================================================================================
// 10. decltype(auto), auto return deduction, std::invoke, std::apply, tuple metaprogramming
// =====================================================================================
namespace s10 {

std::vector<double> g{1, 2, 3};
auto           by_value(std::size_t i) { return g[i]; }        // auto drops the reference: returns double
decltype(auto) by_ref(std::size_t i)   { return (g[i]); }      // decltype(auto) keeps it: returns double&
// Note the parentheses: decltype((g[i])) is double&, decltype(g[i]) would also be double& here because
// operator[] returns a reference; the parentheses matter when returning a plain variable name.

// Perfect-forwarding wrapper: decltype(auto) forwards whatever f returns (value, ref, void).
template <class F, class... A>
decltype(auto) call_logged(F&& f, A&&... a) {
    return std::invoke(std::forward<F>(f), std::forward<A>(a)...);
}

struct Layer { double w = 2.0; double forward(double x) const { return w * x; } };

// Tuple metaprogramming: a "type list" and operations over it.
template <class... Ts> struct type_list { static constexpr std::size_t size = sizeof...(Ts); };
template <class L> struct largest;
template <class T> struct largest<type_list<T>> { using type = T; };
template <class T, class U, class... R> struct largest<type_list<T, U, R...>> {
    using type = typename largest<type_list<std::conditional_t<(sizeof(T) >= sizeof(U)), T, U>, R...>>::type;
};
// tuple_cat/tuple_element/tuple_size are the standard tools; index_of is a common home-made one.
template <class T, class Tuple> struct index_of;
template <class T, class... R> struct index_of<T, std::tuple<T, R...>> : std::integral_constant<std::size_t, 0> {};
template <class T, class U, class... R> struct index_of<T, std::tuple<U, R...>>
    : std::integral_constant<std::size_t, 1 + index_of<T, std::tuple<R...>>::value> {};

void run() {
    std::puts("--- 10. decltype(auto), invoke, apply, tuples");
    static_assert(std::is_same_v<decltype(by_value(0)), double>);
    static_assert(std::is_same_v<decltype(by_ref(0)), double&>);
    by_ref(0) = 10; assert(g[0] == 10);

    Layer L;
    assert(call_logged(&Layer::forward, L, 3.0) == 6.0);         // invoke: member function pointer + object
    assert(call_logged(&Layer::w, L) == 2.0);                    // invoke: data member pointer
    assert(call_logged([](int a, int b) { return a + b; }, 1, 2) == 3);
    decltype(auto) ref = call_logged(by_ref, 1);                 // double&
    static_assert(std::is_same_v<decltype(ref), double&>);

    auto args = std::make_tuple(2.0, 3.0, 4.0);
    auto fma_ = [](double a, double b, double c) { return a * b + c; };
    assert(std::apply(fma_, args) == 10.0);                      // unpacks the tuple: fma_(2,3,4)

    static_assert(type_list<int, double, char>::size == 3);
    static_assert(std::is_same_v<largest<type_list<char, double, int>>::type, double>);
    static_assert(index_of<double, std::tuple<int, double, char>>::value == 1);
    static_assert(std::is_same_v<std::tuple_element_t<2, std::tuple<int, double, char>>, char>);
    using Cat = decltype(std::tuple_cat(std::tuple<int>{}, std::tuple<double, char>{}));
    static_assert(std::tuple_size_v<Cat> == 3);
    std::puts("invoke/apply ok; type_list computed at compile time");
}
}  // namespace s10

// =====================================================================================
// 11. Compile-time strings and std::array tricks
// =====================================================================================
namespace s11 {

// A string literal as a template argument (C++20 class-type NTTP): fixed_string.
template <std::size_t N>
struct fixed_string {
    char data[N]{};
    constexpr fixed_string(const char (&s)[N]) { for (std::size_t i = 0; i < N; ++i) data[i] = s[i]; }
    constexpr std::size_t size() const { return N - 1; }
    constexpr std::string_view view() const { return {data, N - 1}; }
};
template <fixed_string Name>
struct Named { static constexpr std::string_view name() { return Name.view(); } };

// FNV-1a hash at compile time -> switch on strings.
constexpr std::uint64_t fnv1a(std::string_view s) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return h;
}
constexpr int opcode(std::string_view name) {
    switch (fnv1a(name)) {
        case fnv1a("relu"):    return 1;
        case fnv1a("sigmoid"): return 2;
        case fnv1a("tanh"):    return 3;
        default:               return 0;
    }
}

// std::array tricks: build, sort, and search at compile time (all constexpr since C++20).
constexpr auto primes_below_50() {
    std::array<int, 15> p{}; std::size_t k = 0;
    for (int n = 2; n < 50; ++n) { bool ok = true; for (int d = 2; d * d <= n; ++d) if (n % d == 0) { ok = false; break; } if (ok) p[k++] = n; }
    return p;
}
constexpr auto PRIMES = primes_below_50();
static_assert(PRIMES.back() == 47);
static_assert(std::is_sorted(PRIMES.begin(), PRIMES.end()));
static_assert(std::binary_search(PRIMES.begin(), PRIMES.end(), 31));

// Concatenating arrays with index sequences.
template <class T, std::size_t A, std::size_t B, std::size_t... I, std::size_t... J>
constexpr std::array<T, A + B> cat_impl(const std::array<T, A>& a, const std::array<T, B>& b,
                                        std::index_sequence<I...>, std::index_sequence<J...>) {
    return {a[I]..., b[J]...};
}
template <class T, std::size_t A, std::size_t B>
constexpr std::array<T, A + B> cat(const std::array<T, A>& a, const std::array<T, B>& b) {
    return cat_impl(a, b, std::make_index_sequence<A>{}, std::make_index_sequence<B>{});
}
constexpr std::array<int, 5> CAT = cat(std::array{1, 2}, std::array{3, 4, 5});
static_assert(CAT[4] == 5);

void run() {
    std::puts("--- 11. compile-time strings and arrays");
    static_assert(Named<"embedding">::name() == "embedding");
    static_assert(opcode("sigmoid") == 2 && opcode("nope") == 0);
    std::string runtime_name = "tanh";
    std::printf("Named<\"embedding\">::name() = %s ; opcode(\"%s\") = %d (same function, runtime)\n",
                std::string(Named<"embedding">::name()).c_str(), runtime_name.c_str(), opcode(runtime_name));
    std::printf("PRIMES computed at compile time: first %d, last %d, count %zu\n", PRIMES.front(), PRIMES.back(), PRIMES.size());
}
}  // namespace s11

int main() {
    s1::run(); s2::run(); s3::run(); s4::run(); s5::run(); s6::run();
    s7::run(); s8::run(); s9::run(); s10::run(); s11::run();

    // ERROR DEMO — uncomment one at a time and compare the diagnostics (lesson §22):
    // s4::half_sfinae(std::string("x"));   // "no matching function ... candidate template ignored: requirement ... was not satisfied" x2
    // s4::half(std::string("x"));          // "no matching function ... because 'std::string' does not satisfy 'integral'"
    std::puts("\nall checks passed");
    return 0;
}
