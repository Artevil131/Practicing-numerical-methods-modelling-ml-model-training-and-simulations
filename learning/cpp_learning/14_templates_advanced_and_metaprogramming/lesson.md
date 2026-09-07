# Chapter 14 — Advanced Templates and Metaprogramming

This chapter is C++20. Compile everything with `c++ -Wall -Wextra -std=c++20 -O2`. Apple clang 21
(libc++) supports every feature used here; GCC ≥ 11 and MSVC 19.30+ do too. Where a feature is
C++17-only it is marked, because you will read a lot of C++17 library code that predates concepts.

Prerequisites: `../05_templates/lesson.md` (basic templates, specialization, `if constexpr`),
`../07_move_semantics_and_smart_pointers/lesson.md` (value categories), `../13_modern_cpp_and_idioms/lesson.md`
§§7, 12, 20, 22 (folds, C++20 tour, CRTP recap, tag dispatch). Those are assumed; this chapter goes
under them.

## What you'll be able to do after this chapter

- State the template argument deduction rules from memory, explain why `T&&` sometimes isn't an rvalue reference, and write a perfect-forwarding factory that never copies when it could move.
- Write variadic templates three ways (recursion, fold expressions, `index_sequence`) and pick the right one; iterate a `std::tuple` at compile time.
- Read and write type traits, including detection traits, and translate any `enable_if`/SFINAE overload set you meet in library code into C++20 concepts.
- Implement CRTP mixins and static polymorphism, and a complete expression-template `Vec`/`Matrix` in which `a*b + c` allocates nothing — the core mechanism of Eigen, xtensor, and Blaze.
- Compute lookup tables, hashes and small containers at compile time with `constexpr`/`consteval`, and know what `constinit` guarantees.
- Use class-type non-type template parameters, template template parameters and policy classes to build a `Matrix<T, Storage>` whose layout decisions cost nothing at runtime.
- Measure what metaprogramming costs (`-ftime-trace`), read a 200-line template error, and know when to stop.

## Why this matters for ML / numerics / sims

Every fast numerics library in C++ is a metaprogramming library. Eigen's `a*b + c` on 3-vectors compiles to six multiplies and three adds with no function call, no loop and no heap; that is expression templates (§9) plus CRTP (§8) plus inlining. PyTorch's ATen dispatches on dtype with a macro that expands to a template instantiation per type — that is `if constexpr`/tag dispatch (§6). Your `Tensor<T>` (project P02) and autograd (P03) need `Tensor<float>`, `Tensor<double>` and someday `Tensor<half>` from *one* implementation with the fast path chosen at compile time: traits and concepts (§§4–7). Compile-time `sin` tables and bit tables (§10) are how a PIC or LBM kernel avoids `std::sin` in the inner loop; class-type NTTPs (§11) let a `Vec<Shape{3}>` be as cheap as three `double`s. And for competitive programming, `constexpr` sieves and fold-based `min(a, b, c, d)` shave code without shaving speed.

The other reason: you will *read* this. Eigen, `<algorithm>`, `std::ranges`, fmt, Abseil — the code you learn from is written this way. Being able to read it is worth more than being able to write it.

---

## 1. Template argument deduction, precisely

Deduction ([temp.deduct]) compares each function parameter type **P** against the corresponding argument type **A** and tries to find template arguments that make them match. The rules that matter:

| Parameter form `P` | Argument `A` | Deduced `T` | Rule |
|---|---|---|---|
| `T` (by value) | `const int&` lvalue | `int` | Top-level `const` and references are **dropped** (like `auto`) |
| `T` | `int[3]` | `int*` | Arrays and functions **decay** to pointers |
| `T&` | `const int` lvalue | `const int` | `const` preserved because it is inside the reference |
| `const T&` | `int` rvalue | `int` | Binds anything; never deduces a reference type |
| `T&&` (forwarding ref) | lvalue `int` | `int&` | Special rule — see §2 |
| `T&&` | rvalue `int` | `int` | |
| `std::vector<T>` | `std::vector<double>` | `double` | Deduces through templates |
| `std::vector<T>` | `{1, 2, 3}` | **fails** | Braced-init-list is a *non-deduced context* |
| `T` from two args | `1` and `2.0` | **fails** | Deduction must agree exactly; no implicit conversion for deduced params |
| `typename T::value_type` | anything | non-deduced | Nested names can't be deduced; `T` must come from another parameter |

Three consequences you hit constantly:

```cpp
template <class T> T max2(T a, T b) { return a < b ? b : a; }
max2(1, 2.0);            // error: T deduced as int and double — conflicting
max2<double>(1, 2.0);    // OK: explicit argument, 1 converts to double
max2(1, int(2.0));       // OK

template <class T> void f(std::vector<T>&);
f({1, 2, 3});            // error: braced list is non-deduced; f(std::vector<int>{1,2,3}) works
```

Deduction is also what `auto` does (`auto x = expr;` deduces exactly like `template <class T> void g(T x)` would), and what class template argument deduction (CTAD, `std::vector v{1, 2}` → `vector<int>`) does using *deduction guides*. Since C++20 you can also write `void f(auto x)` — an abbreviated function template with identical semantics to `template <class T> void f(T x)`.

Python equivalent: none — Python functions are duck-typed at runtime. Deduction is the compiler doing duck typing once, at compile time, and then generating a specialized function.

---

## 2. Forwarding references, reference collapsing, `std::forward`

`T&&` where `T` is a *deduced* template parameter (or `auto&&`) is not an rvalue reference. Scott Meyers named it a **forwarding reference**; the standard uses that term since C++17 ([temp.deduct.call]/3). It gets a special deduction rule: if the argument is an lvalue of type `U`, `T` is deduced as `U&`. Then **reference collapsing** ([dcl.ref]/6) applies to `T&&`:

```
T = U&    →  T&& = U& &&  →  U&        (& wins over && whenever an & is present)
T = U&&   →  T&& = U&& && →  U&&
T = U     →  T&& = U&&
```

So a forwarding reference binds to *anything* and remembers what it bound to in `T`. `std::forward<T>(x)` is just a conditional cast that restores that value category:

```cpp
template <class T> T&& forward(std::remove_reference_t<T>& x) noexcept { return static_cast<T&&>(x); }
// T = U&  : returns U& &&  = U&   → x stays an lvalue
// T = U   : returns U&&          → x becomes an xvalue, so a move constructor is chosen
```

