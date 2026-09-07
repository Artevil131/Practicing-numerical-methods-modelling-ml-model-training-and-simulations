# Chapter 01 — Contest Fundamentals

## What you'll be able to do after this chapter

- Name the rules, scoring and feedback model of every contest you will meet on the
  Datatähti → BOI → IOI → ICPC path, and the Codeforces/AtCoder rounds you train on.
- Look at a constraint line (`1 ≤ n ≤ 2·10^5`) and answer, before any code, which complexity
  class the setter intends — and reject a wrong idea in 10 seconds instead of 40 minutes.
- Type your personal C++17 contest template from memory, with fast I/O, and know exactly what
  each line costs and buys.
- Choose integer types by computing the worst-case intermediate value, not by guessing.
- Use the compiler builtins, `bitset`, `__int128` and pbds structures that make C++ the contest
  language, and know which of them break on macOS/clang.
- Diagnose a TLE/WA/RE verdict systematically, and find the bug with a brute force + random
  generator + compare loop instead of staring.
- Run a contest by the clock: read everything, bank the easy points, cut losses, upsolve after.

Prerequisites: `../../cpp_learning/` chapters 04 (`vector`/`string`), 08 (STL algorithms,
lambdas), 12 (performance); `../../algorithms_learning/` all eight chapters for the patterns
themselves. This chapter teaches none of the algorithms; it teaches the *environment* they run in.

---

## Where this shows up in contests

Everything here is background for every problem you will ever submit. Concretely:

| Situation                                              | What in this chapter saves you                      |
|--------------------------------------------------------|-----------------------------------------------------|
| CF Div2 A/B, ABC A–C: trivial logic, 60% of WAs        | reading the statement, overflow, edge cases (§3, §6)|
| `n ≤ 2·10^5`, 10^6 lines of output                      | fast I/O, `'\n'` not `endl` (§4)                    |
| IOI subtask 1–2 (small n) worth 10–30 points           | complexity table → brute force is *intended* (§2)   |
| deep DFS on a path graph of 10^6 nodes                 | stack size (§13)                                    |
| "WA on test 7" with no feedback, IOI-style             | stress testing against a brute force (§11)          |
| CF hack phase / anti-hash tests                        | custom hash for `unordered_map`, `mt19937` (§9, §6) |

---

## 1. Contest formats

### 1.1 The olympiad line: Datatähti → BOI/CEOI → IOI

**IOI (International Olympiad in Informatics).** Individual. Two competition days, **5 hours
each, 3 tasks per day**, each task worth 100 points, split into **subtasks** with stricter
constraints (a task with `n ≤ 10^5` typically has subtasks at `n ≤ 10`, `n ≤ 1000`, `n ≤ 10^5`,
sometimes special structure such as "all weights equal"). Score per subtask is all-or-nothing:
one failing test in a subtask zeroes that subtask. Modern IOI (since 2010) shows **full
feedback** — your per-subtask score after each submission — with a submission limit (commonly
50 per task) and a short cooldown. Some tasks are *output-only* (you get the inputs, submit
answer files) or *interactive/communication* (you implement a function the grader calls; no
`main`, no I/O, strict call limits). Medals: roughly top 1/12 gold, next 2/12 silver, next 3/12
bronze of all contestants — about half get a medal.

Consequence for strategy: **subtasks are the game.** 3 × 100 points in 5 hours; a finalist-level
day is 150–220. You almost never fully solve all three. Collect every cheap subtask on all three
tasks first (§14).

**BOI (Baltic Olympiad in Informatics)** and **CEOI (Central European OI)** use the same format
as IOI: 2 days × 5 h × 3 tasks, subtasks, full feedback. They are the team-selection and
warm-up stage; BOI is where Finland picks and trains the IOI four. Difficulty per task is around
IOI's easier half.

**Datatähti** (Finnish national olympiad, run on CSES). Qualification round is online, about a
week long, ~5–7 tasks with subtasks and full feedback; you may use any material. The final is
on-site, IOI-style. Top finalists go to training camps, BOI, then IOI selection. The qualifier
tasks are exactly the level of the CSES Introductory / Sorting-and-Searching sets — chapter 02's
problem ladder.

Other olympiads worth knowing: **JOI** (Japan, very clean hard tasks, excellent for upsolving),
**USACO** (US, four divisions Bronze→Platinum, monthly, 4–5 h, full feedback per test, no
subtasks — Gold/Platinum are IOI-level), **COCI** (Croatia, monthly, partial scoring by tests),
**EJOI** (junior).

### 1.2 Codeforces

Rated rounds, ~2 h to 2 h 15 min, 5–8 problems A–F/G. Divisions by rating: **Div 4** (< 1400),
**Div 3** (< 1600), **Div 2** (< 1900, often held together with Div 1, sharing problems
Div2 C–F = Div1 A–D), **Div 1** (≥ 1900). Global rounds and Div1+2 combined rounds are rated for
everyone.

Two scoring systems:

- **Standard rounds (Div1/Div2 with fixed point values, e.g. 500/1000/1500/2000/2500).** Each
  problem's value decays over the contest (to a floor of 30%); each wrong submission costs 50
  points on that problem. Verdict during the contest is only on **pretests**; after the contest
  all submissions run on **system tests**, and a solution that passes pretests can still fail.
  You may **lock** a problem you have solved and then **hack** other people's solutions in your
  room by supplying a test that breaks them (+100 per successful hack, −50 per failed one).
  Anti-hash tests against `unordered_map` and worst-case tests against `sort` on adversarial
  input are classic hacks.
