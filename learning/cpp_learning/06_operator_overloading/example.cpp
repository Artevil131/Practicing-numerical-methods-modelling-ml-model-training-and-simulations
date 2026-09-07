// Chapter 06 — Operator Overloading: a NumPy-feeling Matrix, a physics Vec3, and functors.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo
//
// What this program demonstrates:
//   1. Matrix with the full arithmetic set: += -= *= /= as members, + - * / as free functions
//      implemented via the compound forms, unary -, == / !=, 2D indexing m(i,j) with const and
//      non-const overloads, operator<< and operator>>, explicit operator bool.
//      DESIGN DECISION (documented here, as the lesson demands): Matrix * Matrix is MATRIX
//      MULTIPLICATION. Elementwise product is the named function hadamard(a, b).
//   2. Vec3 for physics: all operators, dot, cross, norm, and a gravity force expression.
//   3. Functors: Sigmoid / LeakyReLU with operator(), passed to a template — the ancestor of lambdas.
//   4. A note on temporaries: how many allocations A*B + C performs (we count them).

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------------------------
// A global allocation counter so we can *see* the temporaries created by an expression.
// (Matrix bumps this in its allocating constructor. Not something you'd ship — a teaching probe.)
// ---------------------------------------------------------------------------------------------
static int g_matrix_allocs = 0;

// ---------------------------------------------------------------------------------------------
// 1. Matrix
// ---------------------------------------------------------------------------------------------
class Matrix {
public:
    // explicit: prevents `Matrix m = 3;` and `f(3)` where f takes const Matrix&.
    explicit Matrix(std::size_t r = 0, std::size_t c = 0, double fill = 0.0)
        : rows_(r), cols_(c), data_(r * c, fill) {
        if (r * c > 0) ++g_matrix_allocs;
    }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    const std::vector<double>& data() const { return data_; }

    // --- 2D indexing: TWO overloads. The const one is what makes `const Matrix&` parameters
    //     readable; the non-const one returns double& so `m(i,j) = 5.0;` writes into storage.
    double& operator()(std::size_t i, std::size_t j) {
        assert(i < rows_ && j < cols_);          // compiled out with -DNDEBUG; UB if violated then
        return data_[i * cols_ + j];
    }
    const double& operator()(std::size_t i, std::size_t j) const {
        assert(i < rows_ && j < cols_);
        return data_[i * cols_ + j];
    }

    // --- compound assignment: members, mutate *this, return *this by reference so calls chain.
    Matrix& operator+=(const Matrix& o) {
        check_same_shape(o, "+=");
        for (std::size_t i = 0; i < data_.size(); ++i) data_[i] += o.data_[i];
        return *this;
    }
    Matrix& operator-=(const Matrix& o) {
        check_same_shape(o, "-=");
        for (std::size_t i = 0; i < data_.size(); ++i) data_[i] -= o.data_[i];
        return *this;
    }
    Matrix& operator*=(double s) {
        for (double& x : data_) x *= s;
        return *this;
    }
    Matrix& operator/=(double s) { return *this *= (1.0 / s); }

    // --- unary minus as a member (no parameters; the operand is *this).
    Matrix operator-() const {
        Matrix r = *this;
        r *= -1.0;
        return r;
    }

    // --- explicit conversion to bool: usable in `if (m)`, NOT in `double d = m;`.
    explicit operator bool() const { return !data_.empty(); }

private:
    std::size_t rows_, cols_;
    std::vector<double> data_;

    void check_same_shape(const Matrix& o, const char* op) const {
        if (rows_ != o.rows_ || cols_ != o.cols_) {
            std::ostringstream msg;
            msg << "shape mismatch in " << op << ": (" << rows_ << 'x' << cols_ << ") vs ("
                << o.rows_ << 'x' << o.cols_ << ')';
            throw std::invalid_argument(msg.str());
        }
    }
};

// --- Binary arithmetic as FREE functions. The left operand is taken BY VALUE: that copy is the
//     result, and when the argument is a temporary (e.g. the result of A*B) the "copy" is a move.
Matrix operator+(Matrix a, const Matrix& b) { a += b; return a; }
Matrix operator-(Matrix a, const Matrix& b) { a -= b; return a; }
Matrix operator*(Matrix m, double s)        { m *= s;  return m; }
Matrix operator*(double s, Matrix m)        { m *= s;  return m; }   // MUST be free: left is double
Matrix operator/(Matrix m, double s)        { m /= s;  return m; }

// --- Matrix * Matrix = matmul (documented design decision). Both by const ref: the result has a
//     different shape, so copying either operand would be useless.
Matrix operator*(const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows()) throw std::invalid_argument("matmul: inner dimensions differ");
    Matrix c(a.rows(), b.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t k = 0; k < a.cols(); ++k) {
            const double aik = a(i, k);
            for (std::size_t j = 0; j < b.cols(); ++j)
                c(i, j) += aik * b(k, j);       // i-k-j: innermost loop walks contiguous memory
        }
    return c;
}

