# P09 — Particle-in-Cell Plasma 1-D

**Difficulty:** ★★★★★   **Prereq chapters:** C++ 01-13   **Builds on:** P06 (FFT, tridiagonal), P08 (grids), C P15 (particles)

## Goal

An electrostatic 1-D particle-in-cell simulation: electrons as macro-particles on a periodic domain with a neutralizing ion background; each step deposits charge to a grid with cloud-in-cell weighting, solves Poisson's equation for the potential (FFT or tridiagonal), differences to get the field, interpolates it back to particles, and pushes them with leapfrog. Two canonical tests: a cold plasma oscillation whose frequency must equal $\omega_p$, and the two-stream instability whose growth rate must match linear theory before it saturates into phase-space vortices.

## Why

PIC is the method behind fusion and space-plasma codes, and it combines everything before it: particles and leapfrog (C P09/P15, P07), a field grid and staggering (P08), a Poisson solve by FFT or LU (P06), and phase-space rendering (PPM). It's also the first simulation with a *dispersion relation* to verify — you compute a growth rate from theory and measure it from your run. The energy bookkeeping (kinetic + field) is subtle and teaches you what "energy-conserving scheme" means. P09 is the capstone of the physics track in the same way P05 is for ML.

## The math

Normalized units: $\omega_p = 1$, Debye length or $c/\omega_p$ as length, electron mass $m = 1$, charge $q = -1$, $\epsilon_0 = 1$. Then $n_0 = 1$ for the background density (since $\omega_p^2 = n_0 q^2/(\epsilon_0 m)$).

**Equations.** Electrons: $\dot x_i = v_i$, $\dot v_i = \dfrac{q}{m}E(x_i)$. Field from Poisson: $\dfrac{d^2\phi}{dx^2} = -\dfrac{\rho}{\epsilon_0}$, $E = -\dfrac{d\phi}{dx}$, with $\rho = \rho_{\text{ions}} + \rho_e$ and $\rho_{\text{ions}} = +n_0$ uniform.

**PIC cycle** (grid $x_j = j\Delta x$, $j = 0..N_g - 1$, periodic length $L = N_g\Delta x$):

1. *Deposit* (cloud-in-cell = linear weighting): particle at $x$ with $j = \lfloor x/\Delta x\rfloor$, $w = x/\Delta x - j$ contributes $q_m(1 - w)$ to $\rho_j$ and $q_m w$ to $\rho_{j+1}$, divided by $\Delta x$. Macro-particle charge $q_m = -n_0 L/N_p$ so the total electron charge neutralizes the ions.
2. *Solve* Poisson periodically. FFT: $\hat\phi_k = \hat\rho_k / (\epsilon_0 k^2)$ for $k \ne 0$ ($k = 2\pi m/L$), $\hat\phi_0 = 0$ (neutrality makes $\hat\rho_0 = 0$ anyway — assert it). For consistency with centered differences use $K^2 = \left(\dfrac{\sin(k\Delta x/2)}{\Delta x/2}\right)^2$ instead of $k^2$. Alternative: the tridiagonal system $\phi_{j-1} - 2\phi_j + \phi_{j+1} = -\rho_j\Delta x^2$ with periodic wrap (cyclic tridiagonal, Sherman–Morrison) and one point pinned.
3. *Field*: $E_j = -\dfrac{\phi_{j+1} - \phi_{j-1}}{2\Delta x}$.
4. *Gather*: $E(x_i) = (1 - w)E_j + wE_{j+1}$ — the **same** weights as deposit (momentum conservation requires it).
5. *Push* (leapfrog): $v^{n+1/2} = v^{n-1/2} + \dfrac{q}{m}E(x^n)\Delta t$; $x^{n+1} = x^n + v^{n+1/2}\Delta t$; wrap $x$ into $[0, L)$.

**Stability**: $\omega_p\Delta t < 2$ (leapfrog on an oscillator), practically $\Delta t \le 0.2/\omega_p$; $\Delta x \lesssim \lambda_D$ to avoid the finite-grid instability (with a cold beam $\lambda_D = 0$, so use a small thermal velocity or accept some heating).

