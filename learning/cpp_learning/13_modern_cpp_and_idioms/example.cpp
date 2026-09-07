// Chapter 13 — Modern C++17 features and idioms, in one program.
//
// Compile:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp
// Run:      ./ex_demo
// (Also compiles as C++20: add -std=c++20. The C++20-only features are described in lesson.md §12
//  and exercised in exercises.md 13.7 — this file stays strictly C++17.)
//
// Sections:
//   1  structured bindings, if constexpr, fold expressions, CTAD
//   2  std::optional lookup, std::string_view tokenizer
//   3  std::variant + std::visit: a tiny autograd graph (forward AND backward)
//   4  idioms: enum class, using alias, strong types, explicit, Rule of Zero, [[nodiscard]]
//   5  std::function vs template callback
//   6  brief patterns: pimpl, CRTP, tag dispatch, type erasure
//   7  std::filesystem, nested namespaces, inline variables

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// ---- 7 (declared early). Nested namespace + inline constexpr variables ---------------------
namespace phys::constants {                     // C++17 nested namespace definition
inline constexpr double kG  = 6.674e-11;        // inline: ONE object even if included from many TUs
inline constexpr double kPi = 3.14159265358979323846;
}  // namespace phys::constants

// ---- 1. Structured bindings, if constexpr, fold expressions, CTAD -----------------------------
struct Shape { std::size_t rows, cols; };

static std::tuple<double, double, int> solve_stats() { return {0.001, 1e-9, 42}; }

// if constexpr: the untaken branch is NOT instantiated, so std::to_string(Shape) never has to compile.
template <typename T>
std::string describe(const T& x) {
    if constexpr (std::is_floating_point_v<T>) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "float-like %.3f", static_cast<double>(x));
        return buf;
    } else if constexpr (std::is_integral_v<T>) {
        return "int-like " + std::to_string(x);
    } else {
        return "other, " + std::to_string(sizeof(T)) + " bytes";
    }
}

// Fold expressions over a parameter pack.
template <typename... Ts> constexpr auto sum(Ts... xs) { return (xs + ... + 0.0); }
template <typename... Ts> constexpr bool all_positive(Ts... xs) { return ((xs > 0) && ...); }
template <typename... Ts> constexpr std::size_t product(Ts... dims) { return (std::size_t{1} * ... * static_cast<std::size_t>(dims)); }

static void section_bindings() {
    std::cout << "== 1. structured bindings / if constexpr / folds / CTAD ==\n";
    Shape s{3, 4};
    auto [r, c] = s;                                        // struct members in order
    auto [loss, gnorm, iters] = solve_stats();              // tuple
    std::cout << "    shape " << r << 'x' << c << "  loss=" << loss << " |g|=" << gnorm << " iters=" << iters << '\n';

    std::map<std::string, double> params{{"lr", 0.01}, {"wd", 1e-4}};
    std::cout << "   ";
    for (const auto& [name, value] : params) std::cout << ' ' << name << '=' << value;   // const auto&: no copies
    if (auto [it, inserted] = params.insert({"lr", 0.1}); !inserted)   // if-with-initializer
        std::cout << "  (lr already present: " << it->second << ")\n";

    std::cout << "    " << describe(2.0 / 3) << " | " << describe(7) << " | " << describe(s) << '\n';
    static_assert(product(3, 4, 5) == 60);
    static_assert(all_positive(1, 2, 3) && !all_positive(1, -2, 3));
    std::cout << "    sum(1, 2.5, 3) = " << sum(1, 2.5, 3) << "  product(3,4,5) = " << product(3, 4, 5) << '\n';

    std::pair p{1, 2.5};                                    // CTAD: pair<int, double>
    std::array a{1.0, 2.0, 3.0};                            // array<double, 3>
    std::vector v{1, 2};                                    // vector<int> with TWO elements {1,2} — not 2 ones
    static_assert(std::is_same_v<decltype(p), std::pair<int, double>>);
    static_assert(std::is_same_v<decltype(a), std::array<double, 3>>);
    std::cout << "    CTAD: pair<int,double>{" << p.first << ',' << p.second << "} array<double,3> vector v.size()=" << v.size() << '\n';
}

