# P02 — Tensor Template

**Difficulty:** ★★★☆☆   **Prereq chapters:** C++ 05, 06 (plus 01-04)   **Builds on:** P01

## Goal

A `Tensor<T>` class template of arbitrary rank with shape and strides, views (`reshape`, `transpose`, slicing) that share storage without copying, NumPy broadcasting rules for elementwise operations, and reductions (`sum`, `mean`, `max`) along an axis. This is the `torch.Tensor` data model — the piece under `requires_grad` — and it is what P03's autograd wraps.

## Why

Autograd for a transformer needs tensors of rank 3 (`batch × seq × dim`) and 4 (`batch × heads × seq × seq`), batched matmul over leading dims, and broadcasting (bias `[dim]` added to `[batch, seq, dim]`). A strided view is what makes `transpose` free and `reshape` free-when-contiguous, and understanding strides is the difference between knowing and guessing why PyTorch sometimes demands `.contiguous()`. The templates and operator overloading from C++ 05-06 get their real workout here; the broadcasting-aware gradient rule in P03 (sum over broadcast dimensions) only makes sense after you have written broadcasting forward.

## The math

**Shape and strides.** A tensor with shape $(d_0, \dots, d_{r-1})$ and strides $(s_0, \dots, s_{r-1})$ maps index $(i_0, \dots, i_{r-1})$ to flat offset

$$\text{off} = o + \sum_{k=0}^{r-1} i_k s_k$$

where $o$ is a base offset into a shared buffer. Row-major (C-contiguous) strides: $s_{r-1} = 1$, $s_k = s_{k+1} d_{k+1}$. A tensor is *contiguous* iff its strides equal the row-major strides for its shape.

- `transpose(a, b)`: swap $d_a \leftrightarrow d_b$ and $s_a \leftrightarrow s_b$. No data moves.
- `reshape`: allowed without copy only when contiguous (then recompute row-major strides); otherwise copy to a fresh contiguous buffer first (`contiguous()`).
- Slicing along axis $k$ from $a$ to $b$ with step $t$: $d_k \leftarrow \lceil (b-a)/t \rceil$, $o \leftarrow o + a s_k$, $s_k \leftarrow t s_k$.
- `unsqueeze(k)`: insert $d_k = 1$ with any stride (0 is conventional); `squeeze` removes size-1 dims.

**Broadcasting** (NumPy semantics). Align shapes from the right; two dims are compatible if equal or one of them is 1; the result dim is the max. A size-1 dim broadcasts by giving it **stride 0** — the same element is read for every index. So broadcasting is a view too: `broadcast_to(shape)` returns a tensor with stride 0 on expanded dims and no copy. Elementwise ops then iterate over the result shape and read through both operands' strides.

**Reductions along axis $k$**: output shape drops $d_k$ (or keeps it as 1 with `keepdim`); each output element sums $d_k$ inputs spaced by $s_k$. `mean = sum / d_k`. `argmax` returns indices.

**Matmul** for rank 2: as before. Batched matmul for rank $\ge 3$: broadcast the leading dims, multiply the trailing $[m, k] \times [k, n]$ for each batch index.

**Iteration.** A general N-d iterator that advances a multi-index and the flat offsets of several operands at once (odometer style) is the core utility; every elementwise op and reduction is written in terms of it. Fast path: if all operands are contiguous with the same shape, use a single flat loop.

## Spec

**Interface** (`tensor.hpp`, header-only template):

