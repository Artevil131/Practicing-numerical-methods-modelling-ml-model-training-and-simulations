# P14 — Heat and Wave Equations, 1-D and 2-D

**Difficulty:** ★★★☆☆   **Prereq chapters:** C 08, 12 (plus 01-09)   **Builds on:** P04 (PGM writer), P09

## Goal

Explicit finite-difference solvers for the 1-D heat equation, the 1-D wave equation, and the 2-D heat equation with Dirichlet and Neumann boundaries. You will derive the stability limits (CFL conditions), demonstrate them by deliberately violating them and watching the solution explode, verify against analytic solutions, and turn 2-D runs into PGM frame sequences → GIF with ffmpeg.

## Why

Every field simulation in this course — FDTD Maxwell (C++ P08), the Poisson solve in PIC (C++ P09), the lattice Boltzmann fluid (C++ P10) — is "a grid of doubles updated from its neighbours with a stability limit on $\Delta t$". You learn the pattern here on the simplest PDEs, where you can compare to a closed-form answer. The 5-point Laplacian stencil, the ghost-cell boundary technique, double-buffering, and the frame-dump pipeline are copied directly into those projects. The CFL condition is the single most important concept in explicit time-stepping; breaking it on purpose makes it stick.

## The math

**1-D heat equation** $u_t = \alpha u_{xx}$ on $[0, L]$, grid $x_i = i\Delta x$, $i = 0..N$, time levels $u_i^n$.

FTCS (forward time, centered space):

$$u_i^{n+1} = u_i^n + r\left(u_{i+1}^n - 2u_i^n + u_{i-1}^n\right), \qquad r = \frac{\alpha\Delta t}{\Delta x^2}$$

Stability (von Neumann analysis: substitute $u_i^n = \xi^n e^{ikx_i}$, require $|\xi| \le 1$ for all $k$): $\xi = 1 - 4r\sin^2(k\Delta x/2)$, so $r \le \tfrac12$. With $r = 0.51$ the highest-frequency mode grows by $|1 - 4(0.51)| = 1.04$ per step — after 500 steps that's $\times 3\cdot10^8$: a sawtooth explosion.

Analytic test: $u(x, 0) = \sin(\pi x/L)$, $u(0,t) = u(L,t) = 0$ ⇒ $u(x, t) = e^{-\alpha\pi^2 t/L^2}\sin(\pi x/L)$.

**1-D wave equation** $u_{tt} = c^2 u_{xx}$:

$$u_i^{n+1} = 2u_i^n - u_i^{n-1} + C^2\left(u_{i+1}^n - 2u_i^n + u_{i-1}^n\right), \qquad C = \frac{c\Delta t}{\Delta x}$$

