# Chapter 02 — Control Flow

## What you'll be able to do after this chapter

- Write branching code with `if`/`else if`/`else`, `switch`, and the ternary operator, and predict which branch runs for any value.
- Write all three loop forms (`while`, `do-while`, `for`) and choose the right one for counting, accumulating, and "run until converged" patterns.
- Use `break`, `continue`, and (in the one legitimate case) `goto` to control loop exit and cleanup.
- Read an expression like `a & b == c` or `*p++` and know exactly what it does from the precedence table.
- Write the loop skeletons that every numerical program is built from: accumulate, argmax, running mean/variance, nested i/j/k matrix loops.
- Avoid the classic traps: `=` in a condition, dangling `else`, reverse iteration with an unsigned counter.

## Why this matters for ML / numerics / sims

Every numerical algorithm is loops around arithmetic. Matrix multiply is three nested `for` loops. Gradient descent is a `while (not converged)` loop around an inner `for` over parameters. An N-body step is a double loop over pairs of bodies. A BPE tokenizer is a `while (vocab_size < target)` loop around a `for` scan of pairs. The spinning donut is nested loops over two angles with a `switch` or lookup for the shading character. If you get the loop bounds, the accumulator initialization, or the `break` condition wrong, the algorithm is wrong, and C will not raise an exception to tell you; it will print a plausible-looking wrong number. This chapter teaches you to write those loops correctly the first time.

## 1. `if` / `else if` / `else`

```c
#include <stdio.h>
int main(void)
{
    double loss = 0.42;
    if (loss < 0.1) {
        printf("converged\n");
    } else if (loss < 1.0) {
        printf("training\n");
    } else {
        printf("diverging\n");
    }
    return 0;
}
```

Output:

```
training
```

Rules:

- The condition MUST be in parentheses.
- Braces are optional for a single statement, but always write them. Without braces, `if (x) a(); b();` executes `b()` unconditionally. Indentation does not matter to the compiler.
- `else if` is not a keyword; it is an `else` followed by another `if`.
- There is no `elif`, no `:` after the condition, no `and`/`or`/`not` keywords (use `&&`, `||`, `!`).

Python equivalent: `if ... elif ... else` with braces replacing indentation.

## 2. Truthiness: 0 is false, everything else is true

C has no separate boolean concept at the language core. A condition is an integer (or pointer, or floating) expression. Zero (or `NULL`, or `0.0`) is false; any nonzero value is true. Comparison operators produce `int` `1` or `0`.

```c
#include <stdio.h>
#include <stdbool.h>   /* gives you the names bool, true, false (C99) */
int main(void)
{
    int n = 5;
    if (n) printf("n is nonzero\n");
    if (!n) printf("never\n");
    printf("%d %d\n", 3 < 4, 3 > 4);          /* 1 0 */
    printf("%d\n", 5 == 5);                   /* 1 */
    double x = 0.0;
    if (!x) printf("0.0 is false\n");
    bool done = false;
    printf("sizeof(bool)=%zu\n", sizeof done); /* 1 */
    done = 42;                                /* stored as 1: bool normalizes */
    printf("%d\n", done);                     /* 1 */
    return 0;
}
```

Output:

```
n is nonzero
1 0
1
0.0 is false
sizeof(bool)=1
1
```

There is no `None`. A pointer that "points to nothing" is `NULL` (which is false). An `int` that "has no value" does not exist; you pick a sentinel like `-1` yourself.

`<stdbool.h>` gives `bool` (really `_Bool`), `true` (1), `false` (0). Use it for clarity; it costs nothing.

Python equivalent: Python's `if x:` also treats `0`, `0.0`, `None`, `[]`, `""` as false. C has only the numeric case. A C `char*` string is true even if empty (it is a non-NULL address).

## 3. Comparison and logical operators

| Operator | Meaning | Result |
|---|---|---|
| `==` `!=` | equal, not equal | `int` 0/1 |
| `<` `<=` `>` `>=` | ordering | `int` 0/1 |
| `&&` | logical AND | `int` 0/1 |
| `\|\|` | logical OR | `int` 0/1 |
| `!` | logical NOT | `int` 0/1 |

