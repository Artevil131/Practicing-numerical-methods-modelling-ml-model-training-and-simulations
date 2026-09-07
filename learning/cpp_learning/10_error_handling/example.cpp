// Chapter 10 — Error Handling: one program, every technique.
//
// Compile:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp
// Run:      ./ex_demo
//
// Try also:  c++ -Wall -Wextra -std=c++17 -O0 -o ex_demo example.cpp   (asserts stay enabled either way;
//            add -DNDEBUG to see the assert vanish in section 8)
//
// Sections (search for "// ---- N"):
//   1  return code (C style) + errno
//   2  bool + out-param parser
//   3  std::optional lookup
//   4  hand-rolled Expected<T,E> result type
//   5  Matrix with ShapeError (custom exception), at() vs operator()
//   6  stack unwinding + RAII cleanup order
//   7  rethrow, catch(...), nested exceptions
//   8  assert vs exception, noexcept, [[nodiscard]]
//   9  std::system_error from errno
//   main: function-try-block as the outermost boundary

#include <cassert>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ---- 1. Return code (C style). Still needed when talking to C libraries. ------------------
// Convention: 0 = success, negative = -errno. The caller CAN forget to check — that's the flaw.
static int c_style_open_check(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return -errno;
    std::fclose(f);
    return 0;
}

// ---- 2. bool + out-param. Zero cost, but the reader must notice the '&'. --------------------
[[nodiscard]] static bool parse_double(std::string_view s, double& out) {
    double tmp = 0.0;
    const char* b = s.data();
    const char* e = b + s.size();
    auto [ptr, ec] = std::from_chars(b, e, tmp);
    if (ec != std::errc{} || ptr != e) return false;   // garbage, overflow, or trailing junk
    out = tmp;                                          // only touch 'out' on success
    return true;
}

// ---- 3. std::optional: "no value" is a normal outcome, not an error. -----------------------
[[nodiscard]] static std::optional<std::size_t> find_index(const std::vector<int>& v, int target) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i] == target) return i;
    return std::nullopt;
}

// ---- 4. Hand-rolled Expected<T,E> (C++23 has std::expected). -----------------------------
// A value OR an error, and the caller must look. [[nodiscard]] on the TYPE means every function
// returning it is nodiscard automatically.
template <typename T, typename E>
class [[nodiscard]] Expected {
    std::variant<T, E> data_;
    struct ErrTag {};
    Expected(ErrTag, E e) : data_(std::in_place_index<1>, std::move(e)) {}
public:
    Expected(T v) : data_(std::in_place_index<0>, std::move(v)) {}
    static Expected failure(E e) { return Expected(ErrTag{}, std::move(e)); }

    bool ok() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }
    const T& value() const { return std::get<0>(data_); }   // throws std::bad_variant_access if !ok()
    const E& error() const { return std::get<1>(data_); }
};

static Expected<double, std::string> safe_div(double a, double b) {
    if (b == 0.0) return Expected<double, std::string>::failure("division by zero");
    return a / b;
}

// ---- 5. Matrix with a custom exception. The running example. ------------------------------
struct Shape {
    std::size_t rows, cols;
};

// Derive from the CLOSEST standard type so generic handlers still catch it.
class ShapeError : public std::invalid_argument {
    Shape lhs_, rhs_;
    static std::string fmt(Shape a, Shape b, const char* op) {
        return "shape mismatch: (" + std::to_string(a.rows) + "," + std::to_string(a.cols) + ") " +
               op + " (" + std::to_string(b.rows) + "," + std::to_string(b.cols) + ")";
    }
public:
    ShapeError(Shape a, Shape b, const char* op)
        : std::invalid_argument(fmt(a, b, op)), lhs_(a), rhs_(b) {}
    Shape lhs() const noexcept { return lhs_; }
    Shape rhs() const noexcept { return rhs_; }
};

class Matrix {
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<double> data_;
public:
    Matrix() = default;
    Matrix(std::size_t r, std::size_t c, double fill = 0.0) : rows_(r), cols_(c), data_(r * c, fill) {}

    std::size_t rows() const noexcept { return rows_; }
    std::size_t cols() const noexcept { return cols_; }
    Shape shape() const noexcept { return {rows_, cols_}; }

    // Unchecked: for inner loops. The assert documents the contract and vanishes under -DNDEBUG.
    double& operator()(std::size_t i, std::size_t j) noexcept {
        assert(i < rows_ && j < cols_ && "Matrix index out of range");
        return data_[i * cols_ + j];
    }
    double operator()(std::size_t i, std::size_t j) const noexcept {
        assert(i < rows_ && j < cols_ && "Matrix index out of range");
        return data_[i * cols_ + j];
    }

