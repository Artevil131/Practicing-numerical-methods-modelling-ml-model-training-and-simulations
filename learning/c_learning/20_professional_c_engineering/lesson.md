# Chapter 20 — Professional C Engineering

## What you'll be able to do after this chapter

- Design a C API someone else can use without reading its source: opaque handles, a status-code convention, ownership documented per function, const-correctness, semver plus a run-time ABI check, and `-fvisibility=hidden` so only the symbols you chose are exported.
- Build one source three ways — `libmatrix.a`, `libmatrix.dylib`, `libmatrix.so` — explain `install_name`, `@rpath` and `LC_RPATH`, and debug "Library not loaded" with `otool` and `install_name_tool` instead of guessing.
- Ship a `.pc` file so `pkg-config --cflags --libs matrix` works.
- Call your own C matrix library from Python with `ctypes`, passing NumPy arrays by pointer with zero copies, verified against `A @ B` to 0.0 absolute error; and link the same header into C++ with `extern "C"`.
- Run unit, error-path, property, golden and fuzz tests; run libFuzzer, clang-tidy, `scan-build`, cppcheck and clang-format; wire all of it into a macOS + Linux GitHub Actions matrix with warnings-as-errors.
- Open SQLite, Lua, `stb`, CPython's `Objects/`, or `ggml` and know which file to read first.

## Why this matters for ML / numerics / sims

Everything in chapters 1–19 was a program you ran yourself. The step from *"my C works"* to *"my C is a library"* is what makes those nineteen chapters pay off: PyTorch is a Python veneer over a C++/C core, NumPy over a C core, `llama.cpp` a thin CLI over `ggml`. To make your chapter-19 matmul train an MLP you do not rewrite the training loop in C — you compile the kernel into a `.dylib` and hand it NumPy's buffer pointer. That is what this chapter builds, and the same skeleton (opaque handle, status codes, hidden visibility, ctypes binding, fuzzed parser, CI) is what separates code that lives in one folder from code others depend on.

`example.c` here is the whole chapter in one file: a demo program, a shared library, and a fuzz target, depending on which of the three commands in its header comment you run.

---

## 1. API design: the decisions you make once and cannot undo

An API is a promise about *names*, *lifetimes*, *errors* and *layout*. Break any and you break other people's builds — or their running programs.

### 1.1 Naming

C has one global namespace, so: **one short prefix on everything**, no exceptions — `mat_create`, `mat_t`, `MAT_OK`, `MATRIX_VERSION_MAJOR`. C11 §7.1.3 reserves identifiers starting with `_` + uppercase or `__` anywhere, and any leading `_` at file scope; do not take them. NumPy mapping: `np.matmul` lives in a module, and since C has no modules, `mat_` *is* the module.

### 1.2 Opaque handles

```c
/* matrix.h — the caller sees a name and nothing else */
typedef struct mat mat_t;
MAT_API mat_t *mat_create(size_t rows, size_t cols);
MAT_API size_t mat_rows(const mat_t *m);

/* matrix.c — the layout stays yours forever */
struct mat { size_t rows, cols; double *data; int owns_data; };
```
The caller cannot write `sizeof(mat_t)`, declare `mat_t m;` on the stack, or touch `m->rows`. So you can add or reorder fields next release and every already-linked program still runs. The transparent alternative, `typedef struct { size_t rows, cols; double *data; } mat_t;`, bakes `sizeof` into every caller — add a field and callers silently write past your struct. That is an **ABI break**, and it produces memory corruption, not a compiler error. Cost: a heap allocation and an accessor call per field. Pay it for anything with a lifetime or an invariant; skip it for a genuine value type (`struct { double x, y, z; } vec3` is a number, not an object). `example.c` shows both: `mat_t` is opaque, and `mat_matmul_raw(const double *a, const double *b, double *c, size_t m, size_t k, size_t n)` is a raw escape hatch — the one Python will call.

### 1.3 Error reporting: pick one convention, never mix

Four conventions are in circulation: a **status enum returned** (`mat_status s = mat_matmul(a,b,out)`) for the normal case; a **sentinel return** (`mat_t *m = mat_create(...); if (!m)`) for constructors returning a pointer; an **out-parameter** (`double d = mat_det(m, &status)`) when the return slot holds a real value; and an **`errno`-style global**, which breaks with threads and which you should not use.
```c
typedef enum mat_status {
    MAT_OK = 0,        /* success MUST be 0, so `if (s)` reads as "if it failed" */
    MAT_E_ALLOC, MAT_E_SHAPE, MAT_E_INVALID, MAT_E_PARSE, MAT_E_RANGE
} mat_status;
MAT_API const char *mat_strerror(mat_status s);   /* every value has a message. Every one. */
```
Zero is success. Never renumber existing values — ABI break, callers compiled the integer in. Append at the end, and make `mat_strerror` return `"unknown error"` for unknown values so an old caller with a new library prints something sane. **A library never prints to stderr and never calls `exit()`** — it returns a status and the application decides whether that is fatal; `example.c` routes diagnostics through a replaceable logging hook (§13). Make the status hard to ignore:
```c
#if defined(__GNUC__) || defined(__clang__)
#  define MAT_MUST_CHECK __attribute__((warn_unused_result))
#else
#  define MAT_MUST_CHECK
#endif
MAT_API MAT_MUST_CHECK mat_status mat_matmul(const mat_t *a, const mat_t *b, mat_t *out);
```

### 1.4 Ownership: document it in the header or it does not exist

Every pointer crossing the boundary needs one sentence saying who frees it. There is no type system to help, so the comment *is* the contract:
```c
/** @return Ownership TRANSFERS to the caller; free with mat_destroy(). NULL on alloc failure. */
MAT_API mat_t *mat_create(size_t rows, size_t cols);

/** @param data BORROWED. Must outlive the handle. Not freed by mat_destroy(). Zero-copy.
 *  @return Ownership of the *handle* transfers to the caller. */
MAT_API mat_t *mat_wrap(double *data, size_t rows, size_t cols);

/** @return BORROWED pointer into m's storage. Valid until m is destroyed. Do not free. */
MAT_API const double *mat_data_const(const mat_t *m);
```
Three words carry all of it: **owned**, **borrowed**, **transferred**. `mat_wrap` is the one that catches people: borrowed buffer, owned handle, and `mat_destroy` frees the handle without freeing the buffer (`example.c` tracks that with an `owns_data` flag the caller never sees). This is the most valuable habit in the chapter — half of all C library bugs are ownership bugs and they are all preventable by writing the sentence.

### 1.5 Const-correctness

`const` on a parameter is documentation the compiler enforces; apply it to every pointer you do not write through. `const mat_t *m` means "this function will not modify the object through this pointer" — it does *not* make `m->data`'s contents const. Drop `const` from `mat_rows` and every caller holding a const handle stops compiling, for a change that does nothing.

Read declarations right-to-left: `const double *p` is a pointer to const (cannot write `*p`, can reassign `p`), `double *const q` is a const pointer (can write `*q`, cannot reassign `q`). C cannot overload on constness, hence the paired accessors `mat_data(mat_t *)` / `mat_data_const(const mat_t *)`. Compile with `-Wcast-qual` so casting `const` away and writing becomes a warning rather than UB (C11 §6.7.3p6).

### 1.6 Versioning

```c
#define MATRIX_VERSION_MAJOR 1
#define MATRIX_VERSION_MINOR 2
#define MATRIX_VERSION_PATCH 0
#define MATRIX_VERSION ((MATRIX_VERSION_MAJOR<<16)|(MATRIX_VERSION_MINOR<<8)|MATRIX_VERSION_PATCH)
MAT_API unsigned    mat_version(void);          /* what the LOADED library is */
MAT_API const char *mat_version_string(void);
MAT_API int         mat_abi_compatible(unsigned compiled_against);

int mat_abi_compatible(unsigned compiled) {
    return (compiled >> 16) == MATRIX_VERSION_MAJOR              /* same major: ABI promised */
        && ((compiled >> 8) & 0xff) <= MATRIX_VERSION_MINOR;     /* runtime >= compile-time */
}
```
The macro is what the caller *compiled against*; the function is what *actually loaded*. With shared libraries those differ, which is the whole reason both exist.

