// Chapter 03 — References, const, and Value Semantics
//
// Demonstrates: lvalue references (bind once, never null, never reseated),
// const& parameters, accessor pairs returning references, why returning a
// reference to a local dangles, value semantics (copy) vs reference
// semantics (alias) with the NumPy view analogy, copy-counting to show
// copy elision / RVO, const propagation and `mutable`, the range-for
// accidental-copy trap, std::ref / std::cref, decltype, temporary lifetime,
// and reference_wrapper / pointers in containers.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo

#include <algorithm>    // std::copy, std::fill
#include <cmath>        // std::sqrt
#include <cstddef>      // std::size_t
#include <functional>   // std::reference_wrapper, std::ref, std::cref
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>      // std::swap
#include <vector>

// ---------------------------------------------------------------------------
// A Matrix that COUNTS its copies so we can see what copies and what doesn't.
// (Same raw-buffer design as chapter 02, Rule of Three, copy-and-swap.)
// ---------------------------------------------------------------------------
class Matrix {
public:
    explicit Matrix(int rows, int cols, double fill = 0.0)
        : rows_(rows), cols_(cols),
          data_(new double[static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols)]) {
        std::fill(data_, data_ + rows_ * cols_, fill);
    }
    Matrix(const Matrix &o)
        : rows_(o.rows_), cols_(o.cols_),
          data_(new double[static_cast<std::size_t>(o.rows_) * static_cast<std::size_t>(o.cols_)]) {
        std::copy(o.data_, o.data_ + rows_ * cols_, data_);
        ++copies;                                   // <-- count every deep copy
    }
    Matrix &operator=(Matrix o) { swap(*this, o); return *this; }   // by-value param => copy ctor ran (counted there)
    ~Matrix() { delete[] data_; }

    friend void swap(Matrix &a, Matrix &b) noexcept {
        std::swap(a.rows_, b.rows_); std::swap(a.cols_, b.cols_); std::swap(a.data_, b.data_);
        a.norm_valid_ = b.norm_valid_ = false;      // cached norms no longer describe these objects
    }

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // Accessor pair. Non-const returns double& (write access) and must
    // conservatively invalidate the cache: it cannot know if the caller writes.
    // The const overload is written once with the check; the non-const one
    // forwards to it — the one legitimate const_cast idiom.
    const double &at(int i, int j) const {
        if (i < 0 || i >= rows_ || j < 0 || j >= cols_) { std::cerr << "at(): out of range\n"; std::abort(); }
        return data_[i * cols_ + j];
    }
    double &at(int i, int j) {
        norm_valid_ = false;
        return const_cast<double &>(static_cast<const Matrix &>(*this).at(i, j));
    }

    // const member that legitimately writes: a cache. `mutable` members may
    // change inside const functions because they are not observable state.
    double frobenius() const {
        if (!norm_valid_) {
            double s = 0.0;
            for (int k = 0; k < rows_ * cols_; k++) s += data_[k] * data_[k];
            norm_cache_ = std::sqrt(s);
            norm_valid_ = true;
            ++norm_computations;
        }
        return norm_cache_;
    }

    static inline int copies = 0;
    static inline int norm_computations = 0;

private:
    int rows_, cols_;
    double *data_;
    mutable double norm_cache_ = 0.0;
    mutable bool   norm_valid_ = false;
};

std::ostream &operator<<(std::ostream &os, const Matrix &m) {
    for (int i = 0; i < m.rows(); i++) {
        os << "  [";
        for (int j = 0; j < m.cols(); j++) os << std::setw(5) << m.at(i, j);   // const at()
        os << " ]\n";
    }
    return os;
}

// ---------------------------------------------------------------------------
// Parameter passing conventions.
// ---------------------------------------------------------------------------
double lerp(double a, double b, double t) { return a + t * (b - a); }     // small: by value

double trace(const Matrix &m) {                                            // big, read-only: const&
    double t = 0.0;
    for (int i = 0; i < std::min(m.rows(), m.cols()); i++) t += m.at(i, i);
    return t;
}

void scale_inplace(Matrix &m, double s) {                                  // modify caller's object: T&
    for (int i = 0; i < m.rows(); i++)
        for (int j = 0; j < m.cols(); j++) m.at(i, j) *= s;
}

void by_value(Matrix m) { m.at(0, 0) = -1.0; }                             // copies; caller's object untouched

// Return by value: NRVO (named local) — no copy at -O2.
Matrix identity(int n) {
    Matrix out(n, n, 0.0);
    for (int i = 0; i < n; i++) out.at(i, i) = 1.0;
    return out;
}
// Return by value: prvalue — copy elision GUARANTEED by C++17, even at -O0.
Matrix zeros(int r, int c) { return Matrix(r, c, 0.0); }

