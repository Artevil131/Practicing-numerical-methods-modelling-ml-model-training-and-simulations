# Roadmap

## Where I am (2026-09-04)

- Know: Python, NumPy, PyTorch (as a user).
- Zero C / C++.
- Goal: understand ML/DL and physics simulation down to the math **and** the memory. Build
  everything from scratch in C (then C++) before touching Python libraries again.
- Learning style: minimal theory up front, start coding immediately, learn C *through* the
  ML/numerics problems rather than before them.

## Tooling (already installed)

```sh
cc --version            # Apple clang 21, arm64
cc -Wall -Wextra -O2 -o prog file.c -lm     # compile one file (-lm links the math lib)
./prog                                      # run
cc -g -fsanitize=address,undefined -o prog file.c -lm   # debug build: catches bad memory access
```

Mental mapping from what I know:

| NumPy / Python           | C                                                      |
|--------------------------|--------------------------------------------------------|
| `np.zeros(n)`            | `double *a = calloc(n, sizeof(double));` ... `free(a);` |
| `a.shape = (rows, cols)` | you carry `rows`, `cols` yourself; data is flat `a[i*cols + j]` |
| `a @ b`                  | triple `for` loop you write                             |
| `list`                   | array + length + capacity, grow with `realloc`          |
| `dict`                   | none built in — write a hash table (this is a lesson)   |
| `class`                  | `struct` + functions that take a pointer to it          |
| `str`                    | `char *` ending with `'\0'`                             |
| GC                       | none — every `malloc` needs a matching `free`           |

## Where things live

```
ROADMAP.md              this file — position + plan
learning/
  c_learning/           14 base + 6 advanced chapters (lesson + 10 exercises + example) + 16 project specs
  cpp_learning/         13 base + 7 advanced chapters (same format)                     + 12 project specs
  algorithms_learning/  8 chapters of LeetCode patterns (381 problems) to solve in C — from my School/olympia app
  competitive_programming/ 16 chapters to IOI level (segtrees, flows, SA/SAM, DP opts, geometry…) + CSES ladder (400) + training plan
neural_network_c/       my ML implementations in C  (tokenizer, matrix lib, regression, MLP, autograd, LM)
numerical_methods/      ODE/PDE solvers, linear algebra, FFT (create when I get there)
simulations/            donut, N-body, FDTD, PIC, fluids            (create when I get there)
cpp/                    C++ implementations                          (create when I get there)
```

## Phase 0 — C by doing  `c_learning/`

Work chapters 01–07 (`c_learning/README.md`), doing all 10 exercises each. Then start
projects P01 (char tokenizer) and P02 (matrix library) **immediately** — finish chapters
08–14 alongside the projects that need them (each project lists its prereq chapters).

- [ ] `neural_network_c/00_c_basics.c` — read, compile, run, modify (provided, annotated)
- [ ] C chapters 01–07 + exercises
- [ ] P01 char tokenizer → `neural_network_c/tokenization.c`
- [ ] P02 matrix library → `neural_network_c/matrix.{h,c}` + tests + Makefile
- [ ] C chapters 08–14 + exercises (interleaved with projects below)
- [ ] P03 BPE tokenizer, P04 MNIST loader
- [ ] Algorithms chapters 01–04 (arrays/hashing, two pointers, binary search, stack) — a few
      problems a day in C alongside the above; 05–08 (trees, graphs, DP, greedy) after C ch10

## Phase 1 — Classical ML in C

Each one: derive the gradient on paper first, then code it, then check against PyTorch.

- [ ] Linear regression — closed form (normal equations) **and** gradient descent.
- [ ] Logistic regression — sigmoid, binary cross-entropy, gradient, SGD.
- [ ] Linear classifier (softmax regression) on MNIST — multi-class cross-entropy.
- [ ] k-NN and k-means — no gradients, teaches distance loops and argmin.
- [ ] Decision tree — recursion, Gini/entropy, splits. Then random forest (bagging).
- [ ] Naive Bayes.

## Phase 2 — Neural networks in C

- [ ] 2-layer MLP on MNIST, backprop written by hand (matrix form).
- [ ] Generalize: `Layer` struct, arbitrary depth, ReLU/tanh/sigmoid, SGD → momentum → Adam.
- [ ] Gradient checking (finite differences) — proves your backprop is right.
- [ ] Scalar autograd (micrograd in C): a computational graph of nodes with `.grad`.
      Then tensor autograd. This is "how everything flows" in PyTorch.
