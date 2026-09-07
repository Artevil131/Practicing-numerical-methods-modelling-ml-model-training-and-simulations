# Chapter 11 — Function Pointers and Generics

## What you'll be able to do after this chapter

- Declare, store, and call a pointer to a function; read gnarly declarations like `double (*f)(double)` without panicking.
- Pass functions to functions: `map`, `integrate(f, a, b)`, `newton(f, df, x0)`.
- Build dispatch tables (arrays of function pointers) for activation functions and ops.
- Write callbacks that carry state through a `void *ctx` argument — C's substitute for closures.
- Write a generic (`void*` + element size) container and a macro-generated typed container, and know the cost of each.
- Design a `Layer` interface as a struct of function pointers — the skeleton of every C neural-net framework and autograd engine.

## Why this matters for ML / numerics / sims

In Python a function is a value: you pass `np.tanh` to something, you stash `lambda x: x*x` in a dict, `torch.autograd` stores a backward function on every node. C has one mechanism for all of that: the function pointer. Root finding (`newton(f, df, x0)`), numerical integration (`integrate(f, a, b)`), an ODE solver taking `dydt(t, y)`, an activation lookup `act_fns[layer->act]`, a `Layer` with `forward`/`backward`, an autograd `Op` that remembers how to propagate a gradient, a `qsort` comparator to sort tokens by frequency for BPE — all function pointers. Generic containers (`void*` or macro-generated) are how you get a `Vec<Token>` and a `Vec<Matrix*>` without writing the vector code twice.

---

## 1. Function pointer syntax

A function name, used without parentheses, evaluates to the address of the function's code. You can store it in a variable whose type is "pointer to function taking X and returning Y".

```c
#include <stdio.h>
#include <math.h>

double square(double x) { return x * x; }

int main(void) {
    double (*f)(double);   // f is a pointer to a function (double) -> double
    f = square;            // & is optional: square == &square
    printf("%f\n", f(3.0));      // call through the pointer: 9.000000
    printf("%f\n", (*f)(3.0));   // old-style explicit deref, same thing
    f = sqrt;                    // any function with matching signature
    printf("%f\n", f(16.0));     // 4.000000
    return 0;
}
```

How to read `double (*f)(double)`: start at the name `f`. `(*f)` — f is a pointer. `(double)` to the right — to a function taking a double. `double` on the left — returning double. The parentheses around `*f` are mandatory: `double *f(double)` declares a *function* `f` returning `double*`.

| Declaration | Meaning |
|---|---|
| `double (*f)(double)` | pointer to function `(double) -> double` |
| `double *f(double)` | function `f` returning `double*` (not a pointer variable) |
| `int (*cmp)(const void*, const void*)` | pointer to a `qsort`-style comparator |
| `void (*cb)(void*)` | pointer to a callback taking a `void*` |
| `double (*ops[4])(double)` | array of 4 pointers to `(double)->double` functions |

Python equivalent: `f = math.sqrt; f(16.0)`. The difference: in C the *type* of `f` pins the exact signature; you cannot store `pow` (two args) in `f`.

## 2. `typedef` for readability

Nobody writes `double (*)(double)` more than once. Name it:

```c
typedef double (*UnaryFn)(double);          // pointer-to-function type
typedef int (*Comparator)(const void *, const void *);

UnaryFn f = sin;
UnaryFn table[3] = { sin, cos, tanh };
double apply_twice(UnaryFn g, double x) { return g(g(x)); }
// apply_twice(sqrt, 16.0) == 2.0
```

Read a `typedef` of a function pointer exactly like a variable declaration, then mentally replace "variable" with "type name". Convention in this course: `XxxFn` for function pointer typedefs.

## 3. Passing functions to functions

### map / apply over an array

```c
void map_inplace(double *xs, size_t n, UnaryFn f) {
    for (size_t i = 0; i < n; i++) xs[i] = f(xs[i]);
}
// double v[3] = {1, 4, 9}; map_inplace(v, 3, sqrt);  -> {1, 2, 3}
```

Python equivalent: `xs = list(map(f, xs))` or `np.vectorize(f)(xs)`. NumPy's ufuncs are exactly "apply this scalar function over every element", but implemented in C with a loop like this.

### Numerical integration

