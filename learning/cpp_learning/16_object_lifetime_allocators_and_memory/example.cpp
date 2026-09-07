// Chapter 16 — Object lifetime, allocators and memory, in one program.
//
// Compile:  c++ -Wall -Wextra -std=c++20 -O2 -o ex_demo example.cpp
// Run:      ./ex_demo
// Debug:    c++ -Wall -Wextra -std=c++20 -g -fsanitize=address,undefined -o ex_demo example.cpp
//
// This chapter REQUIRES -std=c++20: std::construct_at / destroy_at, std::bit_cast, std::span,
// [[no_unique_address]], and constexpr std::allocator are C++20. std::start_lifetime_as (C++23)
// is discussed in lesson.md §2 but not implemented by Apple clang 21's libc++, so it is not used.
//
// Sections (each is a numbered function called from main):
//   1  storage duration vs lifetime; placement new + explicit destructor; construct_at/destroy_at
//   2  alignas/alignof, std::aligned_alloc, over-aligned operator new, class layout (vptr/EBO/padding)
//   3  an arena (bump) allocator + Allocator-concept wrapper usable by std::vector; timing
//   4  std::pmr: monotonic_buffer_resource + pmr::vector vs std::vector; unsynchronized_pool_resource
//   5  SmallVector<T, N>: small-buffer optimization with uninitialized-memory algorithms; asserted
//   6  exception safety when constructing in raw memory (uninitialized_copy cleans up)
//   7  counting allocations (global operator new override) to find hidden copies in numerics code
//   8  object representation: bit_cast, memcpy, std::byte, span<std::byte> serialization
//   9  unique_ptr with custom deleters (FILE*, mmap) and an RAII wrapper for a POSIX fd
//  10  a node pool for tree/graph nodes (Barnes–Hut cells / autograd nodes) — measured speedup
//  11  ownership design for a tensor library: refcounted Storage + TensorImpl views with strides

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <new>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <fcntl.h>     // open, O_RDONLY            (POSIX, §9)
#include <sys/mman.h>  // mmap, munmap              (POSIX, §9)
#include <unistd.h>    // close                     (POSIX, §9)

// ---------------------------------------------------------------------------------------------
// Timing helper (Chapter 12): min of `reps` runs, in milliseconds.
// ---------------------------------------------------------------------------------------------
template <class F>
static double time_min_ms(int reps, F&& f) {
    using clock = std::chrono::steady_clock;
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = clock::now();
        f();
        double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
        best = std::min(best, ms);
    }
    return best;
}

template <class T>
static inline void do_not_optimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }

// =============================================================================================
// §7 (declared first because everything below allocates): global operator new/delete override
// that counts calls and bytes. malloc/free do the real work. Every allocation in the program
// goes through here — including std::vector, std::string, std::shared_ptr control blocks.
// =============================================================================================
namespace alloc_stats {
static std::size_t g_new_calls = 0, g_new_bytes = 0, g_delete_calls = 0;
struct Snapshot { std::size_t calls, bytes, frees; };
static Snapshot snapshot() { return {g_new_calls, g_new_bytes, g_delete_calls}; }
static void report(const char* label, Snapshot before) {
    std::printf("  %-38s %6zu allocations, %9zu bytes, %6zu frees\n", label,
                g_new_calls - before.calls, g_new_bytes - before.bytes, g_delete_calls - before.frees);
}
}  // namespace alloc_stats