Stability: $C \le 1$ (the Courant number — information cannot travel more than one cell per step). Needs two previous time levels; start with $u^1_i = u^0_i + \Delta t\, v_i + \tfrac{C^2}{2}(u^0_{i+1} - 2u^0_i + u^0_{i-1})$ from initial velocity $v$. Analytic test: standing wave $u = \sin(\pi x/L)\cos(\pi c t/L)$, or a Gaussian pulse splitting into two half-amplitude pulses moving at $\pm c$ (d'Alembert). At $C = 1$ exactly, the scheme is *exact* for the linear wave equation on a uniform grid — a nice check.

Energy $E = \sum_i \left[\tfrac12 (u_t)_i^2 + \tfrac12 c^2 (u_x)_i^2\right]\Delta x$ should be conserved to $O(\Delta t^2)$ with reflecting boundaries.

**2-D heat equation** $u_t = \alpha(u_{xx} + u_{yy})$ on an $N_x \times N_y$ grid:

$$u_{i,j}^{n+1} = u_{i,j}^n + r\left(u_{i+1,j} + u_{i-1,j} + u_{i,j+1} + u_{i,j-1} - 4u_{i,j}\right)^n, \qquad r = \frac{\alpha\Delta t}{\Delta x^2} \le \frac14$$

**Boundaries.** Dirichlet: fixed value in the boundary cells (e.g. a hot wall $u = 1$, cold walls $u = 0$). Neumann (insulated, $\partial u/\partial n = 0$): ghost-cell mirror $u_{-1} = u_{1}$, i.e. copy the neighbour into the boundary cell before each update. Steady state of a square plate with one hot Dirichlet wall and three cold ones is the Laplace solution — compare a mid-line profile with the series solution or with a long-run reference.

Total heat $\sum_{ij} u_{ij}$ is conserved exactly with all-Neumann boundaries (up to roundoff) — check it.

## Spec

**CLI**

```
./heat1d <N> <alpha> <r> <t_end> out.csv               # columns: x, u_numeric, u_exact at t_end; prints max error
./wave1d <N> <c> <C> <t_end> <ic: standing|gauss> out.csv frames/    # csv snapshot every k steps + energy log
./heat2d <Nx> <Ny> <r> <steps> <bc: dirichlet|neumann> <every> frames/   # frames/f_0000.pgm ...
./heat1d --blowup <N> <r>                             # r > 0.5: prints max|u| per step until > 1e6
```

GIF: `ffmpeg -framerate 30 -i frames/f_%04d.pgm -vf "scale=512:-1:flags=neighbor" heat.gif`.

**Signatures** (`fd.h`, `fd.c`, plus the three mains):

```c
typedef struct { int nx, ny; double *u; } Grid2D;     /* u[j*nx + i] */

void heat1d_step(const double *u, double *unew, int n, double r);        /* interior only */
void wave1d_step(const double *uprev, const double *u, double *unew, int n, double C2);
void heat2d_step(const Grid2D *u, Grid2D *unew, double r);
void apply_dirichlet_1d(double *u, int n, double left, double right);
void apply_neumann_1d(double *u, int n);
void apply_dirichlet_2d(Grid2D *g, double top, double bottom, double left, double right);
void apply_neumann_2d(Grid2D *g);
double wave_energy(const double *uprev, const double *u, int n, double dx, double dt, double c);
double grid_sum(const Grid2D *g);
int  grid_write_pgm(const Grid2D *g, const char *path, double vmin, double vmax);  /* fixed color scale! */
void swap_ptrs(double **a, double **b);
```

Use a fixed `vmin/vmax` (e.g. 0..1) for all frames so the GIF doesn't flicker as the range changes.

**Output of `heat1d 100 1.0 0.4 0.1`**: `max abs error 3.1e-05` (or thereabouts: second order in $\Delta x$; halving $\Delta x$ with fixed $r$ quarters the error).

## Milestones

1. **M1 — 1-D heat, Dirichlet, analytic check.** Max error vs the exact decaying sine ~$10^{-5}$ for $N = 100$, and 4x smaller for $N = 200$ (same $r$). You'll know it works when the error ratio is ≈ 4.
2. **M2 — break it.** `--blowup` with $r = 0.51$: max$|u|$ grows a few % per step and the profile turns into a sawtooth; with $r = 0.49$ it decays smoothly. Print the growth factor per step and compare to $|1 - 4r|$.
3. **M3 — 1-D wave.** Gaussian pulse splits into two, reflects off Dirichlet walls with sign flip, off Neumann walls without. Energy conserved to < 0.1% over 10 crossings at $C = 0.5$. At $C = 1$ the pulse shape is preserved exactly (error at roundoff level). At $C = 1.05$: explosion.
4. **M4 — 2-D heat with Dirichlet.** Hot top wall, cold others, $r = 0.25$ (marginal but stable), 2000 steps, 200x200 grid: frames show heat diffusing down; late frames approach a steady state. GIF looks right.
5. **M5 — 2-D Neumann conservation.** Gaussian blob initial condition, all-Neumann: `grid_sum` constant to 1e-12 relative; blob spreads to uniform. Dirichlet with a hot wall: total heat increases monotonically then saturates.
6. **M6 — performance.** 500x500 grid, 5000 steps in a few seconds (~$10^9$ cell updates). If not, check your inner loop is over `i` (contiguous) and you double-buffer with pointer swaps rather than `memcpy`.

## Verification

```python
import numpy as np, pandas as pd
d = pd.read_csv("out.csv")                            # heat1d N=100 alpha=1 r=0.4 t_end=0.1
print(np.abs(d.u_numeric - d.u_exact).max())         # ~3e-5
# independent FTCS in NumPy for the same params
N, r, T = 100, 0.4, 0.1; dx = 1/N; dt = r*dx*dx; steps = int(round(T/dt))
x = np.linspace(0, 1, N+1); u = np.sin(np.pi*x)
for _ in range(steps):
    u[1:-1] = u[1:-1] + r*(u[2:] - 2*u[1:-1] + u[:-2])
print(np.abs(u - d.u_numeric).max())                  # ~1e-15 (same algorithm, same steps)
# 2D steady state check (Dirichlet, top=1): mid-column profile vs Laplace series solution
```

Wave: energy log column `E` — plot it; it should oscillate slightly around a constant, no trend. Growth-factor check: $\log(\max|u^{n+1}|/\max|u^n|)$ at late times equals $\log|1 - 4r|$ to 2 digits for $r > 0.5$.

## Stretch goals

- Implicit (backward Euler) 1-D heat: solve a tridiagonal system per step (Thomas algorithm — you'll need it for the C++ PIC Poisson solver); unconditionally stable, try $r = 10$.
- Crank–Nicolson: second order in time; compare error vs FTCS at equal $\Delta t$.
- 2-D wave equation with a point source: circular ripples; then a slit in a wall — diffraction pattern (preview of FDTD).
- Variable diffusivity $\alpha(x, y)$ (two materials): conserve flux at the interface by using harmonic-mean $\alpha$ between cells.

## Hints

- Two buffers `u` and `unew`, update interior, apply boundaries to `unew`, then swap pointers. Never copy arrays per step.
- Index 2-D as `u[j*nx + i]` and loop `for j { for i { ... } }` so `i` is the fast (contiguous) index.
- Wave equation needs three buffers (`uprev`, `u`, `unew`) rotated cyclically.
- Compute `dt` from `r` (or `C`) and `dx`, then the number of steps from `t_end`; report the actual `t` reached (it won't be exactly `t_end` unless you adjust the last step).
- For PGM frames: clamp `(u - vmin)/(vmax - vmin)` to $[0,1]$, scale to 0-255. Write every `every`-th step; a 2000-step run at `every = 10` gives 200 frames = ~7 s at 30 fps.
- Neumann via mirror: after computing the interior, set `u[0] = u[1]`, `u[n-1] = u[n-2]` (1-D) and the analogous row/column copies in 2-D, corners last.

## Where to put it

`numerical_methods/pde/` — `fd.h`, `fd.c`, `heat1d.c`, `wave1d.c`, `heat2d.c`, `Makefile`, `plot.py`; `frames/` gitignored. Reuse `image_io.c` from P04 or copy `pgm_write`.
