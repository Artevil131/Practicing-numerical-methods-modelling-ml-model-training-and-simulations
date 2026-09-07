# Chapter 09 — Inheritance and Polymorphism

## What you'll be able to do after this chapter

- Build a `Layer` hierarchy (`Linear`, `ReLU`, `Sigmoid`, `Sequential`) and an `Optimizer` hierarchy (`SGD`, `Adam`) with abstract base classes, and call them through `std::unique_ptr<Base>` without leaks or UB.
- Explain what a vtable is, connect it to the struct-of-function-pointers you wrote in C, and say what a virtual call costs.
- Use `override`, `final`, `virtual ~Base()`, `protected`, `Base::f()` correctly and know which of them is mandatory.
- Recognise slicing, `dynamic_cast` and multiple inheritance as smells and know the alternatives: composition, templates/CRTP, `std::variant` + `std::visit`.
- Choose between runtime polymorphism (virtual), static polymorphism (templates) and closed-set polymorphism (`variant`) for a given design — e.g. autograd ops vs network layers.

## Why this matters for ML / numerics / sims

An MLP is a list of layers of *different types* that all answer `forward` and `backward`. An optimiser is one of several update rules applied to the same parameters. An autograd graph node is one of a fixed set of operations. An ODE integrator takes "a right-hand side" that might be gravity, a spring, or a Lorenz system. Every one of these is "many types, one interface". In `../../c_learning/11_function_pointers_and_generics/lesson.md` you built this by hand with a struct of function pointers. C++ gives you three tools with different cost/flexibility trade-offs, and this chapter is about picking the right one — because the wrong one (virtual call per matrix element) can cost 10x in an inner loop.

## 1. Inheritance syntax and what `public` means

```cpp
class Layer { /* ... */ };
class Linear : public Layer { /* ... */ };     // Linear IS-A Layer
```

`public` inheritance means every `Linear` can be used wherever a `Layer` is expected: a `Linear*` converts implicitly to `Layer*`, a `Linear&` to `Layer&`. That is the **is-a** relationship and the only kind of inheritance this course uses. (`private`/`protected` inheritance exist and mean "implemented in terms of"; use composition instead.)

Derived classes inherit all base members. Access:

| Base member is | Visible inside `Derived`? | Visible to outside code? |
|----------------|---------------------------|--------------------------|
| `public`       | yes                       | yes                      |
| `protected`    | yes                       | no                       |
| `private`      | **no**                    | no                       |

`protected` is for state that subclasses legitimately need (e.g. a `param_count_` in an optimiser base). Default to `private`; promote to `protected` only when a derived class needs it. Public data members in a base are an invitation to break invariants from anywhere.

Python equivalent: `class Linear(Layer):`. Python has no access control and every method is virtual; C++ makes both explicit.

## 2. Construction and destruction order

A derived object contains a base subobject. Construction is base first, then derived members, then the derived constructor body. Destruction is the exact reverse.

```cpp
struct Base {
    Base()  { std::cout << "Base()\n"; }
    ~Base() { std::cout << "~Base()\n"; }
};
struct Member { Member() { std::cout << "Member()\n"; } ~Member() { std::cout << "~Member()\n"; } };
struct Derived : Base {
    Member m;
    Derived() : Base() { std::cout << "Derived()\n"; }   // Base() is called first regardless of where you write it
    ~Derived() { std::cout << "~Derived()\n"; }
};
// Derived d;  prints: Base()  Member()  Derived()   then at scope end:  ~Derived()  ~Member()  ~Base()
```

You pass arguments to the base constructor in the initialiser list: `Linear(size_t in, size_t out) : Layer("linear"), W(in, out) {}`. If you omit it, the base's default constructor runs (compile error if it has none). Memory layout: the base subobject sits at the start of the derived object, which is why a `Derived*` can be treated as a `Base*` with no adjustment (for single inheritance).

```
 Linear object in memory:
 +------------------+
 | Layer part       |  <- vptr lives here (if Layer has virtual functions)
 +------------------+
 | Matrix W         |
 | Matrix b         |
 +------------------+
```

## 3. `virtual` functions and dynamic dispatch — the vtable

Without `virtual`, the function called is chosen by the **static** type of the expression:

```cpp
struct Layer  { void describe() const { std::cout << "Layer\n"; } };
struct Linear : Layer { void describe() const { std::cout << "Linear\n"; } };
Linear lin;
Layer& ref = lin;
ref.describe();          // prints "Layer" — static type of ref is Layer&. Almost never what you want.
```

With `virtual`, the call goes through the object's **dynamic** type:

```cpp
struct Layer  { virtual void describe() const { std::cout << "Layer\n"; } };
struct Linear : Layer { void describe() const override { std::cout << "Linear\n"; } };
ref.describe();          // prints "Linear"
```

How it works — exactly your C design, done by the compiler. Each class with virtual functions gets one static table of function pointers (the **vtable**). Each object gets one hidden pointer to its class's vtable (the **vptr**, usually the first 8 bytes). A virtual call is: load vptr → load the function pointer at a fixed slot → indirect call.

```
 C, chapter 11:                              C++:
 struct Layer {                              class Layer { virtual Matrix forward(...); ... };
     Matrix (*forward)(void*, Matrix*);         object:   [ vptr ] [ members... ]
     Matrix (*backward)(void*, Matrix*);        vtable:   Layer::  [ &Layer::~Layer ][ &forward ][ &backward ]
     void  (*free)(void*);                                Linear:: [ &Linear::~Linear ][ &Linear::forward ][ &Linear::backward ]
     void *state;
 };                                          call:  obj->vptr[1](obj, x)
 call:  l->forward(l->state, x)
```

Differences: the table is per *class* (one copy), not per object (your C struct carried three pointers per layer; a C++ object carries one). The compiler fills the table and never gets a slot wrong. `sizeof(Layer)` grows by exactly one pointer.

## 4. `override` — always write it

`override` on a derived function asks the compiler to verify that a base virtual with the same signature exists. Without it, a typo creates a *new, unrelated* function and the base version is silently called:

```cpp
struct Layer  { virtual Matrix forward(const Matrix& x) = 0; };
struct ReLU : Layer {
    Matrix forward(Matrix& x);               // no const: DIFFERENT signature -> hides, does not override.
                                             // Layer::forward still pure -> ReLU is abstract -> error later, elsewhere
    Matrix forward(const Matrix& x) override; // with `override`: compiler checks the match right here
};
```

`override` implies `virtual`; do not write both. `-Wall -Wextra` on Clang includes `-Woverloaded-virtual` which catches some of these, but `override` catches all of them at the declaration.

## 5. Pure virtual functions and abstract classes as interfaces

`= 0` makes a function **pure virtual**: the base gives no implementation and cannot be instantiated. A class with at least one pure virtual is **abstract** — it is an interface.

```cpp
class Layer {
public:
    virtual ~Layer() = default;
    virtual Matrix forward(const Matrix& x) = 0;          // pure: every concrete layer must implement
    virtual Matrix backward(const Matrix& grad_out) = 0;
    virtual void step(double lr) {}                        // non-pure default: layers without params do nothing
    virtual std::string name() const = 0;
};
// Layer l;                    // ERROR: abstract
// std::unique_ptr<Layer> p = std::make_unique<Layer>();   // ERROR
std::unique_ptr<Layer> p = std::make_unique<ReLU>();       // OK: ReLU implements everything
```

A pure virtual *may* still have a body (`Matrix Layer::forward(...) { ... }` defined out of class) which derived classes can call as `Layer::forward(x)` — rare; used for a default that must be explicitly opted into.

Python equivalent: `abc.ABC` with `@abstractmethod`. C++ enforces it at compile time, not at instantiation.

## 6. `virtual ~Base()` — mandatory when deleting through a base pointer

```cpp
struct Base { ~Base() {} };                                    // NOT virtual
struct Derived : Base { std::vector<double> big = std::vector<double>(1'000'000); };
Base* p = new Derived;
delete p;               // UNDEFINED BEHAVIOUR. In practice: ~Base runs, ~Derived does not, `big` leaks.
std::unique_ptr<Base> q = std::make_unique<Derived>();         // same UB when q dies — unique_ptr does not save you
```

`delete p` looks at the static type `Base*`, sees a non-virtual destructor, and calls `Base::~Base` only. With `virtual ~Base()`, the destructor is dispatched like any other virtual function and `Derived::~Derived` runs first, then `Base::~Base`.

