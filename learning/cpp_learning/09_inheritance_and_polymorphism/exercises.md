# Chapter 09 — Exercises

Write each exercise as `ex09_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex09_K ex09_K.cpp`. For the UB exercises also build with
`c++ -g -fsanitize=address,undefined -std=c++17 -o ex09_K ex09_K.cpp` and run.

---

**09.1 — Construction and destruction order**
Write `Base`, `Member`, `Derived : Base` (with a `Member m;` and a second `Member m2;`) and `MoreDerived : Derived`, each constructor/destructor printing its name. Create a `MoreDerived` on the stack, then a `std::unique_ptr<Base>` holding a `MoreDerived` (with a virtual destructor). Predict the full output in a comment before running. Then remove `virtual` from `~Base()` and record what the second case prints now (and why the sanitizer may not complain — no memory is *corrupted*, it is *leaked* / skipped).

Example fragment: `Base() Member() Member() Derived() MoreDerived()` ... reverse on destruction.

<details><summary>Hint</summary>
Members are constructed in declaration order, after the base and before the constructor body; destroyed in reverse.
</details>

---

**09.2 — Static vs dynamic dispatch, and `override`**
Write `struct A { void f(); virtual void g(); void h(int); virtual void k() const; }` and `struct B : A` that redefines all four. Call each through `A&` bound to a `B` and record which version runs. Then (a) add `override` to `B::k` but declare it *without* `const` — record the compiler error; (b) note that `B::h(int)` hides `A::h` and that `B b; b.h(1.5)` compiles but calls `B::h(int)` with a truncated argument; fix with `using A::h;` plus an `h(double)` in `A` to show hiding. Explain in a comment: what does `override` protect against that `-Wall -Wextra` alone did not?

<details><summary>Hint</summary>
Non-virtual functions dispatch on the static type; `override` demands an exact signature match with a base virtual (including `const`).
</details>

---

**09.3 — Vtable by hand vs by compiler**
Re-implement the C course's `Layer` struct-of-function-pointers in C++ (a `struct CLayer { std::vector<double> (*forward)(void*, const std::vector<double>&); void (*destroy)(void*); void* state; }` with `Scale` and `Shift` "subclasses" managed by hand). Alongside it, write the `virtual` version. Print `sizeof` of each layer object and explain the difference (per-object function pointers vs one vptr). Run both over `{1,2,3}` through `Scale(2) → Shift(1)` and check identical results. Finally use `-O2 -S` (or `objdump`/`otool -tv` on the binary) to find the indirect call instruction for one virtual call and paste it in a comment (`blr` on arm64).

<details><summary>Hint</summary>
`c++ -std=c++17 -O2 -S -o - ex09_3.cpp | grep -n blr` finds indirect branches. The C-style struct stores N function pointers per object; the C++ object stores one vptr.
</details>

---

**09.4 — Slicing, and how to make it impossible**
Write a concrete (non-abstract) `struct Shape { virtual double area() const { return 0; } virtual ~Shape() = default; }` and `struct Circle : Shape { double r; double area() const override; }`. Show slicing: `Shape s = Circle{...}; s.area()` returns `0`; `std::vector<Shape> v; v.push_back(Circle{...});` loses `r`. Then (a) fix by making `Shape::area` pure virtual — slicing no longer compiles (paste the error); (b) alternatively keep it concrete but declare `Shape(const Shape&) = delete;` — show that `std::vector<std::unique_ptr<Shape>>` still works while slicing is a compile error. Finish with a function `double total_area(const std::vector<std::unique_ptr<Shape>>&)`.

<details><summary>Hint</summary>
Slicing copies only the base subobject; the copied object's vptr points to `Shape`'s vtable. An abstract class cannot be a by-value variable at all.
</details>

---

**09.5 — `dynamic_cast` and then removing it**
Build `std::vector<std::unique_ptr<Layer>>` with mixed `Linear` and `ReLU` (use `std::vector<double>` weights, nothing fancy). First write `size_t count_params(const std::vector<std::unique_ptr<Layer>>&)` using `dynamic_cast<Linear*>` on each element. Then refactor: add `virtual std::size_t param_count() const { return 0; }` to `Layer`, override in `Linear`, and rewrite `count_params` with no cast. Time both over 1 000 000 iterations of a 10-layer vector and comment on the ratio. Also demonstrate `typeid(*p).name()` for each layer and `dynamic_cast` on a *reference* throwing `std::bad_cast`.

<details><summary>Hint</summary>
`dynamic_cast` walks the type hierarchy at runtime; a virtual call is a single indirect call. The refactor is also more extensible: a new parameterised layer type needs no edit in `count_params`.
</details>

---

**09.6 — Composition instead of a hierarchy**
Take an `Activation` hierarchy with `Tanh`, `ReLU`, `Sigmoid`, `LeakyReLU(slope)` classes (each overriding `f` and `df` on a `std::vector<double>`). Rewrite it as ONE class `Activation` holding two `std::function<double(double)>` members and constructed from lambdas. Then rewrite it AGAIN as a class template `template <typename F, typename DF> class Activation` so the calls inline. Confirm all three produce identical outputs on `{-2,-1,0,1,2}`, and time `forward` on a vector of 10 million elements for the `std::function` version vs the template version. Explain the difference in a comment.

