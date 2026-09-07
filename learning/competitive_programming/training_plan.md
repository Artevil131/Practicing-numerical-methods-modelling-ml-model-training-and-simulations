# Training plan — to IOI-finalist level

Target: Datatähti final → Finnish team selection → BOI → IOI. Secondary: ICPC (NWERC) at university.
Honest scale: IOI medal ≈ Codeforces 2000–2200+; a *finalist* (team member) in Finland is realistic at
CF 1800–2000 with strong implementation. From zero C++ this is 18–30 months of steady work. The plan
below is 12 months to the first serious Datatähti attempt; then it repeats at higher difficulty.

## Principles

1. **Implement from memory.** Every structure in a chapter's `example.cpp` gets re-typed blind until it
   comes out right first time. That is the library you'll have in your head at IOI (no internet there).
2. **Solve, don't read.** A problem counts when your code passes. Reading an approach counts as zero.
   Hints are for after 30–45 minutes of real attempt.
3. **Upsolve everything.** After a contest, solve every problem you didn't get, within 3 days.
4. **Stress-test by default.** Brute force + random generator + diff script for anything non-trivial.
   Most WA's in training are found this way in 5 minutes, not 50.
5. **Timed work weekly.** Virtual contests (Codeforces virtual participation, past Datatähti/BOI sets)
   under real time limits. Speed and nerve are trained separately from knowledge.
6. **Keep a log.** `competitive_programming/log.md`: date, problems, time per problem, what went wrong.
   The pattern in your mistakes is the curriculum for the next month.

## Phase A — foundation (months 1–4)  ‖ runs alongside c_learning / algorithms_learning

| Week | Do |
|---|---|
| 1–2 | CP 01. All 24 CSES Introductory. Set up template, stress-test script, `bits/stdc++.h` shim, local judge folder. |
| 3–6 | CP 02. All 35 CSES Sorting and Searching. Start Codeforces: Div3/Div4 virtuals weekly, aim to finish A–C. |
| 7–10 | algorithms_learning 06–08 (in C++ now). CSES Graph Algorithms first 12 (BFS/DFS/Dijkstra). CSES DP first 12. |
| 11–16 | CP 07 (range queries) + CP 03 (bits). All CSES Range Queries. Weekly Div2 virtual, target A–C solid, D sometimes. |

Exit criteria: CF ~1400. CSES Introductory, Sorting, Range Queries complete. 40+ DP and graph tasks.

## Phase B — the IOI core (months 5–8)

| Week | Do |
|---|---|
| 17–20 | CP 10 (trees) + CP 11 (graphs advanced). Finish CSES Tree Algorithms and Graph Algorithms. |
| 21–24 | CP 13 (DP advanced) + CP 04/05 (number theory, combinatorics). Finish CSES DP, start Mathematics. |
| 25–28 | CP 08 (DSU/offline) + CP 09 (treaps/structures) + CP 06 (matrices). Advanced Techniques first half. |
| 29–32 | CP 14 (strings) + CP 12 (flows). CSES Strings complete; Advanced Graph Problems half. |

Weekly: one Div2 virtual (target A–D), one 2.5 h block of 3 past Datatähti/BOI problems with subtask scoring.
Exit criteria: CF ~1700. Can implement segtree-lazy, LCA, SCC, Dinic, SA, treap from memory in < 15 min each.

## Phase C — contest form (months 9–12)

| Week | Do |
|---|---|
| 33–36 | CP 15 (geometry) + CP 16 (games/constructive/interactive). CSES Geometry, Bitwise, Construction, Interactive. |
| 37–44 | Past olympiads under exam conditions: BOI (2015→), CEOI, JOI Final, USACO Gold/Platinum. IOI past problems by subtask. Alternate with Div1 virtuals (A–B). |
| 45–48 | Weak-spot loop: log → top 3 failure patterns → 10 targeted problems each. CSES Additional Problems I as mocks. |
| 49–52 | Taper: 2 mocks/week, review library, sleep. Datatähti. |

Exit criteria: CF ~1900–2000. Comfortable with 5 h / 3 problems format and subtask strategy.

## Weekly template (≈ 12–15 h)

| Day | Block |
|---|---|
| Mon | Chapter reading + re-type library from memory (1.5 h) |
| Tue | 3–4 ladder problems (2 h) |
| Wed | 3–4 ladder problems (2 h) |
| Thu | Chapter reading + library (1.5 h) |
| Fri | Off or light upsolving |
| Sat | **Timed**: virtual contest or olympiad set, 2.5–5 h |
| Sun | Upsolve Saturday fully; log; pick next week's weak spot (2 h) |

## Problem sources (in order of use)

1. **CSES** — `ladder.md` (400 tasks; all mapped to chapters). Backbone.
2. **Codeforces** — virtuals and problemset by tag+rating. Practice ranges given per chapter.
3. **AtCoder** — ABC (E–G) for clean algorithmic problems; ARC for thinking.
4. **Olympiads** — Datatähti archive (datatahti.fi), BOI (boi.cses.fi), CEOI, JOI, IOI (ioinformatics.org; oj.uz for judging).
5. **USACO Guide** (usaco.guide) — an excellent free structured syllabus; use its Gold/Platinum modules as a second explanation when a chapter here doesn't click.
6. **cp-algorithms.com** — the reference for any algorithm's details.

## Books

- Laaksonen, *Competitive Programmer's Handbook* (free PDF) — read fully once, then per chapter. Finnish author, CSES companion.
- Laaksonen, *Guide to Competitive Programming* (Springer) — the polished version.
- Halim & Halim, *Competitive Programming 4* — breadth reference.
- Cormen et al., *Introduction to Algorithms* — for proofs when you want them.

## For ICPC later

Team practice is different: 5 h, 3 people, 1 computer, 10–13 problems. Add: reading fast, problem
triage, parallel thinking, a shared team notebook (25 pages of library — start yours now from the
`example.cpp` files), and a lot of geometry and math that IOI rarely asks. NWERC problem archive.