Inside the function body, `x` itself is *always an lvalue* (it has a name). Without `forward`, every argument is copied. With `std::move`, every argument is moved — including lvalues the caller still needs. `forward<T>` is the only correct choice, and it must be spelled with the explicit `<T>`:

```cpp
template <class T, class... Args>
std::unique_ptr<T> make(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));   // the pattern; std::make_unique is exactly this
}
Tracker src;
make<Holder>(src);              // Args = Tracker&  → Holder(Tracker) receives an lvalue  → copy
make<Holder>(std::move(src));   // Args = Tracker   → Holder(Tracker) receives an xvalue  → move
// example.cpp §1 prints:   make<Holder>(lvalue): copy;move;   make<Holder>(rvalue): move;move;
```

Rules of thumb: forward each forwarding-reference parameter **exactly once**, on its last use (a forwarded-from object may be gutted). Never `forward` a non-forwarding reference. Never return `std::forward<T>(x)` from a function returning by value — that's just a move (or copy) and `auto` would do it. `const T&&` is *not* a forwarding reference; neither is `std::vector<T>&&`. Only the bare `T&&` / `auto&&` form gets the special rule.

The `auto&&` version is what range-`for` uses: `for (auto&& e : range)` binds to whatever the iterator dereferences to — a reference into a `vector`, a proxy object from `vector<bool>`, or a temporary from a `views::transform`.

---

## 3. Parameter packs: recursion, folds, `sizeof...`, `index_sequence`

A pack `class... Ts` / `Ts... xs` is expanded by writing a *pattern* followed by `...`. Everything to the left of the `...` that mentions the pack is repeated per element: `f(xs)...` → `f(x0), f(x1), f(x2)`; `std::forward<Ts>(xs)...`; `sizeof(Ts)...`; `Base<Ts>...` in a base-clause.

**Three ways to process a pack:**

```cpp
// 1. Recursion (C++11). N+1 instantiations; needs a base case; deep packs stress the compiler.
template <class T> T sum(T x) { return x; }
template <class T, class... R> T sum(T x, R... r) { return x + sum(r...); }

// 2. Fold expression (C++17). One instantiation. [expr.prim.fold]
template <class... T> auto sum(T... x)          { return (x + ... + 0); }   // binary right fold: x0 + (x1 + (x2 + 0))
template <class... T> bool all_pos(T... x)      { return ((x > 0) && ...); }// unary fold; empty pack → true for &&
template <class... T> void print(T const&... x) { ((std::cout << x << ' '), ...); }  // "for each" via comma
template <class... T> auto min_all(T... x)      { auto m = std::numeric_limits<std::common_type_t<T...>>::max(); ((m = x < m ? x : m), ...); return m; }

// 3. Index sequence: when you need the POSITION, e.g. to iterate a tuple.
template <class Tup, class F, std::size_t... I>
void for_each_impl(Tup&& t, F&& f, std::index_sequence<I...>) { (f(std::get<I>(std::forward<Tup>(t))), ...); }
template <class Tup, class F>
void tuple_for_each(Tup&& t, F&& f) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_cvref_t<Tup>>;
    for_each_impl(std::forward<Tup>(t), std::forward<F>(f), std::make_index_sequence<N>{});
}
// tuple_for_each(std::tuple{1, "two", 3.0}, [](auto& e){ std::cout << e << '|'; });   → 1|two|3|
```

Fold operators: any binary operator, including `,`, `&&`, `||`, `<<`. Unary folds over `&&`, `||`, `,` are allowed on empty packs (yielding `true`, `false`, `void()`); every other operator requires a non-empty pack or the binary form with an init value. `sizeof...(Ts)` gives the pack length as a constant.

`std::index_sequence<0, 1, ..., N-1>` is just `std::integer_sequence<std::size_t, 0, 1, ...>` — a type whose only content is a pack of numbers. `std::make_index_sequence<N>` builds it (the library does this in O(log N) instantiations). It is the standard idiom for anything "positional": iterating tuples, unpacking `std::array` into a constructor, `std::apply` itself (§13) is implemented with it.

Python equivalent: `*args` is a pack; `functools.reduce(op, args, init)` is a fold; `for i, x in enumerate(args)` is the index-sequence version. The difference is that here the loop is unrolled and typed per element at compile time.

---

## 4. Type traits

`<type_traits>` is a library of *compile-time functions on types*. Two shapes:

- **Predicates**: `struct is_X<T> : std::bool_constant<...>` with `is_X_v<T>` (C++17 variable template alias).
- **Transformations**: `struct remove_X<T> { using type = ...; }` with `remove_X_t<T>`.

The ones you actually use (cppreference: "Metaprogramming library"):

| Trait | Answers |
|---|---|
| `remove_cvref_t<T>` (C++20) | `const T&` → `T`. The one you want in generic code; `decay_t` also turns arrays/functions into pointers. |
| `is_same_v<A,B>`, `is_convertible_v<A,B>`, `is_base_of_v<B,D>` | Type relations |
| `is_integral_v`, `is_floating_point_v`, `is_arithmetic_v`, `is_pointer_v`, `is_enum_v` | Categories |
| `is_trivially_copyable_v<T>` | May I `memcpy` it? (Yes for `double[4]`, no for `std::vector`) |
| `is_nothrow_move_constructible_v<T>` | Will `vector` reallocation move instead of copy? (Chapter 10 §11) |
| `is_invocable_r_v<R, F, Args...>` | Can `F` be called with `Args...` returning something convertible to `R`? |
| `common_type_t<A,B>` | Type of `a + b` for arithmetic types — what your `Tensor<T>` ⊗ `Tensor<U>` should return |
| `conditional_t<cond, A, B>` | Compile-time `?:` on types |
| `underlying_type_t<E>` | Storage type of an enum |
| `std::type_identity_t<T>` (C++20) | Blocks deduction for one parameter |

**Writing your own trait**: a primary template that says no, and partial specializations that say yes.

```cpp
template <class T> struct is_matrix : std::false_type {};
template <class T> struct is_matrix<Mat<T>> : std::true_type {};
template <class T> inline constexpr bool is_matrix_v = is_matrix<T>::value;

// Transformation trait with a fallback, using void_t to test for a nested type:
template <class T, class = void> struct element_type { using type = T; };
template <class T> struct element_type<T, std::void_t<typename T::value_type>> { using type = typename T::value_type; };
template <class T> struct element_type<Mat<T>, void> { using type = T; };
```

