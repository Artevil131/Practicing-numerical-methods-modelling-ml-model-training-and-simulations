# P08 — ASCII Spinning Donut

**Difficulty:** ★★☆☆☆   **Prereq chapters:** C 12 (plus 01-05), `math.h`   **Builds on:** P02 (rotation matrices by hand, not the library)

## Goal

The classic terminal torus: a parametric torus rotated in 3-D, projected to the terminal grid, depth-sorted with a z-buffer, and shaded by the dot product of the surface normal and a light direction, mapped to the character ramp `.,-~:;=!*#$@`. It redraws at ~30 fps with ANSI escape codes and `nanosleep`. Write it *readable* — separate functions, named variables — then, optionally, obfuscate it into the famous 20-liner for fun.

## Why

Rotation matrices, perspective projection, z-buffering and Lambertian shading are exactly what C++ P12 (the raymarched donut) does with real pixels and what the PPM frames of P15/C++ P07 need for 3-D camera orbit. The frame loop with a fixed time step is the outer loop of every simulation you will write. And `math.h`, `double` precision, and terminal output are covered by C 12 — this is the fun payoff.

## The math

**Torus** with major radius $R_1$ (tube center to torus center) and tube radius $R_2$, parametrized by $\theta$ (around the tube) and $\phi$ (around the center):

Start with a circle in the $xy$-plane: $(R_2\cos\theta + R_1,\ R_2\sin\theta,\ 0)$. Revolve around the $y$-axis by $\phi$:

$$\mathbf{p}(\theta,\phi) = \big((R_1 + R_2\cos\theta)\cos\phi,\ \ R_2\sin\theta,\ \ -(R_1 + R_2\cos\theta)\sin\phi\big)$$

The outward unit normal is the same expression with $R_1 = 0, R_2 = 1$:

$$\mathbf{n}(\theta,\phi) = (\cos\theta\cos\phi,\ \sin\theta,\ -\cos\theta\sin\phi)$$

**Rotation** about $x$ by $A$ and about $z$ by $B$ (apply to both $\mathbf p$ and $\mathbf n$):

$$R_x(A) = \begin{pmatrix}1&0&0\\0&\cos A&-\sin A\\0&\sin A&\cos A\end{pmatrix}, \quad R_z(B) = \begin{pmatrix}\cos B&-\sin B&0\\ \sin B&\cos B&0\\0&0&1\end{pmatrix}$$

$\mathbf p' = R_z(B)\,R_x(A)\,\mathbf p$. Compose them as one $3\times3$ once per frame (trig is per frame, not per point).

**Projection.** Push the donut away from the camera by $K_2$ (e.g. 5): $z = p'_z + K_2$. Perspective divide with focal factor $K_1$:

$$x_{\text{screen}} = \frac{W}{2} + K_1\,\frac{p'_x}{z}, \qquad y_{\text{screen}} = \frac{H}{2} - K_1\,\frac{p'_y}{z}$$

Choose $K_1$ so the torus fills the screen: the widest point is at distance $R_1 + R_2$, so $K_1 = \dfrac{W\,K_2\,\cdot 0.5\cdot 0.8}{R_1 + R_2}$ roughly. Terminal cells are ~2x taller than wide — halve the $y$ scale or multiply the $x$ scale by 2.

**Z-buffer.** Store $1/z$ per cell (larger = closer); write a cell only if the new $1/z$ exceeds the stored one.

**Shading.** Luminance $L = \mathbf n' \cdot \mathbf{\ell}$ with light direction $\mathbf\ell = (0, 1, -1)/\sqrt2$ (from above and behind the viewer). $L \in [-\sqrt2, \sqrt2]$; negative means facing away — skip or draw darkest. Index the ramp with $\lfloor L \cdot 8 \rfloor$ clamped to $[0, 11]$.

## Spec

**CLI**

```
./donut                  # 80x24, runs until Ctrl-C
./donut <W> <H> <fps>    # custom size and frame rate
./donut --frames 60 out/ # headless: write 60 frames as out/frame_000.txt (for testing)
```

**Signatures** (`donut.c`):

```c
typedef struct { double x, y, z; } Vec3;
typedef struct { double m[3][3]; } Mat3;

Mat3  rot_x(double a);
Mat3  rot_z(double b);
Mat3  mat3_mul(Mat3 a, Mat3 b);
Vec3  mat3_apply(Mat3 m, Vec3 v);
double vec3_dot(Vec3 a, Vec3 b);

typedef struct {
    int W, H;
    char   *chars;     /* W*H */
    double *zbuf;      /* W*H, stores 1/z */
} Frame;

void frame_clear(Frame *f);
void render_torus(Frame *f, double A, double B, double R1, double R2, double K1, double K2);
void frame_print(const Frame *f);         /* ANSI: cursor home, then rows */
void sleep_ms(long ms);                   /* nanosleep wrapper */
```

Step sizes: $\theta$ step $0.07$, $\phi$ step $0.02$ (about 90 x 314 samples per frame). Per frame: `A += 0.04; B += 0.02`.

**Headless frame file**: exactly `H` lines of `W` characters, so you can `diff` runs and inspect a frame without a terminal.

## Milestones

