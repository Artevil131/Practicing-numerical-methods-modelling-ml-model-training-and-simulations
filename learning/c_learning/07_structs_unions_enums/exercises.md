# Chapter 07 — Exercises

Write each exercise as `ex07_K.c` in this folder. Compile with:

```
cc -Wall -Wextra -std=c11 -O2 -o ex07_1 ex07_1.c -lm
```

and run every exercise that allocates under `-g -fsanitize=address,undefined` as well.

---

## 07.1 — **Point struct three ways**

Define `typedef struct { double x, y; } Point;`. In `main`, create three points: one with positional initialization, one with designated initializers (only `.y` set), one with `{0}`. Print all three as `(x, y)`. Then write `double point_dist(Point a, Point b)` (by value) and print the distance between the first two.

Example output:
```
(3.0, 4.0) (0.0, 7.0) (0.0, 0.0)
dist = 4.243
```

<details><summary>Hint</summary>

- `sqrt` from `<math.h>`; link with `-lm`.
- Designated: `Point b = {.y = 7.0};` leaves `x` as `0.0`.
</details>

---

## 07.2 — **Padding detective**

Define three structs: `A { char c; int i; char d; }`, `B { char c; char d; int i; }`, `C { char c; double x; int i; short s; }`. Print `sizeof` and `offsetof` for every field of each (`<stddef.h>`). Then define `C2` — the same fields as `C` reordered to minimize size — and print its `sizeof`. Add a comment drawing the byte layout of `C` and `C2`.

Example (arm64):
```
A: size=12  c@0 i@4 d@8
B: size=8   c@0 d@1 i@4
C: size=24  c@0 x@8 i@16 s@20
C2: size=16
```

<details><summary>Hint</summary>

- Largest alignment first, then descending.
- `sizeof` must be a multiple of the largest member alignment.
</details>

---

## 07.3 — **Value vs pointer, observed**

Define `typedef struct { int counts[100]; } Big;` (400 bytes). Write `void bump_val(Big b)` that increments `b.counts[0]` and `void bump_ptr(Big *b)` that increments `b->counts[0]`. Call each once on the same variable and print `counts[0]` after each call. Then call `bump_val` 10,000,000 times and `bump_ptr` 10,000,000 times, timing both with `clock()`. Print the two times.

Example:
```
after bump_val: 0
after bump_ptr: 1
val: 0.xxx s   ptr: 0.0xx s
```

<details><summary>Hint</summary>

- The compiler may optimize away the by-value copy at `-O2` if it can prove it is unused; make the function do something observable (e.g. return `b.counts[0]` and accumulate the returns into a `volatile` sum).
- `<time.h>`, `CLOCKS_PER_SEC`.
</details>

---

## 07.4 — **Float bits with a union**

Using `union { float f; uint32_t u; }`, write functions `int sign_bit(float)`, `int exponent_bits(float)` (raw 8-bit field), `uint32_t mantissa_bits(float)`. For each of `1.0f, -2.5f, 0.1f, 0.0f, INFINITY, NAN` print the hex bits and the three fields. Then reconstruct the value as `(-1)^s * 2^(e-127) * (1 + m/2^23)` for the normal numbers and print it to show it matches.

Example:
```
1.000000  0x3f800000  s=0 e=127 m=0        -> 1.000000
-2.500000 0xc0200000  s=1 e=128 m=2097152  -> -2.500000
```

<details><summary>Hint</summary>

- Mask and shift: sign `>> 31`, exponent `(u >> 23) & 0xff`, mantissa `u & 0x7fffff`.
- `INFINITY` and `NAN` come from `<math.h>`; use `ldexp` or `pow` for `2^(e-127)`.
</details>

---

## 07.5 — **Activation enum with names table**

Define `typedef enum { ACT_RELU, ACT_TANH, ACT_SIGMOID, ACT_COUNT } Activation;` and a parallel `const char *act_names[ACT_COUNT]`. Write `double activate(Activation, double)` using a `switch` that lists every case (no `default`), and `Activation act_from_name(const char *)` that returns `ACT_COUNT` when the name is unknown. `main` takes an activation name and a number from `argv`, and prints the result or an error listing valid names.

Example: `./ex07_5 tanh 0.5` → `tanh(0.5) = 0.462117`
`./ex07_5 gelu 0.5` → `unknown activation 'gelu'; valid: relu tanh sigmoid`

<details><summary>Hint</summary>

- Loop `for (int i = 0; i < ACT_COUNT; i++) if (strcmp(...) == 0)`.
- Deliberately remove one `case` and observe the `-Wall` warning (`-Wswitch`), then put it back.
</details>

