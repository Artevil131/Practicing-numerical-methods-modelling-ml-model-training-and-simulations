# Chapter 04 — `std::vector`, `std::string`, and the Standard Containers

## What you'll be able to do after this chapter

- Use `std::vector<T>` as your default dynamic array — construction, growth, `reserve`, `emplace_back`, `data()` for C interop — and explain why a pointer into it can go stale.
- Rebuild the chapter 02 `Matrix` on top of a flat `std::vector<double>` with zero special member functions (Rule of Zero), and argue why flat beats `vector<vector<double>>`.
- Do the string manipulation a tokenizer needs with `std::string` / `std::string_view`: split, find, substr, byte iteration, numeric conversion.
- Pick the right container — `map`, `unordered_map`, `set`, `deque`, `priority_queue`, `pair`/`tuple` — from a complexity table, and write a custom hash for `unordered_map<pair<int,int>, int>` (BPE pair counts).
- Read and write iterator-based code (`begin/end`, `auto it`, `std::distance`) and call `<algorithm>` functions like `std::sort`, `std::max_element`, `std::accumulate`.

## Why this matters for ML / numerics / sims

In the C course you wrote `IntVec` (a growable array), a hash table, and a handful of linked structures by hand, each with its own memory management and its own bugs. The C++ standard library ships all of them, tested, generic, and RAII-managed. Your BPE tokenizer is `std::string` + `std::unordered_map<std::pair<int,int>, int>` + `std::vector<int>`. Your matrix library is a `std::vector<double>` with an index formula. Your N-body sim is a `std::vector<Body>`. Your priority-based event scheduler is a `std::priority_queue`. Learning these containers well means the data structures chapter of every future project is already done.

---

## 1. `std::vector<T>`: construction

```cpp
#include <vector>
std::vector<double> a;                 // empty, size 0
std::vector<double> b(5);              // 5 elements, value-initialized (0.0)
std::vector<double> c(5, 1.5);         // 5 copies of 1.5
std::vector<double> d{1.0, 2.0, 3.0};  // 3 elements from an initializer list
std::vector<double> e(d);              // deep copy of d (value semantics!)
std::vector<double> f(d.begin(), d.begin() + 2);   // from an iterator range: {1.0, 2.0}
std::vector<std::vector<int>> grid(3, std::vector<int>(4, 0));   // 3 rows of 4 zeros (see section 6 for why not)
```

**Trap**: `std::vector<int> v(5);` is five zeros; `std::vector<int> v{5};` is one element equal to 5. Braces pick the initializer-list constructor whenever one exists.

A vector is three words: pointer to heap buffer, size, capacity. `sizeof(std::vector<double>) == 24`. Copying a vector copies the buffer (O(n), allocation); the vector frees its buffer in its destructor. It is the RAII `IntVec` from the C course, for any `T`.

```
stack                heap
+----------+
| data  ---+------> [1.0][2.0][3.0][ ? ][ ? ][ ? ][ ? ][ ? ]
| size = 3 |         <-------- capacity = 8 ------------->
| cap  = 8 |
+----------+
```

Python equivalent: `list`, but homogeneous and contiguous like a NumPy 1-D array.

## 2. `std::vector<T>`: the core API

| Call | Effect | Complexity |
|---|---|---|
| `v.size()` | number of elements (`std::size_t`) | O(1) |
| `v.empty()` | `size() == 0` | O(1) |
| `v.capacity()` | elements the buffer can hold before reallocating | O(1) |
| `v.reserve(n)` | ensure `capacity() >= n`; does **not** change `size()` | O(n) once |
| `v.resize(n)` / `v.resize(n, val)` | set `size()` to `n`, value-initializing (or `val`) new elements | O(n) |
| `v.push_back(x)` | append a copy of `x` | amortized O(1) |
| `v.emplace_back(args...)` | construct a new element in place from `args` | amortized O(1) |
| `v.pop_back()` | remove the last element (UB if empty) | O(1) |
| `v.back()` / `v.front()` | reference to last / first (UB if empty) | O(1) |
| `v[i]` | unchecked element access; UB if `i >= size()` | O(1) |
| `v.at(i)` | checked access; throws `std::out_of_range` | O(1) |
| `v.data()` | `T*` to the buffer — for C APIs and BLAS | O(1) |
| `v.clear()` | `size()` becomes 0; capacity unchanged | O(n) destructors |
| `v.insert(pos, x)` / `v.erase(pos)` | shift everything after `pos` | O(n) |
| `v.shrink_to_fit()` | request `capacity() == size()` | O(n) |