**The `void_t` detection idiom** (Walter Brown, C++17): `std::void_t<Anything...>` is `void` if all the arguments are valid types. Put an expression inside `decltype(...)` and if it is ill-formed, that partial specialization is silently dropped (SFINAE, §5) and the primary `false_type` is chosen:

```cpp
template <class T, class = void> struct has_size : std::false_type {};
template <class T> struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};
static_assert(has_size<std::string>::value && !has_size<int>::value);
```

`std::declval<T>()` produces an rvalue of type `T` *in an unevaluated context* — it lets you write "an expression of type T" without needing a constructor. Since C++20 all of this collapses to a `requires` expression (§7); you still need to recognize the `void_t` form because it is everywhere in C++17 libraries.

---

## 5. SFINAE: `enable_if`, expression SFINAE, and why it is ugly

**Substitution Failure Is Not An Error** ([temp.deduct]/8): when substituting deduced arguments into a function template's *signature* (not its body) produces an invalid type or expression, the template is removed from the overload set instead of causing a compile error. The mechanism was discovered, not designed; C++11–17 metaprogramming is built on exploiting it.

```cpp
// enable_if: a struct whose ::type exists only if the condition holds.
template <class T> std::enable_if_t<std::is_integral_v<T>, T>       half(T x) { return x / 2; }
template <class T> std::enable_if_t<std::is_floating_point_v<T>, T> half(T x) { return x * T(0.5); }
// half(7) → first; half(7.0) → second; half("s") → both removed → "no matching function".

// Expression SFINAE: the trailing return type must be a valid expression.
template <class T> auto magnitude(const T& x) -> decltype(x.norm())                       { return x.norm(); }
template <class T> auto magnitude(const T& x) -> std::enable_if_t<std::is_arithmetic_v<T>, T> { return x < 0 ? -x : x; }
```

Where to put the `enable_if`: return type (above), a defaulted template parameter (`template <class T, std::enable_if_t<cond, int> = 0>`), or a defaulted function parameter (`void f(T, std::enable_if_t<cond>* = nullptr)`). All three work; the template-parameter form is preferred because it also works for constructors, which have no return type. Two overloads that differ *only* in the default template argument are the same template (redefinition error) — that is why the `int = 0` form is used, so the enable_if type differs.

Why it is ugly, concretely:

1. The condition is hidden in the signature and the reader has to decode `enable_if_t<!is_same_v<decay_t<U>, Foo>, int> = 0`.
2. Overloads must be **mutually exclusive** by hand; two viable ones give an ambiguity error, not a preference.
3. Errors: the compiler lists *every* candidate it rejected with the substitution that failed. On this machine, `half_sfinae(std::string("x"))` prints the call site plus two `note: candidate template ignored: requirement 'std::is_integral_v<std::string>' was not satisfied`. Clang is kind; in 2012 the same thing was a page of `substitution failure` per candidate, and it still is for anything nested inside a library.
4. Hard errors inside the body are **not** SFINAE — only the immediate context is. If `half<T>` is selected and the body does `x / 2` on a type without `/`, you get a hard error deep in the instantiation.

You need to read SFINAE; you should not write it in C++20 code. §7 rewrites both examples.

---

## 6. Tag dispatch vs `if constexpr` vs overloading

Three ways to choose an implementation at compile time:

```cpp
// Tag dispatch (C++98; how std::advance is implemented). Empty struct selects the overload.
template <class It> void advance_impl(It& it, int n, std::random_access_iterator_tag) { it += n; }
template <class It> void advance_impl(It& it, int n, std::input_iterator_tag)         { while (n-- > 0) ++it; }
template <class It> void my_advance(It& it, int n) { advance_impl(it, n, typename std::iterator_traits<It>::iterator_category{}); }

// if constexpr (C++17): one body, the untaken branch is DISCARDED — it may contain code that would
// not compile for this T, as long as it is syntactically valid and depends on the template parameter.
template <class It> void my_advance2(It& it, int n) {
    if constexpr (std::random_access_iterator<It>) it += n;
    else while (n-- > 0) ++it;
}

// Constrained overloads (C++20): see §7; picks the most constrained viable overload.
```

| | Tag dispatch | `if constexpr` | Overloading (SFINAE or concepts) |
|---|---|---|---|
| Where the logic lives | Trait + overload set | Inside one function | Overload set |
| Extensible by users of your library | Yes (add an overload for a new tag) | No (must edit the function) | Yes |
| Good for | Iterator categories, layout tags (`RowMajor`) | Small differing branches, per-type fast paths | Genuinely different algorithms per type |
| Readability | Medium (indirection) | Best for short branches | Best when concepts name the intent |
| Deduction still happens? | Yes | Yes | Yes |

Rule: **`if constexpr` when both branches are short and share most of their code; overloading when they are separate algorithms or you want users to extend the set; tag dispatch only to interoperate with code that already uses tags.** Remember that `if constexpr` outside a template is an ordinary `if` with a constant — both branches must compile.

---

## 7. C++20 concepts and `requires`

A **concept** is a named compile-time predicate over types ([temp.constr]). Constrained templates only participate in overload resolution when their constraints are satisfied; overload resolution prefers the *more constrained* one; and error messages say *which* constraint failed.

```cpp
// Defining concepts.
template <class T> concept Number   = std::integral<T> || std::floating_point<T>;
template <class T> concept HasNorm  = requires(const T& x) { { x.norm() } -> std::floating_point; };
template <class T> concept HasSize  = requires(T t) { { t.size() } -> std::convertible_to<std::size_t>; };
template <class T> concept VectorLike = requires(T v, std::size_t i) {
    typename T::value_type;                         // type requirement
    { v.size() } -> std::convertible_to<std::size_t>;
    { v[i] }     -> std::convertible_to<typename T::value_type>;   // compound requirement
    requires std::is_arithmetic_v<typename T::value_type>;         // nested requirement
};

// Four equivalent ways to constrain a template. Use the first two.
template <Number T>              T half(T x);                    // constrained type parameter
Number auto half(Number auto x);                                // constrained auto (abbreviated template)
template <class T> requires Number<T> T half(T x);              // requires-clause
template <class T> T half(T x) requires Number<T>;              // trailing requires-clause (also for member fns)
```