---

## 07.6 — **Shallow copy trap, then fixed**

Define the `Matrix` struct (`rows, cols, data`) with `mat_zeros`, `mat_free`, and `mat_copy` (deep copy). In `main`: (1) `Matrix a = mat_zeros(2,2); Matrix b = a;` set `b.data[0] = 7` and print `a.data[0]` to show they share memory; (2) do NOT free both (explain in a comment why that would double-free) — instead free `a` and set `b.data = NULL`; (3) then do `Matrix c = mat_zeros(2,2); Matrix d = mat_copy(&c);` modify `d`, show `c` unchanged, and free both correctly. Run under ASan.

Example:
```
shallow: b.data[0]=7 a.data[0]=7 (same block)
deep:    d.data[0]=9 c.data[0]=0 (independent)
```

<details><summary>Hint</summary>

- `mat_copy` = `mat_zeros` + `memcpy`.
- Draw the two-arrows-one-block diagram in a comment.
</details>

---

## 07.7 — **Tagged union `Value`**

Implement `Value` with kinds `VAL_NUM`, `VAL_STR`, `VAL_LIST` where a list holds a heap array of `Value` and a count. Write `value_print` (recursive, prints `[1, "two", [3, 4]]` style), `value_free` (recursive), and constructors `value_num`, `value_str` (copies the string), `value_list(Value *items, size_t n)` (takes ownership of the items array). Build the nested example above, print it, free it, verify with ASan.

Example: `[1, "two", [3, 4]]`

<details><summary>Hint</summary>

- `value_free` for a list frees each element (recursively) then the array.
- Keep the recursion straightforward: the tag decides everything.
</details>

---

## 07.8 — **Matrix ops for linear regression** *(ML)*

Using the `Matrix` type, implement `mat_transpose`, `mat_matmul` (i-k-j order), `mat_print`, and `mat_from_array(rows, cols, const double *src)`. Build `X` (4x2) and `y` (4x1) from literal arrays, compute `XtX = X^T X` and `Xty = X^T y`, and print both. (Solving the 2x2 system for the weights is exercise 08.9 / the numerics chapters; here you build the normal-equation ingredients.) Free everything.

Example with `X = [[1,1],[1,2],[1,3],[1,4]]`, `y = [2,4,6,8]`:
```
XtX =
   4  10
  10  30
Xty =
  20
  60
```

<details><summary>Hint</summary>

- Every op that returns a `Matrix` returns ownership — say so in its comment.
- Check shapes and return an empty matrix (`data == NULL`) on mismatch.
</details>

---

## 07.9 — **Particle array, AoS vs SoA** *(sim)*

Define `Particle { double x, y, vx, vy, m; }` and a `Particles` SoA struct `{ double *x, *y, *vx, *vy, *m; size_t n; }`. Fill `N = 2,000,000` particles identically in both layouts (deterministic pseudo-random). Time a loop that updates only positions (`x += vx*dt; y += vy*dt`) 10 times in each layout. Print both times and the checksum `sum(x)` for each to prove they agree.

Example: `AoS: 0.0xx s  SoA: 0.0xx s  checksum both = 1.234567e+06`

<details><summary>Hint</summary>

- SoA touches only 4 of 5 arrays; AoS pulls all 40 bytes of each particle through cache to update 16 of them.
- Use `-O2`; results vary, SoA is typically faster for this access pattern.
</details>

---

## 07.10 — **Autograd node sketch** *(ML)*

Define `typedef enum { OP_LEAF, OP_ADD, OP_MUL, OP_TANH } Op;` and `typedef struct Node { Op op; struct Node *a, *b; double val, grad; } Node;`. Write constructors `leaf(double)`, `add(Node*, Node*)`, `mul(Node*, Node*)`, `tanh_(Node*)` that heap-allocate a node and compute `val` immediately (forward pass). Write `void backward(Node *n)` that, given `n->grad` already set, pushes gradients to `a` and `b` according to `op` (switch on the enum) and recurses. For `L = tanh(a*b + c)` with `a=0.5, b=-1.0, c=2.0`, set `L->grad = 1` and print `dL/da, dL/db, dL/dc`. Free all nodes. (This naive recursive backward double-counts if a node is reused — note this in a comment; the autograd project fixes it with a topological sort.)

Example:
```
L = 0.905148
dL/da = -0.180707  dL/db = 0.090354  dL/dc = 0.180707
```

<details><summary>Hint</summary>

- `d tanh(x)/dx = 1 - tanh(x)^2`; for `mul`, `a->grad += n->grad * b->val`.
- Keep a small array of every allocated node so freeing is one loop.
</details>