- **Div3, Div4, Educational rounds: ICPC-style.** Ranking by number solved, ties broken by
  penalty time (sum of accept times plus 10 min per wrong submission). Then a **12-hour open
  hacking phase** where anyone can hack anyone; the final standings use the hacked tests.

Problem tags and difficulty ratings (800–3500) make the archive the best grinding tool that
exists. A problem rated *r* is one that people of rating *r* solve about half the time.

### 1.3 AtCoder

Weekend contests, 100 minutes (ABC) to 2–3 hours (AGC). **ABC** (Beginner, rated to 1999),
**ARC** (Regular, rated to 2799), **AGC** (Grand, rated for all). Typically 6–8 problems A–G/Ex.
Scoring by problem value; each wrong submission adds 5 minutes penalty. Full feedback on the
real tests (no pretest/system-test split). AtCoder problems are famous for clean statements and
for requiring a single sharp observation; ARC/AGC problems are the best training for the
"insight" component of IOI tasks.

### 1.4 ICPC

**Teams of three, one computer, 5 hours, 10–13 problems.** Binary verdicts (AC or not), no
partial credit, rank by problems solved then penalty (sum of accept times + 20 min per rejected
submission on a solved problem). Regional (for Finland: NCPC → NWERC) → World Finals. The skills
that matter beyond IOI: parallel work (one codes while two think), printed team notebook of
library code, clean division of labour, and never letting the keyboard idle.

### 1.5 What the formats mean for training

| Format          | Feedback           | Partial credit   | Trains                                          |
|-----------------|--------------------|------------------|-------------------------------------------------|
| IOI/BOI/Datatähti | full, per subtask | yes (subtasks)   | long focus, subtask harvesting, hard single tasks |
| CF standard     | pretests only      | no (per problem) | speed, hacking, judging your own correctness    |
| CF Div3/Edu/ICPC| full               | no               | speed, penalty discipline                       |
| AtCoder         | full               | no               | one-observation problems, clean implementation  |

Grind Codeforces and AtCoder weekly for speed and breadth; do full 5-hour olympiad mock sets
(past BOI/CEOI/IOI days on oj.uz) every 2–4 weeks for stamina and subtask strategy.

---

## 2. The complexity budget

A judge machine runs roughly **10^8 simple operations per second** — a loop of additions and
array reads with good cache behaviour. `std::sort`, `map`, `set`, modulo, division, cache
misses, `long long` on a 32-bit judge, recursion — each multiplies the cost by 2–10. Use the
budget to pick the **class** of algorithm, never to fine-tune.

| Max n           | Allowed complexity            | Typical algorithm                                   |
|-----------------|-------------------------------|-----------------------------------------------------|
| n ≤ 10–11       | O(n!)                         | all permutations                                    |
| n ≤ 20–25       | O(2^n · n)                    | all subsets, bitmask DP                             |
| n ≤ 40          | O(2^{n/2} · n)                | meet in the middle                                  |
| n ≤ 100         | O(n^4)                        | quadruple loop, Floyd on small graphs               |
| n ≤ 500         | O(n^3)                        | Floyd–Warshall, interval DP, matrix chain           |
| n ≤ 5000        | O(n^2)                        | LCS/edit distance, quadratic DP, all pairs          |
| n ≤ 10^5        | O(n √n)                       | Mo's algorithm, sqrt decomposition                  |
| n ≤ 10^6        | O(n log n)                    | sort, segment tree, Dijkstra, binary search on answer|
| n ≤ 10^7–10^8   | O(n)                          | sieve, prefix sums, linear scan (tight at 10^8)     |
| n ≤ 10^18       | O(log n), O(√n) only if ≤10^12| fast exponentiation, digit DP, formula              |

Two consequences you should feel physically:

1. **The bound is a message from the setter.** `n ≤ 20` says "enumerate subsets". `n ≤ 40`
   says "meet in the middle". `n ≤ 5000` says "quadratic is fine, don't bother with n log n".
   `n ≤ 2·10^5` says "n log n or n log² n, and a quadratic solution will TLE by a factor of
   200 — do not even write it".
2. **Logs are constants.** log₂(10^6) ≈ 20, log₂(10^18) ≈ 60. `O(n log n)` and `O(n)` are the
   same class for budgeting; `O(n log² n)` with n = 2·10^5 is 7·10^7 — fine.
   `O(n²)` vs `O(n log n)` is a factor of 10^4 at n = 10^5 — a different universe.

Time limits differ: CSES mostly 1 s, Codeforces 1–3 s, AtCoder 2 s, IOI often 1–3 s with the
grader's per-test overhead. When a limit is 2–4 s and n is 10^5–10^6, the setter expects a
log² or √n factor, or a heavy constant (bitset, FFT).

Memory limits: usually 256 MB (CF, IOI, CSES 512 MB). An `int` array of 10^8 is 400 MB — too
big; 10^7 `long long` is 80 MB — fine; a 5000×5000 `int` table is 100 MB — fine; a
5000×5000 `long long` is 200 MB — dangerously close. Recursion depth 10^6 needs a big stack (§13).

---

## 3. Reading statements

Order of reading, every time:

1. **Constraints first.** They tell you the complexity (§2) and the integer type (§6). Note the
   maximum of every quantity: n, values a_i, number of queries q, coordinates, string length.
2. **Input/output format.** 1-indexed or 0-indexed? Are edges given as `a b` or `a b w`? Is the
   grid given as characters or numbers? Multi-test input (`t` test cases first) — then a
   solution must be **O(sum of n)**, and you must **reset all global state** between cases.
3. **The actual problem**, sample, sample explanation. Re-derive the sample answer by hand;
   that is the fastest way to catch a misread ("at most k" vs "exactly k", "distinct" vs any,
   "subarray" (contiguous) vs "subsequence").
4. **Edge cases before code.** n = 1. All elements equal. Empty answer (0 or −1 or
   "IMPOSSIBLE"?). Answer needs `long long`. Negative numbers allowed? Zero allowed?
   Graph may be disconnected / have self-loops / multi-edges. Coordinates can be negative.
5. **Output format details**: exact strings (`YES` vs `Yes`), precision (`10^-6` absolute or
   relative), any valid answer vs unique answer, order of output.

Notation habits of statements you must decode instantly: "1 ≤ a_i ≤ 10^9" (values fit `int`,
sums do not); "it is guaranteed that a solution exists"; "the sum of n over all test cases
does not exceed 2·10^5"; "print the answer modulo 10^9+7" (every intermediate reduced).

---

## 4. The C++ contest toolkit: headers and I/O

### 4.1 `#include <bits/stdc++.h>`

One header that pulls in the whole standard library. It is a **libstdc++ (GCC) internal
header**, not part of the standard, so it does not exist in Apple clang's libc++. Every judge
you care about uses GCC, so use it in submissions; on macOS make it work locally one of two ways:

**Option A — install GCC (recommended, gives you pbds and `_Find_next` too):**

```sh
brew install gcc            # installs g++-14 (or current version) next to clang
g++-14 -std=c++17 -O2 -Wall -Wextra -o sol sol.cpp
```

Note: plain `g++` on macOS is an alias for clang; you must call the versioned `g++-14`.

**Option B — create the header for clang.** Put a file `bits/stdc++.h` on an include path:

```sh
mkdir -p ~/cp/include/bits
cat > ~/cp/include/bits/stdc++.h <<'EOF'
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
EOF
c++ -std=c++17 -O2 -I ~/cp/include -o sol sol.cpp
```

Either way, alias the command (`alias cpc='...'`) so you never type it. This course ships the
Option-B shim at `learning/competitive_programming/include/bits/stdc++.h` — compile with
`-I learning/competitive_programming/include` from the repo root. The chapter's `example.cpp`
uses explicit standard headers so that plain `c++` compiles it with no flags.

### 4.2 Fast I/O

`cin`/`cout` are synchronised with C `stdio` by default and `cin` is tied to `cout` (every read
flushes output). Two lines at the top of `main` remove both:

```cpp
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    // ...
}
```

After this you **must not mix** `printf/scanf` with `cin/cout` (order of output becomes
undefined). Approximate costs for reading 10^7 integers on a judge:

| Method                                  | Time (approx.) |
|-----------------------------------------|----------------|
| `cin >> x` with sync (default)          | 2.5–4 s        |
| `cin >> x` after `sync_with_stdio(false)` | 0.7–1.0 s    |
| `scanf("%d")`                           | 0.8–1.2 s      |
| `getchar_unlocked`/`fread` custom reader| 0.10–0.20 s    |

`endl` = `'\n'` + **flush**. A flush is a system call; 10^6 `endl` can cost seconds where
10^6 `'\n'` costs milliseconds. Write `'\n'` always. (Exception: interactive problems, where you
must flush after every query — use `cout << x << endl;` or `cout.flush()`.)

The custom reader for the rare problem where input itself is the bottleneck (10^7 numbers in
1 s, e.g. CSES "Sum of Two Values"-style with huge n):

```cpp
static inline int readInt() {
    int c = getchar_unlocked();          // getchar() on Windows judges
    while (c != '-' && (c < '0' || c > '9')) c = getchar_unlocked();
    bool neg = false;
    if (c == '-') { neg = true; c = getchar_unlocked(); }
    int x = 0;
    while (c >= '0' && c <= '9') { x = x * 10 + (c - '0'); c = getchar_unlocked(); }
    return neg ? -x : x;
}
```

`getchar_unlocked` is POSIX (Linux, macOS); on Codeforces (Windows) use `getchar`, or read the
whole input with `fread` into a buffer. Output: build a `string` or use `printf`; `cout` after
sync-off is fine for 10^6 lines.

---

## 5. The personal template

Minimal and readable. You will type it hundreds of times; every line must earn its place.

```cpp
#include <bits/stdc++.h>
using namespace std;
using ll = long long;
#define all(x) begin(x), end(x)

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    cin >> n;
    vector<ll> a(n);
    for (auto& x : a) cin >> x;
    // solve
}
```

Things people add and whether you should:

| Addition                         | Verdict                                                     |
|----------------------------------|-------------------------------------------------------------|
| `using ll = long long;`          | yes — you write it 20× per contest                          |
| `#define all(x) begin(x), end(x)`| yes — `sort(all(a))`, `reverse(all(s))`                     |
| `#define int long long`          | see below                                                   |
| `#define rep(i,a,b) for(int i=a;i<b;i++)` | matter of taste; fine if you can still read it   |
| `#define pb push_back`, `#define F first` | no — saves nothing, obscures code                  |
| `const int INF = 1e9; const ll LINF = 1e18;` | yes; `1e18` leaves room for `LINF + LINF` and `LINF + w` without overflow (`4e18` does not) |
| `const int MOD = 1e9 + 7;`       | yes when needed                                              |
| `mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());` | yes; never `rand()` |

**`#define int long long`.** Makes every `int` a 64-bit integer, so overflow essentially
disappears from your bug list. Costs: `main` must be declared `signed main()` or
`int32_t main()`; all `vector<int>` double in memory and lose some cache speed (~10–30% slower
on memory-bound loops); `%d` in `printf` is wrong; `1 << i` is **still a 32-bit shift** (the
macro renames the keyword, it does not change the type of the literal `1` — write `1LL << i`);
`__builtin_popcount` (32-bit) silently truncates your now-64-bit values — use
`__builtin_popcountll`. Many top competitors use it; many do not.
Recommendation for now: **do not**, and instead learn to see overflow (§6). Revisit after you
have lost a contest problem to overflow.

**Recursive lambdas.** Local recursive functions (a DFS inside `main` capturing the graph) come
in two forms:

```cpp
// (a) std::function: readable, ~2-3x slower per call (type erasure, heap allocation possible)
function<void(int, int)> dfs = [&](int v, int p) {
    for (int u : adj[v]) if (u != p) dfs(u, v);
};
dfs(0, -1);

// (b) self-passing generic lambda: zero overhead, C++14+
auto dfs2 = [&](auto&& self, int v, int p) -> void {
    for (int u : adj[v]) if (u != p) self(self, u, v);
};
dfs2(dfs2, 0, -1);
```

Form (b) needs an explicit return type when the lambda is recursive (the compiler cannot deduce
it from a call to itself). For a DFS over 10^6 nodes, (b) is measurably faster than (a). Plain
global functions with global arrays remain the fastest and simplest for heavy recursion.

---

## 6. Integer overflow discipline

`int` is 32-bit: −2,147,483,648 … 2,147,483,647 ≈ ±2.1·10^9. `long long` is 64-bit:
≈ ±9.2·10^18. `unsigned long long`: 0 … 1.8·10^19. Signed overflow is **undefined behaviour**:
the compiler may assume it never happens, so with `-O2` the result is not "wrong", it is
arbitrary (loops that never end, checks optimised away). Unsigned overflow wraps modulo 2^64 —
defined, and used deliberately in hashing.

The rule: **compute the worst-case intermediate value from the constraints before choosing a
type.** Not the answer — every intermediate.

| Constraint                         | Worst intermediate         | Type            |
|------------------------------------|----------------------------|-----------------|
| a_i ≤ 10^9, sum of n ≤ 2·10^5 of them | 2·10^14                 | `long long`     |
| a_i ≤ 10^9, product of two          | 10^18                      | `long long`     |
| a_i ≤ 10^9, product of three        | 10^27                      | `__int128` or reduce mod first |
| (M−1)² with M = 10^9+7              | ≈ 10^18                    | `long long` — this is why M ≈ 10^9 is chosen |
| n ≤ 10^5, n·(n−1)/2 pairs           | 5·10^9                     | `long long`     |
| distances in Dijkstra, w ≤ 10^9, n ≤ 10^5 | 10^14                | `long long`     |
| 10^9 × 10^9 modulo 10^9+7           | 10^18 before the `%`       | `long long` OK; with modulus ~10^18 need `__int128` |

The single most common bug in the whole sport:

```cpp
int a = 100000, b = 100000;
long long x = a * b;          // WRONG: multiplied as int, overflows, then widened
long long y = (long long)a * b;   // right
long long z = 1LL * a * b;        // right, idiomatic
```

The type of an expression is decided by its operands, not by what you assign it to. Same trap:
`n * (n + 1) / 2` with `int n`, `a * b / gcd(a, b)`, `1 << k` for k ≥ 31 (write `1LL << k`),
`abs(INT_MIN)`, and `mid = (lo + hi) / 2` when `lo + hi` exceeds the type (write
`lo + (hi - lo) / 2`).

**`__int128`** (GCC/clang extension, 128-bit): use for products of two 64-bit values, for
exact comparison of fractions `a/b < c/d` ⇔ `a·d < c·b`, and for modular multiplication with a
modulus near 2^62. No `cin`/`cout` support: convert to `long long` or print by hand.

```cpp
ll mulmod(ll a, ll b, ll m) { return (ll)((__int128)a * b % m); }
```

**Unsigned wraparound for hashing.** `unsigned long long h = h * B + c;` is a polynomial hash
modulo 2^64 with defined overflow. It is fast but **breakable**: the Thue–Morse string
construction produces collisions for every base with modulus 2^64, and Codeforces hackers know
it. Use modulus 2^61−1 with `__int128` multiplication, or a pair of primes near 10^9 with random
bases. Same for `unordered_map`: the default hash is `identity` for integers in libstdc++, and
anti-hash tests force every key into one bucket (O(n²)). Fix: custom hash with `splitmix64`
(shown in `example.cpp`) or use `map`/sorted vectors.

**Negative modulo.** `-7 % 5 == -2` in C++ (truncation toward zero). Always normalise:
`((x % m) + m) % m`.