**Diagnostics.** Kinetic energy $K = \sum_i \tfrac12 m_m v_i^2$ (macro-particle mass $m_m = n_0 L/N_p$); field energy $U = \tfrac{\epsilon_0}{2}\sum_j E_j^2\,\Delta x$; total $K + U$ conserved to ~1% (momentum-conserving CIC schemes don't conserve energy exactly). Total momentum $\sum_i m_m v_i$ conserved to roundoff (self-force-free by symmetric weighting).

**Test 1 — cold plasma oscillation.** Uniform cold electrons with a sinusoidal position perturbation $x_i \to x_i + A\sin(2\pi x_i/L)$, $A \ll \Delta x$. The density perturbation and field oscillate at exactly $\omega_p = 1$ (independent of $k$ for a cold plasma). Measure the frequency from the FFT of $E$ at one grid point or of $U(t)$ (which oscillates at $2\omega_p$). Expect $\omega/\omega_p = 1 \pm 0.01$.

**Test 2 — two-stream instability.** Two cold beams at $v = \pm v_0$, each density $n_0/2$. Linear dispersion relation:

$$1 = \frac{\omega_p^2/2}{(\omega - kv_0)^2} + \frac{\omega_p^2/2}{(\omega + kv_0)^2}$$

Unstable for $kv_0 < \omega_p$ (i.e. $kv_0/\omega_p < 1$); the maximum growth rate is $\gamma_{\max} = \omega_p/2$ at $kv_0 = \sqrt{3}/2\,\omega_p \approx 0.866\,\omega_p$. Choose $L$ so mode $m = 1$ ($k = 2\pi/L$) is the fastest-growing: $L = 2\pi v_0/0.866$. Seed with a tiny sinusoidal perturbation; the field energy grows as $e^{2\gamma t}$; fit the slope of $\ln U(t)$ in the linear phase — expect $2\gamma = 1.0 \pm 0.05$. Saturation: phase space $(x, v)$ rolls up into the classic "eye" vortex; particles trap.

**Landau damping** (stretch): a warm Maxwellian plasma damps a $k\lambda_D = 0.5$ wave at $\gamma/\omega_p \approx -0.153$ with real frequency $\omega \approx 1.416\,\omega_p$ — needs many particles to beat noise.

## Spec

**Interface** (`pic.hpp`):

```cpp
struct PICConfig {
    std::size_t n_particles, n_grid; double L, dt;
    std::string poisson = "fft";        // fft | tridiag
    unsigned seed = 0;
};
struct Species { std::vector<double> x, v; double q_m, m_m; };     // macro charge/mass per particle

class PIC1D {
public:
    explicit PIC1D(const PICConfig&);
    void init_cold_oscillation(double amplitude, int mode = 1);
    void init_two_stream(double v0, double perturb, int mode = 1, double v_thermal = 0.0);
    void init_maxwellian(double v_thermal, double perturb, int mode);
    void step();                                        // deposit -> solve -> field -> gather -> push
    // stages exposed for unit tests:
    void deposit();  void solve_poisson();  void compute_field();  void push();
    const std::vector<double>& rho() const;  const std::vector<double>& phi() const;  const std::vector<double>& E() const;
    const Species& electrons() const;
    double kinetic_energy() const;  double field_energy() const;  double momentum() const;
    double time() const;
    void write_phase_space_ppm(const std::string& path, int W, int H, double vmax) const;    // (x, v) scatter/histogram
    void write_field_row(std::ostream&) const;                                              // for a space-time (x,t) plot
};

// helpers (unit-tested alone)
void cic_deposit(const std::vector<double>& x, double q_m, double dx, std::size_t ng, std::vector<double>& rho);
double cic_gather(const std::vector<double>& E, double x, double dx, std::size_t ng);
void poisson_fft_periodic(const std::vector<double>& rho, double dx, std::vector<double>& phi);      // uses P06 fft
void poisson_tridiag_periodic(const std::vector<double>& rho, double dx, std::vector<double>& phi);  // cyclic Thomas
void gradient_periodic(const std::vector<double>& phi, double dx, std::vector<double>& E);           // E = -dphi/dx
double fit_growth_rate(const std::vector<double>& t, const std::vector<double>& logU, double t0, double t1);   // least squares slope
```

**CLI**

```
./pic oscillation --np 20000 --ng 64 --L 6.2832 --dt 0.05 --steps 1000 --amp 0.001 --out osc.csv
./pic twostream   --np 40000 --ng 128 --v0 1.0 --dt 0.05 --steps 1200 --perturb 1e-3 --every 10 --frames frames/ --out ts.csv
./pic landau      --np 400000 --ng 64 --vth 1.0 --kld 0.5 --steps 2000 --out landau.csv
```

`osc.csv`/`ts.csv`: `step,t,K,U,E_total,P,E_mid` (field at the middle grid point). Phase-space frames: 2-D histogram of $(x, v)$ to grayscale/colormap PPM, $v \in [-v_{\max}, v_{\max}]$.

## Milestones

1. **M1 — deposit/gather unit tests.** One particle at $x = 1.25\Delta x$ deposits 75%/25% to cells 1 and 2; total deposited charge equals $N_p q_m$ to 1e-12; a particle in a uniform field $E_j = E_0$ gathers exactly $E_0$ anywhere; deposit at $x = L - \delta$ wraps to cell 0.
2. **M2 — Poisson solvers agree.** For $\rho = \cos(2\pi x/L)$ both solvers give $\phi = \dfrac{L^2}{4\pi^2}\cos(2\pi x/L)$ (up to the discrete $K^2$ correction, which the tridiagonal solver has automatically); FFT and tridiagonal agree to 1e-12; $E$ from `gradient_periodic` matches the analytic derivative.
3. **M3 — cold plasma oscillation.** $\omega = 1.00 \pm 0.01$ from the FFT of `E_mid`; $K + U$ conserved to 0.1%; momentum to 1e-12. Increase $\Delta t$ to 1.9 and see the frequency error grow (leapfrog phase error $\omega_{\text{num}} = \frac{2}{\Delta t}\arcsin(\omega\Delta t/2)$ — check that formula); at 2.1 it blows up.
4. **M4 — two-stream linear growth.** $\ln U(t)$ is straight over ~3 e-foldings with slope $2\gamma = 1.0 \pm 0.05$ for the resonant $L$; try $kv_0/\omega_p = 1.2$ and see no growth (stable).
5. **M5 — nonlinear saturation + frames.** Phase-space GIF: two lines → wavy → vortex eye. $U$ saturates and oscillates; total energy drifts ≤ 2%. Space-time plot of $E(x, t)$ shows a standing pattern at wavelength $L$.
6. **M6 — noise and convergence.** Repeat M3 with $N_p = 2000, 20000, 200000$: field-energy noise floor scales as $1/N_p$; growth rate estimate stabilizes. Report timing: $10^5$ particles × $10^3$ steps in seconds; OpenMP the push/gather (deposit needs per-thread `rho` buffers reduced at the end).

## Verification

```python
import numpy as np, pandas as pd
o = pd.read_csv("osc.csv"); dt = o.t[1] - o.t[0]
F = np.abs(np.fft.rfft(o.E_mid - o.E_mid.mean())); f = np.fft.rfftfreq(len(o), dt)
print(2*np.pi*f[F.argmax()])                          # ~1.00 (omega_p)
ts = pd.read_csv("ts.csv"); m = (ts.t > 5) & (ts.t < 20)   # adjust to the linear phase you see
slope = np.polyfit(ts.t[m], np.log(ts.U[m]), 1)[0]; print(slope)   # ~1.0 (= 2*gamma, gamma = 0.5)
# theory: max of Im(omega) from the dispersion relation for your k, v0
k, v0 = 2*np.pi/L, 1.0
roots = np.roots([1, 0, -(2*k**2*v0**2 + 1), 0, k**4*v0**4 - k**2*v0**2])   # omega^4 - (2k^2v0^2+1)omega^2 + k^4v0^4 - k^2v0^2 = 0
print(np.abs(roots.imag).max())                       # gamma_theory; 0.5 at k v0 = 0.866
```

Also: `E_total` relative drift, `P` at roundoff, and the phase-space GIF matching the textbook picture (Birdsall & Langdon, ch. 5).

## Stretch goals

- Landau damping with a Maxwellian: measure $\gamma/\omega_p \approx -0.153$ at $k\lambda_D = 0.5$ (use the quiet-start loading — bit-reversed positions and inverse-CDF velocities — to reduce noise).
- Energy-conserving scheme (deposit and gather with different but consistent weightings) and compare the energy drift.
- Electromagnetic 1-D (1d3v with $E_y, B_z$ and the Boris push): a Weibel or laser–plasma test; connects to P08.
- 2-D electrostatic PIC with a 2-D FFT Poisson solver (P06 stretch) — the Kelvin–Helmholtz-like shear-flow instability.

## Hints

- Positions must stay in $[0, L)$: `x = fmod(x, L); if (x < 0) x += L;` after each push. Use the same wrap for the CIC upper cell: `(j + 1) % ng`.
- Deposit is a scatter (random writes); gather is a read — for OpenMP, give each thread its own `rho` and sum them (or use `#pragma omp atomic` and accept the slowdown; measure).
- Leapfrog staggering: velocities live at half steps. Initialize with a half-step backward push ($v^{-1/2} = v^0 - \tfrac{q}{2m}E(x^0)\Delta t$) or your kinetic energy at $t = 0$ is inconsistent by $O(\Delta t)$.
- FFT Poisson: `rho` is real; use `rfft` from P06, divide by $\epsilon_0 K^2$, zero the DC bin, inverse. If `phi` comes out with a large constant offset, the DC bin wasn't zeroed.
- Two-stream seeding: perturb positions of both beams by $A\sin(kx)$ with $A = 10^{-3}\Delta x$ — small enough for a long linear phase, large enough to beat particle noise. Or perturb velocities.
- Growth-rate fit: choose the window by eye first (plot $\ln U$), then automate: the longest interval where the local slope is within 10% of its median.
- The number of macro-particles per cell ($N_p/N_g$) should be ≥ 100 for clean results; noise in $U$ scales as $1/N_p$.

## Where to put it

`cpp/sims/pic/` — `include/pic.hpp`, `src/pic.cpp`, `src/poisson.cpp`, `src/render.cpp`, `apps/pic.cpp`, `tests/test_pic.cpp`, `CMakeLists.txt`, `analyze.py`.