```cpp
std::vector<double> v;
v.reserve(1000);                          // one allocation up front
for (int i = 0; i < 1000; i++) v.push_back(i * 0.5);
std::cout << v.size() << ' ' << v.capacity() << ' ' << v.back() << '\n';   // 1000 1000 499.5
v.resize(3);                              // {0, 0.5, 1}; capacity still 1000
v[1] = 9.0;                               // unchecked
// v.at(10);                              // throws std::out_of_range
double *raw = v.data();                   // pass to a C function: c_style_sum(raw, (int)v.size())
```

`operator[]` vs `.at()`: `[]` is what you use in inner loops (same speed as a C array); `.at()` is for input validation or debugging. With `-D_GLIBCXX_ASSERTIONS` (libstdc++) or `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE` (libc++, macOS) `[]` becomes checked too — useful in debug builds.

`emplace_back(2, 3, 0.0)` constructs a `Matrix(2, 3, 0.0)` directly in the vector's storage; `push_back(Matrix(2, 3, 0.0))` constructs a temporary, then moves/copies it in. For scalars there is no difference; for heavy objects prefer `emplace_back`.

## 3. Growth strategy

When `size() == capacity()` and you `push_back`, the vector allocates a bigger buffer (libc++ doubles, libstdc++ doubles, MSVC ×1.5), copies/moves every element over, and frees the old buffer. Doubling makes n pushes cost O(n) total — amortized O(1) each — exactly the analysis you did for `IntVec`. Consequences:

- Growing one element at a time without `reserve` costs about 2× the final size in copies and ~log₂(n) allocations. Fine for a few thousand elements; for a 10⁸-element particle array, `reserve` first.
- A `std::vector<double>` of 1000 elements might hold 1024 slots. `shrink_to_fit()` if the memory matters.
- After a reallocation, **every pointer, reference, and iterator into the vector is invalid** (section 4).

## 4. Iterator invalidation on `push_back` — the classic bug

```cpp
std::vector<double> v{1, 2, 3};
double &first = v[0];             // reference into the buffer
v.push_back(4);                   // may reallocate: buffer moves, `first` now dangles
std::cout << first;               // UB. Often prints 1 anyway; sometimes garbage; ASan: heap-use-after-free

for (auto it = v.begin(); it != v.end(); ++it)
    if (*it > 2) v.push_back(*it * 10);     // UB: push_back invalidates `it` and `end()`

for (std::size_t i = 0; i < v.size(); i++)
    if (v[i] > 2) v.push_back(v[i] * 10);   // OK-ish (indices survive reallocation) — but v[i] as the ARGUMENT
                                            // is a reference that push_back may invalidate mid-call. Copy first:
                                            // double x = v[i]; v.push_back(x * 10);
```

Rules: `push_back`, `emplace_back`, `insert`, `resize`, `reserve` may invalidate everything if capacity changes. `erase`/`insert` invalidate everything at or after the position even without reallocation. `pop_back` invalidates only `back()` and `end()`. Indices are always safe. If you must hold references across growth, `reserve` the final size first — then `push_back` never reallocates until capacity is hit, and the standard guarantees references stay valid.

This is the same bug as holding a pointer into `IntVec.data` across `intvec_push` in C. The container is just more convenient about hiding it.

## 5. `vector<vector<double>>` vs a flat vector for matrices

```cpp
std::vector<std::vector<double>> nested(rows, std::vector<double>(cols));   // rows+1 allocations, rows separate buffers
std::vector<double> flat(rows * cols);                                        // 1 allocation, contiguous
double x = nested[i][j];
double y = flat[i * cols + j];
```

