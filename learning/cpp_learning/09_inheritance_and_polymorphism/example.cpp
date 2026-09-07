// Chapter 09 — Inheritance and Polymorphism.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo
//
// What this program demonstrates:
//   1. Construction/destruction order of base, members, derived.
//   2. Static vs dynamic dispatch; the vptr (sizeof grows by one pointer); `override`; `final`;
//      `virtual ~Base()` and what it makes correct; calling Base::f() from an override.
//   3. The running example: abstract `Layer` with virtual forward/backward/step, `Linear`, `ReLU`,
//      `Sigmoid`, and `Sequential` holding std::vector<std::unique_ptr<Layer>> — trained for a few
//      hundred steps on XOR so you can see the loss fall.
//   4. `Optimizer` hierarchy (SGD, Adam) with a `protected` member.
//   5. `dynamic_cast` / `typeid` (and the virtual function that replaces the cast).
//   6. Composition: one `Activation` class holding two std::function members.
//   7. Static polymorphism: a template RK4 step and a CRTP mixin — zero indirect calls.
//   8. std::variant + std::visit as closed-set polymorphism for autograd ops.
//   Slicing and multiple inheritance are shown as commented-out code with the reason.

#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

// =============================================================================================
// Minimal Matrix (row-major, Rule of Zero). Chapter 06 operators, only what we need here.
// =============================================================================================
class Matrix {
public:
    Matrix(std::size_t r = 0, std::size_t c = 0, double fill = 0.0) : rows_(r), cols_(c), data_(r * c, fill) {}
    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    double& operator()(std::size_t i, std::size_t j) { return data_[i * cols_ + j]; }
    const double& operator()(std::size_t i, std::size_t j) const { return data_[i * cols_ + j]; }
    std::vector<double>& data() { return data_; }
    const std::vector<double>& data() const { return data_; }
    Matrix& operator-=(const Matrix& o) { for (std::size_t i = 0; i < data_.size(); ++i) data_[i] -= o.data_[i]; return *this; }
private:
    std::size_t rows_, cols_;
    std::vector<double> data_;
};
Matrix operator*(const Matrix& a, const Matrix& b) {           // matmul
    Matrix c(a.rows(), b.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t k = 0; k < a.cols(); ++k)
            for (std::size_t j = 0; j < b.cols(); ++j) c(i, j) += a(i, k) * b(k, j);
    return c;
}
Matrix operator*(double s, Matrix m) { for (double& x : m.data()) x *= s; return m; }
Matrix transpose(const Matrix& m) {
    Matrix t(m.cols(), m.rows());
    for (std::size_t i = 0; i < m.rows(); ++i) for (std::size_t j = 0; j < m.cols(); ++j) t(j, i) = m(i, j);
    return t;
}
Matrix hadamard(const Matrix& a, const Matrix& b) {
    Matrix c = a;
    for (std::size_t i = 0; i < c.data().size(); ++i) c.data()[i] *= b.data()[i];
    return c;
}
Matrix add_rowvec(Matrix m, const Matrix& b) {                 // broadcast 1xN over rows
    for (std::size_t i = 0; i < m.rows(); ++i) for (std::size_t j = 0; j < m.cols(); ++j) m(i, j) += b(0, j);
    return m;
}
Matrix sum_rows(const Matrix& m) {                             // -> 1xN
    Matrix s(1, m.cols());
    for (std::size_t i = 0; i < m.rows(); ++i) for (std::size_t j = 0; j < m.cols(); ++j) s(0, j) += m(i, j);
    return s;
}
template <typename F> Matrix map(const Matrix& m, F f) {
    Matrix r = m;
    for (double& x : r.data()) x = f(x);
    return r;
}

// =============================================================================================
// 1. Construction / destruction order
// =============================================================================================
struct Base {
    Base() { std::cout << "  Base()  "; }
    virtual ~Base() { std::cout << "~Base()\n"; }             // virtual: see section 2
    virtual std::string who() const { return "Base"; }
};
struct Member {
    Member() { std::cout << "Member()  "; }
    ~Member() { std::cout << "~Member()  "; }
};
struct Derived : Base {
    Member m;
    Derived() { std::cout << "Derived()\n"; }
    ~Derived() override { std::cout << "  ~Derived()  "; }
    std::string who() const override { return "Derived (extends " + Base::who() + ")"; }   // Base::who(): non-virtual, qualified
};
struct Final final : Derived {                                 // nobody can derive from Final
    std::string who() const override { return "Final"; }
};

