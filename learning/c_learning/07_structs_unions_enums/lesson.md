# Chapter 07 — Structs, Unions, Enums

## What you'll be able to do after this chapter

- Define your own compound types with `struct`, initialize them three ways, and access fields through values and pointers.
- Decide correctly between passing a struct by value or by pointer, and avoid the shallow-copy double-free trap.
- Predict `sizeof` a struct by reasoning about padding, and reorder fields to shrink it.
- Use `enum` for named constants and `union` for reinterpreting bytes (the float-to-bits trick).
- Build a tagged union — the C representation of "one of several kinds of value" — which is how autograd ops and JSON nodes are stored.
- Write a `Matrix` type (`rows, cols, data`) and sketch a `Layer` type you will reuse for the rest of the course.

## Why this matters for ML / numerics / sims

Everything you build from here is a struct. A matrix is `{rows, cols, data}`. A neural-net layer is `{W, b, activation}`. A particle is `{x, y, vx, vy, mass}`. An autograd node is `{op, inputs, value, grad}`. Python gives you classes and dataclasses; C gives you `struct`, and nothing else — no methods, no constructors, no destructors, no inheritance. You will write `mat_new()` and `mat_free()` as plain functions and call them by hand. That discipline is exactly what makes C programs fast and predictable: the memory layout of a struct is *specified*, so you can `fwrite` it, hand it to a GPU, or stream it through a cache at full bandwidth.

---

## 1. Definition vs declaration vs instance

```c
struct Point {          // DEFINITION of a type named `struct Point`. Allocates nothing.
    double x;
    double y;
};

struct Point p;         // INSTANCE: 16 bytes of storage named p (uninitialized here)
struct Point q = {1.0, 2.0};
```

In C, the type is called `struct Point` — the word `struct` is part of the name. To avoid typing it everywhere:

```c
typedef struct {        // anonymous struct, immediately given the alias `Point`
    double x, y;
} Point;

Point p = {1.0, 2.0};   // no `struct` keyword needed
```

Both forms are common. The tagged form is required when the struct refers to itself (a linked-list node needs `struct Node *next` inside the definition, before the typedef name exists):

```c
typedef struct Node {
    int value;
    struct Node *next;   // must say `struct Node` here; `Node` is not defined yet
} Node;
```

A *forward declaration* `struct Matrix;` tells the compiler the type exists without saying what is in it. You can declare pointers to it but not instances (its size is unknown). Section 13 uses this for the opaque pattern.

**Python equivalent:** `@dataclass class Point: x: float; y: float`. But a C struct is a fixed block of bytes, not a dictionary of attributes; you cannot add fields at runtime.

---

## 2. Initialization: positional, designated, zero

```c
typedef struct { size_t rows, cols; double *data; } Matrix;

Matrix a = {2, 3, NULL};                 // positional: in declaration order
Matrix b = {.rows = 2, .cols = 3};       // designated (C99): named; omitted fields become 0/NULL
Matrix c = {0};                          // everything zero / NULL
Matrix d;                                // UNINITIALIZED: fields hold garbage (UB to read)
```

Designated initializers are the safest: they survive field reordering and make the code self-documenting. `{0}` is the idiom for "all fields zero" — it sets the first field to 0 and the standard guarantees the rest are zero-initialized too. (Compilers may warn about `{0}` under some flags; `{}` is C23 — stick with `{0}` for C11.)

Since C99 you can also create a struct value inline with a *compound literal*:

```c
Matrix e = (Matrix){.rows = 4, .cols = 4, .data = NULL};
mat_print(&(Matrix){.rows = 1, .cols = 1, .data = one});   // temporary, address taken
```

---

## 3. Access: `.` vs `->`

```c
Point p = {1, 2};
Point *pp = &p;

p.x       // field of a struct value
pp->x     // field via pointer; identical to (*pp).x
(*pp).x   // works but ugly
```

