# Chapter 03 — References, `const`, and Value Semantics

## What you'll be able to do after this chapter

- State precisely what an lvalue reference is, what it cannot do (be null, be rebound), and when to use it instead of a pointer.
- Write accessor pairs like `double& at(i,j)` / `double at(i,j) const`, and recognize the dangling-reference bug in a code review.
- Explain C++ value semantics (`a = b` copies) and contrast it with NumPy views and Python object references.
- Decide, for any parameter or return, between by-value, `const T&`, `T&`, and pointer — including why returning a big `Matrix` by value is fast (copy elision / RVO).
- Avoid the accidental-copy trap in range-based `for`, use `mutable` correctly, and store "references" in containers via pointers or `std::reference_wrapper`.

## Why this matters for ML / numerics / sims

Numerical code is dominated by passing large arrays around. Copy a 1000×1000 matrix by accident once per iteration and a fast algorithm becomes a slow one — and the compiler will not tell you. Alias two buffers by accident and an in-place update corrupts the input. The rules in this chapter — what copies, what aliases, when a reference is valid — are exactly the distinction between `a = b` and `a = b.copy()` in NumPy, except that C++ defaults the other way and enforces it at compile time. Getting this right is how you write a `matmul(const Matrix&, const Matrix&)` that never copies its inputs and an autograd `Tensor` whose gradients point at the right storage.

---

## 1. Lvalue references in depth

`T&` declares a reference: a name that denotes an existing object. The compiler typically implements it as a pointer, but the language treats it as the object itself.

```cpp
int x = 5;
int &r = x;        // r IS x from now on
r = 7;             // x == 7
int y = 10;
r = y;             // does NOT rebind r to y: it assigns y's value to x. x == 10.
int &bad;          // ERROR: a reference must be initialized
// int &n = nullptr; // ERROR: no null references
// int &z = 5;       // ERROR: non-const lvalue reference cannot bind to a temporary
const int &c = 5;  // OK: const& CAN bind to a temporary (lifetime extended to c's scope)
```

Three rules that distinguish references from pointers:

| Property | `T*` | `T&` |
|---|---|---|
| Can be null | yes | no — must bind to an object at creation |
| Can be reseated | yes (`p = &other`) | no — every later `r = ...` assigns *through* it |
| Arithmetic / indexing | `p + 1`, `p[i]` | none — it is not an address |
| Syntax at use | `*p`, `p->m` | `r`, `r.m` |
| `sizeof(r)` | pointer size | size of the referred-to `T` |

Because a reference is always bound and never null, a function taking `T&` does not need a null check, and the reader knows the argument is a real object. Use a pointer when "no object" is a legitimate value or when you need to re-point.

Python equivalent: there is none exactly. Python names are closer to *rebindable* pointers (`a = b` rebinds `a`). A C++ reference is a name permanently glued to one object.

## 2. `const&` parameters

```cpp
double sum(const std::vector<double> &v) {     // no copy; cannot modify v
    double s = 0.0;
    for (double x : v) s += x;
    return s;
}
sum(my_vec);                 // binds to my_vec directly
sum({1.0, 2.0, 3.0});        // binds to a temporary vector; legal because const&
```

`const T&` accepts lvalues, temporaries, and literals. `T&` (non-const) accepts only lvalues — which is exactly right for an out-parameter, since modifying a temporary would be pointless. The convention:

| Intent | Signature |
|---|---|
| read a small type (`int`, `double`, pointer, `Vec3`) | `T x` — by value |
| read a large type | `const T &x` |
| modify the caller's object | `T &x` (or `T *x` if you want the call site to show `&`) |
| take ownership / keep a copy | `T x` by value (then `std::move` it in — later chapter) |
| optional argument | `const T *x` (may be `nullptr`) or `std::optional<T>` |

## 3. Returning references: accessors

Returning a reference lets the caller read *and write* an element inside your object without exposing the buffer:

```cpp
class Matrix {
public:
    double &at(int i, int j)       { return data_[i * cols_ + j]; }   // m.at(i,j) = v;
    double  at(int i, int j) const { return data_[i * cols_ + j]; }   // read from const Matrix
    // alternative const version: const double &at(int i, int j) const;
private:
    int rows_, cols_; double *data_;
};

Matrix m(2, 2);
m.at(0, 1) = 3.5;                 // writes into m's buffer
double v = m.at(0, 1);            // reads
const Matrix &cm = m;
// cm.at(0, 1) = 1.0;             // ERROR: const overload returns double (an rvalue), not a reference
```

The non-const overload is chosen for non-const objects, the const overload for const objects and `const&` parameters. This pair is the standard shape for `operator[]`, `operator()`, `.at()`, `.front()`, `.back()` on every container. The returned reference is valid only as long as the object (and its buffer) lives and is not reallocated — see section 16 and `../04_std_vector_string_and_containers/lesson.md` on iterator invalidation.

Returning `double` by value from the const overload costs nothing (8 bytes). For a big element type, return `const T&` instead.

## 4. The dangling reference bug

A reference to an object that no longer exists is *dangling*. Using it is UB — often it "works" in a debug build and breaks at `-O2`.

```cpp
const std::string &make_label(int i) {
    std::string s = "row_" + std::to_string(i);
    return s;                       // ERROR in spirit: s is destroyed at the }; the reference dangles
}                                   // clang: warning: reference to stack memory associated with local variable 's' returned

double &worst() {
    double x = 1.0;
    return x;                       // same bug
}

const Matrix &pick(const Matrix &a, const Matrix &b) { return a.rows() > b.rows() ? a : b; }
const Matrix &r = pick(Matrix(3, 3), Matrix(2, 2));   // temporaries die at end of this statement -> r dangles
```

The compiler warns about the first two (`-Wreturn-stack-address`) but cannot catch the third. Rules:

- Never return a reference to a local variable or to a by-value parameter.
- Returning a reference to a *member* (`at()`), to a `static`, or to something passed in by reference is fine — the object outlives the call.
- When a function returns a reference into one of its arguments, never pass a temporary and keep the result.

Same disease as returning `&local` from a C function; the reference syntax just hides the `&`.

## 5. Value semantics vs reference semantics

In C++, **objects are values**. `Matrix b = a;` creates a second, independent matrix (via the copy constructor — chapter 02). `b = a;` overwrites `b`'s contents with a copy of `a`'s. Same for `int`, `std::string`, `std::vector`, your structs. This is *value semantics*.

Compare:

```python
# Python: names bind to objects; assignment never copies
a = [1, 2, 3]
b = a            # b IS a (same object)
b[0] = 99        # a[0] is 99 too

# NumPy: same, and slices are views
A = np.zeros((3, 3))
B = A            # same array
C = A[0]         # a VIEW into A's memory
C[0] = 1.0       # A[0,0] is 1.0

# Python ints/floats/tuples: immutable, so they *behave* like values
x = 5; y = x; y += 1   # x still 5
```

```cpp
// C++: assignment copies. Always. For everything, unless you ask for a reference.
Matrix A(3, 3);
Matrix B = A;      // independent copy — like NumPy's A.copy()
Matrix &C = A;     // an alias — like NumPy's B = A (no copy, same object)
B.at(0, 0) = 1.0;  // A untouched
C.at(0, 0) = 2.0;  // A.at(0,0) == 2.0
```

So: **C++ objects behave like Python ints (values), not like Python lists (references).** A NumPy view is what you get *only* by explicitly writing `Matrix&`, `const Matrix&`, a pointer, or a view class of your own (a `MatrixView` holding a pointer + strides — a later project).

The upside of value semantics: no aliasing surprises, no "did that function mutate my input?", trivially correct reasoning about ownership. The downside: copying is easy to do by accident and can be expensive. The rest of this chapter is about controlling that.

## 6. When copying is fine and when it is expensive

| Type | Copy cost | Verdict |
|---|---|---|
| `int`, `double`, `bool`, pointers, `enum class` | 1 register move | copy freely |
| `Vec3`, `Particle`, `std::pair<int,int>`, `std::array<double,3>` | a few words, no heap | copy freely |
| `std::string` (short, ≤ ~15 chars) | small; SSO keeps it on the stack | usually fine |
| `std::string` (long), `std::vector<T>`, `Matrix` | heap allocation + O(n) memcpy | avoid unless you need a copy |
| `std::map`, `std::unordered_map` | O(n) allocations | avoid |

