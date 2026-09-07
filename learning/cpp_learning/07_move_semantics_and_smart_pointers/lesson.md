# Chapter 07 — Move Semantics and Smart Pointers

## What you'll be able to do after this chapter

- Return a 100 MB `Matrix` from a function by value and know that no copy happens — and *why*.
- Write a move constructor and move assignment for a class that owns a raw resource, and explain what `std::move` does (nothing) and does not do (move).
- State the Rule of Five and the Rule of Zero and pick the right one for each class you write.
- Replace every `new`/`delete` pair with `std::unique_ptr` / `std::make_unique`, and use `std::shared_ptr` / `std::weak_ptr` only where ownership is genuinely shared.
- Design ownership for a graph (autograd nodes) and a polymorphic container (`std::vector<std::unique_ptr<Layer>>`) without leaks or double frees.
- Use `std::optional<T>` for "maybe a value" instead of a pointer or a sentinel.

## Why this matters for ML / numerics / sims

Your C course `Matrix` was a struct with a `double *data` you had to `free`. Every function that produced a matrix either took an output parameter or returned a pointer the caller had to remember to free. C++'s answer is: return by value, and let *move semantics* make that free of charge. The autograd engine (micrograd port, then tensor autograd) is a graph of nodes that reference each other; getting ownership wrong there means leaks or use-after-free in the backward pass. The `Layer` hierarchy in an MLP is a list of objects of *different* types with *different* sizes — a container of owning pointers. Smart pointers are how C++ expresses "who frees this" in the type system, so the compiler checks it instead of you.

## 1. The problem: returning big objects by value

Pre-C++11, this function copied `result` on return:

```cpp
Matrix matmul(const Matrix& a, const Matrix& b) {
    Matrix result(a.rows(), b.cols());
    // ... fill result ...
    return result;       // C++98: copy-construct the return value from result, then destroy result
}
Matrix c = matmul(a, b); // C++98: possibly a second copy into c
```

For a 1000x1000 matrix that is 8 MB copied for nothing, twice. `result` is about to die anyway — copying it is pointless; we should *steal its buffer*. C++11 made "stealing from something about to die" a language feature.

Python equivalent: none needed; Python passes references to heap objects everywhere, so returning a NumPy array never copies. C++ value semantics (`../03_references_const_and_value_semantics/lesson.md`) give you copies by default; moves make the common case cheap again.

## 2. Value categories: lvalues, rvalues, xvalues

Every expression has a **value category**. The simple, sufficient definitions:

| Category | Definition                                                | Examples                                        |
|----------|-----------------------------------------------------------|-------------------------------------------------|
| lvalue   | names a persistent object; you can take its address       | `x`, `arr[3]`, `*p`, `m(i,j)`, a function returning `T&` |
| prvalue  | a temporary with no name; about to die                    | `42`, `a + b`, `Matrix(3,3)`, a function returning `T` |
| xvalue   | an lvalue you have *promised* is expendable               | `std::move(x)`, a function returning `T&&`      |

"rvalue" = prvalue or xvalue: anything whose resources may be stolen. Mnemonic: lvalues can appear on the *left* of `=`; rvalues can only appear on the right.

```cpp
Matrix a(2, 2), b(2, 2);
a = b;                 // b is an lvalue: a COPIES b (b is still needed)
a = b + b;             // b + b is a prvalue temporary: a may STEAL its buffer
a = std::move(b);      // std::move(b) is an xvalue: a may steal; b is now "moved-from"
```

## 3. Rvalue references: `T&&`

A reference that binds *only* to rvalues. `T&` (lvalue reference) binds to lvalues; `const T&` binds to anything but forbids modification; `T&&` binds to rvalues and *permits* modification — which is exactly what stealing requires.

```cpp
void f(const Matrix& m);   // #1: accepts anything, read-only
void f(Matrix&& m);        // #2: accepts only rvalues; may gut m

Matrix m(3, 3);
f(m);                 // #1  (m is an lvalue)
f(Matrix(3, 3));      // #2  (temporary)
f(std::move(m));      // #2  (you promised m is expendable)
```

Inside `#2`, the parameter `m` is itself a *named* variable — and names are lvalues. To pass it on as an rvalue you must `std::move(m)` again. This trips everyone once.

## 4. Move constructor and move assignment

Take a class that owns a raw buffer (the C-course `Matrix`, before you learned `std::vector`). The copy operations duplicate the buffer; the move operations steal it and leave the source empty but valid.

