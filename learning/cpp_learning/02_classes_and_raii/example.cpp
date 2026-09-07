// Chapter 02 — Classes and RAII
//
// A Matrix class that owns a heap buffer, showing: struct vs class, member
// functions and `this`, constructors with initializer lists, destructor,
// RAII and scope-based lifetime, the shallow-copy double-free bug and the
// Rule of Three, copy-and-swap, = default / = delete, explicit, const
// member functions, static members, friend, access control, and aggregates.
//
// This version uses raw new[]/delete[] ON PURPOSE so you see what
// std::vector will do for you in chapter 04. The real Matrix you use later
// will hold a std::vector<double> and need none of the special members.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo
// Also try:
//   c++ -Wall -Wextra -std=c++17 -g -fsanitize=address -o ex_demo example.cpp && ./ex_demo && rm ex_demo

#include <algorithm>   // std::copy, std::fill
#include <cmath>       // std::sqrt
#include <cstddef>     // std::size_t
#include <cstdio>      // std::fopen, std::fclose, std::fprintf, std::remove
#include <iomanip>
#include <iostream>
#include <utility>     // std::swap

// ---------------------------------------------------------------------------
// 1. A plain aggregate: no invariant, so public fields and no constructor.
//    Member functions are fine on an aggregate. sizeof == 3 doubles, memcpy-able.
// ---------------------------------------------------------------------------
struct Vec3 {
    double x, y, z;

    double norm() const { return std::sqrt(x * x + y * y + z * z); }     // const: does not modify *this
    Vec3 scaled(double s) const { return Vec3{x * s, y * s, z * s}; }
    void normalize() {                                                   // not const: modifies *this
        double n = norm();
        x /= n; y /= n; z /= n;                                          // same as this->x /= n
    }
};

// ---------------------------------------------------------------------------
// 2. A class with an invariant: data_ always points to rows_*cols_ doubles
//    (or is nullptr with rows_ == cols_ == 0). Only member functions can
//    change those three fields, so the invariant can be audited here.
// ---------------------------------------------------------------------------
class Matrix {
public:
    // Default constructor: an empty, valid matrix, relying on the default
    // member initializers below (rows_ = 0, cols_ = 0, data_ = nullptr).
    // Without the live_ counter this would simply be `Matrix() = default;`.
    // Because EVERY constructor creates an object the destructor will later
    // count down, every constructor — this one included — must count up.
    Matrix() { ++live_; }

    // explicit: forbids `Matrix m = 3;` and forbids passing an int where a
    // Matrix is expected. Callers must write Matrix(3, 3) / Matrix{3, 3}.
    // The initializer list runs in DECLARATION order (rows_, cols_, data_),
    // which is why data_'s initializer may safely read rows_ and cols_.
    explicit Matrix(int rows, int cols, double fill = 0.0)
        : rows_(rows), cols_(cols),
          data_(new double[static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols)]) {
        std::fill(data_, data_ + rows_ * cols_, fill);
        ++live_;
    }

    // Copy constructor: build a NEW matrix with its OWN buffer.
    // Without this, the compiler-generated copy would copy the pointer and
    // two objects would delete[] the same buffer -> double free.
    Matrix(const Matrix &other)
        : rows_(other.rows_), cols_(other.cols_),
          data_(new double[static_cast<std::size_t>(other.rows_) * static_cast<std::size_t>(other.cols_)]) {
        std::copy(other.data_, other.data_ + rows_ * cols_, data_);
        ++live_;
    }

    // Copy assignment via copy-and-swap. The parameter is BY VALUE, so the
    // copy constructor has already made an independent copy in `other`.
    // Swapping gives *this the copy and gives `other` our old buffer, which
    // is released when `other` is destroyed at the closing brace.
    // Self-assignment safe with no check; exception safe (if the copy
    // throws, *this is untouched).
    Matrix &operator=(Matrix other) {
        swap(*this, other);
        return *this;               // allows a = b = c
    }

    // Destructor: RAII release. delete[] nullptr is a no-op, so the empty
    // matrix from the default constructor is fine.
    ~Matrix() {
        delete[] data_;
        --live_;
    }

    // friend: a non-member function granted access to private fields.
    // noexcept because swapping ints and pointers cannot fail.
    friend void swap(Matrix &a, Matrix &b) noexcept {
        std::swap(a.rows_, b.rows_);
        std::swap(a.cols_, b.cols_);
        std::swap(a.data_, b.data_);
    }

    // const member functions: callable on const Matrix / const Matrix&.
    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // The const/non-const accessor pair. The compiler picks the const one
    // for const objects (read), the other for non-const (read or write).
    double  at(int i, int j) const { return data_[i * cols_ + j]; }
    double &at(int i, int j)       { return data_[i * cols_ + j]; }

    double trace() const {
        double t = 0.0;
        for (int i = 0; i < std::min(rows_, cols_); i++) t += at(i, i);
        return t;
    }

    // static member functions: no `this`, called as Matrix::identity(3).
    // Idiomatic home for factories (np.zeros / torch.eye).
    static Matrix identity(int n) {
        Matrix m(n, n, 0.0);
        for (int i = 0; i < n; i++) m.at(i, i) = 1.0;
        return m;                    // returned by value; copy usually elided
    }
    static int live() { return live_; }

    // friend operator<<: must be a non-member (left operand is the stream),
    // needs private access to iterate the buffer directly.
    friend std::ostream &operator<<(std::ostream &os, const Matrix &m);

