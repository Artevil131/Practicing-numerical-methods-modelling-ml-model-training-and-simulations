// Chapter 05 — Templates
//
// Demonstrates: function templates + argument deduction + explicit
// instantiation, class template Matrix<T> (float32 vs float64), member
// function templates (converting constructor, apply), non-type template
// parameter Vec<N> for physics vectors, default template arguments, full
// and partial specialization, typename for dependent names, static_assert
// with type traits, if constexpr, variadic templates with fold expressions,
// and a note on what is (and is not) worth templating.
//
// Everything here would normally live in a header (templates must), and is
// in one file only so the example is self-contained.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>          // std::numeric_limits<T>
#include <string>
#include <type_traits>     // std::is_floating_point_v, std::is_arithmetic_v, std::is_same_v
#include <vector>

// ---------------------------------------------------------------------------
// 1. Function templates. The compiler generates one concrete function per
//    distinct T it sees used. Deduction solves T from the argument types.
// ---------------------------------------------------------------------------
template <typename T>
T max_of(T a, T b) { return a > b ? a : b; }

template <typename T>
T sum(const T *a, int n) {
    T s = T{};                                // T{} is zero for every arithmetic type (not `0`, which is an int)
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

// Two type parameters + decltype: mixing float and double is allowed, and
// the result type is whatever a*b yields (double).
template <typename A, typename B>
auto dot(const std::vector<A> &a, const std::vector<B> &b) -> decltype(a[0] * b[0]) {
    decltype(a[0] * b[0]) s = 0;
    for (std::size_t i = 0; i < a.size(); i++) s += a[i] * b[i];
    return s;
}

// Return type cannot be deduced: callers must write zeros<double>(n).
template <typename T>
std::vector<T> zeros(std::size_t n) { return std::vector<T>(n, T{}); }

// ---------------------------------------------------------------------------
// 2. Full specialization: a per-type "dtype" name, like tensor.dtype.
// ---------------------------------------------------------------------------
template <typename T> struct DType { static constexpr const char *name = "unknown"; };
template <> struct DType<float>        { static constexpr const char *name = "float32"; };
template <> struct DType<double>       { static constexpr const char *name = "float64"; };
template <> struct DType<int>          { static constexpr const char *name = "int32"; };
template <> struct DType<std::int64_t> { static constexpr const char *name = "int64"; };

// ---------------------------------------------------------------------------
// 3. Class template: Matrix<T> over a flat std::vector<T>. Default T = double.
//    static_assert turns a 100-line instantiation error into one readable line.
// ---------------------------------------------------------------------------
template <typename T = double>
class Matrix {
    static_assert(std::is_arithmetic_v<T>, "Matrix<T> requires an arithmetic element type");

public:
    Matrix(int rows, int cols, T fill = T{})
        : rows_(rows), cols_(cols), data_(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), fill) {}

    // Member function template: converting constructor Matrix<double>(Matrix<float>).
    // explicit, so precision changes are always visible at the call site (.to(float64)).
    template <typename U>
    explicit Matrix(const Matrix<U> &other)
        : rows_(other.rows()), cols_(other.cols()),
          data_(static_cast<std::size_t>(other.rows()) * static_cast<std::size_t>(other.cols())) {
        for (int i = 0; i < rows_; i++)
            for (int j = 0; j < cols_; j++) (*this)(i, j) = static_cast<T>(other(i, j));
    }

    // Member function template taking any callable (lambda, function pointer, functor).
    template <typename F>
    Matrix &apply(F f) {
        for (T &x : data_) x = f(x);
        return *this;
    }

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    T &operator()(int i, int j)       { return data_[static_cast<std::size_t>(i) * cols_ + j]; }
    T  operator()(int i, int j) const { return data_[static_cast<std::size_t>(i) * cols_ + j]; }

    T sum() const {
        T s = T{};
        for (T x : data_) s += x;
        return s;
    }
    static constexpr const char *dtype() { return DType<T>::name; }
    static constexpr std::size_t itemsize() { return sizeof(T); }

private:
    int rows_, cols_;
    std::vector<T> data_;                    // Inside the class, `Matrix` alone means Matrix<T>.
};

