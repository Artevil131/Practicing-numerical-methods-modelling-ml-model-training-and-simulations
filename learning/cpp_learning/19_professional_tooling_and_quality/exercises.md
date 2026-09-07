# Chapter 19 — Exercises

Write each exercise as `ex19_K.cpp` in this folder unless it says otherwise (several exercises
produce config files or a small CMake project). The baseline compile line for this chapter is the
professional one:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wold-style-cast -Wnon-virtual-dtor -Werror -o ex19_K ex19_K.cpp
```

Every exercise must pass it. Where an exercise asks you to run a tool that is not installed
(`brew install llvm clang-format cppcheck google-benchmark`), install it — the tooling is the
content of this chapter. Record tool output in a comment block at the top of the file.

---

**19.1 — Warning triage**
Copy the following into `ex19_1.cpp` and make it compile *warning-free* under the full flag set above **without adding a single cast** except where a comment says a cast is the right answer (and then justify it):

```cpp
#include <cstdio>
#include <vector>
struct Base { virtual void f(double); };
struct Derived : Base { void f(int); };
double mean(std::vector<double> v) { double s = 0; for (int i = 0; i < v.size(); i++) s += v[i]; return s / v.size(); }
int idx(float x) { return x * 100; }
void print(unsigned n) { printf("%d\n", n); }
int main(int argc, char** argv) { int n = 5; { int n = argc; print(n); } float f = 0.1; double d = f * 2.0;
  std::vector<double> v = {1, 2, 3}; long total = v.size(); print(total); printf("%f %d\n", d, idx((float)mean(v))); return 0; }
```

Record every warning the original produced (flag name + line) in a comment, then your fix for each and *why* it is the honest fix. At least one `-Wshadow`, one `-Wconversion`, one `-Wsign-conversion`, one `-Woverloaded-virtual`, one `-Wnon-virtual-dtor`, one `-Wold-style-cast`, one `-Wformat` and one `-Wdouble-promotion` must appear in your list.

<details><summary>Hint</summary>
`std::size_t` for indices, `%u`/`%zu`/`%ld` in formats, `0.1f`, `using Base::f;` or a matching signature, `virtual ~Base() = default;`, `static_cast<float>` only at the one place where narrowing is the *intent* (`idx`) — and there, `std::lround` is the correct rounding.
</details>

---

**19.2 — Sanitizer report reading**
Write `ex19_2.cpp` containing six small functions, each with exactly one bug: (a) heap-buffer-overflow read, (b) stack-use-after-scope, (c) heap-use-after-free, (d) signed integer overflow, (e) misaligned `double` read through a `char*`, (f) a data race between two `std::thread`s on a plain `int`. `main` takes a letter argument and runs that case. Build twice — `-fsanitize=address,undefined` and `-fsanitize=thread` — with `-O1 -g -fno-omit-frame-pointer`, run each case, and paste the *first three lines* of each report plus the *origin* line ("allocated by", "is located in stack of", "Previous write") into a comment. Then set `ASAN_OPTIONS=detect_stack_use_after_return=1` and add case (g) stack-use-after-return, which is only detected with that option — confirm. Finally explain in a comment why case (e) is UB even though the program printed the right value.

<details><summary>Hint</summary>
For (e): `alignas(8) char buf[16]; double* p = reinterpret_cast<double*>(buf + 1);` — UBSan's `-fsanitize=alignment` fires; the fix is `std::memcpy`. For (f), TSan needs both threads to touch the variable *without* synchronization; a `std::atomic<int>` version should be clean.
</details>

---

**19.3 — A libFuzzer target for your CSV/`.npy` parser**
Write `ex19_3_parser.hpp` with `std::vector<double> parse_csv_row(std::string_view line)` (comma-separated doubles, optional spaces, throws `ParseError` on malformed input) and `ex19_3_fuzz.cpp` with `LLVMFuzzerTestOneInput` calling it. Build with Homebrew LLVM: `$(brew --prefix llvm)/bin/clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -o fuzz ex19_3_fuzz.cpp` and run for 60 seconds with a seed corpus of five valid rows and a dictionary containing `","`, `"e"`, `"-"`, `"inf"`, `"nan"`. Deliberately plant one bug (e.g. reading `line[i+1]` without checking `i+1 < size()` after a `-`), let the fuzzer find it, minimize the crash with `-minimize_crash=1`, fix it, re-run, and `-merge=1` the corpus. Commit `corpus_min/` and a `ex19_3_replay.sh` that runs the target once over the corpus with `-runs=0`. Paste the fuzzer's last status line before and after the fix.

<details><summary>Hint</summary>
Catch `ParseError` inside the target — rejecting garbage is correct. `strtod`-style parsing with `std::from_chars` (C++17, `<charconv>`; Apple clang ≥ 14 supports floating `from_chars`) avoids locale issues. If `-fsanitize=fuzzer` fails, you're using Apple clang: check `which clang++`.
</details>

---

**19.4 — `.clang-tidy` + `.clang-format` + pre-commit for your P01 Matrix**
In your P01 `cpp/matrix/` directory (or a copy), add the `.clang-tidy` and `.clang-format` from the lesson, generate `compile_commands.json` (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`), and run `run-clang-tidy -p build`. Fix every `bugprone-*`, `performance-*` and `cppcoreguidelines-narrowing-conversions` finding *properly* (no `NOLINT`); for `modernize-*`/`readability-*` apply `-fix` and review the diff. Then `clang-format -i` everything, commit with a `.git-blame-ignore-revs`, and add `.pre-commit-config.yaml`; demonstrate that `pre-commit run --all-files` passes and that a deliberately mis-indented file is rejected on commit. Write `ex19_4.md` listing each tidy finding class you hit, how many, and what you changed — plus the two findings you disagreed with and *why* you disabled those checks in `.clang-tidy` (with a comment there).