void* operator new(std::size_t n) {
    ++alloc_stats::g_new_calls;
    alloc_stats::g_new_bytes += n;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { ++alloc_stats::g_delete_calls; std::free(p); }
void operator delete(void* p, std::size_t) noexcept { ++alloc_stats::g_delete_calls; std::free(p); }
// Over-aligned variants: forward to posix_memalign so `new alignas(64) T` still works (§2).
void* operator new(std::size_t n, std::align_val_t al) {
    ++alloc_stats::g_new_calls;
    alloc_stats::g_new_bytes += n;
    void* p = nullptr;
    std::size_t a = std::max(static_cast<std::size_t>(al), sizeof(void*));
    if (posix_memalign(&p, a, n ? n : 1) != 0) throw std::bad_alloc{};
    return p;
}
void operator delete(void* p, std::align_val_t) noexcept { ++alloc_stats::g_delete_calls; std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { ++alloc_stats::g_delete_calls; std::free(p); }

// =============================================================================================
// §1  Storage duration vs lifetime
// =============================================================================================
struct Tracked {                       // non-trivial: lifetime begins when the ctor finishes,
    static inline int alive = 0;       // ends when the dtor starts ([basic.life]/1).
    int id;
    explicit Tracked(int i) : id(i) { ++alive; }
    ~Tracked() { --alive; }
    Tracked(const Tracked& o) : id(o.id) { ++alive; }
    Tracked& operator=(const Tracked&) = default;
};

static void section1_lifetime() {
    std::puts("\n=== 1. storage duration vs lifetime ===");

    // (a) Raw storage: automatic storage duration, but NO Tracked object lives here yet.
    //     alignas(Tracked) is mandatory: std::byte arrays are only 1-byte aligned by default.
    alignas(Tracked) std::byte storage[sizeof(Tracked)];
    std::printf("  storage acquired, Tracked::alive = %d\n", Tracked::alive);

    // (b) Placement new: begins the lifetime of a Tracked inside `storage`. No allocation.
    Tracked* t = ::new (static_cast<void*>(storage)) Tracked{7};
    std::printf("  after placement new, alive = %d, id = %d\n", Tracked::alive, t->id);

    // (c) Explicit destructor call ends the lifetime. The storage still exists (until the
    //     enclosing block exits) and may be reused for another object.
    t->~Tracked();
    std::printf("  after explicit ~Tracked(), alive = %d  (storage still valid, object gone)\n",
                Tracked::alive);

    // (d) C++20 spelling of the same two steps. construct_at is constexpr-friendly and never
    //     picks up a user-declared operator new; destroy_at calls the destructor.
    Tracked* u = std::construct_at(reinterpret_cast<Tracked*>(storage), 8);
    std::printf("  construct_at -> alive = %d, id = %d\n", Tracked::alive, u->id);
    std::destroy_at(u);
    std::printf("  destroy_at   -> alive = %d\n", Tracked::alive);

    // (e) Implicit-lifetime types (trivial aggregates, scalars, arrays of them): certain
    //     operations — malloc, memcpy into storage, a std::byte buffer — implicitly *create* the
    //     object ([intro.object]/13, C++20 P0593). This is what makes the C idiom
    //     `float* f = malloc(n * sizeof(float)); f[0] = 1;` well-defined in C++20.
    struct Vec2 { float x, y; };                         // trivial aggregate => implicit lifetime
    static_assert(std::is_trivially_copyable_v<Vec2> && std::is_trivially_default_constructible_v<Vec2>);
    void* raw = std::malloc(sizeof(Vec2) * 4);           // malloc implicitly creates Vec2[4] as needed
    Vec2* v = static_cast<Vec2*>(raw);                    // the pointer now points to the created array
    v[0] = {1.f, 2.f};
    std::printf("  implicit-lifetime Vec2 from malloc: (%g, %g)\n", v[0].x, v[0].y);
    std::free(raw);

    // (f) std::launder: when storage is reused for a *new* object and you still hold a pointer
    //     to the old one, the old pointer may only be used if the new object is "transparently
    //     replaceable" (same type, no const/reference members). Otherwise launder it.
    struct WithConst { const int k; };
    alignas(WithConst) std::byte wc_storage[sizeof(WithConst)];
    WithConst* w1 = ::new (static_cast<void*>(wc_storage)) WithConst{1};
    std::destroy_at(w1);
    ::new (static_cast<void*>(wc_storage)) WithConst{2};   // NOT transparently replaceable (const member)
    // w1->k here would be UB (compiler may assume k is still 1). Launder the pointer:
    WithConst* w2 = std::launder(w1);
    std::printf("  launder after re-construction with const member: k = %d\n", w2->k);
    std::destroy_at(w2);
}

// =============================================================================================
// §2  Alignment and class layout
// =============================================================================================
struct alignas(64) CacheLine { double d[8]; };            // 64 bytes, aligned to 64 (over-aligned)
struct Padded { char c; double d; char e; };              // 1 + 7pad + 8 + 1 + 7pad = 24
struct Reordered { double d; char c; char e; };           // 8 + 1 + 1 + 6pad = 16
struct Empty {};
struct Derived : Empty { int x; };                        // empty base optimization: sizeof == 4
struct Member { Empty e; int x; };                        // empty *member* costs 1 byte + padding: 8
struct NUA { [[no_unique_address]] Empty e; int x; };     // C++20: member can share address: 4
struct Poly { virtual ~Poly() = default; int x; };        // vptr (8) + int (4) + 4 pad = 16

static void section2_alignment_layout() {
    std::puts("\n=== 2. alignment and layout ===");
    std::printf("  alignof(double)=%zu alignof(CacheLine)=%zu sizeof(CacheLine)=%zu\n",
                alignof(double), alignof(CacheLine), sizeof(CacheLine));
    std::printf("  __STDCPP_DEFAULT_NEW_ALIGNMENT__ = %zu  (plain new guarantees only this)\n",
                static_cast<std::size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__));

    // Over-aligned new: since C++17, `new CacheLine` calls operator new(size, align_val_t{64}).
    auto* cl = new CacheLine{};
    std::printf("  new CacheLine -> address %% 64 = %zu\n",
                static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(cl) % 64));
    delete cl;

    // C11/C++17 std::aligned_alloc: size must be a multiple of alignment. Free with std::free.
    void* p = std::aligned_alloc(64, 64 * 16);
    std::printf("  aligned_alloc(64, 1024) -> address %% 64 = %zu\n",
                static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(p) % 64));
    std::free(p);

    std::printf("  sizeof Padded=%zu Reordered=%zu (same members, reordered: saves 8 bytes)\n",
                sizeof(Padded), sizeof(Reordered));
    std::printf("  sizeof Derived(EBO)=%zu Member=%zu NUA([[no_unique_address]])=%zu Poly(vptr)=%zu\n",
                sizeof(Derived), sizeof(Member), sizeof(NUA), sizeof(Poly));
    static_assert(sizeof(Derived) == sizeof(int));
    static_assert(sizeof(NUA) == sizeof(int));
    static_assert(offsetof(Padded, d) == 8, "double is placed at offset 8 after 7 bytes of padding");
}

// =============================================================================================
// §3  Arena (bump) allocator + Allocator-concept adaptor for std::vector
// =============================================================================================
class Arena {
    std::byte* begin_;
    std::byte* cur_;
    std::byte* end_;
public:
    explicit Arena(std::size_t bytes)
        : begin_(static_cast<std::byte*>(::operator new(bytes))), cur_(begin_), end_(begin_ + bytes) {}
    ~Arena() { ::operator delete(begin_); }
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* allocate(std::size_t n, std::size_t align) {
        std::uintptr_t c = reinterpret_cast<std::uintptr_t>(cur_);
        std::uintptr_t aligned = (c + align - 1) & ~(static_cast<std::uintptr_t>(align) - 1);
        std::byte* p = begin_ + (aligned - reinterpret_cast<std::uintptr_t>(begin_));
        if (p + n > end_) throw std::bad_alloc{};
        cur_ = p + n;
        return p;
    }
    void deallocate(void*, std::size_t) noexcept { /* bump allocators free everything at once */ }
    void reset() noexcept { cur_ = begin_; }           // ends the lifetime of everything inside!
    std::size_t used() const noexcept { return static_cast<std::size_t>(cur_ - begin_); }
};