    // Checked: for API boundaries, tests, user-supplied indices.
    double& at(std::size_t i, std::size_t j) {
        if (i >= rows_ || j >= cols_)
            throw std::out_of_range("Matrix::at(" + std::to_string(i) + "," + std::to_string(j) +
                                    ") out of range for shape (" + std::to_string(rows_) + "," +
                                    std::to_string(cols_) + ")");
        return data_[i * cols_ + j];
    }

    // noexcept swap: the building block of the strong guarantee.
    friend void swap(Matrix& a, Matrix& b) noexcept {
        using std::swap;
        swap(a.rows_, b.rows_);
        swap(a.cols_, b.cols_);
        swap(a.data_, b.data_);
    }
};

// Move ops must be noexcept or std::vector<Matrix> falls back to copying on growth.
static_assert(std::is_nothrow_move_constructible_v<Matrix>);
static_assert(std::is_nothrow_swappable_v<Matrix>);

// API boundary: validate ONCE, throw with a message containing the actual numbers.
[[nodiscard]] static Matrix matmul(const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows()) throw ShapeError(a.shape(), b.shape(), "@");
    Matrix c(a.rows(), b.cols());
    // Inner loops: unchecked operator(), no exceptions, nothing can throw here.
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t k = 0; k < a.cols(); ++k) {
            const double aik = a(i, k);
            for (std::size_t j = 0; j < b.cols(); ++j) c(i, j) += aik * b(k, j);
        }
    return c;
}

[[nodiscard]] static Matrix add(const Matrix& a, const Matrix& b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) throw ShapeError(a.shape(), b.shape(), "+");
    Matrix c(a.rows(), a.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t j = 0; j < a.cols(); ++j) c(i, j) = a(i, j) + b(i, j);
    return c;
}

// ---- 6. Stack unwinding + RAII. ----------------------------------------------------------
struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) { std::cout << "    ctor " << name << '\n'; }
    ~Tracer() { std::cout << "    dtor " << name << '\n'; }   // destructors are implicitly noexcept
    Tracer(const Tracer&) = delete;
    Tracer& operator=(const Tracer&) = delete;
};

static void level_c() {
    Tracer c("c");
    throw std::runtime_error("boom in c");
    // Tracer never("never");   // would be unreachable; the compiler may warn
}
static void level_b() {
    Tracer b("b");
    level_c();
}
static void level_a() {
    Tracer a("a");
    level_b();
}

// ---- 7. Rethrow, catch(...), nested. ------------------------------------------------------
static void load_config(const std::string& path) {
    try {
        throw std::runtime_error("parse error at line 3");
    } catch (const std::exception&) {
        // Wrap with context, keeping the original as the "cause".
        std::throw_with_nested(std::runtime_error("while loading " + path));
    }
}

static void print_chain(const std::exception& e, int depth = 0) {
    std::cout << std::string(static_cast<std::size_t>(4 + 2 * depth), ' ') << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception& inner) {
        print_chain(inner, depth + 1);
    }
}

// ---- 8. noexcept, assert, [[nodiscard]]. --------------------------------------------------
static double dot(const double* a, const double* b, std::size_t n) noexcept {
    assert(a != nullptr && b != nullptr);   // programmer contract, not runtime condition
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}
static_assert(noexcept(dot(nullptr, nullptr, 0)), "dot must be noexcept");

// ---- 9. std::system_error from errno. -----------------------------------------------------
static std::FILE* fopen_or_throw(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) throw std::system_error(errno, std::generic_category(), std::string("open ") + path);
    return f;
}