```
nested:                            flat:
[ptr][ptr][ptr]  -> row0 [..cols..]  [row0 ......][row1 ......][row2 ......]
                 -> row1 [..cols..]   (one block; row-major; index = i*cols + j)
                 -> row2 [..cols..]
```

Flat wins on every axis that matters for numerics: one allocation instead of `rows + 1`; contiguous memory, so iterating the whole matrix is cache-friendly and vectorizable; a single `data()` pointer to hand to BLAS, `fwrite`, or your C matmul; trivial transposition/reshape by changing the index formula; no way for rows to accidentally have different lengths. `nested` wins only on syntax (`m[i][j]`), and `operator()(i, j)` gets you that back. NumPy's `ndarray` is flat storage plus strides; do the same.

## 6. Running example: `Matrix` over `std::vector<double>` — the Rule of Zero

```cpp
class Matrix {
public:
    Matrix() = default;
    Matrix(int rows, int cols, double fill = 0.0)
        : rows_(rows), cols_(cols), data_(static_cast<std::size_t>(rows) * cols, fill) {}

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    double &operator()(int i, int j)       { return data_[static_cast<std::size_t>(i) * cols_ + j]; }
    double  operator()(int i, int j) const { return data_[static_cast<std::size_t>(i) * cols_ + j]; }
    double       *data()       { return data_.data(); }
    const double *data() const { return data_.data(); }

    // NO destructor, NO copy constructor, NO copy assignment. The vector handles all of it.
private:
    int rows_ = 0, cols_ = 0;
    std::vector<double> data_;
};
```

Compare with chapter 02: same interface, 40 fewer lines, no `new`, no `delete[]`, no double-free possible, deep copies for free, and deep `const` (on a `const Matrix`, `data_[k]` is `const double&`). This is the version to build on. `operator()(i, j)` is the conventional matrix index in C++ (`m(i, j)`) since `operator[]` takes one argument before C++23.

## 7. `std::string`

`std::string` is `std::vector<char>` with string conveniences and a guaranteed terminating `'\0'` after `size()` bytes (so `c_str()` is always valid).

```cpp
#include <string>
std::string s = "hello";
s += " world";                       // append (also s.append("..."), s.push_back('!'))
std::string t = s + "!";             // new string
s.size();                            // 11; s.length() is a synonym
s.empty();
s[0];                                // 'h' — char&, unchecked;  s.at(0) checked
s.front(); s.back();
s.substr(6);                         // "world"       (pos)
s.substr(0, 5);                      // "hello"       (pos, count)
s.find("wor");                       // 6             (std::string::npos if absent)
s.find(' ');                         // 5
s.rfind('o');                        // 7
s.compare("hello");                  // <0, 0, >0 like strcmp;  s == "x", s < "y" also work
s.c_str();                           // const char* (valid until s is modified)
s.data();                            // char* since C++17
s.clear(); s.reserve(1024);
std::string rep(5, '-');             // "-----"
```

Numeric conversion:

```cpp
std::to_string(42);        // "42"
std::to_string(3.5);       // "3.500000"  (fixed %f formatting; use a stringstream or printf for control)
std::stod("2.718");        // 2.718        (std::stoi, std::stol, std::stof, std::stoll ...)
std::size_t consumed;
double v = std::stod("1.5abc", &consumed);   // v = 1.5, consumed = 3 -> detect trailing junk
// std::stod("abc") throws std::invalid_argument; out-of-range throws std::out_of_range
```

Line-based input, the way you read a corpus or a CSV:

```cpp
std::string line;
while (std::getline(std::cin, line)) {          // reads up to '\n', drops the '\n'; false at EOF
    if (line.empty()) continue;
    // split on ','
    std::size_t start = 0;
    while (true) {
        std::size_t comma = line.find(',', start);
        std::string field = line.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        // ... use field ...
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
}
```

Iterating bytes — what a byte-level BPE tokenizer does:

```cpp
std::string word = "héllo";                      // UTF-8: 6 bytes, 5 code points
for (unsigned char b : word) std::cout << static_cast<int>(b) << ' ';   // 104 195 169 108 108 111
std::vector<int> ids(word.begin(), word.end()); // each byte becomes an int token 0..255 — (signed char! see gotchas)
```