Semver for a C library: **PATCH** = bug fix, no API/ABI change. **MINOR** = you *added* something (a function, an enum value at the end, a field at the end of a struct the caller never allocates); old callers work unrecompiled. **MAJOR** = anything that breaks either. `./ex_demo` prints the check as its first line:
```
libmatrix 1.2.0 (0x010200), compiled against 0x010200, ABI compatible: yes
```

### 1.7 What actually breaks ABI

**API break** = the caller fails to *compile*: annoying, loud, fixable. **ABI break** = the caller compiles, links, and *misbehaves at run time*. Only shared libraries make the second possible, and it is the dangerous one.

**Breaks ABI:** changing the size or field order of a struct the caller allocates or touches; changing a function's parameters or return type; removing or renaming an exported symbol; renumbering an enum; changing an inline function or macro in the header (callers already inlined the old body). **Does not:** adding an exported function; adding an enum value at the end; adding a field to a struct only *you* allocate (the opaque-handle payoff); changing any `static` function or any function body. The habit that follows: put *nothing* in the public header but declarations, opaque typedefs, enums, and macros you are willing to freeze.

### 1.8 Symbol visibility: `-fvisibility=hidden`

By default every non-`static` function is exported, so every helper you forgot to mark `static` is part of your ABI. Fix it globally, opt in per symbol:
```c
#if defined(_WIN32) && defined(MATRIX_BUILD)
#  define MAT_API __declspec(dllexport)      /* building the DLL */
#elif defined(_WIN32)
#  define MAT_API __declspec(dllimport)      /* consuming it */
#elif defined(__GNUC__) || defined(__clang__)
#  define MAT_API __attribute__((visibility("default")))
#else
#  define MAT_API
#endif
```
```bash
cc -std=c11 -O2 -fvisibility=hidden -DMATRIX_NO_MAIN -dynamiclib \
   -install_name @rpath/libmatrix.dylib -o libmatrix.dylib example.c -lm
nm -gU libmatrix.dylib
```
```
_mat_abi_compatible _mat_cols _mat_create _mat_data _mat_data_const _mat_destroy _mat_frobenius
_mat_matmul _mat_matmul_raw _mat_parse_csv_row _mat_rows _mat_set_log_fn _mat_set_log_level
_mat_strerror _mat_version _mat_version_string _mat_wrap
```
Seventeen symbols, all deliberate. (`nm -g` = external only, `-U` = defined only. The leading `_` is the Mach-O C prefix; ELF has none. Linux: `nm -D --defined-only libmatrix.so`.) Beyond tidiness: a smaller dynamic symbol table, faster load-time binding, no accidental collisions with the host program, and the linker may inline and specialise hidden functions because nobody outside can call them. **Rule:** every function not in the header is `static`; every function in the header is `MAT_API`. There is no third category.

---

## 2. Static `.a` vs shared `.dylib` / `.so`

```bash
# static archive — a bag of .o files plus an index
cc -std=c11 -O2 -DMATRIX_NO_MAIN -c -o matrix.o example.c && ar rcs libmatrix.a matrix.o
ar -t libmatrix.a          # __.SYMDEF SORTED, matrix.o     (ranlib regenerates the index)

# macOS shared
cc -std=c11 -O2 -fvisibility=hidden -DMATRIX_NO_MAIN -dynamiclib \
   -install_name @rpath/libmatrix.1.dylib \
   -compatibility_version 1.0.0 -current_version 1.2.0 -o libmatrix.1.dylib example.c -lm
# -> otool -L: @rpath/libmatrix.1.dylib (compatibility version 1.0.0, current version 1.2.0)

# Linux shared
cc -std=c11 -O2 -fPIC -fvisibility=hidden -DMATRIX_NO_MAIN -shared \
   -Wl,-soname,libmatrix.so.1 -o libmatrix.so.1.2.0 example.c -lm
ln -sf libmatrix.so.1.2.0 libmatrix.so.1 && ln -sf libmatrix.so.1 libmatrix.so
```
`-fPIC` is a no-op on arm64 Darwin (position-independent by default); write it anyway in a portable Makefile.

| | `libmatrix.a` | `libmatrix.dylib` / `.so` |
|---|---|---|
| Linked at | link time, copied in | run time, by `dyld` / `ld.so` |
| Ship / update | one binary; relink every consumer | binary + library; replace the file |
| Symbol resolution | linker takes **only the members that resolve something** | whole library mapped |
| Cross-`.o` optimisation | yes with `-flto` | no, across the boundary |
| ABI discipline | can be sloppy | must be strict (§1.7) |

Verified with the same consumer: `cc -O2 -o use_sta use.c libmatrix.a -lm` (`otool -L` → only libSystem) and `cc -O2 -o use_dyn use.c -L. -lmatrix -Wl,-rpath,@executable_path` (→ `@rpath/libmatrix.dylib`). Both print `success: 19 22 43 50  (lib 1.2.0)`.

**The static-link gotcha:** archives are searched once, in place, so `cc -lmatrix use.o` fails where `cc use.o -lmatrix` succeeds. Libraries go **after** the objects that use them; circular static deps on Linux need `-Wl,--start-group … --end-group`. Which to ship? For your own numerics, **static** — one binary, no rpath archaeology, LTO across the library, no ABI obligations. For a Python extension or plugin, **shared**, necessarily. For a library others build against, both.

---

## 3. `install_name`, `@rpath`, and "Library not loaded"

This wastes more macOS hours than anything else and is simple once you see the two steps.

**Step 1 — the library records its own name.** Every `.dylib` carries an `LC_ID_DYLIB` holding an *install name*: where it expects to be found. Set it with `-install_name`.
```bash
otool -D libmatrix.dylib                        # -> @rpath/libmatrix.dylib
otool -l libmatrix.dylib | grep -A3 LC_ID_DYLIB #    name @rpath/libmatrix.dylib (offset 24)
```
**Step 2 — the consumer copies that string in verbatim at link time.** At run time `dyld` substitutes each `LC_RPATH` entry of the *executable* for `@rpath`, in order. Three magic prefixes: `@rpath/…` (substitute each `LC_RPATH`; almost always what you want), `@executable_path/…` (directory of the running executable), `@loader_path/…` (directory of the binary doing the loading — differs when a `.dylib` loads a `.dylib`; what Python extension modules use). Add rpaths with `-Wl,-rpath,P` at link time or `install_name_tool -add_rpath P` afterwards, and list them with `otool -l use_dyn | grep -A2 LC_RPATH`.

### 3.1 The failure, and three fixes

```
dyld[3477]: Library not loaded: @rpath/libmatrix.dylib
  Referenced from: <B5BC…> /…/build/use_norpath
  Reason: no LC_RPATH's found
```
That last line is the whole diagnosis. In order of how professional they are:
```bash
cc -O2 -o use use.c -L. -lmatrix -Wl,-rpath,@executable_path   # 1. bake it at link time
install_name_tool -add_rpath @executable_path use_norpath      # 2. patch a built binary
DYLD_LIBRARY_PATH=$PWD/far ./use_far                           # 3. debugging ONLY
```
`DYLD_LIBRARY_PATH` is stripped by SIP for system binaries and inherited by children (so it breaks unrelated programs) — use it to confirm a hypothesis, then fix the rpath. To see what dyld really did: `DYLD_PRINT_LIBRARIES=1 ./use_dyn`.

### 3.2 `install_name_tool`, and its trap

```bash
install_name_tool -id   @rpath/libcopy.dylib libcopy.dylib   # the LIBRARY'S OWN name
install_name_tool -change OLD NEW            consumer        # a DEPENDENCY entry
install_name_tool -add_rpath / -delete_rpath / -rpath OLD NEW  consumer
```
**`-change` rewrites dependency records; it does not touch a library's own id — that is `-id`.** Running `-change /tmp/x/libcopy.dylib @rpath/libcopy.dylib libcopy.dylib` leaves `otool -D` still printing `/tmp/x/libcopy.dylib`, with no error. Verify every patch with `otool -D` and `otool -L`.

### 3.3 The Linux column