// ---- driver --------------------------------------------------------------------------------
static void run() {
    std::cout << "== 1. C-style return code ==\n";
    {
        int rc = c_style_open_check("/definitely/not/here.bin");
        if (rc < 0) std::cout << "    rc=" << rc << " (" << std::strerror(-rc) << ")\n";
    }

    std::cout << "== 2. bool + out-param ==\n";
    {
        double v = -1.0;
        std::cout << "    \"3.25\" -> " << (parse_double("3.25", v) ? "ok " : "fail ") << v << '\n';
        std::cout << "    \"3.2x\" -> " << (parse_double("3.2x", v) ? "ok " : "fail ") << v
                  << " (untouched)\n";
        // parse_double("1", v);   // <- uncomment: warning: ignoring return value ... nodiscard
    }

    std::cout << "== 3. std::optional ==\n";
    {
        std::vector<int> v{4, 8, 15, 16, 23, 42};
        if (auto idx = find_index(v, 16)) std::cout << "    16 found at index " << *idx << '\n';
        std::cout << "    99 -> " << find_index(v, 99).value_or(9999) << " (value_or default)\n";
        try {
            (void)find_index(v, 99).value();   // .value() throws; *opt would be UB
        } catch (const std::bad_optional_access& e) {
            std::cout << "    .value() on empty: " << e.what() << '\n';
        }
    }

    std::cout << "== 4. Expected<T,E> ==\n";
    {
        auto r1 = safe_div(1.0, 4.0);
        auto r2 = safe_div(1.0, 0.0);
        if (r1) std::cout << "    1/4 = " << r1.value() << '\n';
        if (!r2) std::cout << "    1/0 -> error: " << r2.error() << '\n';
        // safe_div(1, 2);   // <- uncomment: warning: ignoring return value (class is [[nodiscard]])
    }

    std::cout << "== 5. Matrix shape errors ==\n";
    {
        Matrix a(2, 3, 1.0), b(3, 2, 2.0), c(2, 3, 5.0);
        Matrix ab = matmul(a, b);                           // (2,3)@(3,2) -> (2,2), each entry 6
        std::cout << "    a@b = (" << ab.rows() << "," << ab.cols() << "), ab(0,0)=" << ab(0, 0) << '\n';
        try {
            Matrix bad = matmul(a, c);                      // (2,3)@(2,3): invalid
            (void)bad;
        } catch (const ShapeError& e) {                     // most-derived first
            std::cout << "    ShapeError: " << e.what() << "  [lhs.cols=" << e.lhs().cols
                      << " rhs.rows=" << e.rhs().rows << "]\n";
        }
        try {
            Matrix bad = add(a, b);
            (void)bad;
        } catch (const std::invalid_argument& e) {          // base handler also works
            std::cout << "    as invalid_argument: " << e.what() << '\n';
        }
        try {
            a.at(5, 0) = 1.0;
        } catch (const std::out_of_range& e) {
            std::cout << "    at(): " << e.what() << '\n';
        }
        // a(5, 0) = 1.0;   // UB in release; assert fires in debug. Never do this.
    }

    std::cout << "== 6. Unwinding order (ctor a,b,c then dtor c,b,a then handler) ==\n";
    try {
        level_a();
    } catch (const std::exception& e) {
        std::cout << "    caught: " << e.what() << '\n';
    }

    std::cout << "== 7. Rethrow / catch(...) / nested ==\n";
    try {
        try {
            throw 42;                                       // throwing a non-std type: legal, unwise
        } catch (const std::exception&) {
            std::cout << "    not reached\n";
        } catch (...) {
            std::cout << "    catch(...) got a non-std exception, rethrowing\n";
            throw;                                          // rethrow the SAME object
        }
    } catch (int v) {
        std::cout << "    outer caught int " << v << '\n';
    }
    try {
        load_config("model.yaml");
    } catch (const std::exception& e) {
        std::cout << "    nested chain:\n";
        print_chain(e);
    }

    std::cout << "== 8. noexcept / assert ==\n";
    {
        double x[3] = {1, 2, 3}, y[3] = {4, 5, 6};
        std::cout << "    dot = " << dot(x, y, 3) << "  (noexcept: " << std::boolalpha
                  << noexcept(dot(x, y, 3)) << ")\n";
#ifdef NDEBUG
        std::cout << "    NDEBUG defined: asserts are compiled out\n";
#else
        std::cout << "    NDEBUG not defined: asserts are active\n";
#endif
    }

    std::cout << "== 9. std::system_error ==\n";
    try {
        std::FILE* f = fopen_or_throw("/nonexistent/weights.bin");
        std::fclose(f);
    } catch (const std::system_error& e) {
        std::cout << "    " << e.what() << "\n    code=" << e.code().value()
                  << " is ENOENT: " << (e.code() == std::errc::no_such_file_or_directory) << '\n';
    }

    std::cout << "== 10. an uncaught exception reaches main's handler ==\n";
    throw std::invalid_argument("shape mismatch: (784,128) @ (784,128)");
}

// Function-try-block on main: the last line of defence. Without it, libc++abi prints
// "terminating due to uncaught exception..." and the process aborts (exit via SIGABRT).
int main() try {
    run();
    return 0;
} catch (const std::exception& e) {
    std::cout << "    main caught std::exception: " << e.what() << "\n    exiting with status 1\n";
    return 1;
} catch (...) {
    std::cerr << "fatal: unknown exception\n";
    return 2;
}