There is no chained comparison: `0 < x < 10` compiles but means `(0 < x) < 10`, which is `(0 or 1) < 10`, always true. Write `0 < x && x < 10`.

```c
int x = 50;
if (0 < x < 10) printf("wrong: this prints\n");      /* (0<50)=1, 1<10 -> true */
if (0 < x && x < 10) printf("correct: this does not\n");
```

## 4. Short-circuit evaluation

`&&` and `||` evaluate left to right and STOP as soon as the result is known. The right operand is not evaluated at all if the left one decides the outcome. This is guaranteed by the standard and is used to guard dangerous operations.

```c
#include <stdio.h>
int main(void)
{
    int *p = NULL;
    int n = 0;
    if (p != NULL && *p > 0) printf("positive\n");   /* *p is NOT evaluated: no crash */
    if (n != 0 && 100 / n > 5) printf("big\n");      /* division NOT evaluated */
    if (n == 0 || 100 / n > 5) printf("guarded\n");  /* prints; division skipped */
    return 0;
}
```

Output:

```
guarded
```

Order matters: `*p > 0 && p != NULL` crashes. Put the guard on the left.

Python equivalent: identical semantics (`and`/`or` short-circuit). Python additionally returns the operand value; C returns 0 or 1.

## 5. `switch` / `case` / `break` / fallthrough / `default`

`switch` compares an integer expression against constant `case` labels and jumps to the matching one. Execution then CONTINUES into the next case unless you `break`. This is fallthrough, and forgetting `break` is a classic bug.

```c
#include <stdio.h>
int main(void)
{
    for (int op = 0; op <= 4; op++) {
        switch (op) {
        case 0:
            printf("%d: add\n", op);
            break;
        case 1:
            printf("%d: sub\n", op);
            break;
        case 2:                   /* deliberate fallthrough: 2 and 3 share code */
        case 3:
            printf("%d: mul or div\n", op);
            break;
        default:
            printf("%d: unknown\n", op);
            break;                /* optional on the last case, but write it */
        }
    }
    return 0;
}
```

Output:

```
0: add
1: sub
2: mul or div
3: mul or div
4: unknown
```

Rules:

- The switch expression must be an integer type (`int`, `char`, `enum`). No strings, no floats.
- `case` labels must be compile-time constants (`case 3:`, `case 'x':`, `case OP_ADD:`). Not variables.
- Two cases with the same value is a compile error.
- `default` is optional; without it, an unmatched value does nothing.
- To declare a variable inside a case, wrap the case body in `{ }`.
- Clang has `-Wimplicit-fallthrough` to warn on unmarked fallthrough. In C23 you can write `[[fallthrough]];`; in C11, a comment `/* fallthrough */` is the convention.

`switch` on an `enum` is how you will dispatch operations in an autograd graph (`case OP_ADD: ... case OP_MUL: ...`) or an interpreter for a tiny expression language. The compiler often turns it into a jump table, faster than a chain of `if`.

Python equivalent: `match`/`case` (3.10+), except Python's has no fallthrough and matches structurally. Before 3.10, a dict of functions.

## 6. `while`

Test first, then run the body. Runs zero or more times.

```c
#include <stdio.h>
#include <math.h>
int main(void)
{
    /* Newton's method for sqrt(2): iterate until change is tiny */
    double x = 1.0, prev = 0.0;
    int iters = 0;
    while (fabs(x - prev) > 1e-12) {
        prev = x;
        x = 0.5 * (x + 2.0 / x);
        iters++;
    }
    printf("sqrt(2) = %.15f after %d iters\n", x, iters);
    return 0;
}
```

Output:

```
sqrt(2) = 1.414213562373095 after 6 iters
```

A `while` is the natural loop for "until converged" where you do not know the count in advance. Always include a safety cap (`&& iters < 1000`) for iterative solvers; a diverging Newton iteration otherwise never terminates.

## 7. `do-while`

Run the body first, THEN test. Runs at least once. Note the trailing `;`.

