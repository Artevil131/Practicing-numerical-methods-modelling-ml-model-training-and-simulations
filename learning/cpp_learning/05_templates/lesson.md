# Chapter 05 — Templates

## What you'll be able to do after this chapter

- Explain what problem templates solve compared with C's `void*` and macro generics, and why the result is both type-safe and as fast as hand-written code.
- Write function templates with argument deduction and explicit instantiation, and class templates like `Matrix<T>` (mirroring `torch.float32`/`float64`) and `Vec<N>` for fixed-size physics vectors.
- Use non-type parameters, default template arguments, full and partial specialization, `static_assert` with type traits, and `if constexpr` to branch at compile time.
- Read a 200-line template error message and find the one line that matters.
- Decide when *not* to template.

## Why this matters for ML / numerics / sims

Your matrix library needs to work in `float` (fast, half the memory, what GPUs prefer) and `double` (accurate, what solvers need). In C you would either duplicate every function or use `void*` + `sizeof` and lose all type checking. A `template<typename T> class Matrix` gives you `Matrix<float>` and `Matrix<double>` from one source, with every operation type-checked and inlined. Fixed-size `Vec<3>` for N-body and `Vec<2>` for 2-D fluids compile to exactly the code you would write by hand for that size — no loop overhead, no heap. `std::vector<T>`, `std::unordered_map<K,V>`, `std::sort` — everything you used in chapter 04 is a template; this chapter shows you how they are built so you can build your own.

---

## 1. The problem templates solve

In C you had three ways to write a `max` that works for several types:

```c
/* 1. Duplicate: max_int, max_double, max_float ... */
/* 2. Macro: no type checking, double evaluation, no debugging */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
/* 3. void* + size + comparator: type-erased, slow (indirect call), unsafe */
void *max_generic(void *a, void *b, int (*cmp)(const void *, const void *));
```

A template is a *recipe* the compiler uses to generate a real function or class for each type you use it with. The result is type-checked, debuggable, and optimized exactly like hand-written code — because it *is* hand-written code, written by the compiler at compile time.

```cpp
template <typename T>
T max_of(T a, T b) { return a > b ? a : b; }

max_of(3, 7);        // compiler generates int max_of(int, int)
max_of(2.5, 1.0);    // and double max_of(double, double)
```

Python equivalent: Python functions are already generic (duck typing) but check at run time. Templates are duck typing checked at compile time, with a separate compiled copy per type — like Julia's method specialization, or NumPy's per-dtype inner loops.

## 2. Function templates and argument deduction

```cpp
template <typename T>
T sum(const T *a, int n) {
    T s = T{};                                  // T{} is zero for any arithmetic type
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

double d[] = {1.5, 2.5};  float f[] = {1.0f, 2.0f};  int k[] = {1, 2, 3};
sum(d, 2);      // T deduced as double from the argument type
sum(f, 2);      // T = float
sum(k, 3);      // T = int
```

**Deduction** looks at the function arguments and solves for the template parameters. It does not consider return types or implicit conversions: `max_of(3, 7.5)` is an error ("deduced conflicting types for T: int and double"), not a silent promotion. Fix it at the call site (`max_of(3.0, 7.5)`), by explicit instantiation (next section), or by giving each parameter its own type parameter (`template <typename A, typename B> auto max_of(A a, B b)`).

## 3. Explicit instantiation `max_of<double>(...)`

```cpp
max_of<double>(3, 7.5);        // T fixed to double; 3 converts to 3.0
sum<double>(k, 3);             // ERROR: int* does not convert to const double*
auto z = zeros<float>(10);     // no argument to deduce from: must be explicit
std::vector<int> v = {3, 1, 2};
std::sort(v.begin(), v.end()); // deduced from the iterator arguments
```

When there are several parameters, you may specify a prefix and let the rest deduce: `template <typename R, typename T> R convert(T x)` called as `convert<float>(3.9)` fixes `R`, deduces `T = double`.

