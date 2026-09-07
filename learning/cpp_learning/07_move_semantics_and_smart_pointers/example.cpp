// Chapter 07 — Move Semantics and Smart Pointers.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo
//
// What this program demonstrates:
//   1. A Rule-of-Five `Buffer` (raw owning pointer) with loud special members, so you can SEE
//      when copies and moves happen: return-by-value, std::move, vector reallocation, swap.
//   2. std::move is a cast: moving from const copies; moved-from objects are empty-but-valid.
//   3. noexcept moves and why std::vector relocation depends on them.
//   4. A Rule-of-Zero `Matrix` (std::vector inside) that gets all of this for free.
//   5. std::unique_ptr (make_unique, move-only, arrays, custom deleter), std::shared_ptr
//      (use_count, make_shared), std::weak_ptr (lock), and a shared_ptr cycle broken by weak_ptr.
//   6. Ownership design: a tiny autograd-style Node graph that frees itself in a cascade.
//   7. std::optional for "maybe a value".
//   8. The C course's Layer vtable struct redone as std::vector<std::unique_ptr<Layer>>.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// =============================================================================================
// 1. Rule of Five, made loud.
// =============================================================================================
class Buffer {
public:
    explicit Buffer(std::size_t n, const char* tag = "buf") : p_(new double[n]()), n_(n), tag_(tag) {
        ++live_allocs;
        std::cout << "  Buffer(" << n << ") alloc         [" << tag_ << "]\n";
    }
    ~Buffer() {
        if (p_) { --live_allocs; std::cout << "  ~Buffer() free            [" << tag_ << "]\n"; }
        else      std::cout << "  ~Buffer() (empty, no-op)  [" << tag_ << "]\n";
        delete[] p_;                      // delete[] nullptr is defined to do nothing
    }
    // copy: allocate and copy — O(n)
    Buffer(const Buffer& o) : p_(new double[o.n_]), n_(o.n_), tag_(o.tag_) {
        ++live_allocs;
        std::copy(o.p_, o.p_ + n_, p_);
        std::cout << "  Buffer(const Buffer&) COPY [" << tag_ << "]\n";
    }
    Buffer& operator=(const Buffer& o) {
        std::cout << "  operator=(const Buffer&) COPY-ASSIGN\n";
        if (this != &o) { Buffer tmp(o); swap(tmp); }      // copy-and-swap; tmp frees our old buffer
        return *this;
    }
    // move: steal the pointer, null the source — O(1), never throws
    Buffer(Buffer&& o) noexcept : p_(o.p_), n_(o.n_), tag_(o.tag_) {
        o.p_ = nullptr;                   // ESSENTIAL — otherwise two destructors free one buffer
        o.n_ = 0;
        std::cout << "  Buffer(Buffer&&) MOVE      [" << tag_ << "]\n";
    }
    Buffer& operator=(Buffer&& o) noexcept {
        std::cout << "  operator=(Buffer&&) MOVE-ASSIGN\n";
        if (this != &o) {
            if (p_) --live_allocs;
            delete[] p_;
            p_ = o.p_; n_ = o.n_; tag_ = o.tag_;
            o.p_ = nullptr; o.n_ = 0;
        }
        return *this;
    }
    void swap(Buffer& o) noexcept { std::swap(p_, o.p_); std::swap(n_, o.n_); std::swap(tag_, o.tag_); }

    std::size_t size() const { return n_; }
    double& operator[](std::size_t i) { return p_[i]; }
    const double& operator[](std::size_t i) const { return p_[i]; }

    static int live_allocs;
private:
    double* p_;
    std::size_t n_;
    const char* tag_;
};
int Buffer::live_allocs = 0;

// Returning a local by value: C++17 elides the move entirely (NRVO) or moves. Never copies.
Buffer make_buffer(std::size_t n) {
    Buffer b(n, "made");
    b[0] = 42.0;
    return b;                              // NOT `return std::move(b);` — that would defeat elision
}

// =============================================================================================
// 4. Rule of Zero: nothing to write. std::vector supplies correct copy/move/destroy.
// =============================================================================================
class Matrix {
public:
    Matrix(std::size_t r = 0, std::size_t c = 0) : rows_(r), cols_(c), data_(r * c) {}
    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    double& operator()(std::size_t i, std::size_t j) { return data_[i * cols_ + j]; }
    const double& operator()(std::size_t i, std::size_t j) const { return data_[i * cols_ + j]; }
    const double* raw() const { return data_.data(); }     // to prove the buffer moved, not copied
private:
    std::size_t rows_, cols_;
    std::vector<double> data_;
};
static_assert(std::is_nothrow_move_constructible<Matrix>::value, "Matrix move must be noexcept");

Matrix identity(std::size_t n) {
    Matrix m(n, n);
    for (std::size_t i = 0; i < n; ++i) m(i, i) = 1.0;
    return m;
}

