# Chapter 04 — Exercises

Write each exercise as `ex04_K.cpp` in this folder and compile with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex04_K ex04_K.cpp`. Run the ones that touch element references
under `-fsanitize=address -g` too.

---

**04.1 — Watch the vector grow**
Push 1..100 into an empty `std::vector<int>` and print a line every time `capacity()` changes: `size=N capacity=M`. Then repeat with `reserve(100)` first and show no reallocation happens. Finally record `&v[0]` before and after a reallocation and print whether it changed.

Example (libc++ doubles): `size=1 capacity=1`, `size=2 capacity=2`, `size=3 capacity=4`, `size=5 capacity=8`, ...

<details><summary>Hint</summary>
Store the previous capacity in a variable and compare after each `push_back`. Print addresses with `static_cast<const void*>(v.data())`.
</details>

---

**04.2 — Reproduce and fix iterator invalidation**
Write a loop that iterates a `std::vector<double>` with an iterator and pushes `x * 10` for every `x > 2` — the buggy version from the lesson. Run it under `-fsanitize=address` and paste the first lines of the report into a comment. Then write two correct versions: (a) collect into a second vector and append at the end; (b) index-based with `reserve` up front. Confirm both produce `{1,2,3,4,30,40}` from `{1,2,3,4}`.

<details><summary>Hint</summary>
The sanitizer will report `heap-use-after-free` and point at the `*it` after a `push_back`. For (b), note that even index-based code must copy `v[i]` into a local before passing it to `push_back` if capacity may change.
</details>

---

**04.3 — `Matrix` on a flat vector, Rule of Zero**
Reimplement the chapter 02 `Matrix` with `std::vector<double>` storage, `operator()(i, j)` const/non-const, `rows()`, `cols()`, `data()`, and free functions `Matrix matmul(const Matrix&, const Matrix&)`, `Matrix transpose(const Matrix&)`. Write **no** destructor or copy operations. Show that `Matrix b = a; b(0,0) = 5;` leaves `a` untouched, and that `sizeof(Matrix)` is 32 bytes (two ints + a 24-byte vector). Benchmark `matmul` for 300×300 against a `vector<vector<double>>` version.

Example: `matmul(I, X) == X`; `transpose(transpose(X)) == X`.

<details><summary>Hint</summary>
Loop order `i, k, j` (accumulate `a(i,k) * b(k,j)` into `out(i,j)` with `k` in the middle) is cache-friendlier than `i, j, k` for row-major storage. Time both loop orders as well.
</details>

---

**04.4 — String toolkit**
Write these free functions and test each: `std::vector<std::string> split(std::string_view s, char delim)`, `std::string join(const std::vector<std::string>&, std::string_view sep)`, `std::string trim(std::string_view)` (strip leading/trailing whitespace), `bool starts_with(std::string_view s, std::string_view prefix)`, `std::string to_lower(std::string)`. Read lines from `std::cin` with `std::getline` and print `trim`med, lower-cased, `|`-joined tokens.

Example: input `"  The Cat, sat "` split on `' '` after trim → `the|cat,|sat`.

<details><summary>Hint</summary>
`string_view::find`, `string_view::substr`, and `remove_prefix`/`remove_suffix` do most of the work with no allocation. `std::tolower` from `<cctype>` takes an `int` that must be in `unsigned char` range.
</details>

---

**04.5 — Container zoo with timing**
Insert 10⁶ random `int`s into a `std::vector`, `std::set`, and `std::unordered_set`; then perform 10⁶ membership queries on each (sort the vector first and use `std::binary_search`). Print insert time, query time, and approximate memory (`sizeof` per node estimates are fine) for each. Write a one-paragraph comment on when you would pick each, referencing the performance table in the lesson.

Example output shape: `vector: insert 8 ms, sort 70 ms, query 95 ms` etc.

<details><summary>Hint</summary>
`std::mt19937` from `<random>` with `std::uniform_int_distribution` for the numbers; `std::chrono::steady_clock` for timing. Keep the query keys in a vector so every container is asked the same questions.
</details>

---

**04.6 — Word frequency, two ways**
Read text from `std::cin`, split into words, count with `std::unordered_map<std::string, int>`, then print the top 10 words by count. Do the ranking twice: (a) copy into a `std::vector<std::pair<std::string,int>>` and `std::sort` with a lambda on `.second` descending (ties alphabetically); (b) push everything into a `std::priority_queue` and pop 10. Use structured bindings when iterating the map.

Example: for the input `the cat the dog the end`, top entry is `the 3`.

<details><summary>Hint</summary>
For (b), `std::priority_queue<std::pair<int, std::string>>` orders by count first because `pair` compares lexicographically; for the alphabetical tie-break you will need a custom comparator or to negate the count and use a min-heap.
</details>

---

**04.7 — Iterators and algorithms on a signal**
Generate 1000 samples of `sin(0.05 i) + 0.1 * noise`. Using only `<algorithm>`/`<numeric>` (no hand-written loops except to fill the signal): compute mean (`accumulate`), the index of the max (`max_element` + `distance`), the number of samples above 0.5 (`count_if`), a sorted copy (`sort` on a copy — `partial_sort` for just the top 10), the median, and an argsort of the first 20 samples (`iota` + `sort` with a lambda). Also demonstrate the erase-remove idiom to drop all negative samples.

Example: mean close to 0; the max index is near a peak of the sine (`i ≈ 31 + 126k`).

<details><summary>Hint</summary>
`std::nth_element` finds the median in O(n) without a full sort. Remember `accumulate(..., 0.0)`, not `0`.
</details>

---

**04.8 — BPE pair counting with a custom hash**
Given a corpus (read from `std::cin` or a hard-coded string), convert each byte to an `int` token in `0..255` (through `unsigned char`), store the sequence in `std::vector<int>`, and count all adjacent pairs in `std::unordered_map<std::pair<int,int>, int, PairHash>`. Find the most frequent pair with `std::max_element` and a lambda, print it as two byte values and its count, then **merge** it: create a new token id `256` and rewrite the sequence replacing every occurrence of the pair with `256`. Repeat for 10 merges, printing the pair and the shrinking sequence length each time. This is the core loop of a BPE tokenizer.

Example: on `"aaabdaaabac"` the first merge is `(97,97)` (i.e. `aa`) with count 4, and the sequence shrinks from 11 to 7.

<details><summary>Hint</summary>
The merge pass walks the old vector with index `i`; if `ids[i], ids[i+1]` matches the pair, push the new id and skip 2, else push `ids[i]` and skip 1. Recount pairs from scratch after each merge (O(n) per merge — fine for now; the incremental version uses a `priority_queue`).
</details>

---

**04.9 — Sliding-window statistics and a min-heap event queue**
Two parts. (a) Stream 10⁴ noisy samples and print, every 100 samples, the mean of the last 50 using a `std::deque<double>` window (pop front when it exceeds 50) — then note in a comment why a running sum with a deque beats re-summing. (b) Simulate a discrete-event system: `std::priority_queue` of `(time, particle_id)` as a **min**-heap; start with 5 particles at random times, pop the earliest event, print it, and re-schedule that particle at `time + exponential(1.0)`; stop after 30 events and verify the printed times are non-decreasing.

Example (b): `t=0.13 id=2`, `t=0.41 id=0`, ... strictly non-decreasing.

<details><summary>Hint</summary>
Min-heap: `std::priority_queue<std::pair<double,int>, std::vector<std::pair<double,int>>, std::greater<>>`. `std::exponential_distribution<double>` from `<random>` for the waiting times.
</details>

---

**04.10 — N-body state as a struct-of-vectors vs vector-of-structs**
Implement the same O(n²) gravitational acceleration kernel twice for `n = 2000` bodies: (a) `std::vector<Body>` where `Body{std::array<double,3> pos, vel; double mass;}` (array of structs); (b) `struct Bodies { std::vector<double> x, y, z, vx, vy, vz, m; }` (struct of arrays). Time 10 force evaluations of each with `std::chrono`, verify both produce the same total acceleration to `1e-9`, and comment on which layout the compiler vectorized better (try `-O2 -Rpass=loop-vectorize` on clang to see).

Example: both print `sum |a| = <same number>`; the timing ratio is the interesting output.

<details><summary>Hint</summary>
Softening: use `r² + eps²` in the denominator to avoid division by zero when `i == j` (or skip `i == j`). For (b), the inner loop over `j` reads seven separate contiguous arrays — that is what the vectorizer likes.
</details>
