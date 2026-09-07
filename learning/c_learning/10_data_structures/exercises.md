# Chapter 10 — Exercises

Write each exercise as `ex10_K.c` in this folder. Compile with:

```
cc -Wall -Wextra -std=c11 -O2 -o ex10_1 ex10_1.c -lm
```

Every exercise in this chapter allocates. Run each under `-g -fsanitize=address,undefined` and check for leaks with `leaks --atExit -- ./ex10_K` on a `-O0 -g` build before calling it done. Each container you write here is a candidate module for your `src/` directory in later projects, so give functions consistent `name_verb` prefixes and write ownership comments.

---

## 10.1 — **Generic dynamic array**

Implement `DynArray` (`void *data; size_t len, cap, elem_size`) with `da_init`, `da_push`, `da_at`, `da_pop` (copies the last element out), and `da_free`. Use it with two element types in the same program: `int` and a `struct { double x, y; }`. Push 1000 of each, verify `da_at(&a, 999)` holds what you pushed, pop all and verify order. Print the final `cap` for each.

Example: `ints: len=1000 cap=1024 last=999   points: len=1000 cap=1024 last=(999.0,1998.0)`

<details><summary>Hint</summary>

- Byte offset: `(char *)a->data + i * a->elem_size`.
- `da_pop(a, out)` is `memcpy(out, da_at(a, len-1), elem_size); len--`.
</details>

---

## 10.2 — **`DEFINE_VEC` macro**

Write the `DEFINE_VEC(T, Name)` macro generating `Name`, `Name_push`, `Name_pop`, `Name_free`. Instantiate `IntVec` and `StrVec` (`char *` elements). Push the words of a sentence into a `StrVec` (each word heap-copied), then print them reversed using `pop`, freeing each string as you go. Try `IntVec_push(&v, 2.7)` and note in a comment what the compiler says (`-Wall -Wextra` should warn about implicit conversion — if it does not, add `-Wconversion` and see).

Example: `./ex10_2` with sentence `"the quick brown fox"` → `fox brown quick the`

<details><summary>Hint</summary>