```c
// Composite midpoint rule with n subintervals.
double integrate(UnaryFn f, double a, double b, int n) {
    double h = (b - a) / n, sum = 0.0;
    for (int i = 0; i < n; i++) sum += f(a + (i + 0.5) * h);
    return sum * h;
}
// integrate(sin, 0, M_PI, 1000) -> 2.000000 (exact: 2)
```

### Root finding: Newton's method

Newton needs the function *and* its derivative, so it takes two function pointers:

```c
double newton(UnaryFn f, UnaryFn df, double x0, double tol, int max_iter) {
    double x = x0;
    for (int i = 0; i < max_iter; i++) {
        double fx = f(x);
        if (fabs(fx) < tol) break;
        x -= fx / df(x);
    }
    return x;
}
static double g(double x)  { return x * x - 2.0; }
static double dg(double x) { return 2.0 * x; }
// newton(g, dg, 1.0, 1e-12, 50) -> 1.414214
```

Python equivalent: `scipy.optimize.newton(g, 1.0, fprime=dg)`. Same idea, same signature shape.

## 4. Arrays of function pointers: dispatch tables

An enum indexes an array of function pointers. This replaces a `switch` with a table lookup, and makes "add a new activation" a two-line change.

```c
typedef enum { ACT_IDENTITY, ACT_RELU, ACT_SIGMOID, ACT_TANH, ACT_COUNT } Activation;

static double act_identity(double x) { return x; }
static double act_relu(double x)     { return x > 0 ? x : 0; }
static double act_sigmoid(double x)  { return 1.0 / (1.0 + exp(-x)); }

static const UnaryFn act_fns[ACT_COUNT] = {
    [ACT_IDENTITY] = act_identity,   // designated initializers: index by enum
    [ACT_RELU]     = act_relu,
    [ACT_SIGMOID]  = act_sigmoid,
    [ACT_TANH]     = tanh,
};
static const char *act_names[ACT_COUNT] = { "identity", "relu", "sigmoid", "tanh" };

// act_fns[ACT_RELU](-3.0) -> 0.0
// act_fns[ACT_TANH](0.5)  -> 0.462117
```

Keep the derivative table parallel (`act_dfns[ACT_COUNT]`) and backprop becomes `act_dfns[layer->act](z)`. Pair with X-macros (`../14_preprocessor_and_c_idioms/lesson.md`) so the enum, the name table, and the function table come from one list.

Python equivalent: `ACTS = {"relu": F.relu, "tanh": torch.tanh}; ACTS[name](x)`.

## 5. Callbacks with `void *ctx` — closures, the C way

A Python closure captures variables: `lambda x: a*x + b` remembers `a` and `b`. A C function pointer points only at code — it has no attached data. The C idiom: every callback takes an extra `void *ctx` ("context", "user data") which the *caller* passes back untouched. You put your captured state in a struct and pass its address.

```c
typedef double (*UnaryCtxFn)(double x, void *ctx);

typedef struct { double a, b; } Affine;
static double affine(double x, void *ctx) {
    const Affine *p = ctx;               // cast back to the real type
    return p->a * x + p->b;
}

double integrate_ctx(UnaryCtxFn f, void *ctx, double lo, double hi, int n) {
    double h = (hi - lo) / n, s = 0;
    for (int i = 0; i < n; i++) s += f(lo + (i + 0.5) * h, ctx);
    return s * h;
}

// Affine p = {.a = 2, .b = 1};
// integrate_ctx(affine, &p, 0, 1, 100) -> 2.000000  (∫ 2x+1 dx from 0 to 1)
```

`integrate_ctx` never looks inside `ctx`; it only ferries it. This pattern is everywhere: `pthread_create(..., fn, arg)`, GUI event handlers, `qsort_r`, ODE solvers taking `(t, y, params)`. Anywhere you would write a Python closure or `functools.partial`, write a struct + `void *ctx`.

## 6. `qsort` in depth

```c
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
```

