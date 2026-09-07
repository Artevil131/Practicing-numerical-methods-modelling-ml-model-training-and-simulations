# P10 — Lattice Boltzmann Fluid (D2Q9)

**Difficulty:** ★★★★☆   **Prereq chapters:** C++ 12 (plus 01-08, 11)   **Builds on:** P08 (grids, frames), C P14

## Goal

A D2Q9 lattice Boltzmann solver with BGK collision, streaming, bounce-back walls, velocity inlet / outflow, and a cylinder obstacle; at Reynolds number ~100 it develops the von Kármán vortex street. Output: velocity-magnitude and vorticity PPM frames, drag/lift coefficients, and a Strouhal-number measurement compared with experiment. Validated first on Poiseuille flow, which has an exact parabolic profile.

## Why

LBM is the most approachable route to real fluid dynamics — no pressure Poisson solve, no nonlinear implicit steps — and it is a *different* kind of grid method from P08/P14: instead of discretizing a PDE you evolve particle populations whose moments *are* the fluid. It's also embarrassingly parallel and memory-bound, so it's the best possible playground for the C++ 12 topics: data layout (array-of-populations vs population-of-arrays), pull vs push streaming, cache blocking, OpenMP. And a vortex street is the most satisfying animation in the course.

## The math

**D2Q9 lattice.** Nine discrete velocities $\mathbf c_i$: $(0,0)$; $(\pm1, 0), (0, \pm1)$; $(\pm1, \pm1)$, with weights $w_0 = 4/9$, $w_{1..4} = 1/9$, $w_{5..8} = 1/36$. Lattice units: $\Delta x = \Delta t = 1$, lattice sound speed $c_s = 1/\sqrt3$.

**Populations** $f_i(\mathbf x, t)$; macroscopic density and velocity are moments:

$$\rho = \sum_i f_i, \qquad \rho\mathbf u = \sum_i f_i\mathbf c_i$$

**Equilibrium** (second-order expansion of Maxwell–Boltzmann):

$$f_i^{eq} = w_i\rho\left[1 + \frac{\mathbf c_i\cdot\mathbf u}{c_s^2} + \frac{(\mathbf c_i\cdot\mathbf u)^2}{2c_s^4} - \frac{\mathbf u\cdot\mathbf u}{2c_s^2}\right] = w_i\rho\left[1 + 3\,\mathbf c_i\cdot\mathbf u + \tfrac92(\mathbf c_i\cdot\mathbf u)^2 - \tfrac32 u^2\right]$$

**BGK collision** with relaxation time $\tau$:

$$f_i^*(\mathbf x, t) = f_i(\mathbf x, t) - \frac{1}{\tau}\left(f_i - f_i^{eq}\right)$$

**Streaming**: $f_i(\mathbf x + \mathbf c_i, t + 1) = f_i^*(\mathbf x, t)$.

**Viscosity**: $\nu = c_s^2(\tau - \tfrac12) = \tfrac13(\tau - \tfrac12)$. Stability requires $\tau > 1/2$; accuracy wants $\tau \in [0.5, 2]$ and $|\mathbf u| \lesssim 0.1$ (low Mach). Pressure $p = c_s^2\rho$.

**Reynolds number** for a cylinder of diameter $D$ (lattice cells) and inflow $u_\infty$: $Re = u_\infty D/\nu$. Pick $u_\infty = 0.05$–$0.1$, $D = 20$–$40$, then $\tau = \tfrac12 + 3\nu = \tfrac12 + 3u_\infty D/Re$. For $Re = 100$, $u = 0.05$, $D = 20$: $\nu = 0.01$, $\tau = 0.53$.

**Boundaries.**
- *Bounce-back* (no-slip wall / obstacle): populations that would stream into a solid node are reflected: $f_{\bar i}(\mathbf x) = f_i^*(\mathbf x)$ where $\bar i$ is the opposite direction. Half-way bounce-back places the wall midway between fluid and solid nodes — second-order accurate.
- *Inlet* (left): impose $\mathbf u = (u_\infty, 0)$ via Zou–He, or simply set $f_i = f_i^{eq}(\rho = 1, \mathbf u_\infty)$ (first order but robust).
- *Outlet* (right): copy populations from the neighbouring column ($f_i(N_x - 1) = f_i(N_x - 2)$) or set equilibrium with the interior velocity; both are "good enough" for a vortex street if the domain is long.
- Top/bottom: periodic (simplest) or bounce-back (channel).

