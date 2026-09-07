# P15 — N-Body Gravity

**Difficulty:** ★★★★☆   **Prereq chapters:** C 10 (plus 01-09, 12)   **Builds on:** P09 (leapfrog), P04 (PPM), P10 (trees)

## Goal

A gravitational N-body simulator: $N$ particles under softened Newtonian gravity integrated with leapfrog, with energy and momentum conservation checked every frame, PPM frames of the particle cloud rendered with a simple camera, and two force algorithms — the direct $O(N^2)$ sum and a Barnes–Hut quadtree (2-D) that gets you to $N = 10^4$–$10^5$ in reasonable time. Test cases: two-body Kepler orbit (analytic), a Plummer sphere, and a cold collapse.

## Why

This is the first simulation with a *physical* correctness criterion instead of a plot that looks right: energy and momentum are conserved or you have a bug. The leapfrog stepper is P09's, now with a force loop as the RHS. The quadtree is the tree code of P10 in a spatial setting — build, traverse, free, and the $\theta$ opening criterion is the accuracy/speed knob that C++ P07 parallelizes with OpenMP. The direct-sum force loop is your first data-parallel kernel and a good place to feel cache effects (AoS vs SoA, the C++ P07 topic).

## The math

Particle $i$: mass $m_i$, position $\mathbf r_i$, velocity $\mathbf v_i$. Softened acceleration (softening length $\epsilon$ prevents $1/r^2$ blowups at close encounters):

$$\mathbf a_i = -G\sum_{j\neq i} m_j\,\frac{\mathbf r_i - \mathbf r_j}{\left(|\mathbf r_i - \mathbf r_j|^2 + \epsilon^2\right)^{3/2}}$$

Use $G = 1$ in code units. Note $1/(r^2+\epsilon^2)^{3/2}$: compute `inv = 1/sqrt(d2 + eps2); inv3 = inv*inv*inv` — one `sqrt`, no `pow`.

**Leapfrog (kick-drift-kick)** from P09: $\mathbf v_{i}^{n+1/2} = \mathbf v_i^n + \tfrac{\Delta t}{2}\mathbf a_i^n$; $\mathbf r_i^{n+1} = \mathbf r_i^n + \Delta t\,\mathbf v_i^{n+1/2}$; recompute $\mathbf a^{n+1}$; $\mathbf v_i^{n+1} = \mathbf v_i^{n+1/2} + \tfrac{\Delta t}{2}\mathbf a_i^{n+1}$. One force evaluation per step.

**Conserved quantities** (with softening, energy uses the softened potential):

$$E = \sum_i \tfrac12 m_i |\mathbf v_i|^2 - G\sum_{i<j}\frac{m_i m_j}{\sqrt{|\mathbf r_i - \mathbf r_j|^2 + \epsilon^2}}, \qquad \mathbf P = \sum_i m_i\mathbf v_i, \qquad \mathbf L = \sum_i m_i\,\mathbf r_i\times\mathbf v_i$$

