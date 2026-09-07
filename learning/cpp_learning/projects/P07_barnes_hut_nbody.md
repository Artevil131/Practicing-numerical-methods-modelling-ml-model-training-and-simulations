# P07 — Barnes–Hut N-Body (3-D, parallel)

**Difficulty:** ★★★★☆   **Prereq chapters:** C++ 07, 12 (plus 01-06, 11)   **Builds on:** C P15, P01

## Goal

A 3-D N-body code: an octree with `unique_ptr` children, the Barnes–Hut $\theta$ criterion, kick-drift-kick leapfrog, structure-of-arrays particle storage, and an OpenMP-parallel force loop. Rendered to PPM frames with a simple orthographic camera. Validated against your own $O(N^2)$ direct sum for accuracy and against ideal $N\log N$ / core-count scaling for speed, up to $N = 10^6$ for a handful of steps.

## Why

C P15 got you to $10^5$ in 2-D. Here the point is the engineering that makes a simulation *fast*: memory layout (SoA vs AoS, measured), tree node pooling vs `unique_ptr` allocation (measured), `#pragma omp parallel for` with the right scheduling on a load-imbalanced traversal, and false sharing when accumulating accelerations. These are the C++ 12 performance topics on a workload where a 10x difference is normal. The octree traversal is the template for the P08/P09 grids' neighbour logic; OpenMP carries into P08 and P10 unchanged.

## The math

Same physics as C P15 (softened gravity, leapfrog, energy/momentum/angular momentum), now in 3-D:

$$\mathbf a_i = -G\sum_{j\ne i} m_j\frac{\mathbf r_i - \mathbf r_j}{(|\mathbf r_i - \mathbf r_j|^2 + \epsilon^2)^{3/2}}, \qquad \mathbf L = \sum_i m_i\,\mathbf r_i\times\mathbf v_i \in \mathbb R^3$$

**Octree.** A node is a cube (center $\mathbf c$, half-width $s/2$) with up to 8 children indexed by the 3-bit octant code $(x > c_x) + 2(y > c_y) + 4(z > c_z)$. Each node stores total mass $M$ and center of mass $\mathbf R$; leaves hold one particle (or up to `leaf_capacity` particles — a tunable that trades tree depth for direct-sum work).

**Opening criterion.** For particle $i$ and node with width $s$ at distance $d = |\mathbf r_i - \mathbf R|$: if $s/d < \theta$, use the node's monopole ($M$ at $\mathbf R$); else open it. Optionally add the quadrupole term for accuracy at larger $\theta$. Error vs $\theta$: RMS relative force error $\sim 10^{-3}$ at $\theta = 0.5$, $\sim 10^{-2}$ at $\theta = 1.0$.

**Complexity.** Tree build $O(N\log N)$; force evaluation $O(N\log N)$ for $\theta \sim 0.5$–$1$; direct sum $O(N^2)$. Crossover typically around $N \approx 10^3$.

**Parallel speedup.** Amdahl: with serial fraction $f$ (tree build, if not parallelized), speedup on $p$ cores is $\le 1/(f + (1-f)/p)$. Measure the force loop alone (should scale nearly linearly) and the whole step (limited by the serial tree build — then parallelize the build by octant).