`->` exists because `*pp.x` would parse as `*(pp.x)` (wrong). Rule: if the thing on the left is a pointer, use `->`; if it is a struct, use `.`. Chained: `net->layers[0].W->data[5]` — read left to right, choosing `.` or `->` at each step based on what you have.

---

## 4. Nested structs, arrays of structs, structs containing arrays and pointers

```c
typedef struct { double x, y; } Vec2;
typedef struct {
    Vec2   pos;          // nested struct: embedded inline, 16 bytes
    Vec2   vel;
    double mass;
    int    id;
} Particle;

Particle ps[3] = {
    {.pos = {0, 0}, .vel = {1, 0}, .mass = 1.0, .id = 0},
    {.pos = {1, 0}, .vel = {0, 1}, .mass = 2.0, .id = 1},
    {.pos = {0, 1}, .vel = {-1, 0}, .mass = 0.5, .id = 2},
};
ps[1].pos.x += ps[1].vel.x * 0.01;      // nested access

typedef struct {
    char   name[32];     // array INSIDE the struct: 32 bytes are part of the struct itself
    double *weights;     // pointer: 8 bytes in the struct; the doubles live elsewhere (heap)
    size_t n;
} Layer;
```

The distinction in `Layer` is critical: `name` is *inside* the struct (copying the struct copies the name), while `weights` is a *pointer* (copying the struct copies only the address — see Section 7).

An array of structs (`Particle ps[N]`) is the "AoS" layout: each particle's fields are adjacent. The alternative, "SoA" (`double xs[N], ys[N], ...`), is often faster for SIMD/cache when you loop over one field at a time. NumPy's structured arrays are AoS; separate NumPy arrays are SoA.

---

## 5. Passing structs: by value vs by pointer

```c
double norm_val(Vec2 v)        { return sqrt(v.x*v.x + v.y*v.y); }   // copies 16 bytes
double norm_ptr(const Vec2 *v) { return sqrt(v->x*v->x + v->y*v->y); } // copies 8 (the pointer)

void scale_val(Vec2 v, double k)  { v.x *= k; v.y *= k; }   // modifies the COPY: caller sees nothing
void scale_ptr(Vec2 *v, double k) { v->x *= k; v->y *= k; } // modifies the caller's struct
```

| Pass by | Cost | Callee can modify caller's struct? | Use when |
|---|---|---|---|
| value (`Vec2 v`) | copies whole struct | no (works on a copy) | small plain-data structs (≤ ~32 bytes), no heap pointers inside, read-only use |
| pointer (`Vec2 *v`) | copies 8 bytes | yes | large structs, or you need to modify |
| const pointer (`const Vec2 *v`) | copies 8 bytes | no (compiler enforces) | large structs, read-only |

Anything containing a heap pointer (`Matrix`, `Layer`) should be passed by pointer, always. Passing it by value creates a second struct pointing at the *same* heap block, which is the setup for Section 7's trap. Small PODs like `Vec2`, `Complex`, `Color` are fine by value — the compiler passes them in registers.

**Python equivalent:** Python always passes object references (like pointers). C by-value is genuinely different: the callee gets an independent copy.

---

## 6. Returning structs

Returning a small struct by value is idiomatic and efficient:

```c
Vec2 vec2_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
Vec2 c = vec2_add(a, b);
```

For structs with heap pointers, you have two conventions — pick one and document it:

```c
/* Returns a Matrix by value; the struct is small (24 bytes), but the caller
 * now OWNS m.data and must call mat_free(&m). */
Matrix mat_zeros(size_t rows, size_t cols) {
    Matrix m = {rows, cols, calloc(rows * cols, sizeof(double))};
    return m;         // m.data may be NULL on failure; caller checks
}

/* Fills *out. Caller owns out->data afterwards. Returns 0 or -1. */
int mat_init(Matrix *out, size_t rows, size_t cols);
```