// Returning a reference INTO an argument is fine: the caller's object outlives the call.
const Matrix &larger(const Matrix &a, const Matrix &b) {
    return a.rows() * a.cols() >= b.rows() * b.cols() ? a : b;
}

// Returning a reference to a static is fine: it lives until program exit.
const std::string &default_name() {
    static const std::string s = "unnamed";
    return s;
}

// THIS is the dangling-reference bug. It is shown here as a comment because
// clang refuses to be quiet about it (-Wreturn-stack-address), which is good:
//
//   const std::string &make_label(int i) {
//       std::string s = "row_" + std::to_string(i);
//       return s;      // s is destroyed at the closing brace; the caller gets garbage
//   }

// decltype: "the type of this expression". Here the result type of mixing
// float and double is whatever the multiplication yields (double).
template <typename A, typename B>
auto mixed_dot(const std::vector<A> &a, const std::vector<B> &b) -> decltype(a[0] * b[0]) {
    decltype(a[0] * b[0]) s = 0;
    for (std::size_t i = 0; i < a.size(); i++) s += a[i] * b[i];
    return s;
}

// std::ref: pass a reference through something that would otherwise copy.
// A function template taking its argument BY VALUE (like std::thread does):
template <typename F, typename T>
void call_with_copy(F f, T arg) { f(arg); }            // `arg` is a copy of what the caller passed...
void bump(int &x) { ++x; }                              // ...unless the caller wrapped it in std::ref