// =============================================================================================
// 6. Ownership design: autograd-style graph. children = owning, parents = weak observers.
// =============================================================================================
struct Node {
    double data;
    char op;                                              // 'a'/'b'/'c' leaf, '+', '*'
    std::vector<std::shared_ptr<Node>> children;          // keeps operands alive for backward
    std::vector<std::weak_ptr<Node>> parents;             // back-edges must NOT own, or we cycle
    Node(double d, char o) : data(d), op(o) {}
    ~Node() { std::cout << "  ~Node('" << op << "')\n"; }
};
using NodePtr = std::shared_ptr<Node>;

NodePtr leaf(double v, char name) { return std::make_shared<Node>(v, name); }
NodePtr binary(const NodePtr& a, const NodePtr& b, char op) {
    auto out = std::make_shared<Node>(op == '+' ? a->data + b->data : a->data * b->data, op);
    out->children = {a, b};             // shared_ptr copies: use_count of a and b goes up
    a->parents.push_back(out);          // implicit shared_ptr -> weak_ptr; use_count unchanged
    b->parents.push_back(out);
    return out;
}

// =============================================================================================
// 8. The C course's Layer (struct of function pointers + void* state) becomes this.
// =============================================================================================
struct Layer {
    virtual ~Layer() = default;          // MANDATORY: unique_ptr<Layer> deletes through Layer*
    virtual std::vector<double> forward(const std::vector<double>& x) const = 0;
    virtual std::string name() const = 0;
};
struct Scale : Layer {
    double k;
    explicit Scale(double k_) : k(k_) {}
    std::vector<double> forward(const std::vector<double>& x) const override {
        std::vector<double> y = x;
        for (double& v : y) v *= k;
        return y;
    }
    std::string name() const override { return "Scale(" + std::to_string(k) + ")"; }
};
struct Shift : Layer {
    double b;
    explicit Shift(double b_) : b(b_) {}
    std::vector<double> forward(const std::vector<double>& x) const override {
        std::vector<double> y = x;
        for (double& v : y) v += b;
        return y;
    }
    std::string name() const override { return "Shift(" + std::to_string(b) + ")"; }
};

// 7. optional: "maybe a value", not a pointer, not NAN.
std::optional<double> safe_sqrt(double x) {
    if (x < 0) return std::nullopt;
    return std::sqrt(x);
}

