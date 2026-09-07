# Chapter 16 — Object lifetime, allocators and memory

This chapter requires `-std=c++20` (`std::construct_at`, `std::bit_cast`, `std::span`, `[[no_unique_address]]`). Compile everything here with
`c++ -Wall -Wextra -std=c++20 -O2`. Assumes Chapters 02 (RAII), 05 (templates), 07 (move semantics, smart pointers), 12 (performance) and 13 (idioms) of this course, and the C course's Chapters 05–06 (pointers, `malloc`).

## What you'll be able to do after this chapter

- State precisely when a C++ object's lifetime begins and ends, why that differs from storage duration, and which C-style tricks (`malloc` + use, `memcpy` into a buffer, `*(uint32_t*)&f`) are legal in C++20 and which are UB.
- Construct objects in raw memory (`placement new`, `std::construct_at`), destroy them (`p->~T()`, `std::destroy_at`), use `std::uninitialized_*` algorithms, and keep the strong exception guarantee while doing it.
- Write an allocator that `std::vector` accepts (arena-backed), explain what `std::allocator_traits` does for you, and decide when `std::pmr` is the better tool.
- Implement a `SmallVector<T, N>` with correct move/copy semantics and no UB, and read a class layout (`vptr`, padding, EBO, `[[no_unique_address]]`) from `sizeof`/`offsetof`.
- Find hidden copies in numerics code with a counting `operator new` or a counting allocator, and remove them.
- Design the ownership model of a tensor library — refcounted `Storage`, `TensorImpl` with shape/strides/offset, zero-copy views — and wrap C resources (`FILE*`, `mmap`, device pointers, fds) so they can't leak.

## Why this matters for ML / numerics / sims

A training step in an autograd engine allocates one node per op — thousands per step, millions per epoch. A Barnes–Hut step builds and tears down ~N cells. A tokenizer creates millions of short strings. In all three the *allocator*, not the arithmetic, is often the bottleneck: `malloc`/`free` cost 20–100 ns each and scatter your nodes across the heap, so the traversal that follows misses cache. The example for this chapter measures a bump arena at **15×** over `std::vector`'s per-object `malloc` for small temporaries, and a node pool at **2.4×** for building/traversing/freeing a quadtree of 87 k cells — with the same algorithm.

The second half of the chapter is about *not lying to the compiler*. Strict aliasing, object lifetime and alignment rules are where "it worked at `-O0`" programs break at `-O2`. PyTorch's `Storage`/`TensorImpl` split, NumPy's strided views, Eigen's `Map` — every real numerics library is built on the rules in this chapter.

---

## 1. Storage duration vs lifetime

Two different things ([basic.stc], [basic.life]):

| Concept | What it is about | Controlled by |
|---|---|---|
| **Storage duration** | How long the *bytes* exist: automatic (stack), static, thread, dynamic | Where you declare it / `new`–`delete` / `malloc`–`free` |
| **Lifetime** | How long an *object* exists in those bytes | Constructor start → destructor start |

For a type with a non-trivial constructor, the object's lifetime **begins when initialization is complete** and **ends when the destructor call starts** ([basic.life]/1). Between "storage acquired" and "constructor finished" there is storage but no object; touching it as if it were one is UB.

```cpp
alignas(Tracked) std::byte storage[sizeof(Tracked)];  // storage exists; NO Tracked lives here
Tracked* t = ::new (static_cast<void*>(storage)) Tracked{7};  // lifetime begins (placement new)
t->~Tracked();                                        // lifetime ends; storage remains valid
// storage may now be reused: ::new (storage) Tracked{8};
```

**Placement new** `::new (ptr) T(args)` calls no allocation function — it only runs the constructor at `ptr`. Always write the leading `::` and cast to `void*` so a class-scope `operator new` can't hijack it. The C++20 spelling is `std::construct_at(ptr, args...)` (`<memory>`), which is `constexpr`-usable and always value-initializes correctly; `std::destroy_at(ptr)` calls the destructor (and for arrays, destroys every element).

Outside a library implementation you rarely call these directly — but every container, `std::optional`, `std::variant`, and small-buffer type is built from them, and you'll write one in §5.

Python equivalent: none. Python objects are always heap-allocated and their lifetime is the refcount's. C++ separates the two so a `std::vector<T>` can own bytes for 1000 `T`s while only 10 of them exist.

---

## 2. Implicit-lifetime types, `std::launder`, `std::start_lifetime_as`

C code does `float* f = malloc(n * sizeof *f); f[0] = 1.0f;`. In C++17 that was technically UB (no `float` object was ever created). C++20 (P0593, [intro.object]/13) fixed it: **implicit-lifetime types** — scalars, arrays, trivially-copyable aggregates with a trivial constructor and destructor, and arrays of them — are *implicitly created* by `malloc`, `operator new`, `memcpy`/`memmove`, and by starting the lifetime of a `char`/`unsigned char`/`std::byte` array. The pointer you get back is deemed to point to the created object.

```cpp
struct Vec2 { float x, y; };                      // implicit-lifetime
void* raw = std::malloc(4 * sizeof(Vec2));
Vec2* v = static_cast<Vec2*>(raw);                // OK in C++20: Vec2[4] implicitly exists
v[0] = {1.f, 2.f};
```