Returning a struct by value is *not* returning a stack array — the struct's bytes are copied to the caller. What you must not return is a pointer *to* a local struct.

---

## 7. Struct assignment copies shallowly — the double-free trap

```c
Matrix a = mat_zeros(2, 2);
Matrix b = a;            // copies rows, cols, AND THE POINTER. Not the 32 bytes of data.
b.data[0] = 5.0;         // a.data[0] is now 5.0 too — same block
mat_free(&a);            // frees the block
mat_free(&b);            // DOUBLE FREE: b.data points at the same, already-freed block
```

```
a: [rows=2][cols=2][data ●]──┐
                             ├──> [0.0][0.0][0.0][0.0]   ONE heap block, TWO owners
b: [rows=2][cols=2][data ●]──┘
```

`=` on structs is `memcpy`. It knows nothing about what the pointers mean. If you want an independent copy, write `mat_copy` that allocates a new block and copies the doubles. If you want to hand off ownership, copy the struct and then null out the source's pointer so it can no longer free it.

**Python equivalent:** `b = a` for a NumPy array also makes both names refer to the same buffer — but Python's refcount handles freeing. In C, you are the refcount.

---

## 8. Padding and alignment: `sizeof` surprises

Each field must sit at an address that is a multiple of its alignment (`double`: 8, `int`: 4, `char`: 1, pointers: 8 on arm64). The compiler inserts invisible padding bytes to achieve this, and pads the end so arrays of the struct keep every element aligned.

```c
struct Bad  { char c; double d; int i; };       // sizeof == 24
struct Good { double d; int i; char c; };       // sizeof == 16
```

```
Bad:   [c][pad pad pad pad pad pad pad][d d d d d d d d][i i i i][pad pad pad pad]
        0  1                         7  8              15 16     19 20          23
Good:  [d d d d d d d d][i i i i][c][pad pad pad]
        0              7  8     11 12 13         15
```

Rule of thumb: order fields from largest alignment to smallest and padding is minimized. `sizeof(struct)` is always a multiple of its strictest member's alignment.

`offsetof(type, field)` from `<stddef.h>` gives a field's byte offset:

```c
#include <stddef.h>
printf("%zu %zu %zu\n", offsetof(struct Bad, c), offsetof(struct Bad, d), offsetof(struct Bad, i));
// 0 8 16
```

Why you care: a `Particle` array of 10 million elements at 40 bytes vs 32 bytes is 80 MB of difference and 25% more cache misses. And if you `fwrite` a struct to disk, the padding bytes go with it (Chapter 08).

`_Alignof(type)` gives the alignment; `_Alignas(64)` on a field or variable forces stricter alignment (e.g. to a cache line).

---

## 9. Comparing structs

There is no `==` for structs. `memcmp` is wrong in general: padding bytes hold garbage, and `0.0 == -0.0` in floating point but their bytes differ. Write a function:

```c
int vec2_eq(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }

int mat_eq(const Matrix *a, const Matrix *b, double tol) {
    if (a->rows != b->rows || a->cols != b->cols) return 0;
    for (size_t i = 0; i < a->rows * a->cols; i++)
        if (fabs(a->data[i] - b->data[i]) > tol) return 0;
    return 1;
}
```

**Python equivalent:** `np.allclose(a, b)`.

---

## 10. `union`: same memory, different views

A `union` is like a struct where every field starts at offset 0. Its size is the size of the largest member. Writing one member and reading another *reinterprets the same bytes* — the classic use is looking at a float's bit pattern:

```c
#include <stdint.h>
typedef union { float f; uint32_t u; } FloatBits;

FloatBits fb = {.f = 1.0f};
printf("%08x\n", fb.u);            // 3f800000  (sign 0, exponent 127, mantissa 0)
fb.u ^= 0x80000000u;               // flip the sign bit
printf("%f\n", fb.f);              // -1.000000
```