// Minimal C++20 Allocator: value_type, allocate, deallocate, ==/!=, and rebind-ability via the
// template parameter. std::allocator_traits fills in pointer, size_type, construct, etc.
template <class T>
class ArenaAllocator {
    Arena* arena_;
public:
    using value_type = T;
    // Tell containers that copies of this allocator compare equal iff they share the arena,
    // and that the allocator must travel with the container on move (default) — not on
    // copy-assignment (default is false, which is what we want for a stateful allocator).
    using propagate_on_container_move_assignment = std::true_type;

    explicit ArenaAllocator(Arena& a) noexcept : arena_(&a) {}
    template <class U> ArenaAllocator(const ArenaAllocator<U>& o) noexcept : arena_(o.arena()) {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(arena_->allocate(n * sizeof(T), alignof(T)));
    }
    void deallocate(T* p, std::size_t n) noexcept { arena_->deallocate(p, n * sizeof(T)); }
    Arena* arena() const noexcept { return arena_; }

    template <class U> bool operator==(const ArenaAllocator<U>& o) const noexcept { return arena_ == o.arena(); }
    template <class U> bool operator!=(const ArenaAllocator<U>& o) const noexcept { return arena_ != o.arena(); }
};

template <class T> using arena_vector = std::vector<T, ArenaAllocator<T>>;

static void section3_arena_allocator() {
    std::puts("\n=== 3. arena allocator for std::vector ===");
    using Traits = std::allocator_traits<ArenaAllocator<double>>;
    static_assert(std::is_same_v<Traits::pointer, double*>);
    static_assert(std::is_same_v<Traits::rebind_alloc<int>, ArenaAllocator<int>>);

    Arena arena(1 << 20);
    arena_vector<double> v(ArenaAllocator<double>{arena});
    for (int i = 0; i < 10; ++i) v.push_back(i * 0.5);
    std::printf("  arena_vector<double> size=%zu sum=%g arena.used=%zu bytes (growth 1,2,4,8,16 doubles)\n",
                v.size(), std::accumulate(v.begin(), v.end(), 0.0), arena.used());

    // Timing: the classic "many small temporaries" pattern, e.g. per-sample feature vectors.
    constexpr int kVecs = 20000, kLen = 16;
    Arena big(static_cast<std::size_t>(kVecs) * kLen * sizeof(double) * 2 + 4096);
    double sink = 0;

    double t_std = time_min_ms(5, [&] {
        double s = 0;
        for (int i = 0; i < kVecs; ++i) {
            std::vector<double> tmp(kLen, 1.0 * i);         // malloc + free per iteration
            s += tmp[kLen - 1];
        }
        sink += s;
    });
    double t_arena = time_min_ms(5, [&] {
        double s = 0;
        big.reset();
        for (int i = 0; i < kVecs; ++i) {
            arena_vector<double> tmp(kLen, 1.0 * i, ArenaAllocator<double>{big});  // bump pointer
            s += tmp[kLen - 1];
        }
        sink += s;
    });
    do_not_optimize(sink);
    std::printf("  %d vectors of %d doubles:  std::vector %.3f ms   arena_vector %.3f ms   (%.1fx)\n",
                kVecs, kLen, t_std, t_arena, t_std / t_arena);
}

// =============================================================================================
// §4  std::pmr — the standard's runtime-polymorphic allocator
// =============================================================================================
static void section4_pmr() {
    std::puts("\n=== 4. std::pmr ===");
    // pmr::vector<T> is std::vector<T, std::pmr::polymorphic_allocator<T>>. The *type* is fixed;
    // the memory_resource* inside is a runtime choice. Two pmr::vector<double> with different
    // resources have the same type — unlike arena_vector vs std::vector in §3.
    alignas(std::max_align_t) std::byte stack_buf[4096];
    std::pmr::monotonic_buffer_resource mono(stack_buf, sizeof stack_buf);   // falls back to new_delete
    std::pmr::vector<double> pv(&mono);
    for (int i = 0; i < 100; ++i) pv.push_back(i);
    std::printf("  pmr::vector on a 4 KB stack buffer: size=%zu, data on stack? %s\n", pv.size(),
                (reinterpret_cast<std::byte*>(pv.data()) >= stack_buf &&
                 reinterpret_cast<std::byte*>(pv.data()) < stack_buf + sizeof stack_buf) ? "yes" : "no");

    // unsynchronized_pool_resource: buckets of fixed-size blocks, reuses freed blocks (unlike
    // monotonic), no mutex (single-threaded). Good for node-heavy structures that churn.
    std::pmr::unsynchronized_pool_resource pool;
    std::pmr::vector<std::pmr::string> names(&pool);       // the allocator propagates to elements!
    names.emplace_back("autograd");                        // string uses `pool` too (uses_allocator)
    names.emplace_back("barnes-hut");
    std::printf("  pool-backed pmr::vector<pmr::string>: %s, %s (both strings' allocators == pool: %s)\n",
                names[0].c_str(), names[1].c_str(),
                names[0].get_allocator().resource() == &pool ? "yes" : "no");

    // Timing: same benchmark as §3 with pmr. Expect: close to the hand-written arena, because the
    // virtual call to do_allocate is cheap compared with malloc/free.
    constexpr int kVecs = 20000, kLen = 16;
    std::vector<std::byte> backing(static_cast<std::size_t>(kVecs) * kLen * sizeof(double) * 2 + 4096);
    double sink = 0;
    double t_std = time_min_ms(5, [&] {
        double s = 0;
        for (int i = 0; i < kVecs; ++i) { std::vector<double> tmp(kLen, 1.0 * i); s += tmp[kLen - 1]; }
        sink += s;
    });
    double t_pmr = time_min_ms(5, [&] {
        std::pmr::monotonic_buffer_resource mr(backing.data(), backing.size(), std::pmr::null_memory_resource());
        double s = 0;
        for (int i = 0; i < kVecs; ++i) {
            std::pmr::vector<double> tmp(kLen, 1.0 * i, &mr);
            s += tmp[kLen - 1];
        }
        sink += s;
    });
    double t_pool = time_min_ms(5, [&] {
        std::pmr::unsynchronized_pool_resource pr;
        double s = 0;
        for (int i = 0; i < kVecs; ++i) {
            std::pmr::vector<double> tmp(kLen, 1.0 * i, &pr);
            s += tmp[kLen - 1];
        }
        sink += s;
    });
    do_not_optimize(sink);
    std::printf("  %d vectors of %d doubles:  std::vector %.3f ms   pmr monotonic %.3f ms (%.1fx)   pmr pool %.3f ms (%.1fx)\n",
                kVecs, kLen, t_std, t_pmr, t_std / t_pmr, t_pool, t_std / t_pool);
}

