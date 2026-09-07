// Chapter 04 — std::vector, std::string, and the Standard Containers
//
// Demonstrates: vector construction and core API, growth/capacity,
// iterator invalidation (shown safely), a Matrix over a flat vector with
// the Rule of Zero, std::string operations for tokenizers, string_view,
// std::array, pair/tuple/structured bindings, map vs unordered_map with a
// custom pair hash (BPE pair counts), set/unordered_set, deque,
// stack/queue/priority_queue, iterators, and <algorithm>/<numeric>.
//
// Compile and run:
//   c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo && rm ex_demo

#include <algorithm>       // sort, max_element, find, count_if, remove_if, reverse
#include <array>
#include <cstddef>
#include <deque>
#include <functional>      // std::greater, std::hash
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>         // accumulate, iota
#include <queue>           // std::queue, std::priority_queue
#include <set>
#include <stack>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>         // std::pair
#include <vector>

// ---------------------------------------------------------------------------
// Matrix over a flat std::vector<double>: the Rule of Zero.
// No destructor, no copy ctor, no copy assignment — the vector owns the
// buffer, so copies are deep, const is deep, and nothing can double-free.
// Compare with the raw-pointer version in ../02_classes_and_raii/example.cpp.
// ---------------------------------------------------------------------------
class Matrix {
public:
    Matrix() = default;
    Matrix(int rows, int cols, double fill = 0.0)
        : rows_(rows), cols_(cols),
          data_(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), fill) {}

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    double &operator()(int i, int j)       { return data_[idx(i, j)]; }   // m(i, j) = v
    double  operator()(int i, int j) const { return data_[idx(i, j)]; }   // v = m(i, j)
    double       *data()       { return data_.data(); }                    // for C APIs / BLAS
    const double *data() const { return data_.data(); }

private:
    std::size_t idx(int i, int j) const { return static_cast<std::size_t>(i) * cols_ + j; }
    int rows_ = 0, cols_ = 0;
    std::vector<double> data_;
};

Matrix matmul(const Matrix &a, const Matrix &b) {
    Matrix out(a.rows(), b.cols());
    for (int i = 0; i < a.rows(); i++)
        for (int k = 0; k < a.cols(); k++) {            // i,k,j order: row-major friendly
            double aik = a(i, k);
            for (int j = 0; j < b.cols(); j++) out(i, j) += aik * b(k, j);
        }
    return out;                                          // by value; elided
}

std::ostream &operator<<(std::ostream &os, const Matrix &m) {
    for (int i = 0; i < m.rows(); i++) {
        os << "  [";
        for (int j = 0; j < m.cols(); j++) os << std::setw(5) << m(i, j);
        os << " ]\n";
    }
    return os;
}

