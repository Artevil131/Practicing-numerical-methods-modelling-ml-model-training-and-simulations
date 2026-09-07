# Chapter 05 — Exercises

Write each exercise as `ex05_K.c` in this folder. Compile with:

```sh
cc -Wall -Wextra -std=c11 -O2 -o ex05_K ex05_K.c -lm
```

Also build each one once with `-O1 -g -fsanitize=address,undefined` and run it. Zero warnings, zero sanitizer reports.

---

**05.1 — Draw the boxes, then print them**

Declare `int x = 5; int *p = &x; int **pp = &p;`. Print, each on its own line with a label: `x`, `&x`, `p`, `*p`, `&p`, `pp`, `*pp`, `**pp`. Then execute `**pp = 9;` and print `x`. Before running, draw the three boxes and arrows in a comment at the top of the file and write your predicted values for the equalities `p == &x`, `*pp == p`, `*pp == &x`.

Example (addresses will differ):

```
x     = 5
&x    = 0x16f...
p     = 0x16f...   (same as &x)
*p    = 5
...
after **pp = 9: x = 9
```

<details><summary>Hint</summary>
`%p` needs `(void *)` casts. `pp` has type `int **`; `*pp` is an `int *`; `**pp` is an `int`.
</details>

---

**05.2 — Pointer arithmetic on different types**

Declare `double d[4] = {1.5, 2.5, 3.5, 4.5}; int i4[4] = {1, 2, 3, 4}; char s[] = "abcd";`. For each array, take a pointer to element 0, print the address of `p`, `p + 1`, and `p + 3`, and the byte distance `(char *)(p + 1) - (char *)p`. Then print `*(d + 2)`, `d[2]`, `2[d]` to show they are equal, and `&d[3] - &d[0]` with `%td`.

Example:

```
double: p=0x... p+1=0x... step=8 bytes
int:    p=0x... p+1=0x... step=4 bytes
char:   p=0x... p+1=0x... step=1 bytes
```

<details><summary>Hint</summary>
Cast to `char *` before subtracting to get bytes; subtract typed pointers to get elements. `ptrdiff_t` prints with `%td`.
</details>

---

**05.3 — Out-parameters and swap**

Write `static void swap_d(double *a, double *b)`, `static int divmod(long a, long b, long *q_out, long *r_out)` returning -1 if `b == 0`, and `static int solve_quadratic(double a, double b, double c, double *x1, double *x2)` returning the number of real roots (0, 1, or 2) and writing them to the out-parameters only when they exist. Test `divmod(17, 5)`, `divmod(3, 0)`, and the quadratics `x^2 - 3x + 2`, `x^2 + 2x + 1`, `x^2 + 1`.

Example:

```
17 divmod 5 = 3 r 2
3 divmod 0 = error
x^2-3x+2: 2 roots: 1 2
x^2+2x+1: 1 root: -1
x^2+1: 0 roots
```

<details><summary>Hint</summary>
Discriminant `b*b - 4ac`; use `fabs(disc) < 1e-12` for the one-root case. Do not write to `*x1` when there are no roots.
</details>

---

**05.4 — const audit**

Write four functions with these EXACT signatures and bodies that compile: `static double sum(const double *v, size_t n)`, `static void fill(double *v, size_t n, double val)`, `static void print_fixed_target(double *const p)` (must modify `*p`, must not reassign `p`), and `static void scan(const double *const p)` (read-only, fixed). For each, add a commented-out line that would violate the `const` and paste the compiler's error message next to it. Then declare `const char *const *argv_like` pointing at a small array of string literals and read it aloud in a comment.

Example:

```
sum=10 after fill: 2 2 2 2
```

<details><summary>Hint</summary>
Read right to left from the name: `p` is a const pointer to a const double. Try to compile the violation once to capture the message, then comment it out.
</details>

---

**05.5 — Command-line argument parser**

Write `main(int argc, char *argv[])` for a program used as `./ex05_5 --n 1000 --lr 0.01 [--verbose]`. Walk `argv` with an index loop; for `--n` and `--lr` consume the next argument and parse it with `strtol`/`strtod` plus `endptr` checks; `--verbose` is a flag. Unknown arguments or missing values print a usage line to stderr and return 2. Print the parsed configuration. Test with valid input, missing value, bad number, and no arguments (defaults `n=100`, `lr=0.1`).

Example:

```
$ ./ex05_5 --n 1000 --lr 0.01 --verbose
n=1000 lr=0.01 verbose=1
$ ./ex05_5 --n abc
error: bad value for --n: "abc"
```

<details><summary>Hint</summary>
`strcmp(argv[i], "--n") == 0`. Check `i + 1 < argc` before reading `argv[i + 1]`. `argv[argc]` is `NULL`, so `for (int i = 1; argv[i]; i++)` also works.
</details>

---

**05.6 — Dangling pointer detective**

