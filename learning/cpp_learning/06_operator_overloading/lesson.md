# Chapter 06 — Operator Overloading

## What you'll be able to do after this chapter

- Write `C = A * B + b;` for your own `Matrix` class and have it mean exactly what you decide it means.
- Know which operators go inside the class (members) and which must be free functions, and why.
- Implement the full arithmetic set (`+ - * /`, `+=`, unary `-`, `==`, `!=`) with the minimum amount of duplicated code.
- Give a class 2D indexing `m(i, j)` with correct `const` behaviour, and print it with `std::cout << m`.
- Build callable objects (functors) such as `Sigmoid`, the direct ancestor of lambdas (chapter 08).
- Recognise the costs hidden in `A * B + C` (temporaries) and know the name of the fix (expression templates).

## Why this matters for ML / numerics / sims

In the C course your matrix code looked like `mat_add(mat_mul(A, B), b)` and you had to free every intermediate by hand. NumPy lets you write `A @ B + b`. Operator overloading is how C++ gets the NumPy syntax *and* the C performance: `A * B + b` becomes calls to functions you wrote, and RAII (`../02_classes_and_raii/lesson.md`) frees the intermediates. Every later project uses this: `Vec3` for N-body forces (`r = b.pos - a.pos; f += r * (G * m / r3);`), complex numbers for the FFT, dual numbers for forward-mode autograd, `Matrix` for the MLP. Get the rules right once here and every numeric type you write afterwards is boring to implement.

## 1. What operator overloading is

When you write `a + b` for built-in types, the compiler emits an add instruction. For class types, the compiler rewrites the expression as a function call:

| You write      | Compiler looks for                                        |
|----------------|-----------------------------------------------------------|
| `a + b`        | `a.operator+(b)` **or** `operator+(a, b)`                 |
| `a += b`       | `a.operator+=(b)` **or** `operator+=(a, b)`               |
| `-a`           | `a.operator-()` **or** `operator-(a)`                     |
| `a[i]`         | `a.operator[](i)` (member only)                           |
| `a(i, j)`      | `a.operator()(i, j)` (member only)                        |
| `a == b`       | `a.operator==(b)` **or** `operator==(a, b)`               |
| `out << a`     | `out.operator<<(a)` **or** `operator<<(out, a)`           |
| `a = b`        | `a.operator=(b)` (member only)                            |

`operator+` is just a function with a strange name. You can call it explicitly: `operator+(a, b)`. Nothing magic happens; overload resolution (`../05_templates/lesson.md` covered the rules) picks the best match.

```cpp
struct P { double x, y; };
P operator+(P a, P b) { return {a.x + b.x, a.y + b.y}; }

int main() {
    P p{1, 2}, q{10, 20};
    P r = p + q;            // calls operator+(p, q)
    P s = operator+(p, q);  // identical, nobody writes this
    std::cout << r.x << ' ' << r.y << '\n';   // 11 22
}
```

Python equivalent: `def __add__(self, other)`. Same idea; the difference is that C++ resolves it at compile time and can inline it, so `p + q` costs the same as writing the two additions by hand.

### What you can and cannot overload

Overloadable: `+ - * / % ^ & | ~ ! = < > += -= *= /= %= ^= &= |= << >> >>= <<= == != <= >= && || ++ -- , ->* -> () [] new delete new[] delete[]` and (C++20) `<=>`.

Not overloadable: `.` `.*` `::` `?:` `sizeof` `typeid`, and you cannot invent new tokens (there is no `@`).

You cannot change precedence, associativity or arity: `a * b + c` always groups as `(a * b) + c`, and `operator*` always takes two operands. You also cannot overload operators for built-in types only — at least one operand must be a class or enum type.

## 2. Member vs non-member operators

An operator can be a member function or a free function. Which one is not a matter of taste; three rules decide it.

**Rule 1: the left operand of a member operator is always `*this`.** So `double * Matrix` *cannot* be a member of `Matrix`, because the left operand is a `double` and you cannot add members to `double`. It must be a free function:

```cpp
Matrix operator*(double s, const Matrix& m);   // free: 2.0 * m works
Matrix operator*(const Matrix& m, double s);   // free: m * 2.0 works
```

