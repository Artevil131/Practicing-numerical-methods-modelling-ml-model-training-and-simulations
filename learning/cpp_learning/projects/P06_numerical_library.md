# P06 — Numerical Library

**Difficulty:** ★★★★☆   **Prereq chapters:** C++ 05, 08, 10, 11 (plus 01-04)   **Builds on:** P01, C P05 (Gaussian elimination), C P09 (ODEs)

## Goal

A header-only, templated numerical methods library — `numerics::` — with root finding (bisection, Newton, secant), dense linear algebra (Gaussian elimination with partial pivoting, LU, Householder QR, power iteration, Jacobi eigenvalue), interpolation (Lagrange, natural cubic spline), quadrature (trapezoid, Simpson, Gauss–Legendre), an adaptive RK45 ODE integrator, and an iterative radix-2 FFT. Each function has doctest unit tests against known values and convergence-order checks.

## Why

These are the primitives the physics track consumes: the tridiagonal/LU solver and the FFT are the two Poisson solvers in P09 (PIC); RK45 is the integrator for anything not symplectic; QR and eigenvalues give you PCA (C P07 stretch) and the spectral analysis of the plasma oscillation in P09; Gauss–Legendre appears in FEM if you go that way; cubic splines are how you sample a dispersion curve or resample a signal. Templates (C++ 05) let you run the same solver on `float`, `double`, `long double` and `std::complex<double>`; `std::function`/lambdas (C++ 08) replace C P09's function pointers; exceptions (C++ 10) report non-convergence. This is also the project where you learn to *test numerical code*: known values, residuals, and convergence orders instead of "looks right".

## The math

**Root finding** for $f(x) = 0$:
- Bisection on $[a, b]$ with $f(a)f(b) < 0$: halve until $|b - a| < \text{tol}$. Linear convergence, $\lceil\log_2((b-a)/\text{tol})\rceil$ iterations, cannot fail.
- Newton: $x_{k+1} = x_k - f(x_k)/f'(x_k)$. Quadratic convergence near a simple root: $|e_{k+1}| \approx \tfrac{|f''|}{2|f'|}|e_k|^2$. Can diverge; cap iterations and throw.
- Secant: Newton with $f'(x_k) \approx \dfrac{f(x_k) - f(x_{k-1})}{x_k - x_{k-1}}$; order $\varphi \approx 1.618$.