```c
#include <stdio.h>
int main(void)
{
    int attempts = 0;
    int value;
    do {
        value = attempts * 7 % 5;   /* pretend this is reading input */
        attempts++;
    } while (value != 3 && attempts < 10);
    printf("got %d after %d attempts\n", value, attempts);
    return 0;
}
```

Output:

```
got 3 after 5 attempts
```

Use it for "read input, validate, repeat if bad" and for algorithms that must execute one step before the termination test makes sense (e.g. first iteration of a solver before you have a residual to compare).

Python equivalent: Python has no `do-while`; you write `while True: ... if cond: break`.

## 8. `for` — all three clauses

```c
for (init; condition; step) body
```

Equivalent to:

```c
{
    init;
    while (condition) {
        body
        step;
    }
}
```

except that `continue` jumps to `step`, not past it. A variable declared in `init` (C99+) is scoped to the loop.

```c
#include <stdio.h>
int main(void)
{
    /* standard counted loop */
    for (int i = 0; i < 5; i++) printf("%d ", i);
    printf("\n");

    /* multiple init variables (same type) and multiple steps (comma operator) */
    for (int i = 0, j = 10; i < j; i++, j--) printf("(%d,%d) ", i, j);
    printf("\n");

    /* step by 2, counting down */
    for (int k = 10; k > 0; k -= 2) printf("%d ", k);
    printf("\n");

    /* empty init: variable declared outside so its final value survives */
    int n = 0;
    for (; n * n < 50; n++) { }
    printf("first n with n*n >= 50: %d\n", n);

    /* empty condition = infinite loop; exit with break */
    int count = 0;
    for (;;) {
        if (++count >= 3) break;
    }
    printf("count=%d\n", count);

    /* floating loop variable: legal but dangerous (accumulated rounding) */
    int steps = 0;
    for (double t = 0.0; t < 1.0; t += 0.1) steps++;
    printf("double loop ran %d times (expected 10; may be 11)\n", steps);
    /* correct: integer counter, compute t from it */
    for (int s = 0; s < 10; s++) { double t = s * 0.1; (void)t; }
    return 0;
}
```

Output:

```
0 1 2 3 4
(0,10) (1,9) (2,8) (3,7) (4,6)
10 8 6 4 2
first n with n*n >= 50: 8
count=3
double loop ran 11 times (expected 10; may be 11)
```

Rule for numerics: NEVER use a floating-point loop counter. Count with an integer and compute `t = t0 + i * dt`.

Python equivalent: `for i in range(a, b, step)`. C's `for` is more general (any condition, any step expression) and has no iterator protocol; iterating over an array means iterating over indices.

## 9. `break` and `continue`

- `break`: exit the innermost enclosing loop or `switch` immediately.
- `continue`: skip the rest of the body and go to the next iteration (to `step` in a `for`).

```c
#include <stdio.h>
int main(void)
{
    double data[] = {1.0, -1.0, 2.5, -3.0, 4.0, 1e300, 0.5};
    int n = 7;
    double sum = 0.0;
    int used = 0;
    for (int i = 0; i < n; i++) {
        if (data[i] < 0) continue;          /* skip negatives */
        if (data[i] > 1e100) break;         /* stop at the first absurd value */
        sum += data[i];
        used++;
    }
    printf("sum=%g over %d values\n", sum, used);
    return 0;
}
```

Output:

```
sum=7.5 over 3 values
```

`break` inside a `switch` inside a loop exits the `switch`, not the loop. To leave the loop from inside a `switch`, set a flag or use `goto`.

## 10. Nested loops and the labeled-break substitute

C has no `break 2` or labeled `break`. Three options to exit two levels of loop:

```c
#include <stdio.h>
int main(void)
{
    int grid[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    int target = 7, fi = -1, fj = -1;

    /* Option A: flag variable */
    int found = 0;
    for (int i = 0; i < 3 && !found; i++) {
        for (int j = 0; j < 4; j++) {
            if (grid[i][j] == target) { fi = i; fj = j; found = 1; break; }
        }
    }
    printf("A: found at (%d,%d)\n", fi, fj);

    /* Option B: goto to a label after the loops (accepted C idiom) */
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            if (grid[i][j] == target) { fi = i; fj = j; goto done; }
        }
    }
done:
    printf("B: found at (%d,%d)\n", fi, fj);

    /* Option C: put the loops in a function and `return` (see chapter 03). */
    return 0;
}
```

Output:

```
A: found at (1,2)
B: found at (1,2)
```

Option C is usually the cleanest. Option B is fine and common in C codebases. Option A becomes messy with three levels.

## 11. `goto` — when it is legitimate

`goto label;` jumps to `label:` in the same function. Jumping backwards to build loops is unreadable; do not do it. The accepted uses are:

1. Breaking out of nested loops (above).
2. Centralized cleanup on error in a function that acquires several resources.

```c
#include <stdio.h>
#include <stdlib.h>
int load_two_buffers(size_t n)
{
    int status = -1;
    double *a = malloc(n * sizeof *a);
    if (!a) goto out;
    double *b = malloc(n * sizeof *b);
    if (!b) goto free_a;

    /* ... use a and b ... */
    printf("both buffers allocated\n");
    status = 0;

    free(b);
free_a:
    free(a);
out:
    return status;
}
int main(void) { return load_two_buffers(1000) == 0 ? 0 : 1; }
```

This pattern replaces Python's `try/finally` and nested `with` blocks. You will use it in every function that opens a file and allocates memory. Chapter `../06_*` covers `malloc`/`free` in full; here just see the shape.

Do not `goto` into a block past a variable declaration with an initializer for a VLA; that is a constraint violation. Jumping past ordinary declarations is legal but leaves them uninitialized.

## 12. Ternary `?:`

`cond ? a : b` is an expression: evaluates `cond`, then ONLY ONE of `a` or `b`, and yields it. Both `a` and `b` must have compatible types.

```c
#include <stdio.h>
int main(void)
{
    int x = -3;
    int abs_x = x < 0 ? -x : x;
    printf("%d\n", abs_x);                        /* 3 */
    double relu = x > 0 ? (double)x : 0.0;        /* ReLU activation */
    printf("%g\n", relu);                         /* 0 */
    printf("%s\n", x % 2 == 0 ? "even" : "odd");  /* odd */
    int a = 5, b = 9;
    int max = a > b ? a : b;
    printf("%d\n", max);                          /* 9 */
    return 0;
}
```

Nesting ternaries (`a ? b : c ? d : e`) is legal (right-associative) and unreadable beyond one level. Use `if`.

Python equivalent: `a if cond else b`. Note the different order: Python puts the condition in the middle.

## 13. Comma operator

`a, b` evaluates `a`, discards it, evaluates `b`, and yields `b`. Lowest precedence of all operators. Its only common legitimate use is in `for` steps (`i++, j--`). Do not confuse it with the comma that separates function arguments or initializers (that comma is punctuation, not the operator).

```c
int i = (1, 2, 3);       /* i = 3; the parentheses are required */
int j = 1, 2, 3;         /* ERROR: this comma is a declarator separator */
for (int a = 0, b = 10; a < b; a++, b--) {}   /* OK */
```

## 14. Operator precedence table and the classic traps

Highest to lowest. Operators on the same row have equal precedence and associate in the direction shown.

| Prec | Operators | Assoc | Notes |
|---|---|---|---|
| 1 | `()` `[]` `->` `.` `x++` `x--` (postfix) | left | |
| 2 | `++x` `--x` `+x` `-x` `!` `~` `(type)` `*x` `&x` `sizeof` | right | unary |
| 3 | `*` `/` `%` | left | |
| 4 | `+` `-` | left | |
| 5 | `<<` `>>` | left | shifts |
| 6 | `<` `<=` `>` `>=` | left | |
| 7 | `==` `!=` | left | |
| 8 | `&` | left | bitwise AND (BELOW `==`!) |
| 9 | `^` | left | bitwise XOR |
| 10 | `\|` | left | bitwise OR |
| 11 | `&&` | left | |
| 12 | `\|\|` | left | |
| 13 | `?:` | right | |
| 14 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | right | |
| 15 | `,` | left | |