**Validation — Poiseuille flow** in a channel of height $H$ driven by body force $F$ (add $\Delta\mathbf u = \mathbf F\tau/\rho$ to $\mathbf u$ in the equilibrium): steady profile $u(y) = \dfrac{F}{2\rho\nu}\,y(H - y)$; the LBM result with half-way bounce-back matches to $O(\Delta x^2)$ — expect $< 1\%$ error at $H = 32$.

**Vortex street diagnostics.** Vorticity $\omega = \partial_x u_y - \partial_y u_x$ (centered differences). Force on the cylinder via momentum exchange: $\mathbf F = \sum_{\text{boundary links}} (f_i^* + f_{\bar i})\,\mathbf c_i$ summed over links that bounce back. Drag coefficient $C_D = \dfrac{2F_x}{\rho u_\infty^2 D}$ ≈ 1.3–1.4 at $Re = 100$ (experiment ~1.3). Lift oscillates at the shedding frequency $f_s$; Strouhal number $St = f_sD/u_\infty \approx 0.16$–$0.17$ at $Re = 100$ (experiment 0.164). Shedding begins for $Re \gtrsim 47$; below that the wake is steady — check.

## Spec

**Interface** (`lbm.hpp`):

```cpp
struct LBMConfig { std::size_t nx, ny; double tau; double u_in; std::string top_bottom = "periodic"; /* periodic | wall */ };

class LBM2D {
public:
    explicit LBM2D(const LBMConfig&);
    void add_cylinder(double cx, double cy, double r);
    void add_rectangle(std::size_t x0, std::size_t x1, std::size_t y0, std::size_t y1);
    void set_body_force(double fx, double fy);
    void init_uniform(double rho, double ux, double uy);
    void step();                                       // collide -> stream (or pull), boundaries, macroscopic update
    std::size_t iteration() const;

    const std::vector<double>& rho() const;  const std::vector<double>& ux() const;  const std::vector<double>& uy() const;
    std::vector<double> vorticity() const;
    std::pair<double,double> obstacle_force() const;   // momentum exchange, this step
    double total_mass() const;

    void write_ppm_speed(const std::string& path, double vmax) const;
    void write_ppm_vorticity(const std::string& path, double wmax) const;   // signed colormap
private:
    // layout to decide and benchmark: f[i*nx*ny + idx] (SoA per direction) vs f[idx*9 + i] (AoS per cell)
    std::vector<double> f_, f_tmp_;
    std::vector<uint8_t> solid_;
};

// D2Q9 constants
constexpr int    CX[9] = {0, 1, 0, -1, 0, 1, -1, -1, 1};
constexpr int    CY[9] = {0, 0, 1, 0, -1, 1, 1, -1, -1};
constexpr double W[9]  = {4./9, 1./9, 1./9, 1./9, 1./9, 1./36, 1./36, 1./36, 1./36};
constexpr int    OPP[9]= {0, 3, 4, 1, 2, 7, 8, 5, 6};
double feq(int i, double rho, double ux, double uy);
```

**CLI**

```
./lbm poiseuille --nx 16 --ny 32 --tau 1.0 --force 1e-6 --steps 20000 --out profile.csv
./lbm cylinder  --nx 400 --ny 100 --D 20 --re 100 --u 0.05 --steps 40000 --every 100 --frames frames/ --forces forces.csv
./lbm bench     --nx 1000 --ny 400 --steps 200 --threads 1,2,4,8      # MLUPS (million lattice updates per second)
```

`profile.csv`: `y,u_lbm,u_exact`. `forces.csv`: `step,Fx,Fy,CD,CL`.

**Expected performance**: single-threaded ~20–50 MLUPS in `double` with a good layout; 8 threads 100–200 MLUPS (memory bound). A 400x100 cylinder run for 40k steps: ~1 minute.

## Milestones