**`requires` expressions** (`requires (params) { requirements; }`) evaluate to `bool`: true iff every requirement is well-formed. Four requirement kinds — *simple* (`x + y;` must compile), *type* (`typename T::foo;`), *compound* (`{ expr } noexcept -> Concept;` — checks the expression, optionally `noexcept`, and that its type satisfies the concept), and *nested* (`requires cond;`). A `requires` expression is the C++20 replacement for the whole `void_t`/`declval` machinery.

**Rewriting §5 with concepts** (example.cpp §4):

```cpp
template <std::integral T>       T half(T x) { return x / 2; }
template <std::floating_point T> T half(T x) { return x * T(0.5); }
template <HasNorm T>             auto magnitude(const T& x) { return x.norm(); }
template <class T> requires std::is_arithmetic_v<T>  T magnitude(T x) { return x < 0 ? -x : x; }
```

The error for `half(std::string("x"))` becomes: `candidate template ignored: constraints not satisfied ... because 'std::string' does not satisfy 'integral' ... because 'is_integral_v<std::string>' evaluated to false`. The compiler walks the concept down to the leaf that failed.

**Subsumption** ([temp.constr.order]): if concept `Floating<T>` is defined as `Number<T> && std::is_floating_point_v<T>`, then `Floating` *subsumes* `Number` and the `Floating` overload is preferred for `double` with no ambiguity. Subsumption is computed by normalizing constraints into their atomic `&&`/`||` structure and comparing *named concepts*. Two overloads constrained by textually identical `requires` expressions or `is_arithmetic_v<T>` do **not** subsume each other — always constrain through named concepts if you want ordering. The standard library concepts form a hierarchy for exactly this reason: `std::random_access_iterator` subsumes `std::bidirectional_iterator` subsumes `std::forward_iterator` subsumes `std::input_iterator`.

**Standard concepts** (`<concepts>`, `<iterator>`, `<ranges>`) you will use: `std::same_as`, `std::derived_from`, `std::convertible_to`, `std::integral`, `std::floating_point`, `std::invocable`/`std::regular_invocable`, `std::predicate`, `std::equality_comparable`, `std::totally_ordered`, `std::copyable`, `std::regular` (copyable + default-constructible + equality-comparable — what a "value type" means), `std::ranges::range`, `std::ranges::contiguous_range`, `std::random_access_iterator`.

Concepts do **not** check the body: a constrained template can still fail to compile inside for a type that satisfies the concept if the body uses more than the concept promised. They constrain *callers*, not *implementers* (unlike Rust traits or Haskell type classes). Concepts are also not "definition checking" — the compiler doesn't verify your implementation only uses the constrained operations.

---

## 8. CRTP in depth

The Curiously Recurring Template Pattern: `struct D : Base<D>`. The base knows the derived type at compile time and can `static_cast<D&>(*this)`. It buys three things.

**Static polymorphism** — an "interface" with no vtable, no indirect call, full inlining:

```cpp
template <class D> struct Integrator {
    void run(int steps) { for (int i = 0; i < steps; ++i) derived().step(); }   // inlined per D
    D& derived() { return static_cast<D&>(*this); }
};
struct Euler : Integrator<Euler> { double x = 0, v = 1, dt = 0.1; void step() { x += v * dt; v -= x * dt; } };
struct RK4   : Integrator<RK4>   { /* ... */ void step(); };
// Compared with virtual step(): no 5–20 cycle indirect-branch cost per call, and the loop over
// steps can be vectorized or unrolled across the inlined body. The price: no heterogeneous
// std::vector<Integrator*>; each D is a distinct type (use std::variant if you need a container).
```

**Mixins** — inject a capability defined once in terms of a minimal interface:

```cpp
template <class D> struct Comparable {                     // needs only D::operator< and operator==
    friend bool operator!=(const D& a, const D& b) { return !(a == b); }
    friend bool operator> (const D& a, const D& b) { return b < a; }
    friend bool operator<=(const D& a, const D& b) { return !(b < a); }
    friend bool operator>=(const D& a, const D& b) { return !(a < b); }
};
struct Version : Comparable<Version> { int major, minor; /* < and == */ };
// Chain them: struct Particle : Printable<Particle>, Counted<Particle> {...};
```

Hidden friends (defined inside the class) are found only by ADL on `D`, so they don't pollute the namespace. C++20's `operator<=>` makes `Comparable` unnecessary for ordering specifically, but the mixin pattern remains — `Counted<D>` (per-type instance counter via `static inline int live`), `Printable<D>`, `Singleton<D>`, Eigen's `MatrixBase<Derived>`, which implements `norm()`, `transpose()`, `dot()` once for every dense expression type.

**Pitfalls**: `static_cast<D&>(*this)` from a `Base<D>` that is *not* actually the base of a `D` is UB (`struct Wrong : Base<Other>` compiles). The base can't call derived members in its own constructor (D isn't constructed yet). `sizeof(Base<D>)` can't depend on `D`'s members. Since the base is a template, its members are dependent names — call them with `this->foo()` from D.

---

## 9. Expression templates — full implementation

The problem: with ordinary operator overloading (Chapter 06), `Vec r = a*b + c;` does

```
tmp1 = a*b      → allocate n doubles, one loop
tmp2 = tmp1 + c → allocate n doubles, one loop
r = tmp2        → move (cheap), but two allocations and two passes over memory already happened
```

For `n = 10⁶` doubles that is 16 MB of temporaries and 3× the memory traffic of a fused loop; memory traffic is the bottleneck (Chapter 12 §4). NumPy has exactly this problem (`a*b + c` allocates twice), which is why `numexpr`, `torch.compile`, and JAX's XLA exist.

The solution: make operators return **unevaluated expression objects** that record the operation and their operands. Only assignment to a concrete `Vec` runs a loop — one loop, computing each element of the whole tree. Full code is in `example.cpp` namespace `et`; the skeleton:

```cpp
template <class D> struct Expr {                                  // CRTP base: every node has size() and [i]
    const D& self() const { return static_cast<const D&>(*this); }
    double operator[](std::size_t i) const { return self()[i]; }
    std::size_t size() const { return self().size(); }
};

struct Vec : Expr<Vec> {
    std::vector<double> d;
    template <class E> Vec(const Expr<E>& e) : d(e.size()) { assign(e); }        // THE loop
    template <class E> Vec& operator=(const Expr<E>& e) { d.resize(e.size()); assign(e); return *this; }
    template <class E> void assign(const Expr<E>& e) { for (std::size_t i = 0; i < d.size(); ++i) d[i] = e[i]; }
    double operator[](std::size_t i) const { return d[i]; }  std::size_t size() const { return d.size(); }
};

// Store Vec operands BY REFERENCE (they outlive the full expression) and nested nodes BY VALUE
// (they are temporaries; a reference to them would dangle by the time assign() runs).
template <class T> using operand_t = std::conditional_t<std::is_same_v<T, Vec>, const Vec&, T>;

template <class L, class R, class Op> struct BinOp : Expr<BinOp<L, R, Op>> {
    operand_t<L> l; operand_t<R> r;
    BinOp(const L& l_, const R& r_) : l(l_), r(r_) { assert(l.size() == r.size()); }
    double operator[](std::size_t i) const { return Op::apply(l[i], r[i]); }  // recursion through the tree
    std::size_t size() const { return l.size(); }
};
struct Add { static double apply(double a, double b) { return a + b; } };
struct Mul { static double apply(double a, double b) { return a * b; } };

template <class E> concept IsExpr = std::derived_from<E, Expr<E>>;                  // only OUR types
template <IsExpr L, IsExpr R> auto operator+(const L& l, const R& r) { return BinOp<L, R, Add>(l, r); }
template <IsExpr L, IsExpr R> auto operator*(const L& l, const R& r) { return BinOp<L, R, Mul>(l, r); }
template <IsExpr E> auto operator*(double s, const E& e) { return Scale<E>(s, e); }
```

`Vec r = a*b + c;` now builds the object `BinOp<BinOp<Vec,Vec,Mul>, Vec, Add>` — the program prints exactly this type — holding two `const Vec&` and one `const Vec&`, 24 bytes, on the stack. The `Vec` constructor calls `e[i]`, which inlines to `a.d[i]*b.d[i] + c.d[i]`. At `-O2` the generated loop is indistinguishable from the hand-written fused loop and vectorizes. The example asserts the result is *bit-identical* to the naive two-temporary version (same operations, same order).

Things that follow for free once the tree exists: reductions without materialization (`sum(a*b)` is a dot product with no temporary), broadcasting scalars (`Scale`), lazy slicing/views (a node whose `[i]` maps indices), and mixed precision (`Op::apply` on `common_type_t`).

**Why matrix product is the exception.** For element-wise operations, element `i` of the result needs element `i` of each operand: O(1) per element, so laziness is free. For `A*B`, element `(i,j)` needs row `i` of `A` and column `j` of `B`: if a product node were lazy, `(A*B + C)(i,j)` recomputes the dot product on every access, and `(A*B)*D` recomputes each element of `A*B` `n` times — O(n⁴). Eigen therefore has a `Product<>` node that **evaluates eagerly into a temporary** when nested inside another expression, and evaluates directly into the destination when the assignment is `C = A*B` (or `C.noalias() = A*B` to skip the aliasing check — `A = A*B` needs the temporary because it reads `A` while writing it). The example's `MProd` node does the same: it computes into an owned `Matrix` at construction, so `R = A*B + 2.0*C` is one matmul plus one fused pass.

**How Eigen does it** (so you can read the source): every dense object derives from `MatrixBase<Derived>` (CRTP) → `DenseBase` → `EigenBase`. Operators build `CwiseBinaryOp<Functor, Lhs, Rhs>`; the traits class `traits<CwiseBinaryOp<...>>` computes cost flags and whether the result is vectorizable, `Evaluator<Expr>` gives each node a `coeff(i,j)` / `packet(i,j)` (SIMD) interface, and `call_assignment` picks a loop kernel (linear, vectorized, unrolled for fixed sizes) from those flags. `Eigen::Vector3d a, b, c; a = b.cross(c) + 2*a;` compiles to straight-line code with zero loops. Blaze, xtensor, and Armadillo use the same architecture; NumPy-style libraries can't do this because Python has no compile time.

Costs: template error messages name the whole tree; `auto r = a*b + c;` captures an *expression*, not a `Vec` — if `a` goes out of scope you have dangling references (Eigen documents this as "C++11 and the auto keyword" pitfall). Make your users spell the result type, or provide `.eval()`.

---

## 10. `constexpr` programming

A `constexpr` function ([dcl.constexpr]) can be evaluated at compile time *if* all its inputs are constants; otherwise it runs at runtime like any function. C++20 relaxed it to nearly all of C++: loops, local variables, `if`, `try` (but not `throw` on the taken path), virtual calls, `std::vector`/`std::string` (as long as the allocation is freed before evaluation ends — "transient allocation"), most of `<algorithm>` and `<numeric>`, `std::array`, `std::pair`/`tuple`, `std::optional`, `std::bit_cast`. Not allowed: `reinterpret_cast`, `goto`, uninitialized reads, UB of any kind (the compiler *must* reject UB in constant evaluation — so `constexpr` is also a UB checker), and calling non-`constexpr` functions such as `std::sin` before C++26.

```cpp
constexpr double csin(double x) {                       // Taylor with range reduction; example.cpp §8
    while (x >  pi) x -= 2 * pi;  while (x < -pi) x += 2 * pi;
    double term = x, sum = x;
    for (int k = 1; k < 12; ++k) { term *= -x * x / ((2 * k) * (2 * k + 1)); sum += term; }
    return sum;
}
template <std::size_t N> constexpr std::array<double, N> make_sin_table() {
    std::array<double, N> t{};
    for (std::size_t i = 0; i < N; ++i) t[i] = csin(2 * pi * double(i) / double(N));
    return t;
}
constexpr auto SIN256 = make_sin_table<256>();          // 2 KB in .rodata; zero runtime cost
static_assert(SIN256[64] > 0.999999);                   // sin(π/2), checked at compile time
// Same technique: popcount-of-byte table, CRC tables, Gauss–Legendre nodes, Chebyshev coefficients,
// the 9 D2Q9 lattice weights and directions for LBM, 1/sqrt(i) tables for a Barnes-Hut opening test.
```

`constexpr` forces compile-time evaluation only in a *constant expression context*: initializing a `constexpr` variable, a `static_assert`, a template argument, an array bound. `int n = f(3);` may or may not be folded — usually is at `-O2`, but not guaranteed. Write `constexpr int n = f(3);` to guarantee it and to get a compile error if `f(3)` isn't constant.