// ---------------------------------------------------------------------------
// Custom hash for std::pair<int,int>: std::hash has no specialization for
// pair, and a naive h1 ^ h2 collides for (a,b)/(b,a) and gives 0 for (x,x).
// This is boost::hash_combine.
// ---------------------------------------------------------------------------
struct PairHash {
    std::size_t operator()(const std::pair<int, int> &p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

// string_view parameter: accepts std::string, const char*, and literals with
// no copy. substr on a view is O(1) and allocates nothing.
std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(delim, start);
        out.emplace_back(s.substr(start, pos == std::string_view::npos ? std::string_view::npos : pos - start));
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return out;
}

// Returning two values: a pair, destructured by the caller with a structured binding.
std::pair<double, double> min_max(const std::vector<double> &v) {
    auto [lo, hi] = std::minmax_element(v.begin(), v.end());   // pair of iterators
    return {*lo, *hi};
}

int main() {
    std::cout << std::boolalpha;

    // ---- 1. vector construction ---------------------------------------------
    std::cout << "== vector construction ==\n";
    std::vector<double> a;                    // empty
    std::vector<double> b(5);                 // five 0.0
    std::vector<double> c(5, 1.5);            // five 1.5
    std::vector<double> d{1.0, 2.0, 3.0};     // three elements
    std::vector<int> five_zeros(5), one_five{5};   // THE trap: (5) vs {5}
    std::cout << "a.size()=" << a.size() << " b.size()=" << b.size() << " c[0]=" << c[0] << " d.back()=" << d.back() << '\n';
    std::cout << "vector<int>(5).size() = " << five_zeros.size() << ",  vector<int>{5}.size() = " << one_five.size()
              << " with value " << one_five[0] << '\n';
    std::cout << "sizeof(std::vector<double>) = " << sizeof(std::vector<double>) << " (ptr + size + capacity)\n";

    // ---- 2. growth strategy ---------------------------------------------------
    std::cout << "\n== growth: capacity changes while pushing 1..20 ==\n";
    std::vector<int> g;
    std::size_t last_cap = 0;
    for (int i = 1; i <= 20; i++) {
        g.push_back(i);
        if (g.capacity() != last_cap) {
            last_cap = g.capacity();
            std::cout << "  size=" << g.size() << " capacity=" << g.capacity() << '\n';
        }
    }
    std::vector<int> r;
    r.reserve(20);                             // one allocation; capacity 20, size 0
    const int *before = r.data();
    for (int i = 1; i <= 20; i++) r.push_back(i);
    std::cout << "with reserve(20): buffer moved during pushes? " << (before != r.data()) << '\n';

    // ---- 3. core API -----------------------------------------------------------
    std::cout << "\n== core API ==\n";
    std::vector<double> v{3.0, 1.0, 4.0, 1.5};
    v.push_back(9.0);
    v.emplace_back(2.5);
    std::cout << "size=" << v.size() << " front=" << v.front() << " back=" << v.back() << '\n';
    v.pop_back();
    v.resize(7, -1.0);                          // grows to 7, new elements -1.0
    std::cout << "after pop_back + resize(7,-1): ";
    for (double x : v) std::cout << x << ' ';
    std::cout << "\nv[1]=" << v[1] << " (unchecked)  v.at(1)=" << v.at(1) << " (checked; .at(99) would throw)\n";
    v.clear();
    std::cout << "after clear: size=" << v.size() << " capacity=" << v.capacity() << " (capacity kept)\n";

    // ---- 4. iterator invalidation, demonstrated safely ---------------------------
    std::cout << "\n== iterator invalidation ==\n";
    std::vector<double> w{1, 2, 3};
    const double *p_before = w.data();
    w.push_back(4);                             // capacity 3 -> 4 (or more): reallocation
    std::cout << "buffer address changed after push_back: " << (p_before != w.data())
              << "  -> any reference/iterator taken before is now dangling (UB to use)\n";
    // Correct pattern for "append derived elements while iterating": use indices and copy the value first.
    std::size_t n0 = w.size();
    for (std::size_t i = 0; i < n0; i++) { double x = w[i]; if (x > 2) w.push_back(x * 10); }
    std::cout << "safe append: ";
    for (double x : w) std::cout << x << ' ';
    std::cout << '\n';

    // ---- 5. Matrix over a flat vector --------------------------------------------
    std::cout << "\n== Matrix over flat vector (Rule of Zero) ==\n";
    Matrix I(3, 3);
    for (int i = 0; i < 3; i++) I(i, i) = 1.0;
    Matrix X(3, 3);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) X(i, j) = i * 3 + j;
    Matrix Y = X;                               // deep copy, courtesy of std::vector
    Y(0, 0) = 99;
    std::cout << "X(0,0) after modifying copy Y: " << X(0, 0) << "\n";
    std::cout << "I * X == X row 1: [" << matmul(I, X)(1, 0) << ' ' << matmul(I, X)(1, 1) << ' ' << matmul(I, X)(1, 2) << "]\n";
    std::cout << "sizeof(Matrix) = " << sizeof(Matrix) << ", X.data()[4] = " << X.data()[4] << " (flat, row-major)\n";

    // ---- 6. std::string --------------------------------------------------------------
    std::cout << "\n== std::string ==\n";
    std::string s = "hello";
    s += " world";
    std::cout << s << " | size=" << s.size() << " | substr(6)=" << s.substr(6) << " | find(\"wor\")=" << s.find("wor")
              << " | find('z')==npos: " << (s.find('z') == std::string::npos) << '\n';
    std::cout << "to_string(3.5)=" << std::to_string(3.5) << "  stod(\"2.718\")*2=" << std::stod("2.718") * 2;
    std::size_t consumed = 0;
    double val = std::stod("1.5abc", &consumed);
    std::cout << "  stod(\"1.5abc\") -> " << val << " consumed " << consumed << " of 6 chars (trailing junk detected)\n";