// Non-virtual counterpart, to show static dispatch and the size difference.
struct Plain { double x; void f() const {} };
struct PlainV { double x; virtual void f() const {} virtual ~PlainV() = default; };

// =============================================================================================
// 3. Layer hierarchy
// =============================================================================================
class Layer {
public:
    virtual ~Layer() = default;                                // MANDATORY: deleted through Layer*
    virtual Matrix forward(const Matrix& x) = 0;               // pure: Layer is abstract
    virtual Matrix backward(const Matrix& grad_out) = 0;       // returns grad wrt input, caches param grads
    virtual void step(double /*lr*/) {}                        // default: no parameters
    virtual std::size_t param_count() const { return 0; }     // the virtual that replaces dynamic_cast
    virtual std::string name() const = 0;
};

class Linear : public Layer {
    Matrix W_, b_, x_cache_, dW_, db_;
public:
    Linear(std::size_t in, std::size_t out, std::mt19937_64& rng) : W_(in, out), b_(1, out) {
        std::normal_distribution<double> N(0.0, std::sqrt(2.0 / in));
        for (double& w : W_.data()) w = N(rng);
    }
    Matrix forward(const Matrix& x) override { x_cache_ = x; return add_rowvec(x * W_, b_); }
    Matrix backward(const Matrix& g) override {
        dW_ = transpose(x_cache_) * g;
        db_ = sum_rows(g);
        return g * transpose(W_);
    }
    void step(double lr) override { W_ -= lr * dW_; b_ -= lr * db_; }
    std::size_t param_count() const override { return W_.data().size() + b_.data().size(); }
    std::string name() const override { return "Linear(" + std::to_string(W_.rows()) + "->" + std::to_string(W_.cols()) + ")"; }
};

class ReLU final : public Layer {                              // final: calls through ReLU& can be direct
    Matrix mask_;
public:
    Matrix forward(const Matrix& x) override { mask_ = map(x, [](double v) { return v > 0 ? 1.0 : 0.0; }); return hadamard(x, mask_); }
    Matrix backward(const Matrix& g) override { return hadamard(g, mask_); }
    std::string name() const override { return "ReLU"; }
};

class Sigmoid : public Layer {
    Matrix s_;
public:
    Matrix forward(const Matrix& x) override { s_ = map(x, [](double v) { return 1.0 / (1.0 + std::exp(-v)); }); return s_; }
    Matrix backward(const Matrix& g) override { return hadamard(g, map(s_, [](double s) { return s * (1 - s); })); }
    std::string name() const override { return "Sigmoid"; }
};

// A Layer made of Layers (Composite). Owns them via unique_ptr; move-only as a consequence.
class Sequential : public Layer {
    std::vector<std::unique_ptr<Layer>> layers_;
public:
    void add(std::unique_ptr<Layer> l) { layers_.push_back(std::move(l)); }     // sink parameter
    Matrix forward(const Matrix& x) override {
        Matrix h = x;
        for (auto& l : layers_) h = l->forward(h);            // one virtual call per LAYER — cheap
        return h;
    }
    Matrix backward(const Matrix& g) override {
        Matrix d = g;
        for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) d = (*it)->backward(d);
        return d;
    }
    void step(double lr) override { for (auto& l : layers_) l->step(lr); }
    std::size_t param_count() const override {
        std::size_t n = 0;
        for (const auto& l : layers_) n += l->param_count();
        return n;
    }
    std::string name() const override { return "Sequential"; }
    const std::vector<std::unique_ptr<Layer>>& layers() const { return layers_; }
};