The traps:

**`=` vs `==`**: `if (x = 5)` assigns 5 to `x` and tests 5 (true). Compiles. `-Wall` warns ("using the result of an assignment as a condition"); wrap in double parentheses `if ((x = f()))` only if you really mean it.

**`&` vs `&&`**: `if (flags & MASK == 0)` parses as `flags & (MASK == 0)` because `==` binds tighter than `&`. Always parenthesize bitwise tests: `if ((flags & MASK) == 0)`.

**`i++` vs `++i`**: `i++` yields the OLD value then increments; `++i` increments then yields the NEW value. As a standalone statement they are identical; as a subexpression they differ. `a[i++] = x` stores at old `i`; `a[++i] = x` at new `i`. Never modify a variable twice in one expression (`i = i++`, `a[i] = i++`): that is UB, not "implementation-specific".

**Dangling else**: `else` binds to the NEAREST unmatched `if`, regardless of indentation.

```c
if (a)
    if (b) printf("both\n");
else                          /* indentation lies: this else belongs to if (b) */
    printf("not a\n");        /* actually prints when a && !b */
```

Fix with braces, always.

**Shift and add**: `1 << n + 1` is `1 << (n + 1)`. Parenthesize.

**Unary minus and cast**: `(int)-x` is fine; `-x % y` is `(-x) % y`.

**`*p++`**: postfix `++` binds tighter than `*`, so it is `*(p++)`: dereference `p`, then advance `p`. `(*p)++` increments the pointed-to value. See `../05_pointers/lesson.md`.

When in doubt, parenthesize. Extra parentheses cost nothing.

## 15. Loop patterns for numerics

These are the building blocks of everything that follows. Learn them as idioms.

### Accumulate (sum, dot product)

```c
double sum = 0.0;                        /* initialize! 0 for sums, 1 for products */
for (int i = 0; i < n; i++) sum += x[i];

double dot = 0.0;
for (int i = 0; i < n; i++) dot += x[i] * y[i];
```

Python equivalent: `np.sum(x)`, `np.dot(x, y)`.

### Argmax / max

```c
int best = 0;                            /* start at the FIRST element, not at -1 or 0.0 */
for (int i = 1; i < n; i++)
    if (x[i] > x[best]) best = i;
/* x[best] is the max, best is the argmax. Requires n >= 1. */
```

Starting with `max = 0.0` fails when all values are negative. Starting with `max = -INFINITY` (from `<math.h>`) also works for doubles.

Python equivalent: `np.argmax(x)`.

### Running mean and variance (Welford)

One pass, numerically stable, no need to store the data. Used for normalizing features and in batch norm.

```c
double mean = 0.0, m2 = 0.0;
for (int i = 0; i < n; i++) {
    double delta = x[i] - mean;
    mean += delta / (i + 1);
    m2   += delta * (x[i] - mean);
}
double variance = n > 1 ? m2 / (n - 1) : 0.0;   /* sample variance */
```

Note `(i + 1)` is an `int` and `delta` is `double`, so the division is floating. If you had written `delta / (i + 1)` with `delta` an `int`, it would be integer division.

### Nested i/j/k: matrix multiply

```c
/* C[m][p] = A[m][n] * B[n][p], all stored flat, row-major */
for (int i = 0; i < m; i++)
    for (int j = 0; j < p; j++) {
        double acc = 0.0;
        for (int k = 0; k < n; k++)
            acc += A[i * n + k] * B[k * p + j];
        C[i * p + j] = acc;
    }
```

The accumulator `acc` is re-zeroed for every `(i, j)` pair. Forgetting that is the most common matmul bug. Loop order (`ijk` vs `ikj`) affects cache performance by 5-10x; `../04_arrays_and_strings/lesson.md` covers layout.

Python equivalent: `C = A @ B`.

### Iterating in reverse: the unsigned trap