Python equivalent: `str` is Unicode; `std::string` is `bytes`. `"héllo".encode()` is the C++ string. There is no built-in UTF-8 decoding; for BPE you do not need one — GPT-style tokenizers work on bytes on purpose.

`std::string_view` (C++17, `<string_view>`) is a non-owning pointer + length into someone else's characters. Use it for function parameters that only read a string: `void tokenize(std::string_view text)` accepts `std::string`, `const char*`, and literals without copying, and `sv.substr(a, b)` is O(1) with no allocation. It dangles if the underlying string dies or reallocates — same rules as a reference. `std::span<T>` (C++20) is the same idea for arrays; in C++17 pass `(const T*, size)` or `const std::vector<T>&`.

## 8. `std::array<T, N>`

Fixed size, stack-allocated, zero overhead over `T[N]`, but copyable, comparable, with `.size()`, `.data()`, iterators, and no pointer decay. The right type for small fixed vectors:

```cpp
#include <array>
using Vec3 = std::array<double, 3>;
Vec3 a{1, 2, 3}, b{4, 5, 6};
Vec3 c;
for (std::size_t i = 0; i < 3; i++) c[i] = a[i] + b[i];
std::array<std::array<double, 3>, 3> rot{};     // 3x3 zeros; std::array nests fine because it is not heap-allocated
```

Choose `std::array` when the size is a compile-time constant and small (say ≤ a few KB — it lives on the stack). Otherwise `std::vector`.

## 9. `std::pair`, `std::tuple`, structured bindings

```cpp
#include <utility>   // pair
#include <tuple>
std::pair<int, double> p{3, 0.5};          // p.first == 3, p.second == 0.5
auto q = std::make_pair(1, 2);             // pair<int,int>
std::tuple<int, double, std::string> t{1, 2.0, "three"};
std::get<0>(t); std::get<2>(t);            // by index (compile-time)

auto [idx, val] = p;                       // structured binding (C++17): copies into idx, val
auto &[i2, v2] = p;                        // binds references into p
std::pair<int, int> merge = {101, 32};     // a BPE merge: (left_id, right_id)

std::pair<double, double> min_max(const std::vector<double> &v);   // returning two values
auto [lo, hi] = min_max(data);
```

Structured bindings also destructure `std::array`, plain structs with public fields, and map entries (`for (const auto &[key, count] : counts)`). Python equivalent: tuples and tuple unpacking. Prefer a small named `struct` over a `tuple` of three or more members — `r.loss` beats `std::get<1>(r)`.

## 10. `std::map` vs `std::unordered_map`

Both map keys to values. `std::map` is a balanced red-black tree: keys are kept **sorted**, operations are O(log n), iteration is in key order, and keys need `operator<`. `std::unordered_map` is a hash table — your C hash table, done for you — with O(1) average operations, unordered iteration, and keys need `std::hash` and `operator==`.

```cpp
#include <map>
#include <unordered_map>
std::unordered_map<std::string, int> word_count;
word_count["the"]++;                     // operator[] inserts a value-initialized 0 if absent, then ++
word_count["cat"] += 2;
if (auto it = word_count.find("dog"); it != word_count.end()) std::cout << it->second;   // lookup without inserting
word_count.count("the");                 // 1 or 0
word_count.erase("cat");
for (const auto &[word, n] : word_count) std::cout << word << ':' << n << ' ';   // arbitrary order

std::map<std::string, int> sorted(word_count.begin(), word_count.end());          // now iterates alphabetically
```

**`operator[]` on a `const` map does not compile** (it might insert). Use `find` or `at` (`at` throws if absent).

### Custom hash for `std::pair` keys (BPE pair counts)

`std::hash` is defined for integers, floating point, pointers, `std::string`, `std::string_view` — but **not** for `std::pair` or `std::tuple`. BPE needs `unordered_map<pair<int,int>, int>` for "how often does token `a` follow token `b`". You supply a hash functor:

```cpp
struct PairHash {
    std::size_t operator()(const std::pair<int, int> &p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));   // boost::hash_combine
    }
};
std::unordered_map<std::pair<int, int>, int, PairHash> pair_counts;
for (std::size_t i = 0; i + 1 < ids.size(); i++) pair_counts[{ids[i], ids[i + 1]}]++;

auto best = std::max_element(pair_counts.begin(), pair_counts.end(),
                             [](const auto &a, const auto &b) { return a.second < b.second; });
// best->first is the most frequent pair; best->second its count
```

A naive `h1 ^ h2` is a bad hash (`(a,b)` and `(b,a)` collide, `(x,x)` all hash to 0). The combine above is what Boost uses. `std::pair` already has `operator==`, so only the hash is missing.

## 11. `std::set` / `std::unordered_set`

Keys only, no values: "have I seen this?" `std::set` is sorted (tree), `std::unordered_set` is hashed.

```cpp
#include <set>
#include <unordered_set>
std::unordered_set<std::string> vocab;
vocab.insert("the");
bool known = vocab.count("the") > 0;      // or vocab.find(...) != vocab.end()
std::set<int> ids{5, 1, 3, 1};            // {1, 3, 5} — duplicates dropped, sorted
*ids.begin();                             // 1 (min)   *ids.rbegin() -> 5 (max)
ids.lower_bound(2);                       // iterator to 3: first element >= 2
```

`std::multiset`/`std::multimap` allow duplicate keys.

## 12. `std::deque`

Double-ended queue: O(1) `push_back`, `push_front`, `pop_back`, `pop_front`, O(1) indexed access, but stored in chunks, so not contiguous (no `data()`) and slower to iterate than `vector`. Use when you need to add/remove at both ends — a sliding window over a time series, a BFS frontier.

```cpp
#include <deque>
std::deque<double> window;
window.push_back(x); if (window.size() > 50) window.pop_front();   // 50-sample moving window
```

## 13. `std::stack`, `std::queue`, `std::priority_queue` — adaptors

These wrap another container and restrict its interface.

```cpp
#include <stack>
#include <queue>
std::stack<int> st;          st.push(1); st.top(); st.pop();            // LIFO, over deque by default
std::queue<int> q;           q.push(1);  q.front(); q.pop();            // FIFO, over deque
std::priority_queue<double> pq;                                         // max-heap over vector
pq.push(3.0); pq.push(9.0); pq.push(1.0);
pq.top();                    // 9.0
pq.pop();                    // removes 9.0;  O(log n) push/pop, O(1) top

// min-heap: reverse the comparator
std::priority_queue<double, std::vector<double>, std::greater<double>> minpq;

// heap of (priority, payload): pairs compare lexicographically, so first element is the priority
std::priority_queue<std::pair<double, int>> events;   // (time, event_id) — for discrete-event sims
```

Note `pop()` returns `void`; read `top()` first. A `priority_queue` is the standard tool for "always process the smallest/largest next": Dijkstra, event scheduling, k-nearest neighbours, Huffman coding, or picking the next BPE merge when you maintain counts incrementally.

## 14. Iterators

An iterator is a generalized pointer: `*it` dereferences, `++it` advances, `it != end` tests. For `vector` and `array` iterators *are* essentially pointers; for `map`/`set` they walk the tree. Every container gives `begin()` (first element) and `end()` (one *past* the last — never dereference it), mirroring the C `p != a + n` idiom.

```cpp
std::vector<double> v{3, 1, 2};
for (std::vector<double>::iterator it = v.begin(); it != v.end(); ++it) *it *= 2;   // verbose
for (auto it = v.begin(); it != v.end(); ++it) *it *= 2;                            // same, with auto
for (auto it = v.cbegin(); it != v.cend(); ++it) std::cout << *it;                  // const_iterator: read-only
for (auto it = v.rbegin(); it != v.rend(); ++it) std::cout << *it;                  // reverse

std::map<std::string, int> m{{"a", 1}, {"b", 2}};
auto it = m.find("b");
if (it != m.end()) std::cout << it->first << ' ' << it->second;                     // pair<const Key, Value>

std::distance(v.begin(), it2);   // number of steps between iterators (O(1) for vector, O(n) for map)
auto idx = std::distance(v.begin(), std::max_element(v.begin(), v.end()));         // argmax as an index
```

