# Chapter 19 — Professional Tooling and Quality

## What you'll be able to do after this chapter

- Turn the compiler into a reviewer: build with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Werror`, and fix each class of warning the right way instead of casting it into silence.
- Run ASan/UBSan/TSan builds correctly on macOS and Linux, read a sanitizer report down to the line, and know which sanitizer is unavailable where.
- Fuzz a parser with libFuzzer, run `clang-tidy`/`cppcheck`/`scan-build` with a real `.clang-tidy`, and enforce `.clang-format` in a pre-commit hook.
- Write tests with doctest (fixtures, parameterized, `Approx`), hand-roll property-based tests for linear-algebra identities, and benchmark with Google Benchmark without the optimizer deleting your kernel.
- Profile with Instruments/`perf`/flame graphs, debug optimized builds and core dumps in lldb, and set up CMake presets, `ccache`, Ninja, `compile_commands.json`, a GitHub Actions matrix with sanitizer/tidy/coverage jobs, Doxygen docs, semver releases and a "definition of done" you actually follow.

## Why this matters for ML / numerics / sims

A numerics bug rarely crashes. It produces a loss curve that plateaus 3% higher than it should, an N-body simulation whose energy drifts by 1e-4 per orbit instead of 1e-9, an FDTD field with a faint checkerboard nobody notices for a week. The tools in this chapter exist because humans do not find those bugs by reading: `-Wconversion` catches the `int` that truncated your `double` index scale; UBSan catches the signed overflow in the hash of your BPE tokenizer; a property test finds that `(A·B)ᵀ ≠ Bᵀ·Aᵀ` for your blocked matmul when `n` isn't a multiple of the tile; Google Benchmark tells you the "optimization" made things 12% slower; `llvm-cov` shows the Adam bias-correction branch was never executed by any test. Professionals are not better at avoiding bugs; they have wired the machine to catch them before a human looks. Every tool below has an exact command for macOS arm64 and the Linux equivalent.

Prerequisites: `../11_headers_build_cmake_testing` (CMake, doctest basics, compile flags), `../12_performance` (timing, `DoNotOptimize`), `../10_error_handling`, `../18_software_design_and_library_architecture` (testability seams, layering). Everything here applies to your P01–P12 projects immediately.

---

## 1. Compiler warnings as a tool

Warnings are a free static analyzer that already understands your code. The professional baseline (GCC and clang; MSVC equivalents are `/W4 /permissive- /WX`):

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion \
    -Wformat=2 -Wimplicit-fallthrough -Wcast-align -Wunused -Werror -o prog prog.cpp
```

`-Werror` makes warnings fatal — in CI, always; locally, once the codebase is clean. Turn it on *from day one* of a project; retrofitting `-Wconversion` onto 50k lines is weeks of work. In CMake put them in an `INTERFACE` target (`cmake/CompilerWarnings.cmake` → `target_link_libraries(num_core PRIVATE num::warnings)`) so third-party code you compile isn't held to your standard.

What each flag catches and the *correct* fix (not the cast that silences it):

| Flag | Catches | Fix |
|---|---|---|
| `-Wall -Wextra` | Unused variables/parameters, `==` in a condition that should be `=`, missing `return`, comparison of unsigned with `< 0`, misleading indentation, missing field initializers | Remove the variable; `[[maybe_unused]]` or unnamed parameter `void f(int /*unused*/)`; add the return |
| `-Wpedantic` | Non-standard extensions: VLAs in C++, `__int128` without guards, zero-size arrays, trailing commas in odd places, `#pragma once`-less includes are fine | Replace VLA with `std::vector`/`std::array`; guard extensions with `#ifdef __SIZEOF_INT128__` |
| `-Wshadow` | A local `n` hiding a member `n`, a lambda parameter hiding an outer variable, a loop `i` inside a loop `i` | Rename (members `n_`, parameters `n`); this catches real bugs where the wrong `n` was used |
| `-Wconversion` | Implicit `double→float`, `double→int`, `int64→int32`, `size_t→int`, `int→char` that may lose value | Use the wider type; if the narrowing is intended write `static_cast<int>(x)` *and* ask why — most `size_t→int` conversions are `int` loop indices that should be `std::size_t` |
| `-Wsign-conversion` | `int i` used to index `v[i]` (`int→size_t`), `unsigned - signed` | Use `std::size_t`/`std::ptrdiff_t` for indices; `static_cast` at the one boundary where a signed API (BLAS, MPI, `printf`) meets your unsigned sizes |
| `-Wold-style-cast` | `(double)x`, `(int*)p` | `static_cast<double>(x)`; a `reinterpret_cast` stands out in review, which is the point |
| `-Wnon-virtual-dtor` | Class with virtual functions but non-virtual destructor → `delete base_ptr` is UB ([expr.delete]/3) | `virtual ~Base() = default;` or make the class `final` with no polymorphic deletion |
| `-Woverloaded-virtual` | Derived `f(int)` hides base `virtual f(double)` | `using Base::f;` or fix the signature |
| `-Wnull-dereference` | Paths where a pointer is provably null when dereferenced | Fix the logic; often reveals a missing early return |
| `-Wdouble-promotion` | `float` arithmetic silently done in `double` (`float x; x * 2.0`) — kills `float` SIMD throughput | Use `2.0f` literals; `std::sqrt` on float is fine (overloaded), `sqrt` from `<math.h>` isn't |
| `-Wimplicit-fallthrough` | `case` falling into the next without a comment | `[[fallthrough]];` (C++17) |
| `-Wformat=2` | `printf("%d", size_t)`, non-literal format strings | `%zu`, `%td`, `%.17g`; or `std::format` (C++20) |
| `-Wcast-align` | `reinterpret_cast<double*>(char_ptr)` where alignment may break | `memcpy`, or `alignas`, or `std::assume_aligned` (C++20) |
| `-Wlifetime` (clang, experimental) / `-Wdangling` | Returning a reference to a local, `string_view` of a temporary | Return by value |

Two clang-specific extras worth knowing: `-Weverything` turns on *all* warnings including contradictory ones (use once to discover flags, never in a build), and `-Wno-<name>` disables one — every `-Wno-` in your build file needs a comment justifying it. `-fdiagnostics-show-option` (default on clang) prints the flag name next to each warning so you know what to look up. GCC's `-Wuseless-cast`, `-Wlogical-op`, `-Wduplicated-cond` have no clang equivalents; run both compilers in CI to get both sets.

The dishonest fix and the honest fix:

```cpp
// Warning: implicit conversion loses integer precision: 'size_t' to 'int' [-Wshorten-64-to-32]
int n = v.size();                     // dishonest: int n = (int)v.size();
std::size_t n = v.size();             // honest: the index type is size_t
// If a signed API needs it (cblas takes int):
if (v.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::length_error("too big for BLAS");
const int n_blas = static_cast<int>(v.size());   // one cast, at the boundary, guarded
```

---

## 2. Sanitizers in depth

Sanitizers instrument the code at compile time; they find bugs the moment they happen, with a stack trace, instead of 10,000 iterations later as a wrong number. Cost: ASan ≈ 2× slowdown, 2–3× memory; UBSan ≈ 1.2×; TSan ≈ 5–15×, 5–10× memory; MSan ≈ 3×. Run tests under them in CI, not benchmarks.

### 2.1 Availability

| Sanitizer | Flag | macOS arm64 (Apple clang) | Linux (clang/gcc) | Finds |
|---|---|---|---|---|
| AddressSanitizer | `-fsanitize=address` | Yes | Yes | Heap/stack/global out-of-bounds, use-after-free, use-after-return (`ASAN_OPTIONS=detect_stack_use_after_return=1`), use-after-scope, double free, alloc/dealloc mismatch |
| LeakSanitizer | `-fsanitize=leak` (or via ASan) | **No** on Apple Silicon (`detect_leaks` unsupported). Use `leaks --atExit -- ./prog` or `MallocStackLogging=1 leaks ...` | Yes (on by default in ASan on x86-64/aarch64) | Memory leaks at exit |
| UndefinedBehaviorSanitizer | `-fsanitize=undefined` | Yes | Yes | Signed overflow, shift too large, misaligned access, null deref, `bool` not 0/1, enum out of range, `vptr` type confusion (needs RTTI), float→int overflow (`-fsanitize=float-cast-overflow`), division by zero, VLA bound ≤ 0, unreachable reached |
| ThreadSanitizer | `-fsanitize=thread` | Yes | Yes | Data races, lock-order inversions (deadlock potential), use of a `std::mutex` after destruction. Cannot combine with ASan. |
| MemorySanitizer | `-fsanitize=memory` | **No** (Linux-only, clang-only) | Yes, but every library including libc++ must be MSan-instrumented (`-stdlib=libc++` built with MSan) or you get false positives | Reads of *uninitialized* memory (ASan does not catch these) |
| Integer sanitizer | `-fsanitize=integer` (clang) | Yes | Yes | Also *unsigned* overflow and implicit truncations — not UB, but often bugs; noisy in hash functions (`-fno-sanitize=unsigned-integer-overflow` on those files or `__attribute__((no_sanitize("unsigned-integer-overflow")))`) |
| Valgrind memcheck | (no flag) | No on Apple Silicon | Yes; ~20–50× slower; finds uninitialized reads without recompiling | |

Usual combinations: `-fsanitize=address,undefined` for the everyday debug build; `-fsanitize=thread` as a separate build for anything with threads/OpenMP; `-fsanitize=memory` in a Linux-only CI job if you ship to Linux.

### 2.2 Flags and options that make the reports usable

```sh
c++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls \
    -fsanitize=address,undefined -fno-sanitize-recover=all -o tests tests.cpp
ASAN_OPTIONS=detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1:abort_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ./tests
```