// Custom deleter for a C resource (stateless functor: unique_ptr stays one pointer wide).
struct FileCloser {
    void operator()(std::FILE* f) const { if (f) std::fclose(f); }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

// =============================================================================================
int main() {
    std::cout << "=== 1. Return by value: no copy ===\n";
    {
        Buffer b = make_buffer(4);          // elided: `b` IS the local inside make_buffer
        std::cout << "  b[0] = " << b[0] << ", size " << b.size() << '\n';
    }

    std::cout << "\n=== 2. Copy vs move vs std::move-on-const ===\n";
    {
        Buffer a(3, "a");
        Buffer c = a;                       // lvalue source -> COPY
        Buffer m = std::move(a);            // xvalue source -> MOVE; `a` is now empty but valid
        std::cout << "  after move: a.size()=" << a.size() << " m.size()=" << m.size() << '\n';
        const Buffer k(2, "k");
        Buffer from_const = std::move(k);   // COPY! cannot steal from const. Silent perf bug.
        (void)c; (void)from_const;
        std::cout << "  (scope ends: watch which destructors free and which are no-ops)\n";
    }
    std::cout << "  live allocations now: " << Buffer::live_allocs << " (must be 0)\n";

    std::cout << "\n=== 3. vector reallocation moves noexcept types ===\n";
    {
        std::vector<Buffer> v;              // no reserve: growth relocates existing elements
        v.push_back(Buffer(1, "v0"));       // temporary -> moved in
        v.push_back(Buffer(1, "v1"));       // realloc: v0 moved to new storage
        v.push_back(Buffer(1, "v2"));       // realloc: v0, v1 moved
        std::cout << "  vector holds " << v.size() << " buffers; live allocs = " << Buffer::live_allocs << '\n';
        std::cout << "  is_nothrow_move_constructible<Buffer>: "
                  << std::is_nothrow_move_constructible<Buffer>::value << '\n';
    }
    std::cout << "  live allocations now: " << Buffer::live_allocs << '\n';

    std::cout << "\n=== 3b. std::swap = three moves, size-independent ===\n";
    {
        Buffer x(5, "x"), y(7, "y");
        std::swap(x, y);
        std::cout << "  after swap: x.size()=" << x.size() << " y.size()=" << y.size() << '\n';
    }

    std::cout << "\n=== 4. Rule of Zero Matrix: same behaviour, zero code ===\n";
    {
        Matrix I = identity(3);
        const double* before = I.raw();
        Matrix J = std::move(I);            // vector's move: pointer stolen
        std::cout << "  buffer address unchanged after move: " << (J.raw() == before ? "yes" : "no")
                  << ";  I.rows() (moved-from, unspecified but valid) = " << I.rows() << '\n';
        std::vector<Matrix> mats;
        for (int i = 0; i < 4; ++i) mats.push_back(identity(2));   // moves, never copies
        std::cout << "  pushed " << mats.size() << " matrices by value\n";
    }

    std::cout << "\n=== 5. unique_ptr ===\n";
    {
        auto p = std::make_unique<Matrix>(2, 2);       // no naked new
        (*p)(0, 0) = 3.0;
        std::cout << "  p->rows()=" << p->rows() << "  (*p)(0,0)=" << (*p)(0, 0)
                  << "  sizeof(unique_ptr)=" << sizeof(p) << " == sizeof(Matrix*)=" << sizeof(Matrix*) << '\n';
        // auto q = p;                                 // ERROR: copy is deleted
        auto q = std::move(p);                         // ownership transferred
        std::cout << "  after move: p is " << (p ? "non-null" : "null") << ", q is " << (q ? "non-null" : "null") << '\n';
        Matrix* observer = q.get();                    // raw pointer = "not mine to delete"
        std::cout << "  observer->cols()=" << observer->cols() << '\n';

        auto arr = std::make_unique<double[]>(5);      // zero-initialised, calls delete[]
        arr[4] = 1.5;
        std::cout << "  unique_ptr<double[]>: arr[4]=" << arr[4] << '\n';

        FilePtr f(std::fopen("/dev/null", "w"));       // fclose runs when f dies, on every path
        std::cout << "  FILE* wrapped: " << (f ? "open" : "failed") << ", sizeof(FilePtr)=" << sizeof(f) << '\n';
    }

    std::cout << "\n=== 5b. shared_ptr / weak_ptr ===\n";
    {
        auto a = std::make_shared<Matrix>(3, 3);       // one allocation: object + control block
        std::cout << "  use_count=" << a.use_count();
        auto b = a;
        std::cout << " -> " << a.use_count();
        std::weak_ptr<Matrix> w = a;                   // does not count
        std::cout << " (weak added) -> " << a.use_count() << '\n';
        a.reset(); b.reset();
        std::cout << "  after both reset, w.lock() is " << (w.lock() ? "alive" : "empty") << '\n';
        std::cout << "  sizeof(shared_ptr)=" << sizeof(a) << " (two pointers)\n";
    }

    std::cout << "\n=== 6. Graph ownership: L = (a*b) + c frees itself in a cascade ===\n";
    {
        NodePtr a = leaf(2.0, 'a'), b = leaf(-3.0, 'b'), c = leaf(10.0, 'c');
        NodePtr L = binary(binary(a, b, '*'), c, '+');
        std::cout << "  L->data = " << L->data << "   use_count: a=" << a.use_count()
                  << " (my variable + the '*' node), c=" << c.use_count() << '\n';
        if (auto parent = a->parents[0].lock())
            std::cout << "  a's parent via weak_ptr: '" << parent->op << "'\n";
        std::cout << "  L.reset():\n";
        L.reset();                                     // '+' dies -> releases '*' -> leaves survive
        std::cout << "  use_count now: a=" << a.use_count() << " b=" << b.use_count() << " c=" << c.use_count()
                  << ";  a's old parent is " << (a->parents[0].lock() ? "alive" : "gone") << '\n';
        std::cout << "  leaves die at end of scope:\n";
    }

    std::cout << "\n=== 7. optional ===\n";
    {
        for (double x : {4.0, -1.0}) {
            if (auto r = safe_sqrt(x)) std::cout << "  sqrt(" << x << ") = " << *r << '\n';
            else                       std::cout << "  sqrt(" << x << ") has no real value\n";
        }
        std::cout << "  value_or: " << safe_sqrt(-1.0).value_or(0.0)
                  << "   sizeof(optional<double>)=" << sizeof(std::optional<double>) << " (inline, no heap)\n";
    }

    std::cout << "\n=== 8. vector<unique_ptr<Layer>>: the C vtable struct, owned properly ===\n";
    {
        std::vector<std::unique_ptr<Layer>> net;
        net.push_back(std::make_unique<Scale>(2.0));
        net.push_back(std::make_unique<Shift>(1.0));
        auto last = std::make_unique<Scale>(0.5);
        net.push_back(std::move(last));                // sink: named local must be std::move'd
        std::vector<double> h = {1.0, 2.0, 3.0};
        for (const auto& layer : net) {                // auto&: cannot copy a unique_ptr anyway
            h = layer->forward(h);
            std::cout << "  " << layer->name() << " -> ";
            for (double v : h) std::cout << v << ' ';
            std::cout << '\n';
        }
        // scope end: vector destroys each unique_ptr -> virtual ~Layer -> correct derived dtor
    }

    std::cout << "\nlive Buffer allocations at exit: " << Buffer::live_allocs << " (must be 0)\n";
    return 0;
}