C++23 adds `std::is_implicit_lifetime_v<T>` (libc++ 21: present under `-std=c++23`) and `std::start_lifetime_as<T>(void*)` for the case where the bytes already hold a valid representation (a network packet, an mmap'd file) and you want a `T*` without `memcpy`. **Apple clang 21's libc++ does not implement `std::start_lifetime_as`** (checked: "no member named 'start_lifetime_as'" under `-std=c++23`). Until it lands, `std::memcpy` into a real `T` is the portable, optimizer-friendly answer (the compiler emits no copy for small `T`).

**`std::launder`** ([ptr.launder]) is for a narrower situation: storage is reused for a *new* object and you still hold a pointer/reference to the *old* one. The old pointer is automatically usable only if the new object is *transparently replaceable* — same type, not a complete `const` object, no `const` or reference members (this last restriction was relaxed in C++20 by P0532, but keep the rule of thumb). When it isn't, `std::launder(p)` produces a pointer the compiler agrees points to the new object.

```cpp
struct WithConst { const int k; };
auto* w1 = ::new (buf) WithConst{1};
w1->~WithConst();
::new (buf) WithConst{2};
int a = w1->k;                 // UB in principle: compiler may fold k == 1
int b = std::launder(w1)->k;   // 2, well-defined
```

The other place you need it: reading through a `reinterpret_cast<T*>(bytes)` into an inline buffer after `construct_at` (§5 does this in `inline_ptr()`).

---

## 3. Alignment: `alignas`, `alignof`, `std::aligned_alloc`, over-aligned `new`

Every type has an alignment requirement (`alignof(T)`, a power of two). `double` is 8, `std::max_align_t` is 16 on arm64 macOS, and NEON/AVX loads want 16/32; cache lines are 128 bytes on Apple Silicon (`sysctl hw.cachelinesize`), 64 on x86.

| Tool | Guarantees | Notes |
|---|---|---|
| `alignas(N)` on a type or variable | That object is N-aligned | `struct alignas(64) CacheLine {...}` |
| `new T` for `alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__` (16 here) | Default alignment | Plain `operator new(size_t)` |
| `new T` for over-aligned `T` (C++17) | `alignof(T)` | Calls `operator new(size_t, std::align_val_t)`; delete calls the matching `operator delete` |
| `std::aligned_alloc(align, size)` (C11/C++17) | `align` | `size` **must be a multiple of `align`**; free with `std::free` |
| `posix_memalign(&p, align, size)` | `align` | POSIX; what libc++ uses underneath on macOS |
| `std::assume_aligned<64>(p)` (C++20) | Tells the optimizer | UB if the promise is false |

```cpp
struct alignas(64) CacheLine { double d[8]; };
auto* cl = new CacheLine{};        // operator new(64, align_val_t{64}) → address % 64 == 0
delete cl;                         // operator delete(void*, align_val_t{64})
void* p = std::aligned_alloc(64, 64 * 16);  std::free(p);
```

A `std::byte buf[sizeof(T)]` is only 1-aligned. **Always** write `alignas(T) std::byte buf[sizeof(T)]` when it will hold a `T`; a misaligned `double` store is UB (and on arm64 a `LDR q` of misaligned data faults or is slow).

---

## 4. Class layout: padding, vptr, EBO, `[[no_unique_address]]`

Members are laid out in declaration order (within an access section), each at an offset that satisfies its alignment; the struct's size is rounded up to its alignment.

```
struct Padded    { char c; double d; char e; };   // 24 bytes
   0: c  1..7: pad  8..15: d  16: e  17..23: pad
struct Reordered { double d; char c; char e; };   //  16 bytes
   0..7: d  8: c  9: e  10..15: pad
```

Rule: **sort members by decreasing alignment** and you minimize padding (or use `-Wpadded` to see it). For a particle SoA vs AoS decision, see `../12_performance/lesson.md`.

- **vptr**: any class with a virtual function carries a hidden pointer (8 bytes) to its vtable, at offset 0 on Itanium ABI. `struct Poly { virtual ~Poly(); int x; }` is 16 bytes, not 4. A vector of 1 M polymorphic `Node`s pays 8 MB for pointers you may never use — one reason Chapter 13 preferred `std::variant` for autograd ops.
- **Empty base optimization (EBO)**: an empty *base* class occupies no space (`struct D : Empty { int x; }` → 4), but an empty *member* costs at least 1 byte + padding (`struct M { Empty e; int x; }` → 8). Allocators and comparators are traditionally stored as bases for this reason.
- **`[[no_unique_address]]`** (C++20): lets an empty member share an address: `struct N { [[no_unique_address]] Empty e; int x; }` → 4. `std::vector<T, Alloc>` uses it (or EBO) so a stateless allocator adds zero bytes.

Check with `static_assert(sizeof(X) == 16)` and `offsetof(X, m)` (standard-layout types only). `sizeof(std::vector<int>)` is 24 on libc++ (begin, end, cap), `sizeof(std::string)` 24 with a 22-char SSO buffer, `sizeof(std::shared_ptr<T>)` 16, `sizeof(std::unique_ptr<T>)` 8 (stateless deleter) — knowing these tells you what a copy costs.

---

## 5. The Allocator concept and `std::allocator_traits`

A C++ allocator is a class template that supplies **`value_type`, `allocate(n)`, `deallocate(p, n)`, equality, and copy-construction from `Alloc<U>`** ([allocator.requirements]). Everything else — `pointer`, `size_type`, `rebind`, `construct`, `destroy`, `max_size`, the `propagate_on_container_*` traits — has defaults filled in by `std::allocator_traits<Alloc>`, which is what containers actually call.

```cpp
template <class T>
class ArenaAllocator {
    Arena* arena_;
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    explicit ArenaAllocator(Arena& a) noexcept : arena_(&a) {}
    template <class U> ArenaAllocator(const ArenaAllocator<U>& o) noexcept : arena_(o.arena()) {}
    T* allocate(std::size_t n) { return static_cast<T*>(arena_->allocate(n * sizeof(T), alignof(T))); }
    void deallocate(T* p, std::size_t n) noexcept { arena_->deallocate(p, n * sizeof(T)); }
    Arena* arena() const noexcept { return arena_; }
    template <class U> bool operator==(const ArenaAllocator<U>& o) const noexcept { return arena_ == o.arena(); }
    template <class U> bool operator!=(const ArenaAllocator<U>& o) const noexcept { return !(*this == o); }
};
template <class T> using arena_vector = std::vector<T, ArenaAllocator<T>>;
```

The **rebind constructor** (`ArenaAllocator<U>` → `ArenaAllocator<T>`) is mandatory: `std::map<K, V, Cmp, Alloc<pair<...>>>` internally needs `Alloc<Node>`; `allocator_traits::rebind_alloc<Node>` uses it.

The **`propagate_on_container_*`** traits answer: when a container is copy-assigned / move-assigned / swapped, does the allocator come along? For stateful allocators, the safe defaults (`false`, `true` for move, `false` for swap) mean `a = std::move(b)` with unequal allocators must element-wise move rather than steal the buffer. `select_on_container_copy_construction` decides what a *copy* gets (default: same allocator).

**Equality semantics**: `a1 == a2` must mean "memory from `a1` may be deallocated by `a2`". For an arena allocator that's "same arena".

The `Arena` behind it is a **bump allocator**: one big block, a cursor, `allocate` = align the cursor and advance, `deallocate` = no-op, `reset()` = cursor back to start (which ends the lifetime of everything in it — every pointer into the arena is dangling after `reset()`). Measured in the example, `-O2`, M-series: 20 000 temporaries of 16 doubles — `std::vector` **0.28 ms**, `arena_vector` **0.018 ms** (15×). The whole cost of `std::vector` here is `malloc`/`free`.

---

## 6. `std::pmr` — polymorphic memory resources

`ArenaAllocator<T>` changes the container's **type**: `arena_vector<double>` and `std::vector<double>` cannot be passed to the same function. C++17's `<memory_resource>` fixes this by moving the choice to runtime:

- `std::pmr::memory_resource`: abstract base with virtual `do_allocate(bytes, align)`, `do_deallocate`, `do_is_equal`.
- `std::pmr::polymorphic_allocator<T>`: an allocator holding a `memory_resource*`.
- `std::pmr::vector<T>` = `std::vector<T, polymorphic_allocator<T>>`; likewise `pmr::string`, `pmr::unordered_map`, ….

| Resource | Behavior | Use when |
|---|---|---|
| `new_delete_resource()` | Forwards to `new`/`delete` | Default |
| `null_memory_resource()` | Always throws `bad_alloc` | As upstream of a fixed buffer, to *prove* nothing spills |
| `monotonic_buffer_resource(buf, n, upstream)` | Bump allocator; never frees until destroyed; grows via upstream | Per-step / per-request scratch (autograd tape, one frame of a sim) |
| `unsynchronized_pool_resource` | Size-class buckets, reuses freed blocks, no mutex | Node churn on one thread (trees, graphs, tokens) |
| `synchronized_pool_resource` | Same with a mutex | Multithreaded churn |

```cpp
alignas(std::max_align_t) std::byte stack_buf[4096];
std::pmr::monotonic_buffer_resource mono(stack_buf, sizeof stack_buf);   // upstream: new_delete
std::pmr::vector<double> v(&mono);          // lives in stack_buf until it outgrows it
std::pmr::vector<std::pmr::string> names(&pool);  // uses-allocator construction: elements share `pool`
```

That last line is the killer feature: **the allocator propagates into elements** (`std::uses_allocator`), so a `pmr::vector<pmr::string>` puts every string's characters in the same resource. With a custom `Alloc<T>` you'd have to spell `std::vector<std::basic_string<char, traits, Alloc<char>>, Alloc<...>>`.

Measured (same 20 000×16-double benchmark): `std::vector` **0.28 ms**, `pmr::vector` on a monotonic buffer **0.083 ms** (3.3×), on an `unsynchronized_pool_resource` **0.10 ms** (2.8×). Slower than the hand-rolled arena (0.018 ms) — the virtual `do_allocate` call, the `polymorphic_allocator` pointer stored in every container, and the fact that libc++'s monotonic resource does more bookkeeping — but it is standard, composable, and type-stable.

**When pmr beats a custom allocator**: whenever the container type must stay `std::vector<T>` (public APIs, interop), when elements themselves allocate (strings, nested vectors), when you want to switch strategies at runtime, or when you need per-thread/per-request arenas without templating everything. **When a custom allocator wins**: the last 5× in a hot loop, `constexpr` contexts, or when the extra pointer per container matters (millions of tiny containers).

Python equivalent: none exposed, but CPython's `pymalloc` is a pool resource for small objects, and PyTorch's `CUDACachingAllocator` is a pool with size classes — same design.

---

## 7. Small-buffer optimization: `SmallVector<T, N>`

`std::string` keeps ≤22 chars inline; LLVM's `SmallVector<T, N>` and Eigen's fixed-size types keep small arrays inline; a tensor's `shape`/`strides` (≤4–6 dims) should never touch the heap. The idea: a raw buffer for `N` elements inside the object, a pointer that points either at it or at a heap block.

```cpp
template <class T, std::size_t N>
class SmallVector {
    alignas(T) std::byte inline_[N * sizeof(T)];   // storage; no T lives here until constructed
    T* data_;  std::size_t size_ = 0, cap_ = N;
    T* inline_ptr() noexcept { return std::launder(reinterpret_cast<T*>(inline_)); }
    bool is_inline() const noexcept { return data_ == inline_ptr(); }
    void grow(std::size_t new_cap) {
        T* fresh = allocate(new_cap);
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T>) std::uninitialized_move_n(data_, size_, fresh);
            else                                                   std::uninitialized_copy_n(data_, size_, fresh);
        } catch (...) { deallocate(fresh); throw; }
        std::destroy_n(data_, size_);
        if (!is_inline()) deallocate(data_);
        data_ = fresh; cap_ = new_cap;
    }
public:
    template <class... A> T& emplace_back(A&&... a) {
        if (size_ == cap_) grow(cap_ * 2);
        return *std::construct_at(data_ + size_++, std::forward<A>(a)...);
    }
    // ... see example.cpp for the full Rule of Five
};
```

The parts that are easy to get wrong:

1. **Move constructor cannot just steal a pointer** when the source is inline — the buffer is *inside* the source object. It must move element-wise (`uninitialized_move_n`) then destroy the source's elements. When the source is on the heap, steal the pointer and reset the source to its inline buffer.
2. **The strong guarantee on growth**: move if `is_nothrow_move_constructible_v<T>`, else copy — exactly `std::vector`'s rule (`std::move_if_noexcept`). This is why a hand-written move constructor **must be `noexcept`** (Chapter 07): otherwise every reallocation copies.
3. **`swap` of two inline buffers is O(N)**, not a pointer exchange. Copy-and-swap assignment still works.
4. **Never `memcpy` non-trivially-copyable `T`s** between buffers, even if "it works": `std::string` with SSO has a self-pointer, and `memcpy` breaks it.

The example asserts: 4 elements inline with zero heap allocations; the 5th spills to a heap block of capacity 8; move steals, copy deep-copies, assignment swaps, and every `Tracked` constructed is destroyed.

---

## 8. `std::optional` / `std::variant` storage internals

Both are small-buffer types with exactly one slot:

```
std::optional<double>  (16 bytes on libc++):   union { char dummy; double val; }   bool engaged;   [7 pad]
std::variant<A, B, C>  :                       union { A a; B b; C c; }   index_type index;  (+ padding)
```

- `optional<T>` stores `T` **in place** — no allocation, no pointer. `sizeof(optional<double>) == 16`, `sizeof(optional<char>) == 2`. Constructing the value is `construct_at` into the union member; `reset()` is `destroy_at`. That's why `optional<T&>` doesn't exist (C++26 adds it) and why `std::optional<std::vector<double>>` costs 24 + 8 bytes, not a heap block.
- `variant<Ts...>` stores the **largest** alternative's size, aligned to the strictest; `index()` says which is alive. Assignment that changes the alternative destroys the old one, then constructs the new one; if that constructor throws, the variant becomes `valueless_by_exception()`.
- Both are `constexpr`-friendly because C++20 allows `construct_at` in constant evaluation.

Corollary for your autograd `Op = variant<Leaf, Add, Mul, MatMul...>`: `sizeof(Op)` is `max(sizeof(alternatives)) + index`, so a huge alternative (one holding a `Matrix`) inflates every node. Keep alternatives small: indices and pointers, not data.

---

## 9. Uninitialized-memory algorithms and exception safety

`<memory>` provides the loops you'd otherwise write by hand around `construct_at`:

| Algorithm | Does | Cleans up on throw |
|---|---|---|
| `std::uninitialized_default_construct(_n)` | `::new (p) T` (indeterminate for trivial `T`) | yes |
| `std::uninitialized_value_construct(_n)` | `::new (p) T()` (zeroed for trivial `T`) | yes |
| `std::uninitialized_fill(_n)` | copies one value | yes |
| `std::uninitialized_copy(_n)` / `_move(_n)` | copy / move a range into raw memory | yes (move: source left in valid-but-unspecified state) |
| `std::destroy(_at/_n)` | calls destructors | n/a |

"Cleans up on throw" means: if the *k*-th constructor throws, elements 0..*k*−1 are destroyed before the exception propagates — no leaks, no half-built objects. Measured in the example with a `Throwy` type whose 4th copy throws: `constructed == destroyed == 3`.

If you write the loop yourself, you must reproduce that:

```cpp
std::size_t built = 0;
try {
    for (; built < n; ++built) std::construct_at(dst + built, src[built]);
} catch (...) {
    std::destroy_n(dst, built);   // unwind exactly what was built
    throw;                        // then re-throw; the caller frees dst
}
```

The exception-safety levels (Chapter 10): a container operation should give the **basic** guarantee always (no leaks, invariants hold), the **strong** guarantee where cheap (`push_back` — if it throws, the vector is unchanged), and `noexcept` for moves and swaps. `grow()` in §7 achieves strong by building the new buffer fully before touching the old one.

For trivially-copyable `T`, all of these compile down to `memcpy`/`memset` — the algorithms are free abstractions.

---

## 10. Strict aliasing: `std::bit_cast`, `memcpy`, `std::byte`

The optimizer assumes an `int*` and a `float*` never point to the same bytes ([basic.lval]/11): a store through one can't change what's read through the other, so it can reorder and cache. Violating that assumption is UB and *does* miscompile at `-O2`:

```cpp
float f = 1.0f;
std::uint32_t bad = *reinterpret_cast<std::uint32_t*>(&f);   // UB: aliasing float as uint32_t
```

Legal ways to look at the bits:

| Method | Standard | Notes |
|---|---|---|
| `std::memcpy(&u, &f, sizeof u)` | C and C++ | The classic; compilers emit a register move, no call |
| `std::bit_cast<std::uint32_t>(f)` (C++20, `<bit>`) | C++20 | `constexpr`, requires both types trivially copyable and same size |
| `unsigned char*` / `char*` / `std::byte*` | C++ | The *only* pointer types allowed to alias anything |
| `union { float f; uint32_t u; }` | **C only** (C11 6.5.2.3 fn 95) | UB in C++ (only one union member is active); compilers accept it, standard doesn't |

`std::byte` (C++17, `<cstddef>`) is an enum class: it is not an integer, you can't do arithmetic on it, only `|`, `&`, `^`, `~`, `<<`, `>>` and `std::to_integer<int>(b)`. Use it for "just bytes" buffers so that nobody mistakes them for text.

The fast inverse square root, legally, is three `bit_cast`s (example §8). `-fstrict-aliasing` is on by default at `-O2`; `-fno-strict-aliasing` makes the UB "work" at a performance cost and is not a fix.

---

## 11. Object representation and `std::span<std::byte>` for serialization

`std::as_bytes(span<T>)` → `span<const std::byte>`, `std::as_writable_bytes(span<T>)` → `span<std::byte>` (C++20). With trivially-copyable types this gives a checkpoint format in a few lines:

```cpp
struct Header { std::uint32_t magic, rows, cols, dtype; };   // 16 bytes, trivially copyable
template <class T> void write_pod(std::vector<std::byte>& out, const T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto b = std::as_bytes(std::span<const T, 1>(&v, 1));
    out.insert(out.end(), b.begin(), b.end());
}
template <class T> T read_pod(std::span<const std::byte>& in) {
    T v; std::memcpy(&v, in.data(), sizeof v); in = in.subspan(sizeof v); return v;
}
```

What you are *not* protected from: endianness (`std::endian::native`, `std::byteswap` in C++23), padding bytes (their values are unspecified — zero the struct with `{}` before writing, or write fields individually), `long double` and `bool` representation differences, and pointers (never serialize a pointer). A `.safetensors`-style file is exactly this: a header describing dtype/shape/offsets, then raw little-endian bytes.

---

## 12. `std::unique_ptr` with custom deleters and RAII for OS handles

`unique_ptr<T, D>`'s second parameter is the deleter. If `D` is an empty class, `sizeof(unique_ptr<T, D>) == sizeof(T*)` (EBO / `[[no_unique_address]]`); if it's a function pointer, it's 16 bytes and a call through a pointer.

```cpp
struct FileCloser { void operator()(std::FILE* f) const noexcept { if (f) std::fclose(f); } };
using unique_file = std::unique_ptr<std::FILE, FileCloser>;          // 8 bytes

struct MunmapDeleter { std::size_t len; void operator()(void* p) const noexcept { if (p) munmap(p, len); } };
using unique_mmap = std::unique_ptr<void, MunmapDeleter>;            // 16 bytes: stateful deleter

// CUDA: struct CudaFree { void operator()(float* p) const noexcept { cudaFree(p); } };
//       using device_ptr = std::unique_ptr<float[], CudaFree>;
```

`unique_ptr<void, D>` is legal because `D` does the deleting — this is how you wrap `mmap`, `dlopen` handles, and any opaque `void*` API. Prefer a *stateless* deleter when the API needs no extra data.

For non-pointer handles (`int fd`, `GLuint`, `cudaStream_t`), write a small RAII class: delete copy, move with `std::exchange(o.fd_, -1)`, `reset()` in the destructor, `explicit operator bool`. The example's `Fd` is 20 lines and makes leaking a descriptor impossible (`leaks --atExit` will still catch heap leaks; fds you check with `lsof -p <pid>`).

Rule from the Core Guidelines (R.1, R.11): raw `new`/`delete`, `fopen`/`fclose`, `malloc`/`free` should each appear in exactly one place — inside a class whose destructor undoes them.

---

## 13. Memory pools for tree/graph nodes — measured

Autograd nodes, Barnes–Hut cells, k-d tree nodes, BPE merge-tree nodes: many equal-sized objects, allocated in bursts and freed together. A **free-list pool** allocates `Slot`s in chunks and threads freed slots into a singly linked list:

```cpp
template <class T, std::size_t ChunkObjects = 4096>
class NodePool {
    union Slot { alignas(T) std::byte storage[sizeof(T)]; Slot* next; };  // the C union trick, legal here:
    std::vector<std::unique_ptr<Slot[]>> chunks_;                          // `next` is the active member
    Slot* free_ = nullptr;                                                 // until we construct a T
public:
    template <class... A> T* create(A&&... a) {
        if (!free_) refill();
        Slot* s = free_; free_ = s->next;
        return std::construct_at(reinterpret_cast<T*>(s->storage), std::forward<A>(a)...);
    }
    void destroy(T* p) noexcept {
        std::destroy_at(p);
        Slot* s = std::launder(reinterpret_cast<Slot*>(p));
        s->next = free_; free_ = s;
    }
};
```

Why it's faster: no per-object 16-byte `malloc` header, no size-class lookup, no lock, and nodes allocated together are *adjacent in memory*, so the traversal that follows streams through cache lines instead of chasing pointers all over the heap. Measured (example §10, 87 381 `Cell`s of 64 bytes, depth-8 quadtree, build + sum + free): `new`/`delete` **1.54 ms**, `NodePool` **0.65 ms** — **2.4×**, and the pool's second and later steps pay zero `malloc`s because slots are recycled. For an autograd tape you can go further: a *monotonic* arena reset at the end of each step (`mono.release()`) — no per-node free at all.

Alternatives to know: `std::pmr::unsynchronized_pool_resource` (§6) is the standard version of this; Boost.Pool; jemalloc/mimalloc as a drop-in `malloc` (often 1.5–2× on allocation-heavy code with zero code change: `brew install mimalloc`, then `DYLD_INSERT_LIBRARIES=$(brew --prefix mimalloc)/lib/libmimalloc.dylib ./prog`; Linux: `LD_PRELOAD`).

---

## 14. Tracking allocations to find hidden copies

The single most effective tool for finding *accidental* copies in numerics code is a global counter:

```cpp
static std::size_t g_new_calls = 0, g_new_bytes = 0;
void* operator new(std::size_t n) { ++g_new_calls; g_new_bytes += n; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc{}; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
// plus the align_val_t overloads if anything is over-aligned (example §7 has them)
```

Replacing `operator new` globally is legal and affects *every* allocation in the program — `std::vector`, `std::string`, `std::shared_ptr` control blocks, exceptions. Snapshot the counters around a region and print the delta. The example finds, in 10 calls each:

| Code | Allocations | Bytes |
|---|---|---|
| `Mat add(Mat a, Mat b)` — by value | 20 | 655 360 |
| `Mat add(const Mat&, const Mat&)` | 10 | 327 680 |
| `void add_into(const Mat&, const Mat&, Mat& out)` | 0 | 0 |
| `for (auto row : rows)` over 100 rows | 100 | 6 400 |
| `for (const auto& row : rows)` | 0 | 0 |
| `push_back` ×1000 without `reserve` | 11 | 16 376 |
| with `reserve(1000)` | 1 | 8 000 |

The scoped alternative is a **counting allocator** (`CountingAllocator<T>` in the example): same statistics, but only for the containers you give it, and no global override. Use the global one to hunt, the scoped one in unit tests ("this kernel performs zero allocations").

Other tools: Instruments' **Allocations** template (`xcrun xctrace record --template 'Allocations' --launch ./prog`) shows every allocation with a stack; `heaptrack`/`valgrind --tool=massif` on Linux; `MallocStackLogging=1 ./prog` then `leaks`/`malloc_history` on macOS.

---

## 15. AddressSanitizer in C++: container overflow and friends

`-fsanitize=address` (Chapter 11) does more in C++ than in C:

- **`container-overflow`**: libc++ annotates `std::vector`'s `[size, capacity)` region as poisoned. Reading `v.data()[3]` on a vector with `size() == 1, capacity() == 16` is *not* a heap overflow (the bytes are allocated) — ASan still reports `ERROR: AddressSanitizer: container-overflow` (verified on Apple clang 21). Same for `std::string` and `std::deque`.
- **`new`/`delete` mismatch** (`new[]` freed with `delete`, aligned `new` freed with plain `delete`): `alloc-dealloc-mismatch` (off by default on macOS: `ASAN_OPTIONS=alloc_dealloc_mismatch=1`).
- **`stack-use-after-scope`** / **`stack-use-after-return`** (`ASAN_OPTIONS=detect_stack_use_after_return=1`): a `std::string_view` or `std::span` into a dead local.
- **`-fsanitize=undefined`** catches misaligned access (`alignment`), `reinterpret_cast` to a wrong dynamic type (`vptr`, needs RTTI), signed overflow, and `-fsanitize=type` (Clang ≥ 20, experimental) hunts strict-aliasing violations.
- LeakSanitizer is unavailable on Apple Silicon; use `leaks --atExit -- ./prog`, and `MallocScribble=1` to fill freed memory with `0x55` so use-after-free crashes deterministically.

Custom allocators and pools **hide bugs from ASan**: memory you recycle yourself is never poisoned. Give your pool a debug mode that forwards to `new`/`delete` (or call `__asan_poison_memory_region` from `<sanitizer/asan_interface.h>` on freed slots) — and run the test suite in both modes.

---

## 16. Ownership design for a tensor library

PyTorch's model, which you'll reuse in projects P02–P03:

```
Tensor (handle, cheap to copy) ──► TensorImpl { shared_ptr<Storage>; offset; shape[]; strides[] }
                                                    │
                                                    ▼
                                             Storage { vector<float> data }   ← refcounted, shared by views
```

- **`Storage`** owns the bytes. It is refcounted (`std::shared_ptr<Storage>`; PyTorch uses an intrusive count to save the control-block allocation).
- **`TensorImpl`/`Tensor`** holds *metadata*: which storage, at what element offset, with what `shape` and `strides` (in elements). `t(i, j)` reads `data[offset + i*strides[0] + j*strides[1]]`.
- A **view** (`transpose`, `row`, `slice`, `reshape` of a contiguous tensor) copies the handle and edits metadata: O(1), no bytes moved, `use_count` goes up by one. Writing through the view writes the base — exactly `torch.Tensor.t()` / NumPy's `a.T`.
- **`is_contiguous()`**: `strides == row-major strides of shape`. Kernels check it and either take the fast path (raw pointer + `n`) or call `.contiguous()` to materialize.
- `shape`/`strides` live in a `SmallVector<size_t, 4–6>` (§7): dims never hit the heap. PyTorch's `SizesAndStrides` inlines 5.

The example builds a 2×3 tensor, a transpose view and a row view: **2 allocations total** (the `Storage` and the `vector<float>` inside it), `use_count == 3`, `&a(0,1) == &t(1,0)`, `t.is_contiguous() == false`, and `t.contiguous()` allocates a fresh row-major copy.

Ownership rules that fall out: kernels take `std::span<const float>` / raw pointers + strides (non-owning); `Tensor` handles are passed by value (two pointers + small vectors); only `Storage` ever `new`s; autograd's `grad_fn` holds `Tensor`s (shared storage), so a graph node keeps its inputs alive exactly as long as needed — and a cycle (`Tensor` → `grad_fn` → saved `Tensor` → …) is broken with `std::weak_ptr` or by clearing the graph after `backward()`.

---

## 17. Numbers to remember (Apple M-series, macOS arm64, measured or from `sysctl`)

| Quantity | Value | How to check |
|---|---|---|
| Cache line | 128 B (x86: 64 B) | `sysctl hw.cachelinesize` |
| `std::hardware_destructive_interference_size` | 256 on Apple clang 21 (two lines — the prefetcher pairs them) | `<new>`, `__cpp_lib_hardware_interference_size` |
| L1d / L2 (performance core) | 64 KB (per core, as reported by `hw.l1dcachesize`) / 16 MB shared | `sysctl hw.l1dcachesize hw.perflevel0.l2cachesize` |
| `__STDCPP_DEFAULT_NEW_ALIGNMENT__` | 16 | compile-time constant |
| `malloc`+`free` of a small block | 20–50 ns; a bump allocation ~1 ns | example §3: 15× on 20 k vectors |
| `malloc` header overhead | 16 B per block on macOS libmalloc (rounds small sizes to 16 B classes) | `malloc_size(p)` |
| `sizeof(std::vector<T>)` / `std::string` / `shared_ptr` / `unique_ptr` | 24 / 24 / 16 / 8 | `static_assert` |
| `std::string` SSO capacity (libc++) | 22 chars | `std::string().capacity()` |
| Node pool vs `new`/`delete` on 87 k tree nodes | 2.4× | example §10 |
| `pmr::monotonic_buffer_resource` vs `std::vector` on small temporaries | ~3× (hand-rolled arena: ~15×) | example §4 |
| Cost of a `shared_ptr` copy | one atomic increment, ~5–20 ns; a `Tensor` view is two of them | `use_count()` |

Linux equivalents: `getconf LEVEL1_DCACHE_LINESIZE`, `lscpu`, `perf stat -e cache-misses`, `valgrind --tool=massif`, `heaptrack`.

---

## Gotchas and undefined behavior

- **Reading before the constructor finishes / after the destructor starts** is UB even if the bytes are still there ([basic.life]/7). "It printed the old value" is not a defense.
- **`alignas` forgotten on a `std::byte` buffer** meant to hold `T`: misaligned object, UB; on arm64 a NEON load of it may fault.
- **`std::aligned_alloc(64, 100)`**: size not a multiple of alignment → returns `nullptr` on macOS (glibc is lenient — non-portable).
- **`delete` on memory from `aligned_alloc`/`malloc`**, or `free` on memory from `new`: UB; ASan's `alloc-dealloc-mismatch` finds it.
- **Over-aligned type + custom `operator new` without the `align_val_t` overload**: the compiler silently calls the default aligned `new`, and your counters/pool miss it. Overload both.
- **`reinterpret_cast<T*>(bytes)` then use without `construct_at`/placement new** for non-implicit-lifetime `T` (anything with a constructor, `std::string`): UB. For implicit-lifetime `T`, fine in C++20 (P0593).
- **Pointer to a re-constructed object with a `const` member** used without `std::launder`: UB; the compiler may have hoisted the old value.
- **`memcpy` of non-trivially-copyable objects** (`std::string`, anything with a vptr, `SmallVector`): breaks internal pointers; UB.
- **`union { float f; uint32_t u; }` type punning**: legal in C, UB in C++. Use `std::bit_cast` or `memcpy`.
- **Arena `reset()` while containers still point into it**: every `arena_vector` becomes dangling; their destructors will call `deallocate` (no-op here — but a pool would corrupt its free list).
- **Stateful allocator with `propagate_on_container_swap = false` and unequal allocators**: `swap` is UB ([container.reqmts]). Keep swaps within one arena.
- **`SmallVector` move that steals the pointer when the source is inline**: the pointer points into the (soon dead) source. Move element-wise.
- **Move constructor not `noexcept`** → `vector::push_back` and your `grow()` copy instead of moving.
- **`shared_ptr` cycles** in an autograd graph (`Tensor` ↔ `Node`): `use_count` never hits 0; memory grows every step. `weak_ptr` for back-edges, or an explicit `graph.clear()`.
- **Serializing padding bytes / pointers / `bool`s** and reading them on another compiler: garbage. Write fields, not structs, for anything that leaves the machine.
- **Pool hides use-after-free from ASan**: a freed slot is immediately valid memory again. Debug mode forwarding to `new`/`delete`.

---

## Common mistakes checklist

- [ ] Every raw buffer that will hold a `T` is declared `alignas(T)` and sized `sizeof(T) * n`.
- [ ] Every `construct_at`/placement `new` is paired with a `destroy_at`/explicit destructor call on every path, including exceptional ones (`try`/`catch` + `destroy_n`).
- [ ] Growth moves only when `is_nothrow_move_constructible_v<T>`; hand-written move constructors are `noexcept`.
- [ ] Allocator has `value_type`, `allocate`, `deallocate`, the rebind constructor, and `==` meaning "may deallocate each other's memory".
- [ ] Overrode `operator new`/`delete`? Also the `align_val_t` and sized variants.
- [ ] Type punning goes through `std::bit_cast` or `memcpy`, never `reinterpret_cast` between unrelated non-byte types.
- [ ] Byte buffers are `std::byte`, not `char`; `std::as_bytes`/`as_writable_bytes` at the boundary.
- [ ] Every C resource (`FILE*`, fd, `mmap`, device pointer) lives in a `unique_ptr` with a deleter or a 20-line RAII class; `fclose`/`munmap`/`close` appear exactly once each in the codebase.
- [ ] Hot loops audited with an allocation counter: zero allocations inside kernels, `reserve` before `push_back` loops, `const auto&` in range-for, `_into` output parameters.
- [ ] Node-heavy structures use a pool or `pmr` resource; the pool has a debug mode that forwards to `new`/`delete` for ASan runs.
- [ ] Tensor views share `Storage`; only `Storage` allocates; `is_contiguous()` gates the fast path.
- [ ] Class layout checked with `static_assert(sizeof(X) == …)` where size matters (nodes, particles, cells).

---

## You can move on when...

- You can explain the difference between "storage exists" and "object exists" and name the two operations that begin and end a non-trivial object's lifetime.
- You can write `SmallVector<T, N>` from memory with a correct move constructor for both the inline and heap cases, and say why `memcpy` is not allowed.
- You can list the five members an allocator must provide and explain what `allocator_traits` supplies for free.
- You can choose between a custom allocator, `pmr::monotonic_buffer_resource` and `pmr::unsynchronized_pool_resource` for (a) an autograd tape, (b) a BPE vocabulary, (c) a public `std::vector<float>` API — and justify each.
- You can turn `float f; uint32_t u = *(uint32_t*)&f;` into two legal forms and state why the original is UB in C++ but not in C.
- You can predict `sizeof` for a struct with a vptr, an empty base, an empty `[[no_unique_address]]` member, and a `char`–`double`–`char` triple.
- You can draw the `Tensor → TensorImpl → Storage` diagram and show which allocations a transpose performs (zero) and what `use_count` becomes.
- You can wire a global counting `operator new` into a program and find a hidden copy in five minutes.