---

## 7. Floating point in contests

Avoid it when you can: compare `a/b < c/d` as `a*d < c*b`; do geometry with integer cross
products; binary search over integers not reals when the answer is integral. When you cannot:

- `double` has 53 bits of mantissa ≈ 15–16 decimal digits. Integers up to 2^53 ≈ 9·10^15 are
  exact; above that, not.
- `long double` is 80-bit (64-bit mantissa, ~19 digits) on x86 Linux GCC — i.e. on every
  judge. **On Apple Silicon `long double` is identical to `double`** (and on MSVC too). So a
  solution that only passes because of `long double` precision may pass on the judge and fail
  locally, or the reverse; test precision-critical code on the judge's samples.
- Never compare with `==`. Use `fabs(a - b) < EPS` with EPS around 1e-9 for values near 1, or
  a relative epsilon `fabs(a - b) <= EPS * max(1.0, fabs(a), fabs(b))` for large magnitudes.
- Printing: `cout << fixed << setprecision(10) << x << '\n';` or `printf("%.10f\n", x)`
  (`%.10Lf` for `long double`). Match the required precision plus 2–3 digits; when the checker
  accepts absolute *or* relative error 10^-6, 10 digits is plenty.
- `-0.000000` prints with a minus sign on some platforms; add `+0.0` or check `if (x == 0) x = 0;`
  when the checker compares as text.
- `sqrt`, `acos`, `atan2` are slow (tens of ns) — fine for 10^6 calls, not for 10^8.
- Summing 10^6 doubles of mixed magnitude loses digits; sum in `long double` or sort by
  magnitude if the answer is sensitive.

---

## 8. Bit tricks, builtins, `bitset`

GCC/clang builtins (all O(1), one machine instruction each):

| Builtin                          | Meaning                                  | Note                                |
|----------------------------------|------------------------------------------|-------------------------------------|
| `__builtin_popcount(x)`          | number of set bits in `unsigned`         | `popcountll` for 64-bit             |
| `__builtin_ctz(x)`               | count trailing zeros = index of lowest set bit | **undefined for x = 0**       |
| `__builtin_clz(x)`               | count leading zeros                      | undefined for 0; `31 - clz(x)` = ⌊log₂x⌋ for 32-bit; `63 - clzll(x)` for 64-bit |
| `__builtin_parity(x)`            | popcount mod 2                           |                                     |
| `x & -x`                         | lowest set bit as a value                | pure C, no builtin needed           |
| `x & (x - 1)`                    | clear the lowest set bit                 | `x && !(x & (x-1))` ⇔ power of two |
| `(x >> i) & 1`                   | test bit i                               | `1LL << i` when i ≥ 31              |

C++20 has `std::popcount`, `std::countr_zero`, `std::bit_width` in `<bit>`; with `-std=c++17`
you rely on the builtins.