// ---- 2. std::optional + std::string_view --------------------------------------------------------
// Split WITHOUT allocating strings: each token is a (pointer, length) view into `text`.
static std::vector<std::string_view> split(std::string_view text, std::string_view delims) {
    std::vector<std::string_view> out;
    while (true) {
        const auto start = text.find_first_not_of(delims);
        if (start == std::string_view::npos) break;
        text.remove_prefix(start);
        const auto end = text.find_first_of(delims);
        out.push_back(text.substr(0, end));
        if (end == std::string_view::npos) break;
        text.remove_prefix(end);
    }
    return out;
}

class Vocab {
    std::unordered_map<std::string, int> ids_;              // keys must OWN their text: std::string
public:
    int add(std::string_view tok) {
        auto it = ids_.find(std::string(tok));              // C++17 unordered_map needs a std::string key
        if (it != ids_.end()) return it->second;
        const int id = static_cast<int>(ids_.size());
        ids_.emplace(std::string(tok), id);
        return id;
    }
    [[nodiscard]] std::optional<int> lookup(std::string_view tok) const {   // "not found" is not an error
        auto it = ids_.find(std::string(tok));
        if (it == ids_.end()) return std::nullopt;
        return it->second;
    }
};

static void section_optional_string_view() {
    std::cout << "== 2. std::string_view tokenizer + std::optional lookup ==\n";
    const std::string corpus = "the cat sat on the mat, the end";   // ONE owning buffer
    const auto toks = split(corpus, " ,");                          // views into corpus; no copies
    Vocab vocab;
    for (auto t : toks) vocab.add(t);                                // string_view is cheap to pass by value
    std::cout << "    " << toks.size() << " tokens, first=\"" << toks[0] << "\" last=\"" << toks.back() << "\"\n";
    std::cout << "    id(the)=" << vocab.lookup("the").value_or(-1) << " id(mat)=" << vocab.lookup("mat").value_or(-1)
              << " id(dog)=" << vocab.lookup("dog").value_or(-1) << " (value_or default)\n";
    if (auto id = vocab.lookup("end")) std::cout << "    'end' present with id " << *id << '\n';
    // RULE: a string_view must not outlive the string it views. `toks` dies with `corpus` here — fine.
}

// ---- 3. std::variant autograd -------------------------------------------------------------------
// A closed set of node kinds. Plain structs, no inheritance, contiguous storage, exhaustive visitors.
struct Leaf { double v; };
struct Add  { int a, b; };
struct Mul  { int a, b; };
struct Tanh { int a; };
using Op = std::variant<Leaf, Add, Mul, Tanh>;

struct Node { Op op; double value = 0.0, grad = 0.0; };

// The `overloaded` idiom: glue lambdas into one visitor. The deduction guide is CTAD for our own type.
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

class Graph {
    std::vector<Node> nodes_;                                        // topological order by construction
public:
    int leaf(double v) { nodes_.push_back({Leaf{v}}); return last(); }
    int add(int a, int b) { nodes_.push_back({Add{a, b}}); return last(); }
    int mul(int a, int b) { nodes_.push_back({Mul{a, b}}); return last(); }
    int tanh(int a) { nodes_.push_back({Tanh{a}}); return last(); }

    void forward() {
        for (auto& n : nodes_)
            n.value = std::visit(overloaded{
                [](const Leaf& l) { return l.v; },
                [&](const Add& o) { return nodes_[o.a].value + nodes_[o.b].value; },
                [&](const Mul& o) { return nodes_[o.a].value * nodes_[o.b].value; },
                [&](const Tanh& o) { return std::tanh(nodes_[o.a].value); }}, n.op);
    }
    void backward(int out) {
        for (auto& n : nodes_) n.grad = 0.0;
        nodes_[static_cast<std::size_t>(out)].grad = 1.0;
        for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {  // reverse topological order
            const double g = it->grad;
            std::visit(overloaded{
                [](const Leaf&) {},
                [&](const Add& o) { nodes_[o.a].grad += g; nodes_[o.b].grad += g; },
                [&](const Mul& o) { nodes_[o.a].grad += g * nodes_[o.b].value; nodes_[o.b].grad += g * nodes_[o.a].value; },
                [&](const Tanh& o) { const double t = it->value; nodes_[o.a].grad += g * (1.0 - t * t); }}, it->op);
        }
    }
    double value(int i) const { return nodes_[static_cast<std::size_t>(i)].value; }
    double grad(int i) const { return nodes_[static_cast<std::size_t>(i)].grad; }
    void set_leaf(int i, double v) { std::get<Leaf>(nodes_[static_cast<std::size_t>(i)].op).v = v; }   // throws bad_variant_access if not a Leaf
    const char* kind(int i) const {
        return std::visit(overloaded{[](const Leaf&) { return "leaf"; }, [](const Add&) { return "add"; },
                                     [](const Mul&) { return "mul"; }, [](const Tanh&) { return "tanh"; }},
                          nodes_[static_cast<std::size_t>(i)].op);
    }
    std::size_t size() const { return nodes_.size(); }
private:
    int last() const { return static_cast<int>(nodes_.size()) - 1; }
};

