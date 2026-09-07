# Algorithms Course (LeetCode patterns, in C)

Eight chapters, 381 problems. Transferred from my LeetCode study app (School/olympia) and
rewritten in English in the same style as the C/C++ courses. The point is not LeetCode itself:
these are the algorithmic patterns that the ML / numerics / simulation projects are built from
(hash tables → tokenizer vocab; heaps → k-NN and Barnes–Hut; DP → edit distance / Viterbi;
BFS/Dijkstra → pathfinding; union-find → mesh components; binary search on the answer → line search).

Every chapter folder has:

| File          | What it is                                                                          |
|---------------|-------------------------------------------------------------------------------------|
| `lesson.md`   | The patterns, one section per unit: idea, invariant, recognition, complexity, C layout, skeleton, traced example, pitfalls. |
| `problems.md` | The LeetCode tasks grouped by unit. Hint, key idea, and approach write-up are each collapsed — open them in that order, only when stuck. Progress checklist at the end. |
| `example.c`   | The chapter's pattern skeletons on neutral inputs. Compile, run, read. Not solutions to the listed problems. |
| `lesson_python.md` | The same lesson taught in Python (dict/set/deque/heapq/bisect skeletons, Python-specific gotchas). Read side by side with `lesson.md`. |
| `example.py`  | Python port of `example.c`; `python3 example.py`. Run both, compare. |

## How to work a chapter

```sh
cd algorithms_learning/01_arrays_and_hashing
cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
mkdir -p solutions
# for each problem: read the unit's section in lesson.md, then
#   solutions/<slug>.c   — your solution with your own tests in main()
cc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined -o solutions/two-sum solutions/two-sum.c -lm && ./solutions/two-sum
```

Rules: attempt in C first. Open the hint only after a real attempt, the approach only after
solving or 30 minutes stuck. Tick the box in `problems.md` when your tests pass. Redo a problem
a week later without looking if the first attempt needed the approach.

## Chapters

| #  | Chapter                                   | Units | Problems | Prereq C chapters       |
|----|-------------------------------------------|-------|----------|-------------------------|
| 01 | `01_arrays_and_hashing`                   | 6     | 49       | 01–07, 10 (hash table)  |
| 02 | `02_two_pointers_and_sliding_window`      | 6     | 48       | 01–05                   |
| 03 | `03_binary_search`                        | 6     | 46       | 01–05, 12 (overflow)    |
| 04 | `04_stack_and_monotonic_stack`            | 6     | 47       | 01–07, 10               |
| 05 | `05_trees_and_bsts`                       | 6     | 46       | 03 (recursion), 06, 07, 10 |
| 06 | `06_graphs`                               | 6     | 45       | 06, 07, 10 (queue, heap, union-find) |
| 07 | `07_dynamic_programming`                  | 7     | 55       | 04 (2D arrays), 06, 12  |
| 08 | `08_greedy_and_intervals`                 | 6     | 45       | 10 (heap), 11 (qsort comparator) |

Recommended order is 01 → 08. Chapters 01–04 are doable during C chapters 05–10; start 05–08
after finishing C chapter 10 (data structures).

## Where this fits

- `../c_learning/` — the language. Do C chapters 01–07 before starting here.
- `../c_learning/projects/` — the ML/sim projects; several list algorithm chapters as prereqs
  (P03 BPE ← 01; P07 k-NN ← 08 heap; P10 trees ← 05; P15 Barnes–Hut ← 05/06).
- Source of the material: `~/School/olympia/content/lc/*.json` (Finnish; kept as the original).
