# Chapter 11 — Exercises

Write each single-file exercise as `ex11_K.cpp` in this folder. Several exercises here are
multi-file: put those in a subdirectory `ex11_K/` with its own `CMakeLists.txt`. You need
`brew install cmake ninja` for 11.4 onward. Compile single files with
`c++ -Wall -Wextra -std=c++17 -O2 -o ex11_K ex11_K.cpp`.

---

**11.1 — Provoke and fix the three classic link errors**
Create `ex11_1/` with `a.cpp`, `b.cpp`, and `util.hpp`. Make each of these happen on purpose, record the exact compiler/linker message in a comment, then fix it:
(a) a non-inline function defined in `util.hpp` and included from both `.cpp` files (duplicate symbol);
(b) a template declared in `util.hpp` but defined only in `a.cpp` and used from `b.cpp` (undefined symbol);
(c) `util.hpp` without `#pragma once` included twice by `a.cpp` (redefinition).
Build by hand: `c++ -std=c++17 -Wall -Wextra a.cpp b.cpp -o ex11_1`.

<details><summary>Hint</summary>
The fixes are: `inline` (or move to a `.cpp`), move the template body into the header, `#pragma once`. Read the error messages slowly — the linker names both object files involved.
</details>

---

**11.2 — Forward declarations and an incomplete-type `unique_ptr`**
Create `ex11_2/` with `matrix.hpp`/`matrix.cpp` (a minimal `Matrix`), and `solver.hpp`/`solver.cpp` containing `class Solver` that holds a `std::unique_ptr<Matrix>` member. `solver.hpp` must **not** include `matrix.hpp` — forward-declare instead. First define `~Solver()` implicitly (inside the class) and record the compiler error; then fix it by declaring `~Solver();` in the header and defining it `= default` in the `.cpp`. Confirm `main.cpp` compiles including only `solver.hpp`. Bonus: check with `c++ -E solver.hpp | wc -l` how many preprocessed lines the header expands to with and without the forward declaration.

<details><summary>Hint</summary>
`unique_ptr<T>`'s destructor needs `sizeof(T)` and `T::~T`. Whoever instantiates that destructor must see the complete type — so put it in `solver.cpp`, which includes `matrix.hpp`.
</details>

---

**11.3 — Hand-rolled test macros**
Write `ex11_3.cpp` containing `CHECK(expr)`, `CHECK_CLOSE(a, b, rtol, atol)`, and `CHECK_THROWS(expr, ExceptionType)` macros that count passes/failures and print `file:line: CHECK failed: <expr text>`. `CHECK_THROWS` must fail both when nothing is thrown and when the wrong type is thrown. Use them to test a `close()` function, a `safe_sqrt` that throws `std::domain_error`, and one deliberate failure. `main` returns 0 only when all checks pass.

Example failing line: `ex11_3.cpp:41: CHECK failed: 0.1 + 0.2 == 0.3`.

<details><summary>Hint</summary>
Stringify with `#expr`. For `CHECK_THROWS`, the macro body is a `try { expr; fail; } catch (const T&) { pass; } catch (...) { fail; }` wrapped in `do { } while (0)`.
</details>

---

**11.4 — First CMake project**
Create `ex11_4/` with `include/vec/vec.hpp`, `src/vec.cpp` (a 3-vector with `dot`, `cross`, `norm`), and `apps/demo.cpp`. Write a `CMakeLists.txt` that builds a `STATIC` library `vec`, an executable `demo` linking it, uses `target_compile_features(vec PUBLIC cxx_std_17)`, `target_include_directories(vec PUBLIC include)`, and `-Wall -Wextra` via `target_compile_options`. Build twice: `cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug` and `-B build-release ... Release`. Use `cmake --build build-release -v` to find and paste (as a comment in your CMakeLists) the exact compiler command line for `vec.cpp` in each build type.

<details><summary>Hint</summary>
Look for `-O3 -DNDEBUG` vs `-g` in the verbose output. If Ninja isn't installed, drop `-G Ninja` and you get Makefiles — the rest is identical.
</details>

---

**11.5 — `option()` for sanitizers and `compile_commands.json`**
Extend 11.4: add `option(VEC_SANITIZE "ASan+UBSan" OFF)` that, when ON, adds `-fsanitize=address,undefined -fno-omit-frame-pointer` to both compile and link options of every target (use an `INTERFACE` library to carry the flags). Turn on `CMAKE_EXPORT_COMPILE_COMMANDS`. Add a deliberate out-of-bounds write in `demo.cpp` behind a `#ifdef VEC_BUG` and a `target_compile_definitions` controlled by another option. Show that the sanitized build reports the bug with a stack trace and the plain build does not.

<details><summary>Hint</summary>
`target_link_options` exists since CMake 3.13. An `INTERFACE` library has no sources; anything you `target_link_libraries` to it inherits its `INTERFACE` options.
</details>

---