Write three functions: `static int *bad_local(void)` that returns the address of a local `int` (clang will warn: keep the warning text in a comment, then change the function to return a pointer to a `static int` so it compiles clean and explain what changed), `static const char *ok_literal(void)` returning a literal, and `static double *ok_caller_buffer(double *buf, size_t n)` that fills and returns `buf`. Also demonstrate a use-after-scope inside `main`: declare `int *p;` then in an inner block `int tmp = 5; p = &tmp;`, and after the block write a comment explaining why `*p` is now UB (do not dereference it). Build with `-fsanitize=address` and confirm the program is clean.

Example:

```
static: 42
literal: hello
buffer[2]: 2
```

<details><summary>Hint</summary>
The warning is `-Wreturn-stack-address`. A `static` local lives for the whole program, so its address stays valid; the cost is that every caller shares the same object.
</details>

---

**05.7 — Pointer-walk string functions**

Reimplement `my_strlen`, `my_strcpy`, `my_strcmp`, and `my_strchr` using ONLY pointer arithmetic (no `[]` indexing). `my_strcpy` must use the idiom `while ((*dst++ = *src++) != '\0') {}`. Also write `static void my_reverse(char *s)` using two pointers moving inward. Test against the `<string.h>` versions on `"pointer"`, `""`, and `"a"`, printing `ok` or `MISMATCH` per test.

Example:

```
strlen ok
strcpy ok
strcmp ok
strchr ok
reverse: retniop
```

<details><summary>Hint</summary>
`my_strcmp`: advance both while `*a && *a == *b`, then return `(unsigned char)*a - (unsigned char)*b`. `my_strchr` must also find `'\0'` when `c == 0`, as the real one does.
</details>

---

**05.8 — Generic integration with function pointers (numerics)**

Write `typedef double (*fn1)(double);` and `static double simpson(fn1 f, double a, double b, int n)` (n even). Integrate `sin` on `[0, π]`, `exp` on `[0, 1]`, and your own `static double gaussian(double x)` (`exp(-x*x/2) / sqrt(2π)`) on `[-5, 5]`, with `n = 10, 100, 1000`, printing the value and absolute error against the exact answers (2, e - 1, 1). Then write `static double bisect(fn1 f, double lo, double hi, double tol)` and find the root of `cos(x) - x` on `[0, 1]` and of `x*x*x - 2` on `[1, 2]`.

Example:

```
sin  n=  10 val=2.0001095 err=1.1e-04
sin  n= 100 val=2.0000000 err=1.1e-08
...
root of cos(x)-x = 0.7390851332
```

<details><summary>Hint</summary>
Simpson: `h/3 * (f(a) + 4 sum(odd) + 2 sum(even) + f(b))`. `sin` and `exp` from `<math.h>` have the right signature to be passed directly. `bisect` needs a small wrapper function for `cos(x) - x`.
</details>

---

**05.9 — Tokenizer vocabulary as `char **` (ML)**

Declare `const char *vocab[] = {"<unk>", "the", "cat", "sat", "on", "mat", "."}` and `size_t vocab_size = sizeof vocab / sizeof vocab[0]`. Write `static long vocab_lookup(const char *const *vocab, size_t n, const char *word)` returning the index or 0 for unknown (`<unk>`). Write `static void encode(const char *const *vocab, size_t n, const char *text, long *ids_out, size_t max_ids, size_t *n_out)` that splits `text` on spaces WITHOUT modifying it (walk with a `const char *` pair marking token start/end and compare with `strncmp` + length check) and writes token ids. Write `decode` that prints the words for an id array. Encode `"the cat sat on the dog ."` and decode the result.

Example:

```
ids: 1 2 3 4 1 0 6
decoded: the cat sat on the <unk> .
```

<details><summary>Hint</summary>
For each token `[start, end)`, a vocab entry matches if `strlen(vocab[i]) == (size_t)(end - start)` and `strncmp(vocab[i], start, end - start) == 0`. Skip runs of spaces before each token.
</details>

---

**05.10 — N-body state through pointers (sims)**

Define `struct body { double pos[3], vel[3], mass; }`. Write `static void body_kick(struct body *b, const double *acc, double dt)` (updates velocity through the pointer), `static void body_drift(struct body *b, double dt)` (updates position), and `static void pair_accel(const struct body *a, const struct body *b, double *acc_out)` writing the acceleration on `a` due to `b` (`G = 1`, softening `eps = 1e-3`) into the caller's 3-element array. In `main`, create two bodies in a circular orbit configuration (`m1 = 1` at origin, `m2 = 1e-3` at `(1,0,0)` with `vel (0,1,0)`), integrate 1000 leapfrog steps with `dt = 0.01`, and print the position of body 2 every 250 steps plus the initial and final total energy. Use `->` throughout; pass bodies by pointer only.

Example:

```
step    0: pos=(1.0000, 0.0000, 0.0000) E=-0.000500...
step  250: pos=(...)
...
```

<details><summary>Hint</summary>
Leapfrog: kick half step, drift full step, recompute acceleration, kick half step. `r^2 = dx*dx + dy*dy + dz*dz + eps*eps`, `acc = -G * m_b * d / r^3` where `d` is the vector from `b` to `a`. Energy: `0.5 m v^2` summed minus `G m1 m2 / r`.
</details>