- [ ] Char-level language model: bigram → MLP → tiny transformer (attention, layernorm,
      embeddings) trained on a text file. Ties back to the tokenizer.

## Phase 3 — C++ (start once C feels natural)  `cpp_learning/`

13 chapters + 12 projects (`cpp_learning/README.md`). Redo `Matrix` with classes, RAII,
templates, operator overloading; then `Tensor<T>`, tensor autograd, an NN framework, and a tiny
transformer. Learn what C++ buys you over C and what it costs.

## Phase 4 — Numerical methods  `numerical_methods/`

- [ ] Root finding: bisection, Newton, secant.
- [ ] Linear algebra: Gaussian elimination, LU, QR, power iteration, Jacobi eigen.
- [ ] Interpolation: Lagrange, splines.
- [ ] Integration: trapezoid, Simpson, Gauss quadrature.
- [ ] ODEs: Euler, RK4, adaptive step, symplectic (leapfrog / Verlet).
- [ ] PDEs: finite differences — heat equation, wave equation, stability (CFL).
- [ ] FFT.

## Phase 5 — Physics simulations  `simulations/`

Output as PPM/PGM image frames or CSV → assemble with Python/ffmpeg for viewing.

- [ ] Spinning donut (torus) — ASCII renderer. Rotation matrices, z-buffer, shading.
- [ ] N-body gravity — Verlet, then Barnes–Hut tree (reuses tree code from Phase 1).
- [ ] Electromagnetic waves — FDTD (Yee grid) for Maxwell's equations in 2D.
- [ ] Plasma — particle-in-cell (PIC) 1D electrostatic, then 2D.
- [ ] Fluids — Lattice Boltzmann or Navier–Stokes on a grid.

## Expected level after the three courses (assessed 2026-09-05)

- C: end-of-systems-course level; C++: solid intermediate; algorithms: LeetCode confident-Medium,
  Codeforces ≈ 1400–1600 if problems are actually solved. ML/sims projects: implementation-level
  understanding that most practitioners never get. ETA ~6–9 months at 1.5–2 h/day.

## Competition goals (IOI → ICPC at uni; IMO separately)

- IOI/ICPC need a second layer the LeetCode canon lacks: segment/Fenwick trees, sparse tables,
  modular arithmetic & combinatorics, string hashing/KMP/suffix structures, SCC/bridges/flow/
  matching, DP on trees / digit / bitmask + optimizations, geometry, speed on unlabeled problems.
  Medal territory ≈ CF 2000+. Finnish pipeline: Datatähti → camps → BOI → IOI; ICPC via NWERC.
- Resources: CSES problem set + Laaksonen, *Competitive Programmer's Handbook*; olympia `ioi/` track.
- Switch algorithm solving to **C++** after C++ ch04. Contests are never done in C.
- IMO is a separate discipline (olympiad math + proof writing); programming courses carry no
  weight. Path: MAOL → Nordic → Baltic Way → IMO; olympia `imo/` track.
- Plan: `competitive_programming/training_plan.md` — 12-month plan in 3 phases (foundation ‖ C
  course → IOI core → contest form), weekly template, exit criteria per phase (CF 1400 → 1700 →
  1900–2000). Ladder: `competitive_programming/ladder.md` (all 400 CSES tasks mapped to chapters).

## Phase 6 — professional depth (after Phases 2–3)

- [ ] C 15–20: UB & the standard, pthreads/atomics, POSIX, allocators, SIMD, professional engineering
      (incl. calling my C matrix lib from Python via ctypes — closes the loop back to NumPy)
- [ ] C++ 14–20: metaprogramming & expression templates, concurrency/memory model, lifetime & allocators,
      C++20/23, software design, tooling/quality, numerics & HPC (BLAS, Eigen, OpenMP/MPI, GPU overview)

## Rules for myself

1. Type every program; don't paste. Compile early, compile often.
2. Always build with `-Wall -Wextra`; treat warnings as bugs.
3. Use the sanitizer build when something is weird — it will tell you *where* memory broke.
4. For every ML model: derive gradient on paper → code → verify against PyTorch on the same
   tiny input.
5. Commit each finished item.