// Free-function templates over the class template. Both arguments must be the
// SAME Matrix<T>: matmul(Matrix<float>, Matrix<double>) fails deduction, exactly
// like torch refusing to matmul float32 with float64.
template <typename T>
Matrix<T> matmul(const Matrix<T> &a, const Matrix<T> &b) {
    Matrix<T> out(a.rows(), b.cols());
    for (int i = 0; i < a.rows(); i++)
        for (int k = 0; k < a.cols(); k++) {
            T aik = a(i, k);
            for (int j = 0; j < b.cols(); j++) out(i, j) += aik * b(k, j);
        }
    return out;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const Matrix<T> &m) {
    for (int i = 0; i < m.rows(); i++) {
        os << "  [";
        for (int j = 0; j < m.cols(); j++) os << std::setw(12) << m(i, j);
        os << " ]\n";
    }
    return os;
}

// ---------------------------------------------------------------------------
// 4. Partial specialization: an alternative recipe for a FAMILY of types.
//    Describe<T*>, Describe<std::vector<T>>, Describe<Matrix<T>> recurse on T.
// ---------------------------------------------------------------------------
template <typename T> struct Describe { static std::string get() { return DType<T>::name; } };
template <typename T> struct Describe<T *>            { static std::string get() { return "pointer to " + Describe<T>::get(); } };
template <typename T> struct Describe<std::vector<T>> { static std::string get() { return "vector of " + Describe<T>::get(); } };
template <typename T> struct Describe<Matrix<T>>      { static std::string get() { return "Matrix of " + Describe<T>::get(); } };

// ---------------------------------------------------------------------------
// 5. Non-type template parameter: fixed-size vector for physics. N is a
//    compile-time constant, so the loops unroll and sizeof(Vec<3>) == 24.
//    Vec<2> and Vec<3> are unrelated types: you cannot add them by accident.
// ---------------------------------------------------------------------------
template <int N>
struct Vec {
    static_assert(N >= 1, "Vec<N> needs at least one component");
    double v[N];

    double &operator[](int i)       { return v[i]; }
    double  operator[](int i) const { return v[i]; }

    Vec operator+(const Vec &o) const { Vec r{}; for (int i = 0; i < N; i++) r.v[i] = v[i] + o.v[i]; return r; }
    Vec operator-(const Vec &o) const { Vec r{}; for (int i = 0; i < N; i++) r.v[i] = v[i] - o.v[i]; return r; }
    Vec operator*(double s)     const { Vec r{}; for (int i = 0; i < N; i++) r.v[i] = v[i] * s; return r; }
    double dot(const Vec &o) const { double s = 0.0; for (int i = 0; i < N; i++) s += v[i] * o.v[i]; return s; }
    double norm() const { return std::sqrt(dot(*this)); }
};

