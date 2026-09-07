# Chapter 02 — Classes and RAII

## What you'll be able to do after this chapter

- Write a `Matrix` class that allocates its buffer in the constructor and frees it in the destructor, so that no code path — early `return`, error, exception — can leak it.
- Explain RAII and why it replaces C's `mat_free` + `goto cleanup` discipline.
- Diagnose and fix the shallow-copy double-free bug with the Rule of Three (copy constructor, copy assignment, destructor), including self-assignment and the copy-and-swap idiom.
- Use `explicit`, `const` member functions, `static` members, `friend`, `= default`, `= delete`, and access specifiers deliberately.
- Decide when a plain aggregate `struct` is the right tool and when a class with invariants is, and split a class across `.h`/`.cpp` or keep it header-only.

## Why this matters for ML / numerics / sims

Every project in this course owns memory: matrices, token tables, particle arrays, field grids. In the C course you paired every `mat_alloc` with a `mat_free` and used `goto cleanup` when a function had three allocations and four ways to fail. That works, but every reviewer of every C codebase has found leaks and double-frees anyway. RAII makes the *compiler* insert the `free` at every exit. A `Matrix` that cleans up after itself is the foundation for `Tensor` in your autograd engine, `Grid` in FDTD, and `Vocab` in BPE. Get this chapter right and you will not write a memory leak for the rest of the course.

---

## 1. `struct` vs `class`

In C++ both keywords define a class type. The **only** difference is the default access: `struct` members are `public` by default, `class` members are `private` by default.

```cpp
struct A { int x; };          // x is public
class  B { int x; };          // x is private
class  C { public: int x; };  // identical to A
```

Convention: use `struct` for plain bundles of data with no invariants (a `Particle` with `x, y, z`), and `class` for types that maintain an invariant (a `Matrix` whose `data` pointer must always hold exactly `rows*cols` doubles). The keyword signals intent to the reader; the compiler does not care.

## 2. Members and member functions, `this`

A member function is a function declared inside the class. It is called on an object with `.` (or `->` through a pointer) and can access that object's members directly.

```cpp
struct Vec2 {
    double x, y;

    double norm() const { return std::sqrt(x * x + y * y); }   // reads x, y of *this*
    void scale(double s) { x *= s; y *= s; }                  // modifies *this*
};

Vec2 v{3.0, 4.0};
v.norm();        // 5.0  — equivalent to the C call  vec2_norm(&v)
v.scale(2.0);    // v is now {6, 8}
```

Inside a member function, `this` is a pointer to the object the function was called on (`Vec2*`, or `const Vec2*` in a `const` member function). `x` inside `norm()` means `this->x`. You write `this->` explicitly only when a parameter name shadows a member, or when you need the pointer itself (`return *this;` for chaining).

Python equivalent: `self`. The difference is that C++ passes it implicitly — you never write it in the parameter list.

## 3. Constructors

A constructor is a member function with the class's name and no return type. It runs when an object is created and its job is to establish the class invariant.

```cpp
class Matrix {
public:
    Matrix();                            // default constructor: Matrix m;
    Matrix(int rows, int cols);          // parameterized: Matrix m(3, 4);
    Matrix(int rows, int cols, double fill);
    // ...
private:
    int rows_, cols_;
    double *data_;
};
```

### Member initializer lists

```cpp
Matrix::Matrix(int rows, int cols)
    : rows_(rows), cols_(cols), data_(new double[rows * cols]()) {   // initializer list
    // body: runs after all members are initialized
}
```

The `: member(value), ...` list *initializes* members. Assigning in the body (`rows_ = rows;`) *default-initializes then assigns* — for `int` it makes no difference, but for members with constructors (a `std::string`, a `std::vector`) it does redundant work, and for `const` members or references it is the **only** way to set them. Always use the initializer list.

**Member init order is declaration order, not initializer-list order.** This is a real bug source:

```cpp
class Bad {
    double *data_;       // declared first -> initialized first
    int n_;              // declared second
public:
    Bad(int n) : n_(n), data_(new double[n_]) {}   // n_ is NOT yet set when data_ is initialized -> UB
};
```

`-Wall` (`-Wreorder`) warns when the list order differs from the declaration order. Keep them the same and never let one member's initializer read another declared later.