<details><summary>Hint</summary>
`performance-unnecessary-value-param` on your `Matrix operator*(Matrix a, Matrix b)` is the copy bug from `../12_performance` §7; `readability-identifier-naming` will complain about *everything* until `CheckOptions` matches your convention — set that first.
</details>

---

**19.5 — doctest suite for `Matrix` with fixtures, templates, `Approx` and a death test**
Create `ex19_5_tests.cpp` using doctest (`FetchContent` or the single header). Requirements: a `TEST_CASE_FIXTURE` with a 3×3 well-conditioned matrix and its known inverse; `SUBCASE`s for `transpose`, `trace`, `A*inv(A)`; `TEST_CASE_TEMPLATE` over `float` and `double` for `dot` symmetry where the *tolerance is derived from `std::numeric_limits<T>::epsilon()` and `n`*, not hard-coded; `CHECK_THROWS_AS` for shape mismatch; a `doctest::Approx(0.0).scale(...)` residual check with a comment deriving the scale from `n·ε·‖A‖·‖x‖`; and a hand-rolled `dies()` death test (fork + `waitpid`) asserting that `m.at(9, 9)` on a 2×2 matrix aborts in a debug build (guarded by `#ifndef NDEBUG`). Register the tests with `ctest` via `doctest_discover_tests` and show `ctest -j4 --output-on-failure` and `./ex19_5_tests --order-by=rand -s` both pass.

<details><summary>Hint</summary>
`doctest::Approx(x).epsilon(e)` compares `|a-b| <= e * (scale + max(|a|,|b|))`; with `x == 0` you *need* `scale`. For `float`, `n * eps * ‖a‖ * ‖b‖` with `eps = 1.19e-7` gives a tolerance that will not flake; with `1e-6` hard-coded, `n = 10'000` flakes.
</details>

---

**19.6 — Property-based tests for linear algebra, hand-rolled with shrinking**
Write `for_all(trials, seed, gen, prop)` where `gen(rng, size) → Matrix` and `prop(A, B, …) → std::optional<std::string>` (the failure message). On failure, *shrink*: halve the matrix dimensions and retry until the property passes, then report the smallest failing size and the seed. Test at least six identities from the lesson's table with tolerances proportional to `n·ε·‖A‖‖B‖`: `(AB)ᵀ = BᵀAᵀ`, `A(BC) = (AB)C`, `(A+B)C = AC + BC`, `tr(AB) = tr(BA)`, `solve(A, Ax) = x` for diagonally dominant `A`, and `‖Qx‖ = ‖x‖` for a `Q` built by Gram–Schmidt. Then break your blocked matmul on purpose (drop the remainder tile when `n % tile != 0`) and show the property test finds it, shrinking to the smallest `n` that fails — paste the report.

Example: `PROP (AB)^T = B^T A^T  FAIL  seed=42  shape 33x17x9  rel_err=4.1e-01  (shrunk from 512x260x131)`.

<details><summary>Hint</summary>
Generate non-square shapes: `A` is `m×k`, `B` is `k×n` with `m, k, n ∈ [1, 64]` and occasionally 1 — the size-1 and prime cases find the most bugs. Use `std::mt19937_64` seeded from the command line so a failure is replayable.
</details>

---

**19.7 — Google Benchmark: three matmuls, ranges, and `compare.py`**
Write `ex19_7_bench.cpp` benchmarking `gemm_ijk`, `gemm_ikj`, `gemm_blocked` (tile as a second `Args` argument) with `RangeMultiplier(2)->Range(64, 1024)`, `Unit(kMillisecond)`, `SetItemsProcessed(2n³)` so the output shows FLOP/s, and `Complexity()`. Use `DoNotOptimize(C.data())` and `ClobberMemory()` correctly. Run with `--benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_out=before.json`, then change the blocked tile from 32 to 64, run again to `after.json`, and use `compare.py benchmarks before.json after.json` to produce the delta table. Paste the table and the `Complexity` fit (does `ijk` fit `O(N³)`? at what `n` does it stop fitting and why?). Then remove `ClobberMemory` and `DoNotOptimize` and show what happens to the timings at `-O3`.

<details><summary>Hint</summary>
`brew install google-benchmark` gives headers in `$(brew --prefix)/include` and `compare.py` in `$(brew --prefix google-benchmark)/share/benchmark/tools` (or clone the repo for `tools/`). `-O3 -mcpu=native -DNDEBUG` for the bench; link `-lbenchmark -lpthread`.
</details>