Terminology: a *template* is the recipe; *instantiation* is the compiler generating a concrete function/class from it; a *specialization* is one such concrete instance (`max_of<double>`), whether generated or hand-written (section 8).

## 4. Class templates: `Matrix<T>`

```cpp
template <typename T>
class Matrix {
public:
    Matrix(int rows, int cols, T fill = T{})
        : rows_(rows), cols_(cols), data_(static_cast<std::size_t>(rows) * cols, fill) {}

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    T &operator()(int i, int j)       { return data_[static_cast<std::size_t>(i) * cols_ + j]; }
    T  operator()(int i, int j) const { return data_[static_cast<std::size_t>(i) * cols_ + j]; }

    T sum() const {
        T s = T{};
        for (T x : data_) s += x;
        return s;
    }

private:
    int rows_, cols_;
    std::vector<T> data_;
};

Matrix<double> a(3, 3, 1.0);           // 72 bytes of storage — torch.float64
Matrix<float>  b(3, 3, 1.0f);          // 36 bytes                — torch.float32
Matrix<int>    c(2, 2, 0);             // works too: an index matrix or a label grid
```

`Matrix<double>` and `Matrix<float>` are *different, unrelated types*. You cannot assign one to the other without a conversion function (section 5), and a function taking `const Matrix<double>&` will not accept a `Matrix<float>` — exactly like PyTorch refusing to add a float32 and a float64 tensor without `.to()`. Inside the class body the name `Matrix` alone means "the current instantiation" (`Matrix<T>`); outside, you must write `Matrix<T>`.

Free functions over class templates are themselves templates:

```cpp
template <typename T>
Matrix<T> matmul(const Matrix<T> &a, const Matrix<T> &b) {
    Matrix<T> out(a.rows(), b.cols());
    for (int i = 0; i < a.rows(); i++)
        for (int k = 0; k < a.cols(); k++) {
            T aik = a(i, k);
            for (int j = 0; j < b.cols(); j++) out(i, j) += aik * b(k, j);
        }
    return out;
}
```

Member functions of a class template are instantiated **only when used**. If you never call `sum()` on a `Matrix<std::string>`, the fact that `+=` on strings does something odd never matters. This lazy instantiation is why `std::vector<T>` works for types that lack `operator<` — `sort` is simply never instantiated for them.

## 5. Member function templates

A member of a class (template or not) can itself be a template. The canonical use is a converting constructor between `Matrix<float>` and `Matrix<double>`:

```cpp
template <typename T>
class Matrix {
public:
    // ... as above ...
    template <typename U>
    explicit Matrix(const Matrix<U> &other)                  // Matrix<double> from Matrix<float>: .to(float64)
        : rows_(other.rows()), cols_(other.cols()), data_(other.rows() * static_cast<std::size_t>(other.cols())) {
        for (int i = 0; i < rows_; i++)
            for (int j = 0; j < cols_; j++) (*this)(i, j) = static_cast<T>(other(i, j));
    }

    template <typename F>
    Matrix &apply(F f) {                                    // m.apply([](double x){ return std::tanh(x); })
        for (T &x : data_) x = f(x);
        return *this;
    }
};

Matrix<float>  wf(4, 4, 0.1f);
Matrix<double> wd(wf);          // U deduced as float; T is double
```

Note the two `template` lines: the outer one belongs to the class, the inner one to the member. Outside the class body a member template's definition starts `template <typename T> template <typename U> Matrix<T>::Matrix(const Matrix<U> &other) { ... }` — ugly enough that you will keep these inline.

## 6. Non-type template parameters: `Vec<N>`

Template parameters can be values, not only types — integers, `bool`, enums, pointers (and floating point since C++20). A fixed-size vector for physics:

```cpp
template <int N>
struct Vec {
    double v[N];                                             // no heap; sizeof(Vec<3>) == 24

    double &operator[](int i)       { return v[i]; }
    double  operator[](int i) const { return v[i]; }

    Vec operator+(const Vec &o) const { Vec r; for (int i = 0; i < N; i++) r.v[i] = v[i] + o.v[i]; return r; }
    Vec operator*(double s)     const { Vec r; for (int i = 0; i < N; i++) r.v[i] = v[i] * s; return r; }
    double dot(const Vec &o)    const { double s = 0; for (int i = 0; i < N; i++) s += v[i] * o.v[i]; return s; }
    double norm() const { return std::sqrt(dot(*this)); }
};

using Vec2 = Vec<2>;
using Vec3 = Vec<3>;
Vec3 pos{{1.0, 2.0, 3.0}}, vel{{0.1, 0.0, 0.0}};
pos = pos + vel * 0.01;
```

Because `N` is a compile-time constant, the `for (int i = 0; i < N; i++)` loops are fully unrolled by the optimizer — `Vec<3>::dot` compiles to three multiplies and two adds, nothing else. `Vec<2>` and `Vec<3>` are different types, so you cannot accidentally add a 2-D and a 3-D vector. `std::array<T, N>` is exactly this pattern in the standard library. Non-type arguments must be constant expressions: `Vec<n>` with a runtime `int n` is an error; `constexpr int kDim = 3; Vec<kDim>` is fine.

## 7. Default template arguments

```cpp
template <typename T = double, int Align = 64>
class Matrix { /* ... */ };

Matrix<>        m1(3, 3);      // Matrix<double, 64>  — note the empty <> is required before C++17 CTAD
Matrix<float>   m2(3, 3);      // Matrix<float, 64>
Matrix<float, 32> m3(3, 3);

template <typename T, typename Cmp = std::less<T>>
void my_sort(std::vector<T> &v, Cmp cmp = Cmp{});           // std::sort works this way
```

Defaults are why `std::vector<double>` does not require you to name an allocator and `std::priority_queue<T>` defaults to a `vector` with `std::less`. Function-template defaults are used only when deduction fails for that parameter.

## 8. Template specialization: full and partial

A **full specialization** replaces the generic recipe for one exact set of arguments:

```cpp
template <typename T>
struct TypeName { static const char *get() { return "unknown"; } };

template <> struct TypeName<float>  { static const char *get() { return "float32"; } };   // full specialization
template <> struct TypeName<double> { static const char *get() { return "float64"; } };
template <> struct TypeName<int>    { static const char *get() { return "int32"; } };

TypeName<double>::get();      // "float64" — the dtype string, like tensor.dtype
```

Function templates can be fully specialized too (`template <> bool nearly_equal<float>(float a, float b)`), but overloading a plain function is usually simpler and interacts better with deduction.

A **partial specialization** (class templates only) provides an alternative recipe for a *family* of arguments:

```cpp
template <typename T> struct Storage      { static const char *kind() { return "value"; } };
template <typename T> struct Storage<T *> { static const char *kind() { return "pointer"; } };   // any pointer type
template <typename T> struct Storage<std::vector<T>> { static const char *kind() { return "vector"; } };

template <typename T, int N> struct Vec { /* general */ };
template <typename T>        struct Vec<T, 3> { /* 3-D special case: adds cross() */ };
```

The compiler picks the most specialized matching template. This is how `std::vector<bool>` is packed into bits while every other `std::vector<T>` is not (a decision widely considered a mistake, but a fine example of the mechanism), and how type traits like `std::is_pointer` are implemented.

## 9. Templates must live in headers

A template is not code until it is instantiated, and instantiation happens in the translation unit that *uses* it, with the concrete type known. If `matmul<T>` is defined in `matrix.cpp` and used in `main.cpp`, the compiler processing `main.cpp` sees only the declaration and cannot generate `matmul<double>`; `matrix.cpp` never generates it either because nothing in that file asks for `double`. Result: **undefined symbol at link time**.