**Rule 2: these must be members**: `=`, `[]`, `()`, `->`, and conversion operators. The language requires it.

**Rule 3: symmetric binary operators should be free functions**, so both operands get the same implicit conversions. If `operator+` is a member, `m + 1.0` may work (if you allow `Matrix(double)`) but `1.0 + m` never will.

Compound assignment (`+=`, `*=`) mutates the left operand, which is naturally `*this`, so those are members.

| Operator                 | Member or free?      | Reason                                              |
|--------------------------|----------------------|-----------------------------------------------------|
| `= [] () ->` conversion  | member (required)    | language rule                                       |
| `+= -= *= /=`            | member (convention)  | mutates left operand                                |
| unary `-`, `!`           | member or free       | either; member is common                            |
| `+ - * /` binary         | free (convention)    | symmetry, `scalar * obj`                             |
| `== != < >`              | free (convention)    | symmetry                                            |
| `<< >>` with streams     | free (required)      | left operand is `std::ostream`, not your type       |

### `friend` — free function with access to private members

A free function cannot see `private` members. `friend` grants that. You can declare the friend inside the class and define it right there; it is still a non-member:

```cpp
class Matrix {
    std::vector<double> data_;
    std::size_t rows_, cols_;
public:
    // declared inside, but this is a FREE function, not a member:
    friend Matrix operator+(const Matrix& a, const Matrix& b);
};
```

Prefer to implement free operators in terms of the public interface (`a.rows()`, `a(i,j)`) so you do not need `friend` at all. Use `friend` when the public interface would be too slow (element-by-element through bounds checks) or does not exist.

## 3. The arithmetic set for `Matrix`

Design decision first (see section 10): in this course `*` between two matrices is **matrix multiplication**, elementwise product is a named function `hadamard(a, b)`. Scalar `*` is scaling. Write this in a comment at the top of the class. NumPy chose the opposite (`*` elementwise, `@` matmul); C++ has no `@`, and linear algebra libraries (Eigen, Armadillo) use `*` for matmul, so we follow them.

### 3.1 Compound assignment first, then `+` in terms of `+=`

```cpp
class Matrix {
public:
    Matrix(std::size_t r, std::size_t c, double fill = 0.0)
        : rows_(r), cols_(c), data_(r * c, fill) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }

    Matrix& operator+=(const Matrix& o) {
        check_same_shape(o);
        for (std::size_t i = 0; i < data_.size(); ++i) data_[i] += o.data_[i];
        return *this;            // return reference so a += b += c chains
    }
    Matrix& operator-=(const Matrix& o);      // same pattern
    Matrix& operator*=(double s) {
        for (double& x : data_) x *= s;
        return *this;
    }
    Matrix& operator/=(double s) { return *this *= (1.0 / s); }
private:
    std::size_t rows_, cols_;
    std::vector<double> data_;
    void check_same_shape(const Matrix& o) const {
        if (rows_ != o.rows_ || cols_ != o.cols_)
            throw std::invalid_argument("shape mismatch");
    }
};

// Free functions. Take the LEFT operand BY VALUE: that copy becomes the result.
Matrix operator+(Matrix a, const Matrix& b) { a += b; return a; }
Matrix operator-(Matrix a, const Matrix& b) { a -= b; return a; }
Matrix operator*(Matrix m, double s)        { m *= s;  return m; }
Matrix operator*(double s, Matrix m)        { m *= s;  return m; }
Matrix operator/(Matrix m, double s)        { m /= s;  return m; }
```

Why `Matrix a` by value in `operator+`? We need a copy for the result anyway. Taking the parameter by value makes the compiler do that copy, and when the argument is a temporary (as in `(A * B) + C`) the copy becomes a move (chapter 07) — free. The `return a;` is also a move or elided. One line of logic, no duplication with `+=`.

### 3.2 Matrix multiplication — the one that is not elementwise

```cpp
Matrix operator*(const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows()) throw std::invalid_argument("matmul shape mismatch");
    Matrix c(a.rows(), b.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t k = 0; k < a.cols(); ++k) {
            const double aik = a(i, k);
            for (std::size_t j = 0; j < b.cols(); ++j)
                c(i, j) += aik * b(k, j);      // i-k-j order: inner loop is contiguous
        }
    return c;
}
```