```cpp
class Buffer {
    double* p_ = nullptr;
    std::size_t n_ = 0;
public:
    explicit Buffer(std::size_t n) : p_(new double[n]()), n_(n) {}
    ~Buffer() { delete[] p_; }

    // copy: allocate + memcpy — O(n)
    Buffer(const Buffer& o) : p_(new double[o.n_]), n_(o.n_) {
        std::copy(o.p_, o.p_ + n_, p_);
    }
    Buffer& operator=(const Buffer& o) {
        if (this != &o) { Buffer tmp(o); swap(tmp); }   // copy-and-swap: exception-safe
        return *this;
    }

    // move: steal the pointer, null the source — O(1)
    Buffer(Buffer&& o) noexcept : p_(o.p_), n_(o.n_) {
        o.p_ = nullptr;          // ESSENTIAL: otherwise o's destructor frees OUR buffer
        o.n_ = 0;
    }
    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) {
            delete[] p_;         // release what we held
            p_ = o.p_;  n_ = o.n_;
            o.p_ = nullptr; o.n_ = 0;
        }
        return *this;
    }

    void swap(Buffer& o) noexcept { std::swap(p_, o.p_); std::swap(n_, o.n_); }
    std::size_t size() const { return n_; }
};
```

ASCII view of a move:

```
 before                                 after  Buffer b = std::move(a);
 a: [p_ → 0x1000][n_ 4]                 a: [p_ → nullptr][n_ 0]      (valid, empty)
     0x1000: | 1.0 | 2.0 | 3.0 | 4.0 |   b: [p_ → 0x1000][n_ 4]      (owns the buffer now)
```

Nothing was copied except two machine words. `a`'s destructor will run `delete[] nullptr`, which is defined to do nothing.

Python equivalent: there is none — this is the cost of value semantics. The nearest analogy is `b = a; a = None` for a list, except that C++ does it without reference counting.

## 5. `std::move` is a cast — it moves nothing

`std::move(x)` is `static_cast<T&&>(x)`. It produces an xvalue *naming* `x`. No memory is touched. The move happens only if something *receives* that xvalue via a move constructor/assignment:

```cpp
Matrix a(3, 3);
std::move(a);              // does absolutely nothing; a unchanged
Matrix b = std::move(a);   // NOW the move constructor of Matrix runs; a is moved-from
const Matrix c(3, 3);
Matrix d = std::move(c);   // COPIES: you cannot steal from const. Silent performance bug.
```

Read `std::move(x)` as "I am done with `x`; take what you want". The name is bad and permanent.

## 6. Rule of Five, Rule of Zero

If your class manages a resource with a raw handle (`new`, `malloc`, `fopen`, a socket), the compiler-generated copy would copy the *handle* and two objects would free it. You must then write **all five** special members:

1. destructor
2. copy constructor
3. copy assignment
4. move constructor
5. move assignment

