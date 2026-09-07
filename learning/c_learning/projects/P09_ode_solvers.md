# P09 — ODE Solvers

**Difficulty:** ★★★☆☆   **Prereq chapters:** C 11, 12 (plus 01-09)   **Builds on:** P05 (CSV workflow)

## Goal

A small ODE integration library where the right-hand side $f(t, \mathbf y)$ is a function pointer and the stepper (Euler, midpoint, RK4, leapfrog) is another, so any system can be run with any method from the command line. Four test systems — harmonic oscillator, projectile with drag, large-angle pendulum, Lorenz attractor — dumped to CSV and plotted in Python, with an energy-drift study that shows why leapfrog matters for physics.

## Why

Every simulation later in the course is an ODE integrator with a fancy right-hand side: P15 and C++ P07 (N-body) *are* leapfrog with a gravity force; the C++ PIC particle push is leapfrog; FDTD's time stepping is a staggered leapfrog in disguise. RK4 and adaptive RK45 go into the C++ P06 numerical library. The function-pointer design (`Stepper` and `RHS` types) is the C version of the strategy pattern you'll use for P12's `Layer`. The convergence-order test (halve $h$, error drops by $2^p$) is how you validate every numerical method from here on.

## The math

A first-order system $\dot{\mathbf y} = f(t, \mathbf y)$, $\mathbf y \in \mathbb{R}^n$, step size $h$.

| Method | Update | Order |
|---|---|---|
| Euler | $\mathbf y_{k+1} = \mathbf y_k + h f(t_k, \mathbf y_k)$ | 1 |
| Midpoint (RK2) | $\mathbf k_1 = f(t_k, \mathbf y_k)$, $\mathbf k_2 = f(t_k + \tfrac h2, \mathbf y_k + \tfrac h2 \mathbf k_1)$, $\mathbf y_{k+1} = \mathbf y_k + h\mathbf k_2$ | 2 |
| RK4 | $\mathbf k_1 = f(t_k,\mathbf y_k)$, $\mathbf k_2 = f(t_k+\tfrac h2, \mathbf y_k+\tfrac h2\mathbf k_1)$, $\mathbf k_3 = f(t_k+\tfrac h2, \mathbf y_k+\tfrac h2\mathbf k_2)$, $\mathbf k_4 = f(t_k+h, \mathbf y_k+h\mathbf k_3)$, $\mathbf y_{k+1} = \mathbf y_k + \tfrac h6(\mathbf k_1+2\mathbf k_2+2\mathbf k_3+\mathbf k_4)$ | 4 |

**Leapfrog / velocity Verlet** applies only to second-order systems $\ddot{\mathbf x} = \mathbf a(\mathbf x)$ (force independent of velocity):

$$\mathbf v_{k+1/2} = \mathbf v_k + \tfrac h2\,\mathbf a(\mathbf x_k), \quad \mathbf x_{k+1} = \mathbf x_k + h\,\mathbf v_{k+1/2}, \quad \mathbf v_{k+1} = \mathbf v_{k+1/2} + \tfrac h2\,\mathbf a(\mathbf x_{k+1})$$

Order 2, but *symplectic*: it preserves phase-space volume, so energy oscillates in a bounded band instead of drifting. Euler's energy grows without bound; RK4's decays slowly (visible over $10^5$ periods).

**Global error** for an order-$p$ method scales as $O(h^p)$: halving $h$ divides the error by $\approx 2^p$. Measure $p = \log_2\big(E(h)/E(h/2)\big)$.

**Test systems** (state vector layout in brackets):