static void section_variant() {
    std::cout << "== 3. std::variant autograd: f(x,y) = tanh(x*y + x*x) ==\n";
    Graph g;
    const int x = g.leaf(0.5), y = g.leaf(-1.2);
    const int f = g.tanh(g.add(g.mul(x, y), g.mul(x, x)));
    g.forward();
    g.backward(f);
    std::cout << "    kinds:";
    for (std::size_t i = 0; i < g.size(); ++i) std::cout << ' ' << g.kind(static_cast<int>(i));
    std::cout << "\n    f = " << g.value(f) << "  df/dx = " << g.grad(x) << "  df/dy = " << g.grad(y) << '\n';

    // Check against central finite differences — the test every autograd needs (Chapter 11).
    const double h = 1e-6;
    auto eval_at = [&](double xv) { g.set_leaf(x, xv); g.forward(); return g.value(f); };
    const double numeric = (eval_at(0.5 + h) - eval_at(0.5 - h)) / (2 * h);
    eval_at(0.5);
    std::cout << "    numeric df/dx = " << numeric << "  |diff| = " << std::fabs(numeric - g.grad(x)) << '\n';

    Op op = Mul{0, 1};
    std::cout << "    index()=" << op.index() << " holds<Mul>=" << std::holds_alternative<Mul>(op)
              << " get_if<Add>=" << (std::get_if<Add>(&op) == nullptr ? "nullptr" : "ptr") << '\n';
}

// ---- 4. Idioms -------------------------------------------------------------------------------
enum class Activation { ReLU, Tanh, Sigmoid };                       // scoped, no implicit int conversion
using Vec = std::vector<double>;                                     // alias over typedef
template <class T> using Grid = std::vector<std::vector<T>>;         // alias template

static const char* name(Activation a) {
    switch (a) {                                                     // no default: -Wall warns on a missing case
        case Activation::ReLU: return "relu";
        case Activation::Tanh: return "tanh";
        case Activation::Sigmoid: return "sigmoid";
    }
    return "?";
}

// Strong types: Matrix(Rows{3}, Cols{4}) reads like the math; Matrix(Cols{4}, Rows{3}) does not compile.
struct Rows { std::size_t v; };
struct Cols { std::size_t v; };

class Matrix {                                                       // Rule of Zero: all members are RAII
    std::size_t rows_ = 0, cols_ = 0;                                // initialize everything
    std::vector<double> data_;
public:
    Matrix() = default;
    Matrix(Rows r, Cols c) : rows_(r.v), cols_(c.v), data_(r.v * c.v, 0.0) {}
    explicit Matrix(std::size_t n) : Matrix(Rows{n}, Cols{n}) {}     // explicit: `Matrix m = 5;` is an error
    std::size_t rows() const noexcept { return rows_; }
    std::size_t cols() const noexcept { return cols_; }
    double& operator()(std::size_t i, std::size_t j) noexcept { return data_[i * cols_ + j]; }
    double operator()(std::size_t i, std::size_t j) const noexcept { return data_[i * cols_ + j]; }
    [[nodiscard]] Matrix transpose() const {                         // `m.transpose();` alone -> warning
        Matrix t(Rows{cols_}, Cols{rows_});
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j) t(j, i) = (*this)(i, j);
        return t;
    }
};
static_assert(std::is_nothrow_move_constructible_v<Matrix>);        // free with Rule of Zero
static_assert(std::is_copy_constructible_v<Matrix>);