(Writing any of 1-3 by hand **suppresses** the compiler's implicit moves; then `return result;` silently copies. Writing 4 or 5 deletes the implicit copies. So: all five or none.)

**Rule of Zero**: compose the class from members that already manage themselves — `std::vector`, `std::string`, `std::unique_ptr` — and write *none* of the five. The compiler generates member-wise copy, member-wise move, and a destructor that destroys each member.

```cpp
class Matrix {                     // Rule of Zero. Correct copy, move, destroy — for free.
    std::size_t rows_, cols_;
    std::vector<double> data_;     // vector has its own five; Matrix inherits correct behaviour
public:
    Matrix(std::size_t r, std::size_t c) : rows_(r), cols_(c), data_(r * c) {}
    // nothing else needed
};
```

Prefer Rule of Zero in everything you write for this course. Write Rule of Five exactly once, in an exercise, to understand what `std::vector` does for you. If you must declare a destructor (e.g. `virtual ~Base()`, chapter 09) but the members manage themselves, write `= default` for the other four so you do not lose the moves:

```cpp
struct Base {
    virtual ~Base() = default;
    Base(const Base&) = default;            Base& operator=(const Base&) = default;
    Base(Base&&) noexcept = default;        Base& operator=(Base&&) noexcept = default;
    Base() = default;
};
```

## 7. `noexcept` on move operations — why `std::vector` cares

When `std::vector<T>` grows, it must relocate existing elements to the new buffer. If `T`'s move constructor is `noexcept`, it moves them (fast). If the move constructor *might throw*, vector cannot recover from an exception mid-relocation (half the elements moved, half not), so it falls back to **copying** — for every `push_back` that reallocates. Mark move operations `noexcept`. Stealing a pointer never throws, so this is always honest. `std::vector`'s own move constructor is `noexcept`, so a Rule-of-Zero class gets a `noexcept` move automatically.

```cpp
static_assert(std::is_nothrow_move_constructible<Matrix>::value, "Matrix move must not throw");
```

## 8. When moves happen automatically

You rarely write `std::move`. The compiler moves (or does better — *elides*) in these cases:

| Situation                                        | What happens                                          |
|--------------------------------------------------|-------------------------------------------------------|
| `return local;` (a local variable by name)        | move (C++11), and usually elided entirely (NRVO)      |
| `return Matrix(3,3);` (a prvalue)                 | guaranteed elision in C++17: constructed in place      |
| `Matrix c = f();`                                 | guaranteed elision: `c` *is* the return object         |
| `v.push_back(Matrix(3,3));`                       | temporary is moved into the vector                    |
| `Matrix operator+(Matrix a, const Matrix& b)` called with a temporary | temporary is moved into `a`         |
| Passing a local you no longer need to a function taking by value | **copy**, unless you `std::move(local)`  |

The one place you *do* write `std::move`: handing a named local to something that takes ownership (`v.push_back(std::move(big));`, `layers.push_back(std::move(layer));`, initialising a member from a by-value constructor parameter `: data_(std::move(data))`).

**Never** write `return std::move(local);` — it disables elision and is at best a wash.

## 9. The moved-from state

After `Matrix b = std::move(a);`, `a` is in a **valid but unspecified** state. You may assign to it or destroy it. For standard types, "unspecified" usually means empty, but do not rely on it; for your own types, make it empty (null pointer, zero size). Do not *read* from a moved-from object unless you have reset it:

```cpp
std::vector<double> v = {1, 2, 3};
std::vector<double> w = std::move(v);
v.size();          // legal; probably 0; do not depend on it
v = {4, 5};        // fine: assignment gives v a well-defined state again
```

## 10. `std::swap`

`std::swap(a, b)` is three moves: `T tmp = std::move(a); a = std::move(b); b = std::move(tmp);`. For a Rule-of-Zero `Matrix` this is nine pointer-sized assignments, regardless of matrix size. Swap is the basis of copy-and-swap assignment (section 4) and of algorithms like `std::sort`. Provide a member `swap` for Rule-of-Five classes; Rule-of-Zero classes get an efficient `std::swap` automatically.

## 11. Perfect forwarding: `std::forward` (brief)

Inside a template, `T&&` where `T` is a deduced template parameter is a **forwarding reference**: it binds to *both* lvalues and rvalues and remembers which. `std::forward<T>(x)` restores the original value category so the callee gets a copy for lvalues and a move for rvalues:

```cpp
template <typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

That is essentially how `std::make_unique` and `emplace_back` are written. You will not write forwarding code often; recognise `std::forward` as "pass through, preserving lvalue/rvalue-ness".

## 12. Smart pointers: ownership in the type system

A **smart pointer** is a class that holds a raw pointer and deletes it in its destructor (RAII, `../02_classes_and_raii/lesson.md`). The three standard ones express three ownership policies.

### 12.1 `std::unique_ptr<T>` — sole ownership, zero overhead

Exactly one `unique_ptr` owns the object. Copying is a compile error; moving transfers ownership. `sizeof(unique_ptr<T>) == sizeof(T*)`; the destructor call is inlined. Use it as the default whenever you would have written `new`.

```cpp
#include <memory>

std::unique_ptr<Matrix> p = std::make_unique<Matrix>(3, 3);  // never write `new` yourself
p->rows();                 // -> and * work like a raw pointer
(*p)(1, 1) = 5.0;
Matrix* raw = p.get();     // non-owning view; do NOT delete it

std::unique_ptr<Matrix> q = p;               // ERROR: copy deleted
std::unique_ptr<Matrix> q = std::move(p);    // OK: q owns, p is now nullptr
if (!p) std::cout << "p is empty\n";

Matrix* released = q.release();   // q gives up ownership WITHOUT deleting; you must delete
q.reset();                        // deletes the owned object (if any), q becomes empty
q.reset(new Matrix(2, 2));        // takes ownership of a new object (prefer make_unique)
```

Why `make_unique` and not `unique_ptr<T>(new T)`? (1) no naked `new` to grep for; (2) exception safety in argument lists `f(unique_ptr<A>(new A), g())` — if `g()` throws after `new A` runs, the `A` leaks; `make_unique` closes the gap.

**Arrays**: `std::unique_ptr<double[]> buf(new double[n]);` or `std::make_unique<double[]>(n)` (zero-initialises). Provides `buf[i]`, calls `delete[]`. This is the drop-in replacement for the C course's `double *data = calloc(n, sizeof *data)`. Still prefer `std::vector<double>` unless you have a reason (fixed size, no capacity field).

**Custom deleters**: for resources that are not `new`ed — a `FILE*`, a `malloc`ed block from a C library:

```cpp
struct FileCloser { void operator()(FILE* f) const { if (f) std::fclose(f); } };
std::unique_ptr<FILE, FileCloser> f(std::fopen("data.bin", "rb"));
// f.get() is the FILE*; fclose runs when f dies, on every exit path including exceptions
```

The deleter is part of the type; with a stateless functor the `unique_ptr` is still one pointer wide.

### 12.2 `std::shared_ptr<T>` — reference counting

Several `shared_ptr`s may own the same object; the object is deleted when the *last* one dies. Copying increments an atomic counter; destroying decrements it.

```cpp
auto a = std::make_shared<Matrix>(3, 3);   // count = 1
auto b = a;                                // count = 2
std::cout << a.use_count();                // 2
a.reset();                                 // count = 1; object alive via b
b.reset();                                 // count = 0; Matrix destroyed here
```

Costs, all real: (1) `sizeof(shared_ptr) == 2 * sizeof(void*)`; (2) a separate heap **control block** holding the count (`make_shared` allocates it *together* with the object in one allocation — another reason to use `make_shared`, never `shared_ptr<T>(new T)`); (3) each copy/destroy is an atomic increment/decrement, which is slow in a hot loop and non-trivially slow across threads. Pass `shared_ptr` by `const&` or pass the raw `T&` when you are not transferring ownership.

**Cycles leak.** If `A` holds a `shared_ptr` to `B` and `B` holds a `shared_ptr` to `A`, both counts are at least 1 forever, and neither is freed. Python's garbage collector detects cycles; `shared_ptr` does not.

### 12.3 `std::weak_ptr<T>` — non-owning observer that knows when the object died

A `weak_ptr` refers to an object owned by `shared_ptr`s without keeping it alive. To use it you `lock()`, which returns a `shared_ptr` (empty if the object is gone):

```cpp
std::weak_ptr<Matrix> w = b;         // does not change use_count
if (auto sp = w.lock()) {            // sp is a shared_ptr, count temporarily +1
    std::cout << sp->rows();
} else {
    std::cout << "object already destroyed\n";
}
```

Use `weak_ptr` for the back-edges of a graph: parent → child is `shared_ptr` (owning), child → parent is `weak_ptr` (observing). Cycle broken.

### 12.4 Raw pointers and references are fine — for non-owning access

A function that *uses* an object but does not decide when it dies should take `T&` (must exist) or `T*` (may be null). The C course's habit of passing `Matrix *` everywhere is not wrong in C++; what changes is that a raw pointer now *means* "not mine to delete". Write that in a comment or, better, make it the project convention: **raw pointer = observer, smart pointer = owner**. Never `delete` a raw pointer in modern code — if you find yourself doing it, the owner should have been a `unique_ptr`.

| Type                  | Ownership           | Copyable | Size            | When                                              |
|-----------------------|---------------------|----------|-----------------|---------------------------------------------------|
| `T` (value)           | itself              | yes      | `sizeof(T)`     | default; small or Rule-of-Zero types              |
| `std::unique_ptr<T>`  | sole                | move-only| 1 pointer       | heap object with one owner; polymorphic members   |
| `std::shared_ptr<T>`  | shared              | yes      | 2 pointers + block | genuinely shared lifetime (graph nodes, caches) |
| `std::weak_ptr<T>`    | none, can check     | yes      | 2 pointers      | back-references, caches                           |
| `T*`, `T&`            | none                | yes      | 1 pointer       | function parameters, observation                  |

## 13. Ownership as a design question: the autograd graph

micrograd's `Value` (and PyTorch's autograd `Node`) is a DAG: each node knows the nodes it was computed *from* (its children, in micrograd's naming) so that `backward()` can walk from the loss to the leaves. Who owns a node?

- Nodes are created by operators (`a * b` creates a fresh node) and referenced by the user (`loss`) *and* by other nodes (`loss`'s parent chain). Multiple owners → `std::shared_ptr<Node>`.
- Parent → child (the operands it was built from) must keep the operands alive as long as the result exists, or backward would dereference freed memory → `std::vector<std::shared_ptr<Node>> children;` (owning).
- Child → parent is *not* needed for backward (we walk from the loss down). If you want it (e.g. to count how many consumers a node has for gradient accumulation), it must be `std::weak_ptr<Node>` or the graph cycles and leaks.

```cpp
struct Node;
using NodePtr = std::shared_ptr<Node>;
struct Node {
    double data = 0, grad = 0;
    std::vector<NodePtr> children;                 // owning: keeps operands alive
    std::vector<std::weak_ptr<Node>> parents;      // observing: breaks the cycle
    std::function<void()> backward_fn;             // chapter 08
};
NodePtr add(const NodePtr& a, const NodePtr& b) {
    auto out = std::make_shared<Node>();
    out->data = a->data + b->data;
    out->children = {a, b};
    a->parents.push_back(out);                     // shared_ptr → weak_ptr conversion
    b->parents.push_back(out);
    return out;
}
```

When the user drops `loss`, the whole graph is freed in one cascade, leaves last — exactly PyTorch's behaviour when a tensor's graph goes out of scope. Cost: one atomic op per pointer copy; fine per node, not fine per element, which is why tensor autograd tracks *tensors*, not scalars.

## 14. Passing smart pointers to functions — guidelines

| You want to express                        | Parameter type                    |
|--------------------------------------------|-----------------------------------|
| "I will use it, not keep it"               | `T&` or `const T&` (or `T*` if nullable) |
| "I take ownership" (sink)                  | `std::unique_ptr<T>` by value; caller writes `std::move` |
| "I will share ownership" (store a copy)    | `std::shared_ptr<T>` by value     |
| "I might share ownership, decided inside"  | `const std::shared_ptr<T>&`       |
| "I want to reseat the caller's pointer"    | `std::unique_ptr<T>&` (rare)      |

Do not pass `const std::unique_ptr<T>&` just to "look at" the object — that forces the caller to have a `unique_ptr` at all. Pass `T&`; `*p` or `p.get()` at the call site.

## 15. `std::optional<T>` — "maybe a value" is not a pointer

C returned `-1`, `NULL`, or `NAN` for "no result", and you had to remember the convention. C++17's `std::optional<T>` holds either a `T` or nothing, inline (no heap), and forces the caller to check:

```cpp
#include <optional>

std::optional<double> safe_sqrt(double x) {
    if (x < 0) return std::nullopt;
    return std::sqrt(x);
}
std::optional<std::size_t> find_index(const std::vector<double>& v, double target);

if (auto r = safe_sqrt(-1.0)) std::cout << *r;     // operator bool + operator*
else std::cout << "no real root\n";
double y = safe_sqrt(4.0).value();                  // throws std::bad_optional_access if empty
double z = safe_sqrt(-1.0).value_or(0.0);           // default
```

Use `optional` for return values and optional fields; use pointers only when the object lives elsewhere. `std::optional<Matrix>` is `sizeof(Matrix) + 1 byte (+ padding)`, and the `Matrix` inside is moved in and out like any other.

Python equivalent: returning `None` — except the type system now makes you handle it.

## 16. Converting the C `Layer` vtable struct to `std::vector<std::unique_ptr<Layer>>`

In `../../c_learning/11_function_pointers_and_generics/lesson.md` a layer was a struct with function pointers plus a `void *state`, stored in an array of structs, freed by a hand-written `layer_free` per kind. In C++:

```cpp
struct Layer {                                       // abstract base (chapter 09 in full)
    virtual ~Layer() = default;                      // MANDATORY for delete through base pointer
    virtual Matrix forward(const Matrix& x) = 0;
};
struct Linear : Layer {
    Matrix W, b;                                     // Rule of Zero: vectors inside Matrix
    Linear(std::size_t in, std::size_t out) : W(in, out), b(1, out) {}
    Matrix forward(const Matrix& x) override { return x * W + b; }   // rows broadcast omitted
};
struct ReLU : Layer {
    Matrix forward(const Matrix& x) override { /* max(0,x) */ return x; }
};

std::vector<std::unique_ptr<Layer>> net;             // owns heterogeneous layers
net.push_back(std::make_unique<Linear>(784, 128));
net.push_back(std::make_unique<ReLU>());
net.push_back(std::make_unique<Linear>(128, 10));

Matrix h = x;
for (auto& layer : net) h = layer->forward(h);       // `auto&`: do not copy (cannot — move-only)
// end of scope: vector destroys each unique_ptr, each deletes its Layer via the virtual destructor
```

No `layer_free`, no `switch (kind)`, no leak if `forward` throws halfway. The vector can grow (relocating `unique_ptr`s is a `noexcept` move of one pointer each). This is the skeleton of your C++ MLP; chapter 09 fills in the polymorphism.

## Gotchas and undefined behavior

- **Forgetting to null the source in a move constructor**: two objects own one buffer; the second destructor does `delete[]` on freed memory — UB (double free).
- **Using a moved-from object as if it still had its contents**: not UB for standard types (valid-but-unspecified), but a logic bug you will not see in tests that never reallocate.
- **`std::move` on a `const` object** silently copies. `-Wall` does not warn. Clang has `-Wpessimizing-move` and `-Wredundant-move` for some cases; turn them on.
- **`return std::move(local);`** disables copy elision. Just `return local;`.
- **Writing a destructor without the other four** deletes the implicit move operations. Your class now copies on every return. Use `= default` for the rest.
- **Move constructor not `noexcept`**: `std::vector` copies on reallocation. Benchmark disappears silently.
- **`shared_ptr<T>(new T)`** does two allocations and can leak if another argument throws. Always `make_shared`.
- **Two `shared_ptr`s constructed from the same raw pointer** (`shared_ptr<T> a(p), b(p);`) — two control blocks, double delete, UB. Copy the `shared_ptr`, never re-wrap the raw pointer.
- **`shared_ptr` cycles** leak silently; use `weak_ptr` for back-edges. Check with `use_count()` in a test or the leak sanitizer.
- **Deleting through a base pointer without a `virtual` destructor** is UB (only the base part is destroyed; the derived members leak). `unique_ptr<Base>` does not save you — it calls `delete` on a `Base*`. Chapter 09 repeats this because it matters.
- **`p.release()` without capturing the result** leaks. `release()` hands *you* the responsibility.
- **`std::optional<T&>` does not exist.** Use `T*` or `std::reference_wrapper<T>` for optional references.
- **Dereferencing an empty `optional` with `*`** is UB (not an exception). `.value()` throws; `*` does not check.

## Common mistakes checklist

- [ ] Every class is Rule of Zero (members: `vector`, `string`, `unique_ptr`) *or* Rule of Five — never something in between.
- [ ] Move operations are `noexcept` and leave the source empty.
- [ ] No `return std::move(x);`; no `std::move` on `const` objects.
- [ ] No naked `new`/`delete` outside of a Rule-of-Five class or a custom deleter. `make_unique`/`make_shared` everywhere else.
- [ ] `unique_ptr` by default; `shared_ptr` only when lifetime is genuinely shared; `weak_ptr` on every back-edge.
- [ ] Functions that only *use* an object take `T&`/`const T&`/`T*`, not a smart pointer.
- [ ] "Maybe a value" is `std::optional<T>`, not `T*` or a sentinel.
- [ ] Containers of polymorphic objects are `std::vector<std::unique_ptr<Base>>` and `Base` has `virtual ~Base()`.
- [ ] `std::move(local)` when pushing a named local into a container you will not use it after.

## You can move on when...

- You can explain, in one sentence each, what an lvalue, prvalue and xvalue are, and classify `x`, `x + 1`, `std::move(x)`, `f()` (returning `T`) and `g()` (returning `T&`).
- You can write a move constructor and move assignment for a raw-buffer class from memory, including `noexcept` and nulling the source, and say what breaks if you forget the last step.
- You can state what `std::move` actually does and give an example where it results in a copy.
- You can say how many special members a Rule-of-Zero `Matrix` needs (none) and why `return result;` in `matmul` costs nothing in C++17.
- You can choose between `unique_ptr`, `shared_ptr`, `weak_ptr` and a raw pointer for: a layer in a network, a node in an autograd graph, a node's back-reference to its consumer, a function parameter that just reads a matrix.
- You can explain why `std::vector<std::unique_ptr<Layer>>` frees everything correctly and what one missing keyword in `Layer` would make that UB.
- You reach for `std::optional` when a function may have no result.
