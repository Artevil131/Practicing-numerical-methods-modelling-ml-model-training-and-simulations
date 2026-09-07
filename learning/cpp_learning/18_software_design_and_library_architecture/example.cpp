// Chapter 18 — Software design and library architecture, in one file.
//
// Compile: c++ -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -O2 -o ex_demo example.cpp
// Run:     ./ex_demo
// Also try: c++ -Wall -Wextra -std=c++17 -g -fsanitize=address,undefined -o ex_demo example.cpp && ./ex_demo
//
// Sections (each is a self-contained demonstration of a lesson section):
//   1  Regular value type + compile-time regularity check           (lesson §1)
//   2  Function<R(Args...)>: hand-rolled std::function              (lesson §2.2)
//   3  Type-erased Layer with value semantics; Sequential by composition (lesson §2.1, §8)
//   4  Policy-based Optimizer<SGDPolicy|AdamPolicy>                  (lesson §3)
//   5  Strong types: Rows/Cols/Meters/Seconds, Index<Tag>            (lesson §4)
//   6  pimpl: Solver with unique_ptr<Impl>, dtor defined out of line (lesson §5)
//   7  Views vs owners, hidden friends                               (lesson §6)
//   8  Signal/observer with RAII connection tokens                   (lesson §11)
//   9  Versioned TLV binary: write v2, read with a v1 reader         (lesson §12)
//
// The "library" here is a toy Matrix so the file stays self-contained; in your P01-P05 code the
// same patterns apply to your real Matrix/Tensor.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// =====================================================================================
// 1. A regular value type: Matrix. Copy is deep, == compares contents, move is O(1).
// =====================================================================================
namespace num {

class Matrix {
    std::size_t r_ = 0, c_ = 0;
    std::vector<double> d_;
public:
    Matrix() = default;
    Matrix(std::size_t r, std::size_t c, double fill = 0.0) : r_(r), c_(c), d_(r * c, fill) {}
    std::size_t rows() const noexcept { return r_; }
    std::size_t cols() const noexcept { return c_; }
    double& operator()(std::size_t i, std::size_t j) noexcept { return d_[i * c_ + j]; }
    double operator()(std::size_t i, std::size_t j) const noexcept { return d_[i * c_ + j]; }
    double* data() noexcept { return d_.data(); }
    const double* data() const noexcept { return d_.data(); }
    std::size_t size() const noexcept { return d_.size(); }

    // Hidden friends (§6.4): found only by ADL, keep namespace num's overload set small.
    friend bool operator==(const Matrix& a, const Matrix& b) { return a.r_ == b.r_ && a.c_ == b.c_ && a.d_ == b.d_; }
    friend bool operator!=(const Matrix& a, const Matrix& b) { return !(a == b); }
    friend Matrix operator*(const Matrix& a, const Matrix& b) {
        if (a.c_ != b.r_) throw std::invalid_argument("matmul: shape mismatch");
        Matrix out(a.r_, b.c_);
        for (std::size_t i = 0; i < a.r_; ++i)
            for (std::size_t k = 0; k < a.c_; ++k) {
                const double aik = a(i, k);
                for (std::size_t j = 0; j < b.c_; ++j) out(i, j) += aik * b(k, j);
            }
        return out;
    }
    friend std::ostream& operator<<(std::ostream& os, const Matrix& m) {
        for (std::size_t i = 0; i < m.r_; ++i) {
            os << (i == 0 ? "[[" : " [");
            for (std::size_t j = 0; j < m.c_; ++j) os << (j ? ", " : "") << m(i, j);
            os << (i + 1 == m.r_ ? "]]" : "]\n");
        }
        return os;
    }
    friend double frobenius(const Matrix& m) {           // a customization point by convention: norm(x) via ADL
        return std::sqrt(std::inner_product(m.d_.begin(), m.d_.end(), m.d_.begin(), 0.0));
    }
};

// C++17 approximation of std::regular (syntactic part only).
template <class, class = void> struct has_eq : std::false_type {};
template <class T> struct has_eq<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>> : std::true_type {};
template <class T>
constexpr bool is_regular_v = std::is_default_constructible_v<T> && std::is_copy_constructible_v<T> &&
                              std::is_copy_assignable_v<T> && std::is_nothrow_move_constructible_v<T> &&
                              std::is_destructible_v<T> && has_eq<T>::value;
static_assert(is_regular_v<Matrix>, "Matrix must be a regular type");
static_assert(!is_regular_v<std::unique_ptr<int>>, "unique_ptr is move-only, not regular");

}  // namespace num