**`consteval`** (C++20, "immediate function"): *must* be evaluated at compile time; calling it with a runtime argument is a compile error. Use it for things that make no sense at runtime — parsing a format string, computing a hash of a literal, `pow2(k)` for a table size. **`constinit`** (C++20): the variable *must* be statically initialized (its initializer is a constant expression), but it stays mutable. It exists to kill the static initialization order fiasco for globals (`constinit int g_steps = 1 << 10;`), and is useful for `thread_local` variables whose dynamic init would otherwise cost a guard check on every access.

| Keyword | Value can change at runtime | Initializer must be constant | Function must run at compile time |
|---|---|---|---|
| `const` | no | no | — |
| `constexpr` (variable) | no | yes | — |
| `constexpr` (function) | — | — | no (may) |
| `consteval` (function) | — | — | yes |
| `constinit` (variable) | **yes** | yes | — |

Compile-time evaluation is an *interpreter inside the compiler*: roughly 100–1000× slower than the compiled code, with a step limit (clang: `-fconstexpr-steps=N`, default 1 048 576; `-fconstexpr-depth` for recursion). A 4096-entry table of 12-term Taylor sums is fine; an FFT of 2²⁰ points at compile time is not.

---

## 11. `std::integral_constant`, class-type NTTPs, template template parameters

**`std::integral_constant<T, v>`** is a type that carries a value: `::value`, `constexpr operator T()`, `operator()`. `std::true_type` / `false_type` are `integral_constant<bool, true/false>`; `std::index_sequence` is a pack of them. Its use: turning a value into a *type* so it can participate in overload resolution and template specialization — `f(std::integral_constant<int, 3>{})` vs `f(3)` is the difference between compile-time and runtime dispatch. In C++20 code it is mostly replaced by NTTPs and `if constexpr`, but you'll meet it in every trait.

**Non-type template parameters** have always allowed integers, enums, pointers. C++20 adds *structural types*: literal class types whose bases and non-static members are all public, non-mutable, and themselves structural. This lets you write a `Shape{rows, cols}` as a template argument:

```cpp
struct Shape { std::size_t rows, cols; };
template <Shape S> struct StaticMatrix { std::array<double, S.rows * S.cols> d{}; /* ... */ };
template <Shape A, Shape B> requires (A.cols == B.rows)
StaticMatrix<Shape{A.rows, B.cols}> matmul(const StaticMatrix<A>&, const StaticMatrix<B>&);
StaticMatrix<Shape{2, 3}> a; StaticMatrix<Shape{3, 2}> b;
auto c = matmul(a, b);        // StaticMatrix<Shape{2,2}>; matmul(a, a) is a compile error
```

Also allowed: floating-point NTTPs (`template <double eps>` — C++20, clang supports it) and `fixed_string` (§14). Template arguments must be *template-argument-equivalent*: two `Shape{2,3}` are the same type because their members compare equal member-wise.

**Template template parameters** accept a template, not a type:

```cpp
template <class T, template <class...> class Container = std::vector>
struct Buffer { Container<T> items; };
Buffer<int> a;                       // vector<int>
Buffer<int, std::deque> b;           // deque<int>
// `class...` matches templates with any number of type parameters (vector has <T, Allocator>).
// Since C++17 (P0522) a template with default arguments matches a simpler template template parameter.
```

Use them when the *choice of container* is the parameter, e.g. a graph that can be `Graph<int, std::vector>` or `Graph<int, SmallVector>`. In practice a `class Storage` policy (§12) with a nested `template <class T> using type = ...` is more flexible.

---

## 12. Policy-based design

Andrei Alexandrescu's *Modern C++ Design* (2001): decompose a class into orthogonal *policies* — small classes that each decide one thing — and combine them as template arguments. The host class calls policy members; the compiler inlines everything; the wrong combination is a compile error.

```cpp
struct HeapStorage {                                    // policy: where the elements live
    template <class T> struct type {
        std::vector<T> d;  explicit type(std::size_t n) : d(n) {}
        T* data() { return d.data(); }  const T* data() const { return d.data(); }
    };
};
template <std::size_t N> struct StackStorage {
    template <class T> struct type {
        std::array<T, N> d{};  explicit type(std::size_t n) { assert(n <= N); }
        T* data() { return d.data(); }  const T* data() const { return d.data(); }
    };
};
template <class T, class StoragePolicy = HeapStorage>
class Matrix {
    std::size_t r_, c_;
    typename StoragePolicy::template type<T> s_;          // "template" keyword: dependent template name
public:
    Matrix(std::size_t r, std::size_t c) : r_(r), c_(c), s_(r * c) {}
    T& operator()(std::size_t i, std::size_t j) { return s_.data()[i * c_ + j]; }
};
Matrix<double> big(1000, 1000);                          // heap; 24 bytes + 8 MB elsewhere
Matrix<double, StackStorage<16>> small(3, 3);            // no allocation; sizeof == 144 bytes
```

Other policies for a numerics `Matrix`: `Layout` (row/column major — changes the index formula), `Allocator` (aligned for SIMD, arena for a per-time-step scratch pool, pinned memory for GPU transfers), `Checking` (bounds-checked in debug, unchecked in release), `Threading` (serial vs OpenMP loops). This is how the standard library is designed too: `std::vector<T, Allocator>`, `std::unordered_map<K, V, Hash, KeyEqual, Allocator>`, `std::basic_string<CharT, Traits, Allocator>`.

The cost is the combinatorial explosion of types: `Matrix<double, Heap, RowMajor, Checked>` and `Matrix<double, Heap, RowMajor, Unchecked>` are unrelated types, so functions that should accept both must be templates or take a common CRTP base. Eigen's `Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>` is policy-based design with six parameters and shows both the power (fixed-size stack matrices, alignment control) and the compile-error length.

---

## 13. `decltype(auto)`, `auto` return deduction, `std::invoke`, `std::apply`, tuple metaprogramming

**`auto` return type** deduces like `auto x = expr;` — strips references and top-level `const`. **`decltype(auto)`** deduces exactly `decltype(expr)`, keeping references, which is what a *transparent wrapper* needs:

```cpp
auto           by_value(std::size_t i) { return g[i]; }    // double  (a copy; assigning to it is meaningless)
decltype(auto) by_ref(std::size_t i)   { return (g[i]); }  // double& (the parentheses: decltype((name)) is a reference,
                                                           //          decltype(name) is the declared type)
template <class F, class... A>
decltype(auto) call_logged(F&& f, A&&... a) { return std::invoke(std::forward<F>(f), std::forward<A>(a)...); }
```