```cpp
template <typename T>
class Tensor {
public:
    using Shape = std::vector<std::size_t>;
    using Strides = std::vector<std::ptrdiff_t>;

    Tensor();
    explicit Tensor(Shape shape);                               // zero-filled, contiguous
    Tensor(Shape shape, T fill);
    Tensor(Shape shape, std::vector<T> data);                   // takes ownership
    static Tensor zeros(Shape), ones(Shape), arange(std::size_t n), eye(std::size_t n);
    static Tensor randn(Shape, unsigned seed), rand(Shape, unsigned seed);

    std::size_t rank() const;  const Shape& shape() const;  const Strides& strides() const;
    std::size_t numel() const;  bool is_contiguous() const;
    T&       at(std::initializer_list<std::size_t> idx);          // bounds-checked
    const T& at(std::initializer_list<std::size_t> idx) const;
    T*       data();  const T* data() const;                       // base pointer + offset

    // views (share storage)
    Tensor reshape(Shape) const;                 // throws if not contiguous; use contiguous() first
    Tensor view(Shape) const;                    // same as reshape but never copies
    Tensor transpose(int a, int b) const;
    Tensor T() const;                            // rank-2 transpose
    Tensor slice(int axis, std::size_t start, std::size_t stop, std::size_t step = 1) const;
    Tensor squeeze(int axis) const;  Tensor unsqueeze(int axis) const;
    Tensor broadcast_to(const Shape&) const;     // stride-0 view
    Tensor contiguous() const;                   // copy iff needed
    Tensor clone() const;                        // always copies

    // reductions
    Tensor sum(int axis, bool keepdim = false) const;   T sum() const;
    Tensor mean(int axis, bool keepdim = false) const;
    Tensor max(int axis, bool keepdim = false) const;
    Tensor argmax(int axis) const;                      // returns Tensor<T> of indices (or Tensor<std::size_t>)

    template <typename F> Tensor map(F f) const;        // elementwise unary
    Tensor& operator+=(const Tensor&);  Tensor& operator*=(T);
private:
    std::shared_ptr<std::vector<T>> storage_;
    Shape shape_;  Strides strides_;  std::ptrdiff_t offset_;
};

// broadcasting elementwise ops
template <typename T> Tensor<T> operator+(const Tensor<T>&, const Tensor<T>&);
template <typename T> Tensor<T> operator-(const Tensor<T>&, const Tensor<T>&);
template <typename T> Tensor<T> operator*(const Tensor<T>&, const Tensor<T>&);   // elementwise!
template <typename T> Tensor<T> operator/(const Tensor<T>&, const Tensor<T>&);
template <typename T> Tensor<T> operator+(const Tensor<T>&, T);  /* and the other scalar overloads */
template <typename T> Tensor<T> matmul(const Tensor<T>&, const Tensor<T>&);   // rank 2, and batched
template <typename T> Tensor<T> exp(const Tensor<T>&), log(...), tanh(...), relu(...), sqrt(...), pow(const Tensor<T>&, T);
template <typename T> bool allclose(const Tensor<T>&, const Tensor<T>&, T atol = 1e-9, T rtol = 1e-9);
template <typename T> std::ostream& operator<<(std::ostream&, const Tensor<T>&);  // NumPy-style nested brackets

std::vector<std::size_t> broadcast_shapes(const std::vector<std::size_t>& a, const std::vector<std::size_t>& b);  // throws if incompatible

// I/O: .npy read/write for T = float, double (so NumPy can check everything)
template <typename T> Tensor<T> load_npy(const std::string& path);
template <typename T> void      save_npy(const Tensor<T>&, const std::string& path);
```

Decision to make and document: `operator*` is **elementwise** (NumPy/PyTorch semantics), `matmul` is a function. This differs from P01's `Matrix` — say why in the header comment.

**.npy format** (v1.0): magic `\x93NUMPY`, major 1, minor 0, `uint16` header length, ASCII header dict `{'descr': '<f8', 'fortran_order': False, 'shape': (3, 4), }` padded with spaces to a multiple of 64 bytes and ending in `\n`, then raw data. Write it by hand; parse the shape with a small string scan.

**Tests** (doctest): views share memory (mutating a view mutates the base), transpose strides, reshape of non-contiguous throws, slice with step, broadcasting `[3,1] + [1,4] -> [3,4]`, `[2,3,4] + [4]`, incompatible `[3] + [4]` throws, `sum(axis)` on rank 3 for each axis, batched matmul `[2,3,4] @ [2,4,5] -> [2,3,5]`, `.npy` round trip.

## Milestones