// =============================================================================================
// §5  SmallVector<T, N> — small-buffer optimization
// =============================================================================================
template <class T, std::size_t N>
class SmallVector {
    static_assert(N > 0);
    alignas(T) std::byte inline_[N * sizeof(T)];   // raw storage; NO T objects live here until constructed
    T* data_;
    std::size_t size_ = 0;
    std::size_t cap_ = N;

    T* inline_ptr() noexcept { return std::launder(reinterpret_cast<T*>(inline_)); }
    const T* inline_ptr() const noexcept { return std::launder(reinterpret_cast<const T*>(inline_)); }

    static T* allocate(std::size_t n) {
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
        else
            return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    static void deallocate(T* p) noexcept {
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            ::operator delete(p, std::align_val_t{alignof(T)});
        else
            ::operator delete(p);
    }

    void grow(std::size_t new_cap) {
        T* fresh = allocate(new_cap);
        try {
            // Strong exception guarantee: move only if the move can't throw, else copy so that the
            // old buffer stays intact if a copy throws (same rule std::vector uses).
            if constexpr (std::is_nothrow_move_constructible_v<T>)
                std::uninitialized_move_n(data_, size_, fresh);
            else
                std::uninitialized_copy_n(data_, size_, fresh);
        } catch (...) {
            deallocate(fresh);
            throw;
        }
        std::destroy_n(data_, size_);
        if (!is_inline()) deallocate(data_);
        data_ = fresh;
        cap_ = new_cap;
    }

public:
    SmallVector() noexcept : data_(inline_ptr()) {}
    ~SmallVector() {
        std::destroy_n(data_, size_);
        if (!is_inline()) deallocate(data_);
    }
    SmallVector(const SmallVector& o) : data_(inline_ptr()) {
        if (o.size_ > N) { data_ = allocate(o.size_); cap_ = o.size_; }
        try {
            std::uninitialized_copy_n(o.data_, o.size_, data_);
        } catch (...) {
            if (!is_inline()) deallocate(data_);
            throw;
        }
        size_ = o.size_;
    }
    SmallVector(SmallVector&& o) noexcept(std::is_nothrow_move_constructible_v<T>) : data_(inline_ptr()) {
        if (o.is_inline()) {
            std::uninitialized_move_n(o.data_, o.size_, data_);   // must move element-wise
            std::destroy_n(o.data_, o.size_);
        } else {
            data_ = o.data_; cap_ = o.cap_;                       // steal the heap buffer
            o.data_ = o.inline_ptr(); o.cap_ = N;
        }
        size_ = o.size_; o.size_ = 0;
    }
    SmallVector& operator=(SmallVector o) noexcept(std::is_nothrow_move_constructible_v<T>) {  // copy-and-swap
        swap(o); return *this;
    }
    // swap via three moves. Inline buffers can't be swapped by pointer, so this is O(N) when
    // both sides are inline; the destroy + placement-new pairs end and restart lifetimes in place.
    void swap(SmallVector& o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        SmallVector tmp(std::move(o));
        o.~SmallVector();     ::new (static_cast<void*>(&o)) SmallVector(std::move(*this));
        this->~SmallVector(); ::new (static_cast<void*>(this)) SmallVector(std::move(tmp));
    }

    bool is_inline() const noexcept { return data_ == inline_ptr(); }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return cap_; }
    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    T& operator[](std::size_t i) noexcept { assert(i < size_); return data_[i]; }
    const T& operator[](std::size_t i) const noexcept { assert(i < size_); return data_[i]; }
    T* begin() noexcept { return data_; }
    T* end() noexcept { return data_ + size_; }

    template <class... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == cap_) grow(cap_ * 2);
        T* p = std::construct_at(data_ + size_, std::forward<Args>(args)...);
        ++size_;
        return *p;
    }
    void push_back(const T& v) { emplace_back(v); }
    void push_back(T&& v) { emplace_back(std::move(v)); }
    void pop_back() noexcept { assert(size_ > 0); std::destroy_at(data_ + --size_); }
    void clear() noexcept { std::destroy_n(data_, size_); size_ = 0; }
};