<details><summary>Hint</summary>
`std::function` makes one type-erased call per element; the template version inlines the lambda into the loop and lets the compiler vectorise.
</details>

---

**09.7 — `std::variant` autograd ops with exhaustive visitation**
Define `struct Leaf{}; struct Add{}; struct Mul{}; struct Tanh{}; using Op = std::variant<Leaf, Add, Mul, Tanh>;` and `struct Node { double data, grad = 0; Op op; std::vector<std::shared_ptr<Node>> children; }`. Write `void local_backward(Node&)` with `std::visit` using the *overloaded-lambdas* idiom (`template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; }; template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;`), one lambda per op. Build `L = tanh(a*b + c)` for `a=2, b=-3, c=10`, set `L->grad = 1`, run local_backward in reverse topological order (write the topo sort — a DFS with a visited set), and print `a->grad, b->grad, c->grad`. Then add `struct Exp{}` to the variant WITHOUT adding a lambda and paste the compile error, then implement it.

Example: with `L = tanh(a*b + c) = tanh(4)`, `dL/da = b * (1 - tanh²(4)) ≈ -0.00201`.

<details><summary>Hint</summary>
`d tanh(x)/dx = 1 - tanh²(x)`, and `n.data` already holds `tanh(x)`. Topological order: post-order DFS from `L`, then iterate reversed.
</details>

---

**09.8 — `Layer` hierarchy with a real training step (ML)**
Implement `Layer` (abstract: `forward`, `backward`, `step`, `name`), `Linear` (weights as `std::vector<double>` in row-major, `in x out`, bias, caches input, computes `dW`, `db`, returns `dX`), `ReLU`, and `Sequential : Layer` holding `std::vector<std::unique_ptr<Layer>>`. Train a 2-4-1 network on XOR with MSE loss and plain SGD (`lr = 0.1`, 5000 epochs, full batch of the four points, weights initialised `N(0, 0.5)` with `mt19937_64(0)`). Print the loss every 1000 epochs and the four final predictions (they should approach `0,1,1,0`). Verify `backward` with a finite-difference check on ONE weight before training (relative error `< 1e-5`).

<details><summary>Hint</summary>
For a single sample `x` (row vector), `y = x W + b`, `dW = xᵀ · dy`, `db = dy`, `dx = dy · Wᵀ`. Accumulate `dW`, `db` over the batch inside `backward`; apply in `step`. Do the gradient check by perturbing `W[k]` by `±1e-5` and comparing `(L+ - L-) / 2e-5` with `dW[k]`.
</details>

---

**09.9 — `Optimizer` hierarchy: `SGD`, `Momentum`, `Adam` (ML)**
Implement `class Optimizer { protected: double lr_; public: virtual void update(std::vector<double>& p, const std::vector<double>& g) = 0; virtual ~Optimizer() = default; }`, `SGD`, `Momentum(lr, beta)` (per-parameter velocity, keyed by `&p` in an `std::unordered_map<const void*, std::vector<double>>`), and `Adam(lr, b1, b2, eps)` (per-parameter `m`, `v`, a step counter, bias correction). Minimise the Rosenbrock function `f(x,y) = (1-x)² + 100(y-x²)²` from `(-1.5, 2)` with each optimiser for 5000 steps (analytic gradient), printing `(x, y, f)` every 1000 steps. Hold the three optimisers in a `std::vector<std::unique_ptr<Optimizer>>` and loop over it. Comment on which reaches `(1,1)`.

<details><summary>Hint</summary>
`df/dx = -2(1-x) - 400x(y-x²)`, `df/dy = 200(y-x²)`. Adam: `m = b1 m + (1-b1) g`, `v = b2 v + (1-b2) g²`, `m̂ = m/(1-b1ᵗ)`, `v̂ = v/(1-b2ᵗ)`, `p -= lr m̂ / (√v̂ + eps)`. Use `lr = 1e-3` for SGD/Momentum (Rosenbrock is stiff) and `lr = 1e-2` for Adam.
</details>

---

**09.10 — Static polymorphism for an ODE integrator (numerics / sims)**
Write `template <typename RHS> void rk4_step(RHS f, double& t, std::array<double,4>& y, double h)` (state = `x, y, vx, vy`) and a virtual-based alternative `struct RHSBase { virtual std::array<double,4> operator()(double, const std::array<double,4>&) const = 0; virtual ~RHSBase() = default; }` with `void rk4_step_virtual(const RHSBase&, ...)`. Implement the Kepler problem (central gravity, `G M = 1`) as (a) a lambda for the template version and (b) a class for the virtual version. Integrate one orbit (`x=1, vy=1`, `h = 1e-3`, `2π/h` steps) with each and print the final position (both should return near `(1, 0)`) and the energy drift. Time both for 10 million steps and report the ratio. Then write a CRTP version `template <typename D> struct RHSCRTP { auto operator()(...) const { return static_cast<const D&>(*this).eval(...); } }` and time that too. Comment: at which granularity would you accept the virtual call?

<details><summary>Hint</summary>
RK4 evaluates `f` four times per step; a non-inlined virtual call plus a returned `std::array` copy at each is the entire cost of this tiny RHS. The virtual version becomes acceptable when the RHS itself is expensive (e.g. an N-body force over thousands of particles).
</details>