Rule: **if a class has any virtual function, give it a virtual destructor.** `virtual ~Base() = default;` costs nothing you were not already paying (the vptr is already there). Then, because you declared a destructor, restore the moves with `= default` if the base has data members (chapter 07, section 6). For a pure interface with no data, the compiler-generated ones are all trivial and it does not matter.

Clang warns: `-Wnon-virtual-dtor` (in `-Wall`? no — add `-Wnon-virtual-dtor` or `-Wdelete-non-virtual-dtor`, the latter is on by default and fires on the `delete` line above).

## 7. The running example: `Layer` hierarchy and `Sequential`

```cpp
class Layer {
public:
    virtual ~Layer() = default;
    virtual Matrix forward(const Matrix& x) = 0;
    virtual Matrix backward(const Matrix& grad_out) = 0;  // returns grad w.r.t. input; caches grads for params
    virtual void step(double /*lr*/) {}                    // default: no parameters
    virtual std::string name() const = 0;
};

class Linear : public Layer {
    Matrix W_, b_, x_cache_, dW_, db_;
public:
    Linear(std::size_t in, std::size_t out, std::mt19937_64& rng);
    Matrix forward(const Matrix& x) override { x_cache_ = x; return add_rowvec(x * W_, b_); }
    Matrix backward(const Matrix& g) override {
        dW_ = transpose(x_cache_) * g;  db_ = sum_rows(g);
        return g * transpose(W_);
    }
    void step(double lr) override { W_ -= lr * dW_; b_ -= lr * db_; }
    std::string name() const override { return "Linear"; }
};

class ReLU : public Layer {
    Matrix mask_;
public:
    Matrix forward(const Matrix& x) override { mask_ = greater(x, 0.0); return hadamard(x, mask_); }
    Matrix backward(const Matrix& g) override { return hadamard(g, mask_); }
    std::string name() const override { return "ReLU"; }
};
class Sigmoid : public Layer { /* cache output s; backward: g ⊙ s ⊙ (1-s) */ };

class Sequential : public Layer {                            // a Layer that contains Layers (Composite pattern)
    std::vector<std::unique_ptr<Layer>> layers_;
public:
    void add(std::unique_ptr<Layer> l) { layers_.push_back(std::move(l)); }
    Matrix forward(const Matrix& x) override {
        Matrix h = x;
        for (auto& l : layers_) h = l->forward(h);
        return h;
    }
    Matrix backward(const Matrix& g) override {
        Matrix d = g;
        for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) d = (*it)->backward(d);
        return d;
    }
    void step(double lr) override { for (auto& l : layers_) l->step(lr); }
    std::string name() const override { return "Sequential"; }
};
```

Usage mirrors PyTorch:

```cpp
Sequential net;
net.add(std::make_unique<Linear>(784, 128, rng));
net.add(std::make_unique<ReLU>());
net.add(std::make_unique<Linear>(128, 10, rng));
Matrix logits = net.forward(X);
Matrix dlogits = softmax_ce_backward(logits, y);
net.backward(dlogits);
net.step(0.01);
```

Every `l->forward(h)` is one virtual call per *layer* per batch — nanoseconds against milliseconds of matmul. This is where virtual dispatch belongs.

### `Optimizer` hierarchy

```cpp
class Optimizer {
protected:
    double lr_;                                   // protected: SGD and Adam both need it
public:
    explicit Optimizer(double lr) : lr_(lr) {}
    virtual ~Optimizer() = default;
    virtual void update(Matrix& param, const Matrix& grad) = 0;
};
class SGD : public Optimizer {
public:
    using Optimizer::Optimizer;                   // inherit the constructor
    void update(Matrix& p, const Matrix& g) override { p -= lr_ * g; }
};
class Adam : public Optimizer {
    double b1_, b2_, eps_; long t_ = 0;
    std::unordered_map<const Matrix*, std::pair<Matrix, Matrix>> state_;   // m, v per parameter
public:
    Adam(double lr, double b1 = 0.9, double b2 = 0.999, double eps = 1e-8)
        : Optimizer(lr), b1_(b1), b2_(b2), eps_(eps) {}
    void update(Matrix& p, const Matrix& g) override;   // m = b1 m + (1-b1) g; v = b2 v + (1-b2) g²; bias-correct; step
};
```