The rule: anything that owns a heap buffer costs an allocation plus a copy of every element. In a training loop that runs a million times, a stray copy of a `(784, 128)` weight matrix per step is 800 KB × 10⁶ = an extra terabyte of memory traffic. Pass those by `const&`.

## 7. `const` propagation

`const` on an object makes every member `const` and only `const` member functions callable. It propagates *through* references and pointers you obtain from a `const` object, but **not** through raw pointers stored as members:

```cpp
struct Owner {
    double *buf;                        // pointer member
    double &get(int i) const { return buf[i]; }   // compiles! const applies to the POINTER, not the pointee
};
```

A `const Owner` still lets you write through `buf` — the pointer itself is `double *const`, the doubles are not const. This is *shallow* const, inherited from C. `std::vector<double>` as a member gives *deep* const: on a `const Owner`, `vec[i]` returns `const double&`. One more reason to prefer `std::vector` over raw pointers for owned buffers. (`std::experimental::propagate_const` exists to fix this for pointers; rarely used.)

## 8. `mutable`

Sometimes a `const` member function must legitimately modify a member that is not part of the object's observable state: a cache, a mutex, a call counter for profiling. Mark that member `mutable`:

```cpp
class Matrix {
public:
    double norm() const {
        if (!norm_valid_) {                // compute once, cache
            norm_cache_ = compute_norm();
            norm_valid_ = true;
        }
        return norm_cache_;
    }
    double &at(int i, int j) { norm_valid_ = false; return data_[i * cols_ + j]; }   // writes invalidate
private:
    mutable double norm_cache_ = 0.0;      // may change inside const members
    mutable bool   norm_valid_ = false;
    // ...
};
```

Use `mutable` only for members whose value does not affect what a caller can observe through the public interface. If you find yourself marking real data `mutable`, the function should not be `const`.

## 9. Pass-by-value for small and cheap-to-copy types

For types that fit in one or two registers, pass by value: it is faster than a reference (no indirection, no aliasing worries for the optimizer) and simpler.

```cpp
double lerp(double a, double b, double t) { return a + t * (b - a); }     // not const double&
Vec3 cross(Vec3 a, Vec3 b);                                                // 24 bytes each — by value is fine
int argmax(const std::vector<double> &v);                                  // big: const&
```

A `const double &x` parameter is not wrong, just pointless — and it forces the compiler to assume `x` may alias other memory, which can block vectorization. Threshold: roughly two or three machine words (16–24 bytes). Above that, `const&`.

## 10. Return-by-value and copy elision / RVO

Beginners coming from C fear `Matrix matmul(const Matrix &a, const Matrix &b)` returning a `Matrix` by value: "that copies a huge object!" It does not.

```cpp
Matrix matmul(const Matrix &a, const Matrix &b) {
    Matrix out(a.rows(), b.cols());        // constructed...
    // ... fill out ...
    return out;                            // ...directly in the caller's storage. No copy.
}
Matrix C = matmul(A, B);                   // C IS the `out` object. One constructor call total.
```

Two mechanisms:

- **Guaranteed copy elision (C++17)**: returning a *prvalue* — `return Matrix(r, c);` or `return {r, c};` — never copies. The object is constructed in the caller's slot. This is the standard, not an optimization.
- **Named Return Value Optimization (NRVO)**: returning a named local (`return out;`) is *almost always* elided by every compiler at `-O2` (and usually at `-O0`). Not guaranteed by the standard, but relied upon universally. If it does not happen, C++11 *move semantics* (later chapter) kick in and steal the buffer pointer instead of copying it — still O(1).

So: **return big objects by value.** It is clean, fast, and lets the caller decide the storage. Do not write C-style `void matmul(const Matrix&, const Matrix&, Matrix *out)` unless you specifically need to reuse a preallocated buffer in a hot loop (which is a legitimate optimization — and the by-value version is what you write first). Two things defeat NRVO: returning different named objects on different paths, and returning a parameter.

## 11. `const` methods and `const` objects