Note that both operands are `const Matrix&` here — no copy is useful, the result has a different shape. There is no `*=` for matmul because `A *= B` would have to change `A`'s shape; leave it out rather than surprise people.

Python equivalent: `A @ B`. Elementwise: `hadamard(A, B)` here, `A * B` in NumPy.

## 4. Unary operators

Unary `-` takes no parameters as a member (the operand is `*this`), or one parameter as a free function.

```cpp
Matrix operator-(const Matrix& m) {       // free version
    Matrix r = m;
    r *= -1.0;
    return r;
}
// Or as a member:  Matrix operator-() const { Matrix r = *this; r *= -1.0; return r; }
```

Unary `+` is rarely useful. `++`/`--` exist in prefix (`T& operator++()`) and postfix (`T operator++(int)` — the dummy `int` parameter distinguishes them) forms; you will write them for iterators (`../05_templates/lesson.md`), not for matrices.

## 5. Comparison

```cpp
bool operator==(const Matrix& a, const Matrix& b) {
    return a.rows() == b.rows() && a.cols() == b.cols() && a.data() == b.data();
    //     std::vector already has ==, compare the underlying storage
}
bool operator!=(const Matrix& a, const Matrix& b) { return !(a == b); }
```

Always define `!=` as `!(a == b)` — never write the logic twice. For floating-point data exact `==` is usually the wrong question; provide `allclose(a, b, rtol, atol)` as a named function (like `np.allclose`) and keep `==` exact so nobody is surprised.

Ordering (`<`, `<=`, ...) makes no sense for matrices; do not define it. For types where it does (a `Fraction`, a `Timestamp`), define `<` and derive the rest: `a > b` is `b < a`, `a <= b` is `!(b < a)`, `a >= b` is `!(a < b)`.

**C++20 note:** `auto operator<=>(const T&) const = default;` generates all six comparison operators at once. Not available with `-std=c++17`; know that it exists so you recognise it in other people's code.

## 6. Subscript: `operator[]` and `operator()` for 2D indexing

`operator[]` takes exactly one argument in C++17 (C++23 allows `m[i, j]`). For a 2D matrix the idiom is `operator()` with two indices: `m(i, j)`.

You need **two overloads**: one that lets you write (`m(i,j) = 5;`) and one that works on a `const Matrix` (reading from a `const Matrix&` parameter). Only the `const` one is callable on a const object.

```cpp
class Matrix {
public:
    double&       operator()(std::size_t i, std::size_t j)       { return data_[i * cols_ + j]; }
    const double& operator()(std::size_t i, std::size_t j) const { return data_[i * cols_ + j]; }
    //  ^ returns a reference INTO the vector, so assignment through it works
    //                                                        ^ const member: callable on const Matrix
    // Row access via [] (returns a pointer to the start of row i, like a C double*)
    double*       operator[](std::size_t i)       { return data_.data() + i * cols_; }
    const double* operator[](std::size_t i) const { return data_.data() + i * cols_; }
};

void print_diag(const Matrix& m) {          // m is const → the const operator() is used
    for (std::size_t i = 0; i < m.rows(); ++i) std::cout << m(i, i) << ' ';
}
Matrix m(3, 3);
m(1, 1) = 7.0;      // non-const overload, returns double&, assignment writes into storage
m[2][0] = 4.0;      // via row pointer
```

Python equivalent: `__getitem__`/`__setitem__` with a tuple key `m[i, j]`. C++ splits get/set into the const/non-const overloads and does both through one returned reference.

Bounds checking: do it in a debug build with `assert(i < rows_ && j < cols_)` (from `<cassert>`; compiled out by `-DNDEBUG`). Unchecked out-of-range access is undefined behaviour exactly as in C — you are indexing into a `std::vector` with `[]`, which does not check.

## 7. Stream output and input: `operator<<`, `operator>>`

`std::cout << m` is `operator<<(std::cout, m)`. The left operand is `std::ostream`, so this **must** be a free function. It returns the stream by reference so that `std::cout << a << b` chains.