**`std::bitset<N>`**: a fixed-size (compile-time N) array of bits with `&`, `|`, `^`, `<<`,
`>>`, `.count()`, `.any()`, `.set()`, `.reset()`, `.flip()`, `[i]`. Operations on the whole set
run 64 bits per word operation, so an `O(n²)` reachability / subset-sum / Gaussian elimination
over GF(2) becomes `O(n²/64)`: n = 10^4 → 1.5·10^6 word operations. libstdc++ also offers
`bs._Find_first()` and `bs._Find_next(i)` to iterate set bits — **GCC-only** extensions (they do
not exist in Apple clang's libc++; loop over `i` with `bs[i]` locally or use the brew GCC).
Since N is a template parameter, size it to the maximum constraint (`bitset<100001>`).

Classic use: subset sums of values ≤ 10^5 over n ≤ 10^5 items — `dp |= dp << a_i` is
`O(n · S / 64)` ≈ 1.5·10^8 word ops; too slow for 1 s at those extremes but fine when n·S ≤ 10^9.

---

## 9. pbds: `tree` ordered set and `gp_hash_table`

The GNU Policy-Based Data Structures (`<ext/pb_ds/assoc_container.hpp>`) exist only in libstdc++
— fine on every judge (Codeforces GNU G++, CSES, IOI), **absent with Apple clang/libc++**; use
`g++-14` from brew locally.

```cpp
#include <ext/pb_ds/assoc_container.hpp>
using namespace __gnu_pbds;
template <class T>
using ordered_set = tree<T, null_type, less<T>, rb_tree_tag, tree_order_statistics_node_update>;
// ordered_set<int> s;  s.insert(5);
// s.order_of_key(x)  -> number of elements strictly less than x     O(log n)
// *s.find_by_order(k) -> k-th smallest (0-indexed)                   O(log n)
// multiset behaviour: use less_equal<T>  (then erase by  s.erase(s.find_by_order(s.order_of_key(x))) )
```

This is a `std::set` with two extra operations that a plain `set` cannot do in O(log n): rank of
a value and k-th element. It replaces a Fenwick tree over compressed coordinates in "count
elements smaller than x so far" problems (Josephus Problem II, inversion counting, "Salary
Queries"). Constant factor ≈ 2–3× a `std::set`; n = 2·10^5 inserts + queries is ~0.3 s.

`gp_hash_table<K, V, Hash>` is an open-addressing hash table, typically 2–5× faster than
`unordered_map`, and with a custom hash it resists anti-hash tests. Both are covered in the
Data Structures chapter; here you only need to know they exist and where they compile.

---

## 10. Verdicts: TLE / WA / RE / MLE — causes and how to find them

| Verdict | Most likely cause (in order)                                                        | Check                                                            |
|---------|--------------------------------------------------------------------------------------|------------------------------------------------------------------|
| **WA on sample** | misread statement; off-by-one; wrong output format                          | re-read; print intermediate state on the sample                  |
| **WA on hidden test, passes samples** | overflow; edge case (n=1, all equal, empty); uninitialised variable; global state not reset between test cases; `int` mid in binary search; wrong comparator (non-strict `<=`) | stress test (§11) with tiny n first (n ≤ 6), then large values |
| **TLE** | wrong complexity class (check §2 first!); `endl`; `cin` without sync; `unordered_map` anti-hash; passing vectors by value; `memset` of a 10^7 array per test case; `std::function` in hot recursion; `pow()` for integer powers; `string +=` in a loop building 10^6 chars is fine, `s = s + c` is O(n²) | count operations; time locally on max-size random input |
| **RE** | array out of bounds (often index n or −1); stack overflow from recursion depth; division by zero / `%` by 0; `vector` resized smaller then accessed; `*s.begin()` on empty set; `pop` on empty stack; `1 << 32` | compile with `-fsanitize=address,undefined -g` and run the failing input |
| **MLE** | `new` per node; `vector<vector<int>>` of 10^5×10^5 "just in case"; `bitset` as a local in recursion; recursion depth 10^7 | compute bytes: `count × sizeof` |
| **Idleness limit** (interactive) | forgot to flush                                                          | `cout << endl` / `fflush(stdout)` after every query               |

Judge-specific gotchas: Codeforces runs on **Windows** — `rand()` has `RAND_MAX = 32767`, so
`rand() % n` with n = 10^5 only hits the first third of your array (use `mt19937`); `%lld` is
required for `long long` in `printf`; stack is 256 MB so recursion is safe. CSES/IOI run Linux
with GCC; recursion depth 10^6 is fine because stack limits are raised.

Debugging order that works under time pressure: (1) re-read constraints and statement,
(2) hand-test n = 1 and the smallest tricky case, (3) sanitizers on the sample and a random
input, (4) stress test vs brute force. Do not "fix by intuition" more than twice; go to (4).

---

## 11. Stress testing

The most powerful debugging tool in the sport. Three programs plus a loop:

1. `sol.cpp` — your fast solution.
2. `brute.cpp` — the dumbest correct solution you can write (exponential is fine; it only
   runs on n ≤ 8). Written *independently*: different approach, different variable names.
3. `gen.cpp` — random input generator taking a seed, producing **small** cases (small n, small
   values: bugs show up at n ≤ 6 far more often than at n = 10^5, and small values force
   collisions/ties).

```cpp
// gen.cpp
#include <bits/stdc++.h>
using namespace std;
int main(int argc, char** argv) {
    mt19937 rng(argc > 1 ? atoi(argv[1]) : 1);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    int n = rnd(1, 6);
    cout << n << '\n';
    for (int i = 0; i < n; i++) cout << rnd(1, 10) << " \n"[i == n - 1];
}
```

```bash
#!/bin/bash
# stress.sh — usage: ./stress.sh [iterations]
set -e
c++ -std=c++17 -O2 -o sol sol.cpp
c++ -std=c++17 -O2 -o brute brute.cpp
c++ -std=c++17 -O2 -o gen gen.cpp
for ((i = 1; i <= ${1:-1000}; i++)); do
    ./gen "$i" > in.txt
    ./brute < in.txt > out_brute.txt
    ./sol   < in.txt > out_sol.txt
    if ! cmp -s out_brute.txt out_sol.txt; then
        echo "MISMATCH on seed $i"
        echo "--- input";  cat in.txt
        echo "--- brute";  cat out_brute.txt
        echo "--- sol";    cat out_sol.txt
        exit 1
    fi
done
echo "all ${1:-1000} tests passed"
```

For problems with **multiple valid answers**, replace `cmp` with a `check.cpp` that reads input
and both outputs and verifies validity + optimal value. For **TLE hunting**, make a second
generator that produces maximal n with adversarial structure (sorted input, all equal, a path
graph, a star) and run `time ./sol < big.txt`.

When the loop finds a mismatch you have a 6-element failing case; trace it by hand or with `dbg`
in two minutes. Total setup cost: 3–5 minutes. It pays off from the first use. In an IOI-style
contest with full feedback but hidden tests, this is how you convert "WA on subtask 3" into a
fix.

---

## 12. Debugging macros

Print any number of expressions with their names, compiled out in submissions:

```cpp
#ifdef LOCAL
#define dbg(...) cerr << "[" << #__VA_ARGS__ << "] = ", dbg_out(__VA_ARGS__)
#else
#define dbg(...) ((void)0)
#endif

template <class T> void dbg_print(const T& x) { cerr << x; }
template <class A, class B> void dbg_print(const pair<A, B>& p) {
    cerr << '(' ; dbg_print(p.first); cerr << ", "; dbg_print(p.second); cerr << ')';
}
template <class T, class = decltype(begin(declval<T>()))>
enable_if_t<!is_same<T, string>::value> dbg_print(const T& v) {
    cerr << '{'; bool f = true;
    for (auto& e : v) { if (!f) cerr << ", "; f = false; dbg_print(e); }
    cerr << '}';
}
void dbg_out() { cerr << '\n'; }
template <class H, class... T> void dbg_out(const H& h, const T&... t) {
    dbg_print(h); if (sizeof...(t)) cerr << ", "; dbg_out(t...);
}
```

Compile locally with `-DLOCAL`; the judge compiles without it, so every `dbg(...)` disappears —
you never delete debug output before submitting and never leak it into stdout. Output goes to
`cerr`, which is unbuffered and does not interfere with the checked stdout even if you forget
`-DLOCAL`… but it does cost time, so keep the macro. Usage: `dbg(n, a, dp[i][j], make_pair(l, r))`
prints `[n, a, dp[i][j], make_pair(l, r)] = 5, {1, 2, 3}, 7, (0, 4)`.

Other local-only tools: `assert(invariant)` liberally (it is removed by `-DNDEBUG`, which
judges do not set — so asserts *stay on* in submissions; a failed assert is an RE, which is
often more informative than a WA in full-feedback contests, and it is a legitimate technique for
learning which subtask breaks).

---

## 13. Local judge setup

Directory per contest/chapter; one file per problem; inputs next to it.

```
contest/
  A.cpp  A1.in  A2.in
  B.cpp  B1.in
  stress/  (gen.cpp brute.cpp stress.sh)
```

Compile flags for local runs (a shell alias or a Makefile rule):

```sh
# correctness build: warnings, sanitizers, debug symbols, dbg() enabled
c++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Wconversion -DLOCAL \
    -fsanitize=address,undefined -g -o A A.cpp
# speed build: what the judge does
c++ -std=c++17 -O2 -o A A.cpp
./A < A1.in                       # run on a sample
time ./A < big.in > /dev/null     # measure; "user" time is what counts
```

`-Wshadow` catches the classic `int n` inside a function hiding a global `n`. `-Wconversion` is
noisy but catches `int x = some_long_long`. Sanitizers slow the program 2–3× — fine for
samples and stress tests, not for timing.

**Stack size.** Default main-thread stack is 8 MB on both Linux and macOS. A DFS with 10^6 depth
uses ~50–100 bytes per frame minimum — 100 MB. Judges raise the limit (CF 256 MB; CSES and IOI
effectively unlimited); locally you must:

```sh
# Linux (per shell session)
ulimit -s unlimited
# macOS: the kernel caps ulimit -s at ~64 MB; set the size in the binary instead (hex bytes):
c++ -std=c++17 -O2 -Wl,-stack_size,0x20000000 -o A A.cpp     # 512 MB
```

The macOS flag applies only to the main thread and only to executables (not with
`-fsanitize=address` in some versions — build twice if needed). Alternative that works
everywhere: run the recursive part inside a `std::thread`… whose stack size cannot be set
in standard C++; use `pthread_attr_setstacksize` — or simply avoid: convert to an explicit stack,
or trust the judge.

Compare with the judge: Codeforces "GNU G++17 7.3.0" and "G++20 (64)"; CSES `g++ -std=c++17
-O2`; IOI `g++ -std=gnu++17 -O2 -static -pipe` and generally **no `-DLOCAL`**, hence the macro.

Timing sanity: a `for` loop of 10^8 additions runs ~0.1 s locally with `-O2`. If your
solution on max input takes 0.5 s locally, expect 0.5–1.5 s on the judge — machines differ by
2× either way; aim for ≤ half the limit locally.

---

## 14. Time management within a contest

**Codeforces round (2 h, problems A–F):**

- 0–2 min: read A, solve A, submit. Do not test beyond the samples if it is trivial; a −50 is
  cheaper than 3 minutes.
- Then B, C with the same rhythm but with a 30-second edge-case check (n=1, overflow).
- **Read every problem** before the 30-minute mark, even if stuck on C. Problem order is not
  difficulty order for you; D may be your specialty. Standings show which problem people are
  solving — that is information.
- Stuck 20 minutes without a new idea → switch problem, come back later. Stuck 10 minutes on a
  bug → stress test (§11), do not re-read the same 40 lines a fifth time.
- Last 15 minutes: no new problems; finish the one closest to done; verify no `dbg`/`endl` in
  the submission.

**Olympiad day (5 h, 3 tasks, subtasks):**

- 0–20 min: read all three tasks fully, including all subtasks. Write, for each, the subtasks
  you can certainly do and roughly how long each takes.
- 20–90 min: implement the easy subtasks on all three (typically 20–40 points each). This
  banks 60–120 points and, more importantly, gets a working, tested brute force for stress
  testing the full solutions later.
- Then attack the task where the jump from "subtask I have" to "full" looks shortest for you.
  Each hour, re-decide: is another 40 min on this likely to give more points than a medium
  subtask elsewhere?
- Last 45 min: no new theory. Convert half-done ideas into partial submissions; make sure the
  best-scoring submission of each task is the last one if the rules score the last (IOI scores
  the max over submissions per subtask, most national contests too — check the rules).
- Full-feedback tactic: a submission with `assert` on a suspected property tells you which
  subtask violates it (RE vs WA). Use sparingly; submission limits exist.

**ICPC (team, 5 h, one keyboard):** one person types the easy ones while two read; whoever
solves a problem on paper codes it; keyboard time is the bottleneck — write code on paper
first when the machine is busy; debug on paper printouts; announce claims ("I think F is a
DP over…") loudly and let the others attack the claim.

---

## 15. Upsolving and tracking progress

**Upsolving** = solving, after the contest, the problems you did not solve during it. It is
where rating actually comes from; the contest itself only measures.

- Within 48 hours of every contest: solve at least the first problem you failed, without the
  editorial for the first 45–60 minutes of trying. Then read the editorial *only up to the key
  idea*, close it, and implement.
- For an olympiad set: upsolve until you have full score on every task rated up to your
  target level, even if it takes days.
- Keep a **problem log** (a text file or spreadsheet): date, source+id, verdict in contest,
  time spent, the technique, and one line "what I missed" (e.g. "didn't consider n=1",
  "didn't see it was binary search on answer", "overflow in mid"). Re-read the "what I missed"
  column monthly; the patterns are your training plan.
- Redo problems that needed the editorial 1–2 weeks later from a blank file.

**Tracking.** CSES problem set progress (this course walks through it section by section);
Codeforces rating and, more informatively, the *rating of problems you solve in under 30
minutes*; count of upsolved problems per week. A realistic path: CF 1400 → 1900 needs roughly
300–500 solved problems slightly above your level, over 6–12 months of weekly contests plus
upsolving. IOI medal territory corresponds to ~2000–2200+ and comfortable full scores on BOI
tasks.

---

## Recognition cheatsheet

| Statement signal                                       | What it tells you                          | Section |
|--------------------------------------------------------|--------------------------------------------|---------|
| `n ≤ 20`                                               | 2^n subsets / bitmask DP                   | §2      |
| `n ≤ 40`                                               | meet in the middle                         | §2      |
| `n ≤ 500`                                              | O(n³)                                      | §2      |
| `n ≤ 5000`                                             | O(n²)                                      | §2      |
| `n, q ≤ 2·10^5`                                        | O((n+q) log n): sort, BS, seg tree         | §2      |
| `n ≤ 10^7`, 1 s                                        | O(n), sieve/prefix, fast input needed      | §2, §4  |
| `a_i ≤ 10^9` and sums/products asked                   | `long long`; products of 3 → `__int128`    | §6      |
| "modulo 10^9+7"                                        | reduce every step; `(x%m+m)%m` after minus | §6      |
| `t` test cases, "sum of n ≤ …"                         | O(sum n); reset state; never `memset` big  | §3, §10 |
| "print any valid answer"                               | write a checker for stress tests           | §11     |
| "absolute or relative error 10^-6"                     | `fixed << setprecision(10)`                | §7      |
| interactive / "flush your output"                      | `endl` after each query                    | §4      |
| deep tree, n = 10^6, "the tree may be a path"          | stack size or iterative DFS                | §13     |
| CF round with hacks; `unordered_map` in your code      | custom hash or `map`                       | §6, §9  |

---

## Implementation checklist for contests

Before every submission:

1. Constraints re-read; complexity of the written code matches the table for max n.
2. Every sum/product: worst-case magnitude computed; `long long` where > 2·10^9; `1LL <<`.
3. `ios::sync_with_stdio(false); cin.tie(nullptr);` present; no `endl` (unless interactive).
4. Multi-test: all arrays/maps/counters cleared per test case, in O(size of the test), not O(max).
5. Edge cases run by hand: n = 1, all equal, answer 0/−1/impossible, min and max values.
6. Off-by-one on every range: `[l, r]` vs `[l, r)`, 1-indexed input into 0-indexed arrays.
7. Comparators are strict weak orderings (`<`, never `<=`) — `sort` with `<=` is UB and crashes.
8. Recursion depth ≤ 10^5, or stack is handled / iterative version used.
9. Output format exact: strings, precision, order, trailing newline, one answer per line.
10. No `dbg` output to `cout`; `assert`s you intended to keep are intentional.

---

## Further reading

- Laaksonen, *Competitive Programmer's Handbook* (CPH): ch. 1 (Introduction: template, I/O,
  types, shortening code), ch. 2 (Time complexity — the table is from there), ch. 10 (Bit
  manipulation).
- Laaksonen, *Guide to Competitive Programming* (Springer edition of CPH) ch. 1–2.
- cp-algorithms.com: "Bit manipulation"; "Random number generation" notes in the article on
  hashing (String Hashing — anti-hash section).
- Codeforces blogs (search by title): "Blowing up unordered_map, and how to stop getting hacked
  on it" (neal); "C++ STL: Policy based data structures" (adamant); "Anti-hash test" (Thue–Morse
  construction); "How to test a solution / stress testing" (errichto's video on testing).
- IOI syllabus (ioinformatics.org) — the official list of what can and cannot appear.
- The Codeforces problemset filter by rating — use it as your ladder generator.

---

## You can move on when...

- You can write the template, the fast reader and the `dbg` macro from memory, compile with
  zero warnings, and have `stress.sh` set up in your contest directory.
- Given a constraint line you name the intended complexity class in under 10 seconds, for all
  rows of the table.
- You can list three causes each for TLE, WA-on-hidden-tests and RE, and the first check for each.
- You have solved all 24 CSES Introductory problems in `problems.md` and at least 10 CF
  problems rated 800–1200 in under 15 minutes each.
- You have done one full virtual Codeforces Div2/Div3 round and upsolved the first unsolved
  problem within 48 hours, and started your problem log.