Adam keeps per-parameter state keyed by the parameter's address — the parameter must therefore not move. That is a design constraint to document, exactly like PyTorch's `optimizer.state` keyed by parameter identity.

## 8. Calling the base implementation: `Base::f()`

A derived override can extend rather than replace the base version:

```cpp
class LoggingLinear : public Linear {
public:
    using Linear::Linear;
    Matrix forward(const Matrix& x) override {
        std::cout << "Linear forward, batch " << x.rows() << '\n';
        return Linear::forward(x);        // qualified call: NON-virtual, goes straight to Linear's version
    }
};
```

`Linear::forward(x)` bypasses dispatch. Without the qualifier you would call yourself recursively forever.

Python equivalent: `super().forward(x)`.

## 9. Slicing — copying a derived object into a base by value

```cpp
void bad(Layer l);                  // takes Layer BY VALUE — cannot even compile if Layer is abstract, but if it were concrete:
Linear lin(3, 2, rng);
Layer copy = lin;                   // SLICING: only the Layer subobject is copied; W_, b_ are gone. vptr is Layer's.
std::vector<Layer> v; v.push_back(lin);   // same
```

Polymorphic objects are handled by **pointer or reference** (`Layer&`, `const Layer&`, `Layer*`, `std::unique_ptr<Layer>`), never by value. A common defence is to make the base's copy operations `protected` or deleted so slicing does not compile. Abstract bases cannot be instantiated, so slicing into them fails to compile automatically — one more reason to make bases abstract.

## 10. `dynamic_cast` and `typeid` — rarely needed

`dynamic_cast<Linear*>(layer_ptr)` returns the pointer if the object really is a `Linear` (or derived from it), else `nullptr`; on references it throws `std::bad_cast`. `typeid(*p).name()` gives a mangled type name. Both need the vptr (the class must be polymorphic).

```cpp
for (auto& l : layers_)
    if (auto* lin = dynamic_cast<Linear*>(l.get()))
        std::cout << "Linear with " << lin->param_count() << " params\n";
```

If you find yourself writing `if (dynamic_cast<A*>) ... else if (dynamic_cast<B*>) ...`, you are reimplementing the vtable by hand — add a virtual function instead (`virtual std::size_t param_count() const { return 0; }`). Legitimate uses: debugging, serialisation, or crossing a boundary where you cannot change the base. Cost: a string/pointer walk up the hierarchy; not free.

## 11. `final`

`final` on a class forbids deriving from it; on a virtual function it forbids further overriding. Two uses: documenting intent, and enabling **devirtualisation** — when the compiler knows the dynamic type cannot be anything else, it can call the function directly and inline it.

```cpp
class ReLU final : public Layer { ... };   // nobody derives from ReLU; calls through ReLU& are direct
```

## 12. Multiple inheritance — avoid, except for pure interfaces

C++ allows `class C : public A, public B`. With data in both bases you get the diamond problem, `virtual` inheritance, and pointer adjustments on conversion. Rule for this course: a class derives from at most one class *with data*; any additional bases are pure interfaces (no data, all pure virtual). E.g. `class Linear : public Layer, public Serializable`. Even that is rarely necessary — prefer a free function or a `std::function` member.

## 13. Composition over inheritance

Most of the time a `struct` holding the varying part is simpler than a hierarchy:

```cpp
// Inheritance: a class per activation
class Tanh : public Layer { ... };  class ReLU : public Layer { ... };  class Sigmoid : public Layer { ... };

// Composition: one class, the varying part is a pair of callables
class Activation : public Layer {
    std::function<double(double)> f_, df_;
    Matrix cache_;
public:
    Activation(std::function<double(double)> f, std::function<double(double)> df) : f_(std::move(f)), df_(std::move(df)) {}
    Matrix forward(const Matrix& x) override { cache_ = x; return map(x, f_); }
    Matrix backward(const Matrix& g) override { return hadamard(g, map(cache_, df_)); }
};
net.add(std::make_unique<Activation>([](double x){ return std::tanh(x); }, [](double x){ double t = std::tanh(x); return 1 - t*t; }));
```

Three classes became one class and two lambdas. (Note the `std::function` call per element in `map` — see section 16; a template parameter instead of `std::function` fixes that while keeping the composition.) Ask "does the *type* need to vary, or just a *value*?" If just a value — a function, a number, a strategy — store it as a member.