1. **M1 — storage, shape, strides, `at`.** `arange(24).reshape({2,3,4}).at({1,2,3}) == 23`. Printing matches NumPy's layout.
2. **M2 — views.** `t.transpose(0,2)` has strides `(1, 4, 12)` for the shape above and `is_contiguous()` is false; writing through the view changes the base; `.contiguous()` fixes strides and copies. `slice(1, 0, 3, 2)` gives shape `(2, 2, 4)`.
3. **M3 — N-d iterator + elementwise ops without broadcasting.** `a + b` on same shapes, including non-contiguous operands (a transposed tensor plus a contiguous one). Fast path measured 5-10x faster than the generic path for contiguous inputs.
4. **M4 — broadcasting.** All the shape cases in the test list match NumPy; `broadcast_to` is stride 0 and `numel()` reports the broadcast size. `randn({4,1}) * randn({1,5})` equals the outer product.
5. **M5 — reductions and matmul.** `sum` along each axis of a rank-3 tensor matches `np.sum(x, axis=k)`; batched matmul matches `np.matmul`. `softmax(x, axis=-1)` written as `exp(x - x.max(-1, true)) / sum(...)` in 2 lines works on rank 3.
6. **M6 — `.npy` I/O + NumPy test harness.** A Python script generates random tensors and expected results into `.npy` files; a C++ test loads inputs, computes, and compares with `allclose` against the expected outputs. 30+ cases, all pass.

## Verification

```python
import numpy as np
rng = np.random.default_rng(0)
a = rng.standard_normal((2, 3, 1)); b = rng.standard_normal((4,))
np.save("a.npy", a); np.save("b.npy", b); np.save("a_plus_b.npy", a + b)          # [2,3,4]
np.save("a_sum1.npy", a.sum(axis=1)); np.save("bmm.npy", np.matmul(rng.standard_normal((2,3,4)), rng.standard_normal((2,4,5))))
# after your C++ test writes out.npy:
print(np.abs(np.load("out.npy") - (a + b)).max())     # < 1e-15
x = np.arange(24).reshape(2, 3, 4); print(x.transpose(2, 1, 0).strides)   # (8, 32, 96) bytes -> (1, 4, 12) elements
```

## Stretch goals

- `Tensor<T>::einsum`-lite: a `contract(a, b, axis_a, axis_b)` that generalizes matmul; write attention scores with it in P05.
- SIMD-friendly fast path: ensure the contiguous loop auto-vectorizes (check `-O2 -Rpass=loop-vectorize` on clang) and measure GFLOP/s.
- `Tensor<std::complex<double>>` compiles and `matmul` works — needed if you later want an FFT-based PIC Poisson solver on tensors.
- Copy-on-write: make `operator+=` on a tensor that shares storage with a view copy first (PyTorch does not do this — decide and document).

## Hints

- Storage as `std::shared_ptr<std::vector<T>>` gives you views for free: a view is `{same storage, new shape, new strides, new offset}`. Owning semantics are the shared_ptr's problem.
- Write `broadcast_shapes` first and test it alone; it's pure logic and easy to get subtly wrong (align from the right, pad with 1s on the left).
- The N-d iterator: keep a multi-index `idx[r]`, and for each operand a running offset; to advance, increment `idx[r-1]`, add that operand's stride; on overflow, reset and carry to the next dim (subtract `d_k * s_k`, add `s_{k-1}`). This is the entire engine; write it once as a helper that takes a lambda.
- Broadcasting an operand is just `broadcast_to(out_shape)` then the same iterator — stride-0 dims make it work with no special cases.
- Reductions: `sum(axis)` = iterate over the output shape, inner loop of `d_k` steps with stride `s_k`. For `keepdim=false`, erase the axis from shape/strides afterwards.
- `operator<<`: recursive over rank, `[` at each level, elements separated by `, `, rows on new lines — mimic `repr(np.array)` closely enough that you can eyeball-diff.
- Templates: everything in the header; explicitly instantiate `Tensor<float>` and `Tensor<double>` in a test to catch compile errors early.

## Where to put it

`cpp/tensor/` — `include/tensor.hpp`, `include/npy.hpp`, `tests/test_tensor.cpp`, `tests/gen_cases.py`, `CMakeLists.txt`.