$\mathbf P$ is conserved to roundoff by any symmetric force (Newton's third law); $E$ oscillates in a bounded band with leapfrog (relative drift $\lesssim 10^{-4}$ for sane $\Delta t$); $\mathbf L$ is conserved for the direct sum, approximately for Barnes–Hut.

**Two-body check.** Masses $m_1 = m_2 = m = 1$ ($M = 2m$), separation $a$, circular orbit: each body sits at distance $a/2$ from the center of mass. Centripetal balance $m v^2/(a/2) = G m^2/a^2$ gives $v = \sqrt{G m/(2a)}$ for each body (opposite directions). Period $T = 2\pi\sqrt{a^3/(G M)}$. After one period, positions return to within $\sim 10^{-4}$ (with $\Delta t = T/1000$, $\epsilon = 0$).

**Barnes–Hut** (2-D quadtree). Each node covers a square, stores total mass $M$ and center of mass $\mathbf R$. To compute the force on $\mathbf r_i$, traverse from the root: if a node is a leaf with one particle, sum its force directly; else if $s/d < \theta$ (node width $s$, distance $d$ from $\mathbf r_i$ to its COM), treat the node as one particle at $\mathbf R$ with mass $M$; else recurse into children. $\theta = 0.5$ is standard; $\theta = 0$ recovers the direct sum. Cost $O(N\log N)$ per step.

Tree build: insert particles one by one; a leaf that already holds a particle subdivides and pushes both down (guard against two particles at identical positions — cap depth). Center of mass computed bottom-up after insertion, or accumulated during insertion.

**Plummer sphere** initial conditions (for realistic-looking runs): $r = a\left(u^{-2/3} - 1\right)^{-1/2}$ with $u\sim U(0,1)$, isotropic direction, velocities from the escape-speed rejection sampling (Aarseth, Hénon & Wielen 1974). Virial ratio $2K/|U| \approx 1$ — check it.

## Spec

**CLI**

```
./nbody kepler <dt> <periods> out.csv                       # two bodies; logs t, x1,y1,x2,y2, E, Px,Py, L
./nbody plummer <N> <dt> <steps> <eps> <method: direct|bh> <theta> <every> frames/ energy.csv
./nbody collapse <N> <dt> <steps> <eps> <method> <theta> <every> frames/ energy.csv   # cold uniform sphere
./nbody bench <N> <method> <theta>                          # time one force evaluation; compare a_direct vs a_bh
```

`energy.csv`: `step,t,K,U,E,rel_dE,Px,Py,Lz`. Frames: `frames/f_0000.ppm`, 512x512, particles as white (or velocity-colored) points on black, with a fixed world-to-pixel scale.

**Signatures** (`nbody.h`, `forces.c`, `quadtree.c`, `render.c`, `ic.c`):

```c
typedef struct { double *x, *y, *vx, *vy, *ax, *ay, *m; int n; } Particles;   /* SoA */
Particles *particles_alloc(int n);
void particles_free(Particles *p);

void   accel_direct(Particles *p, double G, double eps2);
double energy_total(const Particles *p, double G, double eps2, double *K, double *U);
void   momentum(const Particles *p, double *px, double *py, double *lz);
void   leapfrog_step(Particles *p, double dt, double G, double eps2,
                     void (*accel)(Particles *, double, double, void *), void *ctx);

typedef struct QNode {
    double cx, cy, half;          /* square: center and half-width */
    double mass, comx, comy;
    int    particle;              /* index if leaf with one body, -1 otherwise */
    struct QNode *child[4];       /* NULL if empty */
} QNode;
QNode *qt_build(const Particles *p, double cx, double cy, double half);   /* pool-allocated nodes */
void   qt_free_all(void);                                                 /* reset the node pool */
void   qt_accel(const QNode *root, const Particles *p, int i, double theta, double G, double eps2,
                double *ax, double *ay);
void   accel_bh(Particles *p, double G, double eps2, void *ctx /* theta */);

void   ic_two_body(Particles *p, double a, double G);
void   ic_plummer(Particles *p, double a, double G, unsigned seed);
void   ic_cold_sphere(Particles *p, double R, unsigned seed);
void   render_ppm(const Particles *p, const char *path, int W, int H, double world_half);
```

**Expected**: `bench 10000 direct` ~ 0.3-0.5 s per force evaluation (100M pair interactions); `bench 10000 bh 0.5` ~ 10x faster with max relative force error ~1e-2 and RMS ~1e-3; `bench 100000 bh` a few seconds.

## Milestones

1. **M1 — direct forces + leapfrog on two bodies.** Circular Kepler orbit closes after one period to $< 10^{-4}$; $E$ relative drift $< 10^{-6}$, $|\mathbf P| < 10^{-14}$. Plot the orbit from CSV — two circles.
2. **M2 — eccentric orbit and energy band.** Start with $v \times 0.7$: an ellipse; leapfrog energy oscillates but returns each orbit; replace leapfrog with Euler (temporarily) and watch the orbit spiral out.
3. **M3 — Plummer sphere, N = 1000, direct.** Virial ratio near 1 at $t = 0$; over 10 dynamical times the sphere stays a sphere, $|\Delta E/E| < 10^{-3}$ with $\epsilon = 0.05$, $\Delta t = 0.01$. PPM frames → GIF.
4. **M4 — cold collapse, N = 2000.** Sphere collapses to a dense core, bounces, ejects a halo; energy conserved to ~1% through the bounce (needs smaller $\Delta t$ or it will not be). Visually dramatic.
5. **M5 — quadtree.** Build for N = 1000, verify every particle is found once (count leaves), COM of root equals the mean; $\theta = 0$ gives accelerations identical to direct sum to 1e-12. $\theta = 0.5$: RMS relative error ~$10^{-3}$.
6. **M6 — scaling.** Time per step vs $N \in \{10^3, 3\cdot10^3, 10^4, 3\cdot10^4, 10^5\}$ for direct and BH; log-log plot shows slopes ≈ 2 and ≈ 1.1. Plummer with $N = 10^5$ for 200 steps, GIF.

## Verification

```python
import numpy as np, pandas as pd
d = pd.read_csv("out.csv")                       # kepler run
print(d.E.std()/abs(d.E.mean()))                  # < 1e-6
print(np.abs(d[["Px","Py"]]).max().max())        # ~1e-15
T = 2*np.pi*np.sqrt(1.0**3/(1*2))                 # a=1, G=1, M=2
row = d.iloc[(d.t - T).abs().argmin()]
print(np.hypot(row.x1 - d.x1[0], row.y1 - d.y1[0]))   # < 1e-4
# force check: dump positions/masses and a_direct, a_bh to files, then
# a_ref = brute force in NumPy with the same eps; compare both.
```

Physical expectations: Plummer half-mass radius stays ≈ $1.3a$; cold collapse core forms at $t \approx \sqrt{3\pi/(32 G\rho)}$ (free-fall time). Energy conservation is the primary bug detector: a sign error in the force makes $E$ grow monotonically; an asymmetric force makes $\mathbf P$ drift.

## Stretch goals

- Individual/adaptive time steps or a block time step scheme; or simply a smaller $\Delta t$ near close encounters using $\Delta t \propto \sqrt{\epsilon/|a|}$.
- 3-D with an octree (8 children); render with a simple orthographic projection and depth-based brightness.
- A spatial grid (uniform cells) as an alternative $O(N)$-ish neighbour structure for short-range forces; compare with BH.
- Multithreading the force loop with `pthreads` (each thread takes a range of `i`); measure the speedup on your core count. (C++ P07 does this with OpenMP.)

## Hints

- SoA (`x[], y[], vx[]...`) is deliberate: the inner loop over `j` streams through `x[j], y[j], m[j]` contiguously. Try AoS once and time it.
- In the direct loop use the symmetry $\mathbf F_{ij} = -\mathbf F_{ji}$ to halve the work (loop `j > i`, update both) — but note this makes threading harder later.
- Quadtree nodes: allocate from a pre-sized pool (array of `QNode` + a counter) rather than `malloc` per node; reset the counter each step. Roughly $2N$ nodes suffice.
- Insertion: which quadrant a point is in is two comparisons; child index `= (x > cx) + 2*(y > cy)`.
- Bound the root square to the current particle extent each step (particles escape in collapses); particles that fly far away can be dropped or the box grown.
- Rendering: accumulate a float brightness buffer (splat each particle as +1 at its pixel, or a 3x3 kernel), then apply `log(1 + k*b)` before mapping to 0-255 so dense cores don't saturate.

## Where to put it

`simulations/nbody/` — `nbody.h`, `main.c`, `forces.c`, `quadtree.c`, `render.c`, `ic.c`, `Makefile`, `plot.py`; `frames/` gitignored. C++ P07 ports this to an octree with OpenMP.
