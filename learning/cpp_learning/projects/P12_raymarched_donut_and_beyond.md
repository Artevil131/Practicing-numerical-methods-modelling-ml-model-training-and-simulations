# P12 — Raymarched Donut and Beyond

**Difficulty:** ★★★☆☆   **Prereq chapters:** C++ 12 (plus 01-08, 11)   **Builds on:** C P08 (donut), P01

## Goal

A signed-distance-field raymarcher: a scene is a function `float sdf(vec3)` composed from primitives (torus, sphere, box, plane) and CSG operators (union, smooth union, intersection, subtraction, repetition); a camera shoots a ray per pixel and marches it by the SDF distance (sphere tracing); normals from the SDF gradient; Phong shading with a point light; soft shadows and ambient occlusion from extra marches; camera orbit over frames; multithreaded per-row rendering; PPM frames → video. The spinning donut, done properly, at 1920x1080.

## Why

C P08 approximated the torus by sampling its surface and splatting characters. Raymarching inverts that: for each pixel, find where the ray hits *any* implicit surface — which is how Shadertoy demos, many game effects, and volumetric renderers work. The SDF machinery (distance functions, gradients by finite differences, smooth minimum) is the same math as level-set methods in fluids; the per-row multithreading with a work queue is the embarrassingly-parallel pattern from P07 and P10 in its simplest form; a `Vec3` class with operator overloading and `constexpr` is a small, satisfying exercise in the C++ 06/13 material. It's also the only project whose output is beautiful by design.

## The math

**Signed distance functions** (distance to the surface, negative inside):

- Sphere radius $r$: $d(\mathbf p) = |\mathbf p| - r$.
- Torus (major $R$, minor $r$, axis $y$): $d(\mathbf p) = \left|\left(\sqrt{p_x^2 + p_z^2} - R,\ p_y\right)\right| - r$.
- Box half-extents $\mathbf b$: $\mathbf q = |\mathbf p| - \mathbf b$; $d = |\max(\mathbf q, 0)| + \min(\max(q_x, q_y, q_z), 0)$.
- Plane with normal $\mathbf n$ ($|\mathbf n| = 1$) at height $h$: $d = \mathbf p\cdot\mathbf n + h$.

**CSG**: union $\min(d_1, d_2)$; intersection $\max(d_1, d_2)$; subtraction $\max(d_1, -d_2)$; smooth union with blend $k$: $h = \text{clamp}(\tfrac12 + \tfrac{d_2 - d_1}{2k}, 0, 1)$, $d = \text{lerp}(d_2, d_1, h) - k h(1 - h)$. Transformations: rotate/translate the *point* by the inverse transform before evaluating the SDF; infinite repetition: $\mathbf p \leftarrow \text{mod}(\mathbf p + \tfrac{\mathbf c}{2}, \mathbf c) - \tfrac{\mathbf c}{2}$.

**Sphere tracing.** Ray $\mathbf r(t) = \mathbf o + t\mathbf d$, $|\mathbf d| = 1$. Starting at $t = 0$: $t \leftarrow t + \text{sdf}(\mathbf r(t))$ until $\text{sdf} < \epsilon$ (hit) or $t > t_{\max}$ or too many steps (miss). Correct because the SDF is a lower bound on the distance to any surface along the ray. Typical: $\epsilon = 10^{-4}$, 128–256 steps, $t_{\max} = 100$.

**Normal** by central differences of the SDF: $\mathbf n = \text{normalize}\big(\text{sdf}(\mathbf p + h\mathbf e_x) - \text{sdf}(\mathbf p - h\mathbf e_x), \ldots\big)$, $h = 10^{-4}$ (6 evaluations; the tetrahedron trick uses 4).

**Camera.** Eye $\mathbf e$, target $\mathbf c$, up $\mathbf u$; forward $\mathbf f = \text{norm}(\mathbf c - \mathbf e)$, right $\mathbf r = \text{norm}(\mathbf f\times\mathbf u)$, true up $\mathbf u' = \mathbf r\times\mathbf f$. For pixel $(i, j)$ in a $W\times H$ image with vertical FOV $\theta$: $u = (2(i + 0.5)/W - 1)\,\tfrac WH\tan\tfrac\theta2$, $v = (1 - 2(j+0.5)/H)\tan\tfrac\theta2$, direction $\mathbf d = \text{norm}(u\,\mathbf r + v\,\mathbf u' + \mathbf f)$. Orbit: $\mathbf e = \mathbf c + \rho(\cos\phi\cos\alpha, \sin\alpha, \sin\phi\cos\alpha)$ with $\phi$ advancing per frame.

