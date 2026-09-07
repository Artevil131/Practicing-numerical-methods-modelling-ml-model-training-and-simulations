# C Projects

Sixteen projects that turn the C chapters (`../01_hello_compile_types` … `../14_preprocessor_and_c_idioms`) into working ML, numerics and simulation code. Each spec is a `PNN_slug.md` in this folder; the code goes where the spec's "Where to put it" section says (`neural_network_c/`, `numerical_methods/`, `simulations/`). No solutions are provided anywhere — the specs give signatures, file formats, milestones and verification snippets only.

Compile everything with `cc -Wall -Wextra -std=c11 -O2 ... -lm`; debug with `-g -fsanitize=address,undefined`.

## Table

| # | Title | Difficulty | Prereq chapters | Builds on |
|---|---|---|---|---|
| P01 | [Character Tokenizer](P01_char_tokenizer.md) | ★☆☆☆☆ | 01-05 | — |
| P02 | [Matrix Library](P02_matrix_library.md) | ★★☆☆☆ | 01-09 | P01 |
| P03 | [Byte-Level BPE Tokenizer](P03_bpe_tokenizer.md) | ★★★☆☆ | 06, 10 | P01 |
| P04 | [MNIST Loader and Viewer](P04_mnist_loader_and_viewer.md) | ★★☆☆☆ | 08, 12 | P02 |
| P05 | [Linear Regression](P05_linear_regression.md) | ★★☆☆☆ | 01-09, 12 | P02 |
| P06 | [Logistic Regression](P06_logistic_regression.md) | ★★☆☆☆ | 01-09, 12 | P05, P04 |
| P07 | [k-Means and k-NN](P07_kmeans_and_knn.md) | ★★☆☆☆ | 10 | P04, P02 |
| P08 | [ASCII Spinning Donut](P08_ascii_spinning_donut.md) | ★★☆☆☆ | 12, math.h | (P02) |
| P09 | [ODE Solvers](P09_ode_solvers.md) | ★★★☆☆ | 11, 12 | P05 |
| P10 | [Decision Tree and Random Forest](P10_decision_tree_and_forest.md) | ★★★☆☆ | 07, 10 | P05 |
| P11 | [Softmax Classifier on MNIST](P11_softmax_classifier_mnist.md) | ★★★☆☆ | 01-09, 12 | P06, P04 |
| P12 | [Multilayer Perceptron on MNIST](P12_mlp_mnist.md) | ★★★★☆ | 11, 13 | P11, P02 |
| P13 | [Scalar Autograd](P13_scalar_autograd.md) | ★★★★☆ | 10, 11, 14 | P12 |
| P14 | [Heat and Wave Equations 1-D/2-D](P14_heat_and_wave_equation_1d_2d.md) | ★★★☆☆ | 08, 12 | P04, P09 |
| P15 | [N-Body Gravity](P15_nbody_gravity.md) | ★★★★☆ | 10 | P09, P04, P10 |
| P16 | [Character-Level Language Model](P16_char_language_model.md) | ★★★★★ | 01-14 | P12, P13, P01/P03, P11 |

## Dependency diagram

Arrows point from a project to the projects that use it.

```
                 P01 char_tokenizer
                  |         \
                  v          \
                 P03 bpe      \
                  |            \
                  |             v
 P02 matrix ------+---------> P16 char_language_model  <---- P13 scalar_autograd
  |  |  |  \                     ^                              ^
  |  |  |   \                    |                              |
  |  |  |    v                   |                              |
  |  |  |   P04 mnist_loader ----+----> P07 kmeans_knn          |
  |  |  |     |     |            |                              |
  |  |  |     |     v            |                              |
  |  |  |     |    P06 logreg -> P11 softmax -> P12 mlp --------+
  |  |  |     |        ^
  |  |  v     |        |
  |  | P05 linreg -----+------> P10 decision_tree ----.
  |  |    |                                            \
  |  |    v                                             v
  |  |  P09 ode_solvers -------> P15 nbody_gravity  (quadtree)
  |  |        |                        ^
  |  |        v                        |
  |  |  P14 heat_wave  <--- (PGM writer from P04)
  |  v
  | P08 ascii_donut   (rotation matrices by hand; standalone)
```

Reading the diagram: `P02` is the root of the ML column; `P04` supplies data and image output to everything on the right; `P09` is the root of the physics column; `P16` is where the ML column ends and hands off to the C++ transformer.

## Recommended order

The default sequence interleaves the two columns so the C chapters are exercised in roughly the order they are taught:

```
P01 -> P02 -> P04 -> P05 -> P08 -> P06 -> P03 -> P07 -> P09 -> P11 -> P10 -> P12 -> P14 -> P13 -> P15 -> P16
```

Do P01–P05 before anything else regardless of track. After that, pick one of the two tracks below or interleave.

### Track A — ML-first

Goal: reach a trained language model as fast as possible, then come back for physics.

```
P01 char_tokenizer
P02 matrix_library
P04 mnist_loader
P05 linear_regression
P06 logistic_regression
P11 softmax_classifier
P12 mlp_mnist               <- the big one: hand backprop + gradient check
P03 bpe_tokenizer
P13 scalar_autograd
P16 char_language_model     <- hand off to C++ P05 tiny_transformer
--- then physics ---
P08 ascii_donut
P09 ode_solvers
P07 kmeans_knn
P10 decision_tree
P14 heat_wave
P15 nbody_gravity
```

### Track B — physics-first

Goal: get to simulations that produce animations early; keep enough ML to reuse the matrix library.

```
P01 char_tokenizer          (short; teaches file I/O)
P02 matrix_library
P04 mnist_loader            (for the PGM/PPM writer — you need it for frames)
P08 ascii_donut
P05 linear_regression       (CSV + Gaussian elimination, needed for Poisson later)
P09 ode_solvers
P14 heat_wave               <- CFL, frames, ffmpeg pipeline
P10 decision_tree           (tree data structure practice before the quadtree)
P15 nbody_gravity           <- hand off to C++ P07 barnes_hut
--- then ML ---
P06 logistic_regression
P07 kmeans_knn
P11 softmax_classifier
P12 mlp_mnist
P03 bpe_tokenizer
P13 scalar_autograd
P16 char_language_model
```

## Conventions shared by all specs

- Binary matrices use the `MAT1` format defined in P02; Python reads them with the `load_mat` helper given there.
- Token id streams use the `int32` count-prefixed format defined in P01.
- Images are PGM (P5) / PPM (P6) written by the P04 `image_io` functions; animations are assembled with `ffmpeg -framerate 30 -i frames/f_%04d.pgm out.gif`.
- Training logs are CSV with a header row, plotted with a `plot.py` next to the code.
- Every gradient is verified with finite differences before anything is trained on it (P05 M4, P06 M2, P11 M2, P12 M2, P13 M3, P16 M3).
- Every simulation reports a conserved quantity every frame (P09 energy, P14 total heat / wave energy, P15 energy, momentum, angular momentum).
