// Chapter 01 — From C to C++
//
// One program touring what C++ adds on top of the C you already know:
// iostream + iomanip, namespaces, bool, nullptr, auto, references,
// const&, overloading, default args, constexpr, named casts, new/delete,
// std::string, range-for, brace init, enum class, inline variables,
// if-with-init, std::array/std::size, char literals, extern "C".
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo

#include <array>      // std::array
#include <cmath>      // std::sqrt, std::exp, std::fabs  (C++ twin of <math.h>)
#include <cstdint>    // std::uint32_t, std::uintptr_t
#include <cstdio>     // std::printf still works
#include <cstdlib>    // std::malloc, std::free
#include <cstring>    // std::memcpy
#include <iomanip>    // std::setw, std::setprecision, std::fixed, std::boolalpha
#include <iostream>   // std::cout, std::cerr
#include <iterator>   // std::size
#include <string>     // std::string, std::to_string, std::stod

// ---------------------------------------------------------------------------
// 1. Namespaces: a named scope. Everything in the standard library is in std.
//    Your own library gets its own namespace so `linalg::dot` and
//    `physics::dot` never collide.
// ---------------------------------------------------------------------------
namespace linalg {

// constexpr: a typed, scoped compile-time constant. Replaces `#define EPS 1e-9`.
inline constexpr double kEps = 1e-9;   // `inline` (C++17): safe to define in a header

// A constexpr function can run at compile time when given constant arguments,
// and is an ordinary function otherwise.
constexpr double square(double x) { return x * x; }

// Function overloading: same name, different parameter lists. The compiler
// picks one at compile time by argument types. (C would need dot2/dot3/dotn.)
double dot(double ax, double ay, double bx, double by) {
    return ax * bx + ay * by;
}
double dot(double ax, double ay, double az, double bx, double by, double bz) {
    return ax * bx + ay * by + az * bz;
}
double dot(const double *a, const double *b, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

}  // namespace linalg

// Compile-time check. Fails the BUILD (not the run) if false.
static_assert(linalg::square(3.0) == 9.0, "square broken");

// ---------------------------------------------------------------------------
// 2. References as out-parameters. `double &lo` is an alias for the caller's
//    variable: no `*`, cannot be null, cannot be reseated. Compare with the
//    C signature  void min_max(const double *a, int n, double *lo, double *hi).
// ---------------------------------------------------------------------------
void min_max(const double *a, int n, double &lo, double &hi) {
    lo = hi = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] < lo) lo = a[i];
        if (a[i] > hi) hi = a[i];
    }
}

// ---------------------------------------------------------------------------
// 3. const T& for passing a big struct without copying it. In C you would
//    write `const Particle *p` and use `p->mass`. Here `p.mass`, and the
//    caller cannot pass NULL.
// ---------------------------------------------------------------------------
struct Particle {          // In C++ `Particle` is a type name directly — no `struct` needed later.
    double x, y, z;
    double vx, vy, vz;
    double mass;
};