**Phong shading** with light at $\mathbf L$, view direction $\mathbf v = -\mathbf d$, $\mathbf l = \text{norm}(\mathbf L - \mathbf p)$, $\mathbf h = \text{norm}(\mathbf l + \mathbf v)$:

$$I = k_a + k_d\max(0, \mathbf n\cdot\mathbf l) + k_s\max(0, \mathbf n\cdot\mathbf h)^{\alpha}$$

(Blinn–Phong form), per RGB channel with the material albedo; gamma-correct at the end: $c \leftarrow c^{1/2.2}$.

**Soft shadow**: march from $\mathbf p + \epsilon\mathbf n$ toward the light; track $\min_t k\,\text{sdf}(\mathbf r(t))/t$ (penumbra factor $k \approx 8$–$32$); clamp to $[0, 1]$; multiply the diffuse and specular terms by it. **Ambient occlusion**: sample the SDF at $\mathbf p + s_i\mathbf n$ for $s_i = 0.01\cdot i$, $i = 1..5$; occlusion $= \sum_i 2^{-i}(s_i - \text{sdf})$; $k_a \leftarrow k_a(1 - \text{clamp}(\text{ao}))$.

**Antialiasing**: $n\times n$ supersampling per pixel (average), or a cheap 2x2 rotated grid.

## Spec

**Interface** (`rm.hpp`):

```cpp
struct Vec3 { double x, y, z; /* +,-,*,/ (scalar and elementwise), dot, cross, length, normalize, abs, max/min elementwise, constexpr where possible */ };
struct Ray { Vec3 o, d; };
struct Hit { bool hit; double t; Vec3 p, n; int material; };
struct Material { Vec3 albedo; double ka, kd, ks, shininess; };
struct Light { Vec3 pos; Vec3 color; };

using SDF = std::function<double(const Vec3&)>;          // or a template parameter for speed — do both, benchmark
struct SceneSample { double d; int material; };
using Scene = std::function<SceneSample(const Vec3&)>;

// primitives and operators (pure functions)
double sd_sphere(const Vec3& p, double r);
double sd_torus (const Vec3& p, double R, double r);
double sd_box   (const Vec3& p, const Vec3& b);
double sd_plane (const Vec3& p, const Vec3& n, double h);
double op_union(double a, double b);  double op_smooth_union(double a, double b, double k);
double op_intersect(double a, double b);  double op_subtract(double a, double b);
Vec3   op_repeat(const Vec3& p, const Vec3& period);
Vec3   rotate_x(const Vec3& p, double a);  Vec3 rotate_y(...);  Vec3 rotate_z(...);

struct Camera { Vec3 eye, target, up; double fov_deg; Ray ray(int i, int j, int W, int H, double dx = 0.5, double dy = 0.5) const; };
static Camera orbit(const Vec3& target, double radius, double azimuth, double elevation, double fov);

struct RenderSettings { int W = 1280, H = 720; int max_steps = 256; double eps = 1e-4, t_max = 100; int aa = 1; bool shadows = true, ao = true; int threads = 0; };
Hit    march(const Scene& scene, const Ray& r, const RenderSettings& s);
Vec3   sdf_normal(const Scene& scene, const Vec3& p, double h = 1e-4);
double soft_shadow(const Scene& scene, const Vec3& p, const Vec3& l_dir, double t_max, double k);
double ambient_occlusion(const Scene& scene, const Vec3& p, const Vec3& n);
Vec3   shade(const Scene& scene, const Hit& h, const Ray& r, const std::vector<Light>& lights, const std::vector<Material>& mats, const RenderSettings& s);

class Image { public: Image(int W, int H); Vec3& at(int i, int j); void write_ppm(const std::string& path, bool gamma = true) const; };
Image render(const Scene& scene, const Camera& cam, const std::vector<Light>&, const std::vector<Material>&, const RenderSettings&);   // multithreaded rows
Scene scene_donut(double time);            // the torus spinning about two axes, floor plane
Scene scene_playground(double time);       // spheres + box CSG, smooth blends, repetition
```

**CLI**

```
./rm still  --scene donut|playground --w 1920 --h 1080 --aa 2 --t 1.0 --out frame.ppm
./rm anim   --scene donut --w 1280 --h 720 --frames 240 --fps 30 --out frames/       # then ffmpeg -framerate 30 -i frames/f_%04d.ppm -c:v libx264 -pix_fmt yuv420p donut.mp4
./rm bench  --w 1280 --h 720 --threads 1,2,4,8 --aa 1     # ms/frame table; std::function vs template SDF
./rm ascii  --w 120 --h 40 --frames 300                    # bonus: the same renderer output as ASCII luminance in the terminal, like C P08
```