// =============================================================================================
// 4. Optimizer hierarchy
// =============================================================================================
class Optimizer {
protected:
    double lr_;                                                // derived classes need it; outsiders do not
public:
    explicit Optimizer(double lr) : lr_(lr) {}
    virtual ~Optimizer() = default;
    virtual void update(std::vector<double>& p, const std::vector<double>& g) = 0;
    virtual std::string name() const = 0;
};
class SGD : public Optimizer {
public:
    using Optimizer::Optimizer;                                // inherit constructor
    void update(std::vector<double>& p, const std::vector<double>& g) override {
        for (std::size_t i = 0; i < p.size(); ++i) p[i] -= lr_ * g[i];
    }
    std::string name() const override { return "SGD"; }
};
class Adam : public Optimizer {
    double b1_, b2_, eps_;
    long t_ = 0;
    std::vector<double> m_, v_;                                // single-parameter version for brevity
public:
    Adam(double lr, double b1 = 0.9, double b2 = 0.999, double eps = 1e-8) : Optimizer(lr), b1_(b1), b2_(b2), eps_(eps) {}
    void update(std::vector<double>& p, const std::vector<double>& g) override {
        if (m_.empty()) { m_.assign(p.size(), 0.0); v_.assign(p.size(), 0.0); }
        ++t_;
        for (std::size_t i = 0; i < p.size(); ++i) {
            m_[i] = b1_ * m_[i] + (1 - b1_) * g[i];
            v_[i] = b2_ * v_[i] + (1 - b2_) * g[i] * g[i];
            double mhat = m_[i] / (1 - std::pow(b1_, double(t_)));
            double vhat = v_[i] / (1 - std::pow(b2_, double(t_)));
            p[i] -= lr_ * mhat / (std::sqrt(vhat) + eps_);
        }
    }
    std::string name() const override { return "Adam"; }
};

// =============================================================================================
// 6. Composition: the varying part is a VALUE (two callables), not a type.
// =============================================================================================
class Activation : public Layer {
    std::function<double(double)> f_, df_;
    Matrix cache_;
public:
    Activation(std::function<double(double)> f, std::function<double(double)> df) : f_(std::move(f)), df_(std::move(df)) {}
    Matrix forward(const Matrix& x) override { cache_ = x; return map(x, f_); }     // std::function call per element:
    Matrix backward(const Matrix& g) override { return hadamard(g, map(cache_, df_)); } // fine here, slow in a hot kernel
    std::string name() const override { return "Activation(fn)"; }
};

// =============================================================================================
// 7. Static polymorphism: template RHS for RK4, and a CRTP mixin.
// =============================================================================================
struct State { double x, v; };
template <typename RHS>
void rk4_step(RHS f, double& t, State& y, double h) {          // f inlined; zero indirect calls
    auto add = [](State a, State b, double s) { return State{a.x + s * b.x, a.v + s * b.v}; };
    State k1 = f(t, y);
    State k2 = f(t + h / 2, add(y, k1, h / 2));
    State k3 = f(t + h / 2, add(y, k2, h / 2));
    State k4 = f(t + h, add(y, k3, h));
    y.x += h / 6 * (k1.x + 2 * k2.x + 2 * k3.x + k4.x);
    y.v += h / 6 * (k1.v + 2 * k2.v + 2 * k3.v + k4.v);
    t += h;
}

template <typename D>
struct VecOps {                                                // CRTP: shared code, static dispatch
    double norm() const { return std::sqrt(self().dot(self())); }
    D normalized() const { return self().scaled(1.0 / norm()); }
private:
    const D& self() const { return static_cast<const D&>(*this); }
};
struct V2 : VecOps<V2> {
    double x, y;
    double dot(const V2& o) const { return x * o.x + y * o.y; }
    V2 scaled(double s) const { return {{}, x * s, y * s}; }
};