private:
    int rows_ = 0;                   // default member initializers: every constructor
    int cols_ = 0;                   // starts from a known state
    double *data_ = nullptr;
    static inline int live_ = 0;     // C++17: one shared counter, defined in-class
};

std::ostream &operator<<(std::ostream &os, const Matrix &m) {
    for (int i = 0; i < m.rows_; i++) {
        os << "  [";
        for (int j = 0; j < m.cols_; j++) os << std::setw(6) << m.data_[i * m.cols_ + j];
        os << " ]\n";
    }
    return os;
}

// ---------------------------------------------------------------------------
// 3. A non-copyable RAII wrapper around a C FILE*. Copying would mean two
//    objects fclose() the same handle, so copying is deleted outright:
//    the bug becomes a compile error instead of a runtime double close.
// ---------------------------------------------------------------------------
class File {
public:
    explicit File(const char *path, const char *mode) : fp_(std::fopen(path, mode)) {}
    ~File() { if (fp_) std::fclose(fp_); }          // fclose(nullptr) is UB: guard it

    File(const File &) = delete;                    // no copies
    File &operator=(const File &) = delete;

    bool ok() const { return fp_ != nullptr; }
    FILE *get() const { return fp_; }

private:
    FILE *fp_;
};

// ---------------------------------------------------------------------------
// 4. RAII replaces `goto cleanup`. Every `return` below destroys a, b, x in
//    reverse order of construction. There is no cleanup code to forget.
// ---------------------------------------------------------------------------
int solve_diagonal(int n, double bad_pivot_at) {
    Matrix a(n, n), b(n, 1), x(n, 1);
    for (int i = 0; i < n; i++) { a.at(i, i) = (i == bad_pivot_at) ? 0.0 : i + 1.0; b.at(i, 0) = 1.0; }
    for (int i = 0; i < n; i++) {
        if (a.at(i, i) == 0.0) {
            std::cout << "    singular pivot at row " << i << " -> early return (live=" << Matrix::live() << " inside)\n";
            return -1;                       // a, b, x destroyed here
        }
        x.at(i, 0) = b.at(i, 0) / a.at(i, i);
    }
    std::cout << "    solved; x(n-1) = " << x.at(n - 1, 0) << '\n';
    return 0;                                // and here
}

// A function taking const Matrix& can only call const members.
double frobenius(const Matrix &m) {
    double s = 0.0;
    for (int i = 0; i < m.rows(); i++)
        for (int j = 0; j < m.cols(); j++) s += m.at(i, j) * m.at(i, j);   // picks the const at()
    // m.at(0, 0) = 1.0;   // ERROR if uncommented: at() const returns double, not double&
    return std::sqrt(s);
}