Therefore: template definitions go in the header, in full. Every `.cpp` that includes it instantiates what it needs, and the linker merges the duplicates (templates are implicitly `inline` in this sense). The escape hatch — explicit instantiation `template class Matrix<double>;` in a `.cpp` plus `extern template class Matrix<double>;` in the header — works only for a fixed, known list of types and is used to cut compile times in big projects.

Practical layout: `matrix.hpp` containing the class template and its free-function templates, nothing else. Non-template helpers can go in a `.cpp` as usual.

## 10. Reading template error messages

A template error at instantiation depth 5 produces a hundred lines. Method:

1. **Find the first line that names your file** (`main.cpp:42:5:`) — that is where you did something wrong. Everything above it is the compiler describing where it was when it noticed.
2. Read the `error:` line immediately following (or preceding) it. Typical: `no matching function for call to 'matmul'`, `invalid operands to binary expression`, `no member named 'size' in 'int'`.
3. Read the `note: candidate template ignored: ...` lines — they tell you *why* each overload was rejected (`deduced conflicting types`, `could not match 'Matrix<T>' against 'Matrix<float>'`).
4. Ignore the `in instantiation of ... requested here` chain unless step 1–3 did not explain it.

`static_assert` (section 12) turns a 100-line error into one line of your own text. Use it at the top of every template that has requirements on `T`.

## 11. `typename` vs `class`, dependent names

`template <class T>` and `template <typename T>` mean exactly the same thing; `typename` is newer and clearer (a `T` need not be a class — it is usually `double`). Use `typename`.

Inside a template, a name that depends on `T` — like `T::value_type` or `std::vector<T>::iterator` — is a **dependent name**. The compiler cannot know, before instantiation, whether `std::vector<T>::iterator` is a type or a static member variable. You must tell it:

```cpp
template <typename Container>
void print_all(const Container &c) {
    typename Container::const_iterator it = c.begin();     // `typename` required: dependent type
    for (; it != c.end(); ++it) std::cout << *it << ' ';
}
// In practice: `auto it = c.begin();` sidesteps the whole issue. Prefer auto.

template <typename T>
using ValueOf = typename T::value_type;                   // alias template; still needs typename
```

The mirror case is `template`: `T::template rebind<U>` when a dependent name is itself a template. Rare in application code; you will see it in library headers.

## 12. `static_assert` with type traits

`<type_traits>` provides compile-time predicates about types. Combine with `static_assert` to give readable errors:

```cpp
#include <type_traits>

template <typename T>
class Matrix {
    static_assert(std::is_arithmetic_v<T>, "Matrix<T> requires an arithmetic T (int, float, double, ...)");
    // ...
};

template <typename T>
T safe_sqrt(T x) {
    static_assert(std::is_floating_point_v<T>, "safe_sqrt needs float/double/long double");
    return x < T{} ? T{} : std::sqrt(x);
}

Matrix<std::string> bad;     // error: static assertion failed: Matrix<T> requires an arithmetic T ...
```

Useful traits: `std::is_floating_point_v<T>`, `std::is_integral_v<T>`, `std::is_arithmetic_v<T>`, `std::is_same_v<A, B>`, `std::is_pointer_v<T>`, `std::is_trivially_copyable_v<T>` (may I `memcpy` it?), `std::is_signed_v<T>`. The `_v` suffix (C++17) is shorthand for `::value`. Also `std::numeric_limits<T>::epsilon()` / `::max()` / `::lowest()` from `<limits>` — the templated replacement for `DBL_EPSILON`, `FLT_MAX`.

## 13. `if constexpr` (C++17)

An `if constexpr` condition is evaluated at compile time and the *untaken branch is not instantiated*. This lets one template handle types that need different code without specialization:

```cpp
template <typename T>
bool nearly_equal(T a, T b) {
    if constexpr (std::is_floating_point_v<T>) {
        return std::fabs(a - b) <= 4 * std::numeric_limits<T>::epsilon() * std::max(std::fabs(a), std::fabs(b));
    } else {
        return a == b;                       // exact for integers; the fabs branch is never compiled for int
    }
}

template <typename T>
const char *dtype() {
    if constexpr (std::is_same_v<T, float>)       return "float32";
    else if constexpr (std::is_same_v<T, double>) return "float64";
    else if constexpr (std::is_integral_v<T>)     return sizeof(T) == 4 ? "int32" : "intN";
    else                                          return "unknown";
}
```

A plain `if` would compile *both* branches for every `T` and fail if one does not type-check (e.g., `std::fabs` on a `std::string`). `if constexpr` is the standard tool for "this template does slightly different things per type": choosing tolerance by precision, using `memcpy` for trivially copyable `T` and element-wise copy otherwise, formatting `float` vs `int`.

## 14. Variadic templates (brief)

`typename... Args` is a *parameter pack*: zero or more types. Used with a fold expression (C++17) or recursion:

```cpp
template <typename... Args>
void print(const Args &...args) {
    ((std::cout << args << ' '), ...);       // fold over the comma operator: expands to (cout<<a<<' '), (cout<<b<<' '), ...
    std::cout << '\n';
}
print("epoch", 3, "loss", 0.125);            // epoch 3 loss 0.125

template <typename... Ts>
auto sum_all(Ts... xs) { return (xs + ...); }      // (a + (b + (c + ...)))
sum_all(1, 2.5, 3.0f);                        // 6.5 (double)

template <typename... Args>
constexpr std::size_t count_args(Args...) { return sizeof...(Args); }
```

This is how `std::make_pair`, `emplace_back(args...)`, `std::tuple`, and `printf`-replacements like `fmt::format` are built. For this course you need to *read* it and write the occasional `print` helper; deeper use (perfect forwarding, `std::forward<Args>(args)...`) comes with the move-semantics chapter.

## 15. Concepts — the C++20 future (mention)

C++20 lets you state requirements directly: `template <std::floating_point T> T safe_sqrt(T x);` or `requires std::is_arithmetic_v<T>`. Errors then say "constraints not satisfied" at the call site instead of failing deep inside. Conceptually it is `static_assert` moved into the signature so that overload resolution can use it. With `-std=c++17` you do not have it; the `static_assert` + `if constexpr` combination covers the same ground with slightly worse error messages. Know the name so library documentation makes sense.

## 16. Template code bloat and compile times

Each distinct instantiation is a separate compiled copy: `Matrix<float>`, `Matrix<double>`, `Matrix<int>`, plus every free function for each. `Vec<2>`, `Vec<3>`, `Vec<4>` are three copies of every member. Costs:

- **Binary size**: usually negligible for numeric code (a few KB per instantiation); noticeable when a large class is instantiated for dozens of types.
- **Compile time**: the real cost. Every `.cpp` that includes `matrix.hpp` re-parses and re-instantiates everything it uses. A heavy template header included in 50 files is compiled 50 times. Mitigations: keep templates small, put non-template code in `.cpp` files, use `extern template` for the few instantiations everyone shares, and structure projects so hot headers change rarely.
- **Error messages** (section 10) and **debuggability**: stepping through `std::sort` internals is unpleasant.

The benefit — one source, every type, zero runtime overhead — outweighs these for a numerics library. Just be aware that "template everything" is not free.

## 17. When NOT to template

- **You only need one type.** If every matrix in your solver is `double`, `class Matrix` with `double` is simpler, compiles faster, and gives better errors. Add `template <typename T>` when you actually need `float` — the refactor is mechanical.
- **The variation is in behaviour, not type.** A `Matrix` that is sometimes row-major, sometimes column-major, is a runtime flag or a stride, not a template parameter (unless you have measured that the branch costs you).
- **The types are unrelated.** A template that only compiles for `float` and `double` and does nothing sensible for anything else is fine; a template that is really two different functions sharing a name should be two functions.
- **It hides a simple thing.** `template <typename T> T square(T x)` is fine. `template <typename T, typename U, typename Op> auto apply_binary(...)` for a one-off is not.