---

**19.8 — ML: profile-guided optimization of the P04 training loop**
Build your P04 NN framework (or the chapter 18 example's `Sequential`) with `-O2 -g -fno-omit-frame-pointer`, train a 784-256-10 MLP on random data for 20 steps, and profile it: `xcrun xctrace record --template 'Time Profiler' --launch -- ./ex19_8` (Linux: `perf record -g`). Export the inverted call tree (or `perf report --no-children`) and paste the top 10 symbols with percentages into a comment. Identify (a) any `malloc`/`free`/`memcpy` in the top 10 and which of your functions call them, (b) the hottest kernel and whether it is your gemm or something you didn't expect (a `Matrix` copy, `std::pow` in Adam, an `exp` in softmax). Fix the top item only, re-profile, and report the before/after step time from a Google Benchmark or `Timer`. Repeat once more. Write the three profiles' top-5 side by side in a table in the comment.

<details><summary>Hint</summary>
Typical first finding: `operator*` returns a fresh `Matrix` every call → `malloc` at 15–30%; fix with `gemm_into` and preallocated activations. Second: `std::pow(b1, t)` per parameter in Adam → hoist it per step. Instruments: "Invert Call Tree" + "Hide System Libraries" off, so you see `malloc` under your frames.
</details>

---

**19.9 — Numerics: fuzz + sanitize + tidy your `.npy` reader, then a CI matrix (multi-file)**
Take (or write, see `../20_cpp_for_numerics_and_hpc` §12) an `.npy` reader `npy::load(std::istream&) → Array` that parses the magic, version, header dict (`descr`, `fortran_order`, `shape`) and the data. Create a mini project `ex19_9/` with `CMakeLists.txt`, `CMakePresets.json` (`dev`, `asan`, `tsan`, `release`), `include/npy.hpp`, `src/npy.cpp`, `tests/test_npy.cpp` (doctest; includes a golden `.npy` written by NumPy in `tests/golden/`), `fuzz/fuzz_npy.cpp` (option `NPY_BUILD_FUZZERS`), `.clang-tidy`, `.clang-format`, and `.github/workflows/ci.yml` with macOS + Ubuntu build/test jobs, an ASan/UBSan job, a tidy/format job, and an `llvm-cov` coverage job that fails under 90% line coverage of `src/`. You cannot run GitHub Actions locally; instead run each job's commands by hand in the order the YAML gives and paste outputs in `ex19_9/RESULTS.md`. Deliverable: the project builds with `cmake --workflow --preset check`, the fuzzer runs 60 s clean, coverage report ≥ 90%.

<details><summary>Hint</summary>
Coverage: `-fprofile-instr-generate -fcoverage-mapping` on the `dev` preset via a `NPY_COVERAGE` option; `LLVM_PROFILE_FILE=%p.profraw ctest`; `xcrun llvm-profdata merge -sparse *.profraw -o m.profdata; xcrun llvm-cov report ./tests -instr-profile=m.profdata`. The header parser has many error branches — that's where coverage will be missing; the fuzz corpus's crash-free rejected inputs make good regression tests to cover them.
</details>

---

**19.10 — Sim: benchmark, profile and regression-test an N-body step (multi-file)**
Take your P07 (or C P15) N-body force kernel: `void accelerations(const Bodies& b, std::vector<Vec3>& acc)` in AoS and SoA versions, plus a Barnes–Hut version if you have it. Build a `bench/` target with Google Benchmark over `N ∈ {256, 1024, 4096, 16384}` for each variant with `Complexity()` (direct should fit `O(N²)`, Barnes–Hut `O(N log N)`), `SetItemsProcessed(N²)` interactions/s, `Repetitions(5)`. Add a *performance regression test* to `ctest`: a doctest case (label `perf`) that runs the SoA kernel at `N = 4096` and `CHECK`s the time is under a threshold stored in `bench/baseline_<hostname>.txt` × 1.25, skipping (not failing) if the file for this hostname doesn't exist. Add a *correctness* property test: total momentum `Σ m·v` is conserved to `1e-12·Σ|m·v|` after 100 leapfrog steps, and energy drift `|E(t)−E(0)|/|E(0)| < 1e-6` for `dt` small enough (state your `dt`). Profile the direct kernel with `xctrace` and paste the per-line hot spots (the `1/sqrt` and the `x*x+y*y+z*z` line should dominate); state whether it is compute- or memory-bound and why (arithmetic intensity: ~20 flops per 24 bytes loaded in SoA — see `../20_cpp_for_numerics_and_hpc` §8).

<details><summary>Hint</summary>
For the perf test, use `std::chrono::steady_clock`, take the minimum of 3 runs, and read the baseline file relative to the source dir via a `-DBASELINE_DIR="${CMAKE_SOURCE_DIR}/bench"` compile definition. `doctest::skip(!file_exists)` as a decorator. Momentum conservation is exact up to rounding for a symmetric pairwise force *only if* you compute each pair once and apply Newton's third law — an asymmetric implementation is the bug the test is there to catch.
</details>