| macOS | Linux |
|---|---|
| `-install_name NAME` | `-Wl,-soname,NAME` |
| `LC_RPATH`, `-Wl,-rpath,P` | `DT_RUNPATH`, `-Wl,-rpath,P` |
| `@executable_path` | `$ORIGIN` (quote it: `-Wl,-rpath,'$ORIGIN'`) |
| `otool -L` / `otool -D` | `ldd` / `objdump -p x.so \| grep SONAME` |
| `nm -gU` | `nm -D --defined-only` |
| `DYLD_LIBRARY_PATH` / `-dynamiclib` | `LD_LIBRARY_PATH` / `-shared -fPIC` |

Linux also has `ldconfig` and `/etc/ld.so.conf` for system-wide search paths; macOS does not, which is precisely why `@rpath` exists.

---

## 4. pkg-config

A two-line protocol: a `.pc` file describes a library, consumers ask for flags. It removes hard-coded `-I/usr/local/include` from every Makefile you will write. Install to `$libdir/pkgconfig/matrix.pc`:
```ini
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: matrix
Description: Small dense double matrix library
Version: 1.2.0
Cflags: -I${includedir}
Libs: -L${libdir} -lmatrix
Libs.private: -lm
```
```bash
$ PKG_CONFIG_PATH=$PWD/pc pkg-config --modversion matrix
1.2.0
$ PKG_CONFIG_PATH=$PWD/pc pkg-config --cflags --libs matrix
-I/usr/local/include -L/usr/local/lib -lmatrix
$ PKG_CONFIG_PATH=$PWD/pc pkg-config --cflags --libs --static matrix
-I/usr/local/include -L/usr/local/lib -lmatrix -lm
$ PKG_CONFIG_PATH=$PWD/pc pkg-config --atleast-version=2.0 matrix || echo "no (exit 1)"
no (exit 1)
```
`Libs.private` holds dependencies needed only for *static* linking — a shared `libmatrix.dylib` already records its own dependency on libm. `Requires` names other pkg-config packages and makes their flags transitive; `Requires.private` does the same for static only. Real use: `pkg-config --libs openblas`, `--cflags gsl`, `--libs libpng`. With Homebrew, `PKG_CONFIG_PATH` usually needs `/opt/homebrew/lib/pkgconfig` plus per-formula `opt/<name>/lib/pkgconfig`.
```make
CFLAGS += $(shell pkg-config --cflags matrix)
LDLIBS += $(shell pkg-config --libs   matrix)
ifeq ($(shell pkg-config --exists matrix && echo ok),)
$(error matrix not found: set PKG_CONFIG_PATH)
endif
```


---

## 5. FFI part 1 — your C matrix library from Python with ctypes

This ties chapter 19's kernels to your actual ML work: build the `.dylib`, load it with `ctypes.CDLL`, declare every signature, hand NumPy's raw buffer pointer straight to C, check against NumPy.
```bash
cc -Wall -Wextra -std=c11 -O2 -fvisibility=hidden -DMATRIX_NO_MAIN -dynamiclib \
   -install_name @rpath/libmatrix.dylib -o libmatrix.dylib example.c -lm
```
`matrix.py`, next to the `.dylib`:
```python
import ctypes, os, sys
import numpy as np

_ext  = {"darwin": ".dylib", "win32": ".dll"}.get(sys.platform, ".so")
_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libmatrix" + _ext)
lib   = ctypes.CDLL(_path)
c_double_p, c_size_t_p = ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_size_t)

# ---- declare EVERY signature. Not optional; see 5.3. ----------------------------------------
ST, VP, I, CP = ctypes.c_size_t, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p
for name, restype, argtypes in [
    ("mat_version", ctypes.c_uint, []),        ("mat_version_string", CP, []),
    ("mat_abi_compatible", I, [ctypes.c_uint]), ("mat_strerror", CP, [I]),
    ("mat_matmul_raw", I, [c_double_p]*3 + [ST]*3), ("mat_create", VP, [ST, ST]),
    ("mat_wrap", VP, [c_double_p, ST, ST]),    ("mat_destroy", None, [VP]),
    ("mat_rows", ST, [VP]), ("mat_cols", ST, [VP]), ("mat_data", c_double_p, [VP]),
    ("mat_matmul", I, [VP]*3), ("mat_frobenius", ctypes.c_double, [VP]),
    ("mat_parse_csv_row", I, [CP, c_double_p, ST, c_size_t_p]),
]:
    f = getattr(lib, name); f.restype, f.argtypes = restype, argtypes

class MatError(Exception): pass

def _check(status):
    if status != 0: raise MatError(lib.mat_strerror(status).decode())

def _ptr(a):
    """C-contiguous float64 -> double*, no copy. Refuses anything that would need one."""
    if a.dtype != np.float64 or not a.flags["C_CONTIGUOUS"]:
        raise TypeError("need C-contiguous float64")
    return a.ctypes.data_as(c_double_p)

def matmul(A, B):
    A = np.ascontiguousarray(A, dtype=np.float64); B = np.ascontiguousarray(B, dtype=np.float64)
    m, k = A.shape; k2, n = B.shape
    if k != k2: raise ValueError(f"shape mismatch {A.shape} @ {B.shape}")
    C = np.empty((m, n), dtype=np.float64)
    _check(lib.mat_matmul_raw(_ptr(A), _ptr(B), _ptr(C), m, k, n))
    return C

def parse_csv_row(line, cap=64):        # out-parameter: a buffer + a count written back
    buf, cnt = (ctypes.c_double * cap)(), ctypes.c_size_t(0)
    _check(lib.mat_parse_csv_row(line.encode(), ctypes.cast(buf, c_double_p), cap, ctypes.byref(cnt)))
    return np.frombuffer(buf, dtype=np.float64, count=cnt.value).copy()

if __name__ == "__main__":
    print("libmatrix", lib.mat_version_string().decode(), hex(lib.mat_version()),
          "abi ok:", bool(lib.mat_abi_compatible(lib.mat_version())))
    rng = np.random.default_rng(0)
    A = rng.standard_normal((64, 48)); B = rng.standard_normal((48, 32))
    C = matmul(A, B); print("max abs err vs NumPy =", np.max(np.abs(C - A @ B)))
    assert np.allclose(C, A @ B, rtol=0, atol=1e-12)

    # opaque handle + zero copy: Python must keep X alive for the handle's whole lifetime
    X = np.ascontiguousarray(rng.standard_normal((5, 5)))
    h = lib.mat_wrap(_ptr(X), 5, 5)
    print("wrap:", lib.mat_rows(h), lib.mat_cols(h), lib.mat_frobenius(h), np.linalg.norm(X))
    X[0, 0] = 1000.0                                   # same memory — C sees the write
    print("after write:", lib.mat_frobenius(h), np.linalg.norm(X))
    lib.mat_destroy(h)                                 # frees the handle, NOT X's buffer

    a, b, out = lib.mat_create(2,3), lib.mat_create(4,5), lib.mat_create(2,5)
    print("mismatched matmul ->", lib.mat_strerror(lib.mat_matmul(a, b, out)).decode())
    for hh in (a, b, out): lib.mat_destroy(hh)
    print("csv:", parse_csv_row("1.5, -2, 3e2,4"))
    for bad in ("1,,2", "1,abc", "1,2,3,4,5"):
        try:    print("  %-11r ->" % bad, parse_csv_row(bad, cap=4))
        except MatError as e: print("  %-11r -> MatError: %s" % (bad, e))
```
Real output on this machine:
```
[matrix WARN] mat_matmul: shape mismatch (2x3)*(4x5)->(2x5)
libmatrix 1.2.0 0x10200 abi ok: True
max abs err vs NumPy = 0.0
wrap: 5 5 5.257095067618713 5.257095067618713
after write: 1000.0138137892616 1000.0138137892617
mismatched matmul -> shape mismatch
csv: [  1.5  -2.  300.    4. ]
  '1,,2'      -> MatError: parse error
  '1,abc'     -> MatError: parse error
  '1,2,3,4,5' -> MatError: buffer too small
```
Read that carefully — it is the chapter in ten lines. **0.0** absolute error means the C loop and NumPy's BLAS happened to accumulate in the same order at these sizes (do not expect that in general — use `atol`, never `==`; chapter 12). The Frobenius pair `…16` vs `…17` differs in the last bit precisely because they *did* sum differently. The in-place write proves `mat_wrap` was genuinely zero-copy, and the warning appears before the first print because C's `stderr` is unbuffered while Python's `stdout` is not.