```
FloatBits (4 bytes):   [ byte0 byte1 byte2 byte3 ]
                       ^ f (float) starts here
                       ^ u (uint32_t) starts here -- same four bytes
```

Type-punning through a union is explicitly allowed in C (it is *not* in C++; there you use `memcpy`). This trick is how you implement fast inverse square roots, inspect NaN payloads, or extract the exponent of a double for range reduction in a numerics library.

A union alone does not know which member is "active". That is what the enum in the next section is for.

---

## 11. `enum`: named integer constants

```c
typedef enum { ACT_RELU, ACT_TANH, ACT_SIGMOID, ACT_COUNT } Activation;
//               0         1         2            3  (auto-numbered from 0)

double activate(Activation a, double x) {
    switch (a) {
        case ACT_RELU:    return x > 0 ? x : 0;
        case ACT_TANH:    return tanh(x);
        case ACT_SIGMOID: return 1.0 / (1.0 + exp(-x));
        case ACT_COUNT:   break;          // not a real activation
    }
    return 0.0 / 0.0;                     // NaN: unreachable if callers are sane
}
```

An enum is an `int` with names. You can assign explicit values (`ACT_RELU = 10`), use them as array sizes (`const char *names[ACT_COUNT]`), and — most usefully — `switch` on them. With `-Wall`, the compiler warns if a `switch` on an enum misses a case and has no `default`, which catches "I added a new op and forgot to handle it" bugs. That is why the example above lists every case and avoids `default`.

Enums are not type-safe: `Activation a = 42;` compiles. Validate values that come from files or user input.

**Python equivalent:** `enum.Enum`, or the strings `"relu"`, `"tanh"` you pass to Keras. Comparing an int is far faster than `strcmp`, which matters in a hot inner loop.

---

## 12. Tagged unions: enum + union = a `Value` type

C has no `Union[int, float, str]` or class hierarchy. The idiom for "one of several kinds of thing" is a struct holding a **tag** (enum) and a **union** of payloads:

```c
typedef enum { VAL_NUM, VAL_STR, VAL_VEC } ValueKind;

typedef struct {
    ValueKind kind;                 // which union member is live
    union {
        double num;
        char  *str;                 // owned heap string
        struct { double *data; size_t n; } vec;
    } as;
} Value;

void value_print(const Value *v) {
    switch (v->kind) {
        case VAL_NUM: printf("%g", v->as.num); break;
        case VAL_STR: printf("\"%s\"", v->as.str); break;
        case VAL_VEC: printf("vec[%zu]", v->as.vec.n); break;
    }
}
void value_free(Value *v) {          // the tag tells us what to free
    if (v->kind == VAL_STR) free(v->as.str);
    if (v->kind == VAL_VEC) free(v->as.vec.data);
}
```

Reading a union member other than the one last written is only meaningful for type-punning; reading `as.str` when `kind == VAL_NUM` gives you a garbage pointer. The tag is your only protection — always switch on it.

Where you will use this:

- **Autograd:** `typedef enum { OP_LEAF, OP_ADD, OP_MUL, OP_TANH } OpKind;` and a `Node { OpKind op; Node *a, *b; double val, grad; }`. Backward pass = `switch (n->op)`.
- **JSON / config parsing:** null/bool/number/string/array/object as a tagged union.
- **Interpreter values, AST nodes, event queues in a sim** (`EVT_COLLISION`, `EVT_SPAWN`, ...).

**Python equivalent:** a class hierarchy with `isinstance` checks, or a `dict` with a `"type"` key. The C version is one contiguous block with zero indirection.

---

## 13. Opaque struct pattern (declare in .h, define in .c)

Sometimes you want callers to use a type only through functions, never touching fields directly. Declare the struct in the header without a body; define it in the .c file:

```c
/* rng.h */
typedef struct Rng Rng;             // incomplete type: exists, contents unknown
Rng  *rng_new(uint64_t seed);       // returns ownership
double rng_uniform(Rng *r);
void  rng_free(Rng *r);

/* rng.c */
struct Rng { uint64_t state; };     // full definition, private to this file
Rng *rng_new(uint64_t seed) { Rng *r = malloc(sizeof *r); if (r) r->state = seed; return r; }
```

Callers can hold `Rng *` but cannot write `r->state` (compile error: incomplete type) or declare `Rng r;` on the stack (unknown size). You can change the internals without recompiling callers. Cost: every instance must be heap-allocated. This is how `FILE *` works — you never see what is inside a `FILE`. Chapter 09 covers the header/source split in detail.

---

## 14. The `Matrix` struct — running example

```c
typedef struct {
    size_t  rows, cols;
    double *data;          // owned; rows*cols doubles, row-major: data[i*cols + j]
} Matrix;

/* Returns a zeroed matrix. Caller owns m.data; release with mat_free.
 * On allocation failure, data is NULL and rows = cols = 0. */
Matrix mat_zeros(size_t rows, size_t cols) {
    Matrix m = {rows, cols, calloc(rows * cols, sizeof *m.data)};
    if (!m.data) m.rows = m.cols = 0;
    return m;
}

/* Frees m->data and resets m to an empty matrix. Safe to call twice. */
void mat_free(Matrix *m) { free(m->data); m->data = NULL; m->rows = m->cols = 0; }

static inline double  mat_get(const Matrix *m, size_t i, size_t j) { return m->data[i * m->cols + j]; }
static inline void    mat_set(Matrix *m, size_t i, size_t j, double v) { m->data[i * m->cols + j] = v; }

/* Returns a deep copy (new heap block). Caller owns the result. */
Matrix mat_copy(const Matrix *src) {
    Matrix m = mat_zeros(src->rows, src->cols);
    if (m.data) memcpy(m.data, src->data, src->rows * src->cols * sizeof *m.data);
    return m;
}

/* out = a @ b. out must already be allocated with a->rows x b->cols. Returns 0 or -1 on shape mismatch. */
int mat_matmul(Matrix *out, const Matrix *a, const Matrix *b) {
    if (a->cols != b->rows || out->rows != a->rows || out->cols != b->cols) return -1;
    for (size_t i = 0; i < a->rows; i++)
        for (size_t k = 0; k < a->cols; k++) {
            double aik = mat_get(a, i, k);
            for (size_t j = 0; j < b->cols; j++)
                out->data[i * out->cols + j] += aik * mat_get(b, k, j);
        }
    return 0;
}
```

The `i-k-j` loop order in `mat_matmul` is not arbitrary: the innermost loop walks `b` and `out` along contiguous memory, which is several times faster than the textbook `i-j-k` order on large matrices. That is a struct-with-flat-data payoff.

**Python equivalent:** `np.zeros((r, c))`, `a.copy()`, `a @ b`. Same row-major layout as NumPy's default `order='C'`.

---

## 15. A `Layer` sketch for a neural net

```c
typedef struct {
    Matrix     W;        // out_features x in_features; owned
    Matrix     b;        // out_features x 1; owned
    Activation act;
    Matrix     last_in;  // cached input for backward; owned, may be empty
} Layer;

typedef struct {
    Layer *layers;       // owned array of n_layers
    size_t n_layers;
} MLP;

/* Returns 0 on success. On failure, frees anything it allocated and returns -1. */
int layer_init(Layer *l, size_t in_f, size_t out_f, Activation act) {
    l->W = mat_zeros(out_f, in_f);
    l->b = mat_zeros(out_f, 1);
    l->act = act;
    l->last_in = (Matrix){0};
    if (!l->W.data || !l->b.data) { mat_free(&l->W); mat_free(&l->b); return -1; }
    return 0;
}
void layer_free(Layer *l) { mat_free(&l->W); mat_free(&l->b); mat_free(&l->last_in); }
```

