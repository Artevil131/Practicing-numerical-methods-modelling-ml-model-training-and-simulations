# C Course

Fourteen chapters, then sixteen projects. Every chapter folder has:

| File           | What it is                                                                  |
|----------------|-----------------------------------------------------------------------------|
| `lesson.md`    | The full explanation of every concept in the chapter. Read it once, slowly. |
| `exercises.md` | 10 exercises, easy → hard. Last 3 always point at ML / numerics / sims.     |
| `example.c`    | One compilable program using everything in the chapter. Read, run, modify.  |

## How to work a chapter

```sh
cd c_learning/01_hello_compile_types
cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo   # 1. run the example
# 2. read lesson.md with example.c open beside it
# 3. do the exercises: ex01_1.c, ex01_2.c, ... in the same folder
cc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined -o ex01_1 ex01_1.c -lm && ./ex01_1
```

Rules: `-Wall -Wextra` always, zero warnings before moving on. Type the code, don't paste.
When something behaves strangely, rebuild with `-fsanitize=address,undefined` before guessing.

## Chapters

| #  | Chapter                              | Core idea                                        | Unlocks project |
|----|--------------------------------------|--------------------------------------------------|-----------------|
| 01 | `01_hello_compile_types`             | Compile pipeline, types, printf, casts, overflow  |                 |
| 02 | `02_control_flow`                    | if/switch/loops, precedence traps, numeric loops  |                 |
| 03 | `03_functions_and_scope`             | Prototypes, by-value, recursion, the call stack   |                 |
| 04 | `04_arrays_and_strings`              | Fixed arrays, row-major, `'\0'` strings, overflow | P01             |
| 05 | `05_pointers`                        | Addresses, arithmetic, out-params, `argv`         | P01             |
| 06 | `06_dynamic_memory`                  | malloc/free, ownership, the four memory bugs, ASan| P02             |
| 07 | `07_structs_unions_enums`            | `Matrix` struct, tagged unions, opaque types      | P02             |
| 08 | `08_file_io`                         | Text/binary I/O, CSV, MNIST IDX, PGM/PPM images   | P04, P14        |
| 09 | `09_multi_file_projects_and_make`    | Headers, linking, Makefiles, a reusable library   | P02             |
| 10 | `10_data_structures`                 | Dynamic array, hash table, tree, heap             | P03, P07, P10   |
| 11 | `11_function_pointers_and_generics`  | Callbacks, dispatch tables, `Layer` vtable        | P09, P12, P13   |
| 12 | `12_numbers_bits_floats`             | IEEE 754, stable softmax, RNG, Box–Muller         | P08, P11        |
| 13 | `13_debugging_testing_perf`          | lldb, ASan, unit tests, gradient check, cache     | P12             |
| 14 | `14_preprocessor_and_c_idioms`       | Macros, X-macros, header-only libs, error idioms  | P13             |

### Advanced (professional level) — after the projects P01–P12 feel comfortable

| #  | Chapter                                  | Core idea                                                        |
|----|------------------------------------------|------------------------------------------------------------------|
| 15 | `15_undefined_behavior_and_the_standard` | The UB catalogue, strict aliasing, promotions, how compilers exploit UB, portability |
| 16 | `16_concurrency_pthreads_and_atomics`    | pthreads, mutex/condvar, C11 atomics & memory orders, thread pool, OpenMP, TSan |
| 17 | `17_posix_systems_programming`           | fds, mmap, fork/exec, pipes, signals, select/poll, sockets, dlopen |
| 18 | `18_memory_allocators_and_layout`        | Write malloc: arena/pool/free-list/buddy; alignment, cache lines, SoA, TLB |
| 19 | `19_simd_and_low_level_performance`      | Roofline, auto-vectorization, NEON intrinsics, tiling, reading assembly, matmul to 50 GFLOP/s |
| 20 | `20_professional_c_engineering`          | API/ABI design, shared libs, Python FFI to your matrix lib, fuzzing, static analysis, CI, reading SQLite/Lua/ggml |

## ARM64 assembly track

`00_arm64_assembly_primer/` teaches AArch64 from zero (registers, AAPCS64 calling convention,
instruction families, macOS toolchain, lldb on asm) with hand-written leaf functions linked from C.
Chapters 01, 02, 03, 04, 05, 07, 12, 13, 16, 19 each carry an `asm.md`: that chapter's C next to the
real `cc -S` output from this machine, annotated line by line, with "predict the asm" tasks.
Read the primer after chapter 05; then read each `asm.md` right after its chapter.

## Projects

See [`projects/README.md`](projects/README.md) — 16 projects from a char tokenizer to an
N-body simulation and a char-level language model, with two tracks (ML-first / physics-first).

Chapters 01–07 are enough to start P01 and P02. Don't finish the whole course before building.

## Reference material (external)

- K&R, *The C Programming Language*, 2nd ed. — still the best short book. Read it after chapter 07.
- Beej's Guide to C Programming — free, modern, thorough: https://beej.us/guide/bgc/
- cppreference C section — the manual: https://en.cppreference.com/w/c
- *Modern C* (Jens Gustedt) — free PDF, for after you're comfortable.