### 5.3 The five ways ctypes will hurt you

1. **Forgetting `restype`** — it defaults to `c_int`, 32 bits: `mat_version_string` returns a truncated pointer and `mat_frobenius` (a `double` in `d0`) reads garbage from `w0`. No warning; a wrong number or a crash.
2. **Forgetting `argtypes`** — a Python `int` goes as `c_int`, so `size_t` parameters get 32-bit values in 64-bit slots (fine by luck on arm64, a real bug elsewhere); `argtypes` also turns silent corruption into `ctypes.ArgumentError`.
3. **Use `c_void_p` for opaque handles**, not `POINTER(SomeStruct)` — you do not have the struct.
4. **Lifetime** — `mat_wrap(_ptr(X), …)` borrows `X`'s buffer; if Python collects `X` the handle dangles with no diagnostic. Keep a reference on the wrapper object.
5. **Layout** — `.ctypes.data_as()` hands over raw bytes with no shape, strides or dtype, so a Fortran-ordered array, a slice like `A[::2]`, or `float32` all silently reinterpret. `np.ascontiguousarray(a, dtype=np.float64)` first — and note it *copies* when it must.

**Speed:** ctypes call overhead is ~1–2 µs — nothing for a 512³ matmul, catastrophic per element in a Python loop. **The FFI boundary belongs outside your inner loop, always**, which is exactly why NumPy's API is whole-array shaped.

---

## 6. FFI part 2 — cffi, and `extern "C"`

### 6.1 cffi

ctypes cannot read your header; `cffi` parses C declarations directly:
```python
from cffi import FFI
ffi = FFI()
ffi.cdef("""
    typedef enum { MAT_OK=0, MAT_E_ALLOC, MAT_E_SHAPE, MAT_E_INVALID,
                   MAT_E_PARSE, MAT_E_RANGE } mat_status;
    typedef struct mat mat_t;                     /* opaque: cffi is fine with this */
    const char *mat_version_string(void);
    mat_status  mat_matmul_raw(const double *a, const double *b, double *c,
                               size_t m, size_t k, size_t n);
""")
lib = ffi.dlopen("./libmatrix.dylib")
p = lambda a: ffi.cast("double *", a.ctypes.data)
assert lib.mat_matmul_raw(p(A), p(B), p(C), 8, 8, 8) == 0
```
`cdef` accepts a *subset* of C — no `#include`, no macros, no `MAT_API` — so you either `sed` the attributes out of your header or keep a small `matrix_cdef.h`. `ffi.dlopen` is "ABI mode" (like ctypes, no compiler at install time); `ffi.set_source(...)` + `ffi.compile()` is "API mode", which generates and compiles a real C extension — faster, type-checked by the actual compiler, and the right choice for packaging. Pick **ctypes** for a script or prototype, **cffi API mode** for a distributed package, and a **CPython extension** or **pybind11** when you need speed at the boundary or Python objects in C; Cython and numba occupy the same niche from the other direction.

### 6.2 `extern "C"`