Default position for this course: `double` everywhere; `Matrix<T>` and `Vec<N>` where the benefit is concrete (`float` for memory/GPU parity; fixed `N` for physics vectors); templates for genuinely generic utilities (`max_of`, `clamp`, `argmax`, `print`).

---

## Gotchas and undefined behavior

- **Template defined in a `.cpp`** used from another file: `undefined symbol` at link time. Put it in the header.
- **Deduction conflict**: `max_of(3, 7.5)` is an error, not a promotion. Specify `<double>` or cast.
- **Deduction ignores return type**: `T zeros(int n)` cannot deduce `T`; call `zeros<double>(n)`.
- **`T s = 0;`** in a template gives an `int` zero converted to `T`; for `T = std::complex<double>` or user types use `T s = T{};` (value-initialization).
- **Arrays decay**: `template <typename T> void f(T a)` called with `double arr[3]` deduces `T = double*`. Use `template <typename T, std::size_t N> void f(T (&a)[N])` or pass `std::array`.
- **`if` vs `if constexpr`**: a plain `if` instantiates both branches; use `if constexpr` when a branch would not type-check for some `T`.
- **Missing `typename`** on a dependent type: error `missing 'typename' prior to dependent type name`. Add it, or use `auto`.
- **Non-type argument must be a constant expression**: `Vec<n>` with runtime `n` does not compile.
- **Specialization must be declared before first use** in every translation unit, or the compiler silently uses the generic version in some files — ill-formed, no diagnostic required. Keep specializations next to the primary template in the header.
- **`Matrix<float>` vs `Matrix<double>`** are unrelated types: no implicit conversion. Provide a converting constructor (section 5) and mark it `explicit` so precision changes are visible.
- **Comparing `T` to `0.0`** inside a template instantiated for `int` is fine (promotion), but `std::numeric_limits<T>::epsilon()` is `0` for integer `T` — guard with `if constexpr (std::is_floating_point_v<T>)`.
- **Code bloat via accidental instantiations**: `Matrix<int>`, `Matrix<long>`, `Matrix<std::size_t>` are three copies. Pick one integer type for index matrices.

## Common mistakes checklist

- [ ] Every template (class and function) is fully defined in a header.
- [ ] `static_assert` with a type trait at the top of templates that have requirements on `T`.
- [ ] `T{}` for zero, `std::numeric_limits<T>` for epsilon/max, never `0`/`DBL_EPSILON`.
- [ ] `if constexpr` wherever a branch depends on `T`'s properties.
- [ ] `typename` before dependent type names (or `auto`).
- [ ] Converting constructors between `Matrix<float>` / `Matrix<double>` are `explicit`.
- [ ] Non-type parameters used for sizes that are truly compile-time constants (`Vec<3>`), not for values that vary at run time.
- [ ] Specializations live next to their primary template.
- [ ] You checked whether plain `double` would have done the job.

## You can move on when...

- You can write `Matrix<T>` with `matmul`, `transpose`, a converting constructor from `Matrix<U>`, and a `static_assert` that `T` is arithmetic, header-only, and instantiate it for `float` and `double` in one program.
- You can write `Vec<N>` with `+`, `*`, `dot`, `norm`, and add a `cross()` that exists only for `N == 3` (via partial specialization or `if constexpr` + `static_assert`).
- You can explain, in two sentences, why a template defined in a `.cpp` causes a linker error.
- You can take a deliberately broken call like `matmul(Matrix<float>, Matrix<double>)`, read the compiler output, and point at the `note:` line that explains the rejection.
- You can state the difference between `if` and `if constexpr` inside a template and give an example where it matters.
- You can name two situations where templating would be the wrong choice for your matrix library.