### Delegating and default member initializers

```cpp
class Matrix {
    int rows_ = 0, cols_ = 0;            // default member initializers (C++11)
    double *data_ = nullptr;
public:
    Matrix() = default;                  // uses the defaults above -> an empty, valid matrix
    Matrix(int r, int c) : Matrix(r, c, 0.0) {}      // delegates to the 3-arg constructor
    Matrix(int r, int c, double fill);
};
```

## 4. Destructor

`~Matrix()` runs automatically when the object's lifetime ends: at the closing `}` of the scope for locals, at `delete` for heap objects, when the containing object is destroyed for members, when a `std::vector<Matrix>` shrinks or dies. You never call it by hand.

```cpp
Matrix::~Matrix() { delete[] data_; }    // delete[] on nullptr is a no-op, so an empty Matrix is safe
```

Destruction order is the reverse of construction: locals in reverse declaration order, members in reverse declaration order, then the enclosing object.

## 5. RAII — the single most important C++ idea

**Resource Acquisition Is Initialization**: acquire a resource (memory, file, lock, socket) in a constructor; release it in the destructor. Then the *language* guarantees release on every exit path, because destructors run on scope exit no matter how the scope is exited.

Compare with C:

```c
/* C: every exit path must remember every free */
int solve(int n) {
    Mat *a = mat_alloc(n, n);   if (!a) return -1;
    Mat *b = mat_alloc(n, 1);   if (!b) { mat_free(a); return -1; }
    Mat *x = mat_alloc(n, 1);   if (!x) { mat_free(a); mat_free(b); return -1; }
    int rc = 0;
    if (fill(a, b) != 0) { rc = -1; goto cleanup; }
    if (lu_solve(a, b, x) != 0) { rc = -1; goto cleanup; }
    print(x);
cleanup:
    mat_free(x); mat_free(b); mat_free(a);
    return rc;
}
```

```cpp
// C++: the destructor is the cleanup label, inserted by the compiler at every exit
int solve(int n) {
    Matrix a(n, n), b(n, 1), x(n, 1);
    if (fill(a, b) != 0) return -1;          // a, b, x destroyed here
    if (lu_solve(a, b, x) != 0) return -1;   // and here
    print(x);
    return 0;                                // and here
}
```

Any `return`, `break`, reaching the end of the block, or a thrown exception (later chapter) passing through this scope destroys `x`, then `b`, then `a`. This is **stack unwinding**. You cannot forget the free because there is no free to write.

RAII is not just for memory. `std::ofstream` closes its file in its destructor. `std::lock_guard` unlocks a mutex. Your own classes can wrap a `FILE*`, a CUDA buffer, a timer that prints elapsed time when it dies. The pattern is always the same: constructor acquires, destructor releases.