1. **M1 — static ring.** Sample the torus with $A = B = 0$, project, plot `#` for every sample (no z-buffer, no shading). You'll know it works when a flattened ring/ellipse fills the terminal with correct aspect ratio.
2. **M2 — rotation.** Apply $R_x(A)$, $R_z(B)$ with fixed $A = 1, B = 0.5$: the ring tilts and shows both the outer rim and the hole.
3. **M3 — z-buffer.** With shading temporarily set to two characters (front `#`, everything else `.`), the far side of the tube stops bleeding through the near side.
4. **M4 — Lambert shading.** The full 12-character ramp; the lit side is `@`/`$`, the terminator fades to `.`; the inside of the hole is dark. Move the light vector and watch the highlight move.
5. **M5 — animation.** Clear with `\x1b[2J`, home with `\x1b[H`, print, `nanosleep` for `1000/fps` ms. Smooth 30 fps, no flicker, no scrolling. Ctrl-C exits cleanly (restore the cursor with `\x1b[?25h`).
6. **M6 — headless mode + golden test.** Write 3 frames to files; check that frame 0 is symmetric across the vertical center when $B = 0$, and that the fraction of non-space characters is within 25-45% for the default parameters.

## Verification

Analytic checks (no library needed):

- With $A = B = 0$ the projected shape must be symmetric: cell $(x, y)$ is non-empty iff $(W-1-x, y)$ is (up to rounding) — count asymmetries in the headless frame, expect < 2%.
- Normals: assert $|\mathbf n| = 1$ to 1e-12 for random $(\theta,\phi)$; assert that $\mathbf n \cdot (\mathbf p - \mathbf c) > 0$ where $\mathbf c$ is the tube center $(R_1\cos\phi, 0, -R_1\sin\phi)$ (normal points outward).
- Rotation preserves length: $|R\mathbf v| = |\mathbf v|$ to 1e-12; $R_x(a)R_x(-a) = I$.
- Visual: the animation should look like the reference at a1k0n.net (search "donut math"). The famous compressed version's output is the target.

Python reference for one frame if you want a pixel-level diff:

```python
import numpy as np
def frame(A, B, W=80, H=24, R1=1, R2=2, K2=5):
    K1 = W*K2*3/(8*(R1+R2)); out = np.full((H, W), ' '); zb = np.zeros((H, W))
    for th in np.arange(0, 2*np.pi, 0.07):
        for ph in np.arange(0, 2*np.pi, 0.02):
            ct, st, cp, sp = np.cos(th), np.sin(th), np.cos(ph), np.sin(ph)
            cA, sA, cB, sB = np.cos(A), np.sin(A), np.cos(B), np.sin(B)
            cx = R2 + R1*ct; cy = R1*st
            x = cx*(cB*cp + sA*sB*sp) - cy*cA*sB; y = cx*(sB*cp - sA*cB*sp) + cy*cA*cB
            z = K2 + cA*cx*sp + cy*sA; ooz = 1/z
            xp, yp = int(W/2 + K1*ooz*x), int(H/2 - K1*ooz*y/2)
            L = cp*ct*sB - cA*ct*sp - sA*st + cB*(cA*st - ct*sA*sp)
            if L > 0 and 0 <= xp < W and 0 <= yp < H and ooz > zb[yp, xp]:
                zb[yp, xp] = ooz; out[yp, xp] = ".,-~:;=!*#$@"[int(L*8)]
    return "\n".join("".join(r) for r in out)
print(frame(1.0, 0.5))
```

(Note this reference uses a1k0n's conventions: `R1` is the tube radius and `R2` the center distance, and $y$ is halved for aspect. Match your parameters accordingly or compare qualitatively.)

## Stretch goals

- Read the terminal size with `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` and fill it.
- Color: 256-color ANSI (`\x1b[38;5;Nm`) with luminance mapped to a gradient; or true-color with RGB.
- Two donuts, or a donut and a sphere, sharing one z-buffer. A second light source.
- Obfuscate: get it under 25 lines while producing byte-identical headless frames to your readable version (diff them).

## Hints

- Precompute `sin`/`cos` of $A$ and $B$ once per frame; inside the double loop only $\sin\theta, \cos\theta, \sin\phi, \cos\phi$ change.
- The `Frame` buffer is one `malloc` of `W*H` chars plus one of `W*H` doubles; clear with `memset` (spaces) and a loop (zeros) each frame.
- Print the whole frame with one `fwrite` per row (or build one big string) after moving the cursor home. Many small `printf`s flicker.
- `nanosleep` takes a `struct timespec`; `1000/fps` ms is 33 ms at 30 fps. Measure actual frame time with `clock_gettime(CLOCK_MONOTONIC, ...)` and subtract.
- Handle `SIGINT` with a handler that sets a `volatile sig_atomic_t` flag so the loop exits and restores the terminal.
- If the donut looks squashed, your aspect correction is wrong; if it looks inside-out, your z-buffer comparison is reversed; if the shading is inverted, your light vector or normal sign is.

## Where to put it

`simulations/donut/` — `donut.c`, `Makefile`, optionally `donut_tiny.c` (the obfuscated version) and `frames/` in `.gitignore`.