// --- Elementwise product under a NAME, so nobody confuses it with matmul.
Matrix hadamard(const Matrix& a, const Matrix& b) {
    if (a.rows() != b.rows() || a.cols() != b.cols())
        throw std::invalid_argument("hadamard: shape mismatch");
    Matrix c(a.rows(), a.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t j = 0; j < a.cols(); ++j) c(i, j) = a(i, j) * b(i, j);
    return c;
}

Matrix transpose(const Matrix& m) {
    Matrix t(m.cols(), m.rows());
    for (std::size_t i = 0; i < m.rows(); ++i)
        for (std::size_t j = 0; j < m.cols(); ++j) t(j, i) = m(i, j);
    return t;
}

// --- Comparison: free, symmetric. != is ALWAYS !(a == b).
bool operator==(const Matrix& a, const Matrix& b) {
    return a.rows() == b.rows() && a.cols() == b.cols() && a.data() == b.data();
}
bool operator!=(const Matrix& a, const Matrix& b) { return !(a == b); }

// Floating-point "equality" belongs in a named function, like np.allclose.
bool allclose(const Matrix& a, const Matrix& b, double atol = 1e-9) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) return false;
    for (std::size_t i = 0; i < a.data().size(); ++i)
        if (std::fabs(a.data()[i] - b.data()[i]) > atol) return false;
    return true;
}

// --- Stream output: MUST be free (left operand is std::ostream). Use `os`, never std::cout,
//     so the same code works for files and string streams.
std::ostream& operator<<(std::ostream& os, const Matrix& m) {
    for (std::size_t i = 0; i < m.rows(); ++i) {
        os << (i == 0 ? "[[" : " [");
        for (std::size_t j = 0; j < m.cols(); ++j)
            os << std::setw(7) << std::setprecision(4) << m(i, j) << (j + 1 < m.cols() ? "," : "");
        os << (i + 1 < m.rows() ? "],\n" : "]]");
    }
    return os;
}

// --- Stream input: reads "rows cols" then the elements. Resizes the target.
std::istream& operator>>(std::istream& is, Matrix& m) {
    std::size_t r = 0, c = 0;
    if (!(is >> r >> c)) return is;
    m = Matrix(r, c);
    for (std::size_t i = 0; i < r; ++i)
        for (std::size_t j = 0; j < c; ++j) is >> m(i, j);
    return is;
}

// ---------------------------------------------------------------------------------------------
// 2. Vec3 — 24 bytes, no heap, everything inlined. The physics workhorse.
// ---------------------------------------------------------------------------------------------
struct Vec3 {
    double x = 0, y = 0, z = 0;

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s)      { x *= s;   y *= s;   z *= s;   return *this; }
    Vec3& operator/=(double s)      { return *this *= (1.0 / s); }
    Vec3  operator-() const         { return {-x, -y, -z}; }

    double&       operator[](int i)       { return i == 0 ? x : (i == 1 ? y : z); }
    const double& operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
inline Vec3 operator+(Vec3 a, const Vec3& b) { return a += b; }
inline Vec3 operator-(Vec3 a, const Vec3& b) { return a -= b; }
inline Vec3 operator*(Vec3 a, double s)      { return a *= s; }
inline Vec3 operator*(double s, Vec3 a)      { return a *= s; }
inline Vec3 operator/(Vec3 a, double s)      { return a /= s; }
inline bool operator==(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(const Vec3& a, const Vec3& b) { return !(a == b); }

// Named functions, not operators: there is no operator that unambiguously means "cross".
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalized(const Vec3& a) { return a / norm(a); }

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}

// ---------------------------------------------------------------------------------------------
// 3. Functors — objects with operator(). State lives in members; the call is inlined.
// ---------------------------------------------------------------------------------------------
struct Sigmoid {
    double operator()(double x) const { return 1.0 / (1.0 + std::exp(-x)); }
};
struct LeakyReLU {
    double slope;                                   // state carried with the "function"
    double operator()(double x) const { return x > 0 ? x : slope * x; }
};

// F is deduced from the argument type: Sigmoid, LeakyReLU, (later) a lambda. Zero overhead.
template <typename F>
Matrix apply(const Matrix& m, F f) {
    Matrix r(m.rows(), m.cols());
    for (std::size_t i = 0; i < m.rows(); ++i)
        for (std::size_t j = 0; j < m.cols(); ++j) r(i, j) = f(m(i, j));
    return r;
}