int main() {
    std::cout << "== aggregate struct ==\n";
    Vec3 v{3.0, 4.0, 0.0};                       // brace init, field by field
    std::cout << "|v| = " << v.norm() << ", sizeof(Vec3) = " << sizeof(Vec3) << '\n';
    Vec3 w = v.scaled(2.0);                      // returns a new Vec3; v unchanged
    v.normalize();                               // mutates v
    std::cout << "w = {" << w.x << "," << w.y << "," << w.z << "}, normalized v = {"
              << v.x << "," << v.y << "," << v.z << "}\n";

    std::cout << "\n== constructors / destructor / RAII ==\n";
    std::cout << "live matrices: " << Matrix::live() << '\n';
    {
        Matrix m(2, 3, 1.5);                     // constructor: new double[6]
        Matrix e;                                // default constructor: empty, no allocation
        std::cout << "inside block, live = " << Matrix::live() << "  (m is 2x3, e is "
                  << e.rows() << "x" << e.cols() << ")\n";
        m.at(1, 2) = 9.0;                        // non-const at() returns double&
        std::cout << m;
    }                                            // ~e then ~m run here, in reverse order
    std::cout << "after block, live = " << Matrix::live() << "  (destructors ran automatically)\n";

    std::cout << "\n== RAII vs goto cleanup ==\n";
    solve_diagonal(4, -1);                       // normal path
    solve_diagonal(4, 2);                        // early-return path; still no leak
    std::cout << "  after both calls, live = " << Matrix::live() << '\n';

    std::cout << "\n== copy constructor (deep copy) ==\n";
    Matrix a(2, 2, 1.0);
    Matrix b = a;                                // copy CONSTRUCTOR: b gets its own buffer
    b.at(0, 0) = 99.0;
    std::cout << "a after modifying b (must still be 1):\n" << a;
    std::cout << "b:\n" << b;
    std::cout << "live = " << Matrix::live() << " (a, b)\n";

    std::cout << "\n== copy assignment (copy-and-swap) ==\n";
    Matrix c(3, 3, 7.0);                         // different shape: assignment must resize
    c = b;                                       // copy ASSIGNMENT: old 3x3 buffer freed by the temp's destructor
    std::cout << "c after c = b (now 2x2):\n" << c;
    Matrix &alias_of_c = c;                      // self-assignment usually arrives through an alias
    c = alias_of_c;                              // (a[i] = a[j] with i == j); safe with copy-and-swap
    std::cout << "c after c = c (unchanged):\n" << c;
    a = b = Matrix::identity(2);                 // chained: operator= returns Matrix&
    std::cout << "a after a = b = identity(2):\n" << a;

    std::cout << "\n== static factory + const member functions ==\n";
    const Matrix I = Matrix::identity(3);        // const object: only const members callable
    std::cout << "trace(I3) = " << I.trace() << ", frobenius(I3) = " << frobenius(I) << '\n';
    // I.at(0, 0) = 5.0;   // ERROR if uncommented: I is const, at() const returns by value
    std::cout << "I.at(1,1) via const overload = " << I.at(1, 1) << '\n';

    std::cout << "\n== explicit ==\n";
    Matrix sq(2, 2, 0.5);                        // OK: direct initialization
    // Matrix bad = 2;                           // ERROR if uncommented: explicit forbids implicit conversion
    // frobenius(2);                             // ERROR if uncommented: same reason
    std::cout << "frobenius(sq) = " << frobenius(sq) << '\n';

    std::cout << "\n== non-copyable RAII File ==\n";
    const char *path = "ex_demo_tmp.txt";
    {
        File f(path, "w");                       // fopen in constructor
        if (f.ok()) {
            for (int i = 0; i < 5; i++) std::fprintf(f.get(), "%d,%d\n", i, i * i);
            std::cout << "wrote 5 lines to " << path << '\n';
        }
        // File g = f;                           // ERROR if uncommented: copy constructor is deleted
    }                                            // fclose in destructor, even if we had returned early
    {
        File f(path, "r");
        int i, sq_i, sum = 0;
        while (f.ok() && std::fscanf(f.get(), "%d,%d", &i, &sq_i) == 2) sum += sq_i;
        std::cout << "read back, sum of squares = " << sum << '\n';
    }
    std::remove(path);

    std::cout << "\n== end of main ==\n";
    std::cout << "live before main's locals die = " << Matrix::live() << " (a, b, c, I, sq)\n";
    return 0;                                    // sq, I, c, b, a destroyed here, in that order
}