**Expected performance**: 1280x720, torus + plane, shadows + AO, no AA: ~0.3–1 s/frame single-threaded, ~50–150 ms on 8 threads. 240 frames at 720p in a minute or two.

## Milestones

1. **M1 — `Vec3`, camera, march a sphere.** A white disc on black at the right size: sphere radius 1 at distance 4 with 60° FOV subtends ≈ 29° → disc diameter ≈ 0.48 of the image height. You'll know the camera math is right when moving the eye moves the disc as expected.
2. **M2 — normals + Lambert + torus.** Shaded torus with correct lighting; normals visualized as RGB (`0.5*n + 0.5`) look smooth with no seams. Compare with C P08's ASCII output at the same angles — same silhouette.
3. **M3 — Phong, floor, soft shadows, AO.** Specular highlight moves with the light; the donut casts a soft shadow on the plane; contact darkening under it. Toggling `--no-shadows` shows the difference.
4. **M4 — CSG playground.** Smooth-union blobs, a box with a sphere subtracted, an infinite grid of small spheres via `op_repeat`. No artifacts at the blend seams (if there are, the smooth-min formula or the step epsilon is off).
5. **M5 — multithreading.** Rows distributed over `std::thread`s with an atomic row counter (work stealing); speedup ≥ 6x on 8 cores; output identical to single-threaded (byte-compare PPMs). Template SDF vs `std::function` measured (expect 1.5–3x).
6. **M6 — animation + AA.** 240-frame orbit with the donut rotating about two axes (C P08's $A, B$), 2x2 supersampling, encoded to mp4. The ASCII mode prints the same scene as a luminance ramp — full circle back to P08.

## Verification

Analytic checks (write them as doctest cases):

- `sd_sphere` at distance 3 from the origin with $r = 1$ returns exactly 2; `sd_torus` at $(R + r, 0, 0)$ returns 0 and at $(R, 0, 0)$ returns $-r$; `sd_box` at a corner returns 0 and outside along a face returns the perpendicular distance.
- `sdf_normal` of a sphere at $\mathbf p$ equals $\mathbf p/|\mathbf p|$ to 1e-6; of a plane equals $\mathbf n$.
- Marching a ray straight at a sphere from distance 5 hits at $t = 4$ to within `eps`; a ray that misses returns `hit = false` after `max_steps` or $t_{\max}$.
- Disc-size check from M1: count white pixels, compare with $\pi (0.24 H)^2$ within 2%.
- Shadow sanity: a point directly under the torus (in the shadow) gets `soft_shadow` ≈ 0; a point far to the side gets 1.

Visual reference: Inigo Quilez's articles ("distance functions", "soft shadows in raymarched SDFs", "normals for an SDF") — your images should look like his diagrams.

## Stretch goals

- Reflections (one bounce: march again from the hit along the reflected direction) and a checkerboard floor.
- Fractal SDF (Mandelbulb or Menger sponge) with distance estimator — the classic raymarching showpiece; watch the step count explode near the surface.
- Depth of field (jitter the ray origin over an aperture disc, focus at the torus) with 16+ samples per pixel.
- SIMD-ify the march over 4 or 8 rays at once with `std::experimental::simd` or manual AoSoA; measure.

## Hints

- Write `Vec3` with `constexpr` operators in a header; `[[nodiscard]]` on the pure functions; `inline` everything small. Check with `-O2 -S` that `dot` and `normalize` inline (or trust the benchmark).
- The scene function returns both distance and material id so `shade` knows the material at the hit; for CSG, propagate the id of the winner of `min`/`max`.
- Sphere tracing gotcha: start the march at $t = \epsilon$ (or offset shadow/AO rays by $\epsilon\mathbf n$) or the ray immediately "hits" the surface it starts on.
- Avoid `pow` in the hot path where possible; the specular exponent is the exception — keep it.
- Row scheduler: `std::atomic<int> next_row{0}`; each thread loops `while ((j = next_row++) < H) render_row(j)`. Deterministic output regardless of thread count because each pixel depends only on its ray.
- Gamma: accumulate in linear space, clamp to $[0,1]$, apply $x^{1/2.2}$, then quantize to 8 bits — do it in one place (`write_ppm`).
- `std::function` calls are not inlined; for the benchmark write the scene as a lambda passed through a template `template <class SceneFn> Image render_t(SceneFn&&, ...)` and compare.

## Where to put it

`cpp/sims/raymarch/` — `include/rm.hpp`, `include/vec3.hpp`, `src/sdf.cpp`, `src/render.cpp`, `src/scenes.cpp`, `apps/rm.cpp`, `tests/test_sdf.cpp`, `CMakeLists.txt`; `frames/` gitignored.