- `-O1 -g`: `-O0` misses bugs the optimizer would expose (and is 5× slower); `-O2` inlines away some frames. `-O1` is the documented sweet spot. `-fno-omit-frame-pointer` gives reliable stack traces.
- `-fno-sanitize-recover=all`: UBSan by default *prints and continues*; this makes the first UB abort, which is what you want in tests.
- `ASAN_OPTIONS` is a colon-separated list; `ASAN_OPTIONS=help=1 ./tests` prints all of them. Useful: `halt_on_error=0` (report all errors, don't stop — for triage), `malloc_context_size=30` (deeper allocation stacks), `detect_leaks=1` (Linux), `quarantine_size_mb=256` (catch use-after-free later after the free), `allocator_may_return_null=1` (test OOM handling), `log_path=asan.log` (per-process files for CI), `symbolize=1`, `fast_unwind_on_malloc=0` (accurate allocation stacks through frames without frame pointers, slower), `suppressions=asan.supp` (third-party noise).
- `UBSAN_OPTIONS=print_stacktrace=1` — without it UBSan prints only the location, no stack.
- `TSAN_OPTIONS=second_deadlock_stack=1:history_size=7` and always `-O1 -g`; TSan needs `-fsanitize=thread` on *every* TU including the library (`libomp` is not instrumented → OpenMP under TSan gives false positives unless you build `libomp` with TSan or use its `ARCHER` tool; simpler: test the serial path under TSan with your own `std::thread` code).

### 2.3 Symbolization

A report with raw addresses is useless. Clang's runtime finds `llvm-symbolizer` on `PATH`; on macOS Apple's toolchain uses `atos` internally and symbolization works out of the box when you compiled with `-g` and the binary hasn't been stripped. If you see `#0 0x1045f8a10 (./tests+0x100004a10)` with no names:

```sh
# macOS
ASAN_SYMBOLIZER_PATH=$(xcrun -f llvm-symbolizer) ./tests
# or post-process a saved log:
xcrun atos -o ./tests -arch arm64 -l 0x1045f4000 0x1045f8a10
# Linux
ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer-18) ./tests        # version suffix matches your clang
# Also: keep the dSYM (macOS) — a sanitizer build produces ./tests.dSYM; don't delete it before reading the report
```

### 2.4 Reading a report

```
==41213==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6020000000d8 at pc 0x000104c3fa18 bp 0x00016f5ee6a0 sp 0x00016f5ee698
READ of size 8 at 0x6020000000d8 thread T0
    #0 0x104c3fa14 in gemm_naive(double const*, double const*, double*, unsigned long) gemm.cpp:17:38
    #1 0x104c3f8dc in main test_gemm.cpp:41:5
0x6020000000d8 is located 0 bytes after 8-byte region [0x6020000000d0,0x6020000000d8)
allocated by thread T0 here:
    #0 0x104f2b1c0 in operator new[](unsigned long)
    #1 0x104c3f7f8 in main test_gemm.cpp:38:19
SUMMARY: AddressSanitizer: heap-buffer-overflow gemm.cpp:17:38 in gemm_naive
Shadow bytes around the buggy address: ... fa fa 00 [fa] fa fa ...
```

Read in this order: (1) the **kind** — `heap-buffer-overflow`, `stack-use-after-scope`, `heap-use-after-free`, `SEGV on unknown address` (null or wild pointer), `attempting free on address which was not malloc()-ed`, `alloc-dealloc-mismatch` (`new[]`/`delete`), `stack-buffer-overflow`, `global-buffer-overflow`, `container-overflow` (writes past `vector::size()` but inside `capacity()` — needs libc++ annotations); (2) **READ/WRITE of size N** at frame #0 — the line where it happened, `17:38` is line 17 column 38; (3) **where the memory came from** — "0 bytes after 8-byte region" means you read one past the end of an allocation of 1 double; "located in stack of thread T0 at offset 32 in frame `main`" for stack bugs, with the variable name; (4) the *freed by* stack for use-after-free. The shadow-byte legend at the bottom (`fa` = heap left redzone, `fd` = freed, `f8` = stack after scope, `00` = addressable) is rarely needed.

UBSan lines look like `gemm.cpp:23:15: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'` — the fix is the type, not a cast. TSan reports show two stacks ("Write of size 8 by thread T2" / "Previous read of size 8 by thread T1") plus where each thread was created and which mutexes were held — the fix is a `reduction`, an `atomic`, or a lock, exactly as in `../12_performance` §12.

### 2.5 CI matrix

Run tests four ways: `Release -O2` (the thing you ship), `Debug -O0 -g` with `_GLIBCXX_ASSERTIONS`/libc++ hardening (`-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG` on libc++ ≥ 18; `-D_LIBCPP_ENABLE_ASSERTIONS=1` on older; catches `v[i]` out-of-range and `*end()`), `ASan+UBSan -O1 -g`, `TSan -O1 -g`. §14 has the YAML.

---

## 3. Fuzzing

A fuzzer feeds a function random-then-mutated inputs, guided by code coverage, millions of times a minute, and saves any input that crashes or trips a sanitizer. It is *the* tool for anything that parses bytes: your `.npy` reader, CSV/JSON loaders, the BPE tokenizer, a checkpoint format, the plugin API version check.

### 3.1 A libFuzzer target

```cpp
// fuzz_npy.cpp — a fuzz target is a function, not a program; libFuzzer provides main().
#include <cstddef>
#include <cstdint>
#include <string>
#include "npy.hpp"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string bytes(reinterpret_cast<const char*>(data), size);
    try { auto arr = npy::parse(bytes); (void)arr; }        // must not crash, hang, leak or trip a sanitizer
    catch (const npy::ParseError&) {}                          // rejecting bad input is correct behavior
    return 0;                                                  // always 0 (non-zero is reserved)
}
```

Rules: deterministic (no time/random inside), fast (< 1 ms per call — the fuzzer wants ≥ 1,000 exec/s), no global state between calls, every *expected* error path caught (an exception escaping = crash = "bug" in the fuzzer's eyes, so catch the ones your API documents).

### 3.2 Building and running

Apple clang **does not ship libFuzzer** (`-fsanitize=fuzzer` fails with "unsupported option"). Install LLVM from Homebrew and use its clang:

```sh
brew install llvm                       # Linux: apt install clang libfuzzer-<ver>-dev (or llvm from apt.llvm.org)
export LLVM=$(brew --prefix llvm)
$LLVM/bin/clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
    -o fuzz_npy fuzz_npy.cpp npy.cpp
mkdir -p corpus && cp tests/golden/*.npy corpus/            # seed corpus: real valid files make it 100× more effective
./fuzz_npy corpus -max_len=4096 -jobs=8 -workers=8 -max_total_time=600 -dict=npy.dict
# crash found -> ./fuzz_npy crash-<sha1>          reproduces it deterministically
./fuzz_npy -minimize_crash=1 -runs=100000 crash-<sha1>       # shrink the input
./fuzz_npy -merge=1 corpus_min corpus                         # deduplicate the corpus by coverage; commit corpus_min
```

Flags: `-max_len` (input size cap), `-jobs`/`-workers` (parallel), `-max_total_time` (seconds; use in CI, e.g. 60), `-runs=N` (exact iteration count, for regression: replay the corpus once with `-runs=0`), `-dict=file` (tokens like `"\x93NUMPY"`, `"'descr'"` that help the mutator get past magic checks), `-timeout=10` (hangs are bugs too), `-rss_limit_mb=2048` (memory blow-ups are bugs), `-use_value_profile=1` (better at comparisons). Output line `#12345 NEW cov: 312 ft: 890 corp: 41/2Kb exec/s: 9800` — `cov` is edges covered; when it stops growing for minutes, either the parser is thoroughly explored or the fuzzer is stuck behind a checksum (then fuzz *below* the checksum with a `#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION` bypass — that macro is defined by libFuzzer builds by convention).

Alternatives: **AFL++** (`brew install afl++`; process-based, works with any compiler), **Honggfuzz**, **OSS-Fuzz** (Google's free continuous fuzzing for open-source projects — if your library goes public, apply). Structure-aware fuzzing with `libprotobuf-mutator` when inputs have grammar. In CMake: an option `NUM_BUILD_FUZZERS` defaulting OFF, and a CI job on Ubuntu with clang that runs each target for 60 s and replays the committed corpus.

---

## 4. Static analysis

### 4.1 `clang-tidy`

```sh
brew install llvm                                   # Linux: apt install clang-tidy
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
$(brew --prefix llvm)/bin/clang-tidy -p build src/tensor/tensor.cpp            # one file
$(brew --prefix llvm)/bin/run-clang-tidy -p build -j 8 'src/.*'                # whole tree, parallel
$(brew --prefix llvm)/bin/run-clang-tidy -p build -fix -checks='modernize-use-nullptr'   # apply one auto-fix
```

A `.clang-tidy` at the repo root; checks are glob patterns, `-` excludes:

```yaml
# .clang-tidy
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  cert-*,
  -cert-err58-cpp,
  clang-analyzer-*,
  concurrency-*,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic,
  -cppcoreguidelines-pro-bounds-constant-array-index,
  -cppcoreguidelines-avoid-c-arrays,
  -cppcoreguidelines-non-private-member-variables-in-classes,
  misc-*,
  -misc-non-private-member-variables-in-classes,
  modernize-*,
  -modernize-use-trailing-return-type,
  -modernize-avoid-c-arrays,
  performance-*,
  portability-*,
  readability-*,
  -readability-magic-numbers,
  -readability-identifier-length,
  -readability-function-cognitive-complexity
WarningsAsErrors: 'bugprone-*,clang-analyzer-*,performance-*'
HeaderFilterRegex: '^(include|src)/.*'         # don't lint third_party/ or system headers
FormatStyle: file
CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: _
  - key: performance-unnecessary-value-param.AllowedTypes
    value: 'span;string_view;MatrixView;ConstMatrixView'
  - key: cppcoreguidelines-narrowing-conversions.WarnOnFloatingPointNarrowingConversion
    value: 'true'
```

Which checks matter *for numerics*:

| Check | Why for numerics |
|---|---|
| `bugprone-narrowing-conversions`, `cppcoreguidelines-narrowing-conversions` | `double` → `float`/`int` silently; the `-Wconversion` of tidy with better messages |
| `bugprone-integer-division` | `1/2` in a `double` context = 0 |
| `bugprone-incorrect-roundings` | `(int)(x + 0.5)` is wrong for negatives; use `std::lround` |
| `bugprone-suspicious-semicolon`, `bugprone-misplaced-widening-cast` | `int64_t(a * b)` overflows in `int` first |
| `bugprone-use-after-move`, `bugprone-dangling-handle` | Moved-from `Tensor`, `string_view` of a temporary |
| `bugprone-unchecked-optional-access` | `*opt` without `has_value()` on your `solve()` |
| `performance-unnecessary-copy-initialization`, `performance-unnecessary-value-param`, `performance-for-range-copy` | The hidden `Matrix` copies of `../12_performance` §7, found automatically |
| `performance-inefficient-vector-operation`, `performance-noexcept-move-constructor` | Missing `reserve`; non-`noexcept` move makes `vector` copy on growth |
| `performance-move-const-arg`, `performance-no-automatic-move` | `std::move` on a `const` — silently copies |
| `modernize-loop-convert`, `modernize-use-auto`, `modernize-use-nullptr`, `modernize-pass-by-value` | Auto-fixable cleanups |
| `readability-container-size-empty`, `readability-implicit-bool-conversion` | `if (ptr)` vs `if (ptr != nullptr)`; `if (v.size())` |
| `misc-const-correctness`, `misc-include-cleaner` (tidy ≥ 17) | Adds `const`; removes unused includes (poor man's IWYU) |
| `cppcoreguidelines-init-variables`, `cppcoreguidelines-prefer-member-initializer` | Uninitialized doubles = MSan's job, caught statically |
| `concurrency-mt-unsafe` | `rand()`, `strtok`, `localtime` in threaded code |
| `clang-analyzer-*` | The path-sensitive Clang Static Analyzer (same engine as `scan-build`): null derefs, leaks, use-after-free along specific paths |

Suppress a single line with `// NOLINT(check-name)` or the next line with `// NOLINTNEXTLINE(check-name)`, always with the check name — a bare `NOLINT` is a code smell. For a whole header block: `// NOLINTBEGIN(...)`/`// NOLINTEND(...)`. Wire it into the editor: clangd runs clang-tidy live if `.clangd` has `Diagnostics: { ClangTidy: { Add: [...] } }` or just picks up `.clang-tidy`.

### 4.2 `cppcheck`

A different engine (no compiler front-end; tolerant of unbuildable code; good at style and some UB):

```sh
brew install cppcheck                                # Linux: apt install cppcheck
cppcheck --enable=warning,style,performance,portability --inconclusive --std=c++17 \
         --project=build/compile_commands.json -i third_party --suppress=missingIncludeSystem \
         --error-exitcode=1 --inline-suppr 2> cppcheck.txt
```

Finds different things than tidy: uninitialized members, `memset` on non-trivial types, array index out of bounds with constant indices, mismatched `new[]`/`delete`, `printf` format mismatches, redundant conditions. Fast enough to run on every commit.

### 4.3 `scan-build` (Clang Static Analyzer as a build wrapper)

```sh
$(brew --prefix llvm)/bin/scan-build -o scan-out --status-bugs -enable-checker optin.cplusplus.UninitializedObject \
    cmake --build build --clean-first            # wraps every compiler invocation; HTML report in scan-out/
$(brew --prefix llvm)/bin/scan-view scan-out/<date>  # opens the report with the path through the code highlighted
```

It is the `clang-analyzer-*` checks with interprocedural path exploration and an HTML report that shows *the sequence of branches* that leads to the bug — invaluable for "how could `p` be null here". Slow (5–10× a build); nightly, not per-commit. Other tools worth knowing: **Infer** (Facebook; `brew install infer`), **PVS-Studio** and **Coverity** (commercial; Coverity Scan is free for open source), **CodeQL** (GitHub; semantic queries over your codebase; free for public repos), **`-fanalyzer`** (GCC ≥ 10's built-in analyzer, Linux).

---

## 5. Formatting

Formatting arguments are the most expensive zero-value activity in software. `clang-format` ends them:

```yaml
# .clang-format
BasedOnStyle: Google
Language: Cpp
Standard: c++17
ColumnLimit: 100
IndentWidth: 4
AccessModifierOffset: -4
DerivePointerAlignment: false
PointerAlignment: Left                 # double* p, not double *p
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AlignConsecutiveAssignments: None
AlignTrailingComments: true
BreakBeforeBraces: Attach
IncludeBlocks: Regroup                 # sort & group includes: own header, project, third-party, std
IncludeCategories:
  - Regex: '^"num/'
    Priority: 1
  - Regex: '^<(Eigen|doctest|benchmark)/'
    Priority: 2
  - Regex: '^<.*\.h>'
    Priority: 3
  - Regex: '^<.*>'
    Priority: 4
SortIncludes: CaseSensitive
SpacesBeforeTrailingComments: 2
QualifierAlignment: Left               # const double, not double const (clang-format >= 14)
```

```sh
brew install clang-format                            # Linux: apt install clang-format
clang-format -i src/**/*.cpp include/**/*.hpp        # in place
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')   # CI: fail if anything would change
git clang-format                                     # format only the lines you changed (from brew llvm)
```

Protect hand-aligned matrices with `// clang-format off` … `// clang-format on`. Commit a one-time "reformat everything" commit and add its hash to `.git-blame-ignore-revs` (`git config blame.ignoreRevsFile .git-blame-ignore-revs`) so `git blame` skips it.

**pre-commit** (`brew install pre-commit`; Linux: `pip install pre-commit`) runs hooks before every commit:

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v18.1.8
    hooks: [{ id: clang-format, types_or: [c++, c] }]
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.6.0
    hooks: [{ id: trailing-whitespace }, { id: end-of-file-fixer }, { id: check-yaml }, { id: check-added-large-files, args: ['--maxkb=500'] }]
  - repo: https://github.com/cheshirekow/cmake-format-precommit
    rev: v0.6.13
    hooks: [{ id: cmake-format }]
```

`pre-commit install` once per clone; `pre-commit run --all-files` in CI. Also `.editorconfig` (tabs/spaces/EOL for every editor) and `.gitattributes` with `* text=auto eol=lf` so Windows contributors don't turn every diff red.

---

## 6. Testing frameworks — doctest

| | doctest | Catch2 v3 | GoogleTest |
|---|---|---|---|
| Distribution | Single header (`doctest.h`), ~6k lines | CMake library (v3 is no longer single-header) | CMake library |
| Compile cost per test file | Lowest (designed for it; ~2× a plain TU) | ~3–5× | ~2× |
| Assertion style | `CHECK(a == b)` — expression decomposed, prints both values | Same | `EXPECT_EQ(a, b)` — macro per comparison |
| Subcases/sections | `SUBCASE` (re-runs the test for each leaf) | `SECTION` | Fixtures via classes only |
| Parameterized | `TEST_CASE_TEMPLATE` for types; data via `SUBCASE` loops or `doctest::Approx`-style helpers | `GENERATE`, `TEMPLATE_TEST_CASE` | `TEST_P` + `INSTANTIATE_TEST_SUITE_P`, `TYPED_TEST` |
| Float compare | `doctest::Approx(x).epsilon(rel).scale(abs)` | `Catch::Approx`, `WithinRel`, `WithinAbs`, `WithinULP` | `EXPECT_NEAR(a,b,abs)`, `EXPECT_DOUBLE_EQ` (4 ULP) |
| Death tests (assert the program aborts) | **No** (run a subprocess yourself) | No | `EXPECT_DEATH(stmt, regex)` — forks; slow but real |
| Mocking | No | No | GoogleMock built in |
| Tests inside production TUs | Yes (`DOCTEST_CONFIG_DISABLE` compiles them out) | No | No |

Pick **doctest** for numerics libraries: fastest compile, header-only, good enough assertions; GoogleTest when you need death tests or mocks; Catch2 if you like `GENERATE`. All three integrate with `ctest` through `doctest_discover_tests`/`catch_discover_tests`/`gtest_discover_tests`.

### 6.1 The features you'll use

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN            // exactly one TU; the rest just #include <doctest/doctest.h>
#include <doctest/doctest.h>
#include "num/matrix.hpp"

TEST_CASE("Matrix multiply: identity and shape errors") {
    num::Matrix I = num::Matrix::identity(3);
    num::Matrix A(3, 3, 2.0);
    CHECK(A * I == A);                                // CHECK continues on failure; REQUIRE aborts the test case
    CHECK_THROWS_AS(A * num::Matrix(2, 2), std::invalid_argument);
    CHECK_MESSAGE(A.rows() == 3, "rows was ", A.rows());
    SUBCASE("transpose twice") { CHECK(A.transpose().transpose() == A); }
    SUBCASE("trace")           { CHECK(trace(A) == doctest::Approx(6.0)); }   // each SUBCASE re-runs the setup above
}

// Float comparison: epsilon is RELATIVE (default 1.19e-7 float epsilon * 100), scale adds an absolute term.
TEST_CASE("solver residual") {
    auto x = solve(A, b);
    CHECK(residual(A, x, b) == doctest::Approx(0.0).epsilon(1e-12).scale(1.0));   // |a-b| <= eps*(scale+max(|a|,|b|)) -> absolute 1e-12 here
    for (std::size_t i = 0; i < x.size(); ++i) { INFO("i = ", i); CHECK(x[i] == doctest::Approx(expected[i]).epsilon(1e-9)); }
}

// Type-parameterized: the same test for float and double (and later __fp16 / your Fixed<16>)
TEST_CASE_TEMPLATE("dot is symmetric", T, float, double) {
    std::vector<T> a{1, 2, 3}, b{4, 5, 6};
    CHECK(dot(a, b) == dot(b, a));
}

// Fixture: a struct whose members every test in it can use; a fresh instance per test case.
struct SmallNet { nn::Layer net = nn::Sequential{nn::Linear(2, 4), nn::Tanh{}, nn::Linear(4, 1)}; num::Matrix x{8, 2, 0.5}; };
TEST_CASE_FIXTURE(SmallNet, "forward shape") { CHECK(net.forward(x).rows() == 8); }
TEST_CASE_FIXTURE(SmallNet, "gradient matches finite differences") { /* ... */ }

// Tags via decorators and test-suite grouping; run with ./tests -ts=slow, or -tc="*gemm*", or -s (success too)
TEST_SUITE("slow") { TEST_CASE("1000 epochs" * doctest::timeout(30) * doctest::skip(std::getenv("CI") != nullptr)) { /* ... */ } }
```

Useful runtime flags: `./tests -s` (show successful assertions), `-tc=<pattern>`, `-ts=<suite>`, `-sf=<file>`, `--order-by=rand --rand-seed=1234` (find order dependence), `-d` (durations), `--reporters=junit --out=report.xml` (for CI dashboards), `-nt` (no throws — turn `CHECK_THROWS` off when debugging with a debugger). With `ctest`: `include(doctest)` + `doctest_discover_tests(tests ADD_LABELS 1)` registers each `TEST_CASE` as a separate ctest test; `ctest -j8 --output-on-failure -L unit`.

A hand-rolled `CHECK`/`TEST` macro pair takes 40 lines and is worth writing once to understand what these frameworks do (the example does that); then use doctest.

### 6.2 Death tests without a framework

To assert that `Matrix(0, 0)(1, 1)` aborts in a debug build, fork a child (POSIX) and check its exit status:

```cpp
#include <sys/wait.h>
#include <unistd.h>
template <class F> bool dies(F&& f) {                 // true if f() terminates the process by signal (SIGABRT from assert)
    pid_t pid = fork();
    if (pid == 0) { std::signal(SIGABRT, SIG_DFL); f(); _exit(0); }   // child: run f; if it returns, exit 0
    int status = 0; waitpid(pid, &status, 0);
    return WIFSIGNALED(status);
}
CHECK(dies([] { num::Matrix m(2, 2); (void)m.at(5, 5); }));
```

Sandboxes and some CI runners block `fork`; guard with a `NUM_ENABLE_DEATH_TESTS` option.

---

## 7. Property-based testing

Example-based tests check `f(3) == 9`. Property tests check `∀x: f(x) == x*x` for hundreds of generated `x`, shrinking a failing input to the smallest one that still fails. **RapidCheck** (`rc::check("reverse twice is identity", [](const std::vector<int>& v){ auto w = v; std::reverse(w.begin(), w.end()); std::reverse(w.begin(), w.end()); RC_ASSERT(w == v); });`) integrates with doctest/GoogleTest and has `rc::gen::inRange`, `rc::gen::container`, arbitrary `std` types, and automatic shrinking. Hypothesis (Python) is the same idea, if you've met it.

For linear algebra the properties are *identities*, and a hand-rolled version is 30 lines: a seeded `std::mt19937_64`, a generator of random matrices of random small shapes, a loop, and a report that prints the seed and shape of the first failure so it can be replayed. The identities worth testing (with a relative tolerance ~ `n · ε · ‖A‖‖B‖`, not `1e-15`):

| Property | Catches |
|---|---|
| `(A·B)ᵀ == Bᵀ·Aᵀ` | Loop-order / indexing bugs in gemm, especially non-square and `n` not multiple of the tile |
| `A·(B·C) == (A·B)·C` | Same, plus accumulation-order sensitivity (use a tolerance) |
| `A·I == A`, `I·A == A`, `A·0 == 0` | Off-by-one in bounds |
| `(A + B)·C == A·C + B·C` | Fused kernels |
| `A·A⁻¹ == I` (well-conditioned `A`) ; `solve(A, A·x) == x` | Solvers; check `‖r‖/‖b‖`, not `x` directly |
| `tr(A·B) == tr(B·A)`; `det(A·B) == det(A)det(B)` | Reductions |
| `‖Q·x‖ == ‖x‖` for orthogonal `Q` from your QR; `QᵀQ == I` | QR/Householder |
| `Aᵀ·A` is symmetric positive semidefinite (Cholesky succeeds) | Symmetry handling |
| `softmax(x + c) == softmax(x)` ; `Σ softmax(x) == 1` ; `logsumexp(x) == log Σ exp(x)` with big `x` | Numerical stability of your ML kernels |
| `∂f/∂x` by autograd == central finite difference `(f(x+h) - f(x-h)) / 2h`, `h = 1e-5`, rel tol 1e-4 | Every autograd op (P03) |
| `decode(encode(s)) == s` for BPE; `encode` is deterministic | Tokenizer |
| Energy/momentum conserved to `O(dt²)` over 100 steps of a symplectic integrator; time-reversal returns to the start | N-body (P07), PIC (P09) |
| Grid sum conserved for a conservative stencil; solution invariant under grid translation/reflection | FDTD (P08), LBM (P10) |
| `fft(ifft(x)) == x`; Parseval `Σ|x|² == (1/N) Σ|X|²`; `fft(δ) == 1` | FFT (P06) |
| `sort` output is a permutation of the input and non-decreasing; `x ∈ set` after `insert(x)` | Competitive-programming data structures |

Structure of a hand-rolled property test (the example implements it): `for_all(n_trials, seed, generator, predicate)` returns the first counterexample or `nullopt`; shrinking is optional but cheap for matrices — retry with `n/2` rows until the property passes, and report the smallest failing size. Always print the seed.

---

## 8. Benchmarking — Google Benchmark

`../12_performance` §1–2 built a `Timer` and `bench_ms`. Google Benchmark does the same with statistics, argument sweeps, and a stable CLI:

```sh
brew install google-benchmark        # Linux: apt install libbenchmark-dev, or FetchContent
c++ -std=c++17 -O3 -mcpu=native -DNDEBUG -o bench bench.cpp -lbenchmark -lpthread  # macOS: add -I/opt/homebrew/include -L/opt/homebrew/lib
```

```cpp
#include <benchmark/benchmark.h>
#include "num/gemm.hpp"

static void BM_gemm_naive(benchmark::State& state) {
    const auto n = static_cast<std::size_t>(state.range(0));
    std::vector<double> A(n * n, 1.0), B(n * n, 1.0), C(n * n);   // setup OUTSIDE the loop is not timed
    for (auto _ : state) {                                          // the timed loop; benchmark decides how many iterations
        gemm_naive(A.data(), B.data(), C.data(), n);
        benchmark::DoNotOptimize(C.data());                         // "someone reads C": the call can't be deleted
        benchmark::ClobberMemory();                                 // "all memory may have been written": no store sinking
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(2 * n * n * n));   // reported as items/s = FLOP/s
    state.counters["GFLOP/s"] = benchmark::Counter(static_cast<double>(2 * n * n * n), benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::kIs1000) ;
}
BENCHMARK(BM_gemm_naive)->RangeMultiplier(2)->Range(64, 1024)->Unit(benchmark::kMillisecond)->Complexity(benchmark::oN);
BENCHMARK(BM_gemm_blocked)->Args({256, 32})->Args({256, 64})->Args({512, 64});    // two args: n, tile
BENCHMARK(BM_gemm_blas)->Arg(512)->MinTime(2.0)->Repetitions(5)->ReportAggregatesOnly();
BENCHMARK_TEMPLATE(BM_dot, float)->Arg(1 << 20);
BENCHMARK_TEMPLATE(BM_dot, double)->Arg(1 << 20);
BENCHMARK_MAIN();
```

- **`DoNotOptimize(x)`** forces `x` to be materialized (an `asm volatile("" : "+r,m"(x) :: "memory")` in recent versions): the compiler must compute it. Pass the *result* or a pointer to it. **`ClobberMemory()`** is `asm volatile("" ::: "memory")`: a compiler barrier that forces pending stores to happen before it, so a loop that writes to `C` can't have its stores hoisted/sunk out of the timed region. Together they're the correct version of `../12_performance`'s `volatile` trick.
- `state.range(0)`, `Arg`, `Args`, `Range(lo, hi)` (powers of `RangeMultiplier`, default 8), `DenseRange(lo, hi, step)`, `ArgsProduct({{64,128},{16,32}})` (cartesian), `Apply(custom_fn)`.
- `state.PauseTiming()/ResumeTiming()` for per-iteration setup (expensive; prefer setup outside or `SetUp`/`TearDown` on a `benchmark::Fixture`).
- `SetBytesProcessed`/`SetItemsProcessed` → `bytes_per_second`/`items_per_second` in the output; `Complexity()` fits `O(n)`, `O(n²)`, `O(n log n)` to the measured times across `Range` and prints the RMS error.
- `--benchmark_filter=gemm --benchmark_repetitions=10 --benchmark_report_aggregates_only=true --benchmark_min_time=1s --benchmark_out=res.json --benchmark_out_format=json --benchmark_counters_tabular=true`. `tools/compare.py benchmarks before.json after.json` prints per-benchmark deltas with a Mann-Whitney U-test p-value — the professional way to claim "12% faster".
- On macOS the "CPU scaling enabled" warning does not apply (no `cpufreq`), but do close browsers; on Linux run `sudo cpupower frequency-set -g performance` and pin with `taskset -c 2`. Both: `Repetitions(≥5)` and look at the *median* and *cv* (coefficient of variation; > 5% means noise, rerun).

**nanobench** (`ankerl::nanobench`, single header) is the lightweight alternative: `ankerl::nanobench::Bench().minEpochIterations(100).run("gemm 256", [&]{ gemm(...); ankerl::nanobench::doNotOptimizeAway(C); });` — prints ns/op, cycles/op (Linux perf counters), instructions, branch misses, and renders comparison tables. Use it inside tests or notebooks; use Google Benchmark for the `bench/` directory that CI tracks.

---

## 9. Profiling

### 9.1 macOS: Instruments

```sh
c++ -std=c++17 -O2 -g -fno-omit-frame-pointer -o bench bench.cpp     # RelWithDebInfo: names + lines, real speed
xcrun xctrace record --template 'Time Profiler' --output prof.trace --launch -- ./bench --benchmark_filter=gemm
open prof.trace                                                        # Instruments GUI: Call Tree -> Invert, Hide System Libraries, Heaviest Stack Trace
xcrun xctrace record --template 'System Trace' --output sys.trace --launch -- ./bench   # thread scheduling, syscalls, VM faults, lock waits
xcrun xctrace record --template 'Allocations' --output alloc.trace --launch -- ./train  # who allocates in the training loop
xcrun xctrace record --template 'CPU Counters' ...                    # cache misses, branch mispredicts (needs config in GUI first)
xcrun xctrace list templates                                           # everything available
xcrun xctrace export --input prof.trace --xpath '/trace-toc/run/data/table[@schema="time-profile"]' > prof.xml   # scripted export
sample ./bench 5 -file sample.txt                                      # 5 s text-mode sampling of a running/launched process; zero setup
```

Time Profiler samples every thread's stack at 1 kHz (configurable to 100 µs). In the GUI: "Invert Call Tree" shows leaf functions (where cycles are *spent*); un-inverted shows the path (who *called* them). "Charge system libraries to callers" attributes `memcpy`/`malloc` time to your code. Double-click a function to see per-line samples — that's where you find the one `Matrix` copy costing 40%. **System Trace** answers "why is my 8-thread OpenMP loop at 3× not 8×": you see threads waiting on a barrier, page faults from first-touch allocation, or the process being descheduled.

### 9.2 Linux: `perf` and flame graphs

```sh
sudo sysctl kernel.perf_event_paranoid=1          # allow user profiling; or -1 for kernel too
perf stat -e cycles,instructions,cache-misses,cache-references,branch-misses,L1-dcache-load-misses ./bench   # hardware counters
perf record -g --call-graph dwarf -F 999 ./bench   # -g: call graphs; dwarf unwinding works without frame pointers (large files); -F Hz
perf record -g -e cache-misses ./bench             # sample on a specific event: WHERE do misses happen
perf report --no-children --sort=dso,symbol        # TUI; 'a' annotates assembly with per-instruction samples
perf annotate gemm_naive                           # per-instruction hotness: find the load that misses
perf top                                           # live, system-wide
# Flame graphs (Brendan Gregg's scripts: git clone https://github.com/brendangregg/FlameGraph)
perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > flame.svg   # open in a browser; width = time
```

A **flame graph** stacks call paths vertically (root at the bottom) with width proportional to samples; plateaus at the top are where time is spent, and you can see at a glance that `gemm_naive` under `Linear::forward` under `Sequential::forward` under `train_step` is 71% of the run. On macOS you get the same picture from Instruments' flame graph view (Time Profiler → the "Flame Graph" tab, Xcode ≥ 15) or by exporting `sample` output through `stackcollapse-sample.awk`. **`-fno-omit-frame-pointer`** costs ~1% and makes frame-pointer unwinding (`perf record -g` without `dwarf`, Instruments' default) reliable; arm64 macOS keeps frame pointers by default at `-O2`, Linux x86-64 does not.

Other profilers: **`valgrind --tool=callgrind`** (Linux; exact instruction counts per line, `kcachegrind` GUI; 50× slow, deterministic — perfect for comparing two versions without noise), **`heaptrack`** (Linux allocations), **Tracy** (frame-based instrumented profiler for sims; nanosecond zones you place with a macro; excellent for "why did step 1,203 take 40 ms"), **`gprof`** (obsolete), **Intel VTune / AMD uProf** (x86, microarchitecture-level).

### 9.3 What to look for

Top-down: (1) is the hot function the one you expected? (2) is it compute-bound (high IPC, few cache misses → SIMD/algorithm) or memory-bound (low IPC, `cache-misses` high → layout, tiling, `../20_cpp_for_numerics_and_hpc` roofline)? (3) is `malloc`/`free`/`memcpy` in the top 10 (→ allocations in the loop)? (4) is there `__psynch_mutexwait`/`futex`/`__ulock_wait` (→ lock contention or OpenMP barrier)? (5) in the annotate view, is time on a `ldr`/`mov` (a load that misses) or on `fmadd`/`vfmadd` (compute)?

---

## 10. Debugging with lldb

```sh
c++ -std=c++17 -O0 -g -fno-omit-frame-pointer -fstandalone-debug -o tests tests.cpp    # -fstandalone-debug: full std:: type info on macOS
lldb ./tests -- -tc="*gemm*"                     # program args after --
```

Inside lldb (gdb equivalents in parentheses; `help <cmd>` and `apropos <word>` work):

| Task | lldb |
|---|---|
| Break at line / function / all methods of a class | `b gemm.cpp:17` · `b gemm_naive` · `br set -r 'Matrix::.*'` (gdb: `rbreak`) |
| Conditional breakpoint | `br set -f gemm.cpp -l 17 -c 'i == 7 && j > 3'` · `br modify -c 'std::isnan(acc)' 1` |
| Hit count / ignore | `br modify -i 999 1` (skip 999 hits) · `br list` |
| Break on exception throw / on `assert` | `br set -E c++` (`catch throw`) · `b __assert_rtn` (macOS) / `b __assert_fail` (glibc) · `b abort` |
| Watchpoint (break when memory changes) | `watchpoint set variable C[5]` · `watchpoint set expression -w write -- (double*)ptr + 5` · `watch modify -c 'C[5] != C[5]'` (NaN!) |
| Run / continue / step | `r` · `c` · `n` (over) · `s` (into) · `finish` (out) · `thread step-inst` (one instruction) · `thread until 42` |
| Stack | `bt` · `bt 5` · `up` / `down` · `frame select 3` · `frame info` |
| Variables | `frame variable` (`fr v`) — all locals, formatted · `fr v -L C` (with addresses) · `p n` / `p A[i*n+k]` · `p/x flags` · `p $rax` |
| Expressions (call functions!) | `expr trace(A)` · `expr (void)dump(A, "A.txt")` · `expr -- x = 0.0` · `expr auto $tmp = A.rows()` (persistent vars) |
| Arrays through pointers | `parray 16 C` · `memory read -f f64 -c 16 C` · `x/16fg C` (gdb) |
| `std::vector`, `std::string`, `std::map`, `unique_ptr`, `optional`… | `fr v v` prints elements: lldb ships libc++ data formatters; `type summary list` shows them; `fr v v --ptr-depth 2` · `p v.__begin_[3]` if formatters fail |
| Custom pretty printer for your `Matrix` | `command script import matrix_lldb.py` with a `SyntheticChildrenProvider`; or quick: `type summary add num::Matrix --summary-string "${var.r_}x${var.c_}"` |
| Threads | `thread list` · `thread select 3` · `thread backtrace all` · `settings set target.process.stop-on-sharedlibrary-events 0` |
| Disassembly / registers | `disassemble --frame` · `di -n gemm_naive -m` (mixed with source) · `register read` |
| Reverse-step | Not in lldb. Linux: **`rr`** (`rr record ./tests; rr replay` → gdb with `reverse-continue`, `reverse-step`, `reverse-finish`; deterministic replay of a race or a heisenbug you caught *once*). No macOS support. Alternatives: UDB (commercial), or `watchpoint` + rerun with a fixed seed. |
| Scripting | `command alias bl breakpoint list` · `~/.lldbinit` · `script print(lldb.frame.FindVariable("n"))` (Python API) |

**Debugging optimized builds** (`-O2 -g`): variables show `<optimized out>` (eliminated or in a register only part of the time), stepping jumps around (instruction scheduling), inlined functions appear as if you were inside the caller. Tactics: `-Og` (clang: same as `-O1`, keeps debuggability better than `-O2`), `__attribute__((noinline))` on the function you're studying, `volatile` a variable temporarily, `-fno-inline-functions`, break on the *assembly* address and `register read`, or print from code (`fprintf(stderr, …)`, "printf debugging" is not a sin when the bug only appears at `-O2`). If a bug appears *only* at `-O2`, it is almost always UB (uninitialized read, strict aliasing, signed overflow) — run UBSan and ASan *at* `-O2` before opening the debugger.

**Core dumps.** macOS: `ulimit -c unlimited` and `sudo mkdir -p /cores && sudo chmod 1777 /cores`; the crashing process writes `/cores/core.<pid>` (multi-GB — it's the whole address space) *if* the binary is not hardened-runtime-restricted (ad-hoc-signed local builds are fine; if not, `codesign -s - -f --entitlements get-task-allow.plist ./prog`). Then `lldb ./prog -c /cores/core.1234` → `bt`. Also `~/Library/Logs/DiagnosticReports/*.ips` are the text crash reports macOS writes anyway — the backtrace is in there, symbolicate with `atos`. Linux: `ulimit -c unlimited`; `cat /proc/sys/kernel/core_pattern` (`core` in cwd, or `|/usr/lib/systemd/systemd-coredump` → `coredumpctl list`, `coredumpctl debug <pid>` opens gdb); `gdb ./prog core` or `lldb ./prog -c core`. Debug info for release builds: keep the `.dSYM` (macOS, `dsymutil prog`) or split DWARF (`-gsplit-dwarf`, Linux; `objcopy --only-keep-debug`) so you can symbolicate a customer's crash from a stripped binary — that's what release engineering (§16) archives.

---

## 11. Build systems at scale

### 11.1 CMake presets

`CMakePresets.json` (CMake ≥ 3.19; version 6 format needs ≥ 3.25) replaces the wiki page of `-D` flags:

```json
{
  "version": 6,
  "configurePresets": [
    { "name": "base", "hidden": true, "generator": "Ninja", "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": { "CMAKE_EXPORT_COMPILE_COMMANDS": "ON", "CMAKE_CXX_STANDARD": "17", "NUM_WARNINGS_AS_ERRORS": "ON" } },
    { "name": "dev",     "inherits": "base", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "NUM_HARDEN_STDLIB": "ON" } },
    { "name": "asan",    "inherits": "base", "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo", "NUM_SANITIZER": "address;undefined" } },
    { "name": "tsan",    "inherits": "base", "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo", "NUM_SANITIZER": "thread" } },
    { "name": "release", "inherits": "base", "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_INTERPROCEDURAL_OPTIMIZATION": "ON" } },
    { "name": "bench",   "inherits": "release", "cacheVariables": { "NUM_BUILD_BENCHMARKS": "ON", "NUM_NATIVE_ARCH": "ON" } },
    { "name": "ci-ubuntu-clang", "inherits": "asan", "cacheVariables": { "CMAKE_CXX_COMPILER": "clang++-18" }, "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" } },
    { "name": "ci-macos", "inherits": "asan", "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin" } }
  ],
  "buildPresets": [ { "name": "dev", "configurePreset": "dev" }, { "name": "release", "configurePreset": "release" } ],
  "testPresets":  [ { "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true }, "execution": { "jobs": 8 } } ],
  "workflowPresets": [ { "name": "check", "steps": [ { "type": "configure", "name": "dev" }, { "type": "build", "name": "dev" }, { "type": "test", "name": "dev" } ] } ]
}
```

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev      # or: cmake --workflow --preset check
cmake --list-presets
```

`CMakeUserPresets.json` (git-ignored) holds personal overrides (your `ccache` path, your Homebrew LLVM). IDEs (CLion, VS Code CMake Tools) read presets directly.

### 11.2 Dependencies: FetchContent vs vcpkg vs Conan

| | `FetchContent` | vcpkg (manifest mode) | Conan 2 |
|---|---|---|---|
| What it is | CMake downloads + builds the dependency's source as part of *your* build | Package manager; builds from source into a per-project `vcpkg_installed/`, cached binaries | Package manager; prebuilt binaries from ConanCenter or local builds; recipes in Python |
| Setup | Nothing extra | `git clone vcpkg && ./bootstrap-vcpkg.sh`; `vcpkg.json` in your repo; `-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` | `pip install conan`; `conanfile.txt`/`.py`; `conan install . --build=missing -s build_type=Release`; `-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake` |
| Good for | Small header-only deps (doctest, fmt, nanobench, Eigen); one-file projects | Medium/large C++ deps (HDF5, OpenBLAS, benchmark, Boost); Windows | Enterprise; multiple configs; binary caching; non-CMake deps |
| Pinning | `GIT_TAG <full commit sha>` (never a branch name) | `builtin-baseline` sha in `vcpkg.json` + `overrides` | Exact versions + lockfile `conan.lock` |
| Rebuild cost | Every clean build rebuilds deps (use `ccache`) | Once per triplet; binaries cached | Once; cached in `~/.conan2` |
| Offline / air-gapped | Needs network unless `FETCHCONTENT_SOURCE_DIR_<name>` is set | Asset caching | Local remote |

```cmake
include(FetchContent)
FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git
                     GIT_TAG ae7a13539fb71f270b87eb2e874fbac80bc8dda2  # v2.4.11 — ALWAYS a sha, with the tag in a comment
                     GIT_SHALLOW TRUE SYSTEM)                          # SYSTEM (3.25): treat headers as system → no warnings from them
FetchContent_MakeAvailable(doctest)
target_link_libraries(num_tests PRIVATE doctest::doctest)
find_package(Eigen3 3.4 CONFIG QUIET)                                  # prefer the system/vcpkg one if present...
if(NOT Eigen3_FOUND) FetchContent_Declare(eigen ...) FetchContent_MakeAvailable(eigen) endif()   # ...fall back to fetching
```

```json
// vcpkg.json
{ "name": "num", "version": "0.4.1", "dependencies": [ "eigen3", "benchmark", { "name": "hdf5", "features": ["cpp"] } ],
  "builtin-baseline": "a1a1cbc975abf909a6e8d3a7c5c1c4d5e6f7a8b9" }
```

### 11.3 Toolchain files and cross-compiling

A toolchain file sets *how* to compile before the project is read: `cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_SYSROOT /opt/sysroots/aarch64)            # where the target's libc/headers live
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)         # run host tools
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)          # find only target libraries
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64)      # ctest can run the tests under QEMU
```

Realistic cross targets for you: building the Linux x86-64 version of your library from the Mac (`brew install FiloSottile/musl-cross/musl-cross` or a Docker container — Docker is honestly simpler), a Raspberry Pi for a sensor project, WebAssembly via Emscripten (`emcmake cmake ..`; run your N-body in a browser), CUDA on a Linux box you SSH into. Never put `-mcpu=native`/`-march=native` in a library's default flags — the binary then only runs on the machine that built it; make it an option (`NUM_NATIVE_ARCH`) for benches.

### 11.4 Faster builds

- **Ninja** (`brew install ninja`; `-G Ninja`): correct dependency tracking, parallel by default, `ninja -t graph`, `ninja -d explain` (why did this rebuild), `ninja -t compdb` — always use it over Makefiles.
- **`ccache`** (`brew install ccache`): `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`; caches object files by hash of preprocessed source + flags; a `git checkout` back and forth becomes free. `ccache -s` shows the hit rate; CI: cache `~/.ccache` (`actions/cache`). `sccache` is the distributed/cloud variant.
- **Unity builds** (`-DCMAKE_UNITY_BUILD=ON`, `CMAKE_UNITY_BUILD_BATCH_SIZE=16`): concatenates TUs to parse headers once; 2–4× faster clean builds, but exposes ODR/`static` name collisions (a good thing to find) and hurts incremental builds. Use for CI release builds.
- **Precompiled headers** (`target_precompile_headers(num_core PRIVATE <vector> <string> <Eigen/Dense>)`): 1.5–2× on header-heavy code.
- **LTO** (`CMAKE_INTERPROCEDURAL_OPTIMIZATION ON` → `-flto=thin`): slower link, faster code (cross-TU inlining); release only.
- **`-ftime-trace`** per TU (`../11_headers_build_cmake_testing`) and **ClangBuildAnalyzer** for the whole build: finds the header that costs 40% of compile time.
- **`compile_commands.json`** (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `ln -s build/dev/compile_commands.json .`): clangd, clang-tidy, IWYU, `cppcheck --project` all read it. It is the *lingua franca* of C++ tooling; without it your editor is guessing.

---

## 12. CI with GitHub Actions

```yaml
# .github/workflows/ci.yml
name: ci
on: { push: { branches: [main] }, pull_request: {} }
concurrency: { group: "${{ github.workflow }}-${{ github.ref }}", cancel-in-progress: true }