C++ mangles names to encode types (that is how overloading works); C does not. Proof, from `nm` on two one-line C++ files:
```
0000000000000000 T __Z8mat_demoi     // int mat_demo(int)              — mangled
0000000000000000 T _mat_demo2        // extern "C" int mat_demo2(int)  — not
```
`__Z8mat_demoi` = `_Z` prefix, `8` characters of name, `i` for the `int` parameter. The fix goes **in the header** so C and C++ callers share it unchanged:
```c
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mat mat_t;
MAT_API mat_t *mat_create(size_t rows, size_t cols);
/* ... the whole public API ... */
#ifdef __cplusplus
}
#endif
```
`__cplusplus` is defined only by a C++ compiler, so the C compiler never sees it; every public C header does this. The C++ side then gets RAII for free:
```cpp
std::unique_ptr<mat_t, void(*)(mat_t*)> m(mat_create(2, 2), mat_destroy);
printf("C++ ok: rows=%zu ver=%s\n", mat_rows(m.get()), mat_version_string());
// c++ -std=c++17 -Wall -o use_cpp use.cpp libmatrix.a -lm  ->  C++ ok: rows=2 ver=1.2.0
```
Three things it does **not** do: make C++ exceptions safe to throw through C frames (they must not cross — catch everything at the boundary); apply to types (struct layout is the ABI's business); allow marking an overloaded function `extern "C"` twice.

---

## 7. Testing at scale

Chapter 13 gave you `TEST`/`CHECK`. This is what to do when one file of asserts is not enough. `example.c`'s suite is the reference:
```
11 tests, 470 checks, 0 failed (4 expected error/warn log lines suppressed)
```
**Unit tests** — one function, known input, known output (`test_matmul_known_values` does the 2×2 case you can do on paper). Cheap, precise, and they only catch what you thought of. **Error-path tests** — `test_matmul_errors` calls with NULL, mismatched shapes, and zero dimensions, asserting the *status code*. Untested error paths are where the segfaults live, because nobody runs them by hand. `test_strerror_covers_every_status` loops over every enum value plus an out-of-range one, so adding a status without a message fails the build.

**Property tests** — algebraic identities on random inputs: `A(BC)==(AB)C`, `A·I==A`, `(AB)ᵀ==BᵀAᵀ`, `A(B+C)==AB+AC`, `‖A‖_F` invariant under transpose. These catch what you did not think of, especially at odd sizes.
```c
for (int trial = 0; trial < 200; trial++) {
    size_t m = 1 + rnd_below(7), k = 1 + rnd_below(7), n = 1 + rnd_below(7);
    CHECK(fabs(lhs - rhs) <= 1e-9 * (1.0 + fabs(lhs)));   /* tolerance, never == */
}
```
The seed must be printed up front and settable from the environment — a property test that cannot be replayed is a flaky test.

**Golden tests** — record a checksum from a run you trust and assert it forever. `test_golden_checksum` hashes a fixed 32×32 product, catching "I refactored the blocking and changed the summation order", which no unit test notices. The discipline: when a golden value changes you must *justify* it in the commit message before updating. A golden *file* (`tests/golden/rows.txt` + `diff`) works the same way for text output.

Hand-rolled (~40 lines, zero dependencies) is what `example.c`, SQLite, Lua and `stb` effectively do, and is right for a small library. Reach for a framework when you need per-test process isolation (so a segfault in test 3 does not hide tests 4–20), fixtures, or mocks: **Unity** (three files, drop them in, no build system) and **CMocka** (function mocking with `will_return`/`expect_value`, forked per test; needs CMake) are the two to know; **greatest**, **µnit** and **Criterion** are the others. Whatever you use, the suite must **exit non-zero on failure** — that is the entire interface CI needs.

**Coverage:**
```bash
cc -std=c11 -O0 -g -fprofile-instr-generate -fcoverage-mapping -o ex_cov example.c -lm
LLVM_PROFILE_FILE=ex.profraw ./ex_cov && xcrun llvm-profdata merge -sparse ex.profraw -o ex.profdata
xcrun llvm-cov report ./ex_cov -instr-profile=ex.profdata
xcrun llvm-cov show  ./ex_cov -instr-profile=ex.profdata --name=mat_parse_csv_row
```
Use it to **find untested branches**, not as a target: 100% lines with no assertions proves nothing, while 70% where the gap is `MAT_E_ALLOC` paths is a defensible position.

**Run the suite under sanitizers.** Tests that pass at `-O2` and crash under ASan were always broken; you just could not see it. Make it a CI job — ASan+UBSan is the standard pairing, and TSan needs a *separate* build (chapter 16) since they are mutually exclusive.
```bash
cc -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -o ex_asan example.c -lm
./ex_asan            # example.c: clean
```

---

## 8. Fuzzing with libFuzzer

A fuzzer generates inputs, watches which paths they reach via compiler-inserted coverage, and mutates the ones that reach new paths. On a parser it finds in seconds what you would never write by hand: empty string, lone comma, `1e999`, a 4 GB line, embedded NUL, `-`, `.`, `1e`. The interface is one function:
```c
#ifdef MATRIX_FUZZ
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char buf[4096];
    if (size >= sizeof buf) return 0;
    memcpy(buf, data, size); buf[size] = '\0';        /* the parser wants a C string */
    double out[64]; size_t count = 0;
    if (mat_parse_csv_row(buf, out, 64, &count) == MAT_OK)
        assert(count <= 64);       /* invariants: this is what the fuzzer is really checking */
    return 0;                      /* non-zero is reserved; always return 0 */
}
#endif
```
A fuzz target must be **deterministic** (no time, RNG, network or files), **fast** (>10 000 exec/s — throughput *is* your bug-finding rate), **leak-free** (ASan's leak checker runs at exit), and it must **assert your invariants**, since otherwise a fuzzer only finds crashes.

### 8.1 Running it — and the Apple clang problem

```bash
cc -g -O1 -fsanitize=fuzzer,address -DMATRIX_FUZZ -o fuzz_csv example.c -lm
```
fails on this machine:
```
ld: library '/Applications/Xcode.app/…/lib/clang/21/lib/darwin/libclang_rt.fuzzer_osx.a' not found
clang: error: linker command failed with exit code 1
```
**Apple's clang ships ASan and UBSan but not libFuzzer.** Use Homebrew LLVM (upstream clang on Linux just works, which is why the CI matrix in §12 fuzzes on Linux):
```bash
brew install llvm
$(brew --prefix llvm)/bin/clang -g -O1 -fsanitize=fuzzer,address,undefined -DMATRIX_FUZZ \
    -o fuzz_csv example.c -lm
mkdir -p corpus && ./fuzz_csv corpus -max_total_time=30
```

### 8.2 Reading the output, keeping the corpus

```
#512    NEW    cov: 68 ft: 91  corp: 9/23b  lim: 8  L: 4/8 MS: 2 CopyPart-
#16384  pulse  cov: 74 ft: 118 corp: 23/94b  exec/s: 16384 rss: 33Mb
```
`cov` = edges covered, `ft` = features, `corp: N/Mb` = N inputs totalling M bytes. Rising `cov` means it is still learning; flat for a long time means saturated, or stuck behind a magic-number check it cannot guess.

On a find it writes `crash-<sha1>` and prints the ASan report. Replay with `./fuzz_csv crash-abc123`, minimise with `-minimize_crash=1`, then **add the minimised input as a permanent unit test** — `test_fuzz_harness_inputs` in `example.c` is exactly that: the fuzzer's greatest hits, rerun in a millisecond on every build. Commit `corpus/` too; it seeds the next run and makes fuzzing incremental across releases. Useful flags: `-max_total_time=60`, `-runs=100000`, `-max_len=256`, `-dict=csv.dict`, `-jobs=8 -workers=8`, `-print_final_stats=1`.

**Alternatives:** AFL++ (`afl-clang-fast`, process-based, so it fuzzes whole programs and survives crashes), honggfuzz in between, and **OSS-Fuzz**, which runs libFuzzer targets continuously and free for open source — SQLite, curl and Lua are all in it, which is a real part of why they are solid.


---

## 9. Static analysis

Three tools finding three different things; they overlap less than you would expect. None ships by default on macOS — `brew install llvm cppcheck`.

**clang-tidy** checks the AST: bug patterns, API misuse, modernisation. `.clang-tidy` at the repo root:
```yaml
Checks: >
  -*, bugprone-*, cert-*, clang-analyzer-*, performance-*, portability-*, readability-*, misc-*,
  -readability-magic-numbers, -readability-identifier-length, -bugprone-easily-swappable-parameters
WarningsAsErrors: 'bugprone-*,clang-analyzer-*,cert-*'
HeaderFilterRegex: '.*'
CheckOptions:
  - key: readability-function-cognitive-complexity.Threshold
    value: '35'
```
```bash
$(brew --prefix llvm)/bin/clang-tidy example.c -- -std=c11 -I.
$(brew --prefix llvm)/bin/run-clang-tidy -p build      # whole project, via compile_commands.json
```
Start from `-*` and add categories; the default set is enormous and mostly C++. `bugprone-*` and `clang-analyzer-*` find real defects, `readability-*` are opinions. Suppress one line with `// NOLINTNEXTLINE(check-name)`, always naming the check and the reason. The compilation database (`compile_commands.json`) comes from CMake's `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` or `bear -- make`; clangd wants the same file.

**scan-build** is the clang static analyzer: path-sensitive symbolic execution, finding null derefs, leaks, use-after-free, uninitialised reads and dead stores that no per-statement check can see.
```bash
$(brew --prefix llvm)/bin/scan-build --status-bugs -o /tmp/scan make   # exit non-zero on findings
```
It writes HTML where each bug is an *annotated path* through your source. Click through it: a false positive usually means your code has an invariant the analyzer cannot see, which is worth an assert either way, and `--status-bugs` is what makes it a gate. **cppcheck** is an independent implementation with different heuristics — notably good at constant-index array bounds, unused variables, and standard-library misuse:
```bash
cppcheck --enable=warning,style,performance,portability --std=c11 \
         --inline-suppr --error-exitcode=1 --quiet --suppress=missingIncludeSystem example.c
```
Higher false-positive rate, so suppress deliberately (`// cppcheck-suppress <id>`) rather than lowering `--enable`. What none of them can do: see across the FFI boundary, reason about your threads (use TSan), or know your invariants. Static analysis is the cheapest of the three defect-finding layers — analysis, sanitizers, fuzzing — and the least thorough. Run all three.

---

## 10. clang-format

Formatting arguments cost more time than formatting. Delete the argument: commit a `.clang-format` and enforce it in CI.
```yaml
Language: Cpp                # yes, "Cpp" is also the setting for C
BasedOnStyle: LLVM
IndentWidth: 4
UseTab: Never
ColumnLimit: 100
PointerAlignment: Right      # double *p, not double* p
DerivePointerAlignment: false
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
BreakBeforeBraces: Attach
SpaceBeforeParens: ControlStatements
IndentCaseLabels: false
AlignConsecutiveMacros: AcrossEmptyLinesAndComments
MaxEmptyLinesToKeep: 1
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
IncludeCategories:           # own headers, then C system headers, then the rest
  - { Regex: '^"matrix', Priority: 1 }
  - { Regex: '^<.*\.h>', Priority: 2 }
  - { Regex: '.*', Priority: 3 }
```
```bash
brew install clang-format                                 # or it comes with `brew install llvm`
clang-format --style=file -i src/*.c include/*.h          # rewrite in place
clang-format --style=file --dry-run --Werror src/*.c      # CI gate
```
`BasedOnStyle` also takes `Google`, `Mozilla`, `WebKit`, `Chromium`, `Microsoft`, `GNU`. Pick one, tweak three things, stop thinking about it; `clang-format --style=llvm --dump-config > .clang-format` gives the full resolved set to edit. Introducing it to an existing codebase: reformat everything in **one commit that changes nothing else**, then
```bash
echo "a1b2c3d4  # clang-format the whole tree" >> .git-blame-ignore-revs
git config blame.ignoreRevsFile .git-blame-ignore-revs
```
Guard the rest with a pre-commit hook, but keep the CI check — hooks are local and skippable.

---

## 11. Reproducible builds and warnings-as-errors

```make
WARN := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-qual \
        -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition \
        -Wvla -Wformat=2 -Wundef -Wwrite-strings -Wswitch-enum -Wnull-dereference \
        -Wpointer-arith -Wredundant-decls
```
What each buys: `-Wshadow`, an inner `i` hiding an outer one; `-Wconversion`/`-Wsign-conversion`, the `size_t`↔`int` truncations that become buffer bugs; `-Wcast-qual`, casting `const` away (§1.5); `-Wstrict-prototypes`/`-Wmissing-prototypes`, `f()` meaning "unspecified args" and non-static functions you forgot to make `static`; `-Wvla`, accidental variable-length arrays; `-Wformat=2`, `printf(user_string)`; `-Wswitch-enum`, which makes adding a `mat_status` break every switch that does not handle it — exactly what you want.

`example.c` is clean under all of those with `-Werror`, with **one** instructive exception:
```
$ cc -std=c11 -Wall -Wextra -Wdouble-promotion -Werror -O2 -o /dev/null example.c -lm
error: implicit conversion increases floating-point precision: 'float' to 'double'
       [-Werror,-Wdouble-promotion]
note: expanded from macro 'NAN'
   66 | #   define NAN __builtin_nanf("0x7fc00000")
```
The warning is correct and the code is not at fault: Apple's `<math.h>` defines `NAN` as a `float`. That is the standard shape of a `-Werror` problem — a third-party header, not your code. Responses, best first: drop that one flag; `#define MAT_NAN ((double)NAN)`; or `#pragma clang diagnostic push/ignored/pop` around the use with a comment.

**`-Werror` in CI, never in the tarball** (`ifeq ($(CI),1)` / `CFLAGS += -Werror`). Unconditional `-Werror` is hostile to users: a new compiler adds a warning and nobody can build your release. **Reproducible builds** — same source in, byte-identical binary out, on a different machine on a different day. It makes "did this binary come from that source?" answerable and build caching correct. What breaks it, and the fix:

- `__DATE__`/`__TIME__` — never use them; pass `-DBUILD_REV=\"$(git rev-parse HEAD)\"` instead.
- Absolute paths in debug info and `__FILE__` — `-ffile-prefix-map=$(PWD)=.`.
- Timestamps in archives — GNU `ar rcsD` (macOS `libtool -static` is already deterministic).
- Filesystem iteration order — `$(sort $(wildcard src/*.c))`, never a bare glob.
- Environment leakage — `CFLAGS` from your shell, locale in sort order (`LC_ALL=C`), `$USER` in a version string; and link order that depends on `-j` scheduling, so write explicit object lists.
- `SOURCE_DATE_EPOCH` — the cross-ecosystem convention; set it from the commit time.

```bash
make clean && make && shasum -a 256 libmatrix.a build/ex_demo > a.txt
make clean && make && shasum -a 256 libmatrix.a build/ex_demo > b.txt
diff a.txt b.txt && echo reproducible          # diffoscope build1/x build2/x to see what differs
```
Pin the toolchain too: record `cc --version` in build metadata and pin CI images by digest, not `latest`.

---

## 12. CI with GitHub Actions

`.github/workflows/ci.yml`:
```yaml
name: ci
on:
  push: { branches: [main] }
  pull_request:
  schedule: [{ cron: '0 6 * * 1' }]     # weekly: catches rot from new compiler releases
permissions: { contents: read }
env: { CI: 1 }

jobs:
  build:
    name: ${{ matrix.os }} / ${{ matrix.cc }} / ${{ matrix.sanitize }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [macos-14, ubuntu-24.04]
        cc: [clang, gcc]
        sanitize: ['none', 'address,undefined']
        exclude:
          - { os: macos-14, cc: gcc }   # macOS "gcc" is a clang shim; testing it twice is noise
    steps:
      - uses: actions/checkout@v4
      - name: Build (warnings are errors) and test
        env: { CC: '${{ matrix.cc }}' }
        run: |
          FLAGS="-std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
                 -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wvla -Wformat=2 -Werror"
          [ "${{ matrix.sanitize }}" = none ] || \
            FLAGS="$FLAGS -g -O1 -fsanitize=${{ matrix.sanitize }} -fno-omit-frame-pointer"
          $CC --version && $CC $FLAGS -o ex_demo example.c -lm && ./ex_demo
      - name: Shared library + exported-symbol gate
        env: { CC: '${{ matrix.cc }}' }
        run: |
          if [ "$RUNNER_OS" = macOS ]; then
            $CC -std=c11 -O2 -fvisibility=hidden -DMATRIX_NO_MAIN -dynamiclib \
                -install_name @rpath/libmatrix.dylib -o libmatrix.dylib example.c -lm
            nm -gU libmatrix.dylib | tee syms.txt && otool -L libmatrix.dylib
          else
            $CC -std=c11 -O2 -fPIC -fvisibility=hidden -DMATRIX_NO_MAIN -shared \
                -Wl,-soname,libmatrix.so.1 -o libmatrix.so example.c -lm
            nm -D --defined-only libmatrix.so | tee syms.txt && ldd libmatrix.so
          fi
          ! grep -E ' [TD] ' syms.txt | grep -vE ' _?mat_' || { echo "leaked symbol!"; exit 1; }
      - if: matrix.sanitize == 'none'                          # FFI round-trip vs NumPy
        run: python3 -m pip install -q numpy && python3 tests/test_ffi.py

  format-and-analyze:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get update && sudo apt-get install -y clang-format clang-tidy cppcheck
      - run: clang-format --style=file --dry-run --Werror *.c *.h
      - run: clang-tidy example.c -- -std=c11 -I.
      - run: cppcheck --enable=warning,style,performance,portability --std=c11
                      --error-exitcode=1 -q --suppress=missingIncludeSystem example.c

  fuzz:
    runs-on: ubuntu-24.04                # libFuzzer is not in Apple clang; see §8.1
    steps:
      - uses: actions/checkout@v4
      - run: |
          clang -g -O1 -fsanitize=fuzzer,address,undefined -DMATRIX_FUZZ -o fuzz_csv example.c -lm
          mkdir -p corpus && ./fuzz_csv corpus -max_total_time=60 -print_final_stats=1
      - uses: actions/upload-artifact@v4
        if: failure()
        with: { name: fuzz-crashes, path: 'crash-*' }

```
Details worth stealing: `fail-fast: false` so one red cell does not hide the others; the matrix in `name:` so the failing job is identifiable from the PR page; `permissions: contents: read` because the default token is too powerful; the symbol-leak check as a hard gate; the weekly run catching new-compiler breakage before a user does; crash artifacts uploaded only `if: failure()`. Keep PR CI under ~10 minutes or people stop reading it — push expensive jobs to a nightly workflow.

---

## 13. Logging, tracing, configuration

A library must not decide what the application prints. The pattern is **levels + a replaceable sink + environment configuration**, all off by default.
```c
typedef enum { MAT_LOG_ERROR = 0, MAT_LOG_WARN, MAT_LOG_INFO, MAT_LOG_DEBUG } mat_log_level;
typedef void (*mat_log_fn)(mat_log_level level, const char *msg, void *user);
MAT_API void mat_set_log_level(mat_log_level level);
MAT_API void mat_set_log_fn(mat_log_fn fn, void *user);   /* NULL restores the default sink */

static mat_log_level g_level = MAT_LOG_ERROR;
#define MAT_LOG(lvl, ...) do { if ((lvl) <= g_level) mat_log_impl((lvl), __VA_ARGS__); } while (0)

MAT_LOG(MAT_LOG_WARN, "mat_matmul: shape mismatch (%zux%zu)*(%zux%zu)->(%zux%zu)", ...);
```
`do { … } while (0)` is the multi-statement-macro idiom (chapter 14); the level check *outside* the call means a debug log in a hot loop costs one predictable branch. Configuration is read once from the environment — `MAT_LOG=error|warn|info|debug`, so `MAT_LOG=debug ./ex_demo` turns on the chatter. The general hierarchy, weakest to strongest: compiled-in defaults → config file → environment variable → explicit API call → command-line flag; a library supports the first, third and fourth, while parsing config files and argv is the application's job. Testability is the real payoff: `test_logging_hook` installs a sink that appends to a buffer instead of printing, so the suite can *assert on log output* and can suppress the four warnings its error-path tests deliberately provoke — hence `(4 expected error/warn log lines suppressed)`. A library that hard-codes `fprintf(stderr, …)` can do neither.

Rules: never log to `stdout` (that is the program's data channel); default to silence; never log secrets or full user input; put enough context in the message to be actionable without a debugger; if you need structure, emit one JSON object per line rather than inventing a format. For *tracing* (timings, not messages), emit Chrome Trace Event JSON — `{"name":"matmul","ph":"X","ts":123,"dur":456,"pid":1,"tid":1}` — and open it in Perfetto; on macOS `os_signpost` integrates with Instruments. Both beat scattered `printf` timing.

---

## 14. Doxygen

Documentation next to the code stays true; documentation in a wiki does not.
```c
/**
 * @brief   Multiply two matrices: out = a * b.
 * @param   a    Left operand, m x k. BORROWED, not modified.
 * @param   b    Right operand, k x n. BORROWED, not modified.
 * @param   out  Destination, m x n, caller-allocated. Overwritten on success, untouched on failure.
 * @return  ::MAT_OK; ::MAT_E_INVALID if any argument is NULL; ::MAT_E_SHAPE if dims do not conform.
 * @warning Aliasing @p out with @p a or @p b is undefined behaviour.
 * @since   1.0.0
 * @see     mat_matmul_raw() for the pointer-based version used by FFI.
 */
MAT_API mat_status mat_matmul(const mat_t *a, const mat_t *b, mat_t *out);
```
```bash
brew install doxygen graphviz && doxygen -g Doxyfile && doxygen Doxyfile   # -> html/
```
Settings worth changing in the generated `Doxyfile`:
```
PROJECT_NAME = matrix          PROJECT_NUMBER = 1.2.0
INPUT = include src README.md  USE_MDFILE_AS_MAINPAGE = README.md
EXTRACT_ALL = NO               # NO => undocumented public items show up as gaps
EXTRACT_STATIC = NO            # internals are not the public API
WARN_IF_UNDOCUMENTED = YES     WARN_AS_ERROR = FAIL_ON_WARNINGS
OPTIMIZE_OUTPUT_FOR_C = YES    JAVADOC_AUTOBRIEF = YES    HAVE_DOT = YES    GENERATE_LATEX = NO
```
`EXTRACT_ALL = NO` + `WARN_IF_UNDOCUMENTED = YES` + `WARN_AS_ERROR` is the combination that keeps docs complete: a public function without a comment fails CI. Document the *header*, not the implementation — that is what users read. Alternatives: Sphinx + Breathe (if the project also has Python docs), and plain Markdown for narrative. Doxygen is an API reference, not a substitute for a README with a five-line working example above the fold.

---

## 15. Reading real C codebases

You learn more C from a week reading `stb_image.h` than a month of tutorials.

**stb** (`stb_image.h`, `stb_truetype.h`) — ~7 000 lines, public domain, single header. **Start here.** Read the top comment block, the `#ifdef STB_IMAGE_IMPLEMENTATION` split, then `stbi__jpeg_decode`. Learn: the single-header pattern (declarations always, definitions behind one macro in exactly one TU), aggressively `static` internals behind an `stbi__` prefix, and how far zero dependencies and zero abstraction get you. The best model for the library *you* will write.

**SQLite** — the most heavily tested C on earth. Do not start with the 8 MB amalgamation; start with `https://sqlite.org/arch.html`, then `sqlite3.h` (a masterclass in §1: every function documented, ABI-stable since 2004), then `src/btree.c`'s header comment and `src/vdbe.c`'s opcode loop. Learn: API stability as a product feature, the amalgamation build (one giant TU so everything inlines), assertions everywhere, TH3's 100% branch coverage.

**Lua** — ~30 000 lines, the cleanest large C program there is. `lobject.h` (tagged-union values), `lvm.c` (bytecode loop), `lgc.c` (incremental GC), `ltable.c` (array/hash hybrid). Learn: a complete language runtime in 30k lines, `TValue` as a model for dynamic typing in C, disciplined macros.

**CPython `Objects/`** — `object.c`, `listobject.c`, `dictobject.c`, `Include/object.h`. Learn: reference counting done seriously (`Py_INCREF`/`Py_DECREF`, borrowed vs new references — the §1.4 vocabulary with real stakes), `PyObject` inheritance-by-struct-prefix, the type-object vtable. `dictobject.c`'s comment on the open-addressing probe sequence is genuinely great technical writing.

**ggml / llama.cpp** — **the ML-relevant one, and the reason this chapter exists.** In order:

1. `ggml.h`, `struct ggml_tensor`: `ne[4]` (element counts per dim), `nb[4]` (strides **in bytes**), `type`, `op`, `src[]`, `data`. That struct is the whole design. Compare with NumPy's shape/strides/dtype — and note that strides in *bytes* is what lets mixed-precision and quantised types work uniformly.
2. `ggml_new_tensor_*` and the arena behind it (`ggml_context`, `ggml_init_params.mem_buffer`): no per-tensor `malloc`, one big buffer, bump-allocated. Chapter 18 was preparation for this.
3. `ggml_add`, `ggml_mul_mat`, `ggml_soft_max` — they compute **nothing**. They allocate a result tensor and record `op` + `src[]`. That is graph construction, exactly like building a PyTorch graph.
4. `ggml_build_forward_expand` / `ggml_graph_compute` — topological sort, then a thread pool walking nodes, with `ggml_compute_forward_*` where arithmetic finally happens. `ggml_compute_forward_mul_mat` is the real kernel: blocking, `GGML_SIMD` macros over NEON/AVX (chapter 19), a `vec_dot_t` function pointer chosen per quantisation type (chapter 11).
5. `ggml-quants.c`, `block_q4_0`: 32 weights as 4-bit values plus one fp16 scale — 18 bytes for what was 128. `quantize_row_q4_0` and `ggml_vec_dot_q4_0_q8_0` are the ~200 lines that make a 7B model run on a laptop.
6. `llama.cpp`: `llama_model_load` (GGUF — a memory-mappable file needing no parsing) and `llm_build_llama` (the transformer as a ggml graph; you will recognise every line from PyTorch).

Everything in this chapter appears there: opaque handles (`llama_context`), status codes, `extern "C"`, an arena allocator, function-pointer dispatch, SIMD kernels, and a C API Python binds to.

**How to read any codebase:** README and `ARCHITECTURE.md`; then the public header, all of it — that is the vocabulary; then follow one operation end to end in `lldb` rather than with your eyes; then `git log --oneline | tail -50` for the earliest commits, which are the simple version of everything; then build it, break something on purpose, and see which test fails.

---

## 16. The binary toolbox

```bash
# --- symbols ---
nm -gU libmatrix.dylib          # macOS: external (-g), defined only (-U)
nm -D --defined-only lib.so     # Linux equivalent
nm -u use_dyn                   # undefined: what this binary needs
                                #   ___stack_chk_fail _mat_matmul_raw _mat_strerror ...
nm -m lib.dylib / nm -C / nm --size-sort -S    # Mach-O detail / demangle C++ / fattest functions

# --- dependencies and identity ---
otool -L use_dyn                # what it needs                 (Linux: ldd)
otool -D libmatrix.dylib        # its own install name          (Linux: objdump -p | grep SONAME)
otool -l libmatrix.dylib        # LC_ID_DYLIB, LC_RPATH, LC_LOAD_DYLIB
otool -hv use_dyn               # header: filetype, arch, PIE.  otool -tvV = disassemble
readelf -d lib.so               # Linux: NEEDED, SONAME, RUNPATH. -Ws symbols, -h header

# --- size and content ---
size -m libmatrix.dylib         # Segment __TEXT: 16384 / __text: 2244 / __cstring: 429
strings -a libmatrix.dylib      # check you are not shipping a build path or a key
file / lipo -info / lipo -create

# --- debug info ---
cc -g -O2 -c -o dbg.o example.c && cc -g -o dbg2 dbg.o -lm
dsymutil dbg2                   # -> dbg2.dSYM/Contents/Resources/DWARF/dbg2
strip -x dbg2                   # ship stripped, archive the .dSYM
atos -o dbg2.dSYM/Contents/Resources/DWARF/dbg2 -l 0x100000000 0x100003f2c   # symbolicate
```
**The `dsymutil` gotcha:** `cc -g -o dbg example.c` compiles and links in one step and clang *deletes the temporary `.o`* — so `dsymutil` warns `unable to open object file` / `no debug symbols in executable`. On macOS the debug info lives in the object files and `dsymutil` gathers it, so you must **compile and link separately**, as the two-`cc` version above does. The release workflow that enables: build with `-g`, `dsymutil`, `strip`, ship the stripped binary, archive the `.dSYM` with the git SHA — six months later `atos` turns a crash address back into file:line.

---

## 17. Licensing basics

Not legal advice; this is the practical shape, and getting it wrong is expensive.

| License | You must | Notable |
|---|---|---|
| **MIT** / **BSD-2/3** / **ISC** | keep the notice | maximally permissive |
| **Apache-2.0** | keep notices, state changes, `NOTICE` | explicit **patent grant** |
| **MPL-2.0** | publish changes to MPL files | file-level copyleft; links into proprietary code |
| **LGPL-2.1/3** | let users relink a modified library | **dynamic linking is the safe path** |
| **GPL-2/3** / **AGPL-3** | release the whole derived work as GPL | linking it in makes *you* GPL; AGPL triggers on network use too |
| **Unlicense / CC0** | nothing | some jurisdictions reject public-domain dedication — hence `stb`'s MIT dual option |

In practice: **(1)** Pick one and commit it — a `LICENSE` file, an SPDX line atop every source file (`/* SPDX-License-Identifier: MIT */`), and the license in README and package metadata. Code with no license is "all rights reserved"; nobody may legally use it, including future you at a different employer. **(2)** MIT or Apache-2.0 for a library you want used — Apache if patents are plausible; Rust's `MIT OR Apache-2.0` dual is a good default. **(3)** Audit what you link: every dependency's license propagates through linking, so GPL in the tree makes your binary GPL. Keep a `THIRD_PARTY_NOTICES` file; `reuse lint` or `scancode-toolkit` automates the check. **(4)** Copied code carries its license — a Stack Overflow snippet is CC-BY-SA, a GPL snippet is GPL, and "I rewrote it a bit" is not a defence. **(5)** For numerics specifically: OpenBLAS is BSD-3, Eigen is MPL-2, MKL is proprietary with redistribution terms, Accelerate is system-provided.

---

## 18. The professional C checklist

**API** — [ ] one prefix, no reserved identifiers. [ ] opaque handles for anything with a lifetime. [ ] one error convention, `0` = success, `strerror` covers every value including unknown. [ ] the library never prints and never `exit()`s. [ ] ownership documented on every pointer: owned / borrowed / transferred. [ ] `const` on every pointer not written through; `-Wcast-qual` clean. [ ] semver macros *and* a run-time version function *and* an ABI check. [ ] header holds declarations, opaque typedefs and enums only. [ ] `extern "C"` guards and include guards; header self-contained. [ ] `-fvisibility=hidden` + export macro; `nm -gU` reviewed symbol by symbol. [ ] thread-safety documented per function, even if the answer is "no".

**Build** — [ ] static and shared both work, macOS and Linux. [ ] `-install_name @rpath/…` / `-Wl,-soname,…`; consumer rpath baked at link time. [ ] a `.pc` file with `Libs.private`. [ ] strict warnings, `-Werror` in CI only. [ ] reproducible: no `__DATE__`, prefix-mapped paths, sorted wildcards, pinned toolchain. [ ] `make clean && make` from a fresh clone, no manual steps.

**Correctness** — [ ] unit, error-path, property (printed seed) and golden tests. [ ] suite exits non-zero on failure, runs in under a minute. [ ] clean under `-fsanitize=address,undefined`; TSan too if threaded. [ ] a fuzz target per parser; corpus and every historical crash committed as tests. [ ] clang-tidy, scan-build, cppcheck clean or suppressed with reasons. [ ] coverage measured, and the gaps are deliberate.

**Process** — [ ] `.clang-format` enforced; `.git-blame-ignore-revs` for the reformat commit. [ ] CI: macOS + Linux, clang + gcc, sanitizers, fuzz job, format/analyze job. [ ] Doxygen on every public symbol with `WARN_AS_ERROR`. [ ] README with a working five-line example above the fold; CHANGELOG with semver headings. [ ] LICENSE, SPDX headers, third-party notices audited. [ ] logging with levels, off by default, sink replaceable, environment-configurable.

---

## Gotchas and undefined behavior

- Adding a field to a **transparent** struct in a public header changes `sizeof` and corrupts memory in callers that were not recompiled. Opaque handles make it impossible.
- Renumbering or inserting an enum value breaks ABI: old callers compiled the integer in. Append only.
- An ignored status code is the most common bug in C library *usage* — the caller then reads an untouched `out` buffer, which is uninitialised, and reading it is UB (C11 §6.3.2.1p2). `warn_unused_result`.
- Casting away `const` and writing through the result is UB (§6.7.3p6) even when it "works". `-Wcast-qual`.
- `exit()` or `abort()` from a library takes down the host process, including a Python interpreter with unsaved state. Return a status.
- ctypes without `restype` truncates a returned pointer to 32 bits and reads a `double` from the wrong register, with no diagnostic. Declare every signature.
- `mat_wrap(numpy_ptr, …)` borrows memory Python may free at any moment: keep a Python-side reference for the handle's whole lifetime, or you have a use-after-free ASan cannot see (it is on the Python heap).
- A non-contiguous or non-`float64` array handed over via `.ctypes.data_as` reinterprets whatever bytes are there. `np.ascontiguousarray(a, dtype=np.float64)` first.
- Throwing a C++ exception through C stack frames is undefined. Catch everything at the `extern "C"` boundary.
- `install_name_tool -change` does **not** modify a library's own install name — that is `-id` — and it fails silently. Verify with `otool -D`.
- Static link order: `cc -lmatrix use.o` fails where `cc use.o -lmatrix` succeeds. Archives are searched once, in place.
- `DYLD_LIBRARY_PATH` is stripped under SIP and inherited by children — debugging only. And `cc -g -o prog x.c` deletes the temporary `.o`, so `dsymutil` finds no debug info: compile and link separately.
- Apple clang has no libFuzzer: `-fsanitize=fuzzer` fails with `libclang_rt.fuzzer_osx.a not found`. Homebrew LLVM or Linux. ASan and TSan also cannot be combined in one build — that is two CI jobs.
- A fuzz target using randomness, time, or the filesystem is not reproducible, and the crash it finds will not replay.
- `-Werror` in a released tarball breaks users' builds on every new compiler release. `__DATE__` in a binary destroys reproducibility for nothing.

## Common mistakes checklist

- [ ] Exporting every non-`static` symbol because `-fvisibility=hidden` was never added, then finding a helper is part of your ABI.
- [ ] A public header defining a struct, freezing the layout by accident.
- [ ] Mixing error conventions — some return status, some `-1`, one sets a global; or `printf` inside a library.
- [ ] Ownership rules that live in your head instead of the header.
- [ ] Bumping major for a bug fix, or shipping an ABI break as a minor.
- [ ] ctypes bindings with no `restype`/`argtypes` "because it worked on my test".
- [ ] Calling a C function per element from a Python loop and concluding C is slow.
- [ ] Tests covering only the happy path; the error branches are the ones that crash.
- [ ] A property test with an unprintable seed, so failures cannot be replayed.
- [ ] Updating a golden value to make the build green without explaining what changed.
- [ ] Fuzzing once, finding nothing, never committing the corpus; or treating a static-analysis false positive as noise instead of as a missing assertion.
- [ ] Reformatting and changing behaviour in the same commit.
- [ ] CI that only builds on your OS with your compiler; `-Werror` in the shipped build.
- [ ] No LICENSE file, or a GPL dependency in a project you call MIT.

## You can move on when...

- You can build `example.c` three ways (demo, `.dylib` with `-fvisibility=hidden`, `.a`) and `nm -gU` shows exactly the seventeen `mat_*` symbols and nothing else.
- Given `dyld: Library not loaded: @rpath/libfoo.dylib … Reason: no LC_RPATH's found`, you can name three fixes and say which one ships.
- You can explain, without looking, `install_name_tool -id` vs `-change`, and `-install_name` vs `-Wl,-soname`.
- Your `matrix.py` reports `max abs err vs NumPy = 0.0` on a 64×48 @ 48×32 product, and you can explain why the Frobenius norms differ in the last bit while the matmul does not.
- You can say what happens if you drop `restype` from `mat_frobenius`, and why it is not a compile error.
- You can write the `extern "C"` guard from memory and decode `__Z8mat_demoi`.
- Your library has unit, error-path, property and golden tests, is clean under `-fsanitize=address,undefined`, and every input the fuzzer found is a committed regression test.
- You have CI building on macOS and Linux with the strict warning set plus `-Werror`, running the sanitizer matrix, fuzzing on Linux, and failing on an unexpected exported symbol.
- You can open `ggml.h`, find `struct ggml_tensor`, and explain `ne[]`, `nb[]`, `op` and `src[]` in terms of NumPy shape/strides and a PyTorch autograd graph.
- You can walk the §18 checklist against your own matrix library and either tick each box or say deliberately why not.