Danger: `decltype(auto) f() { int x = 1; return (x); }` returns a dangling `int&`. Only wrap with `decltype(auto)` when you forward someone else's return.

**`std::invoke(f, args...)`** (C++17, `<functional>`) calls anything callable uniformly — function, lambda, function object, **pointer to member function** (`invoke(&Layer::forward, layer, x)` = `layer.forward(x)`), **pointer to data member** (`invoke(&Layer::w, layer)` = `layer.w`), through references, pointers, or `reference_wrapper`. `std::function`, `std::thread`, `std::async`, `std::bind_front` and the ranges algorithms' projections (`std::ranges::sort(v, {}, &Particle::mass)`) are all defined in terms of `invoke`. `std::invoke_result_t<F, Args...>` gives the return type; `std::is_invocable_v` tests callability.

**`std::apply(f, tuple)`** calls `f(get<0>(t), get<1>(t), ...)` — the `index_sequence` idiom from §3 packaged. Use it to store argument packs (`std::tuple<Args...> saved;`) and replay them, to unpack a `std::array<double, 3>` into a `Vec3(x, y, z)` constructor, or to zip structured records. `std::make_from_tuple<T>(t)` constructs `T` from a tuple.

**Tuple metaprogramming** — the standard tools are `std::tuple_size_v<T>`, `std::tuple_element_t<I, T>`, `std::get<I>`, `std::get<Type>` (when unique), `std::tuple_cat`, `std::tie`, `std::forward_as_tuple`. With those and partial specialization you can build compile-time *type lists*:

```cpp
template <class... Ts> struct type_list { static constexpr std::size_t size = sizeof...(Ts); };
template <class T, class Tuple> struct index_of;                                // position of T in a tuple
template <class T, class... R> struct index_of<T, std::tuple<T, R...>>    : std::integral_constant<std::size_t, 0> {};
template <class T, class U, class... R> struct index_of<T, std::tuple<U, R...>>
    : std::integral_constant<std::size_t, 1 + index_of<T, std::tuple<R...>>::value> {};
static_assert(index_of<double, std::tuple<int, double, char>>::value == 1);
```

Where you use it for real: a `Sequential<Linear, ReLU, Linear>` layer stack stored as `std::tuple<Layers...>` where `forward` is a fold over the tuple and each layer's output type is the next layer's input type, checked at compile time; a `std::variant<Ts...>` of autograd ops where `index_of` gives the tag; an `ECS` (entity component system) for a particle sim where each component array is a tuple element.

---

## 14. Compile-time strings and `std::array` tricks

C++20 lets a class-type NTTP hold a string literal:

```cpp
template <std::size_t N> struct fixed_string {
    char data[N]{};
    constexpr fixed_string(const char (&s)[N]) { for (std::size_t i = 0; i < N; ++i) data[i] = s[i]; }
    constexpr std::string_view view() const { return {data, N - 1}; }
};
template <fixed_string Name> struct Named { static constexpr std::string_view name() { return Name.view(); } };
static_assert(Named<"embedding">::name() == "embedding");      // CTAD deduces fixed_string<10>
```

This is how `fmt`/`std::format` check format strings at compile time and how strongly-typed units libraries print `"m/s²"`. Related tricks:

- **Compile-time hash → `switch` on strings**: `constexpr uint64_t fnv1a(std::string_view)`; `switch (fnv1a(name)) { case fnv1a("relu"): ... }`. Case labels must be constants — they are. Collisions are astronomically unlikely for a handful of labels but not impossible; verify with a `static_assert` that your labels are distinct.
- **`std::array` as a constexpr container**: since C++20 `std::sort`, `std::is_sorted`, `std::binary_search`, `std::accumulate`, `std::iota`, `std::reverse` are all `constexpr`. Build a prime table, sort it, `static_assert` on it.
- **Concatenation and slicing via `index_sequence`**: `return {a[I]..., b[J]...};` with two index sequences — the pattern for `Vec<3>` ⊕ `Vec<2>` → `Vec<5>` and for splicing tensor shapes.
- **`std::to_array`** (C++20) turns a C array or braced list into a `std::array` with deduced size; `std::array<double, 3>{}` initializes to zero (`{}` matters — `std::array<double,3> a;` is uninitialized).

---

## 15. Metaprogramming costs: compile time, error messages, `-ftime-trace`

Templates are not free. Every distinct instantiation is a function the compiler must parse-substitute-check-codegen, and the object file grows by one copy. Numbers from this machine (Apple M-series, clang 21), compiling `example.cpp` (~800 lines, ~100 function instantiations) with `-O2`: 0.6 s total, 0.45 s frontend (parsing + instantiation), 0.14 s backend (optimization + codegen). A heavy Eigen translation unit takes 5–20 s, mostly frontend; a Boost.Spirit grammar takes minutes.