// =====================================================================================
// 2. Function<R(Args...)>: the Concept/Model pattern with one operation.
// =====================================================================================
template <class Sig> class Function;                              // primary template: never defined

template <class R, class... Args>
class Function<R(Args...)> {
    struct Concept {
        virtual ~Concept() = default;
        virtual R call(Args... a) = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    template <class F> struct Model final : Concept {
        F f;
        explicit Model(F g) : f(std::move(g)) {}
        R call(Args... a) override { return std::invoke(f, std::forward<Args>(a)...); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }
    };
    std::unique_ptr<Concept> p_;
public:
    Function() = default;
    // Constrained: F must be callable with the signature AND must not be Function itself,
    // otherwise this template would hijack the copy constructor for non-const lvalues.
    template <class F, class D = std::decay_t<F>,
              class = std::enable_if_t<!std::is_same_v<D, Function> && std::is_invocable_r_v<R, D&, Args...>>>
    Function(F&& f) : p_(std::make_unique<Model<D>>(std::forward<F>(f))) {}
    Function(const Function& o) : p_(o.p_ ? o.p_->clone() : nullptr) {}
    Function(Function&&) noexcept = default;
    Function& operator=(Function o) noexcept { p_ = std::move(o.p_); return *this; }   // unified assignment
    explicit operator bool() const noexcept { return p_ != nullptr; }
    R operator()(Args... a) const {
        if (!p_) throw std::bad_function_call{};
        return p_->call(std::forward<Args>(a)...);
    }
};

static double twice(double x) { return 2 * x; }

// =====================================================================================
// 3. Type-erased Layer. Linear/ReLU/Sequential are plain structs: no base class, no virtual.
// =====================================================================================
namespace nn {
using num::Matrix;

struct Param { double* w; double* g; std::size_t n; };          // non-owning spans into a layer

class Layer {
    struct Concept {
        virtual ~Concept() = default;
        virtual Matrix forward(const Matrix& x) = 0;
        virtual Matrix backward(const Matrix& gout) = 0;
        virtual std::vector<Param> parameters() = 0;
        virtual std::string name() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    template <class T> struct Model final : Concept {
        T v;
        explicit Model(T x) : v(std::move(x)) {}
        Matrix forward(const Matrix& x) override { return v.forward(x); }
        Matrix backward(const Matrix& g) override { return v.backward(g); }
        std::vector<Param> parameters() override { return v.parameters(); }
        std::string name() const override { return v.name(); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }
    };
    std::unique_ptr<Concept> p_;
public:
    template <class T, class D = std::decay_t<T>, class = std::enable_if_t<!std::is_same_v<D, Layer>>>
    Layer(T&& x) : p_(std::make_unique<Model<D>>(std::forward<T>(x))) {}
    Layer(const Layer& o) : p_(o.p_->clone()) {}                  // deep copy: value semantics
    Layer(Layer&&) noexcept = default;
    Layer& operator=(Layer o) noexcept { p_ = std::move(o.p_); return *this; }
    Matrix forward(const Matrix& x) { return p_->forward(x); }
    Matrix backward(const Matrix& g) { return p_->backward(g); }
    std::vector<Param> parameters() { return p_->parameters(); }
    std::string name() const { return p_->name(); }
};

struct Linear {                                                   // plain struct, duck-typed
    Matrix W, b, gW, gb, x_cache;
    Linear(std::size_t in, std::size_t out, std::uint32_t seed = 1)
        : W(in, out), b(1, out), gW(in, out), gb(1, out) {
        std::uint32_t s = seed;                                   // tiny LCG: deterministic init
        for (std::size_t i = 0; i < W.size(); ++i) {
            s = s * 1664525u + 1013904223u;
            W.data()[i] = (static_cast<double>(s >> 8) / 16777216.0 - 0.5) * 2.0 / std::sqrt(static_cast<double>(in));
        }
    }
    Matrix forward(const Matrix& x) {                             // x: (batch, in)
        x_cache = x;
        Matrix y = x * W;
        for (std::size_t i = 0; i < y.rows(); ++i)
            for (std::size_t j = 0; j < y.cols(); ++j) y(i, j) += b(0, j);
        return y;
    }
    Matrix backward(const Matrix& gy) {                           // gy: (batch, out) -> gx: (batch, in)
        for (std::size_t i = 0; i < gW.size(); ++i) gW.data()[i] = 0.0;
        for (std::size_t j = 0; j < gb.size(); ++j) gb.data()[j] = 0.0;
        Matrix gx(x_cache.rows(), W.rows());
        for (std::size_t n = 0; n < gy.rows(); ++n)
            for (std::size_t o = 0; o < W.cols(); ++o) {
                const double g = gy(n, o);
                gb(0, o) += g;
                for (std::size_t i = 0; i < W.rows(); ++i) { gW(i, o) += x_cache(n, i) * g; gx(n, i) += W(i, o) * g; }
            }
        return gx;
    }
    std::vector<Param> parameters() { return { {W.data(), gW.data(), W.size()}, {b.data(), gb.data(), b.size()} }; }
    std::string name() const { return "Linear(" + std::to_string(W.rows()) + "," + std::to_string(W.cols()) + ")"; }
};

struct Tanh {
    Matrix y_cache;
    Matrix forward(const Matrix& x) {
        y_cache = x;
        for (std::size_t i = 0; i < y_cache.size(); ++i) y_cache.data()[i] = std::tanh(x.data()[i]);
        return y_cache;
    }
    Matrix backward(const Matrix& gy) {
        Matrix gx = gy;
        for (std::size_t i = 0; i < gx.size(); ++i) gx.data()[i] *= 1.0 - y_cache.data()[i] * y_cache.data()[i];
        return gx;
    }
    std::vector<Param> parameters() { return {}; }
    std::string name() const { return "Tanh"; }
};

struct Sequential {                                               // CONTAINS Layers; is itself erasable into a Layer
    std::vector<Layer> layers;
    Sequential(std::initializer_list<Layer> ls) : layers(ls) {}
    Matrix forward(const Matrix& x) { Matrix h = x; for (auto& l : layers) h = l.forward(h); return h; }
    Matrix backward(const Matrix& g) { Matrix gh = g; for (auto it = layers.rbegin(); it != layers.rend(); ++it) gh = it->backward(gh); return gh; }
    std::vector<Param> parameters() {
        std::vector<Param> ps;
        for (auto& l : layers) { auto p = l.parameters(); ps.insert(ps.end(), p.begin(), p.end()); }
        return ps;
    }
    std::string name() const {
        std::string s = "Sequential[";
        for (std::size_t i = 0; i < layers.size(); ++i) s += (i ? ", " : "") + layers[i].name();
        return s + "]";
    }
};

// =====================================================================================
// 4. Optimizer as a policy: the per-element update is inlined; no virtual in the hot loop.
// =====================================================================================
struct SGDPolicy {
    double lr;
    static constexpr std::size_t state_per_param = 0;
    void update(double& w, double g, double*, long) const { w -= lr * g; }
};
struct AdamPolicy {
    double lr = 1e-2, b1 = 0.9, b2 = 0.999, eps = 1e-8;
    static constexpr std::size_t state_per_param = 2;             // m, v
    void update(double& w, double g, double* st, long t) const {
        double& m = st[0]; double& v = st[1];
        m = b1 * m + (1 - b1) * g;
        v = b2 * v + (1 - b2) * g * g;
        const double mhat = m / (1 - std::pow(b1, static_cast<double>(t)));
        const double vhat = v / (1 - std::pow(b2, static_cast<double>(t)));
        w -= lr * mhat / (std::sqrt(vhat) + eps);
    }
};
template <class Policy>
class Optimizer {
    Policy pol_;
    std::vector<Param> params_;
    std::vector<double> state_;                                   // Policy::state_per_param doubles per scalar weight
    long t_ = 0;
public:
    Optimizer(Policy p, std::vector<Param> params) : pol_(p), params_(std::move(params)) {
        std::size_t n = 0;
        for (auto& q : params_) n += q.n;
        state_.assign(n * Policy::state_per_param, 0.0);
    }
    void step() {
        ++t_;
        std::size_t off = 0;
        for (auto& q : params_) {
            for (std::size_t i = 0; i < q.n; ++i)
                pol_.update(q.w[i], q.g[i], state_.data() + (off + i) * Policy::state_per_param, t_);
            off += q.n;
        }
    }
};

}  // namespace nn

// =====================================================================================
// 5. Strong types.
// =====================================================================================
template <class T, class Tag>
struct Strong {
    T v;
    constexpr explicit Strong(T x) : v(x) {}
    constexpr T get() const { return v; }
    friend constexpr bool operator==(Strong a, Strong b) { return a.v == b.v; }
    friend constexpr Strong operator+(Strong a, Strong b) { return Strong{a.v + b.v}; }
};
using Rows = Strong<std::size_t, struct RowsTag>;
using Cols = Strong<std::size_t, struct ColsTag>;
using Meters = Strong<double, struct MetersTag>;
using Seconds = Strong<double, struct SecondsTag>;
using MetersPerSecond = Strong<double, struct MpsTag>;
constexpr MetersPerSecond operator/(Meters d, Seconds t) { return MetersPerSecond{d.get() / t.get()}; }
// Meters + Seconds: no such operator -> compile error. Exactly what we want.

static num::Matrix make_matrix(Rows r, Cols c) { return num::Matrix(r.get(), c.get(), 1.0); }

template <class Tag> struct Index { std::size_t i; };
using BodyId = Index<struct BodyTag>;
using NodeId = Index<struct NodeTag>;
struct Bodies { std::vector<double> mass; double& operator[](BodyId k) { return mass[k.i]; } };
struct Tree   { std::vector<int> child;   int&    operator[](NodeId k) { return child[k.i]; } };
static_assert(sizeof(BodyId) == sizeof(std::size_t), "strong index is free");

// =====================================================================================
// 6. pimpl. In a real library the Impl and the out-of-line definitions live in solver.cpp;
//    here they are placed AFTER the class to show that the header needs only the declaration.
// =====================================================================================
class Solver {
public:
    explicit Solver(std::size_t n);
    ~Solver();                                                    // MUST be defined where Impl is complete
    Solver(Solver&&) noexcept;
    Solver& operator=(Solver&&) noexcept;
    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;
    void set(std::size_t i, std::size_t j, double v);
    std::vector<double> solve(std::vector<double> b) const;       // Gaussian elimination, partial pivoting
    int last_pivot_swaps() const;
private:
    struct Impl;                                                  // incomplete here
    std::unique_ptr<Impl> impl_;
};
// ---- "solver.cpp" ----
struct Solver::Impl {
    std::size_t n;
    std::vector<double> a;                                        // could be Eigen::SparseLU; users never see it
    mutable int swaps = 0;
    explicit Impl(std::size_t n_) : n(n_), a(n_ * n_, 0.0) {}
};
Solver::Solver(std::size_t n) : impl_(std::make_unique<Impl>(n)) {}
Solver::~Solver() = default;                                      // here Impl is complete: unique_ptr can delete it
Solver::Solver(Solver&&) noexcept = default;
Solver& Solver::operator=(Solver&&) noexcept = default;
void Solver::set(std::size_t i, std::size_t j, double v) { impl_->a[i * impl_->n + j] = v; }
int Solver::last_pivot_swaps() const { return impl_->swaps; }
std::vector<double> Solver::solve(std::vector<double> b) const {
    const std::size_t n = impl_->n;
    std::vector<double> a = impl_->a;                             // work on a copy: solve() is const
    impl_->swaps = 0;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t p = k;
        for (std::size_t i = k + 1; i < n; ++i) if (std::fabs(a[i * n + k]) > std::fabs(a[p * n + k])) p = i;
        if (p != k) { for (std::size_t j = 0; j < n; ++j) std::swap(a[k * n + j], a[p * n + j]); std::swap(b[k], b[p]); ++impl_->swaps; }
        if (a[k * n + k] == 0.0) throw std::runtime_error("singular matrix");
        for (std::size_t i = k + 1; i < n; ++i) {
            const double f = a[i * n + k] / a[k * n + k];
            for (std::size_t j = k; j < n; ++j) a[i * n + j] -= f * a[k * n + j];
            b[i] -= f * b[k];
        }
    }
    for (std::size_t k = n; k-- > 0;) {
        double s = b[k];
        for (std::size_t j = k + 1; j < n; ++j) s -= a[k * n + j] * b[j];
        b[k] = s / a[k * n + k];
    }
    return b;
}

// =====================================================================================
// 7. Non-owning views: one function serves a Matrix, a block of it, or a raw array.
// =====================================================================================
struct ConstMatrixView {
    const double* data; std::size_t rows, cols, row_stride;
    double operator()(std::size_t i, std::size_t j) const noexcept { return data[i * row_stride + j]; }
    ConstMatrixView block(std::size_t r0, std::size_t c0, std::size_t nr, std::size_t nc) const {
        return {data + r0 * row_stride + c0, nr, nc, row_stride};
    }
};
static ConstMatrixView view(const num::Matrix& m) { return {m.data(), m.rows(), m.cols(), m.cols()}; }
static double trace(ConstMatrixView v) {                          // takes a view, returns a value
    double t = 0; for (std::size_t i = 0; i < std::min(v.rows, v.cols); ++i) t += v(i, i); return t;
}

// =====================================================================================
// 8. Signal / observer with RAII connection tokens. No shared ownership of subscribers.
// =====================================================================================
template <class... Args>
class Signal {
    struct Slot { std::uint64_t id; std::function<void(Args...)> fn; };
    struct State { std::vector<Slot> slots; std::uint64_t next_id = 1; };
    std::shared_ptr<State> st_ = std::make_shared<State>();
public:
    class Connection {
        std::weak_ptr<State> st_; std::uint64_t id_ = 0;
    public:
        Connection() = default;
        Connection(std::weak_ptr<State> s, std::uint64_t id) : st_(std::move(s)), id_(id) {}
        Connection(Connection&& o) noexcept : st_(std::move(o.st_)), id_(std::exchange(o.id_, 0)) {}
        Connection& operator=(Connection&& o) noexcept { disconnect(); st_ = std::move(o.st_); id_ = std::exchange(o.id_, 0); return *this; }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        ~Connection() { disconnect(); }
        void disconnect() {
            if (auto s = st_.lock(); s && id_ != 0) {
                auto& v = s->slots;
                v.erase(std::remove_if(v.begin(), v.end(), [this](const Slot& sl) { return sl.id == id_; }), v.end());
            }
            id_ = 0;                                              // a dead Signal: lock() fails, nothing to do
        }
    };
    [[nodiscard]] Connection connect(std::function<void(Args...)> f) {
        const auto id = st_->next_id++;
        st_->slots.push_back({id, std::move(f)});
        return Connection(st_, id);
    }
    void emit(Args... a) {
        auto copy = st_->slots;                                   // slots may (dis)connect while we iterate
        for (auto& s : copy) s.fn(a...);
    }
    std::size_t size() const { return st_->slots.size(); }
};

struct Logger {                                                   // subscriber; owns its Connection
    std::vector<std::string> lines;
    Signal<int, double>::Connection conn;                         // declared LAST: destroyed first
    explicit Logger(Signal<int, double>& sig)
        : conn(sig.connect([this](int step, double loss) { lines.push_back("step " + std::to_string(step) + " loss " + std::to_string(loss)); })) {}
};

// =====================================================================================
// 9. Versioned TLV binary format. Fixed-width little-endian fields; unknown tags are skipped.
// =====================================================================================
namespace fmt {
enum Tag : std::uint16_t { kNBodies = 1, kTime = 2, kMasses = 3, kSoftening = 4 /* added in v2 */ };
constexpr char kMagic[4] = {'N', 'B', 'D', 'Y'};

struct Writer {
    std::vector<unsigned char> buf;
    template <class T> void put(T v) {                            // little-endian, byte by byte: portable
        static_assert(std::is_arithmetic_v<T>);
        std::uint64_t bits = 0; std::memcpy(&bits, &v, sizeof v);
        for (std::size_t i = 0; i < sizeof v; ++i) buf.push_back(static_cast<unsigned char>((bits >> (8 * i)) & 0xFFu));
    }
    void field(std::uint16_t tag, const void* p, std::uint32_t len) {
        put(tag); put(len);
        const auto* b = static_cast<const unsigned char*>(p);
        buf.insert(buf.end(), b, b + len);
    }
};
struct Reader {
    const std::vector<unsigned char>& buf; std::size_t pos = 0;
    template <class T> bool get(T& out) {
        if (pos + sizeof(T) > buf.size()) return false;
        std::uint64_t bits = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) bits |= static_cast<std::uint64_t>(buf[pos + i]) << (8 * i);
        std::memcpy(&out, &bits, sizeof(T)); pos += sizeof(T); return true;
    }
};

struct SnapshotV1 { std::uint32_t n = 0; double time = 0; std::vector<double> masses; std::vector<std::uint16_t> skipped; };

std::vector<unsigned char> write_v2(std::uint32_t n, double time, const std::vector<double>& masses, double softening) {
    Writer w;
    w.buf.insert(w.buf.end(), kMagic, kMagic + 4);
    w.put<std::uint32_t>(2);                                      // version
    w.field(kNBodies, &n, sizeof n);
    { Writer t; t.put(time); w.field(kTime, t.buf.data(), static_cast<std::uint32_t>(t.buf.size())); }
    { Writer t; for (double m : masses) t.put(m); w.field(kMasses, t.buf.data(), static_cast<std::uint32_t>(t.buf.size())); }
    { Writer t; t.put(softening); w.field(kSoftening, t.buf.data(), static_cast<std::uint32_t>(t.buf.size())); }
    return w.buf;
}
// A v1 READER: knows tags 1..3 only. It must load a v2 file by skipping tag 4.
SnapshotV1 read_v1(const std::vector<unsigned char>& bytes) {
    Reader r{bytes};
    if (bytes.size() < 8 || std::memcmp(bytes.data(), kMagic, 4) != 0) throw std::runtime_error("bad magic");
    r.pos = 4;
    std::uint32_t version = 0; r.get(version);
    if (version < 1) throw std::runtime_error("unsupported version");
    SnapshotV1 s;
    std::uint16_t tag; std::uint32_t len;
    while (r.get(tag) && r.get(len)) {
        if (r.pos + len > bytes.size()) throw std::runtime_error("truncated field");
        Reader f{bytes, r.pos};
        switch (tag) {
            case kNBodies: f.get(s.n); break;
            case kTime:    f.get(s.time); break;
            case kMasses:  s.masses.resize(len / sizeof(double)); for (auto& m : s.masses) f.get(m); break;
            default:       s.skipped.push_back(tag); break;        // forward compatibility: skip unknown
        }
        r.pos += len;
    }
    return s;
}
}  // namespace fmt