// =============================================================================================
// 8. std::variant + std::visit — closed set of autograd ops
// =============================================================================================
struct Leaf {}; struct Add {}; struct Mul {}; struct Tanh {};
using Op = std::variant<Leaf, Add, Mul, Tanh>;
struct Node {
    double data, grad = 0;
    Op op;
    std::vector<std::shared_ptr<Node>> children;
};
using NodePtr = std::shared_ptr<Node>;
NodePtr leaf(double v) { return std::make_shared<Node>(Node{v, 0, Leaf{}, {}}); }
NodePtr add(NodePtr a, NodePtr b) { return std::make_shared<Node>(Node{a->data + b->data, 0, Add{}, {a, b}}); }
NodePtr mul(NodePtr a, NodePtr b) { return std::make_shared<Node>(Node{a->data * b->data, 0, Mul{}, {a, b}}); }
NodePtr tanh_(NodePtr a) { return std::make_shared<Node>(Node{std::tanh(a->data), 0, Tanh{}, {a}}); }

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };   // overloaded-lambdas idiom
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void local_backward(Node& n) {
    std::visit(overloaded{
        [](Leaf) {},
        [&](Add) { n.children[0]->grad += n.grad; n.children[1]->grad += n.grad; },
        [&](Mul) { n.children[0]->grad += n.children[1]->data * n.grad;
                   n.children[1]->grad += n.children[0]->data * n.grad; },
        [&](Tanh) { n.children[0]->grad += (1 - n.data * n.data) * n.grad; },
        // Add a new alternative to Op without a lambda here -> compile error. Closed set, enforced.
    }, n.op);
}
const char* op_name(const Op& op) {
    return std::visit(overloaded{[](Leaf) { return "leaf"; }, [](Add) { return "+"; },
                                 [](Mul) { return "*"; }, [](Tanh) { return "tanh"; }}, op);
}
void topo(const NodePtr& n, std::vector<Node*>& order, std::vector<Node*>& seen) {
    for (Node* s : seen) if (s == n.get()) return;
    seen.push_back(n.get());
    for (const auto& c : n->children) topo(c, order, seen);
    order.push_back(n.get());
}