Notice: `Layer` contains `Matrix` structs by value (they are small: 24 bytes each), but the *data* they point to is on the heap. `layer_free` must free each one. `MLP` owns an array of `Layer`s and its `mlp_free` must loop and call `layer_free` on each before freeing the array. Ownership is a tree, and freeing walks it bottom-up.

---

## 16. Flexible array member

C99 lets the *last* field of a struct be an array of unspecified size, so header and payload share one allocation:

```c
typedef struct {
    size_t len;
    double data[];        // flexible array member: contributes 0 to sizeof
} Vector;

Vector *vector_new(size_t n) {
    Vector *v = malloc(sizeof *v + n * sizeof v->data[0]);   // header + payload in ONE block
    if (v) v->len = n;
    return v;
}
// v->data[i] is valid for i < n; free(v) releases everything.
```

Compared to `{size_t len; double *data;}` this saves an allocation and an indirection, and `memcpy`/`fwrite` of the whole object is one call. Limitations: must be the last member, only one per struct, struct cannot be embedded in another struct or array. Used in real code for strings with inline storage, packets, and tensor headers.

---

## Gotchas and undefined behavior

- **Reading an uninitialized struct field** is UB. `Matrix m;` then `m.rows` is garbage. Use `{0}` or designated initializers.
- **Struct assignment / pass-by-value copies pointers, not what they point to.** Two structs, one heap block: double free waiting to happen.
- **`memcmp` on structs** compares padding bytes (indeterminate) and treats `-0.0 != 0.0`. Write a comparison function.
- **`memcpy` of a struct is fine; `fwrite` of a struct includes padding** and is layout-specific (Chapter 08).
- **Reading a union member that was not the last one written** is only defined as byte reinterpretation; if the sizes differ, the extra bytes are indeterminate.
- **`switch` on an enum with a `default:`** silences the missing-case warning. Prefer listing every case.
- **Returning a pointer to a local struct** is UB; returning the struct by value is fine.
- **Flexible array member with `sizeof`**: `sizeof(Vector)` does not include the array. Always add `n * sizeof elem`.
- **Forgetting `struct` in a self-referential typedef**: `Node *next;` inside `typedef struct {...} Node;` does not compile; you need the tag form.
- **`->` on a struct or `.` on a pointer**: compile error, not UB, but the error message ("member reference type is a pointer; did you mean to use '->'?") is clear once you have seen it.

---

## Common mistakes checklist

- [ ] Every struct is initialized (designated, positional, or `{0}`) before any field is read.
- [ ] Structs holding heap pointers are passed by pointer (`const` if read-only), never by value.
- [ ] Any `b = a` on a struct with heap pointers is either a deliberate ownership transfer (with `a.data = NULL` after) or replaced by a deep-copy function.
- [ ] Each struct with heap fields has a `_free` that frees every owned field and is safe to call on an already-freed struct.
- [ ] Fields ordered largest-to-smallest when the struct will exist in large arrays.
- [ ] Tagged unions are only accessed after switching on the tag.
- [ ] Every `switch` on an enum lists every case (no `default`) so the compiler catches omissions.
- [ ] Comparisons use a function with a tolerance for floating-point fields.

---

## You can move on when...

- You can write a `Matrix` struct with `mat_zeros`, `mat_free`, `mat_get`, `mat_set`, `mat_copy` from memory.
- You can explain, with a diagram, why `Matrix b = a; mat_free(&a); mat_free(&b);` crashes.
- You can predict `sizeof` for `struct { char c; double d; int i; }` and reorder it to 16 bytes.
- You can show `1.0f` is `0x3f800000` using a union and explain each bit group.
- You can define a tagged `Value` type with three kinds and write `value_print` and `value_free` for it.
- You can state when to pass by value and when by pointer, and why heap-owning structs are always by pointer.
- You can explain what an opaque type is and why `FILE *` is one.

Next: `../08_file_io/lesson.md` — reading data into your `Matrix` from CSV and binary files, and writing images out.