```cpp
std::ostream& operator<<(std::ostream& os, const Matrix& m) {
    for (std::size_t i = 0; i < m.rows(); ++i) {
        os << (i == 0 ? "[[" : " [");
        for (std::size_t j = 0; j < m.cols(); ++j)
            os << std::setw(8) << std::setprecision(4) << m(i, j) << (j + 1 < m.cols() ? ", " : "");
        os << (i + 1 < m.rows() ? "],\n" : "]]");
    }
    return os;
}
// Matrix m(2,2); m(0,0)=1; m(1,1)=1; std::cout << m << '\n';
// [[       1,        0],
//  [       0,        1]]
```

Use `os`, not `std::cout`, inside — the same function then works for `std::ofstream` files and `std::ostringstream` strings (`../04_std_vector_string_and_containers/lesson.md`).

Input mirrors it, with `std::istream&` and a non-const reference to fill:

```cpp
std::istream& operator>>(std::istream& is, Matrix& m) {
    for (std::size_t i = 0; i < m.rows(); ++i)
        for (std::size_t j = 0; j < m.cols(); ++j)
            is >> m(i, j);            // stops and sets failbit on bad input, like scanf returning < n
    return is;
}
```

Python equivalent: `__repr__` / `__str__`.

## 8. The function-call operator: functors

`operator()` with any parameter list makes an object *callable*. Such an object is called a **functor** (function object). Unlike a plain function pointer, a functor can carry state in its members and the compiler sees the exact type, so the call is inlined.

```cpp
struct Sigmoid {
    double operator()(double x) const { return 1.0 / (1.0 + std::exp(-x)); }
};
struct LeakyReLU {
    double slope;                                  // state!
    double operator()(double x) const { return x > 0 ? x : slope * x; }
};

template <typename F>
void apply_inplace(Matrix& m, F f) {               // F deduced as Sigmoid / LeakyReLU
    for (std::size_t i = 0; i < m.rows(); ++i)
        for (std::size_t j = 0; j < m.cols(); ++j) m(i, j) = f(m(i, j));
}

Sigmoid sig;
double y = sig(0.0);            // 0.5 — looks like a function call, is a member call
apply_inplace(m, Sigmoid{});
apply_inplace(m, LeakyReLU{0.01});
```

Compare with `../../c_learning/11_function_pointers_and_generics/lesson.md`: in C you passed `double (*f)(double)` plus a `void *ctx` for state and paid an indirect call per element. A functor is the function pointer and the context merged into one object, with no indirect call. Lambdas (chapter 08) are compiler-generated functors — every lambda is secretly a `struct` with an `operator()`.

Python equivalent: `__call__`.

## 9. Conversion operators and `explicit`

A class can define how it converts *to* another type:

```cpp
struct Scalar {                         // a 1x1 result, e.g. from a dot product
    double v;
    operator double() const { return v; }        // implicit conversion Scalar → double
};
Scalar s{3.5};
double d = s + 1.0;    // Scalar converted to double, then added: 4.5
```

Implicit conversions are dangerous in numeric code because they silently pick an overload you did not intend. Mark them `explicit` unless the conversion is genuinely lossless and always wanted:

```cpp
struct Matrix {
    explicit operator bool() const { return !data_.empty(); }   // only in if (m), !m, bool(m)
};
if (m) { ... }               // OK: contextual conversion to bool
double x = m;                // error: explicit — good, this was a bug
```

The same `explicit` keyword on **constructors** (`../02_classes_and_raii/lesson.md`) stops the *other* direction — `double → Matrix`. Without it, `Matrix m = 3.0;` and `f(3.0)` where `f` takes `const Matrix&` would compile and create a matrix you never asked for. Rule: single-argument constructors are `explicit` unless you have a specific reason.

## 10. `operator=` recap

You covered copy assignment in `../02_classes_and_raii/lesson.md`. The shape is:

```cpp
Matrix& operator=(const Matrix& o) {
    if (this != &o) { rows_ = o.rows_; cols_ = o.cols_; data_ = o.data_; }
    return *this;
}
```

For `Matrix` as written here — members are `std::size_t` and `std::vector` — you should not write it at all: the compiler-generated one does exactly this (Rule of Zero, chapter 07). You write `operator=` only when the class owns a raw resource, and then you must also write the destructor and copy constructor (Rule of Three), and in C++11 and later the move operations too (Rule of Five, chapter 07).

