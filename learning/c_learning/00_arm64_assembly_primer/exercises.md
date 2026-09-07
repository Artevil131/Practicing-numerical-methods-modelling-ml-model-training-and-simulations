# Exercises — ARM64 Assembly Primer

Build and check every hand-written function the same way as `example.s`:
add a prototype to a small C driver, `cc -Wall -Wextra -std=c11 -O2 main.c f.s`,
run, and compare against a C reference implementation with `assert`.
Use `cc -O1 -S -o - f.c` to compare your version with the compiler's.

1. **Register views.** Write `int add_i(int a, int b)` in assembly using `w`
   registers, and `long add_l(long a, long b)` using `x` registers. Call
   `add_i(INT_MAX, 1)` and `add_l(INT_MAX, 1)` from C and explain the two
   results from the register width alone.

2. **Predict then check.** Without compiling, write the `-O1` assembly for
   `long sub_then_double(long a, long b) { return (a - b) * 2; }`. Then run
   `cc -O1 -S`. Did the compiler use `mul`, `lsl`, or `add x0, x0, x0`?
   Why is that a valid choice?

3. **Conditional select.** Hand-write `long clamp(long x, long lo, long hi)`
   with two `cmp`/`csel` pairs and no branches. Compare with the compiler.

4. **Unsigned vs signed.** Write `int is_lt_s(long a, long b)` and
   `int is_lt_u(unsigned long a, unsigned long b)` using `cmp` + `cset`.
   Which condition suffix does each need? Test with `a = -1, b = 1`.

5. **Addressing modes.** Write `long get(const long *a, long i)` that returns
   `a[i]` in a single load using the `[base, index, lsl #3]` mode, and
   `void set_second(int *p, int v)` that stores `v` into `p[1]` with an
   immediate offset. Verify with `otool -tv` that each body is one instruction
   plus `ret`.

6. **Post-index loop.** Write `long count_neg(const long *a, size_t n)` that
   counts negative elements. Use `ldr xN, [x0], #8`, `cmp`, and `cinc`
   (conditional increment) so the loop body has no branch except the loop-back.

7. **A non-leaf function.** Write `long twice(long x)` in assembly that calls
   the C function `long g(long)` (you provide `g` in the driver) and returns
   `g(x) + g(x)`. You must save `x30` and keep `x` (and the first result)
   alive across the calls — use `x19`/`x20` and a correct `stp`/`ldp` prologue
   and epilogue. Break the program on purpose once by omitting the `x30` save
   and describe the crash.

8. **Globals.** Declare `long hits;` in C. Write `void hit(void)` in assembly
   that increments it using `adrp _hits@GOTPAGE` / `ldr … @GOTPAGEOFF`. Call
   it a million times and print `hits`. Then read `cc -O1 -S` of the C version
   and confirm the same sequence.

9. **Floating point.** Write `double mean(const double *a, size_t n)` in
   assembly: accumulate with `fadd`, convert `n` with `ucvtf d1, x1`, divide
   with `fdiv`. Handle `n == 0` by returning `0.0` (`fmov d0, xzr`). Compare
   against a C version for a 1000-element array; are the sums bit-identical?
   (They should be: same operation order.)

10. **Reading -O2.** Compile `sum` from `lesson.md` §5 with `-O2 -S` and
    annotate the output yourself: mark (a) the guard that chooses the vector
    path, (b) the vector loop and how many elements each iteration consumes,
    (c) the horizontal reduction, (d) the scalar tail loop. Then add
    `-Rpass=loop-vectorize` to the command and match the remark's
    "vectorized loop (vectorization width: 2, interleaved count: 4)" to what
    you marked.