    std::string word = "h\xc3\xa9llo";           // UTF-8 "héllo": 6 bytes, 5 code points
    std::cout << "bytes of \"h\xc3\xa9llo\" (" << word.size() << " bytes): ";
    for (unsigned char byte : word) std::cout << static_cast<int>(byte) << ' ';   // unsigned char: 195 not -61
    std::cout << "\n";
    std::vector<int> ids;
    for (unsigned char byte : word) ids.push_back(byte);            // byte-level token ids 0..255
    std::cout << "as token ids: size=" << ids.size() << ", ids[1]=" << ids[1] << '\n';

    std::string_view sv = s;                     // non-owning view; no copy
    std::string_view first_word = sv.substr(0, sv.find(' '));       // O(1), no allocation
    std::cout << "string_view first word: " << first_word << " (size " << first_word.size() << ")\n";
    auto fields = split("1.5,2.5,-4", ',');
    double row_sum = 0.0;
    for (const auto &f : fields) row_sum += std::stod(f);
    std::cout << "split CSV row -> " << fields.size() << " fields, sum = " << row_sum << '\n';

    // ---- 7. std::array --------------------------------------------------------------------
    std::cout << "\n== std::array ==\n";
    using Vec3 = std::array<double, 3>;
    Vec3 u{1, 2, 3}, t{4, 5, 6}, cross{};
    cross[0] = u[1] * t[2] - u[2] * t[1];
    cross[1] = u[2] * t[0] - u[0] * t[2];
    cross[2] = u[0] * t[1] - u[1] * t[0];
    std::cout << "u x t = {" << cross[0] << "," << cross[1] << "," << cross[2] << "}  sizeof(Vec3)=" << sizeof(Vec3)
              << "  (u == t) = " << (u == t) << '\n';

    // ---- 8. pair / tuple / structured bindings --------------------------------------------
    std::cout << "\n== pair / tuple / structured bindings ==\n";
    std::vector<double> data{3.0, -1.5, 7.25, 2.0};
    auto [lo, hi] = min_max(data);
    std::cout << "min_max -> lo=" << lo << " hi=" << hi << '\n';
    std::tuple<int, double, std::string> rec{7, 0.125, "loss"};
    auto &[epoch, loss, label] = rec;
    loss *= 2;                                   // reference binding writes into rec
    std::cout << "tuple: epoch=" << epoch << " " << label << "=" << std::get<1>(rec) << '\n';

    // ---- 9. map vs unordered_map, custom hash ---------------------------------------------------
    std::cout << "\n== unordered_map / map ==\n";
    std::unordered_map<std::string, int> counts;
    for (const auto &wd : split("the cat sat on the mat the end", ' ')) counts[wd]++;   // [] inserts 0 then ++
    std::cout << "counts[\"the\"]=" << counts["the"] << ", counts.count(\"dog\")=" << counts.count("dog");
    if (auto it = counts.find("dog"); it == counts.end()) std::cout << "  (find: absent, nothing inserted)\n";
    std::map<std::string, int> sorted(counts.begin(), counts.end());   // tree: sorted iteration
    std::cout << "sorted by key: ";
    for (const auto &[k, n] : sorted) std::cout << k << ':' << n << ' ';
    std::cout << '\n';

    // BPE pair counts over the byte ids of a small corpus
    std::string corpus = "aaabdaaabac";
    std::vector<int> toks;
    for (unsigned char byte : corpus) toks.push_back(byte);
    std::unordered_map<std::pair<int, int>, int, PairHash> pair_counts;
    for (std::size_t i = 0; i + 1 < toks.size(); i++) pair_counts[{toks[i], toks[i + 1]}]++;
    auto best = std::max_element(pair_counts.begin(), pair_counts.end(),
                                 [](const auto &x, const auto &y) { return x.second < y.second; });
    std::cout << "BPE: most frequent pair in \"" << corpus << "\" is (" << best->first.first << "," << best->first.second
              << ") = \"" << static_cast<char>(best->first.first) << static_cast<char>(best->first.second)
              << "\" with count " << best->second << '\n';