Recap from chapter 02 with the value-semantics angle: `const` is the compiler-checked promise "this function does not change the observable state". Declaring locals `const` when they will not change is good hygiene:

```cpp
const int n = v.size();                    // documents intent; the compiler stops accidental writes
const Matrix I = Matrix::identity(3);
const auto &row = table[i];                // read-only alias into the table
```

A `const` object can only be constructed and destroyed, have `const` members called, and be copied *from*. `const` is also part of the type system: `const Matrix&` and `Matrix&` are different types, which is how the accessor overloads in section 3 are selected.

## 12. `const_cast` and why you rarely need it

`const_cast<T&>(x)` or `const_cast<T*>(p)` removes `const`. Legitimate uses are few:

- Calling a C API that takes `char*` but is documented not to write (`legacy_puts(const_cast<char*>(s.c_str()))`).
- Implementing a non-const accessor in terms of the const one to avoid duplicating logic:

```cpp
const double &at(int i, int j) const { /* bounds check, compute index */ return data_[i * cols_ + j]; }
double &at(int i, int j) { return const_cast<double &>(static_cast<const Matrix &>(*this).at(i, j)); }
```

**Writing to an object that was *defined* `const` through a `const_cast` is UB**, even if it compiles. `const_cast` is only safe when the underlying object is actually non-const and merely reached through a `const` path. If you are reaching for `const_cast` to silence an error, the error is usually a missing `const` on some member function.

## 13. `auto&` / `const auto&` in range-for: the accidental-copy trap

```cpp
std::vector<Matrix> layers = ...;            // say 10 matrices of 1 MB each

for (auto m : layers)        { ... }         // COPIES every Matrix: 10 MB of allocation and memcpy per loop
for (const auto &m : layers) { ... }         // no copy; read-only  <-- the default you should type
for (auto &m : layers)       { m.scale(0.5); } // no copy; modifies the elements in place
```

`auto` alone deduces `Matrix`, so each iteration constructs a copy. Worse, `for (auto m : layers) m.scale(0.5);` compiles, runs, and does *nothing* to `layers` — you scaled the copies. This is the single most common performance-and-correctness bug in range-for. Habit: write `const auto &` unless you know the element is tiny or you need to mutate (`auto &`).

One exception: for a proxy type like `std::vector<bool>`, `auto &` fails to compile; use `auto &&` (a forwarding reference — later) or plain `auto`.

## 14. `std::ref` / `std::cref` (mention)

Some templates (`std::thread`, `std::bind`, `std::make_pair`) copy their arguments. To pass a reference through them, wrap it: `std::ref(x)` produces a `std::reference_wrapper<T>` that the template copies cheaply and that converts back to `T&`. `std::cref` for `const`. You will meet this when you start threads for a parallel N-body update: `std::thread t(update_chunk, std::ref(particles), begin, end);`.

## 15. `decltype` (mention)

`decltype(expr)` yields the type of an expression without evaluating it. Useful in templates (`decltype(a * b)` for "whatever multiplying these gives") and for declaring a variable of the same type as another without repeating a long name. `decltype(auto)` as a return type preserves references where plain `auto` would strip them. Chapter 05 uses `decltype` for a `dot` that mixes `float` and `double`.

## 16. Temporary lifetime rules

A temporary (the result of `Matrix(2,2)`, `a + b`, `std::to_string(i)`) lives until the end of the *full expression* it appears in — the semicolon — and is then destroyed. Two important exceptions and one trap:

```cpp
const std::string &s = std::to_string(42);   // binding a temporary to const& (or &&) EXTENDS its life to s's scope. OK.
const char *p = std::to_string(42).c_str();  // TRAP: the string dies at the ';', p dangles immediately
std::cout << std::to_string(42).c_str();     // fine: used within the same full expression

for (char c : get_string()) { ... }          // OK: range-for keeps the range expression alive for the loop
for (char c : get_object().name()) { ... }   // TRAP (pre-C++23): get_object()'s temporary dies before the loop body; name() dangles
```

Lifetime extension applies only to the *directly* bound temporary, not to references obtained through a member call on it. When in doubt, name the temporary: `auto obj = get_object(); for (char c : obj.name())`.