static void section_idioms() {
    std::cout << "== 4. idioms: enum class, strong types, explicit, Rule of Zero ==\n";
    [[maybe_unused]] constexpr int kDebugLevel = 0;                  // no warning even if unused
    Matrix m(Rows{2}, Cols{3});
    m(0, 2) = 7.0;
    const Matrix t = m.transpose();
    std::cout << "    m " << m.rows() << 'x' << m.cols() << " -> t " << t.rows() << 'x' << t.cols() << ", t(2,0)=" << t(2, 0) << '\n';
    // Matrix bad(Cols{3}, Rows{2});   // error: no matching constructor  <- argument order bug caught
    // Matrix m2 = 5;                  // error: explicit constructor
    // m.transpose();                  // warning: ignoring return value of [[nodiscard]]
    Grid<int> grid(2, std::vector<int>(3, 1));
    Vec v{1.0, 2.0};
    std::cout << "    Activation::Tanh -> " << name(Activation::Tanh) << "; sizeof(Activation)=" << sizeof(Activation)
              << "; Grid<int> " << grid.size() << 'x' << grid[0].size() << "; Vec size " << v.size() << '\n';
    std::cout << "    constants: G=" << phys::constants::kG << " pi=" << phys::constants::kPi << '\n';
}

// ---- 5. std::function vs template ----------------------------------------------------------------
template <class F>
static double integrate_t(F f, double a, double b, int n) {         // f inlined into the loop
    const double h = (b - a) / n;
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += f(a + (i + 0.5) * h);
    return s * h;
}
static double integrate_f(const std::function<double(double)>& f, double a, double b, int n) {   // indirect call per element
    const double h = (b - a) / n;
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += f(a + (i + 0.5) * h);
    return s * h;
}

struct Optimizer {                                                   // std::function belongs HERE: stored, called rarely
    std::function<double(int)> lr_schedule = [](int step) { return 0.1 / (1.0 + 0.01 * step); };
};

static void section_callbacks() {
    std::cout << "== 5. std::function vs template callback ==\n";
    auto f = [](double x) { return std::sin(x) * std::exp(-x); };
    const double exact = 0.5 * (1 - std::exp(-5.0) * (std::sin(5.0) + std::cos(5.0)));
    std::cout << "    template:      " << integrate_t(f, 0, 5, 1'000'000) << "\n    std::function: "
              << integrate_f(f, 0, 5, 1'000'000) << "\n    exact:         " << exact << '\n';
    Optimizer opt;
    std::cout << "    stored lr_schedule(100) = " << opt.lr_schedule(100) << "  (fine: one call per step, not per element)\n";
}

// ---- 6. Brief patterns ---------------------------------------------------------------------------
// pimpl: the header would show only this; Impl lives in the .cpp. Here both are in one file.
class Solver {
public:
    explicit Solver(std::size_t n);
    ~Solver();                                                       // must be defined where Impl is complete
    Solver(Solver&&) noexcept;
    Solver& operator=(Solver&&) noexcept;
    double step();
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};
struct Solver::Impl { std::vector<double> state; int calls = 0; };
Solver::Solver(std::size_t n) : p_(std::make_unique<Impl>()) { p_->state.assign(n, 1.0); }
Solver::~Solver() = default;
Solver::Solver(Solver&&) noexcept = default;
Solver& Solver::operator=(Solver&&) noexcept = default;
double Solver::step() { ++p_->calls; for (double& s : p_->state) s *= 0.5; return p_->state[0]; }

// CRTP: compile-time polymorphism. norm() written once, no virtual call.
template <class Derived>
struct VecOps {
    double norm() const { const auto& d = static_cast<const Derived&>(*this); return std::sqrt(d.dot(d)); }
};
struct Vec3 : VecOps<Vec3> {
    double x, y, z;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}   // (C++17 aggregate init with a base needs {{}, ...})
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
};