1. Harmonic oscillator $[x, v]$: $\dot x = v$, $\dot v = -\omega^2 x$. Exact: $x(t) = x_0\cos\omega t + \tfrac{v_0}{\omega}\sin\omega t$. Energy $E = \tfrac12 v^2 + \tfrac12\omega^2x^2$.
2. Projectile with quadratic drag $[x, y, v_x, v_y]$: $\dot{\mathbf v} = -g\hat{\mathbf y} - c\,|\mathbf v|\,\mathbf v$. Without drag ($c = 0$) the range is $v_0^2\sin 2\alpha / g$.
3. Pendulum $[\theta, \omega]$: $\ddot\theta = -\tfrac{g}{\ell}\sin\theta$. Period for amplitude $\theta_0$: $T = 4\sqrt{\ell/g}\,K(\sin\tfrac{\theta_0}{2})$ where $K$ is the complete elliptic integral of the first kind; for $\theta_0 = 90°$, $T \approx 1.18034\, T_{\text{small}}$ with $T_{\text{small}} = 2\pi\sqrt{\ell/g}$. Energy $E = \tfrac12\omega^2 - \tfrac{g}{\ell}\cos\theta$.
4. Lorenz $[x, y, z]$: $\dot x = \sigma(y - x)$, $\dot y = x(\rho - z) - y$, $\dot z = xy - \beta z$ with $\sigma = 10, \rho = 28, \beta = 8/3$. Chaotic: two trajectories differing by $10^{-8}$ separate exponentially (Lyapunov exponent $\approx 0.9$).

## Spec

**CLI**

```
./ode <system> <method> <h> <t_end> out.csv
   system: sho | projectile | pendulum | lorenz
   method: euler | midpoint | rk4 | leapfrog       (leapfrog only for sho/pendulum/projectile-without-drag)
./ode convergence <system> <method>                  # prints h, error, estimated order for h = 0.1, 0.05, ..., 0.1/64
./ode energy sho <method> <h> <n_periods> drift.csv  # energy vs time
```

**CSV**: header `t,y0,y1,...`, one row per step (or every `stride` steps for long runs). Add an `E` column for systems with energy.

**Signatures** (`ode.h`, `ode.c`, `systems.c`):

```c
typedef void (*RHS)(double t, const double *y, double *dydt, int n, void *params);
typedef void (*Stepper)(RHS f, double t, double *y, int n, double h, void *params, double *scratch);

void step_euler   (RHS f, double t, double *y, int n, double h, void *params, double *scratch);
void step_midpoint(RHS f, double t, double *y, int n, double h, void *params, double *scratch);
void step_rk4     (RHS f, double t, double *y, int n, double h, void *params, double *scratch);

/* leapfrog needs acceleration only: state is [x(0..n/2-1), v(n/2..n-1)] */
typedef void (*Accel)(const double *x, double *a, int n_dof, void *params);
void step_leapfrog(Accel a, double *x, double *v, int n_dof, double h, void *params, double *scratch);

/* driver: integrates from t0 to t_end, calls `on_step` every `stride` steps */
typedef void (*StepCallback)(double t, const double *y, int n, void *user);
void integrate(RHS f, Stepper step, double t0, double t_end, double h,
               double *y, int n, void *params, int stride, StepCallback cb, void *user);

/* systems: RHS functions + a params struct each, e.g. */
typedef struct { double omega; } ShoParams;
void sho_rhs(double t, const double *y, double *dydt, int n, void *params);
double sho_energy(const double *y, const ShoParams *p);
typedef struct { double g, c; } ProjectileParams;
typedef struct { double g, l; } PendulumParams;
typedef struct { double sigma, rho, beta; } LorenzParams;
```

`scratch` is a caller-provided buffer of at least `5*n` doubles so steppers never `malloc`.

**Convergence output**

```
h          error         order
0.1        3.2e-05       -
0.05       2.0e-06       4.00
0.025      1.3e-07       3.99
```

## Milestones