`qsort` sorts *any* array because it knows nothing about element types: it gets a byte pointer, a count, an element size, and a comparator that returns negative / zero / positive (like Python's old `cmp`). Your comparator receives `const void*` pointers *to elements* and must cast them.

```c
typedef struct { const char *tok; int count; } TokenFreq;

static int cmp_double_asc(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);      // never `return x - y;` for doubles (truncates to int)
}
static int cmp_tokfreq_desc(const void *a, const void *b) {
    const TokenFreq *p = a, *q = b;
    if (p->count != q->count) return q->count - p->count;   // ints: safe if no overflow
    return strcmp(p->tok, q->tok);                          // tie-break for determinism
}

// double xs[] = {3.2, -1.0, 2.5};
// qsort(xs, 3, sizeof xs[0], cmp_double_asc);      -> -1.0 2.5 3.2
// TokenFreq tf[] = {{"the", 50}, {"a", 50}, {"cat", 7}};
// qsort(tf, 3, sizeof tf[0], cmp_tokfreq_desc);   -> a:50 the:50 cat:7
```

Rules:
- Comparator must be a consistent total order or `qsort` may behave arbitrarily (not UB in the standard sense, but garbage output).
- `return a - b` is fine for small ints only; for `double`, `size_t`, or ints near `INT_MAX` use the `(x > y) - (x < y)` trick.
- `qsort` is not stable. If you need stability, add an index field and tie-break on it.
- `bsearch` uses the same comparator shape for binary search over a sorted array.

Python equivalent: `sorted(tf, key=lambda t: (-t.count, t.tok))`. C has no `key=`; the comparator does the work.

## 7. Generic containers via `void*` + element size

The `qsort` trick generalizes: a container that stores raw bytes, given the element size at creation, can hold anything.

```c
typedef struct {
    unsigned char *data;    // raw bytes
    size_t len, cap;        // in elements
    size_t elem_size;       // bytes per element
} Vec;

Vec vec_new(size_t elem_size) { return (Vec){ NULL, 0, 0, elem_size }; }

int vec_push(Vec *v, const void *elem) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 4;
        void *p = realloc(v->data, ncap * v->elem_size);
        if (!p) return -1;
        v->data = p; v->cap = ncap;
    }
    memcpy(v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return 0;
}
void *vec_at(Vec *v, size_t i) { return v->data + i * v->elem_size; }
void  vec_free(Vec *v) { free(v->data); *v = (Vec){0}; }

// Vec v = vec_new(sizeof(double));
// double x = 2.5; vec_push(&v, &x);
// printf("%f\n", *(double *)vec_at(&v, 0));   -> 2.500000
```

Note: `vec_push(&v, &x)` takes the *address* of `x` because the function only knows "copy `elem_size` bytes from here".

### The cost

| | `void*` generic | Typed (macro-generated) |
|---|---|---|
| Type safety | none — pushing an `int` into a `Vec` of `double` compiles silently | full — compiler rejects it |
| Syntax | `*(double *)vec_at(&v, i)` | `v.data[i]` |
| Code size | one copy of the code | one copy per type |
| Speed | `memcpy` of runtime-sized elements; compiler can't specialize | fully inlined, optimal |

Python equivalent: a Python `list` is a `void*` container — it stores `PyObject*` to anything. NumPy arrays are the typed version: one `dtype`, contiguous, fast.

## 8. Macro-based generics: `VEC_DEFINE(T)`

The preprocessor can stamp out a typed vector per element type. Token pasting `##` builds names like `Vec_double`, `vec_double_push`.

```c
#define VEC_DEFINE(T)                                                       \
    typedef struct { T *data; size_t len, cap; } Vec_##T;                  \
    static inline int vec_##T##_push(Vec_##T *v, T x) {                    \
        if (v->len == v->cap) {                                             \
            size_t ncap = v->cap ? v->cap * 2 : 4;                          \
            T *p = realloc(v->data, ncap * sizeof *p);                      \
            if (!p) return -1;                                              \
            v->data = p; v->cap = ncap;                                     \
        }                                                                   \
        v->data[v->len++] = x;                                              \
        return 0;                                                           \
    }                                                                       \
    static inline void vec_##T##_free(Vec_##T *v) { free(v->data); *v = (Vec_##T){0}; }

VEC_DEFINE(double)   // generates Vec_double, vec_double_push, vec_double_free
VEC_DEFINE(int)      // generates Vec_int, ...

// Vec_double v = {0};
// vec_double_push(&v, 1.5);      v.data[0] == 1.5, typed, no casts
// vec_double_push(&v, "oops");   compile error — that's the point
```

`T` must be a single identifier (`double`, `int`, a typedef'd struct name) because it's pasted into names. `Vec_##T` with `T = unsigned char` won't work; `typedef unsigned char u8;` first. Debugging: compile with `-E` to see the expanded text (`../14_preprocessor_and_c_idioms/lesson.md`).

## 9. C11 `_Generic`: overload-like dispatch

C has no function overloading, but `_Generic` selects an expression based on the *type* of a controlling expression at compile time. It is how `<tgmath.h>` makes `sqrt(x)` call `sqrtf` for `float` and `sqrt` for `double`.

```c
#define ABS(x) _Generic((x),        \
    int:    abs,                    \
    long:   labs,                   \
    float:  fabsf,                  \
    double: fabs                    \
)(x)

#define TYPE_NAME(x) _Generic((x), int: "int", double: "double", \
                                   float: "float", char *: "char*", default: "other")

// ABS(-3)      -> 3          (calls abs)
// ABS(-2.5)    -> 2.500000   (calls fabs)
// TYPE_NAME(1.0f) -> "float"
```

Rules: the controlling expression is not evaluated (only its type matters); types must be distinct; `default:` is optional. `_Generic` is dispatch by type, not by value — no runtime cost.

Python equivalent: there is none exactly — Python dispatches on runtime type (`isinstance`, `functools.singledispatch`). `_Generic` is resolved entirely by the compiler.

## 10. A struct of function pointers: the vtable / interface

Put function pointers inside a struct and you have an *interface*: any object that fills in the slots can be used uniformly. This is what C++ virtual functions compile to (a hidden pointer to a "vtable"), and it is precisely how a neural-net framework in C is structured.

```c
typedef struct Layer Layer;
struct Layer {
    const char *name;
    // The interface. Every layer type fills these in.
    void (*forward)(Layer *self, const double *in, double *out, size_t n);
    void (*backward)(Layer *self, const double *grad_out, double *grad_in, size_t n);
    void (*free)(Layer *self);
    void *state;      // per-layer parameters/cache (like self.weight in nn.Module)
};

// One concrete layer: ReLU. Its state is the cached input mask.
typedef struct { unsigned char *mask; size_t n; } ReluState;

static void relu_forward(Layer *self, const double *in, double *out, size_t n) {
    ReluState *s = self->state;
    for (size_t i = 0; i < n; i++) { s->mask[i] = in[i] > 0; out[i] = s->mask[i] ? in[i] : 0; }
}
static void relu_backward(Layer *self, const double *g_out, double *g_in, size_t n) {
    ReluState *s = self->state;
    for (size_t i = 0; i < n; i++) g_in[i] = s->mask[i] ? g_out[i] : 0;
}
static void relu_free(Layer *self) { ReluState *s = self->state; free(s->mask); free(s); }

Layer relu_layer(size_t n) {
    ReluState *s = malloc(sizeof *s);
    s->mask = calloc(n, 1); s->n = n;
    return (Layer){ "relu", relu_forward, relu_backward, relu_free, s };
}

// Training loop doesn't care which layer it is:
//   for each layer L:  L->forward(L, x, y, n);
//   for each layer L in reverse: L->backward(L, gy, gx, n);
```

The `self` parameter is the same trick as `void *ctx` from section 5: the function needs access to its own object's data. PyTorch's `nn.Module.forward(self, x)` has the identical shape; Python just passes `self` for you.

### Autograd `Op` storing its backward function

In a scalar autograd engine (micrograd-style), each node records how it was made and how to push a gradient to its parents:

```c
typedef struct Value Value;
typedef void (*BackwardFn)(Value *self);

struct Value {
    double data, grad;
    Value *parents[2];
    BackwardFn backward;     // NULL for leaves
};

static void mul_backward(Value *v) {
    v->parents[0]->grad += v->parents[1]->data * v->grad;
    v->parents[1]->grad += v->parents[0]->data * v->grad;
}
Value *val_mul(Value *a, Value *b) {
    Value *v = calloc(1, sizeof *v);
    v->data = a->data * b->data;
    v->parents[0] = a; v->parents[1] = b;
    v->backward = mul_backward;
    return v;
}
// Backward pass: topological order, then  for each v: if (v->backward) v->backward(v);
```

PyTorch does exactly this: every tensor produced by an op has `grad_fn` (e.g. `MulBackward0`), a pointer to the function that knows how to compute parent gradients. `Value.backward` *is* `grad_fn`.

## 11. Comparison with Python

| Python | C |
|---|---|
| `f = math.sqrt` | `UnaryFn f = sqrt;` |
| `lambda x: a*x + b` (closure) | function + struct + `void *ctx` |
| `functools.partial(f, a=2)` | same: struct of bound args + ctx |
| `ops = {"relu": F.relu}` | `UnaryFn act_fns[ACT_COUNT]` |
| `sorted(xs, key=...)` | `qsort(xs, n, sizeof *xs, cmp)` |
| `list` (holds anything) | `Vec` of `void*`/bytes + `elem_size` |
| `np.ndarray` with dtype | `VEC_DEFINE(T)` typed container |
| `nn.Module` with `forward` | `struct Layer { forward, backward, free, state }` |
| `tensor.grad_fn` | `Value.backward` function pointer |
| `@singledispatch` (runtime) | `_Generic` (compile time) |

---

## Gotchas and undefined behavior

- **Calling through a mismatched pointer type is UB.** Casting `int (*)(int)` to `double (*)(double)` and calling it is undefined — the calling convention differs. Never cast function pointers to bridge signature mismatches; write a wrapper function instead.
- **Function pointers are not `void*`.** Converting a function pointer to `void*` is not guaranteed by the C standard (it works on POSIX, but `-Wpedantic` warns). Store function pointers in function pointer types.
- **Calling a NULL function pointer is UB** (a crash in practice). Check `if (L->backward)` for optional slots.
- **`return a - b` in comparators** overflows for large ints and truncates for `double`. Use `(a > b) - (a < b)`.
- **Inconsistent comparator** (violates transitivity, or `cmp(a,b)` and `cmp(b,a)` both positive) gives garbage sort output.
- **`void*` container of the wrong element size**: `vec_new(sizeof(float))` then pushing a `double` copies 4 of its 8 bytes. No warning. This is the price of `void*` genericity.
- **`memcpy` of structs containing pointers** copies the pointers, not what they point at (shallow copy). Two elements then share a heap buffer; freeing both is a double free.
- **Macro generics and `T` with spaces**: `VEC_DEFINE(unsigned int)` breaks token pasting. Use a typedef.
- **`_Generic` and lvalue conversion**: the controlling expression undergoes lvalue conversion, so `const int x` matches `int:`, not `const int:`; arrays decay to pointers (`char[4]` matches `char *`).
- **Storing `sizeof` of the wrong thing**: `qsort(arr, n, sizeof arr, cmp)` (size of the whole array) instead of `sizeof arr[0]`.

## Common mistakes checklist

- [ ] Wrote `double *f(double)` when you meant `double (*f)(double)`.
- [ ] Forgot to pass `ctx`/`self` through to the callback, or captured stack state whose lifetime ended (callback fires after the struct went out of scope).
- [ ] Passed `x` instead of `&x` to a `void*`-based `vec_push`.
- [ ] Dispatch table not indexed by the enum (`act_fns[3]` instead of `act_fns[ACT_TANH]`), or table length not `ACT_COUNT`.
- [ ] Comparator returns `bool` (0/1) instead of negative/zero/positive.
- [ ] A `Layer` slot left uninitialized (garbage function pointer) — use compound literals with all fields or `calloc` and check for NULL.
- [ ] Freed `state` but not the mask inside it (or vice versa) — write `free` alongside `create`, every time.

## You can move on when...

- You can write `typedef` for a function pointer type and explain why the parentheses in `(*f)` are needed.
- You can write `integrate(f, a, b, n)` and `newton(f, df, x0)` from memory and call them with `sin`, `cos`, and your own functions.
- You can explain what `void *ctx` is for and convert a Python closure into a struct + callback.
- You can write a correct `qsort` comparator for a struct with two keys.
- You can implement `vec_push` for a `void*` container and explain what it cannot check.
- You can sketch `struct Layer` with `forward`/`backward`/`free` and say which PyTorch concept each slot corresponds to.