- Line continuations `\` must be the last character on each macro line — no trailing spaces.
- `StrVec` stores *owning* pointers: the vec frees storage, you free strings. Document that.
</details>

---

## 10.3 — **Linked list with `Node **` removal**

Implement a singly linked list of `int` with `push_front`, `push_back` (O(n) is fine), `remove_all(value)` that deletes *every* node with that value using the `Node **pp` technique in a single pass, `length`, `print`, and `free`. Build `[1,2,3,2,4,2]`, remove all `2`s, print. Then remove a value not present and a value at the head to confirm both work.

Example:
```
1 2 3 2 4 2
after remove_all(2): 1 3 4
after remove_all(1): 3 4
```

<details><summary>Hint</summary>

- In `remove_all`, when you delete `*pp` do NOT advance `pp` — the next node has slid into `*pp`.
- Save `next` before `free`.
</details>

---

## 10.4 — **Ring-buffer queue, growable**

Implement `Queue` of `int` as a ring buffer with `queue_push`, `queue_pop`, `queue_len`, and automatic growth: when full, allocate `2*cap`, copy the elements in logical order starting at `head`, reset `head = 0`. Test by pushing 5, popping 3, pushing 10 (forcing wrap-around and then a grow), and popping everything — the output must be in FIFO order. Print `head`, `len`, `cap` after each phase.

Example:
```
after push 5:  head=0 len=5 cap=8
after pop 3:   head=3 len=2 cap=8
after push 10: head=0 len=12 cap=16
popped: 3 4 5 6 7 8 9 10 11 12 13 14
```

<details><summary>Hint</summary>

- Logical element `k` lives at `data[(head + k) % cap]`.
- Growth copies `len` elements, not `cap`.
</details>

---

## 10.5 — **Two hash functions, measured**

Implement `hash_djb2`, `hash_fnv1a`, and a deliberately bad hash (sum of bytes). For a set of 10,000 distinct generated keys (`"key_0"` … `"key_9999"`), bucket each hash into `cap = 16384` buckets and print, for each hash function: the number of empty buckets, the maximum bucket size, and the number of collisions (keys minus non-empty buckets). Do the same with `cap = 16000` (not a power of two) and comment on whether it matters for each hash.

Example (approximate):
```
djb2   : empty=8490 max=5 collisions=2474
fnv1a  : empty=8500 max=5 collisions=2480
bytesum: empty=16250 max=1234 collisions=9866
```

<details><summary>Hint</summary>

- The ideal is a Poisson distribution: with 10,000 keys in 16,384 buckets, ~54% empty, max bucket ~5.
- `count[h % cap]++` with a `calloc`'d `int` array.
</details>

---

## 10.6 — **String→int hash table with chaining**

Implement `StrMap` (chaining) with `strmap_init(cap)`, `strmap_get(m, key, int *out)` (returns 0 if found), `strmap_put(m, key, value)` (copies key), `strmap_remove(m, key)` (frees key and node), `strmap_free`, and growth at load factor 0.75 that doubles and rehashes. Read words from `stdin` and count them; print the 10 most frequent (collect `(key, count)` pairs into an array and `qsort` by count descending, ties by key). Print `count` and `cap` at the end. Verify with ASan and `leaks`.

Example: `cat lesson.md | ./ex10_6` →
```
the 142
a 98
...
unique=1874 cap=4096
```

<details><summary>Hint</summary>

- Tokenize on whitespace with `fgets` + `strtok`; lowercase and strip punctuation if you like.
- Rehash: for each old chain, unlink each node and push it into the new bucket array — no new allocation needed.
</details>

---

## 10.7 — **Open addressing with tombstones**

Implement `IntMap` (`int64_t` → `int64_t`) with linear probing, power-of-two capacity, `hash_u64` from the lesson, a sentinel key `EMPTY = INT64_MIN` and `TOMB = INT64_MIN + 1`, growth at 0.7 load (counting tombstones), `get`, `put`, `remove`. Test: insert 100,000 keys `k → k*k`, remove every third key, re-insert every sixth, then verify every remaining key maps correctly and every removed key is absent. Print `count`, `tombstones`, `cap`, and the average probe length over all successful `get`s.

Example: `count=83334 tombstones=... cap=262144 avg_probes=1.3x`

<details><summary>Hint</summary>

- Rehash on growth drops tombstones: only copy live slots.
- Track probes with a counter incremented in the probe loop; reset it before the verification pass.
</details>

---

## 10.8 — **BPE pair counter** *(ML)*

Using `IntMap` from 10.7 with pair keys `((uint64_t)a << 32) | (uint32_t)b`, read a text file as a sequence of byte tokens (`int` array), count every adjacent pair, find the most frequent pair, and print it as `(a, b) count`. Then perform ONE merge: replace every occurrence of that pair with a new token id `256`, print the new sequence length, and recount to find the next best pair. (The full BPE loop is the tokenizer project; this is its inner step.)

Example on a text file:
```
tokens=48213
best pair: (101,32) 'e',' ' count=1502
after merge: tokens=46711
next best: (116,104) 't','h' count=1187
```

<details><summary>Hint</summary>

- Decode key back with `a = key >> 32; b = key & 0xffffffff`.
- Merge in place with a read index and a write index over the same array.
</details>

---

## 10.9 — **k-nearest neighbors with a bounded max-heap** *(ML)*

Generate 100,000 random 2D points and a query point. Find the `k = 10` nearest using a *max*-heap of size `k` keyed by distance: push until full, then for each new point, if it is closer than the heap's max, pop the max and push the new one. Store `{double dist; int index;}` in the heap. Print the 10 indices and distances sorted ascending (pop the heap into an array and reverse, or `qsort`). Verify against a brute-force `qsort` of all 100,000 distances that the sets match. Time both approaches.

Example:
```
heap:  0.00xx (#4821) 0.00yy (#77123) ...   0.0012 s
qsort: same 10 indices                        0.0150 s
```

<details><summary>Hint</summary>

- A max-heap is the min-heap with the comparison flipped; write it once with a `cmp` or just flip the `<`.
- Compare squared distances; skip the `sqrt` until printing.
</details>

---

## 10.10 — **Event-driven collision sim with a priority queue** *(sim)*

Simulate `N = 20` equal-mass particles on a 1D segment `[0, 1]` with elastic collisions (velocities swap) and elastic walls (velocity negates). Instead of fixed time steps, compute the time of every particle's next event (collision with its right neighbor or with a wall), push `{time, i, kind}` into a min-heap, and repeatedly pop the earliest event, advance all positions to that time, apply it, and push the affected particles' new events. Invalidate stale events by storing per-particle collision counts in the event and skipping events whose counts no longer match. Run until `t = 100`; print the number of events processed and the total kinetic energy at start and end (must match to `1e-9`). Write a CSV row `(t, x_0, ..., x_{N-1})` every 1.0 time units for plotting in Python.

Example:
```
events=8437  KE_start=3.271882  KE_end=3.271882  wrote ex10_10_traj.csv
```

<details><summary>Hint</summary>

- 1D particles never pass each other, so particle `i`'s only possible partners are `i-1` and `i+1`.
- Event `{double t; int i, j; int count_i, count_j;}`; `j = -1` for a wall. Skip on pop if counts differ.
- Growable heap: reuse the `Vec` growth idiom on the event array.
</details>