1. **M1 — Euler on the SHO.** State drifts outward (energy grows). CSV plots as a spiral in the $(x, v)$ plane. You'll know it works when `x(t)` is roughly a cosine with slowly growing amplitude.
2. **M2 — midpoint and RK4 as interchangeable steppers.** Same driver, method chosen by a string → function pointer table. `convergence sho rk4` prints order ≈ 4, `midpoint` ≈ 2, `euler` ≈ 1.
3. **M3 — leapfrog + energy study.** Over 1000 periods with $h = 0.05$: Euler energy explodes, RK4 decreases monotonically by a tiny amount, leapfrog oscillates in a band and returns. Plot all three on one axis (relative energy error, log scale).
4. **M4 — projectile with drag.** With $c = 0$ the numerical range matches the analytic formula to 4 digits at $h = 10^{-3}$. With drag, range shrinks and the trajectory is asymmetric. Detect ground crossing by linear interpolation between the last two steps.
5. **M5 — pendulum period.** Measure the period from zero crossings of $\theta$ for $\theta_0 = 10°, 90°, 170°$; ratios to $T_{\text{small}}$ are $\approx 1.0019, 1.1803, 2.4394$ (check against `scipy.special.ellipk`).
6. **M6 — Lorenz.** Butterfly in a 3-D plot. Two runs with $x_0$ differing by $10^{-8}$ visibly diverge after $t \approx 20$; $\log|\Delta|$ vs $t$ has slope ≈ 0.9.

## Verification

```python
import numpy as np, pandas as pd
from scipy.integrate import solve_ivp
from scipy.special import ellipk
d = pd.read_csv("out.csv")                            # sho, rk4, h=0.01, omega=2, x0=1, v0=0
print(np.abs(d.y0 - np.cos(2*d.t)).max())             # ~1e-9 at h=0.01
# pendulum period ratio for theta0 = 90 deg
k = np.sin(np.radians(90)/2); print(2*ellipk(k**2)/np.pi)   # 1.18034
# lorenz vs scipy at the same times (agrees until chaos amplifies roundoff, t ~ 15-20)
f = lambda t, s: [10*(s[1]-s[0]), s[0]*(28-s[2])-s[1], s[0]*s[1]-8/3*s[2]]
ref = solve_ivp(f, (0, 10), [1, 1, 1], t_eval=d.t[d.t <= 10], rtol=1e-12, atol=1e-12)
print(np.abs(ref.y[0] - d.y0[d.t <= 10]).max())
```

Energy check: relative drift $|E(t) - E_0|/E_0$ after 1000 periods, $h = 0.05$, $\omega = 1$: Euler $\gg 1$; RK4 $\sim 10^{-5}$ and monotone; leapfrog bounded by $\sim 10^{-3}$ with zero secular drift.

## Stretch goals

- Adaptive step (RK45 / Dormand–Prince): embedded 4th/5th order pair, error estimate, step-size control $h_{\text{new}} = h\,(\text{tol}/\text{err})^{1/5}$. Lorenz with far fewer steps at equal accuracy.
- Implicit Euler for the stiff problem $\dot y = -1000(y - \cos t)$: explicit methods blow up unless $h < 0.002$; implicit is stable for any $h$ (needs one Newton step per time step).
- Double pendulum (chaotic; energy check with leapfrog is harder because the mass matrix depends on angles — use RK4 and monitor energy).
- Poincaré section of the Lorenz attractor at $z = 27$.

## Hints

- Steppers must not allocate. Carve `k1..k4` and a temporary state out of `scratch`.
- Keep the RHS signature generic (`const double *y, double *dydt`) so the same stepper handles $n = 2$ and $n = 3$; the systems file owns the meaning of each index.
- Method dispatch: a static table `{ "rk4", step_rk4 }` scanned with `strcmp`. Same for systems.
- For leapfrog, keep `x` and `v` as separate halves of one state array so the CSV writer stays unchanged.
- Zero-crossing detection: sign change of $\theta$ between steps, then interpolate $t^* = t_k - \theta_k\,h/(\theta_{k+1} - \theta_k)$. Period = difference between every second crossing.
- For the convergence test compare against the exact SHO solution at $t = 10$ only; it isolates global error from output frequency.

## Where to put it

`numerical_methods/ode/` — `ode.h`, `ode.c`, `systems.c`, `systems.h`, `main.c`, `Makefile`, `plot.py`. P15 includes `ode.h` for leapfrog.