**11.6 — doctest via FetchContent + ctest**
Extend 11.4/11.5 with `tests/CMakeLists.txt` that pulls doctest `v2.4.11` through `FetchContent`, builds `test_vec` from `tests/test_vec.cpp`, and registers it with `add_test`. Write at least six `TEST_CASE`s including one `SUBCASE` and one `CHECK_THROWS_AS`. Run `ctest --test-dir build-debug --output-on-failure`, then break one test on purpose and confirm ctest reports the failure with the doctest diagnostics.

<details><summary>Hint</summary>
Exactly one TU defines `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`. `FetchContent_MakeAvailable(doctest)` gives you the `doctest::doctest` target. Configure needs network access the first time.
</details>

---

**11.7 — Compile-time profiling**
Write `ex11_7_heavy.cpp` that includes `<iostream>`, `<regex>`, `<algorithm>`, `<random>`, `<map>` and does trivial work, and `ex11_7_light.cpp` that does the same work with `<cstdio>` and only the headers it needs. Time both with `time c++ -std=c++17 -c FILE`. Then run with `-ftime-trace` and open the JSON in `https://ui.perfetto.dev`. Write down (as comments at the top of each file) the wall-clock compile time and the single most expensive include.

<details><summary>Hint</summary>
`-c` stops after producing the `.o`, which is what you want to time. `-ftime-trace` writes `ex11_7_heavy.json` next to the object. In the flame graph, look for the `Source` events.
</details>

---

**11.8 — Gradient check for logistic-regression loss (ML)**
In `ex11_8.cpp`, implement `double bce_loss(const std::vector<double>& w, double b, const std::vector<std::vector<double>>& X, const std::vector<int>& y)` (binary cross-entropy of a sigmoid model) and `void bce_grad(...)` producing analytic gradients for `w` and `b`. Then write a central-difference gradient check with `h = 1e-5`, comparing each component using `close(analytic, numeric, 1e-5, 1e-7)`, on a random dataset with a seeded `std::mt19937` (`n = 20`, `d = 3`). Print each pair and a final PASS/FAIL. Then repeat with `h = 1e-12` and `h = 1e-1` and record in a comment why both fail.

Example output line: `dL/dw[1]: analytic=0.128374 numeric=0.128374 ok`.

<details><summary>Hint</summary>
Loss `L = -mean(y log p + (1-y) log(1-p))`, `p = sigmoid(w·x + b)`. The gradient is `dL/dw = mean((p - y) x)`, `dL/db = mean(p - y)`. Clamp `p` away from 0 and 1 before `log`.
</details>

---

**11.9 — Property tests for a matrix library (numerics)**
In `ex11_9.cpp` (or a CMake project with doctest if you completed 11.6), write a small `Matrix` with `matmul`, `transpose`, `identity(n)`, and `random(r, c, seed)`. Write property tests that hold for random matrices: `A@I == A`, `I@A == A`, `(A@B)^T == B^T@A^T`, `(A^T)^T == A`, `(A@B)@C ≈ A@(B@C)`, and `trace(A@B) == trace(B@A)`. Each uses an `allclose(Matrix, Matrix, rtol, atol)` helper. Run each property for 20 random seeds and dimensions between 1 and 8. Then introduce a subtle bug (swap `i`/`j` in `transpose` for non-square matrices, or use `k < a.rows()` instead of `a.cols()` in matmul) and see which properties catch it.

<details><summary>Hint</summary>
Choose rtol from the accumulated rounding error: `~1e-13` for these sizes. Keep the random values in `[-1, 1]` so magnitudes stay comparable. Note which bugs only appear on non-square shapes — that's why dimensions must vary.
</details>

---

**11.10 — A real project skeleton: N-body with library, demo, benchmark, and tests (sims)**
Create `ex11_10/` as a full CMake project: `include/nbody/{vec3.hpp,body.hpp,integrator.hpp}`, `src/*.cpp`, `apps/demo.cpp` (runs 100 steps of a 3-body problem and prints positions), `bench/bench.cpp` (times 1000 steps of 500 bodies with `std::chrono` — no correctness checks), and `tests/test_nbody.cpp` (doctest) with: two-body circular orbit keeps radius within `1e-6` after one period; total momentum is conserved to `1e-12`; total energy drift over 1000 leapfrog steps is below `1e-4` relative. Options: `NBODY_BUILD_BENCH` (default OFF), `NBODY_SANITIZE`. Use `RelWithDebInfo` for the benchmark directory and `Debug` + sanitizers for the tests directory. Add a `.gitignore` for `build*/`. Write a 10-line `README.md` in `ex11_10/` with the exact commands to configure, build, test, and run the benchmark.

<details><summary>Hint</summary>
Leapfrog (kick-drift-kick) is symplectic — energy oscillates but does not drift, which is what makes the third test possible. For the circular orbit test, set `v = sqrt(G M / r)`. Put the integrator in a `.cpp` and keep `vec3` operators inline in the header.
</details>