// =============================================================================================
int main() {
    std::cout << std::fixed << std::setprecision(4);

    std::cout << "=== 1. Construction / destruction order ===\n";
    {
        Derived d;                                             // Base() Member() Derived()
        std::cout << "  (scope ends)\n";
    }                                                          // ~Derived() ~Member() ~Base()

    std::cout << "\n=== 2. Dispatch, vptr, virtual destructor, Base::f() ===\n";
    {
        Final f_obj;
        Base& as_base = f_obj;
        std::cout << "  through Base&: who() = " << as_base.who() << "   (dynamic dispatch)\n";
        Derived& as_derived = f_obj;
        std::cout << "  Derived::who() explicitly: " << as_derived.Derived::who() << "   (qualified = static)\n";
        std::cout << "  sizeof(Plain)=" << sizeof(Plain) << "  sizeof(PlainV)=" << sizeof(PlainV) << "  (the vptr)\n";
        std::unique_ptr<Base> owned = std::make_unique<Derived>();   // deleted through Base*: needs virtual ~Base
        std::cout << "  unique_ptr<Base> holding Derived; destroying it runs the full chain:\n";
        owned.reset();
        // struct Oops : Final {};   // ERROR: Final is final
        // Base sliced = f_obj;      // compiles (Base is concrete) but copies ONLY the Base part — slicing.
        std::cout << "  (Final's destructor chain follows)\n";
    }

    std::cout << "\n=== 3. Layer hierarchy: train XOR for 3000 full-batch steps ===\n";
    {
        std::mt19937_64 rng(7);
        Sequential net;
        net.add(std::make_unique<Linear>(2, 8, rng));
        net.add(std::make_unique<ReLU>());
        net.add(std::make_unique<Linear>(8, 1, rng));
        net.add(std::make_unique<Sigmoid>());
        std::cout << "  layers:";
        for (const auto& l : net.layers()) std::cout << ' ' << l->name();
        std::cout << "\n  total params (virtual param_count, no casts): " << net.param_count() << '\n';

        Matrix X(4, 2), Y(4, 1);
        const double xs[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        for (std::size_t i = 0; i < 4; ++i) { X(i, 0) = xs[i][0]; X(i, 1) = xs[i][1]; Y(i, 0) = double(int(xs[i][0]) ^ int(xs[i][1])); }

        for (int epoch = 0; epoch <= 3000; ++epoch) {
            Matrix P = net.forward(X);
            double loss = 0;
            Matrix dP(4, 1);
            for (std::size_t i = 0; i < 4; ++i) { double e = P(i, 0) - Y(i, 0); loss += e * e / 4; dP(i, 0) = 2 * e / 4; }
            net.backward(dP);
            net.step(0.5);
            if (epoch % 1000 == 0) std::cout << "  epoch " << std::setw(4) << epoch << "  mse = " << loss << '\n';
        }
        Matrix P = net.forward(X);
        std::cout << "  predictions: ";
        for (std::size_t i = 0; i < 4; ++i) std::cout << P(i, 0) << (i < 3 ? ", " : "\n");

        // 5. dynamic_cast / typeid — works, but param_count() above is the better design.
        std::cout << "  dynamic_cast survey:";
        for (const auto& l : net.layers()) {
            const Layer& ref = *l;                             // typeid on a plain reference (no side effects)
            if (auto* lin = dynamic_cast<Linear*>(l.get())) std::cout << " [Linear " << lin->param_count() << "]";
            else std::cout << " [" << (typeid(ref) == typeid(ReLU) ? "ReLU" : "other") << "]";
        }
        std::cout << '\n';

        // 6. Composition: swap the ReLU class for one Activation object built from two lambdas.
        Sequential net2;
        net2.add(std::make_unique<Linear>(2, 3, rng));
        net2.add(std::make_unique<Activation>([](double x) { return std::tanh(x); },
                                              [](double x) { double t = std::tanh(x); return 1 - t * t; }));
        std::cout << "  composed net: " << net2.layers()[0]->name() << " -> " << net2.layers()[1]->name()
                  << ", forward output shape " << net2.forward(X).rows() << "x" << net2.forward(X).cols() << '\n';
    }

    std::cout << "\n=== 4. Optimizer hierarchy on f(x,y) = (1-x)^2 + 100(y-x^2)^2 ===\n";
    {
        std::vector<std::unique_ptr<Optimizer>> opts;
        opts.push_back(std::make_unique<SGD>(1e-3));
        opts.push_back(std::make_unique<Adam>(1e-2));
        for (auto& opt : opts) {
            std::vector<double> p = {-1.5, 2.0};
            for (int it = 0; it < 20000; ++it) {
                double x = p[0], y = p[1];
                std::vector<double> g = {-2 * (1 - x) - 400 * x * (y - x * x), 200 * (y - x * x)};
                opt->update(p, g);
            }
            double f = (1 - p[0]) * (1 - p[0]) + 100 * (p[1] - p[0] * p[0]) * (p[1] - p[0] * p[0]);
            std::cout << "  " << std::setw(4) << opt->name() << " after 20000 steps: (" << p[0] << ", " << p[1] << ")  f=" << f << '\n';
        }
    }

    std::cout << "\n=== 7. Static polymorphism: template RK4 + CRTP ===\n";
    {
        const double k = 4.0;                                  // harmonic oscillator x'' = -k x
        auto spring = [k](double, State s) { return State{s.v, -k * s.x}; };
        State y{1.0, 0.0};
        double t = 0, h = 1e-3;
        for (int i = 0; i < 1000; ++i) rk4_step(spring, t, y, h);
        std::cout << "  oscillator at t=1: x=" << y.x << " (exact cos(2)=" << std::cos(2.0) << ")\n";
        V2 v{{}, 3.0, 4.0};
        std::cout << "  CRTP: norm=" << v.norm() << "  normalized=(" << v.normalized().x << ", " << v.normalized().y << ")\n";
    }

    std::cout << "\n=== 8. std::variant autograd: L = tanh(a*b + c) ===\n";
    {
        NodePtr a = leaf(2.0), b = leaf(-3.0), c = leaf(10.0);
        NodePtr L = tanh_(add(mul(a, b), c));
        std::vector<Node*> order, seen;
        topo(L, order, seen);
        L->grad = 1.0;
        for (auto it = order.rbegin(); it != order.rend(); ++it) local_backward(**it);
        std::cout << "  L = " << L->data << "  ops in topo order:";
        for (Node* n : order) std::cout << ' ' << op_name(n->op);
        double sech2 = 1 - L->data * L->data;
        std::cout << "\n  dL/da=" << a->grad << " (expect b*sech2=" << -3 * sech2 << ")  dL/db=" << b->grad
                  << "  dL/dc=" << c->grad << " (expect " << sech2 << ")\n";
        std::cout << "  sizeof(Op)=" << sizeof(Op) << " bytes, stored inline; holds_alternative<Tanh>(L->op)="
                  << std::holds_alternative<Tanh>(L->op) << '\n';
    }
    return 0;
}