// cross product exists only for N == 3: a plain function, not a template.
Vec<3> cross(const Vec<3> &a, const Vec<3> &b) {
    return Vec<3>{{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

template <int N>
std::ostream &operator<<(std::ostream &os, const Vec<N> &a) {
    os << '(';
    for (int i = 0; i < N; i++) os << (i ? ", " : "") << a[i];
    return os << ')';
}

// ---------------------------------------------------------------------------
// 6. if constexpr: compile-time branching on properties of T. The untaken
//    branch is NOT instantiated, so std::fabs is never applied to an int.
// ---------------------------------------------------------------------------
template <typename T>
bool nearly_equal(T a, T b) {
    static_assert(std::is_arithmetic_v<T>, "nearly_equal needs a number");
    if constexpr (std::is_floating_point_v<T>) {
        T scale = std::max(std::fabs(a), std::fabs(b));
        return std::fabs(a - b) <= 4 * std::numeric_limits<T>::epsilon() * scale;
    } else {
        return a == b;                         // exact for integers
    }
}

template <typename T>
int significant_digits() {
    if constexpr (std::is_floating_point_v<T>) return std::numeric_limits<T>::max_digits10;   // 9 for float, 17 for double
    else return std::numeric_limits<T>::digits10 + 1;
}

// ---------------------------------------------------------------------------
// 7. typename for dependent names. Container::value_type depends on the
//    template parameter, so the compiler must be told it names a type.
// ---------------------------------------------------------------------------
template <typename Container>
typename Container::value_type mean(const Container &c) {
    typename Container::value_type s{};
    for (const auto &x : c) s += x;            // `auto` sidesteps `typename Container::const_iterator`
    return s / static_cast<typename Container::value_type>(c.size());
}

// ---------------------------------------------------------------------------
// 8. Variadic template + fold expression: a print helper for training logs.
// ---------------------------------------------------------------------------
template <typename... Args>
void log_row(const Args &...args) {
    static_assert(sizeof...(Args) > 0, "log_row needs at least one argument");
    std::size_t i = 0;
    ((std::cout << (i++ ? " | " : "") << args), ...);   // fold over the comma operator
    std::cout << '\n';
}

template <typename... Ts>
auto sum_all(Ts... xs) { return (xs + ...); }           // fold over +

// ---------------------------------------------------------------------------
// 9. Default template argument for a comparator (how std::sort is declared).
// ---------------------------------------------------------------------------
template <typename T, typename Cmp = std::less<T>>
T extreme(const std::vector<T> &v, Cmp cmp = Cmp{}) {
    T best = v[0];
    for (const T &x : v) if (cmp(best, x)) best = x;     // with std::less: keeps the max
    return best;
}

int main() {
    std::cout << std::boolalpha;

    // ---- function templates & deduction ---------------------------------------
    std::cout << "== function templates ==\n";
    std::cout << "max_of(3, 7) = " << max_of(3, 7) << "   max_of(2.5, 1.0) = " << max_of(2.5, 1.0) << '\n';
    // max_of(3, 7.5);                          // ERROR: deduced conflicting types for 'T' (int vs double)
    std::cout << "max_of<double>(3, 7.5) = " << max_of<double>(3, 7.5) << "   (explicit T, 3 converts)\n";
    double d[] = {1.5, 2.5, 3.0};
    float  f[] = {1.0f, 2.0f};
    int    k[] = {1, 2, 3, 4};
    std::cout << "sum(double[]) = " << sum(d, 3) << "  sum(float[]) = " << sum(f, 2) << "  sum(int[]) = " << sum(k, 4) << '\n';
    std::vector<float>  vf{1.0f, 2.0f, 3.0f};
    std::vector<double> vd{0.5, 0.25, 0.125};
    auto mixed = dot(vf, vd);
    std::cout << "dot(vector<float>, vector<double>) = " << mixed << "  result is " << DType<decltype(mixed)>::name << '\n';
    auto z = zeros<float>(4);                   // no argument to deduce from -> explicit
    std::cout << "zeros<float>(4).size() = " << z.size() << ", element type " << DType<decltype(z)::value_type>::name << '\n';

    // ---- Matrix<T>: float32 vs float64 ------------------------------------------
    std::cout << "\n== Matrix<T> ==\n";
    Matrix<float>  wf(3, 3, 0.1f);
    Matrix<double> wd(3, 3, 0.1);
    Matrix<>       wdefault(2, 2);              // T defaults to double
    std::cout << "Matrix<float>:  dtype=" << Matrix<float>::dtype() << " itemsize=" << Matrix<float>::itemsize() << '\n';
    std::cout << "Matrix<double>: dtype=" << Matrix<double>::dtype() << " itemsize=" << Matrix<double>::itemsize() << '\n';
    std::cout << "Matrix<>:       dtype=" << wdefault.dtype() << " (default template argument)\n";
    std::cout << std::setprecision(17);
    std::cout << "wf.sum() = " << wf.sum() << "   wd.sum() = " << wd.sum() << "   (0.1 is not exact in either)\n";
    Matrix<double> promoted(wf);                // converting constructor: U = float deduced, T = double
    std::cout << "Matrix<double>(wf)(0,0) = " << promoted(0, 0) << "  <- float32's nearest value to 0.1\n";
    // Matrix<double> implicit = wf;            // ERROR: converting constructor is explicit
    // matmul(wf, wd);                          // ERROR: deduction conflict Matrix<float> vs Matrix<double>
    Matrix<double> prod = matmul(promoted, wd);
    std::cout << std::setprecision(6) << "matmul(Matrix<double>(wf), wd):\n" << prod;
    wd.apply([](double x) { return std::tanh(x); });                  // lambda
    std::cout << "after apply(tanh): wd(0,0) = " << wd(0, 0) << '\n';
    Matrix<int> labels(2, 2, 7);
    std::cout << "Matrix<int> works too: sum = " << labels.sum() << '\n';
    // Matrix<std::string> nope(2, 2);          // ERROR: static assertion failed: Matrix<T> requires an arithmetic element type

    // ---- specialization -----------------------------------------------------------
    std::cout << "\n== full / partial specialization ==\n";
    std::cout << "DType<float>=" << DType<float>::name << "  DType<int64_t>=" << DType<std::int64_t>::name
              << "  DType<char>=" << DType<char>::name << " (primary template)\n";
    std::cout << "Describe<double>                      = " << Describe<double>::get() << '\n';
    std::cout << "Describe<float*>                      = " << Describe<float *>::get() << '\n';
    std::cout << "Describe<std::vector<int>>            = " << Describe<std::vector<int>>::get() << '\n';
    std::cout << "Describe<std::vector<Matrix<float>>>  = " << Describe<std::vector<Matrix<float>>>::get() << '\n';

    // ---- Vec<N> ---------------------------------------------------------------------
    std::cout << "\n== Vec<N> (non-type template parameter) ==\n";
    Vec<3> r{{1.0, 0.0, 0.0}}, p{{0.0, 2.0, 0.0}};
    Vec<2> q{{3.0, 4.0}};
    std::cout << "sizeof(Vec<3>) = " << sizeof(Vec<3>) << ", sizeof(Vec<2>) = " << sizeof(Vec<2>) << " (no overhead)\n";
    std::cout << "r + p*0.5 = " << r + p * 0.5 << "   |q| = " << q.norm() << "   r x p = " << cross(r, p) << '\n';
    // r + q;                                   // ERROR: Vec<3> and Vec<2> are unrelated types
    // cross(q, q);                             // ERROR: no cross for Vec<2>
    constexpr int kDim = 3;
    Vec<kDim> ok{};                              // constant expression: fine
    // int n = 3; Vec<n> bad;                    // ERROR: n is not a constant expression
    std::cout << "Vec<kDim>{} = " << ok << '\n';

    // ---- if constexpr + type traits -------------------------------------------------
    std::cout << "\n== if constexpr / type traits ==\n";
    std::cout << "nearly_equal(0.1f+0.2f, 0.3f) = " << nearly_equal(0.1f + 0.2f, 0.3f)
              << "   exact (0.1f+0.2f == 0.3f) = " << (0.1f + 0.2f == 0.3f) << '\n';
    std::cout << "nearly_equal(0.1+0.2, 0.3)   = " << nearly_equal(0.1 + 0.2, 0.3)
              << "   exact (0.1+0.2 == 0.3)   = " << (0.1 + 0.2 == 0.3) << '\n';
    std::cout << "nearly_equal(3, 3) = " << nearly_equal(3, 3) << "  (integer branch: exact ==)\n";
    std::cout << "significant_digits<float>() = " << significant_digits<float>()
              << ", <double>() = " << significant_digits<double>() << ", <int>() = " << significant_digits<int>() << '\n';
    std::cout << "epsilon<float> = " << std::numeric_limits<float>::epsilon()
              << ", epsilon<double> = " << std::numeric_limits<double>::epsilon() << "  (replaces FLT_EPSILON/DBL_EPSILON)\n";
    std::cout << "is_same_v<Matrix<>, Matrix<double>> = " << std::is_same_v<Matrix<>, Matrix<double>> << '\n';

    // ---- typename / dependent names -----------------------------------------------
    std::cout << "\n== dependent names ==\n";
    std::cout << "mean(vector<double>) = " << mean(vd) << "   mean(array<float,3>) = " << mean(std::array<float, 3>{1.0f, 2.0f, 6.0f}) << '\n';

    // ---- variadic templates ---------------------------------------------------------
    std::cout << "\n== variadic templates ==\n";
    log_row("epoch", "loss", "lr", "dtype");
    log_row(3, 0.125, 1e-3, Matrix<float>::dtype());
    std::cout << "sum_all(1, 2.5, 3.0f) = " << sum_all(1, 2.5, 3.0f) << " (" << DType<decltype(sum_all(1, 2.5, 3.0f))>::name << ")\n";

    // ---- default template argument (comparator) --------------------------------------
    std::cout << "\n== default template args ==\n";
    std::vector<double> losses{0.9, 0.4, 0.7, 0.2};
    std::cout << "extreme(losses) = " << extreme(losses) << " (std::less -> max)   "
              << "extreme(losses, std::greater<double>{}) = " << extreme(losses, std::greater<double>{}) << " (min)\n";

    std::cout << "\nWhen NOT to template: if every matrix in your solver is double, `class Matrix` with\n"
                 "double is simpler and compiles faster. Add <typename T> when float is actually needed.\n";
    return 0;
}