double kinetic_energy(const Particle &p) {
    return 0.5 * p.mass * (p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
}

// ---------------------------------------------------------------------------
// 4. Default arguments (only trailing ones). Declared once, here.
//    Newton's method as a taste of the numerics to come.
// ---------------------------------------------------------------------------
double newton(double (*f)(double), double (*df)(double), double x0,
              double tol = 1e-12, int max_iter = 50) {
    double x = x0;
    for (int i = 0; i < max_iter; i++) {
        double step = f(x) / df(x);
        x -= step;
        if (std::fabs(step) < tol) break;
    }
    return x;
}
double f_sqrt2(double x)  { return x * x - 2.0; }
double df_sqrt2(double x) { return 2.0 * x; }

// ---------------------------------------------------------------------------
// 5. enum class: scoped (must write Activation::ReLU) and does NOT convert
//    to int implicitly. -Wall warns if a switch misses an enumerator.
// ---------------------------------------------------------------------------
enum class Activation { ReLU, Sigmoid, Tanh };

double apply(Activation a, double x) {
    switch (a) {
        case Activation::ReLU:    return x > 0 ? x : 0.0;
        case Activation::Sigmoid: return 1.0 / (1.0 + std::exp(-x));
        case Activation::Tanh:    return std::tanh(x);
    }
    return 0.0;   // unreachable, but keeps -Wreturn-type quiet for a corrupt value
}

const char *name(Activation a) {
    switch (a) {
        case Activation::ReLU:    return "relu";
        case Activation::Sigmoid: return "sigmoid";
        case Activation::Tanh:    return "tanh";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// 6. extern "C": C++ mangles function names to support overloading (dot above
//    becomes _ZN6linalg3dotEdddd etc). extern "C" turns mangling off so a C
//    object file — your C matrix library — can be linked. Here the "C" function
//    is defined in this same file just so the example is self-contained; in
//    real use it lives in matrix.c and only the declaration has extern "C".
// ---------------------------------------------------------------------------
extern "C" double c_style_sum(const double *a, int n);   // declaration: C linkage

extern "C" double c_style_sum(const double *a, int n) {  // definition (would be in a .c file)
    double s = 0.0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

// A function that only reads, but a legacy API declared it with `char*`.
void legacy_print(char *s) { std::printf("legacy says: %s\n", s); }

int main() {
    // ---- iostream vs printf -------------------------------------------------
    std::cout << "== iostream vs printf ==\n";
    int n = 42;
    double pi = 3.14159265358979;
    std::cout << "n = " << n << ", pi = " << pi << '\n';   // type-safe: no %d/%f to get wrong
    std::cout << std::fixed << std::setprecision(3);       // sticky formatting state
    std::cout << "pi to 3 places: " << pi << '\n';
    std::cout << "width 8: [" << std::setw(8) << n << "]\n";
    std::printf("printf:  n=%d pi=%.3f\n", n, pi);         // still available, still fast
    std::cout.unsetf(std::ios::fixed);                      // back to default float formatting
    std::cout << std::setprecision(6);

    // ---- bool ---------------------------------------------------------------
    bool converged = std::fabs(0.1 + 0.2 - 0.3) < linalg::kEps;
    std::cout << "\n== bool ==\n";
    std::cout << "converged as int:  " << converged << '\n';
    std::cout << std::boolalpha << "converged as word: " << converged << std::noboolalpha << '\n';
    std::cout << "sizeof(bool) = " << sizeof(bool) << '\n';

    // ---- nullptr ------------------------------------------------------------
    std::cout << "\n== nullptr ==\n";
    double *maybe = nullptr;               // not NULL: nullptr has its own type
    if (!maybe) std::cout << "maybe is null\n";
    if (maybe == nullptr) std::cout << "(same check, spelled out)\n";

    // ---- auto ---------------------------------------------------------------
    std::cout << "\n== auto ==\n";
    auto i = 7;                 // int
    auto x = 7.0;               // double
    auto lit = "text";          // const char*  — NOT std::string
    auto str = std::string{"text"};
    std::cout << "i/2 = " << i / 2 << "  (int division: auto deduced int)\n";
    std::cout << "x/2 = " << x / 2 << "  (double)\n";
    std::cout << "sizeof(lit) = " << sizeof(lit) << " (a pointer), str.size() = " << str.size() << '\n';

    // ---- references as out-params ------------------------------------------
    std::cout << "\n== references ==\n";
    double data[] = {3.0, -1.5, 7.25, 2.0, 0.0};
    double lo, hi;
    min_max(data, static_cast<int>(std::size(data)), lo, hi);   // no & at the call site
    std::cout << "min = " << lo << ", max = " << hi << '\n';
    double &alias = lo;         // a reference IS the object: writing alias writes lo
    alias = -100.0;
    std::cout << "after alias = -100: lo = " << lo << '\n';

    // ---- const& parameters --------------------------------------------------
    std::cout << "\n== const& ==\n";
    Particle p{0, 0, 0, 1.0, 2.0, 2.0, 3.0};     // brace initialization, in declaration order
    std::cout << "sizeof(Particle) = " << sizeof(Particle) << " bytes, passed without copying\n";
    std::cout << "KE = " << kinetic_energy(p) << '\n';   // 0.5 * 3 * 9 = 13.5

    // ---- overloading --------------------------------------------------------
    std::cout << "\n== overloading ==\n";
    std::cout << "dot2 = " << linalg::dot(1, 2, 3, 4) << '\n';           // ints convert to double
    std::cout << "dot3 = " << linalg::dot(1, 2, 3, 4, 5, 6) << '\n';
    double a[] = {1, 2, 3}, b[] = {4, 5, 6};
    std::cout << "dotn = " << linalg::dot(a, b, 3) << '\n';

    // ---- default arguments --------------------------------------------------
    std::cout << "\n== default args ==\n";
    std::cout << std::setprecision(15);
    std::cout << "sqrt(2) via Newton, defaults: " << newton(f_sqrt2, df_sqrt2, 1.0) << '\n';
    std::cout << "sqrt(2), 2 iterations only:   " << newton(f_sqrt2, df_sqrt2, 1.0, 1e-12, 2) << '\n';
    std::cout << std::setprecision(6);

    // ---- constexpr ----------------------------------------------------------
    std::cout << "\n== constexpr ==\n";
    constexpr int kN = 4;                        // usable as an array bound
    double buf[kN] = {};                         // {} zero-fills
    constexpr double kEps2 = linalg::square(linalg::kEps);   // computed by the compiler
    std::cout << "kEps^2 = " << kEps2 << ", buf has " << std::size(buf) << " zeros\n";

    // ---- named casts --------------------------------------------------------
    std::cout << "\n== named casts ==\n";
    int hits = 7, trials = 2;
    std::cout << "int/int    = " << hits / trials << '\n';
    std::cout << "static_cast<double> = " << static_cast<double>(hits) / trials << '\n';
    std::cout << "double->int static_cast(2.99) = " << static_cast<int>(2.99) << '\n';

    void *raw = std::malloc(4 * sizeof(double));            // C++ will NOT convert void* implicitly...
    double *dp = static_cast<double *>(raw);                // ...you must say so
    for (int k = 0; k < 4; k++) dp[k] = k * 0.5;
    std::cout << "malloc'd doubles: " << dp[0] << ' ' << dp[1] << ' ' << dp[2] << ' ' << dp[3] << '\n';
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(dp);   // pointer -> integer: reinterpret
    std::cout << "address is 8-byte aligned: " << std::boolalpha << (addr % 8 == 0) << std::noboolalpha << '\n';
    std::free(raw);

    float fl = 1.0f;
    std::uint32_t bits;
    std::memcpy(&bits, &fl, sizeof bits);       // safe type punning (reinterpret_cast<uint32_t&> would be UB)
    std::printf("bits of 1.0f = 0x%08x\n", bits);

    char greeting[] = "hello from const_cast";
    const char *cg = greeting;
    legacy_print(const_cast<char *>(cg));       // OK only because legacy_print does not write

    // ---- new / delete -------------------------------------------------------
    std::cout << "\n== new/delete ==\n";
    double *heap = new double[5]();             // () -> value-initialized to 0.0 (like calloc)
    heap[2] = 42.0;
    std::cout << "heap[1] = " << heap[1] << ", heap[2] = " << heap[2] << '\n';
    delete[] heap;                              // new[] pairs with delete[]  (never free(), never delete)
    int *one = new int(5);
    std::cout << "*one = " << *one << '\n';
    delete one;                                 // new pairs with delete
    std::cout << "(you will rarely write these directly: std::vector does it for you)\n";

    // ---- std::string --------------------------------------------------------
    std::cout << "\n== std::string ==\n";
    std::string s = "token";
    s += "izer";                                // grows automatically; no strcat overflow
    std::string t = s + "_v" + std::to_string(2);
    std::cout << s << " has " << s.size() << " chars; t = " << t << '\n';
    std::cout << "s == \"tokenizer\": " << std::boolalpha << (s == "tokenizer") << std::noboolalpha << '\n';
    std::cout << "stod(\"2.718\") * 2 = " << std::stod("2.718") * 2 << '\n';
    std::printf("c_str for C APIs: %s\n", t.c_str());

    // ---- range-based for ----------------------------------------------------
    std::cout << "\n== range-for ==\n";
    for (double &d : data) d *= 2.0;            // by reference: modifies in place
    for (const auto &d : data) std::cout << d << ' ';   // read-only, no copies
    std::cout << '\n';
    for (char c : s) std::cout << static_cast<int>(static_cast<unsigned char>(c)) << ' ';  // bytes of a string
    std::cout << " (bytes of \"" << s << "\")\n";

    // ---- brace initialization and narrowing --------------------------------
    std::cout << "\n== brace init ==\n";
    double three_point_seven = 3.7;
    int narrow_ok = static_cast<int>(three_point_seven);   // C-style `int n = 3.7;` also compiles (truncates to 3),
                                                           // but clang warns on a literal; the cast says "on purpose"
    // int narrow_err{three_point_seven};       // ERROR if uncommented: {} rejects narrowing double -> int
    int zero{};                                 // value-initialized: 0
    double dz{};                                // 0.0
    std::cout << "int from 3.7 with '=' : " << narrow_ok << "   (with {} it would not compile)\n";
    std::cout << "int{} = " << zero << ", double{} = " << dz << '\n';

    // ---- enum class ---------------------------------------------------------
    std::cout << "\n== enum class ==\n";
    std::array<Activation, 3> acts{Activation::ReLU, Activation::Sigmoid, Activation::Tanh};
    for (Activation act : acts) {
        std::cout << std::setw(8) << name(act) << "(-1) = " << std::setw(9) << apply(act, -1.0)
                  << "   as int: " << static_cast<int>(act) << '\n';   // explicit conversion required
    }

    // ---- if with initializer (C++17) ---------------------------------------
    std::cout << "\n== if (init; cond) ==\n";
    if (double disc = 3.0 * 3.0 - 4.0 * 1.0 * 2.0; disc < 0) {
        std::cout << "no real roots\n";
    } else {
        std::cout << "roots exist, discriminant = " << disc << '\n';   // disc is in scope here too
    }
    // disc is NOT visible here — its scope ended with the if statement.

    // ---- std::array and std::size ------------------------------------------
    std::cout << "\n== std::array / std::size ==\n";
    std::array<double, 3> v{1.0, 2.0, 2.0};
    std::array<double, 3> w = v;                // arrays copy! (C arrays cannot be assigned)
    w[0] = 5.0;
    std::cout << "v = {" << v[0] << "," << v[1] << "," << v[2] << "}  w = {" << w[0] << "," << w[1] << "," << w[2] << "}\n";
    std::cout << "v == w: " << std::boolalpha << (v == w) << std::noboolalpha
              << ", v.size() = " << v.size() << ", sizeof(v) = " << sizeof(v) << " (no overhead)\n";
    std::cout << "|v| = " << std::sqrt(linalg::dot(v.data(), v.data(), static_cast<int>(v.size()))) << '\n';

    // ---- char literals ------------------------------------------------------
    std::cout << "\n== char literals ==\n";
    std::cout << "sizeof('a') = " << sizeof('a') << " in C++ (4 in C)\n";
    std::cout << "'a' prints as: " << 'a' << ", as int: " << static_cast<int>('a') << '\n';

    // ---- extern "C" ---------------------------------------------------------
    std::cout << "\n== extern \"C\" ==\n";
    std::cout << "c_style_sum(data) = " << c_style_sum(data, static_cast<int>(std::size(data))) << '\n';
    std::cout << "(linked with C naming: symbol is _c_style_sum, not a mangled _Z... name)\n";

    std::cerr << "done (this line went to stderr)\n";
    return 0;
}