    // ---- 10. set / unordered_set --------------------------------------------------------------------
    std::cout << "\n== set / unordered_set ==\n";
    std::set<int> ordered{5, 1, 3, 1, 9};                    // dedup + sorted
    std::cout << "set{5,1,3,1,9} -> ";
    for (int x : ordered) std::cout << x << ' ';
    std::cout << " | min=" << *ordered.begin() << " max=" << *ordered.rbegin() << " lower_bound(4)=" << *ordered.lower_bound(4) << '\n';
    std::unordered_set<std::string> vocab{"the", "cat"};
    std::cout << "vocab has \"cat\": " << (vocab.count("cat") > 0) << ", has \"dog\": " << (vocab.count("dog") > 0) << '\n';

    // ---- 11. deque ------------------------------------------------------------------------------------
    std::cout << "\n== deque: sliding window ==\n";
    std::deque<double> window;
    double running = 0.0;
    for (int i = 1; i <= 10; i++) {
        window.push_back(i); running += i;
        if (window.size() > 3) { running -= window.front(); window.pop_front(); }
    }
    std::cout << "last-3 window = {" << window[0] << "," << window[1] << "," << window[2] << "}, mean = " << running / 3 << '\n';

    // ---- 12. adaptors ---------------------------------------------------------------------------------
    std::cout << "\n== stack / queue / priority_queue ==\n";
    std::stack<int> st;  for (int i = 1; i <= 3; i++) st.push(i);
    std::queue<int> qu;  for (int i = 1; i <= 3; i++) qu.push(i);
    std::cout << "stack top=" << st.top() << " (LIFO), queue front=" << qu.front() << " (FIFO)\n";
    std::priority_queue<double> maxheap;
    for (double x : {3.0, 9.0, 1.0, 4.0}) maxheap.push(x);
    std::cout << "max-heap pops: ";
    while (!maxheap.empty()) { std::cout << maxheap.top() << ' '; maxheap.pop(); }   // top() first, then pop()
    using Event = std::pair<double, int>;                                             // (time, id): compares by time first
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events;        // MIN-heap via std::greater
    events.push({0.7, 1}); events.push({0.2, 2}); events.push({0.5, 3});
    std::cout << "\nevent min-heap: ";
    while (!events.empty()) { std::cout << "t=" << events.top().first << " id=" << events.top().second << "  "; events.pop(); }
    std::cout << '\n';

    // ---- 13. iterators ------------------------------------------------------------------------------------
    std::cout << "\n== iterators ==\n";
    std::vector<double> sig{0.3, 0.9, -0.2, 0.7, 0.9};
    for (auto it = sig.begin(); it != sig.end(); ++it) *it *= 10;                 // explicit iterator loop
    auto mx = std::max_element(sig.begin(), sig.end());
    std::cout << "argmax = " << std::distance(sig.begin(), mx) << " (first max), value " << *mx << '\n';
    std::cout << "reversed: ";
    for (auto it = sig.rbegin(); it != sig.rend(); ++it) std::cout << *it << ' ';
    std::cout << '\n';

    // ---- 14. <algorithm> / <numeric> --------------------------------------------------------------------------
    std::cout << "\n== algorithms ==\n";
    std::vector<double> halves{0.5, 0.5, 0.5, 0.5};
    double sum_d = std::accumulate(halves.begin(), halves.end(), 0.0);   // 0.0: sums in double -> 2
    auto   sum_bad = std::accumulate(halves.begin(), halves.end(), 0);   // 0: sums in INT -> each 0+0.5 truncates to 0
    std::cout << "accumulate({.5,.5,.5,.5}, 0.0) = " << sum_d << "   accumulate(..., 0) = " << sum_bad << "  <- the int-init trap\n";
    long above = std::count_if(sig.begin(), sig.end(), [](double x) { return x > 5.0; });
    std::cout << "count_if(> 5) = " << above << '\n';
    std::vector<int> order(sig.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int i, int j) { return sig[i] < sig[j]; });   // argsort
    std::cout << "argsort: ";
    for (int i : order) std::cout << i << ' ';
    std::vector<double> sorted_copy = sig;
    std::sort(sorted_copy.begin(), sorted_copy.end());
    std::cout << " | sorted: ";
    for (double x : sorted_copy) std::cout << x << ' ';
    sig.erase(std::remove_if(sig.begin(), sig.end(), [](double x) { return x < 0; }), sig.end());   // erase-remove idiom
    std::cout << "| after removing negatives: size=" << sig.size() << '\n';

    return 0;
}
