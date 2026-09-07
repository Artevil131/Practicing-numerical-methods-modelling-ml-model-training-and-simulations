# Chapter 16 — Exercises

Write each exercise as `ex16_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++20 -O2 -o ex16_K ex16_K.cpp`. Run every exercise a second time under
`c++ -Wall -Wextra -std=c++20 -g -fsanitize=address,undefined -o ex16_K ex16_K.cpp` — several of them
exist precisely to make the sanitizer speak, and you should record what it says in a comment. For
timing exercises use `-O2`, repeat ≥5 times, report the minimum (Chapter 12).

---

**16.1 — Lifetime tracer**
Write `struct Probe` whose constructor, copy constructor, move constructor, copy/move assignment and destructor each print their name and `this`. Then, in `main`, (a) declare an `alignas(Probe) std::byte buf[sizeof(Probe)]`, (b) construct a `Probe` in it with placement `new`, (c) destroy it explicitly, (d) construct another with `std::construct_at`, (e) destroy it with `std::destroy_at`. Print `sizeof(Probe)`, `alignof(Probe)` and the address of `buf`, and verify all printed `this` values equal `buf`. Finally, remove the `alignas` and add a `double` member: print `reinterpret_cast<std::uintptr_t>(buf) % alignof(Probe)` and explain in a comment why a nonzero result makes step (b) UB.

Example: output lists exactly two `ctor` and two `dtor` lines, all at the same address.

<details><summary>Hint</summary>
`::new (static_cast<void*>(buf)) Probe{}` and `p->~Probe()`. UBSan (`-fsanitize=undefined`) reports `misaligned address` if you force the buffer to an odd offset with `std::byte pad; alignas(1) std::byte buf[...]` inside a struct.
</details>

---