static void section5_small_vector() {
    std::puts("\n=== 5. SmallVector<T, N> ===");
    auto before = alloc_stats::snapshot();
    {
        SmallVector<Tracked, 4> sv;                            // a shape/stride vector: usually <= 4 dims
        assert(sv.is_inline() && sv.size() == 0 && sv.capacity() == 4);
        for (int i = 0; i < 4; ++i) sv.emplace_back(i);
        assert(sv.is_inline() && sv.size() == 4 && Tracked::alive == 4);
        std::printf("  4 elements: inline=%s, heap allocations so far=%zu\n",
                    sv.is_inline() ? "yes" : "no", alloc_stats::g_new_calls - before.calls);

        sv.emplace_back(4);                                     // 5th element spills to the heap
        assert(!sv.is_inline() && sv.size() == 5 && sv.capacity() == 8 && Tracked::alive == 5);
        std::printf("  5 elements: inline=%s, capacity=%zu, heap allocations=%zu, alive=%d\n",
                    sv.is_inline() ? "yes" : "no", sv.capacity(),
                    alloc_stats::g_new_calls - before.calls, Tracked::alive);
        for (std::size_t i = 0; i < sv.size(); ++i) assert(sv[i].id == static_cast<int>(i));

        SmallVector<Tracked, 4> moved(std::move(sv));           // steals heap buffer, O(1)
        assert(moved.size() == 5 && sv.size() == 0 && sv.is_inline() && Tracked::alive == 5);

        SmallVector<Tracked, 4> copy(moved);                    // deep copy
        assert(copy.size() == 5 && Tracked::alive == 10);
        copy.pop_back(); copy.pop_back();
        assert(copy.size() == 3 && Tracked::alive == 8);

        SmallVector<Tracked, 4> small;                          // inline (2 elements) <-> heap (5)
        small.emplace_back(100); small.emplace_back(101);
        small = moved;                                          // copy-assign via copy-and-swap
        assert(small.size() == 5 && !small.is_inline() && moved.size() == 5 && Tracked::alive == 13);
        assert(small[4].id == 4);
        small = SmallVector<Tracked, 4>();                      // move-assign an empty inline one
        assert(small.size() == 0 && small.is_inline() && Tracked::alive == 8);
        std::puts("  move steals the heap buffer; copy deep-copies; assignment swaps; pop_back destroys — all asserts passed");
    }
    assert(Tracked::alive == 0);                                // every constructed object destroyed
    alloc_stats::report("SmallVector section total", before);
    assert(alloc_stats::g_new_calls - before.calls == alloc_stats::g_delete_calls - before.frees);
}

// =============================================================================================
// §6  Exception safety when constructing in raw memory
// =============================================================================================
struct Throwy {
    static inline int constructed = 0, destroyed = 0, throw_on = -1;
    int v;
    explicit Throwy(int x) : v(x) { ++constructed; }
    Throwy(const Throwy& o) : v(o.v) {
        if (constructed == throw_on) throw std::runtime_error("copy failed");
        ++constructed;
    }
    ~Throwy() { ++destroyed; }
};

static void section6_exception_safety() {
    std::puts("\n=== 6. exception safety in raw memory ===");
    std::vector<Throwy> src;
    for (int i = 0; i < 6; ++i) src.emplace_back(i);
    Throwy::constructed = 0; Throwy::destroyed = 0;

    alignas(Throwy) std::byte raw[6 * sizeof(Throwy)];
    Throwy* dst = reinterpret_cast<Throwy*>(raw);
    Throwy::throw_on = 3;                                       // the 4th copy throws
    try {
        // std::uninitialized_copy guarantees: if a constructor throws, every element already
        // constructed in [dst, current) is destroyed before the exception propagates.
        std::uninitialized_copy(src.begin(), src.end(), dst);
        std::puts("  (unreachable)");
    } catch (const std::exception& e) {
        std::printf("  caught '%s': constructed=%d destroyed=%d  (no leaks, no half-built objects)\n",
                    e.what(), Throwy::constructed, Throwy::destroyed);
        assert(Throwy::constructed == Throwy::destroyed);
    }
    Throwy::throw_on = -1;

    // The hand-rolled equivalent — what you must write if you loop yourself:
    std::size_t built = 0;
    try {
        for (; built < src.size(); ++built) std::construct_at(dst + built, src[built]);
        std::destroy_n(dst, built);
        std::puts("  manual loop: constructed all, then destroyed all");
    } catch (...) {
        std::destroy_n(dst, built);                             // unwind exactly what was built
        throw;
    }
}

// =============================================================================================
// §7  Counting allocations to find hidden copies
// =============================================================================================
struct Mat {
    std::size_t r, c;
    std::vector<double> d;
    Mat(std::size_t r_, std::size_t c_) : r(r_), c(c_), d(r_ * c_) {}
    double& operator()(std::size_t i, std::size_t j) { return d[i * c + j]; }
    double operator()(std::size_t i, std::size_t j) const { return d[i * c + j]; }
};
static Mat add_by_value(Mat a, Mat b) {                        // BUG: copies both arguments
    for (std::size_t i = 0; i < a.d.size(); ++i) a.d[i] += b.d[i];
    return a;
}
static Mat add_by_cref(const Mat& a, const Mat& b) {           // one allocation for the result
    Mat out(a.r, a.c);
    for (std::size_t i = 0; i < a.d.size(); ++i) out.d[i] = a.d[i] + b.d[i];
    return out;
}
static void add_into(const Mat& a, const Mat& b, Mat& out) {   // zero allocations
    for (std::size_t i = 0; i < a.d.size(); ++i) out.d[i] = a.d[i] + b.d[i];
}