## 11. Etiquette — do not surprise the reader

1. **Operators mean what they mean for numbers.** `+` adds, `*` multiplies, `==` compares. Overloading `+` to mean "append to log" or `<<` to mean "push into queue" is legal and hated.
2. **Pick one meaning for `*` on matrices and document it in the class comment.** Matmul (this course, Eigen) or elementwise (NumPy). Offer the other under a name (`hadamard`, `matmul`).
3. **Keep the pairs consistent**: if you define `+`, define `+=` and make `a + b` produce the same as `Matrix t = a; t += b;`. Same for `==`/`!=`.
4. **Return types follow built-ins**: `+=` returns `T&`, `+` returns `T` by value, `==` returns `bool`, `<<` returns `std::ostream&`.
5. **Do not overload `&&`, `||`, `,`.** Overloaded versions lose short-circuit evaluation and evaluation-order guarantees.
6. **Free functions for symmetric operators, in the same namespace as the type** so argument-dependent lookup finds them.
7. **Do not add implicit conversions** to make operators "convenient". Add an explicit overload instead (`operator*(double, Matrix)`).

## 12. Cost: temporaries in `A * B + C`

`A * B + C` is evaluated as `operator+(operator*(A, B), C)`. `operator*` allocates and fills a new `Matrix` (a temporary, an *rvalue*). `operator+` takes its first parameter by value — the temporary is moved into it (no copy, chapter 07), `+=` adds `C` in place, and the result is returned. One allocation total, which is the minimum for an expression whose result is a new matrix. Good.

But `D = A + B + C` with `Matrix operator+(const Matrix&, const Matrix&)` (both by const ref) allocates twice: once for `A + B`, once for `(A+B) + C`. Taking the left operand by value fixes this for chained `+`. Elementwise expressions like `y = a * x + b * x * x + c` still allocate once per operator — five temporaries each the size of `x`, where a hand-written loop would allocate once.

The industrial fix is **expression templates**: `operator+` does not compute anything, it returns a tiny object `Sum<A, B>` that remembers its operands; only `operator=` walks the whole expression tree once per element. Eigen, Blaze and xtensor do this. It is an advanced template technique (`../05_templates/lesson.md` gives you the tools); for this course, when an inner loop matters, write the loop. Know the name so you understand why `Eigen::MatrixXd` expressions have strange types when you hover over them.

Python equivalent: NumPy has the same problem — `a*x + b*x*x + c` allocates five temporaries — which is why `numexpr` and `np.add(a, b, out=c)` exist.

## 13. Second example: `Vec3` for physics

A small fixed-size type: no heap, three doubles, every operator inlined. This is the workhorse of the N-body and PIC simulations.

```cpp
struct Vec3 {
    double x = 0, y = 0, z = 0;

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s)      { x *= s;   y *= s;   z *= s;   return *this; }
    Vec3& operator/=(double s)      { return *this *= 1.0 / s; }
    Vec3  operator-() const         { return {-x, -y, -z}; }
    double&       operator[](int i)       { return i == 0 ? x : i == 1 ? y : z; }
    const double& operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
};
inline Vec3 operator+(Vec3 a, const Vec3& b) { return a += b; }
inline Vec3 operator-(Vec3 a, const Vec3& b) { return a -= b; }
inline Vec3 operator*(Vec3 a, double s)      { return a *= s; }
inline Vec3 operator*(double s, Vec3 a)      { return a *= s; }
inline Vec3 operator/(Vec3 a, double s)      { return a /= s; }
inline bool operator==(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(const Vec3& a, const Vec3& b) { return !(a == b); }

inline double dot(const Vec3& a, const Vec3& b)   { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3   cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline double norm(const Vec3& a)      { return std::sqrt(dot(a, a)); }
inline Vec3   normalized(const Vec3& a){ return a / norm(a); }

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}
```

`dot` and `cross` are named functions, not operators — there is no operator that unambiguously means "cross product" (`^` and `%` have been abused for it; do not). Gravity between two bodies:

```cpp
Vec3 r  = b.pos - a.pos;
double d2 = dot(r, r) + eps2;
Vec3 f  = r * (G * a.m * b.m / (d2 * std::sqrt(d2)));
a.acc += f / a.m;
b.acc -= f / b.m;
std::cout << f << '\n';        // (0.0123, -0.5, 0)
```

Because `Vec3` is 24 bytes with no heap, every temporary lives in registers after optimisation. The temporaries argument of section 12 does not apply — the compiler sees through everything.

## 14. Mention only: `operator new`/`delete` and user-defined literals

- **`operator new` / `operator delete`** can be overloaded per class or globally to route allocations through a custom allocator (memory pool, alignment for SIMD, leak counting). You will not need this until you profile and find `malloc` in the top of the list. Know that `new Matrix(...)` can be intercepted.
- **User-defined literals**: `constexpr Mass operator"" _kg(long double v) { return Mass{double(v)}; }` lets you write `5.0_kg`. Used by `<chrono>` (`10ms`) and `std::string` (`"abc"s`). Nice for unit-safe physics code; not needed now.

## Gotchas and undefined behavior

- **Returning a reference to a local**: `Matrix& operator+(...) { Matrix r; ...; return r; }` returns a dangling reference — UB. Binary operators return **by value**.
- **`operator[]` returning by value** (`double operator()(i,j)`) makes `m(i,j) = 5;` a compile error or, worse, a silent no-op assignment to a temporary. Return `double&`.
- **Forgetting the `const` overload** of `operator()` means any function taking `const Matrix&` cannot read elements. You will hit this the first time you write `operator<<`.
- **Forgetting `return *this;`** in `+=`: compiles with a warning (or not), then `a += b` returns garbage and `-Wall` saves you only sometimes. Return type `Matrix&` with no return statement is UB.
- **Self-assignment in `operator=`**: `m = m;` with a raw-pointer class that frees then copies reads freed memory. Check `this != &o` or use copy-and-swap.
- **Ambiguous overloads**: defining both `operator*(Matrix, double)` and a non-`explicit` `Matrix(double)` constructor makes `m * 2` ambiguous between scaling and matmul-with-1x1. `explicit` fixes it.
- **Comparing doubles with `==`** in `Vec3::operator==`: correct as "bitwise identical", wrong as "physically the same". Provide `approx_equal`.
- **Out-of-range `m(i,j)`** is UB, not an exception (`std::vector::operator[]` does not check). Use `assert` in debug builds.
- **Overloading `&&`/`||`** silently drops short-circuiting.
- **Hidden temporaries in loops**: `for (...) total = total + x;` on `Matrix` allocates each iteration; `total += x;` does not.

## Common mistakes checklist

- [ ] Binary `+ - * /` return `T` by value; `+= -= *= /=` return `T&` and end with `return *this;`.
- [ ] `+` implemented as "copy, `+=`, return" — no duplicated arithmetic.
- [ ] `scalar * obj` is a free function (cannot be a member).
- [ ] `operator<<` is a free function taking `std::ostream&` and returning it.
- [ ] `operator()` / `operator[]` have both `const` and non-`const` overloads; both return references.
- [ ] `!=` is `!(a == b)`; `>` `<=` `>=` derived from `<`.
- [ ] Single-argument constructors and conversion operators are `explicit`.
- [ ] Meaning of `*` on matrices is documented at the top of the class.
- [ ] No `friend` unless the public interface is insufficient.
- [ ] `Matrix` with `std::vector` members does not hand-write `operator=` (Rule of Zero).

## You can move on when...

- You can list, without looking, which operators must be members and which must be free, and explain why `2.0 * m` forces a free function.
- You can write the twelve operators for `Vec3` in under ten minutes and they compile clean with `-Wall -Wextra`.
- You can explain why `Matrix operator+(Matrix a, const Matrix& b)` takes the first argument by value and what happens when the argument is a temporary.
- Given `m(i, j) = 3.0;` you can say which overload runs and what it returns; given `const Matrix& m`, you can say why `m(i, j)` still works.
- You can write `operator<<` for any class and have `std::cout << a << b << '\n'` chain.
- You can explain what a functor is, why it is faster than a C function pointer with `void *ctx`, and how it relates to lambdas.
- You can explain how many allocations `A * B + C` performs with this chapter's design and why.
