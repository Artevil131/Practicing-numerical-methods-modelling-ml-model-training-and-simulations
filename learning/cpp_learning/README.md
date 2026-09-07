# C++ Course

Start this after finishing the C course (at least chapters 01–11 and projects P01–P02). C++
is taught here as *what changes and what it buys you* on top of C, not from zero.

Every chapter folder has:

| File           | What it is                                                                  |
|----------------|-----------------------------------------------------------------------------|
| `lesson.md`    | The full explanation of every concept in the chapter.                       |
| `exercises.md` | 10 exercises, easy → hard. Last 3 always point at ML / numerics / sims.     |
| `example.cpp`  | One compilable program using everything in the chapter. Read, run, modify.  |

## How to work a chapter

```sh
cd cpp_learning/01_from_c_to_cpp
c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
# exercises: ex01_1.cpp, ex01_2.cpp, ...
c++ -Wall -Wextra -std=c++17 -g -fsanitize=address,undefined -o ex01_1 ex01_1.cpp && ./ex01_1
```

From chapter 11 on, use CMake instead of one-liners.

## Chapters

| #  | Chapter                                    | Core idea                                            | Unlocks project |
|----|--------------------------------------------|------------------------------------------------------|-----------------|
| 01 | `01_from_c_to_cpp`                         | References, `auto`, namespaces, `std::string`, casts |                 |
| 02 | `02_classes_and_raii`                      | Constructors/destructors, RAII, Rule of Three        | P01             |
| 03 | `03_references_const_and_value_semantics`  | `const&`, value semantics, RVO, dangling refs        | P01             |
| 04 | `04_std_vector_string_and_containers`      | vector/string/map/unordered_map, iterators           | P01             |
| 05 | `05_templates`                             | `Matrix<T>`, `Vec<N>`, specialization, `if constexpr`| P02, P06        |
| 06 | `06_operator_overloading`                  | `A*B+C`, `m(i,j)`, `operator<<`, `Vec3`              | P01             |
| 07 | `07_move_semantics_and_smart_pointers`     | Rule of Five, `unique_ptr`, `shared_ptr`, ownership  | P01, P03        |
| 08 | `08_stl_algorithms_and_lambdas`            | Lambdas, `<algorithm>`, `<random>`                   | P04             |
| 09 | `09_inheritance_and_polymorphism`          | `virtual`, `Layer` hierarchy, `variant` alternative  | P04             |
| 10 | `10_error_handling`                        | Exceptions, `optional`, `expected`, strategy         | P01             |
| 11 | `11_headers_build_cmake_testing`           | CMake, doctest, ctest, compile flags                 | P01             |
| 12 | `12_performance`                           | Cache, SoA, tiling, NEON, OpenMP, BLAS               | P07–P10         |
| 13 | `13_modern_cpp_and_idioms`                 | C++17/20 features, the numerics subset of C++        |                 |

### Advanced (professional level) — after projects P01–P04; compile these with `-std=c++20`

| #  | Chapter                                          | Core idea                                                     |
|----|--------------------------------------------------|---------------------------------------------------------------|
| 14 | `14_templates_advanced_and_metaprogramming`      | Deduction, forwarding, variadics, traits, SFINAE → concepts, CRTP, expression templates, constexpr |
| 15 | `15_concurrency_and_the_memory_model`            | threads, mutex/condvar, atomics & memory orders, futures, thread pool, SPSC queue, OpenMP, TSan |
| 16 | `16_object_lifetime_allocators_and_memory`       | Lifetime rules, placement new, allocators, `std::pmr`, SmallVector, tensor storage/views |
| 17 | `17_cpp20_23_features_ranges_coroutines_modules` | Ranges, span/mdspan, format, coroutines (Generator), modules status, `<=>`, feature-test macros |
| 18 | `18_software_design_and_library_architecture`    | Value semantics, type erasure, strong types, pimpl/ABI, API design, layering, reading ATen/Eigen/LLVM |
| 19 | `19_professional_tooling_and_quality`            | Warnings, sanitizers, fuzzing, clang-tidy/format, doctest, Google Benchmark, profilers, lldb, CMake at scale, CI |
| 20 | `20_cpp_for_numerics_and_hpc`                    | BLAS/LAPACK (Accelerate), Eigen, sparse/CG, mixed precision, SIMD, OpenMP/MPI, GPU overview, `.npy` I/O |

## Projects

See [`projects/README.md`](projects/README.md) — 12 projects: `Matrix` → `Tensor<T>` →
autograd → NN framework → tiny transformer on the ML side; numerical library → Barnes–Hut →
FDTD Maxwell → PIC plasma → Lattice Boltzmann → raymarched donut on the physics side.

## Reference material (external)

- cppreference — the manual: https://en.cppreference.com/w/cpp
- *A Tour of C++* (Stroustrup), 3rd ed. — short, covers C++20.
- C++ Core Guidelines — https://isocpp.github.io/CppCoreGuidelines/
- Compiler Explorer — see the assembly your code becomes: https://godbolt.org
- Eigen source — how a real numerics library is written once you're comfortable.