// A counting allocator: same idea, scoped to one container type instead of the whole program.
template <class T>
struct CountingAllocator {
    using value_type = T;
    static inline std::size_t allocs = 0, bytes = 0;
    CountingAllocator() = default;
    template <class U> CountingAllocator(const CountingAllocator<U>&) noexcept {}
    T* allocate(std::size_t n) { ++allocs; bytes += n * sizeof(T); return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) noexcept { std::allocator<T>{}.deallocate(p, n); }
    template <class U> bool operator==(const CountingAllocator<U>&) const noexcept { return true; }
    template <class U> bool operator!=(const CountingAllocator<U>&) const noexcept { return false; }
};

static void section7_counting() {
    std::puts("\n=== 7. counting allocations: hidden copies ===");
    Mat a(64, 64), b(64, 64), out(64, 64);
    auto s = alloc_stats::snapshot();
    for (int k = 0; k < 10; ++k) { Mat r = add_by_value(a, b); do_not_optimize(r.d.data()); }
    alloc_stats::report("10x add_by_value (copies a AND b)", s);
    s = alloc_stats::snapshot();
    for (int k = 0; k < 10; ++k) { Mat r = add_by_cref(a, b); do_not_optimize(r.d.data()); }
    alloc_stats::report("10x add_by_cref (result only)", s);
    s = alloc_stats::snapshot();
    for (int k = 0; k < 10; ++k) { add_into(a, b, out); do_not_optimize(out.d.data()); }
    alloc_stats::report("10x add_into (none)", s);

    // The range-for copy trap, caught the same way:
    std::vector<std::vector<double>> rows(100, std::vector<double>(8, 1.0));
    s = alloc_stats::snapshot();
    double sum = 0;
    for (auto row : rows) sum += row[0];                       // `auto` copies every row!
    alloc_stats::report("for (auto row : rows)  -- copies", s);
    s = alloc_stats::snapshot();
    for (const auto& row : rows) sum += row[0];
    alloc_stats::report("for (const auto& row : rows)", s);
    do_not_optimize(sum);

    std::vector<double, CountingAllocator<double>> cv;
    for (int i = 0; i < 1000; ++i) cv.push_back(i);           // geometric growth: ~11 allocations
    std::printf("  CountingAllocator: vector grew to %zu via %zu allocations (%zu bytes total)\n",
                cv.size(), CountingAllocator<double>::allocs, CountingAllocator<double>::bytes);
    CountingAllocator<double>::allocs = 0; CountingAllocator<double>::bytes = 0;
    std::vector<double, CountingAllocator<double>> cv2;
    cv2.reserve(1000);
    for (int i = 0; i < 1000; ++i) cv2.push_back(i);
    std::printf("  with reserve(1000): %zu allocation(s)\n", CountingAllocator<double>::allocs);
}

// =============================================================================================
// §8  Object representation: bit_cast, memcpy, std::byte, span<std::byte>
// =============================================================================================
struct Header { std::uint32_t magic; std::uint32_t rows; std::uint32_t cols; std::uint32_t dtype; };
static_assert(std::is_trivially_copyable_v<Header> && sizeof(Header) == 16);

// Serialize a trivially-copyable object into a byte buffer. std::byte is the C++17 "just bytes"
// type: no arithmetic, only bitwise ops, and (with unsigned char / char) the only type allowed to
// alias any object's representation ([basic.lval]/11).
template <class T>
static void write_pod(std::vector<std::byte>& out, const T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto bytes = std::as_bytes(std::span<const T, 1>(&v, 1));   // span<const std::byte, sizeof(T)>
    out.insert(out.end(), bytes.begin(), bytes.end());
}
template <class T>
static T read_pod(std::span<const std::byte>& in) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (in.size() < sizeof(T)) throw std::runtime_error("truncated");
    T v;
    std::memcpy(&v, in.data(), sizeof v);                       // memcpy: the sanctioned type pun
    in = in.subspan(sizeof(T));
    return v;
}

static void section8_representation() {
    std::puts("\n=== 8. object representation and serialization ===");
    // bit_cast: reinterpret the bits of one trivially-copyable type as another. Constexpr,
    // no UB, no strict-aliasing question. `*(uint32_t*)&f` is UB in C++ (unlike C's union trick).
    float f = 1.0f;
    auto bits = std::bit_cast<std::uint32_t>(f);
    std::printf("  bit_cast<uint32_t>(1.0f) = 0x%08x  (sign 0, exp 127, mantissa 0)\n", bits);
    std::printf("  endianness: %s\n", std::endian::native == std::endian::little ? "little" : "big");

    // The fast-inverse-sqrt trick, legally:
    auto q_rsqrt = [](float x) {
        auto i = std::bit_cast<std::uint32_t>(x);
        i = 0x5f3759dfu - (i >> 1);
        float y = std::bit_cast<float>(i);
        return y * (1.5f - 0.5f * x * y * y);                   // one Newton step
    };
    std::printf("  q_rsqrt(4.0f) = %.5f  (exact 0.5)\n", q_rsqrt(4.0f));

    std::vector<std::byte> blob;
    Header h{0x4d415431u, 2, 3, 1};                             // "1TAM" little-endian, 2x3, dtype 1
    std::vector<float> payload{1, 2, 3, 4, 5, 6};
    write_pod(blob, h);
    auto pb = std::as_bytes(std::span<const float>(payload));
    blob.insert(blob.end(), pb.begin(), pb.end());
    std::printf("  serialized %zu bytes: header %zu + %zu floats\n", blob.size(), sizeof h, payload.size());

    std::span<const std::byte> in(blob);
    Header h2 = read_pod<Header>(in);
    std::vector<float> back(h2.rows * h2.cols);
    std::memcpy(back.data(), in.data(), back.size() * sizeof(float));
    std::printf("  deserialized: magic=0x%08x %ux%u first=%g last=%g\n", h2.magic, h2.rows, h2.cols,
                back.front(), back.back());
    assert(back == payload);
}