Range-based `for` is sugar over `begin()`/`end()`: `for (auto &x : v)` is `for (auto it = v.begin(); it != v.end(); ++it) { auto &x = *it; ... }`. You write explicit iterators when you need the position (erase, insert, argmax) or when calling `<algorithm>` functions, which all take `(first, last)` ranges. Iterators are invalidated under the same rules as pointers (section 4).

## 15. `<algorithm>` teaser

```cpp
#include <algorithm>
#include <numeric>     // std::accumulate, std::iota
std::vector<double> v{3.0, 1.0, 4.0, 1.5};

std::sort(v.begin(), v.end());                                 // ascending, O(n log n) introsort
std::sort(v.begin(), v.end(), [](double a, double b) { return a > b; });   // descending (lambda comparator)
auto mx = std::max_element(v.begin(), v.end());                // iterator to the max; *mx is the value
double sum = std::accumulate(v.begin(), v.end(), 0.0);         // 0.0, NOT 0 — an int start truncates!
std::reverse(v.begin(), v.end());
std::fill(v.begin(), v.end(), 0.0);
std::iota(v.begin(), v.end(), 0);                              // 0, 1, 2, 3
auto pos = std::find(v.begin(), v.end(), 2.0);                 // linear search; end() if absent
bool sorted = std::is_sorted(v.begin(), v.end());
std::vector<int> idx(v.size()); std::iota(idx.begin(), idx.end(), 0);
std::sort(idx.begin(), idx.end(), [&](int a, int b) { return v[a] < v[b]; });   // argsort
v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return x < 0; }), v.end());   // erase-remove idiom
```

Lambdas (`[](args) { body }`) are anonymous functions; `[&]` captures surrounding variables by reference. They get a full chapter later; for now this is enough to use the algorithms. The `std::accumulate(..., 0)` trap — integer initial value makes the whole sum integer — is one of the most common numerical bugs in C++ code.

## 16. Performance table

| Operation | `vector` | `deque` | `list` | `map`/`set` | `unordered_map`/`set` | `priority_queue` |
|---|---|---|---|---|---|---|
| index `[i]` | O(1) | O(1) | — | — (`[key]` O(log n)) | — (`[key]` O(1) avg) | — |
| push/pop back | O(1) amortized | O(1) | O(1) | — | — | O(log n) |
| push/pop front | O(n) | O(1) | O(1) | — | — | — |
| insert/erase middle | O(n) | O(n) | O(1) given iterator | O(log n) | O(1) avg | — |
| find by value/key | O(n) (`O(log n)` if sorted + `binary_search`) | O(n) | O(n) | O(log n) | O(1) avg, O(n) worst | — |
| min / max | O(n) | O(n) | O(n) | O(1) (`begin`/`rbegin`) | O(n) | O(1) top |
| iteration order | insertion | insertion | insertion | sorted by key | arbitrary | — |
| contiguous memory | yes | chunked | no | no | no | (vector) |
| iterator stability on insert | invalidated on realloc | invalidated | stable | stable | invalidated on rehash | — |

Big-O hides constants: a `vector` linear scan of 100 `int`s beats an `unordered_map` lookup because the vector is 400 contiguous bytes in L1 cache and the hash table is pointer-chasing. `std::list` (doubly linked) is almost never the right choice for the same reason; it is omitted from this chapter on purpose.

## 17. Choosing a container

1. **Default to `std::vector`.** Contiguous, cache-friendly, smallest overhead, works with C APIs.
2. Need key → value lookup? `std::unordered_map`. Need the keys sorted or range queries (`lower_bound`)? `std::map`.
3. Need "is it in the set?" `std::unordered_set` (or a sorted `vector` + `std::binary_search` if it is built once and queried many times).
4. Need to pop from the front too? `std::deque`.
5. Need the min/max repeatedly while inserting? `std::priority_queue`.
6. Fixed small size known at compile time? `std::array`.
7. Two or three heterogeneous values? `std::pair` / a small `struct`.