**16.2 — Layout detective**
For each of the following, predict `sizeof` and `alignof` in a comment *before* running, then `static_assert` your predictions: `struct A { char c; int i; char d; }`, `struct B { char c; char d; int i; }`, `struct C { double d; char c; }`, `struct D : C { char e; }` (does `e` fit in `C`'s tail padding? on Itanium ABI it does not for a non-POD… check), `struct E {}`, `struct F : E { int x; }`, `struct G { E e; int x; }`, `struct H { [[no_unique_address]] E e; int x; }`, `struct V { virtual void f(); int x; }`, `std::optional<double>`, `std::variant<int, double, char>`, `std::unique_ptr<int>`, `std::unique_ptr<int, void(*)(int*)>`, `std::shared_ptr<int>`, `std::string`, `std::vector<int>`. Then reorder the members of a `struct Particle { bool alive; double x, y, z; float mass; int id; char tag; }` to minimize its size and report before/after.

Example: `Particle` shrinks from 48 bytes to 40 (or better).

<details><summary>Hint</summary>
Sort members by decreasing alignment. `offsetof(T, m)` works on standard-layout types. Compile with `-Wpadded` to have Clang tell you every padding byte it inserts.
</details>

---

**16.3 — `std::launder` and the const-member trap**
Write `struct Cfg { const int version; double lr; }`. Construct one in an `alignas(Cfg) std::byte buf[sizeof(Cfg)]`, keep the pointer `p`, then destroy it and construct a `Cfg{2, 0.01}` in the same storage. Print `p->version` directly and via `std::launder(p)->version` at `-O0` and `-O2`. Then write a function `int read_version(Cfg* p)` that reads `p->version` twice with a re-construction in between and returns the sum; look at the `-O2` assembly (`c++ -S -O2` or Compiler Explorer) and note whether the second load was hoisted. Document what you saw in a comment, and which of the two accesses is UB.

Example: `-O2` may print `1 2` for the direct access when the standard says the direct access is UB — or `2 2`; either is allowed for UB.

<details><summary>Hint</summary>
The compiler is allowed to assume a `const` member never changes for the lifetime of the object it *thinks* `p` points to. `std::launder` tells it "this pointer now refers to whatever object lives there".
</details>

---

**16.4 — Counting allocator for a unit test**
Write `template <class T> struct CountingAlloc` (`value_type`, `allocate`, `deallocate`, rebind constructor, `==`/`!=`) with `static inline` counters for calls and bytes. Use it as `std::vector<double, CountingAlloc<double>>` and `std::unordered_map<int, int, std::hash<int>, std::equal_to<>, CountingAlloc<std::pair<const int, int>>>`. Write a function `void assert_no_alloc(F f)` that snapshots the counters, runs `f`, and throws if anything was allocated. Use it to prove (a) `dot(a, b)` on two counted vectors allocates nothing, (b) `push_back` into a vector with `reserve(n)` allocates nothing for the first `n` pushes, (c) `unordered_map::operator[]` on a *new* key allocates (a node), and (d) `find` does not. Print the number of allocations the map made to insert 1000 keys and compute allocations-per-key.

Example: `inserting 1000 keys: 1013 allocations (1.013 per key; the extras are bucket-array rehashes)`.

<details><summary>Hint</summary>
The rebind constructor `template <class U> CountingAlloc(const CountingAlloc<U>&) noexcept {}` is what lets `unordered_map` build `CountingAlloc<Node>`. Rehash count = how many times the bucket array was reallocated; `reserve(1000)` on the map first and see it drop to ~1001.
</details>

---

**16.5 — Bump arena + `std::vector`, measured**
Implement `class Arena` (bump pointer over one block, `allocate(bytes, align)`, `deallocate` no-op, `reset()`, `used()`) and `template <class T> class ArenaAlloc` satisfying the Allocator requirements. Benchmark: build 50 000 `std::vector<double>` of 8–64 elements (size from a `std::mt19937`) and sum their last elements, with (a) `std::vector<double>`, (b) `std::vector<double, ArenaAlloc<double>>` with `reset()` between repetitions, (c) `std::pmr::vector<double>` on a `std::pmr::monotonic_buffer_resource` over the same block with `std::pmr::null_memory_resource()` as upstream. Report min-of-5 times and the speedups. Then make the arena too small in (c) and show what `null_memory_resource` does.

Example: `std   0.71 ms | arena 0.05 ms (14x) | pmr 0.21 ms (3.4x)`; the too-small case throws `std::bad_alloc`.

<details><summary>Hint</summary>
Alignment: `(cur + align - 1) & ~(align - 1)` on a `std::uintptr_t`. Reserve the arena at `sum(sizes) * sizeof(double) * 2` — vector growth wastes about 2×. Don't time the `mt19937`: pre-generate the sizes.
</details>

---

**16.6 — `SmallVector<T, N>` with a throwing element type**
Implement `SmallVector<T, N>` as in the lesson (inline storage, `emplace_back`, `pop_back`, `size`, `capacity`, `is_inline`, `operator[]`, iterators, Rule of Five, `swap`) and test it with (a) `int`, (b) `std::string` (non-trivial, `noexcept` move), (c) a `Fragile` type whose copy constructor throws on every 7th copy and whose move constructor is **not** `noexcept`. Assert after each operation that `Fragile::alive == expected`. Make growth from inline to heap throw inside (c) and assert the vector is unchanged afterwards (strong guarantee). Finally, write the *bug*: a move constructor that steals `data_` even when the source is inline; run it under ASan and record the report type.

Example: ASan reports `stack-use-after-scope` or `stack-use-after-return` (enable `ASAN_OPTIONS=detect_stack_use_after_return=1`) for the buggy move.

<details><summary>Hint</summary>
With a non-`noexcept` move, `grow()` must copy (`std::uninitialized_copy_n`), catch, free the new buffer, and re-throw — the old buffer is untouched. Count `Fragile` constructions vs destructions to prove no leak. `std::launder(reinterpret_cast<T*>(inline_))` for the inline pointer.
</details>

---

**16.7 — Serialize a checkpoint with `std::span<std::byte>`**
Define `struct TensorHeader { std::uint32_t magic, dtype, ndim; std::uint32_t shape[4]; }` (trivially copyable, no padding — verify with `static_assert(sizeof(TensorHeader) == 28)`) and a function `std::vector<std::byte> save(const std::vector<float>& data, std::span<const std::uint32_t> shape)` that writes header then payload using `std::as_bytes`, and `load(std::span<const std::byte>)` that validates `magic`, checks the payload length matches the product of `shape`, and returns `{shape, data}` via `memcpy`. Add `std::bit_cast<std::uint32_t>` to compute a simple checksum (XOR of all float bit patterns) stored after the payload. Test round trip, then corrupt one byte and show the checksum catches it. Write the blob with `std::fwrite` to a `std::tmpfile()` held in a `unique_ptr<FILE, FileCloser>`, `rewind`, read it back, and verify.

Example: `saved 28 + 96 + 4 = 128 bytes; round trip OK; after corrupting byte 40: checksum mismatch`.

<details><summary>Hint</summary>
`in = in.subspan(sizeof(TensorHeader))` advances the view. Never `reinterpret_cast<const float*>(bytes)` and read — use `memcpy` into `data.data()`. `std::endian::native` tells you what to write in the header for portability.
</details>

---

**16.8 — Autograd node pool (ML)**
Take the `std::variant`-based autograd graph from Chapter 13 exercise 13.3 (or write a minimal one: `Node { Op op; double value, grad; Node* a; Node* b; }`) and allocate nodes from (a) `new`/`delete`, (b) a `NodePool<Node>` free-list pool, (c) a `std::pmr::monotonic_buffer_resource` that you `release()` after each backward pass. Build the graph for `f(x) = Σ_i tanh(x·w_i + b_i)` with 20 000 terms, run forward + backward, free, and repeat 20 times; report min-of-5 timings for each allocator. Verify the gradient against central finite differences once. Then run all three under ASan with a deliberate use-after-free (read a node after the pool recycled it) and record which allocators ASan catches.

Example: `new/delete 3.1 ms | pool 1.2 ms | monotonic 0.9 ms`; ASan catches (a) only.

<details><summary>Hint</summary>
For (c) construct nodes with `std::pmr::polymorphic_allocator<Node>{&mr}.new_object<Node>(...)` or `construct_at` into `mr.allocate(sizeof(Node), alignof(Node))`; `release()` returns all memory to upstream in one call. In the pool, add `#ifdef POOL_DEBUG` that forwards to `new`/`delete` so ASan can see individual objects.
</details>

---

**16.9 — Barnes–Hut cells: pool vs `new`, and cache locality (sims)**
Write a 2D quadtree insert for `N = 100 000` uniformly random bodies: `struct Cell { double cx, cy, half, mass, comx, comy; Cell* child[4]; int body; }`. Allocate cells (a) with `new`, (b) from a `NodePool<Cell>`, (c) from a `std::vector<Cell>` used as an arena with **indices instead of pointers** (`int child[4]`). Time build + one center-of-mass pass + one traversal computing the force on 1000 sample bodies with θ = 0.5, for each layout. Report timings, `sizeof(Cell)` for the pointer and index versions, and count cells. Explain in a comment why (c) is fastest even though (b) already removed `malloc`.

Example: `new 61 ms | pool 38 ms | index-arena 29 ms; sizeof(Cell) 88 → 72 bytes`.

<details><summary>Hint</summary>
Indices are 4 bytes vs 8 for pointers, so more cells per cache line; and a `std::vector<Cell>` arena is contiguous *in insertion order*, which matches traversal order for a depth-first build. `reserve(2 * N)` up front. Reuse the tree each step by `clear()` (no destructors needed — `Cell` is trivial).
</details>

---

**16.10 — `Tensor` with refcounted `Storage` and strided views (ML)**
Implement `struct Storage { std::vector<float> data; }`, `class Tensor` holding `std::shared_ptr<Storage>`, `std::size_t offset`, and `SmallVector<std::size_t, 6> shape, strides` (use your 16.6 or a `std::array` + `ndim`). Provide: `Tensor(shape)` (contiguous), `operator()(i, j)` / `at(std::span<const std::size_t> idx)` for any rank, `transpose(a, b)`, `slice(dim, start, stop, step)`, `reshape(new_shape)` (view if contiguous, else copy), `is_contiguous()`, `contiguous()`, `use_count()`. Prove with a global counting `operator new` that `transpose` and `slice` perform **zero** allocations and `contiguous()` on a transposed tensor performs exactly the allocations of one `Storage`. Then write `void add_(Tensor& out, const Tensor& a, const Tensor& b)` that takes the raw-pointer fast path when all three are contiguous and the strided path otherwise; time both on a 1024×1024 tensor vs its transpose.

Example: `transpose: 0 allocs, use_count 2; contiguous(): 2 allocs; add_ contiguous 0.9 ms, strided 3.7 ms`.

<details><summary>Hint</summary>
Row-major strides: `strides[ndim-1] = 1; strides[i] = strides[i+1] * shape[i+1]`. `is_contiguous()` compares against exactly that. `slice(dim, start, stop, step)` = `offset += start * strides[dim]; shape[dim] = ceil((stop - start) / step); strides[dim] *= step`. Never `const_cast` to write through a `const Tensor&`: give `Tensor` a const and a non-const `operator()`.
</details>