int main() {
    std::cout << std::boolalpha;

    // ---- 1. references: bind once, alias forever ----------------------------
    std::cout << "== references ==\n";
    int x = 5, y = 10;
    int &r = x;                      // r IS x
    r = 7;
    std::cout << "after r = 7:  x = " << x << '\n';
    r = y;                           // NOT a rebind: writes y's value into x
    std::cout << "after r = y:  x = " << x << ", y = " << y << ", (&r == &x) = " << (&r == &x) << '\n';
    int *p = &x;
    p = &y;                          // pointers DO rebind
    std::cout << "pointer rebound: *p = " << *p << '\n';
    const int &lit = 42;             // const& may bind a temporary; lifetime extended to lit's scope
    std::cout << "const int& bound to literal: " << lit << '\n';
    std::cout << "sizeof(r) = " << sizeof(r) << " (size of int, not of a pointer), sizeof(p) = " << sizeof(p) << '\n';
    // int &bad = 42;                // ERROR: non-const lvalue reference cannot bind to a temporary
    // int &none;                    // ERROR: references must be initialized

    // ---- 2. parameter conventions ------------------------------------------
    std::cout << "\n== parameter passing ==\n";
    Matrix::copies = 0;
    Matrix m(3, 3, 2.0);
    std::cout << "lerp(0, 10, 0.25) = " << lerp(0, 10, 0.25) << "   (small types: by value)\n";
    std::cout << "trace(m) = " << trace(m) << "   copies so far: " << Matrix::copies << "  (const&: none)\n";
    scale_inplace(m, 0.5);
    std::cout << "after scale_inplace(m, 0.5): m(0,0) = " << m.at(0, 0) << ", copies: " << Matrix::copies << "  (T&: none)\n";
    by_value(m);
    std::cout << "after by_value(m): m(0,0) = " << m.at(0, 0) << ", copies: " << Matrix::copies << "  (by value: ONE copy, caller untouched)\n";

    // ---- 3. accessor pairs --------------------------------------------------
    std::cout << "\n== accessor pair ==\n";
    m.at(1, 2) = 9.0;                              // non-const at() -> double&: write
    const Matrix &cm = m;                          // const view of the same object
    std::cout << "cm.at(1,2) = " << cm.at(1, 2) << "  (const overload chosen for const&)\n";
    // cm.at(1, 2) = 0.0;                          // ERROR: const double& is not assignable
    double &cell = m.at(0, 0);                     // reference into m's buffer
    cell = 100.0;
    std::cout << "wrote through returned reference: m(0,0) = " << m.at(0, 0) << '\n';

    // ---- 4. value vs reference semantics (NumPy analogy) --------------------
    std::cout << "\n== value semantics vs aliasing ==\n";
    Matrix A(2, 2, 1.0);
    Matrix B = A;                    // COPY: like NumPy   B = A.copy()
    Matrix &C = A;                   // ALIAS: like NumPy  C = A       (or Python's b = a)
    B.at(0, 0) = 5.0;
    C.at(1, 1) = 7.0;
    std::cout << "A after B(0,0)=5 (copy) and C(1,1)=7 (alias):\n" << A;
    std::cout << "B (independent):\n" << B;
    std::cout << "C++ objects behave like Python ints (values), not like Python lists (references).\n";

    // ---- 5. copy elision / RVO ----------------------------------------------
    std::cout << "\n== return by value: copy elision ==\n";
    Matrix::copies = 0;
    Matrix I = identity(4);                        // NRVO: constructed directly in I
    Matrix Z = zeros(4, 4);                        // guaranteed elision (prvalue)
    std::cout << "identity(4) and zeros(4,4) returned by value -> copies = " << Matrix::copies << '\n';
    std::cout << "trace(I) = " << trace(I) << ", Z.frobenius() = " << Z.frobenius() << '\n';
    const Matrix &big = larger(I, m);              // reference into an argument: fine, I outlives it
    std::cout << "larger(I, m) has " << big.rows() << "x" << big.cols() << ", still copies = " << Matrix::copies << '\n';
    std::cout << "default_name() = \"" << default_name() << "\" (reference to a static: fine)\n";

    // ---- 6. mutable cache ---------------------------------------------------
    std::cout << "\n== mutable cache inside const member ==\n";
    Matrix::norm_computations = 0;
    Matrix W(100, 100, 0.5);
    double n1 = W.frobenius();
    double n2 = W.frobenius();                     // cached: no recomputation
    std::cout << "two frobenius() calls: " << n1 << ", " << n2 << " -> computations = " << Matrix::norm_computations << '\n';
    W.at(0, 0) = 3.0;                              // non-const at() invalidates
    W.frobenius();
    std::cout << "after a write and a third call: computations = " << Matrix::norm_computations << '\n';

    // ---- 7. range-for: the accidental copy trap -----------------------------
    std::cout << "\n== range-for copy trap ==\n";
    std::vector<Matrix> layers;
    layers.reserve(3);
    for (int k = 0; k < 3; k++) layers.emplace_back(2, 2, 1.0 * (k + 1));
    Matrix::copies = 0;
    for (auto layer : layers) layer.at(0, 0) = 0.0;          // COPIES each; mutation lost
    std::cout << "for (auto layer : layers):        copies = " << Matrix::copies
              << ", layers[0](0,0) = " << layers[0].at(0, 0) << "  (unchanged!)\n";
    Matrix::copies = 0;
    for (auto &layer : layers) layer.at(0, 0) = 0.0;         // reference: in place
    std::cout << "for (auto &layer : layers):       copies = " << Matrix::copies
              << ", layers[0](0,0) = " << layers[0].at(0, 0) << "  (modified)\n";
    double total = 0.0;
    for (const auto &layer : layers) total += layer.frobenius();   // read-only, no copy: the default
    std::cout << "for (const auto &layer : layers): copies = " << Matrix::copies << ", sum of norms = " << total << '\n';

    // ---- 8. std::ref / std::cref ---------------------------------------------
    std::cout << "\n== std::ref ==\n";
    int counter = 0;
    call_with_copy(bump, counter);                  // T deduced as int: bump gets a copy
    std::cout << "call_with_copy(bump, counter):           counter = " << counter << '\n';
    call_with_copy(bump, std::ref(counter));        // T = reference_wrapper<int>: converts to int&
    std::cout << "call_with_copy(bump, std::ref(counter)): counter = " << counter << '\n';

    // ---- 9. decltype ----------------------------------------------------------
    std::cout << "\n== decltype ==\n";
    std::vector<float>  f{1.0f, 2.0f, 3.0f};
    std::vector<double> d{0.5, 0.25, 0.125};
    auto md = mixed_dot(f, d);                      // decltype(float * double) = double
    std::cout << "mixed_dot(float vec, double vec) = " << md << ", sizeof(result) = " << sizeof(md) << " (double)\n";

    // ---- 10. temporary lifetime ------------------------------------------------
    std::cout << "\n== temporary lifetime ==\n";
    const std::string &ext = std::to_string(12345); // temporary bound to const&: lifetime extended
    std::cout << "extended temporary: " << ext << '\n';
    std::cout << "used within one full expression: " << std::to_string(6789).c_str() << '\n';   // fine
    // const char *dangling = std::to_string(1).c_str();   // TRAP: string dies at ';' -> dangling (UB if used)
    std::cout << "(see comment: c_str() of a temporary dangles after the semicolon)\n";

    // ---- 11. references in containers ----------------------------------------
    std::cout << "\n== references in containers ==\n";
    // std::vector<Matrix&> nope;                  // ERROR: references are not objects
    std::vector<Matrix *> ptrs{&A, &I, &m};        // non-owning handles; may be null
    std::vector<std::reference_wrapper<Matrix>> refs{A, I, m};   // never null; .get() or implicit Matrix&
    for (Matrix *q : ptrs) q->at(0, 0) += 1.0;
    for (Matrix &q : refs) q.at(0, 0) += 1.0;       // reference_wrapper converts to Matrix&
    std::cout << "after +1 via pointers and +1 via reference_wrappers: A(0,0) = " << A.at(0, 0)
              << ", I(0,0) = " << I.at(0, 0) << ", m(0,0) = " << m.at(0, 0) << '\n';
    std::cout << "the objects must outlive both containers; neither container owns anything.\n";

    return 0;
}