// Tag dispatch: pick an overload with an empty struct. (if constexpr often replaces this today.)
struct RowMajorTag {};
struct ColMajorTag {};
static std::size_t index_impl(std::size_t i, std::size_t j, std::size_t rows, std::size_t cols, RowMajorTag) { (void)rows; return i * cols + j; }
static std::size_t index_impl(std::size_t i, std::size_t j, std::size_t rows, std::size_t cols, ColMajorTag) { (void)cols; return j * rows + i; }
template <class Layout> std::size_t index(std::size_t i, std::size_t j, std::size_t rows, std::size_t cols) { return index_impl(i, j, rows, cols, Layout{}); }

// Type erasure: store ANY type with .forward(double) behind one interface, no inheritance required of it.
class Layer {
    struct Concept { virtual ~Concept() = default; virtual double forward(double) const = 0; };
    template <class T> struct Model final : Concept {
        T obj;
        explicit Model(T o) : obj(std::move(o)) {}
        double forward(double x) const override { return obj.forward(x); }
    };
    std::unique_ptr<Concept> p_;
public:
    template <class T> Layer(T obj) : p_(std::make_unique<Model<T>>(std::move(obj))) {}   // intentionally implicit
    double forward(double x) const { return p_->forward(x); }
};
struct Scale { double k; double forward(double x) const { return k * x; } };               // no base class
struct ReLU { double forward(double x) const { return x > 0 ? x : 0; } };

static void section_patterns() {
    std::cout << "== 6. pimpl / CRTP / tag dispatch / type erasure ==\n";
    Solver s(4);
    s.step();
    std::cout << "    pimpl Solver: state[0] after 2 steps = " << s.step() << '\n';
    Vec3 v{3, 4, 12};
    std::cout << "    CRTP Vec3 norm = " << v.norm() << '\n';
    std::cout << "    tag dispatch (1,2) in 3x4: row-major " << index<RowMajorTag>(1, 2, 3, 4)
              << ", col-major " << index<ColMajorTag>(1, 2, 3, 4) << '\n';
    std::vector<Layer> net;
    net.emplace_back(Scale{2.0});
    net.emplace_back(ReLU{});
    net.emplace_back(Scale{-1.0});
    double x = -1.5, y = 1.5;
    for (const auto& l : net) { x = l.forward(x); y = l.forward(y); }
    std::cout << "    type-erased net(-1.5) = " << x << ", net(1.5) = " << y << '\n';
}

// ---- 7. std::filesystem ------------------------------------------------------------------------
namespace fs = std::filesystem;

static void section_filesystem() {
    std::cout << "== 7. std::filesystem ==\n";
    std::error_code ec;
    const fs::path root = fs::temp_directory_path(ec) / "cpp13_demo_checkpoints";
    if (ec) { std::cout << "    no temp dir: " << ec.message() << '\n'; return; }
    fs::create_directories(root / "run1", ec);
    for (int step : {100, 200, 300}) {
        char name[32];
        std::snprintf(name, sizeof name, "step_%06d.bin", step);
        std::ofstream(root / "run1" / name) << std::string(static_cast<std::size_t>(step / 50), 'x');
    }
    std::vector<fs::path> bins;
    for (const auto& entry : fs::directory_iterator(root / "run1"))
        if (entry.path().extension() == ".bin") bins.push_back(entry.path());
    std::sort(bins.begin(), bins.end());
    for (const auto& p : bins)
        std::cout << "    " << p.filename().string() << "  stem=" << p.stem().string() << "  " << fs::file_size(p) << " bytes\n";
    std::cout << "    exists(missing.bin) = " << fs::exists(root / "missing.bin") << "  parent=" << (root / "run1").parent_path().filename().string() << '\n';
    const auto removed = fs::remove_all(root, ec);                   // non-throwing overload
    std::cout << "    cleaned up " << removed << " entries" << (ec ? " (error: " + ec.message() + ")" : "") << '\n';
}

int main() {
    std::cout << std::boolalpha;
    section_bindings();
    section_optional_string_view();
    section_variant();
    section_idioms();
    section_callbacks();
    section_patterns();
    section_filesystem();
    std::cout << "== 8. the subset ==\n"
                 "    vector/array/string_view, const&, RAII + Rule of Zero, enum class, constexpr,\n"
                 "    optional/variant, templates + if constexpr, exceptions at boundaries, assert in kernels.\n";
    return 0;
}