// =============================================================================================
// §9  unique_ptr with custom deleters; RAII for OS handles
// =============================================================================================
struct FileCloser { void operator()(std::FILE* f) const noexcept { if (f) std::fclose(f); } };
using unique_file = std::unique_ptr<std::FILE, FileCloser>;    // sizeof == sizeof(FILE*): stateless deleter

struct MunmapDeleter {                                          // stateful deleter: needs the length
    std::size_t len;
    void operator()(void* p) const noexcept { if (p && p != MAP_FAILED) munmap(p, len); }
};
using unique_mmap = std::unique_ptr<void, MunmapDeleter>;

class Fd {                                                      // hand-written RAII for an int handle
    int fd_ = -1;
public:
    Fd() = default;
    explicit Fd(int fd) noexcept : fd_(fd) {}
    ~Fd() { reset(); }
    Fd(Fd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    Fd& operator=(Fd&& o) noexcept { if (this != &o) { reset(); fd_ = std::exchange(o.fd_, -1); } return *this; }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    void reset() noexcept { if (fd_ >= 0) ::close(fd_); fd_ = -1; }
    int get() const noexcept { return fd_; }
    explicit operator bool() const noexcept { return fd_ >= 0; }
};

// A CUDA-style deleter, shown as a stub (no CUDA on this machine): the pattern is identical.
// struct CudaFree { void operator()(float* p) const noexcept { cudaFree(p); } };
// using device_ptr = std::unique_ptr<float[], CudaFree>;

static void section9_deleters() {
    std::puts("\n=== 9. custom deleters and OS handles ===");
    static_assert(sizeof(unique_file) == sizeof(std::FILE*), "stateless deleter adds no size");
    static_assert(sizeof(unique_mmap) == sizeof(void*) + sizeof(std::size_t));

    unique_file f(std::tmpfile());                              // anonymous temp file, no path needed
    if (f) {
        std::fputs("hello\n", f.get());
        std::fflush(f.get());
        std::printf("  tmpfile via unique_ptr<FILE, FileCloser>: wrote 6 bytes, closes on scope exit\n");
    } else {
        std::puts("  tmpfile() failed (sandbox?) — skipping");
    }

    const std::size_t len = 1 << 16;
    void* m = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m != MAP_FAILED) {
        unique_mmap region(m, MunmapDeleter{len});
        auto* dbl = static_cast<double*>(region.get());        // implicit-lifetime: doubles exist
        dbl[0] = 3.5; dbl[(len / sizeof(double)) - 1] = 4.5;
        std::printf("  mmap'd 64 KB via unique_ptr<void, MunmapDeleter>: first=%g last=%g\n",
                    dbl[0], dbl[(len / sizeof(double)) - 1]);
    } else {
        std::puts("  mmap failed (sandbox?) — skipping");
    }

    Fd fd(::open("/dev/null", O_RDONLY));
    std::printf("  Fd(open(\"/dev/null\")) = %d, valid = %s; closed automatically\n", fd.get(), fd ? "yes" : "no");
}

// =============================================================================================
// §10  Node pool for tree/graph nodes (Barnes–Hut cells, autograd nodes)
// =============================================================================================
struct Cell {                                                   // a Barnes–Hut quadtree cell
    double cx, cy, mass;
    Cell* child[4];
    int depth;
};
static_assert(sizeof(Cell) == 64);

// Fixed-size free-list pool: O(1) alloc/free, no per-object malloc header, contiguous chunks.
template <class T, std::size_t ChunkObjects = 4096>
class NodePool {
    union Slot { alignas(T) std::byte storage[sizeof(T)]; Slot* next; };
    std::vector<std::unique_ptr<Slot[]>> chunks_;
    Slot* free_ = nullptr;

    void refill() {
        auto chunk = std::make_unique<Slot[]>(ChunkObjects);
        for (std::size_t i = 0; i < ChunkObjects; ++i) {         // thread every slot onto the free list
            chunk[i].next = free_;
            free_ = &chunk[i];
        }
        chunks_.push_back(std::move(chunk));
    }
public:
    template <class... Args>
    T* create(Args&&... args) {
        if (!free_) refill();
        Slot* s = free_;
        free_ = s->next;                                         // read the `next` member ...
        return std::construct_at(reinterpret_cast<T*>(s->storage), std::forward<Args>(args)...);  // ... then reuse storage
    }
    void destroy(T* p) noexcept {
        std::destroy_at(p);
        Slot* s = std::launder(reinterpret_cast<Slot*>(p));      // storage reused as Slot again
        s->next = free_;
        free_ = s;
    }
    std::size_t chunks() const noexcept { return chunks_.size(); }
};

static Cell* build_tree_new(int depth, int& count) {
    Cell* c = new Cell{0, 0, 1, {nullptr, nullptr, nullptr, nullptr}, depth};
    ++count;
    if (depth > 0) for (auto& ch : c->child) ch = build_tree_new(depth - 1, count);
    return c;
}
static void free_tree_new(Cell* c) {
    if (!c) return;
    for (auto* ch : c->child) free_tree_new(ch);
    delete c;
}
static Cell* build_tree_pool(NodePool<Cell>& pool, int depth, int& count) {
    Cell* c = pool.create(Cell{0, 0, 1, {nullptr, nullptr, nullptr, nullptr}, depth});
    ++count;
    if (depth > 0) for (auto& ch : c->child) ch = build_tree_pool(pool, depth - 1, count);
    return c;
}
static void free_tree_pool(NodePool<Cell>& pool, Cell* c) {
    if (!c) return;
    for (auto* ch : c->child) free_tree_pool(pool, ch);
    pool.destroy(c);
}
static double sum_mass(const Cell* c) {
    if (!c) return 0;
    double s = c->mass;
    for (auto* ch : c->child) s += sum_mass(ch);
    return s;
}