**Plummer sphere in 3-D** (from C P15): positions from $r = a(u^{-2/3} - 1)^{-1/2}$, uniformly random direction on the sphere ($z = 2u - 1$, $\phi = 2\pi u'$); velocities via the Aarseth rejection scheme. Check virial ratio $2K/|U| \approx 1$.

## Spec

**Interface** (`nbody.hpp`):

```cpp
struct Particles {                   // SoA
    std::vector<double> x, y, z, vx, vy, vz, ax, ay, az, m;
    std::size_t size() const;
    void resize(std::size_t n);
};
struct ParticleAoS { double x, y, z, vx, vy, vz, ax, ay, az, m; };   // for the layout benchmark only

struct OctNode {
    double cx, cy, cz, half;
    double mass = 0, comx = 0, comy = 0, comz = 0;
    int    body = -1;                            // leaf particle index, or -1
    int    count = 0;                            // particles in subtree
    std::array<std::unique_ptr<OctNode>, 8> child;
    bool is_leaf() const;
};

class Octree {
public:
    Octree(const Particles& p, double theta, double eps2, int leaf_capacity = 1);
    void build();                                 // computes bounding cube, inserts all, computes COM bottom-up
    void accel(const Particles& p, std::size_t i, double& ax, double& ay, double& az) const;
    std::size_t node_count() const;  int depth() const;
private:
    std::unique_ptr<OctNode> root_;
    // alternative: a std::vector<OctNode> pool with int child indices — implement both, benchmark
};

void accel_direct(Particles& p, double G, double eps2);                       // OpenMP over i
void accel_barnes_hut(Particles& p, const Octree& tree, double G);           // OpenMP over i, schedule(dynamic)
void leapfrog_step(Particles& p, double dt, double G, double eps2, double theta, bool use_tree);
struct Diagnostics { double K, U, E, px, py, pz, lx, ly, lz; };
Diagnostics diagnostics(const Particles& p, double G, double eps2);          // U via direct sum (or tree for large N)

void ic_plummer(Particles& p, std::size_t n, double a, double G, unsigned seed);
void ic_two_galaxies(Particles& p, std::size_t n_each, double sep, double rel_v, unsigned seed);   // two Plummer spheres on a collision course
void render_ppm(const Particles& p, const std::string& path, int W, int H, double world_half, int axis_up = 2);
```

**CLI**

```
./nbody run --ic plummer --n 100000 --dt 0.01 --steps 500 --eps 0.02 --theta 0.5 --threads 8 --every 5 --frames frames/ --log energy.csv
./nbody bench --n 1000,3000,10000,30000,100000,300000 --theta 0.5 --threads 1,2,4,8      # table of ms/step, direct vs tree
./nbody accuracy --n 10000 --theta 0.3,0.5,0.7,1.0                                        # RMS/max relative force error vs direct
./nbody layout --n 100000                                                                 # SoA vs AoS direct-sum timing
```

`energy.csv`: `step,t,K,U,E,rel_dE,|P|,|L|,ms_build,ms_force`.

**Expected** (Apple M-series, 8 performance cores): direct $N = 10^4$ ≈ 100 ms/step single-threaded, ≈ 15 ms on 8 threads; tree $N = 10^5$, $\theta = 0.5$ ≈ 150–300 ms/step on 8 threads; $N = 10^6$ ≈ 3–5 s/step.

## Milestones

1. **M1 — direct sum, OpenMP, two-body + Plummer diagnostics.** Kepler orbit closes; Plummer $|\Delta E/E| < 10^{-3}$ over 10 time units; `|P|` at roundoff. 8-thread speedup ≥ 5x on the direct loop.
2. **M2 — octree build.** All $N$ particles found in leaves; root COM equals mean position; `node_count() ≈ 2–3 N`; depth $\approx \log_8 N + $ a few. Build time for $N = 10^5$ < 100 ms.
3. **M3 — tree forces.** $\theta = 0$ reproduces the direct sum to 1e-12. `accuracy` prints RMS error ~1e-3 at $\theta = 0.5$. Energy conservation with the tree is slightly worse than direct — quantify it.
4. **M4 — parallel tree traversal.** `schedule(dynamic, 64)` vs `static`: dynamic wins because central particles traverse more nodes. Speedup ≥ 5x on 8 threads. No data races (each thread writes only `a[i]` for its own `i`; run with `-fsanitize=thread` once).
5. **M5 — pool allocator vs `unique_ptr`.** Second `Octree` implementation using a `std::vector<OctNode>` pool and integer child indices; build 1.5–3x faster; traversal similar or faster (better locality). Keep both; note results in a comment.
6. **M6 — scaling study + galaxy collision.** `bench` table shows slopes ≈ 2 (direct) and ≈ 1.1–1.2 (tree) on log-log; two-galaxy merger with $N = 2\times50{,}000$, 1000 steps, GIF; tidal tails visible.

## Verification

```python
import numpy as np, pandas as pd
d = pd.read_csv("energy.csv")
print(d.rel_dE.abs().max(), d["|P|"].max())                  # < 1e-3, ~1e-13
# force accuracy against brute force in NumPy for a saved snapshot (N=2000, eps from the run)
P = np.load("snap.npy"); a_tree = np.load("a_tree.npy")        # x,y,z,m columns; your tree accelerations
r = P[:, None, :3] - P[None, :, :3]; d2 = (r**2).sum(-1) + eps**2; np.fill_diagonal(d2, np.inf)
a = -(P[None, :, 3, None] * r / d2[..., None]**1.5).sum(1)
rel = np.linalg.norm(a - a_tree, axis=1) / np.linalg.norm(a, axis=1)
print(np.sqrt((rel**2).mean()), rel.max())                    # ~1e-3, ~1e-2 at theta=0.5
```

Scaling: fit `log(ms) = p*log(N) + c` to the bench table; expect $p \approx 2.0$ direct, $1.1$–$1.2$ tree.

## Stretch goals

- Quadrupole moments in nodes: same accuracy at $\theta = 0.8$ as monopole at $0.5$ → faster.
- Morton (Z-order) sort of particles each step: improves cache locality of both build and traversal; measure.
- Parallel tree build: partition by top-level octant, build 8 subtrees in parallel, join.
- Individual time steps (block scheme) for dense cores; or a simple SIMD-friendly direct kernel using `float` and compare.

## Hints

- SoA is not optional for the direct kernel: the inner loop reads `x[j], y[j], z[j], m[j]` sequentially; with AoS every access pulls a 80-byte struct for 32 bytes of data. Measure both; the `layout` mode exists for that.
- OpenMP on macOS: Apple clang needs `-Xpreprocessor -fopenmp -lomp` with `libomp` from Homebrew, or use Homebrew's LLVM `clang++` with `-fopenmp`. In CMake: `find_package(OpenMP REQUIRED)` and link `OpenMP::OpenMP_CXX`.
- Insertion into a `unique_ptr` octree: recursive; when a leaf with a body receives a second body, create the appropriate children and push both down. Guard against identical positions with a depth cap (treat as one leaf with two bodies).
- COM computation: a post-order pass after insertion, or accumulate `mass`/`com*mass` at each node during insertion and divide at the end.
- Traversal recursion per particle can be replaced by an explicit stack (`std::vector<const OctNode*>`) — often faster and avoids deep recursion.
- False sharing: threads writing `ax[i]` for adjacent `i` on different cores thrash cache lines only if the chunks are tiny; `schedule(dynamic, 64)` keeps chunks whole cache lines. Never accumulate into a shared scalar inside the parallel loop without `reduction`.
- Bounding cube each step: min/max over positions (parallel `reduction(min:...)`), pad by 1%.
- For $N = 10^6$ frames, splat into a float buffer with `log(1 + b)` tone mapping as in C P15.

## Where to put it

`cpp/sims/nbody/` — `include/nbody.hpp`, `src/octree.cpp`, `src/octree_pool.cpp`, `src/forces.cpp`, `src/ic.cpp`, `src/render.cpp`, `apps/nbody.cpp`, `tests/test_octree.cpp`, `CMakeLists.txt`, `plot.py`.