// ---------------------------------------------------------------------------------------------
int main() {
    std::cout << std::fixed;

    // ---- Matrix basics -----------------------------------------------------------------------
    Matrix A(2, 3), B(3, 2);
    for (std::size_t i = 0; i < 2; ++i)
        for (std::size_t j = 0; j < 3; ++j) {
            A(i, j) = double(i * 3 + j);          // non-const operator(): returns double&
            B(j, i) = 0.1 * double(j + 1);
        }
    std::cout << "A (2x3):\n" << A << "\n";
    std::cout << "B (3x2):\n" << B << "\n";

    // ---- The NumPy-looking line ----------------------------------------------------------------
    Matrix bias(2, 2, 0.5);
    g_matrix_allocs = 0;
    Matrix C = A * B + bias;                        // operator+(operator*(A,B), bias)
    std::cout << "C = A * B + bias:\n" << C << "\n";
    std::cout << "allocations for `A * B + bias`: " << g_matrix_allocs
              << "  (1 for the matmul result; `+` reuses it because it takes its left operand by value)\n\n";

    // ---- scalar on either side, unary minus, division -----------------------------------------
    std::cout << "2.0 * C:\n" << 2.0 * C << "\n";   // free operator*(double, Matrix)
    std::cout << "-C / 2.0:\n" << -C / 2.0 << "\n";

    // ---- hadamard vs matmul: the documented design decision -----------------------------------
    std::cout << "hadamard(C, C):\n" << hadamard(C, C) << "\n";
    std::cout << "C * C (matmul):\n" << C * C << "\n\n";

    // ---- comparison ------------------------------------------------------------------------------
    Matrix D = C;
    std::cout << "C == D: " << (C == D) << ",  C != D: " << (C != D) << '\n';
    D(0, 0) += 1e-12;
    std::cout << "after D(0,0) += 1e-12:  C == D: " << (C == D)
              << ",  allclose(C, D): " << allclose(C, D) << "\n\n";

    // ---- transpose identity check: (A*B)^T == B^T * A^T -------------------------------------------
    std::cout << "(A*B)^T == B^T*A^T ? " << (allclose(transpose(A * B), transpose(B) * transpose(A)) ? "yes" : "no") << "\n\n";

    // ---- explicit operator bool ---------------------------------------------------------------
    Matrix empty;
    if (!empty) std::cout << "empty matrix is falsy; ";
    if (C) std::cout << "C is truthy\n";
    // double d = C;   // does NOT compile: conversion is explicit. Good.

    // ---- shape mismatch throws, is caught ---------------------------------------------------------
    try {
        Matrix bad = A + B;                            // 2x3 + 3x2
        std::cout << bad;                              // never reached
    } catch (const std::invalid_argument& e) {
        std::cout << "caught: " << e.what() << "\n\n";
    }

    // ---- stream round trip -------------------------------------------------------------------------
    std::ostringstream out;
    out << C.rows() << ' ' << C.cols() << '\n';
    for (std::size_t i = 0; i < C.rows(); ++i) {
        for (std::size_t j = 0; j < C.cols(); ++j) out << std::setprecision(17) << C(i, j) << ' ';
        out << '\n';
    }
    std::istringstream in(out.str());
    Matrix back;
    in >> back;                                        // operator>> resizes and fills
    std::cout << "stream round trip exact? " << (back == C ? "yes" : "no") << "\n\n";

    // ---- functors --------------------------------------------------------------------------------
    Matrix Z(1, 5);
    for (std::size_t j = 0; j < 5; ++j) Z(0, j) = double(j) - 2.0;     // -2 -1 0 1 2
    Sigmoid sig;
    std::cout << "sig(0.0) via functor call = " << sig(0.0) << '\n';
    std::cout << "Z:                 " << Z << '\n';
    std::cout << "apply(Z, Sigmoid): " << apply(Z, Sigmoid{}) << '\n';
    std::cout << "apply(Z, LeakyReLU{0.1}): " << apply(Z, LeakyReLU{0.1}) << "\n\n";

    // ---- Vec3 -----------------------------------------------------------------------------------
    Vec3 ex{1, 0, 0}, ey{0, 1, 0};
    std::cout << std::setprecision(4);
    std::cout << "ex x ey = " << cross(ex, ey) << ",  dot(ex, ey) = " << dot(ex, ey) << '\n';
    Vec3 v{3, 4, 0};
    std::cout << "norm(3,4,0) = " << norm(v) << ",  normalized = " << normalized(v) << '\n';
    std::cout << "v[0], v[1], v[2] = " << v[0] << ' ' << v[1] << ' ' << v[2] << '\n';

    // Gravity between two bodies, written as one expression thanks to the operators.
    const double G = 1.0, m1 = 1.0, m2 = 1e-3, eps2 = 1e-9;
    Vec3 x1{0, 0, 0}, x2{1, 0, 0};
    Vec3 r = x2 - x1;
    double d2 = dot(r, r) + eps2;
    Vec3 f = r * (G * m1 * m2 / (d2 * std::sqrt(d2)));   // force on body 1, points toward body 2
    Vec3 a1 = f / m1, a2 = -f / m2;
    std::cout << "force on 1: " << f << "   a1 = " << a1 << "   a2 = " << a2 << '\n';

    return 0;
}