static void section10_node_pool() {
    std::puts("\n=== 10. node pool for tree nodes ===");
    const int depth = 8;                                         // 4^0 + ... + 4^8 = 87381 cells
    int n_new = 0, n_pool = 0;
    double sink = 0;
    double t_new = time_min_ms(5, [&] {
        n_new = 0;
        Cell* root = build_tree_new(depth, n_new);
        sink += sum_mass(root);
        free_tree_new(root);
    });
    NodePool<Cell> pool;
    double t_pool = time_min_ms(5, [&] {
        n_pool = 0;
        Cell* root = build_tree_pool(pool, depth, n_pool);
        sink += sum_mass(root);
        free_tree_pool(pool, root);                              // slots go back to the free list
    });
    do_not_optimize(sink);
    assert(n_new == n_pool);
    std::printf("  build+traverse+free %d cells:  new/delete %.3f ms   NodePool %.3f ms   (%.1fx), pool chunks=%zu\n",
                n_new, t_new, t_pool, t_new / t_pool, pool.chunks());
    std::puts("  (an autograd graph does exactly this every training step: allocate N nodes, backward, free all)");
}

// =============================================================================================
// §11  Tensor ownership: refcounted Storage + TensorImpl with strides (PyTorch's design)
// =============================================================================================
struct Storage {                                                 // owns the bytes; shared by views
    std::vector<float> data;
    explicit Storage(std::size_t n) : data(n) {}
};

class Tensor {                                                   // == TensorImpl: shape + strides + offset
    std::shared_ptr<Storage> storage_;
    std::size_t offset_ = 0;
    SmallVector<std::size_t, 4> shape_, strides_;                 // §5 in use: no heap for <= 4 dims
public:
    Tensor(std::size_t rows, std::size_t cols)
        : storage_(std::make_shared<Storage>(rows * cols)) {
        shape_.push_back(rows); shape_.push_back(cols);
        strides_.push_back(cols); strides_.push_back(1);          // row-major
    }
    // A view: same storage, different metadata. No element is copied.
    Tensor(std::shared_ptr<Storage> s, std::size_t off, std::initializer_list<std::size_t> shape,
           std::initializer_list<std::size_t> strides)
        : storage_(std::move(s)), offset_(off) {
        for (auto x : shape) shape_.push_back(x);
        for (auto x : strides) strides_.push_back(x);
    }
    float& operator()(std::size_t i, std::size_t j) {
        return storage_->data[offset_ + i * strides_[0] + j * strides_[1]];
    }
    Tensor transpose() const { return Tensor(storage_, offset_, {shape_[1], shape_[0]}, {strides_[1], strides_[0]}); }
    Tensor row(std::size_t i) const { return Tensor(storage_, offset_ + i * strides_[0], {1, shape_[1]}, {strides_[0], strides_[1]}); }
    bool is_contiguous() const { return strides_[1] == 1 && strides_[0] == shape_[1]; }
    long use_count() const { return storage_.use_count(); }
    std::size_t rows() const { return shape_[0]; }
    std::size_t cols() const { return shape_[1]; }
    Tensor contiguous() const {                                  // materialize a copy in row-major order
        Tensor out(rows(), cols());
        for (std::size_t i = 0; i < rows(); ++i)
            for (std::size_t j = 0; j < cols(); ++j)
                out(i, j) = const_cast<Tensor&>(*this)(i, j);
        return out;
    }
};

static void section11_tensor_ownership() {
    std::puts("\n=== 11. tensor ownership: Storage + views ===");
    auto s = alloc_stats::snapshot();
    Tensor a(2, 3);
    for (std::size_t i = 0; i < 2; ++i) for (std::size_t j = 0; j < 3; ++j) a(i, j) = static_cast<float>(i * 3 + j);
    Tensor t = a.transpose();                                    // shares storage
    Tensor r = a.row(1);
    alloc_stats::report("Tensor(2,3) + transpose + row view", s);
    std::printf("  storage use_count = %ld (a, t, r)\n", a.use_count());
    std::printf("  a(0,1)=%g  t(1,0)=%g  same element? %s;  t contiguous? %s\n", a(0, 1), t(1, 0),
                &a(0, 1) == &t(1, 0) ? "yes" : "no", t.is_contiguous() ? "yes" : "no");
    t(1, 0) = 42.f;                                              // writing through the view
    std::printf("  after t(1,0)=42: a(0,1)=%g  r(0,0)=%g\n", a(0, 1), r(0, 0));
    Tensor c = t.contiguous();
    std::printf("  t.contiguous(): rows=%zu cols=%zu contiguous=%s, use_count of a's storage still %ld\n",
                c.rows(), c.cols(), c.is_contiguous() ? "yes" : "no", a.use_count());
    std::puts("  (this is torch.Tensor.t() / .contiguous(): a view costs metadata, never a copy)");
}

// =============================================================================================
int main() {
    std::puts("Chapter 16 — object lifetime, allocators, memory  (compiled with -std=c++20)");
    section1_lifetime();
    section2_alignment_layout();
    section3_arena_allocator();
    section4_pmr();
    section5_small_vector();
    section6_exception_safety();
    section7_counting();
    section8_representation();
    section9_deleters();
    section10_node_pool();
    section11_tensor_ownership();
    std::printf("\nprogram total: %zu operator new calls, %zu operator delete calls\n",
                alloc_stats::g_new_calls, alloc_stats::g_delete_calls);
    return 0;
}