jobs:
  build-test:
    strategy:
      fail-fast: false
      matrix:
        include:
          - { os: macos-14,     preset: dev,     name: "macOS arm64 AppleClang Debug" }
          - { os: macos-14,     preset: release, name: "macOS arm64 Release" }
          - { os: ubuntu-24.04, preset: dev,     cxx: g++-14,     name: "Ubuntu GCC 14 Debug" }
          - { os: ubuntu-24.04, preset: release, cxx: clang++-18, name: "Ubuntu Clang 18 Release" }
    name: ${{ matrix.name }}
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - name: deps (macOS)
        if: runner.os == 'macOS'
        run: brew install ninja ccache libomp
      - name: deps (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y ninja-build ccache libopenblas-dev ${{ matrix.cxx == 'clang++-18' && 'clang-18' || 'g++-14' }}
      - uses: actions/cache@v4
        with: { path: ~/.cache/ccache, key: "ccache-${{ matrix.name }}-${{ github.sha }}", restore-keys: "ccache-${{ matrix.name }}-" }
      - name: configure
        run: cmake --preset ${{ matrix.preset }} -DCMAKE_CXX_COMPILER_LAUNCHER=ccache ${{ matrix.cxx && format('-DCMAKE_CXX_COMPILER={0}', matrix.cxx) || '' }}
      - run: cmake --build --preset ${{ matrix.preset }} --parallel
      - run: ctest --preset ${{ matrix.preset }}
      - name: ccache stats
        run: ccache -s

  sanitizers:
    strategy: { matrix: { preset: [asan, tsan] } }
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get install -y ninja-build clang-18
      - run: cmake --preset ${{ matrix.preset }} -DCMAKE_CXX_COMPILER=clang++-18 && cmake --build --preset ${{ matrix.preset }}
      - run: ctest --preset ${{ matrix.preset }}
        env: { ASAN_OPTIONS: "detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1", UBSAN_OPTIONS: "print_stacktrace=1:halt_on_error=1", TSAN_OPTIONS: "second_deadlock_stack=1" }

  tidy-format:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get install -y ninja-build clang-18 clang-tidy-18 clang-format-18
      - run: clang-format-18 --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')
      - run: cmake --preset dev -DCMAKE_CXX_COMPILER=clang++-18
      - run: run-clang-tidy-18 -p build/dev -j "$(nproc)" -warnings-as-errors='*' 'src/.*|include/.*'

  coverage:
    runs-on: macos-14                          # llvm-cov via xcrun; on Ubuntu use clang-18 + llvm-cov-18
    steps:
      - uses: actions/checkout@v4
      - run: brew install ninja
      - run: cmake --preset dev -DNUM_COVERAGE=ON     # adds -fprofile-instr-generate -fcoverage-mapping
      - run: cmake --build --preset dev
      - run: LLVM_PROFILE_FILE="build/dev/%p.profraw" ctest --preset dev
      - run: |
          xcrun llvm-profdata merge -sparse build/dev/*.profraw -o build/dev/merged.profdata
          xcrun llvm-cov report build/dev/tests/num_tests -instr-profile=build/dev/merged.profdata -ignore-filename-regex='third_party|tests' | tee coverage.txt
          xcrun llvm-cov export build/dev/tests/num_tests -instr-profile=build/dev/merged.profdata -format=lcov > coverage.lcov
      - uses: codecov/codecov-action@v4
        with: { files: coverage.lcov }
```

Notes: `macos-14` runners are arm64 (M1); `macos-13` is x86-64 — test both if you ship both. `fail-fast: false` so one red job doesn't cancel the others. Pin action versions (`@v4`) and, for supply-chain paranoia, to a sha. Cache `ccache` *and* `FetchContent`'s `_deps/`. Add a nightly `schedule: - cron: '0 3 * * *'` workflow for the slow stuff: fuzzers for 10 minutes, `scan-build`, full benchmark run with results committed to a `gh-pages` branch (github-action-benchmark plots them). Required status checks on `main` + branch protection = nobody merges red. **Coverage** (`llvm-cov`): `-fprofile-instr-generate -fcoverage-mapping` (clang; GCC uses `--coverage` + `gcovr`); `llvm-cov show ... -format=html -output-dir=cov/` gives per-line HTML; aim for ≥ 80% line coverage on `src/`, 100% on anything that parses input, and *read the uncovered lines* — they're either dead code or untested error paths. Region coverage and branch coverage (`-show-branches=count`) are stricter than line coverage.

---

## 13. Documentation

Three layers, all in the repo:

1. **API reference from comments** — **Doxygen** (`brew install doxygen graphviz`; `doxygen -g` then edit: `EXTRACT_ALL=NO`, `WARN_IF_UNDOCUMENTED=YES`, `WARN_AS_ERROR=FAIL_ON_WARNINGS` in CI so every public symbol *must* have a doc comment, `GENERATE_XML=YES` for Sphinx, `HAVE_DOT=YES` for class diagrams, `INPUT=include/num`, `EXCLUDE_PATTERNS=*/detail/*`). Comment style:

```cpp
/// Solve A·x = b by LU with partial pivoting.
/// @param A  square, dense, row-major; not modified. @param b  length A.rows().
/// @return x with ‖A·x − b‖/‖b‖ ≲ n·ε·cond(A).
/// @throws SingularMatrix if a pivot is exactly zero; near-singular matrices return garbage silently — check `cond()` first.
/// @note O(n³/3) flops; not thread-safe with respect to `A`'s storage being modified concurrently.
[[nodiscard]] std::vector<double> solve(ConstMatrixView A, std::span<const double> b);
```

2. **Narrative docs** (design, tutorials, math) — **Sphinx + Breathe** (`pip install sphinx breathe furo`; Breathe imports Doxygen XML so `.. doxygenclass:: num::Matrix` renders inline; used by PyTorch/Eigen-adjacent projects; supports math via MathJax — essential for numerics docs) or **MkDocs Material + mkdoxy** (`pip install mkdocs-material mkdoxy`; Markdown, simpler, prettier, less flexible). Deploy from CI to GitHub Pages (`peaceiris/actions-gh-pages` or `actions/deploy-pages`). Docs to write: `README.md` (30-second pitch, install, one example), `docs/DESIGN.md` (`../18_software_design_and_library_architecture` §19), `docs/ERRORS.md`, `docs/PERFORMANCE.md` (benchmark table with machine specs), `docs/tutorials/*.md` whose code blocks are *extracted and compiled in CI* (a 20-line Python script, or `examples/` that `#include` a snippet file) — docs that don't compile are lies within a month.
3. **In-repo files GitHub renders**: `CONTRIBUTING.md` (how to build, run tests, the DoD), `CHANGELOG.md`, `LICENSE`, `SECURITY.md`, `.github/ISSUE_TEMPLATE/bug.yml` asking for compiler, OS, `cmake --preset`, a minimal repro.

---

## 14. Dependency hygiene

- **Pin everything**: `GIT_TAG` = commit sha; `vcpkg` baseline; `conan.lock`; `pre-commit` `rev`; GitHub Actions `@v4` (or sha). Unpinned = a build that breaks when someone else pushes.
- **Update deliberately**: **Dependabot** (`.github/dependabot.yml` covers Actions, pip, and — with `package-ecosystem: gitsubmodule` — submodules) or **Renovate** opens PRs; CI proves they're safe.
- **Fewer, bigger, boring**: every dependency is a compile-time cost, an ABI risk, a license, a CVE surface. Prefer the standard library, then a header-only single-purpose lib, then a big well-maintained one (Eigen, fmt, Abseil, Boost). No dependency for < 200 lines you could write.
- **Licenses**: know them. MIT/BSD/Apache-2.0/Boost — fine anywhere; LGPL (FFTW is GPL! — commercial use needs a license or pocketfft) — dynamic linking or your code becomes GPL; GPL — viral. `cmake -DCMAKE_EXPORT_...` doesn't check; **scancode-toolkit**, **FOSSA**, or `licensee` do. Keep `third_party/LICENSES.md`.
- **Vendoring vs fetching**: vendored copies (`third_party/eigen/`) are reproducible and offline but rot; document the upstream version and never patch in place (use a `patches/` dir applied at configure time with `PATCH_COMMAND`).
- **SBOM** (software bill of materials) — CycloneDX/SPDX — increasingly required for anything shipped to a customer; `cmake --graphviz` + a script, or `syft`, generates one.
- **Supply-chain**: verify checksums for downloaded tarballs (`URL_HASH SHA256=…` in `FetchContent`/`ExternalProject`), no `curl | sh`, build deps from source in CI at least once so you know you can.
- **System vs bundled**: `find_package` first, `FetchContent` fallback (§11.2); distro packagers (Homebrew, Debian) require the former, users on Windows require the latter.

---

## 15. Release engineering

- **Semantic versioning** (semver.org): `MAJOR.MINOR.PATCH`. PATCH = bug fixes, no API change; MINOR = additive, backward-compatible (new function, new optional parameter with default); MAJOR = anything that can break a user (removed/renamed function, changed semantics, changed ABI of a shared lib, raised minimum C++ standard). Pre-1.0 everything may break — say so. In CMake: `project(num VERSION 0.4.1)` → `num_VERSION_*` → `configure_file(version.hpp.in)` gives `NUM_VERSION_MAJOR` macros and `num::version()`; `set_target_properties(num PROPERTIES VERSION 0.4.1 SOVERSION 0)` names the shared library `libnum.0.dylib` / `libnum.so.0` (SOVERSION = ABI version; bump on ABI break).
- **`CHANGELOG.md`** in Keep-a-Changelog format (`## [0.4.1] - 2026-09-05` / `### Added` / `### Changed` / `### Deprecated` / `### Removed` / `### Fixed` / `### Security`), an `[Unreleased]` section every PR appends to (or generate from Conventional Commits `feat:`/`fix:`/`feat!:` with `git-cliff`). Deprecate before removing: `[[deprecated("use solve_into(); removed in 1.0")]]` for at least one MINOR release.
- **Tags and releases**: `git tag -a v0.4.1 -m "..." && git push --tags`; a `release.yml` workflow triggered `on: push: tags: ['v*']` that builds the release presets on every OS, runs tests, `cpack -G TGZ` (and `ZIP`, `DEB`, `productbuild`), uploads artifacts with checksums (`shasum -a 256 *.tar.gz > SHA256SUMS`) via `softprops/action-gh-release`, and archives the `.dSYM`/split-DWARF (§10) so crash reports from that version can be symbolicated.
- **Install rules that work**: `install(TARGETS num EXPORT numTargets ...)`, `install(EXPORT numTargets NAMESPACE num:: DESTINATION lib/cmake/num)`, `write_basic_package_version_file`, `configure_package_config_file` → downstream `find_package(num 0.4 CONFIG REQUIRED)` and `target_link_libraries(app num::num)` works from an install tree *and* from `add_subdirectory` *and* from `FetchContent`. Test all three in CI (a `tests/packaging/` mini-project).
- **Reproducible builds** (same source → bit-identical binary; lets anyone verify a release): no `__DATE__`/`__TIME__`/`__FILE__` with absolute paths (`-ffile-prefix-map=$PWD=.`, `-fdebug-prefix-map`), `SOURCE_DATE_EPOCH` for embedded timestamps, deterministic archive order (`ar D`, `-Wl,--build-id` on Linux), no `-march=native`, pinned toolchain (Docker image sha or Nix). `diffoscope` compares two builds and shows what differed. Full determinism needs matching compiler *binaries*; document the exact toolchain in `docs/RELEASING.md`.
- **Release checklist**: `CHANGELOG` moved from Unreleased; version bumped in one place; all CI green including nightly; benchmarks not regressed > 5% vs last release (compare.py); docs built and deployed; tag signed (`git tag -s`); artifacts + checksums uploaded; announce; open next `[Unreleased]`.

---

## 16. Definition of done

A change is *done* when every line below is true. Print it in `CONTRIBUTING.md`; put it in the PR template (`.github/pull_request_template.md`) as checkboxes.

- [ ] Builds with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Werror` on clang *and* GCC, macOS *and* Linux.
- [ ] `clang-format` clean; `clang-tidy` clean (no new `NOLINT` without a named check and a reason).
- [ ] New behavior has tests; a bug fix has a regression test that fails before the fix and passes after; tests are deterministic (fixed seeds, no wall-clock).
- [ ] Float comparisons use tolerances justified in a comment (`n·ε·‖A‖`), not magic `1e-6`.
- [ ] Linear-algebra / autograd / sim kernels have at least one property test (identity, finite difference, conservation law).
- [ ] Anything that parses bytes has a fuzz target with a committed seed corpus that replays clean.
- [ ] ASan+UBSan and (if threads) TSan jobs green; `leaks --atExit` clean on macOS.
- [ ] Coverage of changed lines ≥ 90%; every uncovered line has been *read* and is either unreachable-by-design or has a ticket.
- [ ] Any "faster" claim has a `compare.py` before/after table in the PR with machine, compiler, flags; no > 5% regression elsewhere.
- [ ] Public API has doc comments (units, failure behavior, complexity, thread-safety); Doxygen builds with zero warnings; `CHANGELOG.md` `[Unreleased]` updated; deprecations marked.
- [ ] No new dependency without a one-paragraph justification, license check, and pinned sha.
- [ ] No upward layer dependency; no new global mutable state; error policy respected (`docs/ERRORS.md`).
- [ ] Reviewed by someone else (or, alone: by you, tomorrow, using `../18_software_design_and_library_architecture` §18's checklist).
- [ ] Commit message explains *why*; PR description says how to verify.

---

## Gotchas and undefined behavior

- **`-Werror` on third-party headers** breaks your build when *their* new version adds a warning. Mark them `SYSTEM` (`target_include_directories(... SYSTEM ...)`, `FetchContent ... SYSTEM`) so warnings there are suppressed.
- **Silencing `-Wconversion` with casts** turns a diagnosable narrowing into a silent one. If the cast is wrong (`static_cast<int>(size_t)` on a 5-billion-element tensor), UBSan's `-fsanitize=implicit-conversion` is the only remaining net. Change the type instead.
- **Sanitizers at `-O0`** miss bugs that only manifest with optimization (e.g., a stack variable that's uninitialized at `-O2` but zero at `-O0`). Use `-O1`/`-O2 -g`.
- **ASan and TSan cannot be combined** in one binary; MSan needs *all* code instrumented. Separate CI jobs.
- **Leaks on Apple Silicon**: `detect_leaks=1` prints "LeakSanitizer is not supported" — use `leaks --atExit -- ./tests` (needs `MallocStackLogging=1` for allocation stacks: `MallocStackLogging=1 leaks --atExit -- ./tests`).
- **Fuzz target with an uncaught expected exception** is reported as a crash on every malformed input; catch your documented error type; anything *else* escaping is a real bug.
- **`doctest::Approx(0.0)`** with default `scale` (0) is a *relative* comparison against zero → only exact zero passes. The formula is `|a-b| <= epsilon * (scale + max(|a|,|b|))`, so `.epsilon(1e-12).scale(1.0)` gives an absolute 1e-12; `.scale(1e-12)` alone gives 1e-24 (a common mistake). Or compare `std::fabs(x) < tol` explicitly.
- **`CHECK(a == b)` on doubles** without `Approx`: passes on your machine, fails on CI with a different FMA contraction (`-ffp-contract` defaults differ between clang on arm64 and GCC on x86-64). Every floating comparison needs a tolerance.
- **Test order dependence** from shared static state passes in `./tests` and fails in `ctest -j8` (which runs cases as separate processes in random order). Run `--order-by=rand` locally.
- **`DoNotOptimize` on a large object by value** copies it every iteration — pass a pointer or reference (`DoNotOptimize(C.data())`).
- **Benchmark result without `ClobberMemory`** when the kernel only writes memory: the compiler may keep `C` in registers across iterations or delay stores past the timer. Use both barriers.
- **`Repetitions` without `ReportAggregatesOnly`** floods the output; **no `Repetitions`** gives you one noisy sample — always ≥ 5, compare medians.
- **`perf record` without `-g`/`--call-graph`** gives flat profiles — you see `memcpy` at 30% and not who called it.
- **`xctrace` on a binary without `-g`** shows addresses; keep the `.dSYM`, or Instruments can't symbolicate a stripped release binary.
- **Debugging at `-O2` chasing a bug that is UB**: run sanitizers first; a heisenbug that moves when you add a `printf` is UB 95% of the time.
- **`FetchContent` with `GIT_TAG main`** — your build changes without a commit in your repo; irreproducible. Always a sha.
- **`-march=native` in `CMAKE_CXX_FLAGS`** of a library — binaries crash with `Illegal instruction` on older CPUs. Option, off by default.
- **Coverage from a build with `-O2`**: inlining and dead-code elimination distort region counts. Measure coverage on the `Debug` preset.
- **Unity build hiding ODR violations** the other way: two `static void helper()` in different TUs *collide* under unity. It's a real (if benign) problem in your code; rename or namespace them.
- **`ctest` without `--output-on-failure`** hides the assertion message; put it in the preset.
- **Forgetting `LLVM_PROFILE_FILE=%p.profraw`** when tests run as multiple processes: each overwrites `default.profraw`.

---

## Common mistakes checklist

- [ ] Warnings flags in an `INTERFACE` target; `-Werror` on; third-party marked `SYSTEM`; every `-Wno-` has a comment.
- [ ] Narrowing fixed by changing the type; casts only at ABI boundaries, guarded.
- [ ] Sanitizer builds at `-O1 -g -fno-omit-frame-pointer`; ASan+UBSan and TSan as separate CI jobs; `UBSAN_OPTIONS=print_stacktrace=1`.
- [ ] Every parser has a fuzz target, a seed corpus, and a replay in CI.
- [ ] `.clang-tidy` and `.clang-format` in the repo root; `pre-commit install` documented; `.git-blame-ignore-revs` after reformatting.
- [ ] doctest with `Approx(...).epsilon().scale()`; `SUBCASE`/`TEST_CASE_TEMPLATE` used instead of copy-paste; `--order-by=rand` passes.
- [ ] At least one property test per kernel family with printed seed and shrinking.
- [ ] Benchmarks in `bench/`, Google Benchmark with `DoNotOptimize` + `ClobberMemory`, `Repetitions(5)`, `compare.py` for claims.
- [ ] Profiles taken from `RelWithDebInfo`; flame graph or inverted call tree read before optimizing anything.
- [ ] `compile_commands.json` symlinked; presets for `dev`/`asan`/`tsan`/`release`/`bench`; Ninja + ccache.
- [ ] CI matrix: macOS + Ubuntu × Debug/Release, sanitizer job, tidy/format job, coverage job, nightly fuzz/scan-build.
- [ ] Doxygen with `WARN_AS_ERROR`; tutorials' code compiled in CI.
- [ ] All deps pinned to shas; licenses recorded; Dependabot on.
- [ ] Semver + `CHANGELOG.md` + tagged releases with checksums and archived debug symbols.
- [ ] Definition of done in `CONTRIBUTING.md` and the PR template.

---

## You can move on when...

- You can list what `-Wshadow`, `-Wconversion`, `-Wsign-conversion`, `-Wold-style-cast` and `-Wnon-virtual-dtor` each catch and give the non-cast fix for each.
- You can name which sanitizers work on macOS arm64, which don't, what replaces LSan there, and read an ASan report's three parts (kind, access, origin).
- You can write a libFuzzer target for your `.npy`/CSV parser, build it with Homebrew LLVM, and explain `-max_len`, `-jobs`, `-dict`, `-merge=1`, `-minimize_crash`.
- You can write a `.clang-tidy` that keeps the numerics-relevant checks and say why `bugprone-narrowing-conversions` and `performance-unnecessary-value-param` matter for your Matrix code.
- You can write a doctest file with a fixture, a `TEST_CASE_TEMPLATE`, `SUBCASE`s and correct `Approx` usage, and a hand-rolled property test for `(AB)ᵀ = BᵀAᵀ` that prints its seed.
- You can write a Google Benchmark with `Range`, `DoNotOptimize`, `ClobberMemory`, `SetItemsProcessed` and use `compare.py` to justify a speedup.
- You can profile with `xctrace`/`perf`, produce a flame graph, and say whether a kernel is compute- or memory-bound from the counters.
- You can set a conditional breakpoint and a watchpoint in lldb, print a `std::vector`, debug an `-O2 -g` binary, and open a core dump.
- You can write `CMakePresets.json` with `dev`/`asan`/`release`, choose between FetchContent/vcpkg/Conan for a given dependency, and explain `ccache`, Ninja, unity builds, `compile_commands.json`.
- You can write the GitHub Actions matrix with sanitizer, tidy and `llvm-cov` jobs, and recite the definition of done.