## 17. References in containers: `std::vector<T&>` does not exist

You cannot have `std::vector<Matrix&>`. References are not objects: they have no size, cannot be default-constructed, cannot be reassigned — and a vector must do all three. Options:

| Want | Use | Notes |
|---|---|---|
| a list of non-owning handles to existing objects | `std::vector<Matrix*>` | may be null; classic and clear; the objects must outlive the vector |
| the same, but never null and usable like a reference | `std::vector<std::reference_wrapper<Matrix>>` | `v[i].get()` or implicit conversion to `Matrix&`; `<functional>` |
| the container to *own* the objects | `std::vector<Matrix>` | value semantics; elements move when the vector reallocates, so do not keep references across `push_back` |
| shared ownership between several containers | `std::vector<std::shared_ptr<Matrix>>` | later chapter (smart pointers) |

For an autograd graph where each node must refer to its parents without owning them, `std::vector<Node*>` (or indices into a master `std::vector<Node>`) is the usual answer. Indices are the most robust: they survive reallocation.

---

## Gotchas and undefined behavior

- **Returning a reference to a local** — UB; clang warns (`-Wreturn-stack-address`) in the direct case but not through indirection.
- **Keeping a reference/pointer into a `std::vector` across `push_back`** — reallocation moves the elements; the reference dangles. UB. Reserve up front or use indices.
- **`const char *p = temp_string.c_str();`** on a temporary — dangles at the semicolon.
- **`for (auto x : big_container)`** — silent copies; mutations are lost. Use `const auto &` / `auto &`.
- **`const_cast` then write** on an object defined `const` — UB, regardless of what appears to happen.
- **Shallow const through raw pointer members** — a `const` object can still be mutated through `double *buf`. Hold `std::vector` for deep const.
- **Binding `T&` to a temporary** does not compile; binding `const T&` does and extends the lifetime — but only for the temporary itself, not for sub-objects reached via member calls.
- **Assigning to a reference does not rebind it** — `r = y;` writes into the referent. If you need to re-point, you want a pointer.
- **`auto` strips `const` and `&`** — `auto x = cref_to_matrix;` copies the matrix. `const auto &x = ...` does not.
- **Aliased in-place update**: `void add_into(const Matrix &a, Matrix &out)` called as `add_into(m, m)` — `a` and `out` are the same object. Value semantics protect you only when you *copy*; with references you must think about aliasing exactly as with C pointers (this is why `restrict` exists in C).
- **Reference members** in a class delete the copy assignment operator (a reference cannot be reseated), and make the class non-default-constructible. Prefer pointer members or `std::reference_wrapper` if the class must be assignable.

## Common mistakes checklist

- [ ] Big parameters are `const T&`; small ones are by value; out-params are `T&` (or pointer, consistently).
- [ ] No function returns a reference to a local or a by-value parameter.
- [ ] Every range-for over a container of non-trivial elements uses `const auto &` or `auto &`.
- [ ] Accessors come in const / non-const pairs.
- [ ] Big results are returned by value, relying on copy elision — not via output pointers (unless buffer reuse is a measured need).
- [ ] No `const_cast` except at a C API boundary or in the const-overload-forwarding idiom.
- [ ] `mutable` only on caches / counters / mutexes, never on state a caller can observe.
- [ ] Containers hold values, pointers, or `std::reference_wrapper` — never `T&`.
- [ ] No pointer/reference into a `vector` is kept across a possible reallocation.

## You can move on when...

- You can list three things a pointer can do that a reference cannot, and say why each restriction makes references safer for parameters.
- You can explain why `Matrix C = matmul(A, B);` performs exactly one construction and zero copies, and name the two mechanisms involved.
- You can look at `for (auto m : layers) m.scale(0.5);` and say precisely what it does (and does not) do.
- You can write the "NumPy view vs copy" analogy for C++ correctly: which C++ construct corresponds to `B = A` and which to `B = A.copy()`.
- You can find the dangling reference in `const std::string &r = get_config().name();` and fix it in one line.
- You can explain why `std::vector<double&>` is illegal and choose between `T*`, `reference_wrapper<T>`, and indices for an autograd node's parent list.