If you are unsure, use `vector` and measure. It is faster than intuition suggests up to surprisingly large n.

---

## Gotchas and undefined behavior

- **`v[i]` out of range** is UB (no exception, no message). `.at(i)` throws. In debug builds enable the library hardening macros so `[]` checks too.
- **`back()`, `front()`, `pop_back()` on an empty vector** — UB.
- **References/pointers/iterators into a `vector` after `push_back`/`insert`/`resize`** — dangling if capacity changed. `reserve` first or use indices.
- **`v.push_back(v[0])`** — the argument is a reference into `v` that reallocation may invalidate mid-call. (Implementations are required to handle this case correctly for `push_back`, but not every "self-referencing" call is safe — `v.insert(v.begin(), v[0])` with reallocation is a known hazard.) Copy to a local first.
- **`std::vector<int> v{5}` vs `v(5)`** — one element vs five zeros.
- **`std::accumulate(b, e, 0)`** on doubles sums in `int`. Pass `0.0`.
- **`map[key]` inserts** a default value when `key` is absent — including inside a "does it exist?" check. Use `find`/`count`/`contains` (C++20).
- **`operator[]` on a `const std::map`** does not compile. Use `at` or `find`.
- **Hashing a `pair`** without a custom hash fails to compile with a long template error. Provide the functor (section 10).
- **`std::string` bytes are `char`, which is signed on x86/ARM macOS**. `int id = s[i];` for a byte ≥ 128 gives a negative id. Convert via `static_cast<unsigned char>(s[i])`.
- **`c_str()`/`data()` pointers and `string_view`s** are invalidated by any modification of the string. Same as vector.
- **`std::string_view` to a temporary** (`std::string_view sv = std::to_string(1);`) dangles at the semicolon.
- **Erasing inside a range-for** — UB. Use the erase-remove idiom or an explicit iterator loop with `it = v.erase(it)`.
- **`std::sort` on floats containing NaN** — NaN breaks the strict weak ordering; result is UB. Filter or `std::isnan`-aware comparator.
- **`priority_queue::pop()` returns void** — read `top()` first, then `pop()`.
- **`unordered_map` iteration order** is unspecified and can change after any insert (rehash). Never depend on it for reproducibility — sort keys first if output must be deterministic.

## Common mistakes checklist

- [ ] `reserve()` before a known number of `push_back`s; never hold a reference across growth.
- [ ] Matrices are flat `vector<double>` with `i * cols + j`, not `vector<vector<double>>`.
- [ ] `Matrix` over `vector` has no user-written destructor/copy operations (Rule of Zero).
- [ ] `accumulate` starts from `0.0` for floating point.
- [ ] `find` (not `[]`) for lookups that must not insert; `at`/`find` on const maps.
- [ ] Custom hash provided for `pair`/`tuple`/struct keys, combining fields properly.
- [ ] Bytes from `std::string` go through `unsigned char` before becoming token ids.
- [ ] `std::string_view` / `const std::string&` for read-only string parameters; never store a `string_view` past the string's lifetime.
- [ ] `const auto &[k, v]` when iterating maps.
- [ ] Erase with the erase-remove idiom, not inside a range-for.

## You can move on when...

- You can rewrite chapter 02's `Matrix` over `std::vector<double>` and explain which special members disappeared and why the result is still deep-copying and deep-const.
- You can explain the doubling growth strategy, compute how many reallocations 10⁶ `push_back`s cause, and state which operations invalidate iterators.
- You can split a CSV line into doubles with `find`/`substr`/`stod` and read a corpus line by line with `getline`, treating bytes as `unsigned char`.
- You can write `unordered_map<pair<int,int>, int>` with a working hash from memory and explain why `h1 ^ h2` is bad.
- You can pick the right container for: token→id lookup, a sliding window, a sorted leaderboard, the next event in a simulation, a 3-vector — and cite the complexity of the operations that matter.
- You can compute an argmax with `std::max_element` and `std::distance`, and argsort with `std::iota` + `std::sort` + a lambda.