Python equivalent: passing a function instead of subclassing. `nn.ReLU()` vs `F.relu`.

## 14. Static polymorphism: templates and CRTP

When the set of types is known at compile time, a template parameter does what a virtual function does with **zero overhead** — every call is direct and inlinable:

```cpp
template <typename RHS>
void rk4_step(RHS f, double& t, Vec3& y, double h) {      // f is any callable: lambda, functor, function
    Vec3 k1 = f(t, y);
    Vec3 k2 = f(t + h/2, y + k1 * (h/2));
    Vec3 k3 = f(t + h/2, y + k2 * (h/2));
    Vec3 k4 = f(t + h,   y + k3 * h);
    y += (k1 + k2*2 + k3*2 + k4) * (h/6);
    t += h;
}
```

Compared with `struct RHS { virtual Vec3 operator()(double, Vec3) = 0; }` this is the difference between an inlined 10-flop body and an indirect call per stage. The cost of templates: the code is instantiated per type (binary size), errors are verbose, and you cannot put different `RHS` types in one `std::vector`.

**CRTP** (Curiously Recurring Template Pattern) gives you "base class calls derived implementation" without `virtual`:

```cpp
template <typename Derived>
struct Shape {
    double area() const { return static_cast<const Derived&>(*this).area_impl(); }   // static dispatch
    void print() const { std::cout << "area=" << area() << '\n'; }                    // shared code in base
};
struct Circle : Shape<Circle> {
    double r;
    double area_impl() const { return M_PI * r * r; }
};
Circle c{{}, 2.0}; c.print();     // Shape<Circle>::print -> area -> Circle::area_impl, all inlined
```

Use CRTP for mixins of shared code in performance-critical numeric types (e.g. giving every `VecN` the same `norm()` in terms of its `dot`). It is heavier to read than virtual; reach for it only when profiling says the virtual call matters.

## 15. `std::variant` + `std::visit` — closed-set polymorphism

When the set of alternatives is **fixed and known** (autograd ops: `Add`, `Mul`, `Tanh`, `MatMul`...), a `std::variant` (C++17, `<variant>`) is a type-safe tagged union — exactly the C `enum kind` + `union` from `../../c_learning/07_structs_unions_enums/lesson.md`, but the compiler checks that every case is handled.

```cpp
#include <variant>
struct Add {};  struct Mul {};  struct Tanh {};  struct Leaf {};
using Op = std::variant<Leaf, Add, Mul, Tanh>;

struct Node { double data, grad = 0; Op op; std::vector<std::shared_ptr<Node>> children; };

void backward_local(Node& n) {
    std::visit([&](auto&& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, Add>) {
            n.children[0]->grad += n.grad;  n.children[1]->grad += n.grad;
        } else if constexpr (std::is_same_v<T, Mul>) {
            n.children[0]->grad += n.children[1]->data * n.grad;
            n.children[1]->grad += n.children[0]->data * n.grad;
        } else if constexpr (std::is_same_v<T, Tanh>) {
            n.children[0]->grad += (1 - n.data * n.data) * n.grad;
        }   // Leaf: nothing
    }, n.op);
}
```

`std::visit` dispatches on the active alternative (a jump table, comparable to a virtual call but with no heap object and the value stored inline). Adding a new `Op` type without handling it in a visitor that uses overloaded lambdas (rather than `if constexpr`) is a **compile error** — the closed set is enforced. Also: `std::holds_alternative<Mul>(op)`, `std::get<Mul>(op)` (throws `std::bad_variant_access` if wrong), `std::get_if<Mul>(&op)` (returns `nullptr` if wrong).

| Tool                     | Set of types | Dispatch cost              | Storage                   | Add a type later?         |
|--------------------------|--------------|----------------------------|---------------------------|---------------------------|
| virtual functions        | open         | indirect call              | heap via `unique_ptr`     | new class, no other edits |
| templates / CRTP         | compile-time | none (inlined)             | by value                  | new type, recompile users |
| `std::variant` + `visit` | closed       | jump table, no indirection to heap | inline, `max(sizeof)` | edit the variant + every visitor (compiler tells you where) |

