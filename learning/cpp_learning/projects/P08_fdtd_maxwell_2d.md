# P08 — FDTD Maxwell 2-D

**Difficulty:** ★★★★☆   **Prereq chapters:** C++ 12 (plus 01-08, 11)   **Builds on:** P06, C P14

## Goal

A finite-difference time-domain solver for Maxwell's equations in 2-D (TMz mode: $E_z, H_x, H_y$) on a Yee grid: leapfrogged update equations, the Courant stability limit, perfect-electric-conductor walls, then Mur first-order and PML absorbing boundaries, point and Gaussian-pulse sources, a dielectric slab, and $|E_z|$ frames to PPM. Verified by measuring wave speed, the reflection coefficient off a dielectric interface against Fresnel, and the absorbing boundaries' residual reflection.

## Why

FDTD is C P14's wave equation done the way electromagnetics is actually simulated: two coupled fields staggered in space and time, so the update is a leapfrog in disguise (C P09) and the CFL condition (C P14) reappears as the Courant number. The Yee grid's half-cell offsets are the same idea as the PIC sim's staggered charge/field grid (P09) and the LBM's streaming step (P10). PML is your first encounter with a numerical trick that has no physical analogue — designing a boundary that absorbs — and the reflection-coefficient test is the kind of quantitative validation the simulation projects demand.

## The math

Maxwell's curl equations in a source-free, non-magnetic medium ($\mu = \mu_0$, permittivity $\epsilon = \epsilon_r\epsilon_0$, conductivity $\sigma$):

$$\frac{\partial \mathbf H}{\partial t} = -\frac{1}{\mu_0}\nabla\times\mathbf E, \qquad \frac{\partial \mathbf E}{\partial t} = \frac{1}{\epsilon}\nabla\times\mathbf H - \frac{\sigma}{\epsilon}\mathbf E$$

**TMz** (fields independent of $z$, $E$ along $z$): only $E_z, H_x, H_y$ are nonzero.

$$\frac{\partial H_x}{\partial t} = -\frac{1}{\mu_0}\frac{\partial E_z}{\partial y}, \qquad \frac{\partial H_y}{\partial t} = \frac{1}{\mu_0}\frac{\partial E_z}{\partial x}, \qquad \frac{\partial E_z}{\partial t} = \frac{1}{\epsilon}\left(\frac{\partial H_y}{\partial x} - \frac{\partial H_x}{\partial y}\right) - \frac{\sigma}{\epsilon}E_z$$

**Yee grid.** $E_z$ at integer points $(i, j)$ at integer times $n$; $H_x$ at $(i, j+\tfrac12)$, $H_y$ at $(i+\tfrac12, j)$ at half-integer times $n + \tfrac12$. Centered differences everywhere → second order in space and time. With $\Delta x = \Delta y = \Delta$:

$$H_x\big|^{n+1/2}_{i,j+1/2} = H_x\big|^{n-1/2}_{i,j+1/2} - \frac{\Delta t}{\mu_0\Delta}\left(E_z\big|^n_{i,j+1} - E_z\big|^n_{i,j}\right)$$

$$H_y\big|^{n+1/2}_{i+1/2,j} = H_y\big|^{n-1/2}_{i+1/2,j} + \frac{\Delta t}{\mu_0\Delta}\left(E_z\big|^n_{i+1,j} - E_z\big|^n_{i,j}\right)$$

$$E_z\big|^{n+1}_{i,j} = C_a\,E_z\big|^n_{i,j} + C_b\left(H_y\big|^{n+1/2}_{i+1/2,j} - H_y\big|^{n+1/2}_{i-1/2,j} - H_x\big|^{n+1/2}_{i,j+1/2} + H_x\big|^{n+1/2}_{i,j-1/2}\right)$$

with $C_a = \dfrac{1 - \sigma\Delta t/2\epsilon}{1 + \sigma\Delta t/2\epsilon}$, $C_b = \dfrac{\Delta t/(\epsilon\Delta)}{1 + \sigma\Delta t/2\epsilon}$ (per-cell arrays, so materials are just coefficient maps).

**Courant condition** in 2-D: $S = \dfrac{c\Delta t}{\Delta} \le \dfrac{1}{\sqrt 2}$. Use $S = 0.5$. Work in normalized units: $c = 1$, $\epsilon_0 = \mu_0 = 1$, $\Delta = 1$ — then $\Delta t = S$ and impedances are dimensionless; or use SI and check units carefully. Numerical dispersion: waves with fewer than ~10 cells per wavelength travel measurably slower than $c$; keep $\lambda \ge 20\Delta$.

**Sources.** Soft source: add $J(t)$ to $E_z$ at a point. Gaussian pulse $J(t) = \exp\left(-\left(\frac{t - t_0}{\tau}\right)^2\right)$ with $\tau \approx 20\Delta t$; sinusoidal $\sin(2\pi f t)$ ramped up smoothly. Total-field/scattered-field plane-wave injection is a stretch.