**Linear algebra** ($A \in \mathbb R^{n\times n}$):
- Partial-pivoting LU: $PA = LU$; solve $Ax = b$ as $Ly = Pb$, $Ux = y$. $O(n^3/3)$ flops; reuse the factorization for many right-hand sides (that's why LU and not plain elimination). Determinant $= (-1)^{\text{swaps}}\prod u_{ii}$.
- Householder QR: for column $k$, $v = x - \alpha e_1$ with $\alpha = -\text{sign}(x_1)\lVert x\rVert$, $H = I - 2vv^T/v^Tv$; $A \leftarrow HA$. $R$ is upper triangular, $Q = H_1 H_2 \cdots$. Least squares $\min\lVert Ax - b\rVert$ via $Rx = Q^Tb$ — better conditioned than the normal equations of C P05 (condition number $\kappa$ vs $\kappa^2$).
- Power iteration: $x \leftarrow Ax/\lVert Ax\rVert$ converges to the dominant eigenvector at rate $|\lambda_2/\lambda_1|$; Rayleigh quotient $\lambda = x^TAx/x^Tx$. Inverse iteration with a shift finds the eigenvalue nearest the shift.
- Jacobi eigenvalue (symmetric $A$): repeatedly zero the largest off-diagonal $a_{pq}$ with a Givens rotation of angle $\theta$, $\tan 2\theta = 2a_{pq}/(a_{qq} - a_{pp})$; the off-diagonal Frobenius norm decreases monotonically; eigenvectors accumulate in $V$. Converges quadratically once off-diagonals are small.
- Tridiagonal (Thomas) solve: $O(n)$ forward sweep/back substitution — used by P09.

**Interpolation**:
- Lagrange on nodes $x_0..x_n$: $p(x) = \sum_i y_i \prod_{j\ne i}\frac{x - x_j}{x_i - x_j}$. Demonstrate Runge's phenomenon on $1/(1+25x^2)$ with 11 equispaced vs Chebyshev nodes $x_k = \cos\frac{(2k+1)\pi}{2(n+1)}$.
- Natural cubic spline: piecewise cubics with continuous $p', p''$ and $p''(x_0) = p''(x_n) = 0$; the second derivatives $M_i$ satisfy a tridiagonal system $h_{i-1}M_{i-1} + 2(h_{i-1} + h_i)M_i + h_iM_{i+1} = 6\left(\frac{y_{i+1} - y_i}{h_i} - \frac{y_i - y_{i-1}}{h_{i-1}}\right)$. Error $O(h^4)$.

**Quadrature** on $[a, b]$ with $n$ panels, $h = (b-a)/n$:
- Trapezoid: $h\left[\tfrac12 f_0 + f_1 + \dots + f_{n-1} + \tfrac12 f_n\right]$, error $O(h^2)$.
- Simpson ($n$ even): $\tfrac h3[f_0 + 4f_1 + 2f_2 + \dots + 4f_{n-1} + f_n]$, error $O(h^4)$; exact for cubics.
- Gauss–Legendre with $m$ nodes: exact for polynomials of degree $\le 2m - 1$; nodes are roots of $P_m$ (find with Newton on the recurrence), weights $w_i = \dfrac{2}{(1-x_i^2)[P_m'(x_i)]^2}$. Map $[-1,1] \to [a,b]$.

**Adaptive RK45 (Dormand–Prince)**: 4th- and 5th-order embedded estimates $y^{(4)}, y^{(5)}$ from the same 7 stages (6 with FSAL); error estimate $\text{err} = \lVert y^{(5)} - y^{(4)}\rVert / (\text{atol} + \text{rtol}\lVert y\rVert)$; accept if $\text{err} \le 1$; new step $h \leftarrow h \cdot \min(5, \max(0.2, 0.9\,\text{err}^{-1/5}))$. Use the published Butcher tableau.

**FFT** (radix-2, iterative Cooley–Tukey), $N = 2^m$:

$$X_k = \sum_{n=0}^{N-1} x_n e^{-2\pi i kn/N}$$

Bit-reverse the input order, then for stage $s = 1..m$ with $M = 2^s$ and twiddle $\omega_M = e^{-2\pi i/M}$, combine butterflies: $u = X_j$, $t = \omega X_{j + M/2}$, $X_j = u + t$, $X_{j+M/2} = u - t$. $O(N\log N)$. Inverse: conjugate twiddles, divide by $N$. Real-signal test: a sine of frequency $f$ sampled $N$ times has peaks at bins $f$ and $N - f$ with magnitude $N/2$.

## Spec

**Namespace and headers** (`numerics/*.hpp`, all templates on `typename T`):

```cpp
namespace numerics {
// roots.hpp
struct RootResult { double x; int iterations; bool converged; };
template <class F>           RootResult bisection(F f, double a, double b, double tol = 1e-12, int max_iter = 200);
template <class F, class DF> RootResult newton(F f, DF df, double x0, double tol = 1e-12, int max_iter = 100);
template <class F>           RootResult secant(F f, double x0, double x1, double tol = 1e-12, int max_iter = 100);

// linalg.hpp  (Matrix from P01, or a Tensor<T> rank-2 — pick one and stick to it)
struct LU { Matrix lu; std::vector<std::size_t> perm; int sign; };
LU     lu_decompose(const Matrix& A);                 // throws SingularMatrix
Matrix lu_solve(const LU& f, const Matrix& b);        // b: n x k
double lu_det(const LU& f);
Matrix solve(const Matrix& A, const Matrix& b);       // convenience = lu_decompose + lu_solve
Matrix inverse(const Matrix& A);
struct QR { Matrix Q, R; };
QR     qr_householder(const Matrix& A);               // A: m x n, m >= n
Matrix lstsq(const Matrix& A, const Matrix& b);       // via QR
std::pair<double, Matrix> power_iteration(const Matrix& A, int max_iter = 1000, double tol = 1e-12);
struct Eigen { std::vector<double> values; Matrix vectors; };    // sorted descending
Eigen  jacobi_eigen(const Matrix& A_sym, double tol = 1e-12, int max_sweeps = 100);
std::vector<double> tridiagonal_solve(const std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c, std::vector<double> d);

// interp.hpp
class Lagrange    { public: Lagrange(std::vector<double> x, std::vector<double> y); double operator()(double x) const; };
class CubicSpline { public: CubicSpline(std::vector<double> x, std::vector<double> y); double operator()(double x) const; double derivative(double x) const; };
std::vector<double> chebyshev_nodes(int n, double a, double b);

// quad.hpp
template <class F> double trapezoid(F f, double a, double b, int n);
template <class F> double simpson(F f, double a, double b, int n);
struct GaussLegendre { std::vector<double> x, w; explicit GaussLegendre(int m); template <class F> double integrate(F f, double a, double b) const; };
template <class F> double adaptive_simpson(F f, double a, double b, double tol);

// ode.hpp
template <class State, class F> struct RK45Result { std::vector<double> t; std::vector<State> y; int steps, rejected; };
template <class F> RK45Result<std::vector<double>, F> rk45(F f, double t0, double t1, std::vector<double> y0, double rtol = 1e-8, double atol = 1e-10, double h0 = 1e-3);
// also fixed-step rk4 and leapfrog ports from C P09 for completeness

// fft.hpp
void fft (std::vector<std::complex<double>>& x);      // in place, size must be power of 2, throws otherwise
void ifft(std::vector<std::complex<double>>& x);
std::vector<std::complex<double>> rfft(const std::vector<double>& x);   // real input
std::vector<double> power_spectrum(const std::vector<double>& x);
}
```

**Tests**: one `TEST_CASE` per function with (a) a known value, (b) a residual check where applicable ($\lVert Ax - b\rVert < 10^{-12}\lVert b\rVert$), (c) a convergence-order check where applicable (`log2(err(h)/err(h/2))` within 0.2 of the theoretical order). A `bench` target prints `n` vs time for LU (should scale as $n^3$) and FFT ($N\log N$).

## Milestones

1. **M1 — roots.** $\sqrt 2$ from $x^2 - 2$ by all three methods; iteration counts ≈ 40 (bisection, tol 1e-12), 5 (Newton), 7 (secant). Newton on $x^3 - 2x + 2$ from $x_0 = 0$ cycles — assert it throws/returns `converged=false`.
2. **M2 — LU and QR.** Random 50x50: `solve` residual < 1e-12; `lu_det` matches `np.linalg.det`; Hilbert matrix $n = 10$ shows the danger ($\kappa \sim 10^{13}$). QR: $Q^TQ = I$ to 1e-13, $QR = A$ to 1e-13; `lstsq` on C P05's data equals the normal-equation answer to 1e-8 and beats it on an ill-conditioned polynomial fit.
3. **M3 — eigenvalues.** Power iteration on a 5x5 SPD matrix matches `np.linalg.eigvalsh` top value; Jacobi returns all eigenvalues of a random symmetric 20x20 to 1e-10 and $V^TAV$ is diagonal. PCA of MNIST's 784x784 covariance (top 20) via Jacobi is slow — note how slow; via power iteration + deflation is fine.
4. **M4 — interpolation and quadrature.** Lagrange with 11 equispaced nodes on Runge's function has max error > 1; Chebyshev nodes < 0.1; spline < 0.01 with 21 nodes and error ratio 16 when doubling nodes. $\int_0^\pi\sin x\,dx = 2$: trapezoid order 2, Simpson order 4, Gauss–Legendre $m = 5$ exact to 1e-14.
5. **M5 — RK45.** Lorenz from C P09 to $t = 10$ with `rtol=1e-10` matches `scipy.integrate.solve_ivp(method="RK45", rtol=1e-10, atol=1e-12)` to 1e-6; step count within 20% of scipy's; a stiff-ish problem shows many rejected steps (print them).
6. **M6 — FFT.** Matches `np.fft.fft` on random $N = 1024$ input to 1e-12; `ifft(fft(x)) == x`; a 50 Hz sine sampled at 1 kHz for 1 s peaks at bin 50; `N = 2^20` in well under a second. Bonus: convolve two signals via FFT and compare with the direct $O(N^2)$ convolution.

## Verification

```python
import numpy as np, scipy.integrate as si, scipy.special as sp
A = np.load("A.npy"); b = np.load("b.npy")
print(np.abs(np.linalg.solve(A, b) - np.load("x.npy")).max(), np.linalg.det(A), np.load("det.npy"))
print(np.sort(np.linalg.eigvalsh(np.load("S.npy")))[::-1][:5], np.load("eig.npy")[:5])
x = np.load("sig.npy"); print(np.abs(np.fft.fft(x) - np.load("fft.npy")).max())            # < 1e-12
print(sp.roots_legendre(5))                                                                   # your GaussLegendre(5).x, .w
f = lambda t, s: [10*(s[1]-s[0]), s[0]*(28-s[2])-s[1], s[0]*s[1]-8/3*s[2]]
r = si.solve_ivp(f, (0, 10), [1,1,1], rtol=1e-10, atol=1e-12); print(r.y[:, -1], r.t.size)  # vs your rk45 endpoint and step count
```

## Stretch goals

- Cholesky factorization for SPD systems (half the flops of LU) and its use in Gaussian-process regression on a toy 1-D dataset.
- Real-input FFT that exploits Hermitian symmetry (half the work), and a 2-D FFT (rows then columns) — the P09 Poisson solver in 2-D and the spectral analysis of P08 fields want this.
- Symplectic integrators of order 4 (Yoshida) added to `ode.hpp`; compare energy drift with leapfrog on the Kepler problem.
- Templated on `T = float` and `T = long double`: measure how errors scale; find where `float` LU fails (Hilbert $n = 6$).

## Hints

- Header-only templates: everything in `.hpp` under `include/numerics/`; a `numerics.hpp` umbrella. One test file per header.
- Throw a small exception hierarchy (`NumericsError` → `SingularMatrix`, `NoConvergence`) and test with `CHECK_THROWS_AS`.
- For LU, store $L$ and $U$ in one matrix (unit diagonal of $L$ implicit) and the permutation as a vector — that's how LAPACK does it, and P01's `Matrix` makes this natural.
- Householder: don't form $H$ explicitly; apply $A \leftarrow A - v\,(2/v^Tv)(v^TA)$ column by column. Accumulate $Q$ only if asked.
- Jacobi: the classical version searches the max off-diagonal each rotation ($O(n^2)$ search); cyclic Jacobi sweeps all pairs in order and is simpler — implement cyclic.
- Dormand–Prince coefficients are long; copy them from the paper/Wikipedia into a `constexpr` array and test the tableau with the consistency conditions ($\sum b_i = 1$, rows of $a$ sum to $c_i$).
- FFT bit reversal: compute `rev` incrementally or with a loop over bits; the butterfly stages loop `for (len = 2; len <= N; len <<= 1)`. Precompute twiddles per stage.
- Test tolerances: know what to expect — 1e-12 for well-conditioned linear algebra in double, 1e-14 for Gauss–Legendre on polynomials, `rtol`-ish for adaptive ODE.

## Where to put it

`cpp/numerics/` — `include/numerics/{roots,linalg,interp,quad,ode,fft}.hpp`, `include/numerics/numerics.hpp`, `tests/test_*.cpp`, `bench/bench.cpp`, `tests/check.py`, `CMakeLists.txt`. P08/P09 include this library.