1. **M1 — equilibrium and moments round trip.** `feq` over all 9 directions for given $(\rho, \mathbf u)$ has moments exactly $(\rho, \rho\mathbf u)$ (to 1e-15). Uniform flow with periodic boundaries stays uniform forever; `total_mass()` constant to 1e-12.
2. **M2 — Poiseuille.** Steady parabolic profile within 1% of exact; error drops 4x when doubling $N_y$ (second order). Try $\tau = 0.51$ and $\tau = 1.5$: same profile (viscosity is the only thing that changes, and the exact solution scales with it).
3. **M3 — cylinder at Re = 20.** Steady symmetric wake with two recirculation bubbles; $C_D \approx 2.0$–$2.1$ (literature ~2.0). Streamlines/speed frames look right; no checkerboard noise (if there is, the collision/streaming order or `OPP` is wrong).
4. **M4 — Re = 100 vortex street.** After the transient (~10k steps) the wake oscillates; vorticity GIF shows alternating red/blue vortices. $St = 0.16$–$0.17$ from the FFT of $C_L(t)$; mean $C_D \approx 1.3$–$1.4$.
5. **M5 — Re sweep.** Re = 40: steady; Re = 60: shedding starts; Re = 200: $St \approx 0.19$–$0.20$, wake more chaotic. Table to CSV. Confirm the shedding threshold ≈ 47.
6. **M6 — performance.** Two layouts + push vs pull streaming benchmarked; OpenMP; report MLUPS; the fastest variant ≥ 3x the naive one.

## Verification

```python
import numpy as np, pandas as pd
p = pd.read_csv("profile.csv"); print(np.abs(p.u_lbm - p.u_exact).max() / p.u_exact.max())   # < 0.01
f = pd.read_csv("forces.csv"); m = f.step > 15000
cl = f.CL[m] - f.CL[m].mean(); F = np.abs(np.fft.rfft(cl)); fr = np.fft.rfftfreq(len(cl), d=100)  # every 100 steps
fs = fr[F.argmax()]; D, u = 20, 0.05
print("St =", fs*D/u, " CD =", f.CD[m].mean())          # 0.16-0.17, 1.3-1.4 at Re=100
```

Physical sanity: mass conserved to roundoff with periodic + bounce-back (inlet/outlet break exact conservation — should be small and steady); no negative densities; max $|\mathbf u| < 0.2$ everywhere (else compressibility errors).

## Stretch goals

- TRT or MRT collision operator: stable at lower $\tau$ (higher Re) than BGK; reach Re = 1000.
- Thermal LBM (a second population for temperature) — Rayleigh–Bénard convection cells.
- Smagorinsky LES for a turbulent-ish Re = 5000 wake.
- Immersed moving cylinder or an airfoil shape from an SDF (borrow from P12) — lift vs angle of attack.

## Hints

- Write `collide_and_stream` as one fused loop in *pull* form: for each cell and direction, read $f_i$ from the upstream neighbour $\mathbf x - \mathbf c_i$ (already collided last step) — this needs only one extra buffer and streams beautifully. Push form is easier to reason about first; implement it, get the physics right, then switch.
- Index helper `idx(x, y)` with periodic wrap; solid mask `solid_[idx]` checked in the streaming step for bounce-back.
- Velocity in equilibrium must include the body force half-step if you use forcing (Guo forcing is the accurate way; the simple $\mathbf u + \mathbf F\tau/\rho$ shift is acceptable for Poiseuille).
- Initialize the cylinder run with a slight asymmetry (e.g. $u_y = 10^{-4}\sin(\pi y/N_y)$) or the perfectly symmetric wake takes a very long time to start shedding.
- Frames: `vmax` for speed ≈ $1.5u_\infty$; `wmax` for vorticity ≈ $3u_\infty/D$; keep fixed across frames.
- Layout benchmark: with SoA per direction, the collision needs 9 strided loads per cell but streaming is a memcpy-like shift; with AoS per cell the opposite. Measure; on Apple silicon the SoA variant usually wins.
- OpenMP: `#pragma omp parallel for schedule(static)` over `y` rows; each row is independent in the pull scheme.

## Where to put it

`cpp/sims/lbm/` — `include/lbm.hpp`, `src/lbm.cpp`, `src/render.cpp`, `apps/lbm.cpp`, `tests/test_lbm.cpp` (moments, mass, Poiseuille), `CMakeLists.txt`, `analyze.py`.
