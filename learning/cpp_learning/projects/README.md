# C++ Projects

Twelve projects that turn the C++ chapters (`../01_from_c_to_cpp` … `../13_modern_cpp_and_idioms`) into an ML framework, a numerical library and four physics simulations. Each spec is a `PNN_slug.md` in this folder; code goes under `cpp/` in the subdirectory named in the spec's "Where to put it". No solutions anywhere — signatures, formats, milestones and verification snippets only.

Compile with `c++ -Wall -Wextra -std=c++17 -O2`; every project uses CMake with a test target (doctest) and, where relevant, a bench target. Start every project by porting the corresponding C project's tests, then make them pass in C++.

## Table

| # | Title | Difficulty | Prereq chapters | Builds on |
|---|---|---|---|---|
| P01 | [Matrix Class](P01_matrix_class.md) | ★★☆☆☆ | 01-07, 11 | C P02 |
| P02 | [Tensor Template](P02_tensor_template.md) | ★★★☆☆ | 05, 06 | P01 |
| P03 | [Tensor Autograd](P03_tensor_autograd.md) | ★★★★☆ | 07, 09, 13 | P02, C P13 |
| P04 | [Neural Network Framework](P04_nn_framework.md) | ★★★☆☆ | 09 | P03, C P12 |
| P05 | [Tiny Transformer](P05_tiny_transformer.md) | ★★★★★ | 01-13 | P04, C P16, C P03 |
| P06 | [Numerical Library](P06_numerical_library.md) | ★★★★☆ | 05, 08, 10, 11 | P01, C P05, C P09 |
| P07 | [Barnes–Hut N-Body (3-D, parallel)](P07_barnes_hut_nbody.md) | ★★★★☆ | 07, 12 | C P15, P01 |
| P08 | [FDTD Maxwell 2-D](P08_fdtd_maxwell_2d.md) | ★★★★☆ | 12 | P06, C P14 |
| P09 | [Particle-in-Cell Plasma 1-D](P09_pic_plasma_1d.md) | ★★★★★ | 01-13 | P06, P08, C P15 |
| P10 | [Lattice Boltzmann Fluid (D2Q9)](P10_lattice_boltzmann_fluid.md) | ★★★★☆ | 12 | P08, C P14 |
| P11 | [Random Forest and Gradient Boosting](P11_random_forest_and_gbm.md) | ★★★★☆ | 05, 08 | C P10, P02 |
| P12 | [Raymarched Donut and Beyond](P12_raymarched_donut_and_beyond.md) | ★★★☆☆ | 12 | C P08, P01 |

## Dependency diagram

Arrows point from a project to the projects that use it. Boxes on the left edge are C projects being ported or extended.

```
 C P02 matrix ----> P01 matrix_class ----> P02 tensor_template ----> P03 tensor_autograd ----> P04 nn_framework ----> P05 tiny_transformer
                       |    |                    |                        ^                                                 ^
                       |    |                    v                        |                                                 |
                       |    |               P11 random_forest_gbm    C P13 scalar_autograd            C P16 char_lm + C P03 bpe
                       |    |                    ^
                       |    |                    |
                       |    |               C P10 trees
                       |    |
                       |    +-------> P06 numerical_library  <---- C P05 gauss, C P09 ode
                       |                 |        |
                       |                 |        +----------> P08 fdtd_maxwell_2d  <---- C P14 heat_wave
                       |                 |                         |         |
                       |                 v                         v         v
                       |               (fft, tridiag) --------> P09 pic_plasma_1d      P10 lattice_boltzmann
                       |                                           ^
                       +------> P07 barnes_hut_nbody  <---- C P15 nbody
                       |
                       +------> P12 raymarched_donut  <---- C P08 ascii_donut
```

Reading the diagram: the top row is the ML spine (`P01 → P02 → P03 → P04 → P05`); `P06` is the root of the physics column and feeds `P08` and `P09`; `P07`, `P10`, `P12` are largely standalone ports/extensions of C projects that mainly exercise C++ 12 (performance) and can be slotted in anywhere after `P01`.

## Recommended order

Default sequence, interleaving the two columns so the performance chapter (12) lands when the simulations need it:

```
P01 -> P02 -> P06 -> P03 -> P07 -> P04 -> P12 -> P08 -> P11 -> P05 -> P10 -> P09
```

Do `P01` first regardless of track; it establishes the CMake/doctest workflow every other project assumes.

### Track A — ML-first

Goal: a working transformer as soon as possible; simulations afterwards.

```
P01 matrix_class
P02 tensor_template
P03 tensor_autograd         <- the hard one: broadcasting gradients
P04 nn_framework            <- 98% MNIST is the regression test
P05 tiny_transformer        <- destination of the ML track
P11 random_forest_gbm       (independent; good palate cleanser)
--- then physics ---
P06 numerical_library
P12 raymarched_donut
P07 barnes_hut_nbody
P08 fdtd_maxwell_2d
P10 lattice_boltzmann
P09 pic_plasma_1d
```

### Track B — physics-first

Goal: get animations and validated simulations early; the ML stack later.

```
P01 matrix_class
P06 numerical_library       <- FFT, LU, RK45 needed by the sims
P12 raymarched_donut        (fast win; multithreading pattern)
P07 barnes_hut_nbody        <- OpenMP, octree, scaling study
P08 fdtd_maxwell_2d
P10 lattice_boltzmann
P09 pic_plasma_1d           <- capstone of the physics track
--- then ML ---
P02 tensor_template
P03 tensor_autograd
P04 nn_framework
P11 random_forest_gbm
P05 tiny_transformer
```

## Conventions shared by all specs

- One top-level `cpp/CMakeLists.txt` with `add_subdirectory` per project; each project has `include/`, `src/`, `tests/` (doctest, registered with CTest), and usually `apps/` and `bench/`.
- Tensors and parameters are exchanged with Python as `.npy` (P02 defines the reader/writer); matrices from C use the `MAT1` format (C P02).
- Every gradient is checked numerically and against PyTorch in `float64` (P03 M2–M5, P04 M4, P05 M2–M3).
- Every simulation reports conserved or known quantities each frame and has at least one quantitative test against theory: Kepler/energy (P07), Fresnel and cavity modes (P08), $\omega_p$ and two-stream growth rate (P09), Poiseuille profile and Strouhal number (P10).
- Frames are PPM written with a fixed color scale; assemble with `ffmpeg -framerate 30 -i frames/f_%04d.ppm -c:v libx264 -pix_fmt yuv420p out.mp4`.
- Performance claims are measured, not assumed: each C++ 12-heavy project (P07, P08, P10, P12) has a `bench` mode and a layout/threading comparison in its milestones.