**Measure it**: `c++ -std=c++20 -O2 -ftime-trace -c file.cpp` writes `file.json`; open it in `chrome://tracing`, `https://ui.perfetto.dev`, or summarize with [ClangBuildAnalyzer](https://github.com/aras-p/ClangBuildAnalyzer). Look for `InstantiateFunction` / `InstantiateClass` events with large durations, and for headers that dominate `Source` time (→ precompiled headers or forward declarations). GCC's equivalent is `-ftime-report` (text). Rules that follow from the numbers:

| Cost | Cause | Mitigation |
|---|---|---|
| Frontend time | Deep recursion (recursive `sum`, `index_of`), huge headers | Fold expressions, `index_sequence`, `<type_traits>` intrinsics; include less |
| Backend time / binary size | Many instantiations of large functions | Type-erase the non-hot outer layer (`std::function`, virtual), keep templates for the inner kernel |
| Rebuild cascade | Everything is in headers | Explicit instantiation (`template class Matrix<double>;` in one .cpp), `extern template` in the header |
| Error message length | Errors surface at the deepest instantiation | Concepts (fail at the call site), `static_assert` with a message at the top of the template |

**Reading a template error**: the *first* line is the actual error; the following `note: in instantiation of ...` lines are the stack of templates that led there — read them bottom-up to find *your* call site (the one in your file, not in `/usr/include/c++/v1`). `-fno-elide-type` and `-fdiagnostics-show-template-tree` make clang print type differences as a tree. Concepts move the failure to the top of that stack. `static_assert(std::is_floating_point_v<T>, "Matrix<T> requires floating T");` as the first line of a class template is the C++17 equivalent.

---

## 16. When to stop

The rule: **templates for types, not for cleverness.** A template is justified when the code is genuinely the same for several types (`Matrix<float>` / `Matrix<double>`), when a compile-time constant enables a real optimization (`Vec<3>` unrolled, a `sin` table), or when laziness removes real temporaries (expression templates in a hot loop). It is not justified to avoid writing a second overload, to make a class "flexible" for uses that don't exist, or because the technique is interesting.

Signs to stop: a `requires` clause longer than the function body; a template parameter no caller ever sets to anything but the default; an error message you can't read yourself; a type name that doesn't fit on a line; a header that takes more than a second to compile; a colleague (or you in three months) asking what a function does. In each case the fix is the same — a concrete type, a plain overload, a `std::variant`, a virtual function, or `if constexpr`.

The library engineers whose code you admire agree: Eigen's *core* is heavily templated because a matrix library needs it; its documentation tells *users* to write `MatrixXd`, `Vector3d` and `auto`-free code. Your projects should look like Eigen's users, and only P02/P03/P06 should look like Eigen's internals — and only at the kernel.

---

## Gotchas and undefined behavior

- **`std::forward` without the explicit `<T>`** doesn't compile (the parameter is a non-deduced context by design). `std::forward<T>(x)` twice on the same `x` uses a moved-from object the second time.
- **Forwarding reference vs rvalue reference**: `template <class T> void f(std::vector<T>&&)` is an rvalue reference — no collapsing, lvalues rejected. Only bare `T&&`/`auto&&`.
- **`auto` captures an expression template**: `auto r = a*b + c;` holds references to `a`, `b`, `c`. Using it after they die is UB (dangling). Assign to the concrete type.
- **Storing `const T&` to a temporary node** inside an expression tree dangles. Store nested nodes by value (the `operand_t` trick), containers by reference.
- **CRTP `static_cast` to the wrong derived type** (`struct B : Base<A>`) compiles and is UB at the first call.
- **Empty fold over `+`, `*`, `<`, etc.** is a compile error; only `&&`, `||`, `,` have identities. Use the binary form `(x + ... + 0)`.
- **`decltype(auto)` returning `(local)`** returns a dangling reference.
- **UB in `constexpr` evaluation** (signed overflow, out-of-bounds `std::array` access, uninitialized read) is a compile error — good — but the same function called at runtime is just UB. `constexpr` doesn't make runtime calls safe.
- **`consteval` with a runtime argument** is a compile error even inside a `constexpr` function that happens to be called at runtime.
- **Two overloads differing only in `enable_if` default template *arguments*** are a redefinition; make the *type* differ (`std::enable_if_t<cond, int> = 0` vs `std::enable_if_t<!cond, long> = 0`).
- **SFINAE only applies to the immediate context.** An error inside the function body (or inside a nested template's body) is a hard error, however "conditional" it looks.
- **Concepts don't check bodies.** Satisfying `VectorLike` doesn't guarantee the template compiles for your type.
- **Equivalent constraints written without named concepts are ambiguous**, not ordered. Subsumption needs concept names.
- **NTTP of type `double`**: `Foo<0.1 + 0.2>` and `Foo<0.3>` are different types. Comparisons are bitwise-ish (template-argument-equivalence), and `-0.0` ≠ `0.0` here even though `==` says they're equal.
- **Static initialization order fiasco**: a global `constexpr`/`constinit` fixes it; a global initialized by a runtime call in another translation unit does not — order between TUs is unspecified.
- **Explicit specialization of a function template** is *not* an overload and does not participate in overload resolution the way you expect (Sutter, "Why Not Specialize Function Templates?"). Overload or specialize a class template instead.

---

## Common mistakes checklist

- [ ] Every forwarding-reference parameter is forwarded with `std::forward<T>` exactly once.
- [ ] No `std::move` on a forwarding reference; no `std::forward` on anything else.
- [ ] Packs processed with fold expressions or `index_sequence`, not recursion, unless recursion is genuinely clearer.
- [ ] New code uses concepts; `enable_if`/`void_t` appear only when reading or maintaining C++17 code.
- [ ] Ordering between constrained overloads goes through *named* concepts (subsumption).
- [ ] `if constexpr` for short branches; overloads for separate algorithms; tag dispatch only for legacy tags.
- [ ] Expression-template operators constrained to your own expression types; nested nodes stored by value.
- [ ] Matrix product node evaluates eagerly; users assign to a concrete type, never `auto`.
- [ ] Tables computed once with `constexpr` into `std::array`; `constexpr` variable (not just function) where compile-time is required.
- [ ] `constinit` on mutable globals with constant initializers; `consteval` only where runtime evaluation is nonsense.
- [ ] `decltype(auto)` only in transparent wrappers; `auto` return everywhere else.
- [ ] Policies are orthogonal and each decides one thing; a common CRTP base exists if functions must accept several policy combinations.
- [ ] `-ftime-trace` checked once per project; explicit instantiation used for the 2–3 types you actually use if compile time hurts.
- [ ] Each template answers "which types share this code?" — if the answer is "one", it isn't a template.

---

## You can move on when...

- You can write, from memory, the reference-collapsing table and explain what `std::forward<T>(x)` does when `T = Widget&` and when `T = Widget`.
- You can convert a recursive variadic `max` to a fold, and write `tuple_for_each` with `index_sequence` without looking.
- You can write a `has_foo<T>` detection trait with `void_t` and the same thing as a concept, and explain why the compiler error differs.
- Given an `enable_if` overload set from a real library header, you can rewrite it with concepts and say whether the overloads are ordered by subsumption.
- You can implement `Comparable<T>` and `Integrator<D>` with CRTP and state the two things CRTP cannot do that virtual functions can.
- You can implement expression templates for element-wise `Vec` operations, explain why nested nodes are stored by value, and explain why matrix product must evaluate eagerly.
- You can build a `constexpr` lookup table into a `std::array`, `static_assert` on its contents, and explain the difference between `constexpr`, `consteval`, and `constinit`.
- You can write a `Matrix<T, StoragePolicy>` and a `StaticMatrix<Shape{r, c}>`, and choose between them for a 3×3 rotation vs. a 1000×1000 weight matrix.
- You have run `-ftime-trace` on one of your own files and can name its most expensive instantiation.