Python equivalent: `with open(...) as f:` — but applied to *every* object automatically, not only the ones you remember to put in a `with` block. (CPython's refcounting gets close for simple cases; C++ makes it deterministic and free.)

## 6. Running example: `Matrix` managing a heap buffer

```cpp
class Matrix {
public:
    Matrix(int rows, int cols, double fill = 0.0)
        : rows_(rows), cols_(cols), data_(new double[static_cast<std::size_t>(rows) * cols]) {
        for (int i = 0; i < rows_ * cols_; i++) data_[i] = fill;
    }
    ~Matrix() { delete[] data_; }

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    double  at(int i, int j) const { return data_[i * cols_ + j]; }
    double &at(int i, int j)       { return data_[i * cols_ + j]; }

private:
    int rows_, cols_;
    double *data_;
};

int main() {
    Matrix m(2, 3, 1.0);
    m.at(1, 2) = 7.0;
    std::cout << m.at(1, 2) << '\n';    // 7
}   // ~Matrix runs here: delete[] data_
```

Memory layout — the object is small and lives on the stack; the buffer lives on the heap:

```
stack                       heap
+-----------+
| rows_ = 2 |
| cols_ = 3 |
| data_  ---+----------->  [1.0][1.0][1.0][1.0][1.0][7.0]
+-----------+              row 0          row 1
```

Row-major, `index = i * cols + j` — exactly your C layout and NumPy's default (`order='C'`).

## 7. The shallow-copy double-free bug and the Rule of Three

If you do not write a copy constructor, the compiler generates one that copies each member — for `data_`, it copies the *pointer*. Two objects now share one buffer:

```cpp
Matrix a(2, 2, 1.0);
Matrix b = a;         // compiler-generated copy: b.data_ == a.data_
b.at(0, 0) = 99.0;    // a.at(0,0) is now 99 too!
// end of scope: ~b deletes the buffer, then ~a deletes it AGAIN -> double free -> UB, usually a crash
```

This is the same bug as `Mat b = *a;` in C, except in C nobody would expect struct assignment to deep-copy. In C++ users *do* expect `b = a` to make an independent copy, because that is what `int`, `std::string`, and `std::vector` do.

**Rule of Three**: if a class needs a custom destructor, it almost certainly needs a custom copy constructor and copy assignment operator too — all three manage the same resource.

```cpp
class Matrix {
public:
    // copy constructor: build a NEW object from an existing one
    Matrix(const Matrix &other)
        : rows_(other.rows_), cols_(other.cols_),
          data_(new double[static_cast<std::size_t>(other.rows_) * other.cols_]) {
        std::copy(other.data_, other.data_ + rows_ * cols_, data_);   // <algorithm>
    }

    // copy assignment: overwrite an EXISTING object with a copy of another
    Matrix &operator=(const Matrix &other) {
        if (this == &other) return *this;             // self-assignment check (section 15)
        double *fresh = new double[static_cast<std::size_t>(other.rows_) * other.cols_];
        std::copy(other.data_, other.data_ + other.rows_ * other.cols_, fresh);
        delete[] data_;                               // release old buffer only after the copy succeeded
        data_ = fresh; rows_ = other.rows_; cols_ = other.cols_;
        return *this;                                 // enables a = b = c
    }

    ~Matrix() { delete[] data_; }
    // ...
};
```

When each is used:

```cpp
Matrix a(2, 2);
Matrix b = a;      // copy CONSTRUCTOR (b did not exist)
Matrix c(a);       // copy constructor, other syntax
Matrix d{a};       // copy constructor, brace syntax
c = b;             // copy ASSIGNMENT (c already exists; its old buffer must be released)
f(a);              // copy constructor if f takes Matrix by value
return a;          // copy (or move — chapter on move semantics later) when returning by value
```

(C++11 adds move constructor and move assignment — the "Rule of Five" — so that returning a Matrix from a function does not copy the buffer. That is a later chapter; for now, know that copy elision already removes most copies on return, see `../03_references_const_and_value_semantics/lesson.md`.)

## 8. `= default` and `= delete`

```cpp
class Matrix {
public:
    Matrix() = default;                          // "generate the trivial one" — explicit and documented
    Matrix(const Matrix &) = delete;             // "this operation does not exist" — compile error if used
    Matrix &operator=(const Matrix &) = delete;
};
```

`= delete` is how you make a type non-copyable — right for things that must be unique (a file handle, a GPU buffer, a `std::unique_ptr`). Trying to copy gives a clear compile error instead of a runtime double-free. `= delete` also works on ordinary overloads to forbid a conversion: `void f(double); void f(int) = delete;` refuses `f(3)`.

Rule of thumb for any class you write: either all three special members are compiler-generated (the class holds only well-behaved members like `int`, `std::vector`, `std::string`), or you write/delete all three yourself. This is the **Rule of Zero** vs Rule of Three. A `Matrix` built on `std::vector<double>` instead of `double*` needs *none* of them — chapter 04 does exactly that, and it is the version you should actually use. This chapter uses raw `new[]` so you see what the vector does for you.

## 9. `explicit` constructors

A one-argument constructor doubles as an implicit conversion:

```cpp
class Matrix {
public:
    Matrix(int n);                 // n x n identity, say
};
void print(const Matrix &m);
print(5);                          // compiles! constructs a temporary Matrix(5) — surprising
```

Mark it `explicit` to require the conversion be spelled out:

```cpp
    explicit Matrix(int n);
print(5);                          // ERROR
print(Matrix(5));                  // OK
Matrix m = 5;                      // ERROR (copy-initialization needs implicit conversion)
Matrix m(5);  Matrix m{5};         // OK
```

Default to `explicit` on every constructor that can be called with one argument (including ones where the other parameters have defaults). Leave a constructor implicit only when the conversion is genuinely natural (e.g., `Complex` from `double`).

## 10. `const` member functions and const-correctness

A member function marked `const` after its parameter list promises not to modify the object; inside it `this` is `const Matrix*`. Only `const` member functions may be called on a `const` object or through a `const&`.

```cpp
class Matrix {
public:
    int rows() const { return rows_; }                 // OK on const objects
    double trace() const;                              // reads only
    void fill(double v);                               // not const: modifies
};

void report(const Matrix &m) {
    std::cout << m.rows();      // OK
    m.fill(0.0);                // ERROR: fill is not const
}
```

This is the compiler enforcing "functions taking `const Matrix&` do not modify their argument". Without `const` on `rows()`, you could not even ask a `const Matrix&` its size. Mark every member function that does not modify state `const` — get it right from the start, because retrofitting `const` into a codebase is painful ("const poisoning" spreads through every caller).

The `at()` pair above shows the standard **const overload**: the `const` version returns by value (or `const double&`), the non-const version returns `double&` so `m.at(i,j) = v` works. The compiler picks based on whether `m` is const.

## 11. `static` members

A `static` data member belongs to the class, not to any object — one copy total, like a C global with the class name as its namespace. A `static` member function has no `this` and is called as `Matrix::identity(3)`.

```cpp
class Matrix {
public:
    static Matrix identity(int n) {               // "named constructor"
        Matrix m(n, n, 0.0);
        for (int i = 0; i < n; i++) m.at(i, i) = 1.0;
        return m;
    }
    static int live_count() { return live_; }
    static inline int live_ = 0;                  // C++17: define in-class with `inline`
    Matrix(int r, int c, double fill = 0.0) : /*...*/ { ++live_; }
    ~Matrix() { --live_; delete[] data_; }
};

Matrix I = Matrix::identity(3);
std::cout << Matrix::live_count();                // 1
```

Before C++17 a static data member had to be defined once in a `.cpp` file (`int Matrix::live_ = 0;`). `static inline` avoids that. Static member functions are the idiomatic place for factory functions (`Matrix::zeros`, `Matrix::random`) — mirroring `np.zeros`, `torch.eye`.

## 12. `friend`

A `friend` declaration grants a non-member function (or another class) access to private members. The classic use is `operator<<` for printing, which must be a non-member because its left operand is the stream:

```cpp
class Matrix {
    friend std::ostream &operator<<(std::ostream &os, const Matrix &m);
    // ...
};
std::ostream &operator<<(std::ostream &os, const Matrix &m) {
    for (int i = 0; i < m.rows_; i++) {              // m.rows_ is private, but we're a friend
        for (int j = 0; j < m.cols_; j++) os << m.data_[i * m.cols_ + j] << ' ';
        os << '\n';
    }
    return os;
}
std::cout << m;      // works like any other type now
```

Friendship is not inherited or transitive, and is granted by the class, not requested by the function. Use it sparingly; if a function can do its job through the public interface, do not make it a friend. Operator overloading in general gets its own chapter.

## 13. Access control: `public`, `private`, `protected`

| Specifier | Accessible from |
|---|---|
| `public` | anywhere |
| `private` | member functions and friends of this class only |
| `protected` | this class, friends, and derived classes (matters with inheritance — later) |

Access is checked at compile time and per *class*, not per object: a `Matrix` member function may touch another `Matrix`'s private `data_` (that is how the copy constructor works). Private-by-default for `class` is why the invariant "`data_` holds `rows_*cols_` doubles" can be trusted: only member functions can change those three fields, and you can audit them.

## 14. Getters vs public fields — be pragmatic

The reflex "make every field private and add `get_x()`/`set_x()`" produces boilerplate without protection: a setter that just assigns is a public field with more typing. The real question is **does the class have an invariant?**

- `struct Particle { double x, y, z, vx, vy, vz, mass; };` — any combination of values is valid. Public fields. Done.
- `class Matrix` — `data_` must match `rows_*cols_`. Private, with accessors.
- `class Vocab` — `id_to_token` and `token_to_id` must be inverses. Private.

Accessors also let you change representation later (`Matrix` storing `float` vs `double`, or a stride for transposed views) without touching callers. Return small things by value (`int rows() const`), big things by `const&`, and give a `double& at(i,j)` for mutation rather than a `set(i,j,v)`.

## 15. `operator=` self-assignment check

`m = m` looks stupid but happens through aliases: `a[i] = a[j]` when `i == j`, or `*p = *q` when both point to the same object. Without the check, the naive implementation deletes `data_` and then reads from it:

```cpp
Matrix &operator=(const Matrix &other) {
    delete[] data_;                                      // if other IS *this, other.data_ is now dangling
    data_ = new double[other.rows_ * other.cols_];       // ...
    std::copy(other.data_, ..., data_);                  // reading freed memory: UB
}
```

Two fixes. Either `if (this == &other) return *this;` up front, or — better — allocate the new buffer *before* deleting the old one (section 7 does both). The "allocate first" ordering also gives you **exception safety**: if `new` throws, the object is unchanged. Copy-and-swap makes this automatic.

## 16. Copy-and-swap idiom

Write a `swap` that exchanges the members (cheap: three word swaps), then implement assignment as "copy the source, swap with the copy, let the copy's destructor free my old buffer":

```cpp
class Matrix {
public:
    friend void swap(Matrix &a, Matrix &b) noexcept {
        std::swap(a.rows_, b.rows_);
        std::swap(a.cols_, b.cols_);
        std::swap(a.data_, b.data_);
    }
    Matrix &operator=(Matrix other) {   // note: BY VALUE -> the copy constructor already ran
        swap(*this, other);             // *this now owns the copy; `other` owns my old buffer
        return *this;
    }                                   // `other` is destroyed here, freeing the old buffer
};
```

Properties: self-assignment safe (you copy first, then swap — no check needed), strong exception safety (if the copy fails, `*this` is untouched), and the same code works as move-assignment once you add a move constructor. Less code, fewer bugs — the recommended way to write `operator=` for resource-owning classes.

## 17. When a plain `struct` is right: aggregates and POD

An **aggregate** is a class with no user-declared constructors, no private non-static members, no base classes or virtual functions. It can be brace-initialized field by field:

```cpp
struct Body { double pos[3]; double vel[3]; double mass; };
Body earth{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, 5.97e24};
Body bodies[3] = {};                                       // all zero
```

Aggregates with only trivial members are also **trivially copyable**: `std::memcpy` works, they can be written to a binary file with `fwrite`, and arrays of them are contiguous with no hidden data — essential for SIMD-friendly particle arrays and for interop with C. If your type is "some numbers that travel together" (`Vec3`, `Particle`, `Token{int id; int start; int len;}`, a `Complex`), use an aggregate `struct` with public fields and no constructor. Save `class` for types that own a resource or enforce a relationship between fields.

## 18. Separating a class into `.h` / `.cpp`

Same rules as C. The class *definition* (member declarations, private fields) goes in the header so every user knows the object's size and interface. Member function *definitions* go in the `.cpp`, prefixed with `Matrix::`.

```cpp
// matrix.h
#ifndef MATRIX_H
#define MATRIX_H
#include <cstddef>
class Matrix {
public:
    Matrix(int rows, int cols, double fill = 0.0);
    Matrix(const Matrix &other);
    Matrix &operator=(Matrix other);
    ~Matrix();
    int rows() const { return rows_; }          // defined in-class: implicitly inline
    int cols() const { return cols_; }
    double  at(int i, int j) const;
    double &at(int i, int j);
    friend void swap(Matrix &a, Matrix &b) noexcept;
private:
    int rows_, cols_;
    double *data_;
};
#endif
```

```cpp
// matrix.cpp
#include "matrix.h"
#include <algorithm>
Matrix::Matrix(int rows, int cols, double fill)
    : rows_(rows), cols_(cols), data_(new double[static_cast<std::size_t>(rows) * cols]) {
    std::fill(data_, data_ + rows * cols, fill);
}
Matrix::~Matrix() { delete[] data_; }
double  Matrix::at(int i, int j) const { return data_[i * cols_ + j]; }
double &Matrix::at(int i, int j)       { return data_[i * cols_ + j]; }
// ...
```

Build: `c++ -std=c++17 -O2 -c matrix.cpp` then `c++ -std=c++17 -O2 main.cpp matrix.o`. Changing `matrix.cpp` recompiles one file; changing `matrix.h` recompiles every includer — same trade-off as C.

## 19. Header-only classes

Small classes are often defined entirely in the header. Member functions defined inside the class body are implicitly `inline`, so including the header from many `.cpp` files does not cause duplicate symbols. Member functions defined *outside* the class body but still in the header need the `inline` keyword explicitly. Templates (chapter 05) *must* be header-only. Trade-off: zero build setup and better inlining, versus longer compile times and recompiling everything on any change. For this course's project sizes, header-only is fine.

---

## Gotchas and undefined behavior

- **Double free from shallow copy**: any class with a raw owning pointer and a destructor but no copy constructor/assignment. Symptom: crash at scope exit, or `malloc: *** error: pointer being freed was not allocated`. Fix: Rule of Three, `= delete`, or hold a `std::vector` instead (Rule of Zero).
- **Member init order**: members initialize in declaration order regardless of the initializer list. Reading a later-declared member inside an earlier one's initializer is UB. `-Wreorder` catches the ordering mismatch; it cannot catch the dependency itself.
- **Uninitialized members**: a constructor that forgets a member leaves it indeterminate (for scalars). Use default member initializers (`double *data_ = nullptr;`) so every constructor starts from a known state.
- **Destructor on a partially constructed object**: if a constructor throws (later chapter), the destructor does *not* run — but already-initialized members' destructors do. This is another reason to hold resources in members that clean themselves up (`std::vector`), not in raw pointers.
- **`delete` vs `delete[]`**: buffer from `new double[n]` must go to `delete[]`. Mismatch is UB.
- **Returning a reference to a member from an object that dies**: `double &r = Matrix(2,2).at(0,0);` — the temporary is destroyed at the end of the full expression; `r` dangles. Chapter 03.
- **Self-assignment**: `a = a` with a naive `operator=` deletes then reads the buffer. Copy-and-swap avoids the problem structurally.
- **`explicit` omitted**: `void f(const Matrix&)` accepting `f(3)` silently allocates a matrix. Mark single-argument constructors `explicit`.
- **Forgetting `const` on accessors**: shows up later as "cannot call `rows()` on `const Matrix&`". Add `const` when you write the function, not when the error appears.
- **Missing `#include <algorithm>`** for `std::copy`/`std::fill`/`std::swap` (`<utility>` for `std::swap` officially). It may compile by accident on one platform and fail on another.
- **`static` member defined in a header without `inline`**: duplicate symbol at link time. Use `static inline` (C++17) or define once in a `.cpp`.

## Common mistakes checklist

- [ ] Every class with a destructor either has copy ctor + copy assignment, or deletes them, or holds only self-managing members (Rule of Zero).
- [ ] Initializer list order matches declaration order; no member reads a later one.
- [ ] Single-argument constructors are `explicit`.
- [ ] Every non-mutating member function is `const`; accessors have const/non-const overloads where mutation is intended.
- [ ] `operator=` is self-assignment safe (copy-and-swap, or allocate-before-delete).
- [ ] `new[]` ↔ `delete[]`.
- [ ] `struct` with public fields for aggregates; `class` when there is an invariant.
- [ ] Factory functions (`identity`, `zeros`) are `static` member functions.
- [ ] Header has include guards; member function definitions in the header are inside the class body or marked `inline`.

## You can move on when...

- You can write `Matrix` with constructor, destructor, copy constructor, and copy-and-swap assignment from memory, and it runs clean under `-fsanitize=address`.
- You can explain what happens, step by step, in `Matrix b = a; b = c;` — which special member runs at each point and when each buffer is freed.
- You can point at a C function with `goto cleanup` and say which RAII objects would replace which labels.
- You can say why `Particle` should be a `struct` with public fields and `Matrix` should not.
- You can predict the compile error from `print(5)` when `Matrix(int)` is `explicit`, and from `Matrix b = a;` when the copy constructor is `= delete`.
- You know why member init order is declaration order and can construct a bug that depends on it.