**Boundaries.**
- PEC: $E_z = 0$ on the boundary (just don't update it). Reflects perfectly with sign flip.
- Mur first-order ABC on the left edge: $E_z|^{n+1}_{0,j} = E_z|^n_{1,j} + \dfrac{S - 1}{S + 1}\left(E_z|^{n+1}_{1,j} - E_z|^n_{0,j}\right)$. Absorbs normal incidence well, oblique poorly (residual reflection ~1–10%).
- PML (split-field or, simpler, a graded conductivity layer): in a layer of $N_{pml}$ cells, set $\sigma(\rho) = \sigma_{\max}(\rho/N_{pml})^m$ with $m = 3$–$4$ and matching magnetic loss $\sigma^*/\mu_0 = \sigma/\epsilon_0$ so the impedance is matched (no reflection at the layer's inner edge); $\sigma_{\max} \approx \dfrac{(m+1)\,0.8}{\eta_0 \Delta}$ in SI, or tune. Residual reflection < $10^{-3}$ ($-60$ dB) for 10–20 cells. The uniaxial/split-field PML is the rigorous version; the graded-loss version is a fine first pass and reduces to $C_a, C_b$ maps plus analogous $D_a, D_b$ for $H$.

**Verification physics.**
- Wave speed: time for a pulse peak to travel $L$ cells; $v = L\Delta/(n\Delta t)$ should equal $c$ to ~1% for $\lambda \ge 20\Delta$.
- Fresnel at normal incidence onto a slab with $\epsilon_r$: $r = \dfrac{1 - \sqrt{\epsilon_r}}{1 + \sqrt{\epsilon_r}}$, $t = \dfrac{2}{1 + \sqrt{\epsilon_r}}$ (amplitude); $\epsilon_r = 4 \Rightarrow r = -1/3$. Measure with a 1-D-like setup (plane wave via a line source, PML on the far sides).
- Cavity modes: a PEC box of $L_x\times L_y$ has resonances $f_{mn} = \dfrac{c}{2}\sqrt{(m/L_x)^2 + (n/L_y)^2}$. Excite with a pulse, record $E_z$ at a point, FFT (P06) → peaks at $f_{mn}$.

## Spec

**Interface** (`fdtd.hpp`):

```cpp
struct Grid2 {                      // row-major (nx x ny) field with (i,j) accessor
    std::size_t nx, ny; std::vector<double> v;
    double& operator()(std::size_t i, std::size_t j);  double operator()(std::size_t i, std::size_t j) const;
};
struct FDTDConfig { std::size_t nx, ny; double S = 0.5; std::string boundary = "pec";  /* pec | mur | pml */ std::size_t pml_cells = 12; int pml_order = 3; };

class FDTD2D {
public:
    explicit FDTD2D(const FDTDConfig&);
    void set_material(std::size_t i0, std::size_t i1, std::size_t j0, std::size_t j1, double eps_r, double sigma = 0.0);   // rectangle
    void set_material_circle(double cx, double cy, double r, double eps_r);
    void add_point_source(std::size_t i, std::size_t j, std::function<double(double)> J);  // soft source J(t)
    void add_line_source(std::size_t i, std::function<double(double)> J);                   // plane-wave-ish
    void step();                                        // H half-step, E full step, boundaries, sources
    double time() const;  std::size_t iteration() const;
    const Grid2& Ez() const;  const Grid2& Hx() const;  const Grid2& Hy() const;
    double energy() const;                              // sum eps*Ez^2/2 + mu*(Hx^2+Hy^2)/2
    void write_ppm(const std::string& path, double vmax, bool signed_colormap = true) const;   // blue-white-red
    void record_probe(std::size_t i, std::size_t j);   std::vector<double> probe_series(std::size_t k) const;
};

double gaussian_pulse(double t, double t0, double tau);
double ricker(double t, double f0, double t0);
```

**CLI**

```
./fdtd pulse    --nx 400 --ny 400 --bc pml --steps 800 --every 4 --frames frames/         # point Gaussian, watch ring expand & get absorbed
./fdtd slab     --nx 800 --ny 100 --eps 4 --x0 400 --x1 500 --bc pml --probe 200,50 --probe 600,50 --out slab.csv   # reflection/transmission
./fdtd cavity   --nx 200 --ny 100 --bc pec --steps 8192 --probe 37,61 --out cavity.csv    # resonances via FFT
./fdtd reflect  --bc mur|pml --pml 8,12,16,24                                             # residual reflection table, dB
./fdtd cylinder --nx 600 --ny 400 --eps 2.5 --r 40 --bc pml --source line --f 0.02 --frames frames/   # scattering, standing pattern
```

Frames: `frames/f_%04d.ppm` with a signed colormap (negative blue, positive red, fixed `vmax`). GIF via ffmpeg as in C P14.

## Milestones

1. **M1 — free-space pulse, PEC.** Ring expands at speed $c$ (check: radius vs time from frames, slope = 1 in normalized units to ~1%); reflects off walls with inverted sign; `energy()` conserved to < 0.1% after the source turns off (lossless, PEC). You'll know the Yee indexing is right when the ring is circular, not diamond-shaped, and energy is flat.
2. **M2 — break Courant.** $S = 0.72$: exponential blow-up within ~100 steps; $S = 0.70$: stable. Print max$|E_z|$ per step.
3. **M3 — Mur ABC.** The ring leaves the domain; residual reflection measured as max$|E_z|$ after the pulse would have exited, relative to the incident peak: 1–5%.
4. **M4 — graded-loss PML.** Residual reflection < $10^{-3}$ for 12 cells, $< 10^{-4}$ for 24 (`reflect` mode prints a table in dB). Try $m = 1$ vs $m = 3$ grading — the sharp step reflects.
5. **M5 — dielectric slab.** Probes before and after the slab; separate incident/reflected pulses by time; amplitude ratios match Fresnel ($r = -0.333$, $t = 0.667$ for $\epsilon_r = 4$) within 2%. Speed inside the slab is $c/2$ (frames).
6. **M6 — cavity resonances + cylinder scattering.** FFT of the cavity probe shows peaks at $f_{mn}$ within one frequency bin for the first 5 modes. Cylinder scattering GIF shows a shadow and interference fringes; OpenMP-parallel update gets a 400x400 grid to > 500 steps/s.

## Verification

```python
import numpy as np, pandas as pd
c = pd.read_csv("cavity.csv"); ez = c.Ez.values; dt = c.t[1] - c.t[0]
F = np.abs(np.fft.rfft(ez * np.hanning(len(ez)))); f = np.fft.rfftfreq(len(ez), dt)
peaks = f[np.argsort(F)[-8:]]; print(np.sort(peaks))
Lx, Ly = 199, 99                                   # PEC box interior in cells (Delta = 1, c = 1)
modes = sorted(0.5*np.sqrt((m/Lx)**2 + (n/Ly)**2) for m in range(4) for n in range(4) if m + n > 0)
print(modes[:6])                                   # match peaks within ~1 bin (1/(N*dt))
s = pd.read_csv("slab.csv")                        # columns t, p0 (before slab), p1 (after)
inc = s.p0[s.t < t_split].abs().max(); ref = s.p0[s.t > t_split].abs().max(); tr = s.p1.abs().max()
print(ref/inc, tr/inc)                             # 0.333, 0.667 (eps_r = 4), sign of reflected pulse negative
```

## Stretch goals

- Total-field/scattered-field (TF/SF) plane-wave injection: a clean plane wave with no source artifacts; then radar cross-section of the cylinder vs Mie series.
- Uniaxial PML (proper split fields or CPML) — compare residual reflection with the graded-loss version.
- TEz mode ($E_x, E_y, H_z$) and a metal slit: single/double-slit diffraction, compare fringe spacing with $\lambda L/d$.
- Dispersive material (Drude model, auxiliary differential equation) — a plasma slab that reflects below $\omega_p$; ties directly to P09.

## Hints

- Allocate $H_x$ as `nx × (ny-1)` and $H_y$ as `(nx-1) × ny` (or the same size with unused edges) so the staggering is explicit in the loop bounds; sketch the Yee cell on paper with indices before coding. Off-by-one here produces a stable but *wrong* (anisotropic) wave — the "circular ring" test catches it.
- Per-cell coefficient arrays `Ca, Cb, Da, Db` make materials, conductivity and the graded PML *the same code path*. Set them once.
- Update order per step: $H$ from current $E$; $E$ from new $H$; apply ABC; add sources; record probes; advance time.
- Normalized units ($c = \epsilon_0 = \mu_0 = 1$) avoid a factor-$10^{16}$ mismatch between $E$ and $H$ magnitudes; if you use SI, store $\tilde H = \eta_0 H$.
- For the signed colormap: `v = clamp(Ez/vmax, -1, 1)`; blue for negative, red for positive, white at zero; use a fixed `vmax` for all frames.
- OpenMP: `#pragma omp parallel for collapse(2)` on the $E$ and $H$ loops; measure — memory bandwidth, not compute, is the limit for a 2-D stencil.
- Resonance measurement: place the probe off-center (so it doesn't sit on a node of the low modes), run $2^{13}$ steps, window, FFT with P06.

## Where to put it

`cpp/sims/fdtd/` — `include/fdtd.hpp`, `src/fdtd.cpp`, `src/boundaries.cpp`, `src/render.cpp`, `apps/fdtd.cpp`, `tests/test_fdtd.cpp` (energy conservation, Courant, Fresnel), `CMakeLists.txt`, `analyze.py`.