For the autograd engine: nodes are many and small, the op set is closed, and backward is called per node — `variant` fits. For network layers: few, large, users add their own — `virtual` fits. For an ODE integrator's right-hand side: called per stage per step in the hottest loop — template.

## 16. Cost of virtual calls

A virtual call is: one dependent load (vptr), one load (slot), one indirect branch. On a modern CPU with a well-predicted branch that is ~1-3 ns — *if* the branch predictor is right (same dynamic type each time). The real cost is that **the compiler cannot inline it**: no constant propagation, no vectorisation of the surrounding loop, no fusion.

- Per layer (`forward` on a 64x784 batch): the matmul is ~ms; the virtual call is nothing. Use `virtual`.
- Per element (`virtual double activation(double)` inside a loop over 50 000 values): the indirect call dominates and blocks vectorisation; expect several-x slowdown. Use a template parameter or apply the whole matrix at once inside one virtual call (`virtual Matrix forward(const Matrix&)` — the loop is *inside* the override).

The design rule that falls out: **make the virtual interface coarse-grained.** Dispatch once per array, not once per number.

## Gotchas and undefined behavior

- **Deleting through a base pointer without `virtual ~Base()`** — UB; derived destructor skipped, members leak. `unique_ptr<Base>` does not help.
- **Missing `override`** — a signature mismatch silently creates a new function; base version still called. Always write `override`.
- **Calling a virtual function from a constructor or destructor** dispatches to the *current* class's version, not the derived one (the derived part does not exist yet / is already gone). Not UB, but surprising; if the function is pure virtual there, it *is* UB.
- **Slicing** — passing polymorphic objects by value copies only the base part. Pass by reference/pointer.
- **Hiding instead of overriding**: a derived `void f(int)` hides *all* base overloads `f(...)`, virtual or not. Bring them back with `using Base::f;`.
- **`static_cast<Derived*>` down a wrong branch** — UB. Use `dynamic_cast` when the type is genuinely unknown, or redesign.
- **Object slicing through `std::vector<Base>`** — cannot even hold derived objects. Use `std::vector<std::unique_ptr<Base>>`.
- **Forgetting `= default` moves after declaring `virtual ~Base()`** — silently copies on every return of a derived object with big members.
- **`std::get<T>` on a variant holding a different alternative** throws; `std::get_if` is the safe form.
- **Diamond inheritance without `virtual` bases** duplicates the shared base; avoid multiple inheritance with data entirely.
- **Virtual call per element** in a numeric loop — not a bug, but a 3-10x performance cliff. Dispatch per array.

## Common mistakes checklist

- [ ] Every class with a virtual function has `virtual ~Base() = default;`.
- [ ] Every override is marked `override`; no `virtual` repeated on it.
- [ ] Bases used as interfaces are abstract (at least one `= 0`), so slicing cannot compile.
- [ ] Polymorphic objects are held by `std::unique_ptr<Base>` or passed as `Base&`/`const Base&`; never by value, never in `std::vector<Base>`.
- [ ] `Base::f()` (qualified) when extending, never an unqualified recursive call.
- [ ] `protected` only for state genuinely needed by derived classes; no public data in bases.
- [ ] No `dynamic_cast` chains — replaced by a virtual function.
- [ ] Virtual interface is coarse-grained (per matrix, per layer), not per element.
- [ ] Fixed set of alternatives → considered `std::variant`; hot inner loop → considered a template parameter; only a *value* varies → considered composition with a member.
- [ ] Multiple inheritance only from pure interfaces, if at all.

## You can move on when...

- You can draw the memory of a `Linear` object and its vtable, and write the C equivalent of one virtual call.
- You can explain what `override` catches, what `virtual ~Base()` prevents, and what slicing loses — each in one sentence.
- You can implement `Layer` / `Linear` / `ReLU` / `Sequential` with `forward`, `backward`, `step` and train a two-layer net one step, holding layers as `std::vector<std::unique_ptr<Layer>>`.
- You can implement `Optimizer` / `SGD` / `Adam` and say why `protected double lr_` is acceptable there.
- Given a design ("ODE right-hand side", "autograd op", "network layer", "activation function"), you can pick virtual / template / `variant` / composition and defend the choice with the cost table.
- You can write a `std::visit` over a `variant` of ops and explain what happens at compile time when you add an alternative.
- You can state the cost of a virtual call and the one thing (inlining) it prevents.
