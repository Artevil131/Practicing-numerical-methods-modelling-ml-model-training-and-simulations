# Competitive Programming Course (IOI / ICPC track)

Sixteen chapters covering everything from contest setup to flows, suffix automata, DP optimizations
and geometry — the material that sits *above* the LeetCode patterns in `../algorithms_learning/`.
Written for the path Datatähti → BOI → IOI, with ICPC in mind for later. Everything in C++17.

| File | What it is |
|---|---|
| `NN_slug/lesson.md` | Textbook chapter: idea, proof sketch, complexity, canonical implementation, traced example, variants, pitfalls, recognition table. |
| `NN_slug/problems.md` | Curated ladder for the chapter: CSES tasks (verified IDs), Codeforces practice ranges, olympiad problems. Hint + approach sketch collapsed. Progress checklist. |
| `NN_slug/example.cpp` | The chapter's reference library, tested with asserts against brute force. **Re-type it from memory** — that's the training. |
| `ladder.md` | All 400 CSES tasks as one checklist, mapped to chapters and phases. |
| `training_plan.md` | 12-month plan with weekly template, exit criteria, sources. |
| `include/bits/stdc++.h` | Shim so `#include <bits/stdc++.h>` works on Apple clang: `-I learning/competitive_programming/include`. |

## Setup

```sh
# compile a solution
c++ -std=c++17 -O2 -Wall -Wextra -I learning/competitive_programming/include -o sol sol.cpp
# debug build (catches UB and out-of-bounds — use it on every WA)
c++ -std=c++17 -O0 -g -fsanitize=address,undefined -D_GLIBCXX_DEBUG -I learning/competitive_programming/include -o sol sol.cpp
# deep recursion (2e5-deep DFS) on macOS: raise the stack at link time
c++ ... -Wl,-stack_size,0x20000000 -o sol sol.cpp
# optional: real GCC for libstdc++ extensions (pbds ordered_set, bitset::_Find_first)
brew install gcc && g++-14 -std=c++17 -O2 -o sol sol.cpp
```

Solutions go in `NN_slug/solutions/<source>_<id>.cpp`. Compiled binaries are git-ignored.

## Chapters

| # | Chapter | Covers |
|---|---|---|
| 01 | `01_contest_fundamentals` | Formats, complexity budget, template, fast I/O, stress testing, debugging |
| 02 | `02_complete_search_and_backtracking` | Subsets/permutations, pruning, meet in the middle, ternary search, greedy proofs |
| 03 | `03_bits_and_bitsets` | Bit tricks, submask enumeration, SOS DP, XOR basis, `bitset` speedups |
| 04 | `04_number_theory` | gcd/ext-Euclid, modular inverse, CRT, sieves, Miller–Rabin, Pollard rho, φ, Möbius, BSGS |
| 05 | `05_combinatorics_and_probability` | nCr mod p, Lucas, inclusion–exclusion, Catalan, Burnside, Stirling, expected value |
| 06 | `06_matrices_and_linear_algebra_mod_p` | Matrix exponentiation, linear recurrences, Gauss over ℝ/GF(p)/GF(2), min-plus |
| 07 | `07_range_queries` | Sparse table, Fenwick (1D/2D/range), segment tree + lazy, persistent, merge-sort tree, Mo's |
| 08 | `08_dsu_offline_and_sqrt_techniques` | DSU rollback, offline dynamic connectivity, DSU on tree, parallel binary search, sqrt decomposition |
| 09 | `09_balanced_trees_and_advanced_structures` | Treap (explicit/implicit), pbds, `set` tricks, Li Chao, CHT structure |
| 10 | `10_trees` | Euler tour, LCA (binary lifting / RMQ), HLD, centroid decomposition, rerooting |
| 11 | `11_graphs_advanced` | Shortest paths (all), Euler paths, SCC, 2-SAT, bridges/articulation, MST variants, functional graphs |
| 12 | `12_flows_and_matching` | Dinic, min cut, Kuhn/Hopcroft–Karp, König, Dilworth, project selection, min-cost flow |
| 13 | `13_dp_advanced` | Tree/digit/bitmask/broken-profile DP, Knuth, D&C, CHT, slope trick, Aliens trick, DP + data structures |
| 14 | `14_strings` | Hashing, Z, KMP, Aho–Corasick, suffix array + LCP, suffix automaton, Manacher, Lyndon |
| 15 | `15_geometry` | Integer geometry, segment intersection, hull, rotating calipers, closest pair, sweep, half-planes |
| 16 | `16_game_theory_constructive_and_misc` | Nim, Sprague–Grundy, retrograde analysis, constructive, interactive, randomized, IOI strategy |

Order: 01 → 02 → 03 → 07 → 10 → 11 → 13 → 04 → 05 → 08 → 09 → 06 → 14 → 12 → 15 → 16 (this is the
order in `training_plan.md`). Chapters 04–06 are independent and can be slotted anywhere after 02.

## Prerequisites

`../c_learning/` 01–14, `../cpp_learning/` 01–08 at least (STL, lambdas, templates), and all of
`../algorithms_learning/`. Chapters here reference those instead of repeating them.