```c
size_t n = 5;
for (size_t i = n - 1; i >= 0; i--)     /* INFINITE LOOP: size_t is never < 0 */
    printf("%zu ", i);                   /* when i is 0, i-- wraps to SIZE_MAX */
```

`-Wall -Wextra` warns: "comparison of unsigned expression >= 0 is always true". Correct forms:

```c
for (size_t i = n; i-- > 0; )           /* idiom: test then decrement; body sees n-1 .. 0 */
    printf("%zu ", i);

for (size_t i = n; i > 0; i--)          /* or: use i-1 inside */
    printf("%zu ", i - 1);

for (int i = (int)n - 1; i >= 0; i--)   /* or: signed counter if n fits in int */
    printf("%d ", i);
```

The `i-- > 0` idiom is standard C. Reverse iteration appears in backpropagation (walk the graph from the loss backwards) and in back-substitution for triangular solves.

### Convergence loop with a cap

```c
int iter = 0;
double err = INFINITY;
while (err > tol && iter < max_iter) {
    /* one step of the solver, update err */
    iter++;
}
if (iter == max_iter) fprintf(stderr, "warning: did not converge\n");
```

## Gotchas and undefined behavior

- `if (x = 0)` is always false and clobbers `x`. `-Wall` warns. Some people write `if (0 == x)` to make the typo a compile error.
- `0 < x < 10` compiles and is wrong.
- Missing `break` in `switch` falls through silently.
- `switch` on a `double` or string does not compile. `switch` on a `char` does.
- `for (int i = 0; i < n; i++);` with a stray `;` runs an empty loop, then the "body" once. `-Wempty-body` (in `-Wextra`) catches some cases.
- Modifying a loop variable inside the loop body is legal and confusing. Avoid.
- `i = i++ + 1` and `a[i] = i++` are UB (unsequenced modification and access).
- Floating loop counters accumulate rounding error; count with integers.
- `size_t i; for (i = n-1; i >= 0; i--)` never terminates.
- `while (fabs(x) > 1e-300)` may never terminate if `x` becomes exactly 0 or `NaN`. `NaN > anything` is false, `NaN < anything` is false; a `while (err > tol)` exits on NaN, a `while (!(err <= tol))` does not. Check `isnan(err)` explicitly in solvers.
- Signed `int` loop counter reaching `INT_MAX` and incrementing is UB. With `n` up to 2 billion, use `long` or `size_t`.
- `break` inside `switch` inside a loop exits only the `switch`.
- `goto` jumping over a VLA declaration is a compile error; over a regular initialized variable leaves it uninitialized.
- Dangling `else` binds to the nearest `if`; braces fix it.

## Common mistakes checklist

- [ ] Every `if`, `for`, `while` body is in braces.
- [ ] No `=` inside a condition unless deliberately double-parenthesized.
- [ ] Every `switch` case ends in `break` or a `/* fallthrough */` comment.
- [ ] Every `switch` has a `default` (even if it is just `break;` or an error message).
- [ ] Bitwise tests are parenthesized: `(flags & MASK) != 0`.
- [ ] Accumulators are initialized (0 for sum, 1 for product, first element for max).
- [ ] Argmax loop handles all-negative input.
- [ ] No floating-point loop counter.
- [ ] Reverse loops with unsigned counters use `i-- > 0`.
- [ ] Every convergence loop has an iteration cap and a NaN check.
- [ ] No variable is modified twice in one expression.

## You can move on when...

- You can write `switch` dispatch on an `enum` with a shared case and a `default`, without fallthrough bugs.
- You can convert any `for` loop into an equivalent `while` and back, including where `continue` goes.
- You can explain why `flags & MASK == 0` is wrong and fix it.
- You can write the accumulate, argmax, Welford, and `ijk` matmul loops from memory, with correct initialization.
- You can iterate an array backwards with a `size_t` index and explain why the naive version never ends.
- You can write a `goto cleanup` error-handling skeleton for a function that allocates two resources.
- You can predict the output of `int i = 3; printf("%d %d", i++, i);` -- answer: you cannot, it is UB (unsequenced), and you know why.