// =====================================================================================
int main() {
    using num::Matrix;

    // ---- 1. regular type -------------------------------------------------------------
    std::printf("== 1. regular value type ==\n");
    Matrix a(2, 2, 1.0); a(0, 1) = 2.0;
    Matrix b = a;                                                 // deep copy
    b(0, 0) = 9.0;
    std::printf("a(0,0)=%g  b(0,0)=%g  a==b: %s  is_regular<Matrix>: %d\n", a(0, 0), b(0, 0), a == b ? "yes" : "no", num::is_regular_v<Matrix>);

    // ---- 2. Function -----------------------------------------------------------------
    std::printf("\n== 2. Function<R(Args...)> ==\n");
    Function<double(double)> f = twice;                           // free function
    Function<double(double)> g = [k = 3.0](double x) { return k * x; };   // stateful lambda
    Function<double(double)> h = g;                               // deep copy through clone()
    g = f;                                                        // assignment; h unaffected
    std::printf("f(2)=%g  g(2)=%g (now twice)  h(2)=%g (copy kept k=3)\n", f(2), g(2), h(2));
    Function<double(double)> empty;
    try { empty(1.0); } catch (const std::bad_function_call&) { std::printf("empty(): bad_function_call\n"); }

    // ---- 3+4. type-erased Layer, Sequential, policy optimizer --------------------------
    std::printf("\n== 3/4. type-erased Layer + policy Optimizer ==\n");
    nn::Sequential seq = { nn::Linear(1, 16, 7), nn::Tanh{}, nn::Linear(16, 1, 11) };
    nn::Layer net = seq;                                          // Sequential erased into a Layer value
    nn::Layer untouched = net;                                    // deep copy BEFORE training
    std::printf("net: %s\n", net.name().c_str());

    const std::size_t N = 64;
    Matrix X(N, 1), Y(N, 1);
    for (std::size_t i = 0; i < N; ++i) { X(i, 0) = -3.0 + 6.0 * static_cast<double>(i) / (N - 1); Y(i, 0) = std::sin(X(i, 0)); }
    auto mse_and_grad = [&](nn::Layer& L, Matrix& grad) {
        Matrix P = L.forward(X);
        grad = Matrix(N, 1); double loss = 0;
        for (std::size_t i = 0; i < N; ++i) { const double d = P(i, 0) - Y(i, 0); loss += d * d; grad(i, 0) = 2 * d / static_cast<double>(N); }
        return loss / static_cast<double>(N);
    };
    nn::Optimizer<nn::AdamPolicy> opt(nn::AdamPolicy{0.02}, net.parameters());   // parameters(): spans into net's storage
    Matrix grad;
    for (int step = 0; step <= 600; ++step) {
        const double loss = mse_and_grad(net, grad);
        if (step % 200 == 0) std::printf("step %4d  adam loss %.4f\n", step, loss);
        net.backward(grad);
        opt.step();
    }
    Matrix g2;
    std::printf("untrained copy loss: %.4f  (value semantics: training `net` did not touch `untouched`)\n", mse_and_grad(untouched, g2));

    // ---- 5. strong types ---------------------------------------------------------------
    std::printf("\n== 5. strong types ==\n");
    Matrix m = make_matrix(Rows{2}, Cols{3});                     // make_matrix(Cols{3}, Rows{2}) would not compile
    MetersPerSecond v = Meters{100.0} / Seconds{9.58};
    Bodies bodies{{1.0, 2.0}}; Tree tree{{0, 1, 2}};
    BodyId bi{1}; NodeId ni{2};
    std::printf("matrix %zux%zu  100m/9.58s = %.3f m/s  bodies[BodyId 1]=%g  tree[NodeId 2]=%d\n",
                m.rows(), m.cols(), v.get(), bodies[bi], tree[ni]);
    // bodies[ni];   // error: no operator[](NodeId) — the bug Barnes-Hut code hits silently with size_t

    // ---- 6. pimpl ------------------------------------------------------------------------
    std::printf("\n== 6. pimpl Solver (sizeof(Solver)=%zu, one pointer) ==\n", sizeof(Solver));
    Solver s(3);
    const double A[3][3] = {{0, 2, 1}, {1, 1, 1}, {2, 1, 3}};      // row 0 has a zero pivot -> swaps
    for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j) s.set(i, j, A[i][j]);
    // b = A * (1,1,2): row0: 0+2+2=4, row1: 1+1+2=4, row2: 2+1+6=9  -> expect x = (1, 1, 2)
    std::vector<double> x = s.solve({4, 4, 9});
    std::printf("solve -> x = (%.3f, %.3f, %.3f), pivot swaps = %d\n", x[0], x[1], x[2], s.last_pivot_swaps());
    Solver s2 = std::move(s);                                     // move is a pointer steal (noexcept)
    std::printf("moved: solve again -> x0 = %.3f\n", s2.solve({4, 4, 9})[0]);

    // ---- 7. views ------------------------------------------------------------------------
    std::printf("\n== 7. views vs owners ==\n");
    Matrix big(4, 4);
    for (std::size_t i = 0; i < 4; ++i) for (std::size_t j = 0; j < 4; ++j) big(i, j) = static_cast<double>(i * 4 + j);
    std::printf("trace(big) = %g   trace(big.block(1,1,2,2)) = %g   (one function, no copies)\n",
                trace(view(big)), trace(view(big).block(1, 1, 2, 2)));
    std::printf("frobenius(a) via ADL hidden friend = %.4f\n", frobenius(a));

    // ---- 8. signal / observer ------------------------------------------------------------
    std::printf("\n== 8. Signal with RAII connections ==\n");
    Signal<int, double> on_step;
    int fired = 0;
    {
        Logger log(on_step);
        Signal<int, double>::Connection c;
        c = on_step.connect([&](int step, double) { ++fired; if (step == 2) c.disconnect(); });  // disconnects itself mid-emit
        std::printf("slots after connect: %zu\n", on_step.size());
        for (int i = 1; i <= 4; ++i) on_step.emit(i, 1.0 / i);
        std::printf("slots after self-disconnect: %zu  lambda fired %d times  logger lines %zu\n", on_step.size(), fired, log.lines.size());
    }                                                             // Logger destroyed -> its Connection dtor removes the slot
    std::printf("slots after Logger destroyed: %zu\n", on_step.size());
    Signal<int, double>::Connection orphan;
    { Signal<int, double> tmp; orphan = tmp.connect([](int, double) {}); }   // signal dies first
    orphan.disconnect();                                          // safe: weak_ptr lock() fails, no dangling access
    std::printf("connection outliving its Signal: disconnect() is a no-op, no UB\n");

    // ---- 9. versioned TLV --------------------------------------------------------------
    std::printf("\n== 9. versioned TLV binary ==\n");
    std::vector<unsigned char> file = fmt::write_v2(3, 12.5, {1.0, 2.0, 0.5}, 0.01);
    std::printf("v2 file: %zu bytes, header %c%c%c%c version %u\n", file.size(), file[0], file[1], file[2], file[3],
                static_cast<unsigned>(file[4]) | static_cast<unsigned>(file[5]) << 8);
    fmt::SnapshotV1 snap = fmt::read_v1(file);
    std::printf("v1 reader: n=%u time=%g masses=[%g %g %g] skipped unknown tags:", snap.n, snap.time, snap.masses[0], snap.masses[1], snap.masses[2]);
    for (auto t : snap.skipped) std::printf(" %u", t);
    std::printf("  (forward compatible)\n");
    file[1] = 'X';
    try { fmt::read_v1(file); } catch (const std::exception& e) { std::printf("corrupted magic -> %s\n", e.what()); }

    return 0;
}
